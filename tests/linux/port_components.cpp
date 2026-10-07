// SPDX-License-Identifier: AGPL-3.0-only
#include <sys/stat.h>
#include "Platform.h"
#include "tjsCommHead.h"
#include "tjsUtils.h"
#include "aligned_allocator.h"
#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

// LeakSanitizer requires /proc/<pid>/task, unavailable in restricted runners.
// Address and undefined-behaviour checks remain enabled.
extern "C" const char* __asan_default_options() { return "detect_leaks=0"; }

struct alignas(64) Wide { int value; };

int main() {
    tTVP_stat portableStat{};
    portableStat.atime_seconds = 1;
    assert(portableStat.atime_seconds == 1);
    std::vector<float, aligned_allocator<float>> samples(100, 1.f);
    samples.resize(1000);
    assert(samples.front() == 1.f && samples.back() == 0.f);
    assert(reinterpret_cast<std::uintptr_t>(samples.data()) % 16 == 0);
    aligned_allocator<Wide> wide;
    Wide* w = wide.allocate(2);
    assert(reinterpret_cast<std::uintptr_t>(w) % alignof(Wide) == 0);
    wide.deallocate(w, 2);
    bool overflow = false;
    try { aligned_allocator<double>().allocate(std::numeric_limits<std::size_t>::max()); }
    catch (const std::bad_alloc&) { overflow = true; }
    assert(overflow);

    // Fill the buffer, then insert in the middle to force reallocation while
    // existing iterator indices must remain valid.
    TJS::tVectorList<int> list;
    list.push_back(0);
    const auto initial = list.capacity();
    for (unsigned i = 1; i < initial; ++i) list.push_back(i);
    auto it = list.begin(); ++it;
    int inserted = -1;
    list.insert_before(it, inserted);
    assert(*it == 1 && list.capacity() > initial);
    auto scan = list.begin(); assert(*scan++ == 0); assert(*scan++ == -1); assert(*scan == 1);
    while (list.size() < list.capacity()) list.push_back(123);
    list.insert_after(it, inserted);
    assert(*it == 1); ++it; assert(*it == -1);

    const auto& readOnly = list;
    static_assert(std::is_same<decltype(*readOnly.begin()), const int&>::value, "const iteration must be read-only");
    static_assert(std::is_same<decltype(readOnly.front()), const int&>::value, "const front must be read-only");
    unsigned count = 0;
    for (auto cursor = readOnly.begin(); cursor != readOnly.end(); ++cursor) ++count;
    assert(count == list.size());
    auto reverse = readOnly.rbegin(); assert(*reverse == 123);
    assert(readOnly.rend().end());
    int one = 1; assert(*readOnly.find(one) == 1);
    list.erase(it);
    assert(list.size() == count - 1);
    list.clear(); assert(list.empty());
}

// SPDX-License-Identifier: AGPL-3.0-only
#ifndef __ALIGNED_ALLOCATOR_H__
#define __ALIGNED_ALLOCATOR_H__
// kirikinux2: missing from the public Kirikiroid2 tree (visual/gl/ResampleImage.cpp).
// Standard C++11 allocator returning TAlign-byte aligned storage.
#include <cstddef>
#include <cstdlib>
#include <new>
#include <limits>
#include <utility>

template <class T, int TAlign = 16>
struct aligned_allocator {
	typedef T value_type;
	typedef T *pointer;
	typedef const T *const_pointer;
	typedef T &reference;
	typedef const T &const_reference;
	typedef std::size_t size_type;
	typedef std::ptrdiff_t difference_type;
	static_assert(TAlign > 0 && (TAlign & (TAlign - 1)) == 0, "alignment must be a power of two");
	static const int ALIGN_SIZE = TAlign;
	template <class U> struct rebind { typedef aligned_allocator<U, TAlign> other; };

	aligned_allocator() noexcept {}
	template <class U> aligned_allocator(const aligned_allocator<U, TAlign> &) noexcept {}

	T *allocate(std::size_t n, const void * = 0) {
		if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_alloc();
		void *p = nullptr;
		std::size_t align = (std::size_t)TAlign < sizeof(void *) ? sizeof(void *) : (std::size_t)TAlign;
		if (align < alignof(T)) align = alignof(T);
		if (posix_memalign(&p, align, (n ? n : 1) * sizeof(T)) != 0) throw std::bad_alloc();
		return static_cast<T *>(p);
	}
	void deallocate(T *p, std::size_t) noexcept { free(p); }
	template <class U, class... Args> void construct(U *p, Args &&... args) { ::new ((void *)p) U(std::forward<Args>(args)...); }
	template <class U> void destroy(U *p) { p->~U(); }
};
template <class T, class U, int A> bool operator==(const aligned_allocator<T, A> &, const aligned_allocator<U, A> &) { return true; }
template <class T, class U, int A> bool operator!=(const aligned_allocator<T, A> &, const aligned_allocator<U, A> &) { return false; }

#endif // __ALIGNED_ALLOCATOR_H__

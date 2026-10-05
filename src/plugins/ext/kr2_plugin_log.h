// SPDX-License-Identifier: AGPL-3.0-only
// Minimal replacement for the spdlog calls used by plugins ported from the
// KrKr2 emulator (github.com/2468785842/krkr2).  Keeps the `LOGGER->info("{}",
// x)` call shape so ported files stay close to upstream, but avoids adding
// spdlog/fmt to the AppImage.  "{}" / "{:...}" placeholders are replaced in
// order; format specs are ignored except ":x"/":X" (hex).
#pragma once
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

namespace kr2log {

enum class Level { Trace, Debug, Info, Warn, Error, Critical };

inline Level threshold() {
    static const Level lv = [] {
        const char *e = std::getenv("KR2_PLUGIN_LOG");
        if(!e) return Level::Warn;
        std::string s(e);
        if(s == "trace") return Level::Trace;
        if(s == "debug") return Level::Debug;
        if(s == "info") return Level::Info;
        if(s == "error") return Level::Error;
        if(s == "off") return static_cast<Level>(99);
        return Level::Warn;
    }();
    return lv;
}

template <typename T>
inline void put(std::ostringstream &os, const T &v, bool hex, int prec = -1,
                char fmtc = 0) {
    if constexpr(std::is_floating_point_v<T>) {
        if(prec >= 0) {
            std::ostringstream t;
            if(fmtc == 'f') t.setf(std::ios::fixed, std::ios::floatfield);
            t.precision(prec);
            t << v;
            os << t.str();
        } else {
            os << v;
        }
        return;
    }
    if constexpr(std::is_integral_v<T> && !std::is_same_v<T, bool> &&
                 !std::is_same_v<T, char>) {
        if(hex) {
            os << std::hex << static_cast<unsigned long long>(v) << std::dec;
            return;
        }
        os << static_cast<long long>(v);
    } else if constexpr(std::is_enum_v<T>) {
        os << static_cast<long long>(v);
    } else {
        os << v;
    }
}

inline void formatTo(std::ostringstream &os, const char *f) {
    for(; *f; ++f) {
        if((f[0] == '{' && f[1] == '{') || (f[0] == '}' && f[1] == '}')) ++f;
        os << *f;
    }
}

template <typename T, typename... Rest>
inline void formatTo(std::ostringstream &os, const char *f, const T &v,
                     const Rest &...rest) {
    for(; *f; ++f) {
        if(f[0] == '{' && f[1] == '{') { os << '{'; ++f; continue; }
        if(f[0] == '}' && f[1] == '}') { os << '}'; ++f; continue; }
        if(*f == '{') {
            const char *e = f;
            while(*e && *e != '}') ++e;
            bool hex = false;
            int prec = -1;
            char fmtc = 0;
            for(const char *c = f; c < e; ++c) {
                if(*c == 'x' || *c == 'X') hex = true;
                if(*c == '.') prec = std::atoi(c + 1);
                if(*c == 'f' || *c == 'g') fmtc = *c;
            }
            put(os, v, hex, prec, fmtc);
            formatTo(os, *e ? e + 1 : e, rest...);
            return;
        }
        os << *f;
    }
}

template <typename... A>
inline std::string format(const char *f, const A &...a) {
    std::ostringstream os;
    formatTo(os, f, a...);
    return os.str();
}

struct Logger {
    template <typename... A> void log(Level lv, const char *tag, const char *f, const A &...a) {
        if(static_cast<int>(lv) < static_cast<int>(threshold())) return;
        std::string s = format(f, a...);
        std::fprintf(stderr, "[plugin:%s] %s\n", tag, s.c_str());
    }
    template <typename... A> void trace(const char *f, const A &...a) { log(Level::Trace, "trace", f, a...); }
    template <typename... A> void debug(const char *f, const A &...a) { log(Level::Debug, "debug", f, a...); }
    template <typename... A> void info(const char *f, const A &...a) { log(Level::Info, "info", f, a...); }
    template <typename... A> void warn(const char *f, const A &...a) { log(Level::Warn, "warn", f, a...); }
    template <typename... A> void error(const char *f, const A &...a) { log(Level::Error, "error", f, a...); }
    template <typename... A> void critical(const char *f, const A &...a) { log(Level::Critical, "critical", f, a...); }
};

inline Logger *get() {
    static Logger logger;
    return &logger;
}

} // namespace kr2log

// spdlog-compatible surface used by the ported sources.
namespace spdlog {
inline kr2log::Logger *get(const char *) { return kr2log::get(); }
template <typename... A> inline void info(const char *f, const A &...a) { kr2log::get()->info(f, a...); }
template <typename... A> inline void warn(const char *f, const A &...a) { kr2log::get()->warn(f, a...); }
template <typename... A> inline void error(const char *f, const A &...a) { kr2log::get()->error(f, a...); }
template <typename... A> inline void debug(const char *f, const A &...a) { kr2log::get()->debug(f, a...); }
template <typename... A> inline void critical(const char *f, const A &...a) { kr2log::get()->critical(f, a...); }
} // namespace spdlog

// fmt::format subset and ASCII case helpers used by ported sources.
namespace fmt {
template <typename... A> using format_string = const char *;
template <typename... A> inline std::string format(const char *f, const A &...a) {
    return kr2log::format(f, a...);
}
} // namespace fmt

namespace kr2log {
inline std::string toUpperAscii(std::string s) {
    for(auto &c : s)
        if(c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}
} // namespace kr2log

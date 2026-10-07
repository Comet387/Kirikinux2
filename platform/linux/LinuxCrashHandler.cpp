// SPDX-License-Identifier: AGPL-3.0-only
//---------------------------------------------------------------------------
// Kirikinux2: fatal-signal reporter.
//
// An AppImage is a FUSE mount that disappears when the process dies, so
// systemd-coredump / gdb can neither read the executable's unwind tables nor
// its symbols afterwards and only one anonymous frame survives.  This handler
// unwinds while the process (and the mount) still exists and writes:
//   * every frame as  module+offset  (usable with addr2line -e <module>),
//   * the nearest symbol of the main executable read from its own .symtab
//     (the engine is linked non-PIE and is not stripped),
//   * optionally addr2line -Cfi output when binutils is installed.
// The report goes to stderr and to $XDG_DATA_HOME/kirikinux/crash.log
// (~/.local/share/kirikinux/crash.log).  Afterwards the default action is
// restored and the signal re-raised, so a core dump is still produced.
// Set KIRIKINUX_NO_CRASH_HANDLER=1 to disable it (e.g. when using gdb).
//---------------------------------------------------------------------------
#include <dlfcn.h>
#include <elf.h>
#include <execinfo.h>
#include <fcntl.h>
#include <link.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <initializer_list>

namespace {

constexpr int kMaxFrames = 96;
char g_logPath[1024];
char g_exePath[1024];
const unsigned char *g_exeMap = nullptr;
size_t g_exeSize = 0;
const Elf64_Sym *g_syms = nullptr;
size_t g_symCount = 0;
const char *g_strtab = nullptr;
size_t g_strtabSize = 0;
volatile sig_atomic_t g_inHandler = 0;
int g_logFd = -1;

void writeAll(int fd, const char *s, size_t n) {
    while(fd >= 0 && n > 0) {
        ssize_t w = ::write(fd, s, n);
        if(w <= 0) {
            if(w < 0 && errno == EINTR)
                continue;
            return;
        }
        s += w;
        n -= static_cast<size_t>(w);
    }
}

void out(const char *s) {
    const size_t n = std::strlen(s);
    writeAll(STDERR_FILENO, s, n);
    writeAll(g_logFd, s, n);
}

// async-signal-safe hex formatting
void hex(char *buf, uintptr_t v) {
    static const char digits[] = "0123456789abcdef";
    char tmp[2 + 16 + 1];
    int i = 0;
    do {
        tmp[i++] = digits[v & 0xf];
        v >>= 4;
    } while(v && i < 16);
    int p = 0;
    buf[p++] = '0';
    buf[p++] = 'x';
    while(i > 0)
        buf[p++] = tmp[--i];
    buf[p] = '\0';
}

void dec(char *buf, long v) {
    char tmp[24];
    int i = 0;
    bool neg = v < 0;
    unsigned long u = neg ? static_cast<unsigned long>(-v) : static_cast<unsigned long>(v);
    do {
        tmp[i++] = static_cast<char>('0' + u % 10);
        u /= 10;
    } while(u && i < 22);
    int p = 0;
    if(neg)
        buf[p++] = '-';
    while(i > 0)
        buf[p++] = tmp[--i];
    buf[p] = '\0';
}

// Map the executable once at start-up (not in the handler): its .symtab lets
// us name functions of the non-PIE engine without -rdynamic.
void loadExecutableSymbols() {
    ssize_t n = ::readlink("/proc/self/exe", g_exePath, sizeof(g_exePath) - 1);
    if(n <= 0)
        return;
    g_exePath[n] = '\0';
    int fd = ::open(g_exePath, O_RDONLY | O_CLOEXEC);
    if(fd < 0)
        return;
    struct stat st {};
    if(::fstat(fd, &st) != 0 || st.st_size < static_cast<off_t>(sizeof(Elf64_Ehdr))) {
        ::close(fd);
        return;
    }
    void *map = ::mmap(nullptr, static_cast<size_t>(st.st_size), PROT_READ, MAP_PRIVATE, fd, 0);
    ::close(fd);
    if(map == MAP_FAILED)
        return;
    auto *base = static_cast<const unsigned char *>(map);
    auto *eh = reinterpret_cast<const Elf64_Ehdr *>(base);
    if(std::memcmp(eh->e_ident, ELFMAG, SELFMAG) != 0 || eh->e_ident[EI_CLASS] != ELFCLASS64 ||
       eh->e_shoff == 0 || eh->e_shentsize != sizeof(Elf64_Shdr) ||
       eh->e_shoff + static_cast<size_t>(eh->e_shnum) * sizeof(Elf64_Shdr) > static_cast<size_t>(st.st_size)) {
        ::munmap(map, static_cast<size_t>(st.st_size));
        return;
    }
    auto *sh = reinterpret_cast<const Elf64_Shdr *>(base + eh->e_shoff);
    for(int i = 0; i < eh->e_shnum; ++i) {
        if(sh[i].sh_type != SHT_SYMTAB || sh[i].sh_link >= eh->e_shnum)
            continue;
        const Elf64_Shdr &str = sh[sh[i].sh_link];
        if(sh[i].sh_offset + sh[i].sh_size > static_cast<size_t>(st.st_size) ||
           str.sh_offset + str.sh_size > static_cast<size_t>(st.st_size))
            break;
        g_syms = reinterpret_cast<const Elf64_Sym *>(base + sh[i].sh_offset);
        g_symCount = sh[i].sh_size / sizeof(Elf64_Sym);
        g_strtab = reinterpret_cast<const char *>(base + str.sh_offset);
        g_strtabSize = str.sh_size;
        break;
    }
    g_exeMap = base;
    g_exeSize = static_cast<size_t>(st.st_size);
}

const char *exeSymbol(uintptr_t addr, uintptr_t *offset) {
    const Elf64_Sym *best = nullptr;
    for(size_t i = 0; i < g_symCount; ++i) {
        const Elf64_Sym &s = g_syms[i];
        if(ELF64_ST_TYPE(s.st_info) != STT_FUNC || s.st_value == 0 || s.st_name >= g_strtabSize)
            continue;
        if(addr >= s.st_value && (s.st_size ? addr < s.st_value + s.st_size : addr - s.st_value < 4096)) {
            if(!best || s.st_value > best->st_value)
                best = &s;
        }
    }
    if(!best)
        return nullptr;
    *offset = addr - best->st_value;
    return g_strtab + best->st_name;
}

void runAddr2line(void *const *frames, int count) {
    // addr2line is optional; the raw module+offset list above is the record.
    const char *candidates[] = {"/usr/bin/addr2line", "/bin/addr2line"};
    const char *tool = nullptr;
    for(const char *c : candidates)
        if(::access(c, X_OK) == 0) {
            tool = c;
            break;
        }
    if(!tool || !g_exePath[0])
        return;
    static char addrBuf[kMaxFrames][24];
    static const char *argv[kMaxFrames + 8];
    int a = 0;
    argv[a++] = tool;
    argv[a++] = "-C";
    argv[a++] = "-f";
    argv[a++] = "-i";
    argv[a++] = "-p";
    argv[a++] = "-e";
    argv[a++] = g_exePath;
    for(int i = 0; i < count && a < kMaxFrames + 7; ++i) {
        Dl_info info {};
        if(::dladdr(frames[i], &info) && info.dli_fname && g_exePath[0] &&
           std::strcmp(info.dli_fname, g_exePath) != 0 && info.dli_fbase &&
           reinterpret_cast<uintptr_t>(info.dli_fbase) != 0x400000)
            continue; // only addresses inside the (non-PIE) executable
        // return addresses point after the call; step back into it
        hex(addrBuf[i], reinterpret_cast<uintptr_t>(frames[i]) - (i ? 1 : 0));
        argv[a++] = addrBuf[i];
    }
    argv[a] = nullptr;
    if(a == 7)
        return;
    out("---- addr2line ----\n");
    int pipefd[2];
    if(::pipe(pipefd) != 0)
        return;
    pid_t pid = ::fork();
    if(pid == 0) {
        ::dup2(pipefd[1], STDOUT_FILENO);
        ::dup2(pipefd[1], STDERR_FILENO);
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        // The launcher points LD_LIBRARY_PATH at the bundled libraries; the
        // host's addr2line must use the host's libbfd/libz, so drop it.
        static const char *const envp[] = {"PATH=/usr/bin:/bin", "LC_ALL=C", nullptr};
        ::execve(tool, const_cast<char *const *>(argv), const_cast<char *const *>(envp));
        ::_exit(127);
    }
    ::close(pipefd[1]);
    if(pid < 0) {
        ::close(pipefd[0]);
        return;
    }
    char buf[4096];
    ssize_t r;
    while((r = ::read(pipefd[0], buf, sizeof(buf))) > 0) {
        writeAll(STDERR_FILENO, buf, static_cast<size_t>(r));
        writeAll(g_logFd, buf, static_cast<size_t>(r));
    }
    ::close(pipefd[0]);
    int status = 0;
    ::waitpid(pid, &status, 0);
}

void handler(int sig, siginfo_t *info, void *) {
    if(g_inHandler) {
        ::signal(sig, SIG_DFL);
        ::raise(sig);
        return;
    }
    g_inHandler = 1;
    if(g_logPath[0])
        g_logFd = ::open(g_logPath, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);

    char num[40];
    out("\n==== Kirikinux2 crashed: signal ");
    dec(num, sig);
    out(num);
    out(" (");
    out(sig == SIGSEGV ? "SIGSEGV" : sig == SIGBUS ? "SIGBUS" : sig == SIGFPE ? "SIGFPE"
        : sig == SIGILL ? "SIGILL" : sig == SIGABRT ? "SIGABRT" : "?");
    out(") fault address ");
    hex(num, info ? reinterpret_cast<uintptr_t>(info->si_addr) : 0);
    out(num);
    out(" time ");
    dec(num, static_cast<long>(::time(nullptr)));
    out(num);
    out(" ====\n");

    void *frames[kMaxFrames];
    const int count = ::backtrace(frames, kMaxFrames);
    for(int i = 0; i < count; ++i) {
        const uintptr_t pc = reinterpret_cast<uintptr_t>(frames[i]);
        out("#");
        dec(num, i);
        out(num);
        out("  ");
        hex(num, pc);
        out(num);
        Dl_info dl {};
        if(::dladdr(frames[i], &dl) && dl.dli_fname) {
            out("  ");
            out(dl.dli_fname);
            out("+");
            const uintptr_t base = reinterpret_cast<uintptr_t>(dl.dli_fbase);
            // non-PIE executable: addresses are absolute, base 0x400000
            hex(num, pc - (base == 0x400000 ? 0 : base));
            out(num);
        }
        uintptr_t off = 0;
        const char *name = exeSymbol(pc, &off);
        if(!name && dl.dli_sname) {
            name = dl.dli_sname;
            off = pc - reinterpret_cast<uintptr_t>(dl.dli_saddr);
        }
        if(name) {
            out("  ");
            out(name);
            out("+");
            hex(num, off);
            out(num);
        }
        out("\n");
    }
    runAddr2line(frames, count);
    if(g_logPath[0]) {
        out("==== report written to ");
        out(g_logPath);
        out(" ====\n");
    }
    if(g_logFd >= 0)
        ::close(g_logFd);

    struct sigaction dfl {};
    dfl.sa_handler = SIG_DFL;
    sigemptyset(&dfl.sa_mask);
    ::sigaction(sig, &dfl, nullptr);
    ::raise(sig);
}

} // namespace

void TVPLinuxInstallCrashHandler() {
    const char *off = std::getenv("KIRIKINUX_NO_CRASH_HANDLER");
    if(off && *off && std::strcmp(off, "0") != 0)
        return;

    const char *data = std::getenv("XDG_DATA_HOME");
    const char *home = std::getenv("HOME");
    char dir[900] = {0};
    if(data && *data)
        std::snprintf(dir, sizeof(dir), "%s/kirikinux", data);
    else if(home && *home)
        std::snprintf(dir, sizeof(dir), "%s/.local/share/kirikinux", home);
    if(dir[0]) {
        ::mkdir(dir, 0755); // parent normally exists; failure only disables the file copy
        std::snprintf(g_logPath, sizeof(g_logPath), "%s/crash.log", dir);
    }

    loadExecutableSymbols();
    // backtrace() loads libgcc_s lazily (malloc); do it now, not in the handler.
    void *warm[2];
    ::backtrace(warm, 2);

    static char altstack[256 * 1024];
    stack_t ss {};
    ss.ss_sp = altstack;
    ss.ss_size = sizeof(altstack);
    ::sigaltstack(&ss, nullptr);

    struct sigaction sa {};
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    for(int sig : {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT})
        ::sigaction(sig, &sa, nullptr);
}

// SPDX-License-Identifier: AGPL-3.0-only
// Host applications must use the host's libraries, not AppImage's runtime.
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <map>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <vector>
extern char **environ;

bool KR2LinuxOpenURL(const std::string &url) {
    if (url.empty() || url[0] == '-' || url.find('\0') != std::string::npos) return false;
    std::map<std::string, std::string> values;
    for (char **p = environ; *p; ++p) {
        const char *separator = std::strchr(*p, '=');
        if (separator) values.emplace(std::string(*p, separator - *p), separator + 1);
    }
    if (values.count("KIRIKINUX_HOST_ENV_SAVED")) {
        for (const char *name : {"LD_LIBRARY_PATH", "LD_PRELOAD", "FONTCONFIG_FILE", "FONTCONFIG_PATH",
                               "GTK_PATH", "GTK_MODULES", "GIO_EXTRA_MODULES"}) {
            const std::string key = std::string("KIRIKINUX_HOST_") + name;
            auto saved = values.find(key);
            if (saved == values.end()) values.erase(name);
            else values[name] = saved->second;
        }
    } else {
        // A directly executed engine has no launcher snapshot. Avoid exporting
        // its private loader/font configuration to host desktop helpers.
        for (const char *name : {"LD_LIBRARY_PATH", "LD_PRELOAD", "FONTCONFIG_FILE", "FONTCONFIG_PATH"})
            values.erase(name);
    }
    for (auto it = values.begin(); it != values.end(); ) {
        if (it->first.compare(0, 15, "KIRIKINUX_HOST_") == 0) it = values.erase(it);
        else ++it;
    }
    std::vector<std::string> strings;
    for (const auto &value : values) strings.push_back(value.first + "=" + value.second);
    std::vector<char*> environment;
    for (auto &value : strings) environment.push_back(&value[0]);
    environment.push_back(nullptr);
    char program[] = "xdg-open";
    char *args[] = {program, const_cast<char*>(url.c_str()), nullptr};
    pid_t child;
    if (posix_spawnp(&child, program, nullptr, nullptr, args, environment.data()) != 0) return false;
    int status;
    while (waitpid(child, &status, 0) < 0) if (errno != EINTR) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

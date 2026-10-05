// SPDX-License-Identifier: AGPL-3.0-only
#include <string>
bool KR2LinuxOpenURL(const std::string &);
int main(int argc, char **argv) { return argc == 2 && KR2LinuxOpenURL(argv[1]) ? 0 : 1; }

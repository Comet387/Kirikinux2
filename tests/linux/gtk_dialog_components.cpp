#include <cstdlib>
#include <string>
#include <vector>

int KR2LinuxMessageBox(const std::string &, const std::string &, const std::vector<std::string> &);
int KR2LinuxInputBox(std::string &, const std::string &, const std::string &, const std::vector<std::string> &);

// Linked to real GTK. With DISPLAY unset, both entry points must return the
// documented "no display" result without creating a dialog or changing input.
int main() {
    const std::vector<std::string> buttons{"OK", "Cancel"};
    if (KR2LinuxMessageBox("test", "test", buttons) != -2) return EXIT_FAILURE;
    std::string text = "unchanged";
    if (KR2LinuxInputBox(text, "test", "test", buttons) != -2) return EXIT_FAILURE;
    return text == "unchanged" ? EXIT_SUCCESS : EXIT_FAILURE;
}

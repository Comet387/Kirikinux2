// SPDX-License-Identifier: GPL-2.0-or-later
//
// A deliberately small native Linux window/input target.  It is useful as a
// build and packaging smoke test while the Android-specific host layer is being
// replaced.  It does not execute TJS/KAG scripts yet; the original parser and
// archive code remain in src/core and are not silently replaced here.

#include <SDL2/SDL.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace {

constexpr int kWindowWidth = 960;
constexpr int kWindowHeight = 640;

struct LaunchInfo {
    std::string path;
    std::string kind;
};

bool HasStartupScript(const fs::path& path) {
    std::error_code ec;
    return fs::is_directory(path, ec) && fs::is_regular_file(path / "startup.tjs", ec);
}

bool HasXP3Signature(const fs::path& path) {
    static constexpr std::array<unsigned char, 11> kXP3 = {
        0x58, 0x50, 0x33, 0x0d, 0x0a, 0x20, 0x0a, 0x1a, 0x8b, 0x67, 0x01
    };
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::array<unsigned char, kXP3.size()> header{};
    input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    return input.gcount() == static_cast<std::streamsize>(header.size()) && header == kXP3;
}

LaunchInfo InspectPath(const std::string& arg) {
    LaunchInfo result{arg, "unknown path"};
    std::error_code ec;
    const fs::path path(arg);
    if (!fs::exists(path, ec)) {
        result.kind = "missing path";
    } else if (HasStartupScript(path)) {
        result.kind = "KAG directory (startup.tjs)";
    } else if (fs::is_regular_file(path, ec) && HasXP3Signature(path)) {
        result.kind = "XP3 archive";
    } else if (fs::is_directory(path, ec)) {
        result.kind = "directory";
    } else if (fs::is_regular_file(path, ec)) {
        result.kind = "file";
    }
    return result;
}

void PrintUsage(const char* program) {
    std::cout << "Kirikinux2 Linux native frontend " << KIRIKINUX_VERSION << "\n"
              << "Usage: " << program << " [GAME-DIRECTORY|GAME.XP3]\n\n"
              << "The window/input frontend is native SDL2 and works with X11 or\n"
              << "Wayland through SDL2. Script execution is provided by the\n"
              << "ongoing core port; this binary reports the selected game path.\n";
}

void Draw(SDL_Renderer* renderer, const LaunchInfo& launch, bool has_focus) {
    // Keep rendering intentionally dependency-free: no font or image assets are
    // needed for the smoke test, and SDL's X11 backend remains the only window
    // system dependency.
    SDL_SetRenderDrawColor(renderer, 19, 22, 29, 255);
    SDL_RenderClear(renderer);

    SDL_Rect header{0, 0, kWindowWidth, 72};
    SDL_SetRenderDrawColor(renderer, 40, 53, 74, 255);
    SDL_RenderFillRect(renderer, &header);

    SDL_Rect accent{24, 24, 24, 24};
    SDL_SetRenderDrawColor(renderer, 105, 176, 255, 255);
    SDL_RenderFillRect(renderer, &accent);

    SDL_Rect status{24, 120, kWindowWidth - 48, 74};
    SDL_SetRenderDrawColor(renderer, has_focus ? 35 : 28, 39, 52, 255);
    SDL_RenderFillRect(renderer, &status);

    // A pair of bars provides a visible, deterministic rendering check without
    // introducing SDL_ttf as another runtime dependency.
    SDL_Rect title_bar{48, 138, 230, 8};
    SDL_Rect detail_bar{48, 162, 460, 8};
    SDL_SetRenderDrawColor(renderer, 214, 225, 240, 255);
    SDL_RenderFillRect(renderer, &title_bar);
    SDL_SetRenderDrawColor(renderer, 112, 132, 158, 255);
    SDL_RenderFillRect(renderer, &detail_bar);

    SDL_RenderPresent(renderer);
    (void)launch;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        PrintUsage(argv[0]);
        return 0;
    }
    if (argc > 1 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << KIRIKINUX_VERSION << '\n';
        return 0;
    }

    LaunchInfo launch;
    if (argc > 1) {
        launch = InspectPath(argv[1]);
    } else {
        launch = {"", "no game selected (drop a directory or XP3 archive)"};
    }
    std::cout << "Kirikinux2 Linux: " << launch.kind;
    if (!launch.path.empty()) std::cout << " — " << launch.path;
    std::cout << '\n';

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    SDL_Window* window = SDL_CreateWindow(
        "Kirikinux2 Linux (native SDL2)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kWindowWidth, kWindowHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_StartTextInput();
    SDL_Event event{};
    bool running = true;
    bool focused = true;
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) focused = false;
                    if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) focused = true;
                    break;
                case SDL_DROPFILE:
                    if (event.drop.file) {
                        launch = InspectPath(event.drop.file);
                        std::cout << "Selected " << launch.kind << ": " << launch.path << '\n';
                        SDL_free(event.drop.file);
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE ||
                        (event.key.keysym.sym == SDLK_q && (event.key.keysym.mod & KMOD_CTRL))) {
                        running = false;
                    } else if (event.key.keysym.sym == SDLK_F11) {
                        const Uint32 mode = SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP;
                        SDL_SetWindowFullscreen(window, mode ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
                    }
                    break;
                default:
                    break;
            }
        }
        Draw(renderer, launch, focused);
        SDL_Delay(16);
    }
    SDL_StopTextInput();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

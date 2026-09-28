cmake_minimum_required(VERSION 3.16)

project(Kirikiroid2Linux
    VERSION 1.3.9
    DESCRIPTION "Native Linux frontend for Kirikiroid2"
    LANGUAGES CXX
)

option(KIRIKIROID2_ENABLE_LEGACY_CORE
    "Build the historical Kirikiroid2 core when its unvendored dependencies are present"
    OFF
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# The upstream repository contains the Android/Cocos2d source and deliberately does
# not vendor Cocos2d-x, FFmpeg, FreeType, etc.  The native target therefore starts
# with a small SDL2 frontend.  Keeping this target independent makes it possible to
# build and test the Linux window/input path while the full engine is ported.
find_package(SDL2 2.0.5 CONFIG QUIET)
if (NOT SDL2_FOUND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2>=2.0.5)
    set(KR2_SDL_TARGET PkgConfig::SDL2)
elseif (TARGET SDL2::SDL2)
    set(KR2_SDL_TARGET SDL2::SDL2)
elseif (TARGET SDL2::SDL2-static)
    set(KR2_SDL_TARGET SDL2::SDL2-static)
else()
    message(FATAL_ERROR "SDL2 was found but did not export SDL2::SDL2 or SDL2::SDL2-static")
endif()

add_executable(kirikiroid2-linux
    src/main.cpp
)

target_link_libraries(kirikiroid2-linux PRIVATE ${KR2_SDL_TARGET})
target_compile_features(kirikiroid2-linux PRIVATE cxx_std_17)
target_compile_definitions(kirikiroid2-linux PRIVATE
    KIRIKIROID2_VERSION=\"${PROJECT_VERSION}\"
)

if (UNIX AND NOT APPLE)
    include(GNUInstallDirs)
endif()

install(TARGETS kirikiroid2-linux
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/../LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/kirikiroid2"
)
install(FILES
    "${CMAKE_CURRENT_LIST_DIR}/README.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/doc/kirikiroid2"
)

set(CPACK_PACKAGE_NAME "kirikiroid2-linux")
set(CPACK_PACKAGE_VENDOR "Kirikiroid2 contributors")
set(CPACK_PACKAGE_CONTACT "https://github.com/zeas2/Kirikiroid2")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Linux SDL2 frontend for Kirikiroid2")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_LIST_DIR}/../LICENSE")
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_SECTION "games")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0")
include(CPack)

if (KIRIKIROID2_ENABLE_LEGACY_CORE)
    message(FATAL_ERROR
        "The historical core is not wired into this target yet. The upstream tree "
        "does not include its Cocos2d-x/FFmpeg/vendor submodules; see README.md.")
endif()

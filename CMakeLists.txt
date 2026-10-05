# Kirikinux2 - Linux desktop build of the ORIGINAL Kirikiroid2 engine.
#
# src/core and src/plugins are compiled the way src/core/Android.mk and
# src/plugins/Android.mk compile them; only the platform layer differs:
#   environ/android/AndroidUtils.cpp  ->  platform/linux/LinuxUtils.cpp
#   project/android/jni/src/SDL_android_main.cpp  ->  platform/linux/LinuxMain.cpp
#   zeas2's private cocos2d-x  ->  stock cocos2d-x 3.17.2 + scripts/linux/patch-cocos2dx.py
# See docs/linux-port.md.  Android keeps building with ndk-build (project/android).
cmake_minimum_required(VERSION 3.13)
project(kirikinux2 VERSION 1.3.9 LANGUAGES C CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "This CMake project builds the Linux desktop host only.")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

set(KR2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(KR2_THIRD_PARTY "${KR2_ROOT}/third_party" CACHE PATH "Sources fetched by scripts/linux/fetch-thirdparty.sh")
set(KR2_COCOS2DX_ROOT "${KR2_THIRD_PARTY}/cocos2d-x" CACHE PATH "cocos2d-x 3.17.2 source tree")
option(KR2_WHOLE_ARCHIVE "Link krkr2core/krkr2plugin with --whole-archive (as project/android/jni/Android.mk)" ON)
option(KR2_WRAP_MALLOC "Link with --wrap=malloc/free/realloc/calloc; Application.cpp defines HOOK_MALLOC (as on Android)" ON)
option(KR2_PERMISSIVE "Relax GCC diagnostics the 2016-era sources trip over (-fpermissive, -Wno-narrowing)" ON)
option(KR2_BUILD_UI_RESOURCES "Publish cocos/kr2/cocosstudio/*.csd to .csb at build time" ON)
option(KR2_WITH_JXR "Build visual/LoadJXR.cpp against jxrlib (libjxr-dev)" OFF)
option(KR2_FORCE_BUNDLED_UNRAR "Use third_party/unrar even if the system has libunrar" OFF)
option(KR2_BUNDLE_FFMPEG "Copy the six linked FFmpeg shared libraries into the install tree" ON)
set(KR2_DEFAULT_FONT "" CACHE FILEPATH "CJK font copied to Resources/DroidSansFallback.ttf (auto-detected if empty)")

# Old C sources (minizip, 7-Zip, encoding tables) predate -fno-common and GCC 14's
# stricter defaults; old C++ sources (cocos2d-x 3.17, krkr) predate implicit <limits>.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fcommon -Wno-error=implicit-function-declaration -Wno-error=incompatible-pointer-types -Wno-error=int-conversion")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include limits")

# ------------------------------------------------------------------ cocos2d-x
find_package(Python3 COMPONENTS Interpreter REQUIRED)
if(NOT EXISTS "${KR2_COCOS2DX_ROOT}/cocos/cocos2d.h")
  message(FATAL_ERROR "cocos2d-x not found in ${KR2_COCOS2DX_ROOT}: run scripts/linux/fetch-thirdparty.sh "
                      "(or pass -DKR2_COCOS2DX_ROOT=<cocos2d-x-3.17.2>)")
endif()
# Kirikiroid2 was built against a privately modified cocos2d-x; re-create the API
# differences it relies on (idempotent, see the script for the list).
execute_process(
  COMMAND "${Python3_EXECUTABLE}" "${KR2_ROOT}/scripts/linux/patch-cocos2dx.py" "${KR2_COCOS2DX_ROOT}"
  RESULT_VARIABLE _kr2_patch_result)
if(NOT _kr2_patch_result EQUAL 0)
  message(FATAL_ERROR "scripts/linux/patch-cocos2dx.py failed on ${KR2_COCOS2DX_ROOT}")
endif()

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_LUA_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_JS_LIBS OFF CACHE BOOL "" FORCE)
set(COCOS2DX_ROOT_PATH "${KR2_COCOS2DX_ROOT}")
list(APPEND CMAKE_MODULE_PATH "${COCOS2DX_ROOT_PATH}/cmake/Modules/")
include(CocosBuildSet)
if(NOT ENGINE_BINARY_PATH)
  set(ENGINE_BINARY_PATH "${PROJECT_BINARY_DIR}/engine")
endif()
add_subdirectory("${COCOS2DX_ROOT_PATH}/cocos" "${ENGINE_BINARY_PATH}/cocos/core")

set(KR2_COCOS_INCLUDE_DIRS
  "${KR2_COCOS2DX_ROOT}"
  "${KR2_COCOS2DX_ROOT}/cocos"
  "${KR2_COCOS2DX_ROOT}/cocos/platform"          # "CCFileUtils.h" (as in Android.mk)
  "${KR2_COCOS2DX_ROOT}/cocos/editor-support"
  "${KR2_COCOS2DX_ROOT}/cocos/audio/include"
  "${KR2_COCOS2DX_ROOT}/extensions"
  "${KR2_COCOS2DX_ROOT}/external")

# ------------------------------------------------------- third party / sources
include(cmake/Kr2ThirdParty.cmake)
include(cmake/Kr2Sources.cmake)

set(KR2_COMMON_OPTIONS
  -fno-delete-null-pointer-checks   # tjs2 relies on 'this'/Ptr null checks (see docs)
  -Wno-deprecated-declarations
  -Wno-unused-result
  $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-Wno-inconsistent-missing-override>)
if(KR2_PERMISSIVE)
  list(APPEND KR2_COMMON_OPTIONS
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-fpermissive>
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-Wno-narrowing>
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-Wno-c++11-narrowing>)
endif()
# same definitions as src/core/Android.mk (LOCAL_CPPFLAGS / LOCAL_CFLAGS)
set(KR2_COMMON_DEFINITIONS TJS_TEXT_OUT_CRLF __STDC_CONSTANT_MACROS USE_UNICODE_FSTRING _7ZIP_ST LINUX)

# ------------------------------------------------------------------ krkr2core
add_library(krkr2core STATIC ${KR2_CORE_SOURCES} ${KR2_PLATFORM_SOURCES})
set_target_properties(krkr2core PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(krkr2core PUBLIC
  ${KR2_CORE_INCLUDE_DIRS} ${KR2_COCOS_INCLUDE_DIRS} ${KR2_CODEC_INCLUDE_DIRS} ${OpenCV_INCLUDE_DIRS})
target_compile_definitions(krkr2core PUBLIC ${KR2_COMMON_DEFINITIONS})
target_compile_options(krkr2core PRIVATE ${KR2_COMMON_OPTIONS})
target_link_libraries(krkr2core PUBLIC
  cocos2d
  kr2_7zip kr2_unrar
  PkgConfig::KR2_FFMPEG PkgConfig::KR2_SDL2 PkgConfig::KR2_OPENAL PkgConfig::KR2_CODECS
  PkgConfig::KR2_ONIG PkgConfig::KR2_ARCHIVE PkgConfig::KR2_LZ4 PkgConfig::KR2_JPEG
  ${KR2_CODEC_LIBS} ${OpenCV_LIBS}
  ZLIB::ZLIB Threads::Threads ${CMAKE_DL_LIBS})
set_source_files_properties(${KR2_FFMPEG_USERS} PROPERTIES
  COMPILE_OPTIONS "-include;${KR2_ROOT}/platform/linux/compat/kr2_ffmpeg_compat.h")
if(KR2_GTK3_FOUND)
  pkg_check_modules(KR2_FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
  # Imported-target usage requirements supply GTK's includes and compile flags.
  # Do not copy KR2_GTK3_INCLUDE_DIRS into a source-file property: that bypasses
  # CMake's implicit-include filtering and can emit -isystem /usr/include before
  # libstdc++ headers. <cstdlib>'s #include_next then cannot find stdlib.h.
  set_source_files_properties(${KR2_ROOT}/platform/linux/LinuxDialogs.cpp PROPERTIES
    COMPILE_DEFINITIONS KR2_LINUX_HAVE_GTK3=1)
  target_link_libraries(krkr2core PUBLIC PkgConfig::KR2_GTK3 PkgConfig::KR2_FONTCONFIG)
else()
  message(WARNING "gtk+-3.0 not found: message/input boxes fall back to stderr")
endif()
if(KR2_WITH_JXR)
  target_compile_definitions(krkr2core PRIVATE KR2_HAVE_JXR=1)
  target_include_directories(krkr2core PRIVATE "${KR2_JXR_INCLUDE_DIR}")
  target_link_libraries(krkr2core PUBLIC "${KR2_JXR_GLUE}" "${KR2_JXR_LIB}")
endif()

# ---------------------------------------------------------------- krkr2plugin
# Takes krkr2core's usage requirements without linking it, so the --whole-archive
# group below never contains libkrkr2core.a twice.
add_library(krkr2plugin STATIC ${KR2_PLUGIN_SOURCES})
set_target_properties(krkr2plugin PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(krkr2plugin PRIVATE
  ${KR2_ROOT}/src/plugins ${KR2_ROOT}/src/plugins/ncbind
  $<TARGET_PROPERTY:krkr2core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(krkr2plugin PRIVATE
  $<TARGET_PROPERTY:krkr2core,INTERFACE_COMPILE_DEFINITIONS>)
target_compile_options(krkr2plugin PRIVATE ${KR2_COMMON_OPTIONS})
add_dependencies(krkr2plugin krkr2core)

# ------------------------------------------------------------ krkr2plugin_ext
# Plugins ported from the KrKr2 emulator (github.com/2468785842/krkr2, KiriKiri
# BSD-style licence, see src/plugins/ext/README.md).  They need C++17, so they
# live in their own archive; it is always linked --whole-archive because the
# ncbind self-registration objects are otherwise dropped by the linker.
file(GLOB_RECURSE KR2_PLUGIN_EXT_SOURCES CONFIGURE_DEPENDS ${KR2_ROOT}/src/plugins/ext/*.cpp)
add_library(krkr2plugin_ext STATIC ${KR2_PLUGIN_EXT_SOURCES})
set_target_properties(krkr2plugin_ext PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON POSITION_INDEPENDENT_CODE ON)
target_include_directories(krkr2plugin_ext PRIVATE
  ${KR2_ROOT}/src/plugins ${KR2_ROOT}/src/plugins/ncbind ${KR2_ROOT}/src/plugins/ext
  $<TARGET_PROPERTY:krkr2core,INTERFACE_INCLUDE_DIRECTORIES>)
target_compile_definitions(krkr2plugin_ext PRIVATE
  $<TARGET_PROPERTY:krkr2core,INTERFACE_COMPILE_DEFINITIONS>)
target_compile_options(krkr2plugin_ext PRIVATE ${KR2_COMMON_OPTIONS})
add_dependencies(krkr2plugin_ext krkr2core)

# ----------------------------------------------------------------- executable
set(KR2_BIN_DIR "${CMAKE_BINARY_DIR}/bin")
set(KR2_RES_DIR "${KR2_BIN_DIR}/Resources") # cocos2d-x's default resource root on Linux

add_executable(kirikinux ${KR2_ROOT}/platform/linux/LinuxMain.cpp)
add_library(kr2_legacy_libm OBJECT platform/linux/compat/legacy_libm.c)
target_sources(kirikinux PRIVATE $<TARGET_OBJECTS:kr2_legacy_libm>)
# Cocos 3.17's Linux prebuilt FreeType archive contains absolute relocations.
# Ubuntu enables PIE by default; these legacy archives require a non-PIE host.
# Keep Cocos' old static FreeType private. GTK/Pango must resolve FT_* against
# their system shared FreeType rather than interposing the bundled copy.
target_link_options(kirikinux PRIVATE -no-pie -Wl,--exclude-libs,libfreetype.a)
set_target_properties(kirikinux PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON
  RUNTIME_OUTPUT_DIRECTORY "${KR2_BIN_DIR}" INSTALL_RPATH "$ORIGIN/lib")
target_include_directories(kirikinux PRIVATE ${KR2_CORE}/environ/cocos2d)
target_compile_options(kirikinux PRIVATE ${KR2_COMMON_OPTIONS})
if(KR2_WHOLE_ARCHIVE)
  # LOCAL_WHOLE_STATIC_LIBRARIES := kr2plugin krkr2 ... : keeps self-registering plugins/codecs
  target_link_libraries(kirikinux PRIVATE -Wl,--whole-archive krkr2plugin krkr2plugin_ext krkr2core -Wl,--no-whole-archive)
else()
  target_link_libraries(kirikinux PRIVATE krkr2plugin -Wl,--whole-archive krkr2plugin_ext -Wl,--no-whole-archive)
endif()
target_link_libraries(kirikinux PRIVATE krkr2core)
if(NOT KR2_WRAP_MALLOC)
  target_compile_definitions(krkr2core PRIVATE KR2_NO_MALLOC_WRAP=1)
endif()
if(KR2_WRAP_MALLOC)
  # project/android/jni/Android.mk: -Wl,--wrap=malloc ... (Application.cpp: HOOK_MALLOC)
  target_link_options(kirikinux PRIVATE -Wl,--wrap=malloc -Wl,--wrap=free -Wl,--wrap=realloc -Wl,--wrap=calloc)
endif()

# ------------------------------------------------------------------ resources
file(GLOB KR2_STUDIO_TOP_PNG "${KR2_ROOT}/cocos/kr2/cocosstudio/*.png")
add_custom_target(kr2_resources ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${KR2_RES_DIR}/res/img"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${KR2_ROOT}/cocos/kr2/Resources" "${KR2_RES_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${KR2_ROOT}/cocos/kr2/cocosstudio/img" "${KR2_RES_DIR}/res/img"
  COMMENT "Kirikinux2 UI resources -> ${KR2_RES_DIR}"
  VERBATIM)
if(KR2_STUDIO_TOP_PNG)
  add_custom_command(TARGET kr2_resources POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${KR2_STUDIO_TOP_PNG} "${KR2_RES_DIR}/res/" VERBATIM)
endif()
if(KR2_DEFAULT_FONT)
  add_custom_command(TARGET kr2_resources POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${KR2_DEFAULT_FONT}" "${KR2_RES_DIR}/DroidSansFallback.ttf" VERBATIM)
else()
  message(FATAL_ERROR "No usable UI font found. Restore the bundled font or set KR2_DEFAULT_FONT to a complete CJK font.")
endif()
add_executable(kr2_check_resources platform/linux/tools/kr2_check_resources.cpp)
target_link_libraries(kr2_check_resources PRIVATE ext_freetype ZLIB::ZLIB)
target_link_options(kr2_check_resources PRIVATE -no-pie)
add_dependencies(kr2_resources kr2_check_resources)
add_custom_command(TARGET kr2_resources POST_BUILD
  COMMAND $<TARGET_FILE:kr2_check_resources> "${KR2_RES_DIR}/DroidSansFallback.ttf" "${KR2_RES_DIR}/default.cur"
  COMMENT "Checking CJK/Latin glyph coverage and the packaged cursor" VERBATIM)
if(KR2_BUILD_UI_RESOURCES)
  add_executable(kr2_csd2csb ${KR2_ROOT}/platform/linux/tools/kr2_csd2csb.cpp)
  target_sources(kr2_csd2csb PRIVATE $<TARGET_OBJECTS:kr2_legacy_libm>)
  target_link_options(kr2_csd2csb PRIVATE -no-pie)
  target_include_directories(kr2_csd2csb PRIVATE ${KR2_COCOS_INCLUDE_DIRS})
  # Ubuntu's X11 development package may expose libX11 as a static archive;
  # keep its XCB dependency explicit for the standalone converter tool.
  target_link_libraries(kr2_csd2csb PRIVATE cocos2d xcb)
  add_dependencies(kr2_resources kr2_csd2csb)
  add_custom_command(TARGET kr2_resources POST_BUILD
    COMMAND $<TARGET_FILE:kr2_csd2csb> "${KR2_ROOT}/cocos/kr2/cocosstudio" "${KR2_RES_DIR}/res"
    COMMENT "Publishing Cocos Studio layouts (.csd -> .csb)" VERBATIM)
endif()
add_dependencies(kirikinux kr2_resources)
if(UNIX)
  # Keep the historical target for existing scripts while exposing the
  # canonical Kirikinux2 executable name to new users.
  add_custom_command(TARGET kirikinux POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rm -f "${KR2_BIN_DIR}/kirikinux2"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "$<TARGET_FILE_NAME:kirikinux>" "${KR2_BIN_DIR}/kirikinux2"
    VERBATIM)
endif()

# ------------------------------------------------------------------- install
include(GNUInstallDirs)
set(KR2_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/kirikinux")
install(TARGETS kirikinux RUNTIME DESTINATION "${KR2_INSTALL_DIR}")
get_target_property(KR2_FMOD_LIBRARY ext_fmod IMPORTED_LOCATION)
# The bundled Cocos binary's SONAME is libfmod.so.6, unlike its source filename.
install(FILES "${KR2_FMOD_LIBRARY}" DESTINATION "${KR2_INSTALL_DIR}/lib" RENAME libfmod.so.6)
if(KR2_BUNDLE_FFMPEG)
  foreach(avlib avcodec avfilter avformat avutil swresample swscale)
    foreach(libdir IN LISTS KR2_FFMPEG_LIBRARY_DIRS)
      file(GLOB avfiles "${libdir}/lib${avlib}.so*")
      if(avfiles)
        install(FILES ${avfiles} DESTINATION "${KR2_INSTALL_DIR}/lib")
        break()
      endif()
    endforeach()
  endforeach()
endif()
install(DIRECTORY "${KR2_RES_DIR}" DESTINATION "${KR2_INSTALL_DIR}")
install(FILES platform/linux/kirikinux.desktop DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" RENAME kirikinux2.desktop)
install(FILES icons/kirikinux-linux.png DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps" RENAME kirikinux2.png)
file(RELATIVE_PATH KR2_REL_RUNTIME "/${CMAKE_INSTALL_BINDIR}" "/${KR2_INSTALL_DIR}")
configure_file(platform/linux/kirikinux-launcher.in "${CMAKE_BINARY_DIR}/kirikinux-launcher" @ONLY)
install(PROGRAMS "${CMAKE_BINARY_DIR}/kirikinux-launcher" DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME kirikinux)
install(PROGRAMS "${CMAKE_BINARY_DIR}/kirikinux-launcher" DESTINATION "${CMAKE_INSTALL_BINDIR}" RENAME kirikinux2)

# Headless checks for plugins ported from KrKr2 (no Cocos/GL/FFmpeg needed).
#   cmake -S tests/plugins -B build-plugin-tests && cmake --build build-plugin-tests
#   ./build-plugin-tests/psb_harness sample.scn 'root.scenes.count'
cmake_minimum_required(VERSION 3.16)
project(kirikinux2_plugin_tests CXX)
enable_testing()
get_filename_component(KR2_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(C ${KR2_ROOT}/src/core)
file(GLOB TJS_SOURCES ${C}/tjs2/*.cpp)
list(FILTER TJS_SOURCES EXCLUDE REGEX "/tjsRegExp\\.cpp$")
add_library(harness_tjs STATIC ${TJS_SOURCES} ${KR2_ROOT}/linux/tjs_platform.cpp)
target_compile_definitions(harness_tjs PUBLIC TJS_TEXT_OUT_CRLF __STDC_CONSTANT_MACROS USE_UNICODE_FSTRING TJS_NO_REGEXP=1 LINUX)
target_include_directories(harness_tjs PUBLIC ${KR2_ROOT}/platform/linux/compat ${C} ${C}/tjs2 ${C}/base ${C}/base/win32 ${C}/environ ${C}/environ/win32 ${C}/msg ${C}/msg/win32 ${C}/utils ${C}/visual ${C}/visual/win32 ${C}/sound ${C}/extension ${KR2_ROOT}/src/plugins ${KR2_ROOT}/src/plugins/ncbind ${KR2_ROOT}/src/plugins/ext)
set_target_properties(harness_tjs PROPERTIES CXX_STANDARD 14 CXX_EXTENSIONS ON)
target_compile_options(harness_tjs PRIVATE -w)

set(P ${KR2_ROOT}/src/plugins/ext/psbfile)
add_executable(psb_harness psb_harness.cpp harness_memstream.cpp harness_binaccessor.cpp
  ${P}/PSBFile.cpp ${P}/PSBValue.cpp ${P}/PSBOffsetReader.cpp ${P}/PSBStreamDecompress.cpp ${P}/PSBLz4Stream.cpp
  ${P}/types/PimgType.cpp ${P}/types/MotionType.cpp ${P}/types/ScnType.cpp ${P}/types/ImageType.cpp
  ${P}/types/MmoType.cpp ${P}/types/ArchiveType.cpp ${P}/types/SoundArchiveType.cpp ${P}/resources/ImageMetadata.cpp)
set_target_properties(psb_harness PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
find_package(ZLIB REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LZ4 REQUIRED IMPORTED_TARGET liblz4)
target_link_libraries(psb_harness PRIVATE harness_tjs ZLIB::ZLIB PkgConfig::LZ4 pthread)

# Disassembler for compiled game scripts (analysis tool, not shipped).
add_executable(tjs_disasm tjs_disasm.cpp)
target_link_libraries(tjs_disasm PRIVATE harness_tjs pthread)

# Small compatibility plugins (and the win32dialog.dll shim) through the real ncbind registration path.
set(E ${KR2_ROOT}/src/plugins/ext/compat)
add_executable(compat_harness compat_harness.cpp ${KR2_ROOT}/src/plugins/ncbind/ncbind.cpp
  ${E}/getLangName.cpp ${E}/layerExSave.cpp ${E}/win32ole.cpp ${E}/PackinOne.cpp
  ${KR2_ROOT}/src/plugins/win32dialog.cpp ${KR2_ROOT}/platform/linux/LinuxWin32Dialog.cpp)
set_target_properties(compat_harness PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
target_link_libraries(compat_harness PRIVATE harness_tjs pthread)
add_test(NAME compat-plugins COMMAND compat_harness)

# The GTK-backed dialog renderer is optional at build time.  It exercises the
# same template and callback path used by the game's font selector under Xvfb.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(KR2_PLUGIN_GTK3 QUIET IMPORTED_TARGET gtk+-3.0)
endif()
if(KR2_PLUGIN_GTK3_FOUND)
  add_executable(win32dialog_gui win32dialog_gui.cpp ${KR2_ROOT}/src/plugins/ncbind/ncbind.cpp
    ${KR2_ROOT}/src/plugins/win32dialog.cpp ${KR2_ROOT}/platform/linux/LinuxWin32Dialog.cpp)
  set_target_properties(win32dialog_gui PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
  target_compile_definitions(win32dialog_gui PRIVATE KR2_LINUX_HAVE_GTK3=1)
  target_include_directories(win32dialog_gui PRIVATE ${KR2_ROOT}/src/plugins ${KR2_ROOT}/src/plugins/ncbind)
  target_link_libraries(win32dialog_gui PRIVATE harness_tjs PkgConfig::KR2_PLUGIN_GTK3 pthread)

  # Font selectors (fontList + initialSelect) go to the native font picker.
  pkg_check_modules(KR2_PLUGIN_FONTCONFIG QUIET IMPORTED_TARGET fontconfig)
  add_executable(font_picker_gui font_picker_gui.cpp ${KR2_ROOT}/src/plugins/ncbind/ncbind.cpp
    ${KR2_ROOT}/src/plugins/win32dialog.cpp ${KR2_ROOT}/platform/linux/LinuxWin32Dialog.cpp
    ${KR2_ROOT}/platform/linux/LinuxFontPicker.cpp)
  set_target_properties(font_picker_gui PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
  target_compile_definitions(font_picker_gui PRIVATE KR2_LINUX_HAVE_GTK3=1)
  target_include_directories(font_picker_gui PRIVATE ${KR2_ROOT}/src/plugins ${KR2_ROOT}/src/plugins/ncbind)
  target_link_libraries(font_picker_gui PRIVATE harness_tjs PkgConfig::KR2_PLUGIN_GTK3 PkgConfig::KR2_PLUGIN_FONTCONFIG pthread)
endif()

# psbfile.dll through ncbind: PSBFile.load() + psb:// media (null-media regression).
add_executable(psbmedia_harness psbmedia_harness.cpp harness_memstream.cpp harness_binaccessor.cpp
  ${KR2_ROOT}/src/plugins/ncbind/ncbind.cpp
  ${P}/main.cpp ${P}/PSBMedia.cpp ${P}/PSBMediaRegistry.cpp
  ${P}/PSBFile.cpp ${P}/PSBValue.cpp ${P}/PSBOffsetReader.cpp ${P}/PSBStreamDecompress.cpp ${P}/PSBLz4Stream.cpp
  ${P}/types/PimgType.cpp ${P}/types/MotionType.cpp ${P}/types/ScnType.cpp ${P}/types/ImageType.cpp
  ${P}/types/MmoType.cpp ${P}/types/ArchiveType.cpp ${P}/types/SoundArchiveType.cpp ${P}/resources/ImageMetadata.cpp)
set_target_properties(psbmedia_harness PROPERTIES CXX_STANDARD 17 CXX_EXTENSIONS ON)
target_link_libraries(psbmedia_harness PRIVATE harness_tjs ZLIB::ZLIB PkgConfig::LZ4 pthread)

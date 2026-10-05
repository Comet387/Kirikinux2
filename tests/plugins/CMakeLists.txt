# Headless checks for plugins ported from KrKr2 (no Cocos/GL/FFmpeg needed).
#   cmake -S tests/plugins -B build-plugin-tests && cmake --build build-plugin-tests
#   ./build-plugin-tests/psb_harness sample.scn 'root.scenes.count'
cmake_minimum_required(VERSION 3.16)
project(kirikinux2_plugin_tests CXX)
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

# SPDX-License-Identifier: AGPL-3.0-only
# Archive codec targets, independent of Cocos/OpenGL for component verification.
find_package(Threads REQUIRED)
# 7z C API (base/7zArchive.cpp, base/UtilStreams.cpp include "7zip/C/7z.h").
# zeas2 kept it in src/core/base/7zip; we build p7zip 16.02's LZMA SDK C files with
# the older 7z.h/7zArcIn.c/7zBuf.c/7zDec.c/7zFile.h restored by Kirikiroid2Yuri.
set(KR2_7ZIP_ROOT "${KR2_THIRD_PARTY}/7zip")
if(NOT EXISTS "${KR2_7ZIP_ROOT}/C/7z.h")
  message(FATAL_ERROR "${KR2_7ZIP_ROOT}/C/7z.h not found - run scripts/linux/fetch-thirdparty.sh")
endif()
set(_kr2_7z_c)
foreach(f 7zArcIn 7zBuf 7zCrc 7zCrcOpt 7zDec 7zStream Alloc Bcj2 Bra Bra86 BraIA64 CpuArch Delta Lzma2Dec LzmaDec Ppmd7 Ppmd7Dec)
  if(EXISTS "${KR2_7ZIP_ROOT}/C/${f}.c")
    list(APPEND _kr2_7z_c "${KR2_7ZIP_ROOT}/C/${f}.c")
  else()
    message(FATAL_ERROR "Missing 7-Zip source: ${KR2_7ZIP_ROOT}/C/${f}.c")
  endif()
endforeach()
add_library(kr2_7zip STATIC ${_kr2_7z_c})
target_compile_definitions(kr2_7zip PUBLIC _7ZIP_ST)
target_include_directories(kr2_7zip PUBLIC "${KR2_THIRD_PARTY}" PRIVATE "${KR2_7ZIP_ROOT}/C")
set_target_properties(kr2_7zip PROPERTIES POSITION_INDEPENDENT_CODE ON)

# unrar (UtilStreams.cpp includes "unrar/raros.hpp" and "unrar/dll.hpp")
find_path(KR2_UNRAR_INCLUDE_DIR unrar/dll.hpp)
find_library(KR2_UNRAR_LIBRARY NAMES unrar libunrar.so.5)
if(KR2_UNRAR_INCLUDE_DIR AND KR2_UNRAR_LIBRARY AND NOT KR2_FORCE_BUNDLED_UNRAR)
  add_library(kr2_unrar INTERFACE)
  target_include_directories(kr2_unrar INTERFACE "${KR2_UNRAR_INCLUDE_DIR}")
  target_link_libraries(kr2_unrar INTERFACE "${KR2_UNRAR_LIBRARY}")
  message(STATUS "Kirikinux2: system unrar ${KR2_UNRAR_LIBRARY}")
elseif(EXISTS "${KR2_THIRD_PARTY}/unrar/dll.hpp")
  # OBJECTS + LIB_OBJ of unrar's makefile ("make lib"); the other .cpp files are #included by these
  set(_kr2_unrar_src)
  foreach(f rar strlist strfn pathfn smallfn global file filefn filcreat archive arcread unicode system
            isnt crypt crc rawread encname resource match timefn rdwrfn consio options errhnd rarvm
            secpassword rijndael getbits sha1 sha256 blake2s hash extinfo extract volume list find
            unpack headers threadpool rs16 cmddata ui filestr scantree dll qopen)
    if(EXISTS "${KR2_THIRD_PARTY}/unrar/${f}.cpp")
      list(APPEND _kr2_unrar_src "${KR2_THIRD_PARTY}/unrar/${f}.cpp")
    else()
      message(FATAL_ERROR "Missing UnRAR source: ${KR2_THIRD_PARTY}/unrar/${f}.cpp")
    endif()
  endforeach()
  add_library(kr2_unrar STATIC ${_kr2_unrar_src})
  target_compile_definitions(kr2_unrar PRIVATE RARDLL _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE RAR_SMP)
  target_compile_options(kr2_unrar PRIVATE -Wno-logical-op-parentheses -Wno-switch -Wno-dangling-else)
  target_include_directories(kr2_unrar PUBLIC "${KR2_THIRD_PARTY}")
  target_link_libraries(kr2_unrar PUBLIC Threads::Threads)
  set_target_properties(kr2_unrar PROPERTIES POSITION_INDEPENDENT_CODE ON)
  message(STATUS "Kirikinux2: bundled unrar from ${KR2_THIRD_PARTY}/unrar")
else()
  message(FATAL_ERROR "unrar not found: install libunrar-headers + libunrar (multiverse/non-free) or run scripts/linux/fetch-thirdparty.sh")
endif()

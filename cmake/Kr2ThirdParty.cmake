# SPDX-License-Identifier: AGPL-3.0-only
# Third-party libraries of src/core/Android.mk, taken from the distribution
# where possible.  Not in distributions / not in the public tree:
#   cocos2d-x 3.17.2, p7zip (7z C API), unrar  -> scripts/linux/fetch-thirdparty.sh
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(KR2_GL REQUIRED IMPORTED_TARGET glew)
# Public Cocos headers include GL/glew.h; export its dependency to consumers.
target_link_libraries(cocos2d PkgConfig::KR2_GL)

pkg_check_modules(KR2_FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale libswresample libavfilter)
if(KR2_FFMPEG_libavcodec_VERSION VERSION_GREATER_EQUAL 59)
  message(FATAL_ERROR
    "FFmpeg ${KR2_FFMPEG_libavcodec_VERSION} (libavcodec >= 59, FFmpeg >= 5) removed APIs the "
    "Kodi-derived video code used by Kirikinux2.  Build FFmpeg 4.4 with "
    "scripts/linux/build-ffmpeg4.sh and configure with "
    "-DCMAKE_PREFIX_PATH=${KR2_THIRD_PARTY}/install/ffmpeg4 (build.sh does this automatically).")
endif()
pkg_check_modules(KR2_SDL2 REQUIRED IMPORTED_TARGET sdl2)          # SDL.h / SDL_SetMainReady
pkg_check_modules(KR2_OPENAL REQUIRED IMPORTED_TARGET openal)      # WaveMixer.cpp default renderer
pkg_check_modules(KR2_CODECS REQUIRED IMPORTED_TARGET vorbisfile vorbis ogg opusfile opus)
pkg_check_modules(KR2_ONIG REQUIRED IMPORTED_TARGET oniguruma)     # tjsRegExp
pkg_check_modules(KR2_ARCHIVE REQUIRED IMPORTED_TARGET libarchive)
pkg_check_modules(KR2_LZ4 REQUIRED IMPORTED_TARGET liblz4)
pkg_check_modules(KR2_JPEG REQUIRED IMPORTED_TARGET libturbojpeg libjpeg)
# Kirikiroid2 uses libjpeg-turbo's extended colorspaces. Cocos' prebuilt IJG 9
# headers/library have a different ABI; use one system JPEG implementation in
# both engines, including Cocos Image's decoder.
find_library(KR2_SYSTEM_JPEG NAMES jpeg HINTS ${KR2_JPEG_LIBRARY_DIRS})
find_path(KR2_SYSTEM_JPEG_INCLUDE jpeglib.h HINTS ${KR2_JPEG_INCLUDE_DIRS})
if(NOT KR2_SYSTEM_JPEG OR NOT KR2_SYSTEM_JPEG_INCLUDE)
  message(FATAL_ERROR "libjpeg-turbo headers/library were not found")
endif()
set_target_properties(ext_jpeg PROPERTIES
  IMPORTED_LOCATION "${KR2_SYSTEM_JPEG}"
  INTERFACE_INCLUDE_DIRECTORIES "${KR2_SYSTEM_JPEG_INCLUDE}")
pkg_check_modules(KR2_GTK3 IMPORTED_TARGET gtk+-3.0)               # message boxes (optional)
find_package(OpenCV REQUIRED COMPONENTS core imgproc)

# image codecs LoadPNG/LoadWEBP/FreeType include as <png.h>, "decode.h", <ft2build.h>.
# Prefer the copies cocos2d-x links itself, so only one version ends up in the binary.
set(_kr2_cx "${KR2_COCOS2DX_ROOT}/external")
set(KR2_CODEC_INCLUDE_DIRS)
set(KR2_CODEC_LIBS)
if(EXISTS "${_kr2_cx}/webp/include/linux/decode.h")
  list(APPEND KR2_CODEC_INCLUDE_DIRS "${_kr2_cx}/webp/include/linux")
else()
  pkg_check_modules(KR2_WEBP REQUIRED IMPORTED_TARGET libwebp)
  find_path(KR2_WEBP_DECODE_DIR webp/decode.h HINTS ${KR2_WEBP_INCLUDE_DIRS})
  list(APPEND KR2_CODEC_INCLUDE_DIRS "${KR2_WEBP_DECODE_DIR}/webp")
  list(APPEND KR2_CODEC_LIBS PkgConfig::KR2_WEBP)
endif()
if(EXISTS "${_kr2_cx}/png/include/linux/png.h")
  list(APPEND KR2_CODEC_INCLUDE_DIRS "${_kr2_cx}/png/include/linux")
else()
  pkg_check_modules(KR2_PNG REQUIRED IMPORTED_TARGET libpng)
  list(APPEND KR2_CODEC_LIBS PkgConfig::KR2_PNG)
endif()
if(EXISTS "${_kr2_cx}/freetype2/include/linux/freetype2/ft2build.h")
  list(APPEND KR2_CODEC_INCLUDE_DIRS "${_kr2_cx}/freetype2/include/linux/freetype2")
elseif(EXISTS "${_kr2_cx}/freetype2/include/linux/ft2build.h")
  list(APPEND KR2_CODEC_INCLUDE_DIRS "${_kr2_cx}/freetype2/include/linux")
else()
  pkg_check_modules(KR2_FREETYPE REQUIRED IMPORTED_TARGET freetype2)
  list(APPEND KR2_CODEC_LIBS PkgConfig::KR2_FREETYPE)
endif()

include("${KR2_ROOT}/cmake/Kr2ArchiveCodecs.cmake")

if(KR2_WITH_JXR)
  find_path(KR2_JXR_INCLUDE_DIR JXRGlue.h PATH_SUFFIXES jxrlib)
  find_library(KR2_JXR_GLUE jxrglue)
  find_library(KR2_JXR_LIB jpegxr)
  if(NOT (KR2_JXR_INCLUDE_DIR AND KR2_JXR_GLUE AND KR2_JXR_LIB))
    message(FATAL_ERROR "KR2_WITH_JXR=ON but jxrlib (libjxr-dev) was not found")
  endif()
endif()

# CJK font for the engine (FontImpl.cpp looks for DroidSansFallback.ttf via cocos FileUtils)
if(NOT KR2_DEFAULT_FONT)
  foreach(f
      "${KR2_ROOT}/cocos/kr2/Resources/DroidSansFallback.ttf"
      /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc
      /usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc
      /usr/share/fonts/google-noto-cjk/NotoSansCJK-Regular.ttc
      /usr/share/fonts/truetype/wqy/wqy-microhei.ttc
      /usr/share/fonts/wenquanyi/wqy-microhei/wqy-microhei.ttc
      /usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf)
    if(EXISTS "${f}")
      set(KR2_DEFAULT_FONT "${f}" CACHE FILEPATH "CJK font copied to Resources/DroidSansFallback.ttf" FORCE)
      break()
    endif()
  endforeach()
endif()

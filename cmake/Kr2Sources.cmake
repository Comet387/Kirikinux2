# Source lists of the ORIGINAL engine, mirroring src/core/Android.mk and
# src/plugins/Android.mk.  The only differences from the Android build are
# listed in KR2_CORE_LINUX_EXCLUDES and KR2_PLATFORM_SOURCES.
set(KR2_CORE "${KR2_ROOT}/src/core")

file(GLOB KR2_CORE_SOURCES CONFIGURE_DEPENDS
  ${KR2_CORE}/visual/*.cpp
  ${KR2_CORE}/base/*.cpp
  ${KR2_CORE}/base/win32/*.cpp
  ${KR2_CORE}/environ/*.cpp
  ${KR2_CORE}/environ/ConfigManager/*.cpp   # used by every form; Yuri's CMake lists it too
  ${KR2_CORE}/environ/cocos2d/*.cpp
  ${KR2_CORE}/environ/linux/*.cpp           # POSIX helpers shared with Android
  ${KR2_CORE}/environ/ui/*.cpp
  ${KR2_CORE}/environ/ui/extension/*.cpp
  ${KR2_CORE}/extension/*.cpp
  ${KR2_CORE}/movie/*.cpp
  ${KR2_CORE}/movie/*/*.cpp
  ${KR2_CORE}/msg/*.cpp
  ${KR2_CORE}/sound/*.cpp
  ${KR2_CORE}/sound/win32/*.cpp
  ${KR2_CORE}/tjs2/*.cpp
  ${KR2_CORE}/utils/*.c
  ${KR2_CORE}/utils/*.cpp
  ${KR2_CORE}/utils/encoding/*.c
  ${KR2_CORE}/utils/minizip/*.c
  ${KR2_CORE}/utils/minizip/*.cpp
  ${KR2_CORE}/utils/win32/*.cpp
  ${KR2_CORE}/visual/gl/*.cpp
  ${KR2_CORE}/visual/ogl/*.cpp
  ${KR2_CORE}/visual/win32/*.cpp)
list(APPEND KR2_CORE_SOURCES
  ${KR2_CORE}/environ/win32/SystemControl.cpp
  ${KR2_CORE}/msg/win32/MsgImpl.cpp
  ${KR2_CORE}/msg/win32/OptionsDesc.cpp)

# filter-out of src/core/Android.mk
set(KR2_CORE_ANDROID_EXCLUDES
  ${KR2_CORE}/visual/Resampler.cpp
  ${KR2_CORE}/base/win32/FuncStubs.cpp
  ${KR2_CORE}/base/win32/SusieArchive.cpp
  ${KR2_CORE}/environ/MainFormUnit.cpp
  ${KR2_CORE}/sound/xmmlib.cpp
  ${KR2_CORE}/sound/WaveFormatConverter_SSE.cpp
  ${KR2_CORE}/visual/win32/GDIFontRasterizer.cpp
  ${KR2_CORE}/visual/win32/NativeFreeTypeFace.cpp
  ${KR2_CORE}/visual/win32/TVPSysFont.cpp
  ${KR2_CORE}/visual/win32/VSyncTimingThread.cpp)
# Linux: dependencies that are not part of the public tree (stubbed in
# platform/linux/LinuxFeatureStubs.cpp, same set Kirikiroid2Yuri leaves out)
set(KR2_CORE_LINUX_EXCLUDES
  ${KR2_CORE}/environ/XP3ArchiveRepack.cpp
  ${KR2_CORE}/environ/ui/XP3RepackForm.cpp
  ${KR2_CORE}/visual/LoadBPG.cpp)
if(NOT KR2_WITH_JXR)
  list(APPEND KR2_CORE_LINUX_EXCLUDES ${KR2_CORE}/visual/LoadJXR.cpp)
endif()
list(REMOVE_ITEM KR2_CORE_SOURCES ${KR2_CORE_ANDROID_EXCLUDES} ${KR2_CORE_LINUX_EXCLUDES})

# environ/android/*.cpp is replaced by the Linux host; visual/ARM + sound/ARM
# (the Android NEON module) by no-op entry points.
set(KR2_PLATFORM_SOURCES
  ${KR2_ROOT}/platform/linux/LinuxHost.cpp
  ${KR2_ROOT}/platform/linux/LinuxUtils.cpp
  ${KR2_ROOT}/platform/linux/LinuxDialogs.cpp
  ${KR2_ROOT}/platform/linux/LinuxFeatureStubs.cpp
  ${KR2_ROOT}/platform/linux/LinuxNoNeonStubs.c)

# include directories of src/core/Android.mk (minus vendor/, environ/android)
set(KR2_CORE_INCLUDE_DIRS
  ${KR2_ROOT}/platform/linux/compat
  ${KR2_CORE}
  ${KR2_CORE}/base
  ${KR2_CORE}/base/win32
  ${KR2_CORE}/environ
  ${KR2_CORE}/environ/win32
  ${KR2_CORE}/environ/sdl
  ${KR2_CORE}/msg
  ${KR2_CORE}/msg/win32
  ${KR2_CORE}/extension
  ${KR2_CORE}/sound
  ${KR2_CORE}/sound/win32
  ${KR2_CORE}/tjs2
  ${KR2_CORE}/utils
  ${KR2_CORE}/utils/win32
  ${KR2_CORE}/visual
  ${KR2_CORE}/visual/ARM
  ${KR2_CORE}/visual/win32
  ${KR2_ROOT}/src/plugins)

# src/plugins/Android.mk
file(GLOB KR2_PLUGIN_SOURCES CONFIGURE_DEPENDS
  ${KR2_ROOT}/src/plugins/*.cpp
  ${KR2_ROOT}/src/plugins/*.c
  ${KR2_ROOT}/src/plugins/extrans/*.cpp)
list(APPEND KR2_PLUGIN_SOURCES ${KR2_ROOT}/src/plugins/ncbind/ncbind.cpp)

# Kodi-derived FFmpeg users (FFmpeg 4.x macro compatibility)
file(GLOB KR2_FFMPEG_USERS CONFIGURE_DEPENDS ${KR2_CORE}/movie/ffmpeg/*.cpp)
list(APPEND KR2_FFMPEG_USERS ${KR2_CORE}/sound/FFWaveDecoder.cpp)

/* SPDX-License-Identifier: AGPL-3.0-only */
/*
 * kirikinux2: src/core/visual/ARM and src/core/sound/ARM are the Android NEON
 * module (krkr2_neon_opt in project/android/jni/Android.mk).  They include the
 * NDK's <cpu-features.h>, so the Linux build does not compile them.  The engine
 * calls their entry points unconditionally (SysInitImpl.cpp, WaveMixer.cpp,
 * MainFileSelectorForm.cpp); these no-ops keep the portable C paths active.
 * DetectCPU.cpp likewise reports no NEON on Linux.
 */
void TVPGL_ASM_Init(void) {}
void TVPGL_ASM_Test(void) {}
void TVPWaveMixer_ASM_Init(void *func16, void *func32) { (void)func16; (void)func32; }

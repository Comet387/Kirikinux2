/* SPDX-License-Identifier: AGPL-3.0-only */
#pragma once
/* kirikinux2: the original Protect.h (license / anti-piracy hooks of zeas2's
   private build) is not in the public source.  SysInitIntf.cpp calls
   TVPProtectInit() on every non-win32 platform. */
#ifdef __cplusplus
inline bool TVPProtectInit() { return true; }
inline void TVPUpdateLicense() {}
#endif

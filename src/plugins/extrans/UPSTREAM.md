# Upstream extended transitions

Source: https://github.com/krkrz/SamplePlugin/tree/d94e312065f7298a970b58bfe73de48e52aef9ee/extrans
Commit: d94e312065f7298a970b58bfe73de48e52aef9ee
Authors: W.Dee and Kirikiri Z Project contributors. The Kirikiri Z license in the repository root LICENSE applies.

Preserved upstream rotation, wave, ripple, mosaic and page-turn algorithms and lookup table.
Linux changes: replace Windows-only headers; guard MSVC x86 inline assembly; use SSE2 intrinsics when available;
route legacy scanline access through ExTransBridge.h into the existing Kirikiroid2 textures.
The Windows DLL entry points are replaced by the existing ncbind internal plugin registration chain.

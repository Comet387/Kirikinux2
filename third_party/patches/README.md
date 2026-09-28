# Source provenance

`p7zip/{7z.h,7zArcIn.c,7zBuf.c,7zDec.c,7zFile.h}` are copied byte-for-byte from
[YuriSizuku/Kirikiroid2Yuri](https://github.com/YuriSizuku/Kirikiroid2Yuri), branch
`yuri`, commit `6e61ce3b81416ceb2be3427a5f0471edefab7151`, directory
`thirdparty/patch/p7zip`. Each file retains Igor Pavlov's public-domain notice.
They restore the older C API expected by this repository on top of p7zip 16.02.

`platform/linux/compat/xmmlib.h` comes from the same commit's
`src/core/sound/xmmlib.h`. Its Kirikiri/Risa and Xiph copyright notices are
retained; the project LICENSE and `platform/linux/compat/VORBIS-COPYING` carry
the accompanying terms.

Fetched source trees and binary dependencies are excluded from source releases.
`scripts/linux/fetch-thirdparty.sh` pins Cocos 3.17.2, external v3-deps-158,
p7zip 16.02 and UnRAR 6.0.7 downloads by SHA-256. Their original license files
remain in the extracted trees. The optional FFmpeg 4.4.5 build is version-pinned;
its download has not been verified in this environment.

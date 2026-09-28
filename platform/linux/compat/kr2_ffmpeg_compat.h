#pragma once
/* kirikinux2: src/core/movie/ffmpeg is derived from Kodi 17 and written for the
   FFmpeg 3.x API of zeas2's FFmpeg fork.  FFmpeg 4.x still has every deprecated
   function it calls (AVStream::codec, avcodec_decode_*, av_free_packet, ...) but
   removed the old macro spellings below.  Force-included into the FFmpeg users
   only; CMake rejects FFmpeg >= 5 (see scripts/linux/build-ffmpeg4.sh). */
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
}
#endif
#ifndef FF_INPUT_BUFFER_PADDING_SIZE
#define FF_INPUT_BUFFER_PADDING_SIZE AV_INPUT_BUFFER_PADDING_SIZE
#endif
#ifndef CODEC_FLAG_EMU_EDGE
#define CODEC_FLAG_EMU_EDGE 0 /* removed in FFmpeg 4.0, had become a no-op */
#endif
#ifndef CODEC_FLAG_TRUNCATED
#define CODEC_FLAG_TRUNCATED AV_CODEC_FLAG_TRUNCATED
#endif
#ifndef CODEC_CAP_DR1
#define CODEC_CAP_DR1 AV_CODEC_CAP_DR1
#endif
#ifndef CODEC_CAP_TRUNCATED
#define CODEC_CAP_TRUNCATED AV_CODEC_CAP_TRUNCATED
#endif
/* FFmpeg 4 moved these qp-table type values into libavcodec/internal.h,
   while av_frame_get_qp_table remains part of the public deprecated API. */
#ifndef FF_QSCALE_TYPE_MPEG1
#define FF_QSCALE_TYPE_MPEG1 0
#define FF_QSCALE_TYPE_MPEG2 1
#define FF_QSCALE_TYPE_H264 2
#endif

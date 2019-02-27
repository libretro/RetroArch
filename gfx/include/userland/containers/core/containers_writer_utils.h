/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef VC_CONTAINERS_WRITER_UTILS_H
#define VC_CONTAINERS_WRITER_UTILS_H

/** \file containers_writer_utils.h
 * Helper functions and macros for container writers
 */

#include "containers/containers.h"
#include "containers/containers_codecs.h"
#include "containers/core/containers_io_helpers.h"

/*****************************************************************************
 * Helper inline functions to write format specific structus to an i/o stream
 *****************************************************************************/

STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_write_waveformatex( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_ES_FORMAT_T *format)
{
   /* Write waveformatex structure */
   WRITE_U16(p_ctx, codec_to_waveformat(format->codec), "Codec ID");
   WRITE_U16(p_ctx, format->type->audio.channels, "Number of Channels");
   WRITE_U32(p_ctx, format->type->audio.sample_rate, "Samples per Second");
   WRITE_U32(p_ctx, format->bitrate >> 3, "Average Number of Bytes Per Second");
   WRITE_U16(p_ctx, format->type->audio.block_align, "Block Alignment");
   WRITE_U16(p_ctx, format->type->audio.bits_per_sample, "Bits Per Sample");
   WRITE_U16(p_ctx, format->extradata_size, "Codec Specific Data Size");
   WRITE_BYTES(p_ctx, format->extradata, format->extradata_size);

   return STREAM_STATUS(p_ctx);
}

STATIC_INLINE VC_CONTAINER_STATUS_T vc_container_write_bitmapinfoheader( VC_CONTAINER_T *p_ctx,
   VC_CONTAINER_ES_FORMAT_T *format)
{
   VC_CONTAINER_FOURCC_T fourcc;

   /* Write bitmapinfoheader structure */
   WRITE_U32(p_ctx, 40, "Format Data Size");
   WRITE_U32(p_ctx, format->type->video.width, "Image Width");
   WRITE_U32(p_ctx, format->type->video.height, "Image Height");
   WRITE_U16(p_ctx, 0, "Reserved");
   WRITE_U16(p_ctx, 0, "Bits Per Pixel Count");
   fourcc = codec_to_vfw_fourcc(format->codec);
   WRITE_BYTES(p_ctx, (char *)&fourcc, 4); /* Compression ID */
   LOG_FORMAT(p_ctx, "Compression ID: %4.4s", (char *)&fourcc);
   WRITE_U32(p_ctx, 0, "Image Size");
   WRITE_U32(p_ctx, 0, "Horizontal Pixels Per Meter");
   WRITE_U32(p_ctx, 0, "Vertical Pixels Per Meter");
   WRITE_U32(p_ctx, 0, "Colors Used Count");
   WRITE_U32(p_ctx, 0, "Important Colors Count");

   WRITE_BYTES(p_ctx, format->extradata, format->extradata_size);

   return STREAM_STATUS(p_ctx);
}

/* Helper functions to create and use extra i/o */
typedef struct VC_CONTAINER_WRITER_EXTRAIO_T {
   VC_CONTAINER_IO_T *io;
   unsigned int refcount;
   bool temp;
} VC_CONTAINER_WRITER_EXTRAIO_T;

VC_CONTAINER_STATUS_T vc_container_writer_extraio_create_null(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *null);
VC_CONTAINER_STATUS_T vc_container_writer_extraio_create_temp(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *null);
VC_CONTAINER_STATUS_T vc_container_writer_extraio_delete(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *null);
int64_t vc_container_writer_extraio_enable(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *null);
int64_t vc_container_writer_extraio_disable(VC_CONTAINER_T *context, VC_CONTAINER_WRITER_EXTRAIO_T *null);

#endif /* VC_CONTAINERS_WRITER_UTILS_H */

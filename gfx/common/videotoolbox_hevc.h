/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_H

#if defined(__APPLE__) && !defined(__cplusplus) \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L) && !defined(inline)
#define inline __inline__
#define VIDEOTOOLBOX_HEVC_UNDEF_INLINE
#endif
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>
#ifdef VIDEOTOOLBOX_HEVC_UNDEF_INLINE
#undef inline
#undef VIDEOTOOLBOX_HEVC_UNDEF_INLINE
#endif
#include <formats/rh265.h>
#include <retro_inline.h>

/* HEVC format construction requires these OS versions; older Metal drivers
 * may include the helpers but must guard setup before calling them. */
#if defined(__APPLE__) && defined(__clang__)
#define VIDEOTOOLBOX_HEVC_AVAILABLE __attribute__((availability(macos,introduced=10.13), availability(ios,introduced=11.0), availability(tvos,introduced=11.0)))
#else
#define VIDEOTOOLBOX_HEVC_AVAILABLE
#endif

/* Setup only. The source must have retained its original parameter NALs. */
static INLINE VIDEOTOOLBOX_HEVC_AVAILABLE OSStatus videotoolbox_hevc_format_create(const rh265_video *source,
      unsigned pps_id, CMVideoFormatDescriptionRef *out)
{
   rh265_nal_t sets[3];
   const uint8_t *pointers[3];
   size_t sizes[3];
   unsigned i;
   if (!out) return kCMFormatDescriptionError_InvalidParameter;
   *out = NULL;
   if (rh265_video_get_parameter_sets(source, pps_id, sets)) return kCMFormatDescriptionError_InvalidParameter;
   for (i = 0; i < 3; i++)
   {
      pointers[i] = sets[i].data;
      sizes[i] = sets[i].size;
   }
   /* Submitted access units use four-byte NAL lengths. */
   return CMVideoFormatDescriptionCreateFromHEVCParameterSets(
         kCFAllocatorDefault, 3, pointers, sizes, 4, NULL, out);
}

/* No frames are submitted here. Caller owns the returned session and format;
 * callback/refcon lifetime must cover every later submission and teardown. */
static INLINE VIDEOTOOLBOX_HEVC_AVAILABLE OSStatus videotoolbox_hevc_session_create(const rh265_video *source,
      unsigned pps_id, const VTDecompressionOutputCallbackRecord *callback,
      CMVideoFormatDescriptionRef *format_out, VTDecompressionSessionRef *out)
{
   CMVideoFormatDescriptionRef format = NULL;
   CFDictionaryRef specification = NULL, attributes = NULL;
   CFNumberRef pixel_format = NULL;
   rh265_video_color_t color;
   rh265_sequence_info_t sequence;
   rh265_video_params_t params;
   const void *keys[2], *values[2];
   OSStatus status;
   int32_t pixel;
   if (format_out) *format_out = NULL;
   if (out) *out = NULL;
   if (!out || !format_out || !callback || !callback->decompressionOutputCallback
         || rh265_video_get_color(source, pps_id, &color)
         || !color.video_signal_present
         || rh265_video_get_sequence(source, pps_id, &sequence)
         || rh265_video_get_params(source, pps_id, &params)
         || sequence.profile_space || sequence.field_seq
         || (sequence.profile_idc != 1 && sequence.profile_idc != 2)
         || (sequence.profile_idc == 1 && params.bit_depth != 8)
         || params.chroma_format_idc != 1
         || (params.bit_depth != 8 && params.bit_depth != 10))
      return kCMFormatDescriptionError_InvalidParameter;
   status = videotoolbox_hevc_format_create(source, pps_id, &format);
   if (status) return status;
   pixel = params.bit_depth == 10
      ? (color.full_range ? 0x78663230 : 0x78343230)
      : (color.full_range ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                         : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
   keys[0] = kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder;
   values[0] = kCFBooleanTrue;
   specification = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   pixel_format = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixel);
   keys[0] = kCVPixelBufferPixelFormatTypeKey;
   keys[1] = kCVPixelBufferMetalCompatibilityKey;
   values[0] = pixel_format;
   values[1] = kCFBooleanTrue;
   if (pixel_format)
      attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   status = kCMFormatDescriptionError_AllocationFailed;
   if (specification && attributes)
      status = VTDecompressionSessionCreate(kCFAllocatorDefault, format,
            specification, attributes, callback, out);
   if (attributes) CFRelease(attributes);
   if (pixel_format) CFRelease(pixel_format);
   if (specification) CFRelease(specification);
   if (status)
   {
      if (*out)
      {
         VTDecompressionSessionInvalidate(*out);
         CFRelease(*out);
         *out = NULL;
      }
      CFRelease(format);
   }
   else
      *format_out = format;
   return status;
}

/* Copy a parsed access unit before borrowed packet storage may be reused.
 * One sample contains four-byte lengths and complete escaped VCL NALs.
 * Optional timing is copied by Core Media. */
static INLINE OSStatus videotoolbox_hevc_sample_create_timed(
      CMVideoFormatDescriptionRef format, const rh265_access_unit_t *au,
      const rh265_nal_t *slices, unsigned capacity,
      const CMSampleTimingInfo *timing, CMSampleBufferRef *out)
{
   CMBlockBufferRef block = NULL;
   OSStatus status;
   size_t bytes = 0, total, offset = 0;
   unsigned i;
   if (!out) return kCMFormatDescriptionError_InvalidParameter;
   *out = NULL;
   if (!format || !au || !slices || !au->num_slices
         || au->num_slices > capacity)
      return kCMFormatDescriptionError_InvalidParameter;
   for (i = 0; i < au->num_slices; i++)
   {
      const rh265_nal_t *nal = &slices[i];
      unsigned type;
      if (!nal->data || nal->size < 3 || nal->size > UINT32_MAX
            || nal->size > (size_t)-1 - bytes)
         return kCMFormatDescriptionError_InvalidParameter;
      type = (nal->data[0] >> 1) & 63;
      if ((nal->data[0] & 0x81) || (nal->data[1] >> 3)
            || !(nal->data[1] & 7)
            || !(type <= 9 || (type >= 16 && type <= 21))
            || (int)type != au->first.nal_type
            || (int)(nal->data[1] & 7) - 1 != au->first.temporal_id)
         return kCMFormatDescriptionError_InvalidParameter;
      bytes += nal->size;
   }
   if (bytes != au->coded_bytes || au->num_slices > ((size_t)-1 - bytes) / 4)
      return kCMFormatDescriptionError_InvalidParameter;
   total = bytes + (size_t)au->num_slices * 4;
   status = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL,
         total, kCFAllocatorDefault, NULL, 0, total, 0, &block);
   if (!status) status = CMBlockBufferAssureBlockMemory(block);
   for (i = 0; !status && i < au->num_slices; i++)
   {
      uint32_t size = (uint32_t)slices[i].size;
      uint8_t length[4];
      length[0] = (uint8_t)(size >> 24); length[1] = (uint8_t)(size >> 16);
      length[2] = (uint8_t)(size >> 8); length[3] = (uint8_t)size;
      status = CMBlockBufferReplaceDataBytes(length, block, offset, 4);
      if (!status)
         status = CMBlockBufferReplaceDataBytes(slices[i].data, block,
               offset + 4, slices[i].size);
      offset += 4 + slices[i].size;
   }
   if (!status)
      status = CMSampleBufferCreateReady(kCFAllocatorDefault, block, format,
            1, timing ? 1 : 0, timing, 1, &total, out);
   if (block) CFRelease(block);
   if (status && *out)
   {
      CFRelease(*out);
      *out = NULL;
   }
   return status;
}

static INLINE OSStatus videotoolbox_hevc_sample_create(
      CMVideoFormatDescriptionRef format, const rh265_access_unit_t *au,
      const rh265_nal_t *slices, unsigned capacity, CMSampleBufferRef *out)
{
   return videotoolbox_hevc_sample_create_timed(format, au, slices, capacity, NULL, out);
}

/* Callback and frame_ref must survive synchronous or delayed completion.
 * This owns compressed bytes, not the caller's callback context or slot. */
static INLINE OSStatus videotoolbox_hevc_decode_timed(VTDecompressionSessionRef session,
      CMVideoFormatDescriptionRef format, const rh265_access_unit_t *au,
      const rh265_nal_t *slices, unsigned capacity,
      const CMSampleTimingInfo *timing, void *frame_ref,
      VTDecodeInfoFlags *info)
{
   CMSampleBufferRef sample = NULL;
   OSStatus status;
   if (info) *info = 0;
   if (!session) return kCMFormatDescriptionError_InvalidParameter;
   status = videotoolbox_hevc_sample_create_timed(format, au, slices, capacity, timing, &sample);
   if (status) return status;
   /* Keep temporal processing disabled: the shared codec state owns display
    * order and final pictures must complete without a frame-path drain. */
   status = VTDecompressionSessionDecodeFrame(session, sample,
         kVTDecodeFrame_EnableAsynchronousDecompression, frame_ref, info);
   CFRelease(sample);
   return status;
}

static INLINE OSStatus videotoolbox_hevc_decode(VTDecompressionSessionRef session,
      CMVideoFormatDescriptionRef format, const rh265_access_unit_t *au,
      const rh265_nal_t *slices, unsigned capacity, void *frame_ref,
      VTDecodeInfoFlags *info)
{
   return videotoolbox_hevc_decode_timed(session, format, au, slices, capacity, NULL, frame_ref, info);
}

#endif

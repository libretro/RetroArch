/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_HEVC_H
#define RETROARCH_VIDEOTOOLBOX_HEVC_H

#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>
#include <formats/rh265.h>
#include <retro_inline.h>

/* Setup only. The source must have retained its original parameter NALs. */
static INLINE OSStatus videotoolbox_hevc_format_create(const rh265_video *source,
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
static INLINE OSStatus videotoolbox_hevc_session_create(const rh265_video *source,
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

#endif

/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_VP9_H
#define RETROARCH_VIDEOTOOLBOX_VP9_H
#if defined(__APPLE__) && !defined(__cplusplus)                                \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L)            \
      && !defined(inline)
#define inline __inline__
#define VIDEOTOOLBOX_VP9_UNDEF_INLINE
#endif
#include <CoreMedia/CoreMedia.h>
#include <CoreVideo/CoreVideo.h>
#include <VideoToolbox/VideoToolbox.h>
#ifdef VIDEOTOOLBOX_VP9_UNDEF_INLINE
#undef inline
#undef VIDEOTOOLBOX_VP9_UNDEF_INLINE
#endif
#include "videotoolbox_vp9_config.h"

#define VIDEOTOOLBOX_VP9_CODEC ((CMVideoCodecType)0x76703039)
/* The lifecycle owner resolves supplemental registration when available.
 * Keeping it optional avoids a newer/macOS-only API link on older SDKs/iOS. */
typedef void (*videotoolbox_vp9_register_t)(CMVideoCodecType codec);

/* Cold setup only. A valid format is not evidence of hardware support. */
static INLINE OSStatus videotoolbox_vp9_format_create(const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned chroma_position,
      CMVideoFormatDescriptionRef *out)
{
   videotoolbox_vp9_config_t config;
   CFDataRef data        = NULL;
   CFDictionaryRef atoms = NULL, extensions = NULL;
   CFNumberRef depth = NULL;
   const void *keys[6], *values[6];
   CFStringRef primaries = NULL, transfer = NULL, matrix = NULL;
   unsigned count = 3;
   int32_t bits;
   OSStatus status = kCMFormatDescriptionError_AllocationFailed;
   if (!out)
      return kCMFormatDescriptionError_InvalidParameter;
   *out = NULL;
   if (!videotoolbox_vp9_configure(header, color, chroma_position, &config))
      return kCMFormatDescriptionError_InvalidParameter;
   bits = header->bit_depth;
   data = CFDataCreate(kCFAllocatorDefault, config.vpcc, sizeof(config.vpcc));
   if (!data)
      goto done;
   keys[0]   = CFSTR("vpcC");
   values[0] = data;
   atoms     = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
             &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   if (!atoms)
      goto done;
   depth = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &bits);
   if (!depth)
      goto done;
   keys[0]    = kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms;
   keys[1]    = CFSTR("BitsPerComponent");
   values[0]  = atoms;
   values[1]  = depth;
   /* vpcC alone does not prevent size-based CoreVideo color defaults. */
   keys[2] = kCMFormatDescriptionExtension_FullRangeVideo;
   values[2] = config.color.range == 2 ? kCFBooleanTrue : kCFBooleanFalse;
   if (config.color.primaries == 1)
      primaries = kCVImageBufferColorPrimaries_ITU_R_709_2;
   else if (config.color.primaries == 9)
      primaries = kCVImageBufferColorPrimaries_ITU_R_2020;
   switch (config.color.transfer)
   {
      case 1: transfer = kCVImageBufferTransferFunction_ITU_R_709_2; break;
      case 13: transfer = kCVImageBufferTransferFunction_sRGB; break;
      case 16: transfer = kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ; break;
      case 18: transfer = kCVImageBufferTransferFunction_ITU_R_2100_HLG; break;
   }
   if (config.color.matrix == 1)
      matrix = kCVImageBufferYCbCrMatrix_ITU_R_709_2;
   else if (config.color.matrix == 9)
      matrix = kCVImageBufferYCbCrMatrix_ITU_R_2020;
   if (primaries)
   {
      keys[count] = kCMFormatDescriptionExtension_ColorPrimaries;
      values[count++] = primaries;
   }
   if (transfer)
   {
      keys[count] = kCMFormatDescriptionExtension_TransferFunction;
      values[count++] = transfer;
   }
   if (matrix)
   {
      keys[count] = kCMFormatDescriptionExtension_YCbCrMatrix;
      values[count++] = matrix;
   }
   extensions = CFDictionaryCreate(kCFAllocatorDefault, keys, values, count,
         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   if (!extensions)
      goto done;
   status = CMVideoFormatDescriptionCreate(kCFAllocatorDefault,
         VIDEOTOOLBOX_VP9_CODEC, (int32_t)config.width, (int32_t)config.height,
         extensions, out);
   if (!status && !*out)
      status = kCMFormatDescriptionError_AllocationFailed;
   if (status && *out)
   {
      CFRelease(*out);
      *out = NULL;
   }
done:
   if (extensions)
      CFRelease(extensions);
   if (depth)
      CFRelease(depth);
   if (atoms)
      CFRelease(atoms);
   if (data)
      CFRelease(data);
   return status;
}

/* Caller owns successful format/session references and callback storage.
 * Hardware is required; the output pool requests Metal-compatible IOSurface
 * NV12/P010 in the coded range. Registration alone proves no capability.
 * No packets, callbacks, waits or per-frame resources are created here. */
static INLINE OSStatus videotoolbox_vp9_session_create(const rvp9_hdr *header,
      const video_color_metadata_t *color, unsigned chroma_position,
      videotoolbox_vp9_register_t register_decoder,
      const VTDecompressionOutputCallbackRecord *callback,
      CMVideoFormatDescriptionRef *format_out, VTDecompressionSessionRef *out)
{
   CMVideoFormatDescriptionRef format = NULL;
   CFDictionaryRef specification = NULL, surfaces = NULL, attributes = NULL;
   CFNumberRef number = NULL;
   const void *keys[3], *values[3];
   OSStatus status;
   int32_t pixel;
   if (format_out)
      *format_out = NULL;
   if (out)
      *out = NULL;
   if (!format_out || !out || (void *)format_out == (void *)out || !callback
         || !callback->decompressionOutputCallback)
      return kCMFormatDescriptionError_InvalidParameter;
   status = videotoolbox_vp9_format_create(
         header, color, chroma_position, &format);
   if (status)
      return status;
   if (register_decoder)
      register_decoder(VIDEOTOOLBOX_VP9_CODEC);
   pixel =
         header->bit_depth == 10
               ? (header->color_range ? 0x78663230 : 0x78343230)
               : (header->color_range
                             ? kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
                             : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange);
   status = kCMFormatDescriptionError_AllocationFailed;
   keys[0] =
         kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder;
   values[0]     = kCFBooleanTrue;
   specification = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   if (!specification)
      goto done;
   number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &pixel);
   if (!number)
      goto done;
   surfaces = CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0,
         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   if (!surfaces)
      goto done;
   keys[0]    = kCVPixelBufferPixelFormatTypeKey;
   keys[1]    = kCVPixelBufferMetalCompatibilityKey;
   keys[2]    = kCVPixelBufferIOSurfacePropertiesKey;
   values[0]  = number;
   values[1]  = kCFBooleanTrue;
   values[2]  = surfaces;
   attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 3,
         &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   if (!attributes)
      goto done;
   status = VTDecompressionSessionCreate(
         kCFAllocatorDefault, format, specification, attributes, callback, out);
   if (!status && !*out)
      status = kCMFormatDescriptionError_AllocationFailed;
done:
   if (attributes)
      CFRelease(attributes);
   if (surfaces)
      CFRelease(surfaces);
   if (number)
      CFRelease(number);
   if (specification)
      CFRelease(specification);
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

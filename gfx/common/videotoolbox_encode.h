/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RETROARCH_VIDEOTOOLBOX_ENCODE_H
#define RETROARCH_VIDEOTOOLBOX_ENCODE_H
#include <stdint.h>
#include "videotoolbox_encode_slots.h"
#if defined(VIDEOTOOLBOX_ENCODE_SLOTS)
#if defined(__APPLE__) && defined(__clang__)
#define VT_ENCODE_AVAILABLE __attribute__((availability(macos,introduced=10.13), availability(ios,introduced=11.0), availability(tvos,introduced=11.0)))
#else
#define VT_ENCODE_AVAILABLE
#endif
typedef struct videotoolbox_encode_config
{
   unsigned width, height, slots, bitrate, key_interval, timescale, bit_depth;
   bool hevc, hdr; /* HDR is limited-range Main10 BT.2020/PQ. */
} videotoolbox_encode_config_t;
typedef struct videotoolbox_encode
{
   VTCompressionSessionRef session;
   videotoolbox_encode_slots_t *pool;
   videotoolbox_encode_config_t config;
   unsigned order[VIDEOTOOLBOX_ENCODE_SLOTS], read, count;
   int64_t last_pts;
   OSStatus error;
   bool submitted, closed, drained;
} videotoolbox_encode_t;

static INLINE bool videotoolbox_encode_number(CFMutableDictionaryRef dict,
      CFStringRef key, int32_t value)
{
   CFNumberRef number = CFNumberCreate(NULL, kCFNumberSInt32Type, &value);
   if (!number) return false;
   CFDictionarySetValue(dict, key, number); CFRelease(number); return true;
}

/* Worker only. Invalidate may wait for native callbacks: never call on the
 * render thread. No borrowed packet may be used after this cancellation. */
static INLINE VT_ENCODE_AVAILABLE void videotoolbox_encode_cancel(videotoolbox_encode_t *encoder)
{
   if (!encoder) return;
   videotoolbox_encode_slots_close(encoder->pool);
   if (encoder->session)
   {
      VTCompressionSessionInvalidate(encoder->session);
      CFRelease(encoder->session);
   }
   videotoolbox_encode_slots_quiesce(encoder->pool);
   videotoolbox_encode_slots_release(encoder->pool);
   free(encoder);
}

/* Cold worker startup; hardware is required and independently queried.
 * Unsupported codec/format/property/device returns an error, not a fallback. */
static INLINE VT_ENCODE_AVAILABLE videotoolbox_encode_t *videotoolbox_encode_new_selected(
      const videotoolbox_encode_config_t *config, CFStringRef encoder_id,uint64_t gpu_registry_id,OSStatus *error)
{
   videotoolbox_encode_t *encoder = NULL;
   CFMutableDictionaryRef spec = NULL, attrs = NULL, props = NULL;
   CFTypeRef hardware = NULL, selected_id = NULL;
   OSStatus status = kVTParameterErr;
   if (error) *error = status;
   if ((encoder_id && !CFStringGetLength(encoder_id)) || !config || !config->width || !config->height || (config->width & 1)
         || (config->height & 1) || config->width > 16384 || config->height > 16384
         || config->slots < 2 || config->slots > VIDEOTOOLBOX_ENCODE_SLOTS
         || !config->bitrate || config->bitrate > INT32_MAX
         || !config->key_interval || config->key_interval > INT32_MAX
         || !config->timescale || config->timescale > INT32_MAX
         || (config->bit_depth != 8 && config->bit_depth != 10)
         || (config->bit_depth == 10 && !config->hevc)
         || (config->hdr && config->bit_depth != 10)) return NULL;
   encoder = (videotoolbox_encode_t*)calloc(1, sizeof(*encoder));
   spec = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   attrs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   props = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
   if (!encoder || !spec || !attrs || !props) { status = kVTAllocationFailedErr; goto done; }
   encoder->config = *config;
   encoder->pool = videotoolbox_encode_slots_new(config->slots);
   if (!encoder->pool) { status = kVTAllocationFailedErr; goto done; }
   CFDictionarySetValue(spec, kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder, kCFBooleanTrue);
   if (encoder_id) CFDictionarySetValue(spec,kVTVideoEncoderSpecification_EncoderID,encoder_id);
   if (gpu_registry_id)
   {
#if defined(VIDEOTOOLBOX_ENCODE_TEST_GPU_SELECTION) || (defined(__APPLE__) && defined(__clang__) \
      && !defined(VIDEOTOOLBOX_ENCODE_TEST_TYPES) && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) \
      && __MAC_OS_X_VERSION_MAX_ALLOWED>=101400)
#ifdef VIDEOTOOLBOX_ENCODE_TEST_GPU_SELECTION
      if (true)
#else
      if (__builtin_available(macOS 10.14, iOS 13.0, tvOS 13.0, *))
#endif
      {
         CFNumberRef gpu=CFNumberCreate(NULL,kCFNumberSInt64Type,&gpu_registry_id);
         if (!gpu) { status=kVTAllocationFailedErr; goto done; }
         CFDictionarySetValue(spec,kVTVideoEncoderSpecification_RequiredEncoderGPURegistryID,gpu);
         CFRelease(gpu);
      }
      else
#endif
      { status=kVTParameterErr; goto done; }
   }
   CFDictionarySetValue(attrs, kCVPixelBufferMetalCompatibilityKey, kCFBooleanTrue);
   if (!videotoolbox_encode_number(attrs, kCVPixelBufferWidthKey, config->width)
         || !videotoolbox_encode_number(attrs, kCVPixelBufferHeightKey, config->height)
         || !videotoolbox_encode_number(attrs, kCVPixelBufferPixelFormatTypeKey,
            config->bit_depth == 10 ? kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange
                        : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
         || (config->hevc && config->bit_depth == 10
            && (!videotoolbox_encode_number(attrs, kCVPixelBufferExtendedPixelsRightKey,
                     (32-(config->width&31))&31)
               || !videotoolbox_encode_number(attrs, kCVPixelBufferExtendedPixelsBottomKey,
                     (32-(config->height&31))&31)))
         || !videotoolbox_encode_number(props, kVTCompressionPropertyKey_AverageBitRate, config->bitrate)
         || !videotoolbox_encode_number(props, kVTCompressionPropertyKey_MaxKeyFrameInterval, config->key_interval)
         || !videotoolbox_encode_number(props, kVTCompressionPropertyKey_MaxFrameDelayCount, 1))
   { status = kVTAllocationFailedErr; goto done; }
   CFDictionarySetValue(props, kVTCompressionPropertyKey_RealTime, kCFBooleanTrue);
   CFDictionarySetValue(props, kVTCompressionPropertyKey_AllowFrameReordering, kCFBooleanFalse);
   CFDictionarySetValue(props, kVTCompressionPropertyKey_ProfileLevel,
         config->hevc ? (config->bit_depth == 10 ? kVTProfileLevel_HEVC_Main10_AutoLevel
               : kVTProfileLevel_HEVC_Main_AutoLevel) : kVTProfileLevel_H264_High_AutoLevel);
   CFDictionarySetValue(props, kVTCompressionPropertyKey_ColorPrimaries,
         config->hdr ? kCVImageBufferColorPrimaries_ITU_R_2020 : kCVImageBufferColorPrimaries_ITU_R_709_2);
   CFDictionarySetValue(props, kVTCompressionPropertyKey_TransferFunction,
         config->hdr ? kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ : kCVImageBufferTransferFunction_ITU_R_709_2);
   CFDictionarySetValue(props, kVTCompressionPropertyKey_YCbCrMatrix,
         config->hdr ? kCVImageBufferYCbCrMatrix_ITU_R_2020 : kCVImageBufferYCbCrMatrix_ITU_R_709_2);
   status = VTCompressionSessionCreate(NULL, config->width, config->height,
         config->hevc ? kCMVideoCodecType_HEVC : kCMVideoCodecType_H264,
         spec, attrs, NULL, videotoolbox_encode_slots_output, NULL, &encoder->session);
   if (status || !encoder->session) goto done;
   status = VTSessionSetProperties(encoder->session, props);
   if (status) goto done;
   status = VTCompressionSessionPrepareToEncodeFrames(encoder->session);
   if (status) goto done;
   status = VTSessionCopyProperty(encoder->session,
         kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder, NULL, &hardware);
   if (!status && (!hardware || CFGetTypeID(hardware) != CFBooleanGetTypeID()
            || !CFBooleanGetValue((CFBooleanRef)hardware))) status = kVTCouldNotFindVideoEncoderErr;
   if (!status && encoder_id)
   {
      status=VTSessionCopyProperty(encoder->session,kVTCompressionPropertyKey_EncoderID,NULL,&selected_id);
      if (!status && (!selected_id || CFGetTypeID(selected_id)!=CFStringGetTypeID()
               || !CFEqual(selected_id,encoder_id))) status=kVTCouldNotFindVideoEncoderErr;
   }
done:
   if (selected_id) CFRelease(selected_id);
   if (hardware) CFRelease(hardware);
   if (props) CFRelease(props);
   if (attrs) CFRelease(attrs);
   if (spec) CFRelease(spec);
   if (!status && (!encoder || !encoder->session)) status = kVTCouldNotFindVideoEncoderErr;
   if (error) *error = status;
   if (status) { videotoolbox_encode_cancel(encoder); return NULL; }
   return encoder;
}

/* Explicit selected ID is verified against the actual session, in addition
 * to hardware acceleration. NULL ID preserves system automatic selection. */
static INLINE VT_ENCODE_AVAILABLE videotoolbox_encode_t *videotoolbox_encode_new_with_gpu(
      const videotoolbox_encode_config_t *config,uint64_t gpu_registry_id,OSStatus *error)
{ return videotoolbox_encode_new_selected(config,NULL,gpu_registry_id,error); }

/* Original automatic-selection API. An explicit nonzero GPU registry ID in
 * new_with_gpu requires that encoder device; no preferred-device fallback. */
static INLINE VT_ENCODE_AVAILABLE videotoolbox_encode_t *videotoolbox_encode_new(
      const videotoolbox_encode_config_t *config, OSStatus *error)
{ return videotoolbox_encode_new_with_gpu(config,0,error); }

/* Worker only, after GPU completion. 1 accepted, 0 busy, -1 failed.
 * No pixel lock/copy; the fixed buffer stays immutable through packet ack. */
static INLINE VT_ENCODE_AVAILABLE int videotoolbox_encode_submit(videotoolbox_encode_t *encoder,
      unsigned index, CVPixelBufferRef input, int64_t pts, int64_t duration)
{
   videotoolbox_encode_slot_t *ticket;
   VTEncodeInfoFlags flags = 0;
   OSStatus status;
   if (!encoder || encoder->closed || encoder->error || !input || pts < 0 || duration <= 0
         || (encoder->submitted && pts <= encoder->last_pts)
         || index >= encoder->config.slots
         || CVPixelBufferGetWidth(input) != encoder->config.width
         || CVPixelBufferGetHeight(input) != encoder->config.height
         || CVPixelBufferGetPixelFormatType(input) != (encoder->config.bit_depth == 10
            ? kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange
            : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)) return -1;
   if (encoder->count == encoder->config.slots) return 0;
   ticket = videotoolbox_encode_slots_begin(encoder->pool, index, input);
   if (!ticket) return 0;
   status = VTCompressionSessionEncodeFrame(encoder->session, input,
         CMTimeMake(pts, encoder->config.timescale), CMTimeMake(duration, encoder->config.timescale),
         NULL, ticket, &flags);
   if (status)
   {
      videotoolbox_encode_slots_reject(ticket);
      encoder->error = status; return -1;
   }
   encoder->order[(encoder->read + encoder->count) % VIDEOTOOLBOX_ENCODE_SLOTS] = index;
   encoder->count++; encoder->submitted = true; encoder->last_pts = pts;
   return 1;
}

/* Borrow in submission order; errors/drops stay visible in the returned slot. */
static INLINE const videotoolbox_encode_slot_t *videotoolbox_encode_peek(
      videotoolbox_encode_t *encoder, unsigned *index)
{
   unsigned head;
   if (!encoder || !index || !encoder->count) return NULL;
   head = encoder->order[encoder->read];
   if (!videotoolbox_encode_slots_peek(encoder->pool, head)) return NULL;
   *index = head;
   return &encoder->pool->slot[head];
}
static INLINE bool videotoolbox_encode_ack(videotoolbox_encode_t *encoder)
{
   unsigned index;
   if (!videotoolbox_encode_peek(encoder, &index)
         || !videotoolbox_encode_slots_ack(encoder->pool, index)) return false;
   encoder->read = (encoder->read + 1) % VIDEOTOOLBOX_ENCODE_SLOTS;
   encoder->count--; return true;
}

/* Dedicated worker drain only: CompleteFrames may block. The controller and
 * renderer poll their worker, rather than invoking this function themselves. */
static INLINE VT_ENCODE_AVAILABLE bool videotoolbox_encode_finish(videotoolbox_encode_t *encoder)
{
   if (!encoder || encoder->error) return false;
   if (encoder->drained) return true;
   encoder->closed = true; videotoolbox_encode_slots_close(encoder->pool);
   encoder->error = VTCompressionSessionCompleteFrames(encoder->session, kCMTimeInvalid);
   encoder->drained = !encoder->error;
   return encoder->drained;
}
static INLINE VT_ENCODE_AVAILABLE bool videotoolbox_encode_free(videotoolbox_encode_t *encoder)
{
   if (!encoder) return true;
   if (!encoder->drained || encoder->count) return false;
   videotoolbox_encode_cancel(encoder); return true;
}
#endif
#endif

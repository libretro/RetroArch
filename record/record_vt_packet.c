/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_vt_packet.h"
#include <stdlib.h>
#if defined(__APPLE__) || defined(RECORD_VT_PACKET_TEST)
#if defined(__APPLE__) && !defined(__cplusplus) \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L) && !defined(inline)
#define inline __inline__
#define RECORD_VT_PACKET_UNDEF_INLINE
#endif
#include <CoreMedia/CoreMedia.h>
#ifdef RECORD_VT_PACKET_UNDEF_INLINE
#undef inline
#undef RECORD_VT_PACKET_UNDEF_INLINE
#endif
struct record_vt_packet
{
   CMFormatDescriptionRef format;
   CMSampleBufferRef sample;
   uint8_t *storage;
   size_t capacity;
   record_vt_packet_view_t view;
   unsigned timescale;
};
static bool record_vt_packet_time(CMTime source, unsigned scale, uint64_t *value)
{
   CMTime converted;
   if (!CMTIME_IS_NUMERIC(source) || source.value < 0 || source.epoch)
      return false;
   converted = CMTimeConvertScale(source, (int32_t)scale, kCMTimeRoundingMethod_RoundTowardZero);
   if (!CMTIME_IS_NUMERIC(converted) || converted.value < 0
         || CMTimeCompare(source, converted))
      return false;
   *value = (uint64_t)converted.value;
   return true;
}
record_vt_packet_t *record_vt_packet_new(void *format, unsigned timescale, size_t capacity)
{
   record_vt_packet_t *p;
   int lengths = 0;
   OSStatus status;
   FourCharCode codec;
   if (!format || !timescale || timescale > 0x7fffffffU || !capacity || capacity > 0xfffff7U
         || CMFormatDescriptionGetMediaType(format) != kCMMediaType_Video)
      return NULL;
   codec = CMFormatDescriptionGetMediaSubType(format);
   if (codec == kCMVideoCodecType_H264)
      status = CMVideoFormatDescriptionGetH264ParameterSetAtIndex(format, 0, NULL, NULL, NULL, &lengths);
   else if (codec == kCMVideoCodecType_HEVC)
      status = CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(format, 0, NULL, NULL, NULL, &lengths);
   else
      return NULL;
   if (status || lengths != 4)
      return NULL;
   p = (record_vt_packet_t*)calloc(1, sizeof(*p));
   if (!p)
      return NULL;
   p->storage = (uint8_t*)malloc(capacity);
   if (!p->storage)
   {
      free(p);
      return NULL;
   }
   p->format = format;
   p->capacity = capacity;
   p->timescale = timescale;
   return p;
}
int record_vt_packet_read(record_vt_packet_t *p, void *data, record_vt_packet_view_t *view)
{
   CMSampleBufferRef sample = (CMSampleBufferRef)data;
   CMBlockBufferRef block;
   CFArrayRef attachments;
   CFDictionaryRef attachment;
   CFTypeRef sync;
   char *bytes = NULL;
   size_t contiguous = 0, total = 0;
   record_vt_packet_view_t next;
   if (!p || !sample || !view)
      return -1;
   if (p->sample)
   {
      if (p->sample != sample)
         return 0;
      *view = p->view;
      return 1;
   }
   if (!CMSampleBufferDataIsReady(sample) || CMSampleBufferGetNumSamples(sample) != 1
         || !CMSampleBufferGetFormatDescription(sample)
         || !CMFormatDescriptionEqual(p->format, CMSampleBufferGetFormatDescription(sample)))
      return -1;
   if (!record_vt_packet_time(CMSampleBufferGetPresentationTimeStamp(sample), p->timescale, &next.pts))
      return -1;
   if (!record_vt_packet_time(CMSampleBufferGetDecodeTimeStamp(sample), p->timescale, &next.dts))
   {
      if (CMTIME_IS_VALID(CMSampleBufferGetDecodeTimeStamp(sample)))
         return -1;
      next.dts = next.pts;
   }
   block = CMSampleBufferGetDataBuffer(sample);
   if (!block || CMBlockBufferGetDataPointer(block, 0, &contiguous, &total, &bytes)
         || !bytes || !total || total > p->capacity || contiguous > total
         || CMSampleBufferGetSampleSize(sample, 0) != total)
      return -1;
   next.keyframe = true;
   attachments = CMSampleBufferGetSampleAttachmentsArray(sample, false);
   if (attachments)
   {
      if (CFArrayGetCount(attachments) != 1)
         return -1;
      attachment = (CFDictionaryRef)CFArrayGetValueAtIndex(attachments, 0);
      if (!attachment || CFGetTypeID(attachment) != CFDictionaryGetTypeID())
         return -1;
      sync = CFDictionaryGetValue(attachment, kCMSampleAttachmentKey_NotSync);
      if (sync)
      {
         if (CFGetTypeID(sync) != CFBooleanGetTypeID())
            return -1;
         next.keyframe = !CFBooleanGetValue((CFBooleanRef)sync);
      }
   }
   if (contiguous != total)
   {
      if (CMBlockBufferCopyDataBytes(block, 0, total, p->storage))
         return -1;
      bytes = (char*)p->storage;
   }
   next.data = bytes;
   next.size = total;
   p->sample = sample;
   p->view = next;
   *view = next;
   return 1;
}
bool record_vt_packet_release(record_vt_packet_t *p)
{
   if (!p || !p->sample)
      return false;
   p->sample = NULL;
   return true;
}
bool record_vt_packet_free(record_vt_packet_t *p)
{
   if (!p)
      return true;
   if (p->sample)
      return false;
   free(p->storage);
   free(p);
   return true;
}
#endif

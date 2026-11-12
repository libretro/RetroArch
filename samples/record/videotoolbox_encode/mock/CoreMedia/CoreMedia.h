#ifndef MOCK_VT_PACKET_COREMEDIA_H
#define MOCK_VT_PACKET_COREMEDIA_H
#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
typedef int OSStatus;
typedef unsigned FourCharCode;
typedef const void *CFTypeRef;
typedef const void *CFBooleanRef;
typedef const void *CFDictionaryRef;
typedef const void *CFArrayRef;
typedef struct mock_format { unsigned codec, media; int lengths, error; } *CMFormatDescriptionRef;
typedef struct mock_block { unsigned char bytes[32]; size_t size, contiguous; int error, copy_error; } *CMBlockBufferRef;
typedef struct { int64_t value; int32_t timescale; unsigned flags; int64_t epoch; } CMTime;
typedef struct mock_sample
{
   CMFormatDescriptionRef format;
   CMBlockBufferRef block;
   CMTime pts, dts;
   size_t sample_size;
   int ready, samples, attachments, not_sync;
} *CMSampleBufferRef;
#define kCMMediaType_Video 1u
#define kCMVideoCodecType_H264 2u
#define kCMVideoCodecType_HEVC 3u
#define kCMTimeRoundingMethod_RoundTowardZero 0
#define CMTIME_IS_VALID(t) (((t).flags & 1) != 0)
#define CMTIME_IS_NUMERIC(t) ((t).flags == 1 && (t).timescale > 0)
#define kCMSampleAttachmentKey_NotSync ((const void*)1)
unsigned CMFormatDescriptionGetMediaType(CMFormatDescriptionRef f);
unsigned CMFormatDescriptionGetMediaSubType(CMFormatDescriptionRef f);
int CMFormatDescriptionEqual(CMFormatDescriptionRef a, CMFormatDescriptionRef b);
int CMVideoFormatDescriptionGetH264ParameterSetAtIndex(CMFormatDescriptionRef f, size_t i, const uint8_t **p, size_t *n, size_t *count, int *lengths);
int CMVideoFormatDescriptionGetHEVCParameterSetAtIndex(CMFormatDescriptionRef f, size_t i, const uint8_t **p, size_t *n, size_t *count, int *lengths);
CMTime CMTimeConvertScale(CMTime t, int32_t s, int method);
int CMTimeCompare(CMTime a, CMTime b);
int CMSampleBufferDataIsReady(CMSampleBufferRef s);
int CMSampleBufferGetNumSamples(CMSampleBufferRef s);
CMFormatDescriptionRef CMSampleBufferGetFormatDescription(CMSampleBufferRef s);
CMTime CMSampleBufferGetPresentationTimeStamp(CMSampleBufferRef s);
CMTime CMSampleBufferGetDecodeTimeStamp(CMSampleBufferRef s);
CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef s);
size_t CMSampleBufferGetSampleSize(CMSampleBufferRef s, size_t i);
CFArrayRef CMSampleBufferGetSampleAttachmentsArray(CMSampleBufferRef s, bool create);
int CMBlockBufferGetDataPointer(CMBlockBufferRef b, size_t off, size_t *n, size_t *total, char **p);
int CMBlockBufferCopyDataBytes(CMBlockBufferRef b, size_t off, size_t n, void *p);
long CFArrayGetCount(CFArrayRef a);
const void *CFArrayGetValueAtIndex(CFArrayRef a, long i);
unsigned CFGetTypeID(CFTypeRef p);
unsigned CFDictionaryGetTypeID(void);
unsigned CFBooleanGetTypeID(void);
const void *CFDictionaryGetValue(CFDictionaryRef d, const void *key);
bool CFBooleanGetValue(CFBooleanRef b);
#endif

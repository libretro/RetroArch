/* Compile-only declarations; no runtime implementation. */
/* Minimal CoreMedia/VideoToolbox ABI for the portable setup fixture. */
#ifndef STUB_HEVC_FRAMEWORK_H
#define STUB_HEVC_FRAMEWORK_H
#include <CoreVideo/CoreVideo.h>
#include <stdint.h>
#include <stddef.h>
typedef int32_t OSStatus;
typedef const void *CFDictionaryRef;
typedef const void *CFNumberRef;
typedef const void *CMVideoFormatDescriptionRef;
typedef const void *VTDecompressionSessionRef;
typedef const void *CMBlockBufferRef;
typedef const void *CMSampleBufferRef;
typedef const void *CVImageBufferRef;

typedef unsigned VTDecodeInfoFlags;
typedef struct
{
   int64_t value;
   int32_t timescale;
   uint32_t flags;
   int64_t epoch;
} CMTime;
static const CMTime kCMTimeInvalid = {0, 0, 0, 0};
typedef struct
{
   CMTime duration, presentationTimeStamp, decodeTimeStamp;
} CMSampleTimingInfo;
typedef struct { int32_t width, height; } CMVideoDimensions;
typedef void (*VTDecompressionOutputCallback)(void*, void*, OSStatus,
      VTDecodeInfoFlags, CVImageBufferRef, CMTime, CMTime);
typedef struct
{
   VTDecompressionOutputCallback decompressionOutputCallback;
   void *decompressionOutputRefCon;
} VTDecompressionOutputCallbackRecord;
#define kCFAllocatorDefault NULL
#define kVTDecodeFrame_EnableAsynchronousDecompression 1
#define kVTDecodeInfo_Asynchronous 1
#define kVTDecodeInfo_FrameDropped 2
#define kCMFormatDescriptionError_InvalidParameter (-12710)
#define kCMFormatDescriptionError_AllocationFailed (-12711)
#define kCFNumberSInt32Type 3


#define kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder ((void*)1)
#define kCVPixelBufferPixelFormatTypeKey ((void*)2)
#define kCVPixelBufferMetalCompatibilityKey ((void*)3)
#define kCFBooleanTrue ((void*)4)
static const int kCFTypeDictionaryKeyCallBacks = 0;
static const int kCFTypeDictionaryValueCallBacks = 0;
OSStatus CMVideoFormatDescriptionCreateFromHEVCParameterSets(void*, size_t,
      const uint8_t *const*, const size_t*, int, void*, CMVideoFormatDescriptionRef*);
CFDictionaryRef CFDictionaryCreate(void*, const void**, const void**, int,
      const void*, const void*);
CFNumberRef CFNumberCreate(void*, int, const void*);

OSStatus VTDecompressionSessionCreate(void*, CMVideoFormatDescriptionRef,
      CFDictionaryRef, CFDictionaryRef,
      const VTDecompressionOutputCallbackRecord*, VTDecompressionSessionRef*);
void VTDecompressionSessionInvalidate(VTDecompressionSessionRef);
OSStatus VTDecompressionSessionWaitForAsynchronousFrames(VTDecompressionSessionRef);
CMVideoDimensions CMVideoFormatDescriptionGetDimensions(CMVideoFormatDescriptionRef);
OSStatus CMBlockBufferCreateWithMemoryBlock(void*, void*, size_t, void*,
      const void*, size_t, size_t, unsigned, CMBlockBufferRef*);
OSStatus CMBlockBufferAssureBlockMemory(CMBlockBufferRef);
OSStatus CMBlockBufferReplaceDataBytes(const void*, CMBlockBufferRef, size_t, size_t);
OSStatus CMBlockBufferCopyDataBytes(CMBlockBufferRef, size_t, size_t, void*);
OSStatus CMSampleBufferCreateReady(void*, CMBlockBufferRef, CMVideoFormatDescriptionRef,
      long, long, const CMSampleTimingInfo*, long, const size_t*, CMSampleBufferRef*);
CMBlockBufferRef CMSampleBufferGetDataBuffer(CMSampleBufferRef);
size_t CMSampleBufferGetTotalSampleSize(CMSampleBufferRef);
OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef, CMSampleBufferRef,
      unsigned, void*, VTDecodeInfoFlags*);
CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef);
void CVPixelBufferRelease(CVPixelBufferRef);
#endif

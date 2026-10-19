/* Minimal CoreMedia/VideoToolbox ABI for the portable setup fixture. */
#ifndef VT_HEVC_MOCK_APPLE_H
#define VT_HEVC_MOCK_APPLE_H
#include <stdint.h>
#include <stddef.h>
typedef int32_t OSStatus;
typedef struct object *CFDictionaryRef;
typedef struct object *CFNumberRef;
typedef struct object *CMVideoFormatDescriptionRef;
typedef struct object *VTDecompressionSessionRef;
typedef struct object *CMBlockBufferRef;
typedef struct object *CMSampleBufferRef;
typedef void *CVImageBufferRef;
typedef struct object *CVPixelBufferRef;
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
#define kCVPixelFormatType_420YpCbCr8BiPlanarFullRange 0x34323066
#define kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange 0x34323076
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
void CFRelease(void*);
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

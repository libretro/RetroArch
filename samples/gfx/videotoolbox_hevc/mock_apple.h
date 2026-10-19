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
typedef void *CVImageBufferRef;
typedef unsigned VTDecodeInfoFlags;
typedef int64_t CMTime;
typedef struct { int32_t width, height; } CMVideoDimensions;
typedef void (*VTDecompressionOutputCallback)(void*, void*, OSStatus,
      VTDecodeInfoFlags, CVImageBufferRef, CMTime, CMTime);
typedef struct
{
   VTDecompressionOutputCallback decompressionOutputCallback;
   void *decompressionOutputRefCon;
} VTDecompressionOutputCallbackRecord;
#define kCFAllocatorDefault NULL
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
CMVideoDimensions CMVideoFormatDescriptionGetDimensions(CMVideoFormatDescriptionRef);
#endif

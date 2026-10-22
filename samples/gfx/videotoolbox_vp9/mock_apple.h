/* Minimal CoreMedia/CoreVideo/VideoToolbox declarations for setup tests. */
#ifndef VT_VP9_MOCK_APPLE_H
#define VT_VP9_MOCK_APPLE_H
#include <stddef.h>
#include <stdint.h>
typedef int32_t OSStatus;
typedef const char *CFStringRef;
typedef uint32_t CMVideoCodecType;
typedef struct object *CFDataRef;
typedef struct object *CFDictionaryRef;
typedef struct object *CFNumberRef;
typedef struct object *CMVideoFormatDescriptionRef;
typedef struct object *VTDecompressionSessionRef;
typedef void *CVImageBufferRef;
typedef struct object *CVPixelBufferRef;
CVPixelBufferRef CVPixelBufferRetain(CVPixelBufferRef);
void CVPixelBufferRelease(CVPixelBufferRef);
typedef unsigned VTDecodeInfoFlags;
typedef unsigned VTDecodeFrameFlags;
typedef struct object *CMBlockBufferRef;
typedef struct object *CMSampleBufferRef;

typedef struct
{
   int64_t value;
   int32_t timescale;
   uint32_t flags;
   int64_t epoch;
} CMTime;

typedef struct
{
   CMTime duration, presentationTimeStamp, decodeTimeStamp;
} CMSampleTimingInfo;

typedef struct
{
   uint32_t version;
   void *(*AllocateBlock)(void *, size_t);
   void (*FreeBlock)(void *, void *, size_t);
   void *refCon;
} CMBlockBufferCustomBlockSource;

#define kCFAllocatorNull ((void *)2)
#define kCMBlockBufferCustomBlockSourceVersion 0
#define kVTDecodeFrame_EnableAsynchronousDecompression 1
#define kVTDecodeFrame_DoNotOutputFrame 2
#define kVTDecodeInfo_FrameDropped 2
static const CMTime kCMTimeInvalid = {0, 0, 0, 0};
OSStatus CMBlockBufferCreateWithMemoryBlock(void *, void *, size_t, void *,
      const CMBlockBufferCustomBlockSource *, size_t, size_t, unsigned,
      CMBlockBufferRef *);
OSStatus CMSampleBufferCreateReady(void *, CMBlockBufferRef,
      CMVideoFormatDescriptionRef, long, long, const CMSampleTimingInfo *, long,
      const size_t *, CMSampleBufferRef *);
OSStatus VTDecompressionSessionDecodeFrame(VTDecompressionSessionRef,
      CMSampleBufferRef, VTDecodeFrameFlags, void *, VTDecodeInfoFlags *);

typedef struct
{
   int32_t width, height;
} CMVideoDimensions;

typedef void (*VTDecompressionOutputCallback)(void *, void *, OSStatus,
      VTDecodeInfoFlags, CVImageBufferRef, CMTime, CMTime);

typedef struct
{
   VTDecompressionOutputCallback decompressionOutputCallback;
   void *decompressionOutputRefCon;
} VTDecompressionOutputCallbackRecord;

#define CFSTR(s) (s)
#define kCFAllocatorDefault NULL
#define kCFBooleanTrue ((void *)1)
#define kCFBooleanFalse ((void *)2)
#define kCFNumberSInt32Type 3
#define kCMFormatDescriptionError_InvalidParameter (-12710)
#define kCMFormatDescriptionError_AllocationFailed (-12711)
#define kCMFormatDescriptionExtension_SampleDescriptionExtensionAtoms "atoms"
#define kCMFormatDescriptionExtension_ColorPrimaries "ColorPrimaries"
#define kCMFormatDescriptionExtension_TransferFunction "TransferFunction"
#define kCMFormatDescriptionExtension_YCbCrMatrix "YCbCrMatrix"
#define kCMFormatDescriptionExtension_FullRangeVideo "FullRangeVideo"
extern const char kCVImageBufferColorPrimaries_ITU_R_709_2[];
extern const char kCVImageBufferColorPrimaries_ITU_R_2020[];
extern const char kCVImageBufferTransferFunction_ITU_R_709_2[];
extern const char kCVImageBufferTransferFunction_sRGB[];
extern const char kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ[];
extern const char kCVImageBufferTransferFunction_ITU_R_2100_HLG[];
extern const char kCVImageBufferYCbCrMatrix_ITU_R_709_2[];
extern const char kCVImageBufferYCbCrMatrix_ITU_R_2020[];
#define kVTVideoDecoderSpecification_RequireHardwareAcceleratedVideoDecoder    \
   "hardware"
#define kCVPixelBufferPixelFormatTypeKey "pixel"
#define kCVPixelBufferMetalCompatibilityKey "metal"
#define kCVPixelBufferIOSurfacePropertiesKey "surface"
#define kCVPixelFormatType_420YpCbCr8BiPlanarFullRange 0x34323066
#define kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange 0x34323076
static const int kCFTypeDictionaryKeyCallBacks   = 0;
static const int kCFTypeDictionaryValueCallBacks = 0;
CFDataRef CFDataCreate(void *, const uint8_t *, long);
CFNumberRef CFNumberCreate(void *, int, const void *);
CFDictionaryRef CFDictionaryCreate(
      void *, const void **, const void **, long, const void *, const void *);
void CFRelease(void *);
OSStatus CMVideoFormatDescriptionCreate(void *, CMVideoCodecType, int32_t,
      int32_t, CFDictionaryRef, CMVideoFormatDescriptionRef *);
OSStatus VTDecompressionSessionCreate(void *, CMVideoFormatDescriptionRef,
      CFDictionaryRef, CFDictionaryRef,
      const VTDecompressionOutputCallbackRecord *, VTDecompressionSessionRef *);
void VTDecompressionSessionInvalidate(VTDecompressionSessionRef);
OSStatus VTDecompressionSessionWaitForAsynchronousFrames(
      VTDecompressionSessionRef);
#endif

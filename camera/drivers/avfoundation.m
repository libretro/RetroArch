/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025      - Joseph Mattiello
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <TargetConditionals.h>
#include "../../apple_runtime.h"
#include <Foundation/Foundation.h>
#include <AVFoundation/AVFoundation.h>
#include <libretro.h>
#include <defines/cocoa_defines.h>
/* For image scaling and color space DSP */
#import <Accelerate/Accelerate.h>
#if TARGET_OS_IOS
/* For camera rotation detection */
#import <UIKit/UIKit.h>
#endif

#include "../camera_driver.h"
#include "avfoundation_state.h"
#include "../../verbosity.h"

/* TODO: Add an API to retroarch to allow selection of camera */
#ifndef CAMERA_PREFER_FRONTFACING
#define CAMERA_PREFER_FRONTFACING 1  /* Default to front camera */
#endif

#ifndef CAMERA_MIRROR_FRONT_CAMERA
#define CAMERA_MIRROR_FRONT_CAMERA 1
#endif

@interface AVCameraManager : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property (strong, nonatomic) AVCaptureSession *session;
@property (strong, nonatomic) AVCaptureDeviceInput *input;
@property (strong, nonatomic) AVCaptureVideoDataOutput *output;
@property (assign) uint32_t *frameBuffer;
@property (assign) size_t width;
@property (assign) size_t height;
/* Frames are kept as the device's 4:2:0 YCbCr and lent to the core */
@property (assign) BOOL wantPlanar;

- (bool)setupCameraSession;
- (void)teardownScratchBuffers;
- (void)requestCameraAuthorizationWithCompletion:(void (^)(BOOL granted))completion;
/* The latest planar frame, retained, the manager holding it no more;
 * NULL when none came since the last */
- (CVPixelBufferRef)takeLatestFrame;
/* The quarter turns vImageRotate90 makes for the frame to be upright,
 * counter-clockwise, in degrees, and whether it is mirrored after, on
 * this device and orientation */
- (int)rotationDegrees:(bool *)mirror;
@end

/* Private class extension: cached per-frame scratch buffers.
 * captureOutput:didOutputSampleBuffer: used to malloc + free four full-
 * frame buffers per callback (intermediate colour-converted, rotated,
 * optional mirrored, and scaled).  For a typical 720p BGRA camera that
 * was ~3.5 MB × 4 allocations × 30 fps = hundreds of MB/s of allocator
 * churn on the main thread (the capture delegate queue is the main
 * queue — see setSampleBufferDelegate:queue: below).  Cache the
 * buffers on the manager and grow only when the required size
 * exceeds the current capacity; free them in teardownScratchBuffers
 * on driver teardown.  Ivars are plain C pointers so the file remains
 * identically correct under MRC and ARC. */
@interface AVCameraManager ()
{
   void    *_intermediateBuf;
   size_t   _intermediateCap;
   void    *_rotatedBuf;
   size_t   _rotatedCap;
   void    *_mirroredBuf;
   size_t   _mirroredCap;
   void    *_scaledBuf;
   size_t   _scaledCap;
@public
   /* The latest planar frame, swapped in and out atomically */
   CVPixelBufferRef _latest;
   /* Authorization and session, answered on the main queue */
   avf_state_t _st;
}
@end

/* Camera authorization is macOS 10.14 (iOS 7): its calls live in a
 * class with that availability, so they are made directly there, and
 * the camera reaches it by selector once it has checked the OS. */
API_AVAILABLE(macos(10.14), ios(7.0))
@interface RACameraAuthorization : NSObject
+ (void)authorizeWithCompletion:(void (^)(BOOL granted))completion;
/* Denied or restricted: known without asking */
+ (BOOL)refused;
@end

@implementation RACameraAuthorization

+ (void)authorizeWithCompletion:(void (^)(BOOL granted))completion
{
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];

    switch (status) {
    case AVAuthorizationStatusAuthorized: {
        RARCH_LOG("[Camera] Camera access already authorized.\n");
        completion(YES);
        break;
    }

    case AVAuthorizationStatusNotDetermined: {

        RARCH_LOG("[Camera] Requesting camera authorization...\n");
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo
                                 completionHandler:^(BOOL granted) {
            RARCH_LOG("[Camera] Authorization %s.\n", granted ? "granted" : "denied");
            completion(granted);
        }];
        break;
    }

    case AVAuthorizationStatusDenied: {
        RARCH_ERR("[Camera] Camera access denied by user.\n");
        completion(NO);
        break;
    }

    case AVAuthorizationStatusRestricted: {
        RARCH_ERR("[Camera] Camera access restricted (parental controls?).\n");
        completion(NO);
        break;
    }

    default: {
        RARCH_ERR("[Camera] Unknown authorization status.\n");
        completion(NO);
        break;
    }
    }
}

+ (BOOL)refused
{
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    return status == AVAuthorizationStatusDenied
        || status == AVAuthorizationStatusRestricted;
}

@end

@implementation AVCameraManager

+ (AVCameraManager *)sharedInstance {
    static AVCameraManager *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        instance = [[AVCameraManager alloc] init];
    });
    return instance;
}

- (void)requestCameraAuthorizationWithCompletion:(void (^)(BOOL granted))completion {
    RARCH_LOG("[Camera] Checking camera authorization status...\n");

    /* AVCaptureDevice authorization gating exists on macOS 10.14+ (and iOS 7+).
     * Earlier macOS had no camera TCC prompt, so access is implicitly granted. */
    if (apple_runtime_available(APPLE_RUNTIME_VER(10, 14, 0), 0, 0)) {
        apple_rt_send_id(apple_rt_class("RACameraAuthorization"),
              @selector(authorizeWithCompletion:), completion);
    } else {
        /* Pre-10.14 macOS: no camera authorization API; access is implicit. */
        RARCH_LOG("[Camera] Authorization API unavailable on this OS; assuming granted.\n");
        completion(YES);
    }
}

- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
    @autoreleasepool {
        /* Planar: the capture buffer is kept as it is, for the next poll
         * to lend; the one it replaces goes back to the pool */
        if (self.wantPlanar) {
            CVPixelBufferRef pb = (CVPixelBufferRef)CMSampleBufferGetImageBuffer(sampleBuffer);
            CVPixelBufferRef old;
            if (!pb)
                return;
            CVPixelBufferRetain(pb);
            old = __atomic_exchange_n(&_latest, pb, __ATOMIC_ACQ_REL);
            if (old)
                CVPixelBufferRelease(old);
            return;
        }

        if (!self.frameBuffer)
            return;

        CVPixelBufferRef imageBuffer = (CVPixelBufferRef)CMSampleBufferGetImageBuffer(sampleBuffer);
        if (!imageBuffer) {
            RARCH_ERR("[Camera] Failed to get image buffer.\n");
            return;
        }

        CVPixelBufferLockBaseAddress(imageBuffer, 0);

        size_t sourceWidth = CVPixelBufferGetWidth(imageBuffer);
        size_t sourceHeight = CVPixelBufferGetHeight(imageBuffer);
        OSType pixelFormat = CVPixelBufferGetPixelFormatType(imageBuffer);

#ifdef DEBUG
        RARCH_LOG("[Camera] Processing frame %zux%zu format: %u.\n", sourceWidth, sourceHeight, (unsigned int)pixelFormat);
#endif
        // Intermediate buffer for full-size converted image.  Cached on
        // the manager - grown only when the required size exceeds the
        // current capacity.  See class extension above for rationale.
        size_t intermediateSize = sourceWidth * sourceHeight * 4;
        if (intermediateSize > _intermediateCap) {
            void *tmp = realloc(_intermediateBuf, intermediateSize);
            if (!tmp) {
                RARCH_ERR("[Camera] Failed to allocate intermediate buffer.\n");
                CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
                return;
            }
            _intermediateBuf = tmp;
            _intermediateCap = intermediateSize;
        }
        uint32_t *intermediateBuffer = (uint32_t*)_intermediateBuf;

        vImage_Buffer srcBuffer = {}, intermediateVBuffer = {}, dstBuffer = {};
        vImage_Error err = kvImageNoError;

        // Setup intermediate buffer
        intermediateVBuffer.data = intermediateBuffer;
        intermediateVBuffer.width = sourceWidth;
        intermediateVBuffer.height = sourceHeight;
        intermediateVBuffer.rowBytes = sourceWidth * 4;

        // Setup destination buffer
        dstBuffer.data = self.frameBuffer;
        dstBuffer.width = self.width;
        dstBuffer.height = self.height;
        dstBuffer.rowBytes = self.width * 4;

        // Convert source format to RGBA
        switch (pixelFormat) {
            case kCVPixelFormatType_32BGRA: {
                srcBuffer.data = CVPixelBufferGetBaseAddress(imageBuffer);
                srcBuffer.width = sourceWidth;
                srcBuffer.height = sourceHeight;
                srcBuffer.rowBytes = CVPixelBufferGetBytesPerRow(imageBuffer);

                uint8_t permuteMap[4] = {2, 1, 0, 3}; // BGRA -> RGBA
                err = vImagePermuteChannels_ARGB8888(&srcBuffer, &intermediateVBuffer, permuteMap, kvImageNoFlags);
                break;
            }

            case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
            case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange: {
                // YUV to RGB conversion
                vImage_Buffer srcY = {}, srcCbCr = {};

                srcY.data = CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 0);
                srcY.width = sourceWidth;
                srcY.height = sourceHeight;
                srcY.rowBytes = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 0);

                srcCbCr.data = CVPixelBufferGetBaseAddressOfPlane(imageBuffer, 1);
                srcCbCr.width = sourceWidth / 2;
                srcCbCr.height = sourceHeight / 2;
                srcCbCr.rowBytes = CVPixelBufferGetBytesPerRowOfPlane(imageBuffer, 1);

                vImage_YpCbCrToARGB info;
                vImage_YpCbCrPixelRange pixelRange =
                    (pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange) ?
                        (vImage_YpCbCrPixelRange){16, 128, 235, 240} :  // Video range
                        (vImage_YpCbCrPixelRange){0, 128, 255, 255};    // Full range

                err = vImageConvert_YpCbCrToARGB_GenerateConversion(kvImage_YpCbCrToARGBMatrix_ITU_R_601_4,
                                                                   &pixelRange,
                                                                   &info,
                                                                   kvImage420Yp8_CbCr8,
                                                                   kvImageARGB8888,
                                                                   kvImageNoFlags);

                if (err == kvImageNoError) {
                    err = vImageConvert_420Yp8_CbCr8ToARGB8888(&srcY,
                                                              &srcCbCr,
                                                              &intermediateVBuffer,
                                                              &info,
                                                              NULL,
                                                              255,
                                                              kvImageNoFlags);
                }
                break;
            }

            default:
                RARCH_ERR("[Camera] Unsupported pixel format: %u.\n", (unsigned int)pixelFormat);
                CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
                return;
        }

        if (err != kvImageNoError) {
            RARCH_ERR("[Camera] Error converting color format: %ld.\n", err);
            CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
            return;
        }

        bool shouldMirror;
        int rotationDegrees = [self rotationDegrees:&shouldMirror];

        // Rotate image (cached scratch buffer)
        size_t rotatedSize = sourceWidth * sourceHeight * 4;
        if (rotatedSize > _rotatedCap) {
            void *tmp = realloc(_rotatedBuf, rotatedSize);
            if (!tmp) {
                RARCH_ERR("[Camera] Failed to allocate rotation buffer.\n");
                CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
                return;
            }
            _rotatedBuf = tmp;
            _rotatedCap = rotatedSize;
        }
        vImage_Buffer rotatedBuffer = {};
        rotatedBuffer.data = _rotatedBuf;

        // Set dimensions based on rotation angle
        if (rotationDegrees == 90 || rotationDegrees == 270) {
            rotatedBuffer.width = sourceHeight;
            rotatedBuffer.height = sourceWidth;
        } else {
            rotatedBuffer.width = sourceWidth;
            rotatedBuffer.height = sourceHeight;
        }
        rotatedBuffer.rowBytes = rotatedBuffer.width * 4;

        const Pixel_8888 backgroundColor = {0, 0, 0, 255};

        err = vImageRotate90_ARGB8888(&intermediateVBuffer,
                                     &rotatedBuffer,
                                     rotationDegrees / 90,
                                     backgroundColor,
                                     kvImageNoFlags);

        if (err != kvImageNoError) {
            RARCH_ERR("[Camera] Error rotating image: %ld.\n", err);
            CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
            return;
        }

        // Mirror the image if needed.  On success, the scale step below
        // reads from the mirrored buffer; on failure it falls back to
        // rotated.  Unlike the original code we don't swap pointers —
        // each scratch has a stable identity across frames.
        vImage_Buffer *scaleSource = &rotatedBuffer;
        vImage_Buffer mirroredBuffer = {};

        if (shouldMirror) {
            size_t mirroredSize = rotatedBuffer.height * rotatedBuffer.rowBytes;
            if (mirroredSize > _mirroredCap) {
                void *tmp = realloc(_mirroredBuf, mirroredSize);
                if (!tmp) {
                    RARCH_ERR("[Camera] Failed to allocate mirror buffer.\n");
                    CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
                    return;
                }
                _mirroredBuf = tmp;
                _mirroredCap = mirroredSize;
            }
            mirroredBuffer.data = _mirroredBuf;
            mirroredBuffer.width = rotatedBuffer.width;
            mirroredBuffer.height = rotatedBuffer.height;
            mirroredBuffer.rowBytes = rotatedBuffer.rowBytes;

            err = vImageHorizontalReflect_ARGB8888(&rotatedBuffer, &mirroredBuffer, kvImageNoFlags);

            if (err == kvImageNoError) {
                scaleSource = &mirroredBuffer;
            } else {
                RARCH_ERR("[Camera] Error mirroring image: %ld.\n", err);
                /* scaleSource stays pointed at rotatedBuffer */
            }
        }

        // Calculate aspect fill scaling
        float sourceAspect = (float)scaleSource->width / scaleSource->height;
        float targetAspect = (float)self.width / self.height;

        vImage_Buffer scaledBuffer = {};
        size_t scaledWidth, scaledHeight;

        if (sourceAspect > targetAspect) {
            // Source is wider - scale to match height
            scaledHeight = self.height;
            scaledWidth = (size_t)(self.height * sourceAspect);
        } else {
            // Source is taller - scale to match width
            scaledWidth = self.width;
            scaledHeight = (size_t)(self.width / sourceAspect);
        }

#ifdef DEBUG
        RARCH_LOG("[Camera] Aspect fill scaling from %zux%zu to %zux%zu.\n",
                  scaleSource->width, scaleSource->height, scaledWidth, scaledHeight);
#endif

        size_t scaledSize = scaledWidth * scaledHeight * 4;
        if (scaledSize > _scaledCap) {
            void *tmp = realloc(_scaledBuf, scaledSize);
            if (!tmp) {
                RARCH_ERR("[Camera] Failed to allocate scaled buffer.\n");
                CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
                return;
            }
            _scaledBuf = tmp;
            _scaledCap = scaledSize;
        }
        scaledBuffer.data = _scaledBuf;
        scaledBuffer.width = scaledWidth;
        scaledBuffer.height = scaledHeight;
        scaledBuffer.rowBytes = scaledWidth * 4;

        // Scale maintaining aspect ratio
        err = vImageScale_ARGB8888(scaleSource, &scaledBuffer, NULL, kvImageHighQualityResampling);

        if (err != kvImageNoError) {
            RARCH_ERR("[Camera] Error scaling image: %ld.\n", err);
            CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
            return;
        }

        // Center crop the scaled image into the destination buffer
        size_t xOffset = (scaledWidth > self.width) ? (scaledWidth - self.width) / 2 : 0;
        size_t yOffset = (scaledHeight > self.height) ? (scaledHeight - self.height) / 2 : 0;

        // Copy the centered portion to the destination buffer
        uint32_t *srcPtr = (uint32_t *)scaledBuffer.data;
        uint32_t *dstPtr = (uint32_t *)self.frameBuffer;

        for (size_t y = 0; y < self.height; y++) {
            memcpy(dstPtr + y * self.width,
                   srcPtr + (y + yOffset) * scaledWidth + xOffset,
                   self.width * 4);
        }

        /* Scratch buffers retained on the manager; freed on teardown. */
        CVPixelBufferUnlockBaseAddress(imageBuffer, 0);
    } // End of autorelease pool
}

- (CVPixelBufferRef)takeLatestFrame {
    return __atomic_exchange_n(&_latest, (CVPixelBufferRef)NULL, __ATOMIC_ACQ_REL);
}

- (int)rotationDegrees:(bool *)mirror {
#if TARGET_OS_OSX
    *mirror = true;
    return 0;
#else
    int rotationDegrees = 180; /* most cameras */
    UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
    *mirror = false;
    if (orientation == UIDeviceOrientationPortrait ||
        orientation == UIDeviceOrientationPortraitUpsideDown) {
        /* In portrait the front camera turns the other way */
        if (self.input.device.position == AVCaptureDevicePositionFront) {
            rotationDegrees = 270;
#if CAMERA_MIRROR_FRONT_CAMERA
            /* TODO: Add an API to retroarch to allow for mirroring of front camera */
            *mirror = true;
#endif
        }
    }
    return rotationDegrees;
#endif
}

- (void)teardownScratchBuffers {
    if (_intermediateBuf) { free(_intermediateBuf); _intermediateBuf = NULL; }
    _intermediateCap = 0;
    if (_rotatedBuf)      { free(_rotatedBuf);      _rotatedBuf      = NULL; }
    _rotatedCap = 0;
    if (_mirroredBuf)     { free(_mirroredBuf);     _mirroredBuf     = NULL; }
    _mirroredCap = 0;
    if (_scaledBuf)       { free(_scaledBuf);       _scaledBuf       = NULL; }
    _scaledCap = 0;
}

- (AVCaptureDevice *)selectCameraDevice {
    RARCH_LOG("[Camera] Selecting camera device...\n");

    NSArray<AVCaptureDevice *> *devices;

#if TARGET_OS_OSX
    // On macOS, use default discovery method
    // Could probably due the same as iOS but need to test.
    // Deprecated in 10.15 and still present: sent by selector.
    devices = apple_rt_get_id_arg([AVCaptureDevice class],
          @selector(devicesWithMediaType:), AVMediaTypeVideo);
#else
    // On iOS/tvOS use modern discovery session.
    // Build the type list at runtime: some constants are gated by both SDK
    // (compile time) and OS version (deployment target), so they cannot all
    // live in a single static array literal.
    NSMutableArray<AVCaptureDeviceType> *deviceTypes = [NSMutableArray array];

    // External cameras: iOS 17 / Mac Catalyst 17 only, unavailable on tvOS.
    // The constant is looked up at runtime, so the same binary builds on
    // any SDK and runs on any OS. Listed first to prefer an attached
    // external camera when one is present.
    if (apple_runtime_available(0, APPLE_RUNTIME_VER(17, 0, 0), 0))
    {
        void **external = apple_rt_constant_addr("AVCaptureDeviceTypeExternal");
        if (external)
            [deviceTypes addObject:apple_rt_obj_at(external)];
    }

    // Built-in wide-angle and telephoto are the iOS 10 baseline.
    [deviceTypes addObject:AVCaptureDeviceTypeBuiltInWideAngleCamera];
    [deviceTypes addObject:AVCaptureDeviceTypeBuiltInTelephotoCamera];

    // Ultra-wide was added in iOS 13; the deployment target may be lower, so
    // it needs a runtime availability guard.
    if (apple_runtime_available(0, APPLE_RUNTIME_VER(13, 0, 0), 0))
    {
        void **ultra = apple_rt_constant_addr("AVCaptureDeviceTypeBuiltInUltraWideCamera");
        if (ultra)
            [deviceTypes addObject:apple_rt_obj_at(ultra)];
    }

    //  AVCaptureDeviceTypeBuiltInDualCamera,
    //  AVCaptureDeviceTypeBuiltInDualWideCamera,
    //  AVCaptureDeviceTypeBuiltInTripleCamera,
    //  AVCaptureDeviceTypeBuiltInTrueDepthCamera,
    //  AVCaptureDeviceTypeBuiltInLiDARDepthCamera,
    //  AVCaptureDeviceTypeContinuityCamera,
    AVCaptureDeviceDiscoverySession *discoverySession = [AVCaptureDeviceDiscoverySession
                                                         discoverySessionWithDeviceTypes:deviceTypes
                                                         mediaType:AVMediaTypeVideo
                                                         position:AVCaptureDevicePositionUnspecified];

    devices = discoverySession.devices;
#endif

    if (devices.count == 0) {
        RARCH_ERR("[Camera] No camera devices found.\n");
        return nil;
    }

    // Log available devices
    for (AVCaptureDevice *device in devices) {
        RARCH_LOG("[Camera] Found device: %s - Position: %d.\n",
                  [device.localizedName UTF8String],
                  (int)device.position);
    }

#if TARGET_OS_OSX
    // macOS: Just use the first available camera if only one exists
    if (devices.count == 1) {
        RARCH_LOG("[Camera] Using only available camera: %s.\n",
                  [devices.firstObject.localizedName UTF8String]);
        return devices.firstObject;
    }

    // Try to match by name for built-in cameras
    for (AVCaptureDevice *device in devices) {
        BOOL isFrontFacing = [device.localizedName containsString:@"FaceTime"] ||
                            [device.localizedName containsString:@"Front"];
        if (CAMERA_PREFER_FRONTFACING == isFrontFacing) {
            RARCH_LOG("[Camera] Selected macOS camera: %s.\n",
                      [device.localizedName UTF8String]);
            return device;
        }
    }
#else
    // iOS: Use position property
    AVCaptureDevicePosition preferredPosition = CAMERA_PREFER_FRONTFACING ?
        AVCaptureDevicePositionFront : AVCaptureDevicePositionBack;

    // Try to find preferred camera
    for (AVCaptureDevice *device in devices) {
        if (device.position == preferredPosition) {
            RARCH_LOG("[Camera] Selected iOS camera position: %d.\n",
                      (int)preferredPosition);
            return device;
        }
    }
#endif

    // Fallback to first available camera
    RARCH_LOG("[Camera] Using fallback camera: %s.\n",
              [devices.firstObject.localizedName UTF8String]);
    return devices.firstObject;
}

- (bool)setupCameraSession {
    /* The property retains what it is handed, so the reference the
     * allocation carries is released once it is stored - otherwise the
     * session set up by a previous init is orphaned rather than torn
     * down when this one replaces it. */
    {
        AVCaptureSession *sess = [[AVCaptureSession alloc] init];
        self.session           = sess;
        RARCH_RELEASE(sess);
    }

    // Get camera device
    AVCaptureDevice *device = [self selectCameraDevice];
    if (!device) {
        RARCH_ERR("[Camera] No camera device found.\n");
        return false;
    }

    // Create device input
    NSError *error = nil;
    self.input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
    if (error) {
        RARCH_ERR("[Camera] Failed to create device input: %s.\n",
                  [error.localizedDescription UTF8String]);
        return false;
    }

    if ([self.session canAddInput:self.input]) {
        [self.session addInput:self.input];
        RARCH_LOG("[Camera] Added camera input to session.\n");
    }

    /* Create and configure video output; owned as the session above. */
    {
        AVCaptureVideoDataOutput *out = [[AVCaptureVideoDataOutput alloc] init];
        self.output                   = out;
        RARCH_RELEASE(out);
    }
    if (self.wantPlanar) {
        /* The device's own 4:2:0, full range first */
        OSType planar = 0;
        for (NSNumber *type in self.output.availableVideoCVPixelFormatTypes) {
            OSType t = (OSType)[type unsignedIntValue];
            if (t == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
                planar = t;
                break;
            }
            if (t == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
                planar = t;
        }
        if (!planar) {
            RARCH_ERR("[Camera] The camera gives no 4:2:0 YCbCr.\n");
            return false;
        }
        self.output.videoSettings = @{
            (NSString*)kCVPixelBufferPixelFormatTypeKey: @(planar)
        };
    } else {
        self.output.videoSettings = @{
            (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
        };
    }
    [self.output setSampleBufferDelegate:self queue:dispatch_get_main_queue()];

    if ([self.session canAddOutput:self.output]) {
        [self.session addOutput:self.output];
        RARCH_LOG("[Camera] Added video output to session.\n");
    }

    return true;
}

@end

typedef struct
{
    AVCameraManager *manager;
    unsigned width;
    unsigned height;
    bool want_planar;
} avfoundation_t;

static void generateColorBars(uint32_t *buffer, size_t width, size_t height) {
    const uint32_t colors[] = {
        0xFFFFFFFF,  // White
        0xFFFFFF00,  // Yellow
        0xFF00FFFF,  // Cyan
        0xFF00FF00,  // Green
        0xFFFF00FF,  // Magenta
        0xFFFF0000,  // Red
        0xFF0000FF,  // Blue
        0xFF000000   // Black
    };

    size_t barWidth = width / 8;
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            size_t colorIndex = x / barWidth;
            buffer[y * width + x] = colors[colorIndex];
        }
    }
}

/* The session set up on the main queue, the answer for the camera of
 * generation @gen: dropped when that camera has gone since */
static void avfoundation_setup(AVCameraManager *mgr, int gen,
      BOOL granted)
{
    bool ok = false;
    if (!avf_state_current(&mgr->_st, gen))
        return;
    if (granted) {
        @autoreleasepool {
            ok = [mgr setupCameraSession];
        }
    } else
        RARCH_ERR("[Camera] Camera access not authorized.\n");
    if (ok)
        RARCH_LOG("[Camera] Camera session set up.\n");
    /* Started before the answer came: started now */
    if (avf_state_settle(&mgr->_st, ok)) {
        AVCaptureSession *session = mgr.session;
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [session startRunning];
        });
    }
}

static void *avfoundation_init(const char *device, uint64_t caps,
                             unsigned dims)
{
    unsigned width      = VIDEO_SCALE_W(dims);
    unsigned height     = VIDEO_SCALE_H(dims);
    bool want_planar    = (caps & (UINT64_C(1) << RETRO_CAMERA_BUFFER_PLANAR)) != 0;
    AVCameraManager *mgr;
    avfoundation_t *avf;
    int gen;

    if (!want_planar
          && !(caps & (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER)))
    {
        RARCH_ERR("[Camera] Returns raw framebuffers or planes.\n");
        return NULL;
    }
    /* Refused before: known without asking */
    if (     apple_runtime_available(APPLE_RUNTIME_VER(10, 14, 0), 0, 0)
          && apple_rt_get_bool(apple_rt_class("RACameraAuthorization"),
             @selector(refused)))
    {
        RARCH_ERR("[Camera] Camera access denied or restricted.\n");
        return NULL;
    }

    RARCH_LOG("[Camera] Initializing AVFoundation camera %ux%u%s.\n",
          width, height, want_planar ? ", planar" : "");
    if (!(avf = (avfoundation_t*)calloc(1, sizeof(avfoundation_t))))
    {
        RARCH_ERR("[Camera] Failed to allocate avfoundation_t.\n");
        return NULL;
    }

    mgr                 = [AVCameraManager sharedInstance];
    avf->manager        = mgr;
    avf->width          = width;
    avf->height         = height;
    avf->want_planar    = want_planar;
    mgr.width           = width;
    mgr.height          = height;
    mgr.wantPlanar      = want_planar;

    /* Frames converted for the core, or color bars until they come;
     * planes are lent as the camera gives them */
    if (!want_planar)
    {
        mgr.frameBuffer = (uint32_t*)calloc(width * height, sizeof(uint32_t));
        if (!mgr.frameBuffer)
        {
            RARCH_ERR("[Camera] Failed to allocate frame buffer.\n");
            free(avf);
            return NULL;
        }
    }

    gen = avf_state_begin(&mgr->_st);

    /* Asked without waiting: the session is set up on the main queue
     * once access is granted, at once when it already is */
    [mgr requestCameraAuthorizationWithCompletion:^(BOOL granted) {
        if ([NSThread isMainThread])
            avfoundation_setup(mgr, gen, granted);
        else
            dispatch_async(dispatch_get_main_queue(), ^{
                avfoundation_setup(mgr, gen, granted);
            });
    }];

    if (avf_state_phase(&mgr->_st) == AVF_FAILED)
    {
        RARCH_ERR("[Camera] Failed to setup camera.\n");
        avf_state_end(&mgr->_st);
        free(mgr.frameBuffer);
        mgr.frameBuffer = NULL;
        free(avf);
        return NULL;
    }

    RARCH_LOG("[Camera] AVFoundation camera initialized.\n");
    return avf;
}

static void avfoundation_free(void *data)
{
    CVPixelBufferRef last;
    avfoundation_t *avf = (avfoundation_t*)data;
    if (!avf)
        return;

    RARCH_LOG("[Camera] Freeing AVFoundation camera.\n");

    /* An answer still to come is for this camera: dropped */
    avf_state_end(&avf->manager->_st);

    if (avf->manager.session)
        [avf->manager.session stopRunning];

    if ((last = [avf->manager takeLatestFrame]))
        CVPixelBufferRelease(last);

    if (avf->manager.frameBuffer)
    {
        free(avf->manager.frameBuffer);
        avf->manager.frameBuffer = NULL;
    }

    /* The manager is a singleton; its scratch buffers from the per-
     * frame conversion/rotation/mirror/scale pipeline persist across
     * driver instances.  Free them here so a subsequent init starts
     * clean and stale capacity from a prior session does not linger. */
    [avf->manager teardownScratchBuffers];

    free(avf);
    RARCH_LOG("[Camera] AVFoundation camera freed.\n");
}

/* Starts without waiting for the device: startRunning blocks, so it
 * runs on a background queue, and before access is granted the session
 * starts once it is set up */
static bool avfoundation_start(void *data)
{
    AVCaptureSession *session;
    avfoundation_t *avf = (avfoundation_t*)data;
    if (!avf)
        return false;
    switch (avf_state_start(&avf->manager->_st))
    {
        case AVF_START_FAIL:
            RARCH_ERR("[Camera] Cannot start - no camera session.\n");
            return false;
        case AVF_START_LATER:
            RARCH_LOG("[Camera] Starting once camera access is answered.\n");
            return true;
        default:
            break;
    }

    RARCH_LOG("[Camera] Starting AVFoundation camera.\n");
    session = avf->manager.session;
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [session startRunning];
    });
    return true;
}

static void avfoundation_stop(void *data)
{
    AVCaptureSession *session;
    avfoundation_t *avf = (avfoundation_t*)data;
    if (!avf)
        return;
    avf_state_stop(&avf->manager->_st);
    if (!(session = avf->manager.session))
        return;

    RARCH_LOG("[Camera] Stopping AVFoundation camera...\n");

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [session stopRunning];
    });
}

/* The latest capture buffer lent in place, for the callback only */
static bool avfoundation_poll_planar(avfoundation_t *avf,
      retro_camera_frame_planar_t frame_planar_cb)
{
    struct retro_camera_planar_frame f;
    CVPixelBufferRef pb;
    CFTypeRef matrix;
    OSType fmt;
    bool mirror;
    bool ok = false;

    if (!frame_planar_cb || !(pb = [avf->manager takeLatestFrame]))
        return false;
    fmt = CVPixelBufferGetPixelFormatType(pb);
    if (     (fmt == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
             || fmt == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)
          && CVPixelBufferGetPlaneCount(pb) == 2
          && CVPixelBufferLockBaseAddress(pb, kCVPixelBufferLock_ReadOnly)
             == kCVReturnSuccess)
    {
        f.planes[0]   = (const uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pb, 0);
        f.planes[1]   = (const uint8_t*)CVPixelBufferGetBaseAddressOfPlane(pb, 1);
        f.planes[2]   = f.planes[1] ? f.planes[1] + 1 : NULL;
        f.strides[0]  = CVPixelBufferGetBytesPerRowOfPlane(pb, 0);
        f.strides[1]  = f.strides[2] = CVPixelBufferGetBytesPerRowOfPlane(pb, 1);
        f.chroma_step = 2;
        f.width       = (unsigned)CVPixelBufferGetWidthOfPlane(pb, 0);
        f.height      = (unsigned)CVPixelBufferGetHeightOfPlane(pb, 0);
        /* Clockwise for the core; the planes carry no mirroring */
        f.rotation    = (unsigned)((360 - [avf->manager rotationDegrees:&mirror]) % 360);
        f.flags       = fmt == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
            ? RETRO_CAMERA_PLANAR_FULL_RANGE : 0;
        if (     (matrix = CVBufferGetAttachment(pb, kCVImageBufferYCbCrMatrixKey, NULL))
              && CFEqual(matrix, kCVImageBufferYCbCrMatrix_ITU_R_709_2))
            f.flags  |= RETRO_CAMERA_PLANAR_BT709;
        if (f.planes[0] && f.planes[1] && f.width && f.height)
        {
            frame_planar_cb(&f);
            ok = true;
        }
        CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    }
    CVPixelBufferRelease(pb);
    return ok;
}

static bool avfoundation_poll(void *data,
      retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb,
      retro_camera_frame_planar_t frame_planar_cb)
{
    avfoundation_t *avf = (avfoundation_t*)data;
    (void)frame_gl_cb;
    if (!avf)
        return false;
    if (avf->want_planar)
        return avfoundation_poll_planar(avf, frame_planar_cb);
    if (!frame_raw_cb || !avf->manager.frameBuffer
          || avf_state_phase(&avf->manager->_st) == AVF_FAILED)
        return false;

    /* Until the session runs (access still being asked, or the device
     * warming up), color bars, painted into the frame buffer the
     * frames then overwrite */
    if (!avf->manager.session.isRunning)
        generateColorBars(avf->manager.frameBuffer, avf->width, avf->height);
    frame_raw_cb(avf->manager.frameBuffer, avf->width, avf->height, avf->width * 4);
    return true;
}

camera_driver_t camera_avfoundation = {
   avfoundation_init,
   avfoundation_free,
   avfoundation_start,
   avfoundation_stop,
   avfoundation_poll,
   "avfoundation"
};

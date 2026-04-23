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

#include <TargetConditionals.h>
#include <Foundation/Foundation.h>
#include <AVFoundation/AVFoundation.h>
#include <libretro.h>
/* For image scaling and color space DSP */
#import <Accelerate/Accelerate.h>
#if TARGET_OS_IOS
/* For camera rotation detection */
#import <UIKit/UIKit.h>
#endif

#include "../camera_driver.h"
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

- (bool)setupCameraSession;
- (void)teardownScratchBuffers;
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

- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
    @autoreleasepool {
        if (!self.frameBuffer)
            return;

        CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
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

        // Determine rotation based on platform and camera type
#if TARGET_OS_OSX
        int rotationDegrees = 0; // Default 180-degree rotation for most cases
        bool shouldMirror = true;
#else
        int rotationDegrees = 180; // Default 180-degree rotation for most cases
        bool shouldMirror = false;

        /// For camera rotation detection
        UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
        if (orientation == UIDeviceOrientationPortrait ||
            orientation == UIDeviceOrientationPortraitUpsideDown) {
            // In portrait mode, adjust rotation based on camera type
            if (self.input.device.position == AVCaptureDevicePositionFront) {
                rotationDegrees = 270;
                #if CAMERA_MIRROR_FRONT_CAMERA
                // TODO: Add an API to retroarch to allow for mirroring of front camera
                shouldMirror = true; // Mirror front camera
                #endif
#ifdef DEBUG
                RARCH_LOG("[Camera] Using 270-degree rotation with mirroring for front camera in portrait mode.\n");
#endif
            }
        }
#endif

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
    devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
#else
    // On iOS/tvOS use modern discovery session
    NSArray<AVCaptureDeviceType> *deviceTypes;
    if (@available(iOS 17.0, *)) {
        deviceTypes = @[
            AVCaptureDeviceTypeExternal,
            AVCaptureDeviceTypeBuiltInWideAngleCamera,
            AVCaptureDeviceTypeBuiltInTelephotoCamera,
            AVCaptureDeviceTypeBuiltInUltraWideCamera,
            //        AVCaptureDeviceTypeBuiltInDualCamera,
            //        AVCaptureDeviceTypeBuiltInDualWideCamera,
            //        AVCaptureDeviceTypeBuiltInTripleCamera,
            //        AVCaptureDeviceTypeBuiltInTrueDepthCamera,
            //        AVCaptureDeviceTypeBuiltInLiDARDepthCamera,
            //        AVCaptureDeviceTypeContinuityCamera,
        ];
    } else {
        deviceTypes = @[
            AVCaptureDeviceTypeBuiltInWideAngleCamera,
            AVCaptureDeviceTypeBuiltInTelephotoCamera,
            AVCaptureDeviceTypeBuiltInUltraWideCamera,
            //        AVCaptureDeviceTypeBuiltInDualCamera,
            //        AVCaptureDeviceTypeBuiltInDualWideCamera,
            //        AVCaptureDeviceTypeBuiltInTripleCamera,
            //        AVCaptureDeviceTypeBuiltInTrueDepthCamera,
            //        AVCaptureDeviceTypeBuiltInLiDARDepthCamera,
            //        AVCaptureDeviceTypeContinuityCamera,
        ];
    }
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
    // Initialize capture session
    self.session = [[AVCaptureSession alloc] init];

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

    // Create and configure video output
    self.output = [[AVCaptureVideoDataOutput alloc] init];
    self.output.videoSettings = @{
        (NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
    };
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

static void *avfoundation_init(const char *device, uint64_t caps,
                             unsigned width, unsigned height)
{
    avfoundation_t *avf = (avfoundation_t*)calloc(1, sizeof(avfoundation_t));
    RARCH_LOG("[Camera] Initializing AVFoundation camera %ux%u.\n", width, height);
    if (!avf)
    {
        RARCH_ERR("[Camera] Failed to allocate avfoundation_t.\n");
        return NULL;
    }

    avf->manager        = [AVCameraManager sharedInstance];
    avf->width          = width;
    avf->height         = height;
    avf->manager.width  = width;
    avf->manager.height = height;

    /* Synchronously request camera authorization */
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    __block BOOL granted = NO;
    RARCH_LOG("[Camera] Requesting camera authorization synchronously.\n");
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL g) {
        granted = g;
        dispatch_semaphore_signal(sema);
    }];
    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
    if (!granted)
    {
        RARCH_ERR("[Camera] Camera access not authorized.\n");
        free(avf);
        return NULL;
    }

    /* Allocate frame buffer */
    avf->manager.frameBuffer = (uint32_t*)calloc(width * height, sizeof(uint32_t));
    if (!avf->manager.frameBuffer)
    {
        RARCH_ERR("[Camera] Failed to allocate frame buffer.\n");
        free(avf);
        return NULL;
    }

    /* Initialize capture session on main thread */
    __block bool setupSuccess = false;

    if ([NSThread isMainThread])
    {
        @autoreleasepool {
            setupSuccess = [avf->manager setupCameraSession];
            if (setupSuccess) {
                RARCH_LOG("[Camera] Started camera session.\n");
            }
        }
    }
    else
    {
        dispatch_sync(dispatch_get_main_queue(), ^{
            @autoreleasepool {
                setupSuccess = [avf->manager setupCameraSession];
                if (setupSuccess) {
                    RARCH_LOG("[Camera] Started camera session.\n");
                }
            }
        });
    }

    if (!setupSuccess)
    {
        RARCH_ERR("[Camera] Failed to setup camera.\n");
        free(avf->manager.frameBuffer);
        free(avf);
        return NULL;
    }

    RARCH_LOG("[Camera] AVFoundation camera initialized and started successfully.\n");
    return avf;
}

static void avfoundation_free(void *data)
{
    avfoundation_t *avf = (avfoundation_t*)data;
    if (!avf)
        return;

    RARCH_LOG("[Camera] Freeing AVFoundation camera.\n");

    if (avf->manager.session)
        [avf->manager.session stopRunning];

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

static bool avfoundation_start(void *data)
{
    bool isRunning;
    avfoundation_t *avf = (avfoundation_t*)data;
    if (!avf || !avf->manager.session)
    {
        RARCH_ERR("[Camera] Cannot start - invalid data.\n");
        return false;
    }

    RARCH_LOG("[Camera] Starting AVFoundation camera.\n");

    dispatch_sync(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [avf->manager.session startRunning];
        RARCH_LOG("[Camera] Camera session started on background thread.\n");
    });

    isRunning = avf->manager.session.isRunning;
    RARCH_LOG("[Camera] Camera session running: %s.\n", isRunning ? "YES" : "NO");
    return isRunning;
}

static void avfoundation_stop(void *data)
{
    avfoundation_t *avf = (avfoundation_t*)data;
    if (!avf || !avf->manager.session)
        return;

    RARCH_LOG("[Camera] Stopping AVFoundation camera...\n");

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        [avf->manager.session stopRunning];
        RARCH_LOG("[Camera] Camera session stopped on background thread.\n");
    });
}

static bool avfoundation_poll(void *data,
      retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{
    avfoundation_t *avf = (avfoundation_t*)data;
    if (!avf || !frame_raw_cb)
    {
        RARCH_ERR("[Camera] Cannot poll - invalid data or callback.\n");
        return false;
    }

    if (!avf->manager.session.isRunning)
    {
        RARCH_LOG("[Camera] Camera not running, generating color bars...\n");
        uint32_t *tempBuffer = (uint32_t*)calloc(avf->width * avf->height, sizeof(uint32_t));
        if (tempBuffer)
        {
            generateColorBars(tempBuffer, avf->width, avf->height);
            frame_raw_cb(tempBuffer, avf->width, avf->height, avf->width * 4);
            free(tempBuffer);
            return true;
        }
        return false;
    }

#ifdef DEBUG
    RARCH_LOG("[Camera] Delivering camera frame.\n");
#endif
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

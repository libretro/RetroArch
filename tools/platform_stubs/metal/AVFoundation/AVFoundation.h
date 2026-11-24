/* Stand-in for the AVFoundation capture declarations
 * camera/drivers/avfoundation.m uses, so the compile matrix can check
 * the camera driver's syntax on a host without the macOS SDK. Shapes
 * follow the SDK; nothing here links. */
#ifndef STUB_AVFOUNDATION_H
#define STUB_AVFOUNDATION_H
#include <Foundation/Foundation.h>
#include <CoreMedia/CoreMedia.h>
#include <dispatch/dispatch.h>

typedef NSString *AVMediaType;
typedef NSString *AVCaptureDeviceType;
extern AVMediaType const AVMediaTypeVideo;

typedef NS_ENUM(NSInteger, AVAuthorizationStatus) {
   AVAuthorizationStatusNotDetermined = 0,
   AVAuthorizationStatusRestricted    = 1,
   AVAuthorizationStatusDenied        = 2,
   AVAuthorizationStatusAuthorized    = 3
};
typedef NS_ENUM(NSInteger, AVCaptureDevicePosition) {
   AVCaptureDevicePositionUnspecified = 0,
   AVCaptureDevicePositionBack        = 1,
   AVCaptureDevicePositionFront       = 2
};

@interface AVCaptureDevice : NSObject
+ (AVAuthorizationStatus)authorizationStatusForMediaType:(AVMediaType)t;
+ (void)requestAccessForMediaType:(AVMediaType)t
      completionHandler:(void (^)(BOOL granted))handler;
@property (readonly) NSString *localizedName;
@property (readonly) AVCaptureDevicePosition position;
@end

@interface AVCaptureInput : NSObject
@end
@interface AVCaptureDeviceInput : AVCaptureInput
+ (instancetype)deviceInputWithDevice:(AVCaptureDevice *)d error:(NSError **)e;
@property (readonly) AVCaptureDevice *device;
@end

@interface AVCaptureConnection : NSObject
@end
@interface AVCaptureOutput : NSObject
@end
@protocol AVCaptureVideoDataOutputSampleBufferDelegate <NSObject>
@end
@interface AVCaptureVideoDataOutput : AVCaptureOutput
@property (copy) NSDictionary *videoSettings;
@property (readonly) NSArray<NSNumber *> *availableVideoCVPixelFormatTypes;
- (void)setSampleBufferDelegate:(id<AVCaptureVideoDataOutputSampleBufferDelegate>)d
      queue:(dispatch_queue_t)q;
@end

@interface AVCaptureSession : NSObject
- (BOOL)canAddInput:(AVCaptureInput *)i;
- (void)addInput:(AVCaptureInput *)i;
- (BOOL)canAddOutput:(AVCaptureOutput *)o;
- (void)addOutput:(AVCaptureOutput *)o;
- (void)startRunning;
- (void)stopRunning;
@property (readonly, getter=isRunning) BOOL running;
@end
#endif

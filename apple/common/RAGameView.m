/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"

#include "../../general.h"
#include "gfx/gfx_common.h"
#include "gfx/gfx_context.h"

#include <CoreLocation/CoreLocation.h>

static CLLocationManager *locationManager;
static bool locationChanged;
static CLLocationDegrees currentLatitude;
static CLLocationDegrees currentLongitude;
static CLLocationAccuracy currentHorizontalAccuracy;
static CLLocationAccuracy currentVerticalAccuracy;

// Define compatibility symbols and categories
#ifdef IOS
#include <AVFoundation/AVCaptureSession.h>
#include <AVFoundation/AVCaptureDevice.h>
#include <AVFoundation/AVCaptureOutput.h>
#include <AVFoundation/AVCaptureInput.h>
#include <AVFoundation/AVMediaFormat.h>
#include <CoreVideo/CVOpenGLESTextureCache.h>
#define APP_HAS_FOCUS ([[UIApplication sharedApplication] applicationState] == UIApplicationStateActive)

#define GLContextClass EAGLContext
#define GLAPIType GFX_CTX_OPENGL_ES_API
#define GLFrameworkID CFSTR("com.apple.opengles")
#define RAScreen UIScreen

@interface EAGLContext (OSXCompat) @end
@implementation EAGLContext (OSXCompat)
+ (void)clearCurrentContext { [EAGLContext setCurrentContext:nil];  }
- (void)makeCurrentContext  { [EAGLContext setCurrentContext:self]; }
@end

#elif defined(OSX)
#define APP_HAS_FOCUS ([NSApp isActive])

#define GLContextClass NSOpenGLContext
#define GLAPIType GFX_CTX_OPENGL_API
#define GLFrameworkID CFSTR("com.apple.opengl")
#define RAScreen NSScreen

#define g_view g_instance // < RAGameView is a container on iOS; on OSX these are both the same object

@interface NSScreen (IOSCompat) @end
@implementation NSScreen (IOSCompat)
- (CGRect)bounds
{
	CGRect cgrect  = NSRectToCGRect([self frame]);
	return CGRectMake(0, 0, CGRectGetWidth(cgrect), CGRectGetHeight(cgrect));
}
- (float) scale  { return 1.0f; }
@end

#endif

#ifdef IOS

#include <GLKit/GLKit.h>
#import "views.h"
static const float ALMOST_INVISIBLE = .021f;
static GLKView* g_view;
static UIView* g_pause_indicator_view;

// Camera
static AVCaptureSession *_session;
static NSString *_sessionPreset;
CVOpenGLESTextureCacheRef textureCache;
GLuint outputTexture;
static bool newFrame = false;

#elif defined(OSX)

#include "apple_input.h"

static bool g_has_went_fullscreen;
static NSOpenGLPixelFormat* g_format;

#endif

static bool g_initialized;
static RAGameView* g_instance;
static GLContextClass* g_context;

static int g_fast_forward_skips;
static bool g_is_syncing = true;


@implementation RAGameView
+ (RAGameView*)get
{
   if (!g_instance)
      g_instance = [RAGameView new];
   
   return g_instance;
}

#ifdef OSX

- (id)init
{
   self = [super init];
   [self setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
   return self;
}

- (void)setFrame:(NSRect)frameRect
{
   [super setFrame:frameRect];

   if (g_view && g_context)
      [g_context update];
}

- (void)display
{
   [g_context flushBuffer];
}

// Stop the annoying sound when pressing a key
- (BOOL)acceptsFirstResponder
{
   return YES;
}

- (BOOL)isFlipped
{
   return YES;
}

- (void)keyDown:(NSEvent*)theEvent
{
}

#elif defined(IOS)
// < iOS Pause menu and lifecycle
- (id)init
{
   self = [super init];

   UINib *xib = [UINib nibWithNibName:@"PauseIndicatorView" bundle:nil];
   g_pause_indicator_view = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];

   g_view = [GLKView new];
   g_view.multipleTouchEnabled = YES;
   g_view.enableSetNeedsDisplay = NO;
   [g_view addSubview:g_pause_indicator_view];

   self.view = g_view;
   return self;
}

// Pause Menus
- (void)viewWillLayoutSubviews
{
   UIInterfaceOrientation orientation = self.interfaceOrientation;
   CGRect screenSize = [[UIScreen mainScreen] bounds];
   
   const float width = ((int)orientation < 3) ? CGRectGetWidth(screenSize) : CGRectGetHeight(screenSize);
   const float height = ((int)orientation < 3) ? CGRectGetHeight(screenSize) : CGRectGetWidth(screenSize);

   float tenpctw = width / 10.0f;
   float tenpcth = height / 10.0f;
   
   g_pause_indicator_view.frame = CGRectMake(tenpctw * 4.0f, 0.0f, tenpctw * 2.0f, tenpcth);
   [g_pause_indicator_view viewWithTag:1].frame = CGRectMake(0, 0, tenpctw * 2.0f, tenpcth);
}

- (void)hidePauseButton
{
   [UIView animateWithDuration:0.2
      animations:^{ g_pause_indicator_view.alpha = ALMOST_INVISIBLE; }
      completion:^(BOOL finished) { }
   ];
}

// NOTE: This version only runs on iOS6
- (NSUInteger)supportedInterfaceOrientations
{
   return apple_frontend_settings.orientation_flags;
}

// NOTE: This version runs on iOS2-iOS5, but not iOS6
- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
   switch (interfaceOrientation)
   {
      case UIInterfaceOrientationPortrait:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskPortrait);
      case UIInterfaceOrientationPortraitUpsideDown:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskPortraitUpsideDown);
      case UIInterfaceOrientationLandscapeLeft:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskLandscapeLeft);
      case UIInterfaceOrientationLandscapeRight:
         return (apple_frontend_settings.orientation_flags & UIInterfaceOrientationMaskLandscapeRight);
   }
   
   return YES;
}

void event_process_camera_frame(void* pixelBufferPtr)
{
    CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)pixelBufferPtr;
    
    int width, height;
    CVReturn ret;
    
    width  = CVPixelBufferGetWidth(pixelBuffer);
    height = CVPixelBufferGetHeight(pixelBuffer);
    
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    CVOpenGLESTextureRef renderTexture;
    
    //TODO - rewrite all this
    // create a texture from our render target.
    // textureCache will be what you previously made with CVOpenGLESTextureCacheCreate
    ret = CVOpenGLESTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
                                                       textureCache, pixelBuffer, NULL, GL_TEXTURE_2D,
                                                       GL_RGBA, width, height, GL_BGRA, GL_UNSIGNED_BYTE, 0, &renderTexture);
    if (!renderTexture || ret)
    {
        RARCH_ERR("ioscamera: CVOpenGLESTextureCacheCreateTextureFromImage failed.\n");
        return;
    }
    
    outputTexture = CVOpenGLESTextureGetName(renderTexture);
    glBindTexture(GL_TEXTURE_2D, outputTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    [[NSNotificationCenter defaultCenter] postNotificationName:@"NewCameraTextureReady" object:nil];
    newFrame = true;
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    CVOpenGLESTextureCacheFlush(textureCache, 0);
    CFRelease(renderTexture);
    
    CFRelease(pixelBuffer);
    pixelBuffer = 0;
}

- (void)captureOutput:(AVCaptureOutput *)captureOutput
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection
{
    // TODO: Don't post if event queue is full
    CVPixelBufferRef pixelBuffer = CVPixelBufferRetain(CMSampleBufferGetImageBuffer(sampleBuffer));
    apple_frontend_post_event(event_process_camera_frame, pixelBuffer);}

- (void) onCameraInit
{
    CVReturn ret;
    int width, height;
    
    //FIXME - dehardcode this
    width = 640;
    height = 480;
    
#if COREVIDEO_USE_EAGLCONTEXT_CLASS_IN_API
    ret = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, g_context, NULL, &textureCache);
    
#else
    ret = CVOpenGLESTextureCacheCreate(kCFAllocatorDefault, NULL, (__bridge void *)g_context, NULL, &textureCache);
#endif
    
    //-- Setup Capture Session.
    _session = [[AVCaptureSession alloc] init];
    [_session beginConfiguration];
    
    // TODO: dehardcode this based on device capabilities
    _sessionPreset = AVCaptureSessionPreset640x480;
    
    //-- Set preset session size.
    [_session setSessionPreset:_sessionPreset];
    
    //-- Creata a video device and input from that Device.  Add the input to the capture session.
    AVCaptureDevice * videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if(videoDevice == nil)
        assert(0);
    
    //-- Add the device to the session.
    NSError *error;
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:videoDevice error:&error];
    if(error)
    {
        RARCH_ERR("video device input %s\n", error.localizedDescription.UTF8String);
        assert(0);
    }
    
    [_session addInput:input];
    
    //-- Create the output for the capture session.
    AVCaptureVideoDataOutput * dataOutput = [[AVCaptureVideoDataOutput alloc] init];
    [dataOutput setAlwaysDiscardsLateVideoFrames:NO]; // Probably want to set this to NO when recording
    
	[dataOutput setVideoSettings:[NSDictionary dictionaryWithObject:[NSNumber numberWithInt:kCVPixelFormatType_32BGRA] forKey:(id)kCVPixelBufferPixelFormatTypeKey]];
    
    // Set dispatch to be on the main thread so OpenGL can do things with the data
    [dataOutput setSampleBufferDelegate:self queue:dispatch_get_main_queue()];
    
    [_session addOutput:dataOutput];
    [_session commitConfiguration];
}

- (void) onCameraStart
{
    [_session startRunning];
}

- (void) onCameraStop
{
    [_session stopRunning];
}

- (void) onCameraFree
{
    CVOpenGLESTextureCacheFlush(textureCache, 0);
    CFRelease(textureCache);
}
#endif

- (void)onLocationSetInterval:(unsigned)interval_update_ms interval_update_distance:(unsigned)interval_distance
{
   (void)interval_update_ms;

    // Set a movement threshold for new events (in meters).
    if (interval_distance == 0)
       locationManager.distanceFilter = kCLDistanceFilterNone;
    else
       locationManager.distanceFilter = interval_distance;
}

- (void)onLocationInit
{
    // Create the location manager if this object does not
    // already have one.
    if (nil == locationManager)
        locationManager = [[CLLocationManager alloc] init];
    
    locationManager.delegate = self;
    locationManager.desiredAccuracy = kCLLocationAccuracyBest;
    locationManager.distanceFilter = kCLDistanceFilterNone;

	[[RAGameView get] onLocationSetInterval:0 interval_update_distance:0];
}

- (void)onLocationStart
{
    [locationManager startUpdatingLocation];
}

- (void)onLocationStop
{
    [locationManager stopUpdatingLocation];
}

- (void)onLocationFree
{
    /* TODO - free location manager? */
}

- (double)onLocationGetLatitude
{
    return currentLatitude;
}

- (double)onLocationGetLongitude
{
    return currentLongitude;
}

- (double)onLocationGetHorizontalAccuracy
{
   return currentHorizontalAccuracy;
}

- (double)onLocationGetVerticalAccuracy
{
   return currentVerticalAccuracy;
}

- (bool)onLocationHasChanged
{
   bool hasChanged = locationChanged;
    
   if (hasChanged)
      locationChanged = false;
    
   return hasChanged;
}

- (void)locationManager:(CLLocationManager *)manager didUpdateToLocation:(CLLocation *)newLocation fromLocation:(CLLocation *)oldLocation
{
    locationChanged = true;
    currentLatitude = newLocation.coordinate.latitude;
    currentLongitude = newLocation.coordinate.longitude;
    currentHorizontalAccuracy = newLocation.horizontalAccuracy;
    currentVerticalAccuracy = newLocation.verticalAccuracy;
    RARCH_LOG("didUpdateToLocation - latitude %f, longitude %f\n", (float)currentLatitude, (float)currentLongitude);
}

- (void)locationManager:(CLLocationManager *)manager didUpdateLocations:(NSArray *)locations
{
    locationChanged = true;
    CLLocation *location = [locations objectAtIndex:([locations     count]-1)];
    currentLatitude  = [location coordinate].latitude;
    currentLongitude = [location coordinate].longitude;
    currentHorizontalAccuracy = [location horizontalAccuracy];
    currentVerticalAccuracy = [location verticalAccuracy];
    RARCH_LOG("didUpdateLocations - latitude %f, longitude %f\n", (float)currentLatitude, (float)currentLongitude);
}

@end

static RAScreen* get_chosen_screen()
{
#ifdef OSX
   NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
#else
   @autoreleasepool {
#endif
      
   if (g_settings.video.monitor_index >= RAScreen.screens.count)
   {
      RARCH_WARN("video_monitor_index is greater than the number of connected monitors; using main screen instead.\n");
#ifdef OSX
      [pool drain];
#endif
      return [RAScreen mainScreen];
   }
	
   NSArray *screens = [RAScreen screens];
   RAScreen *s = (RAScreen*)[screens objectAtIndex:g_settings.video.monitor_index];
#ifdef OSX
   [pool drain];
#endif
   return s;
#ifdef IOS
   }
#endif
}

bool apple_gfx_ctx_init(void)
{
   dispatch_sync(dispatch_get_main_queue(),
   ^{
      // Make sure the view was created
      [RAGameView get];      
      
#ifdef IOS // Show pause button for a few seconds, so people know it's there
      g_pause_indicator_view.alpha = 1.0f;
      [NSObject cancelPreviousPerformRequestsWithTarget:g_instance];
      [g_instance performSelector:@selector(hidePauseButton) withObject:g_instance afterDelay:3.0f];
#endif
   });

   g_initialized = true;

   return true;
}

void apple_gfx_ctx_destroy(void)
{
   g_initialized = false;

   [GLContextClass clearCurrentContext];

   dispatch_sync(dispatch_get_main_queue(),
   ^{
#ifdef IOS
      g_view.context = nil;
#endif
      [GLContextClass clearCurrentContext];
      g_context = nil;
   });
}

bool apple_gfx_ctx_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   if (api != GLAPIType)
      return false;


   [GLContextClass clearCurrentContext];

   dispatch_sync(dispatch_get_main_queue(),
   ^{
      [GLContextClass clearCurrentContext];
   
#ifdef OSX
      [g_context clearDrawable];
      [g_context release], g_context = nil;
      [g_format release], g_format = nil;
   
      NSOpenGLPixelFormatAttribute attributes [] = {
         NSOpenGLPFADoubleBuffer,	// double buffered
         NSOpenGLPFADepthSize,
		  (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
#ifdef MAC_OS_X_VERSION_10_7
        (major || minor) ? NSOpenGLPFAOpenGLProfile : 0,
		  (major << 12) | (minor << 8),
#endif
         (NSOpenGLPixelFormatAttribute)nil
      };

      [g_format release];
      [g_context release];

      g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      g_context = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:nil];
      [g_context setView:g_view];
#else
      g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
      g_view.context = g_context;
#endif

      [g_context makeCurrentContext];
   });
   
   [g_context makeCurrentContext];
   return true;
}

void apple_gfx_ctx_swap_interval(unsigned interval)
{
#ifdef IOS // < No way to disable Vsync on iOS?
           //   Just skip presents so fast forward still works.
   g_is_syncing = interval ? true : false;
   g_fast_forward_skips = interval ? 0 : 3;
#elif defined(OSX)
   GLint value = interval ? 1 : 0;
   [g_context setValues:&value forParameter:NSOpenGLCPSwapInterval];
#endif
}

bool apple_gfx_ctx_set_video_mode(unsigned width, unsigned height, bool fullscreen)
{
#ifdef OSX
   dispatch_sync(dispatch_get_main_queue(),
   ^{
      // TODO: Sceen mode support
      
      if (fullscreen && !g_has_went_fullscreen)
      {
         [g_view enterFullScreenMode:get_chosen_screen() withOptions:nil];
         [NSCursor hide];
      }
      else if (!fullscreen && g_has_went_fullscreen)
      {
         [g_view exitFullScreenModeWithOptions:nil];
         [[g_view window] makeFirstResponder:g_view];
         [NSCursor unhide];
      }
      
      g_has_went_fullscreen = fullscreen;
      if (!g_has_went_fullscreen)
         [[g_view window] setContentSize:NSMakeSize(width, height)];
   });
#endif

   // TODO: Maybe iOS users should be apple to show/hide the status bar here?

   return true;
}

void apple_gfx_ctx_get_video_size(unsigned* width, unsigned* height)
{
   RAScreen* screen = get_chosen_screen();
   CGRect size;
	
   if (g_initialized)
   {
#if defined(OSX) && !defined(MAC_OS_X_VERSION_10_7)
      CGRect cgrect = NSRectToCGRect([g_view frame]);
      size = CGRectMake(0, 0, CGRectGetWidth(cgrect), CGRectGetHeight(cgrect));
#else
      size = [g_view bounds];
#endif
   }
   else
      size = [screen bounds];


   *width  = CGRectGetWidth(size)  * [screen scale];
   *height = CGRectGetHeight(size) * [screen scale];
}

void apple_gfx_ctx_update_window_title(void)
{
   static char buf[128], buf_fps[128];
   bool fps_draw = g_settings.fps_show;
   bool got_text = gfx_get_fps(buf, sizeof(buf), fps_draw ? buf_fps : NULL, sizeof(buf_fps));
   static const char* const text = buf; // < Can't access buf directly in the block
    (void)got_text;
    (void)text;
#ifdef OSX
   if (got_text)
   {
      // NOTE: This could go bad if buf is updated again before this completes.
      //       If it poses a problem it should be changed to dispatch_sync.
      dispatch_async(dispatch_get_main_queue(),
      ^{
		  [[g_view window] setTitle:[NSString stringWithCString:text encoding:NSUTF8StringEncoding]];
      });
   }
#endif
   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

bool apple_gfx_ctx_has_focus(void)
{
   return APP_HAS_FOCUS;
}

void apple_gfx_ctx_swap_buffers()
{
   if (--g_fast_forward_skips < 0)
   {
      dispatch_sync(dispatch_get_main_queue(),
      ^{
         [g_view display];
      });

      g_fast_forward_skips = g_is_syncing ? 0 : 3;
   }
}

gfx_ctx_proc_t apple_gfx_ctx_get_proc_address(const char *symbol_name)
{
#ifdef MAC_OS_X_VERSION_10_7
   return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(GLFrameworkID),
                                                            (__bridge CFStringRef)BOXSTRING(symbol_name));
#else
	return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(GLFrameworkID),
															 (CFStringRef)symbol_name);
#endif
}

#ifdef IOS
void apple_bind_game_view_fbo(void)
{
   dispatch_sync(dispatch_get_main_queue(), ^{
      if (g_context)
         [g_view bindDrawable];
   });
}

typedef struct ios_camera
{
  void *empty;
} ioscamera_t;

static void *ios_camera_init(const char *device, uint64_t caps, unsigned width, unsigned height)
{
   if ((caps & (1ULL << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE)) == 0)
   {
      RARCH_ERR("ioscamera returns OpenGL texture.\n");
      return NULL;
   }

   ioscamera_t *ioscamera = (ioscamera_t*)calloc(1, sizeof(ioscamera_t));
   if (!ioscamera)
      return NULL;

   [[RAGameView get] onCameraInit];

   return ioscamera;
}

static void ios_camera_free(void *data)
{
   ioscamera_t *ioscamera = (ioscamera_t*)data;
    
   [[RAGameView get] onCameraFree];

   if (ioscamera)
      free(ioscamera);
   ioscamera = NULL;
}

static bool ios_camera_start(void *data)
{
   (void)data;

   [[RAGameView get] onCameraStart];

   return true;
}

static void ios_camera_stop(void *data)
{
   [[RAGameView get] onCameraStop];
}

static bool ios_camera_poll(void *data, retro_camera_frame_raw_framebuffer_t frame_raw_cb,
      retro_camera_frame_opengl_texture_t frame_gl_cb)
{

   (void)data;
   (void)frame_raw_cb;

   if (frame_gl_cb && newFrame)
   {
	   // FIXME: Identity for now. Use proper texture matrix as returned by iOS Camera (if at all?).
	   static const float affine[] = {
		   1.0f, 0.0f, 0.0f,
		   0.0f, 1.0f, 0.0f,
		   0.0f, 0.0f, 1.0f
	   };
       
	   frame_gl_cb(outputTexture, GL_TEXTURE_2D, affine);
       newFrame = false;
   }

   return true;
}

const camera_driver_t camera_ios = {
   ios_camera_init,
   ios_camera_free,
   ios_camera_start,
   ios_camera_stop,
   ios_camera_poll,
   "ios",
};
#endif

#ifdef HAVE_LOCATION
typedef struct apple_location
{
	void *empty;
} applelocation_t;

static void *apple_location_init()
{
	applelocation_t *applelocation = (applelocation_t*)calloc(1, sizeof(applelocation_t));
	if (!applelocation)
		return NULL;
	
	[[RAGameView get] onLocationInit];
	
	return applelocation;
}

static void apple_location_set_interval(void *data, unsigned interval_update_ms, unsigned interval_distance)
{
   (void)data;
	
	[[RAGameView get] onLocationSetInterval:interval_update_ms interval_update_distance:interval_distance];
}

static void apple_location_free(void *data)
{
	applelocation_t *applelocation = (applelocation_t*)data;
	
	[[RAGameView get] onLocationFree];
	
	if (applelocation)
		free(applelocation);
	applelocation = NULL;
}

static bool apple_location_start(void *data)
{
	(void)data;
	
	[[RAGameView get] onLocationStart];
	
	return true;
}

static void apple_location_stop(void *data)
{
	(void)data;
	
	[[RAGameView get] onLocationStop];
}

static bool apple_location_get_position(void *data, double *lat, double *lon, double *horiz_accuracy,
      double *vert_accuracy)
{
	(void)data;

   bool ret = [[RAGameView get] onLocationHasChanged];

   if (!ret)
      goto fail;
	
	*lat      = [[RAGameView get] onLocationGetLatitude];
   *lon      = [[RAGameView get] onLocationGetLongitude];
   *horiz_accuracy = [[RAGameView get] onLocationGetHorizontalAccuracy];
   *vert_accuracy = [[RAGameView get] onLocationGetVerticalAccuracy];
   return true;

fail:
   *lat            = 0.0;
   *lon            = 0.0;
   *horiz_accuracy = 0.0;
   *vert_accuracy  = 0.0;
   return false;
}

const location_driver_t location_apple = {
	apple_location_init,
	apple_location_free,
	apple_location_start,
	apple_location_stop,
	apple_location_get_position,
	apple_location_set_interval,
	"apple",
};
#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#if TARGET_OS_IPHONE
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif
#if defined(HAVE_COCOA)
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <AppKit/NSScreen.h>
#include <AppKit/NSOpenGL.h>
#elif defined(HAVE_COCOATOUCH)
#include <GLKit/GLKit.h>
#ifdef HAVE_AVFOUNDATION
#import <AVFoundation/AVFoundation.h>
#endif
#endif

#include <retro_assert.h>

#import "../../ui/drivers/cocoa/cocoa_common.h"
#include "../video_context_driver.h"
#include "../../configuration.h"
#include "../../runloop.h"
#include "../../verbosity.h"

#if defined(HAVE_COCOATOUCH)
#define GLContextClass EAGLContext
#define GLFrameworkID CFSTR("com.apple.opengles")
#define RAScreen UIScreen

#ifndef UIUserInterfaceIdiomTV
#define UIUserInterfaceIdiomTV 2
#endif

#ifndef UIUserInterfaceIdiomCarPlay
#define UIUserInterfaceIdiomCarPlay 3
#endif

@interface EAGLContext (OSXCompat) @end
@implementation EAGLContext (OSXCompat)
+ (void)clearCurrentContext { [EAGLContext setCurrentContext:nil];  }
- (void)makeCurrentContext  { [EAGLContext setCurrentContext:self]; }
@end

#else

@interface NSScreen (IOSCompat) @end
@implementation NSScreen (IOSCompat)
- (CGRect)bounds
{
    CGRect cgrect  = NSRectToCGRect(self.frame);
    return CGRectMake(0, 0, CGRectGetWidth(cgrect), CGRectGetHeight(cgrect));
}
- (float) scale  { return 1.0f; }
@end

#define GLContextClass NSOpenGLContext
#define GLFrameworkID CFSTR("com.apple.opengl")
#define RAScreen NSScreen
#endif

#if defined(HAVE_COCOATOUCH)
static GLKView *g_view;
UIView *g_pause_indicator_view;
#endif

static GLContextClass* g_hw_ctx;
static GLContextClass* g_context;

static int g_fast_forward_skips;
static bool g_is_syncing = true;
static bool g_use_hw_ctx;

#if defined(HAVE_COCOA)
static NSOpenGLPixelFormat* g_format;

void *glcontext_get_ptr(void)
{
    return g_context;
}
#endif

static unsigned g_minor = 0;
static unsigned g_major = 0;

/* forward declaration */
void *nsview_get_ptr(void);

#if defined(HAVE_COCOATOUCH)
static void glkitview_init_xibs(void)
{
   /* iOS Pause menu and lifecycle. */
   UINib *xib = (UINib*)[UINib nibWithNibName:BOXSTRING("PauseIndicatorView") bundle:nil];
   g_pause_indicator_view = [[xib instantiateWithOwner:[RetroArch_iOS get] options:nil] lastObject];
}
#endif

void *glkitview_init(void)
{
#if defined(HAVE_COCOATOUCH)
   glkitview_init_xibs();

   g_view = [GLKView new];
   g_view.multipleTouchEnabled = YES;
   g_view.enableSetNeedsDisplay = NO;
   [g_view addSubview:g_pause_indicator_view];
    
   return (__bridge void *)((GLKView*)g_view);
#else
    return nsview_get_ptr();
#endif
}

#if defined(HAVE_COCOATOUCH)
void cocoagl_bind_game_view_fbo(void)
{
#ifdef HAVE_AVFOUNDATION
   /*  Implicitly initializes your audio session */
   AVAudioSession *audio_session = [AVAudioSession sharedInstance];
   [audio_session setCategory:AVAudioSessionCategoryAmbient error:nil];
   [audio_session setActive:YES error:nil];
#endif
   if (g_context)
      [g_view bindDrawable];
}
#endif

static float get_from_selector(Class obj_class, id obj_id, SEL selector, CGFloat *ret)
{
    NSInvocation *invocation = [NSInvocation invocationWithMethodSignature:
    [obj_class instanceMethodSignatureForSelector:selector]];
    [invocation setSelector:selector];
    [invocation setTarget:obj_id];
    [invocation invoke];
    [invocation getReturnValue:ret];
    return *ret;
}

void *get_chosen_screen(void)
{
   settings_t *settings = config_get_ptr();
   NSArray *screens = [RAScreen screens];
   if (!screens || !settings)
      return NULL;

   if (settings->video.monitor_index >= screens.count)
   {
      RARCH_WARN("video_monitor_index is greater than the number of connected monitors; using main screen instead.\n");
#if __has_feature(objc_arc)
      return (__bridge void*)screens;
#else
      return (void*)screens;
#endif
   }
	
#if __has_feature(objc_arc)
   return ((__bridge void*)[screens objectAtIndex:settings->video.monitor_index]);
#else
   return ((void*)[screens objectAtIndex:settings->video.monitor_index]);
#endif
}

float get_backing_scale_factor(void)
{
#if __has_feature(objc_arc)
   RAScreen *screen = (__bridge RAScreen*)get_chosen_screen();
#else
   RAScreen *screen = (RAScreen*)get_chosen_screen();
#endif
   if (!screen)
      return 0.0;
#ifdef HAVE_COCOA
   CGFloat ret;
    CocoaView *g_view = (CocoaView*)nsview_get_ptr();
   SEL selector     = NSSelectorFromString(BOXSTRING("backingScaleFactor"));
   if ([screen respondsToSelector:selector])
        return (float)get_from_selector([[g_view window] class], [g_view window], selector, &ret);
#endif
   return 1.0f;
}

void cocoagl_gfx_ctx_update(void)
{
#if defined(HAVE_COCOA)
#if MAC_OS_X_VERSION_10_7
   CGLUpdateContext(g_context.CGLContextObj);
#else
	[g_context update];
#endif
#endif
}

static void *cocoagl_gfx_ctx_init(void *video_driver)
{
   (void)video_driver;
    
#if defined(HAVE_COCOA)
    CocoaView *g_view = (CocoaView*)nsview_get_ptr();
    if ([g_view respondsToSelector: @selector(setWantsBestResolutionOpenGLSurface:)])
        [g_view setWantsBestResolutionOpenGLSurface:YES];
    
    NSOpenGLPixelFormatAttribute attributes [] = {
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAllowOfflineRenderers,
        NSOpenGLPFADepthSize,
        (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
#ifdef MAC_OS_X_VERSION_10_7
        (g_major || g_minor) ? NSOpenGLPFAOpenGLProfile : 0,
        (g_major << 12) | (g_minor << 8),
#endif
        (NSOpenGLPixelFormatAttribute)0
    };
    
    g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5
    if (g_format == nil)
    {
        /* NSOpenGLFPAAllowOfflineRenderers is 
         not supported on this OS version. */
        attributes[3] = (NSOpenGLPixelFormatAttribute)0;
        g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    }
#endif
    
    if (g_use_hw_ctx)
    {
        //g_hw_ctx  = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:nil];
    }
    g_context = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:(g_use_hw_ctx) ? g_hw_ctx : nil];
    [g_context setView:g_view];
#else
    g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    g_view.context = g_context;
#endif
    
    [g_context makeCurrentContext];
   // Make sure the view was created
   [CocoaView get];
   return (void*)"cocoa";
}

static void cocoagl_gfx_ctx_destroy(void *data)
{
   (void)data;

   [GLContextClass clearCurrentContext];

#if defined(HAVE_COCOA)
    [g_context clearDrawable];
    if (g_context)
        [g_context release];
    g_context = nil;
    if (g_format)
        [g_format release];
    g_format = nil;
    if (g_hw_ctx)
        [g_hw_ctx release];
    g_hw_ctx = nil;
#endif
   [GLContextClass clearCurrentContext];
   g_context = nil;
}

static bool cocoagl_gfx_ctx_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
#if defined(HAVE_COCOATOUCH)
   if (api != GFX_CTX_OPENGL_ES_API)
      return false;
#elif defined(HAVE_COCOA)
   if (api != GFX_CTX_OPENGL_API)
      return false;
#endif
    
   g_minor = minor;
   g_major = major;
  
   return true;
}

static void cocoagl_gfx_ctx_swap_interval(void *data, unsigned interval)
{
   (void)data;
#if defined(HAVE_COCOATOUCH) // < No way to disable Vsync on iOS?
           //   Just skip presents so fast forward still works.
   g_is_syncing = interval ? true : false;
   g_fast_forward_skips = interval ? 0 : 3;
#elif defined(HAVE_COCOA)
   GLint value = interval ? 1 : 0;
   [g_context setValues:&value forParameter:NSOpenGLCPSwapInterval];
    
#endif
}

static void cocoagl_gfx_ctx_show_mouse(void *data, bool state)
{
    (void)data;
    
#ifdef HAVE_COCOA
    if (state)
        [NSCursor unhide];
    else
        [NSCursor hide];
#endif
}

static bool cocoagl_gfx_ctx_set_video_mode(void *data,
        unsigned width, unsigned height, bool fullscreen)
{
#if defined(HAVE_COCOA)
   static bool has_went_fullscreen = false;
   CocoaView *g_view = (CocoaView*)nsview_get_ptr();
   /* TODO: Screen mode support. */
   
   if (fullscreen)
   {
       if (!has_went_fullscreen)
       {
           [g_view enterFullScreenMode:get_chosen_screen() withOptions:nil];
           cocoagl_gfx_ctx_show_mouse(data, false);
       }
   }
   else
   {
      if (has_went_fullscreen)
      {
          [g_view exitFullScreenModeWithOptions:nil];
          [[g_view window] makeFirstResponder:g_view];
          cocoagl_gfx_ctx_show_mouse(data, true);
      }
       
       [[g_view window] setContentSize:NSMakeSize(width, height)];
   }
   
   has_went_fullscreen = fullscreen;
#endif
    
   (void)data;

   // TODO: Maybe iOS users should be able to show/hide the status bar here?

   return true;
}

float cocoagl_gfx_ctx_get_native_scale(void)
{
    static CGFloat ret = 0.0f;
    SEL selector     = NSSelectorFromString(BOXSTRING("nativeScale"));
#if __has_feature(objc_arc)
    RAScreen *screen = (__bridge RAScreen*)get_chosen_screen();
#else
    RAScreen *screen = (RAScreen*)get_chosen_screen();
#endif
    
    if (ret != 0.0f)
       return ret;
    if (!screen)
       return 0.0f;
    
   if ([screen respondsToSelector:selector])
       return (float)get_from_selector([screen class], screen, selector, &ret);
    
   ret          = 1.0f;
   selector     = NSSelectorFromString(BOXSTRING("scale"));
   if ([screen respondsToSelector:selector])
      ret       = screen.scale;
   return ret;
}

static void cocoagl_gfx_ctx_get_video_size(void *data, unsigned* width, unsigned* height)
{
   float screenscale               = cocoagl_gfx_ctx_get_native_scale();
#if defined(HAVE_COCOA)
   CGRect size;
   GLsizei backingPixelWidth, backingPixelHeight;
   CocoaView *g_view               = (CocoaView*)nsview_get_ptr();
   CGRect cgrect                   = NSRectToCGRect([g_view frame]);
#if MAC_OS_X_VERSION_10_7
   SEL selector                    = NSSelectorFromString(BOXSTRING("convertRectToBacking:"));
   if ([g_view respondsToSelector:selector])
      cgrect                       = NSRectToCGRect([g_view convertRectToBacking:[g_view bounds]]);
#endif
   backingPixelWidth               = CGRectGetWidth(cgrect);
   backingPixelHeight              = CGRectGetHeight(cgrect);
   size                            = CGRectMake(0, 0, backingPixelWidth, backingPixelHeight);
#else
   CGRect size                     = g_view.bounds;
#endif
   *width                          = CGRectGetWidth(size)  * screenscale;
   *height                         = CGRectGetHeight(size) * screenscale;
}

static void cocoagl_gfx_ctx_update_window_title(void *data)
{
   static char buf[128] = {0};
   static char buf_fps[128] = {0};
   bool got_text = video_monitor_get_fps(buf, sizeof(buf),
         buf_fps, sizeof(buf_fps));
   settings_t *settings = config_get_ptr();

   (void)got_text;

#if defined(HAVE_COCOA)
   CocoaView *g_view = (CocoaView*)nsview_get_ptr();
   static const char* const text = buf; /* < Can't access buffer directly in the block */
   if (got_text)
       [[g_view window] setTitle:[NSString stringWithCString:text encoding:NSUTF8StringEncoding]];
#endif
    if (settings->fps_show)
        runloop_msg_queue_push(buf_fps, 1, 1, false);
}

static bool cocoagl_gfx_ctx_get_metrics(void *data, enum display_metric_types type,
            float *value)
{
#if __has_feature(objc_arc)
    RAScreen *screen               = (__bridge RAScreen*)get_chosen_screen();
#else
    RAScreen *screen               = (RAScreen*)get_chosen_screen();
#endif
#if defined(HAVE_COCOA)
    NSDictionary *description     = [screen deviceDescription];
    NSSize  display_pixel_size    = [[description objectForKey:NSDeviceSize] sizeValue];
    CGSize  display_physical_size = CGDisplayScreenSize(
        [[description objectForKey:@"NSScreenNumber"] unsignedIntValue]);
    
    float   display_width         = display_pixel_size.width;
    float   display_height        = display_pixel_size.height;
    float   physical_width        = display_physical_size.width;
    float   physical_height       = display_physical_size.height;
    float   scale                 = get_backing_scale_factor();
    float   dpi                   = (display_width/ physical_width) * 25.4f * scale;
#elif defined(HAVE_COCOATOUCH)
    float   scale                 = cocoagl_gfx_ctx_get_native_scale();
    CGRect  screen_rect           = [screen bounds];
    float   display_height        = screen_rect.size.height;
    float   physical_width        = screen_rect.size.width  * scale;
    float   physical_height       = screen_rect.size.height * scale;
    float   dpi                   = 160                     * scale;
    unsigned idiom_type           = UI_USER_INTERFACE_IDIOM();

    switch (idiom_type)
    {
       case -1: /* UIUserInterfaceIdiomUnspecified */
          /* TODO */
          break;
       case UIUserInterfaceIdiomPad:
          dpi = 132 * scale;
          break;
       case UIUserInterfaceIdiomPhone:
          dpi = 163 * scale;
          break;
       case UIUserInterfaceIdiomTV:
       case UIUserInterfaceIdiomCarPlay:
          /* TODO */
          break;
    }
#endif
    
    (void)display_height;
    
    switch (type)
    {
        case DISPLAY_METRIC_MM_WIDTH:
            *value = physical_width;
            break;
        case DISPLAY_METRIC_MM_HEIGHT:
            *value = physical_height;
            break;
        case DISPLAY_METRIC_DPI:
            *value = dpi;
            break;
        case DISPLAY_METRIC_NONE:
        default:
            *value = 0;
            return false;
    }
    
    return true;
}

static bool cocoagl_gfx_ctx_has_focus(void *data)
{
   (void)data;
#if defined(HAVE_COCOATOUCH)
    return ([[UIApplication sharedApplication] applicationState] == UIApplicationStateActive);
#else
    return [NSApp isActive];
#endif
}

static bool cocoagl_gfx_ctx_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;

   return false;
}

static bool cocoagl_gfx_ctx_has_windowed(void *data)
{
   (void)data;

#if defined(HAVE_COCOATOUCH)
   return false;
#else
   return true;
#endif
}

static void cocoagl_gfx_ctx_swap_buffers(void *data)
{
   if (!(--g_fast_forward_skips < 0))
      return;
    
#if defined(HAVE_COCOA)
    [g_context flushBuffer];
#elif defined(HAVE_COCOATOUCH)
    if (g_view)
        [g_view display];
#endif
    
   g_fast_forward_skips = g_is_syncing ? 0 : 3;
}

static gfx_ctx_proc_t cocoagl_gfx_ctx_get_proc_address(const char *symbol_name)
{
   return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(GLFrameworkID),
   (
#if __has_feature(objc_arc)
         __bridge
#endif
CFStringRef)BOXSTRING(symbol_name)
         );
}

static void cocoagl_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   unsigned new_width, new_height;
   (void)frame_count;

   *quit = false;

   cocoagl_gfx_ctx_get_video_size(data, &new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static bool cocoagl_gfx_ctx_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
   return false;
}

static void cocoagl_gfx_ctx_input_driver(void *data, const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
   *input_data = NULL;
}

static void cocoagl_gfx_ctx_bind_hw_render(void *data, bool enable)
{
   (void)data;
   g_use_hw_ctx = enable;
    
    if (enable)
        [g_hw_ctx makeCurrentContext];
    else
        [g_context makeCurrentContext];
}

static uint32_t cocoagl_gfx_ctx_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);
   return flags;
}

static void cocoagl_gfx_ctx_set_flags(void *data, uint32_t flags)
{
   (void)flags;
}

const gfx_ctx_driver_t gfx_ctx_cocoagl = {
   cocoagl_gfx_ctx_init,
   cocoagl_gfx_ctx_destroy,
   cocoagl_gfx_ctx_bind_api,
   cocoagl_gfx_ctx_swap_interval,
   cocoagl_gfx_ctx_set_video_mode,
   cocoagl_gfx_ctx_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   cocoagl_gfx_ctx_get_metrics,
   NULL,
   cocoagl_gfx_ctx_update_window_title,
   cocoagl_gfx_ctx_check_window,
   cocoagl_gfx_ctx_set_resize,
   cocoagl_gfx_ctx_has_focus,
   cocoagl_gfx_ctx_suppress_screensaver,
   cocoagl_gfx_ctx_has_windowed,
   cocoagl_gfx_ctx_swap_buffers,
   cocoagl_gfx_ctx_input_driver,
   cocoagl_gfx_ctx_get_proc_address,
   NULL,
   NULL,
   NULL,
   "cocoagl",
   cocoagl_gfx_ctx_get_flags,
   cocoagl_gfx_ctx_set_flags,
   cocoagl_gfx_ctx_bind_hw_render,
};

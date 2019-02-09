/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

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
#endif

#include <retro_assert.h>
#include <compat/apple_compat.h>

#include "../../ui/drivers/ui_cocoa.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include "../video_driver.h"
#include "../../configuration.h"
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

static enum gfx_ctx_api cocoagl_api = GFX_CTX_NONE;

typedef struct cocoa_ctx_data
{
   bool core_hw_context_enable;
} cocoa_ctx_data_t;

#if defined(HAVE_COCOATOUCH)

static GLKView *g_view;
UIView *g_pause_indicator_view;
#endif

static GLContextClass* g_hw_ctx;
static GLContextClass* g_context;

static int g_fast_forward_skips;
static bool g_is_syncing = true;
static bool g_use_hw_ctx = false;

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

#include "cocoa_gl_shared.h"

static void *cocoagl_gfx_ctx_init(video_frame_info_t *video_info, void *video_driver)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)
      calloc(1, sizeof(cocoa_ctx_data_t));

   if (!cocoa_ctx)
      return NULL;

   return cocoa_ctx;
}

static bool cocoagl_gfx_ctx_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
#if defined(HAVE_COCOATOUCH)
   if (api != GFX_CTX_OPENGL_ES_API)
      return false;
#elif defined(HAVE_COCOA)
   if (api != GFX_CTX_OPENGL_API)
      return false;
#endif

   cocoagl_api = api;
   g_minor     = minor;
   g_major     = major;

   return true;
}

static void cocoagl_gfx_ctx_swap_interval(void *data, int i)
{
   (void)data;
   unsigned interval = (unsigned)i;
#if defined(HAVE_COCOATOUCH) // < No way to disable Vsync on iOS?
           //   Just skip presents so fast forward still works.
   g_is_syncing = interval ? true : false;
   g_fast_forward_skips = interval ? 0 : 3;
#elif defined(HAVE_COCOA)
   GLint value = interval ? 1 : 0;
   [g_context setValues:&value forParameter:NSOpenGLCPSwapInterval];

#endif
}

static bool cocoagl_gfx_ctx_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height, bool fullscreen)
{
#if defined(HAVE_COCOA)
    CocoaView *g_view = (CocoaView*)nsview_get_ptr();
    if ([g_view respondsToSelector: @selector(setWantsBestResolutionOpenGLSurface:)])
        [g_view setWantsBestResolutionOpenGLSurface:YES];

    NSOpenGLPixelFormatAttribute attributes [] = {
        NSOpenGLPFAColorSize,
        24,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAllowOfflineRenderers,
        NSOpenGLPFADepthSize,
        (NSOpenGLPixelFormatAttribute)16, // 16 bit depth buffer
        0,                                /* profile */
        0,                                /* profile enum */
        (NSOpenGLPixelFormatAttribute)0
    };

#if MAC_OS_X_VERSION_10_7
    if (g_major == 3 && (g_minor >= 1 && g_minor <= 3))
    {
       attributes[6] = NSOpenGLPFAOpenGLProfile;
       attributes[7] = NSOpenGLProfileVersion3_2Core;
    }
#endif

#if MAC_OS_X_VERSION_10_10
    if (g_major == 4 && g_minor == 1)
    {
       attributes[6] = NSOpenGLPFAOpenGLProfile;
       attributes[7] = NSOpenGLProfileVersion4_1Core;
    }
#endif

    g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1050
    if (g_format == nil)
    {
        /* NSOpenGLFPAAllowOfflineRenderers is
         not supported on this OS version. */
        attributes[3] = (NSOpenGLPixelFormatAttribute)0;
        g_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
    }
#endif

    if (g_use_hw_ctx)
        g_hw_ctx  = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:nil];
    g_context = [[NSOpenGLContext alloc] initWithFormat:g_format shareContext:(g_use_hw_ctx) ? g_hw_ctx : nil];
    [g_context setView:g_view];
#else
    if (g_use_hw_ctx)
        g_hw_ctx = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    g_context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    g_view.context = g_context;
#endif

    [g_context makeCurrentContext];

#if defined(HAVE_COCOA)
   static bool has_went_fullscreen = false;
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

   /* TODO: Maybe iOS users should be able to show/hide the status bar here? */

   return true;
}

static void cocoagl_gfx_ctx_swap_buffers(void *data, void *data2)
{
   if (!(--g_fast_forward_skips < 0))
      return;

#if defined(HAVE_COCOA)
    [g_context flushBuffer];
    [g_hw_ctx flushBuffer];
#elif defined(HAVE_COCOATOUCH)
    if (g_view)
        [g_view display];
#endif

   g_fast_forward_skips = g_is_syncing ? 0 : 3;
}

static gfx_ctx_proc_t cocoagl_gfx_ctx_get_proc_address(const char *symbol_name)
{
   return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(CFBundleGetBundleWithIdentifier(GLFrameworkID),
   (BRIDGE CFStringRef)BOXSTRING(symbol_name)
         );
}

static void cocoagl_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   unsigned new_width, new_height;

   *quit = false;

   cocoagl_gfx_ctx_get_video_size(data, &new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
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

const gfx_ctx_driver_t gfx_ctx_cocoagl = {
   cocoagl_gfx_ctx_init,
   cocoagl_gfx_ctx_destroy,
   cocoagl_gfx_ctx_get_api,
   cocoagl_gfx_ctx_bind_api,
   cocoagl_gfx_ctx_swap_interval,
   cocoagl_gfx_ctx_set_video_mode,
   cocoagl_gfx_ctx_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   cocoagl_gfx_ctx_get_metrics,
   NULL,
#if defined(HAVE_COCOA)
   cocoagl_gfx_ctx_update_title,
#else
   NULL, /* update_title */
#endif
   cocoagl_gfx_ctx_check_window,
   NULL, /* set_resize */
   cocoagl_gfx_ctx_has_focus,
   cocoagl_gfx_ctx_suppress_screensaver,
#if defined(HAVE_COCOATOUCH)
   NULL,
#else
   cocoagl_gfx_ctx_has_windowed,
#endif
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
   NULL,
   NULL
};

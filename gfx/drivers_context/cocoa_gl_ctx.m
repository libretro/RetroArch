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

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#include <CoreGraphics/CoreGraphics.h>
#else
#include <ApplicationServices/ApplicationServices.h>
#endif
#ifdef OSX
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <AppKit/NSScreen.h>
#include <AppKit/NSOpenGL.h>
#elif defined(HAVE_COCOATOUCH)
#include <GLKit/GLKit.h>
#endif

#include <retro_timers.h>
#include <compat/apple_compat.h>
#include <string/stdstring.h>

#include "../../ui/drivers/ui_cocoa.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include "../../ui/drivers/cocoa/apple_platform.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#if defined(HAVE_COCOATOUCH)
#define GLContextClass  EAGLContext
#define GLFrameworkID   CFSTR("com.apple.opengles")
#else
#define GLContextClass  NSOpenGLContext
#define GLFrameworkID   CFSTR("com.apple.opengl")
#endif

enum cocoa_ctx_flags
{
   COCOA_CTX_FLAG_IS_SYNCING          = (1 << 0),
   COCOA_CTX_FLAG_CORE_HW_CTX_ENABLE  = (1 << 1),
   COCOA_CTX_FLAG_USE_HW_CTX          = (1 << 2)
};

typedef struct cocoa_ctx_data
{
#ifndef OSX
   int fast_forward_skips;
#endif
   unsigned width;
   unsigned height;
   uint8_t flags;
} cocoa_ctx_data_t;

/* TODO/FIXME - static globals */
static enum gfx_ctx_api cocoagl_api = GFX_CTX_NONE;
static GLContextClass* g_hw_ctx     = NULL;
static GLContextClass* g_ctx        = NULL;
static unsigned g_gl_minor          = 0;
static unsigned g_gl_major          = 0;
#if defined(HAVE_COCOATOUCH)
static GLKView *glk_view            = NULL;
#endif

/* Forward declaration */
CocoaView *cocoaview_get(void);

static uint32_t cocoa_gl_gfx_ctx_get_flags(void *data)
{
   uint32_t flags                 = 0;
   cocoa_ctx_data_t    *cocoa_ctx = (cocoa_ctx_data_t*)data;

   if (cocoa_ctx->flags & COCOA_CTX_FLAG_CORE_HW_CTX_ENABLE)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_GLSL
         BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
         break;
      case GFX_CTX_OPENGL_API:
         if (string_is_equal(video_driver_get_ident(), "gl1")) { }
         else
         {
            if (string_is_equal(video_driver_get_ident(), "glcore"))
            {
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
               BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
            }
#ifdef HAVE_GLSL
            BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
         }
         break;
      default:
         break;
   }

   return flags;
}

static void cocoa_gl_gfx_ctx_set_flags(void *data, uint32_t flags)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;

   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      cocoa_ctx->flags |= COCOA_CTX_FLAG_CORE_HW_CTX_ENABLE;
}

#if defined(OSX)
void cocoa_gl_gfx_ctx_update(void)
{
   [g_ctx    update];
   [g_hw_ctx update];
}
#else
#if defined(HAVE_COCOATOUCH)
void *glkitview_init(void)
{
   /* Delete the old view's framebuffer while we may still have a valid
    * GL context.  Without this, the subsequent release of the old GLKView
    * triggers _deleteFramebuffer during dealloc, which calls
    * glPushGroupMarkerEXT with no current context and crashes. */
   if (glk_view)
      [glk_view deleteDrawable];
   /* RELEASE the old view (+1 from the previous [GLKView new]) before
    * overwriting the static.  Under ARC the strong static would retain/
    * release on store, but under MRR the raw assignment below would drop
    * the +1 and leak one GLKView per re-init. */
   RELEASE(glk_view);

   glk_view                      = [GLKView new];
#if TARGET_OS_IOS
   glk_view.multipleTouchEnabled = YES;
#endif
   glk_view.enableSetNeedsDisplay = NO;

   return (BRIDGE void *)((GLKView*)glk_view);
}

void glkitview_bind_fbo(void)
{
   if (glk_view)
      [glk_view bindDrawable];
}
#endif
#endif


static void cocoa_gl_gfx_ctx_destroy(void *data)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;

   if (!cocoa_ctx)
      return;
#ifdef OSX
   [GLContextClass clearCurrentContext];
   [g_ctx clearDrawable];
   if (g_hw_ctx)
      [g_hw_ctx clearDrawable];
   [GLContextClass clearCurrentContext];
#else
   /* Clean up GLKView's framebuffer resources while context is still valid.
    * Failing to do this causes crashes in glPushGroupMarkerEXT when GLKit
    * tries to delete framebuffers after the context has been destroyed. */
   if (glk_view)
      [glk_view deleteDrawable];
   [EAGLContext setCurrentContext:nil];
#endif
   /* RELEASE -releases + nils under MRR, just nils under ARC (see
    * cocoa_common.h).  Doing this unconditionally matches the OSX
    * path's previous behaviour and fixes a latent iOS-MRR leak where
    * the +1 from [[EAGLContext alloc] initWithAPI:...] in set_video_mode
    * was dropped with a raw 'g_ctx = nil'. */
   RELEASE(g_ctx);
   RELEASE(g_hw_ctx);
   /* Deliberately NOT releasing glk_view here.  Its real strong owner
    * is RetroArch_iOS._renderView, which retains the singleton via
    * setViewType:APPLE_VIEW_TYPE_OPENGL_ES on first init and holds it
    * for the rest of the process.  On a video reinit (e.g. content
    * load) the ctx destroy/init pair runs, but setViewType: returns
    * early on (vt == _vt) and never re-invokes glkitview_init - so
    * nilling the static here would leave it nil for the remainder of
    * the session.  The consequence is that swap_buffers'
    *   if (glk_view) [glk_view display];
    * becomes a no-op, get_video_size reads zero from glk_view.bounds,
    * set_video_mode's `glk_view.context = g_ctx` silently misses, and
    * the screen freezes the moment a core is loaded.  The MRR leak
    * this previously aimed to plug only matters at app teardown,
    * which on iOS effectively never runs (RetroArch_iOS is the
    * UIApplication delegate). */

   free(cocoa_ctx);
}

static enum gfx_ctx_api cocoa_gl_gfx_ctx_get_api(void *data) { return cocoagl_api; }

static bool cocoa_gl_gfx_ctx_suppress_screensaver(void *data, bool enable) { return false; }

static void cocoa_gl_gfx_ctx_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

#if MAC_OS_X_VERSION_10_7 && defined(OSX)
/* NOTE: convertRectToBacking only available on MacOS X 10.7 and up.
 * Therefore, make specialized version of this function instead of
 * going through a selector for every call. */
static void cocoa_gl_gfx_ctx_get_video_size_osx10_7_and_up(void *data,
      unsigned* width, unsigned* height)
{
   CocoaView *g_view               = cocoaview_get();
   CGRect _cgrect                  = NSRectToCGRect(g_view.frame);
   CGRect bounds                   = CGRectMake(0, 0, CGRectGetWidth(_cgrect), CGRectGetHeight(_cgrect));
   CGRect cgrect                   = NSRectToCGRect([g_view convertRectToBacking:bounds]);
   GLsizei backingPixelWidth       = CGRectGetWidth(cgrect);
   GLsizei backingPixelHeight      = CGRectGetHeight(cgrect);
   CGRect size                     = CGRectMake(0, 0, backingPixelWidth, backingPixelHeight);
   *width                          = CGRectGetWidth(size);
   *height                         = CGRectGetHeight(size);
}
#elif defined(OSX)
static void cocoa_gl_gfx_ctx_get_video_size(void *data,
      unsigned* width, unsigned* height)
{
   CocoaView *g_view               = cocoaview_get();
   CGRect cgrect                   = NSRectToCGRect([g_view frame]);
   GLsizei backingPixelWidth       = CGRectGetWidth(cgrect);
   GLsizei backingPixelHeight      = CGRectGetHeight(cgrect);
   CGRect size                     = CGRectMake(0, 0, backingPixelWidth, backingPixelHeight);
   *width                          = CGRectGetWidth(size);
   *height                         = CGRectGetHeight(size);
}
#else
/* iOS */
static void cocoa_gl_gfx_ctx_get_video_size(void *data,
      unsigned* width, unsigned* height)
{
   CGRect size                     = glk_view.bounds;
   float viewScale                 = [glk_view contentScaleFactor];
   *width                          = CGRectGetWidth(size)  * viewScale;
   *height                         = CGRectGetHeight(size) * viewScale;
}
#endif

static float cocoa_gl_gfx_ctx_get_refresh_rate(void *data)
{
   /* Body consolidated into cocoa_common.m.  Kept as a named
    * vtable entry because vulkan.c-style code paths reach the
    * ctx driver directly via video_context_driver_get_refresh_rate,
    * bypassing dispserv_apple's own hook. */
   return cocoa_get_refresh_rate();
}

static gfx_ctx_proc_t cocoa_gl_gfx_ctx_get_proc_address(const char *symbol_name)
{
   return (gfx_ctx_proc_t)CFBundleGetFunctionPointerForName(
         CFBundleGetBundleWithIdentifier(GLFrameworkID),
         (BRIDGE CFStringRef)BOXSTRING(symbol_name)
         );
}

static void cocoa_gl_gfx_ctx_bind_hw_render(void *data, bool enable)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;

   cocoa_ctx->flags           |= COCOA_CTX_FLAG_USE_HW_CTX;

#ifdef OSX
   if (enable)
      [g_hw_ctx makeCurrentContext];
   else
      [g_ctx makeCurrentContext];
#else
   if (enable)
      [EAGLContext setCurrentContext:g_hw_ctx];
   else
      [EAGLContext setCurrentContext:g_ctx];
#endif

}

static void cocoa_gl_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   unsigned new_width, new_height;

   *quit                       = false;

#if MAC_OS_X_VERSION_10_7 && defined(OSX)
   cocoa_gl_gfx_ctx_get_video_size_osx10_7_and_up(data, &new_width, &new_height);
#else
   cocoa_gl_gfx_ctx_get_video_size(data, &new_width, &new_height);
#endif

   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static void cocoa_gl_gfx_ctx_swap_interval(void *data, int i)
{
   unsigned interval             = (unsigned)i;
#ifdef OSX
   GLint value                   = interval ? 1 : 0;
   [g_ctx setValues:&value forParameter:NSOpenGLCPSwapInterval];
#else
   cocoa_ctx_data_t *cocoa_ctx   = (cocoa_ctx_data_t*)data;
   /* < No way to disable Vsync on iOS? */
   /*   Just skip presents so fast forward still works. */
   if (interval)
      cocoa_ctx->flags          |=  COCOA_CTX_FLAG_IS_SYNCING;
   else
      cocoa_ctx->flags          &= ~COCOA_CTX_FLAG_IS_SYNCING;
   cocoa_ctx->fast_forward_skips = interval ? 0 : 3;
#endif
}

static void cocoa_gl_gfx_ctx_swap_buffers(void *data)
{
#ifdef OSX
   [g_ctx flushBuffer];
   [g_hw_ctx  flushBuffer];
#else
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   if (!(--cocoa_ctx->fast_forward_skips < 0))
      return;
   if (glk_view)
      [glk_view display];
   cocoa_ctx->fast_forward_skips =
      (cocoa_ctx->flags & COCOA_CTX_FLAG_IS_SYNCING) ? 0 : 3;
#endif
}

static bool cocoa_gl_gfx_ctx_bind_api(void *data, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
   cocoagl_api = api;
   g_gl_minor  = minor;
   g_gl_major  = major;

   return true;
}

#ifdef OSX
static bool cocoa_gl_gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height, bool fullscreen)
{
#if defined(HAVE_COCOA_METAL)
   gfx_ctx_mode_t mode;
   NSView *g_view              = apple_platform.renderView;
#elif defined(HAVE_COCOA)
   CocoaView *g_view           = (CocoaView*)nsview_get_ptr();
#endif
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   static bool
      has_went_fullscreen      = false;
   cocoa_ctx->width            = width;
   cocoa_ctx->height           = height;

   /* NOTE: setWantsBestResolutionOpenGLSurface only
    * available on MacOS X 10.7 and up.
    * Deprecated as of MacOS X 10.14. */
#if MAC_OS_X_VERSION_10_7
   [g_view setWantsBestResolutionOpenGLSurface:YES];
#endif

   {
      NSOpenGLPixelFormat *fmt;
      NSOpenGLPixelFormatAttribute attributes [] = {
         NSOpenGLPFAColorSize,
         24,
         NSOpenGLPFADoubleBuffer,
         NSOpenGLPFAAllowOfflineRenderers,
         NSOpenGLPFADepthSize,
         (NSOpenGLPixelFormatAttribute)16, /* 16 bit depth buffer */
         0,                                /* profile */
         0,                                /* profile enum */
         (NSOpenGLPixelFormatAttribute)0
      };

      switch (g_gl_major)
      {
         case 3:
#if MAC_OS_X_VERSION_10_7
            attributes[6] = NSOpenGLPFAOpenGLProfile;
            attributes[7] = NSOpenGLProfileVersion3_2Core;
#endif
            break;
         case 4:
#if MAC_OS_X_VERSION_10_10
            attributes[6] = NSOpenGLPFAOpenGLProfile;
            attributes[7] = NSOpenGLProfileVersion4_1Core;
#endif
            break;
      }

      fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];

      /* If pixel-format creation failed with NSOpenGLPFAAllowOfflineRenderers
       * (added in 10.5 - some drivers on early 10.5 builds could reject it
       * even though the SDK exposed the constant), retry without it.  The
       * previous guard here was #if MAC_OS_X_VERSION_MIN_REQUIRED < 1050,
       * which meant "compile this fallback only when targeting pre-10.5" -
       * the opposite of what was intended and unreachable in practice since
       * the static `attributes` array above already references
       * NSOpenGLPFAAllowOfflineRenderers unconditionally, so the file
       * demands a 10.5+ SDK (MAC_OS_X_VERSION_MAX_ALLOWED >= 1050) just
       * to compile.  The fallback is now always available at runtime and
       * only exercised when the first -initWithAttributes: returns nil. */
      if (fmt == nil)
      {
         attributes[3]  = (NSOpenGLPixelFormatAttribute)0;
         fmt            = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      }

      if (cocoa_ctx->flags & COCOA_CTX_FLAG_USE_HW_CTX)
      {
         /* In the normal reinit flow -destroy runs before -set_video_mode
          * and both statics are already nil here; guard defensively so an
          * MRR build does not leak the previous +1 if that invariant ever
          * breaks (e.g. a future caller that re-inits without tearing
          * down first).  Safe when already nil under both ARC and MRR. */
         RELEASE(g_ctx);
         RELEASE(g_hw_ctx);
         g_hw_ctx       = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil];
         g_ctx          = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:g_hw_ctx];
      }
      else
      {
         RELEASE(g_ctx);
         g_ctx          = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil];
      }

      RELEASE(fmt);
   }

   [g_ctx setView:g_view];
   {
      /* -[NSWindow setColorSpace:] is NS_AVAILABLE_MAC(10_6).  On 10.5
       * Leopard the selector doesn't exist and the runtime throws
       * "unrecognized selector".  Without this call the window simply
       * uses the default colour space (which is what 10.5 always did
       * anyway), so skip it on systems that lack the method.
       * +[NSColorSpace sRGBColorSpace] itself is 10.5+ and is safe. */
      NSWindow *win = [g_view window];
      if ([win respondsToSelector:@selector(setColorSpace:)])
         [win setColorSpace:[NSColorSpace sRGBColorSpace]];
   }
#ifdef OSX
   [g_ctx makeCurrentContext];
#else
   [EAGLContext setCurrentContext:g_ctx];
#endif

#ifdef HAVE_COCOA_METAL
   mode.width           = width;
   mode.height          = height;
   mode.fullscreen      = fullscreen;
   [apple_platform setVideoMode:mode];
   cocoa_show_mouse(data, !fullscreen);
#else
   /* Hand-rolled fullscreen for the non-Metal path.
    *
    * The previous implementation called -[NSView enterFullScreenMode:
    * withOptions:], which internally captures all displays and moves
    * the view into an AppKit-manufactured NSWindow.  That replacement
    * window is a plain NSWindow, not RAWindow, so -[RAWindow sendEvent:]
    * (the event-pump override that feeds cocoa_input, added in commit
    * 23a945639) stops firing while fullscreen, and keystrokes / mouse
    * clicks get dropped.
    *
    * Instead, create our own borderless RAWindow covering the chosen
    * screen, move the CocoaView into it, and show it above the menu
    * bar.  Because the fullscreen window is itself an RAWindow, our
    * sendEvent: override keeps firing.  SDL, GLFW, and similar
    * libraries use this same pattern for pre-Lion fullscreen on macOS.
    *
    * Extra constraint: on 10.5 Leopard, -[NSWindow setStyleMask:]
    * doesn't exist, so we can't toggle the existing window's style
    * between titled and borderless - the new-window approach is the
    * only option that works on every macOS version we target.
    *
    * HAVE_COCOA_METAL is unaffected: that path goes through
    * -[apple_platform setVideoMode:] above, which drives the native
    * -[NSWindow toggleFullScreen:] API on 10.7+. */
   static NSWindow *saved_windowed_window = NULL;
   static NSWindow *fullscreen_window     = NULL;
   static NSRect    saved_view_frame;

   if (fullscreen)
   {
      if (!has_went_fullscreen)
      {
         NSScreen *screen        = (BRIDGE NSScreen *)cocoa_screen_get_chosen();
         NSRect    screen_frame  = [screen frame];
         /* Look up RAWindow at runtime rather than pulling its
          * @interface out of ui_cocoa.m into a shared header. */
         Class     ra_window_cls = NSClassFromString(@"RAWindow");

         /* Remember where the view lived so we can put it back on exit. */
         saved_windowed_window   = [[g_view window] retain];
         saved_view_frame        = [g_view frame];

         /* Build the fullscreen host window.  NSBorderlessWindowMask is
          * 0 on every macOS version, identical 10.5 through modern.
          * Raising above NSMainMenuWindowLevel is belt-and-braces once
          * the menu bar is hidden below. */
         fullscreen_window = [[ra_window_cls alloc]
               initWithContentRect:screen_frame
                         styleMask:NSBorderlessWindowMask
                           backing:NSBackingStoreBuffered
                             defer:NO];
         [fullscreen_window setLevel:NSMainMenuWindowLevel + 1];
         [fullscreen_window setOpaque:YES];
         [fullscreen_window setHidesOnDeactivate:YES];

         /* Hide menu bar + Dock.  Only valid when fullscreening onto
          * screen 0 (the screen that owns the menu bar); on a
          * secondary screen the menu bar stays put and hiding it would
          * mangle the primary screen. */
         if ([[NSScreen screens] count] > 0
               && [screen isEqual:[[NSScreen screens] objectAtIndex:0]])
            [NSMenu setMenuBarVisible:NO];

         /* Move the CocoaView from the windowed window into the
          * fullscreen window.  Retain across the move so the view
          * isn't released by removeFromSuperview... if it happened
          * to hold the last reference. */
         [g_view retain];
         [g_view removeFromSuperviewWithoutNeedingDisplay];
         [[fullscreen_window contentView] addSubview:g_view];
         /* -[NSWindow contentView] returns id on the 10.5-10.9 SDKs,
          * which means GCC can resolve -bounds either to -[NSView
          * bounds] (NSRect) or -[CALayer bounds] (CGRect).  On 32-bit
          * Darwin those are distinct incompatible structs, so the
          * implicit CGRect -> NSRect (setFrame:'s parameter) coercion
          * fails to compile.  Cast the receiver to NSView* so the
          * right -bounds wins.  Same fix class as 8e428f4e67. */
         [g_view setFrame:[(NSView*)[fullscreen_window contentView] bounds]];
         [g_view release];

         /* Order the windowed window out, bring the fullscreen window
          * up, and route keystrokes to the view. */
         [saved_windowed_window orderOut:nil];
         [fullscreen_window makeKeyAndOrderFront:nil];
         [fullscreen_window makeFirstResponder:g_view];

         cocoa_show_mouse(data, false);
      }
   }
   else
   {
      if (has_went_fullscreen && fullscreen_window)
      {
         /* Put the view back in the windowed window. */
         [g_view retain];
         [g_view removeFromSuperviewWithoutNeedingDisplay];
         [[saved_windowed_window contentView] addSubview:g_view];
         [g_view setFrame:saved_view_frame];
         [g_view release];

         /* Restore the menu bar, tear down the fullscreen window,
          * bring the windowed window back. */
         [NSMenu setMenuBarVisible:YES];

         [fullscreen_window orderOut:nil];
         [fullscreen_window release];
         fullscreen_window = NULL;

         [saved_windowed_window makeKeyAndOrderFront:nil];
         [saved_windowed_window makeFirstResponder:g_view];
         [saved_windowed_window release];
         saved_windowed_window = NULL;

         cocoa_show_mouse(data, true);
      }

      [[g_view window] setContentSize:NSMakeSize(width, height)];
   }
#endif

   has_went_fullscreen = fullscreen;

   return true;
}

static void *cocoa_gl_gfx_ctx_init(void *video_driver)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)
      calloc(1, sizeof(cocoa_ctx_data_t));

   if (!cocoa_ctx)
      return NULL;

#ifndef OSX
   cocoa_ctx->flags |= COCOA_CTX_FLAG_IS_SYNCING;
#endif

#if defined(HAVE_COCOA_METAL)
   [apple_platform setViewType:APPLE_VIEW_TYPE_OPENGL];
#endif

   return cocoa_ctx;
}
#else
static bool cocoa_gl_gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height, bool fullscreen)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;

   /* In the normal reinit flow -destroy runs before -set_video_mode and
    * both statics are already nil here; guard defensively so an iOS-MRR
    * build does not leak the previous +1 if that invariant ever breaks
    * (e.g. a future caller that re-inits without tearing down first). */
   RELEASE(g_ctx);
   RELEASE(g_hw_ctx);

#if defined(HAVE_OPENGLES3)
   if (cocoa_ctx->flags & COCOA_CTX_FLAG_USE_HW_CTX)
   {
      g_hw_ctx      = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
      g_ctx         = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3 sharegroup:g_hw_ctx.sharegroup];
   }
   else
      g_ctx         = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
#elif defined(HAVE_OPENGLES2)
   if (cocoa_ctx->flags & COCOA_CTX_FLAG_USE_HW_CTX)
   {
      g_hw_ctx      = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
      g_ctx         = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2 sharegroup:g_hw_ctx.sharegroup];
   }
   else
      g_ctx         = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
#endif

#ifdef OSX
   [g_ctx makeCurrentContext];
#else
   [EAGLContext setCurrentContext:g_ctx];
#endif

   glk_view.context = g_ctx;

   /* TODO: Maybe iOS users should be able to
    * show/hide the status bar here? */
   return true;
}

static void *cocoa_gl_gfx_ctx_init(void *video_driver)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)
   calloc(1, sizeof(cocoa_ctx_data_t));

   if (!cocoa_ctx)
      return NULL;

#ifndef OSX
   cocoa_ctx->flags |= COCOA_CTX_FLAG_IS_SYNCING;
#endif

   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_ES_API:
#if defined(HAVE_COCOA_METAL)
         /* The Metal build supports both the OpenGL
          * and Metal video drivers */
         [apple_platform setViewType:APPLE_VIEW_TYPE_OPENGL_ES];
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return cocoa_ctx;
}
#endif

#ifdef HAVE_COCOA_METAL
static bool cocoa_gl_gfx_ctx_set_resize(void *data, unsigned width, unsigned height)
{
   return true;
}
#endif

static void cocoa_gl_gfx_ctx_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   /* Body consolidated into cocoa_common.m.  Kept as a named
    * vtable entry because video_thread_wrapper.c's
    * thread_get_video_output_size calls the poke / ctx hook
    * directly, bypassing dispserv_apple. */
   cocoa_get_video_output_size(width, height, desc, desc_len);
}

const gfx_ctx_driver_t gfx_ctx_cocoagl = {
   cocoa_gl_gfx_ctx_init,
   cocoa_gl_gfx_ctx_destroy,
   cocoa_gl_gfx_ctx_get_api,
   cocoa_gl_gfx_ctx_bind_api,
   cocoa_gl_gfx_ctx_swap_interval,
   cocoa_gl_gfx_ctx_set_video_mode,
#if MAC_OS_X_VERSION_10_7 && defined(OSX)
   cocoa_gl_gfx_ctx_get_video_size_osx10_7_and_up,
#else
   cocoa_gl_gfx_ctx_get_video_size,
#endif
   cocoa_gl_gfx_ctx_get_refresh_rate,
   cocoa_gl_gfx_ctx_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   cocoa_get_metrics,
   NULL, /* translate_aspect */
#ifdef OSX
   video_driver_update_title,
#else
   NULL, /* update_title */
#endif
   cocoa_gl_gfx_ctx_check_window,
#if defined(HAVE_COCOA_METAL)
   cocoa_gl_gfx_ctx_set_resize,
#else
   NULL, /* set_resize */
#endif
   cocoa_has_focus,
   cocoa_gl_gfx_ctx_suppress_screensaver,
#if defined(HAVE_COCOATOUCH)
   true,
#else
   true,
#endif
   cocoa_gl_gfx_ctx_swap_buffers,
   cocoa_gl_gfx_ctx_input_driver,
   cocoa_gl_gfx_ctx_get_proc_address,
   NULL, /* image_buffer_init */
   NULL, /* image_buffer_write */
   NULL, /* show_mouse */
   "cocoagl",
   cocoa_gl_gfx_ctx_get_flags,
   cocoa_gl_gfx_ctx_set_flags,
   cocoa_gl_gfx_ctx_bind_hw_render,
   NULL, /* get_context_data */
   NULL, /* make_current */
   NULL, /* create_surface */
   NULL  /* destroy_surface */
};

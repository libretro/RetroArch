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
#if TARGET_OS_OSX
#include <OpenGL/CGLTypes.h>
#include <OpenGL/OpenGL.h>
#include <AppKit/NSScreen.h>
#include <AppKit/NSOpenGL.h>
#elif defined(HAVE_COCOATOUCH)
#include <GLKit/GLKit.h>
#endif

#include <retro_timers.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
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
#if !TARGET_OS_OSX
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

/* Backing-size publication for threaded video.
 * check_window / get_video_size run on the video worker thread when
 * threaded video is active, but -[NSView frame]/-convertRectToBacking:
 * and -[UIView bounds] are main-thread-only.  The main thread publishes
 * the current backing size here (packed as (w << 16) | h) via
 * cocoa_gl_gfx_ctx_publish_size(); the worker thread reads it lock-free.
 * With non-threaded video the publisher and reader are the same (main)
 * thread, so behaviour is unchanged.  16 bits per axis is sufficient for
 * any backing dimension (<= 65535). */
static retro_atomic_size_t cocoa_gl_backing_size;

/* Forward declaration */
CocoaView *cocoaview_get(void);
void cocoa_gl_gfx_ctx_publish_size(void);
/* Defined in ui/drivers/cocoa/cocoa_common.m.  Declared locally (same
 * pattern as the cocoa_gl_gfx_ctx_update() extern in -setFrame:) to
 * avoid touching the CRLF-formatted cocoa_common.h. */
void cocoa_main_thread_sync(void (*func)(void *userdata), void *userdata);

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

#if TARGET_OS_OSX
void cocoa_gl_gfx_ctx_update(void)
{
   [g_ctx    update];
   [g_hw_ctx update];
   /* Runs on the main thread (via -[CocoaView setFrame:]); refresh the
    * published backing size so the worker thread observes the resize. */
   cocoa_gl_gfx_ctx_publish_size();
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


#if TARGET_OS_OSX
static void cocoa_gl_gfx_ctx_destroy_mainthread(void *userdata)
{
   [g_ctx clearDrawable];
   if (g_hw_ctx)
      [g_hw_ctx clearDrawable];
}
#endif

static void cocoa_gl_gfx_ctx_destroy(void *data)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;

   if (!cocoa_ctx)
      return;
#if TARGET_OS_OSX
   /* clearCurrentContext operates on the CALLING thread's current-
    * context slot and must stay here (the render thread); clearDrawable
    * detaches the NSView and is AppKit, so it is marshaled to the main
    * thread. */
   [GLContextClass clearCurrentContext];
   cocoa_main_thread_sync(cocoa_gl_gfx_ctx_destroy_mainthread, NULL);
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

#if TARGET_OS_OSX
/* The view's frame is in points; a Retina backing store has more
 * pixels than points. -convertRectToBacking: is 10.7, so the view is
 * asked once - the answer cannot change while the process runs - and
 * the answer kept, rather than probed per call or decided by the build
 * SDK, which left a binary built on an old SDK blurry on every Retina
 * Mac and one built on a new SDK unable to run anywhere older. */
static void cocoa_gl_gfx_ctx_get_video_size(void *data,
      unsigned* width, unsigned* height)
{
   static int backing              = -1;
   CocoaView *g_view               = cocoaview_get();
   CGRect cgrect                   = NSRectToCGRect([g_view frame]);

   if (backing < 0)
      backing = [g_view respondsToSelector:@selector(convertRectToBacking:)];

   if (backing)
   {
      /* Declared for pre-10.7 SDKs by the category in cocoa_defines.h;
       * only sent where the view answered for it above. */
      NSRect bounds                = NSMakeRect(0, 0,
            CGRectGetWidth(cgrect), CGRectGetHeight(cgrect));
      cgrect                       = NSRectToCGRect(
            [g_view convertRectToBacking:bounds]);
   }

   *width                          = CGRectGetWidth(cgrect);
   *height                         = CGRectGetHeight(cgrect);
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

/* Live backing-size query.  Touches AppKit/UIKit and MUST run on the
 * main thread.  Selects the same implementation the vtable previously
 * exposed directly. */
static void cocoa_gl_live_video_size(unsigned *width, unsigned *height)
{
   cocoa_gl_gfx_ctx_get_video_size(NULL, width, height);
}

/* Publish the current backing size for cross-thread readers.
 * MUST be called on the main thread (resize/layout hooks, or the
 * non-threaded caller path below). */
void cocoa_gl_gfx_ctx_publish_size(void)
{
   unsigned w = 0;
   unsigned h = 0;
   cocoa_gl_live_video_size(&w, &h);
   retro_atomic_store_release_size(&cocoa_gl_backing_size,
         (size_t)(((size_t)(w & 0xFFFF) << 16) | (size_t)(h & 0xFFFF)));
}

/* Thread-safe backing-size getter used by the vtable and check_window.
 * On the main thread it refreshes the published value from AppKit first
 * (preserving exact non-threaded behaviour); on the worker thread it
 * reads the last value published by the main thread, lock-free. */
static void cocoa_gl_gfx_ctx_get_video_size_ts(void *data,
      unsigned *width, unsigned *height)
{
   size_t packed;
   if (sthread_is_main_thread())
      cocoa_gl_gfx_ctx_publish_size();
   packed  = retro_atomic_load_acquire_size(&cocoa_gl_backing_size);
   *width  = (unsigned)((packed >> 16) & 0xFFFF);
   *height = (unsigned)(packed & 0xFFFF);
}

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

#if TARGET_OS_OSX
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

   cocoa_gl_gfx_ctx_get_video_size_ts(data, &new_width, &new_height);

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
#if TARGET_OS_OSX
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
#if TARGET_OS_OSX
   [g_ctx flushBuffer];
   [g_hw_ctx  flushBuffer];
#else
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   if (!(--cocoa_ctx->fast_forward_skips < 0))
      return;
   /* -[GLKView display] presents and resizes the drawable to the view's
    * layer bounds; the resize mutates CALayer state, so this must run on
    * the main thread. The iOS GL/GLES backends are forced non-threaded
    * (video_driver_render_context_is_main_thread_only), so swap_buffers is
    * always on the main thread here and this is safe. */
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

#if TARGET_OS_OSX
static void cocoa_gl_gfx_ctx_init_mainthread(void *userdata)
{
   [apple_platform setViewType:APPLE_VIEW_TYPE_OPENGL];
}

typedef struct
{
   void    *data;
   unsigned width;
   unsigned height;
   bool     fullscreen;
} cocoa_gl_set_video_mode_args_t;

/* Everything in here touches AppKit (context/view attachment, window
 * and fullscreen surgery) and MUST run on the main thread.  With
 * threaded video, cocoa_gl_gfx_ctx_set_video_mode below marshals this
 * over via cocoa_main_thread_sync(); non-threaded callers are already
 * on the main thread and call straight through.  Making the GL context
 * current is deliberately NOT done here: current-context state is
 * per-thread and must be bound on the render (calling) thread. */
static void cocoa_gl_gfx_ctx_set_video_mode_mainthread(void *userdata)
{
   cocoa_gl_set_video_mode_args_t *args = (cocoa_gl_set_video_mode_args_t*)userdata;
   void *data                  = args->data;
   unsigned width              = args->width;
   unsigned height             = args->height;
   bool fullscreen             = args->fullscreen;
   gfx_ctx_mode_t mode;
   NSView *g_view              = [apple_platform renderView];
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)data;
   cocoa_ctx->width            = width;
   cocoa_ctx->height           = height;

   /* Render at the backing store's resolution rather than at point
    * size. 10.7, deprecated in 10.14 and still honoured; asked of the
    * view rather than of the build SDK. */
   if ([g_view respondsToSelector:@selector(setWantsBestResolutionOpenGLSurface:)])
      [g_view setWantsBestResolutionOpenGLSurface:YES];

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

      /* NSOpenGLPFAOpenGLProfile is 10.7 and the 4.1 core profile
       * 10.10, but all three are plain integers in the attribute
       * array - 99, 0x3200, 0x4100 - so the request is spelled out
       * and made unconditionally. A system that does not know the
       * attribute, or cannot give that profile, fails the pixel
       * format; the retry below drops the request and takes the
       * legacy profile, which is what the build-SDK gates used to
       * decide in advance and get wrong in both directions. */
      switch (g_gl_major)
      {
         case 3:
            attributes[6] = (NSOpenGLPixelFormatAttribute)99;
            attributes[7] = (NSOpenGLPixelFormatAttribute)0x3200;
            break;
         case 4:
            attributes[6] = (NSOpenGLPixelFormatAttribute)99;
            attributes[7] = (NSOpenGLPixelFormatAttribute)0x4100;
            break;
      }

      fmt = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];

      /* Two things the system in front of us may refuse. The core
       * profile asked for above is one: a 10.5 or 10.6 system has no
       * NSOpenGLPFAOpenGLProfile at all, and a 10.7 to 10.9 one has
       * no 4.1 core, so the pixel format comes back nil and the
       * request is dropped for a legacy context. The other is
       * NSOpenGLPFAAllowOfflineRenderers, which some early 10.5
       * drivers rejected even though the SDK had the constant.
       * Dropped in that order, since a caller that asked for GL 3 or
       * 4 would rather lose offline renderers than the profile. */
      if (fmt == nil && attributes[6])
      {
         attributes[6]  = (NSOpenGLPixelFormatAttribute)0;
         attributes[7]  = (NSOpenGLPixelFormatAttribute)0;
         fmt            = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      }

      if (fmt == nil)
      {
         attributes[3]  = (NSOpenGLPixelFormatAttribute)0;
         fmt            = [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
      }

      /* A context must never be released while still attached to the
       * view (or current on the render thread).  -destroy guarantees
       * that on the normal reinit path; should -set_video_mode ever be
       * reached with g_ctx/g_hw_ctx still live, detach them before
       * RELEASE, exactly as -destroy_mainthread does.  The caller has
       * already cleared the render thread's current context.  Under
       * ARC RELEASE() is an assignment of nil to a strong static, which
       * still releases, so this applies to both memory models. */
      [g_ctx clearDrawable];
      if (g_hw_ctx)
         [g_hw_ctx clearDrawable];
      RELEASE(g_ctx);
      RELEASE(g_hw_ctx);

      if (cocoa_ctx->flags & COCOA_CTX_FLAG_USE_HW_CTX)
      {
         g_hw_ctx       = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil];
         g_ctx          = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:g_hw_ctx];
      }
      else
         g_ctx          = [[NSOpenGLContext alloc] initWithFormat:fmt shareContext:nil];

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

   /* Window and full-screen surgery lives with the application
    * delegate, which knows whether the system has native full-screen
    * or needs the borderless-window mode. */
   mode.width           = width;
   mode.height          = height;
   mode.fullscreen      = fullscreen;
   [apple_platform setVideoMode:mode];
   cocoa_show_mouse(data, !fullscreen);

   /* Seed/refresh the published backing size while still on the main
    * thread, so a threaded-video worker never observes the initial 0x0
    * before the first resize/layout event fires. */
   cocoa_gl_gfx_ctx_publish_size();
}

static bool cocoa_gl_gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height, bool fullscreen)
{
   cocoa_gl_set_video_mode_args_t args;

   args.data       = data;
   args.width      = width;
   args.height     = height;
   args.fullscreen = fullscreen;

   /* Current-context state is per-thread, so this has to happen here
    * on the render thread and not inside the main-thread body: with
    * threaded video, +currentContext on the main thread would never
    * report g_ctx, and a previous context still live here would be
    * released while current.  Harmless when nothing is current; g_ctx
    * is re-bound below. */
   [GLContextClass clearCurrentContext];

   cocoa_main_thread_sync(cocoa_gl_gfx_ctx_set_video_mode_mainthread, &args);

   /* Bind the context on the calling (render) thread: with threaded
    * video that is the worker thread that will issue all GL commands. */
   [g_ctx makeCurrentContext];

   return true;
}

static void *cocoa_gl_gfx_ctx_init(void *video_driver)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)
      calloc(1, sizeof(cocoa_ctx_data_t));

   if (!cocoa_ctx)
      return NULL;

#if !TARGET_OS_OSX
   cocoa_ctx->flags |= COCOA_CTX_FLAG_IS_SYNCING;
#endif

   /* setViewType creates/attaches the render view (AppKit); marshal to
    * the main thread when the underlying driver init runs on the video
    * worker thread. */
   cocoa_main_thread_sync(cocoa_gl_gfx_ctx_init_mainthread, NULL);

   return cocoa_ctx;
}
#else
static void cocoa_gl_gfx_ctx_init_es_mainthread(void *userdata)
{
   [apple_platform setViewType:APPLE_VIEW_TYPE_OPENGL_ES];
}

/* EAGLContext creation and the GLKView association are UIKit-adjacent
 * and are kept on the main thread; binding the context current happens
 * on the calling (render) thread in the wrapper below. */
static void cocoa_gl_gfx_ctx_set_video_mode_mainthread(void *userdata)
{
   cocoa_ctx_data_t *cocoa_ctx = (cocoa_ctx_data_t*)userdata;

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

   glk_view.context = g_ctx;

   /* Seed/refresh the published backing size while still on the main
    * thread, so a threaded-video worker never observes the initial 0x0
    * before the first layout event fires. */
   cocoa_gl_gfx_ctx_publish_size();
}

static bool cocoa_gl_gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height, bool fullscreen)
{
   cocoa_main_thread_sync(cocoa_gl_gfx_ctx_set_video_mode_mainthread, data);

   /* Bind the context on the calling (render) thread: with threaded
    * video that is the worker thread that will issue all GL commands. */
   [EAGLContext setCurrentContext:g_ctx];

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

#if !TARGET_OS_OSX
   cocoa_ctx->flags |= COCOA_CTX_FLAG_IS_SYNCING;
#endif

   switch (cocoagl_api)
   {
      case GFX_CTX_OPENGL_ES_API:
         /* setViewType creates/attaches the render view (UIKit);
          * marshal to the main thread when the underlying driver init
          * runs on the video worker thread. */
         cocoa_main_thread_sync(cocoa_gl_gfx_ctx_init_es_mainthread, NULL);
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return cocoa_ctx;
}
#endif

static bool cocoa_gl_gfx_ctx_set_resize(void *data, unsigned width, unsigned height)
{
   return true;
}

static void cocoa_gl_gfx_ctx_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   /* Body consolidated into cocoa_common.m.  Kept as a named
    * vtable entry because video_thread_wrapper.c's
    * thread_get_video_output_size calls the poke / ctx hook
    * directly, bypassing dispserv_apple. */
   cocoa_get_video_output_size(width, height, desc, desc_len);
}

/* A miniaturised window has nothing behind it to present to:
 * -flushBuffer returns at once rather than blocking to the display's
 * refresh, so with vsync as the only pacing the loop would spin.
 * AppKit keeps the state, so there is none of ours to keep, and it is
 * asked of the window rather than tracked through notifications.
 *
 * macOS only: on iOS and tvOS the system stops the CADisplayLink when
 * the app leaves the foreground, so there is no loop to pace, and
 * UIWindow has no equivalent of -isMiniaturized. */
static bool cocoa_gl_gfx_ctx_presentable(void *data)
{
#if TARGET_OS_OSX
   CocoaView *g_view = cocoaview_get();
   (void)data;
   if (g_view)
      return ![[g_view window] isMiniaturized];
#else
   (void)data;
#endif
   return true;
}

const gfx_ctx_driver_t gfx_ctx_cocoagl = {
   cocoa_gl_gfx_ctx_init,
   cocoa_gl_gfx_ctx_destroy,
   cocoa_gl_gfx_ctx_get_api,
   cocoa_gl_gfx_ctx_bind_api,
   cocoa_gl_gfx_ctx_swap_interval,
   cocoa_gl_gfx_ctx_set_video_mode,
   cocoa_gl_gfx_ctx_get_video_size_ts,
   cocoa_gl_gfx_ctx_get_refresh_rate,
   cocoa_gl_gfx_ctx_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   cocoa_get_metrics,
   NULL, /* translate_aspect */
#if TARGET_OS_OSX
   video_driver_update_title,
#else
   NULL, /* update_title */
#endif
   cocoa_gl_gfx_ctx_check_window,
   cocoa_gl_gfx_ctx_set_resize,
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
   NULL, /* destroy_surface */
   cocoa_gl_gfx_ctx_presentable
};

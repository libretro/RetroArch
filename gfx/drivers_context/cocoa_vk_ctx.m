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
#include <AppKit/NSScreen.h>
#endif

#include <retro_timers.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>
#include <compat/apple_compat.h>
#include <string/stdstring.h>

#include "../common/vulkan_common.h"

#include "../../ui/drivers/ui_cocoa.h"
#include "../../ui/drivers/cocoa/cocoa_common.h"
#include "../../ui/drivers/cocoa/apple_platform.h"
#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

typedef struct cocoa_vk_ctx_data
{
   gfx_ctx_vulkan_data_t vk;
   int swap_interval;
   unsigned width;
   unsigned height;
} cocoa_vk_ctx_data_t;

/* TODO/FIXME - static globals */
static unsigned g_vk_minor          = 0;
static unsigned g_vk_major          = 0;
/* Forward declaration */
CocoaView *cocoaview_get(void);

/* Backing-size publication for threaded video.  Same rationale as the
 * GL context driver (see cocoa_gl_ctx.m): check_window / get_video_size
 * run on the video worker thread, but the AppKit/UIKit geometry queries
 * they perform are main-thread-only.  The main thread publishes the
 * packed backing size ((w << 16) | h); the worker reads it lock-free. */
static retro_atomic_size_t cocoa_vk_backing_size;
void cocoa_vk_gfx_ctx_publish_size(void);
/* Defined in ui/drivers/cocoa/cocoa_common.m.  Declared locally (same
 * pattern as in cocoa_gl_ctx.m) to avoid touching the CRLF-formatted
 * cocoa_common.h. */
void cocoa_main_thread_sync(void (*func)(void *userdata), void *userdata);

static uint32_t cocoa_vk_gfx_ctx_get_flags(void *data)
{
   uint32_t flags = 0;
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   return flags;
}

static void cocoa_vk_gfx_ctx_set_flags(void *data, uint32_t flags) { }

/* Runs the Vulkan context teardown on the main thread.  MoltenVK
 * internally marshals some CAMetalLayer work to the GCD main queue via
 * dispatch_sync when called off the main thread; with threaded video
 * the worker calling into MoltenVK while the main thread is blocked in
 * the thread wrapper's command wait would deadlock, because that wait
 * only pumps the private trampoline runloop mode, which does NOT drain
 * the GCD main queue.  Running on the main thread short-circuits
 * MoltenVK's internal dispatch (it checks for the main thread and
 * calls straight through). */
static void cocoa_vk_gfx_ctx_destroy_mainthread(void *userdata)
{
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)userdata;

   vulkan_context_destroy(&cocoa_ctx->vk, cocoa_ctx->vk.vk_surface != VK_NULL_HANDLE);
   if (cocoa_ctx->vk.context.queue_lock)
      slock_free(cocoa_ctx->vk.context.queue_lock);
   memset(&cocoa_ctx->vk, 0, sizeof(cocoa_ctx->vk));
}

static void cocoa_vk_gfx_ctx_destroy(void *data)
{
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)data;

   if (!cocoa_ctx)
      return;

   cocoa_main_thread_sync(cocoa_vk_gfx_ctx_destroy_mainthread, cocoa_ctx);

   free(cocoa_ctx);
}

static enum gfx_ctx_api cocoa_vk_gfx_ctx_get_api(void *data) { return GFX_CTX_VULKAN_API; }

static bool cocoa_vk_gfx_ctx_suppress_screensaver(void *data, bool disable)
{
    return [apple_platform setDisableDisplaySleep:disable];
}

static void cocoa_vk_gfx_ctx_input_driver(void *data,
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
static void cocoa_vk_gfx_ctx_get_video_size_osx10_7_and_up(void *data,
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
static void cocoa_vk_gfx_ctx_get_video_size(void *data,
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
static void cocoa_vk_gfx_ctx_get_video_size(void *data,
      unsigned* width, unsigned* height)
{
    UIView *renderView              = apple_platform.renderView;
    CGRect size                     = [renderView bounds];
    float viewScale                 = [renderView contentScaleFactor];
    *width                          = CGRectGetWidth(size)  * viewScale;
    *height                         = CGRectGetHeight(size) * viewScale;
}
#endif

/* Live backing-size query.  Touches AppKit/UIKit and MUST run on the
 * main thread.  Selects the same implementation the vtable previously
 * exposed directly. */
static void cocoa_vk_live_video_size(unsigned *width, unsigned *height)
{
#if MAC_OS_X_VERSION_10_7 && defined(OSX)
   cocoa_vk_gfx_ctx_get_video_size_osx10_7_and_up(NULL, width, height);
#else
   cocoa_vk_gfx_ctx_get_video_size(NULL, width, height);
#endif
}

/* Publish the current backing size for cross-thread readers.
 * MUST be called on the main thread. */
void cocoa_vk_gfx_ctx_publish_size(void)
{
   unsigned w = 0;
   unsigned h = 0;
   cocoa_vk_live_video_size(&w, &h);
   retro_atomic_store_release_size(&cocoa_vk_backing_size,
         (size_t)(((size_t)(w & 0xFFFF) << 16) | (size_t)(h & 0xFFFF)));
}

/* Thread-safe backing-size getter used by the vtable and check_window.
 * On the main thread it refreshes the published value from AppKit first
 * (preserving exact non-threaded behaviour); on the worker thread it
 * reads the last value published by the main thread, lock-free. */
static void cocoa_vk_gfx_ctx_get_video_size_ts(void *data,
      unsigned *width, unsigned *height)
{
   size_t packed;
   if (sthread_is_main_thread())
      cocoa_vk_gfx_ctx_publish_size();
   packed  = retro_atomic_load_acquire_size(&cocoa_vk_backing_size);
   *width  = (unsigned)((packed >> 16) & 0xFFFF);
   *height = (unsigned)(packed & 0xFFFF);
}

static float cocoa_vk_gfx_ctx_get_refresh_rate(void *data)
{
   /* Body consolidated into cocoa_common.m.  Kept as a named
    * vtable entry because vulkan_get_refresh_rate() reaches the
    * ctx driver directly via video_context_driver_get_refresh_rate,
    * bypassing dispserv_apple's own hook. */
   return cocoa_get_refresh_rate();
}

static gfx_ctx_proc_t cocoa_vk_gfx_ctx_get_proc_address(const char *symbol_name)
{
   return NULL;
}

static void cocoa_vk_gfx_ctx_bind_hw_render(void *data, bool enable) { }

static void cocoa_vk_gfx_ctx_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   unsigned new_width, new_height;
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)data;
   *quit                          = false;
   *resize                        = (cocoa_ctx->vk.flags &
         VK_DATA_FLAG_NEED_NEW_SWAPCHAIN) ? true : false;

   cocoa_vk_gfx_ctx_get_video_size_ts(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
}

static void cocoa_vk_gfx_ctx_swap_interval(void *data, int i)
{
   unsigned interval              = (unsigned)i;
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)data;

   if (cocoa_ctx->swap_interval != (int)interval)
   {
      cocoa_ctx->swap_interval    = interval;
      if (cocoa_ctx->vk.swapchain)
         cocoa_ctx->vk.flags     |= VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   }
}

static void cocoa_vk_gfx_ctx_swap_buffers(void *data)
{
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)data;

   if (cocoa_ctx->vk.context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
   {
      cocoa_ctx->vk.context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
      if (cocoa_ctx->vk.swapchain == VK_NULL_HANDLE)
      {
         retro_sleep(10);
      }
      else
         vulkan_present(&cocoa_ctx->vk, cocoa_ctx->vk.context.current_swapchain_index);
   }
   vulkan_acquire_next_image(&cocoa_ctx->vk);
}

static bool cocoa_vk_gfx_ctx_bind_api(void *data, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
   g_vk_minor  = minor;
   g_vk_major  = major;

   return true;
}

static void *cocoa_vk_gfx_ctx_get_context_data(void *data)
{
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)data;
   return &cocoa_ctx->vk.context;
}

#ifdef OSX
typedef struct
{
   void    *data;
   unsigned width;
   unsigned height;
   bool     fullscreen;
   bool     ok;
} cocoa_vk_set_video_mode_args_t;

/* Whole body on the main thread: g_view.layer is AppKit,
 * [apple_platform setVideoMode:] performs window surgery, and
 * vulkan_surface_create reaches MoltenVK, whose internal
 * dispatch_sync-to-main short-circuits only when already on the main
 * thread (see cocoa_vk_gfx_ctx_destroy_mainthread above). */
static void cocoa_vk_gfx_ctx_set_video_mode_mainthread(void *userdata)
{
   cocoa_vk_set_video_mode_args_t *args = (cocoa_vk_set_video_mode_args_t*)userdata;
   gfx_ctx_mode_t mode;
#if defined(HAVE_COCOA_METAL)
   NSView *g_view                 = apple_platform.renderView;
#elif defined(HAVE_COCOA)
   CocoaView *g_view              = (CocoaView*)nsview_get_ptr();
#endif
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)args->data;
   cocoa_ctx->width               = args->width;
   cocoa_ctx->height              = args->height;

   RARCH_LOG("[Vulkan] Native window size: %ux%u.\n",
         cocoa_ctx->width, cocoa_ctx->height);

   if (!vulkan_surface_create(
            &cocoa_ctx->vk,
            VULKAN_WSI_MVK_MACOS,
            NULL,
            (BRIDGE void *)g_view.layer,
            cocoa_ctx->width,
            cocoa_ctx->height,
            cocoa_ctx->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to create surface.\n");
      args->ok                    = false;
      return;
   }

   mode.width                     = args->width;
   mode.height                    = args->height;
   mode.fullscreen                = args->fullscreen;
   [apple_platform setVideoMode:mode];
   cocoa_show_mouse(args->data, !args->fullscreen);

   /* Seed/refresh the published backing size while still on the main
    * thread, so a threaded-video worker never observes the initial 0x0
    * before the first resize/layout event fires. */
   cocoa_vk_gfx_ctx_publish_size();

   args->ok                       = true;
}

static bool cocoa_vk_gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height, bool fullscreen)
{
   cocoa_vk_set_video_mode_args_t args;

   args.data       = data;
   args.width      = width;
   args.height     = height;
   args.fullscreen = fullscreen;
   args.ok         = false;

   cocoa_main_thread_sync(cocoa_vk_gfx_ctx_set_video_mode_mainthread, &args);

   return args.ok;
}

typedef struct
{
   cocoa_vk_ctx_data_t *ctx;
   bool ok;
} cocoa_vk_init_args_t;

/* setViewType creates/attaches the render view (AppKit) and
 * vulkan_context_init reaches MoltenVK; both belong on the main
 * thread (see above). */
static void cocoa_vk_gfx_ctx_init_mainthread(void *userdata)
{
   cocoa_vk_init_args_t *args = (cocoa_vk_init_args_t*)userdata;

   [apple_platform setViewType:APPLE_VIEW_TYPE_VULKAN];
   args->ok = vulkan_context_init(&args->ctx->vk, VULKAN_WSI_MVK_MACOS);
}

static void *cocoa_vk_gfx_ctx_init(void *video_driver)
{
   cocoa_vk_init_args_t args;
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)
   calloc(1, sizeof(cocoa_vk_ctx_data_t));

   if (!cocoa_ctx)
      return NULL;

   args.ctx = cocoa_ctx;
   args.ok  = false;
   cocoa_main_thread_sync(cocoa_vk_gfx_ctx_init_mainthread, &args);
   if (!args.ok)
   {
      free(cocoa_ctx);
      return NULL;
   }

   return cocoa_ctx;
}
#else
typedef struct
{
   void    *data;
   unsigned width;
   unsigned height;
   bool     ok;
} cocoa_vk_set_video_mode_args_t;

/* Whole body on the main thread: the render view / metalLayer access
 * is UIKit and vulkan_surface_create reaches MoltenVK (see the
 * dispatch_sync rationale above the destroy helper). */
static void cocoa_vk_gfx_ctx_set_video_mode_mainthread(void *userdata)
{
   cocoa_vk_set_video_mode_args_t *args = (cocoa_vk_set_video_mode_args_t*)userdata;
   id g_view                      = apple_platform.renderView;
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)args->data;
   cocoa_ctx->width               = args->width;
   cocoa_ctx->height              = args->height;

   if (!vulkan_surface_create(&cocoa_ctx->vk,
                              VULKAN_WSI_MVK_IOS,
                              NULL,
                              (BRIDGE void *)((MetalLayerView*)g_view).metalLayer,
                              cocoa_ctx->width,
                              cocoa_ctx->height,
                              cocoa_ctx->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to create surface.\n");
      args->ok                    = false;
      return;
   }

   /* Seed/refresh the published backing size while still on the main
    * thread, so a threaded-video worker never observes the initial 0x0
    * before the first layout event fires. */
   cocoa_vk_gfx_ctx_publish_size();

   args->ok                       = true;
}

static bool cocoa_vk_gfx_ctx_set_video_mode(void *data,
      unsigned width, unsigned height, bool fullscreen)
{
   cocoa_vk_set_video_mode_args_t args;

   args.data   = data;
   args.width  = width;
   args.height = height;
   args.ok     = false;

   cocoa_main_thread_sync(cocoa_vk_gfx_ctx_set_video_mode_mainthread, &args);

   /* TODO: Maybe iOS users should be able to
    * show/hide the status bar here? */
   return args.ok;
}

typedef struct
{
   cocoa_vk_ctx_data_t *ctx;
   bool ok;
} cocoa_vk_init_args_t;

static void cocoa_vk_gfx_ctx_init_mainthread(void *userdata)
{
   cocoa_vk_init_args_t *args = (cocoa_vk_init_args_t*)userdata;

   [apple_platform setViewType:APPLE_VIEW_TYPE_VULKAN];
   args->ok = vulkan_context_init(&args->ctx->vk, VULKAN_WSI_MVK_IOS);
}

static void *cocoa_vk_gfx_ctx_init(void *video_driver)
{
   cocoa_vk_init_args_t args;
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)
   calloc(1, sizeof(cocoa_vk_ctx_data_t));

   if (!cocoa_ctx)
      return NULL;

   args.ctx = cocoa_ctx;
   args.ok  = false;
   cocoa_main_thread_sync(cocoa_vk_gfx_ctx_init_mainthread, &args);
   if (!args.ok)
   {
      free(cocoa_ctx);
      return NULL;
   }

   return cocoa_ctx;
}
#endif

#ifdef HAVE_COCOA_METAL
typedef struct
{
   cocoa_vk_ctx_data_t *ctx;
   unsigned width;
   unsigned height;
   bool     ok;
} cocoa_vk_set_resize_args_t;

/* Swapchain recreation reaches MoltenVK (CAMetalLayer sizing); run it
 * on the main thread for the same dispatch_sync reason as init /
 * set_video_mode / destroy.  With threaded video this is reached from
 * the worker's frame processing; the thread wrapper's main-thread
 * waits pump the trampoline mode so this can drain even while the
 * user side is blocked. */
static void cocoa_vk_gfx_ctx_set_resize_mainthread(void *userdata)
{
   cocoa_vk_set_resize_args_t *args = (cocoa_vk_set_resize_args_t*)userdata;
   cocoa_vk_ctx_data_t *cocoa_ctx   = args->ctx;

   if (!vulkan_create_swapchain(&cocoa_ctx->vk,
            args->width, args->height, cocoa_ctx->swap_interval))
   {
      RARCH_ERR("[Vulkan] Failed to update swapchain.\n");
      args->ok                    = false;
      return;
   }

   cocoa_ctx->vk.context.flags   |= VK_CTX_FLAG_INVALID_SWAPCHAIN;
   if (cocoa_ctx->vk.flags & VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN)
      vulkan_acquire_next_image(&cocoa_ctx->vk);
   cocoa_ctx->vk.flags           &= ~VK_DATA_FLAG_NEED_NEW_SWAPCHAIN;
   args->ok                       = true;
}

static bool cocoa_vk_gfx_ctx_set_resize(void *data, unsigned width, unsigned height)
{
   cocoa_vk_set_resize_args_t args;
   cocoa_vk_ctx_data_t *cocoa_ctx = (cocoa_vk_ctx_data_t*)data;

   cocoa_ctx->width               = width;
   cocoa_ctx->height              = height;

   args.ctx    = cocoa_ctx;
   args.width  = width;
   args.height = height;
   args.ok     = false;

   cocoa_main_thread_sync(cocoa_vk_gfx_ctx_set_resize_mainthread, &args);

   return args.ok;
}
#endif

static void cocoa_vk_gfx_ctx_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   /* Body consolidated into cocoa_common.m.  Kept as a named
    * vtable entry because video_thread_wrapper.c's
    * thread_get_video_output_size calls the poke / ctx hook
    * directly, bypassing dispserv_apple. */
   cocoa_get_video_output_size(width, height, desc, desc_len);
}

const gfx_ctx_driver_t gfx_ctx_cocoavk = {
   cocoa_vk_gfx_ctx_init,
   cocoa_vk_gfx_ctx_destroy,
   cocoa_vk_gfx_ctx_get_api,
   cocoa_vk_gfx_ctx_bind_api,
   cocoa_vk_gfx_ctx_swap_interval,
   cocoa_vk_gfx_ctx_set_video_mode,
   cocoa_vk_gfx_ctx_get_video_size_ts,
   cocoa_vk_gfx_ctx_get_refresh_rate,
   cocoa_vk_gfx_ctx_get_video_output_size,
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   cocoa_get_metrics,
   NULL, /* translate_aspect */
#ifdef OSX
   video_driver_update_title,
#else
   NULL, /* update_title */
#endif
   cocoa_vk_gfx_ctx_check_window,
#if defined(HAVE_COCOA_METAL)
   cocoa_vk_gfx_ctx_set_resize,
#else
   NULL, /* set_resize */
#endif
   cocoa_has_focus,
   cocoa_vk_gfx_ctx_suppress_screensaver,
#if defined(HAVE_COCOATOUCH)
   true,
#else
   true,
#endif
   cocoa_vk_gfx_ctx_swap_buffers,
   cocoa_vk_gfx_ctx_input_driver,
   cocoa_vk_gfx_ctx_get_proc_address,
   NULL, /* image_buffer_init */
   NULL, /* image_buffer_write */
   NULL, /* show_mouse */
   "cocoavk",
   cocoa_vk_gfx_ctx_get_flags,
   cocoa_vk_gfx_ctx_set_flags,
   cocoa_vk_gfx_ctx_bind_hw_render,
   cocoa_vk_gfx_ctx_get_context_data,
   NULL, /* make_current */
   NULL, /* create_surface */
   NULL  /* destroy_surface */
};

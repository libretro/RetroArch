/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <stdint.h>
#include <stdlib.h>
#include <signal.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <string/stdstring.h>
#include <compat/strcasestr.h>
#include <retro_timers.h>
#include <X11/Xatom.h>

#include "../../configuration.h"
#include "../../frontend/frontend_driver.h"
#include "../../input/input_driver.h"
#include "../../verbosity.h"
#include "../common/x11_common.h"

#ifdef HAVE_XINERAMA
#include "../common/xinerama_common.h"
#endif

#include "../common/vulkan_common.h"

typedef struct gfx_ctx_x_vk_data
{
   bool should_reset_mode;
   bool is_fullscreen;

   int interval;

   gfx_ctx_vulkan_data_t vk;
} gfx_ctx_x_vk_data_t;

typedef struct Hints
{
   unsigned long flags;
   unsigned long functions;
   unsigned long decorations;
   long          inputMode;
   unsigned long status;
} Hints;

/* We use long because X11 wants 32-bit pixels for 32-bit systems and 64 for 64... */
/* ARGB*/
static const unsigned long retroarch_icon_vk_data[] = {
   16, 16,
0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,
0x00000000,0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xfff2f2f2,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xfff2f2f2,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,
0x00000000,0x00000000,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0xff333333,0x00000000,0x00000000,0x00000000,
0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000
};

static int x_vk_nul_handler(Display *dpy, XErrorEvent *event) { return 0; }

static void gfx_ctx_x_vk_destroy_resources(gfx_ctx_x_vk_data_t *x)
{
   x11_input_ctx_destroy();

   if (g_x11_dpy)
   {
      vulkan_context_destroy(&x->vk, g_x11_win != 0);
   }

   if (g_x11_win && g_x11_dpy)
   {
#ifdef HAVE_XINERAMA
      /* Save last used monitor for later. */
      xinerama_save_last_used_monitor(DefaultRootWindow(g_x11_dpy));
#endif
      x11_window_destroy(false);
   }

   x11_colormap_destroy();

   if (g_x11_dpy)
   {
      if (x->should_reset_mode)
      {
         x11_exit_fullscreen(g_x11_dpy);
         x->should_reset_mode = false;
      }
   }
}

static void gfx_ctx_x_vk_destroy(void *data)
{
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)data;
   if (!x)
      return;

   gfx_ctx_x_vk_destroy_resources(x);

#if defined(HAVE_THREADS)
   if (x->vk.context.queue_lock)
      slock_free(x->vk.context.queue_lock);
#endif

   free(data);
}

static void gfx_ctx_x_vk_swap_interval(void *data, int interval)
{
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)data;

   if (x->interval != interval)
   {
      x->interval = interval;
      if (x->vk.swapchain)
         x->vk.need_new_swapchain = true;
   }
}

static void gfx_ctx_x_vk_swap_buffers(void *data)
{
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)data;

   if (x->vk.context.has_acquired_swapchain)
   {
      x->vk.context.has_acquired_swapchain = false;
      if (x->vk.swapchain == VK_NULL_HANDLE)
      {
         retro_sleep(10);
      }
      else
         vulkan_present(&x->vk, x->vk.context.current_swapchain_index);
   }
   vulkan_acquire_next_image(&x->vk);
}

static void gfx_ctx_x_vk_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)data;
   x11_check_window(data, quit, resize, width, height);

   if (x->vk.need_new_swapchain)
      *resize = true;
}

static bool gfx_ctx_x_vk_set_resize(void *data,
      unsigned width, unsigned height)
{
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)data;

   if (!x)
      return false;

   /*
    * X11 loses focus on monitor/resolution swap and exits fullscreen.
    * Set window on top again to maintain both fullscreen and resolution.
    */
   if (x->is_fullscreen) {
      XMapRaised(g_x11_dpy, g_x11_win);
      RARCH_LOG("[X/Vulkan]: Resized fullscreen resolution to %dx%d.\n", width, height);
   }

   /* FIXME/TODO - threading error here */

   if (!vulkan_create_swapchain(&x->vk, width, height, x->interval))
   {
      RARCH_ERR("[X/Vulkan]: Failed to update swapchain.\n");
      x->vk.swapchain = VK_NULL_HANDLE;
      return false;
   }

   if (x->vk.created_new_swapchain)
      vulkan_acquire_next_image(&x->vk);
   x->vk.context.invalid_swapchain = true;
   x->vk.need_new_swapchain        = false;
   return true;
}

static void *gfx_ctx_x_vk_init(void *data)
{
   int nelements           = 0;
   int major               = 0;
   int minor               = 0;
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)
      calloc(1, sizeof(gfx_ctx_x_vk_data_t));

   if (!x)
      return NULL;

   XInitThreads();

   if (!x11_connect())
      goto error;

   /* Use XCB WSI since it's the most supported WSI over legacy Xlib. */
   if (!vulkan_context_init(&x->vk, VULKAN_WSI_XCB))
      goto error;

   return x;

error:
   if (x)
   {
      gfx_ctx_x_vk_destroy_resources(x);
      free(x);
   }
   g_x11_screen = 0;

   return NULL;
}

static bool gfx_ctx_x_vk_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   XEvent event;
   bool true_full            = false;
   int val                   = 0;
   int x_off                 = 0;
   int y_off                 = 0;
   XVisualInfo *vi           = NULL;
   XSetWindowAttributes swa  = {0};
   char *wm_name             = NULL;
   int (*old_handler)(Display*, XErrorEvent*) = NULL;
   gfx_ctx_x_vk_data_t *x    = (gfx_ctx_x_vk_data_t*)data;
   Atom net_wm_icon          = XInternAtom(g_x11_dpy, "_NET_WM_ICON", False);
   Atom cardinal             = XInternAtom(g_x11_dpy, "CARDINAL", False);
   settings_t *settings      = config_get_ptr();
   unsigned opacity          = settings->uints.video_window_opacity 
      * ((unsigned)-1 / 100.0);
   bool disable_composition  = settings->bools.video_disable_composition;
   bool show_decorations     = settings->bools.video_window_show_decorations;
   bool windowed_full        = settings->bools.video_windowed_fullscreen;
   unsigned video_monitor_index = settings->uints.video_monitor_index;

   frontend_driver_install_signal_handler();

   if (!x)
      return false;

   {
      XVisualInfo vi_template;
      /* For default case, just try to obtain a visual from template. */
      int nvisuals = 0;

      memset(&vi_template, 0, sizeof(vi_template));
      vi_template.screen = DefaultScreen(g_x11_dpy);
      vi = XGetVisualInfo(g_x11_dpy, VisualScreenMask, &vi_template, &nvisuals);
      if (!vi || nvisuals < 1)
         goto error;
   }

   swa.colormap = g_x11_cmap = XCreateColormap(g_x11_dpy,
         RootWindow(g_x11_dpy, vi->screen), vi->visual, AllocNone);
   swa.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask |
      LeaveWindowMask | EnterWindowMask |
      ButtonReleaseMask | ButtonPressMask;
   swa.override_redirect = False;

   x->is_fullscreen = fullscreen;

   if (fullscreen && !windowed_full)
   {
      if (x11_enter_fullscreen(g_x11_dpy, width, height))
      {
         x->should_reset_mode = true;
         true_full = true;
      }
      else
         RARCH_ERR("[X/Vulkan]: Entering true fullscreen failed. Will attempt windowed mode.\n");
   }

   wm_name = x11_get_wm_name(g_x11_dpy);
   if (wm_name)
   {
      RARCH_LOG("[X/Vulkan]: Window manager is %s.\n", wm_name);

      if (true_full && strcasestr(wm_name, "xfwm"))
      {
         RARCH_LOG("[X/Vulkan]: Using override-redirect workaround.\n");
         swa.override_redirect = True;
      }
      free(wm_name);
   }
   if (!x11_has_net_wm_fullscreen(g_x11_dpy) && true_full)
      swa.override_redirect = True;

   if (video_monitor_index)
      g_x11_screen = video_monitor_index - 1;

#ifdef HAVE_XINERAMA
   if (fullscreen || g_x11_screen != 0)
   {
      unsigned new_width  = width;
      unsigned new_height = height;

      if (xinerama_get_coord(g_x11_dpy, g_x11_screen,
               &x_off, &y_off, &new_width, &new_height))
         RARCH_LOG("[X/Vulkan]: Using Xinerama on screen #%u.\n", g_x11_screen);
      else
         RARCH_LOG("[X/Vulkan]: Xinerama is not active on screen.\n");

      if (fullscreen)
      {
         width  = new_width;
         height = new_height;
      }
   }
#endif

   RARCH_LOG("[X/Vulkan]: X = %d, Y = %d, W = %u, H = %u.\n",
         x_off, y_off, width, height);

   g_x11_win = XCreateWindow(g_x11_dpy, RootWindow(g_x11_dpy, vi->screen),
         x_off, y_off, width, height, 0,
         vi->depth, InputOutput, vi->visual,
         CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect,
         &swa);
   XSetWindowBackground(g_x11_dpy, g_x11_win, 0);

   XChangeProperty(g_x11_dpy, g_x11_win, net_wm_icon, cardinal, 32, PropModeReplace, (const unsigned char*)retroarch_icon_vk_data, sizeof(retroarch_icon_vk_data) / sizeof(*retroarch_icon_vk_data));

   if (fullscreen && disable_composition)
   {
      uint32_t                value = 1;
      Atom net_wm_bypass_compositor = XInternAtom(g_x11_dpy, "_NET_WM_BYPASS_COMPOSITOR", False);

      RARCH_LOG("[X/Vulkan]: Requesting compositor bypass.\n");
      XChangeProperty(g_x11_dpy, g_x11_win, net_wm_bypass_compositor, cardinal, 32, PropModeReplace, (const unsigned char*)&value, 1);
   }

   if (opacity < (unsigned)-1)
   {
      Atom net_wm_opacity = XInternAtom(g_x11_dpy, "_NET_WM_WINDOW_OPACITY", False);
      XChangeProperty(g_x11_dpy, g_x11_win, net_wm_opacity, cardinal, 32, PropModeReplace, (const unsigned char*)&opacity, 1);
   }

   if (!show_decorations)
   {
      /* We could have just set _NET_WM_WINDOW_TYPE_DOCK instead, 
       * but that removes the window from any taskbar/panel,
       * so we are forced to use the old motif hints method. */
      Hints hints;
      Atom property     = XInternAtom(g_x11_dpy, "_MOTIF_WM_HINTS", False);

      hints.flags       = 2;
      hints.decorations = 0;

      XChangeProperty(g_x11_dpy, g_x11_win, property, property, 32, PropModeReplace, (const unsigned char*)&hints, 5);
   }

   x11_set_window_attr(g_x11_dpy, g_x11_win);
   x11_update_title(NULL);

   if (fullscreen)
      x11_show_mouse(g_x11_dpy, g_x11_win, false);

   if (true_full)
   {
      RARCH_LOG("[X/Vulkan]: Using true fullscreen.\n");
      XMapRaised(g_x11_dpy, g_x11_win);
      x11_set_net_wm_fullscreen(g_x11_dpy, g_x11_win);
   }
   else if (fullscreen)
   {
      /* We attempted true fullscreen, but failed.
       * Attempt using windowed fullscreen. */

      XMapRaised(g_x11_dpy, g_x11_win);
      RARCH_LOG("[X/Vulkan]: Using windowed fullscreen.\n");

      /* We have to move the window to the screen we want
       * to go fullscreen on first.
       * x_off and y_off usually get ignored in XCreateWindow().
       */
      x11_move_window(g_x11_dpy, g_x11_win, x_off, y_off, width, height);
      x11_set_net_wm_fullscreen(g_x11_dpy, g_x11_win);
   }
   else
   {
      XMapWindow(g_x11_dpy, g_x11_win);
      /* If we want to map the window on a different screen,
       * we'll have to do it by force.
       * Otherwise, we should try to let the window manager sort it out.
       * x_off and y_off usually get ignored in XCreateWindow(). */
      if (g_x11_screen)
         x11_move_window(g_x11_dpy, g_x11_win, x_off, y_off, width, height);
   }

   x11_event_queue_check(&event);

   {
      bool quit, resize;
      unsigned width = 0, height = 0;
      x11_check_window(x, &quit, &resize, &width, &height);

      /* FIXME/TODO - threading error here */

      /* Use XCB surface since it's the most supported WSI.
       * We can obtain the XCB connection directly from X11. */
      if (!vulkan_surface_create(&x->vk, VULKAN_WSI_XCB,
               g_x11_dpy, &g_x11_win,
               width, height, x->interval))
         goto error;
   }

   XSync(g_x11_dpy, False);

   x11_install_quit_atom();

   gfx_ctx_x_vk_swap_interval(data, x->interval);

   /* This can blow up on some drivers.
    * It's not fatal, so override errors for this call. */
   old_handler = XSetErrorHandler(x_vk_nul_handler);
   XSetInputFocus(g_x11_dpy, g_x11_win, RevertToNone, CurrentTime);
   XSync(g_x11_dpy, False);
   XSetErrorHandler(old_handler);

   XFree(vi);
   vi = NULL;

   if (!x11_input_ctx_new(true_full))
      goto error;

   return true;

error:
   if (vi)
      XFree(vi);

   gfx_ctx_x_vk_destroy_resources(x);

   if (x)
      free(x);
   g_x11_screen = 0;

   return false;
}

static void gfx_ctx_x_vk_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   void *x_input            = NULL;
#ifdef HAVE_UDEV
   settings_t *settings     = config_get_ptr();
   const char *input_driver = settings->arrays.input_driver;

   if (string_is_equal(input_driver, "udev"))
   {
      *input_data = input_driver_init_wrap(&input_udev, joypad_name);
      if (*input_data)
      {
         *input = &input_udev;
         return;
      }
   }
#endif

   x_input      = input_driver_init_wrap(&input_x, joypad_name);
   *input       = x_input ? &input_x : NULL;
   *input_data  = x_input;
}

static bool gfx_ctx_x_vk_suppress_screensaver(void *data, bool enable)
{
   if (video_driver_display_type_get() != RARCH_DISPLAY_X11)
      return false;

   x11_suspend_screensaver(video_driver_window_get(), enable);

   return true;
}

static enum gfx_ctx_api gfx_ctx_x_vk_get_api(void *data)
{
   return GFX_CTX_VULKAN_API;
}

static bool gfx_ctx_x_vk_bind_api(void *data, enum gfx_ctx_api api,
      unsigned major, unsigned minor)
{
   if (api == GFX_CTX_VULKAN_API)
         return true;

   return false;
}

static void gfx_ctx_x_vk_show_mouse(void *data, bool state)
{
   x11_show_mouse(g_x11_dpy, g_x11_win, state);
}

static void gfx_ctx_x_vk_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)data;

   if (!x)
      return;
}

static void *gfx_ctx_x_vk_get_context_data(void *data)
{
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)data;
   return &x->vk.context;
}

static uint32_t gfx_ctx_x_vk_get_flags(void *data)
{
   uint32_t      flags = 0;
   gfx_ctx_x_vk_data_t *x = (gfx_ctx_x_vk_data_t*)data;

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static void gfx_ctx_x_vk_set_flags(void *data, uint32_t flags) { }

const gfx_ctx_driver_t gfx_ctx_vk_x = {
   gfx_ctx_x_vk_init,
   gfx_ctx_x_vk_destroy,
   gfx_ctx_x_vk_get_api,
   gfx_ctx_x_vk_bind_api,
   gfx_ctx_x_vk_swap_interval,
   gfx_ctx_x_vk_set_video_mode,
   x11_get_video_size,
   x11_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   x11_get_metrics,
   NULL,
   x11_update_title,
   gfx_ctx_x_vk_check_window,
   gfx_ctx_x_vk_set_resize,
   x11_has_focus,
   gfx_ctx_x_vk_suppress_screensaver,
   true, /* has_windowed */
   gfx_ctx_x_vk_swap_buffers,
   gfx_ctx_x_vk_input_driver,
   NULL, /* get_proc_address */
   NULL,
   NULL,
   gfx_ctx_x_vk_show_mouse,
   "vk_x",
   gfx_ctx_x_vk_get_flags,
   gfx_ctx_x_vk_set_flags,

   gfx_ctx_x_vk_bind_hw_render,
   gfx_ctx_x_vk_get_context_data,
   NULL /* make_current */
};

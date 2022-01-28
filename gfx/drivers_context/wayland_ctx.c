/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <unistd.h>

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_EGL
#include <wayland-egl.h>
#include "../common/egl_common.h"
#endif

#ifdef HAVE_LIBDECOR
#include <libdecor.h>
#endif

#include "../../frontend/frontend_driver.h"
#include "../../input/common/wayland_common.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../../verbosity.h"

/* Generated from idle-inhibit-unstable-v1.xml */
#include "../common/wayland/idle-inhibit-unstable-v1.h"

/* Generated from xdg-shell.xml */
#include "../common/wayland/xdg-shell.h"

/* Generated from xdg-decoration-unstable-v1.h */
#include "../common/wayland/xdg-decoration-unstable-v1.h"

static enum gfx_ctx_api wl_api   = GFX_CTX_NONE;

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

static void handle_toplevel_config_common(void *data,
      void *toplevel,
      int32_t width, int32_t height, struct wl_array *states)
{
   const uint32_t *state;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   wl->fullscreen             = false;
   wl->maximized              = false;

   WL_ARRAY_FOR_EACH(state, states, const uint32_t*)
   {
      switch (*state)
      {
         case XDG_TOPLEVEL_STATE_FULLSCREEN:
            wl->fullscreen = true;
            break;
         case XDG_TOPLEVEL_STATE_MAXIMIZED:
            wl->maximized = true;
            break;
         case XDG_TOPLEVEL_STATE_RESIZING:
            wl->resize = true;
            break;
         case XDG_TOPLEVEL_STATE_ACTIVATED:
            wl->activated = true;
            break;
      }
   }
   if (     width  > 0 
         && height > 0)
   {
      wl->prev_width  = width;
      wl->prev_height = height;
      wl->width       = width;
      wl->height      = height;
   }

#ifdef HAVE_EGL
   if (wl->win)
      wl_egl_window_resize(wl->win, wl->width, wl->height, 0, 0);
   else
      wl->win = wl_egl_window_create(wl->surface,
            wl->width * wl->buffer_scale,
            wl->height * wl->buffer_scale);
#endif

   wl->configured = false;
}

/* Shell surface callbacks. */
static void handle_toplevel_config(void *data,
      struct xdg_toplevel *toplevel,
      int32_t width, int32_t height, struct wl_array *states)
{
   handle_toplevel_config_common(data, toplevel, width, height, states);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    handle_toplevel_config,
    handle_toplevel_close,
};

static void gfx_ctx_wl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->surface == NULL) {
      output_info_t *oi, *tmp;
      oi = wl->current_output;

      // If window is not ready get any monitor
      if (!oi)
          wl_list_for_each_safe(oi, tmp, &wl->all_outputs, link)
              break;

      *width  = oi->width;
      *height = oi->height;
   } else {
      *width  = wl->width  * wl->buffer_scale;
      *height = wl->height * wl->buffer_scale;
   }
}

static void gfx_ctx_wl_destroy_resources(gfx_ctx_wayland_data_t *wl)
{
   if (!wl)
      return;

#ifdef HAVE_EGL
   egl_destroy(&wl->egl);

   if (wl->win)
      wl_egl_window_destroy(wl->win);
#endif

#ifdef HAVE_XKBCOMMON
   free_xkb();
#endif

   if (wl->wl_keyboard)
      wl_keyboard_destroy(wl->wl_keyboard);
   if (wl->wl_pointer)
      wl_pointer_destroy(wl->wl_pointer);
   if (wl->wl_touch)
      wl_touch_destroy(wl->wl_touch);

   if (wl->cursor.theme)
      wl_cursor_theme_destroy(wl->cursor.theme);
   if (wl->cursor.surface)
      wl_surface_destroy(wl->cursor.surface);

   if (wl->seat)
      wl_seat_destroy(wl->seat);
   if (wl->xdg_shell)
      xdg_wm_base_destroy(wl->xdg_shell);
   if (wl->compositor)
      wl_compositor_destroy(wl->compositor);
   if (wl->registry)
      wl_registry_destroy(wl->registry);
   if (wl->xdg_surface)
      xdg_surface_destroy(wl->xdg_surface);
   if (wl->surface)
      wl_surface_destroy(wl->surface);
   if (wl->xdg_toplevel)
      xdg_toplevel_destroy(wl->xdg_toplevel);
   if (wl->idle_inhibit_manager)
      zwp_idle_inhibit_manager_v1_destroy(wl->idle_inhibit_manager);
   if (wl->deco)
      zxdg_toplevel_decoration_v1_destroy(wl->deco);
   if (wl->deco_manager)
      zxdg_decoration_manager_v1_destroy(wl->deco_manager);
   if (wl->idle_inhibitor)
      zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);

   if (wl->input.dpy)
   {
      wl_display_flush(wl->input.dpy);
      wl_display_disconnect(wl->input.dpy);
   }

#ifdef HAVE_EGL
   wl->win              = NULL;
#endif
   wl->xdg_shell        = NULL;
   wl->compositor       = NULL;
   wl->registry         = NULL;
   wl->input.dpy        = NULL;
   wl->xdg_surface      = NULL;
   wl->surface          = NULL;
   wl->xdg_toplevel     = NULL;

   wl->width            = 0;
   wl->height           = 0;

}

static void gfx_ctx_wl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   /* this function works with SCALED sizes, it's used from the renderer */
   unsigned new_width, new_height;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   flush_wayland_fd(&wl->input);

   new_width  = *width  * wl->last_buffer_scale;
   new_height = *height * wl->last_buffer_scale;

   gfx_ctx_wl_get_video_size(data, &new_width, &new_height);

   if (  new_width  != *width  * wl->last_buffer_scale ||
         new_height != *height * wl->last_buffer_scale)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;

      wl->last_buffer_scale = wl->buffer_scale;
   }

   *quit = (bool)frontend_driver_get_signal_handler_state();
}

static bool gfx_ctx_wl_set_resize(void *data, unsigned width, unsigned height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

#ifdef HAVE_EGL
   wl_egl_window_resize(wl->win, width, height, 0, 0);
#endif

   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);
   return true;
}

static void gfx_ctx_wl_update_title(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

#ifdef HAVE_LIBDECOR
   if (wl && title[0])
      libdecor_frame_set_title(wl->libdecor_frame, title);
#else
   if (wl && title[0])
   {
      if (wl->deco)
         zxdg_toplevel_decoration_v1_set_mode(wl->deco,
            ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
      xdg_toplevel_set_title(wl->xdg_toplevel, title);
   }
#endif
}

static bool gfx_ctx_wl_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   output_info_t *oi, *tmp;
   oi = wl->current_output;

   if (!oi)
      wl_list_for_each_safe(oi, tmp, &wl->all_outputs, link)
         break;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = (float)oi->physical_width;
         break;

      case DISPLAY_METRIC_MM_HEIGHT:
         *value = (float)oi->physical_height;
         break;

      case DISPLAY_METRIC_DPI:
         *value = (float)oi->width * 25.4f /
                  (float)oi->physical_width;
         break;

      default:
         *value = 0.0f;
         return false;
   }

   return true;
}

#ifdef HAVE_LIBDECOR
static void
handle_libdecor_error(struct libdecor *context,
      enum libdecor_error error, const char *message)
{
   RARCH_ERR("[Wayland]: libdecor Caught error (%d): %s\n", error, message);
}

static struct libdecor_interface libdecor_interface = {
   .error = handle_libdecor_error,
};

static void
handle_libdecor_frame_configure(struct libdecor_frame *frame,
      struct libdecor_configuration *configuration, void *data)
{
   int width, height;
   gfx_ctx_wayland_data_t *wl            = (gfx_ctx_wayland_data_t*)data;
   struct libdecor_state *state          = NULL;
   static const enum 
      libdecor_window_state tiled_states = (
        LIBDECOR_WINDOW_STATE_TILED_LEFT 
      | LIBDECOR_WINDOW_STATE_TILED_RIGHT
      | LIBDECOR_WINDOW_STATE_TILED_TOP 
      | LIBDECOR_WINDOW_STATE_TILED_BOTTOM
   );
   enum libdecor_window_state window_state;
   bool focused      = false;
   bool tiled        = false;

   wl->fullscreen     = false;
   wl->maximized      = false;

   if (libdecor_configuration_get_window_state(
            configuration, &window_state))
   {
      wl->fullscreen  = (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) != 0;
      wl->maximized   = (window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED) != 0;
      focused         = (window_state & LIBDECOR_WINDOW_STATE_ACTIVE) != 0;
      tiled           = (window_state & tiled_states) != 0;
   }

   if (!libdecor_configuration_get_content_size(configuration, frame,
      &width, &height))
   {
      width           = wl->prev_width;
      height          = wl->prev_height;
   }

   if (width > 0 && height > 0)
   {
      wl->prev_width  = width;
      wl->prev_height = height;
      wl->width       = width;
      wl->height      = height;
   }

#ifdef HAVE_EGL
   if (wl->win)
      wl_egl_window_resize(wl->win, wl->width, wl->height, 0, 0);
   else
      wl->win         = wl_egl_window_create(
            wl->surface,
            wl->width  * wl->buffer_scale,
            wl->height * wl->buffer_scale);
#endif

   state = libdecor_state_new(wl->width, wl->height);
   libdecor_frame_commit(frame, state, configuration);
   libdecor_state_free(state);

   wl->configured = false;
}

static void
handle_libdecor_frame_close(struct libdecor_frame *frame,
      void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   command_event(CMD_EVENT_QUIT, NULL);
}

static void
handle_libdecor_frame_commit(struct libdecor_frame *frame,
      void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
}

static struct libdecor_frame_interface libdecor_frame_interface = {
   handle_libdecor_frame_configure,
   handle_libdecor_frame_close,
   handle_libdecor_frame_commit,
};
#endif

#define DEFAULT_WINDOWED_WIDTH 640
#define DEFAULT_WINDOWED_HEIGHT 480

#ifdef HAVE_EGL
#define WL_EGL_ATTRIBS_BASE \
   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
   EGL_RED_SIZE,        1, \
   EGL_GREEN_SIZE,      1, \
   EGL_BLUE_SIZE,       1, \
   EGL_ALPHA_SIZE,      0, \
   EGL_DEPTH_SIZE,      0
#endif

static void *gfx_ctx_wl_init(void *video_driver)
{
   int i;
#ifdef HAVE_EGL
   static const EGLint egl_attribs_gl[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE,
   };

#ifdef HAVE_OPENGLES
#ifdef HAVE_OPENGLES2
   static const EGLint egl_attribs_gles[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };
#endif

#ifdef HAVE_OPENGLES3
#ifdef EGL_KHR_create_context
   static const EGLint egl_attribs_gles3[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
   };
#endif
#endif

#endif

   static const EGLint egl_attribs_vg[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };

   EGLint n;
   EGLint major = 0, minor    = 0;
   const EGLint *attrib_ptr   = NULL;
#endif
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      calloc(1, sizeof(gfx_ctx_wayland_data_t));

   if (!wl)
      return NULL;

   wl_list_init(&wl->all_outputs);

#ifdef HAVE_EGL
   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         attrib_ptr = egl_attribs_gl;
#endif
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
#ifdef HAVE_OPENGLES3
#ifdef EGL_KHR_create_context
         if (g_egl_major >= 3)
            attrib_ptr = egl_attribs_gles3;
         else
#endif
#endif
#ifdef HAVE_OPENGLES2
            attrib_ptr = egl_attribs_gles;
#endif
#endif
         break;
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_VG
         attrib_ptr = egl_attribs_vg;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
#endif

   frontend_driver_destroy_signal_handler_state();

   wl->input.dpy         = wl_display_connect(NULL);
   wl->last_buffer_scale = 1;
   wl->buffer_scale      = 1;

   if (!wl->input.dpy)
   {
      RARCH_ERR("[Wayland]: Failed to connect to Wayland server.\n");
      goto error;
   }

   frontend_driver_install_signal_handler();

   wl->registry = wl_display_get_registry(wl->input.dpy);
   wl_registry_add_listener(wl->registry, &registry_listener, wl);
   wl_display_roundtrip(wl->input.dpy);

   if (!wl->compositor)
   {
      RARCH_ERR("[Wayland]: Failed to create compositor.\n");
      goto error;
   }

   if (!wl->shm)
   {
      RARCH_ERR("[Wayland]: Failed to create shm.\n");
      goto error;
   }

   if (!wl->xdg_shell)
   {
	   RARCH_ERR("[Wayland]: Failed to create shell.\n");
	   goto error;
   }

   if (!wl->idle_inhibit_manager)
   {
	   RARCH_WARN("[Wayland]: Compositor doesn't support zwp_idle_inhibit_manager_v1 protocol!\n");
   }

   if (!wl->deco_manager)
   {
	   RARCH_WARN("[Wayland]: Compositor doesn't support zxdg_decoration_manager_v1 protocol!\n");
   }

   wl->input.fd = wl_display_get_fd(wl->input.dpy);

#ifdef HAVE_EGL
   if (!egl_init_context(&wl->egl,
            EGL_PLATFORM_WAYLAND_KHR,
            (EGLNativeDisplayType)wl->input.dpy,
            &major, &minor, &n, attrib_ptr,
            egl_default_accept_config_cb))
   {
      egl_report_error();
      goto error;
   }

   if (n == 0 || !egl_has_config(&wl->egl))
      goto error;
#endif

   wl->input.keyboard_focus  = true;
   wl->input.mouse.focus     = true;

   wl->cursor.surface        = wl_compositor_create_surface(wl->compositor);
   wl->cursor.theme          = wl_cursor_theme_load(NULL, 16, wl->shm);
   wl->cursor.default_cursor = wl_cursor_theme_get_cursor(wl->cursor.theme, "left_ptr");

   wl->num_active_touches                   = 0;

   for (i = 0;i < MAX_TOUCHES;i++)
   {
       wl->active_touch_positions[i].active = false;
       wl->active_touch_positions[i].id     = -1;
       wl->active_touch_positions[i].x      = (unsigned) 0;
       wl->active_touch_positions[i].y      = (unsigned) 0;
   }

   flush_wayland_fd(&wl->input);

   return wl;

error:
   gfx_ctx_wl_destroy_resources(wl);

   if (wl)
      free(wl);

   return NULL;
}

#ifdef HAVE_EGL
static EGLint *egl_fill_attribs(gfx_ctx_wayland_data_t *wl, EGLint *attr)
{
   switch (wl_api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
      {
         bool debug       = false;
#ifdef HAVE_OPENGL
         unsigned version = wl->egl.major * 1000 + wl->egl.minor;
         bool core        = version >= 3001;
#ifndef GL_DEBUG
         struct retro_hw_render_callback *hwr =
            video_driver_get_hw_context();
#endif

#ifdef GL_DEBUG
         debug            = true;
#else
         debug            = hwr->debug_context;
#endif

         if (core)
         {
            *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
            *attr++ = wl->egl.major;
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = wl->egl.minor;
            /* Technically, we don't have core/compat until 3.2.
             * Version 3.1 is either compat or not depending 
             * on GL_ARB_compatibility. */
            if (version >= 3002)
            {
               *attr++ = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
               *attr++ = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
            }
         }

         if (debug)
         {
            *attr++ = EGL_CONTEXT_FLAGS_KHR;
            *attr++ = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
         }
#endif

         break;
      }
#endif

      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
         *attr++    = EGL_CONTEXT_CLIENT_VERSION; 
         /* Same as EGL_CONTEXT_MAJOR_VERSION */
         *attr++    = wl->egl.major ? (EGLint)wl->egl.major : 2;
#ifdef EGL_KHR_create_context
         if (wl->egl.minor > 0)
         {
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = wl->egl.minor;
         }
#endif
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   *attr = EGL_NONE;
   return attr;
}
#endif

static void gfx_ctx_wl_destroy(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl)
      return;

   gfx_ctx_wl_destroy_resources(wl);

   free(wl);
}

static void gfx_ctx_wl_set_swap_interval(void *data, int swap_interval)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

#ifdef HAVE_EGL
   egl_set_swap_interval(&wl->egl, swap_interval);
#endif
}

static bool gfx_ctx_wl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifdef HAVE_EGL
   EGLint egl_attribs[16];
   EGLint *attr              = egl_fill_attribs(
         (gfx_ctx_wayland_data_t*)data, egl_attribs);
#endif
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   wl->width                  = width  ? width  : DEFAULT_WINDOWED_WIDTH;
   wl->height                 = height ? height : DEFAULT_WINDOWED_HEIGHT;

   wl->surface                = wl_compositor_create_surface(wl->compositor);

   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);
   wl_surface_add_listener(wl->surface, &wl_surface_listener, wl);

#ifdef HAVE_EGL
   wl->win        = wl_egl_window_create(wl->surface, wl->width * wl->buffer_scale, wl->height * wl->buffer_scale);
#endif

#ifdef HAVE_LIBDECOR
   wl->libdecor_context = libdecor_new(wl->input.dpy, &libdecor_interface);
   if (wl->libdecor_context)
   {
      wl->libdecor_frame = libdecor_decorate(wl->libdecor_context, wl->surface, &libdecor_frame_interface, wl);
      if (!wl->libdecor_frame)
      {
         RARCH_ERR("[Wayland]: Failed to crate libdecor frame\n");
         goto error;
      }

      libdecor_frame_set_app_id(wl->libdecor_frame, "retroarch");
      libdecor_frame_set_title(wl->libdecor_frame, "RetroArch");
      libdecor_frame_map(wl->libdecor_frame);
   }

   /* Waiting for libdecor to be configured before starting to draw */
   wl_surface_commit(wl->surface);
   wl->configured = true;

   while (wl->configured)
   {
      if (libdecor_dispatch(wl->libdecor_context, 0) < 0)
      {
         RARCH_ERR("[Wayland]: libdecor failed to dispatch\n");
         goto error;
      }
   }
#else
   wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->xdg_shell, wl->surface);
   xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);

   wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
   xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

   xdg_toplevel_set_app_id(wl->xdg_toplevel, "retroarch");
   xdg_toplevel_set_title(wl->xdg_toplevel, "RetroArch");

   if (wl->deco_manager)
      wl->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
            wl->deco_manager, wl->xdg_toplevel);

   /* Waiting for xdg_toplevel to be configured before starting to draw */
   wl_surface_commit(wl->surface);
   wl->configured = true;

   while (wl->configured)
      wl_display_dispatch(wl->input.dpy);
#endif

   wl_display_roundtrip(wl->input.dpy);
   xdg_wm_base_add_listener(wl->xdg_shell, &xdg_shell_listener, NULL);

#ifdef HAVE_EGL
   if (!egl_create_context(&wl->egl, (attr != egl_attribs)
            ? egl_attribs : NULL))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(&wl->egl, (EGLNativeWindowType)wl->win))
      goto error;
   egl_set_swap_interval(&wl->egl, wl->egl.interval);
#endif

   if (fullscreen)
   {
#ifdef HAVE_LIBDECOR
      libdecor_frame_set_fullscreen(wl->libdecor_frame, NULL);
#else
	   xdg_toplevel_set_fullscreen(wl->xdg_toplevel, NULL);
#endif
	}

   flush_wayland_fd(&wl->input);

   if (fullscreen)
   {
      wl->cursor.visible = false;
      gfx_ctx_wl_show_mouse(wl, false);
   }
   else
      wl->cursor.visible = true;

   return true;

#if defined(HAVE_EGL)
error:
   gfx_ctx_wl_destroy(data);
   return false;
#endif
}

bool input_wl_init(void *data, const char *joypad_name);

static void gfx_ctx_wl_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   /* Input is heavily tied to the window stuff
    * on Wayland, so just implement the input driver here. */
   if (!input_wl_init(&wl->input, joypad_name))
   {
      wl->input.gfx = NULL;
      *input        = NULL;
      *input_data   = NULL;
   }
   else
   {
      wl->input.gfx = wl;
      *input        = &input_wayland;
      *input_data   = &wl->input;
      input_driver_init_joypads();
   }
}

static bool gfx_ctx_wl_has_focus(void *data)
{
   (void)data;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return wl->input.keyboard_focus;
}

static bool gfx_ctx_wl_suppress_screensaver(void *data, bool state)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl->idle_inhibit_manager)
      return false;
   if (state == (!!wl->idle_inhibitor))
      return true;

   if (state)
   {
      RARCH_LOG("[Wayland]: Enabling idle inhibitor\n");
      struct zwp_idle_inhibit_manager_v1 *mgr = wl->idle_inhibit_manager;
      wl->idle_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(mgr, wl->surface);
   }
   else
   {
      RARCH_LOG("[Wayland]: Disabling the idle inhibitor\n");
      zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);
      wl->idle_inhibitor = NULL;
   }
   return true;
}

static enum gfx_ctx_api gfx_ctx_wl_get_api(void *data)
{
   return wl_api;
}

static bool gfx_ctx_wl_bind_api(void *video_driver,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
#ifdef HAVE_EGL
   g_egl_major = major;
   g_egl_minor = minor;
#endif
   wl_api      = api;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            return false;
#endif
#ifdef HAVE_EGL
         if (egl_bind_api(EGL_OPENGL_API))
            return true;
#endif
#endif
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
#ifndef EGL_KHR_create_context
         if (major >= 3)
            return false;
#endif
#ifdef HAVE_EGL
         if (egl_bind_api(EGL_OPENGL_ES_API))
            return true;
#endif
#endif
         break;
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_VG
#ifdef HAVE_EGL
         if (egl_bind_api(EGL_OPENVG_API))
            return true;
#endif
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_wl_swap_buffers(void *data)
{
#ifdef HAVE_EGL
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   egl_swap_buffers(&wl->egl);
#endif
}

static void gfx_ctx_wl_bind_hw_render(void *data, bool enable)
{
#ifdef HAVE_EGL
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   egl_bind_hw_render(&wl->egl, enable);
#endif
}

static uint32_t gfx_ctx_wl_get_flags(void *data)
{
   uint32_t             flags = 0;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->core_hw_context_enable)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   if (string_is_equal(video_driver_get_ident(), "glcore"))
   {
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   }
   else if (string_is_equal(video_driver_get_ident(), "gl"))
   {
#ifdef HAVE_GLSL
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
   }

   return flags;
}

static void gfx_ctx_wl_set_flags(void *data, uint32_t flags)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      wl->core_hw_context_enable = true;
}

static float gfx_ctx_wl_get_refresh_rate(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl || !wl->current_output)
      return false;

   return (float) wl->current_output->refresh_rate / 1000.0f;
}

const gfx_ctx_driver_t gfx_ctx_wayland = {
   gfx_ctx_wl_init,
   gfx_ctx_wl_destroy,
   gfx_ctx_wl_get_api,
   gfx_ctx_wl_bind_api,
   gfx_ctx_wl_set_swap_interval,
   gfx_ctx_wl_set_video_mode,
   gfx_ctx_wl_get_video_size,
   gfx_ctx_wl_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_wl_get_metrics,
   NULL,
   gfx_ctx_wl_update_title,
   gfx_ctx_wl_check_window,
   gfx_ctx_wl_set_resize,
   gfx_ctx_wl_has_focus,
   gfx_ctx_wl_suppress_screensaver,
   true, /* has_windowed */
   gfx_ctx_wl_swap_buffers,
   gfx_ctx_wl_input_driver,
#ifdef HAVE_EGL
   egl_get_proc_address,
#else
   NULL,
#endif
   NULL,
   NULL,
   gfx_ctx_wl_show_mouse,
   "wayland",
   gfx_ctx_wl_get_flags,
   gfx_ctx_wl_set_flags,
   gfx_ctx_wl_bind_hw_render,
   NULL,
   NULL,
};

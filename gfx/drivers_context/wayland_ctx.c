/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include <sys/poll.h>
#include <signal.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include "../../driver.h"
#include "../../general.h"
#include "../../runloop.h"
#include "../video_monitor.h"
#include "../common/egl_common.h"
#include "../common/gl_common.h"

typedef struct gfx_ctx_wayland_data
{
   bool g_resize;
   int g_fd;
   unsigned g_width;
   unsigned g_height;
   struct wl_display *g_dpy;
   struct wl_registry *g_registry;
   struct wl_compositor *g_compositor;
   struct wl_surface *g_surface;
   struct wl_shell_surface *g_shell_surf;
   struct wl_shell *g_shell;
   struct wl_egl_window *g_win;
   struct wl_keyboard *g_wl_keyboard;
   struct wl_pointer  *g_wl_pointer;
} gfx_ctx_wayland_data_t;


static enum gfx_ctx_api g_api;
static unsigned g_major;
static unsigned g_minor;

static volatile sig_atomic_t g_quit;

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

/* Shell surface callbacks. */
static void shell_surface_handle_ping(void *data,
      struct wl_shell_surface *shell_surface,
      uint32_t serial)
{
   (void)data;
   wl_shell_surface_pong(shell_surface, serial);
}

static void shell_surface_handle_configure(void *data,
      struct wl_shell_surface *shell_surface,
      uint32_t edges, int32_t width, int32_t height)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;

   (void)data;
   (void)shell_surface;
   (void)edges;

   wl->g_width = width;
   wl->g_height = height;

   RARCH_LOG("[Wayland/EGL]: Surface configure: %u x %u.\n",
         wl->g_width, wl->g_height);
}

static void shell_surface_handle_popup_done(void *data,
      struct wl_shell_surface *shell_surface)
{
   (void)data;
   (void)shell_surface;
}

static const struct wl_shell_surface_listener shell_surface_listener = {
   shell_surface_handle_ping,
   shell_surface_handle_configure,
   shell_surface_handle_popup_done,
};

/* Registry callbacks. */
static void registry_handle_global(void *data, struct wl_registry *reg,
      uint32_t id, const char *interface, uint32_t version)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;

   (void)data;
   (void)version;

   if (!strcmp(interface, "wl_compositor"))
      wl->g_compositor = (struct wl_compositor*)wl_registry_bind(reg, id, &wl_compositor_interface, 1);
   else if (!strcmp(interface, "wl_shell"))
      wl->g_shell = (struct wl_shell*)wl_registry_bind(reg, id, &wl_shell_interface, 1);
}

static void registry_handle_global_remove(void *data,
      struct wl_registry *registry, uint32_t id)
{
   (void)data;
   (void)registry;
   (void)id;
}

static const struct wl_registry_listener registry_listener = {
   registry_handle_global,
   registry_handle_global_remove,
};



static void gfx_ctx_wl_get_video_size(void *data,
      unsigned *width, unsigned *height);

static void gfx_ctx_wl_destroy_resources(gfx_ctx_wayland_data_t *wl)
{
   if (!wl)
      return;

   egl_destroy(NULL);

   if (wl->g_win)
      wl_egl_window_destroy(wl->g_win);
   if (wl->g_shell)
      wl_shell_destroy(wl->g_shell);
   if (wl->g_compositor)
      wl_compositor_destroy(wl->g_compositor);
   if (wl->g_registry)
      wl_registry_destroy(wl->g_registry);
   if (wl->g_shell_surf)
      wl_shell_surface_destroy(wl->g_shell_surf);
   if (wl->g_surface)
      wl_surface_destroy(wl->g_surface);

   if (wl->g_dpy)
   {
      wl_display_flush(wl->g_dpy);
      wl_display_disconnect(wl->g_dpy);
   }

   wl->g_win        = NULL;
   wl->g_shell      = NULL;
   wl->g_compositor = NULL;
   wl->g_registry   = NULL;
   wl->g_dpy        = NULL;
   wl->g_shell_surf = NULL;
   wl->g_surface    = NULL;

   wl->g_width  = 0;
   wl->g_height = 0;
}

static void flush_wayland_fd(void)
{
   struct pollfd fd = {0};
   driver_t *driver = driver_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;

   wl_display_dispatch_pending(wl->g_dpy);
   wl_display_flush(wl->g_dpy);

   fd.fd = wl->g_fd;
   fd.events = POLLIN | POLLOUT | POLLERR | POLLHUP;

   if (poll(&fd, 1, 0) > 0)
   {
      if (fd.revents & (POLLERR | POLLHUP))
      {
         close(wl->g_fd);
         g_quit = true;
      }

      if (fd.revents & POLLIN)
         wl_display_dispatch(wl->g_dpy);
      if (fd.revents & POLLOUT)
         wl_display_flush(wl->g_dpy);
   }
}

static void gfx_ctx_wl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height,
      unsigned frame_count)
{
   unsigned new_width, new_height;

   (void)frame_count;

   flush_wayland_fd();

   new_width = *width;
   new_height = *height;

   gfx_ctx_wl_get_video_size(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *resize = true;
      *width  = new_width;
      *height = new_height;
   }

   *quit = g_quit;
}

static void gfx_ctx_wl_set_resize(void *data, unsigned width, unsigned height)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;

   (void)data;

   wl_egl_window_resize(wl->g_win, width, height, 0, 0);
}

static void gfx_ctx_wl_update_window_title(void *data)
{
   char buf[128]              = {0};
   char buf_fps[128]          = {0};
   driver_t *driver           = driver_get_ptr();
   settings_t *settings       = config_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;

   (void)data;

   if (video_monitor_get_fps(buf, sizeof(buf),  
            buf_fps, sizeof(buf_fps)))
      wl_shell_surface_set_title(wl->g_shell_surf, buf);

   if (settings->fps_show)
      rarch_main_msg_queue_push(buf_fps, 1, 1, false);
}

static void gfx_ctx_wl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;

   (void)data;

   *width  = wl->g_width;
   *height = wl->g_height;
}

#define DEFAULT_WINDOWED_WIDTH 640
#define DEFAULT_WINDOWED_HEIGHT 480

#define WL_EGL_ATTRIBS_BASE \
   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
   EGL_RED_SIZE,        1, \
   EGL_GREEN_SIZE,      1, \
   EGL_BLUE_SIZE,       1, \
   EGL_ALPHA_SIZE,      0, \
   EGL_DEPTH_SIZE,      0

static bool gfx_ctx_wl_init(void *data)
{
   static const EGLint egl_attribs_gl[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE,
   };

   static const EGLint egl_attribs_gles[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };

#ifdef EGL_KHR_create_context
   static const EGLint egl_attribs_gles3[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
   };
#endif

   static const EGLint egl_attribs_vg[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };

   EGLint egl_major = 0, egl_minor = 0;
   EGLint num_configs;
   const EGLint *attrib_ptr;
   driver_t *driver = driver_get_ptr();

   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      calloc(1, sizeof(gfx_ctx_wayland_data_t));

   (void)data;

   if (!wl)
      return false;

   switch (g_api)
   {
      case GFX_CTX_OPENGL_API:
         attrib_ptr = egl_attribs_gl;
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef EGL_KHR_create_context
         if (g_major >= 3)
            attrib_ptr = egl_attribs_gles3;
         else
#endif
            attrib_ptr = egl_attribs_gles;
         break;
      case GFX_CTX_OPENVG_API:
         attrib_ptr = egl_attribs_vg;
         break;
      default:
         attrib_ptr = NULL;
   }

   g_quit = 0;

   wl->g_dpy = wl_display_connect(NULL);
   if (!wl->g_dpy)
   {
      RARCH_ERR("Failed to connect to Wayland server.\n");
      goto error;
   }

   driver->video_context_data = wl;

   wl->g_registry = wl_display_get_registry(wl->g_dpy);
   wl_registry_add_listener(wl->g_registry, &registry_listener, NULL);
   wl_display_dispatch(wl->g_dpy);

   if (!wl->g_compositor)
   {
      RARCH_ERR("Failed to create compositor.\n");
      goto error;
   }

   if (!wl->g_shell)
   {
      RARCH_ERR("Failed to create shell.\n");
      goto error;
   }

   wl->g_fd = wl_display_get_fd(wl->g_dpy);

   g_egl_dpy = eglGetDisplay((EGLNativeDisplayType)wl->g_dpy);

   if (!g_egl_dpy)
   {
      RARCH_ERR("Failed to create EGL window.\n");
      goto error;
   }

   if (!eglInitialize(g_egl_dpy, &egl_major, &egl_minor))
   {
      RARCH_ERR("Failed to initialize EGL.\n");
      goto error;
   }

   RARCH_LOG("[Wayland/EGL]: EGL version: %d.%d\n", egl_major, egl_minor);

   if (!eglChooseConfig(g_egl_dpy, attrib_ptr, &g_egl_config, 1, &num_configs))
   {
      RARCH_ERR("[Wayland/EGL]: eglChooseConfig failed with 0x%x.\n", eglGetError());
      goto error;
   }

   if (num_configs == 0 || !g_egl_config)
   {
      RARCH_ERR("[Wayland/EGL]: No EGL configurations available.\n");
      goto error;
   }

   return true;

error:
   gfx_ctx_wl_destroy_resources(wl);

   if (wl)
      free(wl);

   if (driver->video_context_data)
      free(driver->video_context_data);
   driver->video_context_data = NULL;

   return false;
}

static EGLint *egl_fill_attribs(EGLint *attr)
{
   switch (g_api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
      {
         unsigned version = g_major * 1000 + g_minor;
         bool core = version >= 3001;
#ifdef GL_DEBUG
         bool debug = true;
#else
         const struct retro_hw_render_callback *hw_render =
            (const struct retro_hw_render_callback*)video_driver_callback();
         bool debug = hw_render->debug_context;
#endif

         if (core)
         {
            *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
            *attr++ = g_major;
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = g_minor;
            /* Technically, we don't have core/compat until 3.2.
             * Version 3.1 is either compat or not depending on GL_ARB_compatibility. */
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

         break;
      }
#endif

      case GFX_CTX_OPENGL_ES_API:
         *attr++ = EGL_CONTEXT_CLIENT_VERSION; /* Same as EGL_CONTEXT_MAJOR_VERSION */
         *attr++ = g_major ? (EGLint)g_major : 2;
#ifdef EGL_KHR_create_context
         if (g_minor > 0)
         {
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = g_minor;
         }
#endif
         break;

      default:
         break;
   }

   *attr = EGL_NONE;
   return attr;
}

static void gfx_ctx_wl_destroy(void *data)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;

   (void)data;

   if (!wl)
      return;

   gfx_ctx_wl_destroy_resources(wl);

   if (driver->video_context_data)
      free(driver->video_context_data);
   driver->video_context_data = NULL;
}

static bool gfx_ctx_wl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   EGLint egl_attribs[16];
   driver_t *driver = driver_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;
   struct sigaction sa = {{0}};
   EGLint *attr = NULL;

   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   attr = egl_fill_attribs(egl_attribs);

   wl->g_width = width ? width : DEFAULT_WINDOWED_WIDTH;
   wl->g_height = height ? height : DEFAULT_WINDOWED_HEIGHT;

   wl->g_surface = wl_compositor_create_surface(wl->g_compositor);
   wl->g_win = wl_egl_window_create(wl->g_surface, wl->g_width, wl->g_height);
   wl->g_shell_surf = wl_shell_get_shell_surface(wl->g_shell, wl->g_surface);

   wl_shell_surface_add_listener(wl->g_shell_surf, &shell_surface_listener, NULL);
   wl_shell_surface_set_toplevel(wl->g_shell_surf);
   wl_shell_surface_set_class(wl->g_shell_surf, "RetroArch");
   wl_shell_surface_set_title(wl->g_shell_surf, "RetroArch");

   g_egl_ctx = eglCreateContext(g_egl_dpy, g_egl_config, EGL_NO_CONTEXT,
         attr != egl_attribs ? egl_attribs : NULL);

   RARCH_LOG("[Wayland/EGL]: Created context: %p.\n", (void*)g_egl_ctx);
   if (g_egl_ctx == EGL_NO_CONTEXT)
      goto error;

   if (g_use_hw_ctx)
   {
      g_egl_hw_ctx = eglCreateContext(g_egl_dpy, g_egl_config, g_egl_ctx,
            attr != egl_attribs ? egl_attribs : NULL);
      RARCH_LOG("[Wayland/EGL]: Created shared context: %p.\n", (void*)g_egl_hw_ctx);

      if (g_egl_hw_ctx == EGL_NO_CONTEXT)
         goto error;
   }

   g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_egl_config,
         (EGLNativeWindowType)wl->g_win, NULL);
   if (!g_egl_surf)
      goto error;

   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
      goto error;

   RARCH_LOG("[Wayland/EGL]: Current context: %p.\n", (void*)eglGetCurrentContext());

   egl_set_swap_interval(data, g_interval);

   if (fullscreen)
      wl_shell_surface_set_fullscreen(wl->g_shell_surf, WL_SHELL_SURFACE_FULLSCREEN_METHOD_DEFAULT, 0, NULL);

   flush_wayland_fd();
   return true;

error:
   gfx_ctx_wl_destroy(data);
   return false;
}

static void gfx_ctx_wl_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;
#if 0
   void *wl    = input_wayland.init();
   *input      = wl ? &input_wayland : NULL;
   *input_data = wl;
#endif
   *input = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_wl_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_wl_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return true;
}

static bool gfx_ctx_wl_has_windowed(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_wl_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   g_major = major;
   g_minor = minor;
   g_api = api;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            return false;
#endif
         return eglBindAPI(EGL_OPENGL_API);
      case GFX_CTX_OPENGL_ES_API:
#ifndef EGL_KHR_create_context
         if (major >= 3)
            return false;
#endif
         return eglBindAPI(EGL_OPENGL_ES_API);
      case GFX_CTX_OPENVG_API:
         return eglBindAPI(EGL_OPENVG_API);
      default:
         break;
   }

   return false;
}

static void keyboard_handle_keymap(void* data,
struct wl_keyboard* keyboard,
uint32_t format,
int fd,
uint32_t size)
{
   /* TODO */
}

static void keyboard_handle_enter(void* data,
struct wl_keyboard* keyboard,
uint32_t serial,
struct wl_surface* surface,
struct wl_array* keys)
{
   /* TODO */
}

static void keyboard_handle_leave(void* data,
struct wl_keyboard* keyboard,
uint32_t serial,
struct wl_surface* surface)
{
   /* TODO */
}

static void keyboard_handle_key(void* data,
struct wl_keyboard* keyboard,
uint32_t serial,
uint32_t time,
uint32_t key,
uint32_t state)
{
   /* TODO */
}

static void keyboard_handle_modifiers(void* data,
struct wl_keyboard* keyboard,
uint32_t serial,
uint32_t modsDepressed,
uint32_t modsLatched,
uint32_t modsLocked,
uint32_t group)
{
   /* TODO */
}

static const struct wl_keyboard_listener keyboard_listener = {
   keyboard_handle_keymap,
   keyboard_handle_enter,
   keyboard_handle_leave,
   keyboard_handle_key,
   keyboard_handle_modifiers,
};

static void pointer_handle_enter(void* data,
struct wl_pointer* pointer,
uint32_t serial,
struct wl_surface* surface,
wl_fixed_t sx,
wl_fixed_t sy)
{
   /* TODO */
}

static void pointer_handle_leave(void* data,
struct wl_pointer* pointer,
uint32_t serial,
struct wl_surface* surface)
{
   /* TODO */
}

static void pointer_handle_motion(void* data,
struct wl_pointer* pointer,
uint32_t time,
wl_fixed_t sx,
wl_fixed_t sy)
{
   /* TODO */
}

static void pointer_handle_button(void* data,
struct wl_pointer* wl_pointer,
uint32_t serial,
uint32_t time,
uint32_t button,
uint32_t state)
{
   /* TODO */
}

static void pointer_handle_axis(void* data,
struct wl_pointer* wl_pointer,
uint32_t time,
uint32_t axis,
wl_fixed_t value)
{
   /* TODO */
}


static const struct wl_pointer_listener pointer_listener = {
   pointer_handle_enter,
   pointer_handle_leave,
   pointer_handle_motion,
   pointer_handle_button,
   pointer_handle_axis,
};

static void seat_handle_capabilities(void *data,
struct wl_seat *seat, unsigned caps)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)
      driver->video_context_data;

   if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl->g_wl_keyboard)
   {
      wl->g_wl_keyboard = wl_seat_get_keyboard(seat);
      wl_keyboard_add_listener(wl->g_wl_keyboard, &keyboard_listener, NULL);
   }
   else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl->g_wl_keyboard)
   {
      wl_keyboard_destroy(wl->g_wl_keyboard);
      wl->g_wl_keyboard = NULL;
   }
   if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl->g_wl_pointer)
   {
      wl->g_wl_pointer = wl_seat_get_pointer(seat);
      wl_pointer_add_listener(wl->g_wl_pointer, &pointer_listener, NULL);
   }
   else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wl->g_wl_pointer)
   {
      wl_pointer_destroy(wl->g_wl_pointer);
      wl->g_wl_pointer = NULL;
   }
}

/* Seat callbacks - TODO/FIXME */
static const struct wl_seat_listener seat_listener = {
   seat_handle_capabilities,
};

const gfx_ctx_driver_t gfx_ctx_wayland = {
   gfx_ctx_wl_init,
   gfx_ctx_wl_destroy,
   gfx_ctx_wl_bind_api,
   egl_set_swap_interval,
   gfx_ctx_wl_set_video_mode,
   gfx_ctx_wl_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   gfx_ctx_wl_update_window_title,
   gfx_ctx_wl_check_window,
   gfx_ctx_wl_set_resize,
   gfx_ctx_wl_has_focus,
   gfx_ctx_wl_suppress_screensaver,
   gfx_ctx_wl_has_windowed,
   egl_swap_buffers,
   gfx_ctx_wl_input_driver,
   egl_get_proc_address,
   NULL,
   NULL,
   NULL,
   "wayland",
   egl_bind_hw_render,
};

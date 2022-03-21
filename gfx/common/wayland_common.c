/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2020 - Daniel De Matteis
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


#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include "wayland_common.h"
#include "../../frontend/frontend_driver.h"

#define SPLASH_SHM_NAME "retroarch-wayland-vk-splash"

#ifdef HAVE_LIBDECOR_H
#include <libdecor.h>
#endif

#define DEFAULT_WINDOWED_WIDTH 640
#define DEFAULT_WINDOWED_HEIGHT 480

void xdg_toplevel_handle_configure_common(gfx_ctx_wayland_data_t *wl,
      void *toplevel,
      int32_t width, int32_t height, struct wl_array *states)
{
   const uint32_t *state;

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

   wl->width  = width  > 0 ? width  : DEFAULT_WINDOWED_WIDTH;
   wl->height = height > 0 ? height : DEFAULT_WINDOWED_HEIGHT;
}

void xdg_toplevel_handle_close(void *data,
      struct xdg_toplevel *xdg_toplevel)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   command_event(CMD_EVENT_QUIT, NULL);
}

#ifdef HAVE_LIBDECOR_H
void libdecor_frame_handle_configure_common(struct libdecor_frame *frame,
      struct libdecor_configuration *configuration,
      gfx_ctx_wayland_data_t *wl)
{
   int width, height;
   struct libdecor_state *state = NULL;
   static const enum
      libdecor_window_state tiled_states = (
         LIBDECOR_WINDOW_STATE_TILED_LEFT
       | LIBDECOR_WINDOW_STATE_TILED_RIGHT
       | LIBDECOR_WINDOW_STATE_TILED_TOP
       | LIBDECOR_WINDOW_STATE_TILED_BOTTOM
   );
   enum libdecor_window_state window_state;
   bool focused   = false;
   bool tiled     = false;

   wl->fullscreen = false;
   wl->maximized  = false;

   if (wl->libdecor_configuration_get_window_state(
         configuration, &window_state))
   {
      wl->fullscreen = (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) != 0;
      wl->maximized  = (window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED) != 0;
      focused        = (window_state & LIBDECOR_WINDOW_STATE_ACTIVE) != 0;
      tiled          = (window_state & tiled_states) != 0;
   }

   if (!wl->libdecor_configuration_get_content_size(configuration, frame,
         &width, &height))
   {
      width  = wl->floating_width;
      height = wl->floating_height;
   }

   if (     width  > 0
         && height > 0)
   {
      wl->width  = width;
      wl->height = height;
   }

   state = wl->libdecor_state_new(wl->width, wl->height);
   wl->libdecor_frame_commit(frame, state, configuration);
   wl->libdecor_state_free(state);

   if (wl->libdecor_frame_is_floating(frame)) {
      wl->floating_width  = width;
      wl->floating_height = height;
   }
}

void libdecor_frame_handle_close(struct libdecor_frame *frame,
      void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   command_event(CMD_EVENT_QUIT, NULL);
}

void libdecor_frame_handle_commit(struct libdecor_frame *frame,
      void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
}
#endif


void gfx_ctx_wl_get_video_size_common(gfx_ctx_wayland_data_t *wl,
      unsigned *width, unsigned *height)
{
   if (!wl->reported_display_size) {
      wl->reported_display_size = true;
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

void gfx_ctx_wl_destroy_resources_common(gfx_ctx_wayland_data_t *wl)
{
   if (wl->input.dpy != NULL && wl->input.fd >= 0)
      close(wl->input.fd);

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

   wl->xdg_shell    = NULL;
   wl->compositor   = NULL;
   wl->registry     = NULL;
   wl->input.dpy    = NULL;
   wl->xdg_surface  = NULL;
   wl->surface      = NULL;
   wl->xdg_toplevel = NULL;

   wl->width        = 0;
   wl->height       = 0;
}

void gfx_ctx_wl_update_title_common(gfx_ctx_wayland_data_t *wl)
{
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

#ifdef HAVE_LIBDECOR_H
   if (wl->libdecor)
   {
      if (wl && title[0])
         wl->libdecor_frame_set_title(wl->libdecor_frame, title);
   }
   else
#endif
   {
      if (wl && title[0])
      {
         if (wl->deco)
            zxdg_toplevel_decoration_v1_set_mode(wl->deco,
               ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
         xdg_toplevel_set_title(wl->xdg_toplevel, title);
      }
   }
}

bool gfx_ctx_wl_get_metrics_common(gfx_ctx_wayland_data_t *wl,
      enum display_metric_types type, float *value)
{
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

bool gfx_ctx_wl_init_common(void *video_driver,
      const toplevel_listener_t *toplevel_listener, gfx_ctx_wayland_data_t **wwl)
{
   int i;
   *wwl = calloc(1, sizeof(gfx_ctx_wayland_data_t));
   gfx_ctx_wayland_data_t *wl = *wwl;

   if (!wl)
      return false;


#ifdef HAVE_LIBDECOR_H
#ifdef HAVE_DYNAMIC
   wl->libdecor = dylib_load("libdecor-0.so");
   if (wl->libdecor)
   {
#define RA_WAYLAND_SYM(rc,fn,params) wl->fn = (rc (*) params)dylib_proc(wl->libdecor, #fn);
#include "wayland/libdecor_sym.h"
   }
#endif
#endif

   wl_list_init(&wl->all_outputs);

   frontend_driver_destroy_signal_handler_state();

   wl->input.dpy         = wl_display_connect(NULL);
   wl->last_buffer_scale = 1;
   wl->buffer_scale      = 1;
   wl->floating_width    = DEFAULT_WINDOWED_WIDTH;
   wl->floating_height   = DEFAULT_WINDOWED_HEIGHT;

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
      RARCH_LOG("[Wayland]: Compositor doesn't support zwp_idle_inhibit_manager_v1 protocol\n");
   }

   if (!wl->deco_manager)
   {
      RARCH_LOG("[Wayland]: Compositor doesn't support zxdg_decoration_manager_v1 protocol\n");
   }

   wl->surface = wl_compositor_create_surface(wl->compositor);

   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);
   wl_surface_add_listener(wl->surface, &wl_surface_listener, wl);

#ifdef HAVE_LIBDECOR_H
   if (wl->libdecor)
   {
      wl->libdecor_context = wl->libdecor_new(wl->input.dpy, &libdecor_interface);
      if (wl->libdecor_context)
      {
         wl->libdecor_frame = wl->libdecor_decorate(wl->libdecor_context, wl->surface, &toplevel_listener->libdecor_frame_interface, wl);
         if (!wl->libdecor_frame)
         {
            RARCH_ERR("[Wayland]: Failed to crate libdecor frame\n");
            goto error;
         }

         wl->libdecor_frame_set_app_id(wl->libdecor_frame, "retroarch");
         wl->libdecor_frame_set_title(wl->libdecor_frame, "RetroArch");
         wl->libdecor_frame_map(wl->libdecor_frame);
      }

      /* Waiting for libdecor to be configured before starting to draw */
      wl_surface_commit(wl->surface);
      wl->configured = true;

      while (wl->configured)
      {
         if (wl->libdecor_dispatch(wl->libdecor_context, 0) < 0)
         {
            RARCH_ERR("[Wayland]: libdecor failed to dispatch\n");
            goto error;
         }
      }
   }
   else
#endif
   {
      wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->xdg_shell, wl->surface);
      xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);

      wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
      xdg_toplevel_add_listener(wl->xdg_toplevel, &toplevel_listener->xdg_toplevel_listener, wl);

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
   }

   wl_display_roundtrip(wl->input.dpy);
   xdg_wm_base_add_listener(wl->xdg_shell, &xdg_shell_listener, NULL);

   /* Bind SHM based wl_buffer to wl_surface until the vulkan surface is ready.
    * This shows the window which assigns us a display (wl_output)
    *  which is usefull for HiDPI and auto selecting a display for fullscreen. */
   if (!draw_splash_screen(wl))
      RARCH_ERR("[Wayland`]: Failed to draw splash screen\n");

   wl_display_roundtrip(wl->input.dpy);

   wl->input.fd = wl_display_get_fd(wl->input.dpy);

   wl->input.keyboard_focus  = true;
   wl->input.mouse.focus     = true;

   wl->cursor.surface        = wl_compositor_create_surface(wl->compositor);
   wl->cursor.theme          = wl_cursor_theme_load(NULL, 16, wl->shm);
   wl->cursor.default_cursor = wl_cursor_theme_get_cursor(wl->cursor.theme, "left_ptr");

   wl->num_active_touches    = 0;

   for (i = 0;i < MAX_TOUCHES;i++)
   {
       wl->active_touch_positions[i].active = false;
       wl->active_touch_positions[i].id     = -1;
       wl->active_touch_positions[i].x      = (unsigned) 0;
       wl->active_touch_positions[i].y      = (unsigned) 0;
   }

   flush_wayland_fd(&wl->input);

   return true;

error:
   return false;
}


bool gfx_ctx_wl_set_video_mode_common_size(gfx_ctx_wayland_data_t *wl,
      unsigned width, unsigned height)
{
   settings_t *settings         = config_get_ptr();
   unsigned video_monitor_index = settings->uints.video_monitor_index;

   wl->width                    = width  ? width  : DEFAULT_WINDOWED_WIDTH;
   wl->height                   = height ? height : DEFAULT_WINDOWED_HEIGHT;

   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);

#ifdef HAVE_LIBDECOR_H
   if (wl->libdecor)
   {
     struct libdecor_state *state = wl->libdecor_state_new(width, height);
     wl->libdecor_frame_commit(wl->libdecor_frame, state, NULL);
     wl->libdecor_state_free(state);
   }
#endif

   return true;

error:
   return false;
}

bool gfx_ctx_wl_set_video_mode_common_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen)
{
   settings_t *settings         = config_get_ptr();
   unsigned video_monitor_index = settings->uints.video_monitor_index;

   if (fullscreen)
   {
      struct wl_output *output = NULL;
      int output_i             = 0;
      output_info_t *oi, *tmp;

      if (video_monitor_index <= 0 && wl->current_output != NULL)
      {
         oi = wl->current_output;
         output = oi->output;
         RARCH_LOG("[Wayland]: Auto fullscreen on display \"%s\" \"%s\"\n", oi->make, oi->model);
      }
      else wl_list_for_each_safe(oi, tmp, &wl->all_outputs, link)
      {
         if (++output_i == video_monitor_index)
         {
            output = oi->output;
            RARCH_LOG("[Wayland]: Fullscreen on display %i \"%s\" \"%s\"\n", output_i, oi->make, oi->model);
            break;
         }
      }

      if (output == NULL)
         RARCH_LOG("[Wayland] Failed to specify monitor for fullscreen, letting compositor decide\n");

#ifdef HAVE_LIBDECOR_H
      if (wl->libdecor)
         wl->libdecor_frame_set_fullscreen(wl->libdecor_frame, output);
      else
#endif
      {
         xdg_toplevel_set_fullscreen(wl->xdg_toplevel, output);
      }
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

error:
   return false;
}

bool gfx_ctx_wl_suppress_screensaver(void *data, bool state)
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

float gfx_ctx_wl_get_refresh_rate(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl || !wl->current_output)
      return false;

   return (float) wl->current_output->refresh_rate / 1000.0f;
}

bool gfx_ctx_wl_has_focus(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return wl->input.keyboard_focus;
}

void gfx_ctx_wl_check_window_common(gfx_ctx_wayland_data_t *wl,
      void (*get_video_size)(void*, unsigned*, unsigned*), bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   /* this function works with SCALED sizes, it's used from the renderer */
   unsigned new_width, new_height;

   flush_wayland_fd(&wl->input);

   new_width  = *width  * wl->last_buffer_scale;
   new_height = *height * wl->last_buffer_scale;

   get_video_size(wl, &new_width, &new_height);

   if (     new_width  != *width  * wl->last_buffer_scale
         || new_height != *height * wl->last_buffer_scale)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;

      wl->last_buffer_scale = wl->buffer_scale;
   }

   *quit = (bool)frontend_driver_get_signal_handler_state();
}

static void shm_buffer_handle_release(void *data,
   struct wl_buffer *wl_buffer)
{
   shm_buffer_t *buffer = data;

   wl_buffer_destroy(buffer->wl_buffer);
   munmap(buffer->data, buffer->data_size);
   free(buffer);
}

static void xdg_surface_handle_configure(void *data, struct xdg_surface *surface,
                                  uint32_t serial)
{
   xdg_surface_ack_configure(surface, serial);
}

#ifdef HAVE_LIBDECOR_H
static void libdecor_handle_error(struct libdecor *context,
      enum libdecor_error error, const char *message)
{
   RARCH_ERR("[Wayland]: libdecor Caught error (%d): %s\n", error, message);
}
#endif

int create_shm_file(off_t size)
{
   int fd;

   int ret;

   #ifdef HAVE_MEMFD_CREATE
   fd = memfd_create(SPLASH_SHM_NAME, MFD_CLOEXEC | MFD_ALLOW_SEALING);

   if (fd >= 0) {
      fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);

      do {
         ret = posix_fallocate(fd, 0, size);
      } while (ret == EINTR);
      if (ret != 0) {
         close(fd);
         errno = ret;
         fd = -1;
      }
   }
   if (fd < 0)
   #endif
   {
      for (unsigned retry_count = 0; retry_count < 100; retry_count++)
      {
         char *name;
         if (asprintf (&name, "%s-%02d", SPLASH_SHM_NAME, retry_count) < 0)
            continue;
         fd = shm_open(name, O_RDWR | O_CREAT, 0600);
         if (fd >= 0)
         {
            shm_unlink(name);
            free(name);
            ftruncate(fd, size);
            break;
         }
         free(name);
      }
   }

   return fd;
}

shm_buffer_t *create_shm_buffer(gfx_ctx_wayland_data_t *wl, int width,
   int height,
   uint32_t format)
{
   struct wl_shm_pool *pool;
   int fd, size, stride, ofd;
   void *data;
   shm_buffer_t *buffer;

   stride = width * 4;
   size = stride * height;

   if (size <= 0)
      return NULL;

   fd = create_shm_file(size);
   if (fd < 0) {
      RARCH_ERR("[Wayland] [SHM]: Creating a buffer file for %d B failed: %s\n",
         size, strerror(errno));
      return NULL;
   }

   data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (data == MAP_FAILED) {
      RARCH_ERR("[Wayland] [SHM]: mmap failed: %s\n", strerror(errno));
      close(fd);
      return NULL;
   }

   buffer = calloc(1, sizeof *buffer);

   pool = wl_shm_create_pool(wl->shm, fd, size);
   buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0,
      width, height,
      stride, format);
   wl_buffer_add_listener(buffer->wl_buffer, &shm_buffer_listener, buffer);
   wl_shm_pool_destroy(pool);

   close(fd);

   buffer->data = data;
   buffer->data_size = size;

   return buffer;
}


void shm_buffer_paint_checkerboard(shm_buffer_t *buffer,
      int width, int height, int scale,
      size_t chk, uint32_t bg, uint32_t fg)
{
   uint32_t *pixels = buffer->data;
   uint32_t color;
   int y, x, sx, sy;
   size_t off;
   int stride = width * scale;

   for (y = 0; y < height; y++) {
      for (x = 0; x < width; x++) {
         color = (x & chk) ^ (y & chk) ? fg : bg;
         for (sx = 0; sx < scale; sx++) {
            for (sy = 0; sy < scale; sy++) {
               off = x * scale + sx
                     + (y * scale + sy) * stride;
               pixels[off] = color;
            }
         }
      }
   }
}


bool draw_splash_screen(gfx_ctx_wayland_data_t *wl)
{
   shm_buffer_t *buffer;

   buffer = create_shm_buffer(wl,
      wl->width * wl->buffer_scale,
      wl->height * wl->buffer_scale,
      WL_SHM_FORMAT_XRGB8888);

   if (buffer == NULL)
     return false;

   shm_buffer_paint_checkerboard(buffer, wl->width,
      wl->height, wl->buffer_scale,
      16, 0xffbcbcbc, 0xff8e8e8e);

   wl_surface_attach(wl->surface, buffer->wl_buffer, 0, 0);
   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);
   if (wl_surface_get_version(wl->surface) >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
      wl_surface_damage_buffer(wl->surface, 0, 0,
         wl->width * wl->buffer_scale,
         wl->height * wl->buffer_scale);
   wl_surface_commit(wl->surface);
   return true;
}


const struct wl_buffer_listener shm_buffer_listener = {
   shm_buffer_handle_release,
};

#ifdef HAVE_LIBDECOR_H
const struct libdecor_interface libdecor_interface = {
   .error = libdecor_handle_error,
};
#endif

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

// Needed for memfd_create
#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* See feature_test_macros(7) */
#endif

#include <stdint.h>
#include <string.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <poll.h>
#include <unistd.h>

#include <string/stdstring.h>

#ifdef HAVE_LIBDECOR_H
#include <libdecor.h>
#endif

#include "wayland_common.h"

#include "../input_keymaps.h"
#include "../../frontend/frontend_driver.h"

#define DND_ACTION WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE
#define FILE_MIME "text/uri-list"
#define TEXT_MIME "text/plain;charset=utf-8"
#define PIPE_MS_TIMEOUT 10

#define IOR_READ 0x1
#define IOR_WRITE 0x2
#define IOR_NO_RETRY 0x4

#define SPLASH_SHM_NAME "retroarch-wayland-splash"

static void keyboard_handle_keymap(void* data,
      struct wl_keyboard* keyboard,
      uint32_t format,
      int fd,
      uint32_t size)
{
   if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
   {
      close(fd);
      return;
   }

#ifdef HAVE_XKBCOMMON
   init_xkb(fd, size);
#endif
   close(fd);
}

static void keyboard_handle_enter(void* data,
      struct wl_keyboard* keyboard,
      uint32_t serial,
      struct wl_surface* surface,
      struct wl_array* keys)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   wl->input.keyboard_focus   = true;
}

static void keyboard_handle_leave(void *data,
      struct wl_keyboard *keyboard,
      uint32_t serial,
      struct wl_surface *surface)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   wl->input.keyboard_focus   = false;

   // Release all keys
   memset(wl->input.key_state, 0, sizeof(wl->input.key_state));
}

static void keyboard_handle_key(void *data,
      struct wl_keyboard *keyboard,
      uint32_t serial,
      uint32_t time,
      uint32_t key,
      uint32_t state)
{
   int value                  = 1;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   uint32_t keysym            = key;

   /* Handle 'duplicate' inputs that correspond
    * to the same RETROK_* key */
   switch (key)
   {
      case KEY_OK:
      case KEY_SELECT:
         keysym = KEY_ENTER;
      case KEY_EXIT:
         keysym = KEY_CLEAR;
      default:
         break;
   }

   if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
   {
      BIT_SET(wl->input.key_state, keysym);
      value = 1;
   }
   else if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
   {
      BIT_CLEAR(wl->input.key_state, keysym);
      value = 0;
   }

#ifdef HAVE_XKBCOMMON
   if (handle_xkb(keysym, value) == 0)
      return;
#endif
   input_keyboard_event(value,
			input_keymaps_translate_keysym_to_rk(keysym),
         0, 0, RETRO_DEVICE_KEYBOARD);
}

static void keyboard_handle_modifiers(void *data,
      struct wl_keyboard *keyboard,
      uint32_t serial,
      uint32_t modsDepressed,
      uint32_t modsLatched,
      uint32_t modsLocked,
      uint32_t group)
{
#ifdef HAVE_XKBCOMMON
   handle_xkb_state_mask(modsDepressed, modsLatched, modsLocked, group);
#endif
}

void keyboard_handle_repeat_info(void *data,
      struct wl_keyboard *wl_keyboard,
      int32_t rate,
      int32_t delay)
{
   /* TODO: Seems like we'll need this to get
    * repeat working. We'll have to do it on our own. */
}

void gfx_ctx_wl_show_mouse(void *data, bool state)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (!wl->wl_pointer)
      return;

   if (state)
   {
      struct wl_cursor_image *image = wl->cursor.default_cursor->images[0];
      wl_pointer_set_cursor(wl->wl_pointer, wl->cursor.serial, wl->cursor.surface, image->hotspot_x, image->hotspot_y);
      wl_surface_attach(wl->cursor.surface, wl_cursor_image_get_buffer(image), 0, 0);
      wl_surface_damage(wl->cursor.surface, 0, 0, image->width, image->height);
      wl_surface_commit(wl->cursor.surface);
   }
   else
      wl_pointer_set_cursor(wl->wl_pointer, wl->cursor.serial, NULL, 0, 0);

   wl->cursor.visible = state;
}

static void pointer_handle_enter(void *data,
      struct wl_pointer *pointer,
      uint32_t serial,
      struct wl_surface *surface,
      wl_fixed_t sx,
      wl_fixed_t sy)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   wl->input.mouse.surface = surface;
   wl->input.mouse.last_x  = wl_fixed_to_int(sx * (wl_fixed_t)wl->buffer_scale);
   wl->input.mouse.last_y  = wl_fixed_to_int(sy * (wl_fixed_t)wl->buffer_scale);
   wl->input.mouse.x       = wl->input.mouse.last_x;
   wl->input.mouse.y       = wl->input.mouse.last_y;
   wl->input.mouse.focus   = true;
   wl->cursor.serial       = serial;

   gfx_ctx_wl_show_mouse(data, wl->cursor.visible);
}

static void pointer_handle_leave(void *data,
      struct wl_pointer *pointer,
      uint32_t serial,
      struct wl_surface *surface)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   wl->input.mouse.focus      = false;
   wl->input.mouse.left       = false;
   wl->input.mouse.right      = false;
   wl->input.mouse.middle     = false;

   if (wl->input.mouse.surface == surface)
      wl->input.mouse.surface = NULL;
}

static void pointer_handle_motion(void *data,
      struct wl_pointer *pointer,
      uint32_t time,
      wl_fixed_t sx,
      wl_fixed_t sy)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   wl->input.mouse.x          = wl_fixed_to_int(
         (wl_fixed_t)wl->buffer_scale * sx);
   wl->input.mouse.y          = wl_fixed_to_int(
         (wl_fixed_t)wl->buffer_scale * sy);
}

static void pointer_handle_button(void *data,
      struct wl_pointer *wl_pointer,
      uint32_t serial,
      uint32_t time,
      uint32_t button,
      uint32_t state)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->input.mouse.surface != wl->surface)
      return;

   if (state == WL_POINTER_BUTTON_STATE_PRESSED)
   {
      switch (button)
      {
         case BTN_LEFT:
            wl->input.mouse.left = true;

            if (BIT_GET(wl->input.key_state, KEY_LEFTALT))
            {
#ifdef HAVE_LIBDECOR_H
               if (wl->libdecor)
                   wl->libdecor_frame_move(wl->libdecor_frame, wl->seat, serial);
               else
#endif
               {
                  xdg_toplevel_move(wl->xdg_toplevel, wl->seat, serial);
               }
            }
            break;
         case BTN_RIGHT:
            wl->input.mouse.right = true;
            break;
         case BTN_MIDDLE:
            wl->input.mouse.middle = true;
            break;
      }
   }
   else
   {
      switch (button)
      {
         case BTN_LEFT:
            wl->input.mouse.left = false;
            break;
         case BTN_RIGHT:
            wl->input.mouse.right = false;
            break;
         case BTN_MIDDLE:
            wl->input.mouse.middle = false;
            break;
      }
   }
}

static void pointer_handle_axis(void *data,
      struct wl_pointer *wl_pointer,
      uint32_t time,
      uint32_t axis,
      wl_fixed_t value) {
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   double dvalue = wl_fixed_to_double(value);
   switch (axis) {
      case WL_POINTER_AXIS_VERTICAL_SCROLL:
         if (dvalue < 0) {
            wl->input.mouse.wu = true;
         } else if (dvalue > 0) {
            wl->input.mouse.wd = true;
         }
         break;
      case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
         if (dvalue < 0) {
            wl->input.mouse.wl = true;
         } else if (dvalue > 0) {
            wl->input.mouse.wr = true;
         }
         break;
   }
}

static void touch_handle_down(void *data,
      struct wl_touch *wl_touch,
      uint32_t serial,
      uint32_t time,
      struct wl_surface *surface,
      int32_t id,
      wl_fixed_t x,
      wl_fixed_t y)
{
   int i;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->num_active_touches < MAX_TOUCHES)
   {
      for (i = 0; i < MAX_TOUCHES; i++)
      {
         /* Use next empty slot */
         if (!wl->active_touch_positions[i].active)
         {
            wl->active_touch_positions[wl->num_active_touches].active = true;
            wl->active_touch_positions[wl->num_active_touches].id     = id;
            wl->active_touch_positions[wl->num_active_touches].x      = (unsigned)
               wl_fixed_to_int(x);
            wl->active_touch_positions[wl->num_active_touches].y      = (unsigned)
               wl_fixed_to_int(y);
            wl->num_active_touches++;
            break;
         }
      }
   }
}

static void reorder_touches(gfx_ctx_wayland_data_t *wl)
{
   int i, j;
   if (wl->num_active_touches == 0)
      return;

   for (i = 0; i < MAX_TOUCHES; i++)
   {
      if (!wl->active_touch_positions[i].active)
      {
         for (j=i+1; j<MAX_TOUCHES; j++)
         {
            if (wl->active_touch_positions[j].active)
            {
               wl->active_touch_positions[i].active =
                  wl->active_touch_positions[j].active;
               wl->active_touch_positions[i].id     =
                  wl->active_touch_positions[j].id;
               wl->active_touch_positions[i].x      = wl->active_touch_positions[j].x;
               wl->active_touch_positions[i].y      = wl->active_touch_positions[j].y;
               wl->active_touch_positions[j].active = false;
               wl->active_touch_positions[j].id     = -1;
               wl->active_touch_positions[j].x      = (unsigned) 0;
               wl->active_touch_positions[j].y      = (unsigned) 0;
               break;
            }

            if (j == MAX_TOUCHES)
               return;
         }
      }
   }
}

static void touch_handle_up(void *data,
      struct wl_touch *wl_touch,
      uint32_t serial,
      uint32_t time,
      int32_t id)
{
   int i;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   for (i = 0; i < MAX_TOUCHES; i++)
   {
      if (  wl->active_touch_positions[i].active &&
            wl->active_touch_positions[i].id == id)
      {
         wl->active_touch_positions[i].active = false;
         wl->active_touch_positions[i].id     = -1;
         wl->active_touch_positions[i].x      = (unsigned)0;
         wl->active_touch_positions[i].y      = (unsigned)0;
         wl->num_active_touches--;
      }
   }
   reorder_touches(wl);
}

static void touch_handle_motion(void *data,
      struct wl_touch *wl_touch,
      uint32_t time,
      int32_t id,
      wl_fixed_t x,
      wl_fixed_t y)
{
   int i;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   for (i = 0; i < MAX_TOUCHES; i++)
   {
      if (  wl->active_touch_positions[i].active &&
            wl->active_touch_positions[i].id == id)
      {
         wl->active_touch_positions[i].x = (unsigned) wl_fixed_to_int(x);
         wl->active_touch_positions[i].y = (unsigned) wl_fixed_to_int(y);
      }
   }
}

static void touch_handle_frame(void *data,
      struct wl_touch *wl_touch) { }

static void touch_handle_cancel(void *data,
      struct wl_touch *wl_touch)
{
   /* If i understand the spec correctly we have to reset all touches here
    * since they were not ment for us anyway */
   int i;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   for (i = 0; i < MAX_TOUCHES; i++)
   {
      wl->active_touch_positions[i].active = false;
      wl->active_touch_positions[i].id     = -1;
      wl->active_touch_positions[i].x      = (unsigned) 0;
      wl->active_touch_positions[i].y      = (unsigned) 0;
   }

   wl->num_active_touches = 0;
}

static void seat_handle_capabilities(void *data,
      struct wl_seat *seat, unsigned caps)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !wl->wl_keyboard)
   {
      wl->wl_keyboard = wl_seat_get_keyboard(seat);
      wl_keyboard_add_listener(wl->wl_keyboard, &keyboard_listener, wl);
   }
   else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && wl->wl_keyboard)
   {
      wl_keyboard_destroy(wl->wl_keyboard);
      wl->wl_keyboard = NULL;
   }
   if ((caps & WL_SEAT_CAPABILITY_POINTER) && !wl->wl_pointer)
   {
      wl->wl_pointer = wl_seat_get_pointer(seat);
      wl_pointer_add_listener(wl->wl_pointer, &pointer_listener, wl);
   }
   else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && wl->wl_pointer)
   {
      wl_pointer_destroy(wl->wl_pointer);
      wl->wl_pointer = NULL;
   }
   if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !wl->wl_touch)
   {
      wl->wl_touch = wl_seat_get_touch(seat);
      wl_touch_add_listener(wl->wl_touch, &touch_listener, wl);
   }
   else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && wl->wl_touch)
   {
      wl_touch_destroy(wl->wl_touch);
      wl->wl_touch = NULL;
   }

}

static void seat_handle_name(void *data,
      struct wl_seat *seat, const char *name) { }

/* Surface callbacks. */

static void wl_surface_enter(void *data, struct wl_surface *wl_surface,
      struct wl_output *output)
{
    output_info_t *oi;
    gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

    wl->input.mouse.surface = wl_surface;

    /* TODO: track all outputs the surface is on, pick highest scale */

    wl_list_for_each(oi, &wl->all_outputs, link)
    {
       if (oi->output == output)
       {
          wl->current_output    = oi;
          wl->last_buffer_scale = wl->buffer_scale;
          wl->buffer_scale      = oi->scale;
          break;
       }
    };
}

static void wl_nop(void *a, struct wl_surface *b, struct wl_output *c) { }

/* Shell surface callbacks. */
static void xdg_shell_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static void xdg_surface_handle_configure(void *data, struct xdg_surface *surface,
                                  uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);
}

static void display_handle_geometry(void *data,
      struct wl_output *output,
      int x, int y,
      int physical_width, int physical_height,
      int subpixel,
      const char *make,
      const char *model,
      int transform)
{
   output_info_t *oi          = (output_info_t*)data;
   oi->physical_width         = physical_width;
   oi->physical_height        = physical_height;
   oi->make                   = strdup(make);
   oi->model                  = strdup(model);
}

static void display_handle_mode(void *data,
      struct wl_output *output,
      uint32_t flags,
      int width,
      int height,
      int refresh)
{
   (void)output;
   (void)flags;

   output_info_t *oi          = (output_info_t*)data;
   oi->width                  = width;
   oi->height                 = height;
   oi->refresh_rate           = refresh;
}

static void display_handle_done(void *data,
      struct wl_output *output) { }

static void display_handle_scale(void *data,
      struct wl_output *output,
      int32_t factor)
{
   output_info_t *oi = (output_info_t*)data;
   oi->scale = factor;
}

bool setup_data_device(gfx_ctx_wayland_data_t *wl)
{
   if (wl->data_device == NULL && wl->data_device_manager != NULL && wl->seat != NULL)
   {
      wl->data_device = wl_data_device_manager_get_data_device(wl->data_device_manager, wl->seat);
      if (wl->data_device != NULL)
      {
         wl_data_device_add_listener(wl->data_device, &data_device_listener, wl);
         return true;
      }
   }
   return false;
}

/* Registry callbacks. */
static void registry_handle_global(void *data, struct wl_registry *reg,
      uint32_t id, const char *interface, uint32_t version)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   RARCH_DBG("[Wayland]: Add global %u, interface %s, version %u\n", id, interface, version);

   if (string_is_equal(interface, wl_compositor_interface.name))
      wl->compositor = (struct wl_compositor*)wl_registry_bind(reg,
            id, &wl_compositor_interface, MIN(version, 4));
   else if (string_is_equal(interface, wl_output_interface.name))
   {
      output_info_t *oi = (output_info_t*)
         calloc(1, sizeof(output_info_t));

      oi->global_id     = id;
      oi->output        = (struct wl_output*)wl_registry_bind(reg,
            id, &wl_output_interface, MIN(version, 2));
      wl_output_add_listener(oi->output, &output_listener, oi);
      wl_list_insert(&wl->all_outputs, &oi->link);
      wl_display_roundtrip(wl->input.dpy);
   }
   else if (string_is_equal(interface, xdg_wm_base_interface.name))
      wl->xdg_shell = (struct xdg_wm_base*)
         wl_registry_bind(reg, id, &xdg_wm_base_interface, MIN(version, 3));
   else if (string_is_equal(interface, wl_shm_interface.name))
      wl->shm = (struct wl_shm*)wl_registry_bind(reg, id, &wl_shm_interface, MIN(version, 1));
   else if (string_is_equal(interface, wl_seat_interface.name))
   {
      wl->seat = (struct wl_seat*)wl_registry_bind(reg, id, &wl_seat_interface, MIN(version, 2));
      wl_seat_add_listener(wl->seat, &seat_listener, wl);
      setup_data_device(wl);
   }
   else if (string_is_equal(interface, wl_data_device_manager_interface.name))
   {
      wl->data_device_manager = (struct wl_data_device_manager*)wl_registry_bind(
                                 reg, id, &wl_data_device_manager_interface, MIN(version, 3));
      setup_data_device(wl);
   }
   else if (string_is_equal(interface, zwp_idle_inhibit_manager_v1_interface.name))
      wl->idle_inhibit_manager = (struct zwp_idle_inhibit_manager_v1*)wl_registry_bind(
                                  reg, id, &zwp_idle_inhibit_manager_v1_interface, MIN(version, 1));
   else if (string_is_equal(interface, zxdg_decoration_manager_v1_interface.name))
      wl->deco_manager = (struct zxdg_decoration_manager_v1*)wl_registry_bind(
                                  reg, id, &zxdg_decoration_manager_v1_interface, MIN(version, 1));
}

static void registry_handle_global_remove(void *data,
      struct wl_registry *registry, uint32_t id)
{
   output_info_t *oi, *tmp;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   wl_list_for_each_safe(oi, tmp, &wl->all_outputs, link)
   {
      if (oi->global_id == id)
      {
         wl_list_remove(&oi->link);
         free(oi);
         break;
      }
   }
}


int ioready(int fd, int flags, int timeoutMS)
{
   int result;

   do
   {
      struct pollfd info;
      info.fd = fd;
      info.events = 0;
      if (flags & IOR_READ)
         info.events |= POLLIN | POLLPRI;
      if (flags & IOR_WRITE)
         info.events |= POLLOUT;
      result = poll(&info, 1, timeoutMS);
   } while ( result < 0 && errno == EINTR && !(flags & IOR_NO_RETRY));

   return result;
}

static ssize_t read_pipe(int fd, void** buffer, size_t* total_length, bool null_terminate)
{
   int ready = 0;
   void* output_buffer = NULL;
   char temp[PIPE_BUF];
   size_t new_buffer_length = 0;
   ssize_t bytes_read = 0;
   size_t pos = 0;

   ready = ioready(fd, IOR_READ, PIPE_MS_TIMEOUT);

   if (ready == 0)
   {
      bytes_read = -1;
      RARCH_WARN("[Wayland]: Pipe timeout\n");
   }
   else if (ready < 0)
   {
      bytes_read = -1;
      RARCH_WARN("[Wayland]: Pipe select error");
   }
   else
      bytes_read = read(fd, temp, sizeof(temp));

   if (bytes_read > 0)
   {
      pos = *total_length;
      *total_length += bytes_read;

      if (null_terminate == true)
         new_buffer_length = *total_length + 1;
      else
          new_buffer_length = *total_length;

      if (*buffer == NULL)
         output_buffer = malloc(new_buffer_length);
      else
         output_buffer = realloc(*buffer, new_buffer_length);
      if (output_buffer == NULL)
         RARCH_WARN("[Wayland]: Out of memory for pipe read\n");
      else
      {
         memcpy((uint8_t*)output_buffer + pos, temp, bytes_read);

         if (null_terminate == true)
            memset((uint8_t*)output_buffer + (new_buffer_length - 1), 0, 1);

         *buffer = output_buffer;
      }
   }

   return bytes_read;
}

void* wayland_data_offer_receive(struct wl_display *display, struct wl_data_offer *offer, size_t *length,
   const char* mime_type, bool null_terminate)
{
   int pipefd[2];
   void *buffer = NULL;
   *length = 0;

   if (offer == NULL)
      RARCH_WARN("[Wayland]: Invalid data offer\n");
   else if (pipe2(pipefd, O_CLOEXEC|O_NONBLOCK) == -1)
      RARCH_WARN("[Wayland]: Could not read pipe");
   else
   {
      wl_data_offer_receive(offer, mime_type, pipefd[1]);

      // Wait for sending client to transfer
      wl_display_roundtrip(display);

      close(pipefd[1]);

      while (read_pipe(pipefd[0], &buffer, length, null_terminate) > 0);
      close(pipefd[0]);
   }
   return buffer;
}


static void data_device_handle_data_offer(void *data,
      struct wl_data_device *data_device, struct wl_data_offer *offer)
{
   data_offer_ctx *offer_data;

   offer_data = calloc(1, sizeof *offer_data);
   offer_data->offer = offer;
   offer_data->data_device = data_device;
   offer_data->dropped = false;

   wl_data_offer_set_user_data(offer, offer_data);
   wl_data_offer_add_listener(offer, &data_offer_listener, offer_data);
}

static void data_device_handle_enter(void *data,
      struct wl_data_device *data_device, uint32_t serial,
      struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y,
      struct wl_data_offer *offer)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   data_offer_ctx *offer_data;
   enum wl_data_device_manager_dnd_action dnd_action = WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE;

   if (offer == NULL)
      return;

   offer_data = wl_data_offer_get_user_data(offer);
   wl->current_drag_offer = offer_data;

   wl_data_offer_accept(offer, serial,
      offer_data->is_file_mime_type ? FILE_MIME : NULL);

   if (offer_data->is_file_mime_type && offer_data->supported_actions & DND_ACTION)
      dnd_action = DND_ACTION;

   if (wl_data_offer_get_version(offer) >= WL_DATA_OFFER_SET_ACTIONS_SINCE_VERSION)
     wl_data_offer_set_actions(offer, dnd_action, dnd_action);
}

static void data_device_handle_leave(void *data,
      struct wl_data_device *data_device)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   data_offer_ctx *offer_data = wl->current_drag_offer;

   if (offer_data != NULL && !offer_data->dropped)
   {
      wl->current_drag_offer = NULL;
      wl_data_offer_destroy(offer_data->offer);
      free(offer_data);
   }
}

static void data_device_handle_motion(void *data,
      struct wl_data_device *data_device, uint32_t time,
      wl_fixed_t x, wl_fixed_t y) { }

static void data_device_handle_drop(void *data,
      struct wl_data_device *data_device)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   data_offer_ctx *offer_data = wl->current_drag_offer;

   offer_data->dropped = true;

   if (offer_data == NULL)
      return;

   int pipefd[2];
   pipe(pipefd);
   void *buffer;
   size_t length;

   buffer = wayland_data_offer_receive(wl->input.dpy, offer_data->offer, &length, FILE_MIME, false);

   close(pipefd[1]);

   size_t len = 0;
   ssize_t read = 0;

   char *line = NULL;

   char file_list[512][512] = { 0 };
   char file_list_i = 0;

   close(pipefd[0]);

   wl->current_drag_offer = NULL;
   if (wl_data_offer_get_version(offer_data->offer) >= WL_DATA_OFFER_FINISH_SINCE_VERSION)
      wl_data_offer_finish(offer_data->offer);
   wl_data_offer_destroy(offer_data->offer);
   free(offer_data);

   FILE *stream = fmemopen(buffer, length, "r");

   if (stream == NULL)
   {
      RARCH_WARN("[Wayland]: Failed to open DnD buffer\n");
      return;
   }

   RARCH_WARN("[Wayland]: Files opp:\n");
   while ((read = getline(&line,  &len, stream)) != -1)
   {
      line[strcspn(line, "\r\n")] = 0;
      RARCH_LOG("[Wayland]: > \"%s\"\n", line);

      // TODO: Convert from file:// URI, Implement file loading
      //if (wayland_load_content_from_drop(g_filename_from_uri(line, NULL, NULL)))
      //   RARCH_WARN("----- wayland_load_content_from_drop success\n");
   }

   fclose(stream);
   free(buffer);
}

static void data_device_handle_selection(void *data,
      struct wl_data_device *data_device, struct wl_data_offer *offer) { }


static void data_offer_handle_offer(void *data, struct wl_data_offer *offer,
      const char *mime_type)
{
   data_offer_ctx *offer_data = data;

   // TODO: Keep list of mime types for offer if beneficial
   if (string_is_equal(mime_type, FILE_MIME))
      offer_data->is_file_mime_type = true;
}

static void data_offer_handle_source_actions(void *data,
      struct wl_data_offer *offer, enum wl_data_device_manager_dnd_action actions)
{
   // Report of actions for this offer supported by compositor
   data_offer_ctx *offer_data = data;
   offer_data->supported_actions = actions;
}

static void data_offer_handle_action(void *data,
      struct wl_data_offer *offer,
      enum wl_data_device_manager_dnd_action dnd_action) { }

const struct wl_registry_listener registry_listener = {
   registry_handle_global,
   registry_handle_global_remove,
};


const struct wl_output_listener output_listener = {
   display_handle_geometry,
   display_handle_mode,
   display_handle_done,
   display_handle_scale,
};

const struct xdg_wm_base_listener xdg_shell_listener = {
    xdg_shell_ping,
};

const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_handle_configure,
};

const struct wl_surface_listener wl_surface_listener = {
    wl_surface_enter,
    wl_nop,
};

const struct wl_seat_listener seat_listener = {
   seat_handle_capabilities,
   seat_handle_name,
};

const struct wl_touch_listener touch_listener = {
   touch_handle_down,
   touch_handle_up,
   touch_handle_motion,
   touch_handle_frame,
   touch_handle_cancel,
};

const struct wl_keyboard_listener keyboard_listener = {
   keyboard_handle_keymap,
   keyboard_handle_enter,
   keyboard_handle_leave,
   keyboard_handle_key,
   keyboard_handle_modifiers,
   keyboard_handle_repeat_info
};

const struct wl_pointer_listener pointer_listener = {
   pointer_handle_enter,
   pointer_handle_leave,
   pointer_handle_motion,
   pointer_handle_button,
   pointer_handle_axis,
};

const struct wl_data_device_listener data_device_listener = {
   .data_offer = data_device_handle_data_offer,
   .enter = data_device_handle_enter,
   .leave = data_device_handle_leave,
   .motion = data_device_handle_motion,
   .drop = data_device_handle_drop,
   .selection = data_device_handle_selection,
};

const struct wl_data_offer_listener data_offer_listener = {
   .offer = data_offer_handle_offer,
   .source_actions = data_offer_handle_source_actions,
   .action = data_offer_handle_action,
};

void flush_wayland_fd(void *data)
{
   struct pollfd fd = {0};
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;

   wl_display_dispatch_pending(wl->dpy);
   wl_display_flush(wl->dpy);

   fd.fd     = wl->fd;
   fd.events = POLLIN | POLLOUT | POLLERR | POLLHUP;

   if (poll(&fd, 1, 0) > 0)
   {
      if (fd.revents & (POLLERR | POLLHUP))
      {
         close(wl->fd);
         frontend_driver_set_signal_handler_state(1);
      }

      if (fd.revents & POLLIN)
         wl_display_dispatch(wl->dpy);
      if (fd.revents & POLLOUT)
         wl_display_flush(wl->dpy);
   }
}



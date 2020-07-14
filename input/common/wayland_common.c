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

#include <stdint.h>
#include <string.h>

#include <unistd.h>

#include "wayland_common.h"

#include "../input_keymaps.h"

static void keyboard_handle_keymap(void* data,
      struct wl_keyboard* keyboard,
      uint32_t format,
      int fd,
      uint32_t size)
{
   (void)data;
   if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
   {
      close(fd);
      return;
   }

#ifdef HAVE_XKBCOMMON
   if (init_xkb(fd, size) < 0)
      RARCH_ERR("[Wayland]: Failed to init keymap.\n");
#endif
   close(fd);

   RARCH_LOG("[Wayland]: Loaded keymap.\n");
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
}

static void keyboard_handle_key(void *data,
      struct wl_keyboard *keyboard,
      uint32_t serial,
      uint32_t time,
      uint32_t key,
      uint32_t state)
{
   (void)serial;
   (void)time;
   (void)keyboard;

   int value                  = 1;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (state == WL_KEYBOARD_KEY_STATE_PRESSED)
   {
      BIT_SET(wl->input.key_state, key);
      value = 1;
   }
   else if (state == WL_KEYBOARD_KEY_STATE_RELEASED)
   {
      BIT_CLEAR(wl->input.key_state, key);
      value = 0;
   }

#ifdef HAVE_XKBCOMMON
   if (handle_xkb(key, value) == 0)
      return;
#endif
   input_keyboard_event(value,
			input_keymaps_translate_keysym_to_rk(key),
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
   (void)data;
   (void)keyboard;
   (void)serial;
#ifdef HAVE_XKBCOMMON
   handle_xkb_state_mask(modsDepressed, modsLatched, modsLocked, group);
#else
   (void)modsDepressed;
   (void)modsLatched;
   (void)modsLocked;
   (void)group;
#endif
}

void keyboard_handle_repeat_info(void *data,
      struct wl_keyboard *wl_keyboard,
      int32_t rate,
      int32_t delay)
{
   (void)data;
   (void)wl_keyboard;
   (void)rate;
   (void)delay;
   /* TODO: Seems like we'll need this to get
    * repeat working. We'll have to do it on our own. */
}

void gfx_ctx_wl_show_mouse(void *data, bool state);

static void pointer_handle_enter(void *data,
      struct wl_pointer *pointer,
      uint32_t serial,
      struct wl_surface *surface,
      wl_fixed_t sx,
      wl_fixed_t sy)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   (void)pointer;
   (void)serial;
   (void)surface;

   wl->input.mouse.last_x = wl_fixed_to_int(sx * (wl_fixed_t)wl->buffer_scale);
   wl->input.mouse.last_y = wl_fixed_to_int(sy * (wl_fixed_t)wl->buffer_scale);
   wl->input.mouse.x      = wl->input.mouse.last_x;
   wl->input.mouse.y      = wl->input.mouse.last_y;
   wl->input.mouse.focus  = true;
   wl->cursor.serial      = serial;

   gfx_ctx_wl_show_mouse(data, wl->cursor.visible);
}

static void pointer_handle_leave(void *data,
      struct wl_pointer *pointer,
      uint32_t serial,
      struct wl_surface *surface)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   wl->input.mouse.focus      = false;
   (void)pointer;
   (void)serial;
   (void)surface;
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

   if (state == WL_POINTER_BUTTON_STATE_PRESSED)
   {
      if (button == BTN_LEFT)
      {
         wl->input.mouse.left = true;

         if (BIT_GET(wl->input.key_state, KEY_LEFTALT))
         {
			 if (wl->xdg_toplevel)
			   xdg_toplevel_move(wl->xdg_toplevel, wl->seat, serial);
			 else if (wl->zxdg_toplevel)
			   zxdg_toplevel_v6_move(wl->zxdg_toplevel, wl->seat, serial);
         }
      }
      else if (button == BTN_RIGHT)
         wl->input.mouse.right = true;
      else if (button == BTN_MIDDLE)
         wl->input.mouse.middle = true;
   }
   else
   {
      if (button == BTN_LEFT)
         wl->input.mouse.left = false;
      else if (button == BTN_RIGHT)
         wl->input.mouse.right = false;
      else if (button == BTN_MIDDLE)
         wl->input.mouse.middle = false;
   }
}

static void pointer_handle_axis(void *data,
      struct wl_pointer *wl_pointer,
      uint32_t time,
      uint32_t axis,
      wl_fixed_t value)
{
   (void)data;
   (void)wl_pointer;
   (void)time;
   (void)axis;
   (void)value;
}

/* TODO: implement check for resize */

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
      struct wl_touch *wl_touch)
{
   /* TODO */
}

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
      struct wl_seat *seat, const char *name)
{
   (void)data;
   (void)seat;
   RARCH_LOG("[Wayland]: Seat name: %s.\n", name);
}

/* Surface callbacks. */

static void wl_surface_enter(void *data, struct wl_surface *wl_surface,
      struct wl_output *output)
{
    output_info_t *oi;
    gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

    /* TODO: track all outputs the surface is on, pick highest scale */

    wl_list_for_each(oi, &wl->all_outputs, link)
    {
       if (oi->output == output)
       {
          RARCH_LOG("[Wayland]: Entering output #%d, scale %d\n", oi->global_id, oi->scale);
          wl->current_output = oi;
          wl->last_buffer_scale = wl->buffer_scale;
          wl->buffer_scale = oi->scale;
          break;
       }
    };
}

static void wl_nop(void *a, struct wl_surface *b, struct wl_output *c)
{
   (void)a;
   (void)b;
   (void)c;
}

/* Shell surface callbacks. */
static void xdg_shell_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static void handle_surface_config(void *data, struct xdg_surface *surface,
                                  uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);
}

static void zxdg_shell_ping(void *data,
      struct zxdg_shell_v6 *shell, uint32_t serial)
{
    zxdg_shell_v6_pong(shell, serial);
}

static void handle_zxdg_surface_config(void *data,
      struct zxdg_surface_v6 *surface,
      uint32_t serial)
{
    zxdg_surface_v6_ack_configure(surface, serial);
}

const struct zxdg_shell_v6_listener zxdg_shell_v6_listener = {
    zxdg_shell_ping,
};

const struct zxdg_surface_v6_listener zxdg_surface_v6_listener = {
    handle_zxdg_surface_config,
};

const struct xdg_wm_base_listener xdg_shell_listener = {
    xdg_shell_ping,
};

const struct xdg_surface_listener xdg_surface_listener = {
    handle_surface_config,
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

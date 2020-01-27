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

#include <poll.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_EGL
#include <wayland-egl.h>
#endif

#ifdef HAVE_VULKAN
#include "../common/vulkan_common.h"
#endif

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../common/gl_common.h"
#endif

#include "../common/wayland_common.h"
#include "../../frontend/frontend_driver.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../../verbosity.h"

/* Generated from idle-inhibit-unstable-v1.xml */
#include "../common/wayland/idle-inhibit-unstable-v1.h"

/* Generated from xdg-shell-unstable-v6.xml */
#include "../common/wayland/xdg-shell-unstable-v6.h"

/* Generated from xdg-shell.xml */
#include "../common/wayland/xdg-shell.h"

/* Generated from xdg-decoration-unstable-v1.h */
#include "../common/wayland/xdg-decoration-unstable-v1.h"

#define WL_ARRAY_FOR_EACH(pos, array, type) \
	for (pos = (type)(array)->data; \
	     (const char *) pos < ((const char *) (array)->data + (array)->size); \
	     (pos)++)

typedef struct touch_pos
{
   bool active;
   int32_t id;
   unsigned x;
   unsigned y;
} touch_pos_t;

typedef struct output_info
{
   struct wl_output *output;
   uint32_t global_id;
   unsigned width;
   unsigned height;
   unsigned physical_width;
   unsigned physical_height;
   int refresh_rate;
   unsigned scale;
   struct wl_list link; /* wl->all_outputs */
} output_info_t;

static int num_active_touches;
static touch_pos_t active_touch_positions[MAX_TOUCHES];

typedef struct gfx_ctx_wayland_data
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
   struct wl_egl_window *win;
#endif
   bool fullscreen;
   bool maximized;
   bool resize;
   bool configured;
   bool activated;
   unsigned prev_width;
   unsigned prev_height;
   unsigned width;
   unsigned height;
   struct wl_registry *registry;
   struct wl_compositor *compositor;
   struct wl_surface *surface;
   struct zxdg_surface_v6 *zxdg_surface;
   struct zxdg_shell_v6 *zxdg_shell;
   struct zxdg_toplevel_v6 *zxdg_toplevel;
   struct xdg_surface *xdg_surface;
   struct xdg_wm_base *xdg_shell;
   struct xdg_toplevel *xdg_toplevel;
   struct wl_keyboard *wl_keyboard;
   struct wl_pointer  *wl_pointer;
   struct wl_touch *wl_touch;
   struct wl_seat *seat;
   struct wl_shm *shm;
   struct zxdg_decoration_manager_v1 *deco_manager;
   struct zxdg_toplevel_decoration_v1 *deco;
   struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
   struct zwp_idle_inhibitor_v1 *idle_inhibitor;
   struct wl_list all_outputs;
   output_info_t *current_output;
   int swap_interval;
   bool core_hw_context_enable;

   unsigned last_buffer_scale;
   unsigned buffer_scale;

   struct
   {
      struct wl_cursor *default_cursor;
      struct wl_cursor_theme *theme;
      struct wl_surface *surface;
      uint32_t serial;
      bool visible;
   } cursor;

   input_ctx_wayland_data_t input;

#ifdef HAVE_VULKAN
   gfx_ctx_vulkan_data_t vk;
#endif
} gfx_ctx_wayland_data_t;

static enum gfx_ctx_api wl_api   = GFX_CTX_NONE;

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

#ifdef HAVE_XKBCOMMON
/* FIXME: Move this into a header? */
int init_xkb(int fd, size_t size);
int handle_xkb(int code, int value);
void handle_xkb_state_mask(uint32_t depressed,
      uint32_t latched, uint32_t locked, uint32_t group);
void free_xkb(void);
#endif

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

static const struct wl_keyboard_listener keyboard_listener = {
   keyboard_handle_keymap,
   keyboard_handle_enter,
   keyboard_handle_leave,
   keyboard_handle_key,
   keyboard_handle_modifiers,
   keyboard_handle_repeat_info
};

static void gfx_ctx_wl_show_mouse(void *data, bool state);

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

static const struct wl_pointer_listener pointer_listener = {
   pointer_handle_enter,
   pointer_handle_leave,
   pointer_handle_motion,
   pointer_handle_button,
   pointer_handle_axis,
};

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

   if (num_active_touches < MAX_TOUCHES)
   {
      for (i = 0; i < MAX_TOUCHES; i++)
      {
         /* Use next empty slot */
         if (!active_touch_positions[i].active)
         {
            active_touch_positions[num_active_touches].active = true;
            active_touch_positions[num_active_touches].id     = id;
            active_touch_positions[num_active_touches].x      = (unsigned)
               wl_fixed_to_int(x);
            active_touch_positions[num_active_touches].y      = (unsigned)
               wl_fixed_to_int(y);
            num_active_touches++;
            break;
         }
      }
   }
}
static void reorder_touches(void)
{
   int i, j;
   if (num_active_touches == 0)
      return;

   for (i = 0; i < MAX_TOUCHES; i++)
   {
      if (!active_touch_positions[i].active)
      {
         for (j=i+1; j<MAX_TOUCHES; j++)
         {
            if (active_touch_positions[j].active)
            {
               active_touch_positions[i].active =
                  active_touch_positions[j].active;
               active_touch_positions[i].id     =
                  active_touch_positions[j].id;
               active_touch_positions[i].x      = active_touch_positions[j].x;
               active_touch_positions[i].y      = active_touch_positions[j].y;
               active_touch_positions[j].active = false;
               active_touch_positions[j].id     = -1;
               active_touch_positions[j].x      = (unsigned) 0;
               active_touch_positions[j].y      = (unsigned) 0;
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
      if (  active_touch_positions[i].active &&
            active_touch_positions[i].id == id)
      {
         active_touch_positions[i].active = false;
         active_touch_positions[i].id     = -1;
         active_touch_positions[i].x      = (unsigned)0;
         active_touch_positions[i].y      = (unsigned)0;
         num_active_touches--;
      }
   }
   reorder_touches();
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
      if (  active_touch_positions[i].active &&
            active_touch_positions[i].id == id)
      {
         active_touch_positions[i].x = (unsigned) wl_fixed_to_int(x);
         active_touch_positions[i].y = (unsigned) wl_fixed_to_int(y);
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
      active_touch_positions[i].active = false;
      active_touch_positions[i].id     = -1;
      active_touch_positions[i].x      = (unsigned) 0;
      active_touch_positions[i].y      = (unsigned) 0;
   }
   num_active_touches = 0;
}
static const struct wl_touch_listener touch_listener = {
   touch_handle_down,
   touch_handle_up,
   touch_handle_motion,
   touch_handle_frame,
   touch_handle_cancel,
};

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

static const struct wl_seat_listener seat_listener = {
   seat_handle_capabilities,
   seat_handle_name,
};

/* Touch handle functions */

bool wayland_context_gettouchpos(void *data, unsigned id,
      unsigned* touch_x, unsigned* touch_y)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (id >= MAX_TOUCHES)
       return false;
   *touch_x = active_touch_positions[id].x;
   *touch_y = active_touch_positions[id].y;
   return active_touch_positions[id].active;
}

/* Surface callbacks. */
static bool gfx_ctx_wl_set_resize(void *data,
      unsigned width, unsigned height);

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

static const struct wl_surface_listener wl_surface_listener = {
    wl_surface_enter,
    wl_nop,
};

/* Shell surface callbacks. */
static void xdg_shell_ping(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static const struct xdg_wm_base_listener xdg_shell_listener = {
    xdg_shell_ping,
};

static void handle_surface_config(void *data, struct xdg_surface *surface,
                                  uint32_t serial)
{
    xdg_surface_ack_configure(surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    handle_surface_config,
};

static void handle_toplevel_config(void *data,
      struct xdg_toplevel *toplevel,
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
   if (width > 0 && height > 0)
   {
      wl->prev_width  = width;
      wl->prev_height = height;
      wl->width       = width;
      wl->height      = height;
   }

#ifdef HAVE_EGL
   if (wl->win)
      wl_egl_window_resize(wl->win, width, height, 0, 0);
   else
      wl->win = wl_egl_window_create(wl->surface,
            wl->width * wl->buffer_scale,
            wl->height * wl->buffer_scale);
#endif

   wl->configured = false;
}

static void handle_toplevel_close(void *data,
      struct xdg_toplevel *xdg_toplevel)
{
	gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
	BIT_SET(wl->input.key_state, KEY_ESC);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    handle_toplevel_config,
    handle_toplevel_close,
};

static void zxdg_shell_ping(void *data,
      struct zxdg_shell_v6 *shell, uint32_t serial)
{
    zxdg_shell_v6_pong(shell, serial);
}

static const struct zxdg_shell_v6_listener zxdg_shell_v6_listener = {
    zxdg_shell_ping,
};

static void handle_zxdg_surface_config(void *data,
      struct zxdg_surface_v6 *surface,
      uint32_t serial)
{
    zxdg_surface_v6_ack_configure(surface, serial);
}

static const struct zxdg_surface_v6_listener zxdg_surface_v6_listener = {
    handle_zxdg_surface_config,
};

static void handle_zxdg_toplevel_config(
      void *data, struct zxdg_toplevel_v6 *toplevel,
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

   if (width > 0 && height > 0)
   {
      wl->prev_width = width;
      wl->prev_height = height;
      wl->width = width;
      wl->height = height;
   }

#ifdef HAVE_EGL
   if (wl->win)
      wl_egl_window_resize(wl->win, width, height, 0, 0);
   else
      wl->win = wl_egl_window_create(wl->surface,
            wl->width * wl->buffer_scale,
            wl->height * wl->buffer_scale);
#endif

   wl->configured = false;
}

static void handle_zxdg_toplevel_close(void *data,
      struct zxdg_toplevel_v6 *zxdg_toplevel)
{
	gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
	BIT_SET(wl->input.key_state, KEY_ESC);
}

static const struct zxdg_toplevel_v6_listener zxdg_toplevel_v6_listener = {
    handle_zxdg_toplevel_config,
    handle_zxdg_toplevel_close,
};

static void display_handle_geometry(void *data,
      struct wl_output *output,
      int x, int y,
      int physical_width, int physical_height,
      int subpixel,
      const char *make,
      const char *model,
      int transform)
{
   (void)data;
   (void)output;
   (void)x;
   (void)y;
   (void)subpixel;
   (void)make;
   (void)model;
   (void)transform;

   output_info_t *oi          = (output_info_t*)data;
   oi->physical_width         = physical_width;
   oi->physical_height        = physical_height;

   RARCH_LOG("[Wayland]: Physical width: %d mm x %d mm.\n",
         physical_width, physical_height);
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

   /* Certain older Wayland implementations report in Hz,
    * but it should be mHz. */
   RARCH_LOG("[Wayland]: Video mode: %d x %d @ %.4f Hz.\n",
         width, height, refresh > 1000 ? refresh / 1000.0 : (double)refresh);
}

static void display_handle_done(void *data,
      struct wl_output *output)
{
   (void)data;
   (void)output;
}

static void display_handle_scale(void *data,
      struct wl_output *output,
      int32_t factor)
{
   output_info_t *oi = (output_info_t*)data;

   RARCH_LOG("[Wayland]: Display scale factor %d.\n", factor);
   oi->scale = factor;
}

static const struct wl_output_listener output_listener = {
   display_handle_geometry,
   display_handle_mode,
   display_handle_done,
   display_handle_scale,
};

/* Registry callbacks. */
static void registry_handle_global(void *data, struct wl_registry *reg,
      uint32_t id, const char *interface, uint32_t version)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   (void)version;

   if (string_is_equal(interface, "wl_compositor"))
      wl->compositor = (struct wl_compositor*)wl_registry_bind(reg,
            id, &wl_compositor_interface, 3);
   else if (string_is_equal(interface, "wl_output"))
   {
      output_info_t *oi = (output_info_t*)
         calloc(1, sizeof(output_info_t));

      oi->global_id     = id;
      oi->output        = (struct wl_output*)wl_registry_bind(reg,
            id, &wl_output_interface, 2);
      wl_output_add_listener(oi->output, &output_listener, oi);
      wl_list_insert(&wl->all_outputs, &oi->link);
      wl_display_roundtrip(wl->input.dpy);
   }
   else if (string_is_equal(interface, "xdg_wm_base"))
      wl->xdg_shell = (struct xdg_wm_base*)
         wl_registry_bind(reg, id, &xdg_wm_base_interface, 1);
   else if (string_is_equal(interface, "zxdg_shell_v6"))
      wl->zxdg_shell = (struct zxdg_shell_v6*)
         wl_registry_bind(reg, id, &zxdg_shell_v6_interface, 1);
   else if (string_is_equal(interface, "wl_shm"))
      wl->shm = (struct wl_shm*)wl_registry_bind(reg, id, &wl_shm_interface, 1);
   else if (string_is_equal(interface, "wl_seat"))
   {
      wl->seat = (struct wl_seat*)wl_registry_bind(reg, id, &wl_seat_interface, 2);
      wl_seat_add_listener(wl->seat, &seat_listener, wl);
   }
   else if (string_is_equal(interface, "zwp_idle_inhibit_manager_v1"))
      wl->idle_inhibit_manager = (struct zwp_idle_inhibit_manager_v1*)wl_registry_bind(
                                  reg, id, &zwp_idle_inhibit_manager_v1_interface, 1);
   else if (string_is_equal(interface, "zxdg_decoration_manager_v1"))
      wl->deco_manager = (struct zxdg_decoration_manager_v1*)wl_registry_bind(
                                  reg, id, &zxdg_decoration_manager_v1_interface, 1);
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

static const struct wl_registry_listener registry_listener = {
   registry_handle_global,
   registry_handle_global_remove,
};

static void gfx_ctx_wl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   *width  = wl->width  * wl->buffer_scale;
   *height = wl->height * wl->buffer_scale;
}

static void gfx_ctx_wl_destroy_resources(gfx_ctx_wayland_data_t *wl)
{
   if (!wl)
      return;

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_destroy(&wl->egl);

         if (wl->win)
            wl_egl_window_destroy(wl->win);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_context_destroy(&wl->vk, wl->surface);

         if (wl->input.dpy != NULL && wl->input.fd >= 0)
            close(wl->input.fd);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

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
   if (wl->zxdg_shell)
      zxdg_shell_v6_destroy(wl->zxdg_shell);
   if (wl->compositor)
      wl_compositor_destroy(wl->compositor);
   if (wl->registry)
      wl_registry_destroy(wl->registry);
   if (wl->xdg_surface)
      xdg_surface_destroy(wl->xdg_surface);
   if (wl->zxdg_surface)
      zxdg_surface_v6_destroy(wl->zxdg_surface);
   if (wl->surface)
      wl_surface_destroy(wl->surface);
   if (wl->xdg_toplevel)
      xdg_toplevel_destroy(wl->xdg_toplevel);
   if (wl->zxdg_toplevel)
      zxdg_toplevel_v6_destroy(wl->zxdg_toplevel);
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
   wl->zxdg_shell       = NULL;
   wl->compositor       = NULL;
   wl->registry         = NULL;
   wl->input.dpy        = NULL;
   wl->xdg_surface      = NULL;
   wl->surface          = NULL;
   wl->xdg_toplevel     = NULL;
   wl->zxdg_toplevel    = NULL;

   wl->width            = 0;
   wl->height           = 0;

}

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

static void gfx_ctx_wl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height,
      bool is_shutdown)
{
   /* this function works with SCALED sizes, it's used from the renderer */
   unsigned new_width, new_height;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   flush_wayland_fd(&wl->input);

   new_width  = *width  * wl->last_buffer_scale;
   new_height = *height * wl->last_buffer_scale;

   gfx_ctx_wl_get_video_size(data, &new_width, &new_height);

   switch (wl_api)
   {
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         /* Swapchains are recreated in set_resize as a
          * central place, so use that to trigger swapchain reinit. */
         *resize = wl->vk.need_new_swapchain;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (new_width != *width * wl->last_buffer_scale || new_height != *height * wl->last_buffer_scale)
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

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         wl_egl_window_resize(wl->win, width, height, 0, 0);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (vulkan_create_swapchain(&wl->vk, width, height, wl->swap_interval))
         {
            wl->vk.context.invalid_swapchain = true;
            if (wl->vk.created_new_swapchain)
               vulkan_acquire_next_image(&wl->vk);
         }
         else
         {
            RARCH_ERR("[Wayland/Vulkan]: Failed to update swapchain.\n");
            return false;
         }

         wl->vk.need_new_swapchain = false;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);
   return true;
}

static void gfx_ctx_wl_update_title(void *data, void *data2)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   char title[128];

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (wl && title[0])
   {
      if (wl->xdg_toplevel || wl->zxdg_toplevel)
      {
         if (wl->deco)
         {
            zxdg_toplevel_decoration_v1_set_mode(wl->deco,
                  ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
         }
      }
      if (wl->xdg_toplevel)
         xdg_toplevel_set_title(wl->xdg_toplevel, title);
      else if (wl->zxdg_toplevel)
         zxdg_toplevel_v6_set_title(wl->zxdg_toplevel, title);
   }
}

static bool gfx_ctx_wl_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl || !wl->current_output || wl->current_output->physical_width == 0 || wl->current_output->physical_height == 0)
      return false;

   switch (type)
   {
      case DISPLAY_METRIC_MM_WIDTH:
         *value = (float)wl->current_output->physical_width;
         break;

      case DISPLAY_METRIC_MM_HEIGHT:
         *value = (float)wl->current_output->physical_height;
         break;

      case DISPLAY_METRIC_DPI:
         *value = (float)wl->current_output->width * 25.4f / (float)wl->current_output->physical_width;
         break;

      default:
         *value = 0.0f;
         return false;
   }

   return true;
}

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

static void *gfx_ctx_wl_init(video_frame_info_t *video_info, void *video_driver)
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

   (void)video_driver;

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

   wl->input.dpy = wl_display_connect(NULL);
   wl->last_buffer_scale = 1;
   wl->buffer_scale = 1;

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

   if (!wl->xdg_shell && !!wl->zxdg_shell)
   {
      RARCH_LOG("[Wayland]: Using zxdg_shell_v6 interface.\n");
   }

   if (!wl->xdg_shell && !wl->zxdg_shell)
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

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
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
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (!vulkan_context_init(&wl->vk, VULKAN_WSI_WAYLAND))
            goto error;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   wl->input.keyboard_focus = true;
   wl->input.mouse.focus = true;

   wl->cursor.surface = wl_compositor_create_surface(wl->compositor);
   wl->cursor.theme = wl_cursor_theme_load(NULL, 16, wl->shm);
   wl->cursor.default_cursor = wl_cursor_theme_get_cursor(wl->cursor.theme, "left_ptr");

   num_active_touches = 0;
   for (i = 0;i < MAX_TOUCHES;i++)
   {
       active_touch_positions[i].active = false;
       active_touch_positions[i].id = -1;
       active_touch_positions[i].x = (unsigned) 0;
       active_touch_positions[i].y = (unsigned) 0;
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
#endif

         break;
      }
#endif

      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
         *attr++ = EGL_CONTEXT_CLIENT_VERSION; /* Same as EGL_CONTEXT_MAJOR_VERSION */
         *attr++ = wl->egl.major ? (EGLint)wl->egl.major : 2;
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

   switch (wl_api)
   {
      case GFX_CTX_VULKAN_API:
#if defined(HAVE_VULKAN) && defined(HAVE_THREADS)
         if (wl->vk.context.queue_lock)
            slock_free(wl->vk.context.queue_lock);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   free(wl);
}

static void gfx_ctx_wl_set_swap_interval(void *data, int swap_interval)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_set_swap_interval(&wl->egl, swap_interval);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         if (wl->swap_interval != swap_interval)
         {
            wl->swap_interval = swap_interval;
            if (wl->vk.swapchain)
               wl->vk.need_new_swapchain = true;
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static bool gfx_ctx_wl_set_video_mode(void *data,
      video_frame_info_t *video_info,
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

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         wl->win        = wl_egl_window_create(wl->surface, wl->width * wl->buffer_scale, wl->height * wl->buffer_scale);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (wl->xdg_shell)
   {
      wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->xdg_shell, wl->surface);
      xdg_surface_add_listener(wl->xdg_surface, &xdg_surface_listener, wl);

      wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
      xdg_toplevel_add_listener(wl->xdg_toplevel, &xdg_toplevel_listener, wl);

      xdg_toplevel_set_app_id(wl->xdg_toplevel, "retroarch");
      xdg_toplevel_set_title(wl->xdg_toplevel, "RetroArch");

      if (wl->deco_manager)
      {
         wl->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
               wl->deco_manager, wl->xdg_toplevel);
      }

      /* Waiting for xdg_toplevel to be configured before starting to draw */
      wl_surface_commit(wl->surface);
      wl->configured = true;

      while (wl->configured)
         wl_display_dispatch(wl->input.dpy);

      wl_display_roundtrip(wl->input.dpy);
      xdg_wm_base_add_listener(wl->xdg_shell, &xdg_shell_listener, NULL);
   }
   else if (wl->zxdg_shell)
   {
      wl->zxdg_surface = zxdg_shell_v6_get_xdg_surface(wl->zxdg_shell, wl->surface);
      zxdg_surface_v6_add_listener(wl->zxdg_surface, &zxdg_surface_v6_listener, wl);

      wl->zxdg_toplevel = zxdg_surface_v6_get_toplevel(wl->zxdg_surface);
      zxdg_toplevel_v6_add_listener(wl->zxdg_toplevel, &zxdg_toplevel_v6_listener, wl);

      zxdg_toplevel_v6_set_app_id(wl->zxdg_toplevel, "retroarch");
      zxdg_toplevel_v6_set_title(wl->zxdg_toplevel, "RetroArch");

      if (wl->deco_manager)
         wl->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
               wl->deco_manager, wl->xdg_toplevel);

      /* Waiting for xdg_toplevel to be configured before starting to draw */
      wl_surface_commit(wl->surface);
      wl->configured = true;

      while (wl->configured)
         wl_display_dispatch(wl->input.dpy);

      wl_display_roundtrip(wl->input.dpy);
      zxdg_shell_v6_add_listener(wl->zxdg_shell, &zxdg_shell_v6_listener, NULL);
   }

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL

         if (!egl_create_context(&wl->egl, (attr != egl_attribs) ? egl_attribs : NULL))
         {
            egl_report_error();
            goto error;
         }

         if (!egl_create_surface(&wl->egl, (EGLNativeWindowType)wl->win))
            goto error;
         egl_set_swap_interval(&wl->egl, wl->egl.interval);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (fullscreen)
   {
	   if (wl->xdg_toplevel)
		   xdg_toplevel_set_fullscreen(wl->xdg_toplevel, NULL);
	   else if (wl->zxdg_toplevel)
		   zxdg_toplevel_v6_set_fullscreen(wl->zxdg_toplevel, NULL);
	}

   flush_wayland_fd(&wl->input);

   switch (wl_api)
   {
      case GFX_CTX_VULKAN_API:
         wl_display_roundtrip(wl->input.dpy);

#ifdef HAVE_VULKAN
         if (!vulkan_surface_create(&wl->vk, VULKAN_WSI_WAYLAND,
                  wl->input.dpy, wl->surface,
                  wl->width * wl->buffer_scale, wl->height * wl->buffer_scale, wl->swap_interval))
            goto error;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (fullscreen)
   {
      wl->cursor.visible = false;
      gfx_ctx_wl_show_mouse(wl, false);
   }
   else
      wl->cursor.visible = true;

   return true;

#if defined(HAVE_EGL) || defined(HAVE_VULKAN)
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
      *input      = NULL;
      *input_data = NULL;
   }
   else
   {
      *input      = &input_wayland;
      *input_data = &wl->input;
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
	(void)data;

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
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         return true;
#else
         break;
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

#ifdef HAVE_VULKAN
static void *gfx_ctx_wl_get_context_data(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return &wl->vk.context;
}
#endif

static void gfx_ctx_wl_swap_buffers(void *data, void *data2)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_swap_buffers(&wl->egl);
#endif
         break;
      case GFX_CTX_VULKAN_API:
#ifdef HAVE_VULKAN
         vulkan_present(&wl->vk, wl->vk.context.current_swapchain_index);
         vulkan_acquire_next_image(&wl->vk);
         flush_wayland_fd(&wl->input);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static gfx_ctx_proc_t gfx_ctx_wl_get_proc_address(const char *symbol)
{
   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         return egl_get_proc_address(symbol);
#else
         break;
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return NULL;
}

static void gfx_ctx_wl_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_EGL
         egl_bind_hw_render(&wl->egl, enable);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
}

static uint32_t gfx_ctx_wl_get_flags(void *data)
{
   uint32_t             flags = 0;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->core_hw_context_enable)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
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
         break;
      case GFX_CTX_VULKAN_API:
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
         BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return flags;
}

static void gfx_ctx_wl_set_flags(void *data, uint32_t flags)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      wl->core_hw_context_enable = true;
}

static void gfx_ctx_wl_show_mouse(void *data, bool state)
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
   gfx_ctx_wl_get_proc_address,
   NULL,
   NULL,
   gfx_ctx_wl_show_mouse,
   "wayland",
   gfx_ctx_wl_get_flags,
   gfx_ctx_wl_set_flags,
   gfx_ctx_wl_bind_hw_render,
#ifdef HAVE_VULKAN
   gfx_ctx_wl_get_context_data,
#else
   NULL,
#endif
   NULL,
};

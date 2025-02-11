/*  RetroArch - A frontend for libretro.
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

#ifndef _WAYLAND_COMMON_H
#define _WAYLAND_COMMON_H

#include <stdint.h>
#include <boolean.h>

#include <linux/input.h>
#include <wayland-client.h>
#include <wayland-cursor.h>

#include "../input_driver.h"

#ifdef HAVE_EGL
#include "../../gfx/common/egl_common.h"
#endif

#ifdef HAVE_VULKAN
#include "../../gfx/common/vulkan_common.h"
#endif

/* Generated from wayland protocol files by generate_wayland_protos.sh */
#include "../../gfx/common/wayland/content-type-v1.h"
#include "../../gfx/common/wayland/cursor-shape-v1.h"
#include "../../gfx/common/wayland/fractional-scale-v1.h"
#include "../../gfx/common/wayland/idle-inhibit-unstable-v1.h"
#include "../../gfx/common/wayland/pointer-constraints-unstable-v1.h"
#include "../../gfx/common/wayland/relative-pointer-unstable-v1.h"
#include "../../gfx/common/wayland/single-pixel-buffer-v1.h"
#include "../../gfx/common/wayland/viewporter.h"
#include "../../gfx/common/wayland/xdg-decoration-unstable-v1.h"
#include "../../gfx/common/wayland/xdg-shell.h"

#define FRACTIONAL_SCALE_V1_DEN 120
#define FRACTIONAL_SCALE_MULT(v, scale_num) \
   (((v) * (scale_num) + FRACTIONAL_SCALE_V1_DEN / 2) / FRACTIONAL_SCALE_V1_DEN)

#define UDEV_KEY_MAX            0x2ff
#define UDEV_MAX_KEYS           (UDEV_KEY_MAX + 7) / 8

#define MAX_TOUCHES             16

#define WL_ARRAY_FOR_EACH(pos, array, type) \
	for (pos = (type)(array)->data; \
	     (const char *) pos < ((const char *) (array)->data + (array)->size); \
	     (pos)++)

#ifdef HAVE_LIBDECOR_H
#include <libdecor.h>
#endif

typedef struct
{
   int16_t x;
   int16_t y;
   bool active;
} wayland_touch_data_t;

typedef struct touch_pos
{
   int32_t id;
   unsigned x;
   unsigned y;
   bool active;
} touch_pos_t;

typedef struct output_info
{
   struct wl_output *output;
   int refresh_rate;
   uint32_t global_id;
   unsigned width;
   unsigned height;
   unsigned physical_width;
   unsigned physical_height;
   unsigned scale;
   char *make;
   char *model;
} output_info_t;

typedef struct display_output
{
   output_info_t *output;
   struct wl_list link;
} display_output_t;

typedef struct surface_output
{
   output_info_t *output;
   struct wl_list link;
} surface_output_t;

struct gfx_ctx_wayland_data;

typedef struct input_ctx_wayland_data
{
   struct wl_display *dpy;
   const input_device_driver_t *joypad;
   struct gfx_ctx_wayland_data *gfx;

   int fd;

   wayland_touch_data_t touches[MAX_TOUCHES]; /* int16_t alignment */
   /* Wayland uses Linux keysyms. */
   uint8_t key_state[UDEV_MAX_KEYS];

   struct
   {
      struct wl_surface *surface;
      int last_x, last_y;
      int x, y;
      int delta_x, delta_y;
      bool last_valid;
      bool focus;
      bool left, right, middle, side, extra;
      bool wu, wd, wl, wr;
   } mouse;

   bool keyboard_focus;
   bool blocked;
} input_ctx_wayland_data_t;

typedef struct data_offer_ctx
{
  struct wl_data_offer *offer;
  struct wl_data_device *data_device;
  bool is_file_mime_type;
  bool dropped;
  enum wl_data_device_manager_dnd_action supported_actions;
} data_offer_ctx;

typedef struct gfx_ctx_wayland_data
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
   struct wl_egl_window *win;
#endif
   struct wl_registry *registry;
   struct wl_compositor *compositor;
   struct wp_viewporter *viewporter;
   struct wp_fractional_scale_manager_v1 *fractional_scale_manager;
   struct wl_surface *surface;
   struct xdg_surface *xdg_surface;
   struct wp_viewport *viewport;
   struct wp_fractional_scale_v1 *fractional_scale;
   struct xdg_wm_base *xdg_shell;
   struct xdg_toplevel *xdg_toplevel;
   struct wl_keyboard *wl_keyboard;
   struct wl_pointer  *wl_pointer;
   struct zwp_relative_pointer_v1 *wl_relative_pointer;
   struct zwp_locked_pointer_v1 *locked_pointer;
   struct wl_touch *wl_touch;
   struct wl_seat *seat;
   struct wl_shm *shm;
   struct wl_data_device_manager *data_device_manager;
   struct wl_data_device *data_device;
   data_offer_ctx *current_drag_offer;
#ifdef HAVE_LIBDECOR_H
   struct libdecor *libdecor_context;
   struct libdecor_frame *libdecor_frame;
#ifdef HAVE_DYLIB
   dylib_t libdecor;
#define RA_WAYLAND_SYM(rc,fn,params) rc (*fn) params;
#include "../../gfx/common/wayland/libdecor_sym.h"
#endif
#endif
   struct zxdg_decoration_manager_v1 *deco_manager;
   struct zxdg_toplevel_decoration_v1 *deco;
   struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
   struct zwp_idle_inhibitor_v1 *idle_inhibitor;
   struct zwp_pointer_constraints_v1 *pointer_constraints;
   struct zwp_relative_pointer_manager_v1 *relative_pointer_manager;
   struct wp_cursor_shape_manager_v1 *cursor_shape_manager;
   struct wp_cursor_shape_device_v1 *cursor_shape_device;
   struct wp_content_type_manager_v1 *content_type_manager;
   struct wp_content_type_v1 *content_type;
   struct wp_single_pixel_buffer_manager_v1 *single_pixel_manager;
   output_info_t *current_output;
#ifdef HAVE_VULKAN
   gfx_ctx_vulkan_data_t vk;
#endif
   input_ctx_wayland_data_t input; /* ptr alignment */
   struct wl_list all_outputs;
   struct wl_list current_outputs;

   struct
   {
      struct wl_cursor *default_cursor;
      struct wl_cursor_theme *theme;
      struct wl_surface *surface;
      uint32_t serial;
      bool visible;
   } cursor;

   int num_active_touches;
   int swap_interval;
   touch_pos_t active_touch_positions[MAX_TOUCHES]; /* int32_t alignment */
   unsigned width;
   unsigned height;
   unsigned buffer_width;
   unsigned buffer_height;
   unsigned floating_width;
   unsigned floating_height;
   unsigned last_buffer_scale;
   unsigned pending_buffer_scale;
   unsigned buffer_scale;
   unsigned last_fractional_scale_num;
   unsigned pending_fractional_scale_num;
   unsigned fractional_scale_num;

   bool core_hw_context_enable;
   bool fullscreen;
   bool maximized;
   bool resize;
   bool configured;
   bool ignore_configuration;
   bool activated;
   bool reported_display_size;
   bool swap_complete;
} gfx_ctx_wayland_data_t;

#ifdef HAVE_XKBCOMMON
/* FIXME: Move this into a header? */
int init_xkb(int fd, size_t len);
int handle_xkb(int code, int value);
void handle_xkb_state_mask(uint32_t depressed,
      uint32_t latched, uint32_t locked, uint32_t group);
void free_xkb(void);
#endif

void gfx_ctx_wl_show_mouse(void *data, bool state);

void flush_wayland_fd(void *data);

extern const struct wl_keyboard_listener keyboard_listener;

extern const struct wl_pointer_listener pointer_listener;

extern const struct zwp_relative_pointer_v1_listener relative_pointer_listener;

extern const struct zwp_locked_pointer_v1_listener locked_pointer_listener;

extern const struct wl_touch_listener touch_listener;

extern const struct wl_seat_listener seat_listener;

extern const struct wp_fractional_scale_v1_listener wp_fractional_scale_v1_listener;

extern const struct wl_surface_listener wl_surface_listener;

extern const struct xdg_wm_base_listener xdg_shell_listener;

extern const struct xdg_surface_listener xdg_surface_listener;

extern const struct wl_output_listener output_listener;

extern const struct wl_registry_listener registry_listener;

extern const struct wl_buffer_listener shm_buffer_listener;

extern const struct wl_data_device_listener data_device_listener;

extern const struct wl_data_offer_listener data_offer_listener;

#endif

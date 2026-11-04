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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>

#include <retro_atomic.h>
#ifdef HAVE_VIDEO_WINDOWS
#include <compat/strl.h>
#include <encodings/utf.h>
#include <features/features_cpu.h>
#endif
#ifdef HAVE_THREADS
#include <retro_spsc.h>
#include <rthreads/rthreads.h>
#endif

#include "wayland_common.h"
#include "../gfx/video_driver.h"
#include "../../frontend/frontend_driver.h"
#include "../../verbosity.h"
#include "../../runloop.h"

#include "dbus_common.h"

#define SPLASH_SHM_NAME "retroarch-wayland-vk-splash"

#define DEFAULT_WINDOW_TITLE "RetroArch"
#define MAIN_WINDOW_TAG "main window"

#ifdef HAVE_LIBDECOR_H
#include <libdecor.h>
#endif

#define DEFAULT_WINDOWED_WIDTH 640
#define DEFAULT_WINDOWED_HEIGHT 480

/* Icon is 16x15 scaled by 16 */
#define SPLASH_WINDOW_WIDTH 240
#define SPLASH_WINDOW_HEIGHT 256

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC		0x0001U
#endif

#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING	0x0002U
#endif

#ifndef F_ADD_SEALS
#define F_ADD_SEALS		(1024 + 9)
#endif

#ifndef F_SEAL_SHRINK
#define F_SEAL_SHRINK		0x0002
#endif

static const unsigned long retroarch_icon_data[] = {
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

/* Detects a fullscreen sizing bug on some hardware and/or compositors */
static bool gfx_ctx_wl_should_use_legacy_fullscreen_configure(
      gfx_ctx_wayland_data_t *wl, int width, int height)
{
   if (!wl)
      return false;

   if (width <= 0 || height <= 0)
      return false;

   /* If the fullscreen menu appears the same size as the SPLASH_WINDOW
    * (240x256), then fall back to the legacy fullscreen configure path */
   if (width <= SPLASH_WINDOW_WIDTH && height <= SPLASH_WINDOW_HEIGHT)
      return true;

   return false;
}

/* The buffer a surface of 'dims' takes at a scale: fractional (num of
 * 120) through a viewport, else whole */
static unsigned wl_scaled_dims(unsigned dims, bool fractional,
      unsigned num, unsigned scale)
{
   unsigned w = VIDEO_SCALE_W(dims);
   unsigned h = VIDEO_SCALE_H(dims);
   if (fractional)
      return VIDEO_SCALE_PACK(FRACTIONAL_SCALE_MULT(w, num),
            FRACTIONAL_SCALE_MULT(h, num));
   return VIDEO_SCALE_PACK(w * scale, h * scale);
}

/* Apply a latched xdg_toplevel configuration.  Runs from the
 * xdg_surface.configure handler: per xdg-shell, toplevel configure
 * events describe pending state that only becomes current when the
 * compositor sends xdg_surface.configure. */
static void xdg_configure_apply(gfx_ctx_wayland_data_t *wl)
{
   int32_t width  = VIDEO_SCALE_W(wl->cfg_pending.dims);
   int32_t height = VIDEO_SCALE_H(wl->cfg_pending.dims);
   bool floating  = wl->cfg_pending.floating;

   wl->fullscreen = wl->cfg_pending.fullscreen;
   if (wl->fullscreen == wl->fullscreen_requested)
      wl->fullscreen_pending = false;
   wl->maximized  = wl->cfg_pending.maximized;
   if (wl->cfg_pending.resizing)
      wl->resize  = true;
   if (wl->cfg_pending.activated)
      wl->activated = true;
   wl->suspended  = wl->cfg_pending.suspended;

   if (width == 0 || height == 0)
   {
      width  = VIDEO_SCALE_W(wl->floating_dims);
      height = VIDEO_SCALE_H(wl->floating_dims);
   }

   if (wl->fullscreen
         && gfx_ctx_wl_should_use_legacy_fullscreen_configure(
            wl, width, height))
   {
      /* Fullscreen matches SPLASH_WINDOW size: fall back to legacy path */
      wl->ignore_configuration = true;
      width                    = 0;
      height                   = 0;
   }

   if (     (width  > 0)
         && (height > 0))
   {
      wl->dims          = VIDEO_SCALE_PACK(width, height);
      wl->buffer_dims   = wl_scaled_dims(wl->dims,
            wl->fractional_scale != NULL, wl->fractional_scale_num,
            wl->buffer_scale);
      wl->resize        = true;
      if (wl->viewport)
      {
         /* Stretch old buffer to fill new size, commit/roundtrip to apply */
         wp_viewport_set_destination(wl->viewport,
               VIDEO_SCALE_W(wl->dims), VIDEO_SCALE_H(wl->dims));
         wl_surface_commit(wl->surface);
      }
   }

   if (floating)
      wl->floating_dims = VIDEO_SCALE_PACK(width, height);
}

static void xdg_toplevel_handle_close(void *data,
      struct xdg_toplevel *xdg_toplevel)
{
   frontend_driver_set_signal_handler_state(1);
}

#ifdef HAVE_LIBDECOR_H
static void libdecor_frame_handle_configure_common(struct libdecor_frame *frame,
      struct libdecor_configuration *configuration,
      gfx_ctx_wayland_data_t *wl)
{
   int width = 0, height = 0;
   struct libdecor_state *state = NULL;
   enum libdecor_window_state window_state;
   bool fullscreen_pending = wl->fullscreen_pending;
#if 0
   static const enum
      libdecor_window_state tiled_states = (
         LIBDECOR_WINDOW_STATE_TILED_LEFT
       | LIBDECOR_WINDOW_STATE_TILED_RIGHT
       | LIBDECOR_WINDOW_STATE_TILED_TOP
       | LIBDECOR_WINDOW_STATE_TILED_BOTTOM
   );
   bool focused   = false;
   bool tiled     = false;
#endif

   wl->fullscreen = false;
   wl->maximized  = false;

   if (wl->libdecor_configuration_get_window_state(
         configuration, &window_state))
   {
      /* LIBDECOR_WINDOW_STATE_SUSPENDED (libdecor >= 0.2.0); numeric
       * value used so older libdecor headers still compile.  Older
       * runtimes never set the bit. */
      wl->fullscreen = (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) != 0;
      wl->maximized  = (window_state & LIBDECOR_WINDOW_STATE_MAXIMIZED) != 0;
      wl->suspended  = (window_state & (1 << 7)) != 0;
#if 0
      focused        = (window_state & LIBDECOR_WINDOW_STATE_ACTIVE) != 0;
      tiled          = (window_state & tiled_states) != 0;
#endif
   }

   if (wl->fullscreen == wl->fullscreen_requested)
      wl->fullscreen_pending = false;

   if (!wl->libdecor_configuration_get_content_size(configuration, frame,
         &width, &height))
   {
      width  = VIDEO_SCALE_W(wl->floating_dims);
      height = VIDEO_SCALE_H(wl->floating_dims);
   }

   if (wl->fullscreen
         && gfx_ctx_wl_should_use_legacy_fullscreen_configure(
            wl, width, height))
   {
      /* Fullscreen matches SPLASH_WINDOW size: fall back to legacy path */
      wl->ignore_configuration = true;
      width                    = 0;
      height                   = 0;
   }

   if (     width  > 0
         && height > 0)
   {
      wl->dims          = VIDEO_SCALE_PACK(width, height);
      wl->buffer_dims   = wl_scaled_dims(wl->dims,
            wl->fractional_scale != NULL, wl->fractional_scale_num,
            wl->buffer_scale);
      wl->resize        = true;
      if (wl->viewport)
      {
         /* Stretch old buffer to fill new size, commit/roundtrip to apply */
         wp_viewport_set_destination(wl->viewport,
               VIDEO_SCALE_W(wl->dims), VIDEO_SCALE_H(wl->dims));
      }
   }

   state = wl->libdecor_state_new(VIDEO_SCALE_W(wl->dims),
         VIDEO_SCALE_H(wl->dims));
   wl->libdecor_frame_commit(frame, state, configuration);
   wl->libdecor_state_free(state);
   if (fullscreen_pending && !wl->fullscreen_pending)
      wl->libdecor_frame_set_visibility(frame, !wl->fullscreen);
   if (wl->viewport)
      wl_surface_commit(wl->surface);

   if (wl->libdecor_frame_is_floating(frame))
      wl->floating_dims = VIDEO_SCALE_PACK(width, height);
}

static void libdecor_frame_handle_close(struct libdecor_frame *frame,
      void *data)
{
   frontend_driver_set_signal_handler_state(1);
}
static void libdecor_frame_handle_commit(struct libdecor_frame *frame,
      void *data) { }

static void libdecor_frame_handle_configure(struct libdecor_frame *frame,
      struct libdecor_configuration *configuration, void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (wl->ignore_configuration)
   {
      /* Committing the configuration is what acknowledges it; an
       * unacknowledged one leaves the frame's state stale and the
       * compositor waiting. Keep the current size. */
      struct libdecor_state *state = wl->libdecor_state_new(
            VIDEO_SCALE_W(wl->dims), VIDEO_SCALE_H(wl->dims));
      wl->libdecor_frame_commit(frame, state, configuration);
      wl->libdecor_state_free(state);
      return;
   }
   libdecor_frame_handle_configure_common(frame, configuration, wl);
   if (wl->driver_configure_handler)
      wl->driver_configure_handler(wl);
   wl->configured = false;
}

static void libdecor_handle_err(struct libdecor *context,
      enum libdecor_error err, const char *message)
{
   RARCH_ERR("[Wayland] libdecor Caught error (%d): %s.\n", err, message);
}

static const struct libdecor_interface libdecor_interface = {
   .error = libdecor_handle_err,
};

static const struct libdecor_frame_interface wl_libdecor_frame_interface = {
   libdecor_frame_handle_configure,
   libdecor_frame_handle_close,
   libdecor_frame_handle_commit,
};
#endif

static void xdg_toplevel_handle_configure(void *data,
      struct xdg_toplevel *toplevel,
      int32_t width, int32_t height, struct wl_array *states)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   const uint32_t *state;

   /* Record only; the state is pending until xdg_surface.configure.
    * A later toplevel.configure before the surface.configure
    * supersedes this one (last-wins), matching the protocol. */
   wl->cfg_pending.dims       = VIDEO_SCALE_PACK(width, height);
   wl->cfg_pending.fullscreen = false;
   wl->cfg_pending.maximized  = false;
   wl->cfg_pending.resizing   = false;
   wl->cfg_pending.activated  = false;
   wl->cfg_pending.floating   = true;
   wl->cfg_pending.suspended  = false;

   WL_ARRAY_FOR_EACH(state, states, const uint32_t*)
   {
      switch (*state)
      {
         case XDG_TOPLEVEL_STATE_FULLSCREEN:
            wl->cfg_pending.fullscreen = true;
            wl->cfg_pending.floating   = false;
            break;
         case XDG_TOPLEVEL_STATE_MAXIMIZED:
            wl->cfg_pending.maximized  = true;
            /* fall-through */
         case XDG_TOPLEVEL_STATE_TILED_LEFT:
         case XDG_TOPLEVEL_STATE_TILED_RIGHT:
         case XDG_TOPLEVEL_STATE_TILED_TOP:
         case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
            wl->cfg_pending.floating   = false;
            break;
         case XDG_TOPLEVEL_STATE_RESIZING:
            wl->cfg_pending.resizing   = true;
            break;
         case XDG_TOPLEVEL_STATE_ACTIVATED:
            wl->cfg_pending.activated  = true;
            break;
         case XDG_TOPLEVEL_STATE_SUSPENDED:
            wl->cfg_pending.suspended  = true;
            break;
      }
   }

   wl->cfg_pending.pending = true;
}

static void xdg_surface_handle_configure_latch(void *data,
      struct xdg_surface *xdg_surface, uint32_t serial)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   /* Size-dependent commits below must acknowledge the new state first. */
   xdg_surface_ack_configure(xdg_surface, serial);

   if (wl->cfg_pending.pending)
   {
      wl->cfg_pending.pending = false;
      /* ignore_configuration drops the configuration exactly as the
       * pre-latching code dropped it at toplevel.configure time;
       * evaluated here, at the atomic point. */
      if (!wl->ignore_configuration)
      {
         xdg_configure_apply(wl);
         if (wl->driver_configure_handler)
            wl->driver_configure_handler(wl);
         wl->configured = false;
      }
   }
}

static const struct xdg_surface_listener wl_xdg_surface_listener = {
   xdg_surface_handle_configure_latch,
};

static void xdg_toplevel_handle_configure_bounds(void *data,
      struct xdg_toplevel *xdg_toplevel,
      int32_t width, int32_t height)
{
   /* Advisory maximum size (since v4); RetroArch sizes from the
    * configure events themselves, so nothing to do. */
}

static void xdg_toplevel_handle_wm_capabilities(void *data,
      struct xdg_toplevel *xdg_toplevel,
      struct wl_array *capabilities)
{
   /* Advertised WM actions (since v5); RetroArch does not currently
    * tailor its UI to them. */
}

static const struct xdg_toplevel_listener wl_xdg_toplevel_listener = {
   xdg_toplevel_handle_configure,
   xdg_toplevel_handle_close,
   xdg_toplevel_handle_configure_bounds,
   xdg_toplevel_handle_wm_capabilities,
};

void gfx_ctx_wl_get_video_size_common(void *data, unsigned *dims)
{
   gfx_ctx_wayland_data_t *wl   = (gfx_ctx_wayland_data_t*)data;
   if (!wl)
      return;
   if (!wl->reported_display_size)
   {
      display_output_t *od;
      output_info_t *oi         = wl->current_output;

      wl->reported_display_size = true;

      /* If window is not ready get any monitor */
      if (!oi)
         wl_list_for_each(od, &wl->all_outputs, link)
         {
            oi = od->output;
            break;
         };

      /* all_outputs may legitimately be empty (last monitor was hot-
       * unplugged before frame size was queried); leave the caller's
       * dims unchanged rather than NULL-deref oi. */
      if (!oi)
         return;

      *dims = oi->dims;
   }
   else
      *dims = wl_scaled_dims(wl->dims, wl->fractional_scale != NULL,
            wl->pending_fractional_scale_num, wl->pending_buffer_scale);
}

static void shm_buffer_free(shm_buffer_t *buffer)
{
   wl_buffer_destroy(buffer->wl_buffer);
   munmap(buffer->data, buffer->data_size);
   free(buffer);
}

void gfx_ctx_wl_destroy_resources_common(gfx_ctx_wayland_data_t *wl)
{
#ifdef HAVE_XKBCOMMON
   free_xkb();
#endif

   if (wl->wl_keyboard)
      wayland_keyboard_release(wl->wl_keyboard);
   /* (this was never destroyed; it went with the connection) */
   if (wl->wl_relative_pointer)
      zwp_relative_pointer_v1_destroy(wl->wl_relative_pointer);
   wl->wl_relative_pointer = NULL;
   if (wl->wl_pointer)
      wayland_pointer_release(wl->wl_pointer);
   if (wl->wl_touch)
      wayland_touch_release(wl->wl_touch);

   if (wl->cursor.surface)
      wl_surface_destroy(wl->cursor.surface);
   if (wl->cursor.theme)
      wl_cursor_theme_destroy(wl->cursor.theme);

   if (wl->viewport)
      wp_viewport_destroy(wl->viewport);
   if (wl->fractional_scale)
      wp_fractional_scale_v1_destroy(wl->fractional_scale);
   if (wl->tearing_control)
      wp_tearing_control_v1_destroy(wl->tearing_control);
   if (wl->idle_inhibitor)
      zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);
   if (wl->deco)
      zxdg_toplevel_decoration_v1_destroy(wl->deco);
   if (wl->xdg_toplevel)
      xdg_toplevel_destroy(wl->xdg_toplevel);
   if (wl->xdg_surface)
      xdg_surface_destroy(wl->xdg_surface);
#ifdef HAVE_LIBDECOR_H
   /* The frame owns xdg objects built on wl->surface and the context
    * still talks to the display, so release before both go away. */
   if (wl->libdecor_frame)
      wl->libdecor_frame_unref(wl->libdecor_frame);
   if (wl->libdecor_context)
      wl->libdecor_unref(wl->libdecor_context);
   if (wl->libdecor)
      dylib_close(wl->libdecor);
#endif
   /* Colour management objects go before the surface they describe */
   wl_color_destroy(&wl->color);
   video_driver_set_display_peak_nits(0.0f);
   if (wl->surface)
      wl_surface_destroy(wl->surface);

   if (wl->deco_manager)
      zxdg_decoration_manager_v1_destroy(wl->deco_manager);
   if (wl->xdg_toplevel_icon_manager)
      xdg_toplevel_icon_manager_v1_destroy(wl->xdg_toplevel_icon_manager);
   if (wl->xdg_toplevel_tag_manager)
      xdg_toplevel_tag_manager_v1_destroy(wl->xdg_toplevel_tag_manager);
   if (wl->idle_inhibit_manager)
      zwp_idle_inhibit_manager_v1_destroy(wl->idle_inhibit_manager);
   else
   {
#ifdef RARCH_HAVE_DBUS_SCREENSAVER
      dbus_close_connection();
#endif
   }
   if (wl->tearing_control_manager)
      wp_tearing_control_manager_v1_destroy(wl->tearing_control_manager);
   if (wl->pointer_constraints)
      zwp_pointer_constraints_v1_destroy(wl->pointer_constraints);
   if (wl->relative_pointer_manager)
      zwp_relative_pointer_manager_v1_destroy (wl->relative_pointer_manager);
   if (wl->content_type_manager)
      wp_content_type_manager_v1_destroy (wl->content_type_manager);
   if (wl->content_type)
      wp_content_type_v1_destroy (wl->content_type);
   if (wl->cursor_shape_manager)
      wp_cursor_shape_manager_v1_destroy (wl->cursor_shape_manager);
   if (wl->cursor_shape_device)
      wp_cursor_shape_device_v1_destroy (wl->cursor_shape_device);
   if (wl->single_pixel_manager)
      wp_single_pixel_buffer_manager_v1_destroy (wl->single_pixel_manager);
   if (wl->seat)
      wayland_seat_release(wl->seat);
   if (wl->xdg_shell)
      xdg_wm_base_destroy(wl->xdg_shell);
   if (wl->data_device)
   {
      if (wl_data_device_get_version(wl->data_device)
            >= WL_DATA_DEVICE_RELEASE_SINCE_VERSION)
         wl_data_device_release(wl->data_device);
      else
         wl_data_device_destroy(wl->data_device);
      wl->data_device = NULL;
   }
   if (wl->data_device_manager)
      wl_data_device_manager_destroy (wl->data_device_manager);
   while (!wl_list_empty(&wl->current_outputs))
   {
      surface_output_t *os = wl_container_of(wl->current_outputs.next, os, link);
      wl_list_remove(&os->link);
      free(os);
   }
   while (!wl_list_empty(&wl->all_outputs))
   {
      display_output_t *od = wl_container_of(wl->all_outputs.next, od, link);
      output_info_t    *oi = od->output;
      wayland_output_release(oi->output);
      wl_list_remove(&od->link);
      free(oi->make);
      free(oi->model);
#ifdef HAVE_VIDEO_WINDOWS
      free(oi->name);
#endif
      free(oi);
      free(od);
   }
   if (wl->shm)
      wl_shm_destroy (wl->shm);
   if (wl->viewporter)
      wp_viewporter_destroy(wl->viewporter);
   if (wl->fractional_scale_manager)
      wp_fractional_scale_manager_v1_destroy(wl->fractional_scale_manager);
   wl_present_destroy(&wl->present);
   wl_frame_destroy(&wl->frame);
   if (wl->compositor)
      wl_compositor_destroy(wl->compositor);
   if (wl->registry)
      wl_registry_destroy(wl->registry);

   if (wl->input.events)
      RARCH_DBG("[Wayland] Input events handled: %u, of them on a thread"
            " other than the frontend's: %u.\n",
            wl->input.events, wl->input.events_elsewhere);
   wl->input.events              = 0;
   wl->input.events_elsewhere    = 0;

   /* after the seat and its devices, which were on it, and before the
    * connection */
   if (wl->input.queue)
      wl_event_queue_destroy(wl->input.queue);
   wl->input.queue               = NULL;

   if (wl->input.dpy)
   {
      wl_display_flush(wl->input.dpy);
      wl_display_disconnect(wl->input.dpy);
   }

   wl->input.dpy                 = NULL;
   wl->registry                  = NULL;
   wl->compositor                = NULL;
   wl->shm                       = NULL;
   wl->data_device_manager       = NULL;
   wl->xdg_shell                 = NULL;
   wl->seat                      = NULL;
   wl->relative_pointer_manager  = NULL;
   wl->pointer_constraints       = NULL;
   wl->content_type              = NULL;
   wl->content_type_manager      = NULL;
   wl->cursor_shape_manager      = NULL;
   wl->cursor_shape_device       = NULL;
   wl->idle_inhibit_manager      = NULL;
   wl->deco_manager              = NULL;
   wl->single_pixel_manager      = NULL;
   wl->tearing_control_manager   = NULL;
   wl->tearing_control           = NULL;
   wl->surface                   = NULL;
   wl->xdg_surface               = NULL;
   wl->xdg_toplevel              = NULL;
   wl->xdg_toplevel_icon_manager = NULL;
   wl->xdg_toplevel_tag_manager  = NULL;
   wl->deco                      = NULL;
#ifdef HAVE_LIBDECOR_H
   wl->libdecor_frame            = NULL;
   wl->libdecor_context          = NULL;
   wl->libdecor                  = NULL;
#endif
   wl->idle_inhibitor            = NULL;
   wl->wl_touch                  = NULL;
   wl->wl_pointer                = NULL;
   wl->wl_keyboard               = NULL;

   wl->dims                     = 0;
   wl->buffer_dims              = 0;
}

void gfx_ctx_wl_update_title_common(void *data)
{
   char title[128];
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   title[0] = '\0';

   video_driver_get_window_title(title, sizeof(title));

   if (wl)
   {
#ifdef HAVE_LIBDECOR_H
      if (wl->libdecor)
      {
         if (title[0])
            wl->libdecor_frame_set_title(wl->libdecor_frame, title);
      }
      else
#endif
      {
         if (title[0])
         {
            if (wl->deco)
               zxdg_toplevel_decoration_v1_set_mode(wl->deco,
                     ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
            xdg_toplevel_set_title(wl->xdg_toplevel, title);
         }
      }
   }
}

static int create_shm_file(off_t size)
{
   int fd, ret;
#ifndef __FreeBSD__
   if ((fd = (int)syscall(SYS_memfd_create, SPLASH_SHM_NAME,
               MFD_CLOEXEC | MFD_ALLOW_SEALING)) >= 0)
#else
   if ((fd = memfd_create(SPLASH_SHM_NAME,
               MFD_CLOEXEC | MFD_ALLOW_SEALING)) >= 0)
#endif
   {
      fcntl(fd, F_ADD_SEALS, F_SEAL_SHRINK);

      do {
         ret = posix_fallocate(fd, 0, size);
      } while (ret == EINTR);
      if (ret != 0)
      {
         close(fd);
         errno = ret;
         fd    = -1;
      }
   }
   if (fd < 0)
   {
      unsigned retry_count;
      for (retry_count = 0; retry_count < 100; retry_count++)
      {
         char *name;
         if (asprintf(&name, "%s-%02d", SPLASH_SHM_NAME, retry_count) < 0)
            continue;
         if ((fd = shm_open(name, O_RDWR | O_CREAT, 0600)) >= 0)
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


static shm_buffer_t *create_shm_buffer(gfx_ctx_wayland_data_t *wl, int width,
   int height,
   uint32_t format)
{
   int fd;
   struct wl_shm_pool *pool = NULL;
   void *data               = NULL;
   shm_buffer_t *buffer     = NULL;
   int stride               = width  * 4;
   int size                 = stride * height;

   if (size <= 0)
      return NULL;

   if ((fd = create_shm_file(size)) < 0)
   {
      RARCH_ERR("[Wayland] [SHM] Creating a buffer file for %d B failed.\n",
         size);
      return NULL;
   }

   data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
   if (data == MAP_FAILED)
   {
      RARCH_ERR("[Wayland] [SHM] mmap failed.\n");
      close(fd);
      return NULL;
   }

   /* Guard the calloc before dereferencing buffer below.  The
    * previous form immediately wrote 'buffer->wl_buffer = ...' with
    * no check; an OOM returning NULL from calloc would segfault.
    * On OOM here we also have to munmap the region and close the
    * fd so we do not leak them - they were both acquired above
    * specifically to be owned by the shm_buffer_t we are about to
    * return. */
   if (!(buffer = calloc(1, sizeof *buffer)))
   {
      RARCH_ERR("[Wayland] [SHM] Out of memory allocating shm_buffer_t.\n");
      munmap(data, size);
      close(fd);
      return NULL;
   }

   pool              = wl_shm_create_pool(wl->shm, fd, size);
   buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0,
      width, height,
      stride, format);
   wl_buffer_add_listener(buffer->wl_buffer, &shm_buffer_listener, buffer);
   wl_shm_pool_destroy(pool);

   close(fd);

   buffer->data      = data;
   buffer->data_size = size;

      return buffer;
}

static void shm_buffer_paint_icon(
      shm_buffer_t *buffer,
      int width, int height, int scale,
      size_t icon_scale)
{
   int y, x;
   uint32_t *pixels = buffer->data;
   int stride       = width * scale;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint32_t color = (uint32_t)retroarch_icon_data[16 * ((y / icon_scale) % 16) + ((x / icon_scale) % 16)];
         int sx;
         if (color >> 4)
         {
            for (sx = 0; sx < scale; sx++)
            {
               int sy;
               for (sy = 0; sy < scale; sy++)
               {
                  size_t  off = x * scale + sx
                        + (y * scale + sy) * stride;
                     pixels[off] = color;
               }
            }
         }
      }
   }
}

static bool wl_create_toplevel_icon(gfx_ctx_wayland_data_t *wl, struct xdg_toplevel *toplevel)
{
   const int icon_size               = wl->buffer_scale > 1 ? 128 : 64;
   struct xdg_toplevel_icon_v1 *icon = xdg_toplevel_icon_manager_v1_create_icon(
      wl->xdg_toplevel_icon_manager);
   shm_buffer_t *icon_buffer;

   xdg_toplevel_icon_v1_set_name(icon, WAYLAND_APP_ID);

   icon_buffer = create_shm_buffer(wl,
      icon_size, icon_size, WL_SHM_FORMAT_ARGB8888);

   if (!icon_buffer)
   {
      RARCH_ERR("[Wayland] Failed to create toplevel icon buffer.\n");
      xdg_toplevel_icon_v1_destroy(icon);
      return false;
   }

   shm_buffer_paint_icon(icon_buffer, icon_size, icon_size, 1, icon_size / 16);
   xdg_toplevel_icon_v1_add_buffer(icon, icon_buffer->wl_buffer, 1);
   xdg_toplevel_icon_manager_v1_set_icon(
      wl->xdg_toplevel_icon_manager, toplevel, icon);

   /* The toplevel keeps its icon once set_icon is sent, and the buffer
    * only has to outlive the icon object, so neither is kept. */
   xdg_toplevel_icon_v1_destroy(icon);
   shm_buffer_free(icon_buffer);

   return true;
}

#ifndef HAVE_LIBDECOR_H
static void shm_buffer_paint_checkerboard(
      shm_buffer_t *buffer,
      int width, int height, int scale,
      size_t chk, uint32_t bg, uint32_t fg)
{
   int y, x;
   uint32_t *pixels = buffer->data;
   int stride       = width * scale;

   for (y = 0; y < height; y++)
   {
      for (x = 0; x < width; x++)
      {
         uint32_t color = (x & chk) ^ (y & chk) ? fg : bg;
         int sx;
         for (sx = 0; sx < scale; sx++)
         {
            int sy;
            for (sy = 0; sy < scale; sy++)
            {
               size_t  off = x * scale + sx
                     + (y * scale + sy) * stride;
               pixels[off] = color;
            }
         }
      }
   }
}

static bool wl_draw_splash_screen(gfx_ctx_wayland_data_t *wl)
{
   if (wl->single_pixel_manager)
   {
      struct wl_buffer *buffer = NULL;
      buffer = wp_single_pixel_buffer_manager_v1_create_u32_rgba_buffer(
         wl->single_pixel_manager, 0, 0, 0, UINT32_MAX);

      if (!buffer)
         return false;

      wl_surface_attach(wl->surface, buffer, 0, 0);
   }
   else
   {
      shm_buffer_t *buffer = create_shm_buffer(wl,
         VIDEO_SCALE_W(wl->buffer_dims),
         VIDEO_SCALE_H(wl->buffer_dims),
         WL_SHM_FORMAT_XRGB8888);

      if (!buffer)
         return false;

      shm_buffer_paint_checkerboard(buffer,
         VIDEO_SCALE_W(wl->buffer_dims),
         VIDEO_SCALE_H(wl->buffer_dims), 1,
         8, 0xffbcbcbc, 0xff8e8e8e);
      shm_buffer_paint_icon(buffer,
         VIDEO_SCALE_W(wl->buffer_dims),
         VIDEO_SCALE_H(wl->buffer_dims), 1,
         16);

      wl_surface_attach(wl->surface, buffer->wl_buffer, 0, 0);
   }

   if (wl_surface_get_version(wl->surface) >= WL_SURFACE_DAMAGE_BUFFER_SINCE_VERSION)
      wl_surface_damage_buffer(wl->surface, 0, 0,
         VIDEO_SCALE_W(wl->buffer_dims),
         VIDEO_SCALE_H(wl->buffer_dims));

   if (wl->viewport)
      wp_viewport_set_destination(wl->viewport,
            VIDEO_SCALE_W(wl->dims), VIDEO_SCALE_H(wl->dims));

   wl_surface_commit(wl->surface);

   return true;
}
#endif

#ifdef HAVE_VIDEO_WINDOWS
/* The outputs last published, on the context's thread */
static video_window_display_t wl_video_monitors[VIDEO_WINDOW_DISPLAYS_MAX];
static unsigned wl_video_monitors_count;

/* The outputs for the screens' Display setting, in the order the
 * fullscreen setting counts them (wl_output_at()). Published only when
 * they changed, or @always: another platform's list may be up. */
static void wl_video_monitors_publish(gfx_ctx_wayland_data_t *wl,
      bool always)
{
   video_window_display_t list[VIDEO_WINDOW_DISPLAYS_MAX];
   display_output_t *od;
   unsigned count               = 0;
   wl->video_monitors_dirty     = false;
   wl->video_monitors_own_dirty = false;
   wl_list_for_each(od, &wl->all_outputs, link)
   {
      video_window_display_t *display;
      output_info_t *oi = od->output;
      if (count >= VIDEO_WINDOW_DISPLAYS_MAX)
         break;
      display           = &list[count];
      memset(display, 0, sizeof(*display));
      display->ordinal  = ++count;
      display->own      = oi == wl->current_output;
      display->valid    = true;
      display->on       = true;
      display->physical = true;
      display->monitor  = true;
      if (oi->name)
         strlcpy(display->name, oi->name, sizeof(display->name));
      if (oi->model)
         utf8cpy(display->model, sizeof(display->model), oi->model,
               sizeof(display->model));
      display->dims     = oi->dims;
   }
   if (     !always
         && count == wl_video_monitors_count
         && !memcmp(list, wl_video_monitors, count * sizeof(*list)))
      return;
   memcpy(wl_video_monitors, list, count * sizeof(*list));
   wl_video_monitors_count = count;
   video_driver_publish_window_displays(list, count);
}
#endif

/* A window kept across a video reinit. The whole context is kept, so
 * every proxy still hands its listener a live context. */
static gfx_ctx_wayland_data_t *wl_kept = NULL;

static void gfx_ctx_wl_destroy(gfx_ctx_wayland_data_t *wl)
{
#ifdef WEBOS
   gfx_ctx_wl_destroy_resources_webos(wl);
#else
   gfx_ctx_wl_destroy_resources_common(wl);
#endif
   free(wl);
}

void gfx_ctx_wl_release_kept(void)
{
   gfx_ctx_wayland_data_t *wl = wl_kept;
   wl_kept                    = NULL;
   if (wl)
      gfx_ctx_wl_destroy(wl);
}

void gfx_ctx_wl_free_common(gfx_ctx_wayland_data_t *wl, bool may_keep)
{
   if (!wl)
      return;

   gfx_ctx_wl_release_kept();

   /* A content load reinits video without shutting the runloop down */
   if (     may_keep
         && wl->surface
         && !(runloop_get_flags() & RUNLOOP_FLAG_SHUTDOWN_INITIATED))
   {
      wl_frame_cancel(&wl->frame);
      /* The next context puts its own on the surface */
      if (wl->tearing_control)
         wp_tearing_control_v1_destroy(wl->tearing_control);
      wl->tearing_control = NULL;
      wl_color_detach(&wl->color);
      wl_display_flush(wl->input.dpy);
      wl_kept = wl;
      RARCH_LOG("[Wayland] Keeping the window across the video reinit.\n");
      return;
   }

#ifdef HAVE_VIDEO_WINDOWS
   /* Its outputs go with it, from its own thread. A kept window let go
    * by gfx_ctx_wl_release_kept() publishes nothing: the next driver
    * may have published by then. */
   wl_video_monitors_count = 0;
   video_driver_publish_window_displays(NULL, 0);
#endif
   gfx_ctx_wl_destroy(wl);
}

static void gfx_ctx_wl_adopt(gfx_ctx_wayland_data_t *wl,
      driver_configure_handler_t driver_configure_handler)
{
#ifdef HAVE_EGL
   static const egl_ctx_data_t egl_zero = {0};
#endif
#ifdef HAVE_VULKAN
   static const gfx_ctx_vulkan_data_t vk_zero = {0};
#endif

#ifdef HAVE_EGL
   wl->egl                      = egl_zero;
#endif
#ifdef HAVE_VULKAN
   wl->vk                       = vk_zero;
#endif
   wl->gl_gpu_list              = NULL;
   wl->driver_configure_handler = driver_configure_handler;
   /* Unlike a cold init there is no splash to protect */
   wl->ignore_configuration     = false;

   frontend_driver_destroy_signal_handler_state();
#ifndef WEBOS
   video_driver_display_type_set(RARCH_DISPLAY_WAYLAND);
#endif
   frontend_driver_install_signal_handler();
   /* Video uninit forgot which output the window is on */
   if (wl->current_output && wl->current_output->refresh_rate > 0)
      video_driver_set_window_refresh_rate(
            (float)wl->current_output->refresh_rate / 1000.0f);
}

gfx_ctx_wayland_data_t *gfx_ctx_wl_take_kept(
      driver_configure_handler_t driver_configure_handler)
{
   gfx_ctx_wayland_data_t *wl = wl_kept;
   if (!wl)
      return NULL;
   wl_kept = NULL;
   gfx_ctx_wl_adopt(wl, driver_configure_handler);
   RARCH_LOG("[Wayland] Took back the kept window.\n");
   return wl;
}

#ifdef HAVE_LIBDECOR_H
/* The icon and the tag of a window libdecor decorates. Returns whether
 * they could be set yet.
 *
 * libdecor makes the window's xdg_toplevel only once the compositor
 * has answered its own start-up. A decoration plugin's start-up waits
 * for that as a side effect, so with a plugin loaded the toplevel is
 * there straight after libdecor_decorate(). With none it is not - and
 * none is what there is when the only plugin installed is the GTK one
 * and the context starts off the main thread, as it does with
 * threaded video: that plugin refuses to start there ("Failed to load
 * plugin 'libdecor-gtk.so'", "No plugins found, falling back on no
 * decorations"). The toplevel then arrives with the first dispatch.
 *
 * Handing a request the NULL is an error libwayland does not recover
 * from ("null value passed for arg 0"): the connection is dead and
 * RetroArch exits. So this is called again once the window has been
 * configured. */
static bool gfx_ctx_wl_libdecor_identify(gfx_ctx_wayland_data_t *wl)
{
   struct xdg_toplevel *xdg_toplevel =
      wl->libdecor_frame_get_xdg_toplevel(wl->libdecor_frame);

   if (!xdg_toplevel)
      return false;

   if (wl->xdg_toplevel_icon_manager)
      wl_create_toplevel_icon(wl, xdg_toplevel);

   if (wl->xdg_toplevel_tag_manager)
   {
      xdg_toplevel_tag_manager_v1_set_toplevel_tag(
         wl->xdg_toplevel_tag_manager, xdg_toplevel, MAIN_WINDOW_TAG);
      xdg_toplevel_tag_manager_v1_set_toplevel_description(
         wl->xdg_toplevel_tag_manager, xdg_toplevel, DEFAULT_WINDOW_TITLE " " MAIN_WINDOW_TAG);
   }
   return true;
}
#endif

bool gfx_ctx_wl_init_common(
      driver_configure_handler_t driver_configure_handler,
      gfx_ctx_wayland_data_t **wwl)
{
   int i;
   gfx_ctx_wayland_data_t *wl;
   settings_t *settings         = config_get_ptr();
   unsigned video_monitor_index = settings->uints.video_monitor_index;

   /* Whichever Wayland context comes next takes back the kept window:
    * the previous one took its EGL window or Vulkan surface with it. */
   if ((*wwl = gfx_ctx_wl_take_kept(driver_configure_handler)))
      return true;

   *wwl                         = calloc(1, sizeof(gfx_ctx_wayland_data_t));
   wl                           = *wwl;

   if (!wl)
      return false;

#ifdef HAVE_LIBDECOR_H
#ifdef HAVE_DYLIB
   if ((wl->libdecor = dylib_load("libdecor-0.so.0")))
   {
#define RA_WAYLAND_SYM(rc,fn,params) wl->fn = (rc (*) params)dylib_proc(wl->libdecor, #fn);
#include "wayland/libdecor_sym.h"
   }
#endif
#endif

   wl->driver_configure_handler = driver_configure_handler;

   wl_list_init(&wl->all_outputs);
   wl_list_init(&wl->current_outputs);

   frontend_driver_destroy_signal_handler_state();

   wl->input.dpy                    = wl_display_connect(NULL);
   /* The seat's events get a queue of their own, which only the input
    * driver's poll dispatches; see wayland_input_dispatch().
    * RETROARCH_WAYLAND_INPUT_QUEUE=0 in the environment leaves them
    * on the default queue, as they were. */
   wl->input.queue                  = NULL;
   wl->input.events                 = 0;
   wl->input.events_elsewhere       = 0;
#ifdef WAYLAND_HAVE_INPUT_QUEUE
   if (wl->input.dpy)
   {
      const char *off = getenv("RETROARCH_WAYLAND_INPUT_QUEUE");
      if (!off || off[0] != '0')
         wl->input.queue            = wl_display_create_queue(wl->input.dpy);
   }
#endif
   wl->last_buffer_scale            = 1;
   wl->buffer_scale                 = 1;
   wl->pending_buffer_scale         = 1;
   wl->last_fractional_scale_num    = FRACTIONAL_SCALE_V1_DEN;
   wl->fractional_scale_num         = FRACTIONAL_SCALE_V1_DEN;
   wl->pending_fractional_scale_num = FRACTIONAL_SCALE_V1_DEN;
   wl->floating_dims                = VIDEO_SCALE_PACK(
         SPLASH_WINDOW_WIDTH, SPLASH_WINDOW_HEIGHT);

   if (!wl->input.dpy)
   {
      RARCH_ERR("[Wayland] Failed to connect to Wayland server.\n");
      return false;
   }

   /* The display server for this session is Wayland's, as X11's is set
    * by its own contexts; it keeps a connection of its own and never
    * waits on it. */
   video_driver_display_type_set(RARCH_DISPLAY_WAYLAND);

   frontend_driver_install_signal_handler();

   wl->registry = wl_display_get_registry(wl->input.dpy);
   wl_registry_add_listener(wl->registry, &registry_listener, wl);
   /* first roundtrip to bind compositor globals */
   wl_display_roundtrip(wl->input.dpy);
   /* second roundtrip for listeners on bound globals (wl_output, wl_seat) */
   wl_display_roundtrip(wl->input.dpy);
   /* the seat's are on the input queue: its capabilities, and with
    * them the keyboard and the pointer, are wanted before the window
    * is set up, as they were when the roundtrip above brought them.
    * The input driver is not polling yet, so nothing else dispatches
    * this queue. */
#ifdef WAYLAND_HAVE_INPUT_QUEUE
   if (wl->input.queue)
      wl_display_roundtrip_queue(wl->input.dpy, wl->input.queue);
#endif

   /* The display's peak luminance, as the compositor describes the
    * output the surface is on or else the first it lists; it arrives
    * with the events dispatched from here on. */
   {
      struct wl_output *output = NULL;
      if (wl->current_output)
         output = wl->current_output->output;
      else if (!wl_list_empty(&wl->all_outputs))
      {
         display_output_t *d = wl_container_of(wl->all_outputs.next, d, link);
         output = d->output->output;
      }
      wl->color.peak_cb = video_driver_set_display_peak_nits;
      wl_color_query_output(&wl->color, output);
   }

   if (!wl->compositor)
   {
      RARCH_ERR("[Wayland] Failed to create compositor.\n");
      return false;
   }

   if (!wl->shm)
   {
      RARCH_ERR("[Wayland] Failed to create shm.\n");
      return false;
   }

   if (!wl->xdg_shell)
   {
      RARCH_ERR("[Wayland] Failed to create shell.\n");
      return false;
   }

   if (!wl->idle_inhibit_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", zwp_idle_inhibit_manager_v1_interface.name);
#ifdef RARCH_HAVE_DBUS_SCREENSAVER
      dbus_ensure_connection();
#endif
   }

   if (!wl->deco_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", zxdg_decoration_manager_v1_interface.name);
   }

   if (!wl->viewporter)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", wp_viewporter_interface.name);
   }

   if (!wl->fractional_scale_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", wp_fractional_scale_manager_v1_interface.name);
   }

   if (!wl->cursor_shape_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", wp_cursor_shape_manager_v1_interface.name);
   }

   if (!wl->content_type_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", wp_content_type_manager_v1_interface.name);
   }

   if (!wl->pointer_constraints)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", zwp_pointer_constraints_v1_interface.name);
   }

   if (!wl->relative_pointer_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", zwp_relative_pointer_manager_v1_interface.name);
   }

   if (!wl->single_pixel_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", wp_single_pixel_buffer_manager_v1_interface.name);
   }

   if (!wl->xdg_toplevel_icon_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", xdg_toplevel_icon_manager_v1_interface.name);
   }

   if (!wl->xdg_toplevel_tag_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", xdg_toplevel_tag_manager_v1_interface.name);
   }

   if (!wl->present.presentation)
   {
      RARCH_LOG("[Wayland]: Compositor doesn't support the %s protocol.\n", wp_presentation_interface.name);
   }


   if (!wl->tearing_control_manager)
   {
      RARCH_LOG("[Wayland] Compositor doesn't support the %s protocol.\n", wp_tearing_control_manager_v1_interface.name);
   }

   wl->surface = wl_compositor_create_surface(wl->compositor);
   if (wl->viewporter)
      wl->viewport = wp_viewporter_get_viewport(wl->viewporter, wl->surface);

   if (wl->fractional_scale_manager)
   {
      wl->fractional_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
           wl->fractional_scale_manager, wl->surface);
      wp_fractional_scale_v1_add_listener(wl->fractional_scale, &wp_fractional_scale_v1_listener, wl);
   }

   if (wl->content_type_manager)
   {
      wl->content_type = wp_content_type_manager_v1_get_surface_content_type(wl->content_type_manager, wl->surface);
      wp_content_type_v1_set_content_type(wl->content_type, WP_CONTENT_TYPE_V1_TYPE_GAME);
   }

   wl_surface_add_listener(wl->surface, &wl_surface_listener, wl);

#ifdef HAVE_LIBDECOR_H
   if (wl->libdecor)
      wl->libdecor_context = wl->libdecor_new(wl->input.dpy, &libdecor_interface);

   /* libdecor_new() can fail (e.g. no usable decoration plugin could be
    * loaded). Treat that as libdecor being unavailable and fall back to
    * plain xdg-shell, rather than passing a NULL context to
    * libdecor_decorate(). Clearing wl->libdecor keeps every downstream
    * "if (wl->libdecor)" branch (window setup and fullscreen toggling)
    * consistently on the xdg-shell path. */
   if (wl->libdecor && !wl->libdecor_context)
   {
      RARCH_WARN("[Wayland] libdecor failed to initialize, falling back to xdg-shell decorations.\n");
      dylib_close(wl->libdecor);
      wl->libdecor = NULL;
   }

   if (wl->libdecor)
   {
      bool identified;

      wl->libdecor_frame = wl->libdecor_decorate(wl->libdecor_context, wl->surface, &wl_libdecor_frame_interface, wl);
      if (!wl->libdecor_frame)
      {
         RARCH_ERR("[Wayland] Failed to create libdecor frame.\n");
         return false;
      }

      /* The window's icon and tag, if libdecor has made the toplevel
       * already; see gfx_ctx_wl_libdecor_identify(). */
      identified = gfx_ctx_wl_libdecor_identify(wl);

      wl->libdecor_frame_set_app_id(wl->libdecor_frame, WAYLAND_APP_ID);
      wl->libdecor_frame_set_title(wl->libdecor_frame, DEFAULT_WINDOW_TITLE);
      wl->libdecor_frame_map(wl->libdecor_frame);

      /* Waiting for libdecor to be configured before starting to draw */
      wl_surface_commit(wl->surface);
      wl->configured = true;

      while (wl->configured)
      {
         if (wl->libdecor_dispatch(wl->libdecor_context, 0) < 0)
         {
            RARCH_ERR("[Wayland] libdecor failed to dispatch.\n");
            return false;
         }
      }

      /* Configured, so the toplevel is there now if it was not
       * before; nothing has been drawn to it yet. */
      if (!identified)
         gfx_ctx_wl_libdecor_identify(wl);
   }
   else
#endif
   {
      wl->xdg_surface = xdg_wm_base_get_xdg_surface(wl->xdg_shell, wl->surface);
      xdg_surface_add_listener(wl->xdg_surface, &wl_xdg_surface_listener, wl);

      wl->xdg_toplevel = xdg_surface_get_toplevel(wl->xdg_surface);
      xdg_toplevel_add_listener(wl->xdg_toplevel, &wl_xdg_toplevel_listener, wl);

      xdg_toplevel_set_app_id(wl->xdg_toplevel, WAYLAND_APP_ID);
      xdg_toplevel_set_title(wl->xdg_toplevel, DEFAULT_WINDOW_TITLE);

      if (wl->deco_manager)
         wl->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
               wl->deco_manager, wl->xdg_toplevel);

      if (wl->xdg_toplevel_icon_manager)
         wl_create_toplevel_icon(wl, wl->xdg_toplevel);

      if (wl->xdg_toplevel_tag_manager)
      {
         xdg_toplevel_tag_manager_v1_set_toplevel_tag(
            wl->xdg_toplevel_tag_manager, wl->xdg_toplevel, MAIN_WINDOW_TAG);
         xdg_toplevel_tag_manager_v1_set_toplevel_description(
            wl->xdg_toplevel_tag_manager, wl->xdg_toplevel, DEFAULT_WINDOW_TITLE " " MAIN_WINDOW_TAG);
      }

      /* Waiting for xdg_toplevel to be configured before starting to draw */
      wl_surface_commit(wl->surface);
      wl->configured = true;

      while (wl->configured)
         wl_display_dispatch(wl->input.dpy);
   }

   wl_display_roundtrip(wl->input.dpy);
   xdg_wm_base_add_listener(wl->xdg_shell, &xdg_shell_listener, NULL);

#ifndef HAVE_LIBDECOR_H
   /* Bind SHM based wl_buffer to wl_surface until the vulkan surface is ready.
    * This shows the window which assigns us a display (wl_output)
    * which is useful for HiDPI and auto selecting a display for fullscreen. */
   if (video_monitor_index == 0 && wl_list_length (&wl->all_outputs) > 1)
   {
      if (!wl_draw_splash_screen(wl))
         RARCH_ERR("[Wayland] Failed to draw splash screen.\n");

      /* Make sure splash screen is on screen and sized */
      wl->configured = true;

      while (wl->configured)
      wl_display_dispatch(wl->input.dpy);
   }
#endif

   /* Ignore configure events until splash screen has been replaced */
   wl->ignore_configuration = true;

   wl->input.fd = wl_display_get_fd(wl->input.dpy);

   wl->input.keyboard_focus  = true;
   wl->input.mouse.focus     = true;

   wl->cursor.surface        = wl_compositor_create_surface(wl->compositor);
   gfx_ctx_wl_cursor_load(wl);

   wl->num_active_touches    = 0;

   for (i = 0; i < MAX_TOUCHES; i++)
   {
       wl->active_touch_positions[i].active = false;
       wl->active_touch_positions[i].id     = -1;
       wl->active_touch_positions[i].x      = (unsigned) 0;
       wl->active_touch_positions[i].y      = (unsigned) 0;
   }

   flush_wayland_fd(&wl->input);
#ifdef HAVE_VIDEO_WINDOWS
   wl_video_monitors_publish(wl, true);
#endif

   return true;
}

static void gfx_ctx_wl_wait_for_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen);

/* Windowed geometry must wait until fullscreen is no longer current. */
static void gfx_ctx_wl_leave_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool wait_for_configure)
{
   if (wait_for_configure && !wl->fullscreen && !wl->fullscreen_pending)
      return;
   wl->fullscreen_requested = false;
   wl->fullscreen_pending = wl->fullscreen_pending || wl->fullscreen;
#ifdef HAVE_LIBDECOR_H
   if (wl->libdecor)
      wl->libdecor_frame_unset_fullscreen(wl->libdecor_frame);
   else
#endif
      xdg_toplevel_unset_fullscreen(wl->xdg_toplevel);
   if (wait_for_configure)
      gfx_ctx_wl_wait_for_fullscreen(wl, false);
}

bool gfx_ctx_wl_set_video_mode_common_size(gfx_ctx_wayland_data_t *wl,
      unsigned width, unsigned height, bool fullscreen)
{
   settings_t *settings         = config_get_ptr();
   unsigned video_monitor_index = settings->uints.video_monitor_index;

   wl->dims          = VIDEO_SCALE_PACK(
         width  ? width  : DEFAULT_WINDOWED_WIDTH,
         height ? height : DEFAULT_WINDOWED_HEIGHT);
   wl->buffer_dims   = wl->dims;

   if (!fullscreen)
   {
      /* What a configure leaving the size to us restores */
      wl->floating_dims        = wl->dims;
      wl->buffer_scale         = wl->pending_buffer_scale;
      wl->fractional_scale_num = wl->pending_fractional_scale_num;
      wl->buffer_dims          = wl_scaled_dims(wl->buffer_dims,
            wl->fractional_scale != NULL, wl->fractional_scale_num,
            wl->buffer_scale);

      /* out of fullscreen first, then the windowed size below */
      gfx_ctx_wl_leave_fullscreen(wl, true);
   }
   if (wl->viewport)
   {
      /* Stretch old buffer to fill new size, commit/roundtrip to apply */
      wp_viewport_set_destination(wl->viewport,
            VIDEO_SCALE_W(wl->dims), VIDEO_SCALE_H(wl->dims));
   }

#ifdef HAVE_LIBDECOR_H
   if (wl->libdecor)
   {
      struct libdecor_state *state;

      wl->libdecor_frame_set_visibility(wl->libdecor_frame, !fullscreen);
      state = wl->libdecor_state_new(
            VIDEO_SCALE_W(wl->dims), VIDEO_SCALE_H(wl->dims));
      wl->libdecor_frame_commit(wl->libdecor_frame, state, NULL);
      wl->libdecor_state_free(state);
   }
#endif

   return true;
}

#define FULLSCREEN_CONFIGURE_TIMEOUT_MS 500

static void gfx_ctx_wl_wait_for_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen)
{
   struct timespec start, now;
   int remaining = FULLSCREEN_CONFIGURE_TIMEOUT_MS;

   clock_gettime(CLOCK_MONOTONIC, &start);

   while ((wl->fullscreen_pending || wl->fullscreen != fullscreen)
         && remaining > 0)
   {
#ifdef HAVE_LIBDECOR_H
      if (wl->libdecor)
      {
         int ret = wl->libdecor_dispatch(wl->libdecor_context, remaining);
         if (ret < 0 && ret != -EINTR)
         {
            RARCH_ERR("[Wayland] libdecor failed to dispatch.\n");
            break;
         }
      }
      else
#endif
      {
         struct pollfd fd;
         int ret;

         flush_wayland_fd(&wl->input);
         if (!wl->fullscreen_pending && wl->fullscreen == fullscreen)
            break;

         fd.fd      = wl->input.fd;
         fd.events  = POLLIN;
         fd.revents = 0;
         ret        = poll(&fd, 1, remaining);
         if (ret < 0)
         {
            if (errno != EINTR)
               break;
         }
         else if (ret > 0 && (fd.revents & (POLLERR | POLLHUP | POLLNVAL)))
            break;
      }

      clock_gettime(CLOCK_MONOTONIC, &now);
      remaining = FULLSCREEN_CONFIGURE_TIMEOUT_MS - (int)(
              (now.tv_sec  - start.tv_sec)  * 1000
            + (now.tv_nsec - start.tv_nsec) / 1000000);
   }

   if (wl->fullscreen_pending || wl->fullscreen != fullscreen)
      RARCH_WARN("[Wayland] No %s configure received; continuing.\n",
            fullscreen ? "fullscreen" : "windowed");
}

/* Output 'monitor', counted from 1 as the monitor index setting counts
 * them; NULL for 0 or one there is not */
static output_info_t *wl_output_at(gfx_ctx_wayland_data_t *wl,
      unsigned monitor)
{
   display_output_t *od;
   unsigned i = 0;
   if (monitor)
   {
      wl_list_for_each(od, &wl->all_outputs, link)
      {
         if (++i == monitor)
            return od->output;
      }
   }
   return NULL;
}

static bool gfx_ctx_wl_set_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen, bool wait_for_configure)
{
   settings_t *settings         = config_get_ptr();
   unsigned video_monitor_index = settings->uints.video_monitor_index;

   wl->fullscreen_requested = fullscreen;
   wl->fullscreen_pending = wl->fullscreen_pending
      || wl->fullscreen != fullscreen;

   if (fullscreen)
   {
      output_info_t *oi;
      struct wl_output *output = NULL;

#ifdef HAVE_LIBDECOR_H
      if (video_monitor_index <= 0)
      {
         RARCH_LOG("[Wayland] Auto fullscreen monitor index, letting compositor decide.\n");
      }
#else
      if (video_monitor_index <= 0 && wl->current_output != NULL)
      {
         oi     = wl->current_output;
         output = oi->output;
         RARCH_LOG("[Wayland] Auto fullscreen on display \"%s\" \"%s\".\n", oi->make, oi->model);
      }
#endif
      else
      {
         if ((oi = wl_output_at(wl, video_monitor_index)))
         {
            output = oi->output;
            RARCH_LOG("[Wayland] Fullscreen on display %i \"%s\" \"%s\".\n", (int)video_monitor_index, oi->make, oi->model);
         }

         if (!output)
            RARCH_LOG("[Wayland] Failed to specify monitor for fullscreen, letting compositor decide.\n");
      }

#ifdef HAVE_LIBDECOR_H
      if (wl->libdecor)
         wl->libdecor_frame_set_fullscreen(wl->libdecor_frame, output);
      else
#endif
      {
         xdg_toplevel_set_fullscreen(wl->xdg_toplevel, output);
      }

      /* Map only once fullscreen, or window managers such as tiling
       * scripts place the window as a normal one first. */
      if (wait_for_configure)
         gfx_ctx_wl_wait_for_fullscreen(wl, true);
   }
   else
      /* A window kept across the reinit that is still fullscreen:
       * gfx_ctx_wl_set_video_mode_common_size() took it out already,
       * before sizing it; this is for a caller that did not go
       * through there. */
      gfx_ctx_wl_leave_fullscreen(wl, wait_for_configure);

   if (wait_for_configure)
      flush_wayland_fd(&wl->input);
   else
      wl_display_flush(wl->input.dpy);

   if (fullscreen)
   {
      wl->cursor.visible = false;
      gfx_ctx_wl_show_mouse(wl, false);
   }
   else
      wl->cursor.visible = true;

   return true;
}

bool gfx_ctx_wl_set_video_mode_common_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen)
{
   return gfx_ctx_wl_set_fullscreen(wl, fullscreen, true);
}

bool gfx_ctx_wl_request_fullscreen(gfx_ctx_wayland_data_t *wl,
      bool fullscreen)
{
   return gfx_ctx_wl_set_fullscreen(wl, fullscreen, false);
}

bool gfx_ctx_wl_suppress_screensaver(void *data, bool state)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl->idle_inhibit_manager)
#ifdef RARCH_HAVE_DBUS_SCREENSAVER
      /* Some Wayland compositors (e.g. Phoc) don't implement Wayland's Idle protocol.
       * They instead rely on things like Gnome Screensaver. */
      return dbus_suspend_screensaver(state);
#else
      return false;
#endif
   if (state != (!!wl->idle_inhibitor))
   {
      if (state)
      {
         struct zwp_idle_inhibit_manager_v1 *mgr = wl->idle_inhibit_manager;
         RARCH_LOG("[Wayland] Enabling idle inhibitor.\n");
         wl->idle_inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(mgr, wl->surface);
      }
      else
      {
         RARCH_LOG("[Wayland] Disabling the idle inhibitor.\n");
         zwp_idle_inhibitor_v1_destroy(wl->idle_inhibitor);
         wl->idle_inhibitor = NULL;
      }
   }

   return true;
}


bool gfx_ctx_wl_has_focus(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return wl->input.keyboard_focus;
}

void gfx_ctx_wl_check_window_common(gfx_ctx_wayland_data_t *wl,
      void (*get_video_size)(void*, unsigned*), bool *quit,
      bool *resize, unsigned *dims)
{
   /* this function works with SCALED sizes, it's used from the renderer */
   unsigned new_dims;
   flush_wayland_fd(&wl->input);
#ifdef HAVE_VIDEO_WINDOWS
   if (wl->video_monitors_dirty || wl->video_monitors_own_dirty)
      wl_video_monitors_publish(wl, false);
#endif

   get_video_size(wl, &new_dims);

   if (     wl->pending_buffer_scale != wl->buffer_scale
         || wl->pending_fractional_scale_num != wl->fractional_scale_num
         || new_dims != *dims)
   {
      wl->buffer_scale         = wl->pending_buffer_scale;
      wl->fractional_scale_num = wl->pending_fractional_scale_num;
      *dims                    = new_dims;
      *resize                  = true;
   }

   *quit = (bool)frontend_driver_get_signal_handler_state();
}

#ifdef HAVE_VIDEO_WINDOWS
/* Focus records the seat's handlers can queue, at least, before the
 * context's thread takes them */
#define WL_VIDEO_WINDOW_FOCUS_QUEUE 32

typedef struct
{
   unsigned screen;
   unsigned generation;
   unsigned focused;
} wl_video_window_focus_t;

/* By screen, what the seat's handlers know a secondary by: its surface
 * and generation, written by the context's thread (the generation
 * first, cleared last), and its keyboard focus, written by the
 * handlers on whichever thread handles the seat. */
typedef struct
{
   retro_atomic_size_t surface;
   retro_atomic_int_t  generation;
   /* The generation that has the keyboard, 0 for none */
   retro_atomic_int_t  focused;
   /* A generation whose focus loss found the queue full */
   retro_atomic_int_t  lost;
} wl_video_window_ids_t;

static wl_video_window_t *wl_video_windows[RETRO_VIDEO_VIEWS_MAX];
static wl_video_window_ids_t wl_video_window_ids[RETRO_VIDEO_VIEWS_MAX];
#ifdef HAVE_THREADS
/* Focus from the seat's handlers on a thread that is not the
 * context's: the frontend's, which polls the input queue */
static retro_spsc_t wl_video_window_queue;
static bool wl_video_window_queue_made;
static retro_atomic_size_t wl_video_window_thread;
#endif

/* On the context's thread */
static void wl_video_window_focus(unsigned screen, unsigned generation,
      bool focused)
{
   wl_video_window_t *state = screen < RETRO_VIDEO_VIEWS_MAX
      ? wl_video_windows[screen] : NULL;
   if (     !state || state->common.closed
         || state->common.request.generation != generation
         || state->common.native->focused   == focused)
      return;
   state->common.native->focused = focused;
   if (focused)
      state->common.pending     |=  VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN);
   else
   {
      state->common.pending     &= ~VIDEO_WINDOW_EVENT_BIT(FOCUS_GAIN);
      state->common.pending     |=  VIDEO_WINDOW_EVENT_BIT(FOCUS_LOSS);
   }
}

void wl_video_window_keyboard(gfx_ctx_wayland_data_t *wl,
      struct wl_surface *surface, bool focused)
{
   unsigned i;
   if (!surface || surface == wl->surface)
      return;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      wl_video_window_ids_t *ids = &wl_video_window_ids[i];
      int generation = retro_atomic_load_acquire_int(&ids->generation);
      if (     !generation
            || retro_atomic_load_acquire_size(&ids->surface)
               != (size_t)(uintptr_t)surface
            || retro_atomic_load_acquire_int(&ids->generation) != generation)
         continue;
      retro_atomic_store_release_int(&ids->focused,
            focused ? generation : 0);
#ifdef HAVE_THREADS
      if (     sthread_get_current_thread_id()
            != (uintptr_t)retro_atomic_load_acquire_size(
               &wl_video_window_thread))
      {
         wl_video_window_focus_t record;
         record.screen     = i;
         record.generation = (unsigned)generation;
         record.focused    = focused;
         if (retro_spsc_write_avail(&wl_video_window_queue)
               >= sizeof(record))
            retro_spsc_write(&wl_video_window_queue, &record,
                  sizeof(record));
         else if (!focused)
            retro_atomic_store_release_int(&ids->lost, generation);
         return;
      }
#endif
      wl_video_window_focus(i, (unsigned)generation, focused);
      return;
   }
}

/* What was queued, a loss the queue had no room for, then where the
 * focus is now: the last is what the window ends up with. */
static void wl_video_windows_take_focus(void)
{
   unsigned i;
#ifdef HAVE_THREADS
   wl_video_window_focus_t record;
   while (     wl_video_window_queue_made
         && retro_spsc_read_avail(&wl_video_window_queue) >= sizeof(record))
   {
      retro_spsc_read(&wl_video_window_queue, &record, sizeof(record));
      wl_video_window_focus(record.screen, record.generation,
            record.focused != 0);
   }
#endif
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
   {
      int lost;
      wl_video_window_ids_t *ids = &wl_video_window_ids[i];
      wl_video_window_t *state   = wl_video_windows[i];
      if (     retro_atomic_load_relaxed_int(&ids->lost)
            && (lost = retro_atomic_fetch_and_int(&ids->lost, 0)))
         wl_video_window_focus(i, (unsigned)lost, false);
      if (state)
         wl_video_window_focus(i, state->common.request.generation,
                  retro_atomic_load_acquire_int(&ids->focused)
               == (int)state->common.request.generation);
   }
}

static video_window_common_t *wl_video_window_at(unsigned screen)
{
   return wl_video_windows[screen]
      ? &wl_video_windows[screen]->common : NULL;
}

static output_info_t *wl_video_window_output(void *owner,
      struct wl_output *output)
{
   display_output_t *od;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)owner;
   if (output)
   {
      wl_list_for_each(od, &wl->all_outputs, link)
      {
         if (od->output->output == output)
            return od->output;
      }
   }
   return NULL;
}

/* The output the window counts as being on: of those it is on, the
 * one with the largest scale, as for the main window */
static output_info_t *wl_video_window_on(const wl_video_window_t *state)
{
   unsigned i;
   output_info_t *on = NULL;
   for (i = 0; i < WL_VIDEO_WINDOW_OUTPUTS; i++)
   {
      output_info_t *oi = wl_video_window_output(state->common.owner,
            state->outputs[i]);
      if (oi && (!on || oi->scale > on->scale))
         on = oi;
   }
   return on;
}

/* The buffer the surface's size takes at the window's scale */
static void wl_video_window_resized(wl_video_window_t *state)
{
   unsigned dims;
   video_window_native_t *window = state->common.native;
   /* A closed window reports nothing more */
   if (state->common.closed)
      return;
   dims = wl_scaled_dims(state->dims, state->fractional_scale != NULL,
         state->fractional_scale_num, state->scale);
   if (dims != window->dims)
   {
      window->dims    = dims;
      state->common.pending |= VIDEO_WINDOW_EVENT_BIT(GEOMETRY);
   }
}

/* The viewport gives the surface its size whatever the buffer's, so a
 * secondary never sets a buffer scale: without one it stays at 1. */
static void wl_video_window_rescale(wl_video_window_t *state)
{
   output_info_t *on;
   if (!state->viewport)
      return;
   if (state->preferred_buffer_scale)
      state->scale = state->preferred_buffer_scale;
   else if ((on = wl_video_window_on(state)) && on->scale > 0)
      state->scale = on->scale;
   wl_video_window_resized(state);
}

void wl_video_windows_rescale(void)
{
   unsigned i;
   for (i = 1; i < RETRO_VIDEO_VIEWS_MAX; i++)
      if (wl_video_windows[i])
         wl_video_window_rescale(wl_video_windows[i]);
}

static void wl_video_window_surface_enter(void *data,
      struct wl_surface *surface, struct wl_output *output)
{
   unsigned i;
   wl_video_window_t *state = (wl_video_window_t*)data;
   /* Shown: the compositor may still size it for a while */
   if (!state->mapped && state->common.native->configured)
   {
      state->mapped = true;
      video_window_unsettled(&state->common, cpu_features_get_time_usec());
   }
   for (i = 0; output && i < WL_VIDEO_WINDOW_OUTPUTS; i++)
      if (state->outputs[i] == output)
         return;
   /* An output since removed leaves its place free */
   for (i = 0; output && i < WL_VIDEO_WINDOW_OUTPUTS; i++)
   {
      if (     !state->outputs[i]
            || !wl_video_window_output(state->common.owner,
               state->outputs[i]))
      {
         state->outputs[i] = output;
         break;
      }
   }
   wl_video_window_rescale(state);
}

static void wl_video_window_surface_leave(void *data,
      struct wl_surface *surface, struct wl_output *output)
{
   unsigned i;
   wl_video_window_t *state = (wl_video_window_t*)data;
   for (i = 0; output && i < WL_VIDEO_WINDOW_OUTPUTS; i++)
      if (state->outputs[i] == output)
         state->outputs[i] = NULL;
   wl_video_window_rescale(state);
}

#ifdef WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION
static void wl_video_window_preferred_buffer_scale(void *data,
      struct wl_surface *surface, int32_t factor)
{
   wl_video_window_t *state = (wl_video_window_t*)data;
   if (factor < 1)
      return;
   state->preferred_buffer_scale = (unsigned)factor;
   wl_video_window_rescale(state);
}

static void wl_video_window_preferred_buffer_transform(void *data,
      struct wl_surface *surface, uint32_t transform) { }
#endif

static const struct wl_surface_listener wl_video_window_surface_listener = {
   wl_video_window_surface_enter,
   wl_video_window_surface_leave,
#ifdef WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION
   wl_video_window_preferred_buffer_scale,
   wl_video_window_preferred_buffer_transform,
#endif
};

static void wl_video_window_preferred_scale(void *data,
      struct wp_fractional_scale_v1 *fractional_scale, uint32_t scale)
{
   wl_video_window_t *state     = (wl_video_window_t*)data;
   state->fractional_scale_num = scale;
   wl_video_window_resized(state);
}

static const struct wp_fractional_scale_v1_listener
wl_video_window_fractional_listener = {
   wl_video_window_preferred_scale,
};

/* The size a configuration gives: the compositor's, the window's own
 * for an axis of 0, and once the window floats again the one asked for
 * while it did not */
static unsigned wl_video_window_size(wl_video_window_t *state,
      unsigned dims, bool floating)
{
   if (floating && state->floating_asked)
   {
      state->floating_asked = false;
      return state->floating_dims;
   }
   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return state->floating_dims;
   if (floating)
      state->floating_dims = dims;
   return dims;
}

/* A configuration the compositor made current, at the size it gives */
static void wl_video_window_configure(wl_video_window_t *state,
      unsigned dims, bool fullscreen, bool suspended, bool floating)
{
   video_window_native_t *window = state->common.native;
   retro_time_t now              = cpu_features_get_time_usec();
   if (state->common.closed)
      return;
   /* Before it is shown, while it is suspended and as it comes back,
    * the compositor sizes it */
   if (!state->mapped || suspended || state->suspended)
      video_window_unsettled(&state->common, now);
   state->dims       = dims;
   state->fullscreen = fullscreen;
   state->floating   = floating;
   if (state->viewport)
   {
      wp_viewport_set_destination(state->viewport,
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      wl_surface_commit(state->surface);
   }
   wl_video_window_resized(state);
   /* Suspended (xdg_wm_base v6) is not shown: skipped, not closed */
   if (!window->configured || suspended != state->suspended)
   {
      state->common.pending &= ~(VIDEO_WINDOW_EVENT_BIT(SUSPEND)
                               | VIDEO_WINDOW_EVENT_BIT(RESUME));
      state->common.pending |= suspended ? VIDEO_WINDOW_EVENT_BIT(SUSPEND)
                                         : VIDEO_WINDOW_EVENT_BIT(RESUME);
   }
   state->suspended    = suspended;
   window->configured  = true;
   window->presentable = !suspended;
   video_window_moved(&state->common, 0, false, dims,
         floating && !fullscreen
         && !state->common.request.settings.fullscreen, now);
}

static void wl_video_window_toplevel_configure(void *data,
      struct xdg_toplevel *toplevel, int32_t width, int32_t height,
      struct wl_array *states)
{
   const uint32_t *s;
   wl_video_window_t *state = (wl_video_window_t*)data;
   /* Pending until xdg_surface.configure */
   state->cfg_dims          = VIDEO_SCALE_PACK(width  > 0 ? width  : 0,
                                               height > 0 ? height : 0);
   state->cfg_fullscreen    = false;
   state->cfg_suspended     = false;
   state->cfg_floating      = true;
   WL_ARRAY_FOR_EACH(s, states, const uint32_t*)
   {
      switch (*s)
      {
         case XDG_TOPLEVEL_STATE_FULLSCREEN:
            state->cfg_fullscreen = true;
            /* fall-through */
         case XDG_TOPLEVEL_STATE_MAXIMIZED:
         case XDG_TOPLEVEL_STATE_TILED_LEFT:
         case XDG_TOPLEVEL_STATE_TILED_RIGHT:
         case XDG_TOPLEVEL_STATE_TILED_TOP:
         case XDG_TOPLEVEL_STATE_TILED_BOTTOM:
            state->cfg_floating   = false;
            break;
         case XDG_TOPLEVEL_STATE_SUSPENDED:
            state->cfg_suspended  = true;
            break;
      }
   }
}

/* Unlike the main window's, this close does not quit */
static void wl_video_window_toplevel_close(void *data,
      struct xdg_toplevel *toplevel)
{
   video_window_mark_closed(&((wl_video_window_t*)data)->common);
}

static void wl_video_window_surface_configure(void *data,
      struct xdg_surface *xdg_surface, uint32_t serial)
{
   wl_video_window_t *state = (wl_video_window_t*)data;
   xdg_surface_ack_configure(xdg_surface, serial);
   wl_video_window_configure(state,
         wl_video_window_size(state, state->cfg_dims, state->cfg_floating),
         state->cfg_fullscreen, state->cfg_suspended, state->cfg_floating);
}

static const struct xdg_surface_listener wl_video_window_xdg_surface_listener = {
   wl_video_window_surface_configure,
};

static const struct xdg_toplevel_listener wl_video_window_toplevel_listener = {
   wl_video_window_toplevel_configure,
   wl_video_window_toplevel_close,
   xdg_toplevel_handle_configure_bounds,
   xdg_toplevel_handle_wm_capabilities,
};

#ifdef HAVE_LIBDECOR_H
static void wl_video_window_libdecor_configure(struct libdecor_frame *frame,
      struct libdecor_configuration *configuration, void *data)
{
   enum libdecor_window_state window_state;
   struct libdecor_state *ls;
   unsigned dims;
   int width                  = 0;
   int height                 = 0;
   bool fullscreen            = false;
   bool suspended             = false;
   bool floating              = true;
   wl_video_window_t *state   = (wl_video_window_t*)data;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)state->common.owner;

   if (wl->libdecor_configuration_get_window_state(configuration,
            &window_state))
   {
      fullscreen = (window_state & LIBDECOR_WINDOW_STATE_FULLSCREEN) != 0;
      /* LIBDECOR_WINDOW_STATE_SUSPENDED, libdecor 0.2 */
      suspended  = (window_state & (1 << 7)) != 0;
      floating   = !(window_state & (LIBDECOR_WINDOW_STATE_FULLSCREEN
               | LIBDECOR_WINDOW_STATE_MAXIMIZED
               | LIBDECOR_WINDOW_STATE_TILED_LEFT
               | LIBDECOR_WINDOW_STATE_TILED_RIGHT
               | LIBDECOR_WINDOW_STATE_TILED_TOP
               | LIBDECOR_WINDOW_STATE_TILED_BOTTOM));
   }
   if (!wl->libdecor_configuration_get_content_size(configuration, frame,
            &width, &height))
      width = height = 0;
   dims = wl_video_window_size(state,
         VIDEO_SCALE_PACK(width > 0 ? width : 0, height > 0 ? height : 0),
         floating);
   /* Committing the configuration is what acknowledges it */
   ls = wl->libdecor_state_new(VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
   wl->libdecor_frame_commit(frame, ls, configuration);
   wl->libdecor_state_free(ls);
   wl_video_window_configure(state, dims, fullscreen, suspended, floating);
}

static void wl_video_window_libdecor_close(struct libdecor_frame *frame,
      void *data)
{
   video_window_mark_closed(&((wl_video_window_t*)data)->common);
}

static const struct libdecor_frame_interface
wl_video_window_libdecor_interface = {
   wl_video_window_libdecor_configure,
   wl_video_window_libdecor_close,
   libdecor_frame_handle_commit,
};
#endif

/* 'monitor' as the main window's fullscreen counts outputs; missing or
 * automatic, the one the window is on, else the compositor's choice */
static struct wl_output *wl_video_window_fullscreen_output(
      const wl_video_window_t *state, unsigned monitor)
{
   output_info_t *oi = wl_output_at(
         (gfx_ctx_wayland_data_t*)state->common.owner, monitor);
   if (!oi)
      oi = wl_video_window_on(state);
   return oi ? oi->output : NULL;
}

static void wl_video_window_set_fullscreen(wl_video_window_t *state,
      bool fullscreen, unsigned monitor)
{
#ifdef HAVE_LIBDECOR_H
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)state->common.owner;
   if (state->libdecor_frame)
   {
      if (fullscreen)
         wl->libdecor_frame_set_fullscreen(state->libdecor_frame,
               wl_video_window_fullscreen_output(state, monitor));
      else
         wl->libdecor_frame_unset_fullscreen(state->libdecor_frame);
      return;
   }
#endif
   if (fullscreen)
      xdg_toplevel_set_fullscreen(state->xdg_toplevel,
            wl_video_window_fullscreen_output(state, monitor));
   else
      xdg_toplevel_unset_fullscreen(state->xdg_toplevel);
}

/* Saved size, else the main window's */
static unsigned wl_video_window_dims(const gfx_ctx_wayland_data_t *wl,
      unsigned dims)
{
   if (dims)
      return dims;
   if (VIDEO_SCALE_W(wl->dims) && VIDEO_SCALE_H(wl->dims))
      return wl->dims;
   return VIDEO_SCALE_PACK(DEFAULT_WINDOWED_WIDTH, DEFAULT_WINDOWED_HEIGHT);
}

static void wl_video_window_release(wl_video_window_t *state)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)state->common.owner;
   wl_video_window_ids_t *ids =
      &wl_video_window_ids[state->common.request.screen];
   retro_atomic_store_release_size(&ids->surface, 0);
   retro_atomic_store_release_int(&ids->generation, 0);
   if (state->deco)
      zxdg_toplevel_decoration_v1_destroy(state->deco);
   if (state->xdg_toplevel)
      xdg_toplevel_destroy(state->xdg_toplevel);
   if (state->xdg_surface)
      xdg_surface_destroy(state->xdg_surface);
#ifdef HAVE_LIBDECOR_H
   if (state->libdecor_frame)
      wl->libdecor_frame_unref(state->libdecor_frame);
#endif
   if (state->fractional_scale)
      wp_fractional_scale_v1_destroy(state->fractional_scale);
   if (state->viewport)
      wp_viewport_destroy(state->viewport);
   wl_surface_destroy(state->surface);
   wl_display_flush(wl->input.dpy);
   free(state);
}

bool wl_video_window_create(void *owner,
      const video_window_request_t *request,
      video_window_native_t *window)
{
   char title[64];
   unsigned dims;
   struct wl_region *opaque;
   wl_video_window_ids_t *ids;
   wl_video_window_t *state;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)owner;

   if (     !wl || !request || !window || window->state
         || !wl->surface || !wl->compositor || !wl->xdg_shell
         || !request->generation
         || !request->screen
         || request->screen >= RETRO_VIDEO_VIEWS_MAX
         || request->settings.mode != VIDEO_WINDOW_SEPARATE
         || wl_video_windows[request->screen]
         || (request->settings.dims
            && (  !VIDEO_SCALE_W(request->settings.dims)
               || !VIDEO_SCALE_H(request->settings.dims))))
      return false;
#ifdef HAVE_THREADS
   if (!wl_video_window_queue_made)
   {
      if (!retro_spsc_init(&wl_video_window_queue,
               WL_VIDEO_WINDOW_FOCUS_QUEUE * sizeof(wl_video_window_focus_t)))
         return false;
      wl_video_window_queue_made = true;
   }
#endif
   if (!(state = (wl_video_window_t*)calloc(1, sizeof(*state))))
      return false;
   if (!(state->surface = wl_compositor_create_surface(wl->compositor)))
   {
      free(state);
      return false;
   }
   dims                        = wl_video_window_dims(wl,
         request->settings.dims);
   state->common.owner         = wl;
   state->common.native        = window;
   state->common.request       = *request;
   state->dims                 = dims;
   state->floating_dims        = dims;
   state->floating             = true;
   state->scale                = 1;
   state->fractional_scale_num = FRACTIONAL_SCALE_V1_DEN;
   wl_surface_add_listener(state->surface,
         &wl_video_window_surface_listener, state);
   if (wl->viewporter)
   {
      state->viewport = wp_viewporter_get_viewport(wl->viewporter,
            state->surface);
      wp_viewport_set_destination(state->viewport,
            VIDEO_SCALE_W(dims), VIDEO_SCALE_H(dims));
      /* Its scale comes before the first configure */
      if (wl->fractional_scale_manager)
      {
         state->fractional_scale =
            wp_fractional_scale_manager_v1_get_fractional_scale(
                  wl->fractional_scale_manager, state->surface);
         wp_fractional_scale_v1_add_listener(state->fractional_scale,
               &wl_video_window_fractional_listener, state);
      }
   }
   if ((opaque = wl_compositor_create_region(wl->compositor)))
   {
      wl_region_add(opaque, 0, 0, 0x7FFFFFFF, 0x7FFFFFFF);
      wl_surface_set_opaque_region(state->surface, opaque);
      wl_region_destroy(opaque);
   }

   /* The seat's handlers tell the window's focus by its surface */
#ifdef HAVE_THREADS
   retro_atomic_store_release_size(&wl_video_window_thread,
         (size_t)sthread_get_current_thread_id());
#endif
   ids = &wl_video_window_ids[request->screen];
   retro_atomic_store_release_int(&ids->generation, (int)request->generation);
   retro_atomic_store_release_size(&ids->surface,
         (size_t)(uintptr_t)state->surface);

   snprintf(title, sizeof(title), "RetroArch - Screen %u",
         video_window_title_screen(request) + 1);
#ifdef HAVE_LIBDECOR_H
   if (wl->libdecor)
   {
      if (!(state->libdecor_frame = wl->libdecor_decorate(
                  wl->libdecor_context, state->surface,
                  &wl_video_window_libdecor_interface, state)))
      {
         wl_video_window_release(state);
         return false;
      }
      wl->libdecor_frame_set_app_id(state->libdecor_frame, WAYLAND_APP_ID);
      wl->libdecor_frame_set_title(state->libdecor_frame, title);
      if (request->settings.fullscreen)
         wl_video_window_set_fullscreen(state, true,
               request->settings.monitor);
      wl->libdecor_frame_map(state->libdecor_frame);
   }
   else
#endif
   {
      state->xdg_surface  = xdg_wm_base_get_xdg_surface(wl->xdg_shell,
            state->surface);
      xdg_surface_add_listener(state->xdg_surface,
            &wl_video_window_xdg_surface_listener, state);
      state->xdg_toplevel = xdg_surface_get_toplevel(state->xdg_surface);
      xdg_toplevel_add_listener(state->xdg_toplevel,
            &wl_video_window_toplevel_listener, state);
      xdg_toplevel_set_app_id(state->xdg_toplevel, WAYLAND_APP_ID);
      xdg_toplevel_set_title(state->xdg_toplevel, title);
      if (wl->deco_manager)
      {
         state->deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
               wl->deco_manager, state->xdg_toplevel);
         zxdg_toplevel_decoration_v1_set_mode(state->deco,
               ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
      }
      if (request->settings.fullscreen)
         wl_video_window_set_fullscreen(state, true,
               request->settings.monitor);
   }
   /* No buffer: the compositor answers with the first configure */
   wl_surface_commit(state->surface);
   wl_display_flush(wl->input.dpy);

   wl_video_windows[request->screen] = state;
   memset(window, 0, sizeof(*window));
   window->state      = state;
   window->display    = wl->input.dpy;
   window->surface    = state->surface;
   window->kind       = VIDEO_WINDOW_NATIVE_WAYLAND;
   window->generation = request->generation;
   wl_video_window_resized(state);
   state->common.pending     = 0;
   video_window_placed(&state->common, 0, dims, cpu_features_get_time_usec());
   return true;
}

void wl_video_window_update(void *owner, video_window_native_t *window,
      const video_window_request_t *request)
{
   const video_window_settings_t *now;
   const video_window_settings_t *was;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)owner;
   wl_video_window_t *state   = window
      ? (wl_video_window_t*)window->state : NULL;

   if (     !state || state->common.owner != owner || !request
         || state->common.closed
         || request->screen     != state->common.request.screen
         || request->generation != state->common.request.generation)
      return;
   now = &request->settings;
   was = &state->common.request.settings;
   if (     (now->dims && (!VIDEO_SCALE_W(now->dims) || !VIDEO_SCALE_H(now->dims)))
         || (     now->dims       == was->dims
               && now->monitor    == was->monitor
               && now->fullscreen == was->fullscreen))
      return;

   if (now->dims != was->dims)
   {
      state->floating_dims  = wl_video_window_dims(wl, now->dims);
      /* The compositor sizes a window that is not floating, so the
       * size waits for the next floating configure */
      state->floating_asked = !window->configured || state->fullscreen
         || !state->floating;
      if (!state->floating_asked)
      {
         state->dims = state->floating_dims;
#ifdef HAVE_LIBDECOR_H
         if (state->libdecor_frame)
         {
            struct libdecor_state *ls = wl->libdecor_state_new(
                  VIDEO_SCALE_W(state->dims), VIDEO_SCALE_H(state->dims));
            wl->libdecor_frame_commit(state->libdecor_frame, ls, NULL);
            wl->libdecor_state_free(ls);
         }
#endif
         if (state->viewport)
            wp_viewport_set_destination(state->viewport,
                  VIDEO_SCALE_W(state->dims), VIDEO_SCALE_H(state->dims));
         wl_surface_commit(state->surface);
         wl_video_window_resized(state);
      }
   }
   /* Position is the compositor's on Wayland. Another output for a
    * fullscreen window is a fullscreen request again, after leaving:
    * compositors keep a fullscreen window where it is. */
   if (was->fullscreen && (!now->fullscreen || now->monitor != was->monitor))
      wl_video_window_set_fullscreen(state, false, 0);
   if (now->fullscreen && (!was->fullscreen || now->monitor != was->monitor))
      wl_video_window_set_fullscreen(state, true, now->monitor);
   wl_display_flush(wl->input.dpy);
   state->common.request = *request;
}

unsigned wl_video_window_poll(void *owner, video_window_event_t *events,
      unsigned capacity)
{
   wl_video_windows_take_focus();
   return video_windows_drain(owner, wl_video_window_at, events, capacity);
}

void wl_video_window_destroy(void *owner, video_window_native_t *window)
{
   wl_video_window_t *state = window ? (wl_video_window_t*)window->state : NULL;
   if (!state || state->common.owner != owner)
      return;
   wl_video_windows[state->common.request.screen] = NULL;
   wl_video_window_release(state);
   memset(window, 0, sizeof(*window));
}

void wl_video_windows_free(void *owner,
      void (*destroy)(void *owner, video_window_native_t *window))
{
   video_windows_sweep(owner, wl_video_window_at, destroy);
}
#endif

static void shm_buffer_handle_release(void *data,
   struct wl_buffer *wl_buffer)
{
   shm_buffer_t *buffer = data;

   shm_buffer_free(buffer);
}

const struct wl_buffer_listener shm_buffer_listener = {
   shm_buffer_handle_release,
};

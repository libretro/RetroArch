/*  RetroArch - A frontend for libretro.
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

#ifndef __RARCH_WAYLAND_WLR_OUTPUT_H
#define __RARCH_WAYLAND_WLR_OUTPUT_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <wayland-client.h>

#include "../video_display_server.h"

RETRO_BEGIN_DECLS

/* wlroots compositors' heads and modes (zwlr_output_manager_v1: sway,
 * Hyprland, river and others), and switching a mode, for a display
 * server's own connection. Everything arrives through events that
 * connection's owner dispatches, and a switch is asked for and its
 * outcome logged when the compositor gives it, so nothing here waits. */

#define WLR_OUTPUT_MAX_HEADS 8

typedef struct wlr_output_mode_info
{
   struct zwlr_output_mode_v1 *mode;
   int32_t width;
   int32_t height;
   int32_t refresh_mhz;
} wlr_output_mode_info_t;

#define WLR_HEAD_ENABLED (1u << 0)

typedef struct wlr_output_head_info
{
   struct zwlr_output_head_v1 *head;
   struct zwlr_output_mode_v1 *current;
   wlr_output_mode_info_t     *modes;
   unsigned                    nmodes;
   unsigned                    capacity;
   uint32_t                    flags;
   char                        name[64];
} wlr_output_head_info_t;

typedef struct wlr_outputs
{
   struct zwlr_output_manager_v1 *manager;
   wlr_output_head_info_t         heads[WLR_OUTPUT_MAX_HEADS];
   unsigned                       nheads;
   /* The last done event's serial, which a configuration must carry */
   uint32_t                       serial;
   bool                           have_serial;
} wlr_outputs_t;

/* From a registry listener: binds the output manager. Returns true
 * when 'interface' was it. */
bool wlr_outputs_bind(wlr_outputs_t *wo, struct wl_registry *registry,
      uint32_t name, const char *interface, uint32_t version);

/* Whether modes can be listed and switched: the heads have been
 * described (a done event) and one has modes. */
bool wlr_outputs_ready(const wlr_outputs_t *wo);

/* The modes of the head called 'connector' (the first enabled head
 * where there is none by that name), sorted by size then rate, the
 * current one marked; free() the result. NULL while not ready. */
video_display_config_t *wlr_outputs_resolution_list(wlr_outputs_t *wo,
      const char *connector, unsigned *len);

/* Asks the compositor to switch that head to its listed mode of that
 * size nearest 'hz' (or 'int_hz'); a zero width or height keeps the
 * current one. Every other head is sent as it is, as the protocol
 * requires. False for a mode the head does not list. */
bool wlr_outputs_set_mode(wlr_outputs_t *wo, const char *connector,
      unsigned width, unsigned height, int int_hz, float hz);

void wlr_outputs_destroy(wlr_outputs_t *wo);

RETRO_END_DECLS

#endif

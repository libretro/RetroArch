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

#ifndef __RARCH_WAYLAND_KWIN_OUTPUT_H
#define __RARCH_WAYLAND_KWIN_OUTPUT_H

#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

#include <wayland-client.h>

#include "../video_display_server.h"

RETRO_BEGIN_DECLS

/* KWin's outputs and their modes (kde_output_device_v2), and switching
 * a mode (kde_output_management_v2), for a display server's own
 * connection. Everything arrives through events that connection's
 * owner dispatches; a switch is asked for and its outcome logged when
 * KWin gives it, so nothing here waits. */

#define KWIN_OUTPUT_MAX_DEVICES 8

typedef struct kwin_output_mode
{
   struct kde_output_device_mode_v2 *mode;
   int32_t width;
   int32_t height;
   int32_t refresh_mhz;
} kwin_output_mode_t;

#define KWIN_DEVICE_DONE    (1u << 0)
#define KWIN_DEVICE_ENABLED (1u << 1)

typedef struct kwin_output_device
{
   struct kde_output_device_v2      *device;
   struct kde_output_device_mode_v2 *current;
   kwin_output_mode_t               *modes;
   unsigned                          nmodes;
   unsigned                          capacity;
   uint32_t                          flags;
   char                              name[64];
} kwin_output_device_t;

typedef struct kwin_outputs
{
   struct kde_output_device_registry_v2 *registry;
   struct kde_output_management_v2      *management;
   kwin_output_device_t                  devices[KWIN_OUTPUT_MAX_DEVICES];
   unsigned                              ndevices;
} kwin_outputs_t;

/* From a registry listener: binds KWin's output globals. Returns true
 * when 'interface' was one of them. */
bool kwin_outputs_bind(kwin_outputs_t *kw, struct wl_registry *registry,
      uint32_t name, const char *interface, uint32_t version);

/* Whether modes can be listed and switched: the management global is
 * bound and some output has described itself. */
bool kwin_outputs_ready(const kwin_outputs_t *kw);

/* The modes of the output called 'connector' (the first enabled output
 * where there is none by that name), sorted by size then rate, the
 * current one marked; free() the result. NULL while not ready. */
video_display_config_t *kwin_outputs_resolution_list(kwin_outputs_t *kw,
      const char *connector, unsigned *len);

/* Asks KWin to switch that output to its listed mode of that size
 * nearest 'hz' (or 'int_hz'); a zero width or height keeps the
 * current one. False for a mode it does not list. */
bool kwin_outputs_set_mode(kwin_outputs_t *kw, const char *connector,
      unsigned width, unsigned height, int int_hz, float hz);

void kwin_outputs_destroy(kwin_outputs_t *kw);

RETRO_END_DECLS

#endif

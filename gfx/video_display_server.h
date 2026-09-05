/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifndef __VIDEO_DISPLAY_SERVER__H
#define __VIDEO_DISPLAY_SERVER__H

#include <retro_common_api.h>
#include <boolean.h>

#include "video_defines.h"
#include "modeline/modeline_core.h"

RETRO_BEGIN_DECLS

enum display_server_flags
{
   DISPSERV_CTX_FLAGS_NONE = 0,
   /* The server can apply a video_modeline_t through its modeline_*
    * ops (the bit the CRT consumer and the menu look for). */
   DISPSERV_CTX_MODELINE
};

/* One-cycle alias for the bit's previous name. */
#define DISPSERV_CTX_CRT_SWITCHRES DISPSERV_CTX_MODELINE

/* One physical output as the display server sees it, for the
 * monitor-index mapping and a future per-head selection. */
typedef struct video_output_info
{
   int  id;             /* server-specific handle (XRandR output index,
                           EnumDisplayMonitors index, DRM connector id) */
   int  x, y;           /* placement in desktop coordinates */
   unsigned width, height;
   bool primary;
   char name[64];       /* connector name (DVI-0, \\.\DISPLAY1, HDMI-A-1) */
} video_output_info_t;

typedef struct video_display_config
{
   unsigned width;
   unsigned height;
   unsigned bpp;
   unsigned refreshrate;
   unsigned idx;
   bool current;
   bool interlaced;
   bool dblscan;
   float refreshrate_float;
} video_display_config_t;

typedef struct video_display_server
{
   void *(*init)(void);
   void (*destroy)(void *data);
   bool (*set_window_opacity)(void *data, unsigned opacity);
   bool (*set_window_progress)(void *data, int progress, bool finished);
   bool (*set_window_decorations)(void *data, bool on);
   bool (*set_resolution)(void *data, unsigned width,
         unsigned height, int int_hz, float hz, int center, int monitor_index, int xoffset, int padjust );
   void *(*get_resolution_list)(void *data,
         unsigned *size);
   const char *(*get_output_options)(void *data);
   void (*set_screen_orientation)(void *data, enum rotation rotation);
   enum rotation (*get_screen_orientation)(void *data);
   float (*get_refresh_rate)(void *data);
   void (*get_video_output_size)(void *data,
         unsigned *width, unsigned *height, char *s, size_t len);
   void (*get_video_output_prev)(void *data);
   void (*get_video_output_next)(void *data);
   bool (*get_metrics)(void *data, enum display_metric_types type,
         float *value);
   uint32_t (*get_flags)(void *data);
   /* Display scanout timing, for Scanline Sync.
    *
    * get_scanline returns the current beam position in scanlines, or a
    * negative value if unavailable. wait_vblank blocks until the next
    * vertical blank and returns false if it cannot.
    *
    * Both are optional and a server may implement one without the
    * other, but Scanline Sync needs get_scanline: it calibrates the
    * total line count from the peak value and targets a specific line.
    * A server offering only wait_vblank cannot drive it.
    *
    * Only win32 implements these today, through D3DKMT. The equivalents
    * elsewhere are drmWaitVBlank on KMS and glXWaitForMscOML on X11 -
    * both vblank waits, neither exposing a live scanout position -
    * while Wayland's presentation-time protocol reports after the fact
    * rather than blocking. None of them are wired up. */
   int  (*get_scanline)(void *data);
   bool (*wait_vblank)(void *data);

   /* Video modeline application. The engine in gfx/modeline/ generates
    * a full video_modeline_t; these ops put it on the wire. A server
    * that cannot program timings leaves them NULL and the consumer
    * generates only.
    *
    * open picks the screen and the vendor path (win32: PowerStrip if
    * asked, else ATI legacy or ADL from the PCI id; x11: XRandR
    * output). close restores the desktop and releases it. caps
    * reports MODELINE_CAPS_* for what the path can do: XRandR adds
    * modes, ADL and PowerStrip rewrite listed ones. enum fills the
    * OS's mode list with the desktop entry tagged MODELINE_DESKTOP
    * and returns the count, or -1. add/update/delete stage a change
    * to one mode and may store a handle in platform_data; flush
    * commits what was staged (ADL's list refresh and monitor resync
    * happen once here rather than per mode). set switches to a mode
    * that is already in the list.
    *
    * list_outputs enumerates the heads the server can drive and
    * returns the count (or -1); open binds one of them through
    * ds->screen ("auto", an index, or a connector name), which is
    * where the RetroArch monitor index lands. */
   int      (*modeline_list_outputs)(void *data, video_output_info_t *out, int max);
   bool     (*modeline_open)(void *data, const video_modeline_disp_t *ds);
   void     (*modeline_close)(void *data);
   unsigned (*modeline_caps)(void *data);
   int      (*modeline_enum)(void *data, video_modeline_t *modes, int max);
   bool     (*modeline_add)(void *data, video_modeline_t *mode);
   bool     (*modeline_update)(void *data, video_modeline_t *mode);
   bool     (*modeline_delete)(void *data, video_modeline_t *mode);
   bool     (*modeline_set)(void *data, video_modeline_t *mode);
   bool     (*modeline_flush)(void *data);
   const char *ident;
} video_display_server_t;

void* video_display_server_init(enum rarch_display_type type);

void video_display_server_destroy(void);

bool video_display_server_get_flags(gfx_ctx_flags_t *flags);

int  video_display_server_get_scanline(void);
bool video_display_server_wait_vblank(void);

bool video_display_server_set_window_opacity(unsigned opacity);

bool video_display_server_set_window_progress(int progress, bool finished);

bool video_display_server_set_window_decorations(bool on);

bool video_display_server_set_resolution(
      unsigned width, unsigned height,
      int int_hz, float hz, int center, int monitor_index, int xoffset, int padjust);

void *video_display_server_get_resolution_list(unsigned *size);

const char *video_display_server_get_output_options(void);

const char *video_display_server_get_ident(void);

void video_display_server_set_screen_orientation(enum rotation rotation);

float video_display_server_get_refresh_rate(void);

bool video_display_server_get_video_output_size(
      unsigned *width, unsigned *height, char *s, size_t len);

bool video_display_server_get_video_output_prev(void);

bool video_display_server_get_video_output_next(void);

bool video_display_server_get_metrics(
      enum display_metric_types type, float *value);

bool video_display_server_can_set_screen_orientation(void);

bool video_display_server_has_resolution_list(void);

void video_switch_refresh_rate_maybe(float *refresh_rate, bool *video_switch_refresh_rate);

bool video_display_server_set_refresh_rate(float hz);

bool video_display_server_has_refresh_rate(float hz);

void video_display_server_restore_refresh_rate(void);

enum rotation video_display_server_get_screen_orientation(void);

/* The current server's modeline ops as an engine ops table, with data
 * bound. Returns false and leaves ops untouched when the server has
 * no modeline application path. */
struct video_modeline_ops;
bool video_display_server_get_modeline_ops(struct video_modeline_ops *ops);

/* True when an SDL2 or SDL3 video driver or context owns the window,
 * so the matching SDL display server can switch among its listed
 * modes. */
bool video_display_server_sdl_available(void);

/* The heads the mode server can drive; count, or -1 without a list. */
int video_display_server_list_outputs(video_output_info_t *out, int max);

extern const video_display_server_t dispserv_win32;
extern const video_display_server_t dispserv_uwp;
extern const video_display_server_t dispserv_x11;
extern const video_display_server_t dispserv_wl;
extern const video_display_server_t dispserv_kms;
extern const video_display_server_t dispserv_android;
extern const video_display_server_t dispserv_apple;
extern const video_display_server_t dispserv_sdl2;
extern const video_display_server_t dispserv_sdl3;

RETRO_END_DECLS

#endif

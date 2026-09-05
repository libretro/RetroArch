/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
 *  Copyright (C) 2026 - The RetroArch team
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

#ifndef __WIN32_MODELINE_H
#define __WIN32_MODELINE_H

#include <boolean.h>
#include <retro_common_api.h>

#include "../modeline/modeline_core.h"

RETRO_BEGIN_DECLS

/* A Windows custom-timing path. ChangeDisplaySettings only switches
 * between driver-listed modes, so a vendor path rewrites the timing
 * the driver holds for a listed mode: AMD ADL for Cedar and newer,
 * the legacy registry (DALDTMCRTBCD) for older Radeons, or a running
 * PowerStrip for anything it supports. dispserv_win32.c picks one at
 * modeline_open and drives it through this table.
 *
 * add/update/delete stage one mode; flush commits what was staged
 * (ADL's list refresh and monitor resync, ATI's EnumDisplaySettings
 * pass). get_timing fills the detailed timing of a listed mode by
 * its width/height/refresh labels and tags type with the path's
 * MODELINE_TIMING_* bit; false leaves it a system mode. */
typedef struct win32_modeline_backend
{
   void *ctx;
   const char *name;
   unsigned (*caps)(void *ctx);
   bool (*get_timing)(void *ctx, video_modeline_t *m);
   bool (*add_mode)(void *ctx, video_modeline_t *m);
   bool (*update_mode)(void *ctx, video_modeline_t *m);
   bool (*delete_mode)(void *ctx, video_modeline_t *m);
   bool (*flush)(void *ctx);
   void (*close)(void *ctx);
} win32_modeline_backend_t;

/* Each returns false, with the table untouched, when its path is not
 * available on this machine (no PowerStrip window, no ADL dll, no
 * elevation for the registry path). device_name is \\.\DISPLAYn,
 * device_key the adapter's registry key under HKLM. */
bool win32_modeline_adl_create(win32_modeline_backend_t *b,
      const char *device_name, const char *device_key,
      const video_modeline_disp_t *ds);
bool win32_modeline_ati_create(win32_modeline_backend_t *b,
      const char *device_name, const char *device_key,
      const video_modeline_disp_t *ds);
bool win32_modeline_pstrip_create(win32_modeline_backend_t *b,
      const char *device_name, const video_modeline_disp_t *ds);

/* Radeon families before Cedar use the registry path. */
bool win32_modeline_ati_is_legacy(int vendor, int device);

/* Monitor resync: after a timing-table refresh the display driver
 * re-plugs the monitor; the ADL path waits for the device
 * notifications before touching the mode again. arm() before the
 * call that triggers the re-plug, wait() after it: a notification
 * that lands between the two is kept, not lost. */
typedef struct win32_modeline_resync win32_modeline_resync_t;
win32_modeline_resync_t *win32_modeline_resync_new(void);
void win32_modeline_resync_free(win32_modeline_resync_t *r);
void win32_modeline_resync_arm(win32_modeline_resync_t *r);
void win32_modeline_resync_wait(win32_modeline_resync_t *r);

RETRO_END_DECLS

#endif

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

#ifndef __VIDEO_MODELINE_LIST_H
#define __VIDEO_MODELINE_LIST_H

#include <retro_common_api.h>

#include "modeline_core.h"

RETRO_BEGIN_DECLS

/* What the engine needs from a display server. The signatures match
 * the modeline_* members of video_display_server_t so a consumer can
 * assign them straight across; a NULL entry is a capability the
 * server does not have. With every entry NULL the engine generates
 * only: caps is ADD, adds succeed without touching a display, and
 * set fails. */
typedef struct video_modeline_ops
{
   void *data;
   bool (*open)(void *data, const video_modeline_disp_t *ds);
   void (*close)(void *data);
   unsigned (*caps)(void *data);
   int  (*enum_modes)(void *data, video_modeline_t *modes, int max);
   bool (*add)(void *data, video_modeline_t *mode);
   bool (*update)(void *data, video_modeline_t *mode);
   bool (*del)(void *data, video_modeline_t *mode);
   bool (*set)(void *data, video_modeline_t *mode);
   bool (*flush)(void *data);
   const char *name;
} video_modeline_ops_t;

/* Allocate a generator with the mode list on the heap and the default
 * policy; NULL on allocation failure. The generator forces
 * LC_NUMERIC to "C" so range lines parse on any locale. */
video_modeline_gen_t *modeline_gen_new(void);
void modeline_gen_free(video_modeline_gen_t *gen);

/* Apply one "key value" option, the same keys the ini accepts. */
void modeline_set_option(video_modeline_gen_t *gen, const char *key,
      const char *value);

/* Re-derive the user mode, user modeline and monitor ranges from the
 * current options. Call after loading options and before get. */
void modeline_parse_options(video_modeline_gen_t *gen);

/* Preset by name; unknown names fall back to generic_15. */
void modeline_set_monitor(video_modeline_gen_t *gen, const char *preset);

/* Lock resolution and/or refresh; 0 leaves that axis free. */
void modeline_set_user_mode(video_modeline_gen_t *gen, int width,
      int height, int refresh);

/* Enumerate the display's modes through ops and rebuild the list; on
 * an LCD preset this also derives the range from the desktop mode.
 * Returns false if the server reported an error. */
bool modeline_list_init(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops);

/* Pick or generate the best mode for the request. Returns a pointer
 * into the list (valid until the next list mutation) tagged
 * MODELINE_ADD or MODELINE_UPDATE when it needs flushing, or NULL when
 * nothing in range. */
video_modeline_t *modeline_get(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops, int width, int height,
      double refresh, int flags);

/* Push pending adds/updates/deletes to the server and then
 * ops->flush. */
bool modeline_flush(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops);

/* Mark every added mode for deletion and every modified one for
 * restore, then flush. */
bool modeline_restore(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops);

/* Switch to mode if it differs from what is on the wire. */
bool modeline_set(video_modeline_gen_t *gen,
      const video_modeline_ops_t *ops, video_modeline_t *mode);

/* Look a listed mode up by id. */
video_modeline_t *modeline_find_id(video_modeline_gen_t *gen, int id);

/* Log one mode on the debug channel prefixed with the server name. */
void modeline_log_mode(const video_modeline_ops_t *ops,
      const video_modeline_t *mode);

RETRO_END_DECLS

#endif

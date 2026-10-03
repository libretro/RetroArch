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

#include <string.h>

#include "dispserv_ps3_modes.h"

typedef struct ps3_mode
{
   unsigned char  id;
   unsigned short width;
   unsigned short height;
} ps3_mode_t;

/* The order the list shows them in, as the old context-driver list
 * did: SD, the 1080-line horizontal scalers under 720p, then 1080 */
static const ps3_mode_t ps3_mode_table[] = {
   {  4,  720,  480 }, /* 480 */
   {  5,  720,  576 }, /* 576 */
   { 13,  960, 1080 },
   {  2, 1280,  720 }, /* 720 */
   { 12, 1280, 1080 },
   { 11, 1440, 1080 },
   { 10, 1600, 1080 },
   {  1, 1920, 1080 }  /* 1080 */
};

#define PS3_MODE_COUNT (sizeof(ps3_mode_table) / sizeof(ps3_mode_table[0]))
#define PS3_MODE_ID_576 5

static const ps3_mode_t *ps3_modes_lookup(unsigned id)
{
   unsigned i;
   for (i = 0; i < PS3_MODE_COUNT; i++)
      if (ps3_mode_table[i].id == id)
         return &ps3_mode_table[i];
   return NULL;
}

unsigned ps3_modes_dims(unsigned id)
{
   const ps3_mode_t *m = ps3_modes_lookup(id);
   if (!m)
      return 0;
   return VIDEO_SCALE_PACK(m->width, m->height);
}

float ps3_modes_hz(unsigned id)
{
   return (id == PS3_MODE_ID_576) ? 50.0f : 59.94f;
}

unsigned ps3_modes_effective(ps3_mode_available_t avail, void *user,
      unsigned current_id, unsigned system_id)
{
   if (     current_id != PS3_MODE_ID_SYSTEM
         && ps3_modes_lookup(current_id)
         && avail(user, current_id))
      return current_id;
   return ps3_modes_lookup(system_id) ? system_id : PS3_MODE_ID_SYSTEM;
}

int ps3_modes_find(ps3_mode_available_t avail, void *user, unsigned dims)
{
   unsigned i;
   for (i = 0; i < PS3_MODE_COUNT; i++)
      if (     VIDEO_SCALE_PACK(ps3_mode_table[i].width,
                  ps3_mode_table[i].height) == dims
            && avail(user, ps3_mode_table[i].id))
         return ps3_mode_table[i].id;
   return -1;
}

unsigned ps3_modes_list(ps3_mode_available_t avail, void *user,
      unsigned current_id, unsigned system_id,
      video_display_config_t *out, unsigned max)
{
   unsigned i;
   unsigned count  = 0;
   unsigned in_use = ps3_modes_effective(avail, user, current_id, system_id);

   for (i = 0; i < PS3_MODE_COUNT; i++)
   {
      const ps3_mode_t *m = &ps3_mode_table[i];
      if (!avail(user, m->id))
         continue;
      if (out && count < max)
      {
         video_display_config_t *cfg = &out[count];
         memset(cfg, 0, sizeof(*cfg));
         cfg->dims              = VIDEO_SCALE_PACK(m->width, m->height);
         cfg->bpp               = 32;
         cfg->refreshrate_float = ps3_modes_hz(m->id);
         cfg->refreshrate       = (unsigned)cfg->refreshrate_float;
         cfg->idx               = m->id;
         cfg->current           = (m->id == in_use);
      }
      count++;
   }

   return count;
}

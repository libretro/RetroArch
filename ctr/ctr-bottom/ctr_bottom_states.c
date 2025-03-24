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

#include "ctr_bottom.h"
#include "ctr_bottom_states.h"

#include "../../paths.h"
#include "../../command.h"
#include "../../gfx/common/ctr_defines.h"
#include "../../retroarch.h"
#include <compat/strl.h>

char *strremove(char *str, const char *sub) {
    char *p, *q, *r;
    if (*sub && (q = r = strstr(str, sub)) != NULL) {
        size_t len = strlen(sub);
        while ((r = strstr(p = r + len, sub)) != NULL) {
            while (p < r)
                *q++ = *p++;
        }
        while ((*q++ = *p++) != '\0')
            continue;
    }
    return str;
}

void ctr_update_state_path(void *data)
{
   size_t _len;
   static char texture_path[PATH_MAX_LENGTH];
   char state_filename[PATH_MAX_LENGTH];

   if (!runloop_get_current_savestate_path(state_filename, 
         sizeof(state_filename)))
      return;

   _len                 = strlcpy(texture_path,
         state_filename, sizeof(texture_path));
   texture_path[_len  ] = '.';
   texture_path[_len+1] = 'p';
   texture_path[_len+2] = 'n';
   texture_path[_len+3] = 'g';
   texture_path[_len+4] = '\0';

   ctr_bottom_state_gfx.texture_name = path_basename(texture_path);
   ctr_bottom_state_gfx.texture_path = strremove(state_filename, path_basename(state_filename));
}


void ctr_update_state_date(void *data)
{
   time_t now       = time(NULL);
   struct tm *t     = localtime(&now);
   snprintf(ctr_bottom_state_savestates.state_date, 
      sizeof(ctr_bottom_state_savestates.state_date), "%02u/%02u/%u",
      ((unsigned)t->tm_mon + 1) % 100,
      (unsigned)t->tm_mday % 100,
      ((unsigned)t->tm_year + 1900) % 10000);
}

void save_state_to_file()
{
   char state_path[PATH_MAX_LENGTH];
   runloop_get_current_savestate_path(state_path, sizeof(state_path));

   command_event(CMD_EVENT_RAM_STATE_TO_FILE, state_path);
}


bool ctr_update_state_date_from_file(void *data)
{
   char state_path[PATH_MAX_LENGTH];
#ifdef USE_CTRULIB_2
   time_t mtime;
#else
   time_t ft;
   u64 mtime;
#endif
   struct tm *t     = NULL;

   if (!runloop_get_current_savestate_path(
            state_path, sizeof(state_path)))
      return false;

#ifdef USE_CTRULIB_2
   if (archive_getmtime(state_path + 5, &mtime) != 0)
	   goto error;
#else
   if (sdmc_getmtime(state_path + 5, &mtime) != 0)
	   goto error;
#endif 

   ctr_bottom_state_savestates.state_data_exist = true;

#ifdef USE_CTRULIB_2
   t     = localtime(&mtime);
#else
   ft    = mtime;
   t     = localtime(&ft);
#endif
   snprintf(ctr_bottom_state_savestates.state_date, 
      sizeof(ctr_bottom_state_savestates.state_date), "%02u/%02u/%u",
      ((unsigned)t->tm_mon + 1) % 100,
      (unsigned)t->tm_mday % 100,
      ((unsigned)t->tm_year + 1900) % 10000);
      ctr_update_state_path(0);
   return true;

error:
   ctr_bottom_state_savestates.state_data_exist = false;
   strlcpy(ctr_bottom_state_savestates.state_date, "00/00/0000", 
         sizeof(ctr_bottom_state_savestates.state_date));
   return false;
}




/*
static void ctr_state_thumbnail(void* data)
{
   uint32_t state_tmp   = 0;
   ctr_video_t *ctr     = (ctr_video_t*)data;
   settings_t *settings = config_get_ptr();
   uint32_t flags       = runloop_get_flags();


            if (ctr_bottom_state.task_save)
            {
               ctr_bottom_state.task_save = false;
			
//               char screenshot_full_path[PATH_MAX_LENGTH];

               struct ctr_bottom_texture_data *o =
                     &ctr->bottom_textures[0]; //CTR_BOTTOM_TEXTURE_THUMBNAIL
               ctr_texture_t            *texture = 
                     (ctr_texture_t *) o->texture;

               if (texture)
                  linearFree(texture->data);
               else
               {
                  o->texture      = (uintptr_t)
                        calloc(1, sizeof(ctr_texture_t));
                  o->frame_coords = linearAlloc(sizeof(ctr_vertex_t));
                  texture         = (ctr_texture_t *)o->texture;
               }

               texture->width         = ctr->texture_width;
               texture->height        = ctr->texture_width;
               texture->active_width  = ctr->frame_coords->u1;
               texture->active_height = ctr->frame_coords->v1;

               texture->data          = linearAlloc(
                     ctr->texture_width * ctr->texture_height * 
                     (ctr->rgb32? 4:2));

               memcpy(texture->data, ctr->texture_swizzled, 
                     ctr->texture_width * ctr->texture_height * 
                     (ctr->rgb32? 4:2));

               ctr_bottom_state_thumbnail_geom(ctr);

               ctr_bottom_state_savestates.state_data_exist = true;
               ctr->render_state_from_png_file = false;

               ctr_update_state_date(ctr);

//               command_event(CMD_EVENT_SAVE_STATE_TO_RAM, NULL);
/*
               if (settings->bools.savestate_thumbnail_enable)
               {
                  sprintf(screenshot_full_path, "%s/%s",
                     dir_get_ptr(RARCH_DIR_SAVESTATE),
                     ctr_bottom_state_gfx.texture_path);

                  take_screenshot(NULL, screenshot_full_path, true,
                     video_driver_cached_frame_has_valid_framebuffer(),
                     true, true);
               }
*/
//               BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
//               ctr->refresh_bottom_menu = true;
/*            }


}
*/
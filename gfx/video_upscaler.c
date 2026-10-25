/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
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

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <gfx/nvidia/ngx_loader.h>

#include "../configuration.h"
#include "video_upscaler.h"

size_t video_upscaler_dlss_dir(char *s, size_t len)
{
   settings_t *settings = config_get_ptr();
   if (!string_is_empty(settings->paths.directory_dlss))
      return strlcpy(s, settings->paths.directory_dlss, len);
   return fill_pathname_join_special(s,
         settings->paths.directory_system, "dlss", len);
}

unsigned video_upscaler_dlss_probe(void)
{
   char dir[DIR_MAX_LENGTH];
   unsigned found = 0;
   if (ngx_lib_get())
      found |= VIDEO_UPSCALER_DLSS_NGX;
   video_upscaler_dlss_dir(dir, sizeof(dir));
   if (ngx_dlss_feature_in(dir))
      found |= VIDEO_UPSCALER_DLSS_FEATURE;
   return found;
}

size_t video_upscaler_status_string(char *s, size_t len)
{
   static const char *modes[VIDEO_TEMPORAL_UPSCALER_LAST] = { "Off", "DLSS" };
   unsigned want  = config_get_ptr()->uints.video_temporal_upscaler;
   unsigned found = video_upscaler_dlss_probe();
   size_t _len;
   if (want >= VIDEO_TEMPORAL_UPSCALER_LAST)
      want = VIDEO_TEMPORAL_UPSCALER_OFF;
   _len  = strlcpy(s, modes[want], len);
   _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_DLSS_NGX)
         ? " (NGX: found" : " (NGX: not found", len - _len);
   _len += strlcpy(s + _len, (found & VIDEO_UPSCALER_DLSS_FEATURE)
         ? ", DLSS runtime: found)" : ", DLSS runtime: not in DLSS Directory)",
         len - _len);
   return _len;
}

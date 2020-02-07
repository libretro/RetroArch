/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (media_detect_cd.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_MEDIA_DETECT_CD_H
#define __LIBRETRO_SDK_MEDIA_DETECT_CD_H

#include <retro_common_api.h>
#include <boolean.h>

RETRO_BEGIN_DECLS

enum media_detect_cd_system
{
   MEDIA_CD_SYSTEM_MEGA_CD,
   MEDIA_CD_SYSTEM_SATURN,
   MEDIA_CD_SYSTEM_DREAMCAST,
   MEDIA_CD_SYSTEM_PSX,
   MEDIA_CD_SYSTEM_3DO,
   MEDIA_CD_SYSTEM_PC_ENGINE_CD
};

typedef struct
{
   char title[256];
   char system[128];
   char region[128];
   char serial[64];
   char maker[64];
   char version[32];
   char release_date[32];
   enum media_detect_cd_system system_id;
} media_detect_cd_info_t;

/* Fill in "info" with detected CD info. Use this when you want to open a specific track file directly, and the pregap is known. */
bool media_detect_cd_info(const char *path, uint64_t pregap_bytes, media_detect_cd_info_t *info);

/* Fill in "info" with detected CD info. Use this when you have a cue file and want it parsed to find the first data track and any pregap info. */
bool media_detect_cd_info_cue(const char *path, media_detect_cd_info_t *info);

RETRO_END_DECLS

#endif

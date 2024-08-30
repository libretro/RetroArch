/* Copyright  (C) 2010-2019 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (runtime_file.h).
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

#ifndef __RUNTIME_FILE_DEFINES_H
#define __RUNTIME_FILE_DEFINES_H

#include <retro_common_api.h>
#include <libretro.h>

RETRO_BEGIN_DECLS

/* Enums */

enum playlist_sublabel_last_played_style_type
{
   PLAYLIST_LAST_PLAYED_STYLE_YMD_HMS = 0,
   PLAYLIST_LAST_PLAYED_STYLE_YMD_HM,
   PLAYLIST_LAST_PLAYED_STYLE_YMD,
   PLAYLIST_LAST_PLAYED_STYLE_YM,
   PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HMS,
   PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HM,
   PLAYLIST_LAST_PLAYED_STYLE_MD_HM,
   PLAYLIST_LAST_PLAYED_STYLE_MDYYYY,
   PLAYLIST_LAST_PLAYED_STYLE_MD,
   PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HMS,
   PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HM,
   PLAYLIST_LAST_PLAYED_STYLE_DDMM_HM,
   PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY,
   PLAYLIST_LAST_PLAYED_STYLE_DDMM,
   PLAYLIST_LAST_PLAYED_STYLE_YMD_HMS_AMPM,
   PLAYLIST_LAST_PLAYED_STYLE_YMD_HM_AMPM,
   PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HMS_AMPM,
   PLAYLIST_LAST_PLAYED_STYLE_MDYYYY_HM_AMPM,
   PLAYLIST_LAST_PLAYED_STYLE_MD_HM_AMPM,
   PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HMS_AMPM,
   PLAYLIST_LAST_PLAYED_STYLE_DDMMYYYY_HM_AMPM,
   PLAYLIST_LAST_PLAYED_STYLE_DDMM_HM_AMPM,
   PLAYLIST_LAST_PLAYED_STYLE_AGO,
   PLAYLIST_LAST_PLAYED_STYLE_LAST
};

/* Note: These must be kept synchronised with
 * 'enum menu_timedate_date_separator_type' in
 * 'menu_defines.h' */
enum playlist_sublabel_last_played_date_separator_type
{
   PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_HYPHEN = 0,
   PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_SLASH,
   PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_PERIOD,
   PLAYLIST_LAST_PLAYED_DATE_SEPARATOR_LAST
};

enum playlist_sublabel_runtime
{
   PLAYLIST_RUNTIME_PER_CORE = 0,
   PLAYLIST_RUNTIME_AGGREGATE,
   PLAYLIST_RUNTIME_LAST
};

RETRO_END_DECLS

#endif

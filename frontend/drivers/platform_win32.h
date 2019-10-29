/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#ifndef _PLATFORM_WIN32_H
#define _PLATFORM_WIN32_H

#if defined(HAVE_LANGEXTRA) && !defined(_XBOX)
#if (defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x0500) || !defined(_MSC_VER)
unsigned short win32_get_langid_from_retro_lang(enum retro_language lang);

enum retro_language win32_get_retro_lang_from_langid(unsigned short langid);
#endif
#endif

#endif

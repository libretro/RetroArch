/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __RARCH_POSIX_STRING_H
#define __RARCH_POSIX_STRING_H

#ifdef _WIN32

#include "../msvc/msvc_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

#undef strcasecmp
#undef strdup
#undef isblank
#undef strtok_r
#define strcasecmp(a, b) rarch_strcasecmp__(a, b)
#define strdup(orig) rarch_strdup__(orig)
#define isblank(c) rarch_isblank__(c)
#define strtok_r(str, delim, saveptr) rarch_strtok_r__(str, delim, saveptr)
int strcasecmp(const char *a, const char *b);
char *strdup(const char *orig);
int isblank(int c);
char *strtok_r(char *str, const char *delim, char **saveptr);

#ifdef __cplusplus
}
#endif

#endif
#endif


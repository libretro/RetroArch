/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __SSNES_POSIX_STRING_H
#define __SSNES_POSIX_STRING_H

#ifdef _WIN32

#ifdef __cplusplus
extern "C" {
#endif

#undef strcasecmp
#define strcasecmp(a, b) strcasecmp_ssnes__(a, b)
#define strdup(orig) strdup_ssnes__(orig)
#define isblank(c) isblank_ssnes__(c)
int strcasecmp(const char *a, const char *b);
char *strdup(const char *orig);
int isblank(int c);

#ifdef __cplusplus
}
#endif

#endif
#endif


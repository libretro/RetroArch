/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
 * Copyright (C) 2012-2013 - Michael Lelli
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _RARCH_FRONTEND_H
#define _RARCH_FRONTEND_H

#if defined(ANDROID)
#define args_type() struct android_app*
#define signature() void* data
#elif defined(__APPLE__) || defined(HAVE_BB10)
#define args_type() void*
#define signature() int argc, char *argv[]
#else
#define args_type() void*
#define signature() int argc, char *argv[]
#endif

int main_entry_iterate(signature(), args_type() args);
void main_exit(args_type() args);

#endif

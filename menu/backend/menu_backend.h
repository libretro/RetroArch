/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef DRIVER_MENU_BACKEND_H__
#define DRIVER_MENU_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_file_list_cbs
{
   int (*action_deferred_push)(void *data, void *userdata, const char
         *path, const char *label, unsigned type);
   int (*action_ok)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_cancel)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_start)(unsigned type,  const char *label, unsigned action);
   int (*action_content_list_switch)(void *data, void *userdata, const char
         *path, const char *label, unsigned type);
   int (*action_toggle)(unsigned type, const char *label, unsigned action);
} menu_file_list_cbs_t;

typedef struct menu_ctx_driver_backend
{
   int      (*iterate)(unsigned);
   const char *ident;
} menu_ctx_driver_backend_t;

#ifdef __cplusplus
}
#endif

#endif

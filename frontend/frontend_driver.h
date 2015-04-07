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

#ifndef __FRONTEND_DRIVER_H
#define __FRONTEND_DRIVER_H

#include <stddef.h>
#include <boolean.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*environment_get_t)(int *argc, char *argv[], void *args,
   void *params_data);
typedef void (*process_args_t)(int *argc, char *argv[]);

typedef struct frontend_ctx_driver
{
   environment_get_t environment_get;
   void (*init)(void *data);
   void (*deinit)(void *data);
   void (*exitspawn)(char *core_path, size_t sizeof_core_path);

   process_args_t process_args;
   void (*exec)(const char *, bool);
   void (*set_fork)(bool exitspawn, bool start_game);
   void (*shutdown)(bool);
   void (*get_name)(char *, size_t);
   void (*get_os)(char *, size_t, int *major, int *minor);
   int  (*get_rating)(void);
   void (*content_loaded)(void);

   const char *ident;

   const struct video_driver *(*get_video_driver)(void);
} frontend_ctx_driver_t;

extern const frontend_ctx_driver_t frontend_ctx_gx;
extern const frontend_ctx_driver_t frontend_ctx_ps3;
extern const frontend_ctx_driver_t frontend_ctx_xdk;
extern const frontend_ctx_driver_t frontend_ctx_qnx;
extern const frontend_ctx_driver_t frontend_ctx_apple;
extern const frontend_ctx_driver_t frontend_ctx_android;
extern const frontend_ctx_driver_t frontend_ctx_psp;
extern const frontend_ctx_driver_t frontend_ctx_ctr;
extern const frontend_ctx_driver_t frontend_ctx_win32;
extern const frontend_ctx_driver_t frontend_ctx_null;

/**
 * frontend_ctx_find_driver:
 * @ident               : Identifier name of driver to find.
 *
 * Finds driver with @ident. Does not initialize.
 *
 * Returns: pointer to driver if successful, otherwise NULL.
 **/
const frontend_ctx_driver_t *frontend_ctx_find_driver(const char *ident);

/**
 * frontend_ctx_init_first:
 *
 * Finds first suitable driver and initialize.
 *
 * Returns: pointer to first suitable driver, otherwise NULL. 
 **/
const frontend_ctx_driver_t *frontend_ctx_init_first(void);

#ifdef __cplusplus
}
#endif

#endif

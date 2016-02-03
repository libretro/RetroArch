/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../defaults.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum frontend_powerstate
{
   FRONTEND_POWERSTATE_NONE = 0,
   FRONTEND_POWERSTATE_NO_SOURCE,
   FRONTEND_POWERSTATE_CHARGING,
   FRONTEND_POWERSTATE_CHARGED,
   FRONTEND_POWERSTATE_ON_POWER_SOURCE
};

enum frontend_architecture
{
   FRONTEND_ARCH_NONE = 0,
   FRONTEND_ARCH_X86,
   FRONTEND_ARCH_X86_64,
   FRONTEND_ARCH_PPC,
   FRONTEND_ARCH_ARM,
   FRONTEND_ARCH_MIPS,
   FRONTEND_ARCH_TILE
};

typedef void (*environment_get_t)(int *argc, char *argv[], void *args,
   void *params_data);
typedef void (*process_args_t)(int *argc, char *argv[]);

typedef struct frontend_ctx_driver
{
   environment_get_t environment_get;
   void (*init)(void *data);
   void (*deinit)(void *data);
   void (*exitspawn)(char *s, size_t len);

   process_args_t process_args;
   void (*exec)(const char *, bool);
   void (*set_fork)(bool exitspawn, bool start_game, bool restart);
   void (*shutdown)(bool);
   void (*get_name)(char *, size_t);
   void (*get_os)(char *, size_t, int *major, int *minor);
   int  (*get_rating)(void);
   void (*content_loaded)(void);
   enum frontend_architecture (*get_architecture)(void);
   enum frontend_powerstate (*get_powerstate)(int *seconds, int *percent);
   int  (*parse_drive_list)(void*);

   const char *ident;

   const struct video_driver *(*get_video_driver)(void);
} frontend_ctx_driver_t;

extern frontend_ctx_driver_t frontend_ctx_gx;
extern frontend_ctx_driver_t frontend_ctx_ps3;
extern frontend_ctx_driver_t frontend_ctx_xdk;
extern frontend_ctx_driver_t frontend_ctx_qnx;
extern frontend_ctx_driver_t frontend_ctx_darwin;
extern frontend_ctx_driver_t frontend_ctx_linux;
extern frontend_ctx_driver_t frontend_ctx_psp;
extern frontend_ctx_driver_t frontend_ctx_ctr;
extern frontend_ctx_driver_t frontend_ctx_win32;
extern frontend_ctx_driver_t frontend_ctx_xenon;
extern frontend_ctx_driver_t frontend_ctx_null;

/**
 * frontend_ctx_find_driver:
 * @ident               : Identifier name of driver to find.
 *
 * Finds driver with @ident. Does not initialize.
 *
 * Returns: pointer to driver if successful, otherwise NULL.
 **/
frontend_ctx_driver_t *frontend_ctx_find_driver(const char *ident);

frontend_ctx_driver_t *frontend_get_ptr(void);

/**
 * frontend_ctx_init_first:
 *
 * Finds first suitable driver and initialize.
 *
 * Returns: pointer to first suitable driver, otherwise NULL.
 **/
frontend_ctx_driver_t *frontend_ctx_init_first(void);

int frontend_driver_parse_drive_list(void *data);

void frontend_driver_content_loaded(void);

void frontend_driver_set_fork(bool a, bool b, bool restart);

void frontend_driver_process_args(int *argc, char *argv[]);

bool frontend_driver_is_inited(void);

void frontend_driver_init_first(void *args);

void frontend_driver_free(void);

environment_get_t frontend_driver_environment_get_ptr(void);

bool frontend_driver_has_get_video_driver_func(void);

const struct video_driver *frontend_driver_get_video_driver(void);

void frontend_driver_shutdown(bool a);

void frontend_driver_deinit(void *args);

void frontend_driver_exitspawn(char *s, size_t len);

bool frontend_driver_has_fork(void);

bool frontend_driver_get_salamander_basename(char *s, size_t len);

#ifdef __cplusplus
}
#endif

#endif

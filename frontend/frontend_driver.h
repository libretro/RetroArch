/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <lists/string_list.h>

#include <libretro.h>

RETRO_BEGIN_DECLS

enum frontend_powerstate
{
   FRONTEND_POWERSTATE_NONE = 0,
   FRONTEND_POWERSTATE_NO_SOURCE,
   FRONTEND_POWERSTATE_CHARGING,
   FRONTEND_POWERSTATE_CHARGED,
   FRONTEND_POWERSTATE_ON_POWER_SOURCE
};

enum frontend_fork
{
   FRONTEND_FORK_NONE = 0,
   FRONTEND_FORK_CORE,
   FRONTEND_FORK_CORE_WITH_ARGS,
   FRONTEND_FORK_RESTART
};

enum frontend_architecture
{
   FRONTEND_ARCH_NONE = 0,
   FRONTEND_ARCH_X86,
   FRONTEND_ARCH_X86_64,
   FRONTEND_ARCH_PPC,
   FRONTEND_ARCH_ARM,
   FRONTEND_ARCH_ARMV7,
   FRONTEND_ARCH_ARMV8,
   FRONTEND_ARCH_MIPS,
   FRONTEND_ARCH_TILE
};

/* different platforms may only support some of these types */
enum path_change_type
{
   PATH_CHANGE_TYPE_MODIFIED = (1 << 0),
   PATH_CHANGE_TYPE_WRITE_FILE_CLOSED = (1 << 1),
   PATH_CHANGE_TYPE_FILE_MOVED = (1 << 2),
   PATH_CHANGE_TYPE_FILE_DELETED = (1 << 3)
};

typedef struct path_change_data
{
   void *data;
} path_change_data_t;

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
   bool (*set_fork)(enum frontend_fork fork_mode);
   void (*shutdown)(bool);
   void (*get_name)(char *, size_t);
   void (*get_os)(char *, size_t, int *major, int *minor);
   int  (*get_rating)(void);
   void (*content_loaded)(void);
   enum frontend_architecture (*get_architecture)(void);
   enum frontend_powerstate (*get_powerstate)(int *seconds, int *percent);
   int  (*parse_drive_list)(void*, bool);
   uint64_t (*get_total_mem)(void);
   uint64_t (*get_free_mem)(void);
   void (*install_signal_handler)(void);
   int (*get_signal_handler_state)(void);
   void (*set_signal_handler_state)(int value);
   void (*destroy_signal_handler_state)(void);
   void (*attach_console)(void);
   void (*detach_console)(void);
#ifdef HAVE_LAKKA
   void (*get_lakka_version)(char *, size_t);
#endif
   void (*watch_path_for_changes)(struct string_list *list, int flags, path_change_data_t **change_data);
   bool (*check_for_path_changes)(path_change_data_t *change_data);
   void (*set_sustained_performance_mode)(bool on);
   const char* (*get_cpu_model_name)(void);
   enum retro_language (*get_user_language)(void);

   const char *ident;

   const struct video_driver *(*get_video_driver)(void);
} frontend_ctx_driver_t;

extern frontend_ctx_driver_t frontend_ctx_gx;
extern frontend_ctx_driver_t frontend_ctx_wiiu;
extern frontend_ctx_driver_t frontend_ctx_ps3;
extern frontend_ctx_driver_t frontend_ctx_xdk;
extern frontend_ctx_driver_t frontend_ctx_qnx;
extern frontend_ctx_driver_t frontend_ctx_darwin;
extern frontend_ctx_driver_t frontend_ctx_unix;
extern frontend_ctx_driver_t frontend_ctx_psp;
extern frontend_ctx_driver_t frontend_ctx_ps2;
extern frontend_ctx_driver_t frontend_ctx_ctr;
extern frontend_ctx_driver_t frontend_ctx_switch;
extern frontend_ctx_driver_t frontend_ctx_win32;
extern frontend_ctx_driver_t frontend_ctx_uwp;
extern frontend_ctx_driver_t frontend_ctx_xenon;
extern frontend_ctx_driver_t frontend_ctx_emscripten;
extern frontend_ctx_driver_t frontend_ctx_dos;
extern frontend_ctx_driver_t frontend_ctx_switch;
extern frontend_ctx_driver_t frontend_ctx_orbis;

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

int frontend_driver_parse_drive_list(void *data, bool load_content);

void frontend_driver_content_loaded(void);

bool frontend_driver_set_fork(enum frontend_fork fork_mode);

void frontend_driver_process_args(int *argc, char *argv[]);

bool frontend_driver_is_inited(void);

void frontend_driver_init_first(void *args);

void frontend_driver_free(void);

enum frontend_architecture frontend_driver_get_cpu_architecture(void);

environment_get_t frontend_driver_environment_get_ptr(void);

bool frontend_driver_has_get_video_driver_func(void);

const struct video_driver *frontend_driver_get_video_driver(void);

void frontend_driver_shutdown(bool a);

void frontend_driver_deinit(void *args);

void frontend_driver_exitspawn(char *s, size_t len);

bool frontend_driver_has_fork(void);

bool frontend_driver_get_core_extension(char *s, size_t len);

bool frontend_driver_get_salamander_basename(char *s, size_t len);

uint64_t frontend_driver_get_total_memory(void);

uint64_t frontend_driver_get_free_memory(void);

void frontend_driver_install_signal_handler(void);

int frontend_driver_get_signal_handler_state(void);

void frontend_driver_set_signal_handler_state(int value);

void frontend_driver_destroy_signal_handler_state(void);

void frontend_driver_attach_console(void);

void frontend_driver_detach_console(void);

bool frontend_driver_can_watch_for_changes(void);

void frontend_driver_watch_path_for_changes(struct string_list *list, int flags, path_change_data_t **change_data);

bool frontend_driver_check_for_path_changes(path_change_data_t *change_data);

void frontend_driver_set_sustained_performance_mode(bool on);

const char* frontend_driver_get_cpu_model_name(void);

enum retro_language frontend_driver_get_user_language(void);

RETRO_END_DECLS

#endif

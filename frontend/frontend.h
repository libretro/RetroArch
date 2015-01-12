/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2015 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>

#if defined(ANDROID)
#include "drivers/platform_android.h"
#define main_entry android_app_entry
#define args_type() struct android_app*
#define signature() void* data
#define signature_expand() data
#define returntype void
#else
#if defined(__APPLE__) || defined(HAVE_BB10) || defined(EMSCRIPTEN)
#define main_entry rarch_main
#else
#define main_entry main
#endif
#define args_type() void*
#define signature() int argc, char *argv[]
#define signature_expand() argc, argv
#define returntype int
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * main_exit:
 *
 * Cleanly exit RetroArch.
 *
 * Also saves configuration files to disk,
 * and (optionally) autosave state.
 **/
void main_exit(args_type() args);
    
/**
 * main_exit_save_config:
 *
 * Saves configuration file to disk, and (optionally)
 * autosave state.
 **/
void main_exit_save_config(void);

/**
 * main_entry:
 *
 * Main function of RetroArch.
 *
 * If HAVE_MAIN_LOOP is defined, will contain main loop and will not
 * be exited from until we exit the program. Otherwise, will 
 * just do initialization.
 *
 * Returns: varies per platform.
 **/
returntype main_entry(signature());

/**
 * main_load_content:
 * @argc             : Argument count.
 * @argv             : Argument variable list.
 * @args             : Arguments passed from callee.
 * @environ_get      : Function passed for environment_get function.
 * @process_args     : Function passed for process_args function.
 *
 * Loads content file and starts up RetroArch.
 * If no content file can be loaded, will start up RetroArch
 * as-is.
 *
 * Returns: false (0) if rarch_main_init failed, otherwise true (1).
 **/
bool main_load_content(int argc, char **argv,
      args_type() args, environment_get_t environ_get,
      process_args_t process_args);

#ifdef __cplusplus
}
#endif

#endif

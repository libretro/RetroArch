/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
 * Copyright (C) 2016-2019 - Brad Parker
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdio.h>

#include "../frontend_driver.h"

static void frontend_dos_init(void *data)
{
   printf("Loading RetroArch...\n");
}

static void frontend_dos_shutdown(bool unused)
{
   (void)unused;
}

static int frontend_dos_get_rating(void)
{
   return -1;
}

enum frontend_architecture frontend_dos_get_architecture(void)
{
   return FRONTEND_ARCH_X86;
}

static void frontend_dos_get_env_settings(int *argc, char *argv[],
      void *data, void *params_data)
{
}

frontend_ctx_driver_t frontend_ctx_dos = {
   frontend_dos_get_env_settings,/* environment_get */
   frontend_dos_init,            /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_dos_shutdown,        /* shutdown */
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_dos_get_rating,      /* get_rating */
   NULL,                         /* load_content */
   frontend_dos_get_architecture,/* get_architecture */
   NULL,                         /* get_powerstate */
   NULL,                         /* parse_drive_list */
   NULL,                         /* get_mem_total */
   NULL,                         /* get_mem_free */
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_sighandler_state */
   NULL,                         /* set_sighandler_state */
   NULL,                         /* destroy_sighandler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
   NULL,                         /* get_user_language */
   "dos",
};

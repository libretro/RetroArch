/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2016 - Daniel De Matteis
 * Copyright (C) 2012-2015 - Jason Fetters
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

#include "../frontend_driver.h"

#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

static volatile sig_atomic_t bsd_sighandler_quit;

static void frontend_bsd_sighandler(int sig)
{
   (void)sig;
   if (bsd_sighandler_quit)
      exit(1);
   bsd_sighandler_quit = 1;
}

static void frontend_bsd_install_signal_handlers(void)
{
   struct sigaction sa;

   sa.sa_sigaction = NULL;
   sa.sa_handler   = frontend_bsd_sighandler;
   sa.sa_flags     = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);
}

static int frontend_bsd_get_signal_handler_state(void)
{
   return (int)bsd_sighandler_quit;
}

static void frontend_bsd_set_signal_handler_state(int value)
{
   bsd_sighandler_quit = value;
}

static void frontend_bsd_destroy_signal_handler_state(void)
{
   bsd_sighandler_quit = 0;
}

frontend_ctx_driver_t frontend_ctx_bsd = {
   NULL,                         /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   NULL,                         /* shutdown */
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   NULL,                         /* get_rating */
   NULL,                         /* load_content */
   NULL,                         /* get_architecture */
   NULL,                         /* get_powerstate */
   NULL,                         /* parse_drive_list */
   NULL,                         /* get_mem_total */
   NULL,                         /* get_mem_free */
   frontend_bsd_install_signal_handlers,
   frontend_bsd_get_signal_handler_state,
   frontend_bsd_set_signal_handler_state,
   frontend_bsd_destroy_signal_handler_state,
   "bsd",
};

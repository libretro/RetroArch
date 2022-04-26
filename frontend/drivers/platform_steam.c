/* RetroArch - A frontend for libretro.
 * Copyright (C) 2022 - M4xw <m4x@m4xw.net>
 * Copyright (C) 2022 - Libretro Team
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

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "../frontend_driver.h"

#include <steam/steam.h>

/* Statics */
static frontend_ctx_driver_t *frontend_ctx_parent;

/* Forward Declare */
frontend_ctx_driver_t frontend_ctx_steam;

static void frontend_steam_init(void *data)
{
   /* Copy vtab from frontend_ctx_parent to frontend_ctx_steam */
#if (defined(__linux__) || defined(__unix__))
   frontend_ctx_parent = frontend_ctx_find_driver("unix");
#endif
   
   /* Things we don't implement */
   frontend_ctx_steam.environment_get = frontend_ctx_parent->environment_get;
   frontend_ctx_steam.exitspawn = frontend_ctx_parent->exitspawn;
   frontend_ctx_steam.exec = frontend_ctx_parent->exec;
   frontend_ctx_steam.set_fork = frontend_ctx_parent->set_fork;
   frontend_ctx_steam.shutdown = frontend_ctx_parent->shutdown;
   frontend_ctx_steam.get_name = frontend_ctx_parent->get_name;
   frontend_ctx_steam.get_os = frontend_ctx_parent->get_os;
   frontend_ctx_steam.get_rating = frontend_ctx_parent->get_rating;
   frontend_ctx_steam.content_loaded = frontend_ctx_parent->content_loaded;
   frontend_ctx_steam.get_architecture = frontend_ctx_parent->get_architecture;
   frontend_ctx_steam.get_powerstate = frontend_ctx_parent->get_powerstate;
   frontend_ctx_steam.parse_drive_list = frontend_ctx_parent->parse_drive_list;
   frontend_ctx_steam.get_total_mem = frontend_ctx_parent->get_total_mem;
   frontend_ctx_steam.get_free_mem = frontend_ctx_parent->get_free_mem;
   frontend_ctx_steam.install_signal_handler = frontend_ctx_parent->install_signal_handler;
   frontend_ctx_steam.get_signal_handler_state = frontend_ctx_parent->get_signal_handler_state;
   frontend_ctx_steam.set_signal_handler_state = frontend_ctx_parent->set_signal_handler_state;
   frontend_ctx_steam.destroy_signal_handler_state = frontend_ctx_parent->destroy_signal_handler_state;
   frontend_ctx_steam.attach_console = frontend_ctx_parent->attach_console;
   frontend_ctx_steam.detach_console = frontend_ctx_parent->detach_console;
   frontend_ctx_steam.get_lakka_version = frontend_ctx_parent->get_lakka_version;
   frontend_ctx_steam.set_screen_brightness = frontend_ctx_parent->set_screen_brightness;
   frontend_ctx_steam.watch_path_for_changes = frontend_ctx_parent->watch_path_for_changes;
   frontend_ctx_steam.check_for_path_changes = frontend_ctx_parent->check_for_path_changes;
   frontend_ctx_steam.set_sustained_performance_mode = frontend_ctx_parent->set_sustained_performance_mode;
   frontend_ctx_steam.get_cpu_model_name = frontend_ctx_parent->get_cpu_model_name;
   frontend_ctx_steam.get_user_language = frontend_ctx_parent->get_user_language;
   frontend_ctx_steam.is_narrator_running = frontend_ctx_parent->is_narrator_running;
   frontend_ctx_steam.accessibility_speak = frontend_ctx_parent->accessibility_speak;
   frontend_ctx_steam.set_gamemode = frontend_ctx_parent->set_gamemode;
   frontend_ctx_steam.get_video_driver = frontend_ctx_parent->get_video_driver;

   /* Invoke Parent Init */
   frontend_ctx_parent->init(data);

   /* Init Mist backend */
   steam_init();
}

static void frontend_steam_deinit(void *data)
{
   /* Deinit Mist backend */
   steam_deinit();

   frontend_ctx_parent->deinit(data);
}

/* Entries will be assigned to the parent frontend drivers & their init routines will be ran */
frontend_ctx_driver_t frontend_ctx_steam = {
    NULL,                  /* environment_get */
    frontend_steam_init,   /* init */
    frontend_steam_deinit, /* deinit */
    NULL,                  /* exitspawn */
    NULL,                  /* process_args */
    NULL,                  /* exec */
    NULL,                  /* set_fork */
    NULL,                  /* shutdown */
    NULL,                  /* get_name */
    NULL,                  /* get_os */
    NULL,                  /* get_rating */
    NULL,                  /* content_loaded   */
    NULL,                  /* get_architecture */
    NULL,                  /* get_powerstate */
    NULL,                  /* parse_drive_list */
    NULL,                  /* get_total_mem */
    NULL,                  /* get_free_mem  */
    NULL,                  /* install_signal_handler */
    NULL,                  /* get_sighandler_state */
    NULL,                  /* set_sighandler_state */
    NULL,                  /* destroy_sighandler_state */
    NULL,                  /* attach_console */
    NULL,                  /* detach_console */
    NULL,                  /* get_lakka_version */
    NULL,                  /* set_screen_brightness */
    NULL,                  /* watch_path_for_changes */
    NULL,                  /* check_for_path_changes */
    NULL,                  /* set_sustained_performance_mode */
    NULL,                  /* get_cpu_model_name */
    NULL,                  /* get_user_language */
    NULL,                  /* is_narrator_running */
    NULL,                  /* accessibility_speak */
    NULL,                  /* set_gamemode        */
    "steam",               /* ident               */
    NULL                   /* get_video_driver    */
};

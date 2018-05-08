/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdint.h>
#include <stddef.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <retro_timers.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#include "frontend.h"
#include "../configuration.h"
#include "../ui/ui_companion_driver.h"
#include "../tasks/tasks_internal.h"

#include "../driver.h"
#include "../paths.h"
#include "../retroarch.h"

/* griffin hack */
#ifdef HAVE_QT
#ifndef HAVE_MAIN
#define HAVE_MAIN
#endif
#endif

#ifndef HAVE_MAIN
#include "../retroarch.h"
#endif

/**
 * main_exit:
 *
 * Cleanly exit RetroArch.
 *
 * Also saves configuration files to disk,
 * and (optionally) autosave state.
 **/
void main_exit(void *args)
{
   settings_t *settings = config_get_ptr();

   if (settings->bools.config_save_on_exit)
      command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#ifdef HAVE_MENU
   /* Do not want menu context to live any more. */
   menu_driver_ctl(RARCH_MENU_CTL_UNSET_OWN_DRIVER, NULL);
#endif
   rarch_ctl(RARCH_CTL_MAIN_DEINIT, NULL);

   command_event(CMD_EVENT_PERFCNT_REPORT_FRONTEND_LOG, NULL);

#if defined(HAVE_LOGGER) && !defined(ANDROID)
   logger_shutdown();
#endif

   frontend_driver_deinit(args);
   frontend_driver_exitspawn(
         path_get_ptr(RARCH_PATH_CORE),
         path_get_realsize(RARCH_PATH_CORE));

   rarch_ctl(RARCH_CTL_DESTROY, NULL);

   ui_companion_driver_deinit();

   frontend_driver_shutdown(false);

   driver_ctl(RARCH_DRIVER_CTL_DEINIT, NULL);
   ui_companion_driver_free();
   frontend_driver_free();
}

/**
 * main_entry:
 *
 * Main function of RetroArch.
 *
 * If HAVE_MAIN is not defined, will contain main loop and will not
 * be exited from until we exit the program. Otherwise, will
 * just do initialization.
 *
 * Returns: varies per platform.
 **/
int rarch_main(int argc, char *argv[], void *data)
{
   void *args                      = (void*)data;
#if defined(HAVE_MAIN) && defined(HAVE_QT)
   const ui_application_t *ui_application = NULL;
#endif

   rarch_ctl(RARCH_CTL_PREINIT, NULL);
   frontend_driver_init_first(args);
   rarch_ctl(RARCH_CTL_INIT, NULL);

   if (frontend_driver_is_inited())
   {
      content_ctx_info_t info;

      info.argc            = argc;
      info.argv            = argv;
      info.args            = args;
      info.environ_get     = frontend_driver_environment_get_ptr();

      if (!task_push_load_content_from_cli(
               NULL,
               NULL,
               &info,
               CORE_TYPE_PLAIN,
               NULL,
               NULL))
         return 1;
   }

   ui_companion_driver_init_first();

#if !defined(HAVE_MAIN)
   do
   {
      unsigned sleep_ms = 0;
      int           ret = runloop_iterate(&sleep_ms);

      if (ret == 1 && sleep_ms > 0)
         retro_sleep(sleep_ms);

      task_queue_check();

      if (ret == -1)
         break;
   }while(1);

   main_exit(args);
#elif defined(HAVE_QT)
   ui_application = ui_companion_driver_get_qt_application_ptr();

   if (ui_application && ui_application->run)
      ui_application->run(args);
#endif

   return 0;
}

#ifndef HAVE_MAIN
#ifdef __cplusplus
extern "C"
#endif
int main(int argc, char *argv[])
{
   return rarch_main(argc, argv, NULL);
}
#endif

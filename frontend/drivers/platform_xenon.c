/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <string.h>

#include <time/time.h>

#include <debug.h>
#include <xenos/xenos.h>
#include <diskio/ata.h>
#include <input/input.h>
#include <console/console.h>
#include <usb/usbmain.h>
#include <ppc/timebase.h>
#include <xenon_soc/xenon_power.h>
#include <elf/elf.h>

#include <boolean.h>

#include "../../dynamic.h"

static void frontend_xenon_init(void *data)
{
   (void)data;
   xenos_init(VIDEO_MODE_AUTO);
   console_init();
   xenon_make_it_faster(XENON_SPEED_FULL);
   usb_init();
   usb_do_poll();
   xenon_ata_init();
#if 0
   dvd_init();
#endif
}

static void frontend_xenon_shutdown(bool unused)
{
   (void)unused;
}

static int frontend_xenon_get_rating(void)
{
   return -1;
}

static void frontend_xenon_get_environment_settings(int *argc, char *argv[],
      void *data, void *params_data)
{
}

enum frontend_architecture frontend_xenon_get_architecture(void)
{
   return FRONTEND_ARCH_PPC;
}

frontend_ctx_driver_t frontend_ctx_qnx = {
   frontend_xenon_get_environment_settings,
   frontend_xenon_init,
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_xenon_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_xenon_get_rating,
   NULL,                         /* load_content */
   frontend_xenon_get_architecture,
   NULL,                         /* get_powerstate */
   NULL,                         /* parse_drive_list */
   NULL,                         /* get_mem_total */
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
   "xenon",
};

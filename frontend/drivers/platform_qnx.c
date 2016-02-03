/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <boolean.h>

#include <bps/bps.h>

#include "../../dynamic.h"

static void frontend_qnx_init(void *data)
{
   (void)data;
   bps_initialize();
}

static void frontend_qnx_shutdown(bool unused)
{
   (void)unused;
   bps_shutdown();
}

static int frontend_qnx_get_rating(void)
{
   /* TODO/FIXME - look at unique identifier per device and 
    * determine rating for some */
   return -1;
}

static void frontend_qnx_get_environment_settings(int *argc, char *argv[],
      void *data, void *params_data)
{
   fill_pathname_join(g_defaults.path.config, "app/native",
         "retroarch.cfg", sizeof(g_defaults.path.config));
   fill_pathname_join(g_defaults.dir.shader, "app/native",
         "shaders_glsl", sizeof(g_defaults.dir.shader));
   fill_pathname_join(g_defaults.dir.overlay, "app/native",
         "overlays", sizeof(g_defaults.dir.overlay));
   fill_pathname_join(g_defaults.dir.core, "app/native",
         "lib", sizeof(g_defaults.dir.core));
   fill_pathname_join(g_defaults.dir.core_info, "app/native",
         "info", sizeof(g_defaults.dir.core_info));
   fill_pathname_join(g_defaults.dir.playlist, "app/native",
         "playlists", sizeof(g_defaults.dir.playlist));
}

enum frontend_architecture frontend_qnx_get_architecture(void)
{
   return FRONTEND_ARCH_ARM;
}

frontend_ctx_driver_t frontend_ctx_qnx = {
   frontend_qnx_get_environment_settings,
   frontend_qnx_init,
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_qnx_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_qnx_get_rating,
   NULL,                         /* load_content */
   frontend_qnx_get_architecture,
   NULL,                         /* get_powerstate */
   NULL,                         /* parse_drive_list */
   "qnx",
};

/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <bps/bps.h>

#include <stdint.h>
#include "../../boolean.h"
#include <stddef.h>
#include <string.h>
#include "../../dynamic.h"
#include "../../libretro_private.h"

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
   fill_pathname_join(g_defaults.config_path, "app/native",
         "retroarch.cfg", sizeof(g_defaults.config_path));
   fill_pathname_join(g_defaults.shader_dir, "app/native",
         "shaders_glsl", sizeof(g_defaults.shader_dir));
   fill_pathname_join(g_defaults.overlay_dir, "app/native",
         "overlays", sizeof(g_defaults.overlay_dir));
   fill_pathname_join(g_defaults.core_dir, "app/native",
         "lib", sizeof(g_defaults.core_dir));
   fill_pathname_join(g_defaults.core_info_dir, "app/native",
         "info", sizeof(g_defaults.core_info_dir));
   fill_pathname_join(g_defaults.playlist_dir, "app/native",
         "playlists", sizeof(g_defaults.playlist_dir));
}

const frontend_ctx_driver_t frontend_ctx_qnx = {
   frontend_qnx_get_environment_settings, /* get_environment_settings */
   frontend_qnx_init,            /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* process_events */
   NULL,                         /* exec */
   frontend_qnx_shutdown,        /* shutdown */
   NULL,                         /* get_name */
   frontend_qnx_get_rating,      /* get_rating */
   "qnx",
};

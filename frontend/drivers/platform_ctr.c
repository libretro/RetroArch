/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Ali Bouhlel
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

#include <3ds.h>

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <string.h>

#include <file/file_path.h>
#ifndef IS_SALAMANDER
#include <file/file_list.h>
#endif

#include "../../general.h"

#ifdef IS_SALAMANDER
#include "../../file_ext.h"
#else
#include "../../menu/menu_list.h"
#endif


int __stacksize__ = 1*1024*1024;

const char* elf_path_cst = "sdmc:/retroarch/test.3dsx";

void wait_for_input(void);

#define DEBUG_HOLD() do{printf("%s@%s:%d.\n",__FUNCTION__, __FILE__, __LINE__);fflush(stdout);wait_for_input();}while(0)

static void frontend_ctr_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   (void)args;

#ifndef IS_SALAMANDER
#if defined(HAVE_LOGGER)
   logger_init();
#elif defined(HAVE_FILE_LOGGER)
   global_t *global  = global_get_ptr();
   global->log_file = fopen("sdmc:/retroarch/retroarch-log.txt", "w");
#endif
#endif

   fill_pathname_basedir(g_defaults.port_dir, elf_path_cst, sizeof(g_defaults.port_dir));
   RARCH_LOG("port dir: [%s]\n", g_defaults.port_dir);

   fill_pathname_join(g_defaults.core_assets_dir, g_defaults.port_dir,
         "downloads", sizeof(g_defaults.core_assets_dir));
   fill_pathname_join(g_defaults.assets_dir, g_defaults.port_dir,
         "media", sizeof(g_defaults.assets_dir));
   fill_pathname_join(g_defaults.core_dir, g_defaults.port_dir,
         "cores", sizeof(g_defaults.core_dir));
   fill_pathname_join(g_defaults.core_info_dir, g_defaults.port_dir,
         "cores", sizeof(g_defaults.core_info_dir));
   fill_pathname_join(g_defaults.savestate_dir, g_defaults.core_dir,
         "savestates", sizeof(g_defaults.savestate_dir));
   fill_pathname_join(g_defaults.sram_dir, g_defaults.core_dir,
         "savefiles", sizeof(g_defaults.sram_dir));
   fill_pathname_join(g_defaults.system_dir, g_defaults.core_dir,
         "system", sizeof(g_defaults.system_dir));
   fill_pathname_join(g_defaults.playlist_dir, g_defaults.core_dir,
         "playlists", sizeof(g_defaults.playlist_dir));
   fill_pathname_join(g_defaults.config_path, g_defaults.port_dir,
         "retroarch.cfg", sizeof(g_defaults.config_path));

#ifndef IS_SALAMANDER
#if 0
   if (argv[1] && (argv[1][0] != '\0'))
   {
      static char path[PATH_MAX_LENGTH];
      struct rarch_main_wrap *args = NULL;

      *path = '\0';
      args = (struct rarch_main_wrap*)params_data;

      if (args)
      {
         strlcpy(path, argv[1], sizeof(path));

         args->touched        = true;
         args->no_content     = false;
         args->verbose        = false;
         args->config_path    = NULL;
         args->sram_path      = NULL;
         args->state_path     = NULL;
         args->content_path   = path;
         args->libretro_path  = NULL;

         RARCH_LOG("argv[0]: %s\n", argv[0]);
         RARCH_LOG("argv[1]: %s\n", argv[1]);
         RARCH_LOG("argv[2]: %s\n", argv[2]);

         RARCH_LOG("Auto-start game %s.\n", argv[1]);
      }
   }
#endif
#endif
}

static void frontend_ctr_deinit(void *data)
{
   (void)data;
#ifndef IS_SALAMANDER
   global_t *global   = global_get_ptr();
   global->verbosity = true;

#ifdef HAVE_FILE_LOGGER
   if (global->log_file)
      fclose(global->log_file);
   global->log_file = NULL;
#endif

   wait_for_input();

   csndExit();
   gfxExit();

#if 0
   sdmcExit();
   fsExit();
   hidExit();
   aptExit();
   srvExit();
#endif
#endif
}

static void frontend_ctr_shutdown(bool unused)
{
   (void)unused;
}

#define PRINTFPOS(X,Y) "\x1b["#X";"#Y"H"
#define PRINTFPOS_STR(X,Y) "\x1b["X";"Y"H"

static void frontend_ctr_init(void *data)
{
#ifndef IS_SALAMANDER
   (void)data;
   global_t *global   = global_get_ptr();
   global->verbosity = true;

#if 0
   srvInit();
   aptInit();
   hidInit();
   fsInit();
   sdmcInit();

   APT_SetAppCpuTimeLimit(NULL, 80);
   gfxInitDefault();
#endif
   gfxInit(GSP_BGR8_OES,GSP_RGB565_OES,false);
   csndInit();
   gfxSet3D(false);
   consoleInit(GFX_BOTTOM, NULL);
#endif
}


static int frontend_ctr_get_rating(void)
{
   return 3;
}

bool select_pressed = false;

void wait_for_input(void)
{
   printf("\n\nPress Start.\n\n");
   fflush(stdout);

   while(aptMainLoop())
   {
      u32 kDown;

      hidScanInput();

      kDown = hidKeysDown();

      if (kDown & KEY_START)
         break;

      if (kDown & KEY_SELECT)
         select_pressed = true;

      rarch_sleep(1);
   }
}

enum frontend_architecture frontend_ctr_get_architecture(void)
{
   return FRONTEND_ARCH_ARM;
}

static int frontend_ctr_parse_drive_list(void *data)
{
   file_list_t *list = (file_list_t*)data;

#ifndef IS_SALAMANDER
   if (!list)
      return -1;

   menu_list_push(list,
         "sdmc:/", "", MENU_FILE_DIRECTORY, 0, 0);
#endif

   return 0;
}

const frontend_ctx_driver_t frontend_ctx_ctr = {
   frontend_ctr_get_environment_settings,
   frontend_ctr_init,
   frontend_ctr_deinit,
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_ctr_shutdown,
   NULL,                         /* get_name */
   NULL,                         /* get_os */
   frontend_ctr_get_rating,
   NULL,                         /* load_content */
   frontend_ctr_get_architecture,
   NULL,                         /* get_powerstate */
   frontend_ctr_parse_drive_list,
   "ctr",
};

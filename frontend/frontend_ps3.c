/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2013 - Daniel De Matteis
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
#include "../boolean.h"
#include <stddef.h>
#include <string.h>
#include <sys/process.h>

#include "../ps3/sdk_defines.h"
#include "../ps3/ps3_input.h"

#include "../gfx/gl_common.h"

#include "../console/rarch_console.h"

#ifdef HAVE_RARCH_EXEC
#include "../console/rarch_console_exec.h"
#endif

#include "../console/rarch_console_libretro_mgmt.h"
#include "../console/rarch_console_input.h"
#include "../console/rarch_console_config.h"
#include "../console/rarch_console_settings.h"
#include "../console/rarch_console_video.h"
#include "../conf/config_file.h"
#include "../conf/config_file_macros.h"
#include "../general.h"
#include "../file.h"

#include "../console/rmenu/rmenu.h"

#define EMULATOR_CONTENT_DIR "SSNE10000"

#ifdef HAVE_HDD_CACHE_PARTITION
#define CACHE_ID "ABCD12345"
#endif

#ifndef __PSL1GHT__
#define NP_POOL_SIZE (128*1024)
static uint8_t np_pool[NP_POOL_SIZE];
#endif

int rarch_main(int argc, char *argv[]);

SYS_PROCESS_PARAM(1001, 0x200000)

#undef main

#ifdef HAVE_SYSUTILS
static void callback_sysutil_exit(uint64_t status, uint64_t param, void *userdata)
{
   (void) param;
   (void) userdata;
#ifdef HAVE_OSKUTIL
   oskutil_params *osk = &g_extern.console.misc.oskutil_handle;
#endif
   gl_t *gl = driver.video_data;

   switch (status)
   {
      case CELL_SYSUTIL_REQUEST_EXITGAME:
         gl->quitting = true;
         rarch_settings_change(S_QUIT);
         break;
#ifdef HAVE_OSKUTIL
      case CELL_SYSUTIL_OSKDIALOG_FINISHED:
         oskutil_close(osk);
         oskutil_finished(osk);
         break;
      case CELL_SYSUTIL_OSKDIALOG_UNLOADED:
         oskutil_unload(osk);
         break;
#endif
   }
}
#endif

#ifdef __PSL1GHT__
void menu_init (void)
{
}

bool rmenu_iterate(void)
{
   rarch_console_load_game_wrap("/dev_hdd0/game/SSNE10000/USRDIR/mm3.nes", 0, 0);

   return false;
}

void menu_free (void)
{
}
#endif

static void get_environment_settings(int argc, char *argv[])
{
   g_extern.verbose = true;

   int ret;
   unsigned int get_type;
   unsigned int get_attributes;
   CellGameContentSize size;
   char dirName[CELL_GAME_DIRNAME_SIZE];
   char contentInfoPath[PATH_MAX];

#ifdef HAVE_HDD_CACHE_PARTITION
   CellSysCacheParam param;
   memset(&param, 0x00, sizeof(CellSysCacheParam));
   strlcpy(param.cacheId,CACHE_ID, sizeof(CellSysCacheParam));

   ret = cellSysCacheMount(&param);
   if(ret != CELL_SYSCACHE_RET_OK_CLEARED)
   {
      RARCH_ERR("System cache partition could not be mounted, it might be already mounted.\n");
   }
#endif

#ifdef HAVE_MULTIMAN
   /* not launched from external launcher, set default path */
   strlcpy(default_paths.multiman_self_file, "/dev_hdd0/game/BLES80608/USRDIR/RELOAD.SELF",
         sizeof(default_paths.multiman_self_file));

   if(path_file_exists(default_paths.multiman_self_file) && argc > 1 &&  path_file_exists(argv[1]))
   {
      g_extern.console.external_launch.support = EXTERN_LAUNCHER_MULTIMAN;
      RARCH_LOG("Started from multiMAN, auto-game start enabled.\n");
   }
   else
#endif
   {
      g_extern.console.external_launch.support = EXTERN_LAUNCHER_SALAMANDER;
      RARCH_WARN("Not started from multiMAN, auto-game start disabled.\n");
   }

   memset(&size, 0x00, sizeof(CellGameContentSize));

   ret = cellGameBootCheck(&get_type, &get_attributes, &size, dirName);
   if(ret < 0)
   {
      RARCH_ERR("cellGameBootCheck() Error: 0x%x.\n", ret);
   }
   else
   {
      RARCH_LOG("cellGameBootCheck() OK.\n");
      RARCH_LOG("Directory name: [%s].\n", dirName);
      RARCH_LOG(" HDD Free Size (in KB) = [%d] Size (in KB) = [%d] System Size (in KB) = [%d].\n", size.hddFreeSizeKB, size.sizeKB, size.sysSizeKB);

      switch(get_type)
      {
         case CELL_GAME_GAMETYPE_DISC:
            RARCH_LOG("RetroArch was launched on Optical Disc Drive.\n");
            break;
         case CELL_GAME_GAMETYPE_HDD:
            RARCH_LOG("RetroArch was launched on HDD.\n");
            break;
      }

      if((get_attributes & CELL_GAME_ATTRIBUTE_APP_HOME) == CELL_GAME_ATTRIBUTE_APP_HOME)
         RARCH_LOG("RetroArch was launched from host machine (APP_HOME).\n");

      ret = cellGameContentPermit(contentInfoPath, default_paths.port_dir);

#ifdef HAVE_MULTIMAN
      if(g_extern.console.external_launch.support == EXTERN_LAUNCHER_MULTIMAN)
      {
         snprintf(contentInfoPath, sizeof(contentInfoPath), "/dev_hdd0/game/%s", EMULATOR_CONTENT_DIR);
         snprintf(default_paths.port_dir, sizeof(default_paths.port_dir), "/dev_hdd0/game/%s/USRDIR", EMULATOR_CONTENT_DIR);
      }
#endif

      if(ret < 0)
      {
         RARCH_ERR("cellGameContentPermit() Error: 0x%x\n", ret);
      }
      else
      {
         RARCH_LOG("cellGameContentPermit() OK.\n");
         RARCH_LOG("contentInfoPath : [%s].\n", contentInfoPath);
         RARCH_LOG("usrDirPath : [%s].\n", default_paths.port_dir);
      }

#ifdef HAVE_HDD_CACHE_PARTITION
      snprintf(default_paths.cache_dir, sizeof(default_paths.cache_dir), "/dev_hdd1/");
#endif
      snprintf(default_paths.core_dir, sizeof(default_paths.core_dir), "%s/cores", default_paths.port_dir);
      snprintf(default_paths.executable_extension, sizeof(default_paths.executable_extension), ".SELF");
      snprintf(default_paths.savestate_dir, sizeof(default_paths.savestate_dir), "%s/savestates", default_paths.core_dir);
      snprintf(default_paths.filesystem_root_dir, sizeof(default_paths.filesystem_root_dir), "/");
      snprintf(default_paths.filebrowser_startup_dir, sizeof(default_paths.filebrowser_startup_dir), default_paths.filesystem_root_dir);
      snprintf(default_paths.sram_dir, sizeof(default_paths.sram_dir), "%s/sram", default_paths.core_dir);

      snprintf(default_paths.system_dir, sizeof(default_paths.system_dir), "%s/system", default_paths.core_dir);

      /* now we fill in all the variables */
      snprintf(default_paths.border_file, sizeof(default_paths.border_file), "%s/borders/Centered-1080p/mega-man-2.png", default_paths.core_dir);
      snprintf(default_paths.menu_border_file, sizeof(default_paths.menu_border_file), "%s/borders/Menu/main-menu.png", default_paths.core_dir);
      snprintf(default_paths.cgp_dir, sizeof(default_paths.cgp_dir), "%s/presets", default_paths.core_dir);
      snprintf(default_paths.input_presets_dir, sizeof(default_paths.input_presets_dir), "%s/input", default_paths.cgp_dir);
      snprintf(default_paths.border_dir, sizeof(default_paths.border_dir), "%s/borders", default_paths.core_dir);
#if defined(HAVE_CG) || defined(HAVE_GLSL)
      snprintf(default_paths.shader_dir, sizeof(default_paths.shader_dir), "%s/shaders", default_paths.core_dir);
      snprintf(default_paths.shader_file, sizeof(default_paths.shader_file), "%s/shaders/stock.cg", default_paths.core_dir);
      snprintf(default_paths.menu_shader_file, sizeof(default_paths.menu_shader_file), "%s/shaders/Borders/Menu/border-only-rarch.cg", default_paths.core_dir);
#endif
      snprintf(g_extern.config_path, sizeof(g_extern.config_path), "%s/retroarch.cfg", default_paths.port_dir);
      snprintf(default_paths.salamander_file, sizeof(default_paths.salamander_file), "EBOOT.BIN");
   }

   g_extern.verbose = false;
}

int main(int argc, char *argv[])
{
#ifdef HAVE_SYSUTILS
   RARCH_LOG("Registering system utility callback...\n");
   cellSysutilRegisterCallback(0, callback_sysutil_exit, NULL);
#endif

#ifdef HAVE_SYSMODULES

#ifdef HAVE_FREETYPE
   cellSysmoduleLoadModule(CELL_SYSMODULE_FONT);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FREETYPE);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FONTFT);
#endif

   cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
   cellSysmoduleLoadModule(CELL_SYSMODULE_FS);
#ifndef __PSL1GHT__
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_GAME);
   cellSysmoduleLoadModule(CELL_SYSMODULE_AVCONF_EXT);
#endif
   cellSysmoduleLoadModule(CELL_SYSMODULE_PNGDEC);
   cellSysmoduleLoadModule(CELL_SYSMODULE_JPGDEC);
   cellSysmoduleLoadModule(CELL_SYSMODULE_NET);
   cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_NP);
#endif

#ifndef __PSL1GHT__
   sys_net_initialize_network();
#endif

#ifdef HAVE_LOGGER
   logger_init();
#endif

#ifndef __PSL1GHT__
   sceNpInit(NP_POOL_SIZE, np_pool);
#endif

   rarch_main_clear_state();
   get_environment_settings(argc, argv);

   config_set_defaults();

   init_drivers_pre();
   driver.input->init();

   rarch_settings_set_default();
   rarch_input_set_controls_default(driver.input);
   rarch_config_load();

#ifdef HAVE_LIBRETRO_MANAGEMENT
   char core_exe_path[PATH_MAX];
   char path_prefix[PATH_MAX];
   const char *extension = default_paths.executable_extension;
   snprintf(path_prefix, sizeof(path_prefix), "%s/", default_paths.core_dir);
   snprintf(core_exe_path, sizeof(core_exe_path), "%sCORE%s", path_prefix, extension);


   if (path_file_exists(core_exe_path))
   {
      if (rarch_libretro_core_install(core_exe_path, path_prefix, path_prefix, 
               g_extern.config_path, extension))
      {
         RARCH_LOG("New default libretro core saved to config file: %s.\n", g_settings.libretro);
         config_save_file(g_extern.config_path);
      }
   }
#endif

   init_libretro_sym();

   driver.input->post_init();

#if (CELL_SDK_VERSION > 0x340000) && !defined(__PSL1GHT__)
   if (g_extern.console.screen.state.screenshots.enable)
   {
#ifdef HAVE_SYSMODULES
      cellSysmoduleLoadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
#endif
#ifdef HAVE_SYSUTILS
      CellScreenShotSetParam screenshot_param = {0, 0, 0, 0};

      screenshot_param.photo_title = "RetroArch PS3";
      screenshot_param.game_title = "RetroArch PS3";
      cellScreenShotSetParameter (&screenshot_param);
      cellScreenShotEnable();
#endif
   }
#ifdef HAVE_SYSUTILS
   if (g_extern.console.sound.custom_bgm.enable)
      cellSysutilEnableBgmPlayback();
#endif
#endif

   driver.video->start();

#ifdef HAVE_OSKUTIL
   oskutil_params *osk = &g_extern.console.misc.oskutil_handle;
   oskutil_init(osk, 0);
#endif

   menu_init();

   switch(g_extern.console.external_launch.support)
   {
      case EXTERN_LAUNCHER_SALAMANDER:
         g_extern.console.rmenu.mode = MODE_MENU;
         break;
#ifdef HAVE_MULTIMAN
      case EXTERN_LAUNCHER_MULTIMAN:
         RARCH_LOG("Started from multiMAN, will auto-start game.\n");
         strlcpy(g_extern.file_state.rom_path, argv[1], sizeof(g_extern.file_state.rom_path));
         rarch_settings_change(S_START_RARCH);
         break;
#endif
      default:
         break;
   }

begin_loop:
   if(g_extern.console.rmenu.mode == MODE_EMULATION)
   {
      driver.input->poll(NULL);
      driver.video->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);
      while(rarch_main_iterate());
   }
   else if (g_extern.console.rmenu.mode == MODE_INIT)
   {
      if(g_extern.main_is_init)
         rarch_main_deinit();

      struct rarch_main_wrap args = {0};

      args.verbose = g_extern.verbose;
      args.config_path = g_extern.config_path;
      args.sram_path = g_extern.console.main_wrap.state.default_sram_dir.enable ? g_extern.console.main_wrap.paths.default_sram_dir : NULL,
         args.state_path = g_extern.console.main_wrap.state.default_savestate_dir.enable ? g_extern.console.main_wrap.paths.default_savestate_dir : NULL,
         args.rom_path = g_extern.file_state.rom_path;
      args.libretro_path = g_settings.libretro;

      int init_ret = rarch_main_init_wrap(&args);

      if (init_ret == 0)
         RARCH_LOG("rarch_main_init succeeded.\n");
      else
         RARCH_ERR("rarch_main_init failed.\n");
   }
   else if(g_extern.console.rmenu.mode == MODE_MENU)
      while(rmenu_iterate());
   else
      goto begin_shutdown;

   goto begin_loop;

begin_shutdown:
   config_save_file(g_extern.config_path);

   if(g_extern.main_is_init)
      rarch_main_deinit();

   driver.input->free(NULL);
   driver.video->stop();
   menu_free();

#ifdef HAVE_OSKUTIL
   if(osk)
      oskutil_unload(osk);
#endif

#ifdef HAVE_LOGGER
   logger_shutdown();
#endif

#if defined(HAVE_SYSMODULES)

#ifdef HAVE_FREETYPE
   /* Freetype font PRX */
   cellSysmoduleLoadModule(CELL_SYSMODULE_FONTFT);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_FREETYPE);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_FONT);
#endif

#ifndef __PSL1GHT__
   /* screenshot PRX */
   if(g_extern.console.screen.state.screenshots.enable)
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_SCREENSHOT);
#endif

   cellSysmoduleUnloadModule(CELL_SYSMODULE_JPGDEC);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_PNGDEC);

#ifndef __PSL1GHT__
   /* system game utility PRX */
   cellSysmoduleUnloadModule(CELL_SYSMODULE_AVCONF_EXT);
   cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_GAME);
#endif

#endif

#ifdef HAVE_HDD_CACHE_PARTITION
   int ret = cellSysCacheClear();

   if(ret != CELL_SYSCACHE_RET_OK_CLEARED)
   {
      RARCH_ERR("System cache partition could not be cleared on exit.\n");
   }
#endif

#ifdef HAVE_RARCH_EXEC
   if(g_extern.console.external_launch.enable)
      rarch_console_exec(g_extern.console.external_launch.launch_app);
#endif

   return 1;
}

/* RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <ctype.h>
#include <boolean.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>

#include <3ds.h>
#include <3ds/svc.h>
#include <3ds/os.h>
#include <3ds/services/cfgu.h>
#include <3ds/services/ptmu.h>
#include <3ds/services/mcuhwc.h>

#include <file/file_path.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef IS_SALAMANDER
#include <lists/file_list.h>
#endif

#include "../frontend_driver.h"
#include "../../verbosity.h"
#include "../../defaults.h"
#include "../../paths.h"
#include "retroarch.h"
#include "file_path_special.h"

#include "ctr/ctr_debug.h"
#include "ctr/exec-3dsx/exec_3dsx.h"
#include "ctr/exec-3dsx/exec_cia.h"

#ifndef IS_SALAMANDER
#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#endif

static enum frontend_fork ctr_fork_mode = FRONTEND_FORK_NONE;
static const char* elf_path_cst         = "sdmc:/retroarch/retroarch.3dsx";

static void get_first_valid_core(char* path_return)
{
   DIR* dir;
   struct dirent* ent;
   const char* extension = envIsHomebrew() ? ".3dsx" : ".cia";

   path_return[0] = '\0';

   dir = opendir("sdmc:/retroarch/cores");
   if (dir != NULL)
   {
      while (ent = readdir(dir))
      {
         if (ent == NULL)
            break;
         if (strlen(ent->d_name) > strlen(extension) && !strcmp(ent->d_name + strlen(ent->d_name) - strlen(extension), extension))
         {
            strcpy(path_return, "sdmc:/retroarch/cores");
            strcat(path_return, "/");
            strcat(path_return, ent->d_name);
            break;
         }
      }
      closedir(dir);
   }
}

static void frontend_ctr_get_environment_settings(int* argc, char* argv[],
      void* args, void* params_data)
{
   (void)args;

   fill_pathname_basedir(g_defaults.dirs[DEFAULT_DIR_PORT], elf_path_cst, sizeof(g_defaults.dirs[DEFAULT_DIR_PORT]));
   RARCH_LOG("port dir: [%s]\n", g_defaults.dirs[DEFAULT_DIR_PORT]);

   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "downloads", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_ASSETS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "media", sizeof(g_defaults.dirs[DEFAULT_DIR_ASSETS]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "cores", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CORE_INFO], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "info", sizeof(g_defaults.dirs[DEFAULT_DIR_CORE_INFO]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SAVESTATE], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "savestates", sizeof(g_defaults.dirs[DEFAULT_DIR_SAVESTATE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SRAM], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "savefiles", sizeof(g_defaults.dirs[DEFAULT_DIR_SRAM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_SYSTEM], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "system", sizeof(g_defaults.dirs[DEFAULT_DIR_SYSTEM]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_PLAYLIST], g_defaults.dirs[DEFAULT_DIR_CORE],
                      "playlists", sizeof(g_defaults.dirs[DEFAULT_DIR_PLAYLIST]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "config", sizeof(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_REMAP], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "config/remaps", sizeof(g_defaults.dirs[DEFAULT_DIR_REMAP]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "filters", sizeof(g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_DATABASE], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "database/rdb", sizeof(g_defaults.dirs[DEFAULT_DIR_DATABASE]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_CURSOR], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "database/cursors", sizeof(g_defaults.dirs[DEFAULT_DIR_CURSOR]));
   fill_pathname_join(g_defaults.dirs[DEFAULT_DIR_LOGS], g_defaults.dirs[DEFAULT_DIR_PORT],
                      "logs", sizeof(g_defaults.dirs[DEFAULT_DIR_LOGS]));
   fill_pathname_join(g_defaults.path.config, g_defaults.dirs[DEFAULT_DIR_PORT],
                      file_path_str(FILE_PATH_MAIN_CONFIG), sizeof(g_defaults.path.config));
}

static void frontend_ctr_deinit(void* data)
{
   Handle lcd_handle;
   u32 parallax_layer_reg_state;
   u8 not_2DS;
   u8 device_model = 0xFF;

   extern PrintConsole* currentConsole;

   (void)data;

#ifndef IS_SALAMANDER
   /* Note: frontend_ctr_deinit() is normally called when
    * forking to load new content. When this happens, the
    * log messages generated in frontend_ctr_exec() *must*
    * be printed to screen (provided bottom screen is not
    * turned off...), since the 'first core launch' warning
    * can prevent sdcard corruption. We therefore close any
    * existing log file, enable verbose logging and revert
    * to console output. (Normal logging will be resumed
    * once retroarch.cfg has been re-read) */
   retro_main_log_file_deinit();
   verbosity_enable();
   retro_main_log_file_init(NULL, false);

   if ((gfxBottomFramebuffers[0] == (u8*)currentConsole->frameBuffer)
         && (ctr_fork_mode == FRONTEND_FORK_NONE))
      wait_for_input();

   CFGU_GetModelNintendo2DS(&not_2DS);

   if (not_2DS && srvGetServiceHandle(&lcd_handle, "gsp::Lcd") >= 0)
   {
      u32* cmdbuf = getThreadCommandBuffer();
      cmdbuf[0]   = 0x00110040;
      cmdbuf[1]   = 2;
      svcSendSyncRequest(lcd_handle);
      svcCloseHandle(lcd_handle);
   }

   /* Only O3DS and O3DSXL support running in 'dual-framebuffer'
    * mode with the parallax barrier disabled
    * (i.e. these are the only platforms that can use
    * CTR_VIDEO_MODE_2D_400x240 and CTR_VIDEO_MODE_2D_800x240) */
   CFGU_GetSystemModel(&device_model); /* (0 = O3DS, 1 = O3DSXL, 2 = N3DS, 3 = 2DS, 4 = N3DSXL, 5 = N2DSXL) */
   if ((device_model == 0) || (device_model == 1))
   {
      parallax_layer_reg_state = (*(float*)0x1FF81080 == 0.0) ? 0x0 : 0x00010001;
      GSPGPU_WriteHWRegs(0x202000, &parallax_layer_reg_state, 4);
   }

   mcuHwcExit();
   ptmuExit();
   cfguExit();
   ndspExit();
   csndExit();
   gfxTopRightFramebuffers[0] = NULL;
   gfxTopRightFramebuffers[1] = NULL;
   gfxExit();
#endif
}

static void frontend_ctr_exec(const char* path, bool should_load_game)
{
   char game_path[PATH_MAX];
   const char* arg_data[3];
   errorConf error_dialog;
   char error_string[200 + PATH_MAX];
   int args           = 0;
   int error          = 0;

   DEBUG_VAR(path);
   DEBUG_STR(path);

   game_path[0]       = '\0';
   arg_data[0]        = NULL;

   arg_data[args]     = elf_path_cst;
   arg_data[args + 1] = NULL;
   args++;

   RARCH_LOG("Attempt to load core: [%s].\n", path);
#ifndef IS_SALAMANDER
   if (should_load_game && !path_is_empty(RARCH_PATH_CONTENT))
   {
      strcpy(game_path, path_get(RARCH_PATH_CONTENT));
      arg_data[args] = game_path;
      arg_data[args + 1] = NULL;
      args++;
      RARCH_LOG("content path: [%s].\n", path_get(RARCH_PATH_CONTENT));
   }
#endif

   if (path && path[0])
   {
#ifdef IS_SALAMANDER
      struct stat sbuff;
      bool file_exists = stat(path, &sbuff) == 0;

      if (!file_exists)
      {
         char core_path[PATH_MAX];

         core_path[0] = '\0';

         /* find first valid core and load it if the target core doesnt exist */
         get_first_valid_core(&core_path[0]);

         if (core_path[0] == '\0')
         {
            errorInit(&error_dialog, ERROR_TEXT, CFG_LANGUAGE_EN);
            errorText(&error_dialog, "There are no cores installed, install a core to continue.");
            errorDisp(&error_dialog);
            exit(0);
         }
      }
#endif

      if (envIsHomebrew())
         exec_3dsx_no_path_in_args(path, arg_data);
      else
      {
         RARCH_WARN("\n");
         RARCH_WARN("\n");
         RARCH_WARN("Warning:\n");
         RARCH_WARN("First core launch may take 20\n");
         RARCH_WARN("seconds! Do not force quit\n");
         RARCH_WARN("before then or your memory\n");
         RARCH_WARN("card may be corrupted!\n");
         RARCH_WARN("\n");
         RARCH_WARN("\n");
         exec_cia(path, arg_data);
      }

      errorInit(&error_dialog, ERROR_TEXT, CFG_LANGUAGE_EN);
      snprintf(error_string, sizeof(error_string),
            "Can't launch core:%s", path);
      errorText(&error_dialog, error_string);
      errorDisp(&error_dialog);
      exit(0); /* couldnt launch new core, but context 
                  is corrupt so we have to quit */
   }
}

#ifndef IS_SALAMANDER
static bool frontend_ctr_set_fork(enum frontend_fork fork_mode)
{
   switch (fork_mode)
   {
      case FRONTEND_FORK_CORE:
         RARCH_LOG("FRONTEND_FORK_CORE\n");
         ctr_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_CORE_WITH_ARGS:
         RARCH_LOG("FRONTEND_FORK_CORE_WITH_ARGS\n");
         ctr_fork_mode  = fork_mode;
         break;
      case FRONTEND_FORK_RESTART:
         RARCH_LOG("FRONTEND_FORK_RESTART\n");
         /*  NOTE: We don't implement Salamander, so just turn
             this into FRONTEND_FORK_CORE. */
         ctr_fork_mode  = FRONTEND_FORK_CORE;
         break;
      case FRONTEND_FORK_NONE:
      default:
         return false;
   }

   return true;
}
#endif

static void frontend_ctr_exitspawn(char* s, size_t len)
{
   bool should_load_game = false;
#ifndef IS_SALAMANDER
   if (ctr_fork_mode == FRONTEND_FORK_NONE)
      return;

   switch (ctr_fork_mode)
   {
      case FRONTEND_FORK_CORE_WITH_ARGS:
         should_load_game = true;
         break;
      default:
         break;
   }
#endif
   frontend_ctr_exec(s, should_load_game);
}

static void frontend_ctr_shutdown(bool unused)
{
   (void)unused;
}

static void ctr_check_dspfirm(void)
{
   FILE* dsp_fp = fopen("sdmc:/3ds/dspfirm.cdc", "rb");

   if (dsp_fp)
      fclose(dsp_fp);
   else
   {
      FILE *code_fp = fopen("sdmc:/3ds/code.bin", "rb");

      if (code_fp)
      {
         size_t code_size;
         uint32_t* code_buffer     = NULL;

         fseek(code_fp, 0, SEEK_END);
         code_size = ftell(code_fp);
         fseek(code_fp, 0, SEEK_SET);

         code_buffer = (uint32_t*) malloc(code_size);

         if (code_buffer)
         {
            uint32_t *ptr = NULL;

            fread(code_buffer, 1, code_size, code_fp);

            for (ptr = code_buffer + 0x40;
                 ptr < (code_buffer + (code_size >> 2)); ptr++)
            {
               const uint32_t dsp1_magic = 0x31505344; /* "DSP1" */
               if (*ptr == dsp1_magic)
               {
                  size_t dspfirm_size = ptr[1];
                  ptr -= 0x40;
                  if ((ptr + (dspfirm_size >> 2)) > 
                        (code_buffer + (code_size >> 2)))
                     break;

                  dsp_fp = fopen("sdmc:/3ds/dspfirm.cdc", "wb");
                  if (!dsp_fp)
                     break;
                  fwrite(ptr, 1, dspfirm_size, dsp_fp);
                  fclose(dsp_fp);
                  break;
               }
            }
            free(code_buffer);
         }
         fclose(code_fp);
      }
      else
      {
         RARCH_WARN("\n");
         RARCH_WARN("\n");
         RARCH_WARN("Warning:\n");
         RARCH_WARN("3DS DSP dump is missing.\n");
         RARCH_WARN("A working DSP dump is required\n");
         RARCH_WARN("for correct operation.\n");
         RARCH_WARN("\n");
         RARCH_WARN("\n");
      }
   }
}

__attribute__((weak)) Result svchax_init(bool patch_srv);
__attribute__((weak)) u32 __ctr_patch_services;

void gfxSetFramebufferInfo(gfxScreen_t screen, u8 id);

static void frontend_ctr_init(void* data)
{
#ifndef IS_SALAMANDER
   extern audio_driver_t audio_null;

   (void)data;

   verbosity_enable();

   gfxInit(GSP_BGR8_OES, GSP_RGB565_OES, false);

   u32 topSize               = 400 * 240 * 3;
   u32 bottomSize            = 320 * 240 * 2;
   linearFree(gfxTopLeftFramebuffers [0]);
   linearFree(gfxTopLeftFramebuffers [1]);
   linearFree(gfxBottomFramebuffers  [0]);
   linearFree(gfxBottomFramebuffers  [1]);
   linearFree(gfxTopRightFramebuffers[0]);
   linearFree(gfxTopRightFramebuffers[1]);

   gfxTopLeftFramebuffers [0] = linearAlloc(topSize * 2);
   gfxTopRightFramebuffers[0] = gfxTopLeftFramebuffers[0] + topSize;

   gfxTopLeftFramebuffers [1] = linearAlloc(topSize * 2);
   gfxTopRightFramebuffers[1] = gfxTopLeftFramebuffers[1] + topSize;

   gfxBottomFramebuffers  [0] = linearAlloc(bottomSize);
   gfxBottomFramebuffers  [1] = linearAlloc(bottomSize);

   gfxSetFramebufferInfo(GFX_TOP, 0);
   gfxSetFramebufferInfo(GFX_BOTTOM, 0);

   gfxSet3D(true);
   consoleInit(GFX_BOTTOM, NULL);

   /* enable access to all service calls when possible. */
   if (svchax_init)
   {
      osSetSpeedupEnable(false);
      svchax_init(__ctr_patch_services);
   }
   osSetSpeedupEnable(true);

   if (csndInit() != 0)
      audio_ctr_csnd = audio_null;
   ctr_check_dspfirm();
   if (ndspInit() != 0)
      audio_ctr_dsp = audio_null;
   cfguInit();
   ptmuInit();
   mcuHwcInit();
#endif
}

static int frontend_ctr_get_rating(void)
{
   u8 device_model = 0xFF;
   
   /*(0 = O3DS, 1 = O3DSXL, 2 = N3DS, 3 = 2DS, 4 = N3DSXL, 5 = N2DSXL)*/
   CFGU_GetSystemModel(&device_model);

   switch (device_model)
   {
      case 0:
      case 1:
      case 3:
         /*Old 3/2DS*/
         return 3;
      case 2:
      case 4:
      case 5:
         /*New 3/2DS*/
         return 6;
      default:
         /*Unknown Device Or Check Failed*/
         break;
   }

   return -1;
}

enum frontend_architecture frontend_ctr_get_architecture(void)
{
   return FRONTEND_ARCH_ARM;
}

static int frontend_ctr_parse_drive_list(void* data, bool load_content)
{
#ifndef IS_SALAMANDER
   file_list_t* list = (file_list_t*)data;
   enum msg_hash_enums enum_idx = load_content
      ? MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR
      : MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY;

   if (!list)
      return -1;

   menu_entries_append_enum(list,
         "sdmc:/",
         msg_hash_to_str(
            MENU_ENUM_LABEL_FILE_DETECT_CORE_LIST_PUSH_DIR),
         enum_idx,
         FILE_TYPE_DIRECTORY, 0, 0);
#endif

   return 0;
}

static uint64_t frontend_ctr_get_mem_total(void)
{
   return osGetMemRegionSize(MEMREGION_ALL);
}

static uint64_t frontend_ctr_get_mem_free(void)
{
   return osGetMemRegionFree(MEMREGION_ALL);
}

static enum frontend_powerstate frontend_ctr_get_powerstate(
      int* seconds, int* percent)
{
   u8                 battery_percent = 0;
   u8                        charging = 0;

   mcuHwcGetBatteryLevel(&battery_percent);

   *percent                           = battery_percent;
   /* 3DS does not support seconds of charge remaining */
   *seconds                           = -1;

   PTMU_GetBatteryChargeState(&charging);

   if (charging)
   {
      if (battery_percent == 100)
         return FRONTEND_POWERSTATE_CHARGED;
      return FRONTEND_POWERSTATE_CHARGING;
   }

   return FRONTEND_POWERSTATE_ON_POWER_SOURCE;
}

static void frontend_ctr_get_os(char* s, size_t len, int* major, int* minor)
{
   OS_VersionBin cver;
   OS_VersionBin nver;

   strlcpy(s, "3DS OS", len);
   Result data_invalid = osGetSystemVersionData(&nver, &cver);
   if (data_invalid == 0)
   {
      *major = cver.mainver;
      *minor = cver.minor;
   }
   else
   {
      *major = 0;
      *minor = 0;
   }

}

static void frontend_ctr_get_name(char* s, size_t len)
{
   u8 device_model = 0xFF;
   
   /*(0 = O3DS, 1 = O3DSXL, 2 = N3DS, 3 = 2DS, 4 = N3DSXL, 5 = N2DSXL)*/
   CFGU_GetSystemModel(&device_model);

   switch (device_model)
   {
      case 0:
         strlcpy(s, "Old 3DS", len);
         break;
      case 1:
         strlcpy(s, "Old 3DS XL", len);
         break;
      case 2:
         strlcpy(s, "New 3DS", len);
         break;
      case 3:
         strlcpy(s, "Old 2DS", len);
         break;
      case 4:
         strlcpy(s, "New 3DS XL", len);
         break;
      case 5:
         strlcpy(s, "New 2DS XL", len);
         break;

      default:
         strlcpy(s, "Unknown Device", len);
         break;
   }
}

frontend_ctx_driver_t frontend_ctx_ctr =
{
   frontend_ctr_get_environment_settings,
   frontend_ctr_init,
   frontend_ctr_deinit,
   frontend_ctr_exitspawn,
   NULL,                         /* process_args */
   frontend_ctr_exec,
#ifdef IS_SALAMANDER
   NULL,
#else
   frontend_ctr_set_fork,
#endif
   frontend_ctr_shutdown,
   frontend_ctr_get_name,
   frontend_ctr_get_os,
   frontend_ctr_get_rating,
   NULL,                         /* load_content */
   frontend_ctr_get_architecture,
   frontend_ctr_get_powerstate,
   frontend_ctr_parse_drive_list,
   frontend_ctr_get_mem_total,
   frontend_ctr_get_mem_free,
   NULL,                         /* install_signal_handler */
   NULL,                         /* get_signal_handler_state */
   NULL,                         /* set_signal_handler_state */
   NULL,                         /* destroy_signal_handler_state */
   NULL,                         /* attach_console */
   NULL,                         /* detach_console */
   NULL,                         /* watch_path_for_changes */
   NULL,                         /* check_for_path_changes */
   NULL,                         /* set_sustained_performance_mode */
   NULL,                         /* get_cpu_model_name */
   NULL,                         /* get_user_language */
   "ctr",
};

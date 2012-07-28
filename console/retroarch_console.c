/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "../boolean.h"
#include "../compat/strl.h"
#include "../libretro.h"
#include "../general.h"
#include "../compat/strl.h"
#include "retroarch_console.h"
#include "../file.h"

#define MAX_ARGS 32

default_paths_t default_paths;

/*============================================================
  VIDEO EXTENSIONS
  ============================================================ */

#if defined(HAVE_HLSL)
#include "../gfx/shader_hlsl.h"
#elif defined(HAVE_CG) && defined(HAVE_OPENGL)
#include "../gfx/shader_cg.h"
#endif

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { "1:1",    1.0f },
   { "2:1",    2.0f },
   { "3:2",    1.5f },
   { "3:4",    0.75f },
   { "4:1",    4.0f },
   { "4:3",    1.3333f },
   { "4:4",    1.0f },
   { "5:4",    1.25f },
   { "6:5",    1.2f },
   { "7:9",    0.7777f },
   { "8:3",    2.6666f },
   { "8:7",    1.1428f },
   { "16:9",   1.7778f },
   { "16:10",  1.6f },
   { "16:15",  3.2f },
   { "19:12",  1.5833f },
   { "19:14",  1.3571f },
   { "30:17",  1.7647f },
   { "32:9",   3.5555f },
   { "Auto",   1.0f },
   { "Custom", 0.0f }
};

char rotation_lut[ASPECT_RATIO_END][PATH_MAX] =
{
   "Normal",
   "Vertical",
   "Flipped",
   "Flipped Rotated"
};

void rarch_set_auto_viewport(unsigned width, unsigned height)
{
   if(width == 0 || height == 0)
      return;

   unsigned aspect_x, aspect_y, len, highest, i;

   len = width < height ? width : height;
   highest = 1;
   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   aspect_x = width / highest;
   aspect_y = height / highest;

   snprintf(aspectratio_lut[ASPECT_RATIO_AUTO].name, sizeof(aspectratio_lut[ASPECT_RATIO_AUTO].name), "%d:%d (Auto)", aspect_x, aspect_y);
   aspectratio_lut[ASPECT_RATIO_AUTO].value = (int)aspect_x / (int)aspect_y;
}

#if defined(HAVE_HLSL) || defined(HAVE_CG) || defined(HAVE_GLSL)
void rarch_load_shader(unsigned slot, const char *path)
{
#if defined(HAVE_HLSL)
   hlsl_load_shader(slot, path);
#elif defined(HAVE_CG) && defined(HAVE_OPENGL)
   gl_cg_load_shader(slot, path);
#else
RARCH_WARN("Shader support is not implemented for this build.\n");
#endif

   if (g_console.info_msg_enable)
      rarch_settings_msg(S_MSG_SHADER_LOADING_SUCCEEDED, S_DELAY_180);
}
#endif

/*============================================================
  RetroArch MAIN WRAP
  ============================================================ */

#ifdef HAVE_RARCH_MAIN_WRAP

static int rarch_main_init_wrap(const struct rarch_main_wrap *args)
{
   int argc = 0;
   char *argv[MAX_ARGS] = {NULL};

   argv[argc++] = strdup("retroarch");
   
   if (args->rom_path)
      argv[argc++] = strdup(args->rom_path);

   if (args->sram_path)
   {
      argv[argc++] = strdup("-s");
      argv[argc++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[argc++] = strdup("-S");
      argv[argc++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[argc++] = strdup("-c");
      argv[argc++] = strdup(args->config_path);
   }

   if (args->verbose)
      argv[argc++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   RARCH_LOG("foo\n");
   for(int i = 0; i < argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
   RARCH_LOG("bar\n");
#endif

   int ret = rarch_main_init(argc, argv);

   char **tmp = argv;
   while (*tmp)
   {
      free(*tmp);
      tmp++;
   }

   return ret;
}

bool rarch_startup (const char * config_path)
{
   bool retval = false;

   if(g_console.initialize_rarch_enable)
   {
      if(g_console.emulator_initialized)
         rarch_main_deinit();

      struct rarch_main_wrap args = {0};

      args.verbose = g_extern.verbose;
      args.config_path = config_path;
      args.sram_path = g_console.default_sram_dir_enable ? g_console.default_sram_dir : NULL,
      args.state_path = g_console.default_savestate_dir_enable ? g_console.default_savestate_dir : NULL,
      args.rom_path = g_console.rom_path;

      int init_ret = rarch_main_init_wrap(&args);
      (void)init_ret;

      if(init_ret == 0)
      {
         g_console.emulator_initialized = 1;
         g_console.initialize_rarch_enable = 0;
         retval = true;
      }
      else
      {
         //failed to load the ROM for whatever reason
         g_console.emulator_initialized = 0;
         g_console.mode_switch = MODE_MENU;
         rarch_settings_msg(S_MSG_ROM_LOADING_ERROR, S_DELAY_180);
      }
   }

   return retval;
}


#endif

#ifdef HAVE_RARCH_EXEC

#ifdef __CELLOS_LV2__
#include <cell/sysmodule.h>
#include <sys/process.h>
#include <sysutil/sysutil_common.h>
#include <netex/net.h>
#include <np.h>
#include <np/drm.h>
#endif

void rarch_exec (void)
{
   if(g_console.return_to_launcher)
   {
      RARCH_LOG("Attempt to load executable: [%s].\n", g_console.launch_app_on_exit);
#if defined(_XBOX)
      XLaunchNewImage(g_console.launch_app_on_exit, NULL);
#elif defined(__CELLOS_LV2__)
      char spawn_data[256];
      for(unsigned int i = 0; i < sizeof(spawn_data); ++i)
         spawn_data[i] = i & 0xff;

      char spawn_data_size[16];
      snprintf(spawn_data_size, sizeof(spawn_data_size), "%d", 256);

      const char * const spawn_argv[] = {
         spawn_data_size,
         "test argv for",
         "sceNpDrmProcessExitSpawn2()",
         NULL
      };

      SceNpDrmKey * k_licensee = NULL;
      int ret = sceNpDrmProcessExitSpawn2(k_licensee, g_console.launch_app_on_exit, (const char** const)spawn_argv, NULL, (sys_addr_t)spawn_data, 256, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      if(ret <  0)
      {
         RARCH_WARN("SELF file is not of NPDRM type, trying another approach to boot it...\n");
	 sys_game_process_exitspawn(g_console.launch_app_on_exit, NULL, NULL, NULL, 0, 1000, SYS_PROCESS_PRIMARY_STACK_SIZE_1M);
      }
      sceNpTerm();
      cellSysmoduleUnloadModule(CELL_SYSMODULE_SYSUTIL_NP);
      cellSysmoduleUnloadModule(CELL_SYSMODULE_NET);
#endif
   }
}

#endif

#ifdef HAVE_RSOUND
bool rarch_console_rsound_start(const char *ip)
{
   strlcpy(g_settings.audio.driver, "rsound", sizeof(g_settings.audio.driver));
   strlcpy(g_settings.audio.device, ip, sizeof(g_settings.audio.device));
   driver.audio_data = NULL;

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
   return g_extern.audio_active;
}

void rarch_console_rsound_stop(void)
{
   strlcpy(g_settings.audio.driver, config_get_default_audio(), sizeof(g_settings.audio.driver));

   // If driver already has started, it must be reinited.
   if (driver.audio_data)
   {
      uninit_audio();
      driver.audio_data = NULL;
      init_drivers_pre();
      init_audio();
   }
}
#endif

/*============================================================
  STRING HANDLING
  ============================================================ */

void rarch_convert_char_to_wchar(wchar_t *buf, const char * str, size_t size)
{
   mbstowcs(buf, str, size / sizeof(wchar_t));
}

const char * rarch_convert_wchar_to_const_char(const wchar_t * wstr)
{
   static char str[256];
   wcstombs(str, wstr, sizeof(str));
   return str;
}

void rarch_extract_directory(char *buf, const char *path, size_t size)
{
   strncpy(buf, path, size - 1);
   buf[size - 1] = '\0';

   char *base = strrchr(buf, '/');
   if (!base)
      base = strrchr(buf, '\\');

   if (base)
      *base = '\0';
   else
      buf[0] = '\0';
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
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

#include <pthread.h>
#include <string.h>

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"

#include "apple_input.h"

#include "file.h"

//#define HAVE_DEBUG_FILELOG
static bool use_tv_mode;

id<RetroArch_Platform> apple_platform;

void apple_event_basic_command(void* userdata)
{
   switch ((enum basic_event_t)userdata)
   {
      case RESET:      rarch_game_reset(); return;
      case LOAD_STATE: rarch_load_state(); return;
      case SAVE_STATE: rarch_save_state(); return;
      case QUIT:       g_extern.system.shutdown = true; return;
   }
}

void apple_event_set_state_slot(void* userdata)
{
   g_extern.state_slot = (uint32_t)userdata;
}

void apple_event_show_rgui(void* userdata)
{
   const bool in_menu = g_extern.lifecycle_mode_state & (1 << MODE_MENU);
   g_extern.lifecycle_mode_state &= ~(1ULL << (in_menu ? MODE_MENU : MODE_GAME));
   g_extern.lifecycle_mode_state |=  (1ULL << (in_menu ? MODE_GAME : MODE_MENU));
}

static void event_reload_config(void* userdata)
{
   objc_clear_config_hack();

   uninit_drivers();
   config_load();
   init_drivers();
}

void apple_refresh_config()
{
   if (apple_is_running)
      apple_frontend_post_event(&event_reload_config, 0);
   else
      objc_clear_config_hack();
}

pthread_mutex_t stasis_mutex = PTHREAD_MUTEX_INITIALIZER;

static void event_stasis(void* userdata)
{
   // HACK: uninit_drivers is the nuclear option; uninit_audio would be better but will
   //       crash when resuming.
   uninit_drivers();
   pthread_mutex_lock(&stasis_mutex);
   pthread_mutex_unlock(&stasis_mutex);
   init_drivers();
}

void apple_enter_stasis()
{
   if (apple_is_running)
   {
      pthread_mutex_lock(&stasis_mutex);
      apple_frontend_post_event(event_stasis, 0);
   }
}

void apple_exit_stasis()
{
   if (apple_is_running)
      pthread_mutex_unlock(&stasis_mutex);
}

#pragma mark EMULATION
static pthread_t apple_retro_thread;
bool apple_is_paused;
bool apple_is_running;
bool apple_use_tv_mode;
RAModuleInfo* apple_core;

void* rarch_main_spring(void* args)
{
   char** argv = args;

   uint32_t argc = 0;
   while (argv && argv[argc]) argc++;
   
   if (rarch_main(argc, argv))
   {
      rarch_main_clear_state();
      dispatch_async_f(dispatch_get_main_queue(), (void*)1, apple_rarch_exited);
   }
   
   return 0;
}

void apple_run_core(RAModuleInfo* core, const char* file)
{
   if (!apple_is_running)
   {
      [apple_platform loadingCore:core withFile:file];

      apple_core = core;
      apple_is_running = true;
      
      static char config_path[PATH_MAX];
      static char core_path[PATH_MAX];
      static char file_path[PATH_MAX];
      
      static const char* argv[] = { "retroarch", "-c", config_path, "-L", core_path, file_path, 0 };

      if (apple_core)
         strlcpy(config_path, apple_core.configPath.UTF8String, sizeof(config_path));
      else
         strlcpy(config_path, RAModuleInfo.globalConfigPath.UTF8String, sizeof(config_path));
   
      if (file && core)
      {
         argv[3] = "-L";
         argv[4] = core_path;
         strlcpy(core_path, apple_core.path.UTF8String, sizeof(core_path));
         strlcpy(file_path, file, sizeof(file_path));
      }
      else
      {
         argv[3] = "--menu";
         argv[4] = 0;
      }
      
      if (pthread_create(&apple_retro_thread, 0, rarch_main_spring, argv))
      {
         apple_rarch_exited((void*)1);
         return;
      }
      
      pthread_detach(apple_retro_thread);
   }
}

void apple_rarch_exited(void* result)
{
   if (result)
      apple_display_alert(@"Failed to load game.", 0);

   RAModuleInfo* used_core = apple_core;
   apple_core = nil;

   if (apple_is_running)
   {
      apple_is_running = false;
      [apple_platform unloadingCore:used_core];
   }

   if (use_tv_mode)
      apple_run_core(nil, 0);
}

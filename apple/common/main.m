/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013 - Jason Fetters
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

char** apple_argv;

id<RetroArch_Platform> apple_platform;

#pragma mark EMULATION
static pthread_t apple_retro_thread;
bool apple_is_running;
bool apple_use_tv_mode;
NSString* apple_core;

void apple_run_core(NSString* core, const char* file)
{
   if (!apple_is_running)
   {
#ifndef OSX
      if (core && file)
      {
         char basedir[256];
         fill_pathname_basedir(basedir, file, sizeof(basedir));
         if (access(basedir, R_OK | W_OK | X_OK))
            apple_display_alert(@"The directory containing the selected file has limited permissions. This may "
                                 "prevent zipped content from loading, and will cause some cores to not function.", 0);
      }
#endif

      [apple_platform loadingCore:core withFile:file];

#ifdef OSX
      [apple_core release];
#endif
      apple_core = [core copy];
      apple_is_running = true;

      static char config_path[PATH_MAX];
      static char core_path[PATH_MAX];
      static char file_path[PATH_MAX];

      if (!apple_argv)
      {
         if (apple_core_info_has_custom_config(apple_core.UTF8String))
            apple_core_info_get_custom_config(apple_core.UTF8String, config_path, sizeof(config_path));
         else
            strlcpy(config_path, apple_platform.globalConfigFile.UTF8String, sizeof(config_path));

         static const char* const argv_game[] = { "retroarch", "-c", config_path, "-L", core_path, file_path, 0 };
         static const char* const argv_menu[] = { "retroarch", "-c", config_path, "--menu", 0 };
   
         if (file && core)
         {
            strlcpy(core_path, apple_core.UTF8String, sizeof(core_path));
            strlcpy(file_path, file, sizeof(file_path));
         }
         
         apple_argv = (char**)((file && core) ? argv_game : argv_menu);
      }
      
      if (pthread_create(&apple_retro_thread, 0, rarch_main_spring, apple_argv))
      {
         apple_argv = 0;      
      
         apple_rarch_exited((void*)1);
         return;
      }
      
      apple_argv = 0;
      
      pthread_detach(apple_retro_thread);
   }
}

void apple_rarch_exited(void* result)
{
   if (result)
      apple_display_alert(@"Failed to load content.", 0);

   NSString* used_core = apple_core;
   apple_core = 0;

   if (apple_is_running)
   {
      apple_is_running = false;
      [apple_platform unloadingCore:used_core];
   }

#ifdef OSX
   [used_core release];
#endif

   if (apple_use_tv_mode)
      apple_run_core(nil, 0);
}

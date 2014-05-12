/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include <string.h>

#import "RetroArch_Apple.h"
#include "rarch_wrapper.h"
#include "../../frontend/frontend.h"
#include "../../file.h"

id<RetroArch_Platform> apple_platform;

#pragma mark EMULATION
bool apple_is_running;
bool apple_use_tv_mode;
NSString* apple_core;

void apple_rarch_exited(void)
{
   NSString *used_core = (NSString*)apple_core;
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

void apple_run_core(NSString* core, const char* file)
{
   static char core_path[PATH_MAX], file_path[PATH_MAX], config_path[PATH_MAX];
   char **argv;
   int argc;
    
   (void)config_path;

   if (apple_is_running)
       return;
    
    [apple_platform loadingCore:core withFile:file];
    
    apple_core = core;
    apple_is_running = true;

   if (file && core)
   {
      strlcpy(core_path, apple_core.UTF8String, sizeof(core_path));
      strlcpy(file_path, file, sizeof(file_path));
   }
   
#ifdef IOS
    if (core_info_has_custom_config(apple_core.UTF8String))
        core_info_get_custom_config(apple_core.UTF8String, config_path, sizeof(config_path));
    else
        strlcpy(config_path, apple_platform.globalConfigFile.UTF8String, sizeof(config_path));
    
    static const char* const argv_game[] = { "retroarch", "-c", config_path, "-L", core_path, file_path, 0 };
    static const char* const argv_menu[] = { "retroarch", "-c", config_path, "--menu", 0 };
    
    argc = (file && core) ? 6 : 4;
#else
   static const char* const argv_game[] = { "retroarch", "-L", core_path, file_path, 0 };
   static const char* const argv_menu[] = { "retroarch", "--menu", 0 };

   argc = (file && core) ? 4 : 2;
#endif
    argv = (char**)((file && core) ? argv_game : argv_menu);

    if (apple_rarch_load_content(argc, argv))
    {
        char basedir[256];
        fill_pathname_basedir(basedir, file ? file : "", sizeof(basedir));
        if (file && access(basedir, R_OK | W_OK | X_OK))
            apple_display_alert("The directory containing the selected file must have write premissions. This will prevent zipped content from loading, and will cause some cores to not function.", "Warning");
        else
            apple_display_alert("Failed to load content.", "Error");
        
        apple_rarch_exited();
    }
}

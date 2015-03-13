/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2015 - Daniel De Matteis
 * Copyright (C) 2012-2014 - Jason Fetters
 * Copyright (C) 2014-2015 - Jay McCarthy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../apple/common/CFExtensions.h"

#include "../frontend.h"
#ifdef IOS
#include "../../menu/drivers/ios.h"
#endif

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <string.h>

static void frontend_apple_get_environment_settings(int *argc, char *argv[],
      void *args, void *params_data)
{
   char temp_dir[PATH_MAX_LENGTH];
   char bundle_path_buf[PATH_MAX_LENGTH], home_dir_buf[PATH_MAX_LENGTH];
   CFURLRef bundle_url;
   CFStringRef bundle_path;
   CFBundleRef bundle = CFBundleGetMainBundle();
    
   (void)temp_dir;

   if (!bundle)
      return;

   bundle_url  = CFBundleCopyBundleURL(bundle);
   bundle_path = CFURLCopyPath(bundle_url);
    
   CFStringGetCString(bundle_path, bundle_path_buf, sizeof(bundle_path_buf), kCFStringEncodingUTF8);
   (void)home_dir_buf;

   CFSearchPathForDirectoriesInDomains(CFDocumentDirectory, CFUserDomainMask, 1, home_dir_buf, sizeof(home_dir_buf));
    
#ifdef OSX
    strlcat(home_dir_buf, "/RetroArch", sizeof(home_dir_buf));
#endif
    
   fill_pathname_join(g_defaults.core_dir, home_dir_buf, "modules", sizeof(g_defaults.core_dir));
   fill_pathname_join(g_defaults.core_info_dir, home_dir_buf, "info", sizeof(g_defaults.core_info_dir));
   fill_pathname_join(g_defaults.overlay_dir, home_dir_buf, "overlays", sizeof(g_defaults.overlay_dir));
   fill_pathname_join(g_defaults.autoconfig_dir, home_dir_buf, "autoconfig/apple", sizeof(g_defaults.autoconfig_dir));
   fill_pathname_join(g_defaults.assets_dir, home_dir_buf, "assets", sizeof(g_defaults.assets_dir));
   fill_pathname_join(g_defaults.system_dir, home_dir_buf, ".RetroArch", sizeof(g_defaults.system_dir));
   strlcpy(g_defaults.menu_config_dir, g_defaults.system_dir, sizeof(g_defaults.menu_config_dir));
   fill_pathname_join(g_defaults.config_path, g_defaults.menu_config_dir, "retroarch.cfg", sizeof(g_defaults.config_path));
   fill_pathname_join(g_defaults.database_dir, home_dir_buf, "rdb", sizeof(g_defaults.database_dir));
   fill_pathname_join(g_defaults.cursor_dir, home_dir_buf, "cursors", sizeof(g_defaults.cursor_dir));
   fill_pathname_join(g_defaults.cheats_dir, home_dir_buf, "cht", sizeof(g_defaults.cheats_dir));
   strlcpy(g_defaults.sram_dir, g_defaults.system_dir, sizeof(g_defaults.sram_dir));
   strlcpy(g_defaults.savestate_dir, g_defaults.system_dir, sizeof(g_defaults.savestate_dir));
    
   CFTemporaryDirectory(temp_dir, sizeof(temp_dir));
   strlcpy(g_defaults.extraction_dir, temp_dir, sizeof(g_defaults.extraction_dir));
    
   fill_pathname_join(g_defaults.shader_dir, home_dir_buf, "shaders_glsl", sizeof(g_defaults.shader_dir));
    
#if defined(OSX)
   fill_pathname_join(g_defaults.shader_dir, home_dir_buf, "shaders_cg", sizeof(g_defaults.shader_dir));
    fill_pathname_join(g_defaults.audio_filter_dir, home_dir_buf, "audio_filters", sizeof(g_defaults.audio_filter_dir));
    fill_pathname_join(g_defaults.video_filter_dir, home_dir_buf, "video_filters", sizeof(g_defaults.video_filter_dir));
#endif
    
   path_mkdir(bundle_path_buf);
    
   if (access(bundle_path_buf, 0755) != 0)
      RARCH_ERR("Failed to create or access base directory: %s\n", bundle_path_buf);
    else
    {
        path_mkdir(g_defaults.system_dir);
        
        if (access(g_defaults.system_dir, 0755) != 0)
            RARCH_ERR("Failed to create or access system directory: %s.\n", g_defaults.system_dir);
    }

   CFRelease(bundle_path);
   CFRelease(bundle_url);
}

extern void apple_rarch_exited(void);

static void frontend_apple_load_content(void)
{
    if (!driver.menu_ctx)
        return;
    if (!driver.menu)
        return;
    if (!driver.menu->userdata)
        return;
    
#ifdef IOS
   if (driver.menu_ctx == &menu_ctx_ios)
   {
      ios_handle_t *ih = (ios_handle_t*)driver.menu->userdata;
      if (ih)
         ih->notify_content_loaded();
   }
#endif
}

static void frontend_apple_shutdown(bool unused)
{
    apple_rarch_exited();
}

static int frontend_apple_get_rating(void)
{
   /* TODO/FIXME - look at unique identifier per device and 
    * determine rating for some */
   return -1;
}
const frontend_ctx_driver_t frontend_ctx_apple = {
   frontend_apple_get_environment_settings, /* environment_get */
   NULL,                         /* init */
   NULL,                         /* deinit */
   NULL,                         /* exitspawn */
   NULL,                         /* process_args */
   NULL,                         /* exec */
   NULL,                         /* set_fork */
   frontend_apple_shutdown,      /* shutdown */
   NULL,                         /* get_name */
   frontend_apple_get_rating,    /* get_rating */
   frontend_apple_load_content,  /* load_content */
   "apple",
};

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

#include "../frontend_driver.h"
#include "../../ui/ui_companion_driver.h"

#include <stdint.h>
#include <boolean.h>
#include <stddef.h>
#include <string.h>

#ifdef IOS
void get_ios_version(int *major, int *minor);
#endif

static bool CopyModel(char** model, uint32_t *majorRev, uint32_t *minorRev)
{
#ifdef OSX
   int mib[2];
   int count;
   unsigned long modelLen;
   char *revStr;
#endif
   char *machineModel;
   bool success  = true;
   size_t length = 1024;

   if (!model || !majorRev || !minorRev)
   {
      RARCH_ERR("CopyModel: Passing NULL arguments\n");
      return false;
   }

#ifdef IOS
   sysctlbyname("hw.machine", NULL, &length, NULL, 0);
#endif

   machineModel  = malloc(length);

   if (!machineModel)
   {
      success   = false;
      goto exit;
   }
#ifdef IOS
   sysctlbyname("hw.machine", machineModel, &length, NULL, 0);
   *model        = strndup(machineModel, length);
#else
   mib[0]        = CTL_HW;
   mib[1]        = HW_MODEL;

   if (sysctl(mib, 2, machineModel, &length, NULL, 0))
   {
      printf("CopyModel: sysctl (error %d)\n", errno);
      success  = false;
      goto exit;
   }

   modelLen      = strcspn(machineModel, "0123456789");

   if (modelLen  == 0)
   {
      RARCH_ERR("CopyModel: Could not find machine model name\n");
      success   = false;
      goto exit;
   }

   *model        = strndup(machineModel, modelLen);

   if (*model    == NULL)
   {
      RARCH_ERR("CopyModel: Could not find machine model name\n");
      success   = false;
      goto exit;
   }

   *majorRev     = 0;
   *minorRev     = 0;
   revStr        = strpbrk(machineModel, "0123456789");

   if (!revStr)
   {
      RARCH_ERR("CopyModel: Could not find machine version number, inferred value is 0,0\n");
      success   = true;
      goto exit;
   }

   count         = sscanf(revStr, "%d,%d", majorRev, minorRev);

   if (count < 2)
   {
      RARCH_ERR("CopyModel: Could not find machine version number\n");
      if (count < 1)
         *majorRev = 0;
      *minorRev     = 0;
      success       = true;
      goto exit;
   }
#endif

exit:
   if (machineModel)
      free(machineModel);
   if (!success)
   {
      if (*model)
         free(*model);
      *model    = NULL;
      *majorRev = 0;
      *minorRev = 0;
   }
   return success;
}

static void frontend_apple_get_name(char *name, size_t sizeof_name)
{
    uint32_t major_rev, minor_rev;
    CopyModel(&name, &major_rev, &minor_rev);
}

static void frontend_apple_get_os(char *name, size_t sizeof_name, int *major, int *minor)
{
   (void)name;
   (void)sizeof_name;
   (void)major;
   (void)minor;
    
#ifdef IOS
    get_ios_version(major, minor);
    snprintf(name, sizeof_name, "iOS %d.%d", *major, *minor);
#endif
}

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
   fill_pathname_join(g_defaults.autoconfig_dir, home_dir_buf, "autoconfig/hid", sizeof(g_defaults.autoconfig_dir));
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
#ifdef HAVE_CG
   fill_pathname_join(g_defaults.shader_dir, home_dir_buf, "shaders_cg", sizeof(g_defaults.shader_dir));
#endif
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
   driver_t          *driver = driver_get_ptr();
   const ui_companion_driver_t *ui = ui_companion_get_ptr();
    
   if (ui && ui->notify_content_loaded)
      ui->notify_content_loaded(driver->ui_companion_data);
}

static void frontend_apple_shutdown(bool unused)
{
    apple_rarch_exited();
}

static int frontend_apple_get_rating(void)
{
   char model[PATH_MAX_LENGTH];

   frontend_apple_get_name(model, sizeof(model));

   /* iPhone 4 */
#if 0
   if (strstr(model, "iPhone3"))
      return -1;
#endif

   /* iPad 1 */
#if 0
   if (strstr(model, "iPad1,1"))
      return -1;
#endif

   /* iPhone 4S */
   if (strstr(model, "iPhone4,1"))
      return 8;

   /* iPad 2/iPad Mini 1 */
   if (strstr(model, "iPad2"))
      return 9;

   /* iPhone 5/5C */
   if (strstr(model, "iPhone5"))
      return 13;

   /* iPhone 5S */
   if (strstr(model, "iPhone6,1") || strstr(model, "iPhone6,2"))
      return 14;

   /* iPad Mini 2/3 */
   if (     strstr(model, "iPad4,4")
         || strstr(model, "iPad4,5")
         || strstr(model, "iPad4,6")
         || strstr(model, "iPad4,7")
         || strstr(model, "iPad4,8")
         || strstr(model, "iPad4,9")
         )
      return 15;

   /* iPad Air */
   if (     strstr(model, "iPad4,1")
         || strstr(model, "iPad4,2")
         || strstr(model, "iPad4,3")
      )
      return 16;

   /* iPhone 6, iPhone 6 Plus */
   if (strstr(model, "iPhone7"))
      return 17;

   /* iPad Air 2 */
   if (strstr(model, "iPad5,3") || strstr(model, "iPad5,4"))
      return 18;

   /* TODO/FIXME - 
      - more ratings for more systems
      - determine rating more intelligently*/
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
   frontend_apple_get_name,      /* get_name */
   frontend_apple_get_os,        /* get_os */
   frontend_apple_get_rating,    /* get_rating */
   frontend_apple_load_content,  /* load_content */
   "apple",
};

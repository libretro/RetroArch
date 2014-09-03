#ifndef _DISP_SHARED_H
#define _DISP_SHARED_H

static void get_title(const char *label, const char *dir,
      unsigned menu_type, unsigned menu_type_is,
      char *title, size_t sizeof_title)
{
   if (!strcmp(label, "core_list"))
      snprintf(title, sizeof_title, "CORE SELECTION %s", dir);
   else if (menu_type == MENU_SETTINGS_DEFERRED_CORE)
      snprintf(title, sizeof_title, "DETECTED CORES %s", dir);
   else if (menu_type == MENU_SETTINGS_CONFIG)
      snprintf(title, sizeof_title, "CONFIG %s", dir);
   else if (menu_type == MENU_SETTINGS_DISK_APPEND)
      snprintf(title, sizeof_title, "DISK APPEND %s", dir);
   else if (menu_type == MENU_SETTINGS_VIDEO_OPTIONS)
      strlcpy(title, "VIDEO OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_INPUT_OPTIONS ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD)
      strlcpy(title, "INPUT OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_OVERLAY_OPTIONS)
      strlcpy(title, "OVERLAY OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_NETPLAY_OPTIONS)
      strlcpy(title, "NETPLAY OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_USER_OPTIONS)
      strlcpy(title, "USER OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_PATH_OPTIONS)
      strlcpy(title, "PATH OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_OPTIONS)
      strlcpy(title, "SETTINGS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_DRIVERS)
      strlcpy(title, "DRIVER OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS)
      strlcpy(title, "PERFORMANCE COUNTERS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS_LIBRETRO)
      strlcpy(title, "CORE PERFORMANCE COUNTERS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_PERFORMANCE_COUNTERS_FRONTEND)
      strlcpy(title, "FRONTEND PERFORMANCE COUNTERS", sizeof_title);
#ifdef HAVE_SHADER_MANAGER
   else if (menu_type == MENU_SETTINGS_SHADER_OPTIONS)
      strlcpy(title, "SHADER OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_SHADER_PARAMETERS)
      strlcpy(title, "SHADER PARAMETERS (CURRENT)", sizeof_title);
   else if (menu_type == MENU_SETTINGS_SHADER_PRESET_PARAMETERS)
      strlcpy(title, "SHADER PARAMETERS (MENU PRESET)", sizeof_title);
#endif
   else if (menu_type == MENU_SETTINGS_FONT_OPTIONS)
      strlcpy(title, "FONT OPTIONS", sizeof_title);
   else if (!strcmp(dir, "General Options"))
      strlcpy(title, "GENERAL OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_AUDIO_OPTIONS)
      strlcpy(title, "AUDIO OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_DISK_OPTIONS)
      strlcpy(title, "DISK OPTIONS", sizeof_title);
   else if (menu_type == MENU_SETTINGS_CORE_OPTIONS)
      strlcpy(title, "CORE OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_information"))
      strlcpy(title, "CORE INFO", sizeof_title);
   else if (menu_type == MENU_SETTINGS_PRIVACY_OPTIONS)
      strlcpy(title, "PRIVACY OPTIONS", sizeof_title);
#ifdef HAVE_SHADER_MANAGER
   else if (!strcmp(label, "video_shader_pass"))
      snprintf(title, sizeof_title, "SHADER %s", dir);
   else if (!strcmp(label, "video_shader_preset"))
      snprintf(title, sizeof_title, "SHADER PRESET %s", dir);
#endif
   else if (menu_type == MENU_SETTINGS_PATH_OPTIONS ||
         menu_type == MENU_SETTINGS_OPTIONS ||
         menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT ||
         !strcmp(label, "custom_viewport_2") ||
         !strcmp(label, "help") ||
         menu_type == MENU_SETTINGS)
      snprintf(title, sizeof_title, "MENU %s", dir);
   else if (!strcmp(label, "history_list"))
      strlcpy(title, "LOAD HISTORY", sizeof_title);
   else if (!strcmp(label, "info_screen"))
      strlcpy(title, "INFO", sizeof_title);
   else if (menu_type == MENU_SETTINGS_OVERLAY_PRESET)
      snprintf(title, sizeof_title, "OVERLAY %s", dir);
   else if (!strcmp(label, "video_filter"))
      snprintf(title, sizeof_title, "FILTER %s", dir);
   else if (!strcmp(label, "audio_dsp_plugin"))
      snprintf(title, sizeof_title, "DSP FILTER %s", dir);
   else if (!strcmp(label, "rgui_browser_directory"))
      snprintf(title, sizeof_title, "BROWSER DIR %s", dir);
   else if (menu_type == MENU_CONTENT_DIR_PATH)
      snprintf(title, sizeof_title, "CONTENT DIR %s", dir);
   else if (menu_type == MENU_SCREENSHOT_DIR_PATH)
      snprintf(title, sizeof_title, "SCREENSHOT DIR %s", dir);
   else if (menu_type == MENU_AUTOCONFIG_DIR_PATH)
      snprintf(title, sizeof_title, "AUTOCONFIG DIR %s", dir);
   else if (menu_type == MENU_SHADER_DIR_PATH)
      snprintf(title, sizeof_title, "SHADER DIR %s", dir);
   else if (menu_type == MENU_FILTER_DIR_PATH)
      snprintf(title, sizeof_title, "FILTER DIR %s", dir);
   else if (menu_type == MENU_DSP_FILTER_DIR_PATH)
      snprintf(title, sizeof_title, "DSP FILTER DIR %s", dir);
   else if (menu_type == MENU_SAVESTATE_DIR_PATH)
      snprintf(title, sizeof_title, "SAVESTATE DIR %s", dir);
#ifdef HAVE_DYNAMIC
   else if (menu_type == MENU_LIBRETRO_DIR_PATH)
      snprintf(title, sizeof_title, "LIBRETRO DIR %s", dir);
#endif
   else if (menu_type == MENU_CONFIG_DIR_PATH)
      snprintf(title, sizeof_title, "CONFIG DIR %s", dir);
   else if (menu_type == MENU_SAVEFILE_DIR_PATH)
      snprintf(title, sizeof_title, "SAVEFILE DIR %s", dir);
   else if (menu_type == MENU_OVERLAY_DIR_PATH)
      snprintf(title, sizeof_title, "OVERLAY DIR %s", dir);
   else if (menu_type == MENU_SYSTEM_DIR_PATH)
      snprintf(title, sizeof_title, "SYSTEM DIR %s", dir);
   else if (menu_type == MENU_ASSETS_DIR_PATH)
      snprintf(title, sizeof_title, "ASSETS DIR %s", dir);
   else
   {
      if (driver.menu->defer_core)
         snprintf(title, sizeof_title, "CONTENT %s", dir);
      else
      {
         const char *core_name = driver.menu->info.library_name;
         if (!core_name)
            core_name = g_extern.system.info.library_name;
         if (!core_name)
            core_name = "No Core";
         snprintf(title, sizeof_title, "CONTENT (%s) %s", core_name, dir);
      }
   }
}

#endif

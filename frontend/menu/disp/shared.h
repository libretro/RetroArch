#ifndef _DISP_SHARED_H
#define _DISP_SHARED_H

static void get_title(const char *label, const char *dir,
      unsigned menu_type, char *title, size_t sizeof_title)
{
   if (!strcmp(label, "core_list"))
      snprintf(title, sizeof_title, "CORE SELECTION %s", dir);
   else if (!strcmp(label, "deferred_core_list"))
      snprintf(title, sizeof_title, "DETECTED CORES %s", dir);
   else if (!strcmp(label, "configurations"))
      snprintf(title, sizeof_title, "CONFIG %s", dir);
   else if (!strcmp(label, "disk_image_append"))
      snprintf(title, sizeof_title, "DISK APPEND %s", dir);
   else if (!strcmp(label, "Video Options"))
      strlcpy(title, "VIDEO OPTIONS", sizeof_title);
   else if (!strcmp(label, "Input Options") ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD)
      strlcpy(title, "INPUT OPTIONS", sizeof_title);
   else if (!strcmp(label, "Overlay Options"))
      strlcpy(title, "OVERLAY OPTIONS", sizeof_title);
   else if (!strcmp(label, "Playlist Options"))
      strlcpy(title, "PLAYLIST OPTIONS", sizeof_title);
   else if (!strcmp(label, "Netplay Options"))
      strlcpy(title, "NETPLAY OPTIONS", sizeof_title);
   else if (!strcmp(label, "User Options"))
      strlcpy(title, "USER OPTIONS", sizeof_title);
   else if (!strcmp(label, "Path Options"))
      strlcpy(title, "PATH OPTIONS", sizeof_title);
   else if (!strcmp(label, "settings"))
      strlcpy(title, "SETTINGS", sizeof_title);
   else if (!strcmp(label, "Driver Options"))
      strlcpy(title, "DRIVER OPTIONS", sizeof_title);
   else if (!strcmp(label, "performance_counters"))
      strlcpy(title, "PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(label, "frontend_counters"))
      strlcpy(title, "FRONTEND PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(label, "core_counters"))
      strlcpy(title, "CORE PERFORMANCE COUNTERS", sizeof_title);
   else if (!strcmp(label, "Shader Options"))
      strlcpy(title, "SHADER OPTIONS", sizeof_title);
   else if (!strcmp(label, "video_shader_parameters"))
      strlcpy(title, "SHADER PARAMETERS (CURRENT)", sizeof_title);
   else if (!strcmp(label, "video_shader_preset_parameters"))
      strlcpy(title, "SHADER PARAMETERS (MENU PRESET)", sizeof_title);
   else if (!strcmp(label, "Font Options"))
      strlcpy(title, "FONT OPTIONS", sizeof_title);
   else if (!strcmp(label, "General Options"))
      strlcpy(title, "GENERAL OPTIONS", sizeof_title);
   else if (!strcmp(label, "Audio Options"))
      strlcpy(title, "AUDIO OPTIONS", sizeof_title);
   else if (!strcmp(label, "disk_options"))
      strlcpy(title, "DISK OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_options"))
      strlcpy(title, "CORE OPTIONS", sizeof_title);
   else if (!strcmp(label, "core_information"))
      strlcpy(title, "CORE INFO", sizeof_title);
   else if (!strcmp(label, "Privacy Options"))
      strlcpy(title, "PRIVACY OPTIONS", sizeof_title);
   else if (!strcmp(label, "video_shader_pass"))
      snprintf(title, sizeof_title, "SHADER %s", dir);
   else if (!strcmp(label, "video_shader_preset"))
      snprintf(title, sizeof_title, "SHADER PRESET %s", dir);
   else if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT ||
         !strcmp(label, "custom_viewport_2") ||
         !strcmp(label, "help") ||
         menu_type == MENU_SETTINGS)
      snprintf(title, sizeof_title, "MENU %s", dir);
   else if (!strcmp(label, "history_list"))
      strlcpy(title, "LOAD HISTORY", sizeof_title);
   else if (!strcmp(label, "info_screen"))
      strlcpy(title, "INFO", sizeof_title);
   else if (!strcmp(label, "input_overlay"))
      snprintf(title, sizeof_title, "OVERLAY %s", dir);
   else if (!strcmp(label, "video_font_path"))
      snprintf(title, sizeof_title, "FONT %s", dir);
   else if (!strcmp(label, "video_filter"))
      snprintf(title, sizeof_title, "FILTER %s", dir);
   else if (!strcmp(label, "audio_dsp_plugin"))
      snprintf(title, sizeof_title, "DSP FILTER %s", dir);
   else if (!strcmp(label, "rgui_browser_directory"))
      snprintf(title, sizeof_title, "BROWSER DIR %s", dir);
   else if (!strcmp(label, "playlist_directory"))
      snprintf(title, sizeof_title, "PLAYLIST DIR %s", dir);
   else if (!strcmp(label, "content_directory"))
      snprintf(title, sizeof_title, "CONTENT DIR %s", dir);
   else if (!strcmp(label, "screenshot_directory"))
      snprintf(title, sizeof_title, "SCREENSHOT DIR %s", dir);
   else if (!strcmp(label, "video_shader_dir"))
      snprintf(title, sizeof_title, "SHADER DIR %s", dir);
   else if (!strcmp(label, "video_filter_dir"))
      snprintf(title, sizeof_title, "FILTER DIR %s", dir);
   else if (!strcmp(label, "audio_filter_dir"))
      snprintf(title, sizeof_title, "DSP FILTER DIR %s", dir);
   else if (!strcmp(label, "savestate_directory"))
      snprintf(title, sizeof_title, "SAVESTATE DIR %s", dir);
   else if (!strcmp(label, "libretro_dir_path"))
      snprintf(title, sizeof_title, "LIBRETRO DIR %s", dir);
   else if (!strcmp(label, "libretro_info_path"))
      snprintf(title, sizeof_title, "LIBRETRO INFO DIR %s", dir);
   else if (!strcmp(label, "rgui_config_directory"))
      snprintf(title, sizeof_title, "CONFIG DIR %s", dir);
   else if (!strcmp(label, "savefile_directory"))
      snprintf(title, sizeof_title, "SAVEFILE DIR %s", dir);
   else if (!strcmp(label, "overlay_directory"))
      snprintf(title, sizeof_title, "OVERLAY DIR %s", dir);
   else if (!strcmp(label, "system_directory"))
      snprintf(title, sizeof_title, "SYSTEM DIR %s", dir);
   else if (!strcmp(label, "assets_directory"))
      snprintf(title, sizeof_title, "ASSETS DIR %s", dir);
   else if (!strcmp(label, "extraction_directory"))
      snprintf(title, sizeof_title, "EXTRACTION DIR %s", dir);
   else if (!strcmp(label, "joypad_autoconfig_dir"))
      snprintf(title, sizeof_title, "AUTOCONFIG DIR %s", dir);
   else
   {
      if (driver.menu->defer_core)
         snprintf(title, sizeof_title, "CONTENT %s", dir);
      else
      {
         const char *core_name = g_extern.menu.info.library_name;
         if (!core_name)
            core_name = g_extern.system.info.library_name;
         if (!core_name)
            core_name = "No Core";
         snprintf(title, sizeof_title, "CONTENT (%s) %s", core_name, dir);
      }
   }
}

static void disp_set_label(unsigned *w, unsigned type, unsigned i,
      const char *label,
      char *type_str, size_t type_str_size,
      const char *entry_label,
      const char *path,
      char *path_buf, size_t path_buf_size)
{
   *type_str = '\0';
   *w = 19;

   if (!strcmp(label, "performance_counters"))
      *w = 28;

   if (!strcmp(label, "history_list"))
      *w = 6;

   if (type == MENU_FILE_CORE)
   {
      strlcpy(type_str, "(CORE)", type_str_size);
      file_list_get_alt_at_offset(driver.menu->selection_buf, i, &path);
      *w = 6;
   }
   else if (type == MENU_FILE_PLAIN)
   {
      strlcpy(type_str, "(FILE)", type_str_size);
      *w = 6;
   }
   else if (type == MENU_FILE_USE_DIRECTORY)
   {
      *type_str = '\0';
      *w = 0;
   }
   else if (type == MENU_FILE_DIRECTORY)
   {
      strlcpy(type_str, "(DIR)", type_str_size);
      *w = 5;
   }
   else if (type == MENU_FILE_CARCHIVE)
   {
      strlcpy(type_str, "(COMP)", type_str_size);
      *w = 6;
   }
   else if (type == MENU_FILE_IN_CARCHIVE)
   {
      strlcpy(type_str, "(CFILE)", type_str_size);
      *w = 7;
   }
   else if (type == MENU_FILE_FONT)
   {
      strlcpy(type_str, "(FONT)", type_str_size);
      *w = 7;
   }
   else if (type == MENU_FILE_SHADER_PRESET)
   {
      strlcpy(type_str, "(PRESET)", type_str_size);
      *w = 8;
   }
   else if (type == MENU_FILE_SHADER)
   {
      strlcpy(type_str, "(SHADER)", type_str_size);
      *w = 8;
   }
   else if (
         type == MENU_FILE_VIDEOFILTER ||
         type == MENU_FILE_AUDIOFILTER)
   {
      strlcpy(type_str, "(FILTER)", type_str_size);
      *w = 8;
   }
   else if (type == MENU_FILE_CONFIG)
   {
      strlcpy(type_str, "(CONFIG)", type_str_size);
      *w = 8;
   }
   else if (type == MENU_FILE_OVERLAY)
   {
      strlcpy(type_str, "(OVERLAY)", type_str_size);
      *w = 9;
   }
   else if (type >= MENU_SETTINGS_CORE_OPTION_START)
      strlcpy(
            type_str,
            core_option_get_val(g_extern.system.core_options,
               type - MENU_SETTINGS_CORE_OPTION_START),
            type_str_size);
   else if (type == MENU_FILE_PUSH || type == MENU_FILE_LINEFEED_SWITCH)
      strlcpy(type_str, "...", type_str_size);
   else if (driver.menu_ctx && driver.menu_ctx->backend &&
         driver.menu_ctx->backend->setting_set_label)
      driver.menu_ctx->backend->setting_set_label(type_str,
            type_str_size, w, type, label, entry_label, i);

   strlcpy(path_buf, path, path_buf_size);
}

#endif

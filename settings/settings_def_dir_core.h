/* Single-source definitions: core directories group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "system_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_system, SYSTEM_DIRECTORY,
      "system_directory",
      g_defaults.dirs[DEFAULT_DIR_SYSTEM], DIRECTORY_CONTENT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "System/BIOS",
      "BIOSes, boot ROMs, and other system specific files are stored in this directory.")
#endif
/* config key "core_assets_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_core_assets, CORE_ASSETS_DIRECTORY,
      "core_assets_directory",
      g_defaults.dirs[DEFAULT_DIR_CORE_ASSETS], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Downloads",
      "Downloaded files are stored in this directory.")
#endif
/* config key "assets_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_assets, ASSETS_DIRECTORY,
      "assets_directory",
      g_defaults.dirs[DEFAULT_DIR_ASSETS], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Assets",
      "Menu assets used by RetroArch are stored in this directory.")
#endif
/* config key "dynamic_wallpapers_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_dynamic_wallpapers, DYNAMIC_WALLPAPERS_DIRECTORY,
      "dynamic_wallpapers_directory",
      g_defaults.dirs[DEFAULT_DIR_WALLPAPERS], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Dynamic Backgrounds",
      "Background images used within the menu are stored in this directory.")
#endif
/* config key "thumbnails_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_thumbnails, THUMBNAILS_DIRECTORY,
      "thumbnails_directory",
      g_defaults.dirs[DEFAULT_DIR_THUMBNAILS], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Thumbnails",
      "Box art, screenshot, and title screen thumbnails are stored in this directory.")
#endif
/* config key "rgui_browser_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
/* FIXME Not RGUI specific */ /* FIXME Not RGUI specific */
S_DIR(directory_menu_content, RGUI_BROWSER_DIRECTORY,
      "rgui_browser_directory",
      g_defaults.dirs[DEFAULT_DIR_MENU_CONTENT], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Start Directory",
      "Set Start Directory for File Browser.")
#endif
/* config key "rgui_config_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
/* FIXME Not RGUI specific */ /* FIXME Not RGUI specific */
S_DIR(directory_menu_config, RGUI_CONFIG_DIRECTORY,
      "rgui_config_directory",
      g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Configuration files",
      "Default configuration file is stored in this directory.")
#endif
/* config key "libretro_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_libretro, LIBRETRO_DIR_PATH,
      "libretro_dir_path",
      g_defaults.dirs[DEFAULT_DIR_CORE], DIRECTORY_NONE, SD_FLAG_NONE, CMD_EVENT_CORE_INFO_INIT, directory_action_start_generic,
      "Cores",
      "Libretro cores are stored in this directory.")
#endif
/* config key "libretro_info_path" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(path_libretro_info, LIBRETRO_INFO_PATH,
      "libretro_info_path",
      g_defaults.dirs[DEFAULT_DIR_CORE_INFO], DIRECTORY_NONE, SD_FLAG_NONE, CMD_EVENT_CORE_INFO_INIT, directory_action_start_generic,
      "Core Info",
      "Application/core information files are stored in this directory.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_LIBRETRODB; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LIBRETRODB) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "content_database_path" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(path_content_database, CONTENT_DATABASE_DIRECTORY,
      "content_database_path",
      g_defaults.dirs[DEFAULT_DIR_DATABASE], DIRECTORY_NONE, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Databases",
      "Databases are stored in this directory.")
#endif
#endif
/* config key "cheat_database_path" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(path_cheat_database, CHEAT_DATABASE_PATH,
      "cheat_database_path",
      g_defaults.dirs[DEFAULT_DIR_CHEATS], DIRECTORY_NONE, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Cheat Files",
      "Cheat files are stored in this directory.")
#endif
/* config key "video_filter_dir" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_video_filter, VIDEO_FILTER_DIR,
      "video_filter_dir",
      g_defaults.dirs[DEFAULT_DIR_VIDEO_FILTER], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Video Filters",
      "CPU-based video filters are stored in this directory.")
#endif
/* config key "audio_filter_dir" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_audio_filter, AUDIO_FILTER_DIR,
      "audio_filter_dir",
      g_defaults.dirs[DEFAULT_DIR_AUDIO_FILTER], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Audio Filters",
      "Audio DSP filters are stored in this directory.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "video_shader_dir" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_video_shader, VIDEO_SHADER_DIR,
      "video_shader_dir",
      g_defaults.dirs[DEFAULT_DIR_SHADER], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Video Shaders",
      "GPU-based video shaders are stored in this directory.")
#endif
#endif

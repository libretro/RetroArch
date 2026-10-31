/* Single-source definitions: custom Vulkan driver path setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor rows are #ifdef HAVE_ADRENOTOOLS; the string tables
 * always carry this row via the strings pass. */
#if defined(HAVE_ADRENOTOOLS) || defined(SETTINGS_DEF_STRINGS_PASS)
/* configuration.c registers "video_vulkan_driver_path" with a literal
 * row under HAVE_ADRENOTOOLS. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS(path_vulkan_driver, VIDEO_VULKAN_DRIVER_PATH,
      "video_vulkan_driver_path",
      directory_system, SD_FLAG_NONE, 0, "zip", setting_get_string_representation_vulkan_driver, 0,
      "Custom Vulkan Driver",
      "Use a Vulkan driver package (.zip) instead of the system driver. The driver runs inside RetroArch: only use packages from a source you trust. Takes effect the next time Vulkan starts. Start: system driver.")
#endif
#endif

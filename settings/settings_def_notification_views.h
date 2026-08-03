/* Single-source definitions: notification view group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL_EX(video_fps_show, FPS_SHOW,
      "fps_show",
      DEFAULT_FPS_SHOW, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Display Framerate",
      "Display the current frames per second.")
S_UINT_EX(fps_update_interval, FPS_UPDATE_INTERVAL,
      "fps_update_interval",
      DEFAULT_FPS_UPDATE_INTERVAL, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 512, 1, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Framerate Update Interval (In Frames)",
      "Framerate display will be updated at the set interval in frames.")
S_BOOL_EX(video_memory_show, MEMORY_SHOW,
      "memory_show",
      DEFAULT_MEMORY_SHOW, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Display Memory Usage",
      "Display the used and total amount of memory on the system.")
S_UINT_EX(memory_update_interval, MEMORY_UPDATE_INTERVAL,
      "memory_update_interval",
      DEFAULT_MEMORY_UPDATE_INTERVAL, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 512, 1, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Memory Usage Update Interval (In Frames)",
      "Memory usage display will be updated at the set interval in frames.")
S_UINT_EX(video_time_show, TIME_SHOW,
      "time_show",
      DEFAULT_TIME_SHOW, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, TIME_SHOW_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_time_show, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Display Time",
      "Display the current time in the preferred format.")
S_BOOL(video_statistics_show, STATISTICS_SHOW,
      "statistics_show",
      DEFAULT_STATISTICS_SHOW, SD_FLAG_NONE, 0, 0,
      "Display Statistics",
      "Display on-screen technical statistics.")
S_BOOL(video_framecount_show, FRAMECOUNT_SHOW,
      "framecount_show",
      DEFAULT_FRAMECOUNT_SHOW, SD_FLAG_NONE, 0, 0,
      "Display Frame Count",
      "Display the current frame count on-screen.")
/* Descriptor and configuration rows are #ifdef HAVE_GFX_WIDGETS #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_GFX_WIDGETS) && defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL_EX(netplay_ping_show, NETPLAY_PING_SHOW,
      "netplay_ping_show",
      DEFAULT_NETPLAY_PING_SHOW, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Display Netplay Ping",
      "Display the ping for the current netplay room.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_GFX_WIDGETS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_GFX_WIDGETS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(menu_show_load_content_animation, MENU_SHOW_LOAD_CONTENT_ANIMATION,
      "menu_show_load_content_animation",
      DEFAULT_MENU_SHOW_LOAD_CONTENT_ANIMATION, SD_FLAG_NONE, 0, 0,
      "\"Load Content\" Startup Notification",
      "Show a brief launch feedback animation when loading content.")
#endif
S_BOOL(notification_show_autoconfig, NOTIFICATION_SHOW_AUTOCONFIG,
      "notification_show_autoconfig",
      DEFAULT_NOTIFICATION_SHOW_AUTOCONFIG, SD_FLAG_NONE, 0, 0,
      "Input (Autoconfig) Connection Notifications",
      "Display an on-screen message when connecting/disconnecting input devices.")
S_BOOL(notification_show_autoconfig_fails, NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
      "notification_show_autoconfig_fails",
      DEFAULT_NOTIFICATION_SHOW_AUTOCONFIG_FAILS, SD_FLAG_NONE, 0, 0,
      "Input (Autoconfig) Failure Notifications",
      "Display an on-screen message when input devices could not be configured.")
/* Descriptor and configuration rows are #ifdef HAVE_CHEATS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEATS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(notification_show_cheats_applied, NOTIFICATION_SHOW_CHEATS_APPLIED,
      "notification_show_cheats_applied",
      DEFAULT_NOTIFICATION_SHOW_CHEATS_APPLIED, SD_FLAG_NONE, 0, 0,
      "Cheat Code Notifications",
      "Display an on-screen message when cheat codes are applied.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_PATCH; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_PATCH) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(notification_show_patch_applied, NOTIFICATION_SHOW_PATCH_APPLIED,
      "notification_show_patch_applied",
      DEFAULT_NOTIFICATION_SHOW_PATCH_APPLIED, SD_FLAG_NONE, 0, 0,
      "Patch Notifications",
      "Display an on-screen message when soft-patching ROMs.")
#endif
S_BOOL(notification_show_remap_load, NOTIFICATION_SHOW_REMAP_LOAD,
      "notification_show_remap_load",
      DEFAULT_NOTIFICATION_SHOW_REMAP_LOAD, SD_FLAG_NONE, 0, 0,
      "Input Remap Loaded Notifications",
      "Display an on-screen message when loading input remap files.")
S_BOOL(notification_show_config_override_load, NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
      "notification_show_config_override_load",
      DEFAULT_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD, SD_FLAG_NONE, 0, 0,
      "Config Override Loaded Notifications",
      "Display an on-screen message when loading configuration override files.")
S_BOOL(notification_show_set_initial_disk, NOTIFICATION_SHOW_SET_INITIAL_DISK,
      "notification_show_set_initial_disk",
      DEFAULT_NOTIFICATION_SHOW_SET_INITIAL_DISK, SD_FLAG_NONE, 0, 0,
      "Initial Disc Restored Notifications",
      "Display an on-screen message when automatically restoring at launch the last used disc of multi-disc content loaded via M3U playlists.")
S_BOOL(notification_show_disk_control, NOTIFICATION_SHOW_DISK_CONTROL,
      "notification_show_disk_control",
      DEFAULT_NOTIFICATION_SHOW_DISK_CONTROL, SD_FLAG_NONE, 0, 0,
      "Disc Control Notifications",
      "Display an on-screen message when inserting and ejecting discs.")
S_BOOL(notification_show_save_state, NOTIFICATION_SHOW_SAVE_STATE,
      "notification_show_save_state",
      DEFAULT_NOTIFICATION_SHOW_SAVE_STATE, SD_FLAG_NONE, 0, 0,
      "Save State Notifications",
      "Display an on-screen message when saving and loading save states.")
/* FIXME: Rename config key and msg hash */
S_BOOL(notification_show_fast_forward, NOTIFICATION_SHOW_FAST_FORWARD,
      "notification_show_fast_forward",
      DEFAULT_NOTIFICATION_SHOW_FAST_FORWARD, SD_FLAG_NONE, 0, 0,
      "Frame Throttle Notifications",
      "Display an on-screen indicator when fast-forward, slow-motion or rewind is active.")
/* Descriptor and configuration rows are #ifdef HAVE_SCREENSHOTS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_SCREENSHOTS) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_SCREENSHOTS); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_SCREENSHOTS))
S_BOOL_EX(notification_show_screenshot, NOTIFICATION_SHOW_SCREENSHOT,
      "notification_show_screenshot",
      DEFAULT_NOTIFICATION_SHOW_SCREENSHOT, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Screenshot Notifications",
      "Display an on-screen message when taking a screenshot.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_SCREENSHOTS #ifdef HAVE_GFX_WIDGETS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_SCREENSHOTS) && defined(HAVE_GFX_WIDGETS) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_SCREENSHOTS); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_SCREENSHOTS))
S_UINT_EX(notification_show_screenshot_duration, NOTIFICATION_SHOW_SCREENSHOT_DURATION,
      "notification_show_screenshot_duration",
      DEFAULT_NOTIFICATION_SHOW_SCREENSHOT_DURATION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, NOTIFICATION_SHOW_SCREENSHOT_DURATION_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_notification_show_screenshot_duration, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Screenshot Notification Persistence",
      "Define the duration of the on-screen screenshot message.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_SCREENSHOTS #ifdef HAVE_GFX_WIDGETS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_SCREENSHOTS) && defined(HAVE_GFX_WIDGETS) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_SCREENSHOTS); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_SCREENSHOTS))
S_UINT_EX(notification_show_screenshot_flash, NOTIFICATION_SHOW_SCREENSHOT_FLASH,
      "notification_show_screenshot_flash",
      DEFAULT_NOTIFICATION_SHOW_SCREENSHOT_FLASH, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, NOTIFICATION_SHOW_SCREENSHOT_FLASH_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_notification_show_screenshot_flash, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Screenshot Flash Effect",
      "Display a white flashing effect on-screen with the desired duration when taking a screenshot.")
#endif
#endif
S_BOOL(notification_show_refresh_rate, NOTIFICATION_SHOW_REFRESH_RATE,
      "notification_show_refresh_rate",
      DEFAULT_NOTIFICATION_SHOW_REFRESH_RATE, SD_FLAG_NONE, 0, 0,
      "Refresh Rate Notifications",
      "Display an on-screen message when setting the refresh rate.")
/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_NETWORKING); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_NETWORKING))
S_BOOL(notification_show_netplay_extra, NOTIFICATION_SHOW_NETPLAY_EXTRA,
      "notification_show_netplay_extra",
      DEFAULT_NOTIFICATION_SHOW_NETPLAY_EXTRA, SD_FLAG_NONE, 0, 0,
      "Extra Netplay Notifications",
      "Display non-essential netplay on-screen messages.")
#endif
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(notification_show_when_menu_is_alive, NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
      "notification_show_when_menu_is_alive",
      DEFAULT_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE, SD_FLAG_NONE, 0, 0,
      "Menu-only Notifications",
      "Display notifications only when menu is open.")
#endif

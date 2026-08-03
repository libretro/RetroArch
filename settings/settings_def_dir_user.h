/* Single-source definitions: user directories group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "overlay_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_overlay, OVERLAY_DIRECTORY,
      "overlay_directory",
      g_defaults.dirs[DEFAULT_DIR_OVERLAY], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Overlays",
      "Overlays are stored in this directory.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "osk_overlay_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_osk_overlay, OSK_OVERLAY_DIRECTORY,
      "osk_overlay_directory",
      g_defaults.dirs[DEFAULT_DIR_OSK_OVERLAY], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Keyboard Overlays",
      "Keyboard Overlays are stored in this directory.")
#endif
#endif
/* config key "screenshot_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_screenshot, SCREENSHOT_DIRECTORY,
      "screenshot_directory",
      g_defaults.dirs[DEFAULT_DIR_SCREENSHOT], DIRECTORY_CONTENT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Screenshots",
      "Screenshots are stored in this directory.")
#endif
/* config key "joypad_autoconfig_dir" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_autoconfig, JOYPAD_AUTOCONFIG_DIR,
      "joypad_autoconfig_dir",
      g_defaults.dirs[DEFAULT_DIR_AUTOCONFIG], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Controller Profiles",
      "Controller profiles used to automatically configure controllers are stored in this directory.")
#endif
/* config key "input_remapping_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_input_remapping, INPUT_REMAPPING_DIRECTORY,
      "input_remapping_directory",
      g_defaults.dirs[DEFAULT_DIR_REMAP], DIRECTORY_NONE, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Input Remaps",
      "Input remaps are stored in this directory.")
#endif
/* config key "playlist_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_playlist, PLAYLIST_DIRECTORY,
      "playlist_directory",
      g_defaults.dirs[DEFAULT_DIR_PLAYLIST], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Playlists",
      "Playlists are stored in this directory.")
#endif
/* config key "content_favorites_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_content_favorites, CONTENT_FAVORITES_DIRECTORY,
      "content_favorites_directory",
      g_defaults.dirs[DEFAULT_DIR_CONTENT_FAVORITES], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Favorites Playlist",
      "Save the Favorites playlist to this directory.")
#endif
/* config key "content_history_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_content_history, CONTENT_HISTORY_DIRECTORY,
      "content_history_directory",
      g_defaults.dirs[DEFAULT_DIR_CONTENT_HISTORY], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "History Playlist",
      "Save the History playlist to this directory.")
#endif
/* config key "content_image_history_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_content_image_history, CONTENT_IMAGE_HISTORY_DIRECTORY,
      "content_image_history_directory",
      g_defaults.dirs[DEFAULT_DIR_CONTENT_IMAGE_HISTORY], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Images Playlist",
      "Save the Images History playlist to this directory.")
#endif
/* config key "content_music_history_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_content_music_history, CONTENT_MUSIC_HISTORY_DIRECTORY,
      "content_music_history_directory",
      g_defaults.dirs[DEFAULT_DIR_CONTENT_MUSIC_HISTORY], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Music Playlist",
      "Save the Music playlist to this directory.")
#endif
/* config key "content_video_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_content_video_history, CONTENT_VIDEO_HISTORY_DIRECTORY,
      "content_video_history_directory",
      g_defaults.dirs[DEFAULT_DIR_CONTENT_VIDEO_HISTORY], DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Videos Playlist",
      "Save the Videos playlist to this directory.")
#endif
/* config key "runtime_log_directory" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_DIR(directory_runtime_log, RUNTIME_LOG_DIRECTORY,
      "runtime_log_directory",
      "", DIRECTORY_DEFAULT, SD_FLAG_NONE, 0, directory_action_start_generic,
      "Runtime Logs",
      "Runtime logs are stored in this directory.")
#endif

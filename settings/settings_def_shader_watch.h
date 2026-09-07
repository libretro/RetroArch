/* Single-source definitions: shader watch and directory-memory group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_BOOL_H
#define S_BOOL_H S_BOOL
#endif
#endif
S_BOOL_H(video_shader_watch_files, SHADER_WATCH_FOR_CHANGES,
      "video_shader_watch_files",
      DEFAULT_VIDEO_SHADER_WATCH_FILES, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Watch Shader Files for Changes",
      "Automatically apply changes made to shader files on disk.")
S_BOOL(video_shader_remember_last_dir, VIDEO_SHADER_REMEMBER_LAST_DIR,
      "video_shader_remember_last_dir",
      DEFAULT_VIDEO_SHADER_REMEMBER_LAST_DIR, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Remember Last Used Shader Directory",
      "Open File Browser at the last used directory when loading shader presets and passes.")

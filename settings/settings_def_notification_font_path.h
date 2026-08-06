/* Single-source definitions: notification font path setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "video_font_path" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS(path_font, VIDEO_FONT_PATH,
      "video_font_path",
      directory_assets, SD_FLAG_NONE, CMD_EVENT_REINIT, "ttf", setting_get_string_representation_video_font_path, ST_UI_TYPE_FONT_SELECTOR,
      "Notification Font",
      "Select the font for on-screen notifications.")
#endif

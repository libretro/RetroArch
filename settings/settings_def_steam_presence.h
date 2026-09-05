/* Single-source definitions: Steam rich presence group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_MIST
S_BOOL_EX(steam_rich_presence_enable, STEAM_RICH_PRESENCE_ENABLE,
      "steam_rich_presence_enable",
      false, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Enable Rich Presence",
      "Share your current status within RetroArch on Steam.")
#endif
#ifdef HAVE_MIST
S_UINT_EX(steam_rich_presence_format, STEAM_RICH_PRESENCE_FORMAT,
      "steam_rich_presence_format",
      DEFAULT_STEAM_RICH_PRESENCE_FORMAT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, (STEAM_RICH_PRESENCE_FORMAT_LAST-1), 1, 0, setting_action_ok_uint, setting_get_string_representation_steam_rich_presence_format, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Rich Presence Content Format",
      "Decide what information related to the content will be shared.")
#endif

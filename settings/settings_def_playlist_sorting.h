/* Single-source definitions: playlist sorting group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL_EX(history_list_enable, HISTORY_LIST_ENABLE,
      "history_list_enable",
      DEFAULT_HISTORY_LIST_ENABLE, SD_FLAG_ADVANCED, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "History",
      "Maintain a playlist of recently used games, images, music, and videos.")
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_UINT_EX(content_history_size, CONTENT_HISTORY_SIZE,
      "content_history_size",
      DEFAULT_CONTENT_HISTORY_SIZE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1.0f, 9999.0f, 1.0f, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "History Size",
      "Limit the number of entries in recent playlist for games, images, music, and videos.")
#endif

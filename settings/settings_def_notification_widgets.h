/* Single-source definitions: graphics widgets setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL_EX(video_font_enable, VIDEO_FONT_ENABLE,
      "video_font_enable",
      DEFAULT_FONT_ENABLE, SD_FLAG_NONE, 0, CMD_EVENT_OSD_NOTIFICATION_TOGGLE, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "On-Screen Notifications",
      "Show on-screen messages.")

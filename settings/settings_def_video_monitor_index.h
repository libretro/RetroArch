/* Single-source definitions: monitor index setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_UINT_EX_H
#define S_UINT_EX_H S_UINT_EX
#endif
#endif
S_UINT_EX_H(video_monitor_index, VIDEO_MONITOR_INDEX,
      "video_monitor_index",
      DEFAULT_MONITOR_INDEX, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 0, 15, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_monitor_index, NULL, NULL, NULL, NULL, 0,
      "Monitor Index",
      "Select which display screen to use.")

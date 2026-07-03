/* Single-source definitions: swapchain wait setting.
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
/* Rows marked _EX carry the extended descriptor arguments
 * (callbacks and ui type); every pass except the menu table
 * drops them and behaves exactly like the base row. */
#ifndef S_UINT_EX_H
#define S_UINT_EX_H(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, stax, selx, lfx, rtx, uix, us, sub) S_UINT_H(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub)
#endif
S_UINT_EX_H(video_monitor_index, VIDEO_MONITOR_INDEX,
      "video_monitor_index",
      DEFAULT_MONITOR_INDEX, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_REINIT, 0, 15, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_video_monitor_index, NULL, NULL, NULL, NULL, 0,
      "Monitor Index",
      "Select which display screen to use.")

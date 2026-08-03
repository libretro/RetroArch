/* Single-source definitions: refresh rate group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_FLOAT_EX_H
#define S_FLOAT_EX_H S_FLOAT_EX
#endif
#endif
S_FLOAT(video_refresh_rate, VIDEO_REFRESH_RATE,
      "video_refresh_rate",
      DEFAULT_REFRESH_RATE, "%.3f Hz", SD_FLAG_ALLOW_INPUT | SD_FLAG_LAKKA_ADVANCED, SDESC_FLG_HAS_RANGE | SDESC_FLG_ENFORCE_MIN, CMD_EVENT_NONE, 0, 0, 0.001, NULL, NULL,
      "Vertical Refresh Rate",
      "Vertical refresh rate of your screen. Used to calculate a suitable audio input rate. This will be ignored if 'Threaded Video' is enabled.")
/* config key "video_refresh_rate" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_FLOAT_EX_H(video_refresh_rate, VIDEO_REFRESH_RATE_AUTO,
      "video_refresh_rate_auto",
      DEFAULT_REFRESH_RATE, "%.3f Hz", SD_FLAG_LAKKA_ADVANCED, 0, CMD_EVENT_NONE, 0, 0, 0, setting_action_ok_video_refresh_rate_auto, setting_get_string_representation_st_float_video_refresh_rate_auto, setting_action_start_video_refresh_rate_auto, setting_action_ok_video_refresh_rate_auto, NULL, NULL, 0,
      "Estimated Screen Refresh Rate",
      "The accurate estimated refresh rate of the screen in Hz.")
#endif

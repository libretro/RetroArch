/* Single-source definitions: frame delay group.
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
#ifndef S_UINT_EX_H
#define S_UINT_EX_H S_UINT_EX
#endif
#endif
S_UINT_EX_H(video_frame_delay, VIDEO_FRAME_DELAY,
      "video_frame_delay",
      DEFAULT_FRAME_DELAY, SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, MAXIMUM_FRAME_DELAY, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_frame_delay, NULL, NULL, NULL, NULL, 0,
      "Frame Delay",
      "Reduces latency at the cost of a higher risk of video stuttering.")
S_BOOL_H(video_frame_delay_auto, VIDEO_FRAME_DELAY_AUTO,
      "video_frame_delay_auto",
      DEFAULT_FRAME_DELAY_AUTO, SD_FLAG_LAKKA_ADVANCED, 0, CMD_EVENT_NONE,
      "Automatic Frame Delay",
      "Adjust effective 'Frame Delay' dynamically.")

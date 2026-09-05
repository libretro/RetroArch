/* Single-source definitions: frame throttle general group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_BOOL_EX_H
#define S_BOOL_EX_H S_BOOL_EX
#endif
#ifndef S_FLOAT_EX_H
#define S_FLOAT_EX_H S_FLOAT_EX
#endif
#endif
S_FLOAT_EX_H(fastforward_ratio, FASTFORWARD_RATIO,
      "fastforward_ratio",
      DEFAULT_FASTFORWARD_RATIO, "%.1fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_SET_FRAME_LIMIT, 0, MAXIMUM_FASTFORWARD_RATIO, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Fast-Forward Rate",
      "The maximum rate at which content will be run when using fast-forward (e.g., 5.0x for 60 fps content = 300 fps cap). If set to 0.0x, fast-forward ratio is unlimited (no FPS cap).")
S_BOOL(fastforward_frameskip, FASTFORWARD_FRAMESKIP,
      "fastforward_frameskip",
      DEFAULT_FASTFORWARD_FRAMESKIP, SD_FLAG_NONE, 0, 0,
      "Fast-Forward Frameskip",
      "Skip frames according to fast-forward rate. This conserves power and allows the use of third party frame limiting.")
S_BOOL_EX_H(vrr_runloop_enable, VRR_RUNLOOP_ENABLE,
      "vrr_runloop_enable",
      false, SD_FLAG_CMD_APPLY_AUTO, 0, CMD_EVENT_NONE, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Sync to Exact Content Framerate (G-Sync, FreeSync)",
      "No deviation from core requested timing. Use for Variable Refresh Rate screens (G-Sync, FreeSync, HDMI 2.1 VRR).")

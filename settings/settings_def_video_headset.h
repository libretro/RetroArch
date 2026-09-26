/* Single-source definitions: headset output group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(video_openxr_enable, VIDEO_OPENXR_ENABLE,
      "video_openxr_enable",
      false, SD_FLAG_CMD_APPLY_AUTO, SDESC_FLG_REFRESH, CMD_EVENT_REINIT,
      "Headset Output (OpenXR)",
      "Also show content in an OpenXR headset: each screen as its own floating screen, with both eyes of stereo 3D, and the menu in front of them. Vulkan only. The window keeps its normal output.")
S_UINT_EX(video_openxr_refresh_rate, VIDEO_OPENXR_REFRESH_RATE,
      "video_openxr_refresh_rate",
      0, SD_FLAG_NONE, 0, 0, 0, 0, 0, 0, setting_action_ok_video_openxr_refresh_rate, setting_get_string_representation_video_openxr_refresh_rate, NULL, NULL, setting_uint_action_left_video_openxr_refresh_rate, setting_uint_action_right_video_openxr_refresh_rate, 0,
      "Headset Refresh Rate",
      "The refresh rate to ask the headset for. Auto picks the one that fits the content best; Headset's Choice never asks. The content runs in step with the headset whenever its rate fits.")
S_FLOAT(video_openxr_distance, VIDEO_OPENXR_DISTANCE,
      "video_openxr_distance",
      1.8f, "%.1f m", SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0.5, 10.0, 0.1, NULL, NULL,
      "Screen Distance",
      "How far in front of you the headset shows the screens.")
S_FLOAT(video_openxr_width, VIDEO_OPENXR_WIDTH,
      "video_openxr_width",
      1.6f, "%.1f m", SD_FLAG_ALLOW_INPUT, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0.3, 10.0, 0.1, NULL, NULL,
      "Screen Width",
      "The width of the main screen in the headset. Other screens are sized to match it.")
S_UINT_EX(video_openxr_controllers, VIDEO_OPENXR_CONTROLLERS,
      "video_openxr_controllers",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_openxr_controllers, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Headset Controllers",
      "How the headset's controllers play. Combined makes both one gamepad for player 1. Separate makes the left controller player 1 and the right player 2.")
S_UINT_EX(video_openxr_laser, VIDEO_OPENXR_LASER,
      "video_openxr_laser",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_openxr_laser, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Laser Pointer",
      "Where a controller's trigger points and touches instead of pressing L2 or R2. Auto: the menu while it is open, and second screens such as a touch screen. Always: every screen, for light guns. Off: nowhere.")

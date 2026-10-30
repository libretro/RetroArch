/* Single-source definitions: headset output group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL_EX(video_openxr_enable, VIDEO_OPENXR_ENABLE,
      "video_openxr_enable",
      false, SD_FLAG_CMD_APPLY_AUTO, SDESC_FLG_REFRESH, CMD_EVENT_REINIT, setting_bool_action_ok_video_openxr_enable, NULL, setting_action_start_video_openxr_enable, NULL, setting_bool_action_ok_video_openxr_enable, setting_bool_action_ok_video_openxr_enable, 0,
      "Headset Output (OpenXR)",
      "Also show content in an OpenXR headset: each screen as its own floating screen, with both eyes of stereo 3D, and the menu in front of them. Vulkan, or OpenGL on Linux. The window keeps its normal output.")
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
S_UINT_EX(video_openxr_placement, VIDEO_OPENXR_PLACEMENT,
      "video_openxr_placement",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_openxr_placement, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Screen Placement",
      "Where the headset keeps the screens. Fixed in Room: where Recenter puts them. Follow Head: always in front of you; on a large screen the edges sit where the lenses blur. Lazy Follow: they stay until you turn well away, then glide back in front of you.")
S_BOOL(video_openxr_menu_follow, VIDEO_OPENXR_MENU_FOLLOW,
      "video_openxr_menu_follow",
      false, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Menu in Front of Screen 1",
      "Opens the menu just in front of Screen 1, turned as it is, so moving Screen 1 moves the menu with it. Off: the menu opens straight ahead of the screens' anchor. A menu placed with Edit Screens stays where it was put.")
S_BOOL(video_openxr_perf_boost, VIDEO_OPENXR_PERF_BOOST,
      "video_openxr_perf_boost",
      false, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Headset Performance Boost",
      "Ask the headset for its highest CPU clock, above what it can sustain, so demanding content keeps up with the headset. Uses more battery and runs warmer; a hot headset may still slow down. Applies at once.")
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
S_UINT_EX(video_openxr_laser_beam, VIDEO_OPENXR_LASER_BEAM,
      "video_openxr_laser_beam",
      1, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_openxr_laser_beam, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Laser Beam",
      "A thin line from each controller to where its laser points. Edit Mode Only: while Edit Screens moves the screens. Always: also for the menu and light guns. Off: never.")
S_BOOL_EX(video_openxr_edit_pause, VIDEO_OPENXR_EDIT_PAUSE,
      "video_openxr_edit_pause",
      false, SD_FLAG_NONE, 0, 0, NULL, setting_get_string_representation_video_openxr_edit_pause, NULL, NULL, NULL, NULL, 0,
      "While Editing Screens",
      "What the content does while Edit Screens moves the headset's screens: keeps running, or pauses until editing ends. Edit mode resumes only what it paused.")
S_BOOL(video_openxr_edit_snap, VIDEO_OPENXR_EDIT_SNAP,
      "video_openxr_edit_snap",
      true, SD_FLAG_NONE, 0, 0,
      "Snap to Screens",
      "Lines up a held screen with the screens near it: parallel or square to one, then in its plane and level, or edge to edge; or hinged to one along a shared edge at any angle. The holding hand buzzes as a snap takes hold.")
S_UINT_EX(video_openxr_edit_snap_angles, VIDEO_OPENXR_EDIT_SNAP_ANGLES,
      "video_openxr_edit_snap_angles",
      1, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 4, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_openxr_edit_snap_angles, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Snap to Angles",
      "Stops a screen the other hand's stick turns or tilts at every step of this angle, with a buzz: turns count from facing you, tilts from upright, and a hinged screen from flat with its neighbour. Snap to Screens wins near another screen. Off: the stick turns and tilts smoothly.")
S_UINT_EX(video_openxr_edit_hinge_gap, VIDEO_OPENXR_EDIT_HINGE_GAP,
      "video_openxr_edit_hinge_gap",
      2, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 5, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_openxr_edit_hinge_gap, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Hinge Gap",
      "Leaves this gap between screens that meet edge to edge, side by side, at a corner or hinged at an angle. A screen clicks to it within 1 cm; elsewhere up to 10 cm the gap stays where you put it. 0: the edges touch.")
S_UINT_EX(video_openxr_edit_smoothing, VIDEO_OPENXR_EDIT_SMOOTHING,
      "video_openxr_edit_smoothing",
      2, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_openxr_edit_smoothing, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Edit Smoothing",
      "Steadies the controllers' aim while Edit Screens moves the screens, so a screen held far out does not shake with the hand: a slow hand is smoothed a lot, a fast one little. Off: the aim as tracked.")

/* Single-source definitions: on-screen overlay display group, lower
 * block (everything below the behind-menu-support conditional).
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_OVERLAY
S_BOOL_CH(input_overlay_hide_in_menu, INPUT_OVERLAY_HIDE_IN_MENU,
      "input_overlay_hide_in_menu",
      DEFAULT_OVERLAY_HIDE_IN_MENU, SD_FLAG_NONE, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, overlay_enable_toggle_change_handler,
      "Hide Overlay in Menu",
      "Hide the overlay while inside the menu, and show it again when exiting the menu.")
S_BOOL_CH(input_overlay_hide_when_gamepad_connected, INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
      "input_overlay_hide_when_gamepad_connected",
      DEFAULT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED, SD_FLAG_NONE, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, overlay_enable_toggle_change_handler,
      "Hide Overlay When Controller is Connected",
      "Hide the overlay when a physical controller is connected in port 1, and show it again when the controller is disconnected.")
S_UINT_EX(input_overlay_show_inputs, INPUT_OVERLAY_SHOW_INPUTS,
      "input_overlay_show_inputs",
      DEFAULT_OVERLAY_SHOW_INPUTS, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, OVERLAY_SHOW_INPUT_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_input_overlay_show_inputs, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Show Inputs on Overlay",
      "Show registered input on the on-screen overlay.")
S_UINT_EX(input_overlay_show_inputs_port, INPUT_OVERLAY_SHOW_INPUTS_PORT,
      "input_overlay_show_inputs_port",
      DEFAULT_OVERLAY_SHOW_INPUTS_PORT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, MAX_USERS-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_input_overlay_show_inputs_port, NULL, NULL, NULL, NULL, 0,
      "Show Inputs From Port",
      "Select the port to monitor for input to display when 'Show Inputs on Overlay' is set to 'Touched'.")
S_BOOL_CH(input_overlay_show_mouse_cursor, INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
      "input_overlay_show_mouse_cursor",
      DEFAULT_OVERLAY_SHOW_MOUSE_CURSOR, SD_FLAG_NONE, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, overlay_show_mouse_cursor_change_handler,
      "Show Mouse Cursor With Overlay",
      "Show the mouse cursor when using an on-screen overlay.")
S_BOOL_CH(input_overlay_auto_rotate, INPUT_OVERLAY_AUTO_ROTATE,
      "input_overlay_auto_rotate",
      DEFAULT_OVERLAY_AUTO_ROTATE, SD_FLAG_NONE, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, overlay_enable_toggle_change_handler,
      "Auto-Rotate Overlay",
      "Where supported, automatically rotate the overlay to match screen orientation/aspect ratio.")
#endif

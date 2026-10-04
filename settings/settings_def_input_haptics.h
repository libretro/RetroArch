/* Single-source definitions: haptics and sensors group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(input_menu_toggle_gamepad_combo, INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
      "input_menu_toggle_gamepad_combo",
      DEFAULT_MENU_TOGGLE_GAMEPAD_COMBO, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, (INPUT_COMBO_LAST-1), 1, 0, setting_action_ok_uint, setting_get_string_representation_gamepad_combo, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Menu Toggle (Controller Combo)",
      "Controller button combination to toggle menu.")
S_UINT_EX(input_quit_gamepad_combo, INPUT_QUIT_GAMEPAD_COMBO,
      "input_quit_gamepad_combo",
      DEFAULT_QUIT_GAMEPAD_COMBO, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, (INPUT_COMBO_LAST-1), 1, 0, setting_action_ok_uint, setting_get_string_representation_gamepad_combo, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Quit (Controller Combo)",
      "Controller button combination to quit RetroArch.")
S_UINT_EX(input_hotkey_block_delay, INPUT_HOTKEY_BLOCK_DELAY,
      "input_hotkey_block_delay",
      DEFAULT_INPUT_HOTKEY_BLOCK_DELAY, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 600, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Hotkey Enable Delay (Frames)",
      "Add a delay in frames before normal input is blocked after pressing the assigned 'Hotkey Enable' key. Allows normal input from the 'Hotkey Enable' key to be captured when it is mapped to another action (e.g. RetroPad 'Select').")
S_BOOL(input_hotkey_device_merge, INPUT_HOTKEY_DEVICE_MERGE,
      "input_hotkey_device_merge",
      DEFAULT_INPUT_HOTKEY_DEVICE_MERGE, SD_FLAG_NONE, 0, 0,
      "Hotkey Device Type Merge",
      "Block all hotkeys from both keyboard and controller device types if either type has 'Hotkey Enable' set.")
S_BOOL(input_hotkey_follows_player1, INPUT_HOTKEY_FOLLOWS_PLAYER1,
      "input_hotkey_follows_player1",
      DEFAULT_INPUT_HOTKEY_FOLLOWS_PLAYER1, SD_FLAG_NONE, 0, 0,
      "Hotkeys Follow Player 1",
      "Hotkeys are bound to core port 1, even if core port 1 is remapped to a different user. Note: keyboard hotkeys will not work if core port 1 is remapped to any user > 1 (keyboard input is from user 1).")
/* config key "menu_swap_ok_cancel_buttons" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(input_menu_swap_ok_cancel_buttons, MENU_INPUT_SWAP_OK_CANCEL,
      "menu_swap_ok_cancel",
      DEFAULT_MENU_SWAP_OK_CANCEL_BUTTONS, SD_FLAG_NONE, 0, 0,
      "Swap OK and Cancel Buttons",
      "Swap buttons for OK/Cancel. Disabled is the Japanese button orientation, enabled is the western orientation.")
#endif
/* config key "menu_swap_scroll_buttons" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(input_menu_swap_scroll_buttons, MENU_INPUT_SWAP_SCROLL,
      "menu_swap_scroll",
      DEFAULT_MENU_SWAP_SCROLL_BUTTONS, SD_FLAG_NONE, 0, 0,
      "Swap Scroll Buttons",
      "Swap buttons for scrolling. Disabled scrolls 10 items with L/R and alphabetically with L2/R2.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(input_all_users_control_menu, INPUT_ALL_USERS_CONTROL_MENU,
      "all_users_control_menu",
      DEFAULT_ALL_USERS_CONTROL_MENU, SD_FLAG_NONE, 0, 0,
      "All Users Control Menu",
      "Allow any user to control the menu. If disabled, only User 1 can control the menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(input_menu_singleclick_playlists, MENU_SINGLECLICK_PLAYLISTS,
      "menu_singleclick_playlists",
      DEFAULT_MENU_SINGLECLICK_PLAYLISTS, SD_FLAG_NONE, 0, 0,
      "Single-Click Playlists",
      "Skip 'Run' menu when launching playlist entries. Press D-Pad while holding OK to access 'Run' menu.")
#endif
/* The configuration row lives under defined(HAVE_MENU); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_MENU))
S_BOOL(input_menu_allow_tabs_back, MENU_ALLOW_TABS_BACK,
      "menu_allow_tabs_back",
      DEFAULT_MENU_ALLOW_TABS_BACK, SD_FLAG_NONE, 0, 0,
      "Allow Back From Tabs",
      "Return to Main Menu from tabs/sidebar when pressing Back.")
#endif
S_BOOL(input_remap_binds_enable, INPUT_REMAP_BINDS_ENABLE,
      "input_remap_binds_enable",
      true, SD_FLAG_ADVANCED, 0, 0,
      "Remap Controls for This Core",
      "Override the input binds with the remapped binds set for the current core.")
S_BOOL(input_remap_sort_by_controller_enable, INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
      "input_remap_sort_by_controller_enable",
      false, SD_FLAG_ADVANCED, 0, 0,
      "Sort Remaps By Gamepad",
      "Remaps will only apply to the active gamepad in which they were saved.")
S_BOOL(input_assign_ports_on_button_press, INPUT_ASSIGN_PORTS_ON_BUTTON_PRESS,
      "input_assign_ports_on_button_press",
      false, SD_FLAG_ADVANCED, 0, 0,
      "Assign Ports on First Button Press",
      "When content starts, no controller is mapped to a core port; each is given the next free core port when a button is first pressed on it. Applies from the next content start.")
S_UINT_EX(input_assign_ports_keyboard, INPUT_ASSIGN_PORTS_KEYBOARD,
      "input_assign_ports_keyboard",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_assign_ports_keyboard, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Keyboard on First Button Press",
      "With 'Assign Ports on First Button Press': what a key bound to a port's controls does. 'Assigns its Port' gives the port its core port, as a button of its controller would. 'Waits for its Controller' leaves that to the controller's buttons, unless the port has no controller connected.")
S_BOOL(input_trigger_full_range, INPUT_TRIGGER_FULL_RANGE,
      "input_trigger_full_range",
      false, SD_FLAG_ADVANCED, 0, 0,
      "Full-Range Analog Triggers",
      "For L2 and R2 on an axis that rests at one end of its range: count the pull from where the trigger rests. A full pull then gives the whole analog range and half a pull presses the button. When off only the second half of the pull counts, as before.")
S_UINT_EX(input_rotation, INPUT_ROTATION,
      "input_rotation",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 4, 1, 0, setting_action_ok_uint, setting_get_string_representation_input_rotation, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Input Rotation",
      "Turns the D-Pad and the analog sticks a core sees, for content shown rotated. The angle is the one Video Rotation would be set to: at 90 degrees, left on the controller is up in the game. 'Auto' follows the Video Rotation setting.")
S_UINT_EX(input_socd_horizontal, INPUT_SOCD_HORIZONTAL,
      "input_socd_horizontal",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_socd, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "SOCD Cleaning (Left + Right)",
      "What a core sees of the D-Pad when left and right are held together. 'OFF' passes both on. 'Neutral' gives neither, 'Last Input Priority' the one pressed later, 'First Input Priority' the one pressed first.")
S_UINT_EX(input_socd_vertical, INPUT_SOCD_VERTICAL,
      "input_socd_vertical",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 4, 1, 0, setting_action_ok_uint, setting_get_string_representation_socd, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "SOCD Cleaning (Up + Down)",
      "What a core sees of the D-Pad when up and down are held together. 'OFF' passes both on. 'Neutral' gives neither, 'Last Input Priority' the one pressed later, 'First Input Priority' the one pressed first, 'Up Priority' always up.")
S_BOOL(input_autodetect_enable, INPUT_AUTODETECT_ENABLE,
      "input_autodetect_enable",
      DEFAULT_INPUT_AUTODETECT_ENABLE, SD_FLAG_ADVANCED, 0, 0,
      "Autoconfig",
      "Automatically configures controllers that have a profile, Plug-and-Play style.")

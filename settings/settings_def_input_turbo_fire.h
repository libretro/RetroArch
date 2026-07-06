/* Single-source definitions: turbo fire options group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(input_turbo_enable, INPUT_TURBO_ENABLE,
      "input_turbo_enable",
      DEFAULT_TURBO_ENABLE, SD_FLAG_NONE, 0, 0,
      "Turbo Fire",
      "Disabled stops all turbo fire operations.")
S_UINT_EX(input_turbo_mode, INPUT_TURBO_MODE,
      "input_turbo_mode",
      DEFAULT_TURBO_MODE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, (INPUT_TURBO_MODE_LAST-1), 1, 0, setting_action_ok_uint, setting_get_string_representation_turbo_mode, NULL, NULL, setting_uint_action_left_with_refresh, setting_uint_action_right_with_refresh, ST_UI_TYPE_UINT_COMBOBOX,
      "Turbo Mode",
      "Select the general behavior of turbo mode.")
S_INT_EX(input_turbo_bind, INPUT_TURBO_BIND,
      "input_turbo_bind",
      DEFAULT_TURBO_BIND, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -1, (RARCH_ANALOG_BIND_LIST_END-1), 1, -1, setting_action_ok_retropad_bind, setting_get_string_representation_retropad_bind, NULL, NULL, setting_action_left_retropad_bind, setting_action_right_retropad_bind, ST_UI_TYPE_UINT_COMBOBOX,
      "Turbo Bind",
      "Turbo activating RetroPad bind. Empty uses the port-specific bind.")
S_UINT_EX(input_turbo_button, INPUT_TURBO_BUTTON,
      "input_turbo_button",
      DEFAULT_TURBO_BUTTON, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, (RARCH_FIRST_CUSTOM_BIND-1), 1, 0, setting_action_ok_retropad_bind, setting_get_string_representation_retropad_bind, NULL, NULL, setting_action_left_retropad_bind, setting_action_right_retropad_bind, ST_UI_TYPE_UINT_COMBOBOX,
      "Turbo Button",
      "Target turbo button in 'Single Button' mode.")
S_BOOL(input_turbo_allow_dpad, INPUT_TURBO_ALLOW_DPAD,
      "input_turbo_allow_dpad",
      DEFAULT_TURBO_ALLOW_DPAD, SD_FLAG_NONE, 0, 0,
      "Turbo Allow D-Pad Directions",
      "If enabled, digital directional inputs (also known as d-pad or 'hatswitch') can be turbo.")
S_UINT_EX(input_turbo_period, INPUT_TURBO_PERIOD,
      "input_turbo_period",
      DEFAULT_TURBO_PERIOD, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 2, 100, 1, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Turbo Period",
      "The period in frames when turbo-enabled buttons are pressed.")
S_UINT_EX(input_turbo_duty_cycle, INPUT_TURBO_DUTY_CYCLE,
      "input_turbo_duty_cycle",
      DEFAULT_TURBO_DUTY_CYCLE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 100, 1, 0, setting_action_ok_uint, setting_get_string_representation_turbo_duty_cycle, NULL, NULL, NULL, NULL, 0,
      "Turbo Duty Cycle",
      "The number of frames from the Turbo Period the buttons are held down for. If this number is equal to or greater than the Turbo Period, the buttons will never release.")

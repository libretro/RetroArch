/* Single-source definitions: sensor and hotkey extras group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_FLOAT_EX(input_sensor_accelerometer_sensitivity, INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
      "input_sensor_accelerometer_sensitivity",
      DEFAULT_SENSOR_ACCELEROMETER_SENSITIVITY, "%.1f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -5.0, 5.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Accelerometer Sensitivity",
      "Adjust the sensitivity of the Accelerometer.")
S_FLOAT_EX(input_sensor_gyroscope_sensitivity, INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
      "input_sensor_gyroscope_sensitivity",
      DEFAULT_SENSOR_GYROSCOPE_SENSITIVITY, "%.1f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -5.0, 5.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Gyroscope Sensitivity",
      "Adjust the sensitivity of the Gyroscope.")
S_UINT_EX(input_bind_timeout, INPUT_BIND_TIMEOUT,
      "input_bind_timeout",
      DEFAULT_INPUT_BIND_TIMEOUT, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1, 30, 1, 1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Bind Timeout",
      "Amount of seconds to wait until proceeding to the next bind.")
S_UINT_EX(input_bind_hold, INPUT_BIND_HOLD,
      "input_bind_hold",
      DEFAULT_INPUT_BIND_HOLD, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 5, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Bind Hold",
      "Amount of seconds to hold an input to bind it.")
S_ACTION(INPUT_SENSOR_SETTINGS,
      "input_sensor_settings",
      "Motion/Light Sensors",
      "Change accelerometer, gyroscope and illuminance settings.")
S_ACTION(INPUT_HAPTIC_FEEDBACK_SETTINGS,
      "input_haptic_feedback_settings",
      "Haptic Feedback/Vibration",
      "Change haptic feedback and vibration settings.")
S_ACTION(INPUT_MENU_SETTINGS,
      "input_menu_settings",
      "Menu Controls",
      "Change menu control settings.")

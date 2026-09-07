/* Single-source definitions: analog deadzone and sensitivity group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_FLOAT_EX(input_axis_threshold, INPUT_BUTTON_AXIS_THRESHOLD,
      "input_axis_threshold",
      DEFAULT_AXIS_THRESHOLD, "%.3f", SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, 0.05, 0.99, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Input Button Axis Threshold",
      "How far an axis must be tilted to result in a button press when using 'Analog to Digital'.")
S_FLOAT_EX(input_analog_deadzone, INPUT_ANALOG_DEADZONE,
      "input_analog_deadzone",
      DEFAULT_ANALOG_DEADZONE, "%.1f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Analog Deadzone",
      "Ignore analog stick movements below deadzone value.")
S_FLOAT_EX(input_analog_sensitivity, INPUT_ANALOG_SENSITIVITY,
      "input_analog_sensitivity",
      DEFAULT_ANALOG_SENSITIVITY, "%.1f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -5.0, 5.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Analog Sensitivity",
      "Adjust the sensitivity of analog sticks.")
S_BOOL(input_sensors_enable, INPUT_SENSORS_ENABLE,
      "input_sensors_enable",
      DEFAULT_INPUT_SENSORS_ENABLE, SD_FLAG_NONE, 0, 0,
      "Auxiliary Sensor Input",
      "Enable input from accelerometer, gyroscope and illuminance sensors, if supported by the current hardware. May have a performance impact and/or increase power drain on some platforms.")

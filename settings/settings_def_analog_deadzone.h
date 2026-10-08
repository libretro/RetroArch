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
S_FLOAT_EX(input_analog_trigger_deadzone, INPUT_ANALOG_TRIGGER_DEADZONE,
      "input_analog_trigger_deadzone",
      DEFAULT_ANALOG_TRIGGER_DEADZONE, "%.2f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 0.95, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Analog Trigger Deadzone",
      "Ignore the first part of an analog trigger's pull, or of a pressure-sensitive button's press; the rest of the way is rescaled to the whole range. Apart from the stick deadzone, which applies to the sticks only.")
S_FLOAT_EX(input_analog_outer_deadzone, INPUT_ANALOG_OUTER_DEADZONE,
      "input_analog_outer_deadzone",
      DEFAULT_ANALOG_OUTER_DEADZONE, "%.2f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 0.5, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Analog Outer Deadzone",
      "Treat a stick tilted this close to its edge as tilted all the way, for sticks that wear and stop short of full. The movement between the deadzone and this edge is spread over the whole range.")
S_FLOAT_EX(input_analog_anti_deadzone, INPUT_ANALOG_ANTI_DEADZONE,
      "input_analog_anti_deadzone",
      DEFAULT_ANALOG_ANTI_DEADZONE, "%.2f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 0.9, 0.05, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Analog Anti-Deadzone",
      "Start a stick's movement at this share of full as soon as it leaves the deadzone. For cores that have a deadzone of their own, which would otherwise come on top of this one: set it to the core's deadzone.")
S_FLOAT_EX(input_analog_response_curve, INPUT_ANALOG_RESPONSE_CURVE,
      "input_analog_response_curve",
      DEFAULT_ANALOG_RESPONSE_CURVE, "%.1f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.5, 3.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Analog Response Curve",
      "How a stick's movement grows with its tilt. 1.0 is a straight line. Above 1.0 small tilts move less, for finer control near the centre; below 1.0 they move more, for a quicker response.")
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

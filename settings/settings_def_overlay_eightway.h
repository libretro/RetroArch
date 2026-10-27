/* Single-source definitions: on-screen overlay diagonal-sensitivity
 * group, the tail block below the overlay preset tables.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_OVERLAY
S_UINT_EX(input_overlay_dpad_diagonal_sensitivity, INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
      "input_overlay_dpad_diagonal_sensitivity",
      DEFAULT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_EIGHTWAY_DIAGONAL_SENSITIVITY, 0, 100, 1, 0, setting_action_ok_uint, setting_get_string_representation_percentage, NULL, NULL, NULL, NULL, 0,
      "D-Pad Diagonal Sensitivity",
      "Adjust the size of diagonal zones. Round for 8-way symmetry; set to 100% for full 8-way.")
S_UINT_EX(input_overlay_abxy_diagonal_sensitivity, INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
      "input_overlay_abxy_diagonal_sensitivity",
      DEFAULT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY, SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_EIGHTWAY_DIAGONAL_SENSITIVITY, 0, 100, 1, 0, setting_action_ok_uint, setting_get_string_representation_percentage, NULL, NULL, NULL, NULL, 0,
      "ABXY Diagonal Sensitivity",
      "Adjust the size of diagonal zones. Round for 8-way symmetry; set to 100% for full 8-way.")
S_UINT_EX(input_overlay_dpad_method, INPUT_OVERLAY_DPAD_METHOD,
      "input_overlay_dpad_method",
      DEFAULT_OVERLAY_DPAD_METHOD, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_overlay_contact_method, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "D-Pad Input Method",
      "How the D-pad reads a touch. 'Direction': the way the finger's centre lies. 'Contact Area': every way the finger's contact covers, so a thumb laid across two directions presses both. 'Both': either.")
S_UINT_EX(input_overlay_abxy_method, INPUT_OVERLAY_ABXY_METHOD,
      "input_overlay_abxy_method",
      DEFAULT_OVERLAY_ABXY_METHOD, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 2, 1, 0, setting_action_ok_uint, setting_get_string_representation_overlay_contact_method, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "ABXY Input Method",
      "How the face buttons read a touch. 'Direction': the button the finger's centre is nearest. 'Contact Area': every button the finger's contact covers, so a thumb between two presses both. 'Both': either.")
S_UINT_EX(input_overlay_contact_scale, INPUT_OVERLAY_CONTACT_SCALE,
      "input_overlay_contact_scale",
      DEFAULT_OVERLAY_CONTACT_SCALE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 50, 300, 10, 0, setting_action_ok_uint, setting_get_string_representation_percentage, NULL, NULL, NULL, NULL, 0,
      "Contact Area Size",
      "How large a finger's contact counts, against the size the touchscreen reports. Raise it for a touchscreen that reports small contacts.")
S_UINT_EX(input_overlay_analog_recenter_zone, INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
      "input_overlay_analog_recenter_zone",
      DEFAULT_INPUT_OVERLAY_ANALOG_RECENTER_ZONE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 100, 1, 0, setting_action_ok_uint, setting_get_string_representation_percentage, NULL, NULL, NULL, NULL, 0,
      "Analog Recenter Zone",
      "Recenter analog input when within this radius of the overlay's center.")
#endif

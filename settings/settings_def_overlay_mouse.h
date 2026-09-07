/* Single-source definitions: overlay mouse group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_mouse_speed, INPUT_OVERLAY_MOUSE_SPEED,
      "input_overlay_mouse_speed",
      DEFAULT_INPUT_OVERLAY_MOUSE_SPEED, "%.1fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.1, 5.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Mouse Speed",
      "Adjust cursor movement speed.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(input_overlay_mouse_hold_to_drag, INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
      "input_overlay_mouse_hold_to_drag",
      DEFAULT_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG, SD_FLAG_NONE, 0, 0,
      "Long Press to Drag",
      "Long press the screen to begin holding a button.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(input_overlay_mouse_hold_msec, INPUT_OVERLAY_MOUSE_HOLD_MSEC,
      "input_overlay_mouse_hold_msec",
      DEFAULT_INPUT_OVERLAY_MOUSE_HOLD_MSEC, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 1000, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Long Press Threshold (ms)",
      "Adjust the hold time required for a long press.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(input_overlay_mouse_dtap_to_drag, INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
      "input_overlay_mouse_dtap_to_drag",
      DEFAULT_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG, SD_FLAG_NONE, 0, 0,
      "Double Tap to Drag",
      "Double tap the screen to begin holding a button on the second tap. Adds latency to mouse clicks.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(input_overlay_mouse_dtap_msec, INPUT_OVERLAY_MOUSE_DTAP_MSEC,
      "input_overlay_mouse_dtap_msec",
      DEFAULT_INPUT_OVERLAY_MOUSE_DTAP_MSEC, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 500, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Double Tap Threshold (ms)",
      "Adjust the allowable time between taps when detecting a double tap.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_mouse_swipe_threshold, INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
      "input_overlay_mouse_swipe_threshold",
      DEFAULT_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD, "%.1f%%", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 10.0, 0.1, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Swipe Threshold",
      "Adjust the allowable drift range when detecting a long press or tap. Expressed as a percentage of the smaller screen dimension.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(input_overlay_mouse_alt_two_touch_input, INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
      "input_overlay_mouse_alt_two_touch_input",
      DEFAULT_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, OVERLAY_MOUSE_BTN_NONE, OVERLAY_MOUSE_BTN_END - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_overlay_mouse_btn, NULL, NULL, NULL, NULL, 0,
      "Alt 2-Touch Input",
      "Use second touch as a mouse button while controlling the cursor.")
#endif

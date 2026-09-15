/* Single-source definitions: overlay lightgun group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_INT_EX(input_overlay_lightgun_port, INPUT_OVERLAY_LIGHTGUN_PORT,
      "input_overlay_lightgun_port",
      DEFAULT_INPUT_OVERLAY_LIGHTGUN_PORT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -1, MAX_USERS - 1, 1, -1, setting_action_ok_uint, setting_get_string_representation_overlay_lightgun_port, NULL, NULL, NULL, NULL, 0,
      "Lightgun Port",
      "Set the core port to receive input from the overlay lightgun.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(input_overlay_lightgun_trigger_on_touch, INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
      "input_overlay_lightgun_trigger_on_touch",
      DEFAULT_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH, SD_FLAG_NONE, 0, 0,
      "Trigger on Touch",
      "Send trigger input with pointer input.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(input_overlay_lightgun_trigger_delay, INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
      "input_overlay_lightgun_trigger_delay",
      DEFAULT_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, OVERLAY_LIGHTGUN_TRIG_MAX_DELAY, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Trigger Delay (frames)",
      "Delay trigger input to allow the cursor time to move. This delay is also used to wait for the correct multi-touch count.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(input_overlay_lightgun_two_touch_input, INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
      "input_overlay_lightgun_two_touch_input",
      DEFAULT_INPUT_OVERLAY_LIGHTGUN_MULTI_TOUCH_INPUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, OVERLAY_LIGHTGUN_ACTION_NONE, OVERLAY_LIGHTGUN_ACTION_END - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_overlay_lightgun_action, NULL, NULL, NULL, NULL, 0,
      "2-Touch Input",
      "Select input to send when two pointers are on screen. Trigger Delay should be nonzero to distinguish from other input.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(input_overlay_lightgun_three_touch_input, INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
      "input_overlay_lightgun_three_touch_input",
      DEFAULT_INPUT_OVERLAY_LIGHTGUN_MULTI_TOUCH_INPUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, OVERLAY_LIGHTGUN_ACTION_NONE, OVERLAY_LIGHTGUN_ACTION_END - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_overlay_lightgun_action, NULL, NULL, NULL, NULL, 0,
      "3-Touch Input",
      "Select input to send when three pointers are on screen. Trigger Delay should be nonzero to distinguish from other input.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(input_overlay_lightgun_four_touch_input, INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
      "input_overlay_lightgun_four_touch_input",
      DEFAULT_INPUT_OVERLAY_LIGHTGUN_MULTI_TOUCH_INPUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, OVERLAY_LIGHTGUN_ACTION_NONE, OVERLAY_LIGHTGUN_ACTION_END - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_overlay_lightgun_action, NULL, NULL, NULL, NULL, 0,
      "4-Touch Input",
      "Select input to send when four pointers are on screen. Trigger Delay should be nonzero to distinguish from other input.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(input_overlay_lightgun_allow_offscreen, INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
      "input_overlay_lightgun_allow_offscreen",
      DEFAULT_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN, SD_FLAG_NONE, 0, 0,
      "Allow Off-Screen",
      "Allow out-of-bounds aiming. Disable to clamp off-screen aim to the in-bounds edge.")
#endif

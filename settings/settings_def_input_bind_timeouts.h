/* Single-source definitions: bind timeout group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if TARGET_OS_IPHONE; the string
 * tables always carry this row via the strings pass. */
#if TARGET_OS_IPHONE || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "keyboard_gamepad_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(input_keyboard_gamepad_enable, INPUT_ICADE_ENABLE,
      "input_icade_enable",
      false, SD_FLAG_NONE, 0, 0,
      "Keyboard Controller Mapping")
#endif
#endif
/* Descriptor and configuration rows are #if TARGET_OS_IPHONE; the string
 * tables always carry this row via the strings pass. */
#if TARGET_OS_IPHONE || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX_NS(input_keyboard_gamepad_mapping_type, INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
      "keyboard_gamepad_mapping_type",
      1, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_keyboard_gamepad_mapping_type, NULL, NULL, NULL, NULL, 0,
      "Keyboard Controller Mapping Type")
#endif
/* Descriptor and configuration rows are #if TARGET_OS_IPHONE; the string
 * tables always carry this row via the strings pass. */
#if TARGET_OS_IPHONE || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "small_keyboard_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(input_small_keyboard_enable, INPUT_SMALL_KEYBOARD_ENABLE,
      "input_small_keyboard_enable",
      false, SD_FLAG_NONE, 0, 0,
      "Small Keyboard")
#endif
#endif

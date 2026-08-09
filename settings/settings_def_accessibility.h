/* Single-source definitions: accessibility group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "accessibility_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_EX(accessibility_enable, ACCESSIBILITY_ENABLED,
      "accessibility_enabled",
      DEFAULT_ACCESSIBILITY_ENABLE, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Accessibility Enable",
      "Enable Text-to-Speech to aid in menu navigation.")
#endif
S_UINT_EX(accessibility_narrator_speech_speed, ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
      "accessibility_narrator_speech_speed",
      DEFAULT_ACCESSIBILITY_NARRATOR_SPEECH_SPEED, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 10, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
      "Text-to-Speech Speed",
      "The speed for the Text-to-Speech voice.")
/* Descriptor and configuration rows are #if defined(__linux__) && !defined(ANDROID); the string
 * tables always carry this row via the strings pass. */
#if (defined(__linux__) && !defined(ANDROID)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(accessibility_narrator_engine, ACCESSIBILITY_NARRATOR_ENGINE,
      "accessibility_narrator_engine",
      DEFAULT_ACCESSIBILITY_NARRATOR_ENGINE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, ACCESSIBILITY_NARRATOR_ENGINE_LAST - 1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_accessibility_narrator_engine, NULL, NULL, NULL, NULL, 0,
      "Text-to-Speech Engine",
      "The Text-to-Speech backend used for the narrator.")
#endif

/* Single-source definitions: back touch group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef VITA; the string
 * tables always carry this row via the strings pass. */
#if defined(VITA) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "input_backtouch_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(input_backtouch_enable, INPUT_TOUCH_ENABLE,
      "input_touch_enable",
      DEFAULT_INPUT_BACKTOUCH_ENABLE, SD_FLAG_NONE, 0, 0,
      "Touch")
#endif
#endif
/* Descriptor and configuration rows are #ifdef VITA; the string
 * tables always carry this row via the strings pass. */
#if defined(VITA) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "input_backtouch_toggle" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(input_backtouch_toggle, INPUT_PREFER_FRONT_TOUCH,
      "input_prefer_front_touch",
      DEFAULT_INPUT_BACKTOUCH_TOGGLE, SD_FLAG_NONE, 0, 0,
      "Prefer Front Touch")
#endif
#endif

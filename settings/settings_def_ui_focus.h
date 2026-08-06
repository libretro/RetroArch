/* Single-source definitions: window focus group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_BOOL_H
#define S_BOOL_H S_BOOL
#endif
#endif

S_BOOL(pause_nonactive, PAUSE_NONACTIVE,
      "pause_nonactive",
      DEFAULT_PAUSE_NONACTIVE, SD_FLAG_LAKKA_ADVANCED, 0, 0,
      "Pause Content When Not Active",
      "Pause content when RetroArch is not the active window.")
/* Descriptor and configuration rows are #if !defined(RARCH_MOBILE); the string
 * tables always carry this row via the strings pass. */
#if !defined(RARCH_MOBILE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_H(video_disable_composition, VIDEO_DISABLE_COMPOSITION,
      "video_disable_composition",
      DEFAULT_DISABLE_COMPOSITION, SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_LAKKA_ADVANCED, 0, CMD_EVENT_REINIT,
      "Disable Desktop Composition",
      "Window managers use composition to apply visual effects, detect unresponsive windows, among other things.")
#endif

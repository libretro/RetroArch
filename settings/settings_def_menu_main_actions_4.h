/* Single-source definitions: content history action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_ACTION_H
#define S_ACTION_H S_ACTION
#endif
#endif
S_ACTION_H(LOAD_CONTENT_HISTORY,
      "load_recent",
      "History",
      "Select content from recent history playlist.")

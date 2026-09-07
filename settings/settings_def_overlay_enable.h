/* Single-source definitions: overlay enable setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "input_overlay_behind_menu" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(input_overlay_behind_menu, INPUT_OVERLAY_BEHIND_MENU,
      "overlay_behind_menu",
      DEFAULT_OVERLAY_BEHIND_MENU, SD_FLAG_NONE, 0, 0,
      "Show Overlay Behind Menu",
      "Show the overlay behind instead of in front of the menu.")
#endif
#endif

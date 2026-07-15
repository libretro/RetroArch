/* Single-source definitions: overlay preset path setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "input_overlay" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS(path_overlay, OVERLAY_PRESET,
      "input_overlay",
      directory_overlay, SD_FLAG_NONE, CMD_EVENT_OVERLAY_INIT, "cfg", NULL, 0,
      "Overlay Preset",
      "Select an overlay from File Browser.")
#endif
#endif

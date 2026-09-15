/* Single-source definitions: overlay auto-scale setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX(input_overlay_auto_scale, INPUT_OVERLAY_AUTO_SCALE,
      "input_overlay_auto_scale",
      DEFAULT_INPUT_OVERLAY_AUTO_SCALE, SD_FLAG_CMD_APPLY_AUTO, 0, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Auto-Scale Overlay",
      "Automatically adjust overlay scale and UI element spacing to match screen aspect ratio. Produces best results with controller overlays.")
#endif

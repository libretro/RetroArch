/* Single-source definitions: overlay opacity setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OVERLAY; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OVERLAY) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(input_overlay_opacity, OVERLAY_OPACITY,
      "input_overlay_opacity",
      DEFAULT_INPUT_OVERLAY_OPACITY, "%.2f", SD_FLAG_CMD_APPLY_AUTO, SDESC_RANGE_MINMAX, CMD_EVENT_OVERLAY_SET_ALPHA_MOD, 0, 1, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Overlay Opacity",
      "Opacity of all UI elements of the overlay.")
#endif

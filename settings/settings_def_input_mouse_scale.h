/* Single-source definitions: mouse scale setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef GEKKO
S_UINT_EX(input_mouse_scale, INPUT_MOUSE_SCALE,
      "input_mouse_scale",
      DEFAULT_MOUSE_SCALE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 4, 1, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0,
      "Mouse Scale",
      "Adjust x/y scale for Wiimote light gun speed.")
#endif

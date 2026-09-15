/* Single-source definitions: UI appearance group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef _3DS
S_BOOL(bottom_font_enable, BOTTOM_FONT_ENABLE,
      "bottom_font_enable",
      DEFAULT_BOTTOM_FONT_ENABLE, SD_FLAG_NONE, 0, 0,
      "Font Enable",
      "Display bottom menu font. Enable to display button descriptions on the bottom screen. This excludes the save state date.")
#endif
#ifdef _3DS
S_INT_EX(bottom_font_color_red, BOTTOM_FONT_COLOR_RED,
      "bottom_font_color_red",
      DEFAULT_BOTTOM_FONT_COLOR, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Color: Red",
      "Adjust bottom screen font red color.")
#endif
#ifdef _3DS
S_INT_EX(bottom_font_color_green, BOTTOM_FONT_COLOR_GREEN,
      "bottom_font_color_green",
      DEFAULT_BOTTOM_FONT_COLOR, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Color: Green",
      "Adjust bottom screen font green color.")
#endif
#ifdef _3DS
S_INT_EX(bottom_font_color_blue, BOTTOM_FONT_COLOR_BLUE,
      "bottom_font_color_blue",
      DEFAULT_BOTTOM_FONT_COLOR, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Color: Blue",
      "Adjust bottom screen font blue color.")
#endif
#ifdef _3DS
S_INT_EX(bottom_font_color_opacity, BOTTOM_FONT_COLOR_OPACITY,
      "bottom_font_color_opacity",
      DEFAULT_BOTTOM_FONT_COLOR, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 255, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Color Opacity",
      "Adjust bottom screen font opacity.")
#endif
#ifdef _3DS
S_FLOAT_EX(bottom_font_scale, BOTTOM_FONT_SCALE,
      "bottom_font_scale",
      DEFAULT_BOTTOM_FONT_SCALE, "%.2f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1, 2, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Font Scale",
      "Adjust bottom screen font scale.")
#endif

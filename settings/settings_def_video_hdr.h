/* Single-source definitions: HDR tuning group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_FLOAT_EX(video_hdr_menu_nits, MENU_HDR_BRIGHTNESS_NITS,
      "video_hdr_menu_nits",
      DEFAULT_MENU_HDR_BRIGHTNESS_NITS, "%.0f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 40.0, 1000.0, 10.0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Brightness",
      "Brightness of the menu in cd/m2 (nits) when using an HDR display. Only visible when HDR is enabled in Settings > Video > HDR.")
S_FLOAT_EX(video_hdr_paper_white_nits, VIDEO_HDR_PAPER_WHITE_NITS,
      "video_hdr_paper_white_nits",
      DEFAULT_VIDEO_HDR_PAPER_WHITE_NITS, "%.0f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.0, 10000.0, 10.0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Brightness",
      "Sets the HDR brightness level in nits. Use in combination with your display's physical brightness settings. For a starting point, set this to 80 and your display's brightness to full. Alternatively, set this to the max nits of your display and turn your display's brightness down until it looks right.")
S_UINT_EX(video_hdr_expand_gamut, VIDEO_HDR_EXPAND_GAMUT,
      "video_hdr_expand_gamut",
      DEFAULT_VIDEO_HDR_EXPAND_GAMUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_video_hdr_expand_gamut, NULL, NULL, NULL, NULL, 0,
      "Colour Boost",
      "Uses your display's full colour range to create a brighter, more saturated image. For colours more faithful to the original game design, set this to Accurate.")

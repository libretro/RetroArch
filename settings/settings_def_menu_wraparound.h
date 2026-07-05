/* Single-source definitions: navigation wraparound setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_navigation_wraparound_enable, NAVIGATION_WRAPAROUND,
      "menu_navigation_wraparound_enable",
      true, SD_FLAG_ADVANCED, 0, 0,
      "Navigation Wrap-Around",
      "Wrap-around to beginning and/or end if boundary of list is reached horizontally or vertically.")

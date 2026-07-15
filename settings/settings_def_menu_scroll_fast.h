/* Single-source definitions: fast menu scrolling toggle.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_scroll_fast, MENU_SCROLL_FAST,
      "menu_scroll_fast",
      DEFAULT_MENU_SCROLL_FAST, SD_FLAG_NONE, 0, 0,
      "Menu Scroll Acceleration",
      "Maximum speed of cursor when holding a direction to scroll.")

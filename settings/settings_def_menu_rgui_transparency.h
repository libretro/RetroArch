/* Single-source definitions: RGUI transparency setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_rgui_transparency, MENU_RGUI_TRANSPARENCY,
      "menu_rgui_transparency",
      DEFAULT_RGUI_TRANSPARENCY, SD_FLAG_NONE, 0, 0,
      "Transparency",
      "Enable background display of content while Quick Menu is active. Disabling transparency may alter theme colors.")

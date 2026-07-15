/* Single-source definitions: menu scrolling group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(menu_linear_filter, MENU_LINEAR_FILTER,
      "menu_linear_filter",
      true, SD_FLAG_NONE, 0, 0,
      "Linear Filter",
      "Adds a slight blur to the menu to soften hard pixel edges.")
S_UINT_EX(menu_rgui_internal_upscale_level, MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
      "rgui_internal_upscale_level",
      DEFAULT_RGUI_INTERNAL_UPSCALE_LEVEL, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, RGUI_UPSCALE_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_rgui_internal_upscale_level, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Internal Upscaling",
      "Upscale menu interface before drawing to screen. When used with 'Menu Linear Filter' enabled, removes scaling artifacts (uneven pixels) while maintaining a sharp image. Has a significant performance impact that increases with upscaling level.")

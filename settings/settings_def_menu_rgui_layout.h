/* Single-source definitions: RGUI layout group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "rgui_border_filler_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_rgui_border_filler_enable, MENU_RGUI_BORDER_FILLER_ENABLE,
      "menu_rgui_border_filler_enable",
      true, SD_FLAG_NONE, 0, 0,
      "Border Filler",
      "Display menu border.")
#endif
/* config key "rgui_background_filler_thickness_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_rgui_background_filler_thickness_enable, MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
      "menu_rgui_background_filler_thickness_enable",
      true, SD_FLAG_NONE, 0, 0,
      "Background Filler Thickness",
      "Increase coarseness of menu background checkerboard pattern.")
#endif
/* config key "rgui_border_filler_thickness_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(menu_rgui_border_filler_thickness_enable, MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
      "menu_rgui_border_filler_thickness_enable",
      true, SD_FLAG_NONE, 0, 0,
      "Border Filler Thickness",
      "Increase coarseness of menu border checkerboard.")
#endif
S_BOOL(menu_rgui_full_width_layout, MENU_RGUI_FULL_WIDTH_LAYOUT,
      "menu_rgui_full_width_layout",
      DEFAULT_RGUI_FULL_WIDTH_LAYOUT, SD_FLAG_NONE, 0, 0,
      "Use Full-Width Layout",
      "Resize and position menu entries to make best use of available screen space. Disable this to use classic fixed-width two column layout.")

/* Single-source definitions: RGUI aspect ratio group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if !defined(DINGUX); the string
 * tables always carry this row via the strings pass. */
#if !defined(DINGUX) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(menu_rgui_aspect_ratio, MENU_RGUI_ASPECT_RATIO,
      "rgui_aspect_ratio",
      DEFAULT_RGUI_ASPECT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RGUI_ASPECT_RATIO_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_rgui_aspect_ratio, NULL, NULL, NULL, NULL, 0,
      "Aspect Ratio",
      "Select menu aspect ratio. Widescreen ratios increase the horizontal resolution of the menu interface. (May require a restart if 'Lock Menu Aspect Ratio' is disabled)")
#endif
/* Descriptor and configuration rows are #if !defined(DINGUX); the string
 * tables always carry this row via the strings pass. */
#if !defined(DINGUX) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The range ceiling is platform-shaped: the Wii lacks the last two
 * lock modes. Two row variants keep the values in the row instead
 * of builder code; the combobox hint rides along. */
#if defined(GEKKO) && !defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(menu_rgui_aspect_ratio_lock, MENU_RGUI_ASPECT_RATIO_LOCK,
      "rgui_aspect_ratio_lock",
      DEFAULT_RGUI_ASPECT_LOCK, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RGUI_ASPECT_RATIO_LOCK_LAST-3, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_rgui_aspect_ratio_lock, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Lock Aspect Ratio",
      "Ensures that the menu is always displayed with the correct aspect ratio. If disabled, the quick menu will be stretched to match the currently loaded content.")
#else
S_UINT_EX(menu_rgui_aspect_ratio_lock, MENU_RGUI_ASPECT_RATIO_LOCK,
      "rgui_aspect_ratio_lock",
      DEFAULT_RGUI_ASPECT_LOCK, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, RGUI_ASPECT_RATIO_LOCK_LAST-1, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_rgui_aspect_ratio_lock, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Lock Aspect Ratio",
      "Ensures that the menu is always displayed with the correct aspect ratio. If disabled, the quick menu will be stretched to match the currently loaded content.")
#endif
#endif

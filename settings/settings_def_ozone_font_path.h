/* Single-source definitions: Ozone font path setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_OZONE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_OZONE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "ozone_font" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS(path_menu_ozone_font, OZONE_FONT,
      "ozone_font",
      directory_assets, SD_FLAG_LAKKA_ADVANCED, CMD_EVENT_REINIT, "ttf", setting_get_string_representation_video_font_path, ST_UI_TYPE_FONT_SELECTOR,
      "Font",
      "Select a different main font to be used by the menu.")
#endif
#endif

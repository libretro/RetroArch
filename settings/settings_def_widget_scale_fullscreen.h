/* Single-source definitions: fullscreen widget scale variant.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Row referencing MENU_WIDGET_SCALE_FACTOR; strings owned by another def file. */
#if !defined(SETTINGS_DEF_STRINGS_PASS) && !defined(SETTINGS_DEF_CONFIG_PASS) && !defined(SETTINGS_DEF_ENUM_PASS)
SDESC_FLOAT_ROW_LV(menu_widget_scale_factor, MENU_WIDGET_SCALE_FACTOR,
                     MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
                     DEFAULT_MENU_WIDGET_SCALE_FACTOR, "%.2fx",
                     SD_FLAG_NONE, 0, 0)
#endif

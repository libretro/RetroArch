/* Single-source definitions: windowed widget scale setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_GFX_WIDGETS #if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_GFX_WIDGETS) && (!(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_FLOAT_EX(menu_widget_scale_factor_windowed, MENU_WIDGET_SCALE_FACTOR_WINDOWED,
      "menu_widget_scale_factor_windowed",
      DEFAULT_MENU_WIDGET_SCALE_FACTOR_WINDOWED, "%.2fx", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0.2, 5.0, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Graphics Widgets Scale Override (Windowed)",
      "Apply a manual scaling factor override when drawing display widgets in windowed mode. Only applies when 'Scale Graphics Widgets Automatically' is disabled. Can be used to increase or decrease the size of decorated notifications, indicators and controls independently from the menu itself.")
#endif

/* Single-source definitions: menubar setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__); the string
 * tables always carry this row via the strings pass. */
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(ui_menubar_enable, UI_MENUBAR_ENABLE,
      "ui_menubar_enable",
      DEFAULT_UI_MENUBAR_ENABLE, SD_FLAG_NONE, 0, CMD_EVENT_REINIT,
      "Show Menu Bar",
      "Show window menu bar.")
#endif

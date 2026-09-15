/* Single-source definitions: ninth main menu action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_LAKKA) || defined(HAVE_WIFI); the string
 * tables always carry this row via the strings pass. */
#if (defined(HAVE_LAKKA) || defined(HAVE_WIFI)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(WIFI_SETTINGS,
      "wifi_settings",
      "Wi-Fi",
      "Scan for wireless networks and establish connection.")
#endif

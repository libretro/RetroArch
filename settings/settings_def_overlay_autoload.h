/* Single-source definitions: overlay autoload-preferred toggle.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_OVERLAY
S_BOOL_CH(input_overlay_enable_autopreferred, OVERLAY_AUTOLOAD_PREFERRED,
      "input_overlay_enable_autopreferred",
      DEFAULT_OVERLAY_ENABLE_AUTOPREFERRED, SD_FLAG_NONE, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, 0, overlay_enable_toggle_change_handler,
      "Autoload Preferred Overlay",
      "Load the autoconfig-preferred overlay for the current core/content, if one exists.")
#endif

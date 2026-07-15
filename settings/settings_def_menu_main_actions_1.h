/* Single-source definitions: first main menu action group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION(CORE_LIST_UNLOAD,
      "unload_core",
      "Unload Core",
      "Release the loaded core.")

/* Single-source definitions: save management action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION(CLOUD_SYNC_SETTINGS,
      "cloud_sync_settings",
      "Cloud Sync",
      "Change cloud sync settings.")

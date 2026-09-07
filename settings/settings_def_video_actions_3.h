/* Single-source definitions: third video action group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION(VIDEO_SYNCHRONIZATION_SETTINGS,
      "video_synchronization_settings",
      "Synchronization",
      "Change video synchronization settings.")
S_ACTION(VIDEO_SCALING_SETTINGS,
      "video_scaling_settings",
      "Scaling",
      "Change video scaling settings.")

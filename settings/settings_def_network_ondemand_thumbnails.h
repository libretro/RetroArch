/* Single-source definitions: on-demand thumbnails setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(network_on_demand_thumbnails, NETWORK_ON_DEMAND_THUMBNAILS,
      "network_on_demand_thumbnails",
      DEFAULT_NETWORK_ON_DEMAND_THUMBNAILS, SD_FLAG_NONE, 0, 0,
      "On-Demand Thumbnail Downloads",
      "Automatically download missing thumbnails while browsing playlists. Has a severe performance impact.")
#endif

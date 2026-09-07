/* Single-source definitions: experimental cores setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING #ifdef HAVE_UPDATE_CORES; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) && defined(HAVE_UPDATE_CORES) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(network_buildbot_show_experimental_cores, CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
      "core_updater_show_experimental_cores",
      DEFAULT_NETWORK_BUILDBOT_SHOW_EXPERIMENTAL_CORES, SD_FLAG_NONE, 0, 0,
      "Show Experimental Cores",
      "Include 'experimental' cores in the Core Downloader list. These are typically for development/testing purposes only, and are not recommended for general use.")
#endif

/* Single-source definitions: updater auto-extract setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(network_buildbot_auto_extract_archive, CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
      "core_updater_auto_extract_archive",
      DEFAULT_NETWORK_BUILDBOT_AUTO_EXTRACT_ARCHIVE, SD_FLAG_NONE, 0, 0,
      "Automatically Extract Downloaded Archive",
      "After downloading, automatically extract files contained in the downloaded archives.")
#endif

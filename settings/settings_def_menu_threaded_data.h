/* Single-source definitions: threaded data runloop setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_THREADS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_THREADS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(threaded_data_runloop_enable, THREADED_DATA_RUNLOOP_ENABLE,
      "threaded_data_runloop_enable",
      DEFAULT_THREADED_DATA_RUNLOOP_ENABLE, SD_FLAG_ADVANCED, 0, 0,
      "Threaded Tasks",
      "Perform tasks on a separate thread.")
#endif

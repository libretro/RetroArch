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
S_BOOL(thread_prefer_fast_cores, THREAD_PREFER_FAST_CORES,
      "thread_prefer_fast_cores",
      DEFAULT_THREAD_PREFER_FAST_CORES, SD_FLAG_ADVANCED, 0, 0,
      "Prefer Performance Cores",
      "Keep the main, video, audio and task threads on the fastest CPU cores of a mixed-core processor. Has no effect on processors whose cores are all alike. Takes effect on restart.")
S_BOOL(video_thread_priority, VIDEO_THREAD_PRIORITY,
      "video_thread_priority",
      DEFAULT_VIDEO_THREAD_PRIORITY, SD_FLAG_ADVANCED, 0, CMD_EVENT_REINIT,
      "Elevate Video Thread Priority",
      "Ask the operating system to schedule the Threaded Video thread ahead of ordinary threads, without real-time scheduling: the Games class on Windows, user-interactive quality of service on Apple systems, a lower nice value on Linux and Android. A system that refuses keeps the default priority.")
S_BOOL(main_thread_priority, MAIN_THREAD_PRIORITY,
      "main_thread_priority",
      DEFAULT_MAIN_THREAD_PRIORITY, SD_FLAG_ADVANCED | SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Elevate Main Thread Priority",
      "Ask the operating system to schedule the main thread, which runs the core and the menu, ahead of ordinary threads, without real-time scheduling. Content running unthrottled, as in fast-forward, then takes CPU time from other programs first. A system that refuses keeps the default priority.")
#endif

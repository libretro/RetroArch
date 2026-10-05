/* Single-source definitions: Windows low-latency power plan.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are Windows desktop only; the
 * string tables always carry this row via the strings pass. */
#if (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(win32_power_plan, WIN32_POWER_PLAN,
      "win32_power_plan",
      DEFAULT_WIN32_POWER_PLAN, SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Low-Latency Power Plan",
      "Switches to a copy of the active Windows power plan that keeps the processor at full speed with no parked cores and re-evaluates performance states every 5 seconds instead of every 15 ms, cutting kernel interrupt activity. Only applies when plugged in. The original plan is restored on exit.")
#endif

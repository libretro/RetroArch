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
      "Switches to a copy of the active Windows power plan that keeps the processor at full speed - minimum and maximum processor state at 100%, energy preference on performance - with no parked cores, re-evaluates performance states every 5 seconds instead of every 15 ms, and keeps PCI Express links and USB ports out of power saving, cutting kernel interrupt activity and wake-up delays. Only applies when plugged in. The original plan is restored on exit.")
S_BOOL(win32_power_plan_idle_disable, WIN32_POWER_PLAN_IDLE_DISABLE,
      "win32_power_plan_idle_disable",
      DEFAULT_WIN32_POWER_PLAN_IDLE_DISABLE, SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Disable Processor Idle States",
      "With the Low-Latency Power Plan on, also keep every processor core out of its idle states, so none has to wake before running. The processor then draws full power and runs hot even with nothing to do. Only applies when plugged in.")
#endif

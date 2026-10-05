#if defined(HAVE_RUNAHEAD) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(run_ahead_performance_guard, RUN_AHEAD_PERFORMANCE_GUARD,
      "run_ahead_performance_guard",
      false, SD_FLAG_NONE, 0, 0,
      "Adjust Run-Ahead Based on Performance Metrics",
      "Automatically reduce Run-Ahead frames when the measured emulation cost exceeds the frame budget. Disable to always use the requested number of frames.")
#endif

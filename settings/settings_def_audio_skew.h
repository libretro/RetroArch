/* Single-source definitions: audio skew group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_FLOAT_EX_H
#define S_FLOAT_EX_H S_FLOAT_EX
#endif
#endif
S_FLOAT_EX_H(audio_max_timing_skew, AUDIO_MAX_TIMING_SKEW,
      "audio_max_timing_skew",
      DEFAULT_MAX_TIMING_SKEW, "%.3f", SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0.0, 0.5, 0.01, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Maximum Timing Skew",
      "The maximum change in audio input rate. Increasing this enables very large changes in timing at the cost of an inaccurate audio pitch (e.g. running PAL cores on NTSC displays).")

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
/* No descriptor row any more, so the setting is not offered in the
 * menu: the number it carried is the device's transfer granularity,
 * which the platform reports and audio_driver_device_block_frames()
 * hands to the driver that needs it. A device fact is not a
 * preference, and every other driver already takes its buffer from
 * the latency alone. The configuration and string rows stay, so a
 * config carrying audio_block_frames still loads and still round
 * trips rather than being dropped on the next save. */
#if defined(SETTINGS_DEF_STRINGS_PASS) || defined(SETTINGS_DEF_CONFIG_PASS)
S_UINT(audio_block_frames, AUDIO_BLOCK_FRAMES,
      "audio_block_frames",
      0, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 0, 1024, 64, 0,
      setting_action_ok_uint, NULL,
      "Block Frames",
      "Number of frames the audio driver moves per block. 0 asks the driver for the device's own value, which is what most setups want; a larger block trades latency for resilience against dropouts.")
#endif

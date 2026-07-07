/* Single-source definitions: microphone general group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_STRING_H
#define S_STRING_H S_STRING
#endif
#endif
#ifdef HAVE_MICROPHONE
#if !defined(RARCH_CONSOLE)
/* config key "microphone_device" differs from the label string; the
 * configuration.c row stays literal for this setting. Strings and
 * enum only: the menu row lives in settings_def_audio_device.h with
 * the device-cycling handlers, and emitting a second one here
 * shadow-built the setting twice with divergent start actions. */
#if defined(SETTINGS_DEF_STRINGS_PASS) || defined(SETTINGS_DEF_ENUM_PASS)
S_STRING_H(microphone_device, MICROPHONE_DEVICE,
      "microphone_device",
      "", SD_FLAG_ALLOW_INPUT, 0, setting_string_action_ok_microphone_device, setting_get_string_representation_string_audio_device, setting_generic_action_start_default, NULL, setting_string_action_left_microphone_device, setting_string_action_right_microphone_device, ST_UI_TYPE_STRING_LINE_EDIT,
      "Device",
      "Override the default input device the microphone driver uses. This is driver dependent.")
#endif
#endif
#endif
#ifdef HAVE_MICROPHONE
/* config key "microphone_rate" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_EX(microphone_sample_rate, MICROPHONE_INPUT_RATE,
      "microphone_input_rate",
      DEFAULT_INPUT_RATE, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1000, 192000, 100.0, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Default Input Rate (Hz)",
      "Audio input sample rate, used if a core doesn't request a specific number.")
#endif
#endif
#ifdef HAVE_MICROPHONE
S_UINT_EX(microphone_resampler_quality, MICROPHONE_RESAMPLER_QUALITY,
      "microphone_resampler_quality",
      DEFAULT_AUDIO_RESAMPLER_QUALITY_LEVEL, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, RESAMPLER_QUALITY_DONTCARE, RESAMPLER_QUALITY_HIGHEST, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_audio_resampler_quality, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Resampler Quality",
      "Lower this value to favor performance/lower latency over audio quality, increase for better audio quality at the expense of performance/lower latency.")
#endif

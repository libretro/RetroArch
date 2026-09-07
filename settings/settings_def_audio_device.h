/* Single-source definitions: audio device group.
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
/* Descriptor and configuration rows are #if !defined(RARCH_CONSOLE); the string
 * tables always carry this row via the strings pass. */
#if !defined(RARCH_CONSOLE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "audio_device" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_H(audio_device, AUDIO_DEVICE,
      "audio_device",
      "", SD_FLAG_NONE, 0, setting_string_action_ok_audio_device, setting_get_string_representation_string_audio_device, setting_string_action_start_audio_device, NULL, setting_string_action_left_audio_device, setting_string_action_right_audio_device, 0,
      "Device",
      "Override the default audio device the audio driver uses. This is driver dependent.")
#endif
#endif
/* Row referencing MICROPHONE_DEVICE; strings owned by another def file. */
#if !defined(SETTINGS_DEF_STRINGS_PASS) && !defined(SETTINGS_DEF_CONFIG_PASS) && !defined(SETTINGS_DEF_ENUM_PASS)
#if !defined(RARCH_CONSOLE)
                  #ifdef HAVE_MICROPHONE
                  SDESC_STRING_ROW(microphone_device, MICROPHONE_DEVICE,
                     "", SD_FLAG_ALLOW_INPUT, 0,
                     setting_string_action_ok_microphone_device,
                     setting_get_string_representation_string_audio_device,
                     setting_string_action_start_microphone_device, NULL,
                     setting_string_action_left_microphone_device,
                     setting_string_action_right_microphone_device,
                     ST_UI_TYPE_STRING_LINE_EDIT),
#endif
#endif
#endif
/* config key "audio_out_rate" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_UINT_EX(audio_output_sample_rate, AUDIO_OUTPUT_RATE,
      "audio_output_rate",
      DEFAULT_OUTPUT_RATE, SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0, 1000, 192000, 100.0, 0, setting_action_ok_uint_special, NULL, NULL, NULL, NULL, NULL, 0,
      "Output Rate (Hz)",
      "Audio output sample rate.")
#endif
/* Row referencing MICROPHONE_INPUT_RATE; strings owned by another def file. */
#if !defined(SETTINGS_DEF_STRINGS_PASS) && !defined(SETTINGS_DEF_CONFIG_PASS) && !defined(SETTINGS_DEF_ENUM_PASS)
#ifdef HAVE_MICROPHONE
                  SDESC_UINT_ROW_EX(microphone_sample_rate, MICROPHONE_INPUT_RATE,
                     DEFAULT_INPUT_RATE,
                     SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, 0,
                     1000, 192000, 100.0, 0,
                     setting_action_ok_uint_special, NULL,
                     NULL, NULL, NULL, NULL, 0),
#endif
#endif

/* Single-source definitions: audio state group.
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
/* Descriptor and configuration rows are #if TARGET_OS_IOS; the string
 * tables always carry this row via the strings pass. */
#if TARGET_OS_IOS || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under (TARGET_OS_IOS); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || ((TARGET_OS_IOS))
S_BOOL(audio_respect_silent_mode, AUDIO_RESPECT_SILENT_MODE,
      "audio_respect_silent_mode",
      DEFAULT_AUDIO_RESPECT_SILENT_MODE, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Respect Silent Mode",
      "Mute all audio in Silent Mode.")
#endif
#endif
S_UINT_EX(audio_fastforward_mode, AUDIO_FASTFORWARD_MODE,
      "audio_fastforward_mode",
      DEFAULT_AUDIO_FASTFORWARD_MODE, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, FASTFORWARD_AUDIO_DISCARD, FASTFORWARD_AUDIO_MAX, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_audio_fastforward_mode, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Fast-Forward Audio",
      "What to do with audio while fast-forwarding. 'Discard' drops samples the output device cannot take. 'Speed Up' prevents crackling but raises pitch. 'Time-Stretch' keeps the pitch, at some CPU cost, and is also the only one of these that applies during slow-motion.")
#if defined(HAVE_AUDIO_LOWPASS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(audio_fastforward_lowpass, AUDIO_FASTFORWARD_LOWPASS,
      "audio_fastforward_lowpass",
      DEFAULT_AUDIO_FASTFORWARD_LOWPASS, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 1000, AUDIO_FASTFORWARD_LOWPASS_OFF, 1000.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_audio_fastforward_lowpass, NULL, NULL, setting_uint_action_left_audio_fastforward_lowpass, setting_uint_action_right_audio_fastforward_lowpass, ST_UI_TYPE_UINT_COMBOBOX_DESC,
      "Fast-Forward Low-Pass Cutoff",
      "Reference cutoff for a low-pass filter applied to fast-forward audio; 'OFF' disables it. The cutoff is divided by the current speed multiplier, so 12000 Hz becomes 4000 Hz at 3x and the sound gets smoother the faster you go. Pick a high reference for a gentle effect: at 3x, 36000 Hz still passes 12 kHz. The filter can never open wider than the core's own bandwidth. Only applies while fast-forwarding, and only when Fast-Forward Audio is set to Speed Up or Time-Stretch.")
#endif
S_BOOL(audio_rewind_mute, AUDIO_REWIND_MUTE,
      "audio_rewind_mute",
      DEFAULT_AUDIO_REWIND_MUTE, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Rewind Audio Mute",
      "Automatically mute audio when using rewind.")
S_FLOAT_EX_H(audio_volume, AUDIO_VOLUME,
      "audio_volume",
      DEFAULT_AUDIO_VOLUME, "%.1f", SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, -80, 12, 1.0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Volume Gain (dB)",
      "Audio volume (in dB). 0 dB is normal volume, and no gain is applied.")
/* Descriptor and configuration rows are #ifdef HAVE_AUDIOMIXER; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_AUDIOMIXER) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_AUDIOMIXER); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_AUDIOMIXER))
S_FLOAT_EX(audio_mixer_volume, AUDIO_MIXER_VOLUME,
      "audio_mixer_volume",
      DEFAULT_AUDIO_MIXER_VOLUME, "%.1f", SD_FLAG_LAKKA_ADVANCED, SDESC_RANGE_MINMAX, 0, -80, 12, 1.0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Mixer Volume Gain (dB)",
      "Global audio mixer volume (in dB). 0 dB is normal volume, and no gain is applied.")
#endif
#endif

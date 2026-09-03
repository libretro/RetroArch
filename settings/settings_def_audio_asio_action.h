/* Single-source definitions: ASIO device action and output channels.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_ASIO; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_ASIO) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(AUDIO_ASIO_CONTROL_PANEL,
      "audio_asio_control_panel", SD_FLAG_NONE, setting_action_asio_control_panel, NULL, 0,
      "Open ASIO Control Panel",
      "Open the ASIO driver control panel to configure device routing and buffer settings.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_ASIO; the string
 * tables always carry this row via the strings pass. The value is the
 * first of the two device outputs to play through, counted from 0 and
 * stepping by two, so the menu shows pairs: 1-2, 3-4, 5-6. Shown with
 * the device's own names for the pair when the driver is running. */
#if defined(HAVE_ASIO) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT(audio_asio_output_channel, AUDIO_ASIO_OUTPUT_CHANNEL,
      "audio_asio_output_channel",
      0, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 62, 2, 0,
      setting_action_ok_uint, setting_get_string_representation_uint_audio_asio_output_channel,
      "ASIO Output Channels",
      "Which two outputs on the audio device RetroArch plays through. An ASIO device lists its outputs in numbered pairs; on a device with more than two, the first pair is not always the one your speakers or headphones are on. Pick the pair that matches the jacks you are listening to, as named by the device. Two-output devices only have 1-2.")
#endif

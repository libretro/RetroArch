/* Single-source definitions: audio format negotiation group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_EX(audio_format_negotiation, AUDIO_FORMAT_NEGOTIATION,
      "audio_format_negotiation",
      DEFAULT_AUDIO_FORMAT_NEGOTIATION, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, AUDIO_FORMAT_NEGOTIATION_INT16, AUDIO_FORMAT_NEGOTIATION_FLOAT, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_audio_format_negotiation, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Audio Format Negotiation (Hint)",
      "Sample format the audio driver requests from the output device. 'Float' asks for 32-bit floating-point, 'Int16' for 16-bit integer. Only affects drivers that can negotiate the format (WASAPI, DirectSound, XAudio2, ALSA, SDL2); others use their fixed format. A hint only: a driver falls back if the device rejects the requested format. 'Int16' pairs with the 'Resample to Fixed Integer' hint to keep the whole audio path in the integer domain.")

S_UINT_EX(audio_output_layout, AUDIO_OUTPUT_LAYOUT,
      "audio_output_layout",
      DEFAULT_AUDIO_OUTPUT_LAYOUT, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_AUDIO_REINIT, 0, 4, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_audio_output_layout, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_COMBOBOX,
      "Output Speaker Layout",
      "Speaker layout to open the output device with. 'Stereo' is the pipeline as it always was. The wider layouts open a wider device where the driver can, and the stereo mix is upmixed to it at the last step: fronts as they are, a centre from the sum, the rear pair at -3 dB, the bass to the LFE. '5.1' puts the rear pair at the back, '5.1 Surround' at the sides; a device that drives them from the other position reports so and is treated as it is. Drivers that cannot open more than stereo stay stereo.")

S_BOOL(audio_headphone_virtual_surround, AUDIO_HEADPHONE_VIRTUAL_SURROUND,
      "audio_headphone_virtual_surround",
      DEFAULT_AUDIO_HEADPHONE_VIRTUAL_SURROUND, SD_FLAG_NONE, 0, CMD_EVENT_AUDIO_REINIT,
      "Headphone Virtual Surround",
      "On a stereo device, widen the mix to a virtual 5.1 and render it to two ears as a head would hear those speakers: the rear pair behind the listener, a mild crossfeed across the fronts. For headphones; on speakers it only narrows the stereo. Has no effect when the device is opened with a wider layout.")

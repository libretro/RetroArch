/* Single-source definitions: microphone setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_MICROPHONE
S_BOOL(microphone_enable, MICROPHONE_ENABLE,
      "microphone_enable",
      DEFAULT_AUDIO_ENABLE, SD_FLAG_NONE, 0, 0,
      "Microphone",
      "Enable audio input in supported cores. Has no overhead if the core isn't using a microphone.")
#endif

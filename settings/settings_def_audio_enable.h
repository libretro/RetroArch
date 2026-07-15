/* Single-source definitions: audio enable setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(audio_enable, AUDIO_ENABLE,
      "audio_enable",
      DEFAULT_AUDIO_ENABLE, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Audio",
      "Enable audio output.")

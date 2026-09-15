/* Single-source definitions: audio DSP plugin path setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "audio_dsp_plugin" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS(path_audio_dsp_plugin, AUDIO_DSP_PLUGIN,
      "audio_dsp_plugin",
      directory_audio_filter, SD_FLAG_LAKKA_ADVANCED, CMD_EVENT_DSP_FILTER_INIT, "dsp", NULL, 0,
      "DSP Plugin",
      "Audio DSP plugin that processes audio before it's sent to the driver.")
#endif

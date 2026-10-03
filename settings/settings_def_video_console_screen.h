/* Single-source definitions: console screen filters, gamma and PS3
 * PAL60.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor rows follow the consoles that have each setting; the
 * string tables always carry them via the strings pass.  Most config
 * keys differ from the label strings, so the configuration.c rows
 * stay literal for all of them. */
#ifndef SETTINGS_DEF_CONFIG_PASS
#if defined(GEKKO) || defined(_XBOX360) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_NS(video_gamma, VIDEO_GAMMA,
      "video_gamma",
      DEFAULT_GAMMA, SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_ADVANCED, SDESC_RANGE_MINMAX, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES, 0, MAX_GAMMA_SETTING, 1, 0, setting_action_ok_uint, NULL,
      "Video Gamma")
#endif
#if defined(_XBOX1) || defined(HW_RVL) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(video_soft_filter, VIDEO_SOFT_FILTER,
      "soft_filter",
      DEFAULT_SOFT_FILTER, SD_FLAG_NONE, 0, CMD_EVENT_VIDEO_APPLY_STATE_CHANGES,
      "Soft Filter")
#endif
#if defined(HAVE_PSGL) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_NS(video_pal60_enable, PAL60_ENABLE,
      "pal60_enable",
      DEFAULT_PAL60_ENABLE, SD_FLAG_NONE, 0, CMD_EVENT_NONE,
      "Use PAL60 Mode")
#endif
#if defined(_XBOX1) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_NS(video_flicker_filter, VIDEO_FILTER_FLICKER,
      "video_filter_flicker",
      DEFAULT_FLICKER_FILTER, SD_FLAG_NONE, SDESC_RANGE_MINMAX, CMD_EVENT_NONE, 0, 5, 1, 0, setting_action_ok_uint, NULL,
      "Flicker filter")
#endif
#endif

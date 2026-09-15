/* Single-source definitions: menu sounds group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(audio_enable_menu, AUDIO_ENABLE_MENU,
      "audio_enable_menu",
      DEFAULT_AUDIO_ENABLE_MENU, SD_FLAG_NONE, 0, 0,
      "Mixer",
      "Play simultaneous audio streams even in the menu.")
/* config key "audio_enable_menu_ok" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(audio_enable_menu_ok, MENU_SOUND_OK,
      "menu_sound_ok",
      DEFAULT_AUDIO_ENABLE_MENU_OK, SD_FLAG_NONE, 0, 0,
      "Enable 'OK' Sound")
#endif
/* config key "audio_enable_menu_cancel" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(audio_enable_menu_cancel, MENU_SOUND_CANCEL,
      "menu_sound_cancel",
      DEFAULT_AUDIO_ENABLE_MENU_CANCEL, SD_FLAG_NONE, 0, 0,
      "Enable 'Cancel' Sound")
#endif
/* config key "audio_enable_menu_notice" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(audio_enable_menu_notice, MENU_SOUND_NOTICE,
      "menu_sound_notice",
      DEFAULT_AUDIO_ENABLE_MENU_NOTICE, SD_FLAG_NONE, 0, 0,
      "Enable 'Notice' Sound")
#endif
/* config key "audio_enable_menu_bgm" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(audio_enable_menu_bgm, MENU_SOUND_BGM,
      "menu_sound_bgm",
      DEFAULT_AUDIO_ENABLE_MENU_BGM, SD_FLAG_NONE, 0, 0,
      "Enable 'BGM' Sound")
#endif
/* config key "audio_enable_menu_scroll" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(audio_enable_menu_scroll, MENU_SOUND_SCROLL,
      "menu_sound_scroll",
      DEFAULT_AUDIO_ENABLE_MENU_SCROLL, SD_FLAG_NONE, 0, 0,
      "Enable 'Scroll' Sounds")
#endif

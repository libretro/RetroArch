/* Single-source definitions: notch write-over setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(ANDROID) || TARGET_OS_IOS; the string
 * tables always carry this row via the strings pass. */
#if defined(ANDROID) || TARGET_OS_IOS || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "video_notch_write_over_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL_NS(video_notch_write_over_enable, VIDEO_NOTCH_WRITE_OVER,
      "video_notch_write_over",
      DEFAULT_NOTCH_WRITE_OVER_ENABLE, SD_FLAG_NONE, 0, 0,
      "Enable fullscreen over notch in Android and iOS devices")
#endif
#endif

/* Single-source definitions: video filter path setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_PATH_DS_H
#define S_PATH_DS_H S_PATH_DS
#endif
#endif
/* config key "video_filter" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS_H(path_softfilter_plugin, VIDEO_FILTER,
      "video_filter",
      directory_video_filter, SD_FLAG_LAKKA_ADVANCED, CMD_EVENT_VIDEO_FILTER_INIT, "filt", setting_get_string_representation_video_filter, 0,
      "Video Filter",
      "Apply a CPU-powered video filter. Might come at a high performance cost. Some video filters might only work for cores that use 32-bit or 16-bit color.")
#endif

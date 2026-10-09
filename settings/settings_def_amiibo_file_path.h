/* Single-source definitions: amiibo file path setting.
 * Grammar identical to settings_def_video_filter_path.h; row order is
 * menu display order; h2json.py parses these rows for Crowdin. */

#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_PATH_DS_H
#define S_PATH_DS_H S_PATH_DS
#endif
#endif
/* config key "amiibo_file" matches the label string. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_PATH_DS_H(path_amiibo, NFC_LOAD_AMIIBO,
      "amiibo_file",
      directory_amiibo, SD_FLAG_NONE, CMD_EVENT_NFC_LOAD_AMIIBO, "bin", NULL, 0,
      "Load Amiibo",
      "Choose an amiibo dump (.bin) from the Amiibo directory to place on the reader.")
#endif

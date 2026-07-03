/* Single-source definitions: logging verbosity group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_UINT_EX_H
#define S_UINT_EX_H S_UINT_EX
#endif
#endif
/* Rows marked _EX carry the extended descriptor arguments
 * (callbacks and ui type); every pass except the menu table
 * drops them and behaves exactly like the base row. */
#ifndef S_BOOL_EX
#define S_BOOL_EX(f, T, n, d, sd, df, c, okx, rpx, stax, selx, lfx, rtx, uix, us, sub) S_BOOL(f, T, n, d, sd, df, c, us, sub)
#endif
#ifndef S_UINT_EX_H
#define S_UINT_EX_H(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, stax, selx, lfx, rtx, uix, us, sub) S_UINT_H(f, T, n, d, sd, df, c, mn, mx, st, ob, ok, rp, us, sub)
#endif
S_UINT_EX_H(libretro_log_level, LIBRETRO_LOG_LEVEL,
      "libretro_log_level",
      DEFAULT_LIBRETRO_LOG_LEVEL, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 3, 1.0, 0, setting_action_ok_uint, setting_get_string_representation_uint_libretro_log_level, NULL, NULL, NULL, NULL, ST_UI_TYPE_UINT_RADIO_BUTTONS,
      "Core Logging Level",
      "Set log level for cores. If a log level issued by a core is below this value, it is ignored.")
S_BOOL_EX(log_to_file, LOG_TO_FILE,
      "log_to_file",
      DEFAULT_LOG_TO_FILE, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Log to File",
      "Redirect system event log messages to file. Requires 'Logging Verbosity' to be enabled.")
S_BOOL(log_to_file_timestamp, LOG_TO_FILE_TIMESTAMP,
      "log_to_file_timestamp",
      DEFAULT_LOG_TO_FILE_TIMESTAMP, SD_FLAG_NONE, 0, 0,
      "Timestamp Log Files",
      "When logging to file, redirect the output from each RetroArch session to a new timestamped file. If disabled, log is overwritten each time RetroArch is restarted.")

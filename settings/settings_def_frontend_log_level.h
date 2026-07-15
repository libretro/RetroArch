/* Single-source definitions: frontend logging verbosity.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_UINT_CH(frontend_log_level, FRONTEND_LOG_LEVEL,
      "frontend_log_level",
      DEFAULT_FRONTEND_LOG_LEVEL, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 3, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_libretro_log_level, ST_UI_TYPE_UINT_RADIO_BUTTONS, frontend_log_level_change_handler,
      "Frontend Logging Level",
      "Sets the log level for the frontend. If a log level set by the frontend is lower than this value, it is ignored.")

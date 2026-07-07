/* Single-source definitions: quit and restart action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Row referencing QUIT_RETROARCH; strings owned by another def file. */
#if !defined(SETTINGS_DEF_STRINGS_PASS) && !defined(SETTINGS_DEF_CONFIG_PASS) && !defined(SETTINGS_DEF_ENUM_PASS)
SDESC_ACTION_ROW_LV(QUIT_RETROARCH, RESTART_RETROARCH,
                     SD_FLAG_NONE, NULL, NULL, 0)
#endif

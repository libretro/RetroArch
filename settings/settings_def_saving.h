/* Single-source definitions: saving group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_BOOL_H
#define S_BOOL_H S_BOOL
#endif
#ifndef S_UINT_EX_H
#define S_UINT_EX_H S_UINT_EX
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_THREADS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_THREADS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX_H(autosave_interval, AUTOSAVE_INTERVAL,
      "autosave_interval",
      DEFAULT_AUTOSAVE_INTERVAL, SD_FLAG_CMD_APPLY_AUTO, (SDESC_FLG_HAS_RANGE | SDESC_FLG_ENFORCE_MIN), CMD_EVENT_AUTOSAVE_INIT, 0, 0, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_autosave_interval, NULL, NULL, NULL, NULL, 0,
      "Save File: SaveRAM Autosave Interval",
      "Automatically save the non-volatile SaveRAM at a regular interval (in seconds).")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_THREADS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_THREADS) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX_H(savestate_automatic_interval, SAVESTATE_AUTOMATIC_INTERVAL,
      "savestate_automatic_interval",
      DEFAULT_SAVESTATE_AUTOMATIC_INTERVAL, SD_FLAG_CMD_APPLY_AUTO, (SDESC_FLG_HAS_RANGE | SDESC_FLG_ENFORCE_MIN), 0, 0, 0, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_autosave_interval, NULL, NULL, NULL, NULL, 0,
      "Save State: Automatic Interval",
      "Automatically save a state at a regular interval (in seconds). Set to 0 to disable.")
#endif
S_BOOL_EX(savestate_auto_index, SAVESTATE_AUTO_INDEX,
      "savestate_auto_index",
      DEFAULT_SAVESTATE_AUTO_INDEX, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Save State: Increment Index Automatically",
      "Before making a save state, the save state index is automatically increased. When loading content, the index will be set to the highest existing index.")
S_UINT_EX(savestate_max_keep, SAVESTATE_MAX_KEEP,
      "savestate_max_keep",
      DEFAULT_SAVESTATE_MAX_KEEP, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 999, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Save State: Maximum Auto-Increment to Keep",
      "Limit the number of save states that will be created when 'Increment Index Automatically' is enabled. If limit is exceeded when saving a new state, the existing state with the lowest index will be deleted. A value of '0' means unlimited states will be recorded.")
/* Descriptor and configuration rows are #ifdef HAVE_BSV_MOVIE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_BSV_MOVIE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX(replay_auto_index, REPLAY_AUTO_INDEX,
      "replay_auto_index",
      DEFAULT_REPLAY_AUTO_INDEX, SD_FLAG_NONE, 0, 0, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Replay: Increment Index Automatically",
      "Before making a replay, the replay index is automatically increased. When loading content, the index will be set to the highest existing index.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_BSV_MOVIE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_BSV_MOVIE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX(replay_max_keep, REPLAY_MAX_KEEP,
      "replay_max_keep",
      DEFAULT_REPLAY_MAX_KEEP, SD_FLAG_NONE, SDESC_RANGE_MINMAX, 0, 0, 999, 1, 0, setting_action_ok_uint, NULL, NULL, NULL, NULL, NULL, 0,
      "Replay: Maximum Auto-Increment to Keep",
      "Limit the number of replays that will be created when 'Increment Index Automatically' is enabled. If limit is exceeded when recording a new replay, the existing replay with the lowest index will be deleted. A value of '0' means unlimited replays will be recorded.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_BSV_MOVIE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_BSV_MOVIE) || defined(SETTINGS_DEF_STRINGS_PASS)
S_UINT_EX_H(replay_checkpoint_interval, REPLAY_CHECKPOINT_INTERVAL,
      "replay_checkpoint_interval",
      DEFAULT_REPLAY_CHECKPOINT_INTERVAL, SD_FLAG_CMD_APPLY_AUTO, (SDESC_FLG_HAS_RANGE | SDESC_FLG_ENFORCE_MIN), 0, 0, 0, 1, 0, setting_action_ok_uint, setting_get_string_representation_uint_replay_checkpoint_interval, NULL, NULL, NULL, NULL, 0,
      "Replay: Checkpoint Interval",
      "Automatically bookmark the game state during replay recording at a regular interval (in seconds).")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_BSV_MOVIE; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_BSV_MOVIE) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration row lives under defined(HAVE_BSV_MOVIE); other passes are
 * unaffected. */
#if !defined(SETTINGS_DEF_CONFIG_PASS) || (defined(HAVE_BSV_MOVIE))
S_BOOL_H(replay_checkpoint_deserialize, REPLAY_CHECKPOINT_DESERIALIZE,
      "replay_checkpoint_deserialize",
      DEFAULT_REPLAY_CHECKPOINT_DESERIALIZE, SD_FLAG_NONE, 0, 0,
      "Replay: Checkpoint Deserialize",
      "Whether to deserialize checkpoints stored in replays during regular playback.")
#endif
#endif
S_BOOL(content_runtime_log, CONTENT_RUNTIME_LOG,
      "content_runtime_log",
      DEFAULT_CONTENT_RUNTIME_LOG, SD_FLAG_NONE, 0, 0,
      "Save Runtime Log (Per Core)",
      "Keep track of how long each item of content has run for, with records separated by core.")
S_BOOL(content_runtime_log_aggregate, CONTENT_RUNTIME_LOG_AGGREGATE,
      "content_runtime_log_aggregate",
      DEFAULT_CONTENT_RUNTIME_LOG_AGGREGATE, SD_FLAG_NONE, 0, 0,
      "Save Runtime Log (Aggregate)",
      "Keep track of how long each item of content has run for, recorded as the aggregate total across all cores.")
/* Descriptor and configuration rows are #if defined(HAVE_ZLIB); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_ZLIB) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(save_file_compression, SAVE_FILE_COMPRESSION,
      "save_file_compression",
      DEFAULT_SAVE_FILE_COMPRESSION, SD_FLAG_ADVANCED, 0, 0,
      "Save File: Compression",
      "Write non-volatile SaveRAM files in an archived format. Dramatically reduces file size at the expense of (negligibly) increased saving/loading times.\nOnly applies to cores that enable saving via the standard libretro SaveRAM interface.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_ZLIB); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_ZLIB) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(savestate_file_compression, SAVESTATE_FILE_COMPRESSION,
      "savestate_file_compression",
      DEFAULT_SAVESTATE_FILE_COMPRESSION, SD_FLAG_ADVANCED, 0, 0,
      "Save State: Compression",
      "Write save state files in an archived format. Dramatically reduces file size at the expense of increased saving/loading times.")
#endif

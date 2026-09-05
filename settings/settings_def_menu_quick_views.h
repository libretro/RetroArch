/* Single-source definitions: quick menu visibility group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(quick_menu_show_take_screenshot, QUICK_MENU_SHOW_TAKE_SCREENSHOT,
      "quick_menu_show_take_screenshot",
      DEFAULT_QUICK_MENU_SHOW_TAKE_SCREENSHOT, SD_FLAG_NONE, 0, 0,
      "Show 'Take Screenshot'",
      "Show the 'Take Screenshot' option.")
S_BOOL(quick_menu_show_savestate_submenu, QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
      "quick_menu_show_savestate_submenu",
      DEFAULT_QUICK_MENU_SHOW_SAVESTATE_SUBMENU, SD_FLAG_NONE, 0, 0,
      "Show 'Save States' Submenu",
      "Show save state options in a submenu.")
S_BOOL(quick_menu_show_save_load_state, QUICK_MENU_SHOW_SAVE_LOAD_STATE,
      "quick_menu_show_save_load_state",
      DEFAULT_QUICK_MENU_SHOW_SAVE_LOAD_STATE, SD_FLAG_NONE, 0, 0,
      "Show 'Save/Load State'",
      "Show the options for saving/loading a state.")
S_BOOL(quick_menu_show_replay, QUICK_MENU_SHOW_REPLAY,
      "quick_menu_show_replay",
      DEFAULT_QUICK_MENU_SHOW_REPLAY, SD_FLAG_NONE, 0, 0,
      "Show 'Replay Controls'",
      "Show the options for recording/playing back replay files.")
S_BOOL(quick_menu_show_undo_save_load_state, QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
      "quick_menu_show_undo_save_load_state",
      DEFAULT_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE, SD_FLAG_NONE, 0, 0,
      "Show 'Undo Save/Load State'",
      "Show the options for undoing save/load state. RetroPad Start triggers save/load undo when hidden.")
S_BOOL(quick_menu_show_add_to_favorites, QUICK_MENU_SHOW_ADD_TO_FAVORITES,
      "quick_menu_show_add_to_favorites",
      DEFAULT_QUICK_MENU_SHOW_ADD_TO_FAVORITES, SD_FLAG_NONE, 0, 0,
      "Show 'Add to Favorites'",
      "Show the 'Add to Favorites' option.")
S_BOOL(quick_menu_show_add_to_playlist, QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
      "quick_menu_show_add_to_playlist",
      DEFAULT_QUICK_MENU_SHOW_ADD_TO_PLAYLIST, SD_FLAG_NONE, 0, 0,
      "Show 'Add to Playlist'",
      "Show the 'Add to Playlist' option.")
S_BOOL(quick_menu_show_start_recording, QUICK_MENU_SHOW_START_RECORDING,
      "quick_menu_show_start_recording",
      DEFAULT_QUICK_MENU_SHOW_START_RECORDING, SD_FLAG_NONE, 0, 0,
      "Show 'Start Recording'",
      "Show the 'Start Recording' option.")
S_BOOL(quick_menu_show_start_streaming, QUICK_MENU_SHOW_START_STREAMING,
      "quick_menu_show_start_streaming",
      DEFAULT_QUICK_MENU_SHOW_START_STREAMING, SD_FLAG_NONE, 0, 0,
      "Show 'Start Streaming'",
      "Show the 'Start Streaming' option.")
S_BOOL(quick_menu_show_set_core_association, QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
      "quick_menu_show_set_core_association",
      DEFAULT_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION, SD_FLAG_NONE, 0, 0,
      "Show 'Set Core Association'",
      "Show the 'Set Core Association' option when content is not running.")
S_BOOL(quick_menu_show_reset_core_association, QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
      "quick_menu_show_reset_core_association",
      DEFAULT_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION, SD_FLAG_NONE, 0, 0,
      "Show 'Reset Core Association'",
      "Show the 'Reset Core Association' option when content is not running.")
S_BOOL(quick_menu_show_resume_content, QUICK_MENU_SHOW_RESUME_CONTENT,
      "quick_menu_show_resume_content",
      DEFAULT_QUICK_MENU_SHOW_RESUME_CONTENT, SD_FLAG_NONE, 0, 0,
      "Show 'Resume'",
      "Show the resume content option.")
S_BOOL(quick_menu_show_restart_content, QUICK_MENU_SHOW_RESTART_CONTENT,
      "quick_menu_show_restart_content",
      DEFAULT_QUICK_MENU_SHOW_RESTART_CONTENT, SD_FLAG_NONE, 0, 0,
      "Show 'Reset'",
      "Show the reset content option.")
S_BOOL(quick_menu_show_close_content, QUICK_MENU_SHOW_CLOSE_CONTENT,
      "quick_menu_show_close_content",
      DEFAULT_QUICK_MENU_SHOW_CLOSE_CONTENT, SD_FLAG_NONE, 0, 0,
      "Show 'Close Content'",
      "Show the close content option.")
S_BOOL(quick_menu_show_options, QUICK_MENU_SHOW_OPTIONS,
      "quick_menu_show_options",
      DEFAULT_QUICK_MENU_SHOW_CORE_OPTIONS, SD_FLAG_NONE, 0, 0,
      "Show 'Core Options'",
      "Show the 'Core Options' option.")
S_BOOL(quick_menu_show_core_options_flush, QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
      "quick_menu_show_core_options_flush",
      DEFAULT_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH, SD_FLAG_NONE, 0, 0,
      "Show 'Flush Options to Disk'",
      "Show the 'Flush Options to Disk' entry in the 'Options > Manage Core Options' menu.")
S_BOOL(quick_menu_show_controls, QUICK_MENU_SHOW_CONTROLS,
      "quick_menu_show_controls",
      DEFAULT_QUICK_MENU_SHOW_CONTROLS, SD_FLAG_NONE, 0, 0,
      "Show 'Controls'",
      "Show the 'Controls' option.")
S_BOOL(quick_menu_show_cheats, QUICK_MENU_SHOW_CHEATS,
      "quick_menu_show_cheats",
      DEFAULT_QUICK_MENU_SHOW_CHEATS, SD_FLAG_NONE, 0, 0,
      "Show 'Cheats'",
      "Show the 'Cheats' option.")

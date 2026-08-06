/* Single-source definitions: custom window size setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if !((defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) || (defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH))); the string
 * tables always carry this row via the strings pass. */
#if (!((defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) || (defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH)))) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL_EX(video_window_custom_size_enable, VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
      "video_window_custom_size_enable",
      DEFAULT_WINDOW_CUSTOM_SIZE_ENABLE, SD_FLAG_NONE, 0, CMD_EVENT_REINIT, setting_bool_action_left_with_refresh, NULL, NULL, NULL, setting_bool_action_left_with_refresh, setting_bool_action_right_with_refresh, 0,
      "Use Custom Window Size",
      "Show all content in a fixed size window of dimensions specified by 'Window Width' and 'Window Height'. When disabled, window size will be set dynamically based on 'Windowed Scale'.")
#endif

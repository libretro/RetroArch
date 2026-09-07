/* Single-source definitions: settings-view visibility group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_BOOL(settings_show_drivers, SETTINGS_SHOW_DRIVERS,
      "settings_show_drivers",
      DEFAULT_SETTINGS_SHOW_DRIVERS, SD_FLAG_NONE, 0, 0,
      "Show 'Drivers'",
      "Show 'Drivers' settings.")
S_BOOL(settings_show_video, SETTINGS_SHOW_VIDEO,
      "settings_show_video",
      DEFAULT_SETTINGS_SHOW_VIDEO, SD_FLAG_NONE, 0, 0,
      "Show 'Video'",
      "Show 'Video' settings.")
S_BOOL(settings_show_audio, SETTINGS_SHOW_AUDIO,
      "settings_show_audio",
      DEFAULT_SETTINGS_SHOW_AUDIO, SD_FLAG_NONE, 0, 0,
      "Show 'Audio'",
      "Show 'Audio' settings.")
S_BOOL(settings_show_input, SETTINGS_SHOW_INPUT,
      "settings_show_input",
      DEFAULT_SETTINGS_SHOW_INPUT, SD_FLAG_NONE, 0, 0,
      "Show 'Input'",
      "Show 'Input' settings.")
S_BOOL(settings_show_latency, SETTINGS_SHOW_LATENCY,
      "settings_show_latency",
      DEFAULT_SETTINGS_SHOW_LATENCY, SD_FLAG_NONE, 0, 0,
      "Show 'Latency'",
      "Show 'Latency' settings.")
S_BOOL(settings_show_core, SETTINGS_SHOW_CORE,
      "settings_show_core",
      DEFAULT_SETTINGS_SHOW_CORE, SD_FLAG_NONE, 0, 0,
      "Show 'Core'",
      "Show 'Core' settings.")
S_BOOL(settings_show_configuration, SETTINGS_SHOW_CONFIGURATION,
      "settings_show_configuration",
      DEFAULT_SETTINGS_SHOW_CONFIGURATION, SD_FLAG_NONE, 0, 0,
      "Show 'Configuration'",
      "Show 'Configuration' settings.")
S_BOOL(settings_show_saving, SETTINGS_SHOW_SAVING,
      "settings_show_saving",
      DEFAULT_SETTINGS_SHOW_SAVING, SD_FLAG_NONE, 0, 0,
      "Show 'Saving'",
      "Show 'Saving' settings.")
S_BOOL(settings_show_logging, SETTINGS_SHOW_LOGGING,
      "settings_show_logging",
      DEFAULT_SETTINGS_SHOW_LOGGING, SD_FLAG_NONE, 0, 0,
      "Show 'Logging'",
      "Show 'Logging' settings.")
S_BOOL(settings_show_file_browser, SETTINGS_SHOW_FILE_BROWSER,
      "settings_show_file_browser",
      DEFAULT_SETTINGS_SHOW_FILE_BROWSER, SD_FLAG_NONE, 0, 0,
      "Show 'File Browser'",
      "Show 'File Browser' settings.")
S_BOOL(settings_show_frame_throttle, SETTINGS_SHOW_FRAME_THROTTLE,
      "settings_show_frame_throttle",
      DEFAULT_SETTINGS_SHOW_FRAME_THROTTLE, SD_FLAG_NONE, 0, 0,
      "Show 'Frame Throttle'",
      "Show 'Frame Throttle' settings.")
S_BOOL(settings_show_recording, SETTINGS_SHOW_RECORDING,
      "settings_show_recording",
      DEFAULT_SETTINGS_SHOW_RECORDING, SD_FLAG_NONE, 0, 0,
      "Show 'Recording'",
      "Show 'Recording' settings.")
S_BOOL(settings_show_onscreen_display, SETTINGS_SHOW_ONSCREEN_DISPLAY,
      "settings_show_onscreen_display",
      DEFAULT_SETTINGS_SHOW_ONSCREEN_DISPLAY, SD_FLAG_NONE, 0, 0,
      "Show 'On-Screen Display'",
      "Show 'On-Screen Display' settings.")
S_BOOL(settings_show_user_interface, SETTINGS_SHOW_USER_INTERFACE,
      "settings_show_user_interface",
      DEFAULT_SETTINGS_SHOW_USER_INTERFACE, SD_FLAG_NONE, 0, 0,
      "Show 'User Interface'",
      "Show 'User Interface' settings.")
S_BOOL(settings_show_ai_service, SETTINGS_SHOW_AI_SERVICE,
      "settings_show_ai_service",
      DEFAULT_SETTINGS_SHOW_AI_SERVICE, SD_FLAG_NONE, 0, 0,
      "Show 'AI Service'",
      "Show 'AI Service' settings.")
S_BOOL(settings_show_accessibility, SETTINGS_SHOW_ACCESSIBILITY,
      "settings_show_accessibility",
      DEFAULT_SETTINGS_SHOW_ACCESSIBILITY, SD_FLAG_NONE, 0, 0,
      "Show 'Accessibility'",
      "Show 'Accessibility' settings.")
S_BOOL(settings_show_power_management, SETTINGS_SHOW_POWER_MANAGEMENT,
      "settings_show_power_management",
      DEFAULT_SETTINGS_SHOW_POWER_MANAGEMENT, SD_FLAG_NONE, 0, 0,
      "Show 'Power Management'",
      "Show 'Power Management' settings.")
S_BOOL(settings_show_achievements, SETTINGS_SHOW_ACHIEVEMENTS,
      "settings_show_achievements",
      DEFAULT_SETTINGS_SHOW_ACHIEVEMENTS, SD_FLAG_NONE, 0, 0,
      "Show 'Achievements'",
      "Show 'Achievements' settings.")
S_BOOL(settings_show_network, SETTINGS_SHOW_NETWORK,
      "settings_show_network",
      DEFAULT_SETTINGS_SHOW_NETWORK, SD_FLAG_NONE, 0, 0,
      "Show 'Network'",
      "Show 'Network' settings.")
S_BOOL(settings_show_playlists, SETTINGS_SHOW_PLAYLISTS,
      "settings_show_playlists",
      DEFAULT_SETTINGS_SHOW_PLAYLISTS, SD_FLAG_NONE, 0, 0,
      "Show 'Playlists'",
      "Show 'Playlists' settings.")
S_BOOL(settings_show_user, SETTINGS_SHOW_USER,
      "settings_show_user",
      DEFAULT_SETTINGS_SHOW_USER, SD_FLAG_NONE, 0, 0,
      "Show 'User'",
      "Show 'User' settings.")
S_BOOL(settings_show_directory, SETTINGS_SHOW_DIRECTORY,
      "settings_show_directory",
      DEFAULT_SETTINGS_SHOW_DIRECTORY, SD_FLAG_NONE, 0, 0,
      "Show 'Directory'",
      "Show 'Directory' settings.")

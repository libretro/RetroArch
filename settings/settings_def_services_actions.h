/* Single-source definitions: services actions.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION_EX(NETWORK_SETTINGS,
      "network_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Network",
      "Change server and network settings.")
/* Descriptor and configuration rows are #ifdef HAVE_LAKKA; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LAKKA) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_NS(LAKKA_SERVICES,
      "lakka_services",
      "Services")
#endif
#ifdef HAVE_LAKKA_SWITCH
S_ACTION(LAKKA_SWITCH_OPTIONS,
      "Switch_Options",
      "Nintendo Switch Options",
      "Manage Nintendo Switch Specific Options.")
#endif
S_ACTION_EX(PLAYLIST_SETTINGS,
      "playlist_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Playlists",
      "Change playlist settings.")
S_ACTION(USER_SETTINGS,
      "user_settings",
      "User",
      "Change privacy, account and username settings.")
S_ACTION(DIRECTORY_SETTINGS,
      "directory_settings",
      "Directory",
      "Change default directories where files are located.")
S_ACTION(PRIVACY_SETTINGS,
      "privacy_settings",
      "Privacy",
      "Change privacy settings.")
S_ACTION(AUDIO_OUTPUT_SETTINGS,
      "audio_output_settings",
      "Output",
      "Change audio output settings.")
#ifdef HAVE_MICROPHONE
S_ACTION(MICROPHONE_SETTINGS,
      "microphone_settings",
      "Microphone",
      "Change audio input settings.")
#endif
S_ACTION(AUDIO_SYNCHRONIZATION_SETTINGS,
      "audio_synchronization_settings",
      "Synchronization",
      "Change audio synchronization settings.")
#ifdef HAVE_MIST
S_ACTION(STEAM_SETTINGS,
      "steam_settings",
      "Steam",
      "Change settings related to Steam.")
#endif

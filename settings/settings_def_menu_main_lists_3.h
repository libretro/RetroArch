/* Single-source definitions: third main menu list actions.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

#ifdef HAVE_LIBNX
S_ACTION(SWITCH_CPU_PROFILE,
      "switch_cpu_profile",
      "CPU Overclock",
      "Overclock the Switch CPU.")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_LAKKA); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LAKKA) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX_NS(REBOOT,
      "reboot", SD_FLAG_NONE, NULL, NULL, CMD_EVENT_REBOOT,
      "Reboot")
#endif
/* Descriptor and configuration rows are #if defined(HAVE_LAKKA); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_LAKKA) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX_NS(SHUTDOWN,
      "shutdown", SD_FLAG_NONE, NULL, NULL, CMD_EVENT_SHUTDOWN,
      "Shutdown")
#endif
S_ACTION_EX(DRIVER_SETTINGS,
      "driver_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Drivers",
      "Change drivers used by the system.")
S_ACTION(VIDEO_SETTINGS,
      "video_settings",
      "Video",
      "Change video output settings.")
S_ACTION(CRT_SWITCHRES_SETTINGS,
      "crt_switchres_settings",
      "CRT SwitchRes",
      "Output native, low-resolution signals for use with CRT displays.")
S_ACTION(VIDEO_OUTPUT_SETTINGS,
      "video_output_settings",
      "Output",
      "Change video output settings.")
S_ACTION(AUDIO_SETTINGS,
      "audio_settings",
      "Audio",
      "Change audio input/output settings.")
/* Descriptor and configuration rows are #ifdef HAVE_AUDIOMIXER; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_AUDIOMIXER) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(AUDIO_MIXER_SETTINGS,
      "audio_mixer_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Mixer",
      "Change audio mixer settings.")
#endif
/* Descriptor and configuration rows are #ifdef HAVE_AUDIOMIXER; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_AUDIOMIXER) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION(MENU_SOUNDS,
      "menu_sounds",
      "Menu Sounds",
      "Change menu sound settings.")
#endif
S_ACTION(INPUT_SETTINGS,
      "input_settings",
      "Input",
      "Change controller, keyboard, and mouse settings.")
S_ACTION_EX(LATENCY_SETTINGS,
      "latency_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Latency",
      "Change settings related to video, audio and input latency.")
S_ACTION(CORE_SETTINGS,
      "core_settings",
      "Core",
      "Change core settings.")
S_ACTION_EX(CONFIGURATION_SETTINGS,
      "configuration_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Configuration",
      "Change default settings for configuration files.")
S_ACTION_EX(SAVING_SETTINGS,
      "saving_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Saving",
      "Change saving settings.")
S_ACTION(LOGGING_SETTINGS,
      "logging_settings",
      "Logging",
      "Change logging settings.")
S_ACTION_EX(FRAME_THROTTLE_SETTINGS,
      "frame_throttle_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Frame Throttle",
      "Change rewind, fast-forward, and slow-motion settings.")
S_ACTION(REWIND_SETTINGS,
      "rewind_settings",
      "Rewind",
      "Change rewind settings.")
S_ACTION_EX(FRAME_TIME_COUNTER_SETTINGS,
      "frame_time_counter_settings", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Frame Time Counter",
      "Change settings influencing the frame time counter. Only active when threaded video is disabled.")
S_ACTION_NS(CHEAT_DETAILS_SETTINGS,
      "cheat_details_settings",
      "Cheat Details")
S_ACTION_NS(CHEAT_SEARCH_SETTINGS,
      "cheat_search_settings",
      "Start or Continue Cheat Search")

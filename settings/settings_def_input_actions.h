/* Single-source definitions: input action group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION(INPUT_HOTKEY_BINDS,
      "input_hotkey_binds",
      "Hotkeys",
      "Change settings and assignments for hotkeys, such as toggling the menu during gameplay.")
S_ACTION(INPUT_RETROPAD_BINDS,
      "input_retropad_binds",
      "RetroPad Binds",
      "Change how the virtual RetroPad is mapped to a physical input device. If an input device is recognized and autoconfigured correctly, users probably do not need to use this menu.\nNote: for core-specific input changes, use the Quick Menu's 'Controls' submenu instead.")
S_ACTION(INPUT_TURBO_FIRE_SETTINGS,
      "input_turbo_fire_settings",
      "Turbo Fire",
      "Change turbo fire settings.")

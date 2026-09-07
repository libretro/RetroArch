/* Single-source definitions: cheat auto-apply group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* config key "apply_cheats_after_load" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(apply_cheats_after_load, CHEAT_APPLY_AFTER_LOAD,
      "cheat_apply_after_load",
      DEFAULT_APPLY_CHEATS_AFTER_LOAD, SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Auto-Apply Cheats During Game Load",
      "Auto-apply cheats when game loads.")
#endif
/* config key "apply_cheats_after_toggle" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(apply_cheats_after_toggle, CHEAT_APPLY_AFTER_TOGGLE,
      "cheat_apply_after_toggle",
      DEFAULT_APPLY_CHEATS_AFTER_TOGGLE, SD_FLAG_CMD_APPLY_AUTO, 0, 0,
      "Apply After Toggle",
      "Apply cheat immediately after toggling.")
#endif

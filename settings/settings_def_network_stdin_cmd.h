/* Single-source definitions: stdin command setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #if defined(HAVE_NETWORKING) #if defined(HAVE_NETWORK_CMD); the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORK_CMD) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "stdin_cmd_enable" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_BOOL(stdin_cmd_enable, STDIN_CMD_ENABLE,
      "stdin_commands",
      DEFAULT_STDIN_CMD_ENABLE, SD_FLAG_ADVANCED, 0, 0,
      "stdin Commands",
      "stdin command interface.")
#endif
#endif

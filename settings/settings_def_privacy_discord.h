/* Single-source definitions: Discord presence setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_DISCORD; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_DISCORD) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(discord_enable, DISCORD_ALLOW,
      "discord_allow",
      false, SD_FLAG_NONE, 0, 0,
      "Discord Rich Presence",
      "Allow the Discord app to show data about the content played. Only available with the native desktop client.")
#endif

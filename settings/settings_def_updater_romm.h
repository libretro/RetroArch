/* Single-source definitions: RomM library source settings.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration.c row of a string stays literal. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(romm_url, ROMM_URL,
      "romm_url",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "RomM Server URL",
      "Address of your RomM server, for example https://romm.example.com. Its library appears under Online Updater.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
/* The configuration.c row of a string stays literal. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(romm_token, ROMM_TOKEN,
      "romm_token",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, setting_get_string_representation_password, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "RomM API Token",
      "A RomM client API token with permission to read ROMs and platforms. Create one in the RomM web interface. It goes with every request, so use an https:// address unless you trust the network.")
#endif
#endif

/* Single-source definitions: achievements account group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "cheevos_username" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_LV(cheevos_username, CHEEVOS_USERNAME, ACCOUNTS_CHEEVOS_USERNAME,
      "cheevos_username",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Username",
      "Input your RetroAchievements account username.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CHEEVOS; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CHEEVOS) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "cheevos_password" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING_LV(cheevos_password, CHEEVOS_PASSWORD, ACCOUNTS_CHEEVOS_PASSWORD,
      "cheevos_password",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, setting_get_string_representation_password, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Password",
      "Input the password of your RetroAchievements account. Max length: 255 characters.")
#endif
#endif

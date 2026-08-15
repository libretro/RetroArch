/* Single-source definitions: ScreenScraper account group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Rows marked _H reserve a MENU_ENUM_LABEL_HELP_ enum member;
 * outside the enum pass they behave exactly like the base row. */
#ifndef SETTINGS_DEF_ENUM_PASS
#ifndef S_ACTION_H
#define S_ACTION_H S_ACTION
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_H(ACCOUNTS_SCREENSCRAPER,
      "accounts_screenscraper",
      "ScreenScraper",
      "Sign in to screenscraper.fr. While signed in, ScreenScraper becomes the primary source for playlist thumbnails and metadata.")
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(screenscraper_username, SCREENSCRAPER_USERNAME,
      "screenscraper_username",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Username",
      "Input your screenscraper.fr account username. A registered account raises the daily scraping quota.")
#endif
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(screenscraper_password, SCREENSCRAPER_PASSWORD,
      "screenscraper_password",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, setting_get_string_representation_password, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Password",
      "Input the password of your screenscraper.fr account.")
#endif
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(screenscraper_devid, SCREENSCRAPER_DEVID,
      "screenscraper_devid",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Developer ID",
      "Developer credentials for the ScreenScraper API. Required; request them on the screenscraper.fr forum.")
#endif
#endif
#if defined(HAVE_NETWORKING) || defined(SETTINGS_DEF_STRINGS_PASS)
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(screenscraper_devpassword, SCREENSCRAPER_DEVPASSWORD,
      "screenscraper_devpassword",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, setting_get_string_representation_password, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Developer Password",
      "Developer password for the ScreenScraper API.")
#endif
#endif

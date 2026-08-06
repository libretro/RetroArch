/* Single-source definitions: WebDAV credentials group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "webdav_url" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(webdav_url, CLOUD_SYNC_URL,
      "cloud_sync_url",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Cloud Storage URL",
      "The URL for the API entry point to the cloud storage service.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "webdav_username" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(webdav_username, CLOUD_SYNC_USERNAME,
      "cloud_sync_username",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Username",
      "Your username for your cloud storage account.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "webdav_password" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(webdav_password, CLOUD_SYNC_PASSWORD,
      "cloud_sync_password",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, setting_get_string_representation_password, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Password",
      "Your password for your cloud storage account.")
#endif
#endif

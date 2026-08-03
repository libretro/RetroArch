/* Single-source definitions: S3 credentials group.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC #ifdef HAVE_S3; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) && defined(HAVE_S3) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "s3_url" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(s3_url, CLOUD_SYNC_S3_URL,
      "cloud_sync_s3_url",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "S3 URL",
      "Your S3 endpoint URL for cloud storage.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC #ifdef HAVE_S3; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) && defined(HAVE_S3) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "access_key_id" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(access_key_id, CLOUD_SYNC_ACCESS_KEY_ID,
      "cloud_sync_access_key_id",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, NULL, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_STRING_LINE_EDIT,
      "Access Key ID",
      "Your access key ID for your cloud storage account.")
#endif
#endif
/* Descriptor and configuration rows are #ifdef HAVE_CLOUDSYNC #ifdef HAVE_S3; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_CLOUDSYNC) && defined(HAVE_S3) || defined(SETTINGS_DEF_STRINGS_PASS)
/* config key "secret_access_key" differs from the label string; the
 * configuration.c row stays literal for this setting. */
#ifndef SETTINGS_DEF_CONFIG_PASS
S_STRING(secret_access_key, CLOUD_SYNC_SECRET_ACCESS_KEY,
      "cloud_sync_secret_access_key",
      "", SD_FLAG_ALLOW_INPUT, 0, NULL, setting_get_string_representation_password, setting_generic_action_start_default, NULL, NULL, NULL, ST_UI_TYPE_PASSWORD_LINE_EDIT,
      "Secret Access Key",
      "Your secret access key for your cloud storage account.")
#endif
#endif

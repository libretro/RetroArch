/* Single-source definitions: user language action.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

S_ACTION_EX(ACCOUNTS_LIST,
      "accounts_list", SD_FLAG_LAKKA_ADVANCED, NULL, NULL, 0,
      "Accounts",
      "Manage currently configured accounts.")
#if (defined(HAVE_KEYCHAIN) && defined(HAVE_CRYPTO) && defined(HAVE_CONFIGFILE)) || defined(SETTINGS_DEF_STRINGS_PASS)
S_ACTION_EX(KEYCHAIN_PASSPHRASE,
      "keychain_passphrase", SD_FLAG_NONE, setting_action_keychain_passphrase, NULL, 0,
      "Keychain Passphrase",
      "Protect the key of the keychain that holds saved passwords with a passphrase, so they can be opened again when the configuration moves to another machine. After such a move, enter the passphrase here once. Leave it empty to remove the passphrase.")
#endif

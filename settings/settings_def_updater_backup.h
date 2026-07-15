/* Single-source definitions: core backup setting.
 * Grammar identical to settings_def_video_sync.h plus S_FLOAT and
 * the _NS no-sublabel variants; the descriptor argument span
 * matches SDESC_<kind>_ROW; row order is menu display order;
 * h2json.py parses these rows for the Crowdin source upload. */

/* Descriptor and configuration rows are #ifdef HAVE_NETWORKING #ifdef HAVE_UPDATE_CORES; the string
 * tables always carry this row via the strings pass. */
#if defined(HAVE_NETWORKING) && defined(HAVE_UPDATE_CORES) || defined(SETTINGS_DEF_STRINGS_PASS)
S_BOOL(core_updater_auto_backup, CORE_UPDATER_AUTO_BACKUP,
      "core_updater_auto_backup",
      DEFAULT_CORE_UPDATER_AUTO_BACKUP, SD_FLAG_NONE, 0, 0,
      "Backup Cores When Updating",
      "Automatically create a backup of any installed cores when performing an online update. Enables easy rollback to a working core if an update introduces a regression.")
#endif

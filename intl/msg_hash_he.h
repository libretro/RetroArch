#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

/* Top-Level Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN_MENU,
   "תפריט ראשי"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "הגדרות"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "מועדפים"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "היסטוריה"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "מוזיקה"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "ייבוא תוכן"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "תפריט מהיר"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "גישה מהירה להגדרות משחק."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "טעינת תוכן"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "סריקת תוכן"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "הצג תפריט שולחן עבודה"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "בטל מצב קיוסק"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "עדכון מקוון"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "הגדרות"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "שינוי הגדרות יישום."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "מידע נוסף"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "מידע מערכת."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "קובץ הגדרות"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "עזרה"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "אתחול RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "אתחול יישום."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "יציאה"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "יציאה."
   )

/* Main Menu > Load Core */


/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "הורדות"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "עיון בקובץ דחוס"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "טען קובץ דחוס"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "מועדפים"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "מוזיקה"
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "עדכון תמונות ממוזערות"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "הורדת תוכן"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "עדכן בסיסי נתונים"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "מידע מערכת"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "מנהל בסיס נתונים"
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "הרשאות"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "רישיון"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "קושחה"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING,
   "חסר"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT,
   "קיים"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPTIONAL,
   "אופציונלי"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REQUIRED,
   "נדרש"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "תאריך הגרסה"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "הידור (קימפול)‏"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "דגם מעבד"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "מאפייני מעבד"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "ארכיטקטורת מעבד:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "ליבות מעבד:"
   )

/* Main Menu > Information > Database Manager */


/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "סידורי"
   )

/* Main Menu > Configuration File */


/* Main Menu > Help */


/* Main Menu > Help > Basic Menu Controls */


/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "סייר קבצים"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "הקלטה"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "שינוי הגדרות הקלטה."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "ממשק משתמש"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "שירות תרגום"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "נגישות"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "ניהול צריכת החשמל"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "הישגים"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "רשת"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "משתמש"
   )

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "קלט"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "וידאו"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "בחר התקן תצוגה."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "שמע"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "בחר התקן שמע."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "מצלמה"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "בחר התקן מצלמה."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
   "רשת אלחוטית"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "שירותי מיקום"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "בחר התקן עבור שירותי מיקום."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "תצורת תפריט"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "בחר התקן תצורת תפריט."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "הקלט"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "בחר התקן הקלטה."
   )

/* Settings > Video */


/* Settings > Video > CRT SwitchRes */


/* Settings > Video > Output */


/* Settings > Video > Fullscreen Mode */


/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "רוחב חלון"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "גובה חלון"
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "יחס גובה-רוחב"
   )

/* Settings > Video > Synchronization */


/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "דגימה מחודשת"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "סינכרון"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "שנה הגדרות סנכון שמע."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "מערבל שמע (מיקסר)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "שנה הגדרות מערבל שמע (מיקסר)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "צלילי תפריט"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "השתק"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "השתק/הפעל שמע."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "השתק מערבל שמע (מיקסר)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "השתק/הפעל מערבל שמע (מיקסר)."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "שמע"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "התקן"
   )

/* Settings > Audio > Resampler */


/* Settings > Audio > Synchronization */


/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "עוצמת שמע"
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "הסר"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "עוצמת שמע"
   )

/* Settings > Audio > Menu Sounds */


/* Settings > Input */


/* Settings > Input > Haptic Feedback/Vibration */


/* Settings > Input > Menu Controls */


/* Settings > Input > Hotkey Binds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "אתחל את המשחק"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "שמור צילום מסך"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "שלח נתוני ניפוי תקלות"
   )

/* Settings > Input > Port # Binds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "סוג התקן"
   )

/* Settings > Latency */


/* Settings > Core */


/* Settings > Configuration */


/* Settings > Saving */


/* Settings > Logging */


/* Settings > File Browser */


/* Settings > Frame Throttle */


/* Settings > Frame Throttle > Rewind */


/* Settings > Frame Throttle > Frame Time Counter */


/* Settings > Recording */


/* Settings > On-Screen Display */


/* Settings > On-Screen Display > On-Screen Overlay */




/* Settings > On-Screen Display > Video Layout */


/* Settings > On-Screen Display > On-Screen Notifications */


/* Settings > User Interface */


/* Settings > User Interface > Views */


/* Settings > User Interface > Views > Quick Menu */


/* Settings > User Interface > Views > Settings */


/* Settings > User Interface > Appearance */


/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */


/* Settings > Network */


/* Settings > Network > Updater */


/* Settings > Playlists */


/* Settings > Playlists > Playlist Management */


/* Settings > User */


/* Settings > User > Privacy */


/* Settings > User > Accounts */


/* Settings > User > Accounts > RetroAchievements */


/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > Directory */


/* Music */

/* Music > Quick Menu */


/* Netplay */


/* Netplay > Host */


/* Import content */


/* Import content > Scan File */


/* Import content > Manual Scan */


/* Playlist > Playlist Item */


/* Playlist Item > Set Core Association */


/* Playlist Item > Information */


/* Quick Menu */


/* Quick Menu > Options */


/* Quick Menu > Controls */


/* Quick Menu > Controls > Load Remap File */


/* Quick Menu > Cheats */


/* Quick Menu > Cheats > Start or Continue Cheat Search */


/* Quick Menu > Cheats > Load Cheat File (Replace) */


/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */


/* Quick Menu > Disc Control */


/* Quick Menu > Shaders */


/* Quick Menu > Shaders > Save */


/* Quick Menu > Shaders > Remove */


/* Quick Menu > Shaders > Shader Parameters */


/* Quick Menu > Overrides */


/* Quick Menu > Achievements */


/* Quick Menu > Information */


/* Miscellaneous UI Items */


/* Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
   "Hebrew - עברית"
   )

/* RGUI: Settings > User Interface > Appearance */


/* RGUI: Settings Options */


/* XMB: Settings > User Interface > Appearance */


/* XMB: Settings Options */


/* Ozone: Settings > User Interface > Appearance */


/* MaterialUI: Settings > User Interface > Appearance */


/* MaterialUI: Settings Options */


/* Qt (Desktop Menu) */


/* Unsorted */


/* Unused (Only Exist in Translation Files) */


/* Unused (Needs Confirmation) */


/* Discord Status */


/* Notifications */


/* Lakka */


/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
   "רשת אלחוטית"
   )
#ifdef HAVE_LAKKA_SWITCH
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
#endif
#ifdef HAVE_LAKKA
#endif
#ifdef GEKKO
#endif
#ifdef HAVE_ODROIDGO2
#else
#endif
#if defined(_3DS)
#endif
#ifdef HAVE_QT
#endif

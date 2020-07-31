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
   "گزین فهرست"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "ساماندهی"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "برگزیده‌ها"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "پیشینه"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "موسیقی"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "فهرست سریع"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "فهرست‌های پخش"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "به‌روز کننده آنلاین"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "ساماندهی"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "برپاساختن برنامه."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "داده"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "راهنما"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "بازآغاز کردن برنامه."
   )

/* Main Menu > Load Core */


/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "بارگیری‌ها"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "برگزیده‌ها"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "موسیقی"
   )

/* Main Menu > Online Updater */


/* Main Menu > Information */


/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "نویسنده"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "دسترسی‌ها"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "گواهینامه"
   )

/* Main Menu > Information > System Information */


/* Main Menu > Information > Database Manager */


/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "نام"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "درباره"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "شاخه"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "پخش کننده"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "سازنده"
   )

/* Main Menu > Configuration File */


/* Main Menu > Help */


/* Main Menu > Help > Basic Menu Controls */


/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "ویدیو"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "کاربر"
   )

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "داده"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "ویدیو"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "صدا"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "دوربین"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
   "وای فای"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "جا"
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "برونداده"
   )

/* Settings > Video > CRT SwitchRes */


/* Settings > Video > Output */


/* Settings > Video > Fullscreen Mode */


/* Settings > Video > Windowed Mode */


/* Settings > Video > Scaling */


/* Settings > Video > Synchronization */


/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "برونداده"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "ساکت"
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "صدا"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "دستگاه"
   )

/* Settings > Audio > Resampler */


/* Settings > Audio > Synchronization */


/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "داده"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "برونداده"
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "پخش"
   )

/* Settings > Audio > Menu Sounds */


/* Settings > Input */


/* Settings > Input > Haptic Feedback/Vibration */


/* Settings > Input > Menu Controls */


/* Settings > Input > Hotkey Binds */


/* Settings > Input > Port # Binds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "دکمه گزینش"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "دکمه آغاز"
   )

/* Settings > Latency */


/* Settings > Core */

#ifndef HAVE_DYNAMIC
#endif

/* Settings > Configuration */


/* Settings > Saving */


/* Settings > Logging */


/* Settings > File Browser */


/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "برگرداندن"
   )

/* Settings > Frame Throttle > Rewind */


/* Settings > Frame Throttle > Frame Time Counter */


/* Settings > Recording */


/* Settings > On-Screen Display */


/* Settings > On-Screen Display > On-Screen Overlay */




/* Settings > On-Screen Display > Video Layout */


/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "اندازه آگاهساز"
   )

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

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "نام کاربری"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "زبان"
   )

/* Settings > User > Privacy */


/* Settings > User > Accounts */


/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "نام کاربری"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "گذرواژه"
   )

/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "فهرست‌های پخش"
   )

/* Music */

/* Music > Quick Menu */


/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "میزبان"
   )

/* Netplay > Host */


/* Import content */


/* Import content > Scan File */


/* Import content > Manual Scan */


/* Explore tab */

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "اجرا"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "تغییر نام"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "حذف"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "داده"
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "نام"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "واپسین بازی"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "پایگاه داده"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "ادامه"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "بازآغاز کردن"
   )

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
   MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS,
   "Afrikaans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN,
   "Albanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ARABIC,
   "Arabic - عربى (Restart Required)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ASTURIAN,
   "Asturian - Asturianu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI,
   "Azerbaijani"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BASQUE,
   "Basque"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BENGALI,
   "Bengali"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN,
   "Bulgarian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CATALAN,
   "Catalan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
   "Chinese (Simplified) - 简体中文 (Restart Required)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
   "Chinese (Traditional) - 繁體中文 (Restart Required)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CROATIAN,
   "Croatian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CZECH,
   "Czech"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_DANISH,
   "Danish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
   "Dutch - Nederlands"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
   "English"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
   "Esperanto - Esperanto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN,
   "Estonian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FILIPINO,
   "Filipino"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FINNISH,
   "Finnish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
   "French - Français"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GALICIAN,
   "Galician"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN,
   "Georgian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
   "German - Deutsch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GREEK,
   "Greek - Ελληνικά"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GUJARATI,
   "Gujarati"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE,
   "Haitian Creole"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
   "Hebrew"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HINDI,
   "Hindi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN,
   "Hungarian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC,
   "Icelandic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN,
   "Indonesian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_IRISH,
   "Irish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
   "Italian - Italiano"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
   "Japanese - 日本語"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_KANNADA,
   "Kannada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
   "Korean - 한국어 (Restart Required)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATIN,
   "Latin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATVIAN,
   "Latvian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN,
   "Lithuanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN,
   "Macedonian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALAY,
   "Malay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALTESE,
   "Maltese"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN,
   "Norwegian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_PERSIAN,
   "Persian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_POLISH,
   "Polish - Polski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL,
   "Portuguese (Brazil) - Português (Brasil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL,
   "Portuguese (Portugal) - Português (Portugal)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN,
   "Romanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
   "Russian - Русский"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SERBIAN,
   "Serbian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVAK,
   "Slovak - Slovenský"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN,
   "Slovenian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
   "Spanish - Español"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWAHILI,
   "Swahili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWEDISH,
   "Swedish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TAMIL,
   "Tamil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TELUGU,
   "Telugu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_THAI,
   "Thai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TURKISH,
   "Turkish - Türkçe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN,
   "Ukrainian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_URDU,
   "Urdu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE,
   "Vietnamese - Tiếng Việt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_WELSH,
   "Welsh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_YIDDISH,
   "Yiddish"
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

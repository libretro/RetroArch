#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

/*
##### NOTE FOR TRANSLATORS ####

PLEASE do NOT modify any `msg_hash_*.h` files, besides `msg_hash_us.h`!

Translations are handled using the localization platform Crowdin:
https://crowdin.com/project/retroarch

Translations from Crowdin are applied automatically and will overwrite
any changes made to the other localization files.
As a result, any submissions directly altering `msg_hash_*.h` files
other than `msg_hash_us.h` will be rejected.
*/

/* Top-Level Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "ସେଟିଂ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "ଇତିଵୃତ୍ତି"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "ସଙ୍ଗୀତ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "ଵିଡ଼ିଓ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "ଵିଷୟଵସ୍ତୁ ଆମଦାନୀ କରିବା"
   )

/* Main Menu */

#ifdef HAVE_LAKKA
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "ଚାଳନତାଲିକା"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "ଵିଷୟଵସ୍ତୁ ଆମଦାନୀ କରିବା"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "ସେଟିଂ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "ସୂଚନା"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "ସିଷ୍ଟମ୍ ସୂଚନା ଦର୍ଶାଇବା।"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "ସହାୟତା"
   )

/* Main Menu > Load Core */


/* Main Menu > Load Content */


/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "ସଙ୍ଗୀତ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "ଵିଡ଼ିଓ"
   )

/* Main Menu > Online Updater */


/* Main Menu > Information */


/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[ସ୍ୱତଃ]"
   )

/* Main Menu > Information > System Information */


/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "ଵର୍ଣ୍ଣନା"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "ଧରଣ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "ଭାଷା"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "ଅଞ୍ଚଳ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "ମିଡ଼ିଆ"
   )

/* Main Menu > Configuration File */


/* Main Menu > Help */

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "ସୂଚନା"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "ଵିଡ଼ିଓ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "ଅଡ଼ିଓ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "ଚାଳନତାଲିକା"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "ଉପଭୋକ୍ତା"
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "ମିଡ଼ିଆ"
   )

#ifdef HAVE_MIST
#endif

/* Settings > Drivers */


MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "ଵିଡ଼ିଓ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "ଅଡ଼ିଓ"
   )
#ifdef HAVE_MICROPHONE
#endif

/* Settings > Video */

#if defined(DINGUX)
#if defined(RS90) || defined(MIYOO)
#endif
#endif

/* Settings > Video > CRT SwitchRes */


/* Settings > Video > Output */

#if defined (WIIU)
#endif
#if defined(DINGUX) && defined(DINGUX_BETA)
#endif

/* Settings > Video > Fullscreen Mode */


/* Settings > Video > Windowed Mode */


/* Settings > Video > Scaling */

#if defined(DINGUX)
#endif
#if defined(RARCH_MOBILE)
#endif

/* Settings > Video > HDR */



/* Settings > Video > Synchronization */


/* Settings > Audio */

#ifdef HAVE_MICROPHONE
#endif

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "ଅଡ଼ିଓ"
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
#endif

/* Settings > Audio > Resampler */


/* Settings > Audio > Synchronization */


/* Settings > Audio > MIDI */


/* Settings > Audio > Mixer Settings > Mixer Stream */


/* Settings > Audio > Menu Sounds */


/* Settings > Input */

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
#endif
#ifdef ANDROID
#endif



/* Settings > Input > Haptic Feedback/Vibration */


/* Settings > Input > Menu Controls */


/* Settings > Input > Hotkeys */











/* Settings > Input > Port # Controls */


/* Settings > Latency */

#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
#endif

/* Settings > Core */

#ifndef HAVE_DYNAMIC
#endif
#ifdef HAVE_MIST







#endif
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


#if defined(ANDROID)
#endif

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */


/* Settings > On-Screen Display > On-Screen Notifications */


/* Settings > User Interface */

#ifdef _3DS
#endif

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "ସେଟିଂ"
   )
#ifdef HAVE_LAKKA
#endif

/* Settings > User Interface > Menu Item Visibility > Quick Menu */


/* Settings > User Interface > Views > Settings */



/* Settings > User Interface > Appearance */


/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */


/* Settings > Achievements > Appearance */


/* Settings > Achievements > Visibility */


/* Settings > Network */


/* Settings > Network > Updater */


/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "ଇତିଵୃତ୍ତି"
   )

/* Settings > Playlists > Playlist Management */


/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "ଭାଷା"
   )

/* Settings > User > Privacy */


/* Settings > User > Accounts */


/* Settings > User > Accounts > RetroAchievements */


/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "ଚାଳନତାଲିକା"
   )

#ifdef HAVE_MIST
/* Settings > Steam */



#endif

/* Music */

/* Music > Quick Menu */


/* Netplay */


/* Netplay > Host */


/* Import Content */


/* Import Content > Scan File */


/* Import Content > Manual Scan */


/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "ଅଞ୍ଚଳ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "ପ୍ରକାଶକ ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "ଧରଣ ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "ଵର୍ଗ ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "ଭାଷା ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "ଅଞ୍ଚଳ ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "ମିଡ଼ିଆ ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "ନିୟନ୍ତ୍ରଣ ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "ଉତ୍ପତ୍ତି ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "ଟ୍ୟାଗ୍ ଅନୁଯାୟୀ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "ସିଷ୍ଟମ୍ ନାମ ଅନୁଯାୟୀ"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "ସୂଚନା"
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */


/* Quick Menu */


/* Quick Menu > Options */


/* Quick Menu > Options > Manage Core Options */


/* Quick Menu > Controls */


/* Quick Menu > Controls > Manage Remap Files */


/* Quick Menu > Controls > Manage Remap Files > Load Remap File */


/* Quick Menu > Cheats */


/* Quick Menu > Cheats > Start or Continue Cheat Search */


/* Quick Menu > Cheats > Load Cheat File (Replace) */


/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "ଵର୍ଣ୍ଣନା"
   )

/* Quick Menu > Disc Control */


/* Quick Menu > Shaders */



/* Quick Menu > Shaders > Shader Parameters */


/* Quick Menu > Overrides */


/* Quick Menu > Achievements */


/* Quick Menu > Information */


/* Miscellaneous UI Items */


/* Settings Options */


/* RGUI: Settings > User Interface > Appearance */


/* RGUI: Settings Options */


/* XMB: Settings > User Interface > Appearance */


/* XMB: Settings Options */


/* Ozone: Settings > User Interface > Appearance */




/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "ଦିଗଚଳନ ଦଣ୍ଡିକା ଦେଖାଇବା"
   )

/* MaterialUI: Settings Options */


/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "ସୂଚନା"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "ସେଟିଂ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "ଚାଳନତାଲିକା"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "ସୂଚନା"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "ଇତିଵୃତ୍ତି"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "ଉପଭୋକ୍ତା"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "ସହାୟତା"
   )

/* Discord Status */


/* Notifications */


MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "ଵିଡ଼ିଓ ସତେଜ ହାର %s Hzକୁ ବଦାଳାଇ ଦିଆଗଲା।"
   )

/* Lakka */


/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "ପ୍ରଦର୍ଶନ ମୋଡ୍ ଚୟନ କରନ୍ତୁ (ପୁନଃଆରମ୍ଭ ଦରକାର ପଡ଼ିବ)"
   )
#ifdef HAVE_LIBNX
#endif
#ifdef HAVE_LAKKA
#ifdef HAVE_RETROFLAG
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAFESHUTDOWN_ENABLE,
#ifdef HAVE_RETROFLAG_RPI5
   "Retroflag Safe Shutdown"
#else
   "Retroflag Safe Shutdown (Reboot required)"
#endif
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAFESHUTDOWN_ENABLE,
#ifdef HAVE_RETROFLAG_RPI5
   "For use with compatible Retroflag case."
#else
   "For use with compatible Retroflag case. Reboot is required when changing."
#endif
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
#endif
#ifdef UDEV_TOUCH_SUPPORT
#endif
#ifdef HAVE_ODROIDGO2
#else
#endif
#ifdef _3DS
#endif
#ifdef HAVE_QT
#endif
#ifdef HAVE_GAME_AI





#endif
#ifdef HAVE_SMBCLIENT
#endif

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
   "Menu utama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Pengaturan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favorit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Riwayat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Gambar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Musik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Video"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Jelajahi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Impor Konten"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Menu Cepat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Akses dengan cepat semua setelan game yang terkait."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Muat Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Pilih core yang akan dipakai."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Muat konten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Pilih konten yang akan dijalankan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Muat Cakram"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Muat media cakram fisik. Pilih dahulu core (Muat Core) yang akan digunakan ke cakram."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Transfer dari Cakram"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Transfer data dari media cakram fisik ke penyimpanan internal. Data akan disimpan sebagai berkas image."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Daftar Putar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Konten terpindai cocok dengan database akan ditampilkan disini."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Impor Konten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Buat dan perbarui daftar putar dengan memindai konten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Tampilkan Menu Desktop"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Pembaruan Daring"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Unduh add-on, komponen, dan konten untuk RetroArch."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Gabung atau buat sesi netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Pengaturan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Atur konfigurasi program."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informasi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Tampilkan informasi sistem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Berkas Konfigurasi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Kelola dan buat berkas konfigurasi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Bantuan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Pelajari lebih lanjut cara kerja aplikasi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Jalankan Ulang RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Jalankan ulang program."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Tutup RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Tutup program."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Unduh Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Unduh dan pasang sebuah inti dari pembaruan daring."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Pasang atau Pulihkan Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Pasang atau pulihkan sebuah inti dari direktori 'Unduhan'."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Mulai Prosesor Video"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Mulai Remote RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Mulai Direktori"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Unduhan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Jelajahi Arsip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Muat Arsip"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favorit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Konten yang ditambahkan ke 'Favorit'' akan muncul disini."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Musik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Musik yang telah dimainkan sebelumnya akan muncul disini."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Gambar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Gambar yang pernah dilihat sebelumnya akan muncul disini."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Video yang pernah diputar sebelumnya akan muncul disini."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Jelajahi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Rambah semua konten yang sesuai dengan database lewat antarmuka pencarian konten tersortir"
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Pengunduh Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Perbarui core terpasang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Ganti dengan versi core yang tersedia di Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Ganti semua core terpasang secara manual atau core bawaan dengan core dari Play Store jika tersedia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Pengunduh Konten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Perbarui Info Berkas Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Perbarui Aset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Perbarui Profil Controler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Perbarui Cheat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Perbarui Database"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Perbarui Overlay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Perbarui Shader GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Perbarui Shader Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Perbarui Shader Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Informasi Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Informasi Diska"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Lihat informasi tentang diska yang dimasukkan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Informasi Jaringan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Informasi Sistem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Manajer Database"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Lihat Database."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Manajer Kursor"
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Nama Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Label Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Nama Sistem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Pengembang Sistem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Kategori"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Pembuat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Jenis Lisensi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Ekstensi Didukung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Syarat Minimal Grafis API"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING,
   "Tidak ada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT,
   "Ada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPTIONAL,
   "Opsional"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REQUIRED,
   "Wajib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Kunci Core Terpasang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Mencegah modifikasi core yang terpasang dan pembaruan versi core yang tidak diinginkan. Konten game tertentu (seperti ROM Arcade) membutuhkan versi core tertentu agar tetap dapat berjalan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Hapus Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Hapus core dari penyimpanan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Cadangkan Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Buat cadangan core yang terpasang saat ini."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Pulihkan Cadangan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Pasang versi core sebelumnya dari daftar core terarsip."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Hapus Cadangan"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Tanggal Pembuatan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Versi Git"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Model CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Lebar Layar (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Tinggi Layar (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Ukuran DPI"
   )

/* Main Menu > Information > Database Manager */


/* Main Menu > Information > Database Manager > Information */


/* Main Menu > Configuration File */


/* Main Menu > Help */


/* Main Menu > Help > Basic Menu Controls */


/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Driver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Ubah setelan driver sistem."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Ubah setelan luaran video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Masukan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latensi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Ubah setelan besar nilai tunda pada video, suara, dan nilai tunda masukan."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Ubah setelan core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Konfigurasi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Ubah setelan default pada berkas konfigurasi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Pencatatan Log"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Ubah setelan pencatatan log."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Perambah Berkas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Rekaman"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Ubah setelan rekaman."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Ubah setelan overlay tampilan, overlay keyboard, dan setelan notifikasi layar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Antarmuka Pengguna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Ubah setelan antarmuka pengguna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Layanan AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Ubah setelan Layanan AI (Terjemahan/TTS/Lainnya)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Aksesibilitas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Ubah setelan Aksesibilitas narator."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Pengelolaan Daya"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Ubah setelan pengelolaan daya."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Pencapaian"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Ubah setelan pencapaian."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Jaringan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Daftar Putar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Ubah setelan daftar putar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Pengguna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Ubah setelan akun, nama pengguna, dan bahasa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Direktori"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Ubah lokasi direktori berada."
   )

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Masukan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Driver masukan yang dipakai. Beberapa driver video memaksa aplikasi untuk mengganti driver masukan tertentu."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Ubah setelan luaran video."
   )
#if defined(DINGUX)
#endif

/* Settings > Video > CRT SwitchRes */


/* Settings > Video > Output */


/* Settings > Video > Fullscreen Mode */


/* Settings > Video > Windowed Mode */


/* Settings > Video > Scaling */

#if defined(DINGUX)
#endif

/* Settings > Video > Synchronization */


/* Settings > Audio */


/* Settings > Audio > Output */


/* Settings > Audio > Resampler */


/* Settings > Audio > Synchronization */


/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Masukan"
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */


/* Settings > Audio > Menu Sounds */


/* Settings > Input */

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
#endif

/* Settings > Input > Haptic Feedback/Vibration */

#if defined(DINGUX) && defined(HAVE_LIBSHAKE)
#endif

/* Settings > Input > Menu Controls */


/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Tutup RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Layanan AI"
   )

/* Settings > Input > Port # Controls */


/* Settings > Latency */


/* Settings > Core */

#ifndef HAVE_DYNAMIC
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

/* Settings > On-Screen Display > Video Layout */


/* Settings > On-Screen Display > On-Screen Notifications */


/* Settings > User Interface */


/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Menu Cepat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Pengaturan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Menu utama"
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */


/* Settings > User Interface > Views > Settings */


/* Settings > User Interface > Appearance */


/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Pencapaian"
   )

/* Settings > Network */


/* Settings > Network > Updater */


/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Riwayat"
   )

/* Settings > Playlists > Playlist Management */


/* Settings > User */


/* Settings > User > Privacy */


/* Settings > User > Accounts */


/* Settings > User > Accounts > RetroAchievements */


/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Unduhan"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Perambah Berkas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Daftar Putar"
   )

/* Music */

/* Music > Quick Menu */


/* Netplay */


/* Netplay > Host */


/* Import Content */


/* Import Content > Scan File */


/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Nama Sistem"
   )

/* Explore tab */

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informasi"
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */


/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Pencapaian"
   )

/* Quick Menu > Options */


/* Quick Menu > Options > Manage Core Options */


/* - Legacy (unused) */

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

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_MENU,
   "ToggleCheevosHardcore" /* not-displayed - needed to resolve submenu */
   )

/* Quick Menu > Information */


/* Miscellaneous UI Items */


/* Settings Options */


/* RGUI: Settings > User Interface > Appearance */


/* RGUI: Settings Options */


/* XMB: Settings > User Interface > Appearance */


/* XMB: Settings Options */


/* Ozone: Settings > User Interface > Appearance */


/* MaterialUI: Settings > User Interface > Appearance */


/* MaterialUI: Settings Options */


/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Pengaturan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Bantuan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Muat Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Daftar Putar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Perambah Berkas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informasi"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Riwayat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Pengguna"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Bantuan"
   )

/* Unused (Needs Confirmation) */


/* Discord Status */


/* Notifications */


/* Lakka */


/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Jalankan Ulang RetroArch"
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

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
   "Hovedmenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Indstillinger"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favoritter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Historik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Billeder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Musik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Videoer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Udforsk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importer indhold"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Hurtig Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Få hurtigt adgang til alle relevante indstillinger i spillet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Indlæs kerne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Vælg kerne der skal bruges."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Indlæs indhold"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Vælg hvilket indhold der skal startes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Indlæs disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Indlæs en fysisk medie disk. Vælg først kernen (Indlæs kerne) til brug med disken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Dump disk"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Dump den fysiske disk til intern opbevaring. Det vil blive gemt som en diskafbildningsfil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Spilleliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Scannet indhold der matcher databasen vil blive vist her."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Importer indhold"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Opret og opdater spillelister ved at scanne indhold."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Vis skrivebords menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Åbn den traditionelle skrivebords menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Deaktiver Kiosk-tilstand (Genstart Påkrævet)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Vis alle konfigurationsrelaterede indstillinger."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Opdater online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Hent tilføjelser, komponenter og indhold til RetroArch."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Deltag i eller vær vært for en netplay session."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Indstillinger"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Indstil programmet."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Vis systemoplysninger."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Konfigurationsfil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Håndter og opret konfigurationsfiler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Hjælp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Læs mere om, hvordan programmet virker."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Genstart RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Genstart programmet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Afslut RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Afslut programmet."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Hent en kerne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Hent og installer en kerne fra onlineopdateringsprogrammet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Installer eller gendan en kerne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Installér eller gendan en kerne fra mappen 'Downloads'."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Start Videoprocessor"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Start Mappe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Gennemse Arkiv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Indlæs Arkiv"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favoritter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Indhold tilføjet til 'Favoritter' vises her."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Musik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Musik som tidligere er blevet afspillet vises her."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Billeder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Billeder som tidligere er blevet set vil blive vist her."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Videoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Videoer som tidligere er blevet afspillet vil blive vist her."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Udforsk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Gennemse alt indhold, der matcher databasen via en kategoriseret søgegrænseflade."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Kerne downloader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Opdater installerede kerner"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Opdater alle installerede kerner til den nyeste tilgængelige version."
   )

/* Main Menu > Information */


/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Installer en tidligere version af kernen fra en liste over arkiverede sikkerhedskopier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Slet sikkerhedskopi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Fjern en fil fra listen over arkiverede sikkerhedskopier."
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Build dato"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Git version"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "CPU model"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "CPU funktioner"
   )
MSG_HASH( /* FIXME Colon should be handled in menu_display.c like the rest */
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "CPU arkitektur:"
   )
MSG_HASH( /* FIXME Colon should be handled in menu_display.c like the rest */
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "CPU kerner:"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CPU_CORES,
   "Mængden af kerner, som CPU'en har."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Frontend identifikator"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "RetroRating Niveau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Strømkilde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Video kontekst driver"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Skærmbredde (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Skærmhøjde (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Skærmens DPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Command Interface understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Network Command Interface understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Network Controller understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Cocoa understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "PNG (RPNG) understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "JPEG (RJPEG) understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "BMP (RBMP) understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "TGA (RTGA) understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "SDL 1.2 understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "SDL 2 understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Vulkan understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Metal understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "OpenGL understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "OpenGL ES understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Threading understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "KMS/EGL understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "udev understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "OpenVG understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "EGL understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "X11 understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Wayland understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "XVideo understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "ALSA understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "OSS understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "OpenAL understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "OpenSL understøttelse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "RSound understøttelse"
   )

/* Main Menu > Information > Database Manager */


/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Co-op Understøttet"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Indlæs konfiguration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Nulstil til standardindstillinger"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Nulstil den aktuelle konfiguration til standardværdier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Gem aktuel konfiguration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Gem ny konfiguration"
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Grundlæggende menukontroller"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Rul Op"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Rul Ned"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Bekræft"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Åbn/luk menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Luk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Slå Tastatur Til/Fra"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Drivere"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Skift drivere, der bruges af systemet."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Skift indstillinger for videooutput."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Lyd"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Skift indstillinger for lydudgang."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Skift controller, tastatur og mus indstillinger."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latenstid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Skift indstillinger relateret til video, lyd og input latenstid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Kerne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Skift kerneindstillinger."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Skift standardindstillinger for konfigurationsfiler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Gemmer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Skift gemmeindstillinger."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Logning"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Skift logføringsindstillinger."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Filhåndtering"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Skift filhåndteringsindstillinger."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Skift indstillingerne for spoling tilbage, spoling frem og slow motion."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Optager"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Skift optagelsesindstillinger."
   )

/* Settings > Drivers */


/* Settings > Video */

#if defined(DINGUX)
#if defined(RS90) || defined(MIYOO)
#endif
#endif

/* Settings > Video > CRT SwitchRes */


/* Settings > Video > Output */

#if defined(DINGUX) && defined(DINGUX_BETA)
#endif

/* Settings > Video > Fullscreen Mode */


/* Settings > Video > Windowed Mode */


/* Settings > Video > Scaling */

#if defined(DINGUX)
#endif

/* Settings > Video > HDR */


/* Settings > Video > Synchronization */


/* Settings > Audio */


/* Settings > Audio > Output */


/* Settings > Audio > Resampler */


/* Settings > Audio > Synchronization */


/* Settings > Audio > MIDI */


/* Settings > Audio > Mixer Settings > Mixer Stream */


/* Settings > Audio > Menu Sounds */


/* Settings > Input */

#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
#endif

/* Settings > Input > Haptic Feedback/Vibration */


/* Settings > Input > Menu Controls */


/* Settings > Input > Hotkeys */


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
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Indstillinger"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Hovedmenu"
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */


/* Settings > User Interface > Views > Settings */


/* Settings > User Interface > Appearance */


/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */


/* Settings > Network */


/* Settings > Network > Updater */


/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Historik"
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


/* Music */

/* Music > Quick Menu */


/* Netplay */


/* Netplay > Host */


/* Import Content */


/* Import Content > Scan File */


/* Import Content > Manual Scan */


/* Explore tab */

/* Playlist > Playlist Item */


/* Playlist Item > Set Core Association */


/* Playlist Item > Information */


/* Quick Menu */


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
   "Indstillinger"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Historik"
   )

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

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
   "Hauptmenü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favoriten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Verlauf"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Musik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Aktiviere Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Inhalt importieren"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Spielmenü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Greife schnell auf alle relevanten Spieleinstellungen zu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Core laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Wähle, welcher Core verwendet werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Inhalt laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Wähle, welcher Inhalt gestartet werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Disk laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Disk dumpen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Dumpt die physische Medien-Disk auf den internen Speicher. Sie wird als Abbild gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Während der Suche gefundener Inhalt wird hier erscheinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Inhalt durchsuchen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Desktop-Menü anzeigen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Deaktiviere Kiosk-Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Online-Aktualisierungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Lade Erweiterungen, Komponenten und Inhalte für RetroArch herunter."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Aktiviere Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Hoste eine Netplay-Sitzung oder trete einer bei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Programm konfigurieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Zeige Informationen über Core, Netzwerk und das System an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Konfigurationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Verwalte und erstelle Konfigurationsdateien."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Hilfe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Erfahre mehr darüber, wie Retroarch funktioniert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "RetroArch neustarten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Programm neu starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "RetroArch beenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Verlasse das Programm."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Core herunterladen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Core installieren oder wiederherstellen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Starte Videoprozessor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Starte Remote-RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Startverzeichnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Öffne Archiv als Ordner"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Archiv laden"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favoriten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Musik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Musik, die bereits abgespielt wurde, wird hier erscheinen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Bilder, die bereits angezeigt wurden, werden hier erscheinen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Videos, die bereits abgespielt wurden, werden hier erscheinen."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Installierte Kerne aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Aktualisiere alle installierten Cores auf die neueste verfügbare Version."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Miniaturansichten aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Lade ein komplettes Miniaturansichtspaket für das ausgewählte System herunter."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Wiedergabelisten Miniaturansichten-Updater"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Inhalt herunterladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Core-Infodateien aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Zusätzliche Daten aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Cheats aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Datenbanken aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Overlays aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "GLSL-Shader aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "CG-Shader aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Slang-Shader aktualisieren"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Core-Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Zeige Informationen über diese Anwendung/diesen Core an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Netzwerk-Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Zeige Netzwerk-Karten und zugewiesene IP-Adressen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "System-Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Zeige Informationen über dieses Gerät an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Datenbanken verwalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Betrachte Datenbanken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Suchanfragen verwalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Betrachte vorherige Suchanfragen."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Core-Name"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Core-Bezeichnung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "System-Name"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "System-Hersteller"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Kategorien"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Autoren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Berechtigungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Lizenz(en)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Unterstützte Erweiterungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING,
   "Fehlt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT,
   "Vorhanden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REQUIRED,
   "Notwendig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Core löschen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Lösche diesen Core von der Festplatte."
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Build-Datum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Git-Version"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Prozessor-Modell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "CPU-Eigenschaften"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "CPU-Architektur:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "CPU-Kerne:"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CPU_CORES,
   "Anzahl der verfügbaren CPU-Kerne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Frontend-Kennung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Frontend-Betriebssystem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "RetroRating-Stufe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Stromquelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Video-Kontext-Treiber"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Bildschirmbreite (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Bildschirmhöhe (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Bildschirm-DPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "LibretroDB-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Overlay-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Befehlsinterface-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Netzwerk-Befehlsinterface-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Cocoa-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "PNG-Unterstützung (RPNG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "JPEG-Unterstützung (RJPEG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "BMP-Unterstützung (RBMP)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "TGA-Unterstützung (RTGA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "SDL1.2-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "SDL2-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Vulkan-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Metal-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "OpenGL-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "OpenGL-ES-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Threading-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "KMS/EGL-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Udev-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "OpenVG-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "EGL-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "X11-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Wayland-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "XVideo-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "ALSA-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "OSS-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "OpenAL-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "OpenSL-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "RSound-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "RoarAudio-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "JACK-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "PulseAudio-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "CoreAudio-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "CoreAudio V3-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "DirectSound-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "WASAPI-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "XAudio2-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "Zlib-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "7zip-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Unterstützung für dynamische Bibliotheken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Dymanisches Laden der libretro-Bilbiothek zur Laufzeit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Cg-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "GLSL-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "HLSL-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "SDL-Image-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "FFmpeg-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "mpv unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "CoreText-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "FreeType-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "STB TrueType-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Netplay-Unterstützung (Peer-to-Peer)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
   "Python-Unterstützung (Script-Unterstützung in Shadern)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Video4Linux2-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "Libusb-Unterstützung"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Datenbank-Auswahl"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Beschreibung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Herausgeber"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Entwickler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Herkunft"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "TGDB-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Famitsu Magazine-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Edge Magazine-Testbericht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Edge Magazine-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Edge Magazine-Ausgabe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Veröffentlichungsmonat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Veröffentlichungsjahr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "BBFC-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "ESRB-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "ELSPA-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "PEGI-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Hardware-Erweiterungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "CERO-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Seriennummer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analog unterstützt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Rumble unterstützt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Co-op unterstützt"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Konfgurationsdatei laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Auf Standardwerte zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Setzt die momentane Konfiguration auf die Standardwerte zurück."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Speichere aktuelle Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Speichere neue Konfiguration"
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Grundlegende Menüsteuerung"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Nach oben scrollen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Nach unten scrollen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Bestätigen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Menü ein-/ausschalten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Beenden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Tastatur ein-/ausschalten"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Treiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Ändere die Treiber, die von diesem System verwendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Videotreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Einstellungen für die Videoausgabe anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Audiotreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Ändere die Einstellungen der Audio-Ausgabe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Eingabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Einstellungen für Joypads, Tastaturen und Mäuse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Einstellungen für Video-, Audio- und Eingangslatenz ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Cores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Ändere die Einstellungen der Cores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Konfigurationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Ändere die Standard-Einstellungen für Konfigurationsdateien."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Ändere die Einstellungen der Speicherfunktion."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Protokollierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Ändere die Log-Einstellungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Dateibrowser"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Bildwiederholrate begrenzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Ändere die Einstellungen für Zurückspulen, Zeitraffer und Zeitlupe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Aufnahme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Ändere die Einstellungen für die Aufnahme-Funktion."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Bildschirm-Meldungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Ändere die Einstellungen für Bildschirm-Overlays, Bildschirmtastatur und Bildschirmbenachrichtigung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Benutzeroberfläche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Ändere die Einstellungen für die Benutzeroberläche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "KI-Dienst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Energieverwaltung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Ändere die Energieverwaltungs-Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Ändere die Einstellungen für Errungenschaften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Netzwerk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Ändere die Einstellungen für das Netzwerk."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Ändere die Einstellungen für die Playlists."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Benutzer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Konten, Benutzername, Sprache."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Verzeichnisse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Ändere die Standard-Verzeichnisse für dieses System"
   )

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Eingabetreiber"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Videotreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Zu verwendender Videotreiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Audiotreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Zu verwendender Audiotreiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Audio-Resampling-Treiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Zu verwendender Audio-Resampling-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Kameratreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Zu verwendender Kameratreiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
   "WLAN-Treiber"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Standort-Treiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Zu verwendender Treiber für Ortsdienste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Menütreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Zu verwendender Menütreiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Aufnahmetreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Zu verwendender Aufnahmetreiber."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Zu verwendender MIDI-Treiber."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Native Signale mit niedgriger Auflösung ausgeben, welche mit CRT Bildschirmen benutzt werden können."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Einstellungen für die Videoausgabe anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Bildschirmschoner ausschalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Hält dein System davon ab, den Bildschirmschoner zu aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Video in separatem Thread"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Verbessert die Leistung, führt aber zu größerer Latenz und kann Videoprobleme verursachen. Verwende dies nur, wenn Du sonst die gewünschte Leistung nicht erreichst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Schwarzes Bild zwischen Einzelbildern einfügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Verwende GPU für Bildschirmfotos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Verwende Bildmaterial nach Shaderdurchläufen für Bildschirmfotos, sofern verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Bilineare Filterung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Wendet einen leichten Weichzeichner auf das Bild an, um die Pixelkanten etwas abzurunden. Diese Option hat einen sehr geringen Einfluss auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Grafikfilter"
   )

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Nur für CRT Bildschirme. Versuche die exakte Core-/Spielauflösung und refresh Rate zu verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "CRT Superauflösung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Umschalten zwischen Nativer- und ultraweiter Superauflösungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "X-Achsenzentrierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Versuche eine dieser Optionen, wenn das Bild nicht richtig zentriert auf dem Bildschirm erscheint."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Benutze eine benutzerdefinierte Aktualisierungsrate."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Benutze eine, in der Konfiguration definierte, Aktualisierungsrate wenn notwendig."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Monitor-Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Wält den Bildschirm aus, der für RetroArch verwendet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Rotation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Erzwinge eine bestimmte Bildrotation. Diese Rotation wird Rotationen hinzugefügt, die vom Core vorgegeben werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Bildwiederholrate"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Geschätzte Bildwiederholrate"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Die geschätzte Bildwiederholrate des Bildschirms in Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Erzwinge Deaktivierung des sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Erzwinge die Deaktivierung der Unterstützung für sRGB FBO. Einige Intel OpenGL-Treiber können unter Windows Probleme verursachen, wenn sRGB FBO aktiviert ist. Diese Einstellung kann dies beheben."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Vollbildmodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Starte im Vollbildmodus. Kann zur Laufzeit geändert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Unechter Vollbildmodus (Windowed Fullscreen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Bevorzuge einen Fenster-Vollbildmodus für den Vollbildmodus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Vollbild-Breite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Benutzerdefinierte Bildbreite für den Vollbildmodus. Wird dieser Wert auf 0 belassen, wird die Desktop-Auflösung verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Vollbild-Höhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Benutzerdefinierte Bildhöhe für den Vollbildmodus. Wird dieser Wert auf 0 belassen, wird die Desktop-Auflösung verwendet."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Skalierung im Fenstermodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Legt die Fenstergröße relativ zu der Core-Bildgröße fest. Alternativ kannst Du weiter unten die Höhe und Breite des Bildes selbst anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Fensterbreite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Wähle die Fensterbreite. Wird dieser Wert auf 0 belassen, wird versucht, das Fenster so groß wie möglich zu skalieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Fensterhöhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Wähle die Fensterhöhe. Wird dieser Wert auf 0 belassen, wird versucht, das Fenster so groß wie möglich zu skalieren."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Pixelgenaue Bildskalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Skaliere das Bild in ganzzahligen Schritten. Die Basisgröße hängt von der vom System gemeldeten Geometrie und dem Seitenverhältnis ab. Wenn 'Seitenverhältnis erzwingen' nicht gewählt ist, werden Höhe u. Breite unabhängig skaliert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Bildseitenverhältnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Konfiguriere Bildseitenverhältnis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Gleitkomma-Wert für das Bild-Seitenverhältnis (Breite / Höhe). Wird verwendet, wenn das Seitenverhältnis auf 'Benutzerdefiniert' eingstellt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "X-Position des Bildes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Y-Position des Bildes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Bildbreite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Benutzerdefinierte Bildhöhe, die verwendet wird, wenn das Seitenverhältnis auf 'Benutzerdefiniert' eingestellt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Bildhöhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Benutzerdefinierte Bildhöhe, die verwendet wird, wenn das Seitenverhältnis auf 'Benutzerdefiniert' eingestellt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Bildränder (Overscan) zuschneiden (Neustart erforderlich)"
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Vertikale Synchronisation (Vsync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Synchronisiert die Video-Ausgabe der Grafikkarte mit der Bildwiederholrate des Bildschirms. Empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "VSync-Intervall"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Verwende ein eigenes Intervall für Vsync. Aktivieren, um die Bildschirm-Wiederholrate zu halbieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Adaptiver Vsync"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Bildverzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Reduziert Latenz, kann aber zu Anzeigefehlern führen. Fügt eine Verzögerung bei V-Sync hinzu (in ms)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Synchronisiere GPU und CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "'Harte' Synchronisation der CPU und der GPU. Reduziert Latenz, braucht aber mehr Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Anzahl der Frames für GPU-CPU-Sync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Anzahl der Bilder, der die CPU der GPU voraus sein darf, wenn die CPU/GPU-Synchronisation aktiviert ist."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Ändere die Einstellungen der Audio-Ausgabe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Synchronisiere Audio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Mixer-Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Anzeigen und/oder anzeigen von Audiomixer-Einstellungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Stumm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Audio stumm-/lautschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Audio-Mixer stummschalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Audio-Mixer laut-/stummschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Lautstärke (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Lautstärkeverstärkung in dB. 0 dB ist die normale Lautstärke, es erfolgt eine Verstärkung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Lautstärke des Audio-Mixers (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Globale Lautstärke des Audio-Mixers (in dB). 0 dB ist die normale Lautstärke ohne zusätzliche Verstärkung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Audio-DSP-Plugin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Audio-DSP-Plugin, welches die Audiodaten verarbeitet, bevor sie zum Treiber gesendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Exklusiver WASAPI-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Erlaube dem WASAPI-Treiber, die vollständige Kontrolle über das Audio-Gerät zu übernehmen. Wenn deaktiviert, wird der Shared-Modus verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "WASAPI-Gleitkomma-Format"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Verwende Gleitkomma-Format für den WASAPI-Treiber, wenn dein Audio-Gerät dies unterstützt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Größe des gemeinsamen WASAPI-Puffers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Zwischenspeicher-Größe (in Frames), wenn der WASAPI-Treiber im Shared-Modus verwendet wird."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Aktiviere Audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Aktiviere Audio-Ausgabe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Soundkarte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Überschreibe das standardmäßig ausgewählte Audiogerät, welches vom Audiotreiber verwendet wird. Dies ist treiberabhängig."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Audiolatenz (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Gewünschte Audiolatenz in Millisekunden. Je nach Audiotreiber kann die gewünschte Latenz nicht erzielt werden; diese Einstellung wird dann ignoriert."
   )

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Audio Resampler Qualität"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Verkleinere diesen Wert um Performance/kleinere Latenz gegenüber Audio Qualität zu bevorzugen, erhöhe ihn wenn du bessere Audioqualität bevorzugst auf kosten der Performance/kleinerer Latenz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Audiofrequenzrate (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Sampling-Rate der Audio-Ausgabe."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Synchronisiere Audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Synchronisiere Audio. Empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Maximaler Audioversatz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Maximale Änderung der Audio-Eingangsrate. Dieser Wert kann erhöht werden, wenn sehr große Änderungen im Timing aktiviert werden sollen, (beispielsweise um einen PAL-Core auf einem NTSC-Bildschirm darzustellen), verursacht jedoch fehlerhafte Tonhöhen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Dynamische Audio-Ratenkontrolle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Hilft, Fehler bei der Audio- und Videosynchronisierung auszubügeln. Wenn deaktiviert, ist eine brauchbare Synchronisation nahezu unmöglich."
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Eingabe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Lautstärke"
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Abspielen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Wird die Wiedergabe des Audiostreams starten. Sobald dieser fertig ist, wird der jetzige Audiostream aus dem Speicher entfernt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Abspielen (Schleife)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Wird die Wiedergabe des Audiostreams starten. Sobald dieser fertig ist, wird dieser erneut von vorne gestartet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Abspielen (Sequenziell)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Wird die Wiedergabe des Audiostreams starten. Sobald dieser fertig ist, springt er zum nächsten Audiostream in sequenzieller Reihenfolge und wiederholt dieses verhalten. Nützlich als Albumwiedergabemodus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Stopp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Dies wird die Wiedergabe des Audiostreams stoppen, aber ihn nicht aus dem Speicher entfernen. Du kannst die Wiedergabe fortsetzen in dem du erneut auf 'Abspielen' klickst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Dies wird die Wiedergabe stoppen und den Audiostream komplett aus dem Speicher entfernen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Lautstärke"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Anpassen der Lautstärke des Audiostreams."
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Menuaudio aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Menuton aktivieren oder deaktivieren"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Maximale Benutzeranzahl"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Maximale Anzahl von Benutzern, die von RetroArch unterstützt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Automatische Konfiguration aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Zeitlimit für Belegung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Zeitdauer in Sekunden, nach der die nächste Tastenbelegung abgefragt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Autofeuer-Frequenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
   "Autofeuer-Haltedauer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "Beschreibt, wie lange eine Taste mit aktivierem Turbo-Modus 'gedrückt' wird. Dieser Wert wird in der Anzahl der Einzelbilder angegeben."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Konfiguriere Hotkey-Einstellungen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Steuerung für diesen Port konfigurieren."
   )

/* Settings > Input > Haptic Feedback/Vibration */


/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Einheitliche Menüsteuerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Verwende die selbe Steuerung für das Menü und die Spiele. Betrifft nur Tastatur."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Vertausche die Tasten für 'OK' und 'Abbrechen'. Wenn deaktiviert, wird die japanische Tastenbelegung verwendet; wenn aktiviert, wird die westliche Tastenbelegung verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Alle Benutzer können das Menü steuern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Ermöglicht jedem Benutzer das Menü zu benutzen. Wenn deaktiviert, kann nur Benutzer 1 das Menü benutzen."
   )

/* Settings > Input > Hotkey Binds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Controller-Tastenkombination für Menü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Spielstand laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Zustand (Savestate) abspeichern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "RetroArch beenden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Zustands-Speicherplatz +"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Zustands-Speicherplatz -"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Zurückspulen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Einzelbildvorlauf"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Spiel zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Nächster Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Vorheriger Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Erstelle Bildschirmfoto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Datenträger einlegen/auswerfen"
   )

/* Settings > Input > Port # Binds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Gerätetyp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Analog-zu-Digital-Typ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Geräteindex"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Maus-Index"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "B-Knopf (nach unten)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Y-Knopf (links)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Select-Knopf"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Start-Knopf"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "A-Knopf (rechts)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "X-Knopf (oben)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "L-Knopf (Schultertaste)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "R-Knopf (Schultertaste)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "L2-Knopf (Abzug)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2-Knopf (Abzug)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "L3-Knopf (Daumen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "R3-Knopf (Daumen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Linker Analogstick X+ (rechts)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Linker Analogstick X- (links)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Linker Analogstick Y+ (nach unten)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Linker Analogstick Y- (nach oben)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Rechter Analogstick X+ (rechts)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Rechter Analogstick X- (links)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Rechter Analogstick Y+ (nach unten)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Rechter Analogstick Y- (nach oben)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Autofeuer aktivieren"
   )

/* Settings > Latency */


/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Gemeinsamen Hardware-Kontext aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Gibt Hardware-gerenderten Cores einen eigenen privaten Kontext. Vermeidet, dass der Hardware-Status zwischen den Frames geschätzt werden muss."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Leeren Core laden, wenn Core beendet wird"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Cores automatisch starten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Vor dem Laden nach fehlender Firmware suchen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Überprüft, ob eventuell benötigte Firmware vorhanden ist, bevor versucht wird, Inhalte zu laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Erlaube Bildrotation"
   )

/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Lädt inhaltsspezifische Core-Einstellungen automatisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Aktiviere benutzerdefinierte Core-Einstellungen beim Start automatisch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Override-Dateien automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Aktiviere benutzerdefininerte Einstellungen beim Start automatisch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Remap-Dateien automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Aktiviere benutzerdefinierte Tastenbelegungen beim Start automatisch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Shader-Voreinstellungen automatisch laden"
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Speichere Spielstände in Ordnern ab, die nach dem verwendeten Core benannt sind."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Überschreibe SaveRAM nicht, wenn ein Zustand (Savestate) geladen wird"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Verhindere, dass das Save-RAM überschrieben wird, wenn ein Spielstand geladen wird. Kann zu fehlerhaften Spielen führen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Intervall für automatisches Speichern des SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Zustand automatisch speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Lade den zuletzt gespeicherten Zustand beim Start automatisch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Zustands-Miniaturansichten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Speicherdaten im Verzeichnis des Inhalts speichern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Zustandsdaten im Verzeichnis des Inhalts speichern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Systemdateien befinden sich im Verzeichnis des Inhalts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Bildschirmfotos im Verzeichnis des Inhalts speichern"
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Logs auf Kommandozeile ausgeben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Aktiviere Logging im Terminal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Core-Loglevel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Setzt den Log-Level für Cores. Wenn der Log-Level, der von einem Core zurückgemeldet wird, unter diesem Wert liegt, wird er ignoriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Leistungsindikatoren"
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Unbekannte Dateierweiterungen filtern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Verwende eingebauten Medienplayer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Nach aktuellem Core filtern"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Zurückspulen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Verwalte die Einstellungen für die Rückspul-Funktion."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Die maximale Geschwindigkeit, mit der Inhalt im Vorlauf-Modus dargestellt wird (z.B. 5x für 60 Inhalt mit 60 Bildern/sek => Begrenzung auf 300 Bilder/sek). Wenn 0x gewählt ist, ist die Vorlauf-Geschwindigkeit unbegrenzt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Bildwiederholrate im Menü begrenzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Stelle sicher, dass die Bildwiederholrate im Menü begrenzt wird."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Zurückspulen (Rewind) aktivieren"
   )

/* Settings > Frame Throttle > Frame Time Counter */


/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Wende Filter auf Aufnahme an"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Zeichnet das Bild auf, nachdem Filter (aber keine Shader) angewendet wurden. Dein Video wird genauso gut aussehen, wie das, was Du auf dem Bildschirm siehst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Verwende GPU für Aufnahme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Verwende Bildmaterial nach Shaderdurchläufen für Aufnahmen, sofern verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Streamingmodus"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Bildschirm-Overlay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Bildschirm-Benachrichtigungen"
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Aktiviere Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Aktiviere das Overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Overlay im Menü nicht anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Verberge das Overlay im Menü und zeige es wieder an, wenn das Menü verlassen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
   "Eingaben im Overlay anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
   "Tastatur/Controller-Tastendrücke im Overlay anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
   "Eingaben von folgendem Port anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
   "Wähle den Port des Controllers aus, dessen Eingaben im Overlay angezeigt werden sollen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Overlays"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Bevorzugtes Overlay automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Overlay-Transparenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Transparenz aller Bedienelemente des Overlays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Overlay-Voreinstellung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Wähle ein Overlay mit dem Dateibrowser aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
   "Overlay-Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
   "Skalierung aller Bedienelemente des Overlays."
   )



/* Settings > On-Screen Display > Video Layout */


/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Bildschirm-Benachrichtigungen aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Zeige Bildschirmbenachrichtungen an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Bildwiederholrate anzeigen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Zeige die aktuelle Anzahl an Einzelbildern an"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Statistiken anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Technische Onscreen-Statistiken anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Schriftart für Bildschirm-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Schritftgröße der Bildschirm-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Wähle die Schriftgröße in Punkten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "X-Position der Bildschirm-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Wähle eine eigene X-Achsenposition für Bildschirmmeldungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Y-Position der Bildschirm-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Wähle eine eigene Y-Achsenposition für Bildschirmmeldungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Rot-Anteil der Bildschirmmeldungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Grün-Anteil der Bildschirmmeldungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Blau-Anteil der Bildschirmmeldungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Aktiviere Hintergrund in Bildschirmmeldungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Rot-Anteil der Hintergrundfarbe der Bildschirmmeldungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Grün-Anteil der Hintergrundfarbe der Bildschirmmeldungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Blau-Anteil der Hintergrundfarbe der Bildschirmmeldungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Transparenz des Hintergrunds der Bildschirmmeldungen"
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Menü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Legt die Einstellungen für das Aussehen des Menübildschirms fest."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Zeige erweiterte Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Aktiviere Kiosk-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Schützt RetroArch, in dem alle Konfigurationsmöglichkeiten ausgeblendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Setze Passwort zur Deaktivierung des Kiosk-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Wird beim Aktivieren des Kiosk-Modus ein Passwort vergeben, kann er später unter Angabe des Passworts wieder deaktiviert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Navigation umbrechen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Springe zum Anfang und/oder Ende, wenn die Grenzen einer Liste horizontal oder vertikal erreicht werden."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Wenn deaktiviert, wird der Inhalt im Hintergrund weiterlaufen, wenn das RetroArch-Menü aufgerufen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Maus-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Touch-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Multithreading"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Führe Aufgaben im Hintergrund aus."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Pausiere das Spiel, wenn RetroArch nicht das aktive Fenster ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Deaktiviere Desktop-Gestaltung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Aktiviere oder deaktiviere Desktop-Gestaltung (nur Windows)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "UI-Companion aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Menüleiste"
   )

/* Settings > User Interface > Views */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Spielmenü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Zeige 'Core laden'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Zeige/verstecke die Option 'Core laden'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Zeige 'Inhalt laden'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Zeige/verstecke die Option 'Inhalt laden'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Zeige/verberge die Option 'Online-Aktualisierungen'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Zeige/verberge die Funktion zum Aktualisieren der Cores (und der Info-Dateien)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Zeige Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Zeige/verstecke die Option 'Informationen'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Zeige Konfigurationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Zeige/verstecke die Option 'Konfigurationen'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Zeige Hilfe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Zeige/verstecke die Option 'Hilfe'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Zeige 'RetroArch beenden'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Zeige/verstecke die Option 'RetroArch beenden'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Zeige Tab 'Einstellungen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Zeige den Reiter 'Einstellungen' im Hauptmenü an"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Wird vor dem Verstecken der Einstellungen ein Passwort vergeben, wird es später möglich sein, den Reiter wiederherzustellen, in dem die Funktion 'Aktiviere den Reiter 'Einstellungen' ausgewählt und das Passwort dort eingegeben wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Zeige Tab 'Favoriten'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Zeige den Reiter 'Favoriten' im Hauptmenü an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Zeige Tab 'Bilder'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Zeige den Reiter 'Bilder' im Hauptmenü an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Zeige Tab 'Musik'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Zeige den Reiter 'Musik' im Hauptmenü an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Zeige Tab 'Video'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Zeige den Reiter 'Video' im Hauptmenü an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Zeige Tab 'Netplay'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Zeigen den Reiter 'Netplay' im Hauptmenü an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Zeige Tab 'Verlauf'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Zeige den Reiter 'Verlauf' im Hauptmenü an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "Zeige Tab 'Inhalte importieren'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "Zeige den Reiter 'Inhalt importieren' im Hauptmenü an"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Uhrzeit / Datum anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Zeige das aktuelle Datum und/oder die Zeit im Menü an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Akkustand anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Zeige den aktuellen Akkustand im Menü an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Core-Namen anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Zeige den Namen des aktuellen Cores im Menü an."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Zeige Startbildschirm"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Zeige einen Startbildschirm im Menü an. Wird automatisch deaktiviert, nachdem das Programm zum ersten Mal gestartet wurde."
   )

/* Settings > User Interface > Views > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Zeige 'Bildschirmfoto erstellen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Zeige/verstecke die Option 'Bildschirmfoto erstellen'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Zeige 'Zustand (Savestate) speichern/laden'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Zeige/verstecke die Optionen zum speichern und laden von Zuständen (Savestates)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Zeige 'Zustand speichern/laden rückgängig machen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Zeige/verstecke die Optionen zum Rückgängig machen von Lade- und Speichervorgängen für Savestates."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Zeige 'Zu Favoriten hinzufügen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Zeige/verstecke die Option 'Zu Favoriten hinzufügen'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Zeige 'Optionen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Zeige/verstecke den Eintrag 'Optionen'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Zeige 'Steuerung'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Zeige/verstecke die Option 'Steuerung'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Zeige 'Cheats'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Zeige/verstecke die Option 'Cheats'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Zeige 'Shader'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Zeige/verstecke die Option 'Shader'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Rückspuleinstellungen anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Rückspuleinstellungsoption anzeigen/verstecken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Latenzoptionen anzeigen/verstecken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Latenzeinstellungen anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Overlayoptionen anzeigen/verstecken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Overlayeinstellungen anzeigen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Zeige 'Speichere Core-Überschreibungen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Zeige/verstecke die Option 'Speichere Core-Überschreibungen'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Zeige 'Speichere Spiele-Überschreibungen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Zeige/verstecke die Option 'Speichere Spiele-Überschreibungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Zeige 'Informationen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Zeige/verstecke die Option 'Informationen'."
   )

/* Settings > User Interface > Views > Settings */


/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Menü-Skalierungsfaktor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Hintergrundbild-Transparenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Framebuffer-Transparenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Modifiziere die Transparenz des Framebuffers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Miniaturansichten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Art der Miniaturansichten, die verwendet werden soll."
   )

/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Aktiviert Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Hardcore-Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Teste inoffizielle Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Aktiviere inoffizielle Errungenschaften und/oder Beta-Funktionen zu Testzwecken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Ausführlicher Modus für Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Aktiviere ausführliche Ausgabe von Bildschirmmeldungen für Errungenschaften."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Netplay öffentlich ankündigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Lege fest, ob Netplay-Spiele öffentlich angekündigt werden sollen. Wenn deaktiviert, müssen sich Clients manuell verbinden und können die öffentliche Lobby nicht verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Verwende Relay-Server"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Leite alle Netplay-Verbindungen durch einen Man-in-the-middle-Server. Hilfreich, wenn sich der Host hinter einer Firewall befindet oder Probleme mit NAT/UPnP hat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Server-Adresse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Die Adresse des Hosts, mit dem eine Verbindung hergestellt werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Netplay TCP/UDP-Port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Der Port der Host-IP-Adresse. Kann entweder ein TCP- oder ein UDP-Port sein."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Server-Passwort"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Server-Passwort für Beobachtermodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Im Beobachtermodus starten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Erlaube Slave-Modus für Clients"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Verbiete Clients, die nicht im Slave-Modus laufen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
   "Netplay ohne Savestates"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Netplay-Synchronisation prüfen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Eingabeverzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Eingabeverzögerung in Einzelbildern, die im Netplay verwendet wird, um die Netzwerklatenz zu verbergen. Reduziert Bildruckeln und CPU-Last, verursacht jedoch erkennbare Eingabeverzögerung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Erlaubte Eingabeverzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Der Bereich der Eingabeverzögerung in Einzelbildern, die im Netplay verwendet werden kann, um die Netzwerklatenz zu verbergen. Reduziert Bildruckeln und CPU-Last, verursacht jedoch unvorhersehbare Eingabeverzögerungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Netplay NAT-Traversal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Versuche, auf eingehende Verbindungen aus dem öffentlichen Internet zu hören. Dabei wird UPnP oder eine ähnliche Technologie benötigt, um das eigene LAN zu verlassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Netzwerk-Befehle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Port für Netzwerk-Befehle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "stdin-Befehle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Aktiviere stdin-Befehlsschnittstelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Online-Aktualisierungen"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Buildbot-Core-URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL zum Core-Verzeichnis auf dem libretro-Buildbot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Buildbot-Assets-URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL zum Assets-Verzeichnis auf dem libretro-Buildbot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Heruntergeladene Archive automatisch entpacken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Entpacke Dateien, die sich in heruntergeladenen Archiven befinden, nach dem Download automatisch."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Aktiviere Playlist für kürzlich geöffnete Inhalte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Länge der Verlaufsliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Begrenzt die Anzahl der Einträge in der Verlaufsliste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Umbenennen von Einträgen erlauben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Erlaube dem Benutzer, Einträge in den Playlists umzubenennen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Erlaubt, Einträge zu entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Erlaube dem Benutzer, Einträge aus den Playlists zu entfernen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Cores:"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Wiedergabeliste löschen"
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Privatsphäre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Ändere deine Privatsphären-Einstellungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Benutzerkonten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Verwalte die aktuell konfigurierten Benutzerkonten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Benutzername"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Gib deinen Benutzernamen hier ein. Dieser wird unter anderem für Netplay-Sitzungen verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Sprache"
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Erlaube Kamera-Zugriff"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Erlaube oder verbiete den Cores Zugriff auf die Kamera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Discord aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Standortbestimmung erlauben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Erlaube oder verbiete den Cores Zugriff auf Ortsdienste."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Retro-Errungenschaften"
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Benutzername"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Passwort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Gib das Passwort deines Errungenschaften-Kontos ein."
   )

/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "Lege das Systemverzeichnis fest. Cores können dieses Verzeichnis verwenden, um ein BIOS, system-spezifische Konfigurationen usw. zu laden."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Speichere alle heruntergeladenen Dateien in diesem Verzeichnis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Dynamische Hintergrundbilder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Verzeichnis, in dem Hintergrundbilder gespeichert werden, die vom Menü je nach verwendetem Inhalt geladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Miniaturansichten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Ergänzende Miniaturansichte (Boxarts, Bildschirmfotos...) werden hier gespeichert."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Dateibrowser"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Konfigurationen"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Legt das Verzeichnis fest, in dem das Menü mit der Suche nach Konfigurationen beginnt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Cores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Verzeichnis, in dem das Programm nach Inhalten/Cores sucht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Core-Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Awendungs- und Core-Informationsdateien werden hier gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Inhaltsdatenbank"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Datenbanken werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
   "Suchanfragen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
   "Gespeicherte Suchanfragen werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Cheat-Datei"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Verzeichnis, in dem alle Cheat-Dateien gespeichert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Grafikfilter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Verzeichnis, in dem alle CPU-basierten Videofilter gespeichert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Audiofilter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Verzeichnis, in dem alle Audio-DSP-Filter gespeichert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Video-Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Verzeichnis, in dem alle GPU-basierten Videoshader gespeichert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Aufnahmen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Aufnahmen werden in diesem Verzeichnis abgelegt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Aufnahme-Konfigurationen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Overlays"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Bildschirmfotos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Verzeichnis, in dem alle Bildschirmfotos abgelegt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Automatische Konfiguration von Controllern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Tastenbelegungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Speichere alle Playlists in diesem Verzeichnis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Speicherdaten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Speichere alle Speicherdaten in diesem Verzeichnis. Wenn kein Verzeichnis festgelegt ist, wird versucht, die Datei im Arbeitsverzeichnis des Inhalts zu speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Zustandsdaten (Savestates)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Speichere alle Zustände (Savestates) in diesem Verzeichnis. Wenn kein Verzeichnis festgelegt ist, wird versucht, die Datei im Arbeitsverzeichnis des Inhalts zu speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Temporäre Dateien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Wenn ein Verzeichnis gewählt wird, wird Inhalt, der temporär entpackt wird (z.B. aus Archiven) in dieses Verzeichnis entpackt."
   )

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Zum Mixer hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Diese Audiospur einem verfügbaren Audiostreamslot hinzufügen. Wenn momentan kein Slot verfügbar ist, wird dies ignoriert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Diese Audiospur einem verfügbaren Audiostreamslot hinzufügen und sie wiedergeben. Wenn momentan kein Slot verfügbar ist, wird dies ignoriert."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Verbinde zu einem Netplay-Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Aktiviere Netplay im Client-Modus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Verbindung trennen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Beendet eine aktive Netplay-Verbindung."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Starte Hosting"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Aktiviere Netplay im Host-Modus (Server)."
   )

/* Import content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Verzeichnis durchsuchen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Durchsuche ein Verzeichnis nach kompatiblen Dateien."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Dieses Verzeichnis durchsuchen>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Datei durchsuchen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Untersuche eine kompatible Datei."
   )

/* Import content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Zum mixer hinzufügen"
   )

/* Import content > Manual Scan */


/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Starte den Inhalt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Titel dieses Eintrags umbenennen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Von der Liste löschen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Entferne diesen Eintrag aus der Playlist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Zu Favoriten hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Füge diesen Eintrag zu deinen Favoriten hinzu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Zeige weiterführende Informationen über diesen Inhalt an."
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Inhaltsdatenbank"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Setze den aktuellen Inhalt wieder fort und verlasse das Spielmenü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Neu starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Starte den Inhalt vom Beginn an neu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Schließen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Schließt das aktuelle Spiel. Alle nicht gespeicherten Änderungen können verloren gehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Bildschirmfoto anfertigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Fertigt ein Foto des Bildschirms an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Spielstand-Speicherplatz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Ändert den aktuell gewählten Spielstand-Speicherplatz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Aktuellen Zustand speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Speichere einen Zustand in dem aktuellen Speicherplatz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Spielstand laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Lade einen gespeicherten Zustand aus dem aktuellen Speicherplatz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Laden des Zustands rückgängig machen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Wenn ein Zustand (Savestate) geladen wurde, wird der Inhalt zum Zustand vor dem Laden des Spielstandes zurückkehren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Speichern des Zustands rückgängig machen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Wenn ein Zustand (Savestate) überschrieben wurde, wird der Inhalt des zuvor gespeicherten Spielstands wiederhergestellt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Zu Favoriten hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Füge diesen Eintrag zu deinen Favoriten hinzu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Aufnahme starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Startet die Aufnahme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Aufnahme beenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Beendet die Aufnahme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Streamen starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Startet das Streamen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Streamen beenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Beendet das Streamen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Optionen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Ändere die Optionen für die aktuell laufende Anwendung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Steuerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Ändere die Steuerung für den aktuell laufenden Inhalt."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Verwende Cheat-Codes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Datenträger-Optionen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Verwaltung von Datenträger-Abbildern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Verwende Shader, um die Videodarstellung zu verbessern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Überschreibungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Optionen zum überschreiben der globalen Konfiguration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Errungenschaften"
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Erstellt Datei mit Spieloptionen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Spieloptionen"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Remap-Datei laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Speichere Core-Remap-Datei"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Lösche Core-Remap-Datei"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Speichere Spiel-Remap-Datei"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Lösche Spiel-Remap-Datei"
   )

/* Quick Menu > Controls > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Remap-Datei"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Cheat-Datei laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Lade eine Cheat-Datei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Speichere Cheat-Datei unter ..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Speichere aktuellen Cheats als Speicherdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Änderungen übernehmen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Änderungen an den Cheats werden sofort übernommen."
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */


/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Cheat-Datei"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Aktiviert"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Füge Datenträgerabbild hinzu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Datenträger-Nummer"
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Shader-Voreinstellung laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Lade eine Shader-Voreinstellung. Die Shader-Pipeline wird automatisch konfiguriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Änderungen übernehmen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Änderungen an der Shader-Konfiguration werden sofort übernommen. Verwende diese Option, wenn Du die Anzahl der Shader-Durchläufe, die Filter, die FBO-Skalierung usw. geändert hast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Vorschau der Shader-Parameter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Modifiziert den aktuellen Shader direkt. Änderungen werden nicht in den Shader-Voreinstellungen gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Shader-Durchläufe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Skalierung"
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Shader-Voreinstellung speichern unter..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Speichere die aktuellen Shader-Einstellungen als neue Shader-Voreinstellung ab."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Core-Voreinstellung speichern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Spiel-Voreinstellung speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Speichere die aktuellen Shader-Einstellungen als Standard-Einstellung für diesen Inhalt."
   )

/* Quick Menu > Shaders > Remove */


/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Keine Shader-Parameter."
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Speichere Core-Überschreibungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Speichere eine überschreibende Konfiguration, die für jeden Inhalt, der mit diesem Core geladen wird, verwendet wird. Wird gegenüber der normalen Konfiguration bevorzugt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Speichere Spiel-Überschreibungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Speichere eine überschreibende Konfiguration, die nur für den aktuellen Inhalt verwendet wird. Wird gegenüber der normalen Konfiguration bevorzugt."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Keine Errungenschaften vorhanden."
   )

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Keine Einträge."
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Kein Core verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Keine Core-Optionen verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Keine Core-Informationen verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Kein Verlauf verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Keine Informationen verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Keine Playlist-Einträge verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Keine Einstellungen gefunden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Kein Netzwerk gefunden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Kein Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Suchen:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Zurück"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Übergeordnetes Verzeichnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Verzeichnis nicht gefunden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Keine Einträge."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Wähle Datei"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Unbekannter Compiler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Geladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Lädt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Entlädt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Dieses Verzeichnis verwenden>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Inhaltsverzeichnis>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Standard>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Keins>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Bildschirmfoto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Titelbildschirm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "EIN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Ja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Nein"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "An"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Aus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Aktiviert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Deaktiviert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "Nicht verfügbar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Gesperrt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Freigeschaltet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Ignorieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Nächster"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Linker Analogstick"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Rechter Analogstick"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "(Taste: %s)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Früh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Spät"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Hintergrundfüllerdicke aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Rahmenfüller aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Linearer Filter für Menü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Menü-Farbschema"
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Pixelgenaue Bildskalierung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Benutzerdefiniert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "AUS"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Dynamisches Hintergrundbild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Lade ein neues Hintergrundbild dynamisch, abhängig vom Inhalt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Horizontale Animation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Menü-Transparenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Menü-Schriftart"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Wähle eine andere Schriftart, die im Menü verwendet werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Menü-Design"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Wähle ein anderes Thema für das Menü aus. Änderungen werden übernommen, nachdem Du das Programm neu gestartet hast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Icon-Schatten aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Menü-Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Wähle einen animierten Effekt für das Hintergrundbild. Kann je nach Effekt GPU-lastig sein. Wenn die Leistung nicht ausreicht, wähle einen einfacheren Effekt oder deaktiviere diese Option."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Menü-Farbschema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Wähle einen anderen Farbverlauf für das Hintergrundbild."
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Aktiviere den Reiter 'Einstellungen'"
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Band"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Schnee (vereinfacht)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Schnee"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Benutzerdefiniert"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Menü-Farbschema"
   )

/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Menü-Icons"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Aktiviere/deaktiviere die Menü-Icons, die auf der linken Seite der Menü-Einträge angezeigt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Menü-Farbschema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Wähle einen anderen Farbverlauf für das Hintergrundbild."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Blau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Blau/Grau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Dunkelblau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Grün"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Rot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Gelb"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "EIN"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Core laden..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Vorschau der Shader-Parameter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Benutzerdefiniert..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Hilfe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Core laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Dateibrowser"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Bildschirmfotos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Core-Informationen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informationen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Fehler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Stopp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Leeren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Wiedergabeliste löschen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Cores:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Inhaltsdatenbank:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Zurücksetzen"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Aktualisierungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Errungenschaften-Konten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Endpunkt der Benutzerkonten-Liste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
   "Retro-Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Core-Zähler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Frontend-Zähler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Horizontales Menü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Verstecke nicht zugewiesene Eingabe-Beschriftungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Zeige Eingabe-Bezeichnungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Overlays"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Inhalt aus Verlauf laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Keine Netplay-Hosts gefunden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Keine Leistungszähler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Keine Playlists."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Abspielen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "Sekunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Core starten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Vorgeschlagene Cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Komprimiertes Archiv kann nicht gelesen werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Benutzer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Maximale Anzahl von Zwischenbildern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Zwingt den Videotreiber dazu, einen bestimmten Framebuffer zu verwenden."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Modizifiert die Shader-Voreinstellung, die momentan im Menü geladen ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Shader-Voreinstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Besuche URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "URL-Pfad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Kompatibler Inhalt gefunden"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "Automatisches Bildseitenverhältnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Aktiviere System-BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
   "Benutzerdefiniertes Verhältnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Aktiviere Aufnahmefunktion"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "Speichere Aufnahme als..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "Speichere Aufnahme im Ausgabeverzeichnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "Erzwinge Bildseitenverhältnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "Wähle aus Playlist"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME,
   "Fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME,
   "Setze die aktuelle Anwendung wieder fort und verlasse das Spielmenü."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "Transparenz der Fußzeile"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "Ändere die Transparenz der Footer-Grafik."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "Transparenz der Kopfzeile"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "Ändere die Transparenz der Header-Grafik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Aktiviere Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "Starte Inhalt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Pfad zur Verlaufsliste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Ausgangs Bildschirm ID"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Wähle den Ausgangsport aus, welcher mit dem CRT Bildschirm verbunden ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Hilfe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Audio/Video-Fehlerbehebung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "Inhalt laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "Nach Inhalten suchen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "Was ist ein Core?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Datenbanken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Netplay-Verzögerung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Lokales Netzwerk durchsuchen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Suche und verbinde mit Netplay-Hosts in deinem lokalen Netzwerk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Netplay-Client aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Beobachtermodus aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Beschreibung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "Begrenze maximale Ausführgeschwindigkeit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Errungenschaften (Hardcore)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Cheat-Durchläufe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Erhöhe oder verringere die Anzahl der Cheats."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Linker Analogstick X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Linker Analogstick Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Rechter Analogstick X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Rechter Analogstick Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Datenbank-Suchanfragen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Datenbank - Filter : Entwickler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Datenbank - Filter : Publisher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Datenbank - Filter : Herkunft"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Datenbank - Filter : Franchise"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Datenbank - Filter : ESRB-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Datenbank - Filter : ELSPA-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Datenbank - Filter : PEGI-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Datenbank - Filter : CERO-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Datenbank - Filter : BBFC-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Datenbank - Filter : Max. Spieler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Datenbank - Filter : Veröffentlichungsdatum nach Monat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Datenbank - Filter : Veröffentlichungsdatum nach Jahr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Datenbank - Filter : Edge Magazine-Ausgabe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Datenbank - Filter : Edge Magazine-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Datenbank-Informationen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Netplay-Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Slang-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "Unterstützung für OpenGL/Direct3D Render-to-Texture (Multi-Pass Shader)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Inhalt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "Wird normalerweise von Entwickler verwendet, die libretro/RetroArch-Apps paketieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Nachfragen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Grundlegende Menüsteuerung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Bestätigen/OK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "Beenden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Nach oben scrollen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Standardwerte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Tastatur ein-/ausschalten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Menü ein-/ausschalten"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "Im Menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "Im Spiel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "Im Spiel (Pausiert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Wiedergebend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Pausiert"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netplay wird gestartet, sobald ein Inhalt geladen wurde."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Konnte keinen geeigneten Core oder Inhalt finden, lade manuell."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Globale Lautstärke des Audio-Mixers"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Netplay-Suche abgeschlossen."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Entschuldigung, nicht implementiert: Cores, die keinen Inhalt verlangen, können nicht an Netplay teilnehmen."
   )
MSG_HASH(
   MSG_NATIVE,
   "Nativ"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "Gerät von Anschluss getrennt"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Unbekannter Netplay-Befehl wurde empfangen"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Datei existiert bereits. Speichere im Backup-Puffer"
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Öffentliche Addresse"
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Portzuordnung fehlgeschlagen"
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Lege Datenträger in Laufwerksschublade ein"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Warte auf Client ..."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Du hast das Spiel verlassen"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "Du bist als Spieler %u beigetreten"
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Dieser Core unterstützt kein Netplay zwischen diesen Systemen"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Dieser Core unterstützt kein Netplay zwischen verschiedenen Systemen"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Gib das Server-Passwort ein:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Falsches Passwort"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Die Verbindung mit einem Netplay-Client wurde getrennt"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Verbindung mit Netplay getrennt"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "Du hast nicht die Berechtigung, an diesem Spiel teilzunehmen"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "Es gibt keine freien Spieler-Plätze mehr"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Konnte nicht zum Spieler-Modus wechseln"
   )
MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Audio-Lautstärke"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Automatisch erkennen"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Lade Zustandsdaten (Savestates) automatisch von"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Fähigkeiten"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Verbinde zu Netplay-Host"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Verbinde zu Port"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Verbindungs-Slot"
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Zu Favoriten hinzugefügt"
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Datenträger hinzufügen"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Anwendungen"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Änderungen übernehmen."
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Shader anwenden"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Audio stummgeschaltet."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Audio laut."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Speichern der Autokonfigurations-Datei fehlgeschlagen."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Autokonfigurations-Datei erfolgreich gespeichert."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Initialisierung der Autospeichern-Funktion fehlgeschlagen."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Zustand automatisch speichern in"
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Überschreiben des SRAM blockieren"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Starte Befehlsinterface auf Port"
   )
MSG_HASH(
   MSG_BYTES,
   "Bytes"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Kann neuen Konfigurationspfad nicht ableiten. Verwende aktuelle Zeit."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Hardcore-Modus aktiviert. Spielstände & die Rückspul-Funktion wurden ausgeschaltet."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "Vergleiche mit bekannten Magic Numbers..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Kompiliert gegen API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Konfigurations-Verzeichnis nicht definiert. Kann neue Konfiguration nicht speichern."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Verbunden mit"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "CRC32-Prüfsummen des Inhalts weichen ab. Andere Spiele können nicht verwendet werden."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Laden des Inhalts übersprungen. Die Implementierung wird ihn selbst laden."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Core unterstützt keine Zustandsdaten (Savestates)."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Core-Optionsdatei wurde erfolgreich erstellt."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Konnte keinen nächsten Treiber finden"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Konnte kein kompatibles System finden"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Konnte keinen gültigen Daten-Track finden"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "Konnte Daten-Track nicht öffnen"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "Konnte Inhalts-Datei nicht lesen"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Konnte Film-Header nicht lesen."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Konnte Status der Filmdatei nicht lesen."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "CRC32-Prüfsumme des Inhalts und die gespeicherte Prüfsumme der Replay-Datei stimmen nicht überein. Wird die Aufzeichnung wiedergegeben, wird sie sehr wahrscheinlich asynchron."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Benutzerdefiniertes Timing gefunden"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Dekompression läuft bereits."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Dekompression fehlgeschlagen."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Erkannte Bildbereich von"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Kein gültiger Patch für diesen Inhalt gefunden."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Gerät von einem gültigen Anschluss trennen."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Geschlossen"
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Ausgeworfen"
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Herunterladen von"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Herunterladen fehlgeschlagen"
   )
MSG_HASH(
   MSG_ERROR,
   "Fehler"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Libretro-Core benötigt Inhalt, es wurde jedoch keiner bereitgestellt."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Libretro-Core benötigt speziellen Inhalt, es wurde jedoch keiner bereitgestellt."
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Fehler beim Verarbeiten der Argumente."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Fehler beim Speichern der Core-Optionen"
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Fehler beim Speichern der Tasten-Umbelegungen."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Fehler beim Löschen der Tasten-Umbelegungen."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Fehler beim Speichern der Shader-Voreinstellungen."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Externe Anwendungen"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Entpacke"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Entpacke Datei"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Fehler beim Speichern der Konfiguration in"
   )
MSG_HASH(
   MSG_FAILED_TO,
   "Fehler beim"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Ankommender Beobachter konnte nicht akzeptiert werden."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Fehler beim zuweisen des Arbeitsspeichers für den gepatchten Inhalt..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Fehler beim Anwenden des Shaders."
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Fehler beim binden des Sockets."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Fehler beim Erstellen des Verzeichnisses."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Fehler beim Entpacken des Inhalts aus dem Archiv"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Benutzername konnte vom Client nicht gelesen werden."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Laden fehlgeschlagen"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Laden des Inhalts fehlgeschlagen"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Laden der Filmdatei fehlgeschlagen"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Laden des Overlays fehlgeschlagen."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Fehler beim Laden des Spielstands von"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Öffnen des Libretro-Cores fehlgeschlagen"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Patch fehlgeschlagen"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Header konnte nicht vom Client empfangen werden."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Empfangen des Nickname fehlgeschlagen."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Empfangen des Nickname vom Host fehlgeschlagen."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Länge des Nickname wurde vom Host nicht empfangen."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "SRAM-Daten wurden vom Host nicht empfangen."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Auswerfen des Datenträgers fehlgeschlagen."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Fehler beim entfernen der temporären Datei"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "Fehler beim Speichern des SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Fehler beim Speichern des Spielstands in"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Fehler beim Senden des Nickname."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Fehler beim Senden der Nickname-Länge."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Fehler beim Senden des Nickname zum Client."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Fehler beim Senden des Nickname zum Host."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "Fehler beim Senden der SRAM-Daten zum Client."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Audiotreiber konnte nicht gestartet werden. Fahre ohne Audio fort."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Konnte Film-Aufzeichnung nicht starten."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Konnte Aufzeichnung nicht starten."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Konnte Bildschirmfoto nicht erstellen."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Laden des Spielstands konnte nicht rückgängig gemacht werden."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Speichern des Spielstands konnte nicht rückgängig gemacht werden."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Audio konnte nicht lautgeschaltet werden."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Fataler Fehler in"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Datei nicht gefunden"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Spielstand gefunden in"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Datenträger-Label gefunden"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Ersten Daten-Track in der Datei gefunden"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Letzten Speicherplatz gefunden"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Shader gefunden"
   )
MSG_HASH(
   MSG_FRAMES,
   "Bilder"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Spieloptionen: Spielspezifische Core-Optionen gefunden in"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Ungültiger Datenträger-Index."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Maus-Status"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Spielfokus ein"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Spielfokus aus"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Der Libretro-Core ist Hardware-gerendert. GPU-Aufzeichnung muss ebenfalls aktiviert werden."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "Prüfsumme entspricht nicht CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Cheat eingeben"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Cheat-Dateiname"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Vorlagen-Dateiname"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Titel umbenennen"
   )
MSG_HASH(
   MSG_INTERFACE,
   "Netzwerk-Karte"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Interner Speicher"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Wechseldatenträger"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Ungültige Nickname-Länge."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "in Bytes"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "in Megabyte"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "in Gigabyte"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "ist gegen eine andere libretro-Version als diese kompiliert."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Frontend für libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Spielstand aus Speicherplatz #%d geladen."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Spielstand aus Speicherplatz #-1 (auto) geladen."
   )
MSG_HASH(
   MSG_LOADING,
   "Laden..."
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Eine oder mehrere Firmware-Dateien fehlen"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Lade Inhalts-Datei"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Lade Verlaufs-Datei"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Lade Spielstand"
   )
MSG_HASH(
   MSG_MEMORY,
   "Speicher"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
   "Filmdatei ist keine gültige BSV1-Datei."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Filmdatei besitzt offenbar eine andere Serializer-Version. Wird höchstwahrscheinlich fehlschlagen."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Film-Wiedergabe beendet."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Beende Film-Aufzeichnung."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Netplay-Initialisierung fehlgeschlagen."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Kein Inhalt, starte Dummy-Core."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Kein Spielstand wurde bis jetzt überschrieben."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Kein Spielstand wurde bis jetzt geladen."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Fehler beim Speichern der Überschreibungen."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Überschreibungen erfolgreich gespeichert."
   )
MSG_HASH(
   MSG_PAUSED,
   "Pausiert."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Lese ersten Daten-Track..."
   )
MSG_HASH(
   MSG_RECEIVED,
   "empfangen"
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Aufzeichnung wurde wegen Größenänderung beendet."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Aufzeichen in"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Cheat-Datei umleiten in"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Speicherdaten umleiten in"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Spielstand umleiten in"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Remap-Datei wurde erfolgreich gespeichert."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Remap-Datei wurde erfolgreich entfernt."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Datenträger aus Laufwerk entfernt."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Entferne temporäre Inhalts-Datei"
   )
MSG_HASH(
   MSG_RESET,
   "Zurücksetzen"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Starte Aufzeichnung neu, da Treiber reinitialisiert wurde."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Alter Spielstand wiederhergestellt"
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Shader: Standard-Shader-Voreinstellung wiederhergestellt zu"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Setze Speicherdaten-Verzeichnis zurück auf"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Setze Spielstand-Verzeichnis zurück auf"
   )
MSG_HASH(
   MSG_REWINDING,
   "Zurückspulen."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Initialisiere Rückspul-Puffer mit Größe"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Fehler beim Initialisieren des Rückspul-Puffers. Zurückspulen wird deaktiviert."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "Implementierung verwendet separaten Audio-Thread. Zurückspulen kann nicht verwendet werden."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Ende des Rückspul-Puffers erreicht."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Neue Konfiguration gespeichert in"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Spielstand in Speicherplatz #%d gespeichert."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Spielstand in Speicherplatz #-1 (auto) gespeichert."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Erfolgreich gespeichert in"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Speichere RAM-Typ"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Speichere aktuellen Zustand (Savestate)"
   )
MSG_HASH(
   MSG_SCANNING,
   "Durchsuchen"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Durchsuchen des Verzeichnisses beendet"
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Sende Befehl"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Mehrere Patches wurden explizit definiert, ignoriere alle..."
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Shader-Voreinstellung erfolgreich gespeichert."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Überspringe Laden des SRAM."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Zeitlupe."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Snel vooruit."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "In Zeitlupe zurückspulen."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM wird nicht gespeichert."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Starte Film-Wiedergabe."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Starte Film-Aufzeichnung in"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Spielstand-Größe"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Spielstand-Speicherplatz"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Erstelle Bildschirmfoto."
   )
MSG_HASH(
   MSG_TO,
   "nach"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Laden des Spielstands wurde rückgängig gemacht."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Speichern des Spielstands wurde rückgängig gemacht."
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Nicht pausiert."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Unbekannter Befehl"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Verwende Core-Namen für neue Konfiguration."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Verwende libretro-Dumy-Core. Überspringe Aufzeichnung."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Verbinde Gerät mit einem gültigen Anschluss."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Trenne Gerät von Anschluss"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Neustart..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Beenden..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Version der libretro-API"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Berechnung der Bildgröße fehlgeschlagen! Wird unter Verwendung von Rohdaten fortfahren. Dies wird wahrscheinlich nicht richtig funktionieren ..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY,
   "Virtuelle Laufwerksschublade."
   )
MSG_HASH(
   MSG_FAILED,
   "fehlgeschlagen"
   )
MSG_HASH(
   MSG_SUCCEEDED,
   "erfolgreich"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "nicht konfiguriert"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "nicht konfiguriert, verwende Rückfalloption"
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Suche nach WLAN-Netzwerken abgeschlossen."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Suche nach WLAN-Netzwerken..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Suche nach Netplay-Hosts..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Suchvorgang vorbereiten..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Passwort eingeben"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Passwort korrekt."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Passwort falsch."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Passwort eingeben"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Passwort korrekt."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Passwort falsch."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Scannen von Datei beendet"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Durchsuchen: "
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Lakka aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Frontend-Name"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Lakka-Version"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Neustart"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
   "Status des internen Speichers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Bildschirm-Auflösung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Beenden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Flacker-Filter aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Gamma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Soft-Filter aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
   "WLAN"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Sucht nach drahtlosen Netzwerken und stellt eine Verbindung her."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Entflackern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "VI-Bildgröße"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Dauerleistungsmodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Verwende PAL60-Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "RetroArch neustarten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Warte auf Audio-Frames"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Touch-Eingabe auf der Vorderseite bevorzugen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Touch-Eingabe aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Kleine Tastatur aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Zeige 'Neu starten'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Zeige/verstecke die Option 'Neu starten'."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
   "GPU Übertakten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE,
   "Über- oder Untertakten der Switch GPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_BACKLIGHT_CONTROL,
   "Bildschirmhelligkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_BACKLIGHT_CONTROL,
   "Anpassen der Switch Bildschirmhelligkeit"
   )
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "CPU Übertakten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Übertakten der Switch CPU"
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
   "Bluetooth aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Aktiviere oder deaktiviere Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Verwalte Service-Einstellungen deines Betriebssystems."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "SAMBA aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Aktiviere die Netzwerkfreigabe deiner Ordner."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "SSH aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Aktiviere Fernzugriff auf die Kommandozeile."
   )
#endif
#ifdef GEKKO
#endif
#ifdef HAVE_ODROIDGO2
#else
#endif
#if defined(_3DS)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "3DS-Bildschirm unten"
   )
#endif
#ifdef HAVE_QT
#endif

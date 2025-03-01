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
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Bilder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Musik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Netzwerkspiel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Erkunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Inhaltslose Cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Inhalte importieren"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Schnellmenü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Schnell auf alle relevanten Spieleinstellungen zugreifen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Core laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Den zu verwendenden Core wählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Nach einer Libretro-Core-Implementierung suchen. In welchem Verzeichnis der Browser beginnt, hängt von dem Core-Verzeichnispfad ab.\nIst als Core-Verzeichnis ein Ordner ausgewählt, wird dieser als Startverzeichnis genutzt. Ist das Core-Verzeichnis ein vollständiger Pfad, wird in dem Ordner begonnen, in dem sich die Datei befindet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Inhalt laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Den zu startenden Inhalt wählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Nach Inhalten suchen. Um Inhalte zu laden, wird ein passender „Core“ und eine Inhaltsdatei benötigt.\nWo das Menü mit der Suche nach Inhalten beginnt, kann über „Dateibrowser Verzeichnis“ eingestellt werden. Falls keines festgelegt ist, wird im Wurzelverzeichnis gestartet.\nDer Browser filtert Erweiterungen für den zuletzt in „Core laden“ eingestellten Core heraus und verwendet diesen Core, wenn der Inhalt geladen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Disc laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Eine physische Medien-Disc laden. Zuerst sollte der Core ausgewählt werden (\"Core laden\"), welcher mit der Disc genutzt werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Disc dumpen"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Physische Medien-Disc auf den internen Speicher dumpen. Sie wird als Abbild gespeichert."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Disc auswerfen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Wirft die Disc aus dem physischen CD/DVD-Laufwerk aus."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Beim Scannen gefundener Inhalt wird hier erscheinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Inhalte importieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Wiedergabelisten durch Scannen von Inhalten erstellen und aktualisieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Desktop-Menü anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Das traditionelle Desktop-Menü öffnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Kiosk Modus deaktivieren (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Alle Konfigurationseinstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Online-Updater"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Erweiterungen, Komponenten und Inhalte für RetroArch herunterladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Netzwerkspiel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Ein Netzwerkspiel hosten oder einem beitreten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Das Programm konfigurieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Systeminformationen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Konfigurationsdateien erstellen und verwalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Hilfe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Mehr darüber erfahren, wie RetroArch funktioniert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Neu starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "RetroArch-Anwendung neu starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Beenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "RetroArch-Anwendung beenden. Speichern der Konfiguration beim Beenden ist aktiviert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "RetroArch-Anwendung beenden. Speichern der Konfiguration beim Beenden ist deaktiviert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "RetroArch beenden. Durch das harte Beenden (SIGKILL usw.) des Programms wird RetroArch beendet, ohne die Konfiguration zu speichern in jedem Fall. Auf Unix-ähnlichen Systemen ermöglicht SIGINT/SIGTERM eine saubere Deinitialisierung, welche das Speichern der Konfigurationsdatei mit einschließt, falls aktiviert."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Core herunterladen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Cores vom Online-Updater herunterladen und installieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Core installieren oder wiederherstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Cores aus dem Download-Verzeichnis installieren oder wiederherstellen."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Videoprozessor starten"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Remote-RetroPad starten"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Startverzeichnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Archiv durchsuchen"
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
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Zu \"Favoriten\" hinzugefügte Inhalte werden hier erscheinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Musik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Musik, die zuvor abgespielt wurde, wird hier erscheinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Bilder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Bilder, die zuvor angezeigt wurden, werden hier erscheinen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Videos, die zuvor abgespielt wurden, werden hier erscheinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Erkunden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Alle mit der Datenbank übereinstimmenden Inhalte über eine kategorisierte Oberfläche durchsuchen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Inhaltslose Cores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Installierte Cores, die ohne Laden von Inhalten arbeiten können, werden hier angezeigt."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Core-Downloader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Installierte Cores aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Alle installierten Cores auf die neueste verfügbare Version aktualisieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Cores auf Play-Store-Versionen umstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Alle älteren und manuell installierten Cores durch die neuesten Versionen aus dem Play Store ersetzen, sofern verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Vorschaubild-Updater"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Ein komplettes Vorschaubilderpaket für das ausgewählte System herunterladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Wiedergabelisten Vorschaubild-Updater"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Vorschaubilder für Einträge in der ausgewählten Wiedergabeliste herunterladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Inhalts-Downloader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Kostenlose Inhalte für den ausgewählten Core herunterladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Core-Systemdateien-Downloader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Zusätzliche Systemdateien herunterladen, die für den korrekten/optimalen Betrieb von Cores erforderlich sind."
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
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Controller-Profile aktualisieren"
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
   "Cg-Shader aktualisieren"
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
   "Informationen über die Anwendung/den Core ansehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Disc-Information"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Informationen über eingelegte Medien-Discs ansehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Netzwerk-Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Netzwerkschnittstelle(n) und zugehörige IP-Adressen ansehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "System-Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Informationen über dieses Gerät ansehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Datenbankmanager"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Datenbanken ansehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Suchanfragenmanager"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Vorherige Suchanfragen ansehen."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Core-Name"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Core-Label"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Core-Version"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Systemname"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Systemhersteller"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Kategorien"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Autor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Berechtigungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Lizenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Unterstützte Erweiterungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Benötigte Grafik-API"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Vollständiger Pfad"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Savestate-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Nein"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Grundlegend (Speichern/Laden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Serialisiert (Speichern/Laden, Zurückspulen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Deterministisch (Speichern/Laden, Zurückspulen, Run-Ahead, Netplay)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "- Hinweis: 'Systemdateien befinden sich im Verzeichnis des Inhalts' ist derzeit aktiviert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "Schauen in: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Fehlt, erforderlich:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Fehlt, optional:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Vorhanden, erforderlich:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Vorhanden, optional:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Installierten Core sperren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Änderungen am aktuell installierten Core verhindern. Kann verwendet werden, um unerwünschte Updates zu vermeiden, wenn Inhalte eine bestimmte Core-Version benötigen (z. B. Arcade-ROM-Sets)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Von „Inhaltslose Cores“ ausschließen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Verhindern, dass dieser Core auf der Registerkarte/im Menü „Inhaltslose Cores“ angezeigt wird. Gilt nur, wenn der Anzeigemodus auf „Benutzerdefiniert“ eingestellt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Core löschen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Diesen Core vom Datenträger löschen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Core-Backup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Eine Sicherungskopie des aktuell installierten Cores erstellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Backup wiederherstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Eine vorherige Version des Cores aus einer Liste von Sicherungskopien installieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Backup löschen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Eine Datei aus der Liste der Sicherungskopien entfernen."
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Kompilierungsdatum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "RetroArch-Version"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Git-Version"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "CPU-Modell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "CPU-Funktionen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "CPU-Architektur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "CPU-Kerne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT verfügbar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Frontend-Kennung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Frontend-Betriebssystem"
   )
MSG_HASH( /* FIXME Maybe add a description? */
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
   "Anzeigenbreite (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Anzeigenhöhe (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Anzeigen-DPI"
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
   "Befehlsschnittstellen-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Netzwerkbefehlsschnittstellen-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Netzwerkcontroller-Unterstützung"
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
   "SDL-1.2-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "SDL-2-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D8_SUPPORT,
   "Direct3D-8-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D9_SUPPORT,
   "Direct3D-9-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D10_SUPPORT,
   "Direct3D-10-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D11_SUPPORT,
   "Direct3D-11-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D12_SUPPORT,
   "Direct3D-12-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GDI_SUPPORT,
   "GDI-Unterstützung"
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
   "udev-Unterstützung"
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
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PIPEWIRE_SUPPORT,
   "PipeWire-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "CoreAudio-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "CoreAudio-V3-Unterstützung"
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
   "zlib-Unterstützung"
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
   "Dynamisches Laden der Libretro-Bibliothek zur Laufzeit"
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
   "mpv-Unterstützung"
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
   "STB-TrueType-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Netzwerkspiel-Unterstützung (Peer-to-Peer)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Video4Linux2-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SSL_SUPPORT,
   "SSL-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "libusb-Unterstützung"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Datenbankauswahl"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Titel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Beschreibung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Kategorie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Sprache"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Konsolenexklusiv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Plattformexklusiv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Medium"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Steuerung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Grafikstil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Handlung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Perspektive"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Ansicht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Fahrzeugtyp"
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
   "Famitsu-Magazine-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Edge-Magazine-Testbericht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Edge-Magazine-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Edge-Magazine-Ausgabe"
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
   "Vibration unterstützt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Co-op unterstützt"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Konfigurationsdatei laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Bestehende Konfigurationsdatei laden und aktuelle Werte ersetzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Aktuelle Konfiguration speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Aktuelle Konfigurationsdatei überschreiben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Neue Konfiguration speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Aktuelle Konfiguration als gesonderte Datei speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Auf Standardwerte zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Die aktuelle Konfiguration auf die Standardwerte zurücksetzen."
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
   "Die von diesem System verwendeten Treiber ändern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Videoausgabe-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Audioeinstellungen für Ein- und Ausgabe ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Eingabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Controller-, Tastatur- und Mauseinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Einstellungen für Video-, Audio- und Eingabelatenz ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Cores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Core-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Die Standardeinstellungen für Konfigurationsdateien ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Einstellungen der Speicherfunktion ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Cloud-Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Cloud-Synchronisationseinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Cloud-Synchronisation aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Es wird versucht, Konfigurationen, SRAM und Zustände mit einem Cloud-Speicheranbieter zu synchronisieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Destruktive Cloud-Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Synchronisieren: Speicherdaten/Savestates"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Synchronisieren: Konfigurationsdateien"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Sync: Vorschaubilder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Sync: Systemdateien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "Wenn aktiviert, werden Speicherdaten/Savestates mit der Cloud synchronisiert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "Wenn aktiviert, werden Konfigurationsdateien mit der Cloud synchronisiert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "Wenn aktiviert, werden Miniaturbilder mit der Cloud synchronisiert. Allgemein nicht empfohlen, außer für große Sammlungen benutzerdefinierter Miniaturbilder. Andernfalls ist der Downloader eine bessere Wahl."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "Wenn aktiviert, werden die Systemdateien mit der Cloud synchronisiert. Dadurch kann sich die Zeit zum Synchronisieren erheblich verlängern; daher mit Vorsicht verwenden."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Wenn deaktiviert, werden die Dateien in einen Sicherungsordner verschoben, bevor sie überschrieben oder gelöscht werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Cloud-Sync-Backend"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Welches Cloud-Speicher-Netzwerkprotokoll verwendet werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "Cloud-Speicher-URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "Die URL für den Zugangspunkt zur API des Cloud-Speicherdienstes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Benutzername"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "Der Benutzername für das eigene Cloud-Speicher-Konto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Passwort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "Das Passwort für das eigene Cloud-Speicher-Konto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Protokollierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Protokoll-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Dateibrowser"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Dateibrowsereinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Konfigurationsdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Komprimierte Archivdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Aufzeichnungs-Konfigurationsdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Datei mit Datenbank-Suchanfragen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Konfigurationsdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Shader-Voreinstellungsdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Shaderdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Eingabezuordnungsdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Cheatdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Overlaydatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Datenbankdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "TrueType-Schriftartendatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Einfache Datei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Video. Auswählen, um diese Datei mit dem Videoplayer zu öffnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Musik. Auswählen, um diese Datei mit dem Musikplayer zu öffnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Bilddatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Bild. Auswählen, um diese Datei mit dem Bildbetrachter zu öffnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Libretro-Core. Bei Auswahl wird dieser Core dem Spiel zugeordnet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Libretro-Core. Auswählen, um diesen Core in RetroArch zu laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Verzeichnis. Auswählen, um dieses Verzeichnis zu öffnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Bildwiederholratenbegrenzung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Einstellungen für Zurückspulen, Vorspulen und Zeitlupe ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Aufnahme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Einstellungen für die Aufnahme-Funktion ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Bildschirmanzeige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Einstellungen für Overlays, Bildschirmtastatur und Benachrichtigungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Benutzeroberfläche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Benutzeroberflächen-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "KI-Dienst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Einstellungen für den KI-Dienst ändern (Übersetzung/TTS/Sonstiges)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Bedienungshilfe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Einstellungen für das Vorlesen von Text ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Energieverwaltung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Energieverwaltungseinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Einstellungen für Errungenschaften ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Netzwerk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Server- und Netzwerkeinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Wiedergabelisteneinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Benutzer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Konten, Benutzername und Sprache ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Verzeichnisse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Die Standard-Verzeichnisse für diverse Daten ändern."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Zuordnung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Medium"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Leistung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Spezifikation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Speicher"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Einstellungen für Steam ändern."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Eingabetreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Zu verwendender Eingabetreiber. Einige Videotreiber können einen anderen Eingabetreiber erzwingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "Der udev-Treiber liest evdev-Ereignisse für Tastaturunterstützung. Er unterstützt auch Tastaturrückrufe, Mäuse und Touchpads.\nIn den meisten Distributionen sind /dev/input-nodes standardmäßig root-only (Modus 600). Es kann eine udev-Regel eingerichtet werden, die diese für non-root zugänglich macht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "Der Eingabetreiber linuxraw benötigt einen aktiven TTY. Tastaturereignisse werden direkt vom TTY gelesen, was es einfacher macht, aber nicht so flexibel wie udev. Mäuse usw. werden überhaupt nicht unterstützt. Dieser Treiber verwendet die ältere Joystick-API (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Eingabetreiber. Der Videotreiber kann einen anderen Eingabetreiber erzwingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Controllertreiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Zu verwendender Controller. (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "DirectInput-Controller-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "Low-Level Human Interface Device-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Raw-Linux-Treiber, verwendet veraltete Joystick-API. Stattdessen udev verwenden, wenn möglich."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Linux-Treiber für Controller, angeschlossen am Parallelport über spezielle Adapter."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Controller-Treiber basierend auf SDL-Bibliotheken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Controller-Treiber mit udev-Schnittstelle, allgemein empfohlen. Verwendet die neueste evdev-Joypad-API zur Unterstützung von Joysticks. Er unterstützt Hotplugging und Force Feedback.\nIn den meisten Distributionen sind /dev/input-nodes standardmäßig root-only (Modus 600). Es kann eine udev-Regel eingerichtet werden, die diese für non-root zugänglich macht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "XInput-Controller-Treiber. Meistens für XBox-Controller."
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
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "OpenGL 1.x Treiber. Mindestversion erforderlich: OpenGL 1.1. Unterstützt keine Shader. Stattdessen neuere OpenGL- Treiber verwenden, sofern möglich."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "OpenGL 2.x Treiber. Dieser Treiber ermöglicht die Verwendung von libretro GL Cores zusätzlich zu softwaregerenderten Cores. Mindestversion erforderlich: OpenGL 2.0 oder OpenGLES 2.0. Unterstützt das GLSL-Shaderformat. Stattdesssen glcore-Treiber verwenden, sofern möglich."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "OpenGL 3.x Treiber. Dieser Treiber ermöglicht die Verwendung von libretro-GL-Cores zusätzlich zu softwaregerenderten Cores. Erforderliche Mindestversion: OpenGL 3.2 oder OpenGLES 3.0+. Unterstützt das Slang-Shaderformat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Vulkan-Treiber. Dieser Treiber ermöglicht die Verwendung von libretro Vulkan-Kernen zusätzlich zu softwaregerenderten Cores. Erforderliche Mindestversion: Vulkan 1.0. Unterstützt HDR- und Slang-Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "SDL 1.2 software-gerenderter Videotreiber. Die Leistung wird als suboptimal angesehen. Der Einsatz sollte nur als letzter Ausweg ansehen werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "SDL 2 softwaregerenderter Videotreiber. Leistung für software-gerenderte libretro-Core-Implementierungen ist abhängig von der Implementierung der SDL-Plattform."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Metall-Treiber für Apple-Plattformen. Unterstützt das Slang-Shaderformat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Direct3D-8-Treiber ohne Shader-Unterstützung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Direct3D-9-Treiber mit Unterstützung für das alte Cg-Shaderformat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Direct3D-9-Treiber mit Unterstützung für das HLSL-Shaderformat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Direct3D-10-Treiber mit Unterstützung für das Slang-Shaderformat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Direct3D-11-Treiber mit Unterstützung für HDR und das Slang-Shaderformat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Direct3D-12-Treiber mit Unterstützung für HDR und das Slang-Shaderformat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "DispmanX-Treiber. Verwendet die DispmanX-API für Videocore-IV-GPU in Raspberry Pi 0..3. Keine Overlay- oder Shader-unterstützung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "LibCACA-Treiber. Erzeugt Zeichenausgabe statt Grafik. Für den praktischen Gebrauch nicht empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Ein Low-Level-Exynos-Videotreiber, der den G2D-Block in Samsung Exynos SoC für Blittingoperationen verwendet. Leistung für sofwaregerenderte Kerne sollte optimal sein."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Einfacher DRM-Video-Treiber. Dies ist ein Low-Level-Videotreiber, der libdrm für Hardwareskalierung mit GPU-Overlays verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Ein Low-Level-Sunxi-Videotreiber, der den G2D-Block in Allwinner SoCs verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Wii-U-Treiber. Unterstützt Slang-Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Switch-Treiber. Unterstützt das GLSL-Shaderformat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "OpenVG-Treiber. Verwendet die OpenVG-hardwarebeschleunigte 2D-Vektorgrafik-API."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "GDI-Treiber. Verwendet eine veraltete Windows-Schnittstelle. Nicht empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Aktueller Videotreiber."
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
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "RSound-Treiber für vernetzte Audiosysteme."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Legacy-Open-Sound-System-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Standard ALSA-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "ALSA-Treiber mit Threading-Unterstützung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "ALSA-Treiber ohne Abhängigkeiten implementiert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "RoarAudio Sound-Systemtreiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "OpenAL-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "OpenSL-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "DirectSound wird hauptsächlich von Windows 95 bis Windows XP verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Windows-Audio-Session-API-Treiber. WASAPI wird hauptsächlich ab Windows 7 verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "PulseAudio-Treiber. Wenn das System PulseAudio verwendet, ist dieser Treiber anstelle von z. B. ALSA zu verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "PipeWire-Treiber. Falls das System PipeWire verwendet, sollte dieser Treiber anstelle von beispielsweise PulseAudio verwendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Jack-Audio-Connection-Kit-Treiber."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Mikrofon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Zu verwendender Mikrofontreiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Mikrofon Resampler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Zu verwendender Mikrofon-Resampling-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Mikrofon-Blockframes"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Audio-Resampling-Treiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Zu verwendender Audio-Resampling-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Fenster-Sinc-Implementierung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Kosinusfaltung-Implementierung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Nächstes-Resampling-Implementierung. Dieser Resampler ignoriert die Qualitätseinstellung."
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
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_DRIVER,
   "Bluetooth-Treiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Zu verwendender Bluetooth-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
   "WLAN-Treiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Zu verwendender WLAN-Treiber."
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
   "Zu verwendender Menütreiber. (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB ist eine RetroArch-GUI, die wie ein Konsolenmenü der 7. Generation aussieht. Sie kann dieselben Funktionen wie Ozone unterstützen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone ist die Standard-GUI von RetroArch auf den meisten Plattformen. Sie ist für die Navigation mit einem Spielcontroller optimiert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI ist eine einfache integrierte GUI für RetroArch. Sie hat die niedrigsten Leistungsanforderungen unter den Menütreibern und kann auf Bildschirmen mit niedriger Auflösung verwendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "Auf mobilen Geräten verwendet RetroArch standardmäßig das mobile UI, MaterialUI. Diese Schnittstelle ist für Touchscreen und Zeigegeräte wie eine Maus/Trackball konzipiert."
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
   MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
   "MIDI-Treiber"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Zu verwendender MIDI-Treiber."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Native Signale mit niedgriger Auflösung zur Verwendung mit CRT-Bildschirmen ausgeben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Ausgabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Videoausgabe-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Vollbildmodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Die Vollbildeinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Fenstermodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Fenstermodus-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Videoskalierungseinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Die Hochkontrastbildeinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Videosynchronisationseinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Bildschirmschoner unterdrücken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Die Aktivierung des Bildschirmschoners des Systems verhindern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Deaktiviert den Bildschirmschoner. Diese Einstellung wird möglicherweise vom Videotreiber nicht berücksichtigt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Video in separatem Thread"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Verbessert die Leistung auf Kosten der Latenz und erhöhtem Stottern vom Video. Nur verwenden, wenn sonst keine volle Geschwindigkeit erreicht werden kann."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Video in separatem Thread ausführen. Dies kann die Leistung auf Kosten der Latenz und eines stärkeren Videostotterns verbessern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Schwarzes Bild einfügen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "WARNUNG: Schnelles Flackern kann auf manchen Bildschirmen zu einem Nachleuchten des Bildes führen. Verwendung auf eigene Gefahr // Schwarze(n) Rahmen zwischen den Bildern einfügen. Kann die Bewegungsunschärfe durch Emulation der CRT-Abtastung stark reduzieren, allerdings auf Kosten der Helligkeit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Fügt Schwarzbild(er) zwischen Frames für verbesserte Bewegungsschärfe ein. Nur die Option benutzen, die für die aktuelle Aktualisierungsrate bestimmt ist. Nicht für die Verwendung bei Aktualisierungsraten mit nicht multiplen 60 Hz wie 144 Hz, 165Hz, usw. Nicht kombinieren mit Swap-Intervall > 1, Unterbilder, Bildverzögerung oder mit exakter Inhaltssignalfrequenz synchronisieren. System-VRR eingeschaltet lassen ist ok, nur nicht diese Einstellung. Wenn -irgendein- temporäres Bild-Nachleuch[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_120,
   "1 - Für eine Bildwiederholfrequenz von 120 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_180,
   "2 - Für eine Bildwiederholfrequenz von 180 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_240,
   "3 - Für eine Bildwiederholfrequenz von 240 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_300,
   "4 - Für eine Bildwiederholfrequenz von 300 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_360,
   "5 - Für eine Bildwiederholfrequenz von 360 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_420,
   "6 - Für eine Bildwiederholfrequenz von 420 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_480,
   "7 - Für eine Bildwiederholfrequenz von 480 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_540,
   "8 - Für eine Bildwiederholfrequenz von 540 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_600,
   "9 - Für eine Bildwiederholfrequenz von 600 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_660,
   "10 - Für eine Bildwiederholfrequenz von 660 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_720,
   "11 - Für eine Bildwiederholfrequenz von 720 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_780,
   "12 - Für eine Bildwiederholfrequenz von 780 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_840,
   "13 - Für eine Bildwiederholfrequenz von 840 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_900,
   "14 - Für eine Bildwiederholfrequenz von 900 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_960,
   "15 - Für eine Bildwiederholfrequenz von 960 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Schwarzbild einfügen - Dunkle Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Passt die Anzahl der Schwarzbilder in Summe der BFI-Ausgabe-Sequenz an. Mehr entspricht einer höheren Bewegungsschärfe, weniger entspricht einer höheren Helligkeit. Nicht anwendbar bei 120 Hz, da es nur 1 BFI-Frame gibt, mit dem insgesamt gearbeitet werden kann. Die Einstellungen höher als möglich beschränken sich auf die maximal mögliche gewählte Aktualisierungsrate."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Stellt die Anzahl von Bildern in der BFI-Sequenz ein, welche schwarz sind. Mehr schwarze Bilder erhöhen die Bewegungsschärfe, verringern aber die Helligkeit. Nicht anwendbar bei 120 Hz, da es insgesamt nur einen Gesamtframe bei 60 Hz gibt, somit muß es schwarz sein, sonst wäre BFI überhaupt nicht aktiv."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Shader-Unterbilder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "WARNUNG: Schnelles Flackern kann auf manchen Bildschirmen zu einem Nachleuchten des Bildes führen. Verwendung auf eigene Gefahr // Simuliert eine einfache rollende Scanline über mehrere Unterbilder, indem der Bildschirm vertikal aufgeteilt wird und jeder Teil des Bildschirms entsprechend der Anzahl der Unterbilder dargestellt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Fügt zusätzlich Shader-Bild(er) zwischen Frames für alle möglichen Shader-Effekte ein, die darauf ausgelegt sind, schneller als die Inhaltsrate zu laufen. Nur die Option benutzen, die für die aktuelle Aktualisierungsrate bestimmt ist. Nicht für die Verwendung bei Aktualisierungsraten mit nicht multiplen 60 Hz wie 144 Hz, 165Hz, usw. Nicht kombinieren mit Swap-Intervall > 1, Schwarzbilder, Bildverzögerung oder mit exakter Inhaltssignalfrequenz synchronisieren. System-VRR eingeschaltet lass[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_120,
   "2 - Für eine Bildwiederholfrequenz von 120 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_180,
   "3 - Für eine Bildwiederholfrequenz von 180 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_240,
   "4 - Für eine Bildwiederholfrequenz von 240 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_300,
   "5 - Für eine Bildwiederholfrequenz von 300 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_360,
   "6 - Für eine Bildwiederholfrequenz von 360 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_420,
   "7 - Für eine Bildwiederholfrequenz von 420 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_480,
   "8 - Für eine Bildwiederholfrequenz von 480 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_540,
   "9 - Für eine Bildwiederholfrequenz von 540 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_600,
   "10 - Für eine Bildwiederholfrequenz von 600 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_660,
   "11 - Für eine Bildwiederholfrequenz von 660 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_720,
   "12 - Für eine Bildwiederholfrequenz von 720 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_780,
   "13 - Für eine Bildwiederholfrequenz von 780 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_840,
   "14 - Für eine Bildwiederholfrequenz von 840 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_900,
   "15 - Für eine Bildwiederholfrequenz von 900 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_960,
   "16 - Für eine Bildwiederholfrequenz von 960 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "GPU-Screenshot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Rollende Scanline-Simulation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "WARNUNG: Schnelles Flackern kann auf manchen Bildschirmen zu einem Nachleuchten des Bildes führen. Verwendung auf eigene Gefahr // Simuliert eine einfache rollende Scanline über mehrere Unterbilder, indem der Bildschirm vertikal aufgeteilt wird und jeder Teil des Bildschirms entsprechend der Anzahl der Unterbilder dargestellt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Simuliert eine einfache rollende Scanline über mehrere Unterbilder, indem der Bildschirm vertikal aufgeteilt wird und jeder Teil des Bildschirms entsprechend der Anzahl der Unterbilder vom oberen Bildschirmrand nach unten gerendert wird."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Screenshots erfassen GPU-schattiertes Material, falls verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Bilineare Filterung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Dem Bild eine leichte Unschärfe hinzufügen, um harte Pixelkanten zu glätten. Diese Option hat nur sehr geringe Auswirkungen auf die Leistung. Sollte bei Verwendung von Shadern deaktiviert werden."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Bildinterpolation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Die Bildinterpolationsmethode, die beim Skalieren von Inhalten über die interne IPU verwendet wird. Bei Verwendung von CPU-gestützten Videofiltern wird 'Bikubisch' oder 'Bilinear' empfohlen. Diese Einstellung hat keine Auswirkung auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bikubisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Nächste-Nachbarn"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Bildinterpolation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Zu verwendende Bildinterpolationsmethode, wenn \"Ganzzahlige Skalierung\" deaktiviert ist. \"Nächste-Nachbarn\" hat den geringsten Einfluss auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Nächste-Nachbarn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Semilinear"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Auto-Shader-Verzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Das automatische Laden von Shadern verzögern (in ms). Kann grafische Fehler beim Verwenden von 'Bildschirmaufnahme'-Software beheben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Videofilter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Einen CPU-gestützten Videofilter anwenden. Kann mit hohen Leistungskosten verbunden sein. Einige Videofilter funktionieren möglicherweise nur mit Cores, die 32-Bit- oder 16-Bit-Farben verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Einen CPU-gestützten Videofilter anwenden. Das kann mit einem hohen Leistungsaufwand verbunden sein. Einige Videofilter funktionieren möglicherweise nur für Cores, die 32-Bit- oder 16-Bit-Farben verwenden. Es können dynamisch verknüpfte Videofilterbibliotheken ausgewählt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Einen CPU-gestützten Videofilter anwenden. Das kann mit einem hohen Leistungsaufwand verbunden sein. Einige Videofilter funktionieren möglicherweise nur für Cores, die 32-Bit- oder 16-Bit-Farben verwenden. Es können interne Videofilterbibliotheken ausgewählt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Videofilter entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Alle aktiven CPU-gestützten Videofilter deaktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Vollbild über Notch auf Android und iOS-Geräten aktivieren"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Nur für CRT-Bildschirme. Versucht die exakte Core-/Spielauflösung und Bildfrequenz zu verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "CRT-Superauflösung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Zwischen nativer und ultraweiter Superauflösung umschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "X-Achsenzentrierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Versuche eine dieser Einstellungen, wenn das Bild nicht richtig auf dem Bildschirm zentriert ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Schwarzschultern anpassen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Versuche eine dieser Einstellungen, um die Schwarzschultern anzupassen und die Bildgröße zu ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Hochauflösendes Menü verwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Zu einer Modeline für den Einsatz mit hochauflösenden Menüs wechseln, wenn kein Inhalt geladen ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Benutzerdefinierte Bildfrequenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Eine in der Konfigurationsdatei definierte Bildwiederholfrequenz verwenden."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Monitor-Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Den Wiedergabebildschirm auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Welcher Monitor bevorzugt werden soll. 0 (Standard) bedeutet, dass kein bestimmter Monitor bevorzugt wird, 1 und höher (1 erster Monitor), schlägt RetroArch vor, diesen Monitor zu verwenden."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Für Wii U GamePad optimieren (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Eine exakte zweifache Skalierung des GamePads als Ansichtsfenster verwenden. Deaktivieren, um mit der nativen TV-Auflösung anzuzeigen."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Bildrotation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Erzwingt eine bestimmte Bildrotation. Diese Drehung wird zu den durch den Core festgelegten Drehungen hinzugefügt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Bildschirmausrichtung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Erzwingt eine bestimmte Ausrichtung des Bildschirms durch das Betriebssystem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "GPU-Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Zu verwendende Grafikkarte auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Horizontaler Versatz der Anzeige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Erzwingt einen horizontalen Versatz des Videos. Der Versatz wird global angewendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Vertikaler Versatz der Anzeige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Erzwingt einen vertikalen Versatz des Videos. Der Versatz wird global angewendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Vertikale Signalfrequenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Vertikale Signalfrequenz des Bildschirms. Wird verwendet, um eine passende Audioeingangsrate zu berechnen.\nDies wird ignoriert, wenn 'Video in separatem Thread' aktiviert ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Geschätzte Bildwiederholfrequenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Die geschätzte Bildwiederholfrequenz des Bildschirms in Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "Die genaue Bildwiederholrate des Monitors (Hz). Diese wird verwendet, um die Audioeingangsrate mit dieser Formel zu berechnen:\nAudiorate = Spieleingangsrate * Bildrate / Spielwiederholrate\nWenn kein Wert verfügbar ist, werden NTSC-Standardwerte angenommen.\nDieser Wert sollte nahe bei 60 Hz bleiben, um große Tonhöhenänderungen zu vermeiden. Wenn der Monitor nicht mit 60 Hz oder einem ähnlichen Wert läuft, VSync deaktivieren und hier den Standardwert belassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Bildschirm-eigene Wiederholfrequenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "Vom Displaytreiber angegebene Bildwiederholfrequenz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Automatischer Aktualisierungsratenwechsel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Bildschirmaktualisierungsrate abhängig vom aktuellen Inhalt automatisch umschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Nur im exklusiven Vollbildmodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Nur im randloses Vollbildfenster"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Alle Vollbildmodi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Automatische Aktualisierungsrate PAL-Schwellenwert"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Maximale Aktualisierungsrate, die als PAL eingestuft wird."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Vertikale Signalfrequenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Vertikale Signalfrequenz des Bildschirms festlegen. '50 Hz' ermöglicht flüssige Videowiedergabe von PAL-Inhalten."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Deaktivierung des sRGB FBO erzwingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Deaktivieren der sRGB-FBO-Unterstützung erzwingen. Einige Intel OpenGL-Treiber haben unter Windows Videoprobleme mit sRGB FBOs. Mit dieser Einstellung kann dies umgangen werden."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Im Vollbild starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Im Vollbildmodus starten. Kann zur Laufzeit geändert werden. Kann durch einen Befehlszeilenschalter überschrieben werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Randloses Vollbildfenster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Im Vollbildmodus ein Vollbildfenster bevorzugen, um ein Umschalten des Anzeigemodus zu verhindern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Vollbildbreite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Benutzerdefinierte Bildbreite für den Vollbildmodus. Wird dieser Wert nicht gesetzt, wird die Desktop-Auflösung verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Vollbildhöhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Benutzerdefinierte Bildhöhe für den Vollbildmodus. Wird dieser Wert nicht gesetzt, wird die Desktop-Auflösung verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Auflösung auf UWP erzwingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Auflösung auf Vollbildgröße erzwingen. Wenn auf 0 gesetzt, wird ein fester Wert von 3840 x 2160 verwendet."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Fenster-Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Die Fenstergröße auf das angegebene Vielfache der Größe der Core-Anzeige setzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Fenster-Deckkraft"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Fenstertransparenz festlegen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Fensterdekorationen anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Titelleiste und Ränder des Fensters anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Menüleiste anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Fenstermenüleiste anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Fensterposition und -größe merken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Den gesamten Inhalt in einem Fenster fester Größe anzeigen, dessen Abmessungen durch \"Fensterbreite\" und \"Fensterhöhe\" festgelegt sind, und die aktuelle Fenstergröße und -position beim Schließen von RetroArch speichern. Wenn deaktiviert, wird die Fenstergröße dynamisch basierend auf der \"Fenster-Skalierung\" festgelegt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Benutzerdefinierte Fenstergröße verwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Den gesamten Inhalt in einem Fenster fester Größe anzeigen, dessen Abmessungen durch \"Fensterbreite\" und \"Fensterhöhe\" festgelegt sind. Wenn deaktiviert, wird die Fenstergröße dynamisch basierend auf der \"Fenster-Skalierung\" festgelegt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Fensterbreite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Benutzerdefinierte Fensterbreite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Fensterhöhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Benutzerdefinierte Fensterhöhe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Maximale Fensterbreite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Die maximale Breite des Anzeigefensters bei der automatischen Größenanpassung auf Basis der 'Fenster-Skalierung' festlegen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Maximale Fensterhöhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Die maximale Höhe des Anzeigefensters bei der automatischen Größenanpassung auf Basis der 'Fenster-Skalierung' festlegen."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Ganzzahlige Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Video nur in Ganzzahlschritten skalieren. Die Grundgröße hängt von der vom Kern gemeldeten Geometrie und dem Seitenverhältnis ab."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "Ganzzahlige Skalenachse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Skaliert entweder Höhe oder Breite oder sowohl Höhe als auch Breite. Halbschritte gelten nur für hochauflösende Quellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Ganzzahlige Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Auf die nächste Ganzzahl auf- oder abrunden. „Intelligent“ wechselt zu Unterskalieren, wenn das Bild zu stark beschnitten wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "Unterskalieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "Überskalieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Intelligent"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Bildseitenverhältnis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Seitenverhältnis angeben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Seitenverhältnis konfigurieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Gleitkommawert für Videoseitenverhältnis (Breite:Höhe)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Wie von Core vorgesehen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Benutzerdefiniert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Voll"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Seitenverhältnis beibehalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "1:1 Pixelseitenverhältnis beibehalten, wenn der Inhalt über die interne IPU skaliert wird. Wenn deaktiviert, wird das Bild auf die gesamte Anzeige gestreckt."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "X-Position des Bildes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Benutzerdefinierter Viewport-Offset zur Definition der X-Achsenposition des Viewports."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Y-Position des Bildes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Angepasster Viewport-Offset zur Definition der Y-Achsenposition des Viewports."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Ansichtsfenster-Ankerpunkt-Ausrichtung X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Ansichtsfenster-Ankerpunkt-Ausrichtung X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Ansichtsfenster-Ankerpunkt-Ausrichtung Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Ansichtsfenster-Ankerpunkt-Ausrichtung Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Benutzerdefinierte Ausrichtung des Ansichtsfensters wird verwendet, um das Ansichtsfenster horizontal zu versetzen (wenn es breiter als die Inhaltshöhe ist). 0.0 bedeutet ganz links und 1.0 bedeutet ganz rechts."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Benutzerdefinierte Ausrichtung des Ansichtsfensters wird verwendet, um das Ansichtsfenster vertikal zu versetzen (wenn es höher als die Inhaltshöhe ist). 0.0 bedeutet oben und 1.0 bedeutet unten."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Ansichtsfenster-Ankerpunkt-Ausrichtung X (Hochformat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Ansichtsfenster-Ankerpunkt-Ausrichtung X (Hochformat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Ansichtsfenster-Ankerpunkt-Ausrichtung Y (Hochformat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Ansichtsfenster-Ankerpunkt-Ausrichtung Y (Hochformat)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Benutzerdefinierte Ausrichtung des Ansichtsfensters wird verwendet, um das Ansichtsfenster horizontal zu versetzen (wenn es breiter als die Inhaltshöhe ist). 0.0 bedeutet ganz links und 1.0 bedeutet ganz rechts. (Hochformat)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Benutzerdefinierte Ausrichtung des Ansichtsfensters wird verwendet, um das Ansichtsfenster vertikal zu versetzen (wenn es höher als die Inhaltshöhe ist). 0.0 bedeutet oben und 1.0 bedeutet unten. (Hochformat)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Bildbreite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Benutzerdefinierte Ansichtsfensterbreite, die verwendet wird, wenn das Bildseitenverhältnis auf \"Benutzerdefiniertes Seitenverhältnis\" eingestellt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Bildhöhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Benutzerdefinierte Ansichtsfensterhöhe, die verwendet wird, wenn das Bildseitenverhältnis auf \"Benutzerdefiniertes Seitenverhältnis\" eingestellt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Overscan zuschneiden (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Einige Pixel an den Bildrändern abschneiden, die üblicherweise von Entwicklern leer gelassen werden und manchmal auch Müll-Pixel enthalten."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "HDR aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "HDR aktivieren, wenn der Bildschirm es unterstützt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Maximale Leuchtdichte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Die Spitzenleuchtdichte (in cd/m2) einstellen, die der Bildschirm wiedergeben kann. Spitzenluminanzwerte findet man z. B. bei RTings."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Papierweißleuchtdichte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Die Leuchtdichte einstellen, die Papierweiß entsprechen soll, d. h. lesbarer Text oder Leuchtdichte am oberen Ende des SDR-Bereichs (Standard Dynamic Range). Nützlich zum Anpassen an unterschiedliche Lichtverhältnisse der Umgebung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "Kontrast"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "Gamma-/Kontraststeuerung für HDR. Nimmt die Farben und erhöht den Gesamtbereich zwischen den hellsten und den dunkelsten Teilen des Bildes. Je höher der HDR-Kontrast ist, desto größer wird dieser Unterschied, und je niedriger der Kontrast ist, desto verwaschener wird das Bild. Hilft das Bild nach dem eigenen Bildschirm und Geschmack einzustellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Gamut erweitern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Entscheidet, ob ein erweiterter Farb-Gamut verwendet werden soll, um HDR10 zu erreichen, wenn der Farbraum in den linearen Raum konvertiert wird."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Vertikale Synchronisation (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Die Video-Ausgabe der Grafikkarte mit der Bildwiederholrate des Bildschirms synchronisieren. Empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "VSync Swap-Intervall"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Benutzerdefinierte Swap-Intervalle für VSync verwenden. Reduziert effektiv die Bildwiederholfrequenz des Monitors um den angegebenen Faktor. \"Automatisch\" legt den Faktor auf Basis der vom Kern gemeldeten Bildwiederholfrequenz fest und sorgt für besseres Frame Pacing, wenn z. B. Inhalte mit 30 FPS auf einem 60-Hz-Bildschirm oder 60 FPS auf einem 120-Hz-Bildschirm ausgeführt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "VSync bleibt aktiviert, bis die Leistung unter die Ziel-Wiederholfrequenz fällt. Kann energieeffizienter sein und Ruckeln minimieren, wenn die Leistung unter Echtzeit fällt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Bildverzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Reduziert die Latenzzeit auf Kosten eines höheren Risikos von Videostottern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Legt fest, wie viele Millisekunden der Core nach der Videopräsentation in den Ruhezustand versetzt wird, bevor er gestartet wird. Reduziert die Latenz auf Kosten eines höheren Risikos von Stottern.\nWerte von 20 und höher werden als Frame-Zeit-Prozentsätze behandelt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Automatische Bildverzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Effektive \"Bildverzögerung\" dynamisch anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Versucht das gewünschte Ziel der \"Bildverzögerung\" zu halten und Frame-Drops zu minimieren. Ausgangspunkt ist die 3/4 Frame-Zeit, wenn \"Bildverzögerung\" 0 (Automatisch) ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "effektiv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "GPU und CPU synchronisieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "CPU und GPU fest synchronisieren. Reduziert Latenz auf Kosten der Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Anzahl der Frames für GPU-CPU-Sync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Anzahl der Bilder, um die die CPU der GPU voraus sein darf, wenn die GPU-CPU-Synchronisation aktiviert ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Legt fest, wie viele Frames die CPU der GPU vorauslaufen kann, wenn „GPU und CPU synchronisieren“ aktiviert ist. Das Maximum ist 3.\n 0: Sofort mit der GPU synchronisieren.\n 1: Mit dem vorherigen Frame synchronisieren.\n 2: usw."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Mit exakter Inhaltssignalfrequenz synchronisieren (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Keine Abweichung vom Core angefragten Timing. Nützlich für Bildschirme mit variabler Synchronisierung (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Mit der exakten Inhaltswiederholfrequenz synchronisieren. Diese Option entspricht dem Erzwingen von x1-Geschwindigkeit, erlaubt aber trotzdem Vorspulen. Keine Abweichung der angeforderten Core-Bildfrequenz, keine dynamische Ratensteuerung des Tons."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Ausgabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Audioausgabe-Einstellungen ändern."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Mikrofon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Audioeingabe-Einstellungen ändern."
   )
#endif
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Audio-Resampler-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Audiosynchronisationseinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "MIDI-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Audiomixer-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Menü-Töne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Menütoneinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Stummschalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Audio stummschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Mixer stummschalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Audiomixer stummschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Lautlos-Modus respektieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Gesamten Ton im Lautlos-Modus stummschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Auto-stumm bei schnellem Vorspulen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Audio automatisch stummschalten, wenn vorgespult wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Audiobeschleunigung bei schnellem Vorspulen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Audio beim schnellen Vorlauf beschleunigen. Verhindert Knackgeräusche, verändert aber die Tonhöhe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Auto-stumm bei Rückspulen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Audio beim Zurückspulen automatisch stumm stellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Lautstärkeanpassung (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Lautstärkeanpassung in dB. 0 dB ist die normale Lautstärke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Audiolautstärke, in dB. 0 dB ist normale Lautstärke ohne Verstärkung. Die Verstärkung kann während der Laufzeit mit den Hotkeys Lautstärke erhöhen/reduzieren gesteuert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Lautstärkeanpassung des Mixers (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Globale Lautstärkeanpassung des Audiomixers (in dB). 0 dB ist die normale Lautstärke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "DSP-Plugin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Audio-DSP-Plugin, welches die Audiodaten verarbeitet, bevor sie an den Treiber gesendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "DSP-Plugin entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Alle aktiven Audio-DSP-Plugins deaktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Exklusiver WASAPI-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Dem WASAPI-Treiber erlauben, die vollständige Kontrolle über das Audio-Gerät zu übernehmen. Wenn deaktiviert, wird der gemeinsame Modus verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "WASAPI-Gleitkomma-Format"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Gleitkomma-Format für den WASAPI-Treiber verwenden, wenn das Audio-Gerät dies unterstützt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Größe des gemeinsamen WASAPI-Puffers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Zwischenspeicher-Größe (in Frames), wenn der WASAPI-Treiber im gemeinsamen Modus verwendet wird."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Audioausgabe aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Audiogerät"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Das Standard-Audiogerät, welches vom Audiotreiber verwendet wird, überschreiben. Dies ist treiberabhängig."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Das Standard-Audiogerät, welches vom Audiotreiber verwendet wird, überschreiben. Dies ist treiberabhängig."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Benutzerdefinierter PCM-Gerätewert für den ALSA-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Benutzerdefinierter Pfadangabe für den OSS-Treiber (z. B. /dev/dsp)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Benutzerdefinierte Portnameangabe für den JACK-Treiber (z. B. system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Benutzerdefinierte IP-Adresse eines RSound-Servers für den RSound-Treiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Audiolatenz (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Maximale Audio-Latenz in Millisekunden. Der Treiber versucht, die tatsächliche Latenz bei 50 % dieses Wertes zu halten. Dieser Wert kann nicht eingehalten werden, wenn der Audiotreiber die angegebene Latenz nicht liefern kann."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Mikrofon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Audioeingabe in unterstützten Cores aktivieren. Hat keinen Einfluss auf die CPU, wenn der Core kein Mikrofon verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Audiogerät"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Überschreibt das Standardeingabegerät, welches der Mikrofontreiber verwendet. Dies ist treiberabhängig."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Überschreibt das Standardeingabegerät, welches der Mikrofontreiber verwendet. Dies ist treiberabhängig."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Audio-Resampler-Qualität"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Diesen Wert verkleinern, für bessere Leistung/kleinere Latenz zu Lasten der Audioqualität; erhöhen, für bessere Audioqualität auf Kosten der Leistung/kleinerer Latenz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Standard-Eingangsfrequenz (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Audioeingangsfrequenz, die verwendet wird, wenn ein Core keine bestimmte Anzahl anfordert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Audioeingabelatenz (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Gewünschte Audioeingangslatenz in Millisekunden. Wird ignoriert, wenn der Mikrofontreiber die angegebene Latenz nicht zur Verfügung stellt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Exklusiver WASAPI-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "RetroArch erlauben, bei Verwendung des WASAPI Mikrofontreibers die ausschließliche Kontrolle über das Mikrofongerät zu übernehmen. Falls deaktiviert, verwendet RetroArch stattdessen den Shared Modus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "WASAPI-Gleitkomma-Format"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Gleitkommaformat für den WASAPI-Treiber verwenden, wenn dies vom Audiogerät unterstützt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Größe des gemeinsamen WASAPI-Puffers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Zwischenspeicher-Größe (in Frames), wenn der WASAPI-Treiber im gemeinsamen Modus verwendet wird."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Audio-Resampler-Qualität"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Diesen Wert verkleinern, für bessere Leistung/kleinere Latenz zu Lasten der Audioqualität; erhöhen, für bessere Audioqualität auf Kosten der Leistung/kleinerer Latenz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Ausgaberate (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Abtastrate der Audioausgabe."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Audio synchronisieren. Empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Maximaler Audioversatz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Maximale Änderung der Audio-Eingangsrate. Das Erhöhen dieses Wertes ermöglicht sehr große Änderungen im Timing (beispielsweise um einen PAL-Core auf einem NTSC-Bildschirm darzustellen), verursacht jedoch fehlerhafte Tonhöhen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Maximaler Audioversatz.\nDefiniert die maximale Änderung der Eingangsrate. Das Erhöhen dieses Wertes erlaubt sehr große Änderungen im Timing, z. B. um PAL-Cores auf NTSC-Bildschirmen zu spielen, auf Kosten von ungenauen Tonhöhen.\nEingangsrate ist definiert als:\nEingangsrate * (1.0 +/- (Max. Audioversatz))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Dynamische Audioratensteuerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Hilft, Fehler bei der Audio- und Videosynchronisierung auszubügeln. Wenn deaktiviert, ist eine brauchbare Synchronisation nahezu unmöglich."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "0 deaktiviert die Ratensteuerung.\nJeder andere Wert bestimmt das Delta der Audioratensteuerung, welches festlegt, wie stark die Eingangsrate dynamisch angepasst werden kann.\nDie Eingangsrate ist definiert als:\nEingangsrate * (1,0 +/- (Steuerungsdelta))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Eingabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Eingabegerät auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Legt das Eingabegerät fest (treiberspezifisch). Bei „Aus“ wird der MIDI-Eingang deaktiviert. Der Gerätename kann auch eingegeben werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Ausgabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Ausgabegerät auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Legt das Ausgabegerät fest (treiberspezifisch). Bei „Aus“ wird die MIDI-Ausgabe deaktiviert. Der Gerätename kann auch eingegeben werden.\nWenn die MIDI-Ausgabe aktiviert ist und der Core und das Spiel / die Anwendung die MIDI-Ausgabe unterstützen, werden einige oder alle Sounds (je nach Spiel/Anwendung) vom MIDI-Gerät erzeugt. Im Falle eines „Null“-MIDI-Treibers werden diese Sounds nicht hörbar sein."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Lautstärke"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Ausgabelautstärke festlegen (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Mixerstream #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Abspielen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Startet die Wiedergabe des Audiostreams. Nach dem Abspielen wird der aktuelle Audiostream aus dem Speicher entfernt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Abspielen (Schleife)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Startet die Wiedergabe des Audiostreams. Nach dem Abspielen wird dieser erneut von vorne gestartet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Abspielen (Sequenziell)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Startet die Wiedergabe des Audiostreams. Nach dem Abspielen wird der nächste Stream in sequenzieller Reihenfolge gestartet und dieses Verhalten wiederholt. Nützlich als Albumwiedergabemodus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Anhalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Stoppt die Wiedergabe des Audiostreams, entfernt diesen aber nicht aus dem Speicher. Er kann mit \"Abspielen\" erneut gestartet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Beendet die Wiedergabe und entfernt den Audiostream aus dem Speicher."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Lautstärkeanpassung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Lautstärke des Audiostreams anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Status: k. A."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Status: Gestoppt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Status: Wiedergabe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Status: Wiedergabe (Schleife)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Status: Wiedergabe (in Folge)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Gleichzeitige Wiedergabe von Audiostreams auch im Menü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "'OK'-Ton aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "'Zurück'-Ton aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Aktiviere 'Benachrichtungs' Ton"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "'BGM' aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Scrolltöne einschalten"
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
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Abfrageverhalten (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Beeinflusst, wie die Eingangsabfrage in RetroArch abgearbeitet wird. Wird dieser Wert auf „Früh“ oder „Spät“ gesetzt, kann sich je nach Konfiguration die Latenz verringern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Beeinflusst, wie die Eingabeabfrage in RetroArch erfolgt.\nFrüh – Eingabe wird vor der Verarbeitung des Frames abgefragt.\nNormal – Eingabe wird abgefragt, wenn die Abfrage angefordert wird.\nSpät – Eingabe wird bei der ersten Anforderung des Eingabestatus pro Frame abgefragt.\n„Früh“ oder „Spät“ können je nach Konfiguration zu einer geringeren Latenz führen. Wird bei Netzwerkspielen ignoriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Steuerungsremap für diesen Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Die Eingabebelegung mit der Neubelegung überschreiben, die für den aktuellen Core festgelegt wurde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Remaps nach Gamepad sortieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Remaps gelten nur für das aktive Gamepad, in dem sie gespeichert wurden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Automatische Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Konfiguriert automatisch Controller mit einem Profil im Plug-and-Play-Stil."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Windows-Hotkeys deaktivieren (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Windows-Tastenkombinationen innerhalb der Anwendung behalten."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Physische Tastatur auswählen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Dieses Gerät als physische Tastatur und nicht als Gamepad verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Wenn RetroArch eine Hardwaretastatur als eine Art Gamepad identifiziert, kann mit dieser Einstellung RetroArch gezwungen werden, das falsch identifizierte Gerät als Tastatur zu behandeln.\nDies kann nützlich sein, wenn ein Computer in einem Android-TV-Gerät emuliert wird und auch eine physische Tastatur besitzt, die an die Box angeschlossen werden kann."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Hilfssensoren-Input"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Input von Beschleunigungsmesser, Gyroskop und Lichtsensor ermöglichen, sofern dies von der aktuellen Hardware unterstützt wird. Kann auf einigen Plattformen Auswirkungen auf die Leistung haben und/oder den Stromverbrauch erhöhen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Automatischer Mausfang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Die Maus beim Auswählen der Anwendung einfangen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "'Spielfokus'-Modus automatisch aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Den 'Spielfokus'-Modus aktivieren, wenn Inhalte gestartet oder fortgesetzt werden. Wenn diese Option auf 'Erkennen' gesetzt ist, wird sie aktiviert, wenn der aktuelle Core über die Frontend-Tastaturrückruffunktion verfügt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "EIN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Erkennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Inhalt pausieren, wenn Controller getrennt wird"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Inhalt pausieren, wenn ein Controller getrennt ist. Fortsetzen mit Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Analog-zu-digital-Grenzwert"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Wie weit eine Achse gekippt werden muss, um einen Tastendruck zu bewirken, wenn \"Analog zu Digital\" verwendet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Totzonenregler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Analogstickbewegungen unterhalb des Deadzone-Wertes ignorieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Analogempfindlichkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Die Empfindlichkeit von analogen Sticks anpassen."
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
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Haltezeit für Belegung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Zeit in Sekunden, für die eine Taste bei der Belegung gehalten werden muss."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Tastatur, Maus oder Controller drücken"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Tasten und Knöpfe freigeben!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Zeitlimit"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Halten"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Turbo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Turbo-Periode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Turbo-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Das allgemeine Verhalten des Turbo-Modus wählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Klassisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Klassisch (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Einzeltaste (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Einzeltaste (Halten)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Turbo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Haptisches Feedback/Vibration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Haptisches Feedback- und Vibrationseinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Menüsteuerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Menü-Steuerungseinstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Einstellungen und Zuweisungen für Hotkeys ändern, z. B. das Menü während des Spielens umschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "RetroPad-Zuweisungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Ändert, wie das virtuelle RetroPad einem physischen Eingabegerät zugeordnet wird. Wenn ein Eingabegerät korrekt erkannt und automatisch konfiguriert wird, brauchen Benutzer dieses Menü wahrscheinlich nicht zu verwenden.\nHinweis: Für Core-spezifische Eingabeänderungen bitte stattdessen das Untermenü „Steuerung“ des Schnellmenüs verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro verwendet eine virtuelle Gamepad-Abstraktion, die als „RetroPad“ bekannt ist, um von Frontends (wie RetroArch) zu Cores und umgekehrt zu kommunizieren. In diesem Menü wird festgelegt, wie das virtuelle RetroPad auf die physischen Eingabegeräte zugeordnet wird und welche virtuellen Eingabeports diese Geräte belegen.\nWenn ein physisches Eingabegerät korrekt erkannt und automatisch konfiguriert wird, benötigt der Benutzer dieses Menü wahrscheinlich gar nicht und sollte für core[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Port %u Controller"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Ändert, wie das virtuelle RetroPad dem physischen Eingabegerät für diesen virtuellen Anschluss zugewiesen wird."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Core-spezifische Eingabezuordnungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Problemumgehung für Verbindungstrennung auf Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Problembehebung für Controller, die die Verbindung trennen und wiederherstellen. Betrifft 2 Spieler mit den gleichen Controllern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Beenden bestätigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "Zum Beenden von RetroArch muss der Beenden-Hotkey zweimal gedrückt werden."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Vibration bei Tastendruck"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Vibration aktivieren (für unterstützte Cores)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Vibrationsstärke"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Die Größenordnung der haptischen Feedback-Effekte angeben."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Einheitliche Menüsteuerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Dieselbe Steuerung für das Menü und die Spiele verwenden. Betrifft die Tastatur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Info-Taste deaktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Wenn aktiviert, wird die Info-Taste ignoriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Suchen-Taste deaktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Wenn aktiviert, wird die Suchen-Taste ignoriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Linken Analogstick für Menü deaktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Linken Analogstick am Navigieren im Menü hindern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Rechten Analogstick für Menü deaktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Rechten Analogstick am Navigieren im Menü hindern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Vertausche OK- und Zurück-Tasten im Menü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Die Tasten für 'OK' und 'Zurück' vertauschen. \"Aus\" verwendet die japanische Tastenbelegung; \"An\" die westliche Tastenbelegung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Vertausche Scroll-Tasten im Menü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Die Tasten für Scrolling vertauschen. \"Aus\" scrollt 10 Einträge mit L/R und alphabetisch mit L2/R2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Alle Benutzer können das Menü steuern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Jedem Benutzer ermöglichen, das Menü zu steuern. Wenn deaktiviert, kann nur Benutzer 1 das Menü steuern."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Hotkeys aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Nach der Zuweisung muss die Taste „Hotkey aktivieren“ gedrückt werden, bevor andere Hotkeys erkannt werden. Das ermöglicht die Zuweisung von Controller-Tasten zu Hotkey-Funktionen, ohne die normale Eingabe zu beeinflussen. Wird der Modifikator nur dem Controller zugewiesen, wird er für Tastatur-Hotkeys nicht benötigt, aber beide Modifikatoren funktionieren für beide Geräte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Wenn dieser Hotkey an die Tastatur, den Controller oder eine Analogachse gebunden ist, werden alle anderen Hotkeys deaktiviert, außer wenn dieser Hotkey gleichzeitig gehalten wird.\nDies ist nützlich für RETRO_KEYBOARD-zentrierte Implementierungen, die einen großen Bereich der Tastatur abfragen, wo Hotkeys stören könnten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Hotkey-Aktivierungsverzögerung (Frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Eine Verzögerung in Frames hinzufügen, bevor die normale Eingabe nach Drücken der zugewiesenen Taste \"Hotkeys aktivieren\" blockiert wird. Ermöglicht die normale Eingabe der \"Hotkeys aktivieren\"-Taste, wenn sie einer anderen Aktion zugeordnet ist (z. B. RetroPad \"Select\")."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Hotkey-Gerätetyp zusammenführen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Blockiert alle Hotkeys von Tastatur- und Controller-Gerätetypen, wenn für einen der Typen „Hotkeys aktivieren“ festgelegt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Menü (Controller-Kombination)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Controller-Tastenkombination, mit der das Menü aufgerufen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Menü umschalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Schaltet die aktuelle Anzeige zwischen Menü und Inhalt um."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Beenden (Controller-Kombination)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Controller-Tastenkombination, mit der RetroArch geschlossen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Beenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Schließt RetroArch und stellt sicher, dass alle Speicherdaten und Konfigurationsdateien auf den Datenträger übertragen werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Schließen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Schließt den aktuellen Inhalt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Inhalt neu starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Starte den aktuellen Inhalt neu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Vorspulen (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Wechselt zwischen Vorspul- und Normalgeschwindigkeit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Vorspulen (Halten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Spult vor, solange gedrückt. Inhalte laufen mit normaler Geschwindigkeit, wenn Taste losgelassen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Zeitlupe (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Wechselt zwischen Zeitlupe und Normalgeschwindigkeit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Zeitlupe (Halten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Aktiviert Zeitlupe, solange gedrückt. Inhalte laufen mit normaler Geschwindigkeit, wenn Taste losgelassen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Zurückspulen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Spult den aktuellen Inhalt zurück, solange Taste gedrückt wird. 'Zurückspulen-Unterstützung' muss aktiviert sein."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Schaltet den Inhalt zwischen pausiert und nicht-pausiert um."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Einzelbildvorlauf"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Setzt den Inhalt im pausierten Zustand um einen Frame fort."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Audio stumm schalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Schaltet die Audioausgabe ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Lautstärke erhöhen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Erhöht Audio-Lautstärkepegel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Lautstärke reduzieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Verringert Audio-Lautstärkepegel."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Savestate laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Lädt Savestate aus dem aktuell gewählten Speicherplatz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Savestate speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Speichert Savestate im aktuell gewählten Speicherplatz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Nächster Savestate-Speicherplatz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Erhöht den aktuell ausgewählten Speicherplatz-Index."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Vorheriger Savestate-Speicherplatz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Verringert den aktuell ausgewählten Speicherplatz-Index."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Disc auswerfen (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Wenn das virtuelle Disc-Fach geschlossen ist, wird es geöffnet und die geladene Disc entfernt. Andernfalls wird die aktuell ausgewählte Disc eingelegt und das Fach geschlossen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Nächste Disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Erhöht den aktuell ausgewählten Disc-Index. Virtuelles Laufwerk muss geöffnet sein."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Vorherige Disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Verringert den aktuell ausgewählten Disc-Index. Virtuelles Laufwerk muss geöffnet sein."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Shader (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Schaltet den aktuell ausgewählten Shader ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Nächster Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Lädt und wendet die nächste Shader-Preset-Datei im 'Video-Shader'-Verzeichnis an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Vorheriger Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Lädt und wendet die vorherige Shader-Preset-Datei im 'Video-Shader'-Verzeichnis an."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Cheats (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Schaltet den ausgewählten Cheat ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Nächster Cheat-Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Erhöht den aktuell ausgewählten Cheat-Index."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Vorheriger Cheat-Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Verringert den aktuell ausgewählten Cheat-Index."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Screenshot erstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Fertigt ein Foto des aktuellen Inhalts an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Aufnahme (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Startet/stoppt die Aufzeichnung der aktuellen Sitzung in einer lokalen Videodatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Streaming (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Startet/stoppt das Streaming der aktuellen Sitzung zu einer Online-Video-Plattform."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Replay abspielen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Replaydatei aus dem aktuell ausgewählten Slot abspielen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Replay aufnehmen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Replaydatei in den aktuell ausgewählten Slot aufnehmen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Aufzeichnung/Wiedergabe stoppen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Stoppt das Aufzeichnen/Abspielen des aktuellen Replays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "Nächster Replayslot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Erhöht den Index des aktuell ausgewählten Replayslots."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Vorheriger Replayslot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Verringert den Index des aktuell ausgewählten Replayslots."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Mauszeiger einfangen (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Fängt die Maus ein oder lässt sie los. Wenn eingefangen, wird der Mauszeiger ausgeblendet und auf das RetroArch-Fenster beschränkt, was relative Mauseingaben verbessert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Spielfokus (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Schaltet den 'Spielfokus'-Modus an/aus. Wenn der Inhalt fokussiert ist, sind Hotkeys deaktiviert (volle Tastatureingabe wird an den laufenden Core übergeben) und die Maus wird eingefangen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Vollbildmodus (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Wechselt zwischen Vollbild- und Fensteranzeigemodus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Desktop-Menü (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Öffnet die WIMP(Windows, Icons, Menus, Pointer)-Desktop-Benutzeroberfläche."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Mit exakter Inhaltssignalfrequenz synchronisieren (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Synchronisierung mit exakter Signalfrequenz des Inhalts umschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Run-Ahead (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Schaltet Run-Ahead ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Präemptive Frames (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Schaltet präemptive Frames ein/aus."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "FPS anzeigen (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Schaltet 'Bilder pro Sekunde' Statusanzeige ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Technische Statistiken anzeigen (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Blendet die Anzeige der technischen Statistiken auf dem Bildschirm ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Tastatur-Overlay (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Schaltet Tastatur-Overlay ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Nächstes Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Wechselt zum nächsten verfügbaren Layout des aktuell aktiven Overlays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "KI-Dienst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Erfasst ein Bild des aktuellen Inhalts für eine Übersetzung und/oder liest den Text auf dem Bildschirm vor. 'KI-Dienst' muss aktiviert und konfiguriert sein."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Netzwerkspiel-Ping (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Blendet den Ping-Zähler für den aktuellen Netzwerkspielraum ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Netzwerkspiel-Hosting (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Schaltet Netzwerkspiel-Hosting ein/aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Netzwerkspiel Spieler-/Zuschauermodus (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Schaltet das aktuelle Netzwerkspiel zwischen 'Spieler'- und 'Zuschauer'-Modus um."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Netzwerkspielerchat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Sendet eine Chat-Nachricht an das aktuelle Netzwerkspiel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Netplay Fade Chat (Umschalten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Zwischen verblassenden und statischen Netzwerkspiel-Chatnachrichten umschalten."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "Debugging-Informationen senden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SEND_DEBUG_INFO,
   "Sendet Diagnoseinformationen über das Gerät und die RetroArch-Konfiguration zur Analyse an unsere Server."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Gerätetyp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Bestimmt den emulierten Controllertyp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Analog-zu-Digital-Typ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Verwendet angegebenen Analogstick für Steuerkreuz-Eingabe. „Erzwungene“ Einstellung überschreibt die native Analogsteuerung des Core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Angegebenen Analogstick für Steuerkreuz-Eingabe verwenden.\nWenn der Core nativ Analogeingaben unterstützt, wird das Steuerkreuz-Mapping deaktiviert, außer eine „(Erzwungen)“-Option wird gewählt.\nWird das Steuerkreuz-Mapping erzwungen, erhält der Core keine Analogeingaben vom angegebenen Stick."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Geräteindex"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "Der physische Controller, wie von RetroArch erkannt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Reserviertes Gerät für diesen Spieler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Dieser Controller wird diesem Spieler entsprechend dem Reservierungsmodus zugewiesen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "Keine Reservierung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "Bevorzugt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "Reserviert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "Gerätereservierungstyp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "Bevorzugt: Wenn das angegebene Gerät vorhanden ist, wird es diesem Spieler zugewiesen.Reserviert: Diesem Spieler wird kein anderer Controller zugewiesen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Zugeordneter Port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Bestimmt, welcher Core-Port Eingaben vom Frontend-Controller-Port %u erhält."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Alle Belegungen festlegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Alle Richtungen und Schaltflächen nacheinander in der Reihenfolge zuordnen, in der sie in diesem Menü erscheinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Eingabebelegungen zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Die Einstellungen für die Zuordnung auf ihre Standardwerte setzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Controller-Profil speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Eine automatische Konfigurationsdatei speichern, die automatisch angewendet wird, wenn dieser Controller erneut erkannt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Maus-Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "Die physische Maus wie von RetroArch erkannt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "B-Knopf (unten)"
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
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "Steuerkreuz oben"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "Steuerkreuz unten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "Steuerkreuz links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "Steuerkreuz rechts"
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
   "L2-Knopf (Trigger)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2-Knopf (Trigger)"
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
   "Linker Analogstick Y+ (unten)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Linker Analogstick Y- (oben)"
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
   "Rechter Analogstick Y+ (unten)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Rechter Analogstick Y- (oben)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Pistolenabzug"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Pistole nachladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Pistole Aux A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Pistole Aux B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Pistole Aux C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Pistole Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Pistole Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Pistole Steuerkreuz oben"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Pistole Steuerkreuz unten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Pistole Steuerkreuz links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Pistole Steuerkreuz rechts"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Run-Ahead nicht verfügbar]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "Der aktuelle Core ist aufgrund fehlender deterministischer Savestate-Unterstützung nicht mit Run-ahead kompatibel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Anzahl der Run-Ahead-Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Die Anzahl Frames, die im Voraus ausgeführt werden sollen. Verursacht Gameplay-Probleme wie z. B. Ruckeln, wenn die Anzahl der spielinternen Verzögerungsframes überschritten wird."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "Führt zusätzliche Corelogik aus, um die Latenz zu reduzieren. Eine einzelne Instanz wird in einem zukünftigen Frame ausgeführt und lädt dann den aktuellen Status neu. Eine zweite Instanz behält eine reine Video-Coreinstanz in einem zukünftigen Frame bei, um Probleme mit dem Audiostatus zu vermeiden. Aus Effizienzgründen führen präemptive Frames bei Bedarf vergangene Frames mit neuen Eingaben durch."
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Führt zusätzliche Corelogik aus, um die Latenz zu reduzieren. Eine einzelne Instanz wird in einem zukünftigen Frame ausgeführt und lädt dann den aktuellen Status neu. Aus Effizienzgründen führen präemptive Frames bei Bedarf vergangene Frames mit neuen Eingaben durch."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "Einzelinstanz-Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "Zweitinstanz-Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   "Präemptive-Frames-Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Run-Ahead Warnungen ausblenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Blendet die Warnmeldung aus, die bei Verwendung von Run-Ahead angezeigt wird wenn der Core keine Savestates unterstützt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Anzahl präemptiver Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Die Anzahl Frames, die im Voraus ausgeführt werden sollen. Verursacht Gameplayprobleme wie z. B. Ruckeln, wenn die Anzahl der spielinternen Verzögerungsframes überschritten wird."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Gemeinsamen Hardware-Kontext aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Hardware-gerenderten Cores ihren eigenen privaten Kontext geben. Vermeidet, dass der Hardwarestatus zwischen den Frames geschätzt werden muss."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Cores das Wechseln des Videotreibers erlauben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Erlaubt es Cores zu einem anderen als den derzeit geladenen Videotreiber zu wechseln."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Leeren Core laden, wenn Core beendet wird"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Einige Cores verfügen über eine Funktion zum Herunterfahren. Durch das Laden eines Leer-Cores wird das Beenden von RetroArch verhindert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Einige Cores haben eine Funktion zum Herunterfahren.\nWenn diese Option deaktiviert bleibt, wird RetroArch beendet, wenn diese Funktion ausgeführt wird. Ist diese Option aktiviert, wird stattdessen ein Dummy-Core geladen, sodass RetroArch nicht beendet wird und das Menü benutzbar bleibt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Core automatisch starten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Vor dem Laden auf fehlende Firmware prüfen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Überprüfen, ob benötigte Firmware vorhanden ist, bevor versucht wird, Inhalte zu laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE,
   "Einige Cores benötigen möglicherweise Firmware- oder BIOS-dateien. Wenn diese Option aktiviert ist, verhindert RetroArch den Start des Cores, wenn die erforderlichen Firmware-Elemente fehlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Core-Optionskategorien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Cores erlauben, Einstellungen in Kategorie-basierten Untermenüs darzustellen. Hinweis: Core muss neu gestartet werden, um Änderungen zu übernehmen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Core-Informationen-Cache"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Einen dauerhaften lokalen Cache der Informationen installierter Cores anlegen. Reduziert erheblich die Ladezeiten auf Plattformen mit langsamem Datenträgerzugriff."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Umgehung der Core-Info-Savestates-Funktionen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Legt fest, ob Savestate-Fähigkeiten der Core-Info ignoriert werden sollen, so dass mit verwandten Funktionen experimentiert werden kann (Run-Ahead, Zurückspulen usw.)."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Core beim Starten von Inhalten immer neu laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Startet RetroArch neu, wenn Inhalte gestartet werden, auch wenn der angeforderte Core bereits geladen ist. Dies kann die Systemstabilität auf Kosten längerer Ladezeiten verbessern."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Erlaube Bildrotation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Erlaubt Cores die Bildrotation einzustellen. Wenn deaktiviert, werden Rotationsanfragen ignoriert. Nützlich für Setups, bei denen der Bildschirm selbst gedreht wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Cores verwalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Informationen zu installierten Cores anzeigen und Offline-Wartungsarbeiten ausführen (Sichern, Wiederherstellen, Löschen usw.)."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Cores verwalten"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Über Steam verfügbare Cores installieren oder deinstallieren."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Core installieren"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Core deinstallieren"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "Zeige \"Cores verwalten\""
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Die Option \"Cores verwalten\" im Hauptmenü anzeigen."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Installiere Core: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "Der Core wird deinstalliert, wenn RetroArch beendet wird."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "Der Core wird gerade heruntergeladen"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Konfiguration beim Beenden speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Änderungen an der Konfigurationsdatei beim Beenden speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Änderungen an den Konfigurationsdateien beim Beenden speichern. Nützlich für Änderungen, die im Menü vorgenommen wurden. Die Konfigurationsdatei wird überschrieben, „#include“s und Kommentare werden nicht beibehalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Remap-Dateien beim Beenden speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Änderungen an allen aktiven Eingabe-Remap-Dateien beim Schließen von Inhalten oder Beenden von RetroArch speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Inhaltsspezifische Core-Einstellungen automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Benutzerdefinierte Core-Einstellungen beim Start automatisch laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Überschreibende Dateien automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Benutzerdefininerte Einstellungen beim Start automatisch laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Remap-Dateien automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Benutzerdefinierte Tastenbelegungen beim Start automatisch laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Anfängliche Disc-Index-Dateien automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "Wechselt beim Starten von Multi-Disc-Inhalten zur zuletzt verwendeten Disc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Shader-Voreinstellungen automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Globale Core-Einstellungsdatei verwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Alle Core-Einstellungen in einer gemeinsamen Datei (retroarch-core-options.cfg) speichern. Wenn diese Option deaktiviert ist, werden die Einstellungen für jeden Core in einem separaten Core-spezifischen Ordner / einer Datei im RetroArch-Verzeichnis 'Konfigurationen' gespeichert."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Speicherdaten nach Core-Namen in Ordner sortieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Speicherdaten in Ordnern sichern, die nach dem verwendeten Core benannt sind."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Savestates nach Core-Namen in Ordner sortieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Savestates in Ordnern sichern, die nach dem verwendeten Core benannt sind."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Speicherdaten nach Inhaltsverzeichnis in Ordner sortieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Speicherdaten in Ordnern sichern, die nach dem Verzeichnis benannt sind, in dem sich der ausgeführte Inhalt befindet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Savestates nach Inhalts-Verzeichnis in Ordner sortieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Savestates in Ordnern sichern, die nach dem Verzeichnis benannt sind, in dem sich der ausgeführte Inhalt befindet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "SaveRAM beim Laden eines Savestates nicht überschreiben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Verhindern, dass das SaveRAM überschrieben wird, wenn ein Savestate geladen wird. Kann zu verbuggten Spielen führen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Intervall für automatisches Speichern des SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Das nichtflüchtige SaveRAM automatisch in regelmäßigen Abständen speichern (in Sekunden)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Automatisches Speichern des nichtflüchtigen SRAM in regelmäßigen Abständen. Dies ist standardmäßig deaktiviert, sofern nicht anders eingestellt. Das Intervall wird in Sekunden angegeben. Ein Wert von 0 deaktiviert die automatische Speicherung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Intervall für Replay-Kontrollpunkte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Automatisch Lesezeichen für Spielstatus während Replay-Aufzeichnung in einem regelmäßigen Intervall setzen (in Sekunden)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Speichert den Spielstatus während der Replay-Aufnahme in einem regelmäßigen Intervall. Dies ist standardmäßig deaktiviert, es sei denn, es wird etwas anderes festgelegt. Das Intervall wird in Sekunden gemessen. Ein Wert von 0 deaktiviert die Aufzeichnung von Kontrollpunkten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Savestate-Speicherplatz-Index automatisch inkrementieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Vor dem Erstellen eines Savestates wird der Index des Speicherplatzes automatisch erhöht. Beim Laden von Inhalten wird der höchste vorhandene Index gewählt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Replay-Index automatisch erhöhen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Vor dem Erstellen eines Replays wird der Replay-Index automatisch erhöht. Beim Laden von Inhalten wird der Index auf den höchsten vorhandenen Index gesetzt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Maximale Anzahl automatisch erzeugter Savestates"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Die Anzahl der Savestates begrenzen, die erzeugt werden, wenn 'Savestate-Speicherplatz-Index automatisch inkrementieren' aktiviert ist. Wenn beim Speichern eines neuen Savestates das Limit überschritten wird, wird der Savestate mit dem niedrigsten Index gelöscht. Bei einem Wert von '0' werden unbegrenzt viele Savestates erzeugt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Maximal zu behaltende automatisch-erhöhter Replays"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Begrenzt die Anzahl zu erstellender Replays, wenn „Replay-Index automatisch erhöhen“ aktiviert ist. Wird mit einer neuen Replay-Aufzeichnung das Limit überschritten, wird das vorhandene Replay mit dem niedrigsten Index gelöscht. Ein Wert von „0“ bedeutet, dass unbegrenzt viele Replays aufgezeichnet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Savestate automatisch erstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Beim Schließen des Inhalts automatisch einen Savestate erstellen. Dieser Savestate wird beim Start geladen, wenn „Savestate automatisch laden“ aktiviert ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Savestate automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Lade den zuletzt erstellten Savestate beim Start automatisch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Savestate-Vorschaubilder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Vorschaubilder von Savestates im Menü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "SaveRAM Kompression"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Nicht-flüchtige SaveRAM Dateien in einem archivierten Format speichern. Reduziert die Dateigröße stark auf Kosten von (leicht) erhöhten Speicher-/Ladezeiten.\nNur für Cores, die das Speichern über Libretros Standard-SaveRAM-Schnittstelle ermöglichen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Savestate Kompression"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Savestate-Dateien in einem archivierten Format speichern. Reduziert die Dateigröße drastisch auf Kosten erhöhter Speicher- und Ladezeiten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Screenshots nach Inhaltsverzeichnis in Ordner sortieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Screenshots werden in Ordnern gespeichert, die nach dem Verzeichnis benannt sind, in dem sich der ausgeführte Inhalt befindet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Speicherdaten im Verzeichnis des Inhalts speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Inhaltsverzeichnis als Speicherverzeichnis verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Savestates im Verzeichnis des Inhalts speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Inhaltsverzeichnis als Verzeichnis für Speicherstände verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Systemdateien befinden sich im Verzeichnis des Inhalts"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Inhalteverzeichnis als System-/BIOS-verzeichnis verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Screenshots im Verzeichnis des Inhalts speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Inhaltsverzeichnis als Screenshot-Verzeichnis verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Spielzeit speichern (pro Core)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Verfolgen, wie lange jedes Inhaltselement ausgeführt wurde, wobei die Datensätze nach Core getrennt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Spielzeit speichern (Gesamtsumme)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Verfolgen, wie lange jedes Inhaltselement ausgeführt wurde, und zeichnet die Gesamtsumme über alle Cores hinweg auf."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Protokollausführlichkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Protokoll ins Terminal oder in eine Datei schreiben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Frontend Protokollierungsstufe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Die Protokollierungsstufe fürs Frontend festlegen. Wenn die Stufe, die vom Frontend gemeldet wird, unter diesem Wert liegt, wird sie ignoriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Core Protokollierungsstufe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Die Protokollierungsstufe für Cores festlegen. Wenn die Nachrichtenstufe, die vom Core gemeldet wird, unter diesem Wert liegt, wird sie ignoriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Legt die Protokollierungsstufe für Libretro-Cores (GET_LOG_INTERFACE) fest. Liegt die vom Core gemeldete Nachrichtenstufe unter diesem Wert, wird sie ignoriert. DEBUG-Protokolle werden immer ignoriert, außer der Verbose-Modus ist aktiviert (--verbose).\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (Debuggen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Warnung)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (Fehler)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "In Datei protokollieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Logmeldungen des Systems in eine Datei umleiten. Erfordert die Aktivierung der 'Protokollausführlichkeit'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Protokolldateien zeitstempeln"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Beim Protokollieren in eine Datei die Ausgabe von jeder RetroArch-Sitzung in eine neue zeitgestempelte Datei umleiten. Wenn deaktiviert, wird das Protokoll bei jedem Neustart von RetroArch überschrieben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Leistungsindikatoren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Leistungsindikatoren für RetroArch und Cores. Daten können zur Ermittlung von Systemengpässen und zur Feinabstimmung beitragen."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Versteckte Dateien und Ordner zeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Versteckte Dateien und Ordner im Dateimanager anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Unbekannte Dateierweiterungen filtern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Im Dateibrowser nur Dateien mit unterstützten Dateitypen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Nach aktuellem Core filtern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "Die im Dateibrowser anzuzeigenden Dateien durch aktuellen Core filtern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Letztes Startverzeichnis merken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Den Dateibrowser im zuletzt verwendeten Pfad öffnen, wenn Inhalte aus dem Startverzeichnis geladen werden. Hinweis: Der Pfad wird beim Neustart von RetroArch zurückgesetzt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Eingebauten Mediaplayer verwenden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Integrierten Bildbetrachter verwenden"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Zurückspulen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Einstellungen für die Rückspul-Funktion ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Frame-Zeit-Zähler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Einstellungen, die den Frame-Zeit-Zähler beeinflussen.\nNur aktiv, wenn 'Video in separatem Thread' deaktiviert ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Vorspulrate"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Die maximale Geschwindigkeit, mit der Inhalt beim Vorspulen dargestellt wird (z. B. 5x für 60 FPS Inhalt => Begrenzung auf 300 FPS). Wenn 0x gewählt wird, ist die Geschwindigkeit unbegrenzt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "Die maximale Geschwindigkeit, mit der Inhalte beim Vorspulen abgespielt werden.(z. B. 5,0 für Inhalte mit 60 fps => 300 fps Obergrenze).\nRetroArch pausiert, um sicherzustellen, dass die maximale Geschwindigkeit nicht überschritten wird. Bitte sich nicht darauf verlasssen, dass diese Obergrenze absolut genau ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Frameskip beim Vorspulen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Einzelbilder je nach Vorspulgeschwindigkeit überspringen. Dies spart Energie und ermöglicht die Verwendung externer Frame-Begrenzung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Zeitlupen-Rate"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "Der Faktor, um den die Abspielgeschwindigkeit verringert wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Bildwiederholrate im Menü begrenzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Stellt sicher, dass die Bildwiederholrate im Menü begrenzt wird."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Zurückspulen-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Zu einem vorherigen Punkt des Spiels zurückkehren. Dies verursacht einen schweren Leistungseinbruch beim Spielen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Anzahl Rückspul-Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Die Anzahl der Frames, die pro Schritt zurückgespult werden sollen. Höhere Werte erhöhen die Rückspul-Geschwindigkeit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Rückspulpuffergröße (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "Die Speichermenge (in MB), die für den Rückspulpuffer zur Verfügung stehen soll. Das Erhöhen von diesem Wert ermöglicht längeres Zurückspulen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Rückspulpuffer-Größenschritt (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Jedes Mal, wenn die Rückspulpuffergröße erhöht oder verringert wird, ändert sie sich um diesen Betrag."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Nach Vorspulen zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Den Frame-Zeit-Zähler nach dem Vorspulen zurücksetzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Nach Savestate-Laden zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Den Frame-Zeit-Zähler nach dem Laden eines Savestates zurücksetzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Nach Savestate-Speichern zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Den Frame-Zeit-Zähler nach dem Erstellen eines Savestates zurücksetzen."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Aufnahmequalität"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Benutzerdefiniert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Niedrig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "Mittel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Hoch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Verlustfrei"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM (Schnell)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM (Hohe Qualität)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Eigene Aufnahme-Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Aufnahme-Threads"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Filter mitaufnehmen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Video aufnehmen, nachdem Filter (aber keine Shader) angewendet wurden. Das Video wird genauso gut aussehen, wie das, was Du auf dem Bildschirm siehst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Verwende GPU-Aufnahme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Bildmaterial nach Shaderdurchläufen aufnehmen, sofern verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Streamingmodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "Lokal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Benutzerdefiniert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Streamingqualität"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Benutzerdefiniert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Niedrig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "Mittel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Hoch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Eigene Streaming-Konfiguration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Streamtitel"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Bildschirm-Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Die Rahmen und Steuerelemente auf der Anzeige anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Video-Layout"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Das Video-Layout anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Bildschirm-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Bildschirm-Benachrichtigungen anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Benachrichtigungssichtbarkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Die Sichtbarkeit bestimmter Benachrichtigungstypen umschalten."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Overlay anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Overlays für Rahmen und Steuerelemente auf dem Bildschirm verwenden."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Overlay hinter dem Menü anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Overlays hinter statt vor dem Menü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Overlay im Menü nicht anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Overlay im Menü ausblenden und wieder anzeigen, wenn das Menü verlassen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Overlay ausblenden wenn ein Controller verbunden ist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Overlay ausblenden, wenn ein physischer Controller an Port 1 angeschlossen ist, und wieder anzeigen, wenn der Controller getrennt wird."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Overlay ausblenden, wenn ein physischer Controller an Port 1 angeschlossen ist. Overlay wird nicht automatisch wiederhergestellt, wenn der Controller getrennt wird."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Eingaben im Overlay anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Registrierte Eingaben auf dem Bildschirm-Overlay anzeigen. \"Berührung\" hebt Overlay-Elemente hervor, die berührt bzw. geklickt werden. \"Physisch (Controller)\" hebt die tatsächliche an Cores übergebene Eingabe, typischerweise von einem angeschlossenen Controller/einer Tastatur, hervor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Berührung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Physisch (Controller)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Eingaben anzeigen von Port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Port des Eingabegeräts, der überwacht werden soll, wenn \"Eingaben im Overlay anzeigen\" auf \"Physisch (Controller)\" gesetzt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Mauszeiger mit dem Overlay anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Den Mauszeiger anzeigen wenn ein Overlay verwendet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Overlay automatisch rotieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Wenn dies durch das aktuelle Overlay unterstützt wird, wird das Layout automatisch gedreht, um es an die Bildschirmausrichtung/das Seitenverhältnis anzupassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Overlay automatisch skalieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Overlay-Maßstab und den Abstand der UI-Elemente automatisch an das Seitenverhältnis des Bildschirms anpassen. Erzielt die besten Ergebnisse mit Controller-Overlays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "D-Pad Diagonale Empfindlichkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Größe der diagonalen Zonen anpassen. Für 8-Wege-Symmetrie auf 100 % einstellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "ABXY Überlappungsempfindlichkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Größe der Überlappungszonen für ABXY-Tastenbereich anpassen. Für 8-Wege-Symmetrie auf 100 % setzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Analoge Neuzentrierungszone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Die Eingabe des analogen Sticks bezieht sich auf das erste Drücken, wenn er innerhalb dieser Zone gedrückt wird."
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
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "Bevorzugt das Laden von Overlays basierend auf Systemnamen, bevor auf die Standardeinstellung zurückgegriffen wird. Wird ignoriert, wenn eine Überschreibung für die Overlay-Voreinstellung festgelegt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Overlay-Deckkraft"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Deckkraft aller Bedienelemente des Overlays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Overlay-Voreinstellung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Ein Overlay im Dateibrowser auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(Querformat) Overlay-Skalierungsfaktor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Skalierungsfaktor aller UI-Elemente des Overlays bei Verwendung der Querformatausrichtung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(Querformat) Overlay-Seitenverhältnisanpassung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Einen Korrekturfaktor für das Seitenverhältnis auf das Overlay anwenden, wenn die Querformatanzeige verwendet wird. Positive Werte erhöhen (und negative Werte verringern) die effektive Overlay-Breite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Querformat) Overlay horizontale Trennung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Wenn dies durch die aktuelle Voreinstellung unterstützt wird, wird der Abstand zwischen UI-Elementen in der linken und rechten Hälfte eines Overlays angepasst, wenn die Querformatanzeige verwendet wird. Positive Werte erhöhen (und negative Werte verringern) die Trennung der beiden Hälften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Querformat) Overlay vertikale Trennung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Wenn dies durch die aktuelle Voreinstellung unterstützt wird, wird der Abstand zwischen UI-Elementen in der oberen und unteren Hälfte eines Overlays angepasst, wenn die Querformatanzeige verwendet wird. Positive Werte erhöhen (und negative Werte verringern) die Trennung der beiden Hälften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Querformat) X-Versatz des Overlays"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Horizontaler Versatz des Overlays bei Verwendung der Querformatanzeige. Positive Werte verschieben das Overlay nach rechts, negative Werte nach links."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Querformat) Y-Versatz des Overlays"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Vertikaler Versatz des Overlays bei Verwendung der Querformatanzeige. Positive Werte verschieben das Overlay nach oben, negative Werte nach unten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(Hochformat) Overlay-Skalierungsfaktor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Skalierungsfaktor aller UI-Elemente des Overlays bei Verwendung der Hochformatanzeige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(Hochformat) Overlay-Seitenverhältnisanpassung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Einen Korrekturfaktor für das Seitenverhältnis auf das Overlay anwenden, wenn die Hochformatanzeige verwendet wird. Positive Werte erhöhen (und negative Werte verringern) die effektive Overlay-Höhe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Hochformat) Overlay horizontale Trennung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Wenn dies durch die aktuelle Voreinstellung unterstützt wird, wird der Abstand zwischen UI-Elementen in der linken und rechten Hälfte eines Overlays angepasst, wenn die Hochformatanzeige verwendet wird. Positive Werte erhöhen (und negative Werte verringern) die Trennung der beiden Hälften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Hochformat) Overlay vertikale Trennung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Wenn dies durch die aktuelle Voreinstellung unterstützt wird, wird der Abstand zwischen UI-Elementen in der oberen und unteren Hälfte eines Overlays angepasst, wenn die Hochformatanzeige verwendet wird. Positive Werte erhöhen (und negative Werte verringern) die Trennung der beiden Hälften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Hochformat) X-Versatz des Overlays"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Horizontaler Versatz des Overlays bei Verwendung der Hochformatanzeige. Positive Werte verschieben das Overlay nach rechts, negative Werte nach links."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Hochformat) Y-Versatz des Overlays"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Vertikaler Versatz des Overlays bei Verwendung der Hochformatanzeige. Positive Werte verschieben das Overlay nach oben, negative Werte nach unten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "Tastatur-Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "Ein Tastatur-Overlay auswählen und anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "Overlay-Lightgun, Maus, und Zeiger aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "Verwendet alle Berührungseingaben, bei denen keine Overlay-Steuerelemente gedrückt werden, um Zeigegeräteeingaben für den Core zu erstellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "Overlay-Lightgun"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Konfiguriert die vom Overlay gesendete Lightgun-Eingabe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Overlay-Maus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Konfiguriert die vom Overlay gesendeten Mauseingaben. Hinweis: Durch Tippen mit einem, zwei oder drei Fingern werden Klicks mit der linken, rechten und mittleren Taste gesendet."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "Tastatur-Overlay-Voreinstellung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Bitte ein Tastatur-Overlay im Dateibrowser auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Größe von Tastatur-Overlay automatisch anpassen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Passt das Tastatur-Overlay an sein ursprüngliches Seitenverhältnis an. Deaktivieren, um auf den Bildschirm zu strecken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Deckkraft des Tastatur-Overlays"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Deckkraft aller Bedienelemente des Tastatur-Overlays."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Lightgun-Port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Legt den Core-Port fest, um Eingaben von der Overlay-Lightgun zu erhalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Jeder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Beim Berühren auslösen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Auslöseeingabe mit Zeigereingabe senden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Auslöseverzögerung (Frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Verzögert die Auslöseeingabe, um die Bewegung des Cursors zu ermöglichen. Diese Verzögerung wird auch verwendet, um auf die korrekte Multi-Touch-Anzahl zu warten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "2-fach-Toucheingabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Wählt die zu sendende Eingabe aus, wenn zwei Zeiger auf dem Bildschirm angezeigt werden. Die Auslöseverzögerung sollte zur Unterscheidung von anderen Eingaben ungleich Null sein."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "3-fach-Toucheingabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Wählt die zu sendende Eingabe aus, wenn drei Zeiger auf dem Bildschirm angezeigt werden. Die Auslöseverzögerung sollte zur Unterscheidung von anderen Eingaben ungleich Null sein."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "4-fach-Toucheingabe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Wählt die zu sendende Eingabe aus, wenn vier Zeiger auf dem Bildschirm angezeigt werden. Die Auslöseverzögerung sollte zur Unterscheidung von anderen Eingaben ungleich Null sein."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Off-Screen erlauben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Erlaubt das Zielen außerhalb des Spielfelds. Die Option deaktivieren, um das Ziel außerhalb des Bildschirms auf die Innenkante zu klemmen."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Mausgeschwindigkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Cursorbewegungsgeschwindigkeit anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Lang drücken zum Ziehen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Langes Drücken auf den Bildschirm, um mit dem Halten einer Taste zu beginnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Schwellwert für langes Drücken (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Passt die Haltezeit an, die für einen langen Druck erforderlich ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Doppeltes Tippen zum Ziehen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Dopelt auf den Bildschirm tippen, um beim zweiten Tippen eine Taste gedrückt zu halten. Fügt Latenz bei Mausklicks hinzu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Doppeltipp-Schwellwert (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Passt die zulässige Zeit zwischen den Tippvorgängen an, wann ein Doppeltipp erkannt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Wischgesten-Schwellenwert"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Passt den zulässigen Driftbereich an, wann ein langes Drücken oder Tippen erkannt wird. Wird als Prozentsatz der kleineren Bildschirmabmessung ausgedrückt."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Video-Layout aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Video-Layouts werden für Rahmen und andere Grafiken verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Video-Layout-Pfad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Ein Video-Overlay im Dateibrowser auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Ausgewählte Ansicht"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Eine Ansicht innerhalb des geladenen Layouts auswählen."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Bildschirm-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Bildschirmbenachrichtungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Grafik-Widgets"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Verwende dekorierte Animationen, Benachrichtigungen, Anzeigen und Steuerelemente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Grafik-Widgets automatisch skalieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Größe dekorierter Benachrichtigungen, Anzeigen und Steuerelemente basierend auf der aktuellen Menüskala automatisch ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Skalierung von Grafik-Widgets überschreiben (Vollbild)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Eine manuelle Überschreibung des Skalierungsfaktors anwenden, wenn Anzeige-Widgets im Vollbildmodus gezeigt werden. Gilt nur, wenn 'Grafik-Widgets automatisch skalieren' deaktiviert ist. Kann verwendet werden, um die Größe dekorierter Benachrichtigungen, Anzeigen und Steuerelemente unabhängig vom Menü zu erhöhen oder zu verringern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Skalierung von Grafik-Widgets überschreiben (Fenster)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Eine manuelle Überschreibung des Skalierungsfaktors anwenden, wenn Anzeige-Widgets im Fenstermodus gezeigt werden. Gilt nur, wenn 'Grafik-Widgets automatisch skalieren' deaktiviert ist. Kann verwendet werden, um die Größe dekorierter Benachrichtigungen, Anzeigen und Steuerelemente unabhängig vom Menü zu erhöhen oder zu verringern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Bildwiederholrate anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Aktuelle Bilder pro Sekunde anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Aktualisierungsintervall für die Bildwiederholrate (In Frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "Die Bildwiederholratenanzeige wird im eingestellten Intervall in Frames aktualisiert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Bildzähler anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Aktuelle Bildanzahl auf dem Bildschirm anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Statistiken anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Technische Statistiken auf dem Bildschirm anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Speichernutzung anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Die verwendete und die gesamte Arbeitsspeichermenge des Systems anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Aktualisierungsintervall für die Speichernutzung (In Frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "Die Anzeige der Speichernutzung wird im eingestellten Intervall in Frames aktualisiert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Netzwerkspiel-Ping anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Ping für den aktuellen Netzwerkspielraum anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "\"Inhalt laden\" Startbenachrichtigung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Beim Laden von Inhalten eine kurze Startfeedback-Animation zeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Eingabe (Automatische Konfiguration) Verbindungsbenachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Cheat-Code-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Eine Meldung auf dem Bildschirm anzeigen, wenn Cheat-Codes angewendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Patch-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Eine Meldung auf dem Bildschirm anzeigen, wenn Soft-Patches angewendet werden."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Eine Bildschirmmeldung anzeigen, wenn Eingabegeräte angeschlossen/getrennt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Benachrichtigungen beim Laden von Eingabe-Remaps"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Beim Laden von Eingabe-Remap-Dateien eine Bildschirmmeldung anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Benachrichtigungen beim Laden von Konfigurationsüberschreibungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Beim Laden von Konfigurationsüberschreibungsdateien eine Meldung auf dem Bildschirm anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Initial-Disc-Wiederherstellungsbenachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Eine Bildschirmmeldung anzeigen, wenn beim Start automatisch die zuletzt verwendete Disc von Multi-Disc-Inhalten wiederhergestellt wird, die über M3U-Wiedergabelisten geladen wurde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Disc-Steuerungsbenachrichtigung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Beim Einlegen und Auswerfen einer Disc eine Bildschirmmeldung anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Savestate-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "Zeigt auf dem Bildschirm eine Nachricht beim Speichern und Laden von Savestates an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Benachrichtigungen beim Vorspulen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Eine Bildschirmanzeige anzeigen, wenn Inhalte vorgespult werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Screenshot-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Eine Bildschirmmeldung anzeigen, wenn ein Screenshot aufgenommen wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Screenshot-Benachrichtigungspersistenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Definiert die Dauer der Screenshot-Meldung auf dem Bildschirm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Schnell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Sehr schnell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Sofort"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Screenshot Blitzeffekt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Beim Aufnehmen eines Screenshots einen weißen Blinkeffekt mit der gewünschten Dauer auf dem Bildschirm anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "EIN (Normal)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "EIN (Schnell)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Wiederholfrequenz-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Eine Bildschirmmeldung anzeigen, wenn die Wiederholfrequenz eingestellt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Zusätzliche Netzwerkspiel-Benachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Nicht essentielle Netzwerkspiel-Nachrichten anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Benachrichtigungen nur im Menü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Benachrichtigungen nur anzeigen, wenn das Menü geöffnet ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Benachrichtigungs-Schriftart"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Die Schriftart für Bildschirm-Benachrichtigungen auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Benachrichtigungs-Schriftgröße"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Gibt die Schriftgröße in Punkten an. Wenn Widgets verwendet werden, wirkt sich diese Größe nur auf die Anzeige der Statistiken auf dem Bildschirm aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Benachrichtigungs-Position (horizontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Eine eigene X-Achsenposition für Bildschirmmeldungen wählen. 0 ist der linke Rand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Benachrichtigungs-Position (vertikal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Eine eigene Y-Achsenposition für Bildschirmmeldungen wählen. 0 ist der untere Rand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Benachrichtigungs-Farbe (Rot)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Legt den Rotanteil der OSD-Textfarbe fest. Gültige Werte liegen zwischen 0 und 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Benachrichtigungs-Farbe (Grün)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Legt den Grünanteil der OSD-Textfarbe fest. Gültige Werte liegen zwischen 0 und 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Benachrichtigungs-Farbe (Blau)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Legt den Blauanteil der OSD-Textfarbe fest. Gültige Werte liegen zwischen 0 und 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Benachrichtigungs-Hintergrund"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Aktiviert eine Hintergrundfarbe für die OSD-Bildschirmanzeige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Benachrichtigungs-Hintergrundfarbe (Rot)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Legt den Rotanteil des OSD-Hintergrunds fest. Gültige Werte liegen zwischen 0 und 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Benachrichtigungs-Hintergrundfarbe (Grün)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Legt den Grünanteil des OSD-Hintergrunds fest. Gültige Werte liegen zwischen 0 und 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Benachrichtigungs-Hintergrundfarbe (Blau)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Legt den Blauanteil des OSD-Hintergrunds fest. Gültige Werte liegen zwischen 0 und 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Benachrichtigungshintergrund-Deckkraft"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Legt die Transparenz für den OSD-Hintergrund fest. Gültige Werte liegen zwischen 0,0 und 1,0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Sichtbarkeit von Menüelementen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Die Sichtbarkeit von Menüelementen in RetroArch umschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Darstellung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Einstellungen für das Aussehen des Menüs ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "Appsymbol"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Appsymbol ändern."
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "3DS untererer Bildschirm Erscheinungsbild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Einstellungen für das Erscheinungsbild des unteren Bildschirms ändern."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Erweiterte Einstellungen zeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Erweiterte Einstellungen für Power-Nutzer anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Kiosk-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Schützt das Setup, indem alle konfigurationsbezogenen Einstellungen ausgeblendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Passwort zur Deaktivierung des Kiosk-Modus festlegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Wird beim Aktivieren des Kiosk-Modus ein Passwort vergeben, kann der Modus später unter Angabe des Passworts im Hauptmenü wieder deaktiviert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Navigationsumlauf"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Zum Anfang und/oder Ende springen, wenn die Listengrenze horizontal oder vertikal erreicht wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Inhalt pausieren, wenn das Menü aktiv ist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Pausiert den Inhalt, wenn das Menü aktiv ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Inhalte nach dem Verwenden von Savestates fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Das Menü automatisch schließen und den Inhalt fortsetzen, nachdem ein Savestate erstellt oder geladen wurde. Das Deaktivieren dieser Option kann die Savestate-Operationen auf sehr langsamen Geräten beschleunigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Inhalte nach dem Disc-Wechsel fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Das Menü automatisch schließen und den Inhalt fortsetzen, nachdem eine neue Disc eingelegt oder geladen wurde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Programm beim Schließen von Inhalten beenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "RetroArch automatisch beenden, wenn Inhalte geschlossen werden. 'CLI' wird nur beendet, wenn der Inhalt über die Befehlszeile gestartet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Menü-Bildschirmschoner-Timeout"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Wenn das Menü aktiv ist, wird bei Inaktivität nach der angegebenen Zeit ein Bildschirmschoner angezeigt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Menü-Bildschirmschoner-Animation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Animationseffekt anzeigen, während der Menü-Bildschirmschoner aktiv ist. Bringt mäßige Leistungseinbußen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Schnee"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Sternenfeld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Menü-Bildschirmschoner-Animationsgeschwindigkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Geschwindigkeit des Animationseffekts des Menü-Bildschirmschoners anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Maus-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Die Steuerung des Menüs mit einer Maus ermöglichen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Touch-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Die Steuerung des Menüs mit einem Touchscreen ermöglichen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Multithreading"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Aufgaben in separaten Threads ausführen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Inhalt pausieren, wenn nicht aktiv"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Den Inhalt pausieren, wenn RetroArch nicht das aktive Fenster ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Deaktiviere Desktopgestaltung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Fenstermanager verwenden Desktopgestaltung, um unter anderem visuelle Effekte anzuwenden und nicht reagierende Fenster zu erkennen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Abschaltung der Desktopgestaltung erzwingen. Abschaltung bisher nur gültig für Windows Vista/7."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Menü Scroll-Beschleunigung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Maximale Geschwindigkeit des Cursors beim Halten einer Bildlaufrichtung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Menü Scroll-Verzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Initiale Verzögerung in Millisekunden, wenn eine Richtung zum Scrollen gedrückt gehalten wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "UI-Companion"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "UI-Companion beim Booten starten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Begleitenden Treiber für die Benutzeroberfläche beim Booten starten (wenn verfügbar)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Desktop-Menü (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Desktop-Menü beim Start öffnen"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Schnellmenü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Die Sichtbarkeit von Schnellmenü-Einträgen umschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Die Sichtbarkeit von Menüelementen im Einstellungen-Menü umschalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "\"Core laden\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Die Option \"Core laden\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "\"Inhalt laden\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Die Option \"Inhalt laden\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "\"Disc laden\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Die Option \"Disc laden\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "\"Disc dumpen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Die Option \"Disc dumpen\" im Hauptmenü anzeigen."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "\"Disc auswerfen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Die Option \"Disc auswerfen\" im Hauptmenü anzeigen."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "\"Online-Updater\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Die Option \"Online-Updater\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "\"Core-Downloader\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Das Aktualisieren von Cores (und Core-Infodateien) in der Option \"Online-Updater\" ermöglichen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Alten \"Vorschaubild-Updater\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Die alte Methode zum Herunterladen von Vorschaubildern in der Option \"Online-Updater\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "\"Informationen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Die Option \"Informationen\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "\"Konfigurationen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Die Option \"Konfigurationen\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "\"Hilfe\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Die Option \"Hilfe\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "\"RetroArch beenden\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Die Option \"RetroArch beenden\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "\"RetroArch neu starten\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Die Option \"RetroArch neu starten\" im Hauptmenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "\"Einstellungen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Das „Einstellungen“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Kennwort zum Wiederherstellen der \"Einstellungen\" festlegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Wird vor dem Verstecken der Einstellungen ein Passwort vergeben, können diese später unter Angabe des Passworts über die Funktion \"Einstellungen-Tab aktivieren\" im Hauptmenü wiederhergestellt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "\"Favoriten\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Das „Favoriten“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "\"Bilder\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Das „Bilder“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "\"Musik\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Das „Musik“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "\"Videos\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Das „Videos“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "\"Netzwerkspiel\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Das „Netzwerkspiel“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "\"Verlauf\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Das „Verlauf“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "\"Inhalte importieren\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "Das „Inhalte importieren“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH( /* FIXME can now be replaced with MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD */
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "\"Inhalte importieren\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "\"Inhalte importieren\"-Eintrag im Hauptmenü oder im Wiedergabelisten-Untermenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Hauptmenü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Wiedergabelisten-Menü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "\"Wiedergabelisten\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Wiedergabelisten im Hauptmenü anzeigen. Wird in GLUI ignoriert, wenn Wiedergabelisten und Navigationsleiste aktiviert sind."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "Wiedergabelisten-Tabs anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "Wiedergabelisten-Tabs anzeigen. Wirkt sich nicht auf RGUI aus. Navigationsleiste muss in GLUI aktiviert sein. (Neustart erforderlich auf Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "\"Erkunden\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Das „Erkunden“-Menü anzeigen. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Zeige 'Inhaltslose Cores'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Den Typ des Cores angeben, der im Menü „Inhaltslose Cores“ angezeigt werden soll. Bei der Einstellung „Benutzerdefiniert“ kann die Sichtbarkeit einzelner Cores über das Menü „Cores verwalten“ umgeschaltet werden. (Neustart erforderlich bei Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Alle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Einzelzweck"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Benutzerdefiniert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Uhrzeit / Datum anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Das aktuelle Datum und/oder die Zeit im Menü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Datum/Uhrzeit Stil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Den Stil, in dem das aktuelle Datum und/oder die aktuelle Uhrzeit im Menü angezeigt werden, ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Datum Trennzeichen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Das Zeichen angeben, das als Trennzeichen zwischen den Komponenten Jahr/Monat/Tag verwendet werden soll, wenn das aktuelle Datum im Menü angezeigt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Akkustand anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Den aktuellen Akkustand im Menü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Core-Namen anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Den Namen des aktuellen Cores im Menü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Zeige Menübeschreibungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Zusätzliche Informationen zu Menüeinträgen anzeigen."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Zeige Startbildschirm"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Zeige einen Startbildschirm im Menü an. Wird automatisch deaktiviert, nachdem das Programm zum ersten Mal gestartet wurde."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "\"Fortsetzen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Die Option zum Fortsetzen des Inhalts anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "\"Neu starten\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Die Option zum neu Starten des Inhalts anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "\"Schließen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Die Option zum Schließen des Inhalts anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Untermenü „Savestates“ anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Die Optionen von Savestate in einem Untermenü anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "\"Savestate speichern/laden\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Optionen zum Speichern/Laden eines Savestates anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "„Replay-Steuerung“ anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "Zeigt die Optionen für das Aufzeichenen/Abspielen von Replaydateien an."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "\"Speichern/Laden des Savestates rückgängig machen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Optionen für das Zurücksetzen des Speicherns/Ladens eines Savestates anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "„Core-Einstellungen“ anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Die Option „Core-Einstellungen“ anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "\"Optionen auf Datenträger schreiben\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Den Eintrag \"Optionen auf Datenträger schreiben\" im Menü \"Optionen > Core-Optionen verwalten\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "\"Steuerung\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Die Option \"Steuerung\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "\"Screenshot erstellen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Die Option \"Screenshot erstellen\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "\"Aufnahme starten\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Die Option \"Aufnahme starten\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "\"Streamen starten\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Die Option \"Streamen starten\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "\"Bildschirm-Overlay\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Die Option \"Bildschirm-Overlay\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "\"Video-Layout\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Die Option \"Video-Layout\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "\"Latenz\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Die Option \"Latenz\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "\"Zurückspulen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Die Option „Zurückspulen“ anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "\"Core-Überschreibungen speichern\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Die Option \"Core-Überschreibungen speichern\" im Menü \"Überschreibungen\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "„Inhaltsverzeichnisüberschreibungen speichern“ anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Die Option „Inhaltsverzeichnisüberschreibungen speichern“ im Menü „Überschreibungen“ anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "\"Spiel-Überschreibungen speichern\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Die Option \"Spiel-Überschreibungen speichern\" im Menü \"Überschreibungen\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "\"Cheats\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Die Option \"Cheats\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "\"Shader\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Die Option \"Shader\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "\"Zu Favoriten hinzufügen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Die Option \"Zu Favoriten hinzufügen\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "„Zur Wiedergabeliste hinzufügen“ anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Die Option „Zur Wiedergabeliste hinzufügen“ anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "\"Core zuordnen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Die Option „Core-Zuordnung festlegen“ anzeigen, wenn Inhalte nicht ausgeführt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "\"Core-Zuordnung zurücksetzen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Die Option „Core-Zuordnung zurücksetzen“ anzeigen, wenn Inhalte nicht ausgeführt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "\"Vorschaubilder herunterladen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Die Option „Thumbnails herunterladen“ anzeigen, wenn Inhalte nicht ausgeführt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "\"Informationen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Die Option \"Informationen\" anzeigen."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "\"Treiber\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Die \"Treiber\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "\"Video\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Die \"Video\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "\"Audio\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Die \"Audio\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "\"Eingabe\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Die \"Eingabe\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "\"Latenz\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Die \"Latenz\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "\"Cores\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Die \"Cores\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "\"Konfigurationen\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Die \"Konfigurationen\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "\"Speichern\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Die \"Speichern\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "\"Protokollierung\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Die \"Protokollierung\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "\"Dateibrowser\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Die \"Dateibrowser\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "\"Bildwiederholratenbegrenzung\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Die \"Bildwiederholratenbegrenzung\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "\"Aufnahme\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Die \"Aufnahme\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "\"Bildschirmanzeige\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Die \"Bildschirmanzeige\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "\"Benutzeroberfläche\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Die \"Benutzeroberfläche\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "\"KI-Dienst\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Die \"KI-Dienst\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "\"Bedienungshilfe\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Die \"Bedienungshilfe\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "\"Energieverwaltung\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Die \"Energieverwaltung\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "\"Errungenschaften\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Die \"Errungenschaften\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "\"Netzwerk\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Die \"Netzwerk\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "\"Wiedergabelisten\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Die \"Wiedergabelisten\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "\"Benutzer\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Die \"Benutzer\"-Einstellungen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "\"Verzeichnisse\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Die \"Verzeichnisse\"-Einstellungen anzeigen."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "„Steam“ anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "„Steam“-Einstellungen anzeigen."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Skalierungsfaktor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Größe der Benutzeroberflächen-Elemente im Menü skalieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Hintergrundbild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Ein Bild auswählen, das als Menühintergrund verwendet werden soll. Manuelle und dynamische Bilder übersteuern das „Farbthema“."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Hintergrundbild-Transparenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Legt die Deckkraft des Hintergrundbilds fest."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Deckkraft"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Ändert die Deckkraft des Standardhintergrunds für Menüs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Systemfarbthema verwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Das Farbthema des Betriebssystems verwenden (falls vorhanden). Überschreibt die Designeinstellungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Primäres Vorschaubild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Art der Vorschaubilder, die verwendet werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Vorschaubildskalierungs-Schwellwert"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Skaliert automatisch Vorschaubilder mit einer Breite/Höhe, die kleiner als der angegebene Wert ist. Verbessert die Bildqualität. Bringt moderate Leistungseinbußen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Lauftext-Animation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Wähle die horizontale Bildlaufmethode, mit der lange Menütexte angezeigt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Lauftext-Geschwindigkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Animationsgeschwindigkeit beim Scrollen von langem Menütext."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Gleichmäßiger Lauftext"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Eine flüssige Bildlaufanimation verwenden, wenn langer Menütext angezeigt wird. Hat einen geringen Einfluss auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Auswahl beim Wechseln von Tabs merken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Vorherige Cursorposition in Tabs merken. RGUI hat keine Tabs, aber Wiedergabelisten und Einstellungen verhalten sich als solche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Immer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Nur für Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Nur für Hauptmenü und Einstellungen"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "KI-Dienst Ausgabeart"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Übersetzung als Text-Overlay (Bildmodus), als Text-To-Speech (Sprachmodus) abspielen oder einen System-Erzähler wie NVDA (Erzählermodus) verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "KI-Dienst URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Eine http:// URL, die auf den zu verwendenden Übersetzungsdienst verweist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "KI-Dienst aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Ermöglicht das Ausführen des KI-Dienstes beim Drücken des KI-Dienst-Hotkeys."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Während der Übersetzung pausieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Den Core pausieren, während die Anzeige übersetzt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Ausgangssprache"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "Die Sprache, die der Dienst übersetzen soll. Bei 'Standard' wird versucht die Sprache automatisch zu erkennen. Das Auswählen einer Sprache verringert Fehler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Zielsprache"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "Die Sprache, in die der Dienst übersetzt. 'Standard' ist Englisch."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Bedienungshilfe aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Text-zu-Sprache zur Ünterstützung bei der Menünavigation aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Text-zu-Sprache Geschwindigkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Lesegeschwindigkeit der Text-zu-Sprache-Funktion."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Verdiene Errungenschaften in klassischen Spielen. Weitere Informationen unter 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Hardcore-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Deaktiviert Cheats, Zurückspulen, Zeitlupe und Laden von Savestates. Errungenschaften, die im Hardcore-Modus erzielt wurden, sind eindeutig gekennzeichnet, um anderen anzuzeigen, dass diese ohne Emulator-Hilfsfunktionen erreicht wurden. Das Umschalten dieser Einstellung zur Laufzeit startet das Spiel neu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Bestenlisten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Sendet regelmäßig im Kontext stehende Spielinformationen an die RetroAchievements-Webseite. Hat keinen Effekt, wenn 'Hardcore Modus' aktiviert ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Errungenschaftenabzeichen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Zeigt Abzeichen in der Errungenschaftenliste an."
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
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Freischalt-Ton"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Einen Ton abspielen, wenn eine Errungenschaft freigeschaltet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Automatische Bildschirmaufnahme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Macht automatisch einen Screenshot, wenn eine Errungenschaft erzielt wird."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Zugabemodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Die Sitzung mit aktivierten Errungenschaften (auch den zuvor freigeschalteten) starten."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Darstellung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Position und Versatz der Benachrichtigungen für Errungenschaften auf dem Bildschirm ändern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Legt den Rand/die Ecke des Bildschirms fest, wo die Errungenschaftsbenachrichtigungen angezeigt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Oben links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "Oben mittig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Oben rechts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Unten links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Unten mittig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "Unten rechts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Angepasstes Auffüllen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Legt fest, ob Errungenschaftsbenachrichtigungen an andere Arten von Bildschirmbenachrichtigungen ausgerichtet werden sollen. Deaktivieren, um manuelle Abstands/Positionswerte festzulegen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "Manuelles horizontales Auffüllen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Abstand zum linken/rechten Bildschirmrand, der den Bildschirm-Overscan ausgleichen kann."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "Manuelles vertikales Auffüllen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Abstand zum oberen/unteren Bildschirmrand, der den Bildschirm-Overscan ausgleichen kann."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "Sichtbarkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "Ändert, welche Nachrichten und Elemente auf dem Bildschirm angezeigt werden. Deaktiviert nicht die Funktionalität."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "Zusammenfassung beim Starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "Zeigt Informationen über das zu ladende Spiel und den aktuellen Fortschritt des Nutzers.\n„Alle identifizierten Spiele“ zeigen eine Zusammenfassung für Spiele ohne veröffentlichte Errungenschaften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "Alle identifizierten Spiele"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "Spiele mit Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "Entsperrbenachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "Zeigt eine Benachrichtigung, wenn eine Errungenschaft freigeschaltet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "Meisterschaftsbenachrichtigungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "Zeigt eine Benachrichtigung, wenn alle Errungenschaften eines Spiels freigeschaltet sind."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Aktive Challenge-Indikatoren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Zeigt Indikatoren auf dem Bildschirm, während bestimmte Errungenschaften erzielt werden können."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Fortschritts-Indikator"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Zeigt einen Indikator auf dem Bildschirm, wenn Fortschritte bei bestimmten Errungenschaften erzielt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "Startnachrichten einer Rangliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "Zeigt eine Beschreibung einer Rangliste, wenn sie aktiv wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Einreichungsnachrichten einer Rangliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Zeigt eine Nachricht mit dem eingereichten Wert an, wenn der Versuch einer Rangliste abgeschlossen ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Fehlschlagnachrichten einer Rangliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Zeigt eine Nachricht an, wenn ein Ranglistenversuch fehlschlägt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Ranglisten-Tracker"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Zeigt Tracker auf dem Bildschirm mit dem aktuellen Wert der aktiven Rangliste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Login-Meldungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Zeigt Nachrichten an, die sich auf die RetroAchievements-Kontoanmeldung beziehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Ausführliche Meldungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Zeigt zusätzliche Diagnose- und Fehlermeldungen an."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Netzwerkspiel öffentlich ankündigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Legt fest, ob Netzwerkspiele öffentlich angekündigt werden sollen. Wenn deaktiviert, müssen sich Clients manuell verbinden anstelle die öffentliche Lobby zu verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Relayserver verwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Leite alle Netzwerkspiel-Verbindungen durch einen Man-in-the-middle-Server. Hilfreich, wenn sich der Host hinter einer Firewall befindet oder Probleme mit NAT/UPnP hat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Relayserver-Standort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Einen Relayserver auswählen. Geografisch näher gelegene Standorte haben generell geringere Latenzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Benutzerdefinierte Relayserver-Adresse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Hier die Adresse eines eigenen Relayservers eingeben. Format: Adresse oder Adresse|Port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "Nordamerika (Ostküste, USA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "Westeuropa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "Südamerika (Südosten, Brasilien)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Südostasien"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_5,
   "Ostasien (Chuncheon, Südkorea)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Benutzerdefiniert"
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
   "Netzwerkspiel-TCP/UDP-Port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Der Port der Host-IP-Adresse. Kann entweder ein TCP- oder ein UDP-Port sein."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Maximale gleichzeitige Verbindungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Die maximale Anzahl aktiver Verbindungen, die der Host akzeptiert, bevor er neue Verbindungen ablehnt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Pinglimit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "Die maximale Verbindungslatenz (Ping), die der Host akzeptiert. 0 bedeutet kein Limit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Server-Passwort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Das Kennwort, das von Clients verwendet wird, um sich mit dem Host zu verbinden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Server-Passwort für Zuschauermodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Das Kennwort, das von Clients verwendet wird, die sich als Zuschauer mit dem Host verbinden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Netzwerkspiel-Zuschauermodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Das Netzwerkspiel im Zuschauermodus starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Gibt an, ob Netzwerkspiel im Zuschauermodus gestartet werden soll. Wenn aktiviert, wird Netzwerkspiel im Zuschauermodus starten. Es ist jederzeit möglich, den Modus später zu ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Verblassender Chat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Chat-Nachrichten langsam ausblenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Chatfarbe (Nickname)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Format (Nickname): #RRGGBB oder RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Chatfarbe (Nachricht)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Format (Nachricht): #RRGGBB oder RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Pausieren zulassen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Spielern erlauben, während des Netzwerkspiels zu pausieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Erlaube Slave-Modus für Clients"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Verbindungen im Slave-Modus zulassen. Slave-Modus-Clients benötigen auf beiden Seiten nur sehr wenig Rechenleistung, leiden jedoch erheblich unter der Netzwerklatenz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Verbiete Clients, die nicht im Slave-Modus laufen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Keine nicht-Slave-Modus-Verbindungen zulassen. Nicht empfohlen, außer für sehr schnelle Netzwerke mit sehr schwachen Maschinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Netzwerkspiel-Synchronisationsperiode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "Die Periode (in Frames), in der geprüft wird, ob Host und Client synchron sind."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "Die Frequenz in Frames, mit der beim Netzwerkspiel geprüft wird, ob Host und Clients synchron sind. Bei den meisten Cores hat dies keine sichtbaren Auswirkungen und kann ignoriert werden. Bei nichtdeterministischen Cores legt dieser Wert fest, wie oft die Netzwerkmitglieder synchronisiert werden. Bei fehlerhaften Cores verursachen Werte größer als 0 erhebliche Leistungsprobleme. Bei 0 werden keine Überprüfungen durchgeführt. Diese Einstellung wird nur auf dem Host verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Eingabeverzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Eingabeverzögerung in Frames, die im Netzwerkspiel verwendet wird, um die Netzwerklatenz zu verbergen. Reduziert Bildruckeln und CPU-Last, auf Kosten spürbarer Eingabeverzögerung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Die Anzahl der Frames der Eingabelatenz, die beim Netzwerkspiel verwendet wird, um die Netzwerklatenz zu verbergen.\nDies verzögert beim Netzwerkspiel die lokale Eingabe, sodass der ausgeführte Frame näher an den Frames ist, die vom Netzwerk empfangen werden. Dies verringert Ruckler und macht Netzwerkspiele weniger CPU-intensiv, verursacht allerdings eine spürbare Eingabeverzögerung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Erlaubte Eingabeverzögerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Der Bereich der Eingabeverzögerung in Frames, die im Netzwerkspiel verwendet werden kann, um die Netzwerklatenz zu verbergen. Reduziert Bildruckeln und CPU-Last, verursacht jedoch unvorhersehbare Eingabeverzögerungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Der Bereich der Frames der Eingabelatenz, die beim Netzwerkspiel verwendet wird, um die Netzwerklatenz zu verbergen.\nDamit wird beim Netzwerkspiel die Eingabelatenz in Frames dynamisch angepasst, um die CPU-Zeit, Eingabelatenz und Netzwerklatenz auszugleichen. Dies verringert Ruckler und macht Netzwerkspiele weniger CPU-intensiv, varursacht allerdings unvorhersehbare Eingabeverzögerung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Netzwerkspiel-NAT-Traversal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Versucht als Host, mit Hilfe von UPnP oder ähnlichen Technologien zum Verlassen des eigenen LANs, auf eingehende Verbindungen aus dem öffentlichen Internet zu lauschen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Digitale Eingaben teilen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Gerät %u anfragen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Anfragen, mit dem angegebenen Eingabegerät zu spielen."
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
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "Netzwerk-RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Netzwerk-RetroPad Basisport"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Nutzer %d Netzwerk-RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "stdin-Befehle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "stdin Befehlsschnittstelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "On-Demand-Vorschaubilder-Download"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Fehlende Vorschaubilder automatisch herunterladen, während die Wiedergabelisten durchsucht werden. Hat schwerwiegende Auswirkungen auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Updater-Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Zugriff auf Core-Updater-Einstellungen"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Buildbot-Core-URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL zum Core-Verzeichnis auf dem Libretro-Buildbot."
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Experimentelle Cores anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "\"Experimentelle\" Cores in der Core-Downloader-Liste auflisten. Diese dienen normalerweise nur zu Entwicklungs-/Testzwecken und werden nicht für den allgemeinen Gebrauch empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Backup von Cores bei Aktualisierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Erstellt automatisch Sicherungskopien aller installierten Cores, wenn ein Online-Update ausgeführt wird. Ermöglicht einfaches Zurücksetzen auf einen funktionierenden Core, wenn das Update eine Regression einführt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Maximale Anzahl an Core-Backups"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Angeben, wie viele automatisch generierte Sicherungen für jeden installierten Core aufbewahrt werden sollen. Wenn diese Grenze erreicht ist, wird beim Erstellen einer neuen Sicherung über ein Online-Update die älteste Sicherung gelöscht. Manuelle Core-Sicherungen sind von dieser Einstellung nicht betroffen."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Verlauf"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Eine Wiedergabeliste der zuletzt verwendeten Spiele, Bilder, Musik und Videos anlegen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Verlaufs-Größe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Begrenzt die Anzahl der Einträge in der Verlaufsliste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Favoritenanzahl"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Die Anzahl der Einträge in der Favoritenliste begrenzen. Sobald das Limit erreicht ist, werden neue Ergänzungen verhindert, bis alte Einträge entfernt wurden. Das Festlegen eines Werts von -1 ermöglicht „unbegrenzte“ Einträge.\nWARNUNG: Das Verringern des Wertes löscht vorhandene Einträge!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Umbenennen von Einträgen erlauben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Umbenennen von Wiedergabelisten-Einträgen erlauben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Erlaubt, Einträge zu entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Löschen von Wiedergabelisten-Einträgen erlauben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Wiedergabelisten alphabetisch sortieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Inhaltswiedergabelisten in alphabetischer Reihenfolge sortieren, mit Ausnahme der Wiedergabelisten 'Verlauf', 'Bilder', 'Musik' und 'Videos'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Wiedergabelisten im alten Format abspeichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Schreibt Wiedergabelisten im veralteten Klartextformat. Wenn deaktiviert, werden Wiedergabelisten mit JSON formatiert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Wiedergabelisten komprimieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Komprimiert Wiedergabelistendaten beim Schreiben auf den Datenträger. Reduziert die Dateigröße und Ladezeiten auf Kosten einer (vernachlässigbar) erhöhten CPU-Auslastung. Kann mit Wiedergabelisten im alten oder neuen Format verwendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Zugeordnete Cores in Wiedergabelisten anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Angeben, wann Wiedergabelisteneinträge mit dem aktuell zugeordneten Core gekennzeichnet werden sollen (falls vorhanden).\nDiese Einstellung wird ignoriert, wenn Sublabels für Wiedergabelisten aktiviert sind."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Sublabels zu Wiedergabelisten anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Zusätzliche Informationen für jeden Wiedergabelisteneintrag anzeigen, wie die aktuelle Core-Zuordnung und die Laufzeit (falls verfügbar). Hat einen variablen Einfluss auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Inhaltsspezifische Symbole im Verlauf und in den Favoriten anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Spezifische Symbole für jeden Verlaufs- und Favoriten-Wiedergabelisteneintrag anzeigen. Kann zu Leistungseinbußen führen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Spielzeit:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Zuletzt gespielt:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "Sekunde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "Sekunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "Minute"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "Minuten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "Stunde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "Stunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "Tag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "Tage"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "Woche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "Wochen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "Monat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "Monate"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "Jahr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "Jahre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "zuvor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Wiedergabelisteneintragsnummer anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Eintragsnummern in Wiedergabelisten anzeigen. Das Anzeigeformat ist abhängig vom aktuell ausgewählten Menütreiber."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Wiedergabelisten-Sublabel Laufzeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Festlegen, welche Art von Laufzeitprotokolldaten in Wiedergabelisten-Sublabels angezeigt werden soll.\nDas entsprechende Laufzeitprotokoll muss über das Optionsmenü 'Speichern' aktiviert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "'Zuletzt gespielt' Datum-und-Zeit Stil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Stil des Datums und der Uhrzeit festlegen, die für die Zeitstempelinformationen 'Zuletzt gespielt' angezeigt werden. Die '(AM/PM)' Einstellungen haben auf einigen Plattformen geringe Auswirkungen auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Oberflächlicher Archiv-Abgleich"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Wenn Wiedergabelisten nach Einträgen durchsucht werden, welche komprimierten Dateien zugeordnet sind, wird nur der Name der Archivdatei statt [file name] + [content] abgeglichen. Aktiviere diese Option, um doppelte Inhaltsverlaufseinträge beim Laden komprimierter Dateien zu vermeiden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Ohne Core-Übereinstimmung scannen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Erlauben, dass gescannte Inhalte zu einer Wiedergabeliste hinzugefügt werden, ohne dass ein Core installiert ist, der sie unterstützt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "Per Checksumme auf mögliche Duplikate scannen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "Manchmal duplizieren ISOs Seriennummern, insbesondere bei PSP/PSN-Titeln. Sich allein auf die Seriennummer zu verlassen, kann manchmal dazu führen, daß der Scanner Inhalte in das falsche System einfügt. Dadurch wird eine CRC-Prüfung hinzugefügt, die das Scannen erheblich verlangsamt, aber möglicherweise präziser ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Wiedergabelisten verwalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Wartungsarbeiten an Wiedergabelisten ausführen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Portable Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Wenn diese Option aktiviert ist und auch das Verzeichnis 'Dateibrowser' ausgewählt ist, wird der aktuelle Wert des Parameters 'Dateibrowser' in der Wiedergabeliste gespeichert. Wenn die Wiedergabeliste auf ein anderes System geladen wird, auf dem dieselbe Option aktiviert ist, wird der Wert des Parameters 'Dateibrowser' mit dem Wert der Wiedergabeliste verglichen. Wenn dies nicht der Fall ist, werden die Pfade der Wiedergabelisteneinträge automatisch korrigiert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "Dateinamen zur Miniaturansichten Zuordnung verwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "Wenn aktiviert, werden Miniaturansichten anhand des Dateinamens des Eintrags und nicht anhand seines Labels gefunden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "Alle unterstützten Bildtypen für Thumbnails erlauben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "Wenn aktiviert, können lokale Thumbnails in allen von RetroArch unterstützten Bildtypen (wie z. B. jpeg) hinzugefügt werden. Kann zu einer geringen Leistungseinbuße führen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "Verwalten"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Standard-Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Gib den Core an, der beim Starten von Inhalten über einen Wiedergabelisteneintrag verwendet werden soll, dem kein Core zugeordnet wurde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Core-Zuordnungen zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Vorhandene Core-Zuordnungen für alle Wiedergabelisteneinträge entfernen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Bezeichnungen-Anzeigemodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Die Art und Weise, wie die Inhaltsbezeichnungen in dieser Wiedergabeliste angezeigt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Sortiermethode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Festlegen, wie Einträge in dieser Wiedergabeliste sortiert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Wiedergabeliste bereinigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Core-Zuordnungen überprüfen und ungültige sowie doppelte Einträge entfernen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Wiedergabeliste aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Neue Inhalte hinzufügen und ungültige Einträge entfernen, indem der zuletzt zur Erstellung oder Bearbeitung der Wiedergabeliste verwendete Vorgang \"Manueller Scan\" wiederholt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Wiedergabeliste löschen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Wiedergabeliste vom Dateisystem entfernen."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Privatsphäre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Privatsphäre-Einstellungen ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Benutzerkonten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Aktuell konfigurierte Benutzerkonten verwalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Benutzername"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Gib Deinen Benutzernamen hier ein. Dieser wird unter anderem für Netzwerkspielsitzungen verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Sprache"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Sprache der Benutzeroberfläche festlegen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Übersetzt das Menü und alle Bildschirmmeldungen in die Sprache, die hier ausgewählt wurde. Damit die Änderungen wirksam werden, ist ein Neustart erforderlich.\nDie Vollständigkeit der Übersetzung wird neben jeder Option angezeigt. Falls Menüelemente in der gewählten Sprache nicht verfügbar sind, wird auf Englisch zurückgegriffen."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Erlaube Kamera-Zugriff"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Cores Zugriff auf die Kamera gewähren."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Der Discord App erlauben, Daten über den gespielten Inhalt anzuzeigen.\nNur verfügbar mit dem nativen Desktop-Client."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Standortbestimmung erlauben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Cores Zugriff auf den Standort gewähren."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Verdiene Errungenschaften in klassischen Spielen. Weitere Informationen unter 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Anmeldedaten für das RetroAchievements-Konto. Bitte retroachievements.org besuchen und sich für ein kostenloses Konto anmelden.\nNach erfolgter Registrierung muss der Benutzername und das Passwort in RetroArch eingeben werden."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Benutzername"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Den Benutzernamen Deines RetroAchievements-Kontos eingeben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Passwort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Gib das Passwort Deines RetroAchievements-Kontos ein. Maximale Länge: 255 Zeichen."
   )

/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOS, Boot-ROM und andere systemspezifische Dateien werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Heruntergeladene Dateien werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Von RetroArch verwendete Menü-Assets werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Dynamische Hintergrundbilder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Hintergrundbilder, die im Menü verwendet werden, werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Vorschaubilder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Box-Art-, Screenshot- und Titelbild-Vorschaubilder werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Startverzeichnis"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Das Startverzeichnis für den Dateibrowser festlegen."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Konfigurationsdateien"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Die Standardkonfigurationsdatei wird in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Libretro-Cores werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Core-Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Awendungs- und Core-Informationsdateien werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Datenbanken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Datenbanken werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Cheat-Dateien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Cheat-Dateien werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Videofilter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "CPU-basierte Video-Filter werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Audiofilter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Audio-DSP-Filter werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Video-Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "GPU-basierte Video-Shader werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Aufnahmen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Videoaufnahmen werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Aufnahme-Konfigurationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Aufzeichnungskonfigurationen werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Overlays werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Tastatur-Overlays"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "Tastatur-Overlays werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Video-Layouts"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Video-Layouts werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Screenshots werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Controller-Profile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "In diesem Verzeichnis werden Controller-Profile gespeichert, mit denen Controller automatisch konfiguriert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Eingabe-Remaps"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Eingabe-Remaps werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Wiedergabelisten werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Favoriten-Wiedergabeliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Die Favoriten-Wiedergabeliste wird in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Verlauf-Wiedergabeliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Die Verlauf-Wiedergabeliste wird in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Bilder-Wiedergabeliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Die Bilder-Wiedergabeliste wird in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Musik-Wiedergabeliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Die Musik-Wiedergabeliste wird in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Videos-Wiedergabeliste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Die Videos-Wiedergabeliste wird in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Laufzeitprotokolle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Laufzeitprotokolle werden in diesem Verzeichnis gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Speicherdaten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Alle Speicherdaten werden in diesem Verzeichnis gespeichert. Wenn kein Verzeichnis festgelegt ist, wird versucht, die Datei im Arbeitsverzeichnis des Inhalts zu speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Alle Speicherdaten (*.srm) in diesem Verzeichnis speichern. Dies beinhaltet verwandte Dateitypen wie .rt, .psrm usw. Bestimmte Kommandozeilenoptionen überschreiben diese Einstellung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Savestates"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Savestates und Replays werden in diesem Verzeichnis gespeichert. Wenn nicht festgelegt, wird versucht, sie in dem Verzeichnis zu speichern, in dem sich der Inhalt befindet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Temporäre Dateien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Archivierte Inhalte werden temporär in dieses Verzeichnis entpackt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Systemereignisprotokolle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Systemereignisprotokolle werden in diesem Verzeichnis gespeichert."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Rich-Presence aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Den aktuellen Status in RetroArch auf Steam teilen."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Inhaltsformat für Rich-Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Festlegen, welche Informationen über den Inhalt geteilt werden sollen."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Inhalt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Core-Name"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Systemname"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Inhalt (Systemname)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Inhalt (Core-Name)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Inhalt (Systemname - Core-Name)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Zum Mixer hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Diese Audiospur einem verfügbaren Audiostreamslot hinzufügen.\nWenn momentan kein Slot verfügbar ist, wird dies ignoriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Zum Mixer hinzufügen und abspielen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Diese Audiospur einem verfügbaren Audiostreamslot hinzufügen und sie wiedergeben.\nWenn momentan kein Slot verfügbar ist, wird dies ignoriert."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Selbst hosten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Zu Netzwerkspiel-Host verbinden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Netzwerkspieladresse eingeben und als Client verbinden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Verbindung zum Netzwerkspiel trennen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Aktive Netzwerkspiel-Verbindung trennen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Lobby-Filter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Nur verbindbare Räume"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Nur installierte Cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Kennwortgeschützte Räume"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Netzwerkspielliste aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Nach Netzwerkspielen suchen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "LAN-Netzwerkspielliste aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Im LAN nach Netzwerkspielen suchen."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Hosting starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Netzwerkspiel im Host-Modus (Server) starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Netzwerkspiel-Hosting stoppen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "Client rausschmeißen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Einen Client aus dem aktuell gehosteten Raum schmeißen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "Client sperren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Einen Client aus dem aktuell gehosteten Raum verbannen."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Verzeichnis scannen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Ein Verzeichnis nach kompatiblen Inhalten durchsuchen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Dieses Verzeichnis scannen>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "Auswählen, um das gewählte Verzeichnis nach Inhalten zu durchsuchen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Datei scannen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Eine Datei nach kompatiblen Inhalten durchsuchen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Manueller Scan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Konfigurierbarer Scan basierend auf den Namen der Inhaltsdateien. Erfordert nicht, dass der Inhalt mit den Datenbanken übereinstimmt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "Scannen"
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Zum mixer hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Zum Mixer hinzufügen und abspielen"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Inhaltsverzeichnis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Ein Verzeichnis auswählen, in dem nach Inhalten gesucht werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Systemname"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Einen 'Systemnamen' angeben, dem gefundene Inhalte zugeordnet werden sollen. Wird verwendet, um die generierte Wiedergabelistendatei zu benennen und Vorschaubilder der Wiedergabeliste zu identifizieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Benutzerdefinierter Systemname"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Manuell einen 'Systemnamen' für gefundene Inhalte angeben. Wird nur verwendet, wenn 'Systemname' auf '<Benutzerdefiniert>' gesetzt ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Standard-Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Einen Standard-Core auswählen, der beim Starten gefundener Inhalte verwendet werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Dateierweiterungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Liste der Dateitypen, die in den Scan aufgenommen werden sollen, getrennt durch Leerzeichen. Wenn leer, enthält es alle Dateitypen oder wenn ein Core angegeben ist, alle vom Core unterstützten Dateien."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Rekursiv scannen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Wenn diese Option aktiviert ist, werden alle Unterverzeichnisse des angegebenen 'Inhaltsverzeichnises' in den Scan einbezogen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "In Archiven scannen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Wenn diese Option aktiviert ist, werden Archivdateien (.zip, .7z usw.) nach gültigen/unterstützten Inhalten durchsucht. Kann einen erheblichen Einfluss auf die Scanleistung haben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Arcade DAT-Datei"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Wähle eine Logiqx oder MAME XML-DAT-Datei aus, um die automatische Benennung von gescannten Arcade-Inhalten (MAME, FinalBurn Neo usw.) zu ermöglichen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Arcade DAT-Filter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Wenn eine Arcade-DAT-Datei verwendet wird, wird der Inhalt nur dann zur Wiedergabeliste hinzugefügt, wenn ein passender DAT-Dateieintrag gefunden wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Vorhandene Wiedergabelisten überschreiben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Wenn diese Option aktiviert ist, wird jede vorhandene Wiedergabeliste vor dem Scannen von Inhalten gelöscht. Wenn deaktiviert, bleiben vorhandene Wiedergabelisteneinträge erhalten und es werden nur Inhalte hinzugefügt, die in der Wiedergabeliste fehlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Vorhandene Einträge validieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Wenn aktiviert, werden Einträge in bestehenden Wiedergabelisten vor dem Scannen neuer Inhalte überprüft. Einträge, die auf fehlende Inhalte und/oder Dateien mit ungültigen Erweiterungen verweisen, werden entfernt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Scan starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Ausgewählten Inhalt scannen."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Liste wird geladen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Erscheinungsjahr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Spieleranzahl"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Tag (Etikett)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Namenssuche ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Alles anzeigen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Zusätzlicher Filter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Alles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Zusätzlichen Filter hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u Einträge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Nach Entwickler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Nach Publisher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Nach Erscheinungsjahr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Nach Spielerzahl"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Nach Genre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Nach Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Nach Kategorie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Nach Sprache"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Nach Region"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Nach Konsolenexklusivität"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Nach Plattformexklusivität"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Nach Rumble-Funktion"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Nach Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Nach Medium"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Nach Steuerung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Nach Grafikstil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Nach Gameplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Nach Handlung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "Nach Pacing"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Nach Perspektive"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Nach Setting"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Nach Ansicht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Nach Fahrzeugtyp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Nach Ursprung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Nach Franchise"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Nach Tag (Etikett)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Nach Systemname"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Bereichsfilter setzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Ansicht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Als Ansicht speichern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Diese Ansicht löschen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Namen der neuen Ansicht eingeben"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Ansicht existiert bereits mit dem gleichen Namen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "Ansicht wurde gespeichert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "Ansicht wurde gelöscht"
   )

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
   "Umbenennen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
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
   "Den Inhalt zu 'Favoriten' hinzufügen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "Zur Wiedergabeliste hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "Den Inhalt einer Wiedergabeliste hinzufügen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "Neue Wiedergabeliste erstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "Eine neue Wiedergabeliste erstellen und dieser den aktuellen Eintrag hinzufügen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Core zuordnen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Core einstellen, der diesem Inhalt zugeordnet ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Core-Zuordnung zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Core zurücksetzen, der diesem Inhalt zugeordnet ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Weiterführende Informationen über diesen Inhalt ansehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Vorschaubilder herunterladen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Screenshot-/Box-Art-/Titelbild-Vorschaubilder für den aktuellen Inhalt herunterladen. Aktualisiert alle vorhandenen Vorschaubilder."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Aktueller Core"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Dateipfad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Eintrag: %lu/%lu"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Spielzeit"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Zuletzt gespielt"
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
   "Den Inhalt fortsetzen und das Schnellmenü verlassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Neu starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Den Inhalt vom Anfang neu starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Schließen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Den Inhalt schließen. Alle nicht gespeicherte Änderungen können verloren gehen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Screenshot erstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Foto des Bildschirms aufnehmen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Savestate-Speicherplatz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Den aktuell gewählten Speicherplatz ändern."
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
   "Laden des Savestates rückgängig machen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Wenn ein Savestate geladen wurde, wird der Inhalt zum Zustand vor dem Laden des Savestates zurückgesetzt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Speichern des Savestates rückgängig machen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Wenn ein Savestate überschrieben wurde, wird es auf den vorherigen Zustand zurückgesetzt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "Replayslot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "Den aktuell gewählten Speicherplatz ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Replay abspielen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Replaydatei aus dem aktuell ausgewählten Slot abspielen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Replay aufnehmen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Replaydatei in den aktuell ausgewählten Slot aufnehmen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Aufzeichnung/Wiedergabe stoppen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "Stoppt das Aufzeichnen/Abspielen des aktuellen Replays"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Zu Favoriten hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Den Inhalt zu 'Favoriten' hinzufügen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Aufnahme starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Videoaufnahme starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Aufnahme beenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Videoaufnahme beenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Streamen starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Streaming zum gewählten Ziel starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Streamen beenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Stream beenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Savestates"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Zugriff auf die Optionen von Savestate."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Core-Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Die Optionen für den Inhalt ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Steuerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Die Steuerung für den Inhalt ändern."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Verwende Cheat-Codes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Disc-Steuerung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Verwaltung von Disc-Abbildern."
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
   "Einstellungen zum Überschreiben der globalen Konfiguration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Errungenschaften"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Errungenschaften und zugehörige Einstellungen anzeigen."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Core-Optionen verwalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Überschreibende Einstellungen für den aktuellen Inhalt speichern oder löschen."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Spieleinstellungen speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Core-Optionen speichern, die nur für den aktuellen Inhalt gelten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Spieloptionen entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Core-Optionen löschen, die nur für den aktuellen Inhalt gelten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Inhaltsverzeichnis-Einstellungen speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Core-Optionen speichern, die für alle Inhalte aus dem Verzeichnis der aktuellen Inhalt gelten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Optionen für Inhaltsverzeichnisse entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Core-Optionen löschen, die für alle Inhalte aus dem Verzeichnis der aktuellen Inhalt gelten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Aktive Einstellungsdatei"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "Die aktuell verwendete Einstellungsdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Einstellungen zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Alle Core-Einstellungen auf Standardwerte zurücksetzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Optionen auf Datenträger schreiben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Die aktuellen Einstellungen in die aktive Optionsdatei schreiben. Stellt sicher, dass die Optionen erhalten bleiben, falls das Frontend aufgrund eines Fehlers nicht ordnungsgemäß herunterfährt."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Spieleinstellungsdatei erstellen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Spieleinstellungsdatei speichern"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Tastenzuordnungsdateien verwalten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Tastenzuordnungsdateien für den aktuellen Inhalt laden, speichern oder löschen."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Aktive Tastenzuordnungsdatei"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "Die aktuell verwendete Tastenzuordnungsdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Remap-Datei laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Eingabezuordnungen laden und akuelle ersetzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Remapdatei speichern unter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Aktuelle Eingabezuordnungen als neue Remapdatei speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Speichere Core-Remap-Datei"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Eine Remapdatei speichern, die für alle mit diesem Core geladenen Inhalte gilt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Core-Remapdatei entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Remapdatei löschen, die für alle mit diesem Core geladenen Inhalte gilt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Inhaltsverzeichnis-Remap-Datei speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Remapdatei speichern, die für alle Inhalte gilt, die aus demselben Verzeichnis wie die aktuelle Datei geladen werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Remapdatei für das Spielinhaltsverzeichnis entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Remapdatei löschen, die für alle Inhalte gilt, die aus demselben Verzeichnis wie die aktuelle Datei geladen werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Speichere Spiel-Remap-Datei"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "Remapdatei speichern, die nur für den aktuellen Inhalt gilt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Spiel-Remapdatei entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "Remapdatei löschen, die nur für den aktuellen Inhalt gilt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Tastenzuordnung zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Alle Tastenzuordnungsoptionen auf Standardwerte zurücksetzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Eingabe-Remap-Datei aktualisieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Aktive Remap-Datei mit den aktuellen Eingabe-Remap-Optionen überschreiben."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Remap-Datei"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Cheatsuche starten oder fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT,
   "Speicher durchsuchen, um neue Cheats zu erstellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Cheat-Datei laden (ersetzen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Eine Cheat-Datei laden und bereits angewandte Cheats überschreiben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Cheat-Datei laden (Anhängen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Eine Cheat-Datei laden und an vorhandene Cheats anhängen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Spielspezifische Cheats neu laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Speichere Cheat-Datei unter ..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Aktuelle Cheats als Cheat-Datei speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Neuen Cheat oben hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Neuen Cheat unten hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Alle Cheats löschen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Cheats beim Spielstart anwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Cheats beim Laden des Spiels automatisch anwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Sofort anwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Cheats sofort nach dem Einschalten anwenden."
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

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Cheat-Suche starten oder neu starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Links oder Rechts drücken, um die Wortbreite zu ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Links oder Rechts drücken, um den Wert zu ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Gleich zu %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Kleiner als vorher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Kleiner als oder gleich zu vorher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Größer als vorher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Größer als oder gleich zu vorher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Gleich zu vorher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Nicht gleich zu vorher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Links oder Rechts drücken, um den Wert zu ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Gleich zu vorher +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Durchsuche Speicher nach Werten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Links oder Rechts drücken, um den Wert zu ändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Gleich zu vorher +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Die %u Treffer zu Deiner Liste hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Lösche Treffer #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Erstelle Code zu Treffer #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Speicheradresse des Treffers: %08X Maske: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Cheat-Datei (ersetzen)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Cheat-Datei (Anhängen)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Cheat-Details"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Cheat-Listenposition."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Aktiviert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Beschreibung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Speicher-Suchgröße"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Typ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Wert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Speicheradresse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Durchsuche Adresse: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Speicheradressenmaske"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Adressen-Bitmaske bei Speichersuchgröße < 8-bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Anzahl Iterationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Die Häufigkeit, mit der der Cheat angewandt wird. Verwende diese Option mit den beiden anderen Iterationsoptionen, um große Speicherbereiche zu beeinflussen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Adresserhöhung jede Iteration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Nach jeder Iteration wird die 'Speicheradresse' um diese Zahl mal der 'Speichersuchgröße' erhöht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Werterhöhung jede Iteration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Nach jeder Iteration wird der 'Wert' um diesen Betrag erhöht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Vibrieren, wenn der Speicherwert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Vergleichswert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Vibrations-Port"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Intensität des primären Vibrators"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Primäre Vibrationsdauer (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Intensität des sekundären Vibrators"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Sekundäre Vibrationsdauer (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Nach diesem einen neuen Cheat hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Vor diesem einen neuen Cheat hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Diesen Cheat danach kopieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Diesen Cheat davor kopieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Diesen Cheat entfernen"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Disc auswerfen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Virtuelles Disc-Fach öffnen und aktuell geladene Disc entfernen. Wenn 'Inhalt pausieren, wenn das Menü aktiv ist' aktiviert ist, registrieren einige Cores möglicherweise keine Änderungen, es sei denn, der Inhalt wird nach jeder Disc-Steueraktion einige Sekunden lang fortgesetzt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Disc einlegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Disc einfügen, die dem 'Aktuellen Disc-Index' entspricht, und das virtuelle Laufwerk schließen. Wenn 'Inhalt pausieren, wenn das Menü aktiv ist' aktiviert ist, registrieren einige Cores möglicherweise keine Änderungen, es sei denn, der Inhalt wird nach jeder Disc-Steueraktion einige Sekunden lang fortgesetzt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Neue Disc laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Aktuelle Disc auswerfen, eine neue Disc vom Dateisystem auswählen, einlegen und das virtuelle Disc-Fach schließen.\nHINWEIS: Dies ist eine veraltete Funktion. Es wird stattdessen empfohlen, Multi-Disc-Titel über M3U-Wiedergabelisten zu laden, die die Disc-Auswahl mit den Optionen 'Disc auswerfen/einlegen' und 'Aktueller Disc-Index' ermöglichen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Eine neue Disc vom Dateisystem auswählen und einlegen, ohne das virtuelle Laufwerk zu schließen.\nHINWEIS: Dies ist eine veraltete Funktion. Es wird stattdessen empfohlen, Multi-Disc-Titel über M3U-Wiedergabelisten zu laden, die die Disc-Auswahl mit der Option 'Aktueller Disc-Index ' ermöglichen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Aktueller Disc-Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Die aktuelle Disc aus der Liste der verfügbaren Abbilder wählen. Die Disc wird geladen, wenn 'Disc einlegen' ausgewählt wird."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Video-Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Video-Shader-Pipeline aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Shader-Dateien auf Änderungen beobachten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Änderungen an Shader-Dateien auf dem Datenträger automatisch übernehmen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Shaderdateien auf neue Änderungen beobachten. Nach dem Speichern von Änderungen an einem Shader auf den Datenträger, wird dieser automatisch neu kompiliert und auf den Inhalt angewendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Das zuletzt verwendete Shader-Verzeichnis merken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Dateibrowser beim Laden von Shader-Voreinstellungen im zuletzt verwendeten Verzeichnis öffnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Voreinstellung laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Lade eine Shader-Voreinstellung. Die Shader-Pipeline wird automatisch konfiguriert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Lädt eine Shader-Voreinstellung direkt. Das Shadermenü wird entsprechend aktualisiert.\nDer im Menü angezeigte Skalierungsfaktor ist nur dann betriebssicher, wenn die Voreinstellung einfache Skalierungsmethoden verwendet (z. B. Quellskalierung, gleicher Skalierungsfaktor für X/Y)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Voreinstellung voranstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Voreinstellung vor aktuell geladene Voreinstellung stellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Voreinstellung anhängen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Voreinstellung hinter aktuell geladene Voreinstellung hängen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
   "Voreinstellung speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "Die aktuelle Shader-Voreinstellung speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
   "Voreinstellung entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "Automatische Shader-Voreinstellungen entfernen."
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
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "Nach dem Ändern der Shader-Einstellungen, wie z. B. Anzahl der Shaderdurchgänge, Filter, FBO-Skala, diese Option verwenden, um Änderungen durchzuführen.\nDiese Shader-Einstellungen zu ändern, ist ein komplexer Vorgang, daher muss genau vorgegangen werden.\nWerden Shader angewendet, werden die Shadereinstellungen in einer temporären Datei gespeichert (retroarch.slangp/.cgp/.glslp) und geladen. Die Datei bleibt nach dem Beenden von RetroArch erhalten und wird im Shaderverzeichnis gespeicher[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Vorschau der Shader-Parameter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Den aktuellen Shader modifizieren. Änderungen werden nicht in den Shader-Voreinstellungen gespeichert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Shader-Durchläufe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Erhöhe oder verringere die Anzahl der Shader-Pipeline-Durchgänge. Separate Shader können an jeden Pipeline-Durchgang gebunden werden und dessen Skalierung und Filterung konfigurieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch erlaubt es, verschiedene Shader in beliebigen Shader-Durchläufen mit benutzerdefinierten Hardwarefiltern und Skalierungsfaktoren zu kombinieren.\nDiese Option gibt die Anzahl der zu verwendenden Shader-Durchläufe an. Wenn diese Option auf 0 gesetzt und angewandt wird, wird ein „leerer“ Shader verwendet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "Pfad zum Shader. Alle Shader müssen den gleichen Typ haben (z. B. Cg, GLSL oder Slang). Shaderverzeichnis setzen, um festzulegen, wo der Browser nach Shadern sucht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "Hardwarefilter für diesen Durchgang. Ist „Standard“ eingestellt, wird entsprechend der Einstellung „Bilineare Filterung“ in den Videoeinstellungen entweder „Linear“ oder „Nächster“ gefiltert."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Skalierung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "Skalierung für diesen Durchgang. Der Skalierungsfaktor wird akkumuliert, d. h. 2x für den ersten Durchgang und 2x für den zweiten Durchgang ergibt eine Gesamtskalierung von 4×.\nWenn es einen Skalierungsfaktor für den letzten Durchgang gibt, wird das Ergebnis mit dem im Standardfilter angegebenen Filter auf den Bildschirm gestreckt, abhängig von der Einstellung der bilinearen Filterung in den Videoeinstellungen.\nWenn „Standard“ eingestellt ist, wird entweder auf den Vollbildschirm ge[...]"
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Einfache Voreinstellungen"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Eine Shader-Voreinstellung speichern, die nur die von Dir vorgenommenen Parameteränderungen und einen Link zur ursprünglichen Voreinstellung enthält."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Shader-Voreinstellung speichern unter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Die aktuellen Shader-Einstellungen als neue Shader-Voreinstellung speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Globale Voreinstellung speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Die aktuellen Shader-Einstellungen als globale Standardeinstellung speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Core-Voreinstellung speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Die aktuellen Shader-Einstellungen als Standard für diesen Core speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Voreinstellung des Inhaltsverzeichnisses speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Die aktuellen Shader-Einstellungen als Standard für alle Dateien im aktuellen Verzeichnis speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Spiel-Voreinstellung speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Die aktuellen Shader-Einstellungen als Standard-Einstellung für diesen Inhalt speichern."
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Keine automatischen Shader-Voreinstellungen gefunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Globale Voreinstellung entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Die globale Voreinstellung, die von allen Inhalten und Cores verwendet wird, entfernen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Core-Voreinstellung entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Die Core-Voreinstellung, die von allen Inhalten verwendet wird, die mit dem aktuell geladenen Core ausgeführt werden, entfernen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Inhaltsverzeichnis-Voreinstellung entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Die Inhaltsverzeichnis-Voreinstellung, die von allen Inhalten im aktuellen Arbeitsverzeichnis verwendet wird, entfernen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Spielvoreinstellung entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Entfernt die Spielvoreinstellung, die nur für das betreffende Spiel verwendet wird."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Keine Shader-Parameter"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "Aktive Überschreibungsdatei"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "Die aktuell verwendete Überschreibungsdatei."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "Überschreibungsdatei laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "Aktuelle Konfiguration laden und ersetzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "Overrides speichern als"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "Die aktuelle Konfiguration als eine neue Override-Datei speichern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Core-Überschreibungen speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Eine überschreibende Konfiguration speichern, die für jeden Inhalt gilt, der mit diesem Core geladen wird. Hat Vorrang vor der Hauptkonfiguration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Core-Überschreibungen entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Die Überschreibungskonfigurationsdatei für alle Inhalte löschen, die mit diesem Core geladen werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Überschreibende Konfiguration fürs Inhaltsverzeichnis speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Eine überschreibende Konfigurationsdatei speichern, die für jeden Inhalt gilt, der aus demselben Verzeichnis wie die aktuelle Datei geladen wird. Hat Vorrang vor der Hauptkonfiguration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Inhaltsverzeichnisüberschreibungen entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Die Überschreibungskonfigurationsdatei für alle Inhalte löschen, die aus demselben Verzeichnis wie die aktuelle Datei geladen werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Spiel-Überschreibungen speichern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Eine überschreibende Konfiguration speichern, die nur für den aktuellen Inhalt gilt. Hat Vorrang vor der Hauptkonfiguration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Spielüberschreibungen entfernen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Die Überschreibungskonfigurationsdatei löschen, die nur für den aktuellen Inhalt gilt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "Überschreibung zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "Alle Einstellungen auf Standardwerte zurücksetzen."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Keine Errungenschaften vorhanden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Errungenschaften-Hardcore-Modus-Pausieren abbrechen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Den Errungenschaften-Hardcore-Modus für die aktuelle Sitzung aktiviert lassen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Errungenschaften-Hardcore-Modus-Fortsetzen abbrechen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Den Errungenschaften-Hardcore-Modus für die aktuelle Sitzung deaktiviert lassen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Errungenschaften-Hardcore-Modus pausieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Pausiert den Hardcore-Modus der Errungenschaften für die aktuelle Sitzung. Diese Aktion aktiviert Cheats, Zurückspulen, Zeitlupe und Laden von Savestates."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Errungenschaften-Hardcore-Modus fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Setzt den Hardcore-Modus der Errungenschaften für die aktuelle Sitzung fort. Diese Aktion deaktiviert Cheats, Zurückspulen, Zeitlupe, und Laden von Savestates und setzt das aktuelle Spiel zurück."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "RetroAchievements-Server ist nicht erreichbar"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Eine oder mehrere Entsperrungen haben es nicht auf den Server geschafft. Die Entsperrungen werden erneut versucht, solange die App geöffnet bleibt."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "RetroAchievements-Server ist nicht erreichbar. Es wird bis zum Erfolg erneut versucht oder bis die App geschlossen wird."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "Alle ausstehenden Anfragen wurden erfolgreich mit dem RetroAchievements-Server synchronisiert."
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "Spiel wird identifiziert"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "Spieldaten werden abgerufen"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "Sitzung wird gestartet"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Nicht eingeloggt"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Netzwerkfehler"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Unbekanntes Spiel"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Errungenschaften lassen sich mit diesem Core nicht aktivieren"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Datenbankeintrag"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Datenbankinformationen für aktuellen Inhalt anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Keine Einträge vorhanden"
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
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Keine Core-Backups verfügbar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Keine Favoriten verfügbar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Kein Verlauf verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Keine Bilder verfügbar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Keine Musik verfügbar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Keine Videos verfügbar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Keine Informationen verfügbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Keine Wiedergabelisten-Einträge verfügbar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Keine Einstellungen gefunden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Keine Bluetooth-Geräte gefunden"
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
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "Miniaturansichten umlaufend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RANDOM_SELECT,
   "Zufällig auswählen"
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
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "Zurück zum übergeordneten Verzeichnis."
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90 Grad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180 Grad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270 Grad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90 Grad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180 Grad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270 Grad"
   )

/* Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_95_PLUS,
   ">95 %"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_75_PLUS,
   "75–95 %"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_50_PLUS,
   "50–74 %"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_25_PLUS,
   "25–49 %"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_25_MINUS,
   "<25 %"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Unbekannter Compiler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Teilen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Ausschließen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Abstimmen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Analoge Eingaben teilen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Maximum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Mitteln"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Nein"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Keine Präferenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Links/Rechts abprallen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Nach links scrollen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Bildmodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Sprachmodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Erzählermodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Verlauf & Favoriten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Alle Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Verlauf & Favoriten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Immer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Nie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Pro Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Gesamtsumme"
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
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "Keine Quelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Dieses Verzeichnis verwenden>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "Dieses als Verzeichnis auswählen."
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
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad mit Analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Kein"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Unbekannt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Unten + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Start halten (2 Sekunden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Select halten (2 Sekunden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Unten + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Deaktiviert>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Sich ändert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Sich nicht ändert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Sich erhöht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Sich verringert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= Vergleichswert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= Vergleichswert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< Vergleichswert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> Vergleichswert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Sich um Vergleichswert erhöht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Sich um Vergleichswert verringert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Alle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Deaktiviert>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Fester Wert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Um Wert erhöhen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Um Wert verringern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Nächsten Cheat ausführen wenn Wert = Speicherwert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Nächsten Cheat ausführen wenn Wert != Speicherwert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Nächsten Cheat ausführen wenn Wert < Speicherwert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Nächsten Cheat ausführen wenn Wert > Speicherwert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Bit, maximaler Wert = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-Bit, maximaler Wert = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-Bit, maximaler Wert = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-Bit, maximaler Wert = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-Bit, maximaler Wert = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-Bit, maximaler Wert = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Systemstandard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Alphabetisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Keins"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Vollständige Bezeichnungen anzeigen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "()-Informationen entfernen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "[]-Informationen entfernen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "() und [] entfernen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Region behalten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Disc-Index behalten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Region und Disc-Index behalten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Systemstandard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Titelbildschirm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "Logo für Inhalt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Schnell"
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
   "K.A."
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
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Inoffiziell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Nicht unterstützt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Kürzlich freigeschaltet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Fast fertig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Aktive Herausforderungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Nur Tracker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Nur Benachrichtigungen"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Standard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Nächster"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Haupt-Icons"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Inhalts-Icons"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Inhaltsverzeichnis>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Benutzerdefiniert>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Nicht spezifiziert>"
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
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Analog links (Erzwungen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Analog rechts (Erzwungen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "Taste %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Maus 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Maus 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Maus 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Maus 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Maus 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Mausrad hoch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Mausrad runter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Mausrad links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Mausrad rechts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Früh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Spät"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "JJJJ-MM-TT HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "JJJJ-MM-TT HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "JJJJ-MM-TT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "JJJJ-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "MM-TT-JJJJ HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "MM-TT-JJJJ HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "MM-TT HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "MM-TT-JJJJ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD,
   "MM-TT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "TT-MM-JJJJ HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "TT-MM-JJJJ HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "TT-MM HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "TT-MM-JJJJ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM,
   "TT-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "JJJJ-MM-TT HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "JJJJ-MM-TT HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "MM-TT-JJJJ HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "MM-TT-JJJJ HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "MM-TT HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "TT-MM-JJJJ HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "TT-MM-JJJJ HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "TT-MM HH:MM (AP/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "Vor"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Hintergrundfüllerdicke aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Grobheit des Schachbrettmusters im Menühintergrund erhöhen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Rahmenfüller aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Rahmenfüllerdicke aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Grobheit des Menürahmen-Schachbretts erhöhen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Menürahmen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Layout in voller Breite verwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Die Größe und Position der Menüeinträge ändern, um den verfügbaren Bildschirmbereich optimal zu nutzen. Deaktiviere diese Option, um das klassische zweispaltige Layout mit fester Breite zu verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Linearfilter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Fügt dem Menü eine leichte Unschärfe hinzu, um harte Pixelkanten zu glätten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Interne Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Menüoberfläche vor dem Darstellen auf dem Bildschirm hochskalieren. Bei Verwendung mit 'Linearer Filter für Menü' werden Skalierungsartefakte (ungleichmäßige Pixel) entfernt, während ein scharfes Bild erhalten bleibt. Hat einen erheblichen Einfluss auf die Leistung, der mit dem Skalierungslevel zunimmt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Bildseitenverhältnis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Menü-Seitenverhältnis wählen. Breitbildverhältnisse erhöhen die horizontale Auflösung der Menüoberfläche. (Möglicherweise ist ein Neustart erforderlich, wenn 'Menü-Seitenverhältnis sperren' deaktiviert ist.)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Seitenverhältnis sperren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Stellt sicher, dass das Menü immer mit dem richtigen Seitenverhältnis angezeigt wird. Wenn deaktiviert, wird das Schnellmenü auf den aktuell geladenen Inhalt gestreckt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Farbschema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Ein anderes Farbthema wählen. Wenn 'Benutzerdefiniert' ausgewählt wird, können voreingestellte Dateien für Menüthemen verwendet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Eigene Designvorlage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Im Dateibrowser ein voreingestelltes Menüthema auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Transparenz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Die Hintergrundanzeige des Inhalts aktivieren, wenn das Schnellmenü aktiv ist. Das Deaktivieren der Transparenz kann die Farben des Themas verändern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Schatteneffekte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Schlagschatten für Menütext, Rahmen und Vorschaubilder aktivieren. Hat eine mäßige Auswirkung auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Hintergrundanimation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Hintergrundpartikelanimation aktivieren. Hat erhebliche Auswirkungen auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Hintergrundanimationsgeschwindigkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Geschwindigkeit der Hintergrundpartikelanimation anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Bildschirmschoner-Hintergrund-Animation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Einen Hintergrundpartikel-Effekt anzeigen, während der Menü-Bildschirmschoner aktiv ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Wiedergabelisten-Vorschaubilder anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Aktiviert die Anzeige von herunterskalierten Vorschaubildern während dem Betrachten von Wiedergabelisten. Umschaltbar mit RetroPad Select. Wenn deaktiviert, können Vorschaubilder immer noch mit RetroPad Start zum Vollbild gewechselt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Oberes Vorschaubild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Art der Miniaturansicht, die oben rechts in der Wiedergabeliste angezeigt wird. Dieser Typ von Miniaturansicht kann durch Drücken von RetroPad Y gewechselt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Unteres Vorschaubild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Art des Vorschaubilds, das unten rechts in den Wiedergabelisten angezeigt werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Vorschaubilder tauschen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Vertauscht die Anzeigepositionen von 'Oberes Vorschaubild' und 'Unteres Vorschaubild'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Vorschaubilder-Downscaling-Methode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Resampling-Methode zum Verkleinern großer Vorschaubilder."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Vorschauverzögerung (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Verzögert das Laden der zugehörigen Vorschaubilder bei der Auswahl eines Wiedergabelisteneintrags. Ein Wert von mindestens 256 ms ermöglicht schnelles und verzögerungsfreies Scrollen auch auf den langsamsten Geräten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Erweiterte ASCII-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Die Anzeige von nicht standardmäßigen ASCII-Zeichen ermöglichen. Erforderlich für Kompatibilität mit bestimmten nicht englischen westlichen Sprachen. Hat einen moderaten Einfluss auf die Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Schaltersymbole"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Symbole anstelle von EIN/AUS-Text verwenden, um 'Umschalt'-Menüeinstellungen darzustellen."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Nächste-Nachbarn (schnell)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (langsam)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Kein"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (zentriert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (zentriert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (zentriert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (zentriert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (zentriert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "An Bildschirmgröße anpassen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Ganzzahlige Skalierung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "An Bildschirmgröße anpassen (gestreckt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Benutzerdefiniert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Klassisch Rot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Klassisch Orange"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Klassisch Gelb"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Klassisch Grün"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Klassisch Blau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Klassisches Violett"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Klassisch Grau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Legacy Rot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Dunkelviolett"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Mitternachtsblau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Gold"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektrisches Blau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Apfelgrün"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vulkanrot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Lagune"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "Zuckerwatte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Gruvbox hell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Den Kernel hacken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized hell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango hell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dynamisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Dunkelgrau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Hellgrau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Schnee (leicht)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Schnee (schwer)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Regen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Sternenfeld"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Sekundäres Vorschaubild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Art des Vorschaubildes, das auf der linken Seite gezeigt werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "Symbolminiaturbild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "Art des anzuzeigenden Miniaturbildes für die Wiedergabeliste."
   )
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
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Horizontale Animation für das Menü aktivieren. Verursacht Leistungseinbuße."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Animation der horizontalen Icon-Auswahl"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Die Animation, die beim Scrollen zwischen Tabs ausgelöst wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animation beim Auf-/Abbewegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Die Animation, die beim Auf- oder Abbewegen ausgelöst wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animation des Öffnens/Schließens von Menüs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animation, die beim Öffnen eines Untermenüs verwendet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Alphafaktor für Farbschema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Schriftart"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Eine andere Schriftart wählen, die im Menü verwendet werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Schriftfarbe (Rot)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Schriftfarbe (Grün)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Schriftfarbe (Blau)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Ein anderes Layout für die XMB-Schnittstelle wählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Symboldesign"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Wähle ein anderes Thema für das Menü aus. Änderungen werden übernommen, nachdem Du das Programm neu gestartet hast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Schaltersymbole"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "Symbole anstelle von EIN/AUS-Text verwenden, um „Umschalt“-Menüeinstellungen darzustellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Schatteneffekte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Schlagschatten für Symbole, Miniaturansichten und Buchstaben zeichnen. Dies führt zu einer geringfügigen Leistungseinbuße."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Shader-Pipeline"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Wähle einen animierten Effekt für das Hintergrundbild. Kann je nach Effekt GPU-lastig sein. Wenn die Leistung nicht ausreicht, wähle einen einfacheren Effekt oder deaktiviere diese Option."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Farbschema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Eine anderes Hintergrundfarbschema auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Vertikale Anordnung der Miniaturbilder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Das linke Vorschaubild unter dem rechten auf der rechten Seite des Bildschirms anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Vorschaubilder-Skalierungsfaktor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Größe der Vorschaubilder reduzieren, indem die maximal zulässige Breite skaliert wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Vertikaler Verblassungsfaktor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Kopfzeilentitel anzeigen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Randsaumbreite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Horizontaler Versatz der Randtitel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Registerkarte „Einstellungen“ aktivieren (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Tab mit Programmeinstellungen anzeigen."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Band"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Bänder (vereinfacht)"
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
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Schneeflocken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Benutzerdefiniert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Einfarbig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Einfarbig invertiert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Systematisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Automatisch invertiert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Apfelgrün"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Hell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Morgenblau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Sonnenstrahl"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Dunkelviolett"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektrisches Blau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Legacy Rot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Mitternachtsblau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Hintergrundbild"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vulkanrot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Limettengrün"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Pikachu Gelb"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Familien-Rot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Flammend heiß"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Eiskalt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Dunkelgrau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Hellgrau"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Seitenleiste einklappen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Die linke Seitenleiste immer einklappen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Wiedergabelistennamen kürzen (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Die Anbieternamen aus den Wiedergabelistennamen entfernen. Zum Beispiel wird \"Sony - PlayStation\" zu \"PlayStation\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Wiedergabelisten nach Namenskürzung sortieren (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Wiedergabelisten werden in alphabetischer Reihenfolge neu sortiert, nachdem die Herstellerkomponente ihrer Namen entfernt wurde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Sekundäres Vorschaubild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Das Inhaltsmetadatenfenster durch ein zusätzliches Vorschaubild ersetzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Lauftext für Inhaltsmetadaten verwenden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Wenn aktiviert, belegen alle Inhaltsmetadaten in der rechten Seitenleiste der Wiedergabelisten (zugeordneter Core, Spielzeit) eine einzelne Zeile. Text, der die Breite der Seitenleiste überschreitet, wird als Lauftext angezeigt. Wenn deaktiviert, werden alle Inhaltsmetadaten statisch angezeigt und so umbrochen, dass sie so viele Zeilen wie erforderlich belegen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Vorschaubilder-Skalierungsfaktor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Vorschaubilderleiste skalieren."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Farbschema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Ein anderes Farbthema wählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Basic Weiß"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Basic Schwarz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY,
   "Boysenbeere"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Den Kernel hacken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solarized dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized hell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Dunkelgrau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Hellgrau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN,
   "Violetter Regen"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Symbole"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Symbole links neben den Menüeinträgen anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Schaltersymbole"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "Symbole anstelle von EIN/AUS-Text verwenden, um „Umschalt“-Menüeinstellungen darzustellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Wiedergabelistensymbole (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Systemspezifische Symbole in den Wiedergabelisten anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Quer-Layout optimieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Das Menülayout automatisch an den Bildschirm anpassen, wenn die Querformatanzeige verwendet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Navigationsleiste anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Permanent Menünavigationsverknüpfungen anzeigen. Ermöglicht das schnelle Umschalten zwischen Menükategorien. Empfohlen für Touchscreen-Geräte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Navigationsleiste automatisch drehen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Die Navigationsleiste automatisch auf die rechte Seite des Bildschirms bewegen, wenn die Querformatanzeige verwendet wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Farbschema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Eine anderes Hintergrundfarbschema auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Übergangsanimationen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Animationseffekte beim Navigieren zwischen verschiedenen Ebenen des Menüs aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Hochformat Vorschaubildansicht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Den Vorschaubildansichtsmodus der Wiedergabelisten im Hochformat angeben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Querformat Vorschaubildansicht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Den Vorschaubildansichtsmodus der Wiedergabelisten im Querformat angeben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Sekundäre Vorschaubilder in Listenansicht zeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Zeigt ein sekundäres Vorschaubild an, wenn die Vorschaubildmodi vom Typ \"Liste\" verwendet werden. Diese Einstellung gilt nur, wenn der Bildschirm eine ausreichende physische Breite hat, um zwei Miniaturansichten anzuzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Vorschaubilder mit Hintergrund anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Ermöglicht das Auffüllen von nicht verwendetem Platz in Vorschaubildern mit einem einfarbigen Hintergrund. Dies gewährleistet eine einheitliche Anzeigegröße für alle Bilder und verbessert das Erscheinungsbild des Menüs beim Anzeigen von Vorschaubildern mit unterschiedlichen Basisabmessungen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Primäres Vorschaubild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Haupttyp der Vorschaubilder, die jedem Wiedergabelisteneintrag zugeordnet werden. Dienen normalerweise als Inhaltssymbole."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Sekundäres Vorschaubild"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Zusätzliche Art der Vorschaubilder, die jedem Wiedergabelisteneintrag zugeordnet werden. Die Verwendung hängt vom aktuellen Vorschaubildansichtsmodus der Wiedergabeliste ab."
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
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Material UI dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozon dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Cutie Blau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Cutie Grün"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Cutie Violett"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Cutie Rot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Den Kernel hacken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Dunkelgrau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Hellgrau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Ausblenden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Gleiten"
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
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Liste (klein)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Liste (mittel)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Doppel-Symbol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Liste (klein)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Liste (mittel)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Liste (groß)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "AUS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "EIN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Miniaturansichten ausschließen"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Datei"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "Core &laden..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "Core dea&ktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "B&earbeiten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Suchen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Ansicht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Geschlossene Docks"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Vorschau der Shader-Parameter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "Ein&stellungen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Dockpositionen merken:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Fenstergeometrie merken:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Letzten Inhaltsbrowser-Tab merken:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Thema:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Systemstandard>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Dunkel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Benutzerdefiniert..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Hilfe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "Über RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Dokumentation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Benutzerdefinierten Core laden..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Core laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Core wird geladen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Dateibrowser"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Anfang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Hoch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Inhaltsbrowser"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Titelbildschirm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Alle Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Core-Informationen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Frag mich>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informationen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Warnung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Fehler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Netzwerkfehler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Bitte das Programm neu starten, um die Änderungen anzuwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Protokoll"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 Einträge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Bild hier ablegen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Nicht wieder anzeigen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Stopp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Core zuordnen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Versteckte Wiedergabelisten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Ausblenden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Hervorhebungsfarbe:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "Aus&wählen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Farbe wählen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Thema auswählen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Benutzerdefiniertes Design"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Dateipfad ist leer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Datei ist leer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Datei konnte nicht zum Lesen geöffnet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Datei konnte nicht zum Schreiben geöffnet werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Datei existiert nicht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Geladenen Core zuerst vorschlagen:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Ansicht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Symbole"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Liste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Leeren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Fortschritt:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "Maximale Anzahl Listeneinträge in \"Alle Wiedergabelisten\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "Maximale Anzahl Rastereinträge \"Alle Wiedergabelisten\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Versteckte Dateien und Ordner anzeigen:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Neue Wiedergabeliste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Bitte Namen für neue Wiedergabeliste eingeben:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Wiedergabeliste löschen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Wiedergabeliste umbenennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Möchtest Du Wiedergabeliste \"%1\" wirklich löschen?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Frage"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Datei konnte nicht gelöscht werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Datei konnte nicht umbenannt werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Liste von Dateien wird gesammelt..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Dateien werden zur Wiedergabeliste hinzugefügt..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Wiedergabelisteneintrag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Pfad:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Inhaltsdatenbank:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Erweiterungen:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(durch Leerzeichen getrennt; schließt standardmäßig alle ein)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Archiv-Inhalte filtern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(verwendet, um Vorschaubilder zu finden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Möchtest Du Artikel \"%1\" wirklich löschen?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Bitte wähle zuerst eine einzelne Wiedergabeliste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Löschen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Eintrag Hinzufügen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Datei(en) hinzufügen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Ordner hinzufügen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Bearbeiten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Dateien auswählen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Ordner auswählen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<mehrere>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Fehler beim Aktualisieren des Wiedergabelisteneintrags."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Bitte alle erforderlichen Felder ausfüllen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "RetroArch aktualisieren (Nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch erfolgreich aktualisiert. Bitte die Anwendung neu starten, damit die Änderungen wirksam werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Aktualisierung fehlgeschlagen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Mitwirkende"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Aktueller Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Nach unten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Nach oben"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Speichern"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Entfernen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Durchläufe entfernen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Übernehmen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Durchlauf hinzufügen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Alle Durchläufe entfernen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Keine Shader-Durchläufe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Durchlauf zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Alle Durchläufe zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Parameter zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Vorschaubild herunterladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Ein Download läuft bereits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Auf Wiedergabeliste starten:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Vorschaubild"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Maximaler Vorschaubilder-Cache:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Maximale Größe von Drag-n-Drop-Vorschaubildern:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Alle Vorschaubilder herunterladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Gesamtes System"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Diese Wiedergabeliste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Vorschaubilder erfolgreich heruntergeladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Erfolgreich: %1 Fehlgeschlagen: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Core-Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Zurücksetzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Alle zurücksetzen"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Core-Updater-Einstellungen"
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
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Turbo/Totzone"
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
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Keine Disc ausgewählt"
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
   "Bildschirm-Overlay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Verlauf"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Inhalte aus der Verlaufsliste auswählen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Wenn Inhalte geladen werden, wird die Inhalt-Core-Kombination im Verlauf gespeichert.\nDer Verlauf wird im selben Verzeichnis wie die RetroArch-Konfigurationsdatei gespeichert. Wurde beim Start keine Konfigurationsdatei geladen, wird der Verlauf nicht gespeichert oder geladen und ist auch nicht im Hauptmenü vorhanden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Subsysteme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Zugriff auf die Subsystemeinstellungen für aktuelle Inhalte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Aktueller Inhalt: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Keine Netzwerkspiele gefunden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Keine Netzwerkspiel-Clients gefunden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Keine Leistungsindikatoren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Keine Wiedergabelisten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Verbunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Port %d Gerätename: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Geräte-Anzeigename: %s\nGeräte-Konfigurationsname: %s\nGeräte-VID/PID: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Cheat-Einstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Cheatsuche starten oder fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Mit Mediaplayer abspielen"
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
   MENU_ENUM_SUBLABEL_START_CORE,
   "Core ohne Inhalt starten."
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
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "Tastatur"
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
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Maximale Zahl von Zwischenbildern. Diese Einstellung kann dem Videotreiber vorschreiben, einen bestimmten Videopuffermodus zu verwenden.\nEinfache Pufferung – 1\nDoppelte Pufferung – 2\nDreifache Pufferung – 3\nDie Auswahl des richtigen Puffermodus kann einen großen Einfluss auf die Leistung haben."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "CPU und GPU fest synchronisieren. Reduziert Latenz auf Kosten der Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Maximale Frame-Latenzzeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Zwingt den Videotreiber dazu, einen bestimmten Framebuffer zu verwenden."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Modifiziert die Shader-Voreinstellung, die momentan im Menü benutzt wird."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Shader-Voreinstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Shader-Voreinstellungen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
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
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Spitzname: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Nach kompatiblen Inhalten wird gesucht …"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "Kein Core gefunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "Keine Playlist gefunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Kompatibler Inhalt gefunden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "Fehler beim Auffinden passender Inhalte mittels CRC oder Dateiname"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Gong starten"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "Automatisches Bildseitenverhältnis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "Spitzname (LAN): %s"
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
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "Betrachte Treffer #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "Treffer auswählen, der angezeigt werden soll."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "Seitenverhältnis erzwingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "Wähle aus Wiedergabenliste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "Liste von %u Treffern ansehen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "Erstelle Code zu diesem Treffer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "Diesen Treffer löschen"
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
   "Netzwerkspiel"
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
   "Ausgabebildschirm-ID"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Den Ausgabeport auswählen, welcher mit dem CRT-Bildschirm verbunden ist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Hilfe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "Leeren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Audio/Video-Fehlerbehebung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "Virtuelles Controller-Overlay wird geändert"
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
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "Debugging-Informationen senden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
   "Sendet Diagnoseinformationen über das Gerät und die RetroArch-Konfiguration zur Analyse an unsere Server."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Datenbanken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Netzwerkspiel-Verzögerung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Lokales Netzwerk durchsuchen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Netzwerkspiele im lokalen Netzwerk suchen und zu ihnen verbinden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Netzwerkspiel-Client"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Netzwerkspiel-Zuschauer"
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
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "Suche nach neuem Cheat-Code starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "Nach einem neuen Cheat suchen. Die Anzahl der Bits kann geändert werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Suche fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Suche weiter nach einem neuen Cheat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Errungenschaften (Hardcore)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Cheat-Details"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Verwaltet die Einstellungen für Cheat-Details."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Cheatsuche starten oder fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Starte oder setze eine Cheat-Code-Suche fort."
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
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Cheatsuche starten oder fortsetzen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Datenbank-Suchanfragen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Datenbank - Filter: Entwickler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Datenbank - Filter: Publisher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Datenbank - Filter: Herkunft"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Datenbank - Filter: Franchise"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Datenbank - Filter: ESRB-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Datenbank - Filter: ELSPA-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Datenbank - Filter: PEGI-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Datenbank - Filter: CERO-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Datenbank - Filter: BBFC-Bewertung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Datenbank - Filter: Max. Spieler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Datenbank - Filter: Veröffentlichungsdatum nach Monat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Datenbank - Filter: Veröffentlichungsdatum nach Jahr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Datenbank - Filter: Edge-Zeitschriftenausgabe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Datenbank - Filter: Edge-Bewertung"
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
   "Netzwerkspiel-Einstellungen"
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
   "Im Spiel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Pausiert"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netzwerkspiel wird gestartet, sobald der Inhalt geladen ist."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "Der Inhalt muss geladen werden, bevor das Netzwerkspiel gestartet wird."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Konnte keinen geeigneten Core oder Inhalt finden, lade manuell."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Der System-Grafiktreiber ist nicht mit dem aktuellen Grafiktreiber in RetroArch kompatibel. Greife auf den %s Treiber zurück. Bitte RetroArch neu starten, damit die Änderungen wirksam werden."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Core Installation erfolgreich"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Core Installation fehlgeschlagen"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Drücke fünf Mal rechts, um alle Cheats zu löschen."
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "Speichern der Debugging-Informationen fehlgeschlagen."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_DEBUG_INFO,
   "Senden der Debugging-Informationen an den Server fehlgeschlagen."
   )
MSG_HASH(
   MSG_SENDING_DEBUG_INFO,
   "Debugging-Informationen werden gesendet..."
   )
MSG_HASH(
   MSG_SENT_DEBUG_INFO,
   "Debugging-Informationen erfolgreich an den Server gesendet. Deine ID-Nummer ist %u."
   )
MSG_HASH(
   MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
   "Drücke noch zweimal, um Diagnoseinformationen an das RetroArch-Team zu senden."
   )
MSG_HASH(
   MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
   "Drücke noch einmal, um Diagnoseinformationen an das RetroArch-Team zu senden."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Globale Lautstärke des Audio-Mixers"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Netzwerkspiel-Suche abgeschlossen."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Entschuldigung, nicht implementiert: Cores, die keinen Inhalt verlangen, können nicht an Netzwerkspielen teilnehmen."
   )
MSG_HASH(
   MSG_NATIVE,
   "Nativ"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Unbekannten Netzwerkspiel-Befehl empfangen"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Datei existiert bereits. Speichere im Backup-Puffer"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "Verbindung hergestellt mit: \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Verbindung hergestellt mit: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Netzwerkspiel-Port-Zuordnung erfolgreich"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "Externes Netzwerk hat eine private oder gemeinsame Adresse. Erwäge die Verwendung eines Relay-Servers."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Netplay UPnP-Port-Zuordnung fehlgeschlagen"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "Keine Argumente angegeben und kein Menü integriert - zeige Hilfe an..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Lege Disc ins Laufwerk ein"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Auf Client warten ..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "Auf Deinen Raum kann nicht über das Internet zugegriffen werden."
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
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "Du bist mit Eigabegeräten %.*s beigetreten"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "Spieler %.*s hat das Spiel verlassen"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s ist als Spieler %u beigetreten"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s ist mit Eingabegeräten %.*s beigetreten"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Ein Netzwerkspiel-Verbindungsversuch ist fehlgeschlagen, da beim Teilnehmer RetroArch nicht oder in einer älteren Version ausgeführt wird."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Ein Netplay-Teilnehmer nutzt eine alte RetroArch-Version. Verbindung nicht möglich."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "WARNUNG: Ein Netzwerkspiel-Teilnehmer nutzt eine andere Version von RetroArch. Falls Probleme auftreten, benutzt dieselbe Version."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Ein Netzwerkspiel-Teilnehmer nutzt einen anderen Core. Kann nicht verbinden."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "WARNUNG: Ein Netzwerkspiel-Teilnehmer nutzt eine andere Version vom Core. Falls Probleme auftreten, benutzt dieselbe Version."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Dieser Core unterstützt kein Netzwerkspiel zwischen diesen Plattformen"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Dieser Core unterstützt kein Netzwerkspiel zwischen verschiedenen Plattformen"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Gib das Server-Passwort ein:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Netzwerkspiel-Chatnachricht eingeben:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Möchtest Du eine Verbindung zulassen von:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Falsches Passwort"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" hat die Verbindung getrennt"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Ein Netzwerkspiel-Client hat die Verbindung getrennt"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Verbindung mit Netzwerkspiel getrennt"
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
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "Die angeforderten Eingabegeräte sind nicht verfügbar"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Konnte nicht zum Spieler-Modus wechseln"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Netzwerkspiel-Teilnehmer \"%s\" hat pausiert"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Dein Spitzname wurde geändert zu \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "Client rausgeschmissen: „%s“"
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "Client konnte nicht rausgeschmissen werden: „%s“"
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "Client gesperrt: „%s“"
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "Fehler beim Sperren des Clients: „%s“"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Im Spiel"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "Zuschauer"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "Geräte"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "Chat unterstützt"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "Verursacht Verlangsamung"
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
   MSG_CAPABILITIES,
   "Fähigkeiten"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Verbinde zu Netzwerkspiel"
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
   MSG_FETCHING_CORE_LIST,
   "Core-Liste wird abgerufen..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Abrufen der Core-Liste fehlgeschlagen!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "Neueste Version bereits installiert: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Aktualisiere Core: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Core wird heruntergeladen: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Entpacke Core: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Core installiert: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Installieren des Cores fehlgeschlagen: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Scanne Cores..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Überprüfe Core: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Alle installierten Cores sind aktuell"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Alle unterstützten Cores wurden auf Play Store Versionen umgestellt"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "Cores aktualisiert: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "Cores übersprungen: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Core Update deaktiviert - Core gesperrt: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Setze Cores zurück: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Cores zurückgesetzt: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Reinige Wiedergabeliste: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Wiedergabeliste bereinigt: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Aktualisieren fehlgeschlagen - Wiedergabeliste enthält keinen gültigen Scan-Datensatz: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Aktualisieren fehlgeschlagen - ungültiges/fehlendes Inhaltsverzeichnis: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Aktualisieren fehlgeschlagen - ungültiger/fehlender Systemname: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Aktualisieren fehlgeschlagen - ungültiger Core: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Aktualisieren fehlgeschlagen - ungültige/fehlende Arcade-DAT-Datei: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Aktualisieren fehlgeschlagen - Arcade-DAT-Datei zu groß (zu wenig Speicher): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Zu Favoriten hinzugefügt"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Favorit kann nicht hinzugefügt werden: Wiedergabeliste voll"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "Zur Wiedergabeliste hinzugefügt"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Fehler beim Hinzufügen zur Playlist: Playlist ist voll"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Core zugeordnet: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "Core-Zuordnung für den Wiedergabelisteneintrag wurde zurückgesetzt."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Angefügte Disc"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Anhängen der Disc fehlgeschlagen"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Anwendungen"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Änderungen an Cheats werden übernommen."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Patch angewandt: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Shader anwenden"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Audio aus."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Audio an."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Fehler beim Speichern des Controller-Profils."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Controller-Profil erfolgreich gespeichert."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "Controller-Profil im Controller-Profilverzeichnis als\n„%s“ gespeichert"
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
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "Der Host führt ein anderes Spiel aus."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "Dein Ping ist zu hoch für diesen Host."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Laden des Inhalts übersprungen. Die Implementierung wird ihn selbst laden."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Core unterstützt keine Speicherabbilder."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Core-Einstellungsdatei wurde erfolgreich erstellt."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "Core-Einstellungsdatei erfolgreich entfernt."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Alle Core-Einstellungen auf Standard zurückgesetzt."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "Core-Optionen gespeichert in:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "Fehler beim Speichern der Core-Optionen in:"
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
   "Datentrack konnte nicht geöffnet werden"
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
   "Gerät von einem gültigen Port trennen."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Virtuelles Laufwerk geschlossen."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Virtuelles Laufwerk ausgeworfen."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Wird heruntergeladen"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "Index"
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
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Core unterstützt kein VFS und das Laden von einer lokalen Kopie ist fehlgeschlagen"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Fehler beim Verarbeiten der Argumente."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Fehler beim Speichern der Core-Einstellungsdatei."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "Fehler beim Löschen der Core-Einstellungsdatei."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Fehler beim Speichern der Remap-Datei."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Fehler beim Löschen der Remap-Datei."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Fehler beim Speichern der Shader-Voreinstellung."
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
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Ankommender Beobachter konnte nicht akzeptiert werden."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Zuweisung des Arbeitsspeichers für den gepatchten Inhalt fehlgeschlagen..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Fehler beim Anwenden des Shaders."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Shader-Voreinstellung konnte nicht angewendet werden:"
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
   MSG_OSK_OVERLAY_NOT_SET,
   "Tastatur-Overlay ist nicht eingestellt."
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
   "Client-Header konnte nicht empfangen werden."
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
   "Fehler beim Auswerfen der Disc."
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
   MSG_FAILED_TO_LOAD_SRAM,
   "Fehler beim Laden des SRAM"
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
   "Aufzeichnung konnte nicht starten."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Konnte Screenshot nicht erstellen."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Laden des Savestates konnte nicht rückgängig gemacht werden."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Speichern des Savestates konnte nicht rückgängig gemacht werden."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Audio konnte nicht eingeschaltet werden."
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
   "Disc-Label gefunden"
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
   MSG_FOUND_LAST_REPLAY_SLOT,
   "Letzten Replayslot gefunden"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "Nicht von aktueller Aufnahme"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "Nicht kompatibel mit Replay"
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
   "Spielspezifische Core-Optionen gefunden in"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Ordnerspezifische Core-Optionen gefunden in"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Ungültiger Disc-Index."
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
   MSG_INPUT_OVERRIDE_FILENAME,
   "Dateiname für Eingabeüberschreibungen"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "Dateiname für Eingabezuordnungen"
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
   "ist für eine andere Libretro-Version als diese kompiliert worden."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Frontend für Libretro"
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
   "Wird geladen"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Eine oder mehrere Firmware-Dateien fehlen"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Inhalts-Datei wird geladen"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Verlaufs-Datei wird geladen"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Favoritendatei wird geladen"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Savestate wird geladen"
   )
MSG_HASH(
   MSG_MEMORY,
   "Speicher"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "Eingegebene Replay-Filmdatei ist keine gültige REPLAY-Datei."
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
   "Netzwerkspiel-Initialisierung fehlgeschlagen."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "Core unterstützt kein Netzwerkspiel."
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
   MSG_OVERRIDES_ERROR_REMOVING,
   "Fehler beim Entfernen der Überschreibungen."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Überschreibungen erfolgreich gespeichert."
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "Überschreibungen erfolgreich entfernt."
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "Überschreibungen erfolgreich zurückgesetzt."
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "Nichts zu speichern. Überschreibungen nicht gespeichert."
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "Wird nicht gespeichert. Überschreibungen sind aktiv."
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
   MSG_REMAP_FILE_RESET,
   "Alle Tastenzuordnungsoptionen auf Standardwerte zurückgesetzt."
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
   MSG_REWIND_UNSUPPORTED,
   "Zurückspringen nicht verfügbar, da dieser Core keine serialisierte Savestate-Unterstützung bietet."
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
   "Scanne"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Verzeichnisscan abgeschlossen."
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
   MSG_SLOW_MOTION,
   "Zeitlupe."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Vorspulen."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "In Zeitlupe zurückspulen."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Überspringe Laden des SRAM."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM wird nicht gespeichert."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Überschreiben des SRAM blockieren"
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
   MSG_REPLAY_SLOT,
   "Replayslot"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Erstelle Screenshot."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Screenshot gespeichert"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Errungenschaft freigeschaltet"
   )
MSG_HASH(
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "Seltene Errungenschaft freigeschaltet"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "Ranglistenversuch gestartet"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "Ranglistenversuch fehlgeschlagen"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "%s für %s eingereicht" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "Rang: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "Beste(r): %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Vorschaubildart ändern"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Vollbild-Vorschaubilder"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Metadaten umschalten"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Kein Vorschaubild verfügbar"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "Alle möglichen Thumbnail-Downloads wurden bereits für diesen Playlist-Eintrag versucht."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Zum Beenden erneut drücken..."
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
   "Speichern des Savestates wird rückgängig gemacht"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Unbekannt"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Fortgesetzt."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Unbekannten Befehl \"%s\" erhalten.\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Verwende Core-Namen für neue Konfiguration."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Libretro-Dummy-Core wird verwendet. Aufzeichnung übersprungen."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Verbinde Gerät mit einem gültigen Port."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Trenne Gerät von Port"
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
   "Version der Libretro-API"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Berechnung der Bildgröße fehlgeschlagen! Wird unter Verwendung von Rohdaten fortfahren. Dies wird wahrscheinlich nicht richtig funktionieren ..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Virtuelles Laufwerk konnte nicht ausgeworfen werden."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Virtuelles Laufwerk konnte nicht geschlossen werden."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Automatisches Laden des Speicherzustands aus"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "Automatisches Laden des Savestates aus \"%s\" fehlgeschlagen."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "Automatisches Laden des Savestates aus \"%s\" erfolgreich."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "in Port konfiguriert"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s konfiguriert in Port %u"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "von Port getrennt"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s wurde von Port %u getrennt"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "nicht konfiguriert"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) nicht konfiguriert"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "nicht konfiguriert, verwende Rückfalloption"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) nicht konfiguriert, verwendet Rückfalloption"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Bluetooth-Scan abgeschlossen."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "Kopplung entfernt. RetroArch neu starten, um erneut zu verbinden/koppeln."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Suche nach WLAN-Netzwerken abgeschlossen."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Suche nach Bluetooth-Geräten..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Suche nach WLAN-Netzwerken..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Aktiviere WLAN..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Deaktiviere WLAN..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Trenne WLAN..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Suche nach Netzwerkspielen..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Bereite Inhaltsscan vor..."
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
   MSG_CONFIG_OVERRIDE_LOADED,
   "Konfigurationsüberschreibung geladen."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Spiel-Remap-Datei geladen."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Inhaltsverzeichnis-Remap-Datei geladen."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Core Remap-Datei geladen."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "Eingabe-Remap-Optionen gespeichert in:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "Fehler beim Speichern der Eingabe-Remap-Optionen in:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Run-Ahead aktiviert. Latenz-Frames entfernt: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Run-Ahead aktiviert mit Sekundärinstanz. Latenz-Frames entfernt: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Run-Ahead deaktiviert."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Run-Ahead wurde deaktiviert, da dieser Core keine Savestates unterstützt."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "Run-Ahead nicht verfügbar, da dieser Core keine deterministische Savestate-Unterstützung bietet."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Erstellen des Savestates fehlgeschlagen. Run-Ahead wurde deaktiviert."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Laden des Savestates fehlgeschlagen. Run-Ahead wurde deaktiviert."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Fehler beim Erstellen der zweiten Instanz. Run-Ahead wird nun nur noch eine Instanz verwenden."
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "Präemptive Frames aktiviert. Latenzframes entfernt: %u."
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "Präemptive Frames deaktiviert."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Präemptive Frames wurden deaktiviert, da dieser Core keine Savestates unterstützt."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "Präemptive Frames sind nicht verfügbar, da dieser Core keine deterministische Savestate-Unterstüzung bietet."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "Speicher für präemptive Frames konnte nicht zugewiesen werden."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "Erstellen des Savestates fehlgeschlagen. Präemptive Frames wurden deaktiviert."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "Laden des Savestates fehlgeschlagen. Präemptive Frames wurden deaktiviert."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Dateiscan abgeschlossen."
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Cheatsuche erfolgreich gestartet."
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Fehler beim Starten der Cheatsuche."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Suche wurde nicht initialisiert/gestartet."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Neue Trefferzahl = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "%u Treffer hinzugefügt."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Fehler beim Hinzufügen von Treffern."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Code von Treffer erstellt."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Fehler beim Erstellen des Codes."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Treffer gelöscht."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Nicht genügend Platz. Die maximale Anzahl an gleichzeitigen Cheats ist 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Neuer Cheat am Anfang der Liste hinzugefügt."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Neuer Cheat am Ende der Liste hinzugefügt."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Alle Cheats gelöscht."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Neuer Cheat vor diesem hinzugefügt."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Neuer Cheat nach diesem hinzugefügt."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Cheat vor diesem kopiert."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Cheat nach diesem kopiert."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Cheat gelöscht."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Fehler beim Einstellen der Disc."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Fehler beim Einsetzen der zuletzt verwendeten Disc."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "Verbindung zum Client fehlgeschlagen."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Verbindung zum Host fehlgeschlagen."
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "Netplay-Host voll."
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "Du wurdest von diesem Host verbannt."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Fehler beim Empfang des Headers vom Host."
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "RetroAchievements: Angemeldet als „%s“."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Du musst den Errungenschaften-Hardcore-Modus pausieren oder deaktivieren um Savestates laden zu können."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Ein Savestate wurde geladen. Errungenschaften-Hardcore-Modus wurde für die aktuelle Sitzung deaktiviert."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Ein Cheat wurde aktiviert. Errungenschaften-Hardcore-Modus wurde für die aktuelle Sitzung deaktiviert."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST,
   "Hardcore-Modus für Errungenschaften von Host geändert."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST,
   "Netplay-Host muss aktualisiert werden. Hardcore-Modus für Errungenschaften wurde für aktuelle Sitzung deaktiviert."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "%s gemeistert"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "%s vervollständigt"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Errungenschaften-Hardcore-Modus aktiviert. Savestates & Rückspul-Funktion sind nicht nutzbar."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "Dieses Spiel hat keine Errungenschaften."
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "Alle %d Errungenschaften für diese Sitzung aktiviert"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "%d inoffizielle Errungenschaften aktiviert"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "Es sind %d von %d Errungenschaften freigeschaltet"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "%d nicht unterstützt"
)
MSG_HASH(
   MSG_CHEEVOS_RICH_PRESENCE_SPECTATING,
   "Zuschauer %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "Hardcore pausiert. Manuelle Einstellung der Videobildverzögerung nicht gestattet."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL,
   "Hardcore pausiert. Vsync-Swap-Intervall über 1 nicht gestattet."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION,
   "Hardcore pausiert. Das Einfügen schwarzer Rahmen ist nicht gestattet."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED,
   "Hardcore pausiert. Einstellung nicht gestattet: %s=%s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE,
   "Hardcore pausiert. Es lassen sich keine Hardcore-Errungenschaften für %s mit %s verdienen"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "RetroAchievements: Spiel konnte nicht identifiziert werden."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "RetroAchievements Spielladen fehlgeschlagen: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "RetroAchievements Medienwechsel fehlgeschlagen: %s"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "Niedrigste"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Niedriger"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Höher"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Höchste"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Warnung: Fehlende Assets; bitte den Online-Updater verwenden, falls verfügbar."
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Warnung: Fehlende Schriftarten für die ausgewählte Sprache; bitte den Online-Updater verwenden, falls verfügbar."
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Warnung: Nicht unterstützte Sprache – Englisch wird verwendet."
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Dumpe Disc..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Laufwerk %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Bitte zuerst einen Core laden."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Fehler beim Lesen vom Laufwerk. Dump abgebrochen."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Fehler beim Schreiben. Dump abgebrochen."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Es ist keine Disc in das Laufwerk eingelegt."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Shader-Voreinstellung erfolgreich entfernt."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Fehler beim Löschen der Shader-Voreinstellung."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Ungültige Arcade-DAT-Datei ausgewählt."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "Ausgewählte Arcade-DAT-Datei ist zu groß (unzureichender freier Speicher)."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Fehler beim Laden der Arcade-DAT-Datei (ungültiges Format?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Ungültige manuelle Scankonfiguration."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Keine gültigen Inhalte gefunden."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Scanne Inhalt: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Aktuelle Einträge werden geprüft: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Scanne: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Säubere M3U-Einträge: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Scan abgeschlossen: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Scanne Core: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Sicherung des installierten Cores existiert bereits: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Sichere Core: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Entferne veraltete Backups: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Core-Backup erstellt: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "Ausgewählte Core-Sicherung ist bereits installiert: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Wiederherstellen des Cores: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Core-Wiederherstellung abgeschlossen: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "Ausgewählte Core-Datei ist bereits installiert: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Installiere Core: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Core Installation abgeschlossen: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "Ungültige Core-Datei ausgewählt: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Core-Backup fehlgeschlagen: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Core-Wiederherstellung fehlgeschlagen: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Core-Installation fehlgeschlagen: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Core-Wiederherstellung deaktiviert - Core ist gesperrt: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "Core-Installation deaktiviert - Core gesperrt: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Fehler beim Sperren des Cores: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Fehler beim Entsperren des Cores: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "Fehler beim Entfernen des Cores aus der Liste „Inhaltslose Cores“: "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "Fehler beim Hinzufügen des Cores zur Liste „Inhaltslose Cores“: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "Löschen des Cores deaktiviert - Core gesperrt: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Nicht unterstützter Video-Modus"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "Kann nicht in das Core-Informationen-Verzeichnis schreiben - Core-Informationen-Cache wird deaktiviert"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "Eintragsstatus gefunden in"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Eintragsstatus wird geladen aus"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "GameMode kann nicht aufgerufen werden"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "GameMode kann nicht aufgerufen werden - stelle sicher, dass der GameMode-Daemon installiert ist/läuft"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "Synchronisation mit exakter Inhaltssignalfrequenz aktiviert."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "Synchronisation mit exakter Inhaltssignalfrequenz deaktiviert."
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "Videoaktualisierungsrate auf %s Hz geändert."
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
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Joy-Con teilen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Skalierungsüberschreibung für Grafik-Widgets"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Beim Zeichnen von Anzeige-Widgets eine manuelle Überschreibung des Skalierungsfaktors anwenden. Gilt nur, wenn \"Grafik-Widgets automatisch skalieren\" deaktiviert ist. Kann verwendet werden, um die Größe dekorierter Benachrichtigungen, Anzeigen und Steuerelemente unabhängig vom Menü selbst zu erhöhen oder zu verringern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Bildschirmauflösung"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Bildschirmauflösung: Standard"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Bildschirmauflösung: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Bildschirmauflösung: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Angewendet: Standard"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Angewendet: %dx%d\nSTART zum Zurücksetzen"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Angewendet: %dx%d - %s\nSTART zum Zurücksetzen"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Wiederhergestellt: Standard"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Wiederhergestellt: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Wiederhergestellt: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Anzeigemodus auswählen (Neustart erforderlich)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Beenden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Externen Dateizugriff ermöglichen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Die Einstellungen für die Windows-Dateizugriffsberechtigungen öffnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Die Windows-Berechtigungseinstellungen öffnen, um die Option broadFileSystemAccess zu aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Öffnen..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Ein anderes Verzeichnis mit der Systemdateiauswahl öffnen"
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
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Nach Bluetooth-Geräten suchen und mit ihnen verbinden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
   "WLAN"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Nach drahtlosen Netzwerken suchen und mit ihnen verbinden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "WLAN aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Mit Netzwerk verbinden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Mit Netzwerk verbinden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Verbindung trennen"
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
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Overscan-Korrektur (oben)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Das Zuschneiden des Overscans anpassen, indem die Bildgröße um die angegebene Anzahl von Scanlinien (vom oberen Bildschirmrand) reduziert wird. Kann Skalierungsartefakte verursachen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Overscan-Korrektur (unten)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Das Zuschneiden des Overscans anpassen, indem die Bildgröße um die angegebene Anzahl von Scanlinien (vom unteren Bildschirmrand) reduziert wird. Kann Skalierungsartefakte verursachen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Dauerleistungsmodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "CPU-Leistung und -Verbrauch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "Richtlinie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Reglermodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Manuell"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Ermöglicht das manuelle Anpassen aller Details in jeder CPU: Regler, Frequenzen usw. Nur für fortgeschrittene Benutzer empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Leistung (verwaltet)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Standard und empfohlener Modus. Maximale Leistung beim Abspielen, während Strom gespart wird, wenn die Wiedergabe pausiert oder die Menüs durchsucht werden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Individuell verwaltet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Ermöglicht die Auswahl der Regler, die in Menüs und während des Spiels verwendet werden sollen. Performance, Ondemand oder Schedutil werden während des Spiels empfohlen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Maximale Leistung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Immer maximale Leistung: höchste Frequenzen für bestes Erlebnis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Minimaler Verbrauch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Die niedrigste verfügbare Frequenz verwenden, um Strom zu sparen. Nützlich bei batteriebetriebenen Geräten, aber die Leistung wird deutlich reduziert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Ausgewogen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Passt sich der aktuellen Auslastung an. Funktioniert mit den meisten Geräten und Emulatoren gut und hilft, Strom zu sparen. Anspruchsvolle Spiele und Cores können auf manchen Geräten einen Leistungsabfall erleiden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Minimale Frequenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Maximale Frequenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Minimale Kern-Frequenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Maximale Kern-Frequenz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "CPU-Regler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Kern-Regler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Menü-Regler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "Spielmodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Kann die Leistung verbessern, die Latenzzeit verringern und Audio-Crackling beheben. Die Komponente https://github.com/FeralInteractive/gamemode wird hierzu benötigt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Das Aktivieren von Linux GameMode kann die Latenzzeit verbessern, Audioknackser beheben und die Gesamtleistung maximieren, indem CPU und GPU automatisch für die beste Leistung konfiguriert werden.\nDie GameMode-Software muss installiert sein, damit dies funktioniert. Siehe https://github.com/FeralInteractive/gamemode für Informationen zur Installation von GameMode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Verwende PAL60-Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "RetroArch neu starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "RetroArch beenden und neu starten. Erforderlich, um bestimmte Menüeinstellungen zu aktivieren (z. B. beim Ändern des Menütreibers)."
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
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "Den vorderen statt den hinteren Touchscreen nutzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Touch-Eingabe aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Tastatur-Controller-Zuordnung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Tastatur-Controller-Zuordnungstyp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Kleine Tastatur aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Eingabe-Block-Timeout"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Anzahl an Millisekunden, die gewartet werden, bevor eine vollständige Eingabe getätigt wird. Verwende diese Option, wenn Probleme beim gleichzeitigen Drücken von Tasten auftreten (nur Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "\"Neustart\" anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Die Option \"Neustart\" anzeigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Zeige 'Beenden'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Zeige die Option 'Beenden'."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Passwortgeschützt"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "Internet (nicht verbindbar)"
   )
MSG_HASH(
   MSG_LOCAL,
   "Lokal"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Interner Speicherstatus: Lesen/Schreiben"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Interner Speicherstatus: Schreibgeschützt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Bildschirmhelligkeit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Bildschirmhelligkeit erhöhen oder verringern."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "CPU Übertakten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Übertakten der Switch CPU."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Aktiviere oder deaktiviere Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Dienste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Betriebssystem-Dienste verwalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "SAMBA aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Freigegebene Ordner per SMB-Protokoll teilen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "SSH aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Aktiviere Fernzugriff auf die Kommandozeile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Wi-Fi-Zugangspunkt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Wi-Fi Access Point aktivieren oder deaktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Zeitzone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Zeitzone wählen, um Datum und Zeit an Ihren Standort anzupassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "Zeigt eine Liste der verfügbaren Zeitzonen an. Nach Auswahl einer Zeitzone werden Zeit und Datum an die gewählte Zeitzone angepasst. Es wird davon ausgegangen, dass die System-/Hardware-Uhr auf UTC eingestellt ist."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Optionen für Nintendo Switch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Nintendo Switch spezifische Optionen verwalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "CPU-Übertaktung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "CPU-Übertaktungsfrequenzen aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "CEC-Unterstützung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "CEC-Handshake mit TV beim Docken aktivieren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "Bluetooth ERTM deaktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "Bluetooth ERTM deaktivieren, um das Koppeln einiger Geräte zu verbessern"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Wi-Fi Access Point wird ausgeschaltet."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Trenne von Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Verbinde mit WLAN '%s'"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Keine SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Wi-Fi Access Point ist bereits gestartet"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Wi-Fi Access Point wird nicht ausgeführt"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Starte Wi-Fi Access Point mit SSID=%s und Passwort=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Konnte die Wi-Fi Access Point Konfigurationsdatei nicht erstellen."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Falsche Konfigurationsdatei - APNAME oder PASSWORD in %s nicht gefunden"
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Maus-Skala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "X/Y-Skala für Wiimote Lichtpistolengeschwindigkeit anpassen."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Touchscreen-Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "Die x/y-Skalierung der Touchscreen-Koordinaten an die Anzeigenskalierung des Betriebssystems anpassen."
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "VMouse als Zeigegerät"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "Aktivieren, um Berührungsereignisse vom Eingabe-Touchscreen weiterzuleiten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "VMouse als Maus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "Virtuelle Mausemulation unter Verwendung von Berührungsereignissen aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "VMouse im Touchpad-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Die Option Zusammen mit der Maus aktivieren, um den Touchscreen als Touchpad zu verwenden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "VMouse im Trackball-Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Diese Option zusammen mit der Maus aktivieren, um den Touchscreen als Trackball zu verwenden und dem Mauszeiger Trägheit zu verleihen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "VMouse-Gesten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "Touchscreen-Gesten wie Tippen, Ziehen mit dem Finger und Wischen mit dem Finger aktivieren."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "RGA-Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA-Skalierung und bikubischer Filter. Kann Fehler bei Widgets verursachen."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Kontextspezifische Skalierung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Hardware-Kontextskalierung (falls verfügbar)."
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "New3DS-Taktfrequenz / -L2-Cache aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "New3DS-Taktfrequenz (804MHz) und -L2-Cache aktivieren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "3DS-Bildschirm unten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Statusanzeige auf dem unteren Bildschirm aktivieren. Deaktivieren, um die Akkulaufzeit zu erhöhen und die Leistung zu verbessern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "3DS Anzeigemodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Wählt zwischen 3D- und 2D-Anzeigemodus. Im '3D' Modus sind Pixel quadratisch und ein Tiefeneffekt wird beim Anzeigen des Schnellmenüs angewendet. '2D' Modus bietet die beste Leistung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (Pixel-Raster-Effekt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (hohe Auflösung)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Hier berühren, um das\nRetroArch-Menü\naufzurufen."
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "Asset(s) nicht gefunden"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Keine\nDaten"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "Kein\nScreenshot"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Spiel fortsetzen"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Speicherpunkt\nerstellen"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Speicherpunkt\nladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "Unterer Bildschirm Asset-Verzeichnis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "Unterer Bildschirm Asset Verzeichnis. Verzeichnis muss \"bottom_menu.png\" enthalten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "Schrift aktivieren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "Text am unteren Bildschirm einblenden. Aktivieren, um Schaltflächenbeschreibungen auf dem unteren Bildschirm anzuzeigen. Dies schließt das Savestate-Speicherdatum aus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Schriftfarbe Rot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Rote Schriftfarbe auf unteren Bildschirm anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Schriftfarbe Grün"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Grüne Schriftfarbe auf unteren Bildschirm anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Schriftfarbe Blau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Blaue Schriftfarbe auf unteren Bildschirm anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Schriftfarbendeckkraft"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "Schriftdeckkraft auf unteren Bildschirm anpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "Schriftgröße"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "Schriftgröße auf unteren Bildschirm anpassen."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Scan abgeschlossen. <br><br>\nDas korrekte Scannen nach Inhalten erfordert, dass:\n<ul><li>ein kompatibler Core bereits heruntergeladen worden ist</li>\n<li>die Core-Infodateien aktualisiert wurden</li>\n<li>die Datenbanken aktualisiert wurden</li>\n<li>RetroArch nach dem Ausführen jeglicher der oberen Punkte neu gestartet wurde.</li></ul>\nSchließlich muss der Inhalt mit den <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">Datenbanken von hier</a> übereinstimmen. Falls es immer noch nicht funktioniert, kann <a href=\"https://www.github.com/libretro/RetroArch/issues\">hier ein Fehlerbericht erstattet werden</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "Mauseingabe per Touchscreen ist aktiviert"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "Mauseingabe per Touchscreen ist deaktiviert"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "sdl2-Mikrofon benötigt sdl2-Audiotreiber"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "RetroArch Bedienungshilfe an. Hauptmenü, Core laden."
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "gestoppt."
   )
#ifdef HAVE_GAME_AI
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_MENU_OPTION,
   "KI-Player überschreiben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_MENU_OPTION,
   "KI-Spieler überschreiben Unterlabel"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_GAME_AI_OPTIONS,
   "Spiel-KI"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P1,
   "P1 überschreiben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P1,
   "Spieler 01 überschreiben"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P2,
   "P2 überschreiben"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P2,
   "Spieler 02 überschreiben"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_SHOW_DEBUG,
   "Fehlerinformation anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG,
   "Fehlerinformation anzeigen"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_GAME_AI,
   "„Spiel-KI“ anzeigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_GAME_AI,
   "Die Option „Spiel-KI“ anzeigen."
   )
#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <string.h>

#include <compat/strl.h>

#include "../menu_hash.h"
#include "../../configuration.h"

 /* IMPORTANT:
  * For non-english characters to work without proper unicode support,
  * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
  * If you save this file as UTF-8, you'll break non-english characters
  * (e.g. German "Umlauts" and Portugese diacritics).
 */
/* DO NOT REMOVE THIS. If it causes build failure, it's because you saved the file as UTF-8. Read the above comment. */
extern const char force_iso_8859_1[sizeof("äÄöÖßüÜ")==7+1 ? 1 : -1];

const char *menu_hash_to_str_de(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_INFORMATION_LIST:
         return "Information";
      case MENU_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Verwende integrierten Player"; /* FIXME/UPDATE */
      case MENU_LABEL_VALUE_CONTENT_SETTINGS:
         return "Content-Einstellungen"; /* FIXME */
      case MENU_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Lade Content";
      case MENU_LABEL_VALUE_LOAD_ARCHIVE:
         return "Lade Archiv";
      case MENU_LABEL_VALUE_OPEN_ARCHIVE:
         return "Öffne Archiv";
      case MENU_VALUE_ASK_ARCHIVE:
         return "Nachfragen";
      case MENU_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Privatsphäre-Einstellungen";
      case MENU_VALUE_HORIZONTAL_MENU: /* Don't change. Breaks everything. (Would be: "Horizontales Menu") */
         return "Horizontal Menu";
      case MENU_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "Keine Einstellungen gefunden.";
      case MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "Keine Leistungszähler.";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Treiber-Einstellungen";
      case MENU_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Konfigurations-Einstellungen";
      case MENU_LABEL_VALUE_CORE_SETTINGS:
         return "Core-Einstellungen";
      case MENU_LABEL_VALUE_VIDEO_SETTINGS:
         return "Video-Einstellungen";
      case MENU_LABEL_VALUE_LOGGING_SETTINGS:
         return "Logging-Einstellungen";
      case MENU_LABEL_VALUE_SAVING_SETTINGS:
         return "Spielstand-Einstellungen";
      case MENU_LABEL_VALUE_REWIND_SETTINGS:
         return "Zurückspul-Einstellungen";
      case MENU_VALUE_SHADER:
         return "Shader";
      case MENU_VALUE_CHEAT:
         return "Cheat";
      case MENU_VALUE_USER:
         return "Benutzer";
      case MENU_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "Aktiviere System-BGM";
      case MENU_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Warte auf Audio-Frames";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW: /* TODO/FIXME */
         return "Zeige Core-Eingabe-Beschriftungen";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Verstecke unzugewiesene Core-Eingabe-Beschriftungen";
      case MENU_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Zeige OSD-Nachrichten";
      case MENU_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Schriftart der OSD-Nachrichten";
      case MENU_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Schriftgröße der OSD-Nachrichten";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "X-Position der OSD-Nachrichten";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Y-Position der OSD-Nachrichten";
      case MENU_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Aktiviere Soft-Filter";
      case MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Aktiviere Flacker-Filter";
      case MENU_VALUE_DIRECTORY_CONTENT:
         return "<Content-Verz.>";
      case MENU_VALUE_UNKNOWN:
         return "Unbekannt";
      case MENU_VALUE_DONT_CARE:
         return "Mir egal";
      case MENU_VALUE_LINEAR:
         return "Linear";
      case MENU_VALUE_NEAREST:
         return "Nächster";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "<Voreinstellung>";
      case MENU_VALUE_DIRECTORY_NONE:
         return "<Keins>";
      case MENU_VALUE_NOT_AVAILABLE:
         return "Nicht verfügbar";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Eingabebelegungs-Verzeichnis";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Eingabegerät-Autoconfig-Verzeichnis";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Aufnahme-Konfigurationsverzeichnis";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Aufnahme-Ausgabeverzeichnis";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Bildschirmfoto-Verzeichnis";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Wiedergabelisten-Verzeichnis";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Spielstand-Verzeichnis";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Savestate-Verzeichnis";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "stdin-Befehle";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Grafiktreiber";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "Aktiviere Aufnahmefunktion";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Aktiviere GPU-Aufnahmefunktion";
      case MENU_LABEL_VALUE_RECORD_PATH: /* FIXME/UPDATE */
         return "Aufnahmepfad";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Verwende Aufnahme-Ausgabeverzeichnis";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Aufnahme-Konfiguration";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Aktiviere Aufnahme von Post-Filtern";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Core-Assets-Verzeichnis"; /* FIXME/UPDATE */
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Assets-Verzeichnis";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Dynamische-Bildschirmhintergründe-Verzeichnis";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Browser-Directory";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Konfigurations-Verzeichnis";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Core-Info-Verzeichnis";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Core-Verzeichnis";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Cursor-Verzeichnis";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Content-Datenbankverzeichnis";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "System/BIOS-Verzeichnis";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Cheat-Datei-Verzeichnis";
      case MENU_LABEL_VALUE_CACHE_DIRECTORY: /* FIXME/UPDATE */
         return "Entpack-Verzeichnis";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Audio-Filter-Verzeichnis";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Grafikshader-Verzeichnis";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Grafikfilter-Verzeichnis";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Overlay-Verzeichnis";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "OSK-Overlay-Verzeichnis";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Tausche Netplay-Eingabe";
      case MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Aktiviere Netplay-Zuschauermodus";
      case MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "IP-Addresse für Netplay";
      case MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "TCP/UDP-Port für Netplay";
      case MENU_LABEL_VALUE_NETPLAY_ENABLE:
         return "Aktiviere Netplay";
      case MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Verzögere Netplay-Frames";
      case MENU_LABEL_VALUE_NETPLAY_MODE:
         return "Aktiviere Netplay-Client";
      case MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Zeige Startbildschirm";
      case MENU_LABEL_VALUE_TITLE_COLOR:
         return "Menü-Titel-Farbe";
      case MENU_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Hover-Farbe für Menü-Einträge";
      case MENU_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Zeige Uhrzeit / Datum";
      case MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Threaded Data Runloop";
      case MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Normale Farbe für Menü-Einträge";
      case MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Zeige erweitere Einstellungen";
      case MENU_LABEL_VALUE_COLLAPSE_SUBGROUPS_ENABLE:
         return "Untergruppen einklappen";
      case MENU_LABEL_VALUE_MOUSE_ENABLE:
         return "Maus-Unterstützung";
      case MENU_LABEL_VALUE_POINTER_ENABLE:
         return "Touch-Unterstützung";
      case MENU_LABEL_VALUE_CORE_ENABLE:
         return "Zeige Core-Namen";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Aktiviere DPI-Override";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "DPI-Override";
      case MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Bildschirmschone aussetzen";
      case MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Deaktiviere Desktop-Komposition";
      case MENU_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Nicht im Hintergrund laufen";
      case MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "UI-Companion beim Hochfahren starten";
      case MENU_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Menüleiste";
      case MENU_LABEL_VALUE_ARCHIVE_MODE:
         return "Verknüpfte Aktion bei Archivdateien";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Netzwerk-Befehle";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Port für Netzwerk-Befehle";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Aktiviere Verlaufsliste";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Länge der Verlaufsliste";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Geschätzte Monitor-Bildrate";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Dummy bei Core-Abschaltung";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE: /* TODO/FIXME */
         return "Cores nicht automatisch starten";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Begrenze maximale Ausführungsgeschwindigkeit";
      case MENU_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Maximale Ausführungsgeschwindigkeitd";
      case MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Lade Remap-Dateien automatisch";
      case MENU_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Zeitlupen-Verhältnis";
      case MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Core-Spezifische Konfiguration";
      case MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Lade Override-Dateien automatisch";
      case MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Speichere Konfiguration beim Beenden";
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Bilineare Filterung (HW)";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Erlaube Bildrotation";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Synchronisiere GPU und CPU";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "VSync-Intervall";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "Vertikale Synchronisation (VSync)";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Threaded Video";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotation";
      case MENU_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Aktiviere GPU-Bildschirmfotos";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Bildränder (Overscan) zuschneiden (Neustart erforderlich)";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Bildseitenverhältnis-Index";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Automatisches Bildseitenverhältnis";
      case MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Erzwinge Bildseitenverhältnis";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Bildwiederholrate";
      case MENU_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Erzwinge Deaktivierung des sRGB FBO";
      case MENU_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Unechter Vollbild-Modus (Windowed Fullscreen)";
      case MENU_LABEL_VALUE_PAL60_ENABLE:
         return "Verwende PAL60-Modus";
      case MENU_LABEL_VALUE_VIDEO_VFILTER:
         return "Bild entflackern";
      case MENU_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Kalibriere VI-Bildbreite";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Setze schwarze Frames ein";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Synchronisiere Frames fest mit GPU";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Sortiere Speicherdaten per Ordner";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Sortiere Save States per Ordner";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Verwende Vollbildmodus";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Fenterskalierung";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Ganzzahlige Bildskalierung";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Leistungsindikatoren";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Core-Logging-Stufe";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Log-Ausführlichkeit";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Automatisches Laden von Save States";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Automatische Indexierung von Save States";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Automatische Save States";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Autospeicherungsintervall";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Blockiere SRAM-Überschreibung";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "HW-Shared-Context aktivieren";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Starte RetroArch neu";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Benutzername";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Sprache";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Erlaube Kamera-Zugriff";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Erlaube Standort-Lokalisierung";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pausiere, wenn das Menü aktiv ist";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Zeige Tastatur-Overlay";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Aktiviere Overlay";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Monitor-Index";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Bildverzögerung";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Auslastungsgrad";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Turbo-Dauer";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Schwellwert der Eingabe-Achsen";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Bind-Remapping aktivieren";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Maximale Benutzerzahl";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Automatische Konfiguration aktivieren";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Audio-Frequenzrate (KHz)";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Maximaler Audioversatz";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Cheat-Durchgänge";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Speichere Core-Remap-Datei";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Speichere Spiel-Remap-Datei";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Änderungen übernehmen";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Änderungen übernehmen";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Zurückspulen (Rewind) aktivieren";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Lade Content (Sammlung)";  /* FIXME */
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Lade Content (Core erkennen)";  /* FIXME */
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Lade Content (Verlauf)"; /* FIXME/UPDATE */
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Aktiviere Audio";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Zeige Framerate";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Stumm";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Lautstärke (dB)";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Synchronisiere Audio";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Audio Rate Control Delta";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Shader-Durchgänge";  /* FIXME */
      case MENU_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Lade Konfigurationsdatei"; /* FIXME/UPDATE */
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Genauigkeit des Zurückspulens (Rewind)";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Lade Remap-Datei";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Benutzerdefiniertes Verhältnis";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Diesen Ordner verwenden>";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Starte Content";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Datenträger-Optionen";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Core-Optionen";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Core-Cheat-Optionen";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Lade Cheat-Datei";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Speichere Cheat-Datei unter...";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Core-Zähler";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Bildschirmfoto";
      case MENU_LABEL_VALUE_RESUME:
         return "Fortsetzen";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Datenträger-Nummer";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Frontendzähler";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Füge Datenträgerabbild hinzu";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Datenträgerstatus";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Keine Wiedergabelisten-Eintrage verfügbar.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Keine Core-Informationen verfügbar.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Keine Core-Optionen verfügbar.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Kein Core verfügbar.";
      case MENU_VALUE_NO_CORE:
         return "Kein Core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Datenbankmanager";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Cursormanager";
      case MENU_VALUE_MAIN_MENU: /* Don't change. Breaks everything. (Would be: "Hauptmenü") */
         return "Main Menu"; 
      case MENU_LABEL_VALUE_SETTINGS:
         return "Einstellungen";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "RetroArch beenden";
      case MENU_LABEL_VALUE_HELP:
         return "Hilfe";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Speichere neue Konfiguration";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Starte Content neu";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Core-Updater";
      case MENU_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "Buildbot-Cores-URL";
      case MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "Buildbot-Assets-URL";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Navigation umbrechen";
      case MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Bekannte Dateiendungen filtern";
      case MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Heruntergeladene Archive automatisch entpacken";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Systeminformationen";
      case MENU_LABEL_VALUE_ONLINE_UPDATER:
         return "Online-Aktualisierungen";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Core-Informationen";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Ordner nicht gefunden.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Keine Einträge.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Lade Core";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Lade Content"; /* FIXME */
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Schließe Content"; /* FIXME */
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Datenbank-Einstellungen";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Speichere Savestate";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Lade Savestate";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Content fortsetzen";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Eingabe-Treiber";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Audio-Treiber";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Joypad-Treiber";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Audio-Resampler-Treiber";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Aufnahme-Treiber";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Menü-Treiber";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Kamera-Treiber";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Standort-Treiber";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Komprimiertes Archiv kann nicht gelesen werden.";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Overlay-Skalierung";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay-Voreinstellung";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Audiolatenz (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Soundkarte";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Tastatur-Overlay-Voreinstellung";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Overlay-Transparenz";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Menühintergrund";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Dynamischer Hintergrund";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Core-Input-Optionen";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Shader-Optionen";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Momentane Shaderparameter"; /* FIXME/UPDATE */
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Menü Shaderparameter (Menü)";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Speiche Shader-Voreinstellung unter...";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Keine Shaderparameter";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Lade Shader-Voreinstellung";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Videofilter";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Audio-DSP-Plugin";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Starte Download: ";
      case MENU_VALUE_SECONDS:
         return "Sekunden";
      case MENU_VALUE_OFF: /* Don't change. Needed for XMB atm. (Would be: "AN") */
         return "OFF"; 
      case MENU_VALUE_ON: /* Don't change. Needed for XMB atm. (Would be: "AUS") */
         return "ON"; 
      case MENU_LABEL_VALUE_UPDATE_ASSETS:
         return "Aktualisiere Assets";
      case MENU_LABEL_VALUE_UPDATE_CHEATS:
         return "Aktualisiere Cheats";
      case MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Aktualisiere Autoconfig-Profile";
      case MENU_LABEL_VALUE_UPDATE_DATABASES:
         return "Aktualisiere Datenbanken";
      case MENU_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Aktualisiere Overlays";
      case MENU_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Aktualisiere CG-Shader";
      case MENU_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Aktualisiere GLSL-Shader";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Core-Name";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Core-Beschriftung";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "System-Name";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "System-Hersteller";
      case MENU_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Kategorien";
      case MENU_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Autoren";
      case MENU_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Berechtigungen";
      case MENU_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Lizenz(en)";
      case MENU_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Unterstütze Erweiterungen";
      case MENU_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NOTES:
         return "Core-Hinweise";
      case MENU_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Build-Datum";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Git-Version";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "CPU-Eigenschaften";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Frontend-Kennung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Frontend-Name";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "Frontend-Betriebssystem";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "RetroRating-Stufe";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Energiequelle";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "Keine Quelle";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Lädt";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Geladen";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Entlädt";
      case MENU_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Video-Context-Treiber";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Bildschirmbreite (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Bildschirmhöhe (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Bildschirm-DPI";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "LibretroDB-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Overlay-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Befehlsinterface-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Netzwerk-Befehlsinterface-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Cocoa-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "PNG-Unterstützung (RPNG)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "SDL1.2-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "SDL2-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "OpenGL-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "OpenGL-ES-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Threading-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "KMS/EGL-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Udev-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "OpenVG-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "EGL-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "X11-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Wayland-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "XVideo-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "ALSA-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "OSS-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "OpenAL-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "OpenSL-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "RSound-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "RoarAudio-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "JACK-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "PulseAudio-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "DirectSound-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "XAudio2-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Zlib-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "7zip-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Dynamic-Library-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Cg-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "GLSL-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "HLSL-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "Libxml2-XML-Parsing-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "SDL-Image-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "Unterstützung für OpenGL/Direct3D Render-to-Texture (Multi-Pass Shader)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "FFmpeg-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "CoreText-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "FreeType-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Netplay-Unterstützung (Peer-to-Peer)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Python-Unterstützung (Script-Unterstützung in Shadern)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Video4Linux2-Unterstützung";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Libusb-Unterstützung";
      case MENU_LABEL_VALUE_YES:
         return "Ja";
      case MENU_LABEL_VALUE_NO:
         return "Nein";
      case MENU_VALUE_BACK:
         return "ZURÜCK";
      case MENU_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Bildschirmauflösung";
      case MENU_VALUE_DISABLED:
         return "Deaktiviert";
      case MENU_VALUE_PORT:
         return "Port";
      case MENU_VALUE_NONE:
         return "Keins";
      case MENU_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Entwickler";
      case MENU_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Publisher";
      case MENU_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Beschreibung";
      case MENU_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Name";
      case MENU_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Herkunft";
      case MENU_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franchise";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Veröffentlichungsmonat";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Veröffentlichungsjahr";
      case MENU_VALUE_TRUE:
         return "True";
      case MENU_VALUE_FALSE:
         return "False";
      case MENU_VALUE_MISSING:
         return "Fehlt";
      case MENU_VALUE_PRESENT:
         return "Vorhanden";
      case MENU_VALUE_OPTIONAL:
         return "Optional";
      case MENU_VALUE_REQUIRED:
         return "Notwendig";
      case MENU_VALUE_STATUS:
         return "Status";
      case MENU_LABEL_VALUE_AUDIO_SETTINGS:
         return "Audio-Einstellungen";
      case MENU_LABEL_VALUE_INPUT_SETTINGS:
         return "Eingabe-Einstellungen";
      case MENU_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "OSD-Einstellungen";
      case MENU_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Overlay-Einstellungen";
      case MENU_LABEL_VALUE_MENU_SETTINGS:
         return "Menü-Einstellungen";
      case MENU_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Media-Player-Einstellungen"; /* UPDATE/FIXME */
      case MENU_LABEL_VALUE_UI_SETTINGS:
         return "Benutzeroberflächen-Einstellungen";
      case MENU_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Menü-Dateibrowser-Einstellungen";
      case MENU_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Core-Updater-Einstellungen"; /* UPDATE/FIXME */
      case MENU_LABEL_VALUE_NETWORK_SETTINGS:
         return "Netzwerk-Einstellungen";
      case MENU_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Wiedergabelisten-Einstellungen";
      case MENU_LABEL_VALUE_USER_SETTINGS:
         return "Benutzer-Einstellungen";
      case MENU_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Verzeichnis-Einstellungen";
      case MENU_LABEL_VALUE_RECORDING_SETTINGS:
         return "Aufnahme-Einstellungen";
      case MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "Keine Informationen verfügbar.";
      case MENU_LABEL_VALUE_INPUT_USER_BINDS:
         return "Spieler %u Tastenbelegung";
      case MENU_VALUE_LANG_ENGLISH:
         return "Englisch";
      case MENU_VALUE_LANG_JAPANESE:
         return "Japanisch";
      case MENU_VALUE_LANG_FRENCH:
         return "Französisch";
      case MENU_VALUE_LANG_SPANISH:
         return "Spanisch";
      case MENU_VALUE_LANG_GERMAN:
         return "Deutsch";
      case MENU_VALUE_LANG_ITALIAN:
         return "Italienisch";
      case MENU_VALUE_LANG_DUTCH:
         return "Niederländisch";
      case MENU_VALUE_LANG_PORTUGUESE:
         return "Portugiesisch";
      case MENU_VALUE_LANG_RUSSIAN:
         return "Russisch";
      case MENU_VALUE_LANG_KOREAN:
         return "Koreanisch";
      case MENU_VALUE_LANG_CHINESE_TRADITIONAL:
         return "Chinesisch (Traditionell)";
      case MENU_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "Chinesisch (Vereinfacht)";
      case MENU_VALUE_LANG_ESPERANTO:
         return "Esperanto";
      case MENU_VALUE_LEFT_ANALOG:
         return "Linker Analogstick";
      case MENU_VALUE_RIGHT_ANALOG:
         return "Rechter Analogstick";
      default:
         break;
   }

   return "null";
}

int menu_hash_get_help_de(uint32_t hash, char *s, size_t len)
{
   uint32_t      driver_hash = 0;
   settings_t      *settings = config_get_ptr();

   /* If this one throws errors, stop sledgehammering square pegs into round holes and */
   /* READ THE COMMENTS at the top of the file. */ (void)sizeof(force_iso_8859_1);

   switch (hash)
   {
      case MENU_LABEL_INPUT_DRIVER:
         driver_hash = menu_hash_calculate(settings->input.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_INPUT_DRIVER_UDEV:
               {
                  /* Work around C89 limitations */
                  char u[501];
                  char t[501];

                  snprintf(t, sizeof(t),
                        "udev-Eingabetreiber. \n"
                        " \n"
                        "Dieser Treiber kann ohne X ausgeführt werden. \n"
                        " \n"
                        "Er verwende die neue evdev-Joypad-API \n"
                        "für die Joystick-Unterstützung und unterstützt \n"
                        "auch Hotplugging und Force-Feedback (wenn das \n"
                        "Gerät dies unterstützt). \n"
                        " \n"
                        );
                  snprintf(u, sizeof(u),
                        "Der Treiber liest evdev-Ereigniss für Tastatur- \n"
                        "Unterstützung und kann auch mit  Tastatur-Callbacks, \n"
                        "Mäusen und Touchpads umgehen. \n"
                        " \n"
                        "Standardmäßig sind die /dev/input-Dateien in den \n"
                        "meisten Linux-Distribution nur für den Root- \n"
                        "Benutzer lesbar (mode 600). Du kannst eine udev- \n"
                        "Regel erstellen, die auch den Zugriff für andere \n"
                        "Benutzer erlaubt.");
                  strlcat(s, t, len);
                  strlcat(s, u, len);
               }
               break;
            case MENU_LABEL_INPUT_DRIVER_LINUXRAW:
               snprintf(s, len,
                     "linuxraw-Eingabetreiber. \n"
                     " \n"
                     "Dieser Treiber erforder eine aktive TTY-Schnittstelle. \n"
                     "Tastatur-Ereignisse werden direkt von der TTY gelesen, \n"
                     "was es einfacher, aber weniger flexibel als udev macht. \n"
                     "Mäuse, etc, werden nicht unterstützt. \n"
                     " \n"
                     "Dieser Treiber verwendet die alte Joystick-API \n"
                     "(/dev/input/js*).");
               break;
            default:
               snprintf(s, len,
                     "Eingabetreiber.\n"
                     " \n"
                     "Abhängig vom Grafiktreiber kann ein anderer Eingabe- \n"
                     "treiber erzwungen werden.");
               break;
         }
         break;
      case MENU_LABEL_LOAD_CONTENT:
         snprintf(s, len,
               "Lade Content. \n"
               "Suche nach Content. \n"
               " \n"
               "Um Content zu laden benötigst du den passenden \n"
               "Libretro-Core und die Content-Datei. \n"
               " \n"
               "Um einzustellen, welcher Ordner standardmäßig \n"
               "geöffnet wird, um nach Content zu suchen, solltest \n"
               "du das Content-Verzeichnis setzen. Wenn es nicht \n"
               "gesetzt ist, wird es im Root-Verzeichen starten. \n"
               " \n"
               "Der Browser wird nur Dateierweiterungen des \n"
               "zuletzt geladenen Cores zeigen und diesen Core \n"
               "nutzen, wenn Content geladen wird."
               );
         break;
      case MENU_LABEL_CORE_LIST:
         snprintf(s, len,
               "Lade Core. \n"
               " \n"
               "Suche nach einer Libretro-Core- \n"
               "Implementierung. In welchem Verzeichnis der \n"
               "Browser startet, hängt vom deinem Core-Verzeichnis \n"
               "ab. Falls du es nicht eingestellt hast, wird er \n"
               "im Root-Verzeichnis starten. \n"
               " \n"
               "Ist das Core-Verzeichnis ein Ordner, wird das \n"
               "Menü diesen als Startverzeichnis nutzen. Ist \n"
               "das Core-Verzeichnis ein Pfad zu einer Datei, \n"
               "wird es in dem Verzeichnis starten, in dem \n"
               "sich die Datei befindet.");
         break;
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
         snprintf(s, len,
               "Lade Content aus dem Verlauf. \n"
               " \n"
               "Wenn Content geladen wird, wird der Content \n"
               "sowie der dazugehörige Core im Verlauf gespeichert. \n"
               " \n"
               "Der Verlauf wird im selben Verzeichnis wie die \n"
               "RetroArch-Konfigurationsdatei gespeicher. Wenn \n"
               "beim Start keine Konfigurationsdatei geladen wurde, \n"
               "wird keine Verlauf geladen oder gespeichert und nicht \n"
               "im Hauptmenü angezeigt."
               );
         break;
      case MENU_LABEL_VIDEO_DRIVER:
         driver_hash = menu_hash_calculate(settings->video.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_VIDEO_DRIVER_GL:
               snprintf(s, len,
                     "OpenGL-Grafiktreiber. \n"
                     " \n"
                     "Dieser Treiber erlaubt es, neben software- \n"
                     "gerenderten Cores aus Libretro-GL-Cores zu \n"
                     "verwenden. \n"
                     " \n"
                     "Die Leistung, sowohl bei software-gerenderten, \n"
                     "als auch bei Libretro-GL-Cores, hängt von dem \n"
                     "GL-Treiber deiner Grafikkarte ab.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SDL2:
               snprintf(s, len,
                     "SDL2-Grafiktreiber.\n"
                     " \n"
                     "Dies ist ein SDL2-Grafiktreiber \n"
                     "mit Software-Rendering."
                     " \n"
                     "Die Leistung hängt von der SDL- \n"
                     "Implementierung deiner Plattform ab.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SDL1:
               snprintf(s, len,
                     "SDL-Grafiktreiber.\n"
                     " \n"
                     "Dies ist ein SDL1.2-Grafiktreiber \n"
                     "mit Software-Rendering."
                     " \n"
                     "Die Leistung ist suboptimal und du \n"
                     "solltest ihn nur als letzte \n"
                     "Möglichkeit verwenden.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_D3D:
               snprintf(s, len,
                     "Direct3D-Grafiktreiber. \n"
                     " \n"
                     "Die Leistung bei software-gerenderten \n"
                     "Cores hängt von dem D3D-Treiber deiner \n"
                     "Grafikkarte ab.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_EXYNOS:
               snprintf(s, len,
                     "Exynos-G2D-Grafiktreiber. \n"
                     " \n"
                     "Dies ist ein Low-Level-Exynos-Grafiktreiber. \n"
                     "Er verwendet den G2D-Block in Samsung-Exynos-SoCs. \n"
                     "für Blitting-Operationen. \n"
                     " \n"
                     "Die Leistung bei software-gerendeten Cores sollte \n"
                     "optimal sein.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SUNXI:
               snprintf(s, len,
                     "Sunxi-G2D-Grafiktreiber\n"
                     " \n"
                     "Dies ist ein Low-Level-Sunxi-Grafiktreiber. \n"
                     "Er verwendet den G2D-Block in Allwinnder-SoCs.");
               break;
            default:
               snprintf(s, len,
                     "Momentaner Grafiktreiber.");
               break;
         }
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Audio-DSP-Plugin.\n"
               " Verarbeitet Audiodaten, bevor \n"
               "sie zum Treiber gesendet werden."
               );
         break;
      case MENU_LABEL_AUDIO_RESAMPLER_DRIVER:
         driver_hash = menu_hash_calculate(settings->audio.resampler);

         switch (driver_hash)
         {
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC:
               snprintf(s, len,
                     "Windowed-SINC-Implementierung.");
               break;
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC:
               snprintf(s, len,
                     "Convoluted-Kosinus-Implementierung.");
               break;
         }
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         snprintf(s, len,
               "Lade Shader-Voreinstellung. \n"
               " \n"
               " Lade eine "
#ifdef HAVE_CG
               "Cg"
#endif
#ifdef HAVE_GLSL
#ifdef HAVE_CG
               "/"
#endif
               "GLSL"
#endif
#ifdef HAVE_HLSL
#if defined(HAVE_CG) || defined(HAVE_HLSL)
               "/"
#endif
               "HLSL"
#endif
               "-Voreinstellung. \n"
               "Das Menüshader-Menü wird entsprechend \n"
               "aktualisiert."
               " \n"
               "Wenn der CGP komplexe Methoden verwendet, \n"
               "(also andere als Quellskalierung mit dem \n"
               "selben Faktor für X/Y) kann der im Menü \n"
               "angezeigte Skalierungsfaktor inkorrekt sein."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         snprintf(s, len,
               "Für diesen Durchgang skalieren. \n"
               " \n"
               "Der Skalierungsfaktor wird multipliziert, \n"
               "d.h. 2x im ersten Durchgang und 2x im \n"
               "zweiten Durchgang bedeute eine 4x Gesamt- \n"
               "Skalierung."
               " \n"
               "Wenn es im letzten Durchgang einen \n"
               "Skalierungsfaktor gibt, wird das Ergebnis \n"
               "mit dem als 'Standardfilter' eingestellten \n"
               "Filter auf die Bildschirmgröße gestreckt. \n"
               " \n"
               "Wenn 'Mir egal' eingestellt ist, wird \n"
               "entweder einfache Skalierung or Vollbild- \n"
               "Streckung verwendet - abhängig davon, ob \n"
               "es der letzte Durchgang ist oder nicht."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         snprintf(s, len,
               "Shader-Durchgänge. \n"
               " \n"
               "RetroArch erlaubt es dir, verschiedene Shader \n"
               "in verschiedenen Durchgängen miteinander zu \n"
               "kombinieren. \n"
               " \n"
               "Diese Option legt die Anzahl der Shader- \n"
               "Durchgänge fest. Wenn du die Anzahl auf 0 setzt, \n"
               "verwendest du einen 'leeren' Shader."
               " \n"
               "Die 'Standardfilter'-Option beeinflusst den \n"
               "Streckungsfilter");
         break;
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Shader-Parameter. \n"
               " \n"
               "Verändert den momentanen Shader. Wird nicht in \n"
               "der CGP/GLSLP-Voreinstellungs-Datei gespeichert.");
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         snprintf(s, len,
               "Shader-Voreinstellung-Parameter. \n"
               " \n"
               "Verändert die Shader-Voreinstellung, die aktuell \n"
               "im Menü aktiv ist."
               );
         break;
      /*
       * FIXME: Some stuff still missing here.
       */
      case MENU_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "(De-)Aktiviere die Bildschirmtastatur.");
         break;
      case MENU_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Audio stummschalten.");
         break;
      case MENU_LABEL_REWIND:
         snprintf(s, len,
               "Halte die Taste zum Zurückspulen gedrückt.\n"
               " \n"
               "Die Zurückspulfunktion muss eingeschaltet \n"
               "sein.");
         break;
      case MENU_LABEL_EXIT_EMULATOR:
         snprintf(s, len,
               "Taste zum Beenden von RetroArch."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nWenn du es stattdessen mittels SIGKILL \n"
               "beendest, wird RetroArch nicht den RAM \n"
               "sichern. Bei unixoiden Betriebssystemen \n"
               "erlaubt SIGINT/SIGTERM ein sauberes \n"
               "Beenden."
#endif
               );
         break;
      case MENU_LABEL_LOAD_STATE:
         snprintf(s, len,
               "Lädt einen Savestate.");
         break;
      case MENU_LABEL_SAVE_STATE:
         snprintf(s, len,
               "Speichert einen Savestate.");
         break;
      case MENU_LABEL_NETPLAY_FLIP_PLAYERS:
         snprintf(s, len,
               "Netplay-Spieler tauschen.");
         break;
      case MENU_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "Erhöht den Cheat-Index.\n");
         break;
      case MENU_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Verringert den Cheat-Index.\n");
         break;
      case MENU_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Wendet vorherigen Shader im Verzeichnis an.");
         break;
      case MENU_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Wendet nächsten Shader im Verzeichnis an.");
         break;
      case MENU_LABEL_RESET:
         snprintf(s, len,
               "Setzt den Content zurück.\n");
         break;
      case MENU_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Pausiert den Content und setzt ihn wieder fort.");
         break;
      case MENU_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Schaltet den Cheat-Index ein und aus.\n");
         break;
      case MENU_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Halte den Knopf gedrückt, um vorzuspulen. Beim Loslassen \n"
               "wird das Vorspulen beendet.");
         break;
      case MENU_LABEL_SLOWMOTION:
         snprintf(s, len,
               "Halte den Knopf gedrückt, um die Zeitlupe einzuschalten.");
         break;
      case MENU_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "Frame-Advance, wenn der Content pausiert ist.");
         break;
      case MENU_LABEL_MOVIE_RECORD_TOGGLE:
         snprintf(s, len,
               "Aufnahme ein- und ausschalten.");
         break;
      case MENU_LABEL_L_X_PLUS:
      case MENU_LABEL_L_X_MINUS:
      case MENU_LABEL_L_Y_PLUS:
      case MENU_LABEL_L_Y_MINUS:
      case MENU_LABEL_R_X_PLUS:
      case MENU_LABEL_R_X_MINUS:
      case MENU_LABEL_R_Y_PLUS:
      case MENU_LABEL_R_Y_MINUS:
         snprintf(s, len,
               "Achse für Analog-Stick (DualShock-artig).\n"
               " \n"
               "Zugewiesen wie gewöhnlich, wenn jedoch eine echte \n"
               "Analogachse zugewiesen wird, kann sie auch wirklich \n"
               "analog gelesen werden.\n"
               " \n"
               "Positive X-Achse ist rechts. \n"
               "Positive Y-Achse ist unten.");
         break;
      default:
         return -1;
   }

   return 0;
}

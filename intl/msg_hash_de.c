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
#include <string/stdstring.h>

#include "../msg_hash.h"
#include "../configuration.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Winvalid-source-encoding"
#endif

int menu_hash_get_help_de_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   uint32_t      driver_hash = 0;
   settings_t      *settings = config_get_ptr();

   switch (msg)
   {
      case MENU_ENUM_LABEL_CORE_LIST:
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
      case MENU_ENUM_LABEL_INPUT_DRIVER:
         if (settings)
            driver_hash = msg_hash_calculate(settings->input.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_INPUT_DRIVER_UDEV:
               {
                  /* Work around C89 limitations */
                  const char * t =
                        "udev-Eingabetreiber. \n"
                        " \n"
                        "Dieser Treiber kann ohne X ausgeführt werden. \n"
                        " \n"
                        "Er verwende die neue evdev-Joypad-API \n"
                        "für die Joystick-Unterstützung und unterstützt \n"
                        "auch Hotplugging und Force-Feedback (wenn das \n"
                        "Gerät dies unterstützt). \n"
                        " \n";
                  const char * u =
                        "Der Treiber liest evdev-Ereignisse für die Tastatur- \n"
                        "Unterstützung und kann auch mit Tastatur-Callbacks, \n"
                        "Mäusen und Touchpads umgehen. \n"
                        " \n"
                        "Standardmäßig sind die /dev/input-Dateien in den \n"
                        "meisten Linux-Distribution nur vom Root- \n"
                        "Benutzer lesbar (mode 600). Du kannst eine udev- \n"
                        "Regel erstellen, die auch den Zugriff für andere \n"
                        "Benutzer erlaubt.";
                  strlcpy(s, t, len);
                  strlcat(s, u, len);
               }
               break;
            case MENU_LABEL_INPUT_DRIVER_LINUXRAW:
               snprintf(s, len,
                     "linuxraw-Eingabetreiber. \n"
                     " \n"
                     "Dieser Treiber erfordert eine aktive TTY-Schnittstelle. \n"
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
      case MENU_ENUM_LABEL_LOAD_CONTENT:
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
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         snprintf(s, len,
               "Lade Content aus dem Verlauf. \n"
               " \n"
               "Wenn Content geladen wird, wird der Content \n"
               "sowie der dazugehörige Core im Verlauf gespeichert. \n"
               " \n"
               "Der Verlauf wird im selben Verzeichnis wie die \n"
               "RetroArch-Konfigurationsdatei gespeichert. Wenn \n"
               "beim Start keine Konfigurationsdatei geladen wurde, \n"
               "wird keine Verlauf geladen oder gespeichert und nicht \n"
               "im Hauptmenü angezeigt."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_DRIVER:
         snprintf(s, len,
               "Momentaner Grafiktreiber.");

         if (string_is_equal(settings->video.driver, "gl"))
         {
            snprintf(s, len,
                  "OpenGL-Grafiktreiber. \n"
                  " \n"
                  "Dieser Treiber erlaubt es, neben software- \n"
                  "gerenderten Cores auch Libretro-GL-Cores zu \n"
                  "verwenden. \n"
                  " \n"
                  "Die Leistung, sowohl bei software-gerenderten, \n"
                  "als auch bei Libretro-GL-Cores, hängt von dem \n"
                  "GL-Treiber deiner Grafikkarte ab.");
         }
         else if (string_is_equal(settings->video.driver, "sdl2"))
         {
            snprintf(s, len,
                  "SDL2-Grafiktreiber.\n"
                  " \n"
                  "Dies ist ein SDL2-Grafiktreiber \n"
                  "mit Software-Rendering."
                  " \n"
                  "Die Leistung hängt von der SDL- \n"
                  "Implementierung deiner Plattform ab.");
         }
         else if (string_is_equal(settings->video.driver, "sdl1"))
         {
            snprintf(s, len,
                  "SDL-Grafiktreiber.\n"
                  " \n"
                  "Dies ist ein SDL1.2-Grafiktreiber \n"
                  "mit Software-Rendering."
                  " \n"
                  "Die Leistung ist suboptimal und du \n"
                  "solltest ihn nur als letzte \n"
                  "Möglichkeit verwenden.");
         }
         else if (string_is_equal(settings->video.driver, "d3d"))
         {
            snprintf(s, len,
                  "Direct3D-Grafiktreiber. \n"
                  " \n"
                  "Die Leistung bei software-gerenderten \n"
                  "Cores hängt von dem D3D-Treiber deiner \n"
                  "Grafikkarte ab.");
         }
         else if (string_is_equal(settings->video.driver, "exynos"))
         {
            snprintf(s, len,
                  "Exynos-G2D-Grafiktreiber. \n"
                  " \n"
                  "Dies ist ein Low-Level-Exynos-Grafiktreiber. \n"
                  "Er verwendet den G2D-Block in Samsung-Exynos-SoCs. \n"
                  "für Blitting-Operationen. \n"
                  " \n"
                  "Die Leistung bei software-gerendeten Cores sollte \n"
                  "optimal sein.");
         }
         else if (string_is_equal(settings->video.driver, "sunxi"))
         {
            snprintf(s, len,
                  "Sunxi-G2D-Grafiktreiber\n"
                  " \n"
                  "Dies ist ein Low-Level-Sunxi-Grafiktreiber. \n"
                  "Er verwendet den G2D-Block in Allwinner-SoCs.");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Audio-DSP-Plugin.\n"
               " Verarbeitet Audiodaten, bevor \n"
               "sie zum Treiber gesendet werden."
               );
         break;
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         if (settings)
            driver_hash = msg_hash_calculate(settings->audio.resampler);

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
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
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
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Shader-Parameter. \n"
               " \n"
               "Verändert den momentanen Shader. Wird nicht in \n"
               "der CGP/GLSLP-Voreinstellungs-Datei gespeichert.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
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
      case MENU_ENUM_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "(De-)Aktiviere die Bildschirmtastatur.");
         break;
      case MENU_ENUM_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Audio stummschalten.");
         break;
      case MENU_ENUM_LABEL_REWIND:
         snprintf(s, len,
               "Halte die Taste zum Zurückspulen gedrückt.\n"
               " \n"
               "Die Zurückspulfunktion muss eingeschaltet \n"
               "sein.");
         break;
      case MENU_ENUM_LABEL_EXIT_EMULATOR:
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
      case MENU_ENUM_LABEL_LOAD_STATE:
         snprintf(s, len,
               "Lädt einen Savestate.");
         break;
      case MENU_ENUM_LABEL_SAVE_STATE:
         snprintf(s, len,
               "Speichert einen Savestate.");
         break;
      case MENU_ENUM_LABEL_NETPLAY_FLIP_PLAYERS:
         snprintf(s, len,
               "Netplay-Spieler tauschen.");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "Erhöht den Cheat-Index.\n");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Verringert den Cheat-Index.\n");
         break;
      case MENU_ENUM_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Wendet vorherigen Shader im Verzeichnis an.");
         break;
      case MENU_ENUM_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Wendet nächsten Shader im Verzeichnis an.");
         break;
      case MENU_ENUM_LABEL_RESET:
         snprintf(s, len,
               "Setzt den Content zurück.\n");
         break;
      case MENU_ENUM_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Pausiert den Content und setzt ihn wieder fort.");
         break;
      case MENU_ENUM_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Schaltet den Cheat-Index ein und aus.\n");
         break;
      case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Halte den Knopf gedrückt, um vorzuspulen. Beim Loslassen \n"
               "wird das Vorspulen beendet.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION:
         snprintf(s, len,
               "Halte den Knopf gedrückt, um die Zeitlupe einzuschalten.");
         break;
      case MENU_ENUM_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "Frame-Advance, wenn der Content pausiert ist.");
         break;
      case MENU_ENUM_LABEL_MOVIE_RECORD_TOGGLE:
         snprintf(s, len,
               "Aufnahme ein- und ausschalten.");
         break;
      case MENU_ENUM_LABEL_L_X_PLUS:
      case MENU_ENUM_LABEL_L_X_MINUS:
      case MENU_ENUM_LABEL_L_Y_PLUS:
      case MENU_ENUM_LABEL_L_Y_MINUS:
      case MENU_ENUM_LABEL_R_X_PLUS:
      case MENU_ENUM_LABEL_R_X_MINUS:
      case MENU_ENUM_LABEL_R_Y_PLUS:
      case MENU_ENUM_LABEL_R_Y_MINUS:
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
      case MSG_UNKNOWN:
      default:
         /* TODO/FIXME - translate */
         if (string_is_empty(s))
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

const char *msg_hash_to_str_de(enum msg_hash_enums msg)
{
   switch (msg)
   {
      MSG_HASH(MENU_ENUM_LABEL_SETTINGS,
         "Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_SHUTDOWN,
         "Ausschalten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
         "Passwort")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
         "Benutzername")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
         "Konten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TAB,
         "Hinzufügen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ARCHIVE_MODE,
         "Verknüpfte Aktion bei Archivdateien")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
         "Nachfragen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
         "Assets-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
         "Warte auf Audio-Frames")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
         "Soundkarte")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
         "Audio-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
         "Audio-DSP-Plugin")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
         "Aktiviere Audio")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
         "Audio-Filter-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
         "Audiolatenz (ms)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
         "Maximaler Audioversatz")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
         "Stumm")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
         "Audio-Frequenzrate (kHz)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
         "Audio Rate Control Delta")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
         "Audio-Resampler-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
         "Audio-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
         "Synchronisiere Audio")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
         "Lautstärke (dB)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
         "Autospeicherungsintervall")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
         "Lade Override-Dateien automatisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
         "Lade Remap-Dateien automatisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
         "Blockiere SRAM-Überschreibung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
         "Buildbot-Assets-URL")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY, /* FIXME/UPDATE */
         "Entpack-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
         "Erlaube Kamera-Zugriff")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
         "Kamera-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEAT,
         "Cheat")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
         "Änderungen übernehmen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
         "Cheat-Datei-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
         "Lade Cheat-Datei")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
         "Speichere Cheat-Datei unter...")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
         "Cheat-Durchgänge")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
         "Schließe")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
         "Lade Konfigurationsdatei") /* FIXME/UPDATE */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
         "Konfigurations-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
         "Speichere Konfiguration beim Beenden")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CONFIRM_ON_EXIT,
         "Zum Beenden Nachfragen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST,
         "Lade Content (Sammlung)")  /* FIXME/TODO - rewrite */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
         "Content-Datenbankverzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
         "Länge der Verlaufsliste")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
         "Content-Einstellungen") /* FIXME */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
         "Core-Assets-Verzeichnis") /* FIXME/UPDATE */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
         "Cheats")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
         "Core-Zähler")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
         "Zeige Core-Namen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
         "Core-Informationen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
         "Autoren")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
         "Kategorien")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
         "Core-Beschriftung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
         "Core-Name")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NOTES,
         "Core-Hinweise")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
         "Firmware")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
         "Lizenz(en)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
         "Berechtigungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
         "Unterstütze Erweiterungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
         "System-Hersteller")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
         "System-Name")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS, /* UPDATE/FIXME */
         "Core-Input-Optionen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_LIST,
         "Lade Core")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
         "Optionen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
         "Core-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE, /* TODO/FIXME */
         "Cores nicht automatisch starten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SPECIFIC_CONFIG,
         "Core-Spezifische Konfiguration")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
         "Heruntergeladene Archive automatisch entpacken")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
         "Buildbot-Cores-URL")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
         "Core-Updater")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
         "Core-Updater-Einstellungen") /* UPDATE/FIXME */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
         "Cursor-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
         "Cursormanager")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
         "Benutzerdefiniertes Verhältnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
         "Datenbankmanager")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
         "Von der Playlist löschen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST,
         "Lade Content (Core erkennen)")  /* FIXME */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
         "<Content-Verz.>")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
         "<Voreinstellung>")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
         "<Keins>")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
         "Ordner nicht gefunden.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
         "Verzeichnis-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DISABLED,
         "Deaktiviert")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS,
         "Datenträgerstatus")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
         "Füge Datenträgerabbild hinzu")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_INDEX,
         "Datenträger-Nummer")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_OPTIONS, /* UPDATE/FIXME */
         "Datenträger-Optionen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DONT_CARE,
         "Mir egal")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE,
         "Aktiviere DPI-Override")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE,
         "DPI-Override")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
         "Treiber-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
         "Dummy bei Core-Abschaltung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
         "Dynamischer Hintergrund")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
         "Dynamische-Bildschirmhintergründe-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR,
         "Hover-Farbe für Menü-Einträge")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR,
         "Normale Farbe für Menü-Einträge")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_FALSE,
         "False")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
         "Maximale Ausführungsgeschwindigkeit")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_SHOW,
         "Zeige Framerate")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
         "Begrenze maximale Ausführungsgeschwindigkeit")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
         "Frontendzähler")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP,
         "Hilfe")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
         "Aktiviere Verlaufsliste")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
         "Verlauf")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU, /* Don't change. Breaks everything. (Would be, "Horizontales Menu") */
         "Horizontal Menu")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
         "Bilder")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
         "Information")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
         "Jeder nutze kann Menü Steuern")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
         "Automatische Konfiguration aktivieren")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD,
         "Schwellwert der Eingabe-Achsen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
         "Verstecke nicht zugewiesene Core-Eingabe-Beschriftungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW, /* TODO/FIXME */
         "Zeige Core-Eingabe-Beschriftungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
         "Eingabe-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
         "Auslastungsgrad")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
         "Maximale Benutzerzahl")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE,
         "Zeige Tastatur-Overlay")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
         "Aktiviere Overlay")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY, /* UPDATE/FIXME */
         "Eingabebelegungs-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
         "Bind-Remapping aktivieren")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
         "Eingabe-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
         "Turbo-Dauer")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
         "Spieler %u Tastenbelegung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
         "Eingabegerät-Autoconfig-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
         "Joypad-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET,
         "Tastatur-Overlay-Voreinstellung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
         "Chinesisch (Vereinfacht)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
         "Chinesisch (Traditionell)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
         "Niederländisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
         "Englisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
         "Esperanto")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
         "Französisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
         "Deutsch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
         "Italienisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
         "Japanisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
         "Koreanisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE,
         "Portugiesisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
         "Russisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
         "Spanisch")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
         "Linker Analogstick")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
         "Core-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
         "Core-Info-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
         "Core-Logging-Stufe")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LINEAR,
         "Linear")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
         "Lade Archiv")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT,
         "Lade Content") /* FIXME */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
         "Lade Content (Verlauf)") /* FIXME/UPDATE */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
         "Lade Content")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_STATE,
         "Lade Savestate")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
         "Erlaube Standort-Lokalisierung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
         "Standort-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
         "Logging-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
         "Log-Ausführlichkeit")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MAIN_MENU,
         "Hauptmenü")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MANAGEMENT,
         "Datenbank-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
         "Menü-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
         "Menü-Dateibrowser-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
         "Menü-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
         "Menühintergrund")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MISSING,
         "Fehlt")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
         "Maus-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
         "Media-Player-Einstellungen") /* UPDATE/FIXME */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
         "Musik")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
         "Bekannte Dateiendungen filtern") /* TODO/FIXME - rewrite */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
         "Navigation umbrechen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NEAREST,
         "Nächster")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT, /* TODO, Original string changed */
         "Tausche Netplay-Eingabe")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
         "Verzögere Netplay-Frames")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
         "Aktiviere Netplay")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS, /* TODO, Original string changed */
         "IP-Addresse für Netplay")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
         "Aktiviere Netplay-Client")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
         "Benutzername")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
         "Aktiviere Netplay-Zuschauermodus")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
         "TCP/UDP-Port für Netplay")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
         "Netzwerk-Befehle")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
         "Port für Netzwerk-Befehle")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
         "Netzwerk-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO,
         "Nein")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NONE,
         "Keins")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
         "Nicht verfügbar")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE,
         "Kein Core")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
         "Kein Core verfügbar.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
         "Keine Core-Informationen verfügbar.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
         "Keine Core-Optionen verfügbar.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
         "Keine Informationen verfügbar.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ITEMS,
         "Keine Einträge.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
         "Keine Leistungszähler.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
         "Keine Wiedergabelisten-Eintrage verfügbar.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
         "Keine Einstellungen gefunden.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
         "Keine Shaderparameter")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OFF, /* Don't change. Needed for XMB atm. (Would be, "AN") */
         "OFF")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ON, /* Don't change. Needed for XMB atm. (Would be, "AUS") */
         "ON")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
         "Online-Aktualisierungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
         "OSD-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
         "Öffne Archiv")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OPTIONAL,
         "Optional")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
         "OSK-Overlay-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
         "Overlay-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
         "Overlay-Transparenz")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
         "Overlay-Voreinstellung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
         "Overlay-Skalierung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
         "Overlay-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
         "Verwende PAL60-Modus")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
         "Pausiere, wenn das Menü aktiv ist")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
         "Nicht im Hintergrund laufen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
         "Leistungsindikatoren")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
         "Wiedergabelisten-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
         "Wiedergabelisten-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
         "Touch-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PORT,
         "Port")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PRESENT,
         "Vorhanden")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
         "Privatsphäre-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
         "RetroArch beenden")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32,
         "CRC32")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
         "Beschreibung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
         "Entwickler")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
         "Franchise")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5,
         "MD5")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
         "Name")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
         "Herkunft")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
         "Publisher")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
         "Veröffentlichungsmonat")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
         "Veröffentlichungsjahr")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
         "SHA1")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
         "Starte Content")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_REBOOT,
         "Neustart")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
         "Aufnahme-Konfigurationsverzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
         "Aufnahme-Ausgabeverzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
         "Aufnahme-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
         "Aufnahme-Konfiguration")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
         "Aufnahme-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
         "Aktiviere Aufnahmefunktion")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_PATH, /* FIXME/UPDATE */
         "Aufnahmepfad")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
         "Verwende Aufnahme-Ausgabeverzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
         "Lade Remap-Datei")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
         "Speichere Core-Remap-Datei")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
         "Speichere Spiel-Remap-Datei")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_REQUIRED,
         "Notwendig")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
         "Starte neu")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
         "Starte RetroArch neu")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RESUME,
         "Fortsetzen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
         "Fortsetzen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
         "RetroKeyboard")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROPAD,
         "RetroPad")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
         "Zurückspulen (Rewind) aktivieren")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
         "Genauigkeit des Zurückspulens (Rewind)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
         "Zurückspul-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
         "Browser-Directory")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
         "Konfigurations-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
         "Zeige Startbildschirm")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
         "Rechter Analogstick")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN,
         "Start")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
         "Spielstand-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
         "Automatische Indexierung von Save States")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
         "Automatisches Laden von Save States")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
         "Automatische Save States")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
         "Savestate-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
         "Speichere neue Konfiguration")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_STATE,
         "Speichere Savestate")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
         "Spielstand-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
         "Durchsuche Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_FILE,
         "Durchsuche Datei")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
         "<- Durchsuche ->")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
         "Bildschirmfoto-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
         "Bildschirmauflösung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SECONDS,
         "Sekunden")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SETTINGS,
         "Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER,
         "Shader")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
         "Änderungen übernehmen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
         "Shaders")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
         "Zeige erweitere Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
         "Zeige versteckte Ordner und Dateien")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
         "Zeitlupen-Verhältnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
         "Sortiere Speicherdaten per Ordner")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
         "Sortiere Save States per Ordner")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_STATUS,
         "Status")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
         "stdin-Befehle")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
         "Bildschirmschoner aussetzen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
         "Aktiviere System-BGM")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
         "System/BIOS-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
         "Systeminformationen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
         "7zip-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
         "ALSA-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
         "Build-Datum")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
         "Cg-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
         "Cocoa-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
         "Befehlsinterface-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
         "CoreText-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
         "CPU-Eigenschaften")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
         "Bildschirm-DPI")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
         "Bildschirmhöhe (mm)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
         "Bildschirmbreite (mm)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
         "DirectSound-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
         "Dynamic-Library-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
         "EGL-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
         "Unterstützung für OpenGL/Direct3D Render-to-Texture (Multi-Pass Shader)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
         "FFmpeg-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
         "FreeType-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
         "Frontend-Kennung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
         "Frontend-Name")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
         "Frontend-Betriebssystem")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
         "Git-Version")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
         "GLSL-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
         "HLSL-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
         "JACK-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
         "KMS/EGL-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
         "LibretroDB-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
         "Libusb-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT,
         "Libxml2-XML-Parsing-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
         "Netplay-Unterstützung (Peer-to-Peer)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
         "Netzwerk-Befehlsinterface-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
         "OpenAL-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
         "OpenGL-ES-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
         "OpenGL-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
         "OpenSL-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
         "OpenVG-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
         "OSS-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
         "Overlay-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
         "Energiequelle")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
         "Geladen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
         "Lädt")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
         "Entlädt")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
         "Keine Quelle")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
         "PulseAudio-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
         "Python-Unterstützung (Script-Unterstützung in Shadern)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
         "RetroRating-Stufe")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
         "RoarAudio-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
         "PNG-Unterstützung (RPNG)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
         "RSound-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
         "SDL2-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
         "SDL-Image-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
         "SDL1.2-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
         "Threading-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
         "Udev-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
         "Video4Linux2-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
         "Video-Context-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
         "Wayland-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
         "X11-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
         "XAudio2-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
         "XVideo-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
         "Zlib-Unterstützung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
         "Bildschirmfoto")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
         "Threaded Data Runloop")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
         "Zeige Uhrzeit / Datum")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_TITLE_COLOR,
         "Menü-Titel-Farbe")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_TRUE,
         "True")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
         "UI-Companion beim Hochfahren starten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
         "Menüleiste")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
         "Komprimiertes Archiv kann nicht gelesen werden.")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UNKNOWN,
         "Unbekannt")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
         "Aktualisiere Assets")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
         "Aktualisiere Autoconfig-Profile")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
         "Aktualisiere CG-Shader")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
         "Aktualisiere Cheats")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
         "Aktualisiere Datenbanken")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
         "Aktualisiere GLSL-Shader")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
         "Aktualisiere Overlays")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_USER,
         "Benutzer")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
         "Benutzeroberflächen-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
         "Sprache")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
         "Benutzer-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
         "Verwende integrierten Player") /* FIXME/UPDATE */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
         "<Diesen Ordner verwenden>")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
         "Erlaube Bildrotation")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
         "Automatisches Bildseitenverhältnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
         "Bildseitenverhältnis-Index")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
         "Setze schwarze Frames ein")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
         "Bildränder (Overscan) zuschneiden (Neustart erforderlich)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
         "Deaktiviere Desktop-Komposition")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
         "Grafiktreiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
         "Videofilter")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
         "Grafikfilter-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
         "Aktiviere Flacker-Filter")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
         "Zeige OSD-Nachrichten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
         "Schriftart der OSD-Nachrichten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
         "Schriftgröße der OSD-Nachrichten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
         "Erzwinge Bildseitenverhältnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
         "Erzwinge Deaktivierung des sRGB FBO")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
         "Bildverzögerung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
         "Verwende Vollbildmodus")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
         "Gamma")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
         "Aktiviere GPU-Aufnahmefunktion")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
         "Aktiviere GPU-Bildschirmfotos")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
         "Synchronisiere GPU und CPU")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
         "Synchronisiere Frames fest mit GPU")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
         "X-Position der OSD-Nachrichten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
         "Y-Position der OSD-Nachrichten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
         "Monitor-Index")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
         "Aktiviere Aufnahme von Post-Filtern")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
         "Bildwiederholrate")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
         "Geschätzte Bildwiederholrate")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
         "Rotation")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
         "Fenterskalierung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
         "Ganzzahlige Bildskalierung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
         "Video-Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
         "Grafikshader-Verzeichnis")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
         "Shader-Durchgänge")  /* FIXME */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
         "Momentane Shaderparameter") /* FIXME/UPDATE */
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
         "Lade Shader-Voreinstellung")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS,
         "Menü Shaderparameter (Menü)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
         "Speichere Shader-Voreinstellung unter...")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
         "HW-Shared-Context aktivieren")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
         "Bilineare Filterung (HW)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
         "Aktiviere Soft-Filter")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
         "VSync-Intervall")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
         "Videos")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
         "Threaded Video")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
         "Bild entflackern")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
         "Bildchirmauflösung Höhe")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
         "Bildchirmauflösung Breite")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
         "Bildchirmauflösung X")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
         "Bildchirmauflösung Y")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
         "Kalibriere VI-Bildbreite")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
         "Vertikale Synchronisation (VSync)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
         "Unechter Vollbild-Modus (Windowed Fullscreen)")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
         "Wlan-Treiber")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
         "Wlan")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
         "Icon Schatten")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHOW_HISTORY,
         "Zeige Verlauf")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHOW_IMAGES,
         "Zeige Bilder")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHOW_MUSIC,
         "Zeige Musik")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHOW_SETTINGS,
         "Zeige Einstellungen")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHOW_VIDEO,
         "Zeige Videos")
      MSG_HASH(MENU_ENUM_LABEL_VALUE_YES,
         "Ja")
      default:
         break;
   }

   return "null";
}

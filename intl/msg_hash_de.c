/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
         snprintf(s, len,
            "Lade Inhalt. \n"
            "Suche nach Inhalt. \n"
            " \n"
            "Um Inhalte zu laden brauchst du\n"
            "einen 'Core'. \n"
            " \n"
            "Um einzustellen wo das Verzeichnis beginnt, \n"
            "setze das   \n"
            "'%s'. \n"
            "Falls diese nicht gesetzt ist, startet \n"
            "die Suche beim obersten Verzeichnis.\n"
            " \n"
            "Beim Durchsuchen werden Inhalte gefiltert. \n"
            "Nur Inhalte mit der Dateiendung, welche \n"
            "mit den ausgewählten Core funktionieren \n"
            "werden angezeigt. \n"
            "Dieser Core wird dann auch für den Inhalt verwendet.",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY)
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
      case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
         snprintf(s, len,
               "%s. \n"
               " \n"
               "Setzt das Startverzeichnis des Dateibrowsers.",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY)
               );
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
      case MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST:
         snprintf(s, len, "Erfolgsliste");
         break;
      case MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE:
         snprintf(s, len, "Erfolgsliste (Hardcore)");
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC:
         {
            /* Work around C89 limitations */
            char u[501];
            const char * t =
                    "RetroArch verwendet eine einzigartige\n"
                            "Art der Synchronisation von Audio/Video.\n"
                            "Diese wird durch die Bildwiederholrate\n"
                            "des Monitors kalibriert.\n"
                            "\n"
                            "Falls du irgenwelches Knistern oder Risse\n"
                            "feststellst, kannst du folgende Möglichkeiten:\n"
                            "\n";
            snprintf(u, sizeof(u),
                           "a) Gehe zu '%s' -> '%s' und aktiviere\n"
                           "'%s'. Die Bildwiederholungsrate spielt\n"
                           "in diesem Modus keine Rolle. \n"
                           "Die Bildwiederholungsrate wird höher sein,\n"
                           "allerdings läuft das Video weniger flüssig.\n"
                           "b) Gehe zu '%s' -> '%s' und beachte\n"
                           "'%s'. Lass es bis 2048 Frames laufen und\n"
                           "bestätige mit 'OK'.\n",
                   msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                   msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                   msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_THREADED),
                   msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                   msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                   msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO)
            );
            strlcpy(s, t, len);
            strlcat(s, u, len);
         }
       break;
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
          snprintf(s, len,
                "Du kannst das virtuelle Gamepad-Overlay\n"
                "unter '%s' -> '%s' ändern."
                " \n"
                "Darin kannst du die Grösse, die Transparenz\n"
                "und vieles mehr anpassen.\n"
                " \n"
                "WICHTIG: Standartmässig, ist das virtuelle\n"
                "Gamepad-Overlay im Menü nicht ersichtlich.\n"
                "Wenn du dies ändern möchtest,\n"
                "kannst du '%s' auf Nein stellen.",
                msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
                msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
          );
          break;
      case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
         snprintf(s, len,
            "Um Inhalte zu suchen, gehe zu '%s' und\n"
            "wähle entweder '%s' oder '%s'.\n"
            " \n"
            "Die Dateien werden mit Einträgen in der\n"
            "Datenbank verglichen.\n"
            "Wenn es einen Treffer gibt, wird der Inhalt\n"
            "zur Sammlung hinzugefügt.\n"
            " \n"
            "Danach kannst du einfach den Inhalt unter\n"
            "'%s' -> '%s' laden,\n"
            "anstatt jedesmal die Datei neu zu suchen.\n"
            " \n"
            "WICHTIG: Inhalte für einige Cores sind zum\n"
            "Teil noch nicht scannbar.",
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST)
         );
         break;
      case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
         snprintf(s, len,
            "Du kannst folgende Steuerelemente mit\n"
            "deinem Controller oder deiner Tastatur verwenden\n"
            "um durch das Menü zu navigieren: \n"
            " \n"
         );
         break;
      case MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY:
         snprintf(s, len, "Übergeordnetes Verzeichnis");
         break;
      case MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE:
         snprintf(s, len, "SAMBA aktivieren");
         break;
      case MENU_ENUM_LABEL_VALUE_SHUTDOWN:
         snprintf(s, len, "Herunterfahren");
         break;
      case MENU_ENUM_LABEL_VALUE_SSH_ENABLE:
         snprintf(s, len, "SSH aktivieren");
         break;
      case MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST:
         snprintf(s, len, "Vorschaubilder aktualisieren");
         break;
      case MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA:
         snprintf(s, len, "Lakka aktualisieren");
         break;
      case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
          snprintf(s, len,
             "RetroArch alleine macht nichts. \n"
             " \n"
             "Damit es etwas tut, musst du \n"
             "ein Programm darin laden. \n"
             "\n"
             "Wir nennen so ein Programm 'Libretro core', \n"
             "oder 'core' als Abkürzung. \n"
             " \n"
             "Um einen Core zu laden, wählen Sie einen \n"
             "unter '%s' aus.\n"
             " \n"
#ifdef HAVE_NETWORKING
             "Du erhälst Cores durch verschiedene Wege: \n"
             "* Herunterladen unter\n"
             "'%s' -> '%s'.\n"
             "* Manuelles hinzufügen nach\n"
             "'%s'.",
             msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST),
             msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
             msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
             msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
             "Du erhälst Cores wenn du diese \n"
             "manuell hinzufügst unter\n"
             "'%s'.",
             msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
          );
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
      #include "msg_hash_de.h"
      default:
         break;
   }

   return "null";
}

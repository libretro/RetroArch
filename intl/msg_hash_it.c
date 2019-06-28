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

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../msg_hash.h"
#include "../configuration.h"

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int menu_hash_get_help_it_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   settings_t      *settings = config_get_ptr();

   switch (msg)
   {
      case MENU_ENUM_LABEL_CORE_LIST:
         snprintf(s, len,
               "Carica Core. \n"
               " \n"
               "Sfoglia per una implementazione per il \n"
               "core libretro. Dove il browser \n"
               "si avvia dipende dal percorso impostato per \n"
               "Core Directory. Se vuoto, si avvierà nella root. \n"
               " \n"
               "Se la Core Directory è una directory, il menù \n"
               "userà quella come cartella principale. Se la Core \n"
               "Directory è un percorso completo, si avvierà \n"
               "nella cartella dove si trova il file.");
         break;
      case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
         snprintf(s, len,
               "Puoi usare i seguenti controlli sotto \n"
               "sia su gamepad che su tastiera\n"
               "per controllare il menù: \n"
               " \n"
               );
         break;
      case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
         snprintf(s, len,
               "Benvenuto a RetroArch\n"
               "\n"
               "Per ulteriori informazioni, vai su Aiuto.\n"
               );
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC:
         {
            /* Work around C89 limitations */
            char u[501];
            char t[501];

            strlcpy(t,
                  "RetroArch si basa su una forma unica di\n"
                  "sincronizzazione audio/video che necessita essere\n"
                  "calibrata rispetto alla frequenza di aggiornamento\n"
                  "del tuo schermo per ottenere le migliori performance.\n"
                  " \n"
                  "Se accadono alcuni crepitii audio o del tearing\n"
                  "video, di solito significa che hai bisogno di\n"
                  "calibrare i settaggi. Alcuni suggerimenti sotto:\n"
                  " \n", sizeof(t));
            snprintf(u, sizeof(u),
                  "a) Vai su '%s' -> '%s', e abilita\n"
                  "'Threaded Video'. La frequenza di aggiornamento non sarà\n"
                  "influenzata in questo modo, il framerate sarà più alto,\n"
                  "ma il video potrebbe risultare meno fluido.\n"
                  "b) Vai su '%s' -> '%s', e guarda su\n"
                  "'%s'. Lascia caricare per\n"
                  "2048 fotogrammi, allora premi 'OK'.",
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO));
            strlcpy(s, t, len);
            strlcat(s, u, len);
         }
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
         snprintf(s, len,
               "Per scansionare il contenuto, vai a '%s' e\n"
               "seleziona '%s' oppure '%s'.\n"
               " \n"
               "I files saranno comparati alle entrate del database.\n"
               "Se c'è un riscontro, sarà aggiunta un'entrata\n"
               "alla playlist.\n"
               " \n"
               "Puoi accedere facilmente a questo contenuto\n"
               "andando su '%s' ->\n"
               "'%s'\n"
               "invece di dover andare attraverso il\n"
               "filebrowser ogni volta.\n"
               " \n"
               "NOTA: Il contenuto per alcuni core potrebbe non essere\n"
               "scansionabile. Gli esempi includono: \n"
               "MAME, FBA, e forse altri core."
               ,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
                  );
         break;
      case MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
         strlcpy(s, "Estraendo, per favore attendi...\n", len);
         break;
      case MENU_ENUM_LABEL_INPUT_DRIVER:
         {
            const char *lbl = settings ? settings->arrays.input_driver : NULL;

            if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
            {
               /* Work around C89 limitations */
               const char * t =
                  "udev Input driver. \n"
                  " \n"
                  "Questo driver può caricare senza X. \n"
                  " \n"
                  "Usa la recente evdev joypad API \n"
                  "per il supporto del joystick. Supporta \n"
                  "hotplugging e force feedback (se \n"
                  "supportato dal dispositivo). \n"
                  " \n";
               const char * u =
                  "Il driver legge gli eventi evdev per il supporto \n"
                  "della tastiera. Supporta anche la callback della tastiera, \n"
                  "mouse e touchpads. \n"
                  " \n"
                  "Come predefinito nella maggior parte delle distribuzioni, i nodi /dev/input \n"
                  "sono only-root (modalità 600). Puoi settare una regola udev \n"
                  "che fa queste accessibili ai non-root.";
               strlcpy(s, t, len);
               strlcat(s, u, len);
            }
            else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
               snprintf(s, len,
                     "linuxraw Input driver. \n"
                     " \n"
                     "questo driver richiede un'attiva TTY. Gli eventi \n"
                     "della tastiera sono letti direttamente dal TTY che \n"
                     "che lo rende più semplice, ma non tanto flessibile quanto udev. \n" "Mouse, ecc., non sono supportati. \n"
                     " \n"
                     "Questo driver usa la più vecchia API per il joystick \n"
                     "(/dev/input/js*).");
            else
               snprintf(s, len,
                     "Driver input.\n"
                     " \n"
                     "Dipende dal driver video, potrebbe \n"
                     "forzare un differente driver input.");
         }
         break;
      case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
         snprintf(s, len,
               "Carica Contenuto. \n"
               "Seleziona per contenuto. \n"
               " \n"
               "Per caricare i giochi, hai bisogno di \n"
               "un 'Core' da usare, e un gioco per quel core.\n"
               " \n"
               "Per controllare dove il menù comincia \n"
               " a selezionare per contenuto, imposta  \n"
               "'File Browser Directory'. \n"
               "Se non impostato, si avvierà nella root. \n"
               " \n"
               "Il browser filtrerà le\n"
               "estensioni per l'ultimo core impostato \n"
               "in 'Carica Core', e userà quel core \n"
               "quando il gioco viene caricato."
               );
         break;
      case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
         snprintf(s, len,
               "Caricando contenuto dalla cronologia. \n"
               " \n"
               "Quando il contenuto è caricato, le combinazioni \n"
               "contenuto e core sono salvati nella cronologia. \n"
               " \n"
               "La cronologia è salvata in un file nella stessa \n"
               "directory come il file di configurazione RetroArch. Se \n"
               "nessun file di configurazione viene caricato all'avvio, la \n"
               "cronologia non sarà salvata o caricata, e non apparirà \n"
               "nel menù principale."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_DRIVER:
         snprintf(s, len,
               "Driver video attuale.");

         if (string_is_equal(settings->arrays.video_driver, "gl"))
         {
            snprintf(s, len,
                  "Diver video OpenGL. \n"
                  " \n"
                  "Questo driver permette ai libretro core GL di \n"
                  "essere usati in aggiunta alle implementazioni \n"
                  "renderizzate via software dei core.\n"
                  " \n"
                  "Le performance per il rendering software e \n"
                  "le implementazioni del libretro core G dipende \n"
                  "dalla tua scheda grafica \n"
                  "sottostante driver GL).");
         }
         else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
         {
            snprintf(s, len,
                  "Driver video SDL 2.\n"
                  " \n"
                  "Questo è un driver video SDL 2 renderizzato \n"
                  "via software.\n"
                  " \n"
                  "Le performance per le implementazioni dei core \n"
                  "renderizzati via software dipende \n"
                  "dall'implementazzione sulla tua piattaforma SDL.");
         }
         else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
         {
            snprintf(s, len,
                  "Driver video SDL.\n"
                  " \n"
                  "Questo è un driver video SDL 1.2 renderizzato \n"
                  "via software.\n"
                  " \n"
                  "Le performance sono considerate quasi ottimali. \n"
                  "Considera di usare questo soltanto come ultima scelta.");
         }
         else if (string_is_equal(settings->arrays.video_driver, "d3d"))
         {
            snprintf(s, len,
                  "Driver video Direct3D. \n"
                  " \n"
                  "Le performance per i core renderizzati via \n"
                  "software dipende dal driver D3D inerente \n"
                  "alla tua scheda video).");
         }
         else if (string_is_equal(settings->arrays.video_driver, "exynos"))
         {
            snprintf(s, len,
                  "Exynos-G2D Video Driver. \n"
                  " \n"
                  "Questo è un driver video Exynos a basso livello. \n"
                  "Usa il blocco G2D nei SoC Samsung Exynos \n"
                  "per operazioni blit. \n"
                  " \n"
                  "Le performance per i core renderizzati via software \n"
                  "dovrebbero essere ottimali.");
         }
         else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
         {
            snprintf(s, len,
                  "Driver video Sunxi-G2D. \n"
                  " \n"
                  "Questo è un driver video Sunxi a basso livello. \n"
                  "Usa il blocco G2D nei Soc Allwinner.");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Plugin audio DSP.\n"
               "Processa l'audio prima di inviarlo \n"
               "al driver."
               );
         break;
      case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
         {
            const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

            if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
               strlcpy(s,
                     "Implementazione SINC in modalità finestra.", len);
            else if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
               strlcpy(s,
                     "Implementazione coseno complesso.", len);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
         snprintf(s, len,
               "Carica Shader Preimpostati. \n"
               " \n"
               " Carica un "
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
               " preimposta direttamente. \n"
               "Il menù degli shader è aggiornato di conseguenza. \n"
               " \n"
               "Se la CGP usa metodi di scala che non sono \n"
               "semplici, (es. scala fonte, stessa scala \n"
               "fattore per X/Y), il fattore di scala mostrato \n"
               "nel menù potrebbe non essere corretto."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
         snprintf(s, len,
               "Scala per questo passaggio. \n"
               " \n"
               "Il fattore di scala accumula, es. 2x \n"
               "per il primo passaggio e 2x per il secondo \n"
               "passaggio darà una scala totale di 4x. \n"
               " \n"
               "Se c'è un fattore di scala per l'ultimo \n"
               "passaggio, il risultato è allungare lo \n"
               "schermo con il filtro specificato in \n"
               "'Filtro Predefinito'. \n"
               " \n"
               "Se 'Non considerare' è impostato, sia la scala \n"
               "1x che allunga a pieno schermo saranno \n"
               "usati a seconda se è o non è l'ultimo \n"
               "passaggio."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
         snprintf(s, len,
               "Passaggi Shader. \n"
               " \n"
               "RetroArch permette di mixare e confrontare vari \n"
               "shaders con i passaggi arbitrari dello shader, con \n"
               "filtri hardware personalizzati e fattori di scala. \n"
               " \n"
               "Questa opzione specifica il numero dei passaggi \n"
               "shader da usare. Se imposti questo a 0, e usi \n"
               "Applica modifiche agli shader, usi uno shader 'vuoto'. \n"
               " \n"
               "L'opzione Filtro Predefinito riguarderà il \n"
               "filtro di allungamento immagine.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Parametri shader. \n"
               " \n"
               "Modifica direttamente l'attuale shader. Non sarà \n"
               "salvato al file preimpostato CGP/GLSLP.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         snprintf(s, len,
               "Parametri Shader Preimpostati. \n"
               " \n"
               "Modifica lo shader preimpostato attualmente nel menù."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
         snprintf(s, len,
               "Percorso allo shader. \n"
               " \n"
               "Tutti gli shaders devono essere dello stesso \n"
               "tipo (es. CG, GLSL or HLSL). \n"
               " \n"
               "Imposta la Directory Shader per stabilire dove \n"
               "il browser comincia a cercare gli \n"
               "shader."
               );
         break;
      case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
         snprintf(s, len,
               "Salva la configurazione sul disco all'uscita.\n"
               "Utile per i menù in quanto i settaggi possono \n"
               "essere modificati. Sovrascrive la configurazione.\n"
               " \n"
               "#include ed i commenti non sono \n"
               "conservati. \n"
               " \n"
               "Per design, il file di configurazione è \n"
               "considerato immutabile in quanto è \n"
               "piacevolmente mantenuto dall'utente, \n"
               "e non dovrebbe essere sovrascritto \n"
               "alle spalle dell'utente."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
               "\nQuesto non è il caso per le \n"
               "console comunque, dove \n"
               "guardare al file di configurazione \n"
               "manualmente non è veramente un'opzione."
#endif
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
         snprintf(s, len,
               "Filtro hardware per questo passaggio. \n"
               " \n"
               "Se 'Non prendere cura' è impostato, allora il \n"
               "'Filtro Predefinito' sarà usato."
               );
         break;
      case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
         snprintf(s, len,
               "Salva automaticamente la SRAM non-volatile \n"
               "ad un itervallo regolare.\n"
               " \n"
               "Questo è disattivato come predefinito a meno che non \n"
               "è impostato diversamente. L'intervallo è misurato in \n"
               "secondi. \n"
               " \n"
               "Il valore 0 disattiva il salvataggio automatico.");
         break;
      case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
         snprintf(s, len,
               "Tipo di dispositivo di input. \n"
               " \n"
               "Sceglie quale tipo di dispositivo usare. Questo è \n"
               "rilevante per il libretro core."
               );
         break;
      case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
         snprintf(s, len,
               "Imposta il livello dei log per i libretro core \n"
               "(GET_LOG_INTERFACE). \n"
               " \n"
               " Se il livello dei log rilasciato da un libretro \n"
               " core è sotto il livello libretro_log, \n"
               " sarà ignorato.\n"
               " \n"
               " DEBUG log sono sempre ignorati a meno che \n"
               " la modalità verbose mode è attivata (--verbose).\n"
               " \n"
               " DEBUG = 0\n"
               " INFO  = 1\n"
               " WARN  = 2\n"
               " ERROR = 3"
               );
         break;
      case MENU_ENUM_LABEL_STATE_SLOT_INCREASE:
      case MENU_ENUM_LABEL_STATE_SLOT_DECREASE:
         snprintf(s, len,
               "Slot dello stato di salvataggio.\n"
               " \n"
               " Con lo slot impostato a 0, il nome dello stato di salvataggio è *.state \n"
               " (o che cosa è stato impostato sulla riga di comando).\n"
               "Quando lo slot è != 0, il percorso sarà (percorso)(d), \n"
               "dove (d) è il numero dello slot.");
         break;
      case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
         snprintf(s, len,
               "Applica le modifiche allo shader. \n"
               " \n"
               "Dopo che modifichi i settaggi dello shader, usa questo per \n"
               "applicare i cambiamenti. \n"
               " \n"
               "Modificare i settaggi dello shader è un \n"
               "operazione costosa quindi deve essere \n"
               "fatta esplicitamente. \n"
               " \n"
               "Quando applichi gli shader, i settaggi del menù \n"
               "degli shader sono salvati ad un file temporaneo (sia \n"
               "menu.cgp che menu.glslp) e caricati. Il file \n"
               "rimane dopo che RetroArch esce. Il file è \n"
               "salvato alla Directory Shader."
               );
         break;
      case MENU_ENUM_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "Attiva menù.");
         break;
      case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "Attiva presa mouse.\n"
               " \n"
               "Quando usi il mouse, RetroArch nasconde il \n"
               "mouse, e tiene il puntatore del mouse dentro \n"
               "la finestra per permettere al relativo input del mouse \n"
               "di funzionare meglio.");
         break;
      case MENU_ENUM_LABEL_DISK_NEXT:
         snprintf(s, len,
               "Spostati tra le immagini del disco. Usa dopo \n"
               "l'espulsione. \n"
               " \n"
               " Completa premendo il tasto espulsione di nuovo.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
         snprintf(s, len,
               "Filtro video basato sulla CPU.");
#else
         snprintf(s, len,
               "Filtro video basato sulla CPU.\n"
               " \n"
               "Percorso di una libreria dinamica.");
#endif
         break;
      case MENU_ENUM_LABEL_AUDIO_DEVICE:
         snprintf(s, len,
               "Escludi il dispositivo audio predefinito \n"
               "che il driver audio usa.\n"
               "Dipende dal driver. E.g.\n"
#ifdef HAVE_ALSA
               " \n"
               "ALSA vuole un dipositivo PCM."
#endif
#ifdef HAVE_OSS
               " \n"
               "OSS vuole un percorso (e.g. /dev/dsp)."
#endif
#ifdef HAVE_JACK
               " \n"
               "JACK vuole i nomi delle porte (e.g. system:playback1\n"
               ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
               " \n"
               "RSound vuole l'indirizzo IP di un \n"
               "server RSound."
#endif
               );
         break;
      case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
         snprintf(s, len,
               "Toggles eject for disks.\n"
               " \n"
               "Used for multiple-disk content.");
         break;
      case MENU_ENUM_LABEL_ENABLE_HOTKEY:
         snprintf(s, len,
               "Enable other hotkeys.\n"
               " \n"
               " If this hotkey is bound to either keyboard, \n"
               "joybutton or joyaxis, all other hotkeys will \n"
               "be disabled unless this hotkey is also held \n"
               "at the same time. \n"
               " \n"
               "This is useful for RETRO_KEYBOARD centric \n"
               "implementations which query a large area of \n"
               "the keyboard, where it is not desirable that \n"
               "hotkeys get in the way.");
         break;
      case MENU_ENUM_LABEL_REWIND_ENABLE:
         snprintf(s, len,
               "Enable rewinding.\n"
               " \n"
               "This will take a performance hit, \n"
               "so it is disabled by default.");
         break;
      case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "Core Directory. \n"
               " \n"
               "A directory for where to search for \n"
               "libretro core implementations.");
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         snprintf(s, len,
               "Refresh Rate Auto.\n"
               " \n"
               "The accurate refresh rate of our monitor (Hz).\n"
               "This is used to calculate audio input rate with \n"
               "the formula: \n"
               " \n"
               "audio_input_rate = game input rate * display \n"
               "refresh rate / game refresh rate\n"
               " \n"
               "If the implementation does not report any \n"
               "values, NTSC defaults will be assumed for \n"
               "compatibility.\n"
               " \n"
               "This value should stay close to 60Hz to avoid \n"
               "large pitch changes. If your monitor does \n"
               "not run at 60Hz, or something close to it, \n"
               "disable VSync, and leave this at its default.");
         break;
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         snprintf(s, len,
               "Forces a certain rotation \n"
               "of the screen.\n"
               " \n"
               "The rotation is added to rotations which\n"
               "the libretro core sets (see Video Allow\n"
               "Rotate).");
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         snprintf(s, len,
               "Fullscreen resolution.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
         break;
      case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
         snprintf(s, len,
               "Fastforward ratio."
               " \n"
               "The maximum rate at which content will\n"
               "be run when using fast forward.\n"
               " \n"
               " (E.g. 5.0 for 60 fps content => 300 fps \n"
               "cap).\n"
               " \n"
               "RetroArch will go to sleep to ensure that \n"
               "the maximum rate will not be exceeded.\n"
               "Do not rely on this cap to be perfectly \n"
               "accurate.");
         break;
      case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
         snprintf(s, len,
               "Which monitor to prefer.\n"
               " \n"
               "0 (default) means no particular monitor \n"
               "is preferred, 1 and up (1 being first \n"
               "monitor), suggests RetroArch to use that \n"
               "particular monitor.");
         break;
      case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
         snprintf(s, len,
               "Forces cropping of overscanned \n"
               "frames.\n"
               " \n"
               "Exact behavior of this option is \n"
               "core-implementation specific.");
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
         snprintf(s, len,
               "Only scales video in integer \n"
               "steps.\n"
               " \n"
               "The base size depends on system-reported \n"
               "geometry and aspect ratio.\n"
               " \n"
               "If Force Aspect is not set, X/Y will be \n"
               "integer scaled independently.");
         break;
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         snprintf(s, len,
               "Audio volume, expressed in dB.\n"
               " \n"
               " 0 dB is normal volume. No gain will be applied.\n"
               "Gain can be controlled in runtime with Input\n"
               "Volume Up / Input Volume Down.");
         break;
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
         snprintf(s, len,
               "Audio rate control.\n"
               " \n"
               "Setting this to 0 disables rate control.\n"
               "Any other value controls audio rate control \n"
               "delta.\n"
               " \n"
               "Defines how much input rate can be adjusted \n"
               "dynamically.\n"
               " \n"
               " Input rate is defined as: \n"
               " input rate * (1.0 +/- (rate control delta))");
         break;
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         snprintf(s, len,
               "Maximum audio timing skew.\n"
               " \n"
               "Defines the maximum change in input rate.\n"
               "You may want to increase this to enable\n"
               "very large changes in timing, for example\n"
               "running PAL cores on NTSC displays, at the\n"
               "cost of inaccurate audio pitch.\n"
               " \n"
               " Input rate is defined as: \n"
               " input rate * (1.0 +/- (max timing skew))");
         break;
      case MENU_ENUM_LABEL_OVERLAY_NEXT:
         snprintf(s, len,
               "Toggles to next overlay.\n"
               " \n"
               "Wraps around.");
         break;
      case MENU_ENUM_LABEL_LOG_VERBOSITY:
         snprintf(s, len,
               "Enable or disable verbosity level \n"
               "of frontend.");
         break;
      case MENU_ENUM_LABEL_VOLUME_UP:
         snprintf(s, len,
               "Increases audio volume.");
         break;
      case MENU_ENUM_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Decreases audio volume.");
         break;
      case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Forcibly disable composition.\n"
               "Only valid on Windows Vista/7 for now.");
         break;
      case MENU_ENUM_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "Enable or disable frontend \n"
               "performance counters.");
         break;
      case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "System Directory. \n"
               " \n"
               "Sets the 'system' directory.\n"
               "Cores can query for this\n"
               "directory to load BIOSes, \n"
               "system-specific configs, etc.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
         snprintf(s, len,
               "Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically load any savestate\n"
               "with this path on startup if 'Auto Load State\n"
               "is enabled.");
         break;
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         snprintf(s, len,
               "Use threaded video driver.\n"
               " \n"
               "Using this might improve performance at \n"
               "possible cost of latency and more video \n"
               "stuttering.");
         break;
      case MENU_ENUM_LABEL_VIDEO_VSYNC:
         snprintf(s, len,
               "Video V-Sync.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "Attempts to hard-synchronize \n"
               "CPU and GPU.\n"
               " \n"
               "Can reduce latency at cost of \n"
               "performance.");
         break;
      case MENU_ENUM_LABEL_REWIND_GRANULARITY:
         snprintf(s, len,
               "Rewind granularity.\n"
               " \n"
               " When rewinding defined number of \n"
               "frames, you can rewind several frames \n"
               "at a time, increasing the rewinding \n"
               "speed.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT:
         snprintf(s, len,
               "Take screenshot.");
         break;
      case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
         snprintf(s, len,
               "Sets how many milliseconds to delay\n"
               "after VSync before running the core.\n"
               "\n"
               "Can reduce latency at cost of\n"
               "higher risk of stuttering.\n"
               " \n"
               "Maximum is 15.");
         break;
      case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
         snprintf(s, len,
               "Sets how many frames CPU can \n"
               "run ahead of GPU when using 'GPU \n"
               "Hard Sync'.\n"
               " \n"
               "Maximum is 3.\n"
               " \n"
               " 0: Syncs to GPU immediately.\n"
               " 1: Syncs to previous frame.\n"
               " 2: Etc ...");
         break;
      case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         snprintf(s, len,
               "Inserts a black frame inbetween \n"
               "frames.\n"
               " \n"
               "Useful for 120 Hz monitors who want to \n"
               "play 60 Hz material with eliminated \n"
               "ghosting.\n"
               " \n"
               "Video refresh rate should still be \n"
               "configured as if it is a 60 Hz monitor \n"
               "(divide refresh rate by 2).");
         break;
      case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
         snprintf(s, len,
               "Show startup screen in menu.\n"
               "Is automatically set to false when seen\n"
               "for the first time.\n"
               " \n"
               "This is only updated in config if\n"
               "'Save Configuration on Exit' is enabled.\n");
         break;
      case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len, "Toggles fullscreen.");
         break;
      case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
         snprintf(s, len,
               "Block SRAM from being overwritten \n"
               "when loading save states.\n"
               " \n"
               "Might potentially lead to buggy games.");
         break;
      case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
         snprintf(s, len,
               "Pause gameplay when window focus \n"
               "is lost.");
         break;
      case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
         snprintf(s, len,
               "Screenshots output of GPU shaded \n"
               "material if available.");
         break;
      case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len,
               "Screenshot Directory. \n"
               " \n"
               "Directory to dump screenshots to."
               );
         break;
      case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
         snprintf(s, len,
               "VSync Swap Interval.\n"
               " \n"
               "Uses a custom swap interval for VSync. Set this \n"
               "to effectively halve monitor refresh rate.");
         break;
      case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
         snprintf(s, len,
               "Savefile Directory. \n"
               " \n"
               "Save all save files (*.srm) to this \n"
               "directory. This includes related files like \n"
               ".bsv, .rt, .psrm, etc...\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "Savestate Directory. \n"
               " \n"
               "Save all save states (*.state) to this \n"
               "directory.\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len,
               "Assets Directory. \n"
               " \n"
               " This location is queried by default when \n"
               "menu interfaces try to look for loadable \n"
               "assets, etc.");
         break;
      case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len,
               "Dynamic Wallpapers Directory. \n"
               " \n"
               " The place to store wallpapers that will \n"
               "be loaded dynamically by the menu depending \n"
               "on context.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
         snprintf(s, len,
               "Slowmotion ratio."
               " \n"
               "When slowmotion, content will slow\n"
               "down by factor.");
         break;
      case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
         snprintf(s, len,
               "Turbo period.\n"
               " \n"
               "Describes speed of which turbo-enabled\n"
               "buttons toggle."
               );
         break;
      case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
         snprintf(s, len,
               "Enable input auto-detection.\n"
               " \n"
               "Will attempt to auto-configure \n"
               "joypads, Plug-and-Play style.");
         break;
      case MENU_ENUM_LABEL_CAMERA_ALLOW:
         snprintf(s, len,
               "Allow or disallow camera access by \n"
               "cores.");
         break;
      case MENU_ENUM_LABEL_LOCATION_ALLOW:
         snprintf(s, len,
               "Allow or disallow location services \n"
               "access by cores.");
         break;
      case MENU_ENUM_LABEL_TURBO:
         snprintf(s, len,
               "Turbo enable.\n"
               " \n"
               "Holding the turbo while pressing another \n"
               "button will let the button enter a turbo \n"
               "mode where the button state is modulated \n"
               "with a periodic signal. \n"
               " \n"
               "The modulation stops when the button \n"
               "itself (not turbo button) is released.");
         break;
      case MENU_ENUM_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "Enable/disable on-screen keyboard.");
         break;
      case MENU_ENUM_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Mute/unmute audio.");
         break;
      case MENU_ENUM_LABEL_REWIND:
         snprintf(s, len,
               "Hold button down to rewind.\n"
               " \n"
               "Rewind must be enabled.");
         break;
      case MENU_ENUM_LABEL_EXIT_EMULATOR:
         snprintf(s, len,
               "Key to exit RetroArch cleanly."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
               "\nKilling it in any hard way (SIGKILL, \n"
               "etc) will terminate without saving\n"
               "RAM, etc. On Unix-likes,\n"
               "SIGINT/SIGTERM allows\n"
               "a clean deinitialization."
#endif
               );
         break;
      case MENU_ENUM_LABEL_LOAD_STATE:
         snprintf(s, len,
               "Loads state.");
         break;
      case MENU_ENUM_LABEL_SAVE_STATE:
         snprintf(s, len,
               "Saves state.");
         break;
      case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Decrement cheat index.\n");
         break;
      case MENU_ENUM_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Applies previous shader in directory.");
         break;
      case MENU_ENUM_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Applies next shader in directory.");
         break;
      case MENU_ENUM_LABEL_RESET:
         snprintf(s, len,
               "Reset the content.\n");
         break;
      case MENU_ENUM_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Toggle between paused and non-paused state.");
         break;
      case MENU_ENUM_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Toggle cheat index.\n");
         break;
      case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Hold for fast-forward. Releasing button \n"
               "disables fast-forward.");
         break;
      case MENU_ENUM_LABEL_SLOWMOTION_HOLD:
         snprintf(s, len,
               "Hold for slowmotion.");
         break;
      case MENU_ENUM_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "Frame advance when content is paused.");
         break;
      case MENU_ENUM_LABEL_BSV_RECORD_TOGGLE:
         snprintf(s, len,
               "Toggle between recording and not.");
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
               "Axis for analog stick (DualShock-esque).\n"
               " \n"
               "Bound as usual, however, if a real analog \n"
               "axis is bound, it can be read as a true analog.\n"
               " \n"
               "Positive X axis is right. \n"
               "Positive Y axis is down.");
         break;
      case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
         snprintf(s, len,
               "RetroArch da solo non fa nulla. \n"
               " \n"
               "Per farlo funzionare, hai bisogno di \n"
               "caricare un programma su di esso. \n"
               "\n"
               "Noi chiamiamo tale programma 'Libretro core', \n"
               "o abbreviato 'core'. \n"
               " \n"
               "Per caricare un core, selezionane uno da\n"
               "'Carica Core'.\n"
               " \n"
#ifdef HAVE_NETWORKING
               "Puoi ottenere i core in diversi modi: \n"
               "* Scaricali andando su\n"
               "'%s' -> '%s'.\n"
               "* Manualmente trasferiscili su\n"
               "'%s'.",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
                  "Puoi ottenere i core da\n"
                  "manualmente trasferendoli su\n"
                  "'%s'.",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
                  );
         break;
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
         snprintf(s, len,
               "Puoi cambiare lo schema del gamepad virtuale\n"
               "andando su '%s' \n"
               "-> '%s'."
               " \n"
               "Da lì puoi cambiare lo schema,\n"
               "la dimensione e l'opacità dei tasti, ecc.\n"
               " \n"
               "NOTA: Come predefinito, gli schemi del gamepad virtuale\n"
               "sono nascosti nel menù.\n"
               "Se vorresti cambiare questa impostazione,\n"
               "puoi impostare '%s' a spento/OFF.",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
               );
         break;
      case MSG_UNKNOWN:
      default:
         if (s[0] == '\0')
            strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

const char *msg_hash_to_str_it(enum msg_hash_enums msg)
{
   switch (msg)
   {
#include "msg_hash_it.h"
      default:
         break;
   }

   return "null";
}

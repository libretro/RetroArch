/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
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
extern const char force_iso_8859_1[sizeof("àèéìòù")==6+1 ? 1 : -1];

const char *menu_hash_to_str_it(uint32_t hash)
{

   switch (hash)
   {
      case MENU_LABEL_VALUE_INPUT_ICADE_ENABLE:
         return "Keyboard Gamepad Mapping Enable";
      case MENU_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE:
         return "Keyboard Gamepad Mapping Type";
      case MENU_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE:
         return "Small Keyboard Enable";
      case MENU_LABEL_VALUE_SAVE_CURRENT_CONFIG:
         return "Save Current Config";
      case MENU_LABEL_VALUE_STATE_SLOT:
         return "State Slot";
      case MENU_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS:
         return "Accounts Cheevos";
      case MENU_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME:
         return "Username";
      case MENU_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD:
         return "Password";
      case MENU_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS:
         return "Retro Achievements";
      case MENU_LABEL_VALUE_ACCOUNTS_LIST:
         return "Accounts";
      case MENU_LABEL_VALUE_ACCOUNTS_LIST_END:
         return "Accounts List Endpoint";
      case MENU_LABEL_VALUE_DEBUG_PANEL_ENABLE:
         return "Debug Panel Enable";
      case MENU_LABEL_VALUE_HELP_SCANNING_CONTENT:
         return "Scansiona per contenuto";
	  case MENU_LABEL_VALUE_CHEEVOS_DESCRIPTION:
         return "Descrizione";
      case MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         return "Problemi Audio/Video";
      case MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD:
         return "Cambia i settaggi del gamepad virtuale";
      case MENU_LABEL_VALUE_HELP_WHAT_IS_A_CORE:
         return "Che cosa è un core?";
      case MENU_LABEL_VALUE_HELP_LOADING_CONTENT:
         return "Carica Contenuto";
      case MENU_LABEL_VALUE_HELP_LIST:
         return "Aiuto";
      case MENU_LABEL_VALUE_HELP_CONTROLS:
         return "Menù di base dei controlli";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS:
         return "Menù di base dei controlli";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP:
         return "Scorri verso l'alto";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN:
         return "Scorri verso il basso";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM:
         return "Conferma/OK";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK:
         return "Indietro";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_START:
         return "Predefinito";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO:
         return "Info";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU:
         return "Menù a comparsa";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT:
         return "Esci";
      case MENU_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD:
         return "Tastiera a comparsa";
      case MENU_LABEL_VALUE_OPEN_ARCHIVE:
         return "Apri archivio come cartella";
      case MENU_LABEL_VALUE_LOAD_ARCHIVE:
         return "Carica archivio con il core";
      case MENU_LABEL_VALUE_INPUT_BACK_AS_MENU_TOGGLE_ENABLE:
         return "Indietro quando il menù a comparsa è abilitato";
      case MENU_LABEL_VALUE_INPUT_MENU_TOGGLE_GAMEPAD_COMBO:
         return "Combo gamepad per il menù a comparsa";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU:
         return "Nascondi overlay nel menù";
      case MENU_VALUE_LANG_POLISH:
         return "Polacco";
      case MENU_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED:
         return "Autocarica overlay preferito";
      case MENU_LABEL_VALUE_UPDATE_CORE_INFO_FILES:
         return "Aggiorna i files info dei core";
      case MENU_LABEL_VALUE_DOWNLOAD_CORE_CONTENT:
         return "Contenuto scaricato";
      case MENU_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "<Scansiona questa directory>";
      case MENU_LABEL_VALUE_SCAN_FILE:
         return "Scansiona file";
      case MENU_LABEL_VALUE_SCAN_DIRECTORY:
         return "Scansiona directory";
      case MENU_LABEL_VALUE_ADD_CONTENT_LIST:
         return "Aggiungi Contenuto";
      case MENU_LABEL_VALUE_INFORMATION_LIST:
         return "Informazioni";
      case MENU_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Usa Media Player interno";
      case MENU_LABEL_VALUE_CONTENT_SETTINGS:
         return "Menù rapido";
      case MENU_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Carica Contenuto";
      case MENU_VALUE_ASK_ARCHIVE:
         return "Chiedi";
      case MENU_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Privacy";
      case MENU_VALUE_HORIZONTAL_MENU: /* Don't change. Breaks everything. (Would be: "Menú orizzontale") */
         return "Horizontal Menu";
         /* Don't change these yet. Breaks everything. */
	  case MENU_VALUE_SETTINGS_TAB:
         return "Settaggi scheda";
      case MENU_VALUE_HISTORY_TAB:
         return "Cronologia scheda";
      case MENU_VALUE_ADD_TAB:
         return "Aggiungi scheda";
      case MENU_VALUE_PLAYLISTS_TAB:
         return "Scheda Playlist";
      case MENU_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "Nessun settaggio trovato.";
      case MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "Nessun contatore di performance.";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Driver";
      case MENU_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Configurazione";
      case MENU_LABEL_VALUE_CORE_SETTINGS:
         return "Core";
      case MENU_LABEL_VALUE_VIDEO_SETTINGS:
         return "Video";
      case MENU_LABEL_VALUE_LOGGING_SETTINGS:
         return "Logging";
      case MENU_LABEL_VALUE_SAVING_SETTINGS:
         return "Salvataggi";
      case MENU_LABEL_VALUE_REWIND_SETTINGS:
         return "Riavvolgi";
      case MENU_VALUE_SHADER:
         return "Shader";
      case MENU_VALUE_CHEAT:
         return "Trucchi";
      case MENU_VALUE_USER:
         return "Utente";
      case MENU_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "Abilita musica di sistema";
      case MENU_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_VALUE_RETROKEYBOARD:
         return "RetroTastiera";
      case MENU_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Blocco fotogrammi";
      case MENU_LABEL_VALUE_INPUT_BIND_MODE:
         return "Modalità di collegamento";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Mostra le etichette descrittive degli input del core";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Nascondi descrizione core non caricata";
      case MENU_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Mostra messaggi sullo schermo";
      case MENU_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Carattere per i messaggi sullo schermo";
      case MENU_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Dimensione messaggi sullo schermo";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Posizione X per i messaggi sullo schermo";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Posizione Y per i messaggi sullo schermo";
      case MENU_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Abilita filtro leggero";
      case MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Filtro per il flickering";
      case MENU_VALUE_DIRECTORY_CONTENT:
         return "<Directory contenuto>";
      case MENU_VALUE_UNKNOWN:
         return "Sconosciuto";
      case MENU_VALUE_DONT_CARE:
         return "Non considerare";
      case MENU_VALUE_LINEAR:
         return "Lineare";
      case MENU_VALUE_NEAREST:
         return "Più vicino";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "<Predefinito>";
      case MENU_VALUE_DIRECTORY_NONE:
         return "<Nessuno>";
      case MENU_VALUE_NOT_AVAILABLE:
         return "N/D";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Directory per il remapping dei dispositivi di input";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Directory per l'autoconfigurazione dei dispositivi di input";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Directory della configurazione di registrazione";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Directory output di registrazione";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Directory delle screenshot";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Directory della playlist";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Directory dei file di salvataggio";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Directory degli stati di salvataggio";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Comandi stdin";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Driver Video";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "Abilita registrazione";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Abilita registrazione GPU";
      case MENU_LABEL_VALUE_RECORD_PATH:
         return "File di Output";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Usa Directory Output";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Configura registrazione";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Abilita registrazione post-filtro";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Directory dei download";
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Directory degli asset";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Directory degli sfondi dinamici";
      case MENU_LABEL_VALUE_BOXARTS_DIRECTORY:
         return "Directory copertine";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Directory di selezione file";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Directory di configurazione";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Directory di informazione dei core";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Directory dei core";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Directory cursore";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Directory del database";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "Directory System/BIOS";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Directory Trucchi";
	  case MENU_LABEL_VALUE_CACHE_DIRECTORY:
         return "Directory Cache";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Directory Filtro Audio";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Directory Shader Video";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Directory Filtro Video";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Directory Overlay";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Directory Overlay OSK";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Scambia ingressi in rete";
      case MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Abilita spettatore in rete";
      case MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Indirizzo IP";
      case MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Porta TCP/UDP Rete";
      case MENU_LABEL_VALUE_NETPLAY_ENABLE:
         return "Abilita Rete";
      case MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Mostra fotogrammi in rete";
      case MENU_LABEL_VALUE_NETPLAY_MODE:
         return "Abilita Client di rete";
      case MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Mostra schermata di avvio";
      case MENU_LABEL_VALUE_TITLE_COLOR:
         return "Colore dei titoli dei menù";
      case MENU_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Colore evidenziato delle voci dei menù";
      case MENU_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Mostra ora / data";
      case MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Carica ciclo di dati nei thread";
      case MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Colore normale voce dei menù";
      case MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Mostra settaggi avanzati";
      case MENU_LABEL_VALUE_MOUSE_ENABLE:
         return "Supporto mouse";
      case MENU_LABEL_VALUE_POINTER_ENABLE:
         return "Supporto touch";
      case MENU_LABEL_VALUE_CORE_ENABLE:
         return "Mostra nome dei core";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Abilita DPI Override";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "DPI Override";
      case MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Spegni salvaschermo";
      case MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Disattiva composizione desktop";
      case MENU_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Non caricare in background";
      case MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "Avvia UI Companion all'avvio";
      case MENU_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Barra dei menù";
      case MENU_LABEL_VALUE_ARCHIVE_MODE:
         return "Azione per associare i tipi di archivio";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Comandi di rete";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Porta dei comandi di rete";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Abilita cronologia";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Dimensione cronologia";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Fotogrammi stimati del monitor";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Valore fittizio sull'arresto del core";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "Non avviare automaticamente un core";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limita la velocità massima di caricamento";
      case MENU_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Velocità massima di caricamento";
      case MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Carica file di rimappatura automaticamente";
      case MENU_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Slow-Motion Ratio";
      case MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configurazione per core";
      case MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Carica file di override automaticamente";
      case MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Salva configurazione all'uscita";
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Filtro bilineare hardware";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma video";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Permetti rotazione";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Sincronizza GPU";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "Intervallo di swap vsync";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "VSync";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Threaded Video";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotazione";
      case MENU_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Abiita Screenshot GPU";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Riduci Overscan (Riavvia)";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Indice di aspect ratio";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Aspect ratio automatico";
      case MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Forza aspect ratio";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Frequenza di aggiornamento";
      case MENU_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Forza-disattiva sRGB FBO";
      case MENU_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Modalità schermo intero con finestra";
      case MENU_LABEL_VALUE_PAL60_ENABLE:
         return "Usa modalità PAL60";
      case MENU_LABEL_VALUE_VIDEO_VFILTER:
         return "Deflicker";
      case MENU_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Imposta la larghezza dello schermo";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Inserimento cornice nera";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Sincronizza fotogrammi GPU";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Ordina i salvataggi nelle cartelle";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Ordina gli stati di salvataggio nelle cartelle";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Usa modalità a schermo intero";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Scala a finestra";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Scala a numero intero";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Contatore performance";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Livello dei log del core";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Dettaglio dei log";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Carica automaticamente gli stati di salvataggio";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Cataloga automaticamente gli stati di salvataggio";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Salva stato automaticamente";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Intervallo di autosalvataggio SaveRAM";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Non sovrascrivere SaveRAM al caricamento degli stati di salvataggio";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Abilita contesto condiviso HW";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Riavvia RetroArch";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nome utente";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Linguaggio";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Consenti fotocamera";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Consenti posizionamento";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "In pausa quando il menù è attivato";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Mostra Overlay Tastiera";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Mostra Overlay";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Indice del monitor";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Ritarda fotogramma";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Ciclo dati";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Modalità Turbo";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Soglia Input Axis";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Abilita rimappatura dei controlli";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Utenti massimi";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Abilita autoconfigurazione";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Frequenza audio output (KHz)";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Variazione massima di sincronia dell'audio";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Trucchi usati";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Salva rimappatura file del core";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Salva rimappatura file di gioco";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Applica i cambiamenti nei trucchi";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Applica i cambiamenti negli shader";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Abilita riavvolgi";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Seleziona dalla collezione";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Seleziona il file ed intercetta il core";
      case MENU_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "Seleziona file scaricati ed intercetta il core";
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Carica Recenti";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Abilita audio";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Mostra frequenza dei fotogrammi";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Silenzia audio";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Livello volume audio (dB)";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Abilita sincro audio";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Delta di controllo frequenza audio";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Passaggi dello shader";
      case MENU_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Carica Configurazione";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Livello della funzione riavvolgi";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Carica file di rimappatura";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Frequenza personalizzata";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Usa questo directory>";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Avvia contenuto";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Opzioni disco";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Opzioni del core";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Opzione dei trucchi per il core";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Carica i trucchi";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Salva i trucchi come";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Contatore dei core";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Cattura Screenshot";
      case MENU_LABEL_VALUE_RESUME:
         return "Riprendi";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Indice del disco";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Contatori Frontend";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Aggiungi immagine disco";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Stato del vassoio del disco";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Nessuna voce della playlist disponibile.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Nessuna informazione sul core disponibile.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Nessuna opzione per il core disponibile.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Nessun core disponibile.";
      case MENU_VALUE_NO_CORE:
         return "Nessun Core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Gestore database";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Gestore cursori";
      case MENU_VALUE_MAIN_MENU: /* Don't change. Breaks everything. (Would be: "Menú principale") */
         return "Main Menu"; 
      case MENU_LABEL_VALUE_SETTINGS:
         return "Settaggi";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Esci da RetroArch";
		 case MENU_LABEL_VALUE_SHUTDOWN:
         return "Spegni";
      case MENU_LABEL_VALUE_HELP:
         return "Aiuto";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Salva Configurazione";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Riavvia contenuto";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Aggiorna i core";
      case MENU_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "Buildbot Cores URL";
      case MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "Buildbot Assets URL";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Navigazione avvolgente";
      case MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filtra con estensioni supportate";
      case MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Estrai automaticamente gli archivi scaricati";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informazione di sistema";
	  case MENU_LABEL_VALUE_DEBUG_INFORMATION:
         return "Informazioni di debug";
	  case MENU_LABEL_VALUE_ACHIEVEMENT_LIST:
         return "Lista Obiettivi";
      case MENU_LABEL_VALUE_ONLINE_UPDATER:
         return "Aggiorna Online";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Informazioni del core";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Directory non trovata.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Nessun oggetto.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Carica Core";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Seleziona contenuto";
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Chiudi contenuto";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Settaggi del database";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Salva stato";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Carica stato";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Riprendi contenuto";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Driver di Input";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Driver Audio";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Driver del Joypad";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Driver di ricampionamento Audio";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Driver di Registrazione";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Driver Menù";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Driver Fotocamera";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Driver di Posizione";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Incapace di leggere i file compressi.";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Scala Overlay";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay Predefinito";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Latenza audio (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Dispositivo audio";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Preimpostato Overlay Tastiera";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Opacità Overlay";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Menù sfondi";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Sfondo dinamico";
      case MENU_LABEL_VALUE_BOXART:
         return "Mostra Copertina";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Opzioni di rimappatura degli input del core";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Opzioni Shader";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Antemprima Parametri Shader";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Parametri shader del menù";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Salva Shader Preimpostati come";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Nessun parametro shader.";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Carica Shader Preimpostati";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtro Video";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Plugin audio DSP";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Avviando il download: ";
      case MENU_VALUE_SECONDS:
         return "secondi";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      case MENU_LABEL_VALUE_UPDATE_ASSETS:
         return "Aggiorna Asset";
      case MENU_LABEL_VALUE_UPDATE_CHEATS:
         return "Aggiorna Trucchi";
      case MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Aggiorna profili di autoconfigurazione";
      case MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES_HID:
         return "Aggiorna i profili di autoconfigurazione (HID)";
      case MENU_LABEL_VALUE_UPDATE_DATABASES:
         return "Aggiorna Database";
      case MENU_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Aggiorna Overlay";
      case MENU_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Aggiorna Cg Shader";
      case MENU_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Aggiorna GLSL Shader";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Nome core";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Etichetta core";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "Nome del sistema";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "Produttore del sitema";
      case MENU_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Categorie";
      case MENU_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Autori";
      case MENU_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Permessi";
      case MENU_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Licenza(e)";
      case MENU_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Estensioni supportate";
      case MENU_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NOTES:
         return "Note del core";
      case MENU_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Data della build";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Versione git";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "Caratteristiche CPU";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Identificatore frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Nome frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "OS Frontend";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "Livello di RetroRating";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Fonte di alimentazione";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "Nessuna fonte";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Caricando";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Caricato";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Scarico";
      case MENU_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Contesto driver video";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Mostra larghezza (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Mostra altezza (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Mostra DPI";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "Supporto LibretroDB";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Supporto overlay";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Supporto interfaccia di comando";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Supporto interfaccia comando di rete";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Supporto Cocoa";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "Supporto PNG (RPNG)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "Supporto SDL1.2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "Supporto SDL2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "Supporto OpenGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "Supporto OpenGL ES";
      case MENU_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Supporto Threading";
      case MENU_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "Supporto KMS/EGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Supporto Udev";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "Supporto OpenVG";
      case MENU_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "Supporto EGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "Supporto X11";
      case MENU_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Supporto Wayland";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "Supporto XVideo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "Supporto ALSA";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "Supporto OSS";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "Supporto OpenAL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "Supporto OpenSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "Supporto RSound";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "Supporto RoarAudio";
      case MENU_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "Supporto JACK";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "Supporto PulseAudio";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "Supporto DirectSound";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "Supporto XAudio2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Supporto Zlib";
      case MENU_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "Supporto 7zip";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Supporto libreria dinamica";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Supporto Cg";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "Supporto GLSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "Supporto HLSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "Supporto analisi XML libxml2 XML";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "Supporto immagine SDL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "Supporto renderizza-a-texture (multi-pass shaders) OpenGL/Direct3D";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "Supporto FFmpeg";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "Supporto CoreText";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "Supporto FreeType";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Supporto Netplay (peer-to-peer) ";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Supporto Python (supporto script in shaders) ";
      case MENU_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Supporto Video4Linux2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Supporto Libusb";
      case MENU_LABEL_VALUE_YES:
         return "Sì";
      case MENU_LABEL_VALUE_NO:
         return "No";
      case MENU_VALUE_BACK:
         return "INDIETRO";
      case MENU_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Risoluzione schermo";
      case MENU_VALUE_DISABLED:
         return "Disattivato";
      case MENU_VALUE_PORT:
         return "Porta";
      case MENU_VALUE_NONE:
         return "Nessuno";
      case MENU_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Sviluppatore";
      case MENU_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Editore";
      case MENU_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Descrizione";
      case MENU_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Nome";
      case MENU_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Origine";
      case MENU_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franchise";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Mese di uscita";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Anno di uscita";
      case MENU_VALUE_TRUE:
         return "Vero";
      case MENU_VALUE_FALSE:
         return "Falso";
      case MENU_VALUE_MISSING:
         return "Mancante";
      case MENU_VALUE_PRESENT:
         return "Presente";
      case MENU_VALUE_OPTIONAL:
         return "Opzionale";
      case MENU_VALUE_REQUIRED:
         return "Richiesto";
      case MENU_VALUE_STATUS:
         return "Stato";
      case MENU_LABEL_VALUE_AUDIO_SETTINGS:
         return "Audio";
      case MENU_LABEL_VALUE_INPUT_SETTINGS:
         return "Input";
      case MENU_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Mostra sullo schermo";
      case MENU_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Overlay sullo schermo";
      case MENU_LABEL_VALUE_MENU_SETTINGS:
         return "Menù";
      case MENU_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Multimedia";
      case MENU_LABEL_VALUE_UI_SETTINGS:
         return "Interfaccia Utente";
      case MENU_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Seleziona file di menù";
      case MENU_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Aggiorna";
      case MENU_LABEL_VALUE_NETWORK_SETTINGS:
         return "Rete";
      case MENU_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Playlist";
      case MENU_LABEL_VALUE_USER_SETTINGS:
         return "Utente";
      case MENU_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Directory";
      case MENU_LABEL_VALUE_RECORDING_SETTINGS:
         return "Registrando";
      case MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "Nessuna informazione disponibile.";
      case MENU_LABEL_VALUE_INPUT_USER_BINDS:
         return "Assegna Ingresso Utente %u";
      case MENU_VALUE_LANG_ENGLISH:
         return "Inglese";
      case MENU_VALUE_LANG_JAPANESE:
         return "Giapponese";
      case MENU_VALUE_LANG_FRENCH:
         return "Francese";
      case MENU_VALUE_LANG_SPANISH:
         return "Spagnolo";
      case MENU_VALUE_LANG_GERMAN:
         return "Tedesco";
      case MENU_VALUE_LANG_ITALIAN:
         return "Italiano";
      case MENU_VALUE_LANG_DUTCH:
         return "Olandese";
      case MENU_VALUE_LANG_PORTUGUESE:
         return "Portoghese";
      case MENU_VALUE_LANG_RUSSIAN:
         return "Russo";
      case MENU_VALUE_LANG_KOREAN:
         return "Coreano";
      case MENU_VALUE_LANG_CHINESE_TRADITIONAL:
         return "Cinese (Tradizionale)";
      case MENU_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "Cinese (Semplificato)";
      case MENU_VALUE_LANG_ESPERANTO:
         return "Esperanto";
      case MENU_VALUE_LEFT_ANALOG:
         return "Analogico sinistro";
      case MENU_VALUE_RIGHT_ANALOG:
         return "Analogico destro";
      case MENU_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Imposta tasti di scelta rapida di input";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Aumenta fotogrammi";
      case MENU_VALUE_SEARCH:
         return "Cerca:";
      case MENU_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "Usa visualizzatore di immagini interno";
      default:
         break;
   }

   return "null";
}

int menu_hash_get_help_it(uint32_t hash, char *s, size_t len)
{
   uint32_t driver_hash = 0;
   settings_t      *settings = config_get_ptr();
   
   /* If this one throws errors, stop sledgehammering square pegs into round holes and */
   /* READ THE COMMENTS at the top of the file. */
   (void)sizeof(force_iso_8859_1);

   switch (hash)
   {
      case MENU_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC:
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
               menu_hash_to_str(MENU_LABEL_VALUE_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO));
            strlcat(s, t, len);
            strlcat(s, u, len);
         }
         break;
      case MENU_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
         snprintf(s, len,
               "Per scansionare il contenuto, vai a '%s' e\n"
               "seleziona '%s' oppure '%s'.\n"
               " \n"
               "I files saranno comparati alle entrate del database.\n"
               "Se c'è un riscontro, sarà aggiunta un'entrata\n"
               "alla collezione.\n"
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
               menu_hash_to_str(MENU_LABEL_VALUE_ADD_CONTENT_LIST),
               menu_hash_to_str(MENU_LABEL_VALUE_SCAN_DIRECTORY),
               menu_hash_to_str(MENU_LABEL_VALUE_SCAN_FILE),
               menu_hash_to_str(MENU_LABEL_VALUE_LOAD_CONTENT_LIST),
               menu_hash_to_str(MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST)
               );
         break;
      case MENU_LABEL_VALUE_MENU_CONTROLS_PROLOG:
         snprintf(s, len,
               "Puoi usare i seguenti controlli sotto \n"
               "sia su gamepad che su tastiera\n"
               "per controllare il menù: \n"
               " \n"
               );
         break;
      case MENU_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
         strlcpy(s, "Estraendo, per favore attendi...\n", len);
         break;
      case MENU_LABEL_WELCOME_TO_RETROARCH:
         snprintf(s, len,
               "Benvenuto a RetroArch\n"
               "\n"
               "Per ulteriori informazioni, vai su Aiuto.\n"
               );
         break;
      case MENU_LABEL_INPUT_DRIVER:
         driver_hash = menu_hash_calculate(settings->input.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_INPUT_DRIVER_UDEV:
               snprintf(s, len,
                     "udev Input driver. \n"
                     " \n"
                     "Questo driver può caricare senza X. \n"
                     " \n"
                     "Usa la recente evdev joypad API \n"
                     "per il supporto del joystick. Supporta \n"
                     "hotplugging e force feedback (se \n"
                     "supportato dal dispositivo). \n"
                     " \n"
                     "Il driver legge gli eventi evdev per il supporto \n"
                     "della tastiera. Supporta anche la callback della tastiera, \n"
                     "mouse e touchpads. \n"
                     " \n"
                     "Come predefinito nella maggior parte delle distribuzioni, i nodi /dev/input \n"
                     "sono only-root (modalità 600). Puoi settare una regola udev \n"
                     "che fa queste accessibili ai non-root."
                     );
               break;
            case MENU_LABEL_INPUT_DRIVER_LINUXRAW:
               snprintf(s, len,
                     "linuxraw Input driver. \n"
                     " \n"
                     "questo driver richiede un'attiva TTY. Gli eventi \n"
                     "della tastiera sono letti direttamente dal TTY che \n"
                     "che lo rende più semplice, ma non tanto flessibile quanto udev. \n" "Mouse, ecc, non sono supportati. \n"
                     " \n"
                     "Questo driver usa la più vecchia API per il joystick \n"
                     "(/dev/input/js*).");
               break;
            default:
               snprintf(s, len,
                     "Driver input.\n"
                     " \n"
                     "Dipende dal driver video, potrebbe \n"
                     "forzare un differente driver input.");
               break;
         }
         break;
      case MENU_LABEL_LOAD_CONTENT:
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
      case MENU_LABEL_CORE_LIST:
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
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
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
      case MENU_LABEL_VIDEO_DRIVER:
         driver_hash = menu_hash_calculate(settings->video.driver);

         switch (driver_hash)
         {
            case MENU_LABEL_VIDEO_DRIVER_GL:
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
               break;
            case MENU_LABEL_VIDEO_DRIVER_SDL2:
               snprintf(s, len,
                     "Driver video SDL 2.\n"
                     " \n"
                     "Questo è un driver video SDL 2 renderizzato \n"
                     "via software.\n"
                     " \n"
                     "Le performance per le implementazioni dei core \n"
                     "renderizzati via software dipende \n"
                     "dall'implementazzione sulla tua piattaforma SDL.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SDL1:
               snprintf(s, len,
                     "Driver video SDL.\n"
                     " \n"
                     "Questo è un driver video SDL 1.2 renderizzato \n"
                     "via software.\n"
                     " \n"
                     "Le performance sono considerate quasi ottimali. \n"
                     "Considera di usare questo soltanto come ultima scelta.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_D3D:
               snprintf(s, len,
                     "Driver video Direct3D. \n"
                     " \n"
                     "Le performance per i core renderizzati via \n"
                     "software dipende dal driver D3D inerente \n"
                     "alla tua scheda video).");
               break;
            case MENU_LABEL_VIDEO_DRIVER_EXYNOS:
               snprintf(s, len,
                     "Exynos-G2D Video Driver. \n"
                     " \n"
                     "This is a low-level Exynos video driver. \n"
                     "Uses the G2D block in Samsung Exynos SoC \n"
                     "for blit operations. \n"
                     " \n"
                     "Performance for software rendered cores \n"
                     "should be optimal.");
               break;
            case MENU_LABEL_VIDEO_DRIVER_SUNXI:
               snprintf(s, len,
                     "Driver video Sunxi-G2D. \n"
                     " \n"
                     "Questo è un driver video Sunxi a bsso livello. \n"
                     "Usa il blocco G2D nei Soc Allwinner.");
               break;
            default:
               snprintf(s, len,
                     "Driver video attuale.");
               break;
         }
         break;
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         snprintf(s, len,
               "Plugin audio DSP.\n"
               "Processa l'audio prima di inviarlo \n"
               "al driver."
               );
         break;
      case MENU_LABEL_AUDIO_RESAMPLER_DRIVER:
         driver_hash = menu_hash_calculate(settings->audio.resampler);

         switch (driver_hash)
         {
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_SINC:
               snprintf(s, len,
                     "Implementazione SINC in modalità finestra.");
               break;
            case MENU_LABEL_AUDIO_RESAMPLER_DRIVER_CC:
               snprintf(s, len,
                     "Implementazione coseno complesso.");
               break;
         }
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET:
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
               "semplici, (i.e. scala fonte, stessa scala \n"
               "fattore per X/Y), il fattore di scala mostrato \n"
               "nel menù potrebbe essere non corretto."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         snprintf(s, len,
               "Scala per questo passaggio. \n"
               " \n"
               "Il fattore di scala accumula, i.e. 2x \n"
               "per il primo passaggio e 2x per il secondo \n"
               "passaggio darà un scala totale di 4x. \n"
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
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
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
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         snprintf(s, len,
               "Parametri shader. \n"
               " \n"
               "Modifica direttamente l'attuale shader. Non sarà \n"
               "salvato al file preimpostato CGP/GLSLP.");
         break;
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         snprintf(s, len,
               "Parametri Shader Preimpostati. \n"
               " \n"
               "Modifica lo shader preimpostato attualmente nel menù."
               );
         break;
      case MENU_LABEL_VIDEO_SHADER_PASS:
         snprintf(s, len,
               "Percorso allo shader. \n"
               " \n"
               "Tutti gli shaders devono essere dello stesso \n"
               "tipo (i.e. CG, GLSL or HLSL). \n"
               " \n"
               "Imposta la Directory Shader per stabilire dove \n"
               "il browser comincia a cercare gli \n"
               "shader."
               );
         break;
      case MENU_LABEL_CONFIG_SAVE_ON_EXIT:
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
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         snprintf(s, len,
               "Filtro hardware per questo passaggio. \n"
               " \n"
               "Se 'Non prendere cura' è impostato, allora il \n"
               "'Filtro Predefinito' sarà usato."
               );
         break;
      case MENU_LABEL_AUTOSAVE_INTERVAL:
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
      case MENU_LABEL_INPUT_BIND_DEVICE_TYPE:
         snprintf(s, len,
               "Tipo di dispositivo di input. \n"
               " \n"
               "Sceglie quale tipo di dispositivo usare. Questo è \n"
               "rilevante per il libretro core."
               );
         break;
      case MENU_LABEL_LIBRETRO_LOG_LEVEL:
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
      case MENU_LABEL_STATE_SLOT_INCREASE:
      case MENU_LABEL_STATE_SLOT_DECREASE:
         snprintf(s, len,
               "Slot dello stato di salvataggio.\n"
               " \n"
               " Con lo slot impostato a 0, il nome dello stato di salvataggio è *.state \n"
               " (o che cosa è stato impostato sulla riga di comando).\n"
               "Quando lo slot è != 0, il percorso sarà (percorso)(d), \n"
               "dove (d) è il numero dello slot.");
         break;
      case MENU_LABEL_SHADER_APPLY_CHANGES:
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
               "rimane do che RetroArch esce. Il file è \n"
               "salvato alla Directory Shader."
               );
         break;
      case MENU_LABEL_INPUT_BIND_DEVICE_ID:
         snprintf(s, len,
               "Dispositivo di input. \n"
               " \n"
               "Scegli quale gamepad usare per l'utente N. \n"
               "Il nome del pad è disponibile."
               );
         break;
      case MENU_LABEL_MENU_TOGGLE:
         snprintf(s, len,
               "Toggles menu.");
         break;
      case MENU_LABEL_GRAB_MOUSE_TOGGLE:
         snprintf(s, len,
               "Toggles mouse grab.\n"
               " \n"
               "When mouse is grabbed, RetroArch hides the \n"
               "mouse, and keeps the mouse pointer inside \n"
               "the window to allow relative mouse input to \n"
               "work better.");
         break;
      case MENU_LABEL_DISK_NEXT:
         snprintf(s, len,
               "Cycles through disk images. Use after \n"
               "ejecting. \n"
               " \n"
               " Complete by toggling eject again.");
         break;
      case MENU_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
         snprintf(s, len,
               "CPU-based video filter.");
#else
         snprintf(s, len,
               "CPU-based video filter.\n"
               " \n"
               "Path to a dynamic library.");
#endif
         break;
      case MENU_LABEL_AUDIO_DEVICE:
         snprintf(s, len,
               "Override the default audio device \n"
               "the audio driver uses.\n"
               "This is driver dependent. E.g.\n"
#ifdef HAVE_ALSA
               " \n"
               "ALSA wants a PCM device."
#endif
#ifdef HAVE_OSS
               " \n"
               "OSS wants a path (e.g. /dev/dsp)."
#endif
#ifdef HAVE_JACK
               " \n"
               "JACK wants portnames (e.g. system:playback1\n"
               ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
               " \n"
               "RSound wants an IP address to an RSound \n"
               "server."
#endif
               );
         break;
      case MENU_LABEL_DISK_EJECT_TOGGLE:
         snprintf(s, len,
               "Toggles eject for disks.\n"
               " \n"
               "Used for multiple-disk content.");
         break;
      case MENU_LABEL_ENABLE_HOTKEY:
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
      case MENU_LABEL_REWIND_ENABLE:
         snprintf(s, len,
               "Enable rewinding.\n"
               " \n"
               "This will take a performance hit, \n"
               "so it is disabled by default.");
         break;
      case MENU_LABEL_LIBRETRO_DIR_PATH:
         snprintf(s, len,
               "Core Directory. \n"
               " \n"
               "A directory for where to search for \n"
               "libretro core implementations.");
         break;
      case MENU_LABEL_VIDEO_REFRESH_RATE_AUTO:
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
      case MENU_LABEL_VIDEO_ROTATION:
         snprintf(s, len,
               "Forces a certain rotation \n"
               "of the screen.\n"
               " \n"
               "The rotation is added to rotations which\n"
               "the libretro core sets (see Video Allow\n"
               "Rotate).");
         break;
      case MENU_LABEL_VIDEO_SCALE:
         snprintf(s, len,
               "Fullscreen resolution.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
         break;
      case MENU_LABEL_FASTFORWARD_RATIO:
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
      case MENU_LABEL_VIDEO_MONITOR_INDEX:
         snprintf(s, len,
               "Which monitor to prefer.\n"
               " \n"
               "0 (default) means no particular monitor \n"
               "is preferred, 1 and up (1 being first \n"
               "monitor), suggests RetroArch to use that \n"
               "particular monitor.");
         break;
      case MENU_LABEL_VIDEO_CROP_OVERSCAN:
         snprintf(s, len,
               "Forces cropping of overscanned \n"
               "frames.\n"
               " \n"
               "Exact behavior of this option is \n"
               "core-implementation specific.");
         break;
      case MENU_LABEL_VIDEO_SCALE_INTEGER:
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
      case MENU_LABEL_AUDIO_VOLUME:
         snprintf(s, len,
               "Audio volume, expressed in dB.\n"
               " \n"
               " 0 dB is normal volume. No gain will be applied.\n"
               "Gain can be controlled in runtime with Input\n"
               "Volume Up / Input Volume Down.");
         break;
      case MENU_LABEL_AUDIO_RATE_CONTROL_DELTA:
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
      case MENU_LABEL_AUDIO_MAX_TIMING_SKEW:
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
      case MENU_LABEL_OVERLAY_NEXT:
         snprintf(s, len,
               "Toggles to next overlay.\n"
               " \n"
               "Wraps around.");
         break;
      case MENU_LABEL_LOG_VERBOSITY:
         snprintf(s, len,
               "Enable or disable verbosity level \n"
               "of frontend.");
         break;
      case MENU_LABEL_VOLUME_UP:
         snprintf(s, len,
               "Increases audio volume.");
         break;
      case MENU_LABEL_VOLUME_DOWN:
         snprintf(s, len,
               "Decreases audio volume.");
         break;
      case MENU_LABEL_VIDEO_DISABLE_COMPOSITION:
         snprintf(s, len,
               "Forcibly disable composition.\n"
               "Only valid on Windows Vista/7 for now.");
         break;
      case MENU_LABEL_PERFCNT_ENABLE:
         snprintf(s, len,
               "Enable or disable frontend \n"
               "performance counters.");
         break;
      case MENU_LABEL_SYSTEM_DIRECTORY:
         snprintf(s, len,
               "System Directory. \n"
               " \n"
               "Sets the 'system' directory.\n"
               "Cores can query for this\n"
               "directory to load BIOSes, \n"
               "system-specific configs, etc.");
         break;
      case MENU_LABEL_SAVESTATE_AUTO_SAVE:
         snprintf(s, len,
               "Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically load any savestate\n"
               "with this path on startup if 'Auto Load State\n"
               "is enabled.");
         break;
      case MENU_LABEL_VIDEO_THREADED:
         snprintf(s, len,
               "Use threaded video driver.\n"
               " \n"
               "Using this might improve performance at \n"
               "possible cost of latency and more video \n"
               "stuttering.");
         break;
      case MENU_LABEL_VIDEO_VSYNC:
         snprintf(s, len,
               "Video V-Sync.\n");
         break;
      case MENU_LABEL_VIDEO_HARD_SYNC:
         snprintf(s, len,
               "Attempts to hard-synchronize \n"
               "CPU and GPU.\n"
               " \n"
               "Can reduce latency at cost of \n"
               "performance.");
         break;
      case MENU_LABEL_REWIND_GRANULARITY:
         snprintf(s, len,
               "Rewind granularity.\n"
               " \n"
               " When rewinding defined number of \n"
               "frames, you can rewind several frames \n"
               "at a time, increasing the rewinding \n"
               "speed.");
         break;
      case MENU_LABEL_SCREENSHOT:
         snprintf(s, len,
               "Take screenshot.");
         break;
      case MENU_LABEL_VIDEO_FRAME_DELAY:
         snprintf(s, len,
               "Sets how many milliseconds to delay\n"
               "after VSync before running the core.\n"
               "\n"
               "Can reduce latency at cost of\n"
               "higher risk of stuttering.\n"
               " \n"
               "Maximum is 15.");
         break;
      case MENU_LABEL_VIDEO_HARD_SYNC_FRAMES:
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
      case MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION:
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
      case MENU_LABEL_RGUI_SHOW_START_SCREEN:
         snprintf(s, len,
               "Show startup screen in menu.\n"
               "Is automatically set to false when seen\n"
               "for the first time.\n"
               " \n"
               "This is only updated in config if\n"
               "'Save Configuration on Exit' is enabled.\n");
         break;
      case MENU_LABEL_CORE_SPECIFIC_CONFIG:
         snprintf(s, len,
               "Load up a specific config file \n"
               "based on the core being used.\n");
         break;
      case MENU_LABEL_VIDEO_FULLSCREEN:
         snprintf(s, len, "Toggles fullscreen.");
         break;
      case MENU_LABEL_BLOCK_SRAM_OVERWRITE:
         snprintf(s, len,
               "Block SRAM from being overwritten \n"
               "when loading save states.\n"
               " \n"
               "Might potentially lead to buggy games.");
         break;
      case MENU_LABEL_PAUSE_NONACTIVE:
         snprintf(s, len,
               "Pause gameplay when window focus \n"
               "is lost.");
         break;
      case MENU_LABEL_VIDEO_GPU_SCREENSHOT:
         snprintf(s, len,
               "Screenshots output of GPU shaded \n"
               "material if available.");
         break;
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
         snprintf(s, len,
               "Screenshot Directory. \n"
               " \n"
               "Directory to dump screenshots to."
               );
         break;
      case MENU_LABEL_VIDEO_SWAP_INTERVAL:
         snprintf(s, len,
               "VSync Swap Interval.\n"
               " \n"
               "Uses a custom swap interval for VSync. Set this \n"
               "to effectively halve monitor refresh rate.");
         break;
      case MENU_LABEL_SAVEFILE_DIRECTORY:
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
      case MENU_LABEL_SAVESTATE_DIRECTORY:
         snprintf(s, len,
               "Savestate Directory. \n"
               " \n"
               "Save all save states (*.state) to this \n"
               "directory.\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
         break;
      case MENU_LABEL_ASSETS_DIRECTORY:
         snprintf(s, len,
               "Assets Directory. \n"
               " \n"
               " This location is queried by default when \n"
               "menu interfaces try to look for loadable \n"
               "assets, etc.");
         break;
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         snprintf(s, len,
               "Dynamic Wallpapers Directory. \n"
               " \n"
               " The place to store wallpapers that will \n"
               "be loaded dynamically by the menu depending \n"
               "on context.");
         break;
      case MENU_LABEL_SLOWMOTION_RATIO:
         snprintf(s, len,
               "Slowmotion ratio."
               " \n"
               "When slowmotion, content will slow\n"
               "down by factor.");
         break;
      case MENU_LABEL_INPUT_AXIS_THRESHOLD:
         snprintf(s, len,
               "Defines axis threshold.\n"
               " \n"
               "How far an axis must be tilted to result\n"
               "in a button press.\n"
               " Possible values are [0.0, 1.0].");
         break;
      case MENU_LABEL_INPUT_TURBO_PERIOD:
         snprintf(s, len, 
               "Turbo period.\n"
               " \n"
               "Describes speed of which turbo-enabled\n"
               "buttons toggle."
               );
         break;
      case MENU_LABEL_INPUT_AUTODETECT_ENABLE:
         snprintf(s, len,
               "Enable input auto-detection.\n"
               " \n"
               "Will attempt to auto-configure \n"
               "joypads, Plug-and-Play style.");
         break;
      case MENU_LABEL_CAMERA_ALLOW:
         snprintf(s, len,
               "Allow or disallow camera access by \n"
               "cores.");
         break;
      case MENU_LABEL_LOCATION_ALLOW:
         snprintf(s, len,
               "Allow or disallow location services \n"
               "access by cores.");
         break;
      case MENU_LABEL_TURBO:
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
      case MENU_LABEL_OSK_ENABLE:
         snprintf(s, len,
               "Enable/disable on-screen keyboard.");
         break;
      case MENU_LABEL_AUDIO_MUTE:
         snprintf(s, len,
               "Mute/unmute audio.");
         break;
      case MENU_LABEL_REWIND:
         snprintf(s, len,
               "Hold button down to rewind.\n"
               " \n"
               "Rewind must be enabled.");
         break;
      case MENU_LABEL_EXIT_EMULATOR:
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
      case MENU_LABEL_LOAD_STATE:
         snprintf(s, len,
               "Loads state.");
         break;
      case MENU_LABEL_SAVE_STATE:
         snprintf(s, len,
               "Saves state.");
         break;
      case MENU_LABEL_NETPLAY_FLIP_PLAYERS:
         snprintf(s, len,
               "Netplay flip users.");
         break;
      case MENU_LABEL_CHEAT_INDEX_PLUS:
         snprintf(s, len,
               "Increment cheat index.\n");
         break;
      case MENU_LABEL_CHEAT_INDEX_MINUS:
         snprintf(s, len,
               "Decrement cheat index.\n");
         break;
      case MENU_LABEL_SHADER_PREV:
         snprintf(s, len,
               "Applies previous shader in directory.");
         break;
      case MENU_LABEL_SHADER_NEXT:
         snprintf(s, len,
               "Applies next shader in directory.");
         break;
      case MENU_LABEL_RESET:
         snprintf(s, len,
               "Reset the content.\n");
         break;
      case MENU_LABEL_PAUSE_TOGGLE:
         snprintf(s, len,
               "Toggle between paused and non-paused state.");
         break;
      case MENU_LABEL_CHEAT_TOGGLE:
         snprintf(s, len,
               "Toggle cheat index.\n");
         break;
      case MENU_LABEL_HOLD_FAST_FORWARD:
         snprintf(s, len,
               "Hold for fast-forward. Releasing button \n"
               "disables fast-forward.");
         break;
      case MENU_LABEL_SLOWMOTION:
         snprintf(s, len,
               "Hold for slowmotion.");
         break;
      case MENU_LABEL_FRAME_ADVANCE:
         snprintf(s, len,
               "Frame advance when content is paused.");
         break;
      case MENU_LABEL_MOVIE_RECORD_TOGGLE:
         snprintf(s, len,
               "Toggle between recording and not.");
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
               "Axis for analog stick (DualShock-esque).\n"
               " \n"
               "Bound as usual, however, if a real analog \n"
               "axis is bound, it can be read as a true analog.\n"
               " \n"
               "Positive X axis is right. \n"
               "Positive Y axis is down.");
         break;
      case MENU_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
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
               menu_hash_to_str(MENU_LABEL_VALUE_ONLINE_UPDATER),
               menu_hash_to_str(MENU_LABEL_VALUE_CORE_UPDATER_LIST),
               menu_hash_to_str(MENU_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
               "Puoi ottenere i core da\n"
               "manualmente trasferendoli su\n"
               "'%s'.",
               menu_hash_to_str(MENU_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
               );
         break;
      case MENU_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
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
               menu_hash_to_str(MENU_LABEL_VALUE_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_OVERLAY_SETTINGS),
               menu_hash_to_str(MENU_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
               );
		 break;
      default:
         if (s[0] == '\0')
            strlcpy(s, menu_hash_to_str(MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
         return -1;
   }

   return 0;
}

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

#include "../msg_hash.h"

/* IMPORTANT:
 * For non-english characters to work without proper unicode support,
 * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
 * If you save this file as UTF-8, you'll break non-english characters
 * (e.g. German "Umlauts" and Portugese diacritics).
 */
/* DO NOT REMOVE THIS. If it causes build failure, it's because you saved the file as UTF-8. Read the above comment. */
extern const char force_iso_8859_1[sizeof("àèéìòù")==6+1 ? 1 : -1];

const char *msg_hash_to_str_it(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE:
         return "Abilita mappatura gamepad tastiera";
      case MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE:
         return "Tipologia di mappatura gamepad tastiera";
      case MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE:
         return "Abilita tastiera ridotta";
      case MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG:
         return "Salva configurazione attuale";
      case MENU_ENUM_LABEL_VALUE_STATE_SLOT:
         return "Slot di stato";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS:
         return "Obiettivi dell'account";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME:
         return "Nome utente";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD:
         return "Password";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS:
         return "Retro Obiettivi";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST:
         return "Account";
      case MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END:
         return "Lista degli account";
      case MENU_ENUM_LABEL_VALUE_DEBUG_PANEL_ENABLE:
         return "Abilita pannello di debug";
      case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT:
         return "Scansiona per contenuto";
	  case MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION:
         return "Descrizione";
      case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         return "Problemi Audio/Video";
      case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD:
         return "Cambia i settaggi del gamepad virtuale";
      case MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE:
         return "Che cosa è un core?";
      case MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT:
         return "Carica Contenuto";
      case MENU_ENUM_LABEL_VALUE_HELP_LIST:
         return "Aiuto";
      case MENU_ENUM_LABEL_VALUE_HELP_CONTROLS:
         return "Menù di base dei controlli";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS:
         return "Menù di base dei controlli";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP:
         return "Scorri verso l'alto";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN:
         return "Scorri verso il basso";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM:
         return "Conferma/OK";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK:
         return "Indietro";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START:
         return "Predefinito";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO:
         return "Info";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU:
         return "Menù a comparsa";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT:
         return "Esci";
      case MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD:
         return "Tastiera a comparsa";
      case MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE:
         return "Apri archivio come cartella";
      case MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE:
         return "Carica archivio con il core";
      case MENU_ENUM_LABEL_VALUE_INPUT_BACK_AS_MENU_TOGGLE_ENABLE:
         return "Indietro quando il menù a comparsa è abilitato";
      case MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
         return "Combo gamepad per il menù a comparsa";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU:
         return "Nascondi overlay nel menù";
      case MENU_ENUM_LABEL_VALUE_LANG_POLISH:
         return "Polacco";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED:
         return "Autocarica overlay preferito";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES:
         return "Aggiorna i files info dei core";
      case MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT:
         return "Contenuto scaricato";
      case MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "<Scansiona questa directory>";
      case MENU_ENUM_LABEL_VALUE_SCAN_FILE:
         return "Scansiona file";
      case MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY:
         return "Scansiona directory";
      case MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST:
         return "Aggiungi Contenuto";
      case MENU_ENUM_LABEL_VALUE_INFORMATION_LIST:
         return "Informazioni";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Usa Media Player interno";
      case MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS:
         return "Menù rapido";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Carica Contenuto";
      case MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE:
         return "Chiedi";
      case MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Privacy";
#if 0
      case MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU:
         return "Menú orizzontale";
#else
      case MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU:
         return "Horizontal Menu";
#endif
#if 0
	  case MENU_ENUM_LABEL_VALUE_SETTINGS_TAB:
         return "Settaggi scheda";
#else
      case MENU_ENUM_LABEL_VALUE_SETTINGS_TAB:
         return "Settings tab";
#endif
#if 0
      case MENU_ENUM_LABEL_VALUE_HISTORY_TAB:
         return "Cronologia scheda";
#else
      case MENU_ENUM_LABEL_VALUE_HISTORY_TAB:
         return "History tab";
#endif
#if 1
      case MENU_ENUM_LABEL_VALUE_ADD_TAB:
         return "Add tab";
#else
      case MENU_ENUM_LABEL_VALUE_ADD_TAB:
         return "Aggiungi scheda";
#endif
#if 0
      case MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB:
         return "Scheda Playlist";
#else
      case MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB:
         return "Playlists tab";
#endif
      case MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "Nessun settaggio trovato.";
      case MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "Nessun contatore di performance.";
      case MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS:
         return "Driver";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Configurazione";
      case MENU_ENUM_LABEL_VALUE_CORE_SETTINGS:
         return "Core";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS:
         return "Video";
      case MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS:
         return "Logging";
      case MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS:
         return "Salvataggi";
      case MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS:
         return "Riavvolgi";
      case MENU_ENUM_LABEL_VALUE_SHADER:
         return "Shader";
      case MENU_ENUM_LABEL_VALUE_CHEAT:
         return "Trucchi";
      case MENU_ENUM_LABEL_VALUE_USER:
         return "Utente";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "Abilita musica di sistema";
      case MENU_ENUM_LABEL_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_ENUM_LABEL_VALUE_RETROKEYBOARD:
         return "RetroTastiera";
      case MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Blocco fotogrammi";
      case MENU_ENUM_LABEL_VALUE_INPUT_BIND_MODE:
         return "Modalità di collegamento";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Mostra le etichette descrittive degli input del core";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Nascondi descrizione core non caricata";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Mostra messaggi sullo schermo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Carattere per i messaggi sullo schermo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Dimensione messaggi sullo schermo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Posizione X per i messaggi sullo schermo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Posizione Y per i messaggi sullo schermo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Abilita filtro leggero";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Filtro per il flickering";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT:
         return "<Directory contenuto>";
      case MENU_ENUM_LABEL_VALUE_UNKNOWN:
         return "Sconosciuto";
      case MENU_ENUM_LABEL_VALUE_DONT_CARE:
         return "Non considerare";
      case MENU_ENUM_LABEL_VALUE_LINEAR:
         return "Lineare";
      case MENU_ENUM_LABEL_VALUE_NEAREST:
         return "Più vicino";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT:
         return "<Predefinito>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE:
         return "<Nessuno>";
      case MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE:
         return "N/D";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Directory per la rimappatura dei dispositivi di input";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Directory per l'autoconfigurazione dei dispositivi di input";
      case MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Directory della configurazione di registrazione";
      case MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Directory output di registrazione";
      case MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Directory delle screenshot";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Directory della playlist";
      case MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Directory dei file di salvataggio";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Directory degli stati di salvataggio";
      case MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Comandi stdin";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER:
         return "Driver Video";
      case MENU_ENUM_LABEL_VALUE_RECORD_ENABLE:
         return "Abilita registrazione";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Abilita registrazione GPU";
      case MENU_ENUM_LABEL_VALUE_RECORD_PATH:
         return "File di Output";
      case MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Usa Directory Output";
      case MENU_ENUM_LABEL_VALUE_RECORD_CONFIG:
         return "Configura registrazione";
      case MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Abilita registrazione post-filtro";
      case MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Directory dei download";
      case MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Directory degli asset";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Directory degli sfondi dinamici";
      case MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Directory di selezione file";
      case MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Directory di configurazione";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Directory di informazione dei core";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Directory dei core";
      case MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Directory cursore";
      case MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Directory del database";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "Directory System/BIOS";
      case MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Directory Trucchi";
	  case MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY:
         return "Directory Cache";
      case MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Directory Filtro Audio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Directory Shader Video";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Directory Filtro Video";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Directory Overlay";
      case MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Directory Overlay OSK";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Scambia ingressi in rete";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Abilita spettatore in rete";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Indirizzo IP";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Porta TCP/UDP Rete";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE:
         return "Abilita Rete";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Mostra fotogrammi in rete";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_MODE:
         return "Abilita Client di rete";
      case MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Mostra schermata di avvio";
      case MENU_ENUM_LABEL_VALUE_TITLE_COLOR:
         return "Colore dei titoli dei menù";
      case MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Colore evidenziato delle voci dei menù";
      case MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Mostra ora / data";
      case MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Carica ciclo di dati nei thread";
      case MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Colore normale voce dei menù";
      case MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Mostra settaggi avanzati";
      case MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE:
         return "Supporto mouse";
      case MENU_ENUM_LABEL_VALUE_POINTER_ENABLE:
         return "Supporto touch";
      case MENU_ENUM_LABEL_VALUE_CORE_ENABLE:
         return "Mostra nome dei core";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Abilita DPI Override";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "DPI Override";
      case MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Spegni salvaschermo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Disattiva composizione desktop";
      case MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Non caricare in background";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "Abilita UI Companion all'avvio";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE:
         return "Abilita UI Companion";
      case MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Barra dei menù";
      case MENU_ENUM_LABEL_VALUE_ARCHIVE_MODE:
         return "Azione per associare i tipi di archivio";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Comandi di rete";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Porta dei comandi di rete";
      case MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Abilita cronologia";
      case MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Dimensione cronologia";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Fotogrammi stimati del monitor";
      case MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Valore fittizio sull'arresto del core";
      case MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE: /* TODO/FIXME */
         return "Non avviare automaticamente un core";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limita la velocità massima di caricamento";
      case MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Velocità massima di caricamento";
      case MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Carica file di rimappatura automaticamente";
      case MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Slow-Motion Ratio";
      case MENU_ENUM_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configurazione per core";
	  case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS:
         return "Usa opzioni core per gioco se disponibili";
	  case MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE:
         return "Crea file opzioni di gioco";
      case MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Carica file di override automaticamente";
      case MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Salva configurazione all'uscita";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH:
         return "Filtro bilineare hardware";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma video";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Permetti rotazione";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Sincronizza GPU";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "Intervallo di swap vsync";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC:
         return "VSync";
      case MENU_ENUM_LABEL_VALUE_VIDEO_THREADED:
         return "Threaded Video";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotazione";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Abilita Screenshot GPU";
      case MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Riduci Overscan (Riavvia)";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Indice di aspect ratio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Aspect ratio automatico";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Forza aspect ratio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Frequenza di aggiornamento";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Forza-disattiva sRGB FBO";
      case MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Modalità schermo intero con finestra";
      case MENU_ENUM_LABEL_VALUE_PAL60_ENABLE:
         return "Usa modalità PAL60";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER:
         return "Deflicker";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Imposta la larghezza dello schermo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Inserimento cornice nera";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Sincronizza fotogrammi GPU";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Ordina i salvataggi nelle cartelle";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Ordina gli stati di salvataggio nelle cartelle";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Usa modalità a schermo intero";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE:
         return "Scala a finestra";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Scala a numero intero";
      case MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE:
         return "Contatore performance";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Livello dei log del core";
      case MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY:
         return "Dettaglio dei log";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Carica automaticamente gli stati di salvataggio";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Cataloga automaticamente gli stati di salvataggio";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Salva stato automaticamente";
      case MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Intervallo di autosalvataggio SaveRAM";
      case MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Non sovrascrivere SaveRAM al caricamento degli stati di salvataggio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Abilita contesto condiviso HW";
      case MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH:
         return "Riavvia RetroArch";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nome utente";
      case MENU_ENUM_LABEL_VALUE_USER_LANGUAGE:
         return "Linguaggio";
      case MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW:
         return "Consenti fotocamera";
      case MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW:
         return "Consenti posizionamento";
      case MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO:
         return "In pausa quando il menù è attivato";
      case MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Mostra Overlay Tastiera";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Mostra Overlay";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Indice del monitor";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Ritarda fotogramma";
      case MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Ciclo dati";
      case MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Modalità Turbo";
      case MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Soglia Input Axis";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Abilita rimappatura dei controlli";
      case MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS:
         return "Utenti massimi";
      case MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Abilita autoconfigurazione";
      case MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Frequenza audio output (KHz)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Variazione massima di sincronia dell'audio";
      case MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Trucchi usati";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Salva rimappatura file del core";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Salva rimappatura file di gioco";
      case MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Applica i cambiamenti nei trucchi";
      case MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Applica i cambiamenti negli shader";
      case MENU_ENUM_LABEL_VALUE_REWIND_ENABLE:
         return "Abilita riavvolgi";
      case MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Seleziona dalla collezione";
      case MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST:
         return "Seleziona il file ed intercetta il core";
      case MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "Seleziona file scaricati ed intercetta il core";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Carica Recenti";
      case MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE:
         return "Abilita audio";
      case MENU_ENUM_LABEL_VALUE_FPS_SHOW:
         return "Mostra frequenza dei fotogrammi";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MUTE:
         return "Silenzia audio";
      case MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME:
         return "Livello volume audio (dB)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SYNC:
         return "Abilita sincro audio";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Delta di controllo frequenza audio";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Passaggi dello shader";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATIONS:
         return "Carica Configurazione";
      case MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY:
         return "Livello della funzione riavvolgi";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Carica file di rimappatura";
      case MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO:
         return "Frequenza personalizzata";
      case MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Usa questa directory>";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Avvia contenuto";
      case MENU_ENUM_LABEL_VALUE_DISK_OPTIONS: /* UPDATE/FIXME */
         return "Opzioni disco";
      case MENU_ENUM_LABEL_VALUE_CORE_OPTIONS:
         return "Opzioni";
      case MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS: /* UPDATE/FIXME */
         return "Opzione dei trucchi per il core";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Carica i trucchi";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Salva i trucchi come";
      case MENU_ENUM_LABEL_VALUE_CORE_COUNTERS:
         return "Contatore dei core";
      case MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Cattura Screenshot";
      case MENU_ENUM_LABEL_VALUE_RESUME:
         return "Riprendi";
      case MENU_ENUM_LABEL_VALUE_DISK_INDEX:
         return "Indice del disco";
      case MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Contatori Frontend";
      case MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Aggiungi immagine disco";
      case MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Stato del vassoio del disco";
      case MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Nessuna voce della playlist disponibile.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Nessuna informazione sul core disponibile.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Nessuna opzione per il core disponibile.";
      case MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Nessun core disponibile.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE:
         return "Nessun Core";
      case MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER:
         return "Gestore database";
      case MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER:
         return "Gestore cursori";
      case MENU_ENUM_LABEL_VALUE_MAIN_MENU:
         return "Menú principale"; 
      case MENU_ENUM_LABEL_VALUE_SETTINGS:
         return "Settaggi";
      case MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH:
         return "Esci da RetroArch";
	  case MENU_ENUM_LABEL_VALUE_SHUTDOWN:
         return "Spegni";
      case MENU_ENUM_LABEL_VALUE_REBOOT:
         return "Riavvia";
      case MENU_ENUM_LABEL_VALUE_HELP:
         return "Aiuto";
      case MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Salva nuova configurazione";
      case MENU_ENUM_LABEL_VALUE_RESTART_CONTENT:
         return "Riavvia";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Aggiorna i core";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "Indirizzo Buildbot Core";
      case MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "Indirizzo Buildbot Assets";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Navigazione avvolgente";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filtra con estensioni supportate";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Estrai automaticamente gli archivi scaricati";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informazione di sistema";
	  case MENU_ENUM_LABEL_VALUE_DEBUG_INFORMATION:
         return "Informazioni di debug";
	  case MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST:
         return "Lista Obiettivi";
      case MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER:
         return "Aggiorna Online";
      case MENU_ENUM_LABEL_VALUE_CORE_INFORMATION:
         return "Informazioni del core";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Directory non trovata.";
      case MENU_ENUM_LABEL_VALUE_NO_ITEMS:
         return "Nessun oggetto.";
      case MENU_ENUM_LABEL_VALUE_CORE_LIST:
         return "Carica Core";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT:
         return "Seleziona contenuto";
      case MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT:
         return "Chiudi";
      case MENU_ENUM_LABEL_VALUE_MANAGEMENT:
         return "Settaggi del database";
      case MENU_ENUM_LABEL_VALUE_SAVE_STATE:
         return "Salva stato";
      case MENU_ENUM_LABEL_VALUE_LOAD_STATE:
         return "Carica stato";
      case MENU_ENUM_LABEL_VALUE_RESUME_CONTENT:
         return "Riprendi";
      case MENU_ENUM_LABEL_VALUE_INPUT_DRIVER:
         return "Driver di Input";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER:
         return "Driver Audio";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER:
         return "Driver del Joypad";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Driver di ricampionamento Audio";
      case MENU_ENUM_LABEL_VALUE_RECORD_DRIVER:
         return "Driver di Registrazione";
      case MENU_ENUM_LABEL_VALUE_MENU_DRIVER:
         return "Driver Menù";
      case MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER:
         return "Driver Fotocamera";
      case MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER:
         return "Driver di Posizione";
      case MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Incapace di leggere i file compressi.";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE:
         return "Scala Overlay";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay Predefinito";
      case MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY:
         return "Latenza audio (ms)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE:
         return "Dispositivo audio";
      case MENU_ENUM_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Preimpostato Overlay Tastiera";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY:
         return "Opacità Overlay";
      case MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER:
         return "Menù sfondi";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Sfondo dinamico";
      case MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Opzioni di rimappatura degli input del core";
      case MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS: /* UPDATE/FIXME */
         return "Shaders";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Antemprima Parametri Shader";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Parametri shader del menù";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Salva Shader Preimpostati come";
      case MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Nessun parametro shader.";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Carica Shader Preimpostati";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER:
         return "Filtro Video";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Plugin audio DSP";
      case MENU_ENUM_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Avviando il download: ";
      case MENU_ENUM_LABEL_VALUE_SECONDS:
         return "secondi";
      case MENU_ENUM_LABEL_VALUE_OFF:
         return "OFF";
      case MENU_ENUM_LABEL_VALUE_ON:
         return "ON";
      case MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS:
         return "Aggiorna Asset";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS:
         return "Aggiorna Trucchi";
      case MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Aggiorna profili di autoconfigurazione";
      case MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES_HID:
         return "Aggiorna i profili di autoconfigurazione (HID)";
      case MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES:
         return "Aggiorna Database";
      case MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Aggiorna Overlay";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Aggiorna Cg Shader";
      case MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Aggiorna GLSL Shader";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Nome core";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Etichetta core";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "Nome del sistema";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "Produttore del sitema";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Categorie";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Autori";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Permessi";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Licenza(e)";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Estensioni supportate";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NOTES:
         return "Note del core";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Data della build";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Versione git";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "Caratteristiche CPU";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Identificatore frontend";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Nome frontend";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "OS Frontend";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "Livello di RetroRating";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Fonte di alimentazione";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "Nessuna fonte";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Caricando";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Caricato";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Scarico";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Contesto driver video";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Mostra larghezza (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Mostra altezza (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Mostra DPI";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "Supporto LibretroDB";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Supporto overlay";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Supporto interfaccia di comando";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Supporto interfaccia comando di rete";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Supporto Cocoa";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "Supporto PNG (RPNG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "Supporto SDL1.2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "Supporto SDL2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "Supporto OpenGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "Supporto OpenGL ES";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Supporto Threading";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "Supporto KMS/EGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Supporto Udev";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "Supporto OpenVG";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "Supporto EGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "Supporto X11";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Supporto Wayland";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "Supporto XVideo";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "Supporto ALSA";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "Supporto OSS";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "Supporto OpenAL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "Supporto OpenSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "Supporto RSound";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "Supporto RoarAudio";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "Supporto JACK";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "Supporto PulseAudio";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "Supporto DirectSound";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "Supporto XAudio2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Supporto Zlib";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "Supporto 7zip";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Supporto libreria dinamica";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Supporto Cg";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "Supporto GLSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "Supporto HLSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "Supporto analisi XML libxml2 XML";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "Supporto immagine SDL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "Supporto renderizza-a-texture (multi-pass shaders) OpenGL/Direct3D";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "Supporto FFmpeg";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "Supporto CoreText";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "Supporto FreeType";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Supporto Netplay (peer-to-peer) ";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Supporto Python (supporto script in shaders) ";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Supporto Video4Linux2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Supporto Libusb";
      case MENU_ENUM_LABEL_VALUE_YES:
         return "Sì";
      case MENU_ENUM_LABEL_VALUE_NO:
         return "No";
      case MENU_ENUM_LABEL_VALUE_BACK:
         return "INDIETRO";
      case MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Risoluzione schermo";
      case MENU_ENUM_LABEL_VALUE_DISABLED:
         return "Disattivato";
      case MENU_ENUM_LABEL_VALUE_PORT:
         return "Porta";
      case MENU_ENUM_LABEL_VALUE_NONE:
         return "Nessuno";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Sviluppatore";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Editore";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Descrizione";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Nome";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Origine";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franchise";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Mese di uscita";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Anno di uscita";
      case MENU_ENUM_LABEL_VALUE_TRUE:
         return "Vero";
      case MENU_ENUM_LABEL_VALUE_FALSE:
         return "Falso";
      case MENU_ENUM_LABEL_VALUE_MISSING:
         return "Mancante";
      case MENU_ENUM_LABEL_VALUE_PRESENT:
         return "Presente";
      case MENU_ENUM_LABEL_VALUE_OPTIONAL:
         return "Opzionale";
      case MENU_ENUM_LABEL_VALUE_REQUIRED:
         return "Richiesto";
      case MENU_ENUM_LABEL_VALUE_STATUS:
         return "Stato";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS:
         return "Audio";
      case MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS:
         return "Input";
      case MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Mostra sullo schermo";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Overlay sullo schermo";
      case MENU_ENUM_LABEL_VALUE_MENU_SETTINGS:
         return "Menù";
      case MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Multimedia";
      case MENU_ENUM_LABEL_VALUE_UI_SETTINGS:
         return "Interfaccia Utente";
      case MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Seleziona file";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Aggiorna";
      case MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS:
         return "Rete";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Playlist";
      case MENU_ENUM_LABEL_VALUE_USER_SETTINGS:
         return "Utente";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Directory";
      case MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS:
         return "Registrazione";
      case MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "Nessuna informazione disponibile.";
      case MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS:
         return "Assegna Ingresso Utente %u";
      case MENU_ENUM_LABEL_VALUE_LANG_ENGLISH:
         return "Inglese";
      case MENU_ENUM_LABEL_VALUE_LANG_JAPANESE:
         return "Giapponese";
      case MENU_ENUM_LABEL_VALUE_LANG_FRENCH:
         return "Francese";
      case MENU_ENUM_LABEL_VALUE_LANG_SPANISH:
         return "Spagnolo";
      case MENU_ENUM_LABEL_VALUE_LANG_GERMAN:
         return "Tedesco";
      case MENU_ENUM_LABEL_VALUE_LANG_ITALIAN:
         return "Italiano";
      case MENU_ENUM_LABEL_VALUE_LANG_DUTCH:
         return "Olandese";
      case MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE:
         return "Portoghese";
      case MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN:
         return "Russo";
      case MENU_ENUM_LABEL_VALUE_LANG_KOREAN:
         return "Coreano";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL:
         return "Cinese (Tradizionale)";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "Cinese (Semplificato)";
      case MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO:
         return "Esperanto";
      case MENU_ENUM_LABEL_VALUE_LEFT_ANALOG:
         return "Analogico sinistro";
      case MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG:
         return "Analogico destro";
      case MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Imposta tasti di scelta rapida di input";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Aumenta fotogrammi";
      case MENU_ENUM_LABEL_VALUE_SEARCH:
         return "Cerca:";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "Usa visualizzatore di immagini interno";
      case MENU_ENUM_LABEL_VALUE_ENABLE:
         return "Attivare";
      case MENU_ENUM_LABEL_VALUE_START_CORE:
         return "Avvia Core";
      case MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR:
         return "Tipo di ritardo";
      default:
         break;
   }

   return "null";
}

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
#include <rhash.h>

#include "menu_hash.h"

#include "../configuration.h"

 /* IMPORTANT:
  * For non-english characters to work without proper unicode support,
  * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
  * If you save this file as UTF-8, you'll break non-english characters
  * (e.g. German "Umlauts" and Portugese diacritics).
 */
extern const char encoding_test[sizeof("ø")==2 ? 1 : -1];

static const char *menu_hash_to_str_spanish(uint32_t hash)
{
   switch (hash)
   {
      default:
         break;
   }

   return "null";
}

static const char *menu_hash_to_str_german(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Bilineare Filterung (HW)";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Erlaube Bildrotation";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Synchronisiere GPU und CPU";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "VSync Intervall";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "Vertikale Synchronisation (VSync)";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Threaded Video";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotation";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Bildränder (Overscan) zuschneiden (Neustart erforderlich)";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Setze schwarze Frames ein";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Sortiere Speicherdaten per Ordner";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Sortiere Save States per Ordner";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Vollbild";
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
         return "Video-Frame-Verzögerung";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Auslastungsgrad";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Turbo-Verzögerung";
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
         return "Lade Content (Historie)";
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
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Konfigurationsdatei";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Genauigkeit des Zurückspulens (Rewind)";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Lade Remap-Datei";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Speichere Remap-Datei";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Erzwinge eigenes Seitenverhältnis";
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
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Bildschirmfoto";
      case MENU_LABEL_VALUE_RESUME:
         return "Zurück";
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
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Aufnahmeeinstellungen";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu"; // Don't change. Breaks everything. (Would be: "Hauptmenü")
      case MENU_LABEL_VALUE_SETTINGS:
         return "Frontend Einstellungen";
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
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Systeminformationen";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Optionen"; /* FIXME */
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Core-Informationen";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Ordner nicht gefunden.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Keine Einträge.";
      case MENU_LABEL_CORE_LIST:
         return "Core-Liste";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Lade Content"; /* FIXME */
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Entlade Core"; /* FIXME */
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Verwaltung";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Speichere Save State";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Lade Save State";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Zurück";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Treibereinstellungen";
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
      case MENU_LABEL_VALUE_BOXART:
         return "Zeige Boxart";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Core-Input-Remapping-Optionen";
      case MENU_LABEL_VALUE_VIDEO_OPTIONS:
         return "Video-Optionen";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Shader-Optionen";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Keine Shaderparameter";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Videofilter";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "DSP-Plugin";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Starte Download: ";
      case MENU_VALUE_OFF:
         return "OFF"; // Don't change. Needed for XMB atm. (Would be: "AN")
      case MENU_VALUE_ON:
         return "ON"; // Don't change. Needed for XMB atm. (Would be: "AUS")
      default:
         break;
   }

   return "null";
}

static const char *menu_hash_to_str_portuguese(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VIDEO_FONT_ENABLE:
         return "video_font_enable";
      case MENU_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Mostrar Mensagem de Tela";
      case MENU_LABEL_VIDEO_FONT_PATH:
         return "video_font_path";
      case MENU_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Fonte da Mensagem de Tela";
      case MENU_LABEL_VIDEO_FONT_SIZE:
         return "video_font_size";
      case MENU_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Tamanho da Mensagem de Tela";
      case MENU_LABEL_VIDEO_MESSAGE_POS_X:
         return "video_message_pos_x";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Posição X da Mensagem de Tela";
      case MENU_LABEL_VIDEO_MESSAGE_POS_Y:
         return "video_message_pos_y";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Posição Y da Mensagem de Tela";
      case MENU_LABEL_VIDEO_SOFT_FILTER:
         return "soft_filter";
      case MENU_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Ativar Filtro de Suavização";
      case MENU_LABEL_VIDEO_FILTER_FLICKER:
         return "video_filter_flicker";
      case MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Filtro de Cintilação";
      case MENU_VALUE_DIRECTORY_CONTENT:
         return "<Dir. de Conteúdo>";
      case MENU_VALUE_UNKNOWN:
         return "Desconhecido";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "<Padrão>";
      case MENU_VALUE_DIRECTORY_NONE:
         return "<Nenhum>";
      case MENU_VALUE_NOT_AVAILABLE:
         return "N/A";
      case MENU_LABEL_INPUT_REMAPPING_DIRECTORY:
         return "input_remapping_directory";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Dir. de Remapeamento de Controladores";
      case MENU_LABEL_JOYPAD_AUTOCONFIG_DIR:
         return "joypad_autoconfig_dir";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Dir. de Config. Automática de Dispositivos de Entrada";
      case MENU_LABEL_RECORDING_CONFIG_DIRECTORY:
         return "recording_config_directory";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Dir. de Config. de Gravação";
      case MENU_LABEL_RECORDING_OUTPUT_DIRECTORY:
         return "recording_output_directory";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Dir. de Gravações";
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
         return "screenshot_directory";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Dir. de Capturas de Telas";
      case MENU_LABEL_PLAYLIST_DIRECTORY:
         return "playlist_directory";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Dir. de Listas de Jogos";
      case MENU_LABEL_SAVEFILE_DIRECTORY:
         return "savefile_directory";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Dir. de Saves";
      case MENU_LABEL_SAVESTATE_DIRECTORY:
         return "savestate_directory";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Dir. de Savestates";
      case MENU_LABEL_STDIN_CMD_ENABLE:
         return "stdin_commands";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Comandos stdin";
      case MENU_LABEL_VIDEO_DRIVER:
         return "video_driver";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Driver de Vídeo";
      case MENU_LABEL_RECORD_ENABLE:
         return "record_enable";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "Ativar Gravação";
      case MENU_LABEL_VIDEO_GPU_RECORD:
         return "video_gpu_record";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Ativar Gravação na GPU";
      case MENU_LABEL_RECORD_PATH:
         return "record_path";
      case MENU_LABEL_VALUE_RECORD_PATH:
         return "Caminho da Gravação";
      case MENU_LABEL_RECORD_USE_OUTPUT_DIRECTORY:
         return "record_use_output_directory";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Usar Dir. de Gravações";
      case MENU_LABEL_RECORD_CONFIG:
         return "record_config";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Configurar Gravação";
      case MENU_LABEL_VIDEO_POST_FILTER_RECORD:
         return "video_post_filter_record";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Ativar Filtro Pós-Gravação";
      case MENU_LABEL_CORE_ASSETS_DIRECTORY:
         return "core_assets_directory";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Dir. de Recursos (Assets) dos Cores";
      case MENU_LABEL_ASSETS_DIRECTORY:
         return "assets_directory";
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Dir. de Recursos (Assets)";
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "dynamic_wallpapers_directory";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Dir. de Papéis de Parede Dinâmicos";
      case MENU_LABEL_BOXARTS_DIRECTORY:
         return "boxarts_directory";
      case MENU_LABEL_VALUE_BOXARTS_DIRECTORY:
         return "Dir. de Boxarts";
      case MENU_LABEL_RGUI_BROWSER_DIRECTORY:
         return "rgui_browser_directory";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Dir. de Navegação";
      case MENU_LABEL_RGUI_CONFIG_DIRECTORY:
         return "rgui_config_directory";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Dir. de Configurações";
      case MENU_LABEL_LIBRETRO_INFO_PATH:
         return "libretro_info_path";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Dir. de Informações de Cores";
      case MENU_LABEL_LIBRETRO_DIR_PATH:
         return "libretro_dir_path";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Dir. de Cores";
      case MENU_LABEL_CURSOR_DIRECTORY:
         return "cursor_directory";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Dir. de Cursores";
      case MENU_LABEL_CONTENT_DATABASE_DIRECTORY:
         return "content_database_path";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Dir. de Base de Dados de Conteúdos";
      case MENU_LABEL_SYSTEM_DIRECTORY:
         return "system_directory";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "Diretório System";
      case MENU_LABEL_EXTRACTION_DIRECTORY:
         return "extraction_directory";
      case MENU_LABEL_CHEAT_DATABASE_PATH:
         return "cheat_database_path";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Dir. de Arquivos Cheat";
      case MENU_LABEL_VALUE_EXTRACTION_DIRECTORY:
         return "Dir. de Descompactação";
      case MENU_LABEL_AUDIO_FILTER_DIR:
         return "audio_filter_dir";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Dir. de Filtros de Áudio";
      case MENU_LABEL_VIDEO_FILTER_DIR:
         return "video_filter_dir";
      case MENU_LABEL_VIDEO_SHADER_DIR:
         return "video_shader_dir";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Dir. de Shaders de Vídeo";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Dir. de Filtros de Vídeo";
      case MENU_LABEL_OVERLAY_DIRECTORY:
         return "overlay_directory";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Dir. de Overlays";
      case MENU_LABEL_OSK_OVERLAY_DIRECTORY:
         return "osk_overlay_directory";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Dir. de Overlay de Teclado";
      case MENU_LABEL_NETPLAY_CLIENT_SWAP_INPUT:
         return "netplay_client_swap_input";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Trocar Controlador de Netplay";
      case MENU_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "netplay_spectator_mode_enable";
      case MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Ativar Espectador de Netplay";
      case MENU_LABEL_NETPLAY_IP_ADDRESS:
         return "netplay_ip_address";
      case MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Endereço IP";
      case MENU_LABEL_NETPLAY_TCP_UDP_PORT:
         return "netplay_tcp_udp_port";
      case MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Portas TCP/UDP de Netplay";
      case MENU_LABEL_NETPLAY_ENABLE:
         return "netplay_enable";
      case MENU_LABEL_VALUE_NETPLAY_ENABLE:
         return "Ativar Netplay";
      case MENU_LABEL_NETPLAY_DELAY_FRAMES:
         return "netplay_delay_frames";
      case MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Quadros de Retardo de Netplay";
      case MENU_LABEL_NETPLAY_MODE:
         return "netplay_mode";
      case MENU_LABEL_VALUE_NETPLAY_MODE:
         return "Ativar Cliente de Netplay";
      case MENU_LABEL_RGUI_SHOW_START_SCREEN:
         return "rgui_show_start_screen";
      case MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Mostrar Tela de Início";
      case MENU_LABEL_TITLE_COLOR:
         return "menu_title_color";
      case MENU_LABEL_VALUE_TITLE_COLOR:
         return "Cor do Menu Título";
      case MENU_LABEL_ENTRY_HOVER_COLOR:
         return "menu_entry_hover_color";
      case MENU_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Cor de Realce do Menu Inicial";
      case MENU_LABEL_TIMEDATE_ENABLE:
         return "menu_timedate_enable";
      case MENU_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Mostrar Hora / Data";
      case MENU_LABEL_THREADED_DATA_RUNLOOP_ENABLE:
         return "threaded_data_runloop_enable";
      case MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Ativar Runloop de Threads de Dados";
      case MENU_LABEL_ENTRY_NORMAL_COLOR:
         return "menu_entry_normal_color";
      case MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Cor do Menu Inicial";
      case MENU_LABEL_SHOW_ADVANCED_SETTINGS:
         return "menu_show_advanced_settings";
      case MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Mostrar Configurações Avançadas";
      case MENU_LABEL_COLLAPSE_SUBGROUPS_ENABLE:
         return "menu_collapse_subgroups_enable";
      case MENU_LABEL_VALUE_COLLAPSE_SUBGROUPS_ENABLE:
         return "Juntar Subgrupos";
      case MENU_LABEL_MOUSE_ENABLE:
         return "menu_mouse_enable";
      case MENU_LABEL_VALUE_MOUSE_ENABLE:
         return "Suporte a Mouse";
      case MENU_LABEL_POINTER_ENABLE:
         return "menu_pointer_enable";
      case MENU_LABEL_VALUE_POINTER_ENABLE:
         return "Suporte a Touch";
      case MENU_LABEL_CORE_ENABLE:
         return "menu_core_enable";
      case MENU_LABEL_VALUE_CORE_ENABLE:
         return "Mostrar Nome dos Cores";
      case MENU_LABEL_DPI_OVERRIDE_ENABLE:
         return "dpi_override_enable";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Ativar Sobreposição de DPI";
      case MENU_LABEL_DPI_OVERRIDE_VALUE:
         return "dpi_override_value";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "Sobreposição de DPI";
      case MENU_LABEL_SUSPEND_SCREENSAVER_ENABLE:
         return "suspend_screensaver_enable";
      case MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Suspender Proteção de Tela";
      case MENU_LABEL_VIDEO_DISABLE_COMPOSITION:
         return "video_disable_composition";
      case MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Desativar Desktop Composition";
      case MENU_LABEL_PAUSE_NONACTIVE:
         return "pause_nonactive";
      case MENU_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Não Rodar em Background";
      case MENU_LABEL_UI_COMPANION_START_ON_BOOT:
         return "ui_companion_start_on_boot";
      case MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "Ativar UI Companion ao Iniciar";
      case MENU_LABEL_UI_MENUBAR_ENABLE:
         return "ui_menubar_enable";
      case MENU_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Barra de Menu (Dica)";
      case MENU_LABEL_ARCHIVE_MODE:
         return "archive_mode";
      case MENU_LABEL_VALUE_ARCHIVE_MODE:
         return "Modo Archive";
      case MENU_LABEL_NETWORK_CMD_ENABLE:
         return "network_cmd_enable";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Comandos de Rede";
      case MENU_LABEL_NETWORK_CMD_PORT:
         return "network_cmd_port";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Porta para Ações de Rede";
      case MENU_LABEL_HISTORY_LIST_ENABLE:
         return "history_list_enable";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Ativar Lista de Histórico";
      case MENU_LABEL_CONTENT_HISTORY_SIZE:
         return "Content History Size";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Tamanho da Lista de Histórico";
      case MENU_LABEL_VIDEO_REFRESH_RATE_AUTO:
         return "video_refresh_rate_auto";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Taxa de Atualização de Quadros Estimada";
      case MENU_LABEL_DUMMY_ON_CORE_SHUTDOWN:
         return "dummy_on_core_shutdown";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Desligar Core Dummy On";
      case MENU_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "core_set_supports_no_content_enable";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "Não Iniciar Cores Automaticamente";
      case MENU_LABEL_FRAME_THROTTLE_SETTINGS:
         return "fastforward_ratio_throttle_enable";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Limitar Velocidade Máxima de Execução";
      case MENU_LABEL_FASTFORWARD_RATIO:
         return "fastforward_ratio";
      case MENU_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Velocidade Máxima de Execução";
      case MENU_LABEL_AUTO_REMAPS_ENABLE:
         return "auto_remaps_enable";
      case MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Carregar Automaticamente Arquivos Remapeados";
      case MENU_LABEL_SLOWMOTION_RATIO:
         return "slowmotion_ratio";
      case MENU_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Taxa de Câmera Lenta";
      case MENU_LABEL_CORE_SPECIFIC_CONFIG:
         return "core_specific_config";
      case MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configuração por Core";
      case MENU_LABEL_AUTO_OVERRIDES_ENABLE:
         return "auto_overrides_enable";
      case MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Carregar Automaticamente Arquivos de Sobreposição";
      case MENU_LABEL_CONFIG_SAVE_ON_EXIT:
         return "config_save_on_exit";
      case MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Salvar Configuração ao Sair";
      case MENU_LABEL_VIDEO_SMOOTH:
         return "video_smooth";
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Filtragem Bilinear por Hardware";
      case MENU_LABEL_VIDEO_GAMMA:
         return "video_gamma";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma de Vídeo";
      case MENU_LABEL_VIDEO_ALLOW_ROTATE:
         return "video_allow_rotate";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Permitir Rotação";
      case MENU_LABEL_VIDEO_HARD_SYNC:
         return "video_hard_sync";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Sincronizar GPU com CPU";
      case MENU_LABEL_VIDEO_SWAP_INTERVAL:
         return "video_swap_interval";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "Intervalo de Permuta do Sincronismo Vertical";
      case MENU_LABEL_VIDEO_VSYNC:
         return "video_vsync";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "Sincronismo Vertical";
      case MENU_LABEL_VIDEO_THREADED:
         return "video_threaded";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Vídeo em Threads";
      case MENU_LABEL_VIDEO_ROTATION:
         return "video_rotation";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotação";
      case MENU_LABEL_VIDEO_GPU_SCREENSHOT:
         return "video_gpu_screenshot";
      case MENU_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Ativar Captura de Tela via GPU";
      case MENU_LABEL_VIDEO_CROP_OVERSCAN:
         return "video_crop_overscan";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Descartar Overscan (Recarregue)";
      case MENU_LABEL_VIDEO_ASPECT_RATIO_INDEX:
         return "aspect_ratio_index";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Índice de Proporções de Tela";
      case MENU_LABEL_VIDEO_ASPECT_RATIO_AUTO:
         return "video_aspect_ratio_auto";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Proporção de Tela Automática";
      case MENU_LABEL_VIDEO_FORCE_ASPECT:
         return "video_force_aspect";
      case MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Forçar Proporção de Tela";
      case MENU_LABEL_VIDEO_REFRESH_RATE:
         return "video_refresh_rate";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Taxa de Atualização de Tela";
      case MENU_LABEL_VIDEO_FORCE_SRGB_DISABLE:
         return "video_force_srgb_disable";
      case MENU_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Forcar Desativação de sRGB FBO";
      case MENU_LABEL_VIDEO_WINDOWED_FULLSCREEN:
         return "video_windowed_fullscreen";
      case MENU_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Modo Tela Cheia em Janela";
      case MENU_LABEL_PAL60_ENABLE:
         return "pal60_enable";
      case MENU_LABEL_VALUE_PAL60_ENABLE:
         return "Usar Modo PAL60";
      case MENU_LABEL_VIDEO_VFILTER:
         return "video_vfilter";
      case MENU_LABEL_VALUE_VIDEO_VFILTER:
         return "Eliminar Cintilação";
      case MENU_LABEL_VIDEO_VI_WIDTH:
         return "video_vi_width";
      case MENU_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Fixar Largura de Tela VI";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Inserção de Quadro Negro";
      case MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         return "video_black_frame_insertion";
      case MENU_LABEL_VIDEO_HARD_SYNC_FRAMES:
         return "video_hard_sync_frames";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Quadros de Sincronização entre GPU e CPU";
      case MENU_LABEL_SORT_SAVEFILES_ENABLE:
         return "sort_savefiles_enable";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Ordenar Saves em Pastas";
      case MENU_LABEL_SORT_SAVESTATES_ENABLE:
         return "sort_savestates_enable";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Ordenar Savestates em Pastas";
      case MENU_LABEL_VIDEO_FULLSCREEN:
         return "video_fullscreen";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Usar Modo Tela Cheia";
      case MENU_LABEL_PERFCNT_ENABLE:
         return "perfcnt_enable";
      case MENU_LABEL_VIDEO_SCALE:
         return "video_scale";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Interpolação em Janela";
      case MENU_LABEL_VIDEO_SCALE_INTEGER:
         return "video_scale_integer";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Interpolação em Múltiplos Inteiros";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Contadores de Desempenho";
      case MENU_LABEL_LIBRETRO_LOG_LEVEL:
         return "libretro_log_level";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Nível de Registro de Core";
      case MENU_LABEL_LOG_VERBOSITY:
         return "log_verbosity";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Detalhamento de Registro";
      case MENU_LABEL_SAVESTATE_AUTO_SAVE:
         return "savestate_auto_save";
      case MENU_LABEL_SAVESTATE_AUTO_LOAD:
         return "savestate_auto_load";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Carregar Savestate Automaticamente";
      case MENU_LABEL_SAVESTATE_AUTO_INDEX:
         return "savestate_auto_index";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Índice Automático de Savestates";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Savestate Automático";
      case MENU_LABEL_AUTOSAVE_INTERVAL:
         return "autosave_interval";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Intervalo de Gravação Automática de SaveRAM";
      case MENU_LABEL_BLOCK_SRAM_OVERWRITE:
         return "block_sram_overwrite";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Não Sobrescrever SaveRAM ao Carregar Savestate";
      case MENU_LABEL_VIDEO_SHARED_CONTEXT:
         return "video_shared_context";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Ativar Contexto Compartilhado de Hardware";
      case MENU_LABEL_RESTART_RETROARCH:
         return "restart_retroarch";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Reiniciar RetroArch";
      case MENU_LABEL_NETPLAY_NICKNAME:
         return "netplay_nickname";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nome de Usuário";
      case MENU_LABEL_USER_LANGUAGE:
         return "user_language";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Idioma";
      case MENU_LABEL_CAMERA_ALLOW:
         return "camera_allow";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Permitir Câmera";
      case MENU_LABEL_LOCATION_ALLOW:
         return "location_allow";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Permitir Localização";
      case MENU_LABEL_PAUSE_LIBRETRO:
         return "menu_pause_libretro";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pausar Quando o Menu for Ativado";
      case MENU_LABEL_INPUT_OSK_OVERLAY_ENABLE:
         return "input_osk_overlay_enable";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Mostrar Overlay de Teclado";
      case MENU_LABEL_INPUT_OVERLAY_ENABLE:
         return "input_overlay_enable";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Mostrar Overlay";
      case MENU_LABEL_VIDEO_MONITOR_INDEX:
         return "video_monitor_index";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Índice de Monitores";
      case MENU_LABEL_VIDEO_FRAME_DELAY:
         return "video_frame_delay";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Retardo de Quadro";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Ciclo de Trabalho";
      case MENU_LABEL_INPUT_DUTY_CYCLE:
         return "input_duty_cycle";
      case MENU_LABEL_INPUT_TURBO_PERIOD:
         return "input_turbo_period";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Período de Turbo";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Limiar de Eixo do Controlador";
      case MENU_LABEL_INPUT_AXIS_THRESHOLD:
         return "input_axis_threshold";
      case MENU_LABEL_INPUT_REMAP_BINDS_ENABLE:
         return "input_remap_binds_enable";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Ativar Remapeamentos";
      case MENU_LABEL_INPUT_MAX_USERS:
         return "input_max_users";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Usuários Máximos";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Ativar Autoconfiguração";
      case MENU_LABEL_INPUT_AUTODETECT_ENABLE:
         return "input_autodetect_enable";
      case MENU_LABEL_AUDIO_OUTPUT_RATE:
         return "audio_output_rate";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Taxa de Amostragem de Áudio (KHz)";
      case MENU_LABEL_AUDIO_MAX_TIMING_SKEW:
         return "audio_max_timing_skew";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Distorção Máxima de Sincronização de Áudio";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Códigos de Cheat";
      case MENU_LABEL_CHEAT_APPLY_CHANGES:
         return "cheat_apply_changes";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Salvar Remapeamento de Core";
      case MENU_LABEL_REMAP_FILE_SAVE_CORE:
         return "remap_file_save_core";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Salvar Remapeamento de Jogo";
      case MENU_LABEL_REMAP_FILE_SAVE_GAME:
         return "remap_file_save_game";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Aplicar Alterações de Cheats";
      case MENU_LABEL_CHEAT_NUM_PASSES:
         return "cheat_num_passes";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Aplicar Alterações de Shaders";
      case MENU_LABEL_SHADER_APPLY_CHANGES:
         return "shader_apply_changes";
      case MENU_LABEL_COLLECTION:
         return "collection";
      case MENU_LABEL_REWIND_ENABLE:
         return "rewind_enable";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Ativar Retrocesso";
      case MENU_LABEL_CONTENT_COLLECTION_LIST:
         return "content_collection_list";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Carregar Conteúdo (Coleção)"; /* FIXME */
      case MENU_LABEL_DETECT_CORE_LIST:
         return "detect_core_list";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Carregar Conteúdo (Detectar Core)"; /* FIXME */
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
         return "history_list";
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Carregar Conteúdo (Histórico)";
      case MENU_LABEL_AUDIO_ENABLE:
         return "audio_enable";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Ativar Áudio";
      case MENU_LABEL_FPS_SHOW:
         return "fps_show";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Mostrar Taxa de Quadros";
      case MENU_LABEL_AUDIO_MUTE:
         return "audio_mute_enable";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Desligar Áudio";
      case MENU_LABEL_VIDEO_SHADER_PASS:
         return "video_shader_pass";
      case MENU_LABEL_AUDIO_VOLUME:
         return "audio_volume";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Volume de Áudio (dB)";
      case MENU_LABEL_AUDIO_SYNC:
         return "audio_sync";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Ativar Sincronismo de Áudio";
      case MENU_LABEL_AUDIO_RATE_CONTROL_DELTA:
         return "audio_rate_control_delta";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Variação Máxima de Taxa de Áudio";
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         return "video_shader_filter_pass";
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         return "video_shader_scale_pass";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Número de Shaders";
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         return "video_shader_num_passes";
      case MENU_LABEL_RDB_ENTRY_DESCRIPTION:
         return "rdb_entry_description";
      case MENU_LABEL_RDB_ENTRY_ORIGIN:
         return "rdb_entry_origin";
      case MENU_LABEL_RDB_ENTRY_PUBLISHER:
         return "rdb_entry_publisher";
      case MENU_LABEL_RDB_ENTRY_DEVELOPER:
         return "rdb_entry_developer";
      case MENU_LABEL_RDB_ENTRY_FRANCHISE:
         return "rdb_entry_franchise";
      case MENU_LABEL_RDB_ENTRY_MAX_USERS:
         return "rdb_entry_max_users";
      case MENU_LABEL_RDB_ENTRY_NAME:
         return "rdb_entry_name";
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         return "rdb_entry_edge_magazine_rating";
      case MENU_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         return "rdb_entry_famitsu_magazine_rating";
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         return "rdb_entry_edge_magazine_issue";
      case MENU_LABEL_RDB_ENTRY_RELEASE_MONTH:
         return "rdb_entry_releasemonth";
      case MENU_LABEL_RDB_ENTRY_RELEASE_YEAR:
         return "rdb_entry_releaseyear";
      case MENU_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
         return "rdb_entry_enhancement_hw";
      case MENU_LABEL_RDB_ENTRY_SHA1:
         return "rdb_entry_sha1";
      case MENU_LABEL_RDB_ENTRY_CRC32:
         return "rdb_entry_crc32";
      case MENU_LABEL_RDB_ENTRY_MD5:
         return "rdb_entry_md5";
      case MENU_LABEL_RDB_ENTRY_BBFC_RATING:
         return "rdb_entry_bbfc_rating";
      case MENU_LABEL_RDB_ENTRY_ESRB_RATING:
         return "rdb_entry_esrb_rating";
      case MENU_LABEL_RDB_ENTRY_ELSPA_RATING:
         return "rdb_entry_elspa_rating";
      case MENU_LABEL_RDB_ENTRY_PEGI_RATING:
         return "rdb_entry_pegi_rating";
      case MENU_LABEL_RDB_ENTRY_CERO_RATING:
         return "rdb_entry_cero_rating";
      case MENU_LABEL_RDB_ENTRY_ANALOG:
         return "rdb_entry_analog";
      case MENU_LABEL_CONFIGURATIONS:
         return "configurations";
      case MENU_LABEL_VALUE_CONFIGURATIONS: /* FIXME */
         return "Arquivos de Configuração";
      case MENU_LABEL_LOAD_OPEN_ZIP:
         return "load_open_zip";
      case MENU_LABEL_REWIND_GRANULARITY:
         return "rewind_granularity";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Granularidade do Retrocesso";
      case MENU_LABEL_REMAP_FILE_LOAD:
         return "remap_file_load";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Carregar Remapeamento";
      case MENU_LABEL_REMAP_FILE_SAVE_AS:
         return "remap_file_save_as";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Salvar Remapeamento Como";
      case MENU_LABEL_CUSTOM_RATIO:
         return "custom_ratio";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Proporção de Tela Personalizada";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Usar este diretório>";
      case MENU_LABEL_USE_THIS_DIRECTORY:
         return "use_this_directory";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Iniciar Conteúdo";
      case MENU_LABEL_RDB_ENTRY_START_CONTENT:
         return "rdb_entry_start_content";
      case MENU_LABEL_CUSTOM_BIND:
         return "custom_bind";
      case MENU_LABEL_CUSTOM_BIND_ALL:
         return "custom_bind_all";
      case MENU_LABEL_DISK_OPTIONS:
         return "core_disk_options";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Opções de Discos do Core";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Opções de Core";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Opções de Cheat do Core";
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
         return "core_cheat_options";
      case MENU_LABEL_CORE_OPTIONS:
         return "core_options";
      case MENU_LABEL_DATABASE_MANAGER_LIST:
         return "database_manager_list";
      case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
         return "deferred_database_manager_list";
      case MENU_LABEL_CURSOR_MANAGER_LIST:
         return "cursor_manager_list";
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
         return "deferred_cursor_manager_list";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Carregar Cheat";
      case MENU_LABEL_CHEAT_FILE_LOAD:
         return "cheat_file_load";
      case MENU_LABEL_CHEAT_FILE_SAVE_AS:
         return "cheat_file_save_as";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Salvar Cheat Como";
      case MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL:
         return "deferred_rdb_entry_detail";
      case MENU_LABEL_FRONTEND_COUNTERS:
         return "frontend_counters";
      case MENU_LABEL_CORE_COUNTERS:
         return "core_counters";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Contadores de Cores";
      case MENU_LABEL_DISK_CYCLE_TRAY_STATUS:
         return "disk_cycle_tray_status";
      case MENU_LABEL_DISK_IMAGE_APPEND:
         return "disk_image_append";
      case MENU_LABEL_DEFERRED_CORE_LIST:
         return "deferred_core_list";
      case MENU_LABEL_DEFERRED_CORE_LIST_SET:
         return "deferred_core_list_set";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Capturar Tela";
      case MENU_LABEL_INFO_SCREEN:
         return "info_screen";
      case MENU_LABEL_VALUE_RESUME:
         return "Retomar";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Índice de Discos";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Contadores de Frontend";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Anexar Imagem de Disco";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Estado do Drive de Disco";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Lista de Jogos Vazia.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Nenhuma Informação de Core Disponível.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Nenhuma Opção de Core Disponível.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Nenhum Core Disponível.";
      case MENU_VALUE_NO_CORE:
         return "Nenhum Core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Gerenciador de Base de Dados";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Gerenciador de Cursores";
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Configurações de Gravação";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_SETTINGS:
         return "frontend_settings";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Frontend Configurações";
      case MENU_LABEL_QUIT_RETROARCH:
         return "quit_retroarch";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Sair do RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Ajuda";
      case MENU_LABEL_HELP:
         return "help";
      case MENU_LABEL_SAVE_NEW_CONFIG:
         return "save_new_config";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Salvar Nova Configuração";
      case MENU_LABEL_RESTART_CONTENT:
         return "restart_content";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Reiniciar Conteúdo";
      case MENU_LABEL_TAKE_SCREENSHOT:
         return "take_screenshot";
      case MENU_LABEL_CORE_UPDATER_LIST:
         return "core_updater_list";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Atualização de Cores";
      case MENU_LABEL_CORE_UPDATER_BUILDBOT_URL:
         return "core_updater_buildbot_url";
      case MENU_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "URL de Cores - Buildbot";
      case MENU_LABEL_BUILDBOT_ASSETS_URL:
         return "buildbot_assets_url";
      case MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "URL de Recursos (Assets) - Buildbot";
      case MENU_LABEL_NAVIGATION_WRAPAROUND_VERTICAL:
         return "menu_navigation_wraparound_vertical_enable";
      case MENU_LABEL_NAVIGATION_WRAPAROUND_HORIZONTAL:
         return "menu_navigation_wraparound_horizontal_enable";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND_HORIZONTAL:
         return "Navegação Horizontal Circular";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND_VERTICAL:
         return "Navegação Vertical Circular";
      case MENU_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "menu_navigation_browser_filter_supported_extensions_enable";
      case MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Navegador - Filtrar por Extensões Suportadas";
      case MENU_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "core_updater_auto_extract_archive";
      case MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Extrair Arquivos Baixados Automaticamente";
      case MENU_LABEL_SYSTEM_INFORMATION:
         return "system_information";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informação de Sistema";
      case MENU_LABEL_OPTIONS:
         return "options";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Opções"; /* FIXME */
      case MENU_LABEL_CORE_INFORMATION:
         return "core_information";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Informação de Core";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Diretório Não Encontrado.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Nenhum Item.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Carregar Core";
      case MENU_LABEL_CORE_LIST:
         return "core_list";
      case MENU_LABEL_LOAD_CONTENT:
         return "load_content_default";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Carregar Conteúdo";  /* FIXME */
      case MENU_LABEL_CLOSE_CONTENT:
         return "unload_core";
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Descarregar Core"; /* FIXME */
      case MENU_LABEL_MANAGEMENT:
         return "database_settings";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Gerenciamento Avançado";
      case MENU_LABEL_PERFORMANCE_COUNTERS:
         return "performance_counters";
      case MENU_LABEL_SAVE_STATE:
         return "savestate";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Salvar Savestate";
      case MENU_LABEL_LOAD_STATE:
         return "loadstate";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Carregar Savestate";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Retomar Conteúdo";
      case MENU_LABEL_RESUME_CONTENT:
         return "resume_content";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Configurações de Drivers";
      case MENU_LABEL_INPUT_DRIVER:
         return "input_driver";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Driver de Controladores";
      case MENU_LABEL_AUDIO_DRIVER:
         return "audio_driver";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Driver de Áudio";
      case MENU_LABEL_JOYPAD_DRIVER:
         return "input_joypad_driver";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Driver de Joypad";
      case MENU_LABEL_AUDIO_RESAMPLER_DRIVER:
         return "audio_resampler_driver";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Driver de Amostragem de Áudio";
      case MENU_LABEL_RECORD_DRIVER:
         return "record_driver";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Driver de Gravação";
      case MENU_LABEL_MENU_DRIVER:
         return "menu_driver";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Driver de Menu";
      case MENU_LABEL_CAMERA_DRIVER:
         return "camera_driver";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Driver de Câmera";
      case MENU_LABEL_LOCATION_DRIVER:
         return "location_driver";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Driver de Localização";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Incapaz de Ler Arquivo Comprimido.";
      case MENU_LABEL_OVERLAY_SCALE:
         return "input_overlay_scale";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Interpolação de Overlay";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Predefinições de Overlay";
      case MENU_LABEL_OVERLAY_PRESET:
         return "input_overlay";
      case MENU_LABEL_KEYBOARD_OVERLAY_PRESET:
         return "input_osk_overlay";
      case MENU_LABEL_AUDIO_DEVICE:
         return "audio_device";
      case MENU_LABEL_AUDIO_LATENCY:
         return "audio_latency";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Latência de Áudio (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Dispositivo de Áudio";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Predefinições de Overlay de Teclado";
      case MENU_LABEL_OVERLAY_OPACITY:
         return "input_overlay_opacity";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Opacidade de Overlay";
      case MENU_LABEL_MENU_WALLPAPER:
         return "menu_wallpaper";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Papel de Parede do Menu";
      case MENU_LABEL_DYNAMIC_WALLPAPER:
         return "menu_dynamic_wallpaper_enable";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Papel de Parede Dinâmico";
      case MENU_LABEL_BOXART:
         return "menu_boxart_enable";
      case MENU_LABEL_VALUE_BOXART:
         return "Mostrar Boxart";
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return "core_input_remapping_options";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Opções de Remapeamento de Controlador de Core";
      case MENU_LABEL_VIDEO_OPTIONS:
         return "video_options";
      case MENU_LABEL_VALUE_VIDEO_OPTIONS:
         return "Opções de Vídeo";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Opções de Shaders";
      case MENU_LABEL_SHADER_OPTIONS:
         return "shader_options";
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         return "video_shader_parameters";
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         return "video_shader_preset_parameters";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Salvar Predefinições de Shader Como";
      case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         return "video_shader_preset_save_as";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Nenhum Parâmetro de Shader Disponível.";
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         return "video_shader_preset";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Carregar Predefinições de Shader";
      case MENU_LABEL_VIDEO_FILTER:
         return "video_filter";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtros de Vídeo";
      case MENU_LABEL_DEFERRED_VIDEO_FILTER:
         return "deferred_video_filter";
      case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
         return "deferred_core_updater_list";
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         return "audio_dsp_plugin";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Plugin de DSP de Áudio";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Iniciando Download: ";
      case MENU_VALUE_SECONDS:
         return "Segundos";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      default:
         break;
   }
 
   return "null";
}

static const char *menu_hash_to_str_italian(uint32_t hash)
{
   switch (hash)
   {
      default:
         break;
   }

   return "null";
}

static const char *menu_hash_to_str_french(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Filtre bilineaire (HW)";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Autoriser la rotation";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Synchroniser le GPU au CPU";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "Intervale de synchro verticale";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "Synchronisation verticale";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Threader l'affichage";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotation";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Tronquer l'overscan (Reload)";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Inserer des images noires";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Classer les sauvegardes par dossier";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Classer les sauvegardes d'état par dossier";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Plein écran";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Zoom (en fenêtre)";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Aligner aux pixels de l'écran";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Compteurs de performance";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Niveau de log des cores";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Logs verbeux";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Auto Load State"; // TODO
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Indice automatique de sauvegarde d'etat";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Sauvegarde automatique";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Intervale de sauvegarde SaveRAM";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Ne pas écraser la SaveRAM en chargeant l'état";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Partager le contexte matériel";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Redémarrer RetroArch";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nom d'utilisateur";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Langue";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Autoriser la caméra";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Autoriser la localisation";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pause quand le menu est activé";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Afficher l'overlay clavier";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Activer les overlays";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Écran";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Delai d'image";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Rapport de cycle";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Delai du turbo";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Seuil des axes";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Autoriser le remapping des entrées";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Nombre d'utilisateurs";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Activer l'autoconfiguration";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Fréquence de sortie (KHz)";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Limite max de l'ajustement";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Nombre de passages";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Charger un fichier remap de core";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Charger un fichier remap de jeu";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Appliquer les changements";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Appliquer les changements";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Activer le rembobinage";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Via les collections";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Via les fichiers + détecter le core";
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Via l'historique";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Activer le son";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Afficher le FPS";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Muet";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Volume sonnore (dB)";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Synchroniser le son";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Delta du taux de contrôle";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Nombre de passages";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Charger un fichier de config";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Précision du rembobinage";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Charger un fichier de remap";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Enregistrer un fichier de remap sous";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Forcer une résolution";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Choisir ce dossier>";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Lancer le contenu";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Options de disques";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Options du core";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Options de triche";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Capturer l écran";
      case MENU_LABEL_VALUE_RESUME:
         return "Reprendre";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Numéro du disque";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Compteurs du Frontend";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Ajouter une image de disque";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "État du lecteur de disque";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Playlist vide.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Pad d'informations disponibles.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Pas d'options disponibles.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Aucun core disponible.";
      case MENU_VALUE_NO_CORE:
         return "Aucun core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Base de données";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Curseurs";
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Réglage de capture video";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Réglages du frontend";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Quitter RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Aide";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Sauvegarder la configuration";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Redémarrer le contenu";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Mise à jour des cores";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informations du système";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Réglages du core";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Informations sur le core";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Dossier non trouvé.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Vide.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Charger un core";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Via les fichiers";
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Quitter le core";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Gestion avancée";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Sauvegarder un état";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Charger un etat";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Reprendre";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Réglage des pilotes";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Impossible de lire l'archive.";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Zoom de l'Overlay";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Préréglages d'Overlay";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Latence audio (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Carte son";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Prereglages d'overlay clavier";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Transparence de l'overlay";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Fond d'écran";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Fond d'écran dynamique";
      case MENU_LABEL_VALUE_BOXART:
         return "Afficher les vignettes";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Options de remap d'entrées du core";
      case MENU_LABEL_VALUE_VIDEO_OPTIONS:
         return "Options vidéo";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Options de shaders";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Aucun paramètres.";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtre vidéo";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Module DSP";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Téléchargement de : ";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      default:
         break;
   }

   return "null";
}

static const char *menu_hash_to_str_dutch(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Core Informatie";
      case MENU_LABEL_CORE_LIST:
         return "Laad Core";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Frontend Instellingen";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Core Instellingen";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Systeem Informatie";
      default:
         break;
   }

   return "null";
}

static const char *menu_hash_to_str_english(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_USE_BUILTIN_PLAYER:
         return "use_builtin_player";
      case MENU_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Use Builtin Player";
      case MENU_LABEL_CONTENT_SETTINGS:
         return "content_settings";
      case MENU_LABEL_VALUE_CONTENT_SETTINGS:
         return "Content Settings";
      case MENU_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_LABEL_LOAD_CONTENT_LIST:
         return "load_content";
      case MENU_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Load Content";
      case MENU_VALUE_LOAD_ARCHIVE:
         return "Load Archive";
      case MENU_VALUE_OPEN_ARCHIVE:
         return "Open Archive";
      case MENU_VALUE_ASK_ARCHIVE:
         return "Ask";
      case MENU_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Privacy Settings";
      case MENU_VALUE_HORIZONTAL_MENU:
         return "Horizontal Menu";
      case MENU_LABEL_NO_SETTINGS_FOUND:
         return "menu_label_no_settings_found";
      case MENU_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "No settings found.";
      case MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "No performance counters.";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Driver Settings";
      case MENU_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Configuration Settings";
      case MENU_LABEL_VALUE_CORE_SETTINGS:
         return "Core Settings";
      case MENU_LABEL_VALUE_VIDEO_SETTINGS:
         return "Video Settings";
      case MENU_LABEL_VALUE_LOGGING_SETTINGS:
         return "Logging Settings";
      case MENU_LABEL_VALUE_SAVING_SETTINGS:
         return "Saving Settings";
      case MENU_LABEL_VALUE_REWIND_SETTINGS:
         return "Rewind Settings";
      case MENU_LABEL_CUSTOM_VIEWPORT_1:
         return "custom_viewport_1";
      case MENU_LABEL_VALUE_CUSTOM_VIEWPORT_1:
         return "Set Upper-Left Corner";
      case MENU_LABEL_CUSTOM_VIEWPORT_2:
         return "custom_viewport_2";
      case MENU_LABEL_VALUE_CUSTOM_VIEWPORT_2:
         return "Set Bottom-Right Corner";
      case MENU_VALUE_SHADER:
         return "Shader";
      case MENU_VALUE_CHEAT:
         return "Cheat";
      case MENU_VALUE_USER:
         return "User";
      case MENU_LABEL_SYSTEM_BGM_ENABLE:
         return "system_bgm_enable";
      case MENU_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "System BGM Enable";
      case MENU_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_LABEL_AUDIO_BLOCK_FRAMES:
         return "audio_block_frames";
      case MENU_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Block Frames";
      case MENU_LABEL_INPUT_BIND_MODE:
         return "input_bind_mode";
      case MENU_LABEL_VALUE_INPUT_BIND_MODE:
         return "Bind Mode";
      case MENU_LABEL_AUTOCONFIG_DESCRIPTOR_LABEL_SHOW:
         return "autoconfig_descriptor_label_show";
      case MENU_LABEL_VALUE_AUTOCONFIG_DESCRIPTOR_LABEL_SHOW:
         return "Display Autoconfig Descriptor Labels";
      case MENU_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "input_descriptor_label_show";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Display Core Input Descriptor Labels";
      case MENU_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "input_descriptor_hide_unbound";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Hide Unbound Core Input Descriptors";
      case MENU_LABEL_VIDEO_FONT_ENABLE:
         return "video_font_enable";
      case MENU_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Display OSD Message";
      case MENU_LABEL_VIDEO_FONT_PATH:
         return "video_font_path";
      case MENU_LABEL_VALUE_VIDEO_FONT_PATH:
         return "OSD Message Font";
      case MENU_LABEL_VIDEO_FONT_SIZE:
         return "video_font_size";
      case MENU_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "OSD Message Size";
      case MENU_LABEL_VIDEO_MESSAGE_POS_X:
         return "video_message_pos_x";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "OSD Message X Position";
      case MENU_LABEL_VIDEO_MESSAGE_POS_Y:
         return "video_message_pos_y";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "OSD Message Y Position";
      case MENU_LABEL_VIDEO_SOFT_FILTER:
         return "soft_filter";
      case MENU_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Soft Filter Enable";
      case MENU_LABEL_VIDEO_FILTER_FLICKER:
         return "video_filter_flicker";
      case MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Flicker filter";
      case MENU_VALUE_DIRECTORY_CONTENT:
         return "<Content dir>";
      case MENU_VALUE_UNKNOWN:
         return "Unknown";
      case MENU_VALUE_DONT_CARE:
         return "Don't care";
      case MENU_VALUE_LINEAR:
         return "Linear";
      case MENU_VALUE_NEAREST:
         return "Nearest";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "<Default>";
      case MENU_VALUE_DIRECTORY_NONE:
         return "<None>";
      case MENU_VALUE_NOT_AVAILABLE:
         return "N/A";
      case MENU_LABEL_INPUT_REMAPPING_DIRECTORY:
         return "input_remapping_directory";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Input Remapping Directory";
      case MENU_LABEL_JOYPAD_AUTOCONFIG_DIR:
         return "joypad_autoconfig_dir";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Input Device Autoconfig Directory";
      case MENU_LABEL_RECORDING_CONFIG_DIRECTORY:
         return "recording_config_directory";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Recording Config Directory";
      case MENU_LABEL_RECORDING_OUTPUT_DIRECTORY:
         return "recording_output_directory";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Recording Output Directory";
      case MENU_LABEL_SCREENSHOT_DIRECTORY:
         return "screenshot_directory";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Screenshot Directory";
      case MENU_LABEL_PLAYLIST_DIRECTORY:
         return "playlist_directory";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Playlist Directory";
      case MENU_LABEL_SAVEFILE_DIRECTORY:
         return "savefile_directory";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Savefile Directory";
      case MENU_LABEL_SAVESTATE_DIRECTORY:
         return "savestate_directory";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Savestate Directory";
      case MENU_LABEL_STDIN_CMD_ENABLE:
         return "stdin_commands";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "stdin Commands";
      case MENU_LABEL_VIDEO_DRIVER:
         return "video_driver";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Video Driver";
      case MENU_LABEL_RECORD_ENABLE:
         return "record_enable";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "Record Enable";
      case MENU_LABEL_VIDEO_GPU_RECORD:
         return "video_gpu_record";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "GPU Record Enable";
      case MENU_LABEL_RECORD_PATH:
         return "record_path";
      case MENU_LABEL_VALUE_RECORD_PATH:
         return "Record Path";
      case MENU_LABEL_RECORD_USE_OUTPUT_DIRECTORY:
         return "record_use_output_directory";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Use Output Directory";
      case MENU_LABEL_RECORD_CONFIG:
         return "record_config";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Record Config";
      case MENU_LABEL_VIDEO_POST_FILTER_RECORD:
         return "video_post_filter_record";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Post filter record Enable";
      case MENU_LABEL_CORE_ASSETS_DIRECTORY:
         return "core_assets_directory";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Core Assets Directory";
      case MENU_LABEL_ASSETS_DIRECTORY:
         return "assets_directory";
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Assets Directory";
      case MENU_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "dynamic_wallpapers_directory";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Dynamic Wallpapers Directory";
      case MENU_LABEL_BOXARTS_DIRECTORY:
         return "boxarts_directory";
      case MENU_LABEL_VALUE_BOXARTS_DIRECTORY:
         return "Boxarts Directory";
      case MENU_LABEL_RGUI_BROWSER_DIRECTORY:
         return "rgui_browser_directory";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Browser Directory";
      case MENU_LABEL_RGUI_CONFIG_DIRECTORY:
         return "rgui_config_directory";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Config Directory";
      case MENU_LABEL_LIBRETRO_INFO_PATH:
         return "libretro_info_path";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Core Info Directory";
      case MENU_LABEL_LIBRETRO_DIR_PATH:
         return "libretro_dir_path";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Core Directory";
      case MENU_LABEL_CURSOR_DIRECTORY:
         return "cursor_directory";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Cursor Directory";
      case MENU_LABEL_CONTENT_DATABASE_DIRECTORY:
         return "content_database_path";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Content Database Directory";
      case MENU_LABEL_SYSTEM_DIRECTORY:
         return "system_directory";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "System Directory";
      case MENU_LABEL_EXTRACTION_DIRECTORY:
         return "extraction_directory";
      case MENU_LABEL_CHEAT_DATABASE_PATH:
         return "cheat_database_path";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Cheat File Directory";
      case MENU_LABEL_VALUE_EXTRACTION_DIRECTORY:
         return "Extraction Directory";
      case MENU_LABEL_AUDIO_FILTER_DIR:
         return "audio_filter_dir";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Audio Filter Directory";
      case MENU_LABEL_VIDEO_FILTER_DIR:
         return "video_filter_dir";
      case MENU_LABEL_VIDEO_SHADER_DIR:
         return "video_shader_dir";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Video Shader Directory";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Video Filter Directory";
      case MENU_LABEL_OVERLAY_DIRECTORY:
         return "overlay_directory";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Overlay Directory";
      case MENU_LABEL_OSK_OVERLAY_DIRECTORY:
         return "osk_overlay_directory";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "OSK Overlay Directory";
      case MENU_LABEL_NETPLAY_CLIENT_SWAP_INPUT:
         return "netplay_client_swap_input";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Swap Netplay Input";
      case MENU_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "netplay_spectator_mode_enable";
      case MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Netplay Spectator Enable";
      case MENU_LABEL_NETPLAY_IP_ADDRESS:
         return "netplay_ip_address";
      case MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "IP Address";
      case MENU_LABEL_NETPLAY_TCP_UDP_PORT:
         return "netplay_tcp_udp_port";
      case MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Netplay TCP/UDP Port";
      case MENU_LABEL_NETPLAY_ENABLE:
         return "netplay_enable";
      case MENU_LABEL_VALUE_NETPLAY_ENABLE:
         return "Netplay Enable";
      case MENU_LABEL_NETPLAY_DELAY_FRAMES:
         return "netplay_delay_frames";
      case MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Netplay Delay Frames";
      case MENU_LABEL_NETPLAY_MODE:
         return "netplay_mode";
      case MENU_LABEL_VALUE_NETPLAY_MODE:
         return "Netplay Client Enable";
      case MENU_LABEL_RGUI_SHOW_START_SCREEN:
         return "rgui_show_start_screen";
      case MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Show Start Screen";
      case MENU_LABEL_TITLE_COLOR:
         return "menu_title_color";
      case MENU_LABEL_VALUE_TITLE_COLOR:
         return "Menu title color";
      case MENU_LABEL_ENTRY_HOVER_COLOR:
         return "menu_entry_hover_color";
      case MENU_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Menu entry hover color";
      case MENU_LABEL_TIMEDATE_ENABLE:
         return "menu_timedate_enable";
      case MENU_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Display time / date";
      case MENU_LABEL_THREADED_DATA_RUNLOOP_ENABLE:
         return "threaded_data_runloop_enable";
      case MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Threaded data runloop";
      case MENU_LABEL_ENTRY_NORMAL_COLOR:
         return "menu_entry_normal_color";
      case MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Menu entry normal color";
      case MENU_LABEL_SHOW_ADVANCED_SETTINGS:
         return "menu_show_advanced_settings";
      case MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Show Advanced Settings";
      case MENU_LABEL_COLLAPSE_SUBGROUPS_ENABLE:
         return "menu_collapse_subgroups_enable";
      case MENU_LABEL_VALUE_COLLAPSE_SUBGROUPS_ENABLE:
         return "Collapse Subgroups";
      case MENU_LABEL_MOUSE_ENABLE:
         return "menu_mouse_enable";
      case MENU_LABEL_VALUE_MOUSE_ENABLE:
         return "Mouse Support";
      case MENU_LABEL_POINTER_ENABLE:
         return "menu_pointer_enable";
      case MENU_LABEL_VALUE_POINTER_ENABLE:
         return "Touch Support";
      case MENU_LABEL_CORE_ENABLE:
         return "menu_core_enable";
      case MENU_LABEL_VALUE_CORE_ENABLE:
         return "Display core name";
      case MENU_LABEL_DPI_OVERRIDE_ENABLE:
         return "dpi_override_enable";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "DPI Override Enable";
      case MENU_LABEL_DPI_OVERRIDE_VALUE:
         return "dpi_override_value";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "DPI Override";
      case MENU_LABEL_SUSPEND_SCREENSAVER_ENABLE:
         return "suspend_screensaver_enable";
      case MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Suspend Screensaver";
      case MENU_LABEL_VIDEO_DISABLE_COMPOSITION:
         return "video_disable_composition";
      case MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Disable Desktop Composition";
      case MENU_LABEL_PAUSE_NONACTIVE:
         return "pause_nonactive";
      case MENU_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Don't run in background";
      case MENU_LABEL_UI_COMPANION_START_ON_BOOT:
         return "ui_companion_start_on_boot";
      case MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "UI Companion Start On Boot";
      case MENU_LABEL_UI_MENUBAR_ENABLE:
         return "ui_menubar_enable";
      case MENU_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Menubar (Hint)";
      case MENU_LABEL_ARCHIVE_MODE:
         return "archive_mode";
      case MENU_LABEL_VALUE_ARCHIVE_MODE:
         return "Archive File Assocation Action";
      case MENU_LABEL_NETWORK_CMD_ENABLE:
         return "network_cmd_enable";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Network Commands";
      case MENU_LABEL_NETWORK_CMD_PORT:
         return "network_cmd_port";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Network Command Port";
      case MENU_LABEL_HISTORY_LIST_ENABLE:
         return "history_list_enable";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "History List Enable";
      case MENU_LABEL_CONTENT_HISTORY_SIZE:
         return "Content History Size";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "History List Size";
      case MENU_LABEL_VIDEO_REFRESH_RATE_AUTO:
         return "video_refresh_rate_auto";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Estimated Monitor Framerate";
      case MENU_LABEL_DUMMY_ON_CORE_SHUTDOWN:
         return "dummy_on_core_shutdown";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Dummy On Core Shutdown";
      case MENU_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "core_set_supports_no_content_enable";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "Don't automatically start a core";
      case MENU_LABEL_FRAME_THROTTLE_SETTINGS:
         return "fastforward_ratio_throttle_enable";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Limit Maximum Run Speed";
      case MENU_LABEL_FASTFORWARD_RATIO:
         return "fastforward_ratio";
      case MENU_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Maximum Run Speed";
      case MENU_LABEL_AUTO_REMAPS_ENABLE:
         return "auto_remaps_enable";
      case MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Load Remap Files Automatically";
      case MENU_LABEL_SLOWMOTION_RATIO:
         return "slowmotion_ratio";
      case MENU_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Slow-Motion Ratio";
      case MENU_LABEL_CORE_SPECIFIC_CONFIG:
         return "core_specific_config";
      case MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configuration Per-Core";
      case MENU_LABEL_AUTO_OVERRIDES_ENABLE:
         return "auto_overrides_enable";
      case MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Load Override Files Automatically";
      case MENU_LABEL_CONFIG_SAVE_ON_EXIT:
         return "config_save_on_exit";
      case MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Save Configuration On Exit";
      case MENU_LABEL_VIDEO_SMOOTH:
         return "video_smooth";
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "HW Bilinear Filtering";
      case MENU_LABEL_VIDEO_GAMMA:
         return "video_gamma";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Video Gamma";
      case MENU_LABEL_VIDEO_ALLOW_ROTATE:
         return "video_allow_rotate";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Allow rotation";
      case MENU_LABEL_VIDEO_HARD_SYNC:
         return "video_hard_sync";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Hard GPU Sync";
      case MENU_LABEL_VIDEO_SWAP_INTERVAL:
         return "video_swap_interval";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "VSync Swap Interval";
      case MENU_LABEL_VIDEO_VSYNC:
         return "video_vsync";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "VSync";
      case MENU_LABEL_VIDEO_THREADED:
         return "video_threaded";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Threaded Video";
      case MENU_LABEL_VIDEO_ROTATION:
         return "video_rotation";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotation";
      case MENU_LABEL_VIDEO_GPU_SCREENSHOT:
         return "video_gpu_screenshot";
      case MENU_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "GPU Screenshot Enable";
      case MENU_LABEL_VIDEO_CROP_OVERSCAN:
         return "video_crop_overscan";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Crop Overscan (Reload)";
      case MENU_LABEL_VIDEO_ASPECT_RATIO_INDEX:
         return "aspect_ratio_index";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Aspect Ratio Index";
      case MENU_LABEL_VIDEO_ASPECT_RATIO_AUTO:
         return "video_aspect_ratio_auto";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Auto Aspect Ratio";
      case MENU_LABEL_VIDEO_FORCE_ASPECT:
         return "video_force_aspect";
      case MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Force aspect ratio";
      case MENU_LABEL_VIDEO_REFRESH_RATE: 
         return "video_refresh_rate";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Refresh Rate";
      case MENU_LABEL_VIDEO_FORCE_SRGB_DISABLE:
         return "video_force_srgb_disable";
      case MENU_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Force-disable sRGB FBO";
      case MENU_LABEL_VIDEO_WINDOWED_FULLSCREEN:
         return "video_windowed_fullscreen";
      case MENU_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Windowed Fullscreen Mode";
      case MENU_LABEL_PAL60_ENABLE:
         return "pal60_enable";
      case MENU_LABEL_VALUE_PAL60_ENABLE:
         return "Use PAL60 Mode";
      case MENU_LABEL_VIDEO_VFILTER:
         return "video_vfilter";
      case MENU_LABEL_VALUE_VIDEO_VFILTER:
         return "Deflicker";
      case MENU_LABEL_VIDEO_VI_WIDTH:
         return "video_vi_width";
      case MENU_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Set VI Screen Width";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Black Frame Insertion";
      case MENU_LABEL_VIDEO_BLACK_FRAME_INSERTION:
         return "video_black_frame_insertion";
      case MENU_LABEL_VIDEO_HARD_SYNC_FRAMES:
         return "video_hard_sync_frames";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Hard GPU Sync Frames";
      case MENU_LABEL_SORT_SAVEFILES_ENABLE:
         return "sort_savefiles_enable";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Sort Saves In Folders";
      case MENU_LABEL_SORT_SAVESTATES_ENABLE:
         return "sort_savestates_enable";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Sort Savestates In Folders";
      case MENU_LABEL_VIDEO_FULLSCREEN:
         return "video_fullscreen";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Use Fullscreen Mode";
      case MENU_LABEL_PERFCNT_ENABLE:
         return "perfcnt_enable";
      case MENU_LABEL_VIDEO_SCALE:
         return "video_scale";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Windowed Scale";
      case MENU_LABEL_VIDEO_SCALE_INTEGER:
         return "video_scale_integer";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Integer Scale";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Performance Counters";
      case MENU_LABEL_LIBRETRO_LOG_LEVEL:
         return "libretro_log_level";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Core Logging Level";
      case MENU_LABEL_LOG_VERBOSITY:
         return "log_verbosity";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Logging Verbosity";
      case MENU_LABEL_SAVESTATE_AUTO_SAVE:
         return "savestate_auto_save";
      case MENU_LABEL_SAVESTATE_AUTO_LOAD:
         return "savestate_auto_load";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Auto Load State";
      case MENU_LABEL_SAVESTATE_AUTO_INDEX:
         return "savestate_auto_index";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Save State Auto Index";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Auto Save State";
      case MENU_LABEL_AUTOSAVE_INTERVAL:
         return "autosave_interval";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "SaveRAM Autosave Interval";
      case MENU_LABEL_BLOCK_SRAM_OVERWRITE:
         return "block_sram_overwrite";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Don't overwrite SaveRAM on loading savestate";
      case MENU_LABEL_VIDEO_SHARED_CONTEXT:
         return "video_shared_context";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "HW Shared Context Enable";
      case MENU_LABEL_RESTART_RETROARCH:
         return "restart_retroarch";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Restart RetroArch";
      case MENU_LABEL_NETPLAY_NICKNAME:
         return "netplay_nickname";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Username";
      case MENU_LABEL_USER_LANGUAGE:
         return "user_language";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Language";
      case MENU_LABEL_CAMERA_ALLOW:
         return "camera_allow";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Allow Camera";
      case MENU_LABEL_LOCATION_ALLOW:
         return "location_allow";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Allow Location";
      case MENU_LABEL_PAUSE_LIBRETRO:
         return "menu_pause_libretro";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pause when menu activated";
      case MENU_LABEL_INPUT_OSK_OVERLAY_ENABLE:
         return "input_osk_overlay_enable";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Display Keyboard Overlay";
      case MENU_LABEL_INPUT_OVERLAY_ENABLE:
         return "input_overlay_enable";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Display Overlay";
      case MENU_LABEL_VIDEO_MONITOR_INDEX:
         return "video_monitor_index";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Monitor Index";
      case MENU_LABEL_VIDEO_FRAME_DELAY:
         return "video_frame_delay";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Frame Delay";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Duty Cycle";
      case MENU_LABEL_INPUT_DUTY_CYCLE:
         return "input_duty_cycle";
      case MENU_LABEL_INPUT_TURBO_PERIOD:
         return "input_turbo_period";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Turbo Period";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Input Axis Threshold";
      case MENU_LABEL_INPUT_AXIS_THRESHOLD:
         return "input_axis_threshold";
      case MENU_LABEL_INPUT_REMAP_BINDS_ENABLE:
         return "input_remap_binds_enable";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Remap Binds Enable";
      case MENU_LABEL_INPUT_MAX_USERS:
         return "input_max_users";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Max Users";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Autoconfig Enable";
      case MENU_LABEL_INPUT_AUTODETECT_ENABLE:
         return "input_autodetect_enable";
      case MENU_LABEL_AUDIO_OUTPUT_RATE:
         return "audio_output_rate";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Audio Output Rate (KHz)";
      case MENU_LABEL_AUDIO_MAX_TIMING_SKEW:
         return "audio_max_timing_skew";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Audio Maximum Timing Skew";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Cheat Passes";
      case MENU_LABEL_CHEAT_APPLY_CHANGES:
         return "cheat_apply_changes";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Save Core Remap File";
      case MENU_LABEL_REMAP_FILE_SAVE_CORE:
         return "remap_file_save_core";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Save Game Remap File";
      case MENU_LABEL_REMAP_FILE_SAVE_GAME:
         return "remap_file_save_game";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Apply Cheat Changes";
      case MENU_LABEL_CHEAT_NUM_PASSES:
         return "cheat_num_passes";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Apply Shader Changes";
      case MENU_LABEL_SHADER_APPLY_CHANGES:
         return "shader_apply_changes";
      case MENU_LABEL_COLLECTION:
         return "collection";
      case MENU_LABEL_REWIND_ENABLE:
         return "rewind_enable";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Rewind Enable";
      case MENU_LABEL_CONTENT_COLLECTION_LIST:
         return "content_collection_list";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Select From Collection";
      case MENU_LABEL_DETECT_CORE_LIST:
         return "detect_core_list";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Select File And Detect Core";
      case MENU_LABEL_LOAD_CONTENT_HISTORY:
         return "history_list";
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Select From History";
      case MENU_LABEL_AUDIO_ENABLE:
         return "audio_enable";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Audio Enable";
      case MENU_LABEL_FPS_SHOW:
         return "fps_show";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Display Framerate";
      case MENU_LABEL_AUDIO_MUTE:
         return "audio_mute_enable";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Audio Mute";
      case MENU_LABEL_VIDEO_SHADER_PASS:
         return "video_shader_pass";
      case MENU_LABEL_AUDIO_VOLUME:
         return "audio_volume";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Audio Volume Level (dB)";
      case MENU_LABEL_AUDIO_SYNC:
         return "audio_sync";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Audio Sync Enable";
      case MENU_LABEL_AUDIO_RATE_CONTROL_DELTA:
         return "audio_rate_control_delta";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Audio Rate Control Delta";
      case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
         return "video_shader_filter_pass";
      case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
         return "video_shader_scale_pass";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Shader Passes";
      case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
         return "video_shader_num_passes";
      case MENU_LABEL_RDB_ENTRY_DESCRIPTION:
         return "rdb_entry_description";
      case MENU_LABEL_RDB_ENTRY_ORIGIN:
         return "rdb_entry_origin";
      case MENU_LABEL_RDB_ENTRY_PUBLISHER:
         return "rdb_entry_publisher";
      case MENU_LABEL_RDB_ENTRY_DEVELOPER:
         return "rdb_entry_developer";
      case MENU_LABEL_RDB_ENTRY_FRANCHISE:
         return "rdb_entry_franchise";
      case MENU_LABEL_RDB_ENTRY_MAX_USERS:
         return "rdb_entry_max_users";
      case MENU_LABEL_RDB_ENTRY_NAME:
         return "rdb_entry_name";
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_RATING:
         return "rdb_entry_edge_magazine_rating";
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_REVIEW:
         return "rdb_entry_edge_magazine_review";
      case MENU_LABEL_RDB_ENTRY_FAMITSU_MAGAZINE_RATING:
         return "rdb_entry_famitsu_magazine_rating";
      case MENU_LABEL_RDB_ENTRY_EDGE_MAGAZINE_ISSUE:
         return "rdb_entry_edge_magazine_issue";
      case MENU_LABEL_RDB_ENTRY_RELEASE_MONTH:
         return "rdb_entry_releasemonth";
      case MENU_LABEL_RDB_ENTRY_RELEASE_YEAR:
         return "rdb_entry_releaseyear";
      case MENU_LABEL_RDB_ENTRY_ENHANCEMENT_HW:
         return "rdb_entry_enhancement_hw";
      case MENU_LABEL_RDB_ENTRY_SHA1:
         return "rdb_entry_sha1";
      case MENU_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_LABEL_RDB_ENTRY_CRC32:
         return "rdb_entry_crc32";
      case MENU_LABEL_RDB_ENTRY_MD5:
         return "rdb_entry_md5";
      case MENU_LABEL_RDB_ENTRY_BBFC_RATING:
         return "rdb_entry_bbfc_rating";
      case MENU_LABEL_RDB_ENTRY_ESRB_RATING:
         return "rdb_entry_esrb_rating";
      case MENU_LABEL_RDB_ENTRY_ELSPA_RATING:
         return "rdb_entry_elspa_rating";
      case MENU_LABEL_RDB_ENTRY_PEGI_RATING:
         return "rdb_entry_pegi_rating";
      case MENU_LABEL_RDB_ENTRY_CERO_RATING:
         return "rdb_entry_cero_rating";
      case MENU_LABEL_RDB_ENTRY_ANALOG:
         return "rdb_entry_analog";
      case MENU_LABEL_CONFIGURATIONS:
         return "configurations";
      case MENU_LABEL_VALUE_CONFIGURATIONS: /* FIXME */
         return "Load Configuration File";
      case MENU_LABEL_LOAD_OPEN_ZIP:
         return "load_open_zip";
      case MENU_LABEL_REWIND_GRANULARITY:
         return "rewind_granularity";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Rewind Granularity";
      case MENU_LABEL_REMAP_FILE_LOAD:
         return "remap_file_load";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Load Remap File";
      case MENU_LABEL_REMAP_FILE_SAVE_AS:
         return "remap_file_save_as";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Save Remap File As";
      case MENU_LABEL_CUSTOM_RATIO:
         return "custom_ratio";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Custom Ratio";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Use this directory>";
      case MENU_LABEL_USE_THIS_DIRECTORY:
         return "use_this_directory";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Start Content";
      case MENU_LABEL_RDB_ENTRY_START_CONTENT:
         return "rdb_entry_start_content";
      case MENU_LABEL_CUSTOM_BIND:
         return "custom_bind";
      case MENU_LABEL_CUSTOM_BIND_ALL:
         return "custom_bind_all";
      case MENU_LABEL_DISK_OPTIONS:
         return "core_disk_options";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Core Disk Options";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Core Options";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Core Cheat Options";
      case MENU_LABEL_CORE_CHEAT_OPTIONS:
         return "core_cheat_options";
      case MENU_LABEL_CORE_OPTIONS:
         return "core_options";
      case MENU_LABEL_DATABASE_MANAGER_LIST:
         return "database_manager_list";
      case MENU_LABEL_DEFERRED_DATABASE_MANAGER_LIST:
         return "deferred_database_manager_list";
      case MENU_LABEL_CURSOR_MANAGER_LIST:
         return "cursor_manager_list";
      case MENU_LABEL_DEFERRED_CURSOR_MANAGER_LIST:
         return "deferred_cursor_manager_list";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Cheat File Load";
      case MENU_LABEL_CHEAT_FILE_LOAD:
         return "cheat_file_load";
      case MENU_LABEL_CHEAT_FILE_SAVE_AS:
         return "cheat_file_save_as";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Cheat File Save As";
      case MENU_LABEL_DEFERRED_RDB_ENTRY_DETAIL:
         return "deferred_rdb_entry_detail";
      case MENU_LABEL_FRONTEND_COUNTERS:
         return "frontend_counters";
      case MENU_LABEL_CORE_COUNTERS:
         return "core_counters";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Core Counters";
      case MENU_LABEL_DISK_CYCLE_TRAY_STATUS:
         return "disk_cycle_tray_status";
      case MENU_LABEL_DISK_IMAGE_APPEND:
         return "disk_image_append";
      case MENU_LABEL_DEFERRED_CORE_LIST:
         return "deferred_core_list";
      case MENU_LABEL_DEFERRED_CORE_LIST_SET:
         return "deferred_core_list_set";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Take Screenshot";
      case MENU_LABEL_INFO_SCREEN:
         return "info_screen";
      case MENU_LABEL_VALUE_RESUME:
         return "Resume";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Disk Index";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Frontend Counters";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Disk Image Append";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Disk Cycle Tray Status";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "No playlist entries available.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "No core information available.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "No core options available.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "No cores available.";
      case MENU_VALUE_NO_CORE:
         return "No Core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Database Manager";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Cursor Manager";
      case MENU_VALUE_RECORDING_SETTINGS:
         return "Recording Settings";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_SETTINGS:
         return "frontend_settings";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Frontend Settings";
      case MENU_LABEL_QUIT_RETROARCH:
         return "quit_retroarch";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Quit RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Help";
      case MENU_LABEL_HELP:
         return "help";
      case MENU_LABEL_SAVE_NEW_CONFIG:
         return "save_new_config";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Save New Config";
      case MENU_LABEL_RESTART_CONTENT:
         return "restart_content";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Restart Content";
      case MENU_LABEL_TAKE_SCREENSHOT:
         return "take_screenshot";
      case MENU_LABEL_CORE_UPDATER_LIST:
         return "core_updater_list";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Core Updater";
      case MENU_LABEL_CORE_UPDATER_BUILDBOT_URL:
         return "core_updater_buildbot_url";
      case MENU_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "Buildbot Cores URL";
      case MENU_LABEL_BUILDBOT_ASSETS_URL:
         return "buildbot_assets_url";
      case MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "Buildbot Assets URL";
      case MENU_LABEL_NAVIGATION_WRAPAROUND_VERTICAL:
         return "menu_navigation_wraparound_vertical_enable";
      case MENU_LABEL_NAVIGATION_WRAPAROUND_HORIZONTAL:
         return "menu_navigation_wraparound_horizontal_enable";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND_HORIZONTAL:
         return "Navigation Wrap-Around Horizontal";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND_VERTICAL:
         return "Navigation Wrap-Around Vertical";
      case MENU_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "menu_navigation_browser_filter_supported_extensions_enable";
      case MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filter by supported extensions";
      case MENU_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "core_updater_auto_extract_archive";
      case MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Automatically extract downloaded archive";
      case MENU_LABEL_SYSTEM_INFORMATION:
         return "system_information";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "System Information";
      case MENU_LABEL_OPTIONS:
         return "core_settings";
      case MENU_LABEL_VALUE_OPTIONS:
         return "Core Settings";
      case MENU_LABEL_CORE_INFORMATION:
         return "core_information";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Core Information";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Directory not found.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "No items.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Load Core";
      case MENU_LABEL_CORE_LIST:
         return "program_core_list";
      case MENU_LABEL_LOAD_CONTENT:
         return "load_content_default";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Select File";
      case MENU_LABEL_CLOSE_CONTENT:
         return "unload_core";
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Close Content";
      case MENU_LABEL_MANAGEMENT:
         return "database_settings";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Database Settings";
      case MENU_LABEL_PERFORMANCE_COUNTERS:
         return "performance_counters";
      case MENU_LABEL_SAVE_STATE:
         return "savestate";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Save State";
      case MENU_LABEL_LOAD_STATE:
         return "loadstate";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Load State";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Resume Content";
      case MENU_LABEL_RESUME_CONTENT:
         return "resume_content";
      case MENU_LABEL_INPUT_DRIVER:
         return "input_driver";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Input Driver";
      case MENU_LABEL_AUDIO_DRIVER:
         return "audio_driver";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Audio Driver";
      case MENU_LABEL_JOYPAD_DRIVER:
         return "input_joypad_driver";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Joypad Driver";
      case MENU_LABEL_AUDIO_RESAMPLER_DRIVER:
         return "audio_resampler_driver";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Audio Resampler Driver";
      case MENU_LABEL_RECORD_DRIVER:
         return "record_driver";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Record Driver";
      case MENU_LABEL_MENU_DRIVER:
         return "menu_driver";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Menu Driver";
      case MENU_LABEL_CAMERA_DRIVER:
         return "camera_driver";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Camera Driver";
      case MENU_LABEL_LOCATION_DRIVER:
         return "location_driver";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Location Driver";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Unable to read compressed file.";
      case MENU_LABEL_OVERLAY_SCALE:
         return "input_overlay_scale";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Overlay Scale";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Overlay Preset";
      case MENU_LABEL_OVERLAY_PRESET:
         return "input_overlay";
      case MENU_LABEL_KEYBOARD_OVERLAY_PRESET:
         return "input_osk_overlay";
      case MENU_LABEL_AUDIO_DEVICE:
         return "audio_device";
      case MENU_LABEL_AUDIO_LATENCY:
         return "audio_latency";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Audio Latency (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Audio Device";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Keyboard Overlay Preset";
      case MENU_LABEL_OVERLAY_OPACITY:
         return "input_overlay_opacity";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Overlay Opacity";
      case MENU_LABEL_MENU_WALLPAPER:
         return "menu_wallpaper";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Menu Wallpaper";
      case MENU_LABEL_DYNAMIC_WALLPAPER:
         return "menu_dynamic_wallpaper_enable";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Dynamic Wallpaper";
      case MENU_LABEL_BOXART:
         return "menu_boxart_enable";
      case MENU_LABEL_VALUE_BOXART:
         return "Display Boxart";
      case MENU_LABEL_CORE_INPUT_REMAPPING_OPTIONS:
         return "core_input_remapping_options";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Core Input Remapping Options";
      case MENU_LABEL_VIDEO_OPTIONS:
         return "video_options";
      case MENU_LABEL_VALUE_VIDEO_OPTIONS:
         return "Video Options";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Shader Options";
      case MENU_LABEL_SHADER_OPTIONS:
         return "shader_options";
      case MENU_LABEL_VIDEO_SHADER_PARAMETERS:
         return "video_shader_parameters";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Current Shader Parameters";
      case MENU_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
         return "video_shader_preset_parameters";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Menu Shader Parameters (Menu)";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Shader Preset Save As";
      case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
         return "video_shader_preset_save_as";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "No shader parameters.";
      case MENU_LABEL_VIDEO_SHADER_PRESET:
         return "video_shader_preset";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Load Shader Preset";
      case MENU_LABEL_VIDEO_FILTER:
         return "video_filter";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Video Filter";
      case MENU_LABEL_DEFERRED_VIDEO_FILTER:
         return "deferred_video_filter";
      case MENU_LABEL_DEFERRED_CORE_UPDATER_LIST:
         return "deferred_core_updater_list";
      case MENU_LABEL_AUDIO_DSP_PLUGIN:
         return "audio_dsp_plugin";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Audio DSP Plugin";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Starting download: ";
      case MENU_VALUE_SECONDS:
         return "seconds";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      default:
         break;
   }

   return "null";
}

const char *menu_hash_to_str(uint32_t hash)
{
   const char *ret = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings)
      return "null";

   switch (settings->user_language)
   {
      case RETRO_LANGUAGE_FRENCH:
         ret = menu_hash_to_str_french(hash);
         break;
      case RETRO_LANGUAGE_GERMAN:
         ret = menu_hash_to_str_german(hash);
         break;
      case RETRO_LANGUAGE_SPANISH:
         ret = menu_hash_to_str_spanish(hash);
         break;
      case RETRO_LANGUAGE_ITALIAN:
         ret = menu_hash_to_str_italian(hash);
         break;
      case RETRO_LANGUAGE_PORTUGUESE:
         ret = menu_hash_to_str_portuguese(hash);
         break;
      case RETRO_LANGUAGE_DUTCH:
         ret = menu_hash_to_str_dutch(hash);
         break;
      default:
         break;
   }

   if (ret && strcmp(ret, "null") != 0)
      return ret;

   return menu_hash_to_str_english(hash);
}

uint32_t menu_hash_calculate(const char *s)
{
   return djb2_calculate(s);
}

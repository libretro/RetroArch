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

#include "../msg_hash.h"

#if defined(_MSC_VER) && !defined(_XBOX)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#pragma warning( disable: 4566 )
#endif

int menu_hash_get_help_pl_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   int ret = 0;

   switch (msg)
   {
      case MSG_UNKNOWN:
      default:
         ret = -1;
         break;
   }

   return ret;
}

const char *msg_hash_to_str_pl(enum msg_hash_enums msg)
{
   switch (msg)
   {
      case MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST:
         return "Dodaj treść";
      case MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE:
         return "Pytaj";
      case MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Katalog assetów";
      case MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Block Frames";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE:
         return "Urządzenie audio";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER:
         return "Kontroler dźwięku";
      case MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Wtyczki audio DSP";
      case MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE:
         return "Włącz dźwięk";
      case MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Katalog filtrów audio";
      case MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY:
         return "Opóźnienie dźwięku (ms)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Audio Maximum Timing Skew";
      case MENU_ENUM_LABEL_VALUE_AUDIO_MUTE:
         return "Wycisz dźwięk";
      case MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Częstotliwość próbkowania dźwięku (Hz)";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Audio Rate Control Delta";
      case MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Kontroler resamplera dźwięku";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS:
         return "Ustawienia dźwięku";
      case MENU_ENUM_LABEL_VALUE_AUDIO_SYNC:
         return "Włącz synchronizację dźwięku";
      case MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME:
         return "Poziom głośności (dB)";
      case MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Częstotliwość automatycznego zapisu SaveRAM";
      case MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Load Override Files Automatically"; /* this one's rather complicated */
      case MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Automatycznie wczytuj pliki remapowania";
      case MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Nie nadpisuj SaveRAM przy wczytywaniu stanu";
      case MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "URL assetów buildbota";
      case MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY: /* UPDATE/FIXME */
         return "Katalog do wypakowywania archiwów";
      case MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW:
         return "Zezwalaj na dostęp do kamerki";
      case MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER:
         return "Kontroler kamerki";
      case MENU_ENUM_LABEL_VALUE_CHEAT:
         return "Cheat";
      case MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Zastosuj zmiany cheatów";
      case MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Katalog z plikami cheatów";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Wczytaj plik z cheatami";
      case MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Zapisz plik z cheatami jako";
      case MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Liczba przebiegów cheatów";
      case MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT:
         return "Zamknij";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATIONS:
         return "Wczytaj konfigurację";
      case MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Ustawienia konfiguracji";
      case MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Zapisz konfigurację przy wyjściu";
      case MENU_ENUM_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Kolekcji";
      case MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Katalog bazy danych treści";
      case MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Rozmiar historii treści";
      case MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS:
         return "Szybkie menu";
      case MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Katalog pobranych";
      case MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Opcje cheatów rdzenia";
      case MENU_ENUM_LABEL_VALUE_CORE_COUNTERS:
         return "Liczniki rdzenia";
      case MENU_ENUM_LABEL_VALUE_CORE_ENABLE:
         return "Wyświetlaj nazwę rdzenia";
      case MENU_ENUM_LABEL_VALUE_CORE_INFORMATION:
         return "Informacje o rdzeniu";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Autorzy";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Kategorie";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Oznaczenie rdzenia";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Nazwa rdzenia";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware(s)";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Licencja(-e)";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Zezwolenia";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Wspierane rozszerzenia";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "Producent systemu";
      case MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "Nazwa systemu";
      case MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Opcje remapowania kontrolera rdzenia"; /* this is quite bad */
      case MENU_ENUM_LABEL_VALUE_CORE_LIST:
         return "Wczytaj rdzeń";
      case MENU_ENUM_LABEL_VALUE_CORE_OPTIONS:
         return "Opcje";
      case MENU_ENUM_LABEL_VALUE_CORE_SETTINGS:
         return "Ustawienia rdzenia";
      case MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE: /* TODO/FIXME */
         return "Nie uruchamiaj rdzenia automatycznie";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Automatycznie wypakowuj pobierane archiwa";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "URL rdzeni buildbota";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Aktualizator rdzeni";
      case MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Ustawienia aktualizatora";
      case MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Katalog z kursorami";
      case MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER:
         return "Menedżer kursorów";
      case MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO:
         return "Włąsny współczynnik";
      case MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER:
         return "Menedżer bazy danych";
      case MENU_ENUM_LABEL_VALUE_FAVORITES: /* TODO/FIXME - update */
         return "Wybierz plik i dopasuj rdzeń";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT:
         return "<Katalog treści>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT:
         return "<Domyślny>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE:
         return "<Żaden>";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Nie znaleziono katalogu.";
      case MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Ustawienia katalogów";
      case MENU_ENUM_LABEL_VALUE_DISABLED:
         return "Wyłączone";
      case MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Disk Cycle Tray Status";
      case MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Dopisz do obrazu dysku";
      case MENU_ENUM_LABEL_VALUE_DISK_INDEX:
         return "Indeks dysku";
      case MENU_ENUM_LABEL_VALUE_DISK_OPTIONS:
         return "Opcje dysku rdzenia";
      case MENU_ENUM_LABEL_VALUE_DONT_CARE:
         return "Bez znaczenia";
      case MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "Wybierz pobrany plik i dopasuj rdzeń"; /* TODO/FIXME - rewrite */
      case MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT:
         return "Pobierz treści";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Pomiń wykryte DPI";
      case MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "Własne DPI";
      case MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS:
         return "Ustawienia kontrolerów";
      case MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Atrapa rdzenia przy zatrzymaniu rdzenia";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Dynamiczna tapeta";
      case MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Katalog dynamicznych tapet";
      case MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Kolor zaznaczonego elementu menu";
      case MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Zwykły kolor elementu menu";
      case MENU_ENUM_LABEL_VALUE_FALSE:
         return "Fałsz";
      case MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Maksymalna szybkość działania";
      case MENU_ENUM_LABEL_VALUE_FPS_SHOW:
         return "Wyświetlaj FPS";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limituj maksymalną szybkość działania";
      case MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Frame Throttle Settings";
      case MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Liczniki frontendu";
      case MENU_ENUM_LABEL_VALUE_HELP:
         return "Pooc";
      case MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Włącz historię treści";
      case MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU: /* Don't change. Breaks everything. (Would be: "Menu poziome") */
         return "Horizontal Menu";
      case MENU_ENUM_LABEL_VALUE_INFORMATION_LIST:
         return "Informacje";
      case MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Włącz autokonfigurację";
      case MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Próg osi";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Ukryj nieprzypisane przyciski";
      case MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Wyświetl opisy przycisków dla tego rdzenia"; /* UPDATE/FIXME */
      case MENU_ENUM_LABEL_VALUE_INPUT_DRIVER:
         return "Kontroler wejścia";
      case MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Cykl zmian";
      case MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Input Hotkey Binds";
      case MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS:
         return "Maksymalna liczba użytkowników";
      case MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Wyświetlaj klawiaturę ekranową";
      case MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Wyświetlaj nakładkę";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Katalog plików remapowania";
      case MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Włącz remapowanie bindów";
      case MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS:
         return "Ustawienia wprowadzania";
      case MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Okres turbo";
      case MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS:
         return "Wprowadź bindy dla użytkownika %u";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Katalog autokonfiguracji kontrolerów gier";
      case MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER:
         return "Kontroler gamepadów";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "chińśki (Uproszczony)";
      case MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL:
         return "chiński (Tradycyjny)";
      case MENU_ENUM_LABEL_VALUE_LANG_DUTCH:
         return "duński";
      case MENU_ENUM_LABEL_VALUE_LANG_ENGLISH:
         return "angielski";
      case MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO:
         return "esperanto";
      case MENU_ENUM_LABEL_VALUE_LANG_FRENCH:
         return "francuski";
      case MENU_ENUM_LABEL_VALUE_LANG_GERMAN:
         return "niemiecki";
      case MENU_ENUM_LABEL_VALUE_LANG_ITALIAN:
         return "włoski";
      case MENU_ENUM_LABEL_VALUE_LANG_JAPANESE:
         return "japoński";
      case MENU_ENUM_LABEL_VALUE_LANG_KOREAN:
         return "koreański";
      case MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL:
         return "portugalski (brazil)";
      case MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL:
         return "portugalski (portugal)";
      case MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN:
         return "rosyjski";
      case MENU_ENUM_LABEL_VALUE_LANG_SPANISH:
         return "hiszpański";
      case MENU_ENUM_LABEL_VALUE_LEFT_ANALOG:
         return "Lewy analog";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Katalog rdzeni";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Katalog informacji o rdzeniach";
      case MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Poziom logowania rdzenia";
      case MENU_ENUM_LABEL_VALUE_LINEAR:
         return "Liniowe";
      case MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE:
         return "Wczytaj archiwum";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Wczytaj z ostatnio używanych";
      case MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Wczytaj treść";
      case MENU_ENUM_LABEL_VALUE_LOAD_STATE:
         return "Wczytaj stan";
      case MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW:
         return "Zezwalaj na dostęp do lokalizacji";
      case MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER:
         return "Kontroler lokalizacji";
      case MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS:
         return "Ustawienia logowania";
      case MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY:
         return "Szczegółowość logowania";
      case MENU_ENUM_LABEL_VALUE_MAIN_MENU:
         return "Menu główne";
      case MENU_ENUM_LABEL_VALUE_MANAGEMENT:
         return "Ustawienia bazy danych";
      case MENU_ENUM_LABEL_VALUE_MENU_DRIVER:
         return "Kontroler menu";
      case MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Menu File Browser Settings";
      case MENU_ENUM_LABEL_VALUE_MENU_SETTINGS:
         return "Ustawienia menu";
      case MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER:
         return "Tapeta menu";
      case MENU_ENUM_LABEL_VALUE_MISSING:
         return "Brak";
      case MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE:
         return "Obsługa myszy";
      case MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Ustawienia multimediów";
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filtruj według wspieranych rozszerzeń"; /* TODO/FIXME - rewrite */
      case MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Zawijanie nawigacji";
      case MENU_ENUM_LABEL_VALUE_NEAREST:
         return "Najbliższe";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT: /* TODO: Original string changed */
         return "Zamień kontrolery w grze sieciowej";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Opóxnione klatki w grze sieciowej";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE:
         return "Włącz grę sieciową";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS: /* TODO: Original string changed */
         return "Adres IP";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_MODE:
         return "Tryb klienta gry sieciowej";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nazwa użytkownika";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Tryb obserwatora gry sieciowej";
      case MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Port TCP/UDP gry sieciowej";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Komendy sieciowe";
      case MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Port dla komend sieciowych";
      case MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS:
         return "Ustawienia sieci";
      case MENU_ENUM_LABEL_VALUE_NO:
         return "Nie";
      case MENU_ENUM_LABEL_VALUE_NONE:
         return "Żaden";
      case MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE:
         return "B/D";
      case MENU_ENUM_LABEL_VALUE_NO_CORE:
         return "Brak rdzenia";
      case MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Brak dostępnych rdzeni.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Brak informacji o rdzeniu.";
      case MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Brak dostępnych opcji rdzenia.";
      case MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "Brak dostępnych informacji.";
      case MENU_ENUM_LABEL_VALUE_NO_ITEMS:
         return "Brak elementów.";
      case MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "Brak liczników wydajności.";
      case MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Brak wpisów w playliście.";
      case MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "Nie znaleziono ustawień.";
      case MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Brak parametrów shadera.";
      case MENU_ENUM_LABEL_VALUE_OFF: /* Don't change. Needed for XMB atm. (Would be: "WYŁĄCZONE") */
         return "OFF";
      case MENU_ENUM_LABEL_VALUE_ON: /* Don't change. Needed for XMB atm. (Would be: "WŁĄCZONE") */
         return "ON";
      case MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER:
         return "Aktualizator sieciowy";
      case MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Ustawienia OSD";
      case MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE:
         return "Otwórz archiwum";
      case MENU_ENUM_LABEL_VALUE_OPTIONAL:
         return "Opcjonalny";
      case MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Katalog klawiatur ekranowych";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED:
         return "Automatycznie wczytaj preferowaną nakładkę";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Katalog nakładek";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY:
         return "Nieprzeźroczystość nakładki";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET:
         return "Preset nakładki";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE:
         return "Skala nakładki";
      case MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Ustawienia przycisków ekranowych";
      case MENU_ENUM_LABEL_VALUE_PAL60_ENABLE:
         return "Użyj trybu PAL60";
      case MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pauzuj przy wejściu do menu";
      case MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Wstrzymaj gdy w tle";
      case MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE:
         return "Liczniki wydajności";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Katalog historii";
      case MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Ustawienia playlisty";
      case MENU_ENUM_LABEL_VALUE_POINTER_ENABLE:
         return "Obsługa dotyku";
      case MENU_ENUM_LABEL_VALUE_PORT:
         return "Port";
      case MENU_ENUM_LABEL_VALUE_PRESENT:
         return "Obecny";
      case MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Ustawienia prywatności";
      case MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH:
         return "Opuść RetroArch";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Opis";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Deweloper";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franczyza";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Nazwa";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Pochodzenie";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Wydawca";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Miesiąc wydania";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Rok wydania";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Uruchom treść";
      case MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Katalog konfiguracji nagrywania";
      case MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Katalog wyjściowy nagrywania";
      case MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS:
         return "Ustawienia nagrywania";
      case MENU_ENUM_LABEL_VALUE_RECORD_CONFIG:
         return "Konfiguracja nagrywania";
      case MENU_ENUM_LABEL_VALUE_RECORD_DRIVER:
         return "Kontroler nagrywania";
      case MENU_ENUM_LABEL_VALUE_RECORD_ENABLE:
         return "Włącz nagrywanie";
      case MENU_ENUM_LABEL_VALUE_RECORD_PATH: /* FIXME/UPDATE */
         return "Ścieżka nagrywania";
      case MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Używaj katalogu wyjściowego nagrywania";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Wczytaj plik remapowania";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Zapisz plik remapowania dla rdzenia";
      case MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Zapisz plik remapowania dla gry";
      case MENU_ENUM_LABEL_VALUE_REQUIRED:
         return "Wymagany";
      case MENU_ENUM_LABEL_VALUE_RESTART_CONTENT:
         return "Restartuj";
      case MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH:
         return "Uruchom ponownie RetroArch";
      case MENU_ENUM_LABEL_VALUE_RESUME:
         return "Wznów";
      case MENU_ENUM_LABEL_VALUE_RESUME_CONTENT:
         return "Wznów";
      case MENU_ENUM_LABEL_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_ENUM_LABEL_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_ENUM_LABEL_VALUE_REWIND_ENABLE:
         return "Włącz przewijanie";
      case MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY:
         return "Płynność przewijania";
      case MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS:
         return "Ustawienia przewijania";
      case MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Katalog przeglądarki";
      case MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Katalog zapisanych konfiguracji";
      case MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Pokazuj ekran startowy";
      case MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG:
         return "Prawy analog";
      case MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Katalog zapisów treści";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Indeks automatycznie zapisywanego stanu";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Automatyczne wczytywanie stanu";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Automatyczny zapis stanu";
      case MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Katalog zapisanych stanów";
      case MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Zapisz nową konfigurację";
      case MENU_ENUM_LABEL_VALUE_SAVE_STATE:
         return "Zapisz stan";
      case MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS:
         return "Ustawienia zapisywania";
      case MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY:
         return "Przeszukaj katalog";
      case MENU_ENUM_LABEL_VALUE_SCAN_FILE:
         return "Skanuj plik";
      case MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "<Przeszukaj ten katalog>";
      case MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Katalog zrzutów";
      case MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Rozdzielczość ekranu";
      case MENU_ENUM_LABEL_VALUE_SEARCH:
         return "Szukaj:";
      case MENU_ENUM_LABEL_VALUE_SECONDS:
         return "sekund";
      case MENU_ENUM_LABEL_VALUE_SETTINGS:
         return "Ustawienia";
      case MENU_ENUM_LABEL_VALUE_SHADER:
         return "Shader";
      case MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Zastosuj zmiany shaderów";
      case MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS:
         return "Opcje shadera";
      case MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Pokaż zaawansowane ustawienia";
      case MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Współczynnik spowolnienia";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Sortuj zapisy w folderach";
      case MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Sortuj zapisane stany w folderach";
      case MENU_ENUM_LABEL_VALUE_STATUS:
         return "Status";
      case MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Komendy stdin";
      case MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Wstrzymaj wygaszacz";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "Włącz BGM systemu";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "Katalog systemu";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informacje o systemie";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "Wsparcie 7zip";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "Wsparcie ALSA";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Data kompilacji";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Wsparcie Cg";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Wsparcie Cocoa";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Wsparcie interfejsu wiersza poleceń";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "Wsparcie CoreText";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "Właściwości CPU";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Metryczne DPI wyświetlacza";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Metryczna wysokość wyświetlacza (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Metryczna szerokość wyświetlacza (mm)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "Wsparcie DirectSound";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT:
         return "Wsparcie WASAPI";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Wsparcie bibliotek dynamicznych";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "Wsparcie EGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "Wsparcie renderowania do tekstury w OpenGL/Direct3D (wielokrotne przebiegi shaderów)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "Wsparcie FFmpeg";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "Wsparcie FreeType";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Identyfikator frontendu";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Nazwa frontendu";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "System operacyjny hosta:";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Wersja git";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "Wsparcie GLSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "Wsparcie HLSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "Wsparcie JACK";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "Wsparcie KMS/EGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "Wsparcie LibretroDB";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Wsparcie libusb";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "Wsparcie parsowania XML libxml2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Wsparcie Netplay (peer-to-peer)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Wsparcie interfejsu komend sieciowych";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "Wsparcie OpenAL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "Wsparcie OpenGL ES";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "Wsparcie OpenGL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "Wsparcie OpenSL";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "Wsparcie OpenVG";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "Wsparcie OSS";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Wsparcie nakładek graficznych";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "Źródło zasilania";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Naładowano";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "Ładowanie";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Na baterii";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "Brak źródła";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "Wsparcie PulseAudio";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Wsparcie Pythona (skrypty w shaderach)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "Poziom RetroRating";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "Wsparcie RoarAudio";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "Wsparcie PNG (RPNG)";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "Wsparcie RSound";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "Wsparcie SDL2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "Wsparcie SDL image";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "Wsparcie SDL1.2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Wsparcie wielu wątków";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Wsparcie Udev";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Wsparcie Video4Linux2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Kontroler wideo";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Wsparcie Wayland";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "Wsparcie X11";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "Wsparcie XAudio2";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "Wsparcie XVideo";
      case MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Wsparcie Zlib";
      case MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Zapisz zrzut";
      case MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE: /* TODO/FIXME - update */
         return "Osobny wątek odbierania danych";
      case MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Wyświetl czas/datę";
      case MENU_ENUM_LABEL_VALUE_TITLE_COLOR:
         return "Kolor tytułu menu";
      case MENU_ENUM_LABEL_VALUE_TRUE:
         return "Prawda";
      case MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "UI Companion Start On Boot";
      case MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Pasek menu";
      case MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Nie udało się odczytać skompresowanego pliku.";
      case MENU_ENUM_LABEL_VALUE_UNKNOWN:
         return "Nieznane";
      case MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS:
         return "Aktualizuj assety";
      case MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Aktualizuj profile autokonfiguracji";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Aktualizuj shadery Cg";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS:
         return "Aktualizuj cheaty";
      case MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES:
         return "Aktualizuj pliki informacji o rdzeniach";
      case MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES:
         return "Aktualizuj bazy danych";
      case MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Aktualizuj shadery GLSL";
      case MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Aktualizuj nakładki graficzne";
      case MENU_ENUM_LABEL_VALUE_USER:
         return "Użytkownik";
      case MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS:
         return "Ustawienia interfejsu użytkownika";
      case MENU_ENUM_LABEL_VALUE_USER_LANGUAGE:
         return "Język";
      case MENU_ENUM_LABEL_VALUE_USER_SETTINGS:
         return "Ustawienia użytkownika";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "Użyj wbudowanej przeglądarki obrazów";
      case MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Użyj wbudowanego odtwarzacza mediów";
      case MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Użyj tego katalogu>";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Zezwól na obrót";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Auto współczynnik proporcji";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "łczynnika proporcji";
      case MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Wstawiaj czarne klatki";
      case MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Wytnij overscan (restart)";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Wyłącz efekty kompozycji pulpitu";
      case MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER:
         return "Kontroler wideo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER:
         return "Filtr obrazu";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Katalog filtrów wideo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Filtr migotania";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Wyświetlaj wiadomości OSD";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Czcionka wiadomości OSD";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Rozmiar wiadomości OSD";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Wymuś współczynnik proporcji";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Wymuś wyłączenie sRGB FBO";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Opóźnienie klatek";
      case MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Pełny ekran";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma wideo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Włącz nagrywanie z użyciem GPU";
      case MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Wykonuj zrzuty ekranu z wykorzystaniem GPU";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Ścisła synchronizacja GPU";
      case MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Hard GPU Sync Frames";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Współrzędna X dla wiadomości OSD";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Współrzędna Y dla wiadomości OSD";
      case MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Indeks monitora";
      case MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Nagrywaj wraz z zaaplikowanymi filtrami";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Częstotliwość odświeżania";
      case MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Szacowana częstotliwość odświeżania monitora";
      case MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION:
         return "Obrót";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE:
         return "Skala w oknie";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Skaluj w liczbach całkowitych";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS:
         return "Ustawienia wideo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Katalog shaderów";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Liczba przebiegów shadera";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Podgląd parametrów shadera";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Wczytaj preset shaderów";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Parametry shadera menu";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Zapisz preset shadera jako";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Enable Hardware Shared Context";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH:
         return "Sprzętowe filtrowanie dwuliniowe";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Włącz filtr programowy";
      case MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "VSync Swap Interval";
      case MENU_ENUM_LABEL_VALUE_VIDEO_THREADED:
         return "Osobny wątek wideo";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER:
         return "Redukcja migotania";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Set VI Screen Width";
      case MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC:
         return "Synchronizacja pionowa";
      case MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Tryb pełnego ekranu w oknie";
      case MENU_ENUM_LABEL_VALUE_YES:
         return "Tak";
      case MSG_APPENDED_DISK:
         return "Dopisano do dysku";
      case MSG_APPLYING_CHEAT:
         return "Applying cheat changes.";
      case MSG_APPLYING_SHADER:
         return "Aplikowanie shadera";
      case MSG_AUDIO_MUTED:
         return "Wyciszono dźwięk.";
      case MSG_AUDIO_UNMUTED:
         return "Przywrócono dźwięk.";
      case MSG_AUTOSAVE_FAILED:
         return "Nie udało się zainicjalizować automatycznego zapisu.";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Blokowanie nadpisania SRAM";
      case MSG_BYTES:
         return "bajtów";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Nie ustawiono katalogu konfiguracji. Nie można zapisać nowej konfiguracji.";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "Rdzeń nie wspiera zapisów stanu.";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "Nie udało się odczytać pliku treści";
      case MSG_CUSTOM_TIMING_GIVEN:
         return "Wprowadzono niestandardowy timing";
      case MSG_DETECTED_VIEWPORT_OF:
         return "Wykrywanie viewportu";
      case MSG_FAILED_TO:
         return "Nie udało się";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Nie udało się zaaplikować shadera.";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "Nie udało się wczytać treści";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "Nie udało się wczytać pliku filmu";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Nie udało się wczytać nakładki.";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Nie udało się wczytać stanu z";
      case MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY:
         return "Nie udało się usunąć dysku z tacki.";
      case MSG_FAILED_TO_REMOVE_TEMPORARY_FILE:
         return "Nie udało się usunąć pliku tymczasowego";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Nie udało się zapisać SRAM";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Nie udało się zapisać stanu do";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "Nie udało się rozpocząć nagrywania filmu.";
      case MSG_FAILED_TO_START_RECORDING:
         return "Nie udało się rozpocząć nagrywania.";
      case MSG_FAILED_TO_TAKE_SCREENSHOT:
         return "Nie udało się zapisać zrzutu.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Nie udało się przywrócić dźwięku.";
      case MSG_FOUND_SHADER:
         return "Znaleziono shader";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Otrzymano nieprawidłowy indeks dysku.";
      case MSG_GRAB_MOUSE_STATE:
         return "Grab mouse state";
      case MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING:
         return "Ten rdzeń libretro używa renderowania sprzętowego. Konieczne jest użycie nagrywania wraz z zaaplikowanymi shaderami.";
      case MSG_LIBRETRO_ABI_BREAK:
         return "został skompilowany dla innej wersji libretro, różnej od obecnie używanej.";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "Wczytano stan ze slotu #%d.";
      case MSG_LOADED_STATE_FROM_SLOT_AUTO:
         return "Wczytano stan ze slotu #-1 (auto).";
      case MSG_LOADING_CONTENT_FILE:
         return "Wczytywanie pliku treści";
      case MSG_LOADING_STATE:
         return "Wczytywanie stanu";
      case MSG_MOVIE_PLAYBACK_ENDED:
         return "Zakończono odtwarzanie filmu.";
      case MSG_MOVIE_RECORD_STOPPED:
         return "Zatrzymano nagrywanie filmu.";
      case MSG_NETPLAY_FAILED:
         return "Nie udało się zainicjalizować gry sieciowej.";
      case MSG_PAUSED:
         return "Wstrzymano.";
      case MSG_PROGRAM:
         return "RetroArch";
      case MSG_RECEIVED:
         return "otrzymano";
      case MSG_RECORDING_TERMINATED_DUE_TO_RESIZE:
         return "Przerwano nagrywanie z powodu zmiany rozmiaru.";
      case MSG_RECORDING_TO:
         return "Nagrywanie do";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "Przekierowywanie pliku cheatów do";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Przekierowywanie pliku zapisu do";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Przekierowywanie zapisu stanu do"; /* TODO */
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Usunięto dysk z tacki.";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "Usuwanie tymczasowego pliku treści";
      case MSG_RESET:
         return "Reset";
      case MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT:
         return "Ponowne rozpoczęcie nagrywania z powodu reinicjalizacji kontrolera.";
      case MSG_REWINDING:
         return "Przewijanie.";
      case MSG_REWIND_INIT:
         return "Inicjalizowanie bufora przewijania o rozmiarze";
      case MSG_REWIND_INIT_FAILED:
         return "Nie udało się zainicjalizować bufora przewijania. Przewijanie zostanie wyłączone.";
      case MSG_REWIND_INIT_FAILED_THREADED_AUDIO:
         return "Implementacja używa osobnego wątku do przetwarzania dźwięku. Przewijanie nie jest możliwe.";
      case MSG_REWIND_REACHED_END:
         return "W buforze przewijania nie ma więcej danych.";
      case MSG_SAVED_STATE_TO_SLOT:
         return "Zapisano stan w slocie #%d.";
      case MSG_SAVED_STATE_TO_SLOT_AUTO:
         return "Zapisano stan w slocie #-1 (auto).";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "Pomyślnie zapisano do";
      case MSG_SAVING_RAM_TYPE:
         return "Zapisywanie typu RAM";
      case MSG_SAVING_STATE:
         return "Zapisywanie stanu";
      case MSG_SCANNING:
         return "Skanowanie";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "Zakończono skanowanie katalogu";
      case MSG_SENDING_COMMAND:
         return "Wysyłanie komendy";
      case MSG_SHADER:
         return "Shader";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Pomijanie wczytywania SRAM.";
      case MSG_SLOW_MOTION:
         return "Spowolnione tempo.";
      case MSG_SLOW_MOTION_REWIND:
         return "Przewijanie w spowolnionym tempie.";
      case MSG_SRAM_WILL_NOT_BE_SAVED:
         return "SRAM nie zostanie zapisany.";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "Rozpoczynanie odtwarzania filmu.";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Rozpoczynanie zapisu filmu do";
      case MSG_STATE_SIZE:
         return "Rozmiar stanu";
      case MSG_STATE_SLOT:
         return "Slot zapisu";
      case MSG_TAKING_SCREENSHOT:
         return "Zapisywanie zrzutu.";
      case MSG_TO:
         return "do";
      case MSG_UNKNOWN:
         return "Nieznane";
      case MSG_UNPAUSED:
         return "Wznowiono.";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Nieznana komenda";
      case MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED:
         return "Atrapa rdzenia w użyciu. Pomijanie nagrywania.";
      case MSG_VIEWPORT_SIZE_CALCULATION_FAILED:
         return "Obliczanie rozmiaru viewportu nie powiodło się! Kontynuowanie z użyciem surowych danych. Prawdopodobnie nie będzie to działało poprawnie...";
      case MSG_VIRTUAL_DISK_TRAY:
         return "wirtualna tacka."; /* this is funny */
      default:
         break;
   }

   return "null";
}

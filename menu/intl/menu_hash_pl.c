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

#include "../menu_hash.h"
#include "../../configuration.h"

 /* IMPORTANT:
  * For non-english characters to work without proper unicode support,
  * we need this file to be encoded in ISO 8859-2, not UTF-8.
  * If you save this file as UTF-8, you'll break non-english characters
  * (e.g. German "Umlauts" and Portugese diacritics).
 */

const char *menu_hash_to_str_pl(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED:
         return "Automatycznie wczytaj preferowan± nak³adkê";
      case MENU_LABEL_VALUE_UPDATE_CORE_INFO_FILES:
         return "Aktualizuj pliki informacji o rdzeniach";
      case MENU_LABEL_VALUE_DOWNLOAD_CORE_CONTENT:
         return "Pobierz tre¶ci";
      case MENU_LABEL_VALUE_SCAN_THIS_DIRECTORY:
         return "<Przeszukaj ten katalog>";
      case MENU_LABEL_VALUE_SCAN_FILE:
         return "Skanuj plik";
      case MENU_LABEL_VALUE_SCAN_DIRECTORY:
         return "Przeszukaj katalog";
      case MENU_LABEL_VALUE_ADD_CONTENT_LIST:
         return "Dodaj tre¶æ";
      case MENU_LABEL_VALUE_INFORMATION_LIST:
         return "Informacje";
      case MENU_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "U¿yj wbudowanego odtwarzacza mediów";
      case MENU_LABEL_VALUE_CONTENT_SETTINGS:
         return "Szybkie menu";
      case MENU_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Wczytaj tre¶æ";
      case MENU_LABEL_VALUE_LOAD_ARCHIVE:
         return "Wczytaj archiwum";
      case MENU_LABEL_VALUE_OPEN_ARCHIVE:
         return "Otwórz archiwum";
      case MENU_VALUE_ASK_ARCHIVE:
         return "Pytaj";
      case MENU_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Ustawienia prywatno¶ci";
      case MENU_VALUE_HORIZONTAL_MENU: /* Don't change. Breaks everything. (Would be: "Menu poziome") */
         return "Horizontal Menu";
      case MENU_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "Nie znaleziono ustawieñ.";
      case MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "Brak liczników wydajno¶ci.";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Ustawienia kontrolerów";
      case MENU_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Ustawienia konfiguracji";
      case MENU_LABEL_VALUE_CORE_SETTINGS:
         return "Ustawienia rdzenia";
      case MENU_LABEL_VALUE_VIDEO_SETTINGS:
         return "Ustawienia wideo";
      case MENU_LABEL_VALUE_LOGGING_SETTINGS:
         return "Ustawienia logowania";
      case MENU_LABEL_VALUE_SAVING_SETTINGS:
         return "Ustawienia zapisywania";
      case MENU_LABEL_VALUE_REWIND_SETTINGS:
         return "Ustawienia przewijania";
      case MENU_VALUE_SHADER:
         return "Shader";
      case MENU_VALUE_CHEAT:
         return "Cheat";
      case MENU_VALUE_USER:
         return "U¿ytkownik";
      case MENU_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "W³±cz BGM systemu";
      case MENU_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Block Frames";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Wy¶wietl opisy przycisków dla tego rdzenia"; /* UPDATE/FIXME */
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Ukryj nieprzypisane przyciski";
      case MENU_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Wy¶wietlaj wiadomo¶ci OSD";
      case MENU_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Czcionka wiadomo¶ci OSD";
      case MENU_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Rozmiar wiadomo¶ci OSD";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Wspó³rzêdna X dla wiadomo¶ci OSD";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Wspó³rzêdna Y dla wiadomo¶ci OSD";
      case MENU_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "W³±cz filtr programowy";
      case MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Filtr migotania";
      case MENU_VALUE_DIRECTORY_CONTENT:
         return "<Katalog tre¶ci>";
      case MENU_VALUE_UNKNOWN:
         return "Nieznane";
      case MENU_VALUE_DONT_CARE:
         return "Bez znaczenia";
      case MENU_VALUE_LINEAR:
         return "Liniowe";
      case MENU_VALUE_NEAREST:
         return "Najbli¿sze";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "<Domy¶lny>";
      case MENU_VALUE_DIRECTORY_NONE:
         return "<¯aden>";
      case MENU_VALUE_NOT_AVAILABLE:
         return "B/D";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Katalog plików remapowania";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Katalog autokonfiguracji kontrolerów gier";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Katalog konfiguracji nagrywania";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Katalog wyj¶ciowy nagrywania";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Katalog zrzutów";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Katalog historii";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Katalog zapisów tre¶ci";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Katalog zapisanych stanów";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Komendy stdin";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Kontroler wideo";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "W³±cz nagrywanie";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "W³±cz nagrywanie z u¿yciem GPU";
      case MENU_LABEL_VALUE_RECORD_PATH: /* FIXME/UPDATE */
         return "¦cie¿ka nagrywania";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "U¿ywaj katalogu wyj¶ciowego nagrywania";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Konfiguracja nagrywania";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Nagrywaj wraz z zaaplikowanymi filtrami";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Katalog pobranych";
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Katalog assetów";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Katalog dynamicznych tapet";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Katalog przegl±darki";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Katalog zapisanych konfiguracji";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Katalog informacji o rdzeniach";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Katalog rdzeni";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Katalog z kursorami";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Katalog bazy danych tre¶ci";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "Katalog systemu";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Katalog z plikami cheatów";
      case MENU_LABEL_VALUE_CACHE_DIRECTORY: /* UPDATE/FIXME */
         return "Katalog do wypakowywania archiwów";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Katalog filtrów audio";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Katalog shaderów";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Katalog filtrów wideo";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Katalog nak³adek";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Katalog klawiatur ekranowych";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Zamieñ kontrolery w grze sieciowej";
      case MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Tryb obserwatora gry sieciowej";
      case MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Adres IP";
      case MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Port TCP/UDP gry sieciowej";
      case MENU_LABEL_VALUE_NETPLAY_ENABLE:
         return "W³±cz grê sieciow±";
      case MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Opóxnione klatki w grze sieciowej";
      case MENU_LABEL_VALUE_NETPLAY_MODE:
         return "Tryb klienta gry sieciowej";
      case MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Pokazuj ekran startowy";
      case MENU_LABEL_VALUE_TITLE_COLOR:
         return "Kolor tytu³u menu";
      case MENU_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Kolor zaznaczonego elementu menu";
      case MENU_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Wy¶wietl czas/datê";
      case MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Osobny w±tek odbierania danych";
      case MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Zwyk³y kolor elementu menu";
      case MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Poka¿ zaawansowane ustawienia";
      case MENU_LABEL_VALUE_MOUSE_ENABLE:
         return "Obs³uga myszy";
      case MENU_LABEL_VALUE_POINTER_ENABLE:
         return "Obs³uga dotyku";
      case MENU_LABEL_VALUE_CORE_ENABLE:
         return "Wy¶wietlaj nazwê rdzenia";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Pomiñ wykryte DPI";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "W³asne DPI";
      case MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Wstrzymaj wygaszacz";
      case MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Wy³±cz efekty kompozycji pulpitu";
      case MENU_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Wstrzymaj gdy w tle";
      case MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "UI Companion Start On Boot";
      case MENU_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Pasek menu";
      case MENU_LABEL_VALUE_ARCHIVE_MODE:
         return "Archive File Assocation Action";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Komendy sieciowe";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Port dla komend sieciowych";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "W³±cz historiê tre¶ci";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Rozmiar historii tre¶ci";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Szacowana czêstotliwo¶æ od¶wie¿ania monitora";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Atrapa rdzenia przy zatrzymaniu rdzenia";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE: /* TODO/FIXME */
         return "Nie uruchamiaj rdzenia automatycznie";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limituj maksymaln± szybko¶æ dzia³ania";
      case MENU_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Maksymalna szybko¶æ dzia³ania";
      case MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Automatycznie wczytuj pliki remapowania";
      case MENU_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Wspó³czynnik spowolnienia";
      case MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Osobna konfiguracja dla ka¿dego rdzenia";
      case MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Load Override Files Automatically"; /* this one's rather complicated */
      case MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Zapisz konfiguracjê przy wyj¶ciu";
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Sprzêtowe filtrowanie dwuliniowe";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma wideo";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Zezwól na obrót";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "¦cis³a synchronizacja GPU";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "VSync Swap Interval";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "Synchronizacja pionowa";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Osobny w±tek wideo";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Obrót";
      case MENU_LABEL_VALUE_VIDEO_GPU_SCREENSHOT:
         return "Wykonuj zrzuty ekranu z wykorzystaniem GPU";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Wytnij overscan (restart)";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX:
         return "Indeks wspó³czynnika proporcji";
      case MENU_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO:
         return "Auto wspó³czynnik proporcji";
      case MENU_LABEL_VALUE_VIDEO_FORCE_ASPECT:
         return "Wymu¶ wspó³czynnik proporcji";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE:
         return "Czêstotliwo¶æ od¶wie¿ania";
      case MENU_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE:
         return "Wymu¶ wy³±czenie sRGB FBO";
      case MENU_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN:
         return "Tryb pe³nego ekranu w oknie";
      case MENU_LABEL_VALUE_PAL60_ENABLE:
         return "U¿yj trybu PAL60";
      case MENU_LABEL_VALUE_VIDEO_VFILTER:
         return "Redukcja migotania";
      case MENU_LABEL_VALUE_VIDEO_VI_WIDTH:
         return "Set VI Screen Width";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Wstawiaj czarne klatki";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES:
         return "Hard GPU Sync Frames";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Sortuj zapisy w folderach";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Sortuj zapisane stany w folderach";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Pe³ny ekran";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Skala w oknie";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Skaluj w liczbach ca³kowitych";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Liczniki wydajno¶ci";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Poziom logowania rdzenia";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Szczegó³owo¶æ logowania";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Automatyczne wczytywanie stanu";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Indeks automatycznie zapisywanego stanu";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Automatyczny zapis stanu";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Czêstotliwo¶æ automatycznego zapisu SaveRAM";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Nie nadpisuj SaveRAM przy wczytywaniu stanu";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "HW Shared Context Enable";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Uruchom ponownie RetroArch";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nazwa u¿ytkownika";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Jêzyk";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Zezwalaj na dostêp do kamerki";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Zezwalaj na dostêp do lokalizacji";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pauzuj przy wej¶ciu do menu";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Wy¶wietlaj klawiaturê ekranow±";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Wy¶wietlaj nak³adkê";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Indeks monitora";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Opó¼nienie klatek";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Cykl zmian";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Okres turbo";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Próg osi";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "W³±cz remapowanie bindów";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Maksymalna liczba u¿ytkowników";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "W³±cz autokonfiguracjê";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Czêstotliwo¶æ próbkowania d¼wiêku (KHz)";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Audio Maximum Timing Skew";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Liczba przebiegów cheatów";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Zapisz plik remapowania dla rdzenia";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Zapisz plik remapowania dla gry";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Zastosuj zmiany cheatów";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Zastosuj zmiany shaderów";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "W³±cz przewijanie";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Wybierz z kolekcji";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Wybierz plik i dopasuj rdzeñ";
      case MENU_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST:
         return "Wybierz pobrany plik i dopasuj rdzeñ"; /* this makes little sense */
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Wczytaj z ostatnio u¿ywanych";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "W³±cz d¼wiêk";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Wy¶wietlaj FPS";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Wycisz d¼wiêk";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Poziom g³o¶no¶ci (dB)";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "W³±cz synchronizacjê d¼wiêku";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Audio Rate Control Delta";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Liczba przebiegów shadera";
      case MENU_LABEL_VALUE_RDB_ENTRY_SHA1:
         return "SHA1";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Wczytaj konfiguracjê";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "P³ynno¶æ przewijania";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Wczytaj plik remapowania";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "W³±sny wspó³czynnik";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<U¿yj tego katalogu>";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Uruchom tre¶æ";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Opcje dysku rdzenia";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Opcje";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Opcje cheatów rdzenia";
      case MENU_LABEL_VALUE_CHEAT_FILE_LOAD:
         return "Wczytaj plik z cheatami";
      case MENU_LABEL_VALUE_CHEAT_FILE_SAVE_AS:
         return "Zapisz plik z cheatami jako";
      case MENU_LABEL_VALUE_CORE_COUNTERS:
         return "Liczniki rdzenia";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Zapisz zrzut";
      case MENU_LABEL_VALUE_RESUME:
         return "Wznów";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Indeks dysku";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Liczniki frontendu";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Dopisz do obrazu dysku";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "Disk Cycle Tray Status";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Brak wpisów w playli¶cie.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Brak informacji o rdzeniu.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Brak dostêpnych opcji rdzenia.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Brak dostêpnych rdzeni.";
      case MENU_VALUE_NO_CORE:
         return "Brak rdzenia";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Mened¿er bazy danych";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Mened¿er kursorów";
      case MENU_VALUE_MAIN_MENU: /* Don't change. Breaks everything. (Would be: "Menu g³ówne") */
         return "Main Menu"; 
      case MENU_LABEL_VALUE_SETTINGS:
         return "Ustawienia";
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Opu¶æ RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Pooc";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Zapisz now± konfiguracjê";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Restartuj";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Aktualizator rdzeni";
      case MENU_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL:
         return "URL rdzeni buildbota";
      case MENU_LABEL_VALUE_BUILDBOT_ASSETS_URL:
         return "URL assetów buildbota";
      case MENU_LABEL_VALUE_NAVIGATION_WRAPAROUND:
         return "Zawijanie nawigacji";
      case MENU_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
         return "Filtruj wed³ug wspieranych rozszerzeñ";
      case MENU_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
         return "Automatycznie wypakowuj pobierane archiwa";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informacje o systemie";
      case MENU_LABEL_VALUE_ONLINE_UPDATER:
         return "Aktualizator sieciowy";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Informacje o rdzeniu";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Nie znaleziono katalogu.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Brak elementów.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Wczytaj rdzeñ";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Wybierz plik";
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Zamknij";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Ustawienia bazy danych";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Zapisz stan";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Wczytaj stan";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Wznów";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Kontroler wej¶cia";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Kontroler d¼wiêku";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Kontroler gamepadów";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Kontroler resamplera d¼wiêku";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Kontroler nagrywania";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Kontroler menu";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Kontroler kamerki";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Kontroler lokalizacji";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Nie uda³o siê odczytaæ skompresowanego pliku.";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Skala nak³adki";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Preset nak³adki";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Opó¼nienie d¼wiêku (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Urz±dzenie audio";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Preset klawiatury ekranowej";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Nieprze¼roczysto¶æ nak³adki";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Tapeta menu";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Dynamiczna tapeta";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Opcje remapowania kontrolera rdzenia"; /* this is quite bad */
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Opcje shadera";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PARAMETERS:
         return "Podgl±d parametrów shadera";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_PARAMETERS:
         return "Parametry shadera menu";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS:
         return "Zapisz preset shadera jako";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Brak parametrów shadera.";
      case MENU_LABEL_VALUE_VIDEO_SHADER_PRESET:
         return "Wczytaj preset shaderów";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtr obrazu";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Wtyczki audio DSP";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Rozpoczynanie pobierania: ";
      case MENU_VALUE_SECONDS:
         return "sekund";
      case MENU_VALUE_ON: /* Don't change. Needed for XMB atm. (Would be: "W£¡CZONE") */
         return "ON";
      case MENU_VALUE_OFF: /* Don't change. Needed for XMB atm. (Would be: "WY£¡CZONE") */
         return "OFF";
      case MENU_LABEL_VALUE_UPDATE_ASSETS:
         return "Aktualizuj assety";
      case MENU_LABEL_VALUE_UPDATE_CHEATS:
         return "Aktualizuj cheaty";
      case MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES:
         return "Aktualizuj profile autokonfiguracji";
      case MENU_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES_HID:
         return "Aktualizuj profile autokonfiguracji (HID)";
      case MENU_LABEL_VALUE_UPDATE_DATABASES:
         return "Aktualizuj bazy danych";
      case MENU_LABEL_VALUE_UPDATE_OVERLAYS:
         return "Aktualizuj nak³adki graficzne";
      case MENU_LABEL_VALUE_UPDATE_CG_SHADERS:
         return "Aktualizuj shadery Cg";
      case MENU_LABEL_VALUE_UPDATE_GLSL_SHADERS:
         return "Aktualizuj shadery GLSL";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NAME:
         return "Nazwa rdzenia";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_LABEL:
         return "Oznaczenie rdzenia";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_NAME:
         return "Nazwa systemu";
      case MENU_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER:
         return "Producent systemu";
      case MENU_LABEL_VALUE_CORE_INFO_CATEGORIES:
         return "Kategorie";
      case MENU_LABEL_VALUE_CORE_INFO_AUTHORS:
         return "Autorzy";
      case MENU_LABEL_VALUE_CORE_INFO_PERMISSIONS:
         return "Zezwolenia";
      case MENU_LABEL_VALUE_CORE_INFO_LICENSES:
         return "Licencja(-e)";
      case MENU_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS:
         return "Wspierane rozszerzenia";
      case MENU_LABEL_VALUE_CORE_INFO_FIRMWARE:
         return "Firmware";
      case MENU_LABEL_VALUE_CORE_INFO_CORE_NOTES:
         return "Dodatkowe informacje o rdzeniu";
      case MENU_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE:
         return "Data kompilacji";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION:
         return "Wersja git";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES:
         return "W³a¶ciwo¶ci CPU";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER:
         return "Identyfikator frontendu";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME:
         return "Nazwa frontendu";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS:
         return "System operacyjny hosta:";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL:
         return "Poziom RetroRating";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE:
         return "¬ród³o zasilania";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE:
         return "Brak ¼ród³a";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING:
         return "£adowanie";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED:
         return "Na³adowano";
      case MENU_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING:
         return "Na baterii";
      case MENU_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER:
         return "Kontroler wideo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH:
         return "Metryczna szeroko¶æ wy¶wietlacza (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT:
         return "Metryczna wysoko¶æ wy¶wietlacza (mm)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI:
         return "Metryczne DPI wy¶wietlacza";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT:
         return "Wsparcie LibretroDB";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT:
         return "Wsparcie nak³adek graficznych";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT:
         return "Wsparcie interfejsu wiersza poleceñ";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT:
         return "Wsparcie interfejsu komend sieciowych";
      case MENU_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT:
         return "Wsparcie Cocoa";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT:
         return "Wsparcie PNG (RPNG)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT:
         return "Wsparcie SDL1.2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT:
         return "Wsparcie SDL2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT:
         return "Wsparcie OpenGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT:
         return "Wsparcie OpenGL ES";
      case MENU_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT:
         return "Wsparcie wielu w±tków";
      case MENU_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT:
         return "Wsparcie KMS/EGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT:
         return "Wsparcie Udev";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT:
         return "Wsparcie OpenVG";
      case MENU_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT:
         return "Wsparcie EGL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT:
         return "Wsparcie X11";
      case MENU_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT:
         return "Wsparcie Wayland";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT:
         return "Wsparcie XVideo";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT:
         return "Wsparcie ALSA";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT:
         return "Wsparcie OSS";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT:
         return "Wsparcie OpenAL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT:
         return "Wsparcie OpenSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT:
         return "Wsparcie RSound";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT:
         return "Wsparcie RoarAudio";
      case MENU_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT:
         return "Wsparcie JACK";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT:
         return "Wsparcie PulseAudio";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT:
         return "Wsparcie DirectSound";
      case MENU_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT:
         return "Wsparcie XAudio2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT:
         return "Wsparcie Zlib";
      case MENU_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT:
         return "Wsparcie 7zip";
      case MENU_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT:
         return "Wsparcie bibliotek dynamicznych";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT:
         return "Wsparcie Cg";
      case MENU_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT:
         return "Wsparcie GLSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT:
         return "Wsparcie HLSL";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBXML2_SUPPORT:
         return "Wsparcie parsowania XML libxml2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT:
         return "Wsparcie SDL image";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT:
         return "Wsparcie renderowania do tekstury w OpenGL/Direct3D (wielokrotne przebiegi shaderów)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT:
         return "Wsparcie FFmpeg";
      case MENU_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT:
         return "Wsparcie CoreText";
      case MENU_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT:
         return "Wsparcie FreeType";
      case MENU_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT:
         return "Wsparcie Netplay (peer-to-peer)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT:
         return "Wsparcie Pythona (skrypty w shaderach)";
      case MENU_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT:
         return "Wsparcie Video4Linux2";
      case MENU_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT:
         return "Wsparcie libusb";
      case MENU_LABEL_VALUE_YES:
         return "Tak";
      case MENU_LABEL_VALUE_NO:
         return "Nie";
      case MENU_VALUE_BACK:
         return "WSTECZ";
      case MENU_LABEL_VALUE_SCREEN_RESOLUTION:
         return "Rozdzielczo¶æ ekranu";
      case MENU_VALUE_DISABLED:
         return "Wy³±czone";
      case MENU_VALUE_PORT:
         return "Port";
      case MENU_VALUE_NONE:
         return "¯aden";
      case MENU_LABEL_VALUE_RDB_ENTRY_DEVELOPER:
         return "Deweloper";
      case MENU_LABEL_VALUE_RDB_ENTRY_PUBLISHER:
         return "Wydawca";
      case MENU_LABEL_VALUE_RDB_ENTRY_DESCRIPTION:
         return "Opis";
      case MENU_LABEL_VALUE_RDB_ENTRY_NAME:
         return "Nazwa";
      case MENU_LABEL_VALUE_RDB_ENTRY_ORIGIN:
         return "Pochodzenie";
      case MENU_LABEL_VALUE_RDB_ENTRY_FRANCHISE:
         return "Franczyza";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH:
         return "Miesi±c wydania";
      case MENU_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR:
         return "Rok wydania";
      case MENU_VALUE_TRUE:
         return "Prawda";
      case MENU_VALUE_FALSE:
         return "Fa³sz";
      case MENU_VALUE_MISSING:
         return "Brak";
      case MENU_VALUE_PRESENT:
         return "Obecny";
      case MENU_VALUE_OPTIONAL:
         return "Opcjonalny";
      case MENU_VALUE_REQUIRED:
         return "Wymagany";
      case MENU_VALUE_STATUS:
         return "Status";
      case MENU_LABEL_VALUE_AUDIO_SETTINGS:
         return "Ustawienia d¼wiêku";
      case MENU_LABEL_VALUE_INPUT_SETTINGS:
         return "Ustawienia wprowadzania";
      case MENU_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS:
         return "Ustawienia OSD";
      case MENU_LABEL_VALUE_OVERLAY_SETTINGS:
         return "Ustawienia przycisków ekranowych";
      case MENU_LABEL_VALUE_MENU_SETTINGS:
         return "Ustawienia menu";
      case MENU_LABEL_VALUE_MULTIMEDIA_SETTINGS:
         return "Ustawienia multimediów";
      case MENU_LABEL_VALUE_UI_SETTINGS:
         return "Ustawienia interfejsu u¿ytkownika";
      case MENU_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS:
         return "Menu File Browser Settings";
      case MENU_LABEL_VALUE_CORE_UPDATER_SETTINGS:
         return "Ustawienia aktualizatora";
      case MENU_LABEL_VALUE_NETWORK_SETTINGS:
         return "Ustawienia sieci";
      case MENU_LABEL_VALUE_PLAYLIST_SETTINGS:
         return "Ustawienia playlisty";
      case MENU_LABEL_VALUE_USER_SETTINGS:
         return "Ustawienia u¿ytkownika";
      case MENU_LABEL_VALUE_DIRECTORY_SETTINGS:
         return "Ustawienia katalogów";
      case MENU_LABEL_VALUE_RECORDING_SETTINGS:
         return "Ustawienia nagrywania";
      case MENU_LABEL_VALUE_NO_INFORMATION_AVAILABLE:
         return "Brak dostêpnych informacji.";
      case MENU_LABEL_VALUE_INPUT_USER_BINDS:
         return "Wprowad¼ bindy dla u¿ytkownika %u";
      case MENU_VALUE_LANG_ENGLISH:
         return "angielski";
      case MENU_VALUE_LANG_JAPANESE:
         return "japoñski";
      case MENU_VALUE_LANG_FRENCH:
         return "francuski";
      case MENU_VALUE_LANG_SPANISH:
         return "hiszpañski";
      case MENU_VALUE_LANG_GERMAN:
         return "niemiecki";
      case MENU_VALUE_LANG_ITALIAN:
         return "w³oski";
      case MENU_VALUE_LANG_DUTCH:
         return "duñski";
      case MENU_VALUE_LANG_PORTUGUESE:
         return "portugalski";
      case MENU_VALUE_LANG_RUSSIAN:
         return "rosyjski";
      case MENU_VALUE_LANG_KOREAN:
         return "koreañski";
      case MENU_VALUE_LANG_CHINESE_TRADITIONAL:
         return "chiñski (Tradycyjny)";
      case MENU_VALUE_LANG_CHINESE_SIMPLIFIED:
         return "chiñ¶ki (Uproszczony)";
      case MENU_VALUE_LANG_ESPERANTO:
         return "esperanto";
      case MENU_VALUE_LEFT_ANALOG:
         return "Lewy analog";
      case MENU_VALUE_RIGHT_ANALOG:
         return "Prawy analog";
      case MENU_LABEL_VALUE_INPUT_HOTKEY_BINDS:
         return "Input Hotkey Binds";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_SETTINGS:
         return "Frame Throttle Settings";
      case MENU_VALUE_SEARCH:
         return "Szukaj:";
      case MENU_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER:
         return "U¿yj wbudowanej przegl±darki obrazów";
      default:
         break;
   }

   return "null";
}

int menu_hash_get_help_pl(uint32_t hash, char *s, size_t len)
{
   int ret = 0;

   switch (hash)
   {
      case 0:
      default:
         ret = -1;
         break;
   }

   return ret;
}

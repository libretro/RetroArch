#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

MSG_HASH(
      MSG_COMPILER,
      "Kompilator"
      )
MSG_HASH(
      MSG_UNKNOWN_COMPILER,
      "Nieznany kompilator"
      )
MSG_HASH(
      MSG_NATIVE,
      "Natywny"
      )
MSG_HASH(
      MSG_DEVICE_DISCONNECTED_FROM_PORT,
      "Urządzenie zostało odłączone od portu"
      )
MSG_HASH(
      MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
      "Otrzymano nieznane polecenie gry sieciowej"
      )
MSG_HASH(
      MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
      "Plik już istnieje. Zapisywanie do bufora kopii zapasowej"
      )
MSG_HASH(
      MSG_GOT_CONNECTION_FROM,
      "Mam połączenie od: \"%s\""
      )
MSG_HASH(
      MSG_GOT_CONNECTION_FROM_NAME,
      "Mam połączenie od: \"%s (%s)\""
      )
MSG_HASH(
      MSG_PUBLIC_ADDRESS,
      "Adres publiczny"
      )
MSG_HASH(
      MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
      "Brak argumentów i brak wbudowanego menu, wyświetlanie pomocy..."
      )
MSG_HASH(
      MSG_SETTING_DISK_IN_TRAY,
      "Ustawianie dysku w zasobniku"
      )
MSG_HASH(
      MSG_WAITING_FOR_CLIENT,
      "Czekam na klienta ..."
      )
MSG_HASH(
      MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
      "Opuściłeś grę"
      )
MSG_HASH(
      MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
      "Dołączyłeś jako gracz %u"
      )
MSG_HASH(
      MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
      "Dołączyłeś z urządzeniami wejściowymi %.*s"
      )
MSG_HASH(
      MSG_NETPLAY_PLAYER_S_LEFT,
      "Gracz %.*s opuścił grę"
      )
MSG_HASH(
      MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
      "%.*s dołączył jako gracz %u"
      )
MSG_HASH(
      MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
      "%.*s dołączył z urządzeniami wejściowymi %.*s"
      )
MSG_HASH(
      MSG_NETPLAY_NOT_RETROARCH,
      "Próba połączenia online nie powiodła się, ponieważ peer nie działa w trybie RetroArch lub używa starej wersji RetroArch."
      )
MSG_HASH(
      MSG_NETPLAY_OUT_OF_DATE,
      "Grający online korzysta ze starej wersji RetroArch. Nie można połączyć."
      )
MSG_HASH(
      MSG_NETPLAY_DIFFERENT_VERSIONS,
      "OSTRZEŻENIE: Grający online korzysta z innej wersji RetroArch. Jeśli wystąpią problemy, użyjcie tej samej wersji."
      )
MSG_HASH(
      MSG_NETPLAY_DIFFERENT_CORES,
      "Grający online korzysta z innego rdzenia. Nie można połączyć."
      )
MSG_HASH(
      MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
      "OSTRZEŻENIE: Grający online ma inną wersję rdzenia. Jeśli wystąpią problemy, użyj tej samej wersji."
      )
MSG_HASH(
      MSG_NETPLAY_ENDIAN_DEPENDENT,
      "Ten rdzeń nie obsługuje gry online między architekturami tych systemów"
      )
MSG_HASH(
      MSG_NETPLAY_PLATFORM_DEPENDENT,
      "Ten rdzeń nie obsługuje gry online między architekturami"
      )
MSG_HASH(
      MSG_NETPLAY_ENTER_PASSWORD,
      "Wprowadź hasło do serwera gry online:"
      )
MSG_HASH(
      MSG_NETPLAY_INCORRECT_PASSWORD,
      "Niepoprawne hasło"
      )
MSG_HASH(
      MSG_NETPLAY_SERVER_NAMED_HANGUP,
      "\"%s\" Został rozłączony"
      )
MSG_HASH(
      MSG_NETPLAY_SERVER_HANGUP,
      "Klient gry online został odłączony"
      )
MSG_HASH(
      MSG_NETPLAY_CLIENT_HANGUP,
      "Gracz online odłączony"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
      "Nie masz uprawnień do grania"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
      "Brak wolnych miejsc dla graczy"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
      "Żądane urządzenia wejściowe nie są dostępne"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY,
      "Nie można przełączyć do trybu odtwarzania"
      )
MSG_HASH(
      MSG_NETPLAY_PEER_PAUSED,
      "Gracz \"%s\" wstrzymał grę"
      )
MSG_HASH(
      MSG_NETPLAY_CHANGED_NICK,
      "Twój pseudonim został zmieniony na \"%s\""
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
      "Nadaj rdzeniom sprzętowym własny prywatny kontekst. Unikaj konieczności przejmowania zmian stanu sprzętu pomiędzy klatkami."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
      "Włącz animację poziomą menu. Zwiększy wydajność."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_SETTINGS,
      "Dostosuj ustawienia wyglądu ekranu menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
      "Synchronizacja CPU i GPU. Zmniejsza opóźnienie kosztem wydajności."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_THREADED,
      "Poprawia wydajność kosztem opóźnień i częstszego rwania obrazu. Używaj tylko wtedy, gdy nie możesz uzyskać pełnej prędkości w przeciwnym razie."
      )
MSG_HASH(
      MSG_AUDIO_VOLUME,
      "Głośność dźwięku"
      )
MSG_HASH(
      MSG_AUTODETECT,
      "Automatyczne wykrywanie"
      )
MSG_HASH(
      MSG_AUTOLOADING_SAVESTATE_FROM,
      "Automatyczne ładowanie stanu zapisu"
      )
MSG_HASH(
      MSG_CAPABILITIES,
      "Możliwości"
      )
MSG_HASH(
      MSG_CONNECTING_TO_NETPLAY_HOST,
      "Łączenie z hostem gry"
      )
MSG_HASH(
      MSG_CONNECTING_TO_PORT,
      "Łączenie z portem"
      )
MSG_HASH(
      MSG_CONNECTION_SLOT,
      "Gniazdo połączenia"
      )
MSG_HASH(
      MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
      "Przepraszamy, niezaimplementowane: rdzenie, które nie wymagają treści, nie mogą uczestniczyć w grze sieciowej."
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
      "Hasło"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
      "Konta Cheevos"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
      "Nazwa Użytkownika"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
      "Konta"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
      "Lista punktów klienta"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
      "Retro osiągniecia"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
      "Lista osiągnięć"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
      "Lista osiągnięć (Hardcore)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
      "Skanuj zawartość"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
      "Konfiguracje"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ADD_TAB,
      "Importuj zawartość"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
      "Pokoje gry internetowej"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
      "Zapytać"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
      "Assety"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
      "Zablokuj klatki"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
      "Urządzenie audio"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
      "Sterownik audio"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
      "Plugin dzwięku DSP"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
      "Włącz dźwięk"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
      "Filtr audio"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
      "Turbo/Martwa strefa"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
      "Opóźnienie dźwięku (ms)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
      "Maksymalne przesunięcie czasowe dźwięku"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
      "Wycisz dźwięk"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
      "Szybkość wyjścia audio (Hz)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
      "Dynamiczna kontrola szybkości audio"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
      "Sterownik audio resamplera"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
      "Dźwiek"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
      "Synchronizacja dźwięku"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
      "Poziom głośności dźwięku (dB)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
      "Tryb WASAPI"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
      "Format WASAPI Float"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
      "Współdzielony bufor WASAPI"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
      "Autozapis SaveRAM"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
      "Automatyczne zastępowanie plików"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
      "Automatycznie ładuj pliki zmian"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
      "Automatycznie załaduj Shadery"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
      "Z powrotem"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
      "Potwierdź"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
      "Informacje"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
      "Wyjdź"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
      "Przewiń w dół"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
      "Przewiń do góry"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
      "Start"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
      "Przełącz klawiaturę"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
      "Przełącz menu"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
      "Podstawowe ustawienia menu"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
      "Potwierdź/OK"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
      "Informacje"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
      "Wyjdź"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
      "Przewiń do góry"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
      "Domyślne"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
      "Przełącz klawiaturę"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
      "Przełącz menu"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
      "Nie zastępuj SaveRAM przy ładowaniu stanu zapisu"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
      "Włącz Bluetooth"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
      "Adres URL zasobów Buildbot"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
      "Pamięć podręczna"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
      "Zezwalaj na kamerę"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
      "Sterownik kamery"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT,
      "Oszukać"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
      "Zatwierdź zmiany"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
      "Oszukane pliki"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
      "Oszukane pliki"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
      "Załaduj oszukany plik"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
      "Zapisz oszukany plik jako"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
      "Oszukane przepustki"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
      "Opis"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Osiągnięcia trybu hardcore"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
      "Tabele wyników"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
      "Odznaki osiągnięć"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ACHIEVEMENTS,
      "Zablokowane osiągnięcia:"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
      "Zablokowany"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
      "Retro osiągnięcia"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
      "Sprawdź nieoficjalne osiągnięcia"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ACHIEVEMENTS,
      "Odblokowane osiągnięcia:"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
      "Odblokowany"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
      "Hardcore"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
      "Osiągnięcia trybu pełnego"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
      "Automatyczny zrzut ekranu osiągnięć"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
      "Zamknij zawartość"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIG,
      "Konfiguracja"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
      "Załaduj konfigurację"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
      "Konfiguracja"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
      "Zapisz konfigurację przy wyjściu"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
      "Baza danych"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
      "Zawartość"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
      "Rozmiar listy historii")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
      "Zezwalaj na usuwanie wpisów")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
      "Szybkie menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
      "Pobrane pliki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
      "Pobrane pliki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
      "Kody")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
      "Liczniki rdzeniowe")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
      "Pokaż nazwę rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
      "Informacje podstawowe")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
      "Autor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
      "Kategoria")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
      "Etykieta rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
      "Nazwa rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
      "Firmware(s)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
      "Licencja")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
      "Uprawnienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
      "Obsługiwane rozszerzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
      "Producent systemu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
      "Nazwa systemu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
      "Sterowanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_LIST,
      "Załaduj rdzeń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
      "Opcje")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
      "Rdzeń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
      "Rozpocznij rdzeń automatycznie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
      "Automatycznie wyodrębnij pobrane archiwum")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
      "Adres URL rdzeni Buildbot")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
      "Aktualizacja Rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
      "Aktualizacja")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
      "Architektura procesora:")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CPU_CORES,
      "Rdzeń procesora")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
      "Kursor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
      "Menedżer kursorów")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
      "Niestandardowy współczynnik")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
      "Menedżer bazy danych")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
      "Wybór bazy danych")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
      "Usuń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FAVORITES,
      "Katalog startowy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
      "<Treść dir>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
      "<Domyślny>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
      "<Żaden>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
      "Nie znaleziono katalogu.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
      "Szczegóły")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS,
      "Status podajnika cyklu dysku")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
      "Dołącz obraz dysku")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_INDEX,
      "Indeks dysku")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
      "Kontrola dysku")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DONT_CARE,
      "Brak")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
      "Pliki do pobrania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
      "Pobierz rdzeń...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
      "Program do pobierania treści")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE,
      "Zastąp włączone DPI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE,
      "Nadpisz DPI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
      "Sterowniki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
      "Atrapa rdzenia przy zatrzymaniu rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
      "Sprawdź brakujące oprogramowanie sprzętowe przed ładowaniem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
      "Dynamiczne tło")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
      "Dynamiczne tła")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
      "Włącz osiągnięcia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FALSE,
      "Fałszywy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
      "Mnożnik prędkości")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
      "Ulubione")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_SHOW,
      "Wyświetl ilość klatek na sekundę")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
      "Ogranicz maksymalną prędkość działania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
      "Manipulacja klatek")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
      "Liczniki frontendu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
      "Automatycznie ładuj zależne od zawartości opcje rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
      "Utwórz plik opcji gry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
      "Plik opcji gry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP,
      "Pomoc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
      "Rozwiązywanie problemów audio/wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
      "Zmiana nakładki wirtualnego gamepada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
      "Podstawowa kontrola menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_LIST,
      "Pomoc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
      "Ładowanie zawartości")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
      "Skanowanie w poszukiwaniu treści")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
      "Co to jest rdzeń?")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
      "Włącz listę historii")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
      "Historia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
      "Menu poziome")
MSG_HASH(MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
      "Obraz")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INFORMATION,
      "Informacja")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
      "Informacja")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
      "Typ analogowo-cyfrowy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
      "Menu sterowania wszystkich użytkowników")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
      "Lewy analog X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
      "Lewy analog X- (lewy)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
      "Lewy analog X+ (po prawej)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
      "Lewy analog Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
      "Lewy analog Y- (w górę)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
      "Lewy analog Y+ (dół)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
      "Prawo analog X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
      "Prawy analog X- (po lewej)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
      "Prawy analog X+ (po prawej)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
      "Prawo analog Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
      "Prawy analog Y- (w górę)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
      "Prawy analog Y+ (w dół)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
      "Spust")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
      "Przeładowanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
      "Pomocniczy A")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
      "Pomocniczy B")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
      "Pomocniczy C")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
      "Start pistoletu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
      "Wybierz")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
      "D-pad góra")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
      "D-pad dół")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
      "D-pad lewo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
      "D-pad prawo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
      "Włącz autoconfig")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
      "Zamień przyciski menu ok i anuluj")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
      "Powiąż wszystko")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
      "Wszystkie domyślne powiązania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
      "Limit czasu powiązania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
      "Wiązanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
      "Ukryj niezwiązane podstawowe deskryptory wejściowe")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
      "Wyświetl etykiety deskryptorów wejściowych")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
      "Indeks urządzeń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
      "Rodzaj urządzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
      "Indeks myszy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
      "Sterownik wejściowy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
      "Cykl zapisu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
      "Wejściowe powiązania skrótów")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
      "Włączanie mapowania gamepada klawiatury")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
      "Przycisk (po prawej)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
      "Przycisk B (w dół)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
      "W dół D-pad")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
      "Przycisk L2 (spust)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
      "Przycisk L3")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
      "Przycisk L (ramię)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
      "Lewy D-pad")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
      "R2 przycisk (spust)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
      "R3 przycisk")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
      "Przycisk R")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
      "Prawy D-pad")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
      "Wybierz przycisk")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
      "Przycisk Start")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
      "Do góry D-pad")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
      "Przycisk X (u góry)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
      "Przycisk Y (po lewej)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_KEY,
      "(Klucz: %s)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
      "Mysz 1")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
      "Mysz 2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
      "Mysz 3")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
      "Mysz 4")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
      "Mysz 5")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
      "Kółko do góry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
      "Kółko do dołu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
      "Kółko w lewo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
      "Kółko w prawo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
      "Typ odwzorowania klawiatury gamepada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
      "Maksymalna liczba użytkowników")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
      "Menu przełączenia gamepad combo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
      "Indeks kodów -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
      "Indeks kodów +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
      "Włącz kody")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
      "Przełącznik wysuwania dysku")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
      "Następny dysk")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
      "Poprzedni dysk")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
      "Włącz klawisze skrótów")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
      "Szybkie zatrzymanie do przodu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
      "Szybkie przewijanie do przodu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
      "Zawansowane klatki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
      "Przełączanie pełnoekranowe")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
      "Sprawdź przełącznik myszy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
      "Przełącznik ostrości gry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
      "Wczytaj zapis")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
      "Przełączanie menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
      "Przełącznik nagrywania filmu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
      "Przełącznik wyciszania dźwięku")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
      "Gra online przełącza tryb gracz/widz")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
      "Przełączanie klawiatury ekranowej")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
      "Nakładka next")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
      "Wstrzymaj przełącznik")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
      "Zamknij RetroArch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
      "Zresetuj grę")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
      "Przewijanie do tyłu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
      "Zapisz stan")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
      "Zrób zrzut ekranu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
      "Następny moduł cieniujący")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
      "Poprzedni moduł cieniujący")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
      "Zwolnione tempo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
      "Przełącznik spowolnienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
      "Slot zapisu -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
      "Slot zapisu +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
      "Głośność -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
      "Głośność +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
      "Wyświetl nakładkę")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
      "Ukryj nakładkę w menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
      "Pokaż nakładki na nakładce")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
      "Pokaż wejścia portu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
      "Zachowanie typu ankiety")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
      "Wcześnie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
      "Późno")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
      "Normalny")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
      "Preferuj sterowanie dotykiem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
      "Odwzorowanie wejścia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
      "Włącz sporządzanie mapy powiązań na nowo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
      "Zapisz Autoconfig")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
      "Sterowanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
      "Włącz małą klawiaturę")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
      "Włącz dotyk")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
      "Włącz turbo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
      "Okres turbo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
      "Wprowadź powiązania użytkownika %u")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
      "Opóźnienie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
      "Status wewnętrznej pamięci")
MSG_HASH(MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
      "Wprowadź autoconfig")
MSG_HASH(MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
      "Sterownik joypada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
      "Usługi")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
      "chiński (uproszczony)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
      "chiński (tradycyjny)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
      "holenderski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
      "angielski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
      "esperanto")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
      "francuski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
      "niemiecki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
      "włoski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
      "japoński")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
      "koreański")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_POLISH,
      "polski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL,
      "portugalski (brazylia)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL,
      "portugalski (portugalia)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
      "rosyjski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
      "hiszpański")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE,
      "wietnamski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ARABIC,
      "arabski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_GREEK,
      "grščina")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_TURKISH,
      "turščina")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
      "Lewy analog")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
      "Rdzeń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
      "Informacje o rdzeniu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
      "Poziom zalogowania rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LINEAR,
      "Liniowy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
      "Załaduj archiwum")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
      "Załaduj ostatnie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
      "Załaduj zawartość")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_DISC,
      "Load Disc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMP_DISC,
      "Dump Disc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_STATE,
      "Wczytaj zapis")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
      "Zezwalaj na lokalizację")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
      "Sterownik lokalizacji")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
      "Zalogowanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
      "Zalogowanie rozmowy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MAIN_MENU,
      "Menu główne")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MANAGEMENT,
      "Ustawienia bazy danych")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
      "Kolor menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
      "Niebieski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
      "Niebiesko szary")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
      "Ciemny niebieski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
      "Zielony")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
      "NVIDIA Shield")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
      "Czerwony")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
      "Żółty")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
      "Nieprzezroczystość stopki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
      "Nieprzezroczystość nagłówka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
      "Sterownik menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
      "Menu obrotowe przepustnicy częstotliwości wyświetlania klatek")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
      "File Browser")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
      "Menu filtra liniowego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
      "Animacja pozioma")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
      "Wygląd")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
      "Tło")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
      "Nieprzezroczystość tła")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MISSING,
      "Brakujący")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MORE,
      "...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
      "Obsługa myszy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
      "Multimedia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
      "Muzyka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
      "Filtruj nieznane rozszerzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
      "Nawigacja owinięciem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NEAREST,
      "Najbliższy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY,
      "Gra online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
      "Zezwalaj na klientów w trybie slave")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
      "Sprawdź klatki gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
      "Wejściowe klatki opóźnień")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
      "Zakres latencji wejściowych klatek")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
      "Opóźnij klatki gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
      "Odłącz od hosta gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
      "Włącz grę online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
      "Połącz się z hostem gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
      "Uruchom hosta gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
      "Zatrzymaj hosta gry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
      "Adres serwera")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
      "Zeskanuj sieć lokalną")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
      "Włącz klienta gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
      "Nazwa Użytkownika")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
      "Hasło serwera")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
      "Upublicznianie sesji online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
      "Zażądaj urządzenia %u")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
      "Odmów klientów w trybie innym niż slave")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
      "Ustawienia gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
      "Udostępnianie wejścia analogowego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
      "Max")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
      "Średni")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
      "Udostępnianie wejścia cyfrowego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
      "Podziel")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
      "Zahacz")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
      "Głosuj")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
      "Żaden")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
      "Brak preferencji")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
      "Tryb widza gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
      "Tryb bezstanowej gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
      "Hasło spontaniczne serwera")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
      "Widz gry online włączone")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
      "Port TCP gry online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
      "Gra online NAT Traversal")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
      "Polecenia sieciowe")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
      "Port poleceń sieciowych")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
      "Informacje o sieci")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
      "Gamepad sieciowy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
      "Port zdalnej sieci")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
      "Sieć")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO,
      "Nie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NONE,
      "Nic")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
      "N/A")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
      "Brak osiągnięć do wyświetlenia.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE,
      "Bez rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
      "Brak dostępnych rdzeni.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
      "Brak dostępnych podstawowych informacji.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
      "Brak opcji podstawowych rdzenia.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
      "Brak wpisów do wyświetlenia.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
      "Brak historii.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
      "Brak informacji.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ITEMS,
      "Nie ma plików.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
      "Nie znaleziono hostów gry online.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
      "Nie znaleziono sieci.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
      "Brak liczników wydajności.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
      "Brak playlist")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
      "Brak dostępnych pozycji na liście odtwarzania.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
      "Nie znaleziono ustawień.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
      "Brak parametrów modułu cieniującego.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OFF,
      "Wyłącz")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ON,
      "Włącz")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONLINE,
      "Online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
      "Aktualizacja online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
      "Wyświetlacz na ekranie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
      "Nakładka na ekranie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
      "Powiadomienia na ekranie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
      "Przeglądaj archiwum")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OPTIONAL,
      "Opcjonalny")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY,
      "Nakładka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
      "Automatycznie ładuj preferowaną nakładkę")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
      "Nakładka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
      "Krycie nakładki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
      "Ustawienia nakładki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
      "Skala nakładki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
      "Nakładka na ekranie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
      "Użyj trybu PAL60")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
      "Nadrzędny katalog")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
      "Wstrzymaj przy włączonym menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
      "Nie pracuj w tle")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
      "Liczniki wydajności")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
      "Listy odtwarzania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
      "Listy odtwarzania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
      "Listy odtwarzania")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Label Display Mode"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Change how the content labels are displayed in this playlist."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
    "Show full labels"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
    "Remove () content"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
    "Remove [] content"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
    "Remove () and []"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
    "Keep region"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
    "Keep disc index"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
    "Keep region and disc index"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
      "Obsługa dotyku")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PORT,
      "Port")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PRESENT,
      "Obecny")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
      "Prywatność")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
      "Zamknij RetroArch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
      "Obsługa analog")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
      "Ocena BBFC")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
      "Ocena CERO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
      "Tryb kooperacji")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32,
      "CRC32")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
      "Opis")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
      "Deweloper")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
      "Problem magazynu Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
      "Ocena magazynu Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
      "Recenzja magazynu Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
      "Ocena ELSPA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
      "Sprzęt ulepszający")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
      "Ocena ESRB")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
      "Ocena magazynu Famitsu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
      "Seria")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
      "Gatunek muzyczny")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5,
      "MD5")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
      "Nazwa")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
      "Pochodzenie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
      "Ocena PEGI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
      "Wydawca")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
      "Miesiąc wydania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
      "Rok wydania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
      "Obsługa wibracji")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
      "Kod seryjny")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
      "SHA1")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
      "Rozpocznij zawartość")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
      "Ocena TGDB")
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(MENU_ENUM_LABEL_VALUE_REBOOT,
      "Restart (RCM)")
#else
MSG_HASH(MENU_ENUM_LABEL_VALUE_REBOOT,
      "Restart")
#endif
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
      "Konfig. Nagrywania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
      "Wyjście nagrywania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
      "Nagranie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
      "Załaduj konfigurację nagrywania...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
      "Sterowniki nagrywania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
      "Sterownik MIDI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
      "Włącz nagrywanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_PATH,
      "Zapisz wyjścia jako...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
      "Zapisz nagrania w katalogu wyjściowym")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE,
      "Plik zmian")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
      "Załaduj plik zmiany")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
      "Zapisz plik zmiany rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
      "Zapisz plik zmiany gry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
      "Usuń plik zmiany rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
      "Usuń plik zmiany gry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REQUIRED,
      "Wymagany")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
      "Uruchom ponownie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
      "Uruchom RetroArch ponownie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESUME,
      "Wznów")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
      "Wznów zawartość")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
      "Retro klawiatura")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROPAD,
      "Retro pad")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
      "Retro pad w/Analog")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
      "Osiągnięcia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
      "Włącz przewianie do tyłu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
      "Przewijanie granularności")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
      "Przewijanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
      "Przeglądarka plików")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
      "Konfiguracja")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
      "Wyświetl ekran startowy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
      "Prawy Analog")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
      "Dodaj do ulubionych")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
      "Dodaj do ulubionych")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
      "Zresetuj domyślny rdzeń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN,
      "Uruchom")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
      "Uruchom muzykę")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
      "Włącz SAMBA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
      "Zapisz plik")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
      "Zapisz indeks auto stanu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
      "Automatyczne załadowanie stanu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
      "Automatyczne zapisanie stanu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
      "Zapisz stan")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
      "Zapisz miniatury")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
      "Zapisz bieżącą konfigurację")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "Zapisz przesłonięcia rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
      "Zapisz nadpisania katalogu zawartości")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "Zapisz nadpisania gry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
      "Zapisz nową konfigurację")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_STATE,
      "Zapisz stan")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
      "Zapisywanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
      "Skanuj katalog")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_FILE,
      "Zeskanuj plik")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
      "<Zeskanuj ten katalog>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
      "Zrzut ekranu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
      "Rozdzielczość ekranu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SEARCH,
      "Szukaj")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SECONDS,
      "sekundy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SETTINGS,
      "Ustawienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
      "Ustawienia etykiety")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER,
      "Shader")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
      "Zatwierdź zmiany")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
      "Shadery")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
      "Wstążka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
      "Wstążka (uproszczona)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
      "Prosty śnieg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
      "Śnieg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
      "Pokaż ustawienia zaawansowane")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
      "Pokaż ukryte pliki i foldery")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHUTDOWN,
      "Zamknąć")
   MSG_HASH(MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
      "Współczynnik spowolnienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
      "Przejdź do przodu w celu skrócenia czasu oczekiwania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
      "Liczba klatek do uruchomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
      "Sortuj zapisy w folderach")
    MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
      "RunAhead Użyj drugiej instancji")
    MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
      "Ukryj ostrzeżenia RunAhead")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
      "Sortuj zapis w folderach")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
      "Napisz zapis stanów do treści dir")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
      "Napisz zapisuje do treści dir")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
      "Pliki systemowe znajdują się w katalogu treści")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
      "Zapisuj zrzuty ekranu w katalog treści")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
      "Włącz SSH")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_CORE,
      "Rozpocznij rdzeń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
      "Uruchom zdalny Retro pad")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
      "Uruchom procesor wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATE_SLOT,
      "Slot zapisu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATUS,
      "Status")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
      "Polecenia STDIN")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
      "Sugerowane rdzenie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
      "Wstrzymaj wygaszacz ekranu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
      "Włącz System BGM ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
      "System/BIOS")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
      "Informacje o systemie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
      "Obsługa 7zip")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
      "Obsługa ALSA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
      "Data Builda")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
      "Wsparcie Cg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
      "Wsparcie Cocoa")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
      "Obsługa interfejsu poleceń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
      "Obsługa CoreText")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
      "Funkcje procesora")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
      "Wyświetl DPI metryczne")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
      "Wyświetl wysokość metryczną (mm)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
      "Wyświetl szerokość metryczną (mm)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
      "Wsparcie DirectSound")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
      "Obsługa WASAPI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
      "Obsługa dynamicznej biblioteki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
      "Dynamiczne ładowanie biblioteki libretro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
      "Wsparcie EGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
      "Obsługuje renderowanie do tekstury (shadery wieloprzebiegowe) OpenGL/Direct3D")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
      "Wsparcie FFmpeg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
      "Wsparcie FreeType")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
      "Wsparcie STB TrueType")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
      "Identyfikator Frontend")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
      "Nazwa frontendu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
      "System operacyjny Frontend")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
      "Wersja Git")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
      "Wsparcie GLSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
      "Wsparcie HLSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
      "Obsługa JACK")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
      "Obsługa KMS/EGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
      "Wersja Lakka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
      "Obsługa LibretroDB")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
      "Wsparcie Libusb")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
      "Wsparcie Gry online (peer-to-peer)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
      "Obsługa interfejsu dowodzenia sieciowego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
      "Obsługa sieciowego gamepada")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
      "Obsługa OpenAL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
      "Obsługa OpenGL ES")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
      "Obsługa OpenGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
      "Obsługa OpenSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
      "Obsługa OpenVG")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
      "Obsługa OSS")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
      "Obsługa nakładek")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
      "Źródło prądu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
      "Naładowany")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
      "Ładowanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
      "Rozładowywanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
      "Brak źródła")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
      "Obsługa PulseAudio")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
      "Obsługa Pythona (obsługa skryptów w modułach cieniujących)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
      "Obsługa BMP (RBMP)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
      "Poziom Oceny Retro")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
      "Obsługa JPEG (RJPEG)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
      "Obsługa RoarAudio")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
      "Obsługa PNG (RPNG)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
      "Wsparcie RSound")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
      "Obsługa TGA (RTGA)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
      "Wsparcie SDL2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
      "Obsługa obrazów SDL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
      "Wsparcie SDL1.2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
      "Obsługa Slangu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
      "Przewlekanie wsparcia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
      "Wsparcie dla Udev")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
      "Obsługa Video4Linux2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
      "Sterownik kontekstowy wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
      "Wsparcie Vulkan")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
      "Obsługa Wayland")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
      "Wsparcie X11")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
      "Wsparcie XAudio2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
      "Wsparcie XVideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
      "Obsługa Zlib")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
      "Zrób zrzut ekranu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
      "Zadania z wątkami")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS,
      "Miniatury")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
      "Miniatury po lewej stornie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
      "Miniatury dyspozycji pionowej")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
      "Miniatury")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
      "Zaktualizuj miniatury")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
      "Boxarts")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
      "Zrzuty ekranu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
      "Ekrany tytułowe")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
      "Pokaż datę/czas")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TRUE,
      "Prawdziwe")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
      "Włącz Companion UI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
      "Uruchom Companion UI przy włączeniu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
      "Pasek menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
      "Uruchom menu okienkowe przy włączeniu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
      "Włącz menu okienkowe (wymagany restar)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
      "Nie można odczytać skompresowanego pliku.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
      "Cofnij załadowanie stanu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
      "Cofnij zapisanie stanu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNKNOWN,
      "Nieznany")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
      "Aktualizacja")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
      "Zaktualizuj zasoby")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
      "Zaktualizuj profile joypad")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
      "Zaktualizuj shadery CG")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
      "Zaktualizuj kody")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
      "Zaktualizuj podstawowe pliki informacyjne")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
      "Zaktualizuj bazy danych")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
      "Zaktualizuj shadery GLSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
      "Zaktualizuj Lakka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
      "Zaktualizuj nakładki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
      "Zaktualizuj Shadery Slang")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER,
      "Użytkownik")
MSG_HASH(MENU_ENUM_LABEL_VALUE_KEYBOARD,
      "Kbd")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
      "Interfejs użytkownika")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
      "Język")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
      "Użytkownik")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
      "Użyj wbudowanej przeglądarki zdjęć")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
      "Użyj wbudowanego odtwarzacza multimedialnego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
      "<Użyj tego katalogu>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
      "Pozwól na rotacje")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
      "Konfiguruj współczynnik kształtu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
      "Auto. Współczynnik proporcji")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
      "Proporcja obrazu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
      "Wstawianie czarnej klatki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
      "Przytnij Overscan (Przeładuj)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
      "Wyłącz kompozycję pulpitu")
#if defined(_3DS)
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
      "Dolny ekran 3DS")
#endif
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
      "Sterownik wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
      "Filtr wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
      "Filtr wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
      "Filtr migotania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
      "Włącz powiadomienia na ekranie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
      "Czcionka powiadomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
      "Rozmiar powiadomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
      "Wymuś współczynnik proporcji")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
      "Wymuś wyłączenie sRGB FBO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
      "Opóźnienie klatki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
      "Użyj trybu pełnoekranowego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
      "Gamma wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
      "Użyj zapisu GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
      "Włącz zrzut ekranu GPU ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
      "Trudna synchronizacja z GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
      "Twarde klatki do synchronizacji z GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
      "Maksymalne obrazy swapchain")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
      "Pozycja X powiadomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
      "Pozycja Y powiadomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
      "Monitoruj indeks")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
      "Użyj funkcji Nagrywania po filtrowaniu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
      "Odświeżanie w pionie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
      "Szacowana liczba klatek na sekundę na ekranie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
      "Ustaw częstotliwość odświeżania raportowanego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
      "Obrót")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
      "Skala okna")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
      "Skala całkowita")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
      "Wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
      "Moduł cieniujący wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
      "Przepustka Shadera")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
      "Podgląd parametrów modułu cieniującego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
      "Załaduj ustawienia Shader")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
      "Zapisz ustawienie Shadera jako")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
      "Zapis ustawienia podstawowe rdzenia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
      "Zapisz ustawienie zawartości katalogu \"zawartości\"")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
      "Zapisz ustawienie gry")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
      "Włącz udostępniony kontekst sprzętu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
      "Filtrowanie bilinearne")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
      "Włącz filtr miękki")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
      "Interwał wymiany pionowej synchronizacji (V-Sync)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
      "Wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
      "Wątek wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
      "Migotanie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
      "Niestandardowy współczynnik proporcji Wysokość")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
      "Niestandardowy współczynnik proporcji Szerokość")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
      "Niestandardowy współczynnik kształtu X Poz.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
      "Niestandardowy współczynnik kształtu Y Poz.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
      "Ustaw VI szerokość ekranu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
      "Synchronizacja pionowa (V-Sync)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
      "Tryb pełnoekranowy z pełnym ekranem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
      "Szerokość okna")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
      "Wysokość okna")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
      "Pełna szerokość ekranu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
      "Pełnoekranowa wysokość")
MSG_HASH(MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
      "Sterownik Wi-Fi")
MSG_HASH(MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
      "Wi-Fi")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
      "Menu czynnika alfa")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
      "Kolor czcionki czerwony")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
      "Kolor czcionki zielony")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
      "Kolor czcionki niebieski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_FONT,
      "Czcionka menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
      "Niestandardowy ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
      "FlatUI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
      "Monochromia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
      "Odwrócona monochromia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
      "Systematyczny")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
      "NeoActive")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
      "Pixel")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
      "RetroActive")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
      "RetroSystem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
      "Dot-Art")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
      "Kolor menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
      "Zielone jabłko")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
      "Ciemny")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
      "Jasny")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
      "Poranny błękit")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
      "Ciemny fiolet")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
      "Elektryczny błękit")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
      "Złoty")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
      "Czerwone dziedzictwo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
      "Niebieska północ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
      "Zwykły")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
      "Podmorski")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
      "Czerwień wulkaniczna")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
      "Animowany efekt tłą")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR,
      "Współczynnik skali menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
      "Włącz cienie ikony")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
      "Pokaż kartę Historii")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
      "Pokaż kartę Importuj zawartość")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
      "Pokaż kartę Listy odtwarzania")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
      "Pokaż kartę Ulubione")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
      "Pokaż kartę Obraz")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
      "Pokaż kartę Muzyka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
      "Pokaż kartę Ustawienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
      "Pokaż kartę Wideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
      "Pokaż kartę Gry Online")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
      "Układ menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_THEME,
      "Motyw ikon menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_YES,
      "Tak")
MSG_HASH(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
      "Ustawienia Shader")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
      "Włącz lub wyłącz osiągnięcia. Aby uzyskać więcej informacji, odwiedź http://retroachievements.org")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
      "Włącz lub wyłącz nieoficjalne osiągnięcia i/lub funkcje beta do celów testowych.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Włączanie i wyłączanie stanów zapiu, kodów, przewijanie do tyłu, szybko do przodu, pauza i zwolnij dla wszystkich gier.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
      "Włącz lub wyłącz tabele wyników w grze. Nie działa, jeśli tryb Hardcore jest wyłączony.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
      "Włącz lub wyłącz wyświetlanie znaczków na liście osiągnięć.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
      "Włącz lub wyłącz powiadomienia OSD dla osiągnięć.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
      "Automatycznie wykonuj zrzut ekranu po uruchomieniu osiągnięcia.")
MSG_HASH(MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
      "Zmień sterowniki używane przez system.")
MSG_HASH(MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
      "Zmień ustawienia osiągnięć.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_SETTINGS,
      "Zmień ustawienia rdzenia.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
      "Zmień ustawienia nagrywania.")
MSG_HASH(MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
      "Zmień nakładkę ekranu i nakładkę klawiatury oraz ustawienia powiadomień na ekranie.")
MSG_HASH(MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
      "Zmień ustawienia przewijania, przyśpieszania i spowalniania gry.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
      "Zmień ustawienia zapisu.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
      "Zmień ustawienia rejestrowania.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
      "Zmień ustawienia interfejsu użytkownika.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_SETTINGS,
      "Zmień konto, nazwę użytkownika i ustawienia językowe.")
MSG_HASH(MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
      "Zmień ustawienia prywatności.")
 MSG_HASH(MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
      "Zmień ustawienia urządzeń MIDI.")
MSG_HASH(MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
      "Zmień domyślne katalogi, w których znajdują się pliki.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
      "Zmień ustawienia listy odtwarzania.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
      "Skonfiguruj ustawienia serwera i sieci.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
      "Skanuj zawartość i dodaj do bazy danych.")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
      "Zmień ustawienia wyjścia audio.")
MSG_HASH(MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
      "Włącz lub wyłącz bluetooth.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
      "Zapisuje zmiany w pliku konfiguracyjnym przy wyjściu.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
      "Zmień domyślne ustawienia plików konfiguracyjnych.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
      "Zarządzaj i twórz pliki konfiguracyjne.")
MSG_HASH(MENU_ENUM_SUBLABEL_CPU_CORES,
      "Ilość rdzeni procesora.")
MSG_HASH(MENU_ENUM_SUBLABEL_FPS_SHOW,
      "Wyświetla bieżącą liczbę klatek na sekundę na ekranie.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
      "Skonfiguruj ustawienia skrótu.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
      "Kombinacja przycisków gamepada do przełączania menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
      "Zmień ustawienia joypada, klawiatury i myszy.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
      "Skonfiguruj elementy sterujące dla tego użytkownika.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
      "Włącz lub wyłącz rejestrowanie w terminalu.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY,
      "Dołącz lub obsługuj sesję gry online.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
      "Wyszukaj i połącz się z hostami gry online w sieci lokalnej.")
MSG_HASH(MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
      "Wyświetl informacje o systemie.")
MSG_HASH(MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
      "Pobierz dodatki, komponenty i treści do RetroArch.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
      "Włącz lub wyłącz udostępnianie sieciowe folderów.")
MSG_HASH(MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
      "Zarządzaj usługami na poziomie systemu operacyjnego.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
      "Pokaż ukryte pliki/katalogi w przeglądarce plików.")
MSG_HASH(MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
      "Zmień ustawienia związane z wideo, dźwiękiem i opóźnieniem sygnału wejściowego.")
MSG_HASH(MENU_ENUM_SUBLABEL_SSH_ENABLE,
      "Włącz lub wyłącz zdalny dostęp do wiersza poleceń.")
MSG_HASH(MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
      "Zapobiega włączaniu wygaszacza ekranu systemu.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
      "Ustawia rozmiar okna względem głównego rozmiaru wyświetlania. Alternatywnie możesz ustawić szerokość i wysokość okna poniżej dla ustalonego rozmiaru okna.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_LANGUAGE,
      "Ustaw język interfejsu.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
      "Wstawia czarną klatke między klatkami. Przydatny dla użytkowników z ekranami 120Hz, którzy chcą odtwarzać zawartość 60 Hz, aby wyeliminować efekt duchów.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
      "Zmniejsza opóźnienie kosztem większego ryzyka rwania obrazu. Dodaje opóźnienie po V-Sync (w ms).")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
      "Określa liczbę klatek, jaką procesor może uruchomić przed GPU, gdy używana jest 'Trudna synchronizacja z GPU'.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
      "Informuje sterownik wideo, aby jawnie użył określonego trybu buforowania.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
      "Określa, który ekran wyświetlacza ma być używany.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
      "Dokładna szacowana częstotliwość odświeżania ekranu w Hz.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
      "Częstotliwość odświeżania zgłoszona przez sterownik ekranu.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
      "Zmień ustawienia wyjścia wideo.")
MSG_HASH(MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
      "Skanuje sieci bezprzewodowe i nawiązuje połączenie.")
MSG_HASH(MENU_ENUM_SUBLABEL_HELP_LIST,
      "Dowiedz się więcej o tym, jak działa program.")
MSG_HASH(MSG_ADDED_TO_FAVORITES,
      "Dodano do ulubionych")
MSG_HASH(MSG_RESET_CORE_ASSOCIATION,
      "Kojarzenie wejścia do listy odtwarzania zostało zresetowane.")
MSG_HASH(MSG_APPENDED_DISK,
      "Dołączony dysk")
MSG_HASH(MSG_APPLICATION_DIR,
      "Aplikacja Dir")
MSG_HASH(MSG_APPLYING_CHEAT,
      "Stosowanie zmian w kodzie.")
MSG_HASH(MSG_APPLYING_SHADER,
      "Zastosuj Shader")
MSG_HASH(MSG_AUDIO_MUTED,
      "Wyciszenie Dźwięku.")
MSG_HASH(MSG_AUDIO_UNMUTED,
      "Dźwięk nie jest wyciszony.")
MSG_HASH(MSG_AUTOCONFIG_FILE_ERROR_SAVING,
      "Błąd podczas zapisywania pliku autoconfig.")
MSG_HASH(MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
      "Plik Autoconfig został pomyślnie zapisany.")
MSG_HASH(MSG_AUTOSAVE_FAILED,
      "Nie można zainicjować autozapisu.")
MSG_HASH(MSG_AUTO_SAVE_STATE_TO,
      "Automatycznie zapisz stan do")
MSG_HASH(MSG_BLOCKING_SRAM_OVERWRITE,
      "Blokowanie nadpisywania SRAM")
MSG_HASH(MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
      "Wprowadzanie interfejsu poleceń na porcie")
MSG_HASH(MSG_BYTES,
      "bajty")
MSG_HASH(MSG_CANNOT_INFER_NEW_CONFIG_PATH,
      "Nie można określić nowej ścieżki konfiguracji. Użyj bieżącego czasu.")
MSG_HASH(MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Tryb Hardcore włączony, stan zapisu i przewijanie do tyłu były wyłączone.")
MSG_HASH(MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
      "Porównując ze znanymi magicznymi liczbami...")
MSG_HASH(MSG_COMPILED_AGAINST_API,
      "Skompilowany z API")
MSG_HASH(MSG_CONFIG_DIRECTORY_NOT_SET,
      "Nie skonfigurowano katalogu konfiguracyjnego. Nie można zapisać nowej konfiguracji.")
MSG_HASH(MSG_CONNECTED_TO,
      "Połączony z")
MSG_HASH(MSG_CONTENT_CRC32S_DIFFER,
      "Zawartość CRC32 różni się. Nie można używać różnych gier.")
MSG_HASH(MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
      "Pominięto ładowanie treści. Implementacja załaduje ją samodzielnie.")
MSG_HASH(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
      "Rdzeń nie obsługuje stanów zapisywania.")
MSG_HASH(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
      "Plik opcji rdzenia został pomyślnie utworzony.")
MSG_HASH(MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
      "Nie można znaleźć następnego sterownika")
MSG_HASH(MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
      "Nie można znaleźć zgodnego systemu.")
MSG_HASH(MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
      "Nie można znaleźć prawidłowej ścieżki danych")
MSG_HASH(MSG_COULD_NOT_OPEN_DATA_TRACK,
      "Nie można otworzyć ścieżki danych")
MSG_HASH(MSG_COULD_NOT_READ_CONTENT_FILE,
      "Nie można odczytać pliku zawartości")
MSG_HASH(MSG_COULD_NOT_READ_MOVIE_HEADER,
      "Nie można odczytać nagłówka filmu.")
MSG_HASH(MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
      "Nie można odczytać stanu z filmu.")
MSG_HASH(MSG_CRC32_CHECKSUM_MISMATCH,
      "Niezgodność sumy kontrolnej CRC32 między plikiem treści a zapisaną sumą kontrolną w nagłówku pliku odtwarzania. Powtórka najprawdopodobniej zsynchronizuje się podczas odtwarzania.")
MSG_HASH(MSG_CUSTOM_TIMING_GIVEN,
      "Podano niestandardowy czas")
MSG_HASH(MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
      "Dekompresja już trwa.")
MSG_HASH(MSG_DECOMPRESSION_FAILED,
      "Dekompresja nie powiodła się.")
MSG_HASH(MSG_DETECTED_VIEWPORT_OF,
      "Wykryty obszar widoku")
MSG_HASH(MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
      "Nie znaleziono poprawki treści.")
MSG_HASH(MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
      "Odłącz urządzenie od poprawnego portu.")
MSG_HASH(MSG_DISK_CLOSED,
      "Zamknięte")
MSG_HASH(MSG_DISK_EJECTED,
      "Wyrzucony")
MSG_HASH(MSG_DOWNLOADING,
      "Ściąganie")
MSG_HASH(MSG_INDEX_FILE,
      "Index")
MSG_HASH(MSG_DOWNLOAD_FAILED,
      "Pobieranie nie udane")
MSG_HASH(MSG_ERROR,
      "Błąd")
MSG_HASH(MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
      "Rdzeń Libretro wymaga treści, ale nic nie zostało dostarczone.")
MSG_HASH(MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
      "Rdzeń Libretro wymaga specjalnych treści, ale żadne nie zostały dostarczone.")
MSG_HASH(MSG_ERROR_PARSING_ARGUMENTS,
      "Błąd podczas analizowania argumentów.")
MSG_HASH(MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
      "Błąd podczas zapisywania pliku opcji podstawowych.")
MSG_HASH(MSG_ERROR_SAVING_REMAP_FILE,
      "Błąd podczas zapisywania pliku remap.")
MSG_HASH(MSG_ERROR_REMOVING_REMAP_FILE,
      "Błąd podczas usuwania pliku remap.")
MSG_HASH(MSG_ERROR_SAVING_SHADER_PRESET,
      "Błąd podczas zapisywania ustawienia modułu cieniującego.")
MSG_HASH(MSG_EXTERNAL_APPLICATION_DIR,
      "Aplikacja zewnętrzna Dir")
MSG_HASH(MSG_EXTRACTING,
      "Wyodrębnianie")
MSG_HASH(MSG_EXTRACTING_FILE,
      "Wyodrębnianie pliku")
MSG_HASH(MSG_FAILED_SAVING_CONFIG_TO,
      "Nie powiodło się zapisanie konfiguracji do")
MSG_HASH(MSG_FAILED_TO,
      "Nie udało się")
MSG_HASH(MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
      "Nie udało się przyjąć przychodzącego widza.")
MSG_HASH(MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
      "Nie można przydzielić pamięci na poprawioną zawartość...")
MSG_HASH(MSG_FAILED_TO_APPLY_SHADER,
      "Nie można zastosować modułu cieniującego.")
MSG_HASH(MSG_FAILED_TO_BIND_SOCKET,
      "Nie powiodło się wiązanie gniazda.")
MSG_HASH(MSG_FAILED_TO_CREATE_THE_DIRECTORY,
      "Nie można utworzyć katalogu.")
MSG_HASH(MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
      "Nie można wyodrębnić treści ze skompresowanego pliku")
MSG_HASH(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
      "Nie udało się uzyskać pseudonimu od klienta.")
MSG_HASH(MSG_FAILED_TO_LOAD,
      "Nie udało się załadować")
MSG_HASH(MSG_FAILED_TO_LOAD_CONTENT,
      "Nie udało się wczytać treści")
MSG_HASH(MSG_FAILED_TO_LOAD_MOVIE_FILE,
      "Nie udało się załadować pliku filmowego")
MSG_HASH(MSG_FAILED_TO_LOAD_OVERLAY,
      "Nie udało się załadować nakładki.")
MSG_HASH(MSG_FAILED_TO_LOAD_STATE,
      "Nie udało się załadować stanu z")
MSG_HASH(MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
      "Nie udało się otworzyć rdzenia libretro")
MSG_HASH(MSG_FAILED_TO_PATCH,
      "Nie udało się załatać")
MSG_HASH(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
      "Nie można odebrać nagłówka od klienta.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME,
      "Nie udało się odebrać pseudonimu.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
      "Nie udało się odebrać pseudonimu z hosta.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
      "Nie można odebrać rozmiaru pseudonimu z hosta.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
      "Nie można odebrać danych SRAM z hosta.")
MSG_HASH(MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
      "Nie udało się usunąć dysku z zasobnika.")
MSG_HASH(MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
      "Nie udało się usunąć pliku tymczasowego")
MSG_HASH(MSG_FAILED_TO_SAVE_SRAM,
      "Nie udało się zapisać SRAM")
MSG_HASH(MSG_FAILED_TO_SAVE_STATE_TO,
      "Nie udało się zapisać stanu do")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME,
      "Nie udało się wysłać pseudonimu.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_SIZE,
      "Nie udało się wysłać rozmiaru pseudonimu.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
      "Nie udało się wysłać pseudonimu do klienta.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
      "Nie udało się wysłać pseudonimu do hosta.")
MSG_HASH(MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
      "Nie udało się wysłać danych SRAM do klienta.")
MSG_HASH(MSG_FAILED_TO_START_AUDIO_DRIVER,
      "Nie można uruchomić sterownika audio. Będzie kontynuowany bez dźwięku.")
MSG_HASH(MSG_FAILED_TO_START_MOVIE_RECORD,
      "Nie można uruchomić nagrywania filmu.")
MSG_HASH(MSG_FAILED_TO_START_RECORDING,
      "Nie można rozpocząć nagrywania.")
MSG_HASH(MSG_FAILED_TO_TAKE_SCREENSHOT,
      "Nie udało się zrobić zrzutu ekranu.")
MSG_HASH(MSG_FAILED_TO_UNDO_LOAD_STATE,
      "Nie udało się cofnąć stanu obciążenia.")
MSG_HASH(MSG_FAILED_TO_UNDO_SAVE_STATE,
      "Nie udało się cofnąć stanu zapisania.")
MSG_HASH(MSG_FAILED_TO_UNMUTE_AUDIO,
      "Nie udało się wyłączyć dźwięku.")
MSG_HASH(MSG_FATAL_ERROR_RECEIVED_IN,
      "Błąd krytyczny odebrany w")
MSG_HASH(MSG_FILE_NOT_FOUND,
      "Nie znaleziono pliku")
MSG_HASH(MSG_FOUND_AUTO_SAVESTATE_IN,
      "Znajdź stan automatycznego zapisywania w")
MSG_HASH(MSG_FOUND_DISK_LABEL,
      "Znaleziono etykietę dysku")
MSG_HASH(MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
      "Znaleziono pierwszą ścieżkę danych w pliku")
MSG_HASH(MSG_FOUND_LAST_STATE_SLOT,
      "Znaleziono ostatni stan automatu")
MSG_HASH(MSG_FOUND_SHADER,
      "Znaleziony moduł cieniujący")
MSG_HASH(MSG_FRAMES,
      "Klatki")
MSG_HASH(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
      "Opcje gry: podstawowe opcje zależne od danej gry")
MSG_HASH(MSG_GOT_INVALID_DISK_INDEX,
      "Otrzymałem nieprawidłowy indeks dysku.")
MSG_HASH(MSG_GRAB_MOUSE_STATE,
      "Sprawdź stan myszy")
MSG_HASH(MSG_GAME_FOCUS_ON,
      "Skup się na grze")
MSG_HASH(MSG_GAME_FOCUS_OFF,
      "Nie skupiaj się na grze")
MSG_HASH(MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
      "Rdzeń Libretro jest renderowany sprzętowo. Musi także korzystać z nagrywania po cieniowaniu.")
MSG_HASH(MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
      "Skomplikowana suma kontrolna nie pasuje do CRC32.")
MSG_HASH(MSG_INPUT_CHEAT,
      "Wejdź w kod")
MSG_HASH(MSG_INPUT_CHEAT_FILENAME,
      "Wprowadź nazwę kodu")
MSG_HASH(MSG_INPUT_PRESET_FILENAME,
      "Wprowadź wstępnie ustawioną nazwę pliku")
MSG_HASH(MSG_INPUT_RENAME_ENTRY,
      "Zmień nazwę tytułu")
MSG_HASH(MSG_INTERFACE,
      "Interfejs")
MSG_HASH(MSG_INTERNAL_STORAGE,
      "Pamięć wewnętrzna")
MSG_HASH(MSG_REMOVABLE_STORAGE,
      "Magazyn wymienny")
MSG_HASH(MSG_INVALID_NICKNAME_SIZE,
      "Nieprawidłowy rozmiar pseudonimu.")
MSG_HASH(MSG_IN_BYTES,
      "w bajtach")
MSG_HASH(MSG_IN_GIGABYTES,
      "w gigabajtach")
MSG_HASH(MSG_IN_MEGABYTES,
      "w megabajtach")
MSG_HASH(MSG_LIBRETRO_ABI_BREAK,
      "jest skompilowany przeciwko innej wersji libretro niż ta implementacja libretro.")
MSG_HASH(MSG_LIBRETRO_FRONTEND,
      "Frontend dla libretro")
MSG_HASH(MSG_LOADED_STATE_FROM_SLOT,
      "Załadowany stan z gniazda #%d.")
MSG_HASH(MSG_LOADED_STATE_FROM_SLOT_AUTO,
      "Załadowany stan z gniazda #-1 (automatyczny).")
MSG_HASH(MSG_LOADING,
      "Ładuję")
MSG_HASH(MSG_FIRMWARE,
      "Brak jednego lub więcej plików oprogramowania układowego")
MSG_HASH(MSG_LOADING_CONTENT_FILE,
      "Ładowanie pliku zawartości")
MSG_HASH(MSG_LOADING_HISTORY_FILE,
      "Ładowanie pliku historii")
MSG_HASH(MSG_LOADING_STATE,
      "Ładowanie stanu")
MSG_HASH(MSG_MEMORY,
      "Pamięć")
MSG_HASH(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
      "Plik filmu nie jest prawidłowym plikiem BSV1.")
MSG_HASH(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
      "Wygląda na to, że format filmu ma inną wersję serializera. Najprawdopodobniej zawiedzie.")
MSG_HASH(MSG_MOVIE_PLAYBACK_ENDED,
      "Zakończono odtwarzanie filmu.")
MSG_HASH(MSG_MOVIE_RECORD_STOPPED,
      "Zatrzymywanie nagrywania filmu.")
MSG_HASH(MSG_NETPLAY_FAILED,
      "Nie udało się zainicjować gry sieciowej.")
MSG_HASH(MSG_NO_CONTENT_STARTING_DUMMY_CORE,
      "Bez zawartości, zaczynając sztuczny rdzeń.")
MSG_HASH(MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
      "Żaden stan zapisu nie został jeszcze nadpisany.")
MSG_HASH(MSG_NO_STATE_HAS_BEEN_LOADED_YET,
      "Żaden stan nie został jeszcze załadowany.")
MSG_HASH(MSG_OVERRIDES_ERROR_SAVING,
      "Błąd podczas zapisywania zastąpień.")
MSG_HASH(MSG_OVERRIDES_SAVED_SUCCESSFULLY,
      "Przesłonięcia zostały pomyślnie zapisane.")
MSG_HASH(MSG_PAUSED,
      "Wstrzymane.")
MSG_HASH(MSG_PROGRAM,
      "RetroArch")
MSG_HASH(MSG_READING_FIRST_DATA_TRACK,
      "Czytanie pierwszej ścieżki danych...")
MSG_HASH(MSG_RECEIVED,
      "Odebrane")
MSG_HASH(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
      "Nagrywanie zakończyło się z powodu zmiany rozmiaru.")
MSG_HASH(MSG_RECORDING_TO,
      "Nagrywanie do")
MSG_HASH(MSG_REDIRECTING_CHEATFILE_TO,
      "Przekierowuję plik oszustów do")
MSG_HASH(MSG_REDIRECTING_SAVEFILE_TO,
      "Przekierowanie pliku zapisu do")
MSG_HASH(MSG_REDIRECTING_SAVESTATE_TO,
      "Przekierowuję stan zapisywania do")
MSG_HASH(MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
      "Plik remap został pomyślnie zapisany.")
MSG_HASH(MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
      "Plik remap został pomyślnie usunięty.")
MSG_HASH(MSG_REMOVED_DISK_FROM_TRAY,
      "Usunięto dysk z zasobnika.")
MSG_HASH(MSG_REMOVING_TEMPORARY_CONTENT_FILE,
      "Usuwanie tymczasowego pliku zawartości")
MSG_HASH(MSG_RESET,
      "Reset")
MSG_HASH(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
      "Ponowne uruchamianie nagrywania z powodu ponownego uruchomienia sterownika.")
MSG_HASH(MSG_RESTORED_OLD_SAVE_STATE,
      "Przywrócono stary stan zapisywania.")
MSG_HASH(MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
      "Shadery: przywracanie domyślnego ustawienia modułu cieniującego do")
MSG_HASH(MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
      "Przywracanie zapisywania katalogu plików do")
MSG_HASH(MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
      "Przywracanie katalogu stanu zapisu do")
MSG_HASH(MSG_REWINDING,
      "Przewijanie.")
MSG_HASH(MSG_REWIND_INIT,
      "Inicjowanie bufora przewijania z rozmiarem")
MSG_HASH(MSG_REWIND_INIT_FAILED,
      "Nie można zainicjować buforu przewijania. Przewijanie zostanie wyłączone.")
MSG_HASH(MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
      "Implementacja wykorzystuje nagrany dźwięk. Nie można użyć do przewijania.")
MSG_HASH(MSG_REWIND_REACHED_END,
      "Osiągnięty koniec bufora przewijania.")
MSG_HASH(MSG_SAVED_NEW_CONFIG_TO,
      "Zapisano nową konfigurację do")
MSG_HASH(MSG_SAVED_STATE_TO_SLOT,
      "Zapisany stan do gniazda #%d.")
MSG_HASH(MSG_SAVED_STATE_TO_SLOT_AUTO,
      "Zapisany stan do gniazda #-1 (auto).")
MSG_HASH(MSG_SAVED_SUCCESSFULLY_TO,
      "Zapisano pomyślnie do")
MSG_HASH(MSG_SAVING_RAM_TYPE,
      "Zapisywanie typu pamięci RAM")
MSG_HASH(MSG_SAVING_STATE,
      "Zapisywanie stanu")
MSG_HASH(MSG_SCANNING,
      "Skanowwanie")
MSG_HASH(MSG_SCANNING_OF_DIRECTORY_FINISHED,
      "Skanowanie katalogu zakończone")
MSG_HASH(MSG_SENDING_COMMAND,
      "Wysyłam polecenie")
MSG_HASH(MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
      "Kilka łatek jest jawnie zdefiniowanych, ignorując wszystko...")
MSG_HASH(MSG_SHADER,
      "Shader")
MSG_HASH(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
      "Preset Shadera został pomyślnie zapisany.")
MSG_HASH(MSG_SKIPPING_SRAM_LOAD,
      "Pomijanie obciążenia SRAM.")
MSG_HASH(MSG_SLOW_MOTION,
      "Zwolnione tempo.")
MSG_HASH(MSG_FAST_FORWARD,
      "Szybko na przód.")
MSG_HASH(MSG_SLOW_MOTION_REWIND,
      "Zwolnij tempo.")
MSG_HASH(MSG_SRAM_WILL_NOT_BE_SAVED,
      "SRAM nie zostanie zapisany.")
MSG_HASH(MSG_STARTING_MOVIE_PLAYBACK,
      "Rozpoczynanie odtwarzania filmu.")
MSG_HASH(MSG_STARTING_MOVIE_RECORD_TO,
      "Rozpoczęcie nagrywania filmu na")
MSG_HASH(MSG_STATE_SIZE,
      "Wielkość państwa")
MSG_HASH(MSG_STATE_SLOT,
      "Stan gniazda")
MSG_HASH(MSG_TAKING_SCREENSHOT,
      "Zrzut ekranu.")
MSG_HASH(MSG_TO,
      "do")
MSG_HASH(MSG_UNDID_LOAD_STATE,
      "Anulować stan zpisu.")
MSG_HASH(MSG_UNDOING_SAVE_STATE,
      "Cofanie stanu zapisu")
MSG_HASH(MSG_UNKNOWN,
      "Nieznany")
MSG_HASH(MSG_UNPAUSED,
      "Anulowano.")
MSG_HASH(MSG_UNRECOGNIZED_COMMAND,
      "Polecenie nierozpoznane")
MSG_HASH(MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
      "Używanie nazwy rdzenia dla nowej konfiguracji.")
MSG_HASH(MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
      "Używanie rdzenia manekinowego libretro. Pomijanie nagrania.")
MSG_HASH(MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
      "Podłącz urządzenie z prawidłowego portu.")
MSG_HASH(MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
      "Odłączanie urządzenia od portu")
MSG_HASH(MSG_VALUE_REBOOTING,
      "Ponowne uruchamianie...")
MSG_HASH(MSG_VALUE_SHUTTING_DOWN,
      "Wyłączanie...")
MSG_HASH(MSG_VERSION_OF_LIBRETRO_API,
      "Wersja API libretro")
MSG_HASH(MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
      "Obliczenie wielkości wyświetlania nie powiodło się! Będzie nadal korzystać z nieprzetworzonych danych. Prawdopodobnie to nie zadziała ...")
MSG_HASH(MSG_VIRTUAL_DISK_TRAY,
      "Wirtualna taca dysku.")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
      "Pożądane opóźnienie dźwięku w milisekundach. Może nie być honorowane, jeśli sterownik audio nie może zapewnić określonego opóźnienia.")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MUTE,
      "Wycisz/włącz dźwięk.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
      "Pomaga wygładzać niedoskonałości w synchronizacji czasu podczas synchronizowania dźwięku i obrazu. Pamiętaj, że jeśli jest wyłączona, właściwa synchronizacja jest prawie niemożliwa do uzyskania."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
      "Zezwalaj lub nie zezwalaj na dostęp do kamery przez rdzenie."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
      "Zezwalaj lub nie zezwalaj na dostęp do usług lokalizacyjnych przez rdzenie."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
      "Maksymalna liczba użytkowników obsługiwanych przez RetroArch."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
      "Sprawdź, w jaki sposób odbywa się odpytywanie wejścia wewnątrz RetroArch. Ustawienie na 'Wcześnie' lub 'Późno' może spowodować mniejsze opóźnienie, w zależności od konfiguracji."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
      "Pozwala dowolnemu użytkownikowi kontrolować menu. Jeśli wyłączone, tylko użytkownik 1 może kontrolować menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
      "Głośność dźwięku (w dB). 0 dB to normalna głośność i nie jest stosowane żadne wzmocnienie."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
      "Pozwól sterownikowi WASAPI na przejęcie wyłącznej kontroli nad urządzeniem audio. Jeśli jest wyłączona, zamiast tego użyje trybu wspólnego."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
      "Użyj formatu float dla sterownika WASAPI, jeśli jest on obsługiwany przez twoje urządzenie audio."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
      "Pośrednia długość bufora (w klatkach), gdy używany jest sterownik WASAPI w trybie współdzielonym."
      )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Synchronizuj dźwięk. Zalecane."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Ilość sekund oczekiwania na przejście do następnej więzi."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Ilość sekund do przechowywania danych wejściowych, aby je powiązać."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Opisuje okres, w którym przełączane są przyciski z włączoną funkcją turbo. Liczby są opisane w klatkach."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "Opisuje, jak długi powinien być okres przycisku z włączoną funkcją turbo. Liczby są opisane w klatkach."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Synchronizuje wyjście wideo karty graficznej z częstotliwością odświeżania ekranu. Zalecana."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Pozwól rdzeniom ustawić rotację. Po wyłączeniu żądania obrotu są ignorowane. Przydatny w przypadku konfiguracji, w których ręcznie obraca się ekran."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Niektóre rdzenie mogą mieć funkcję wyłączania. Jeśli jest włączona, uniemożliwi rdzeniu wyłączenie RetroArch. Zamiast tego ładuje fałszywy rdzeń."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Przed próbą załadowania zawartości sprawdź, czy wszystkie wymagane oprogramowanie układowe jest obecne."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Pionowa częstotliwość odświeżania ekranu. Służy do obliczenia odpowiedniej wartości wejściowej audio. UWAGA: Zostanie to zignorowane, jeśli włączone jest 'Wideo wątkowe'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Włącz wyjście audio."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Maksymalna zmiana częstotliwości wejściowej audio. Zwiększenie to umożliwia bardzo duże zmiany w taktowaniu kosztem niedokładnego nachylenia dźwięku (np. Uruchamianie rdzeni PAL na wyświetlaczach NTSC)."
   )
MSG_HASH(
   MSG_FAILED,
   "nie udało się"
   )
MSG_HASH(
   MSG_SUCCEEDED,
   "udało się"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "nie skonfigurowane"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "nieskonfigurowane, korzystające z trybu awaryjnego"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Baza danych kursora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Baza danych - Filtr: Developer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Baza danych - Filtr: Wydawca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Wyłączony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Włączone"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Ścieżka historii treści"
   )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
      "Baza danych - Filtr: Pochodzenie")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
      "Baza danych - Filtr: Franczyza")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
      "Baza danych - Filtr: Ocena ESRB")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
      "Baza danych - Filtr: Ocena ELSPA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
      "Baza danych - Filtr: Ocena PEGI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
      "Baza danych - Filtr: Ocena CERO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
      "Baza danych - Filtr: Ocena BBFC")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
      "Baza danych - Filtr: Maks. Liczba użytkowników")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
      "Baza danych - Filtr: Data wydania na miesiąc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
      "Baza danych - Filtr: Data wydania na rok")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
      "Baza danych - Filter: Problem z magazynem Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
      "Baza danych - Filtr: Ocena magazynu Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
      "Informacje o bazie danych")
MSG_HASH(MSG_WIFI_SCAN_COMPLETE,
      "Skanowanie Wi-Fi zostało zakończone.")
MSG_HASH(MSG_SCANNING_WIRELESS_NETWORKS,
      "Skanowanie sieci bezprzewodowych...")
MSG_HASH(MSG_NETPLAY_LAN_SCAN_COMPLETE,
      "Zakończono skanowanie gry online.")
MSG_HASH(MSG_NETPLAY_LAN_SCANNING,
      "Skanowanie w poszukiwaniu hostów gry online...")
MSG_HASH(MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
      "Wstrzymaj grę, gdy RetroArch nie jest aktywnym oknem.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
      "Włącz lub wyłącz kompozycję (tylko system Windows).")
MSG_HASH(MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
      "Włącz lub wyłącz listę najnowszych odtwarzanych, zdjęć, muzyki i filmów.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
      "Ogranicz liczbę wpisów na liście odtwarzania dla gier, zdjęć, muzyki i filmów.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
      "Zunifikowane sterowanie menu")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
      "Użyj tych samych elementów sterujących zarówno do menu, jak i do gry. Dotyczy klawiatury.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
      "Pokaż komunikaty na ekranie.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
      "Użytkownik %d Zdalny Włącz")
MSG_HASH(MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
      "Pokaż poziom naładowania baterii")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SELECT_FILE,
      "Wybierz plik")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FILTER,
      "Filtr")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCALE,
      "Skala")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
      "Gra online rozpocznie się po załadowaniu zawartości.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
      "Nie można znaleźć odpowiedniego pliku rdzenia lub treści, załaduj ręcznie.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
      "Przeglądaj adres URL"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_URL,
      "Ścieżka adresu URL"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_START,
      "Start"
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
      "Bokeh")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
      "Płatek śniegu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
      "Odśwież listę pokoi")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
      "Przezwisko: %s")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
      "Przezwisko (lan): %s")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
      "Znaleziono zgodną zawartość")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
      "Odcina kilka pikseli wokół krawędzi obrazu, zwykle pozostawionych pustych przez programistów, które czasami zawierają także piksele śmieci.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
      "Dodaje lekkie rozmycie obrazu, aby odciąć krawędzie twardych pikseli. Ta opcja ma bardzo mały wpływ na wydajność.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FILTER,
      "Zastosuj filtr wideo zasilany przez procesor. UWAGA: Może przyjść na koszt wysokiej wydajności. Niektóre filtry wideo działają tylko w przypadku rdzeni, które używają kolorów 32-bitowych lub 16-bitowych.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
      "Wprowadź nazwę użytkownika swojego konta RetroAchievements.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
      "Wprowadź hasło do swojego konta RetroAchievements.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
      "Wprowadź tutaj swoją nazwę użytkownika. Będzie to wykorzystywane między innymi do gry internetowej.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
      "Przechwyć obraz po zastosowaniu filtrów (ale nie shaderów). Twój film będzie wyglądał tak fantazyjnie jak to, co widzisz na ekranie.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_LIST,
      "Wybierz, którego rdzenia użyć.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
      "Wybierz, które treści chcesz rozpocząć.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
      "Pokaż interfejsy sieciowe i powiązane adresy IP.")
MSG_HASH(MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
      "Pokaż informacje dotyczące konkretnego urządzenia.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
      "Ustaw niestandardowy rozmiar szerokości okna wyświetlacza. Pozostawienie go na 0 spowoduje przeskalowanie okna tak dużego, jak to możliwe.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
      "Ustaw niestandardowy rozmiar wysokości okna wyświetlacza. Pozostawienie go na 0 spowoduje przeskalowanie okna tak dużego, jak to możliwe.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
      "Ustaw niestandardową szerokość w trybie pełnoekranowym bez okien. Pozostawienie go na 0 spowoduje użycie rozdzielczości pulpitu.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
      "Ustaw niestandardowy rozmiar wysokości dla trybu pełnoekranowego bez okien. Pozostawienie go na 0 spowoduje użycie rozdzielczości pulpitu")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
      "Określ niestandardową pozycję osi X dla tekstu na ekranie.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
      "Określ niestandardową pozycję osi Y dla tekstu na ekranie.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
      "Określ rozmiar czcionki w punktach.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
      "Ukryj nakładkę w menu i pokaż ją ponownie po wyjściu z menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
      "Pokaż wejścia klawiatury/kontrolera na nakładce ekranowej.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
      "Wybierz port dla nakładki, aby usłyszeć, czy opcja Pokaż nakładki na nakładkę jest włączona.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
      "Tutaj pojawi się skanowana zawartość."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
      "Skaluje wideo tylko w krokach całkowitych. Rozmiar bazy zależy od raportowanej przez system geometrii i współczynnika kształtu. Jeśli 'Force Aspect' nie jest ustawione, X/Y będzie liczbą całkowitą skalowaną niezależnie."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
      "Zrzuty ekranu z zacienionych materiałów GPU, jeśli są dostępne."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
      "Wymusza pewien obrót ekranu. Obrót jest dodawany do rotacji, które ustawia rdzeń."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
      "Przymusowe wyłączenie obsługi sRGB FBO. Niektóre sterowniki Intel OpenGL w systemie Windows mają problemy z wideo z obsługą sRGB FBO, jeśli jest włączona. Włączenie tego może obejść to."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
      "Zacznij w trybie pełnoekranowym. Można zmienić w czasie wykonywania."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
      "Jeśli jest to tryb pełnoekranowy, korzystaj z trybu pełnoekranowego w oknie."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
      "Rejestruje wyjście z zacienionego materiału GPU, jeśli jest dostępne."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
      "Podczas zapisywania stanu, wskaźnik stanu zapisu jest automatycznie zwiększany przed zapisaniem. Podczas ładowania zawartości indeks zostanie ustawiony na najwyższy istniejący indeks."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
      "Blokuj Zapisz pamięć RAM przed nadpisaniem podczas ładowania stanów zapisu. Może potencjalnie prowadzić do zbugowanie gier."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
      "Maksymalna prędkość odtwarzania zawartości przy użyciu przyśpieszenia (np. 5.0x przy zawartości 60 klatek na sekundę = 300 klatek na sekundę). Ustawienie na 0.0x oznacza brak limitu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
      "W zwolnionym tempie zawartość spowalnia o określony/ustawiony współczynnik."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
      "Uruchamiaj logikę rdzeniową lub więcej klatek z wyprzedzeniem, a następnie ładuj stan z powrotem, aby zmniejszyć postrzegane opóźnienie wejściowe."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
      "Liczba klatek do uruchomienia. Powoduje problemy z grą, takie jak drganie, jeśli przekroczysz liczbę klatek wystąpią opóźnienia wewnętrzne w grze."
      )
    MSG_HASH(
      MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
      "Zastosuj drugą instancję rdzenia RetroArch, aby uruchomić z wyprzedzeniem. Zapobiega problemom z dźwiękiem ze względu na stan ładowania."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
      "Ukrywa komunikat ostrzegawczy, który pojawia się podczas korzystania z RunAhead, a rdzeń nie obsługuje stanów składowania."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_REWIND_ENABLE,
      "Włącz przewijanie. To zajmie wydajność podczas gry."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
      "Podczas przewijania określonej liczby klatek można przewijać kilka klatek na raz, zwiększając prędkość przewijania do tyłu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
      "Ustawia poziom logu dla rdzeni. Jeśli poziom dziennika wydany przez rdzeń jest poniżej tej wartości, jest on ignorowany."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
      "Włącz liczniki wydajności dla RetroArch (i rdzeni)."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
      "Automatycznie tworzy stan zapisywania na końcu środowiska wykonawczego RetroArch. RetroArch automatycznie załaduje ten stan zapisu Auto Load State jest włączony."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
      "Automatycznie ładuj stan automatycznego zapisywania podczas uruchamiania."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
      "Pokaż miniatury stanów zapisu w menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
      "Automatyczne zapisywanie nieulotnej pamięci RAM w regularnych odstępach czasu. Jest to domyślnie wyłączone, chyba że ustawiono inaczej. Odstęp mierzony jest w sekundach. Wartość 0 wyłącza automatyczne zapisywanie."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
      "Jeśli włączone, przesłania powiązania wejściowe z remapped reminds zestaw dla bieżącego rdzenia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
      "Włącz automatyczne wykrywanie wejścia. Spróbuje skonfigurować joypad, styl Plug-and-Play."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
      "Zamień przyciski na OK/Anuluj. Wyłączone to orientacja japońskiego przycisku, włączona jest orientacja zachodnia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
      "Jeśli wyłączone, zawartość będzie nadal działać w tle, gdy menu RetroArch jest przełączane."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
      "Sterownik wideo do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
      "Sterownik audio do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_DRIVER,
      "Wprowadź sterownik, którego chcesz użyć. W zależności od sterownika wideo może wymusić inny sterownik wejściowy."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
      "Sterownik joypad do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
      "Sterownik resamplera audio do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
      "Sterownik kamery do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
      "Sterownik lokalizacji do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_DRIVER,
      "Sterownik menu do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_RECORD_DRIVER,
      "Zapisz sterownik, którego chcesz użyć."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MIDI_DRIVER,
      "Sterownik MIDI do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_WIFI_DRIVER,
      "Sterownik WiFi do użycia."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
      "Filtruj pliki wyświetlane w przeglądarce plików według obsługiwanych rozszerzeń."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
      "Wybierz zdjęcie, aby ustawić jako tapetę menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
      "Dynamicznie ładuj nową tapetę w zależności od kontekstu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
      "Zastąp domyślne urządzenie audio używane przez sterownik. Zależne od sterownika."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
      "Wtyczka audio DSP przetwarzająca dźwięk przed wysłaniem go do sterownika."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
      "Prędkość próbkowania wyjścia audio."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
      "Krycie wszystkich elementów interfejsu użytkownika nakładki."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
      "Skala wszystkich elementów interfejsu nakładki."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
      "Włącz nakładkę."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
      "Wybierz nakładkę z przeglądarki plików."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
      "Adres hosta, z którym chcesz się połączyć."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
      "Port adresu IP hosta. Może to być port TCP lub UDP."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
      "Hasło do połączenia z hostem netplay. Używane tylko w trybie hosta."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
      "Czy publicznie ogłaszać gry internetowe. W przypadku braku ustawienia klienci muszą ręcznie łączyć się, zamiast korzystać z publicznego lobby."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
      "Hasło do połączenia z hostem netplay z tylko uprawnieniami obserwatora. Używane tylko w trybie hosta."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
      "Czy rozpocząć netplay w trybie obserwatora."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
      "Określa, czy zezwalać na połączenia w trybie slave. Klienty w trybie slave wymagają bardzo małej mocy obliczeniowej po każdej ze stron, ale znacznie ucierpią z powodu opóźnień sieci."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
      "Określa, czy nie zezwalać na połączenia nie w trybie slave. Niezalecane, z wyjątkiem bardzo szybkich sieci z bardzo słabymi maszynami."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
      "Czy uruchomić netplay w trybie niewymagającym stanów zapisu. Jeśli jest ustawiona na wartość true, wymagana jest bardzo szybka sieć, ale nie jest wykonywane przewijanie, więc nie będzie jittera."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
      "Częstotliwość w klatkach, w których netplay będzie weryfikować, czy host i klient są zsynchronizowane."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
      "Podczas hostowania, próbuj słuchać połączeń z publicznego Internetu, używając UPnP lub podobnych technologii, aby uciec z sieci LAN."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
      "Włącz interfejs poleceń Stdin."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
      "Włącz kontrolki myszy w menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_POINTER_ENABLE,
      "Włącz sterowanie dotykowe w menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_THUMBNAILS,
      "Typ wyświetlanej miniatury."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
      "Typ miniatury do wyświetlenia po lewej stronie."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
      "Wyświetl miniaturę po prawej stronie ekranu.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
      "Pokazuje aktualną datę i/lub czas w menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
      "Pokazuje aktualny poziom naładowania baterii w menu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
      "Przewijanie do początku i/lub końca, jeśli granica listy jest osiągnięta poziomo lub pionowo."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
      "Włącza funkcję netplay w trybie hosta (serwer)."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
      "Włącza grę sieciową w trybie klienta.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
      "Odłącz aktywne połączenie gry online.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
      "Skanuje katalog w poszukiwaniu kompatybilnych plików.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_FILE,
      "Skanuje zgodny plik.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
      "Używa niestandardowego interwału wymiany dla Vsync. Ustaw, aby efektywnie zmniejszyć o połowę częstotliwość odświeżania monitora."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
      "Sortuj pliki zapisu w folderach nazwanych po używanym rdzeniu."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
      "Sortuj stany zachowywania w folderach nazwanych po używanym rdzeniu."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
      "Żądanie odtworzenia za pomocą danego urządzenia wejściowego.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
      "Adres URL do głównego katalogu Updater na kompilatorze Libretro.")
MSG_HASH(MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
      "Adres URL do katalogu aktualizującego zasoby na kompilatorze Libretro.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
      "Po pobraniu automatycznie wyodrębnia pliki zawarte w pobranych archiwach."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
      "Zeskanuj nowe pokoje.")
MSG_HASH(MENU_ENUM_SUBLABEL_INFORMATION,
      "Zobacz więcej informacji o zawartości.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
      "Dodaj wpis do ulubionych.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
      "Dodaj wpis do ulubionych.")
MSG_HASH(MENU_ENUM_SUBLABEL_RUN,
      "Uruchom tytuł.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
      "Dostosowuje ustawienia przeglądarki plików.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
      "Domyślnie włącz niestandardowe elementy sterujące podczas uruchamiania."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
      "Domyślnie włącz niestandardową konfigurację podczas uruchamiania."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
      "Domyślnie włącz spersonalizowane opcje rdzenia podczas uruchamiania.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_ENABLE,
      "Wyświetla bieżącą nazwę rdzenia w menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
      "Zobacz bazy danych.")
MSG_HASH(MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
      "Wyświetl poprzednie wyszukiwania.")
MSG_HASH(MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
      "Przechwytuje obraz ekranu.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
      "Zamyka aktualną zawartość. Wszelkie niezapisane zmiany mogą zostać utracone."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_STATE,
      "Załaduj zapisany stan z aktualnie wybranego gniazda.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_STATE,
      "Zapisz stan w aktualnie wybranym gnieździe.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESUME,
      "Wznów aktualnie uruchomioną zawartość i opuść szybkie menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESUME_CONTENT,
      "Wznów aktualnie uruchomioną zawartość i opuść szybkie menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_STATE_SLOT,
      "Zmienia aktualnie wybrany przedział stanu.")
MSG_HASH(MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
      "Jeśli stan został załadowany, zawartość wróci do stanu sprzed załadowania.")
MSG_HASH(MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
      "Jeśli stan został nadpisany, zostanie przywrócony do poprzedniego stanu zapisu.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
      "Usługa RetroAchievements. Aby uzyskać więcej informacji, odwiedź http://retroachievements.org"
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
      "Zarządzaj aktualnie skonfigurowanymi kontami."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
      "Zarządza ustawieniami przewijania.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESTART_CONTENT,
      "Ponownie uruchamia zawartość od początku.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "Zapisuje plik konfiguracji zastąpienia, który będzie obowiązywał dla wszystkich treści załadowanych tym rdzeniem. Ma pierwszeństwo przed główną konfiguracją.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "Zapisuje plik konfiguracji zastąpienia, który będzie dotyczył tylko bieżącej treści. Ma pierwszeństwo przed główną konfiguracją.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
      "Skonfiguruj kody.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
      "Skonfiguruj shadery w celu polepszenia obrazu.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
      "Zmień ustawienia dla aktualnie wyświetlanej treści.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_OPTIONS,
      "Zmień opcje aktualnie wyświetlanej treści.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
      "Pokaż zaawansowane ustawienia dla zaawansowanych użytkowników (domyślnie ukryty).")
MSG_HASH(MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
      "Wykonuj zadania w oddzielnym wątku.")
MSG_HASH(MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
      "Ustawia katalog systemowy. Rdzenie mogą wysyłać zapytania do tego katalogu, aby załadować BIOS, konfiguracje specyficzne dla systemu itp.")
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
      "Ustawia katalog startowy dla przeglądarki plików.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CONTENT_DIR,
      "Zwykle ustawiane przez programistów, którzy pakują aplikacje libretro/RetroArch, aby wskazywały na zasoby."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
      "Katalog do przechowywania tapet dynamicznie ładowanych przez menu w zależności od kontekstu.")
MSG_HASH(MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
      "Dodatkowe miniaturki (boxarty/miscale itp.) Są tutaj przechowywane."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
      "Ustawia katalog początkowy dla przeglądarki konfiguracji menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
      "Liczba klatek opóźnienia wejściowego dla netplay do wykorzystania do ukrycia opóźnień sieci. Zmniejsza drgania i sprawia, że gra jest mniej intensywna, kosztem zauważalnego opóźnienia wejścia.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
      "Zakres klatek opóźnień wejściowych, które można wykorzystać do ukrycia opóźnień sieci. Zmniejsza fluktuacje i sprawia, że netplay jest mniej obciążający procesor, kosztem nieprzewidywalnego opóźnienia wejściowego.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_CYCLE_TRAY_STATUS,
      "Cykluj bieżący dysk. Jeśli dysk zostanie włożony, wyskoczy. Jeśli dysk nie został włożony, zostanie włożony. ")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_INDEX,
      "Zmień indeks dysku.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_OPTIONS,
      "Zarządzanie obrazem dysku.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
      "Wybierz obraz dysku, który chcesz wstawić.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
      "Upewnia się, że liczba klatek na sekundę jest ograniczona w menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_LAYOUT,
      "Wybierz inny układ interfejsu XMB.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_THEME,
      "Wybierz inny motyw dla ikony. Zmiany zaczną obowiązywać po ponownym uruchomieniu programu.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
      "Włącz cienie dla wszystkich ikon. Będzie to miało niewielki wpływ na wydajność.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
      "Wybierz inny motyw gradientu tła.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
      "Zmodyfikuj krycie tapety w tle.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
      "Wybierz inny motyw gradientu tła.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
      "Wybierz animowany efekt tła. Może być intensywnie wykorzystujący GPU w zależności od efektu. Jeśli wydajność jest niezadowalająca, wyłącz to lub powróć do prostszego efektu.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_FONT,
      "Wybierz inną główną czcionkę, która ma być używana w menu.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
      "Pokaż zakładkę Ulubione w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
      "Pokaż kartę Obrazu w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
      "Pokaż zakładkę Muzyka w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
      "Pokaż kartę Video w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
      "Pokaż zakładkę Netplay w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
      "Pokaż kartę Ustawień w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
      "Pokaż kartę ostatnich historii w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
      "Pokaż kartę treści importu w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
      "Pokaż kartę listy odtwarzania w menu głównym.")
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
      "Pokaż ekran startowy w menu. Po pierwszym uruchomieniu program jest automatycznie ustawiany na false.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
      "Zmodyfikuj krycie grafiki nagłówka.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
      "Zmodyfikuj krycie grafiki stopki.")
MSG_HASH(MENU_ENUM_SUBLABEL_DPI_OVERRIDE_ENABLE,
      "Menu zazwyczaj dynamicznie się skaluje. Jeśli chcesz zamiast tego ustawić określony rozmiar skalowania, włącz to.")
MSG_HASH(MENU_ENUM_SUBLABEL_DPI_OVERRIDE_VALUE,
      "Ustaw tutaj niestandardowy rozmiar skalowania. UWAGA: Musisz włączyć 'DPI Override', aby ten rozmiar skali zaczął obowiązywać.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
      "Zapisz wszystkie pobrane pliki w tym katalogu.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
      "Zapisz wszystkie zmienione kontrolki do tego katalogu.")
MSG_HASH(MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
      "Katalog, w którym program szuka treści/rdzeni.")
MSG_HASH(MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
      "Pliki informacji o aplikacji/rdzeniu przechowywane są tutaj .")
MSG_HASH(MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
      "Jeśli joypad jest podłączony, to zostanie automatycznie skonfigurowany, jeśli plik konfiguracyjny odpowiadający mu jest obecny w tym katalogu.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
      "Jeśli ustawione na katalog, zawartość, która jest czasowo wyodrębniana (np. Z archiwów), zostanie wyodrębniona do tego katalogu."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
      "Zapisane zapytania są przechowywane w tym katalogu.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
      "Bazy danych są przechowywane w tym katalogu."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
      "Ta lokalizacja jest domyślnie sprawdzana, gdy interfejsy menu próbują znaleźć ładowalne zasoby itp."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
      "Zapisz wszystkie pliki zapisu w tym katalogu. Jeśli nie jest ustawione, spróbuje zapisać w katalogu roboczym pliku zawartości.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
      "Zapisz wszystkie stany zachowania w tym katalogu. Jeśli nie jest ustawione, spróbuje zapisać w katalogu roboczym plików zawartości.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
      "Katalog do zrzutu zrzutów ekranu do.")
MSG_HASH(MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
      "Definiuje katalog, w którym nakładki są przechowywane dla łatwego dostępu.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
      "Pliki do pobrania są tutaj przechowywane."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
      "Katalog, w którym przechowywane są pliki filtrów DSP audio."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
      "Katalog, w którym przechowywane są pliki filtrów wideo oparte na procesorze."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
      "Definiuje katalog, w którym pliki shaderów wideo oparte na GPU są przechowywane dla łatwego dostępu.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
      "Nagrania zostaną zrzucone do tego katalogu.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
      "Tutaj będą przechowywane konfiguracje nagrywania.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
      "Wybierz inną czcionkę dla powiadomień na ekranie.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
      "Zmiany w konfiguracji modułu cieniującego zostaną natychmiast zastosowane. Użyj tej opcji, jeśli zmienisz liczbę przejść dla modułów cieniujących, filtrowanie, skalę FBO itp.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
      "Zwiększ lub zmniejsz ilość przejść potoku cieniującego. Możesz powiązać osobny moduł cieniujący z każdym przebiegiem potoku i skonfigurować jego skalę i filtrowanie."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
      "Załaduj preset modułu cieniującego. Potok cieniujący zostanie automatycznie skonfigurowany.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
      "Zapisz bieżące ustawienia modułu cieniującego jako nowe ustawienie domyślne modułu cieniującego.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
      "Zapisz bieżące ustawienia modułu cieniującego jako ustawienia domyślne dla tej aplikacji/rdzenia.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
      "Zapisz bieżące ustawienia modułu cieniującego jako ustawienia domyślne dla wszystkich plików w bieżącym katalogu zawartości.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
      "Zapisz bieżące ustawienia modułu cieniującego jako ustawienia domyślne dla zawartości.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
      "Modyfikuje bezpośrednio bieżący moduł cieniujący. Zmiany nie zostaną zapisane w ustawionym pliku.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
      "Modyfikuje domyślne ustawienie modułu cieniującego w menu.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
      "Zwiększ lub zmniejsz ilość kodów."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
      "Zmiany kodu odniosą skutek natychmiast.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
      "Zapisz bieżące kody jako plik składowania."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
      "Szybki dostęp do wszystkich istotnych ustawień w grze.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_INFORMATION,
      "Wyświetl informacje dotyczące aplikacji/rdzenia.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
      "Wartość zmiennoprzecinkowa dla współczynnika proporcji wideo (szerokość/wysokość), jeśli współczynnik proporcji jest ustawiony na 'Config'.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
      "Niestandardowa wysokość ekranu, która jest używana, jeśli współczynnik proporcji jest ustawiony na 'Niestandardowy'.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
      "Niestandardowa szerokość ekranu, która jest używana, jeśli współczynnik proporcji jest ustawiony na 'Niestandardowy'.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
      "Niestandardowe przesunięcie wyświetlania używane do definiowania położenia osi X. Są one ignorowane, jeśli włączona jest opcja 'Integer Scale'. Zostanie wtedy automatycznie wyśrodkowany.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
      "Niestandardowe przesunięcie wyświetlania używane do definiowania położenia osi Y. Są one ignorowane, jeśli włączona jest opcja 'Integer Scale'. Zostanie wtedy automatycznie wyśrodkowany.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
      "Użyj serwera przekaźnikowego")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
      "Przekaż połączenia sieciowe przez serwer pośredniczący. Przydatne, jeśli host znajduje się za zaporą lub ma problemy z NAT/UPnP.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
      "Lokalizacja serwera przekaźnikowego")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
      "Wybierz określony serwer przekazujący, którego chcesz użyć. Geograficznie bliższe lokalizacje mają zazwyczaj mniejsze opóźnienie.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
      "Dodaj do miksera")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
      "Dodaj do miksera i odtwarzaj")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
      "Dodaj do miksera")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
      "Dodaj do miksera i odtwarzaj")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
      "Filtruj według bieżącego rdzenia")
MSG_HASH(
      MSG_AUDIO_MIXER_VOLUME,
      "Globalna objętość miksera audio"
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
      "Globalna objętość miksera audio (w dB). 0 dB to normalna głośność i nie jest stosowane żadne wzmocnienie."
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
      "Poziom głośności miksera audio (dB)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
      "Wycisz mikser dźwięku"
      )
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
      "Wycisz/włącz dźwięk miksera.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
      "Pokaż aktualizacje online")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
      "Pokaż/ukryj opcję 'Aktualizacja online'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
      "Widok")
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
      "Pokaż lub ukryj elementy na ekranie menu."
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
      "Pokaż program Aktualizacja rdzenia")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
      "Pokaż/ukryj możliwość aktualizacji rdzeni (i podstawowych plików informacyjnych).")
MSG_HASH(MSG_PREPARING_FOR_CONTENT_SCAN,
      "Przygotowanie do skanowania zawartości...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_DELETE,
      "Usuń rdzeń")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_DELETE,
      "Usuń ten rdzeń z dysku.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
      "Nieprzezroczysty bufor klatki")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
      "Zmodyfikuj krycie bufora klatki.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
      "Ulubione")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
      "Treści, które dodałeś do \"Ulubionych\" pojawią się tutaj.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
      "Muzyka")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_MUSIC,
      "Tutaj pojawi się muzyka, która została wcześniej odtworzona.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
      "Obraz")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_IMAGES,
      "Tutaj pojawią się obrazy, które wcześniej były oglądane.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
      "Wideo")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_VIDEO,
      "Tutaj będą wyświetlane filmy, które zostały wcześniej odtworzone.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
      "Ikony menu")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
      "Włącz/wyłącz ikony menu pokazane po lewej stronie wpisów menu.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
      "Włącz kartę Ustawienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
      "Ustaw hasło dla włączania karty Ustawienia")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
      "Wprowadź hasło")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
      "Hasło poprawne.")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
      "Hasło niepoprawne.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
      "Włącza kartę Ustawienia. Wymagane jest ponowne uruchomienie karty.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
      "Podanie hasła podczas ukrywania karty ustawień pozwala później przywrócić ją z menu, przechodząc do karty Menu główne, wybierając opcję Włącz kartę Ustawienia i wprowadzając hasło.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
      "Zezwalaj na zmianę nazw wpisów")
MSG_HASH(MENU_ENUM_SUBLABEL_RENAME_ENTRY,
      "Zmień nazwę tytułu.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
      "Zmień nazwę")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
      "Pokaż ładowanie rdzenia")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
      "Pokaż/ukryj opcję 'Załaduj rdzeń'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
      "Pokaż zawartość")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
      "Pokaż/ukryj opcję \"Wczytaj zawartość\".")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
      "Pokaż informacje")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
      "Pokaż/ukryj opcję 'Informacje'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
      "Pokaż konfiguracje")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
      "Pokaż/ukryj opcję 'Konfiguracje'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
      "Pokaż pomoc")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
      "Pokaż/ukryj opcję \"Pomoc\".")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
      "Pokaż restart")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
      "Pokaż/ukryj opcję 'Restart'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
      "Pokaż wyłączenie")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
      "Pokaż/ukryj opcję 'Wyłącz'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
      "Szybkie menu")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
      "Pokaż lub ukryj elementy na ekranie szybkiego menu.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
      "Pokaż zrzut ekranu")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
      "Pokaż/ukryj opcję \"Zrób zrzut ekranu\".")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
      "Pokaż stan zapisywania/ładowania")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
      "Pokaż/ukryj opcje zapisywania/ładowania stanu.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
      "Pokaż opcje cofnij zapisanie/załadowanie stanu")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
      "Pokaż/ukryj opcje cofania stanu zapisywania/ładowania.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
      "Pokaż dodaj do ulubionych")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
      "Pokaż/ukryj opcję 'Dodaj do ulubionych'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
      "Pokaż Rozpocznij nagrywanie")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
      "Pokaż/ukryj opcję 'Rozpocznij nagrywanie'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
      "Pokaż rozpoczęcie strumieniowanie")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
      "Pokaż / ukryj opcję 'Rozpocznij transmisję'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
      "Pokaż Resetuj podstawowe skojarzenie")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
      "Pokaż/ukryj opcję 'Resetuj podstawowe skojarzenie'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
      "Pokaż opcje")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
      "Pokaż/ukryj opcję 'Opcje'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
      "Pokaż elementy sterujące")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
      "Pokaż/ukryj opcję 'Sterowanie'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
      "Pokaż kody")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
      "Pokaż/ukryj opcję 'Kody'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
      "Pokaż shadery")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
      "Pokaż/ukryj opcję 'Shadery'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
      "Pokaż zapis nadpisu rdzenia")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
      "Pokaż/ukryj opcję 'Zapisz podstawowe przesłonięcia'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
      "Pokaż zapisy gry")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
      "Pokaż/ukryj opcję 'Zachowaj pominięcia gry'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
      "Pokaż informacje")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
      "Pokaż/ukryj opcję 'Informacje'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
      "Włącz tło powiadomień ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
      "Czerwony kolor powiadomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
      "Zielony kolor powiadomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
      "Niebieski kolor powiadomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
      "Krycie tła powiadomienia")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
      "Wyłącz tryb kiosku")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
      "Wyłącz tryb kiosku. Ponowne uruchomienie jest wymagane, aby zmiana mogła w pełni działać.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
      "Włącz tryb kiosku")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
      "Chroni konfigurację, ukrywając wszystkie ustawienia związane z konfiguracją.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
      "Ustaw hasło do wyłączania trybu kiosku")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
      "Podanie hasła podczas włączania trybu kiosku umożliwia późniejsze wyłączenie go z menu, przechodząc do Menu głównego, wybierając Wyłącz tryb kiosku i wprowadzając hasło.")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD,
      "Wprowadź hasło")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
      "Hasło poprawne.")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
      "Hasło niepoprawne.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
      "Powiadomienie czerwony kolor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
      "Powiadomienie zielony kolor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
      "Powiadomienie niebieski kolor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
      "Pokaż liczbę klatek na wyświetlaczu")
MSG_HASH(MSG_CONFIG_OVERRIDE_LOADED,
      "Przeładowanie konfiguracji zostało załadowane.")
MSG_HASH(MSG_GAME_REMAP_FILE_LOADED,
      "Załadowano plik remapu gry.")
MSG_HASH(MSG_CORE_REMAP_FILE_LOADED,
      "Załadowano plik wymiany rdzenia.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
      "Automatycznie dodawaj zawartość do listy odtwarzania")
MSG_HASH(MENU_ENUM_SUBLABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
      "Automatycznie skanuje załadowaną zawartość, aby pojawiły się w listach odtwarzania.")
MSG_HASH(MSG_SCANNING_OF_FILE_FINISHED,
      "Skanowanie pliku zakończone")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
      "Przezroczystość okna")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
      "Jakość resamplera audio")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
      "Obniż tę wartość, aby faworyzować wydajność/zmniejszyć opóźnienie w stosunku do jakości dźwięku, zwiększaj, jeśli chcesz uzyskać lepszą jakość dźwięku kosztem wydajności/mniejszego opóźnienia.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
      "Oglądaj pliki modułu cieniującego w poszukiwaniu zmian")
MSG_HASH(MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
      "Automatycznie zastosuj zmiany wprowadzone w plikach modułu cieniującego na dysku.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
      "Pokaż dekoracje okien")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
      "Wyświetl statystyki")
MSG_HASH(MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
      "Pokaż techniczne statystyki na ekranie.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
      "Włącz wypełnienie granicy")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
      "Włącz grubość wypełniacza")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
      "Włącz grubość wypełniacza tła")
MSG_HASH(MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION, "Tylko dla wyświetlaczy CRT. Próby użycia dokładnej rozdzielczości rdzenia/gry i częstotliwości odświeżania.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION, "CRT Przełącz rozdzielczość")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
      "Przełącz między natywnymi i superszybkimi superrozdzielczościami."
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER, "CRT Super rozdzielczość")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
      "Pokaż ustawienia przewijania")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
      "Pokaż/ukryj opcje przewijania.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
      "Pokaż/ukryj opcje opóźnień.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
      "Pokaż ustawienia opóźnień")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
      "Pokaż/ukryj opcje nakładki.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
	  "Pokaż ustawienia nakładek")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
      "Włącz dźwięk menu")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
      "Włącz lub wyłącz dźwięk menu.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
      "Ustawienia miksera")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
      "Zobacz i/lub zmień ustawienia miksera audio.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
      "Opcje zastępowania konfiguracji")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
      "Opcje nadpisania konfiguracji globalnej.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
      "Rozpocznie odtwarzanie strumienia audio. Po zakończeniu usunie bieżący strumień audio z pamięci.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
      "Rozpocznie odtwarzanie strumienia audio. Po zakończeniu zostanie zapętlony i odtworzony ponownie od początku.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
      "Rozpocznie odtwarzanie strumienia audio. Po zakończeniu przeskakuje do następnego strumienia audio w kolejności sekwencyjnej i powtórzy to zachowanie. Przydatny jako tryb odtwarzania albumu.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
      "Spowoduje to zatrzymanie odtwarzania strumienia audio, ale nie spowoduje usunięcia go z pamięci. Możesz zacząć grać ponownie, wybierając opcję 'Odtwarzaj'.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
      "Spowoduje to zatrzymanie odtwarzania strumienia audio i całkowite usunięcie go z pamięci.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
      "Dostosuj głośność strumienia audio.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
      "Dodaj tę ścieżkę audio do dostępnego gniazda strumienia audio. Jeśli obecnie nie ma wolnych miejsc, zostanie to zignorowane.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
      "Dodaj tę ścieżkę audio do dostępnego gniazda strumienia audio i odtwarzaj. Jeśli obecnie nie ma wolnych miejsc, zostanie to zignorowane.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
      "Odtwarzaj")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
      "Odtwarzaj (Zapętlone)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
      "Odtwarzaj (Sekwencyjny)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
      "Zatrzymaj")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
      "Usuń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
      "Głośność")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
      "Obecny rdzeń")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
	   "Oczyść")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
      "W Menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
      "W Grze")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
      "W Grze (Wstrzymano)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
      "Gra")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
      "Wstrzymano")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
      "Włącz Discord"
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
      "Włącz lub wyłącz wsparcie dla Discrod. Nie będzie działać z wersją przeglądarkową, jedynie z natywnym klientem."
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
      "Wejście")
MSG_HASH(MENU_ENUM_SUBLABEL_MIDI_INPUT,
      "Wybierz urządzenie wejściowe.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
      "Wyjście")
MSG_HASH(MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
      "Wybierz urządzenie wyjściowe.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
      "Głośność")
MSG_HASH(MENU_ENUM_SUBLABEL_MIDI_VOLUME,
      "Ustaw głośność wyjściową (%).")
MSG_HASH(MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
      "Zarządzanie energią")
MSG_HASH(MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
      "Zmień ustawienia zarządzania energią.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
      "Trwały tryb wydajności")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
      "Wsparcie dla MPV")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
    "Adaptacyjny Vsync"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
    "V-Sync jest włączony, dopóki wydajność nie spadnie poniżej docelowej częstotliwości odświeżania. Może zminimalizować zacinanie, gdy wydajność spada poniżej czasu rzeczywistego oraz może być bardziej energooszczędne."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
    "CRT SwitchRes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
    "Wyjściowe natywne sygnały niskiej rozdzielczości do użytku z wyświetlaczami CRT."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
    "Przejrzyj te opcje, jeśli obraz nie jest wyśrodkowany na wyświetlaczu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
    "Centracja X-Axis"
    )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
      "W razie potrzeby użyj niestandardowej częstotliwości odświeżania określonej w pliku konfiguracyjnym.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
      "Użyj niestandardowej częstotliwości odświeżania")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
      "Wybierz port wyjściowy podłączony do wyświetlacza CRT.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
      "Identyfikator wyświetlania wyjścia")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
    "Rozpocznij nagrywanie"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
    "Rozpoczyna nagrywanie."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
    "Zatrzymaj nagrywanie"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
    "Zatrzymuje nagrywanie."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
    "Rozpocznij transmisję"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
    "Rozpoczyna transmisję."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
    "Zatrzymuje transmitowanie"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
    "Zatrzymuje przesyłanie transmisji."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
    "Jakość rekordu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
    "Jakość strumienia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
    "Port Strumienia UDP"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH,
    "Twitch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE,
    "YouTube"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
      "Klucz strumienia Twitch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
      "Klucz strumienia YouTube")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
      "Tryb przesyłania strumieniowego")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
      "Tytuł strumienia")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
    "Podziel Joy-Con"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
    "Przywróć domyślne"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
    "Zresetuj bieżącą konfigurację do wartości domyślnych."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
    "OK"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
      "Kolor menu")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
    "podstawowy odcień bieli"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
    "podstawowy odcień czerni"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
    "Wybierz inny motyw koloru."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "Użyj preferowanego motywu kolorystycznego systemu")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "Użyj motywu kolorystycznego systemu operacyjnego (jeśli jest dostępny) - zastępuje ustawienia kompozycji.")
MSG_HASH(MSG_RESAMPLER_QUALITY_LOWEST,
      "Najniższa")
MSG_HASH(MSG_RESAMPLER_QUALITY_LOWER,
      "Niższa")
MSG_HASH(MSG_RESAMPLER_QUALITY_NORMAL,
      "Normalna")
MSG_HASH(MSG_RESAMPLER_QUALITY_HIGHER,
      "Wyższa")
MSG_HASH(MSG_RESAMPLER_QUALITY_HIGHEST,
      "Najwyższa")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
    "Brak muzyki."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
    "Brak filmów."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
    "Brak zdjęć."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
    "Brak ulubionych."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
      "Zapamiętaj położenie i rozmiar okna")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
    "Obsługa CoreAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
    "Obsługa CoreAudio V3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
      "Menu widgetów")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
      "Shadery Video")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
      "Skanuj bez dopasowania podstawowego")
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
      "Po wyłączeniu zawartość jest dodawana tylko do list odtwarzania, jeśli zainstalowany jest rdzeń, który obsługuje jej rozszerzenie. Włączając to, niezależnie od tego doda do listy odtwarzania. W ten sposób możesz zainstalować rdzeń, którego potrzebujesz później po skanowaniu")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
      "Podświetlanie ikony poziomej animacji")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
      "Animacja Przesuń w górę/w dół")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
      "Menu główne animacji Otwiera/Zamyka")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
    "Informacje o dysku"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISC_INFORMATION,
    "Wyświetl informacje o włożonych dyskach multimedialnych."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
    "Pokaż Ustaw podstawowe stowarzyszenie"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
    "Pokaż/ukryj opcję „Ustaw podstawowe stowarzyszenie”."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
    "Pokaż Pobierz Miniatury"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
    "Pokaż/ukryj opcję „Pobierz miniatury”."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
    "Pokaż aktualizację starszych miniatur"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
    "Pokaż/ukryj możliwość pobierania starszych pakietów miniatur."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
    "Pokaż podpowiedzi menu"
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
     "Styl daty/godziny"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
    "Wznów zawartość po użyciu stanów zapisu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
    "Współczynnik skali miniatur"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
    "Próg skalowania miniatur"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
    "Animacja paska tekstu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
    "Szybkość paska tekstu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
    "Sortuj listy alfabetycznie"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
    "Zapisz listy odtwarzania w starym formacie"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
    "Pokaż powiązane rdzenie na listach odtwarzania"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
    "Pokaż etykiety pomocnicze listy odtwarzania"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
    "Czas działania etykiety podrzędnej listy odtwarzania"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
    "Dopasowanie rozmycia archiwum"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
    "Zarządzanie listami odtwarzania"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
    "Pobieranie miniatur na żądanie"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
    "Pokaż kursor myszy z nakładką"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
    "Dołącz szczegóły pamięci"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
    "Synchronizuj z dokładną liczbą klatek na sekundę (G-Sync, FreeSync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
    "Zaloguj się do pliku"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
    "Pliki dziennika ze znacznikami czasu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
    "Zapisz dziennik środowiska wykonawczego (na rdzeń)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
    "Zapisz dziennik czasu wykonywania (agregacji)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
    "Naciśnij dwukrotnie wyjście"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
    "Wibruj po naciśnięciu klawisza"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
    "Włącz wibracje urządzenia (dla obsługiwanych rdzeni)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
    "Wprowadź próg osi przycisku"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
    "Martwa strefa analoga"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
    "Czułość analoga"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
    "Rozpocznij lub kontynuuj wyszukiwanie kodu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
    "Załaduj plik kodu (dołącz)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
    "Odśwież Kody specyficzne dla gry"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
    "Dodaj nowy kod do góry"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
    "Dodaj nowy kod do dołu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
    "Usuń wszystkie kody"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
    "Automatyczne stosowanie cheatów podczas ładowania gry"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
    "Zastosuj po przełączeniu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
    "Zapisz plik zmiany mapowania zawartości"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
    "Rozmiar bufora przewijania (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
    "Krok zmiany rozmiaru bufora (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
    "Wyślij informacje o debugowaniu"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "Pokaż uruchom ponownie RetroArch"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "Pokaż/ukryj opcję „Uruchom ponownie RetroArch”."
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "Pokaż zamknij RetroArch"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "Pokaż/ukryj opcję „Zakończ RetroArch."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
    "Pokaż uruchom ponownie RetroArch"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
    "Pokaż/ukryj opcję „Uruchom ponownie RetroArch”."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
    "Dzienniki uruchomia"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
    "Zapisz pliki dziennika środowiska wykonawczego do tego katalogu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_DIR,
    "Dzienniki zdarzeń systemowych"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
    "Zapisz wszystkie listy odtwarzania w tym katalogu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
    "Wykonuj zadania konserwacyjne na wybranej liście odtwarzania (np. Ustaw/przywróć domyślne skojarzenia rdzenia)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
    "Podczas wyszukiwania list odtwarzania dla wpisów powiązanych ze skompresowanymi plikami, dopasuj tylko nazwę pliku archiwum zamiast [nazwa pliku] + [treść]. Włącz to, aby uniknąć duplikatów wpisów historii treści podczas ładowania skompresowanych plików."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
    "Wybiera typ rekordu dziennika środowiska wykonawczego, który ma być wyświetlany na sublabelach listy odtwarzania. (Należy pamiętać, że odpowiedni dziennik środowiska wykonawczego musi być włączony za pomocą menu opcji „Zapisywanie”)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
    "Wyświetla dodatkowe informacje dla każdego wpisu listy odtwarzania, takie jak bieżące powiązanie rdzenia i środowisko wykonawcze (jeśli jest dostępne). Ma zmienny wpływ na wydajność."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
    "Określ, kiedy oznaczyć pozycje listy odtwarzania aktualnie powiązanym rdzeniem (jeśli istnieje). UWAGA: To ustawienie jest ignorowane po włączeniu sublabeli listy odtwarzania."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
    "Sortuje listy odtwarzania treści w porządku alfabetycznym. Pamiętaj, że listy odtwarzania historii ostatnio używanych gier, obrazów, muzyki i filmów są wykluczone."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
    "Pozwól użytkownikowi usunąć wpisy z list odtwarzania."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
    "Pozwól użytkownikowi zmieniać nazwy wpisów na listach odtwarzania."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
    "Automatycznie pobieraj brakujące miniatury podczas przeglądania list odtwarzania. Ma poważny wpływ na wydajność."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
    "Brak odchylenia od żądanego czasu rdzenia. Użyj dla ekranów o zmiennej częstotliwości odświeżania, G-Sync, FreeSync."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_TO_FILE,
    "Przekierowuje komunikaty dziennika zdarzeń systemowych do pliku. Wymaga „oznajmiania rejestrowania”, aby włączyć."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
    "Podczas logowania do pliku przekierowuje dane wyjściowe z każdej sesji RetroArch do nowego pliku ze znacznikami czasu. Jeśli opcja jest wyłączona, dziennik jest nadpisywany przy każdym ponownym uruchomieniu RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
    "Śledzi, jak długo każdy element treści został uruchomiony, z rekordami oddzielonymi rdzeniem."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
    "Śledzi, jak długo każdy element treści został uruchomiony, zapisany jako suma całkowita dla wszystkich rdzeni."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
    "Jak daleko należy przechylać oś, aby uzyskać naciśnięcie przycisku."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
    "Pamiętaj o wielkości i pozycji okna, włączenie tej opcji ma pierwszeństwo przed skalą okienkową"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_DIR,
    "Zapisz pliki dziennika zdarzeń systemowych w tym katalogu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
    "Automatyczne zamykanie menu i wznawianie bieżącej zawartości po wybraniu „Zapisz stan” lub „Wczytaj stan” z Szybkiego menu. Wyłączenie tego może poprawić wydajność stanu zapisywania na bardzo wolnych urządzeniach."
    )
   #ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Uruchom ponownie program."
    )
#else
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Zamknij program."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
    "Uruchom ponownie program."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
    "Dopasuj ramki i elementy sterujące na ekranie"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Dostosuj powiadomienia na ekranie"
    )
MSG_HASH( 
    MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE, 
   "Rozmiar listy ulubionych" 
    )
MSG_HASH( 
    MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE, 
   "Ogranicz liczbę wpisów na liście odtwarzania ulubionych. Po osiągnięciu limitu nowe dodatki będą blokowane, dopóki stare wpisy nie zostaną usunięte. Ustawienie wartości -1 pozwala na „nieograniczoną” liczbę (99999) wpisów. OSTRZEŻENIE: Zmniejszenie wartości spowoduje usunięcie istniejących wpisów!"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
    "Zainstaluj lub przywróć rdzeń"
    )
 MSG_HASH(
    MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
    "Zainstaluj lub przywróć rdzeń z katalogu pobranych plików."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
    "Zainstaluj rdzeń z narzędzia do aktualizacji online."
    ) 
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
    "Aktualizator miniatur list odtwarzania"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
    "Pobierz pojedyncze miniatury dla każdego wpisu wybranej listy odtwarzania."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
    "Naciśnij dwukrotnie klawisz skrótu, aby wyjść z RetroArch."
     )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
    "Użyj pliku globalnych opcji podstawowych"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
    "Zapisz wszystkie opcje rdzenia w pliku wspólnych ustawień (retroarch-core-options.cfg). Po wyłączeniu opcje dla każdego rdzenia zostaną zapisane w osobnym folderze/pliku specyficznym dla rdzenia w katalogu „Config” RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
    "Za każdym razem, gdy zwiększysz lub zmniejszysz wartość rozmiaru bufora przewijania za pomocą tego interfejsu, zmieni się on o tę wartość"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
    "Ilość pamięci (w MB) do zarezerwowania dla bufora przewijania. Zwiększenie tego zwiększy ilość historii przewijania."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
    "Pokaż kursor myszy podczas korzystania z nakładki ekranowej."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
    "Rozpocznij lub kontynuuj wyszukiwanie kodów"
    )
 MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
    "Załaduj plik kodów i zastąp istniejące kody."
    )
 MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
    "Automatyczne stosowanie kodów podczas ładowania gry."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
    "Zastosuj kody natychmiast po przełączeniu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Zapisuje plik konfiguracji zastępowania, który będzie obowiązywał dla całej zawartości ładowanej z tego samego katalogu, co bieżący plik. Będzie mieć pierwszeństwo przed główną konfiguracją."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
    "Załaduj plik kodów i dołącz do istniejących kodów."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
    "Poziom rejestrowania interfejsu użytkownika"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
    "Ustawia poziom dziennika dla interfejsu użytkownika. Jeśli poziom dziennika wydany przez interfejs użytkownika jest poniżej tej wartości, jest on ignorowany."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Pokaż ponownie treść"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Pokaż/ukryj opcję „Uruchom ponownie zawartość”."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Pokaż Zamknij zawartość"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Pokaż/ukryj opcję „Zamknij zawartość”."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Pokaż znów zawartość"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Pokaż/ukryj opcję „Wznów treść”."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
    "Pokaż dane wejściowe"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
    "Pokaż lub ukryj „Ustawienia wprowadzania” na ekranie Ustawień."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
    "Usługa AI"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
    "Zmień ustawienia dla usługi AI (Tłumaczenie/TTS/Różne)."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
      "Wyjście usługi AI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
      "Adres URL usługi AI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
      "Usługa AI włączona")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
      "Wstrzymuje grę podczas tłumaczenia (tryb obrazu) lub kontynuuje działanie (tryb mowy)")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
      "Adres URL http: // wskazujący na usługę tłumaczeniową do użycia.")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
      "Włącz usługę AI, aby była uruchamiana po naciśnięciu klawisza skrótu usługi AI.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
      "Język docelowy")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
      "Język, na który będzie tłumaczona usługa. Jeśli ustawione na „Nie przejmuj się”, domyślnie będzie ustawiony na angielski.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
      "Język źródłowy")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
      "Język, z którego usługa będzie tłumaczona. Jeśli ustawione na „Nie przejmuj się”, spróbuje automatycznie wykryć język. Ustawienie go na określony język sprawi, że tłumaczenie będzie dokładniejsze.")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CZECH,
    "czeski"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DANISH,
    "duński"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWEDISH,
   "szwedzki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CROATIAN,
   "chorwacki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CATALAN,
   "kataloński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN,
   "bułgarski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BENGALI,
   "bengalski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BASQUE,
   "baskijski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI,
   "azerbejdżański"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN,
   "albański"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS,
   "afrykanerski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN,
   "estoński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FILIPINO,
   "filipiński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FINNISH,
   "fiński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GALICIAN,
   "galicyjski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN,
   "gruziński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GUJARATI,
   "gudżarati"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE,
   "kreolski haitański"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
   "hebrajski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HINDI,
   "hinduski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN,
   "węgierski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC,
   "islandzki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN,
   "indonezyjski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_IRISH,
   "Irlandski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_KANNADA,
   "kannada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATIN,
   "łacina"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATVIAN,
   "łotewski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN,
   "litewski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN,
   "macedoński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALAY,
   "malajski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALTESE,
   "maltański"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN,
   "norweski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_PERSIAN,
   "perski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN,
   "rumuński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SERBIAN,
   "serbski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVAK,
   "słowacki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN,
   "słoweński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWAHILI,
   "suahili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TAMIL,
   "tamilski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TELUGU,
   "Telugu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_THAI,
   "tajski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN,
   "ukraiński"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_URDU,
   "urdu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_WELSH,
   "walijski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_YIDDISH,
   "jidysz"
   )
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_DISC,
      "Załaduj fizyczny dysk z mediami. Najpierw wybierz rdzeń (Załaduj Rdzeń), którego zamierzasz używać z dyskiem.")
MSG_HASH(MENU_ENUM_SUBLABEL_DUMP_DISC,
      "Zrzuć fizyczny dysk z nośnikiem do pamięci wewnętrznej. Zostanie zapisany jako plik obrazu.")
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Tryb obrazu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Tryb mowy"
   )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
      "Usuń")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
      "Usuń wstępne ustawienia modułu cieniującego określonego typu.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Usuń globalne ustawienie wstępne")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Usuń ustawienie globalne, używane przez całą zawartość i wszystkie rdzenie.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Usuń ustawienia wstępne rdzenia")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Usuń ustawienie wstępne rdzenia, używane przez całą zawartość uruchomioną z aktualnie załadowanym rdzeniem.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Usuń ustawienie katalogu treści")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Usuń ustawienie wstępne katalogu treści, używane przez całą zawartość w bieżącym katalogu roboczym.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Usuń ustawienie gry")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Usuń ustawienie wstępne gry, używane tylko dla konkretnej gry, o której mowa.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
      "Licznik czasu klatki")
MSG_HASH(MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
      "Dostosuj ustawienia wpływające na licznik czasu klatek (aktywne tylko, gdy wątkowe wideo jest wyłączone).")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
      "Używaj nowocześnie udekorowanych animacji, powiadomień, wskaźników i kontrolek zamiast starego systemu opartego tylko na tekście.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
      "Pokaż menu pulpitu"
    )
MSG_HASH(MENU_ENUM_SUBLABEL_SHOW_WIMP,
    "Otwiera menu pulpitu, jeśli jest zamknięte."
    )
MSG_HASH(MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
    "Wysyła informacje diagnostyczne o twoim urządzeniu i konfiguracji RetroArch na nasze serwery w celu analizy."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
    "Dźwięki menu"
    )
   MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
    "Włącz dźwięk OK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
    "Włącz anuluj dźwięk"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
    "Włącz dźwięk powiadomienia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
    "Włącz dźwięk BGM"
    )
   MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
    "Niestandardowa konfiguracja strumienia"
    )
   MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
    "Nagrywanie wątków"
    )
   MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_URL,
    "Streaming URL"
    )
   MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
      "Interwał aktualizacji klatek na sekundę (w klatkach)")
MSG_HASH(MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
     "Wyświetlanie klatek będzie aktualizowane w ustalonych odstępach czasu (w klatkach).")
   MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
    "Wyświetla bieżącą liczbę klatek na ekranie."
   )
   MSG_HASH(
    MENU_ENUM_SUBLABEL_MEMORY_SHOW,
    "Obejmuje bieżące użycie pamięci/sumę na ekranie z FPS/klatek."
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
    "Opóźnienie Auto-Shadera"
    )
 MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
    "Opóźnia automatyczne ładowanie shaderów (w ms). Może obejść usterki graficzne podczas korzystania z oprogramowania do przechwytywania ekranu."
    )
 MSG_HASH(MENU_ENUM_SUBLABEL_START_CORE,
    "Zacznij rdzeń bez zawartości."
    )

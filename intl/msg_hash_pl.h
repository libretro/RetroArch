#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

/* Top-Level Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN_MENU,
   "Menu główne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Ustawienia etykiety"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Ulubione"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Historia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Muzyka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Pokoje gry internetowej"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importuj zawartość"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Szybkie menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Szybki dostęp do wszystkich istotnych ustawień w grze."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Załaduj rdzeń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Wybierz, którego rdzenia użyć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Załaduj zawartość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Wybierz, które treści chcesz rozpocząć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Załaduj dysk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Zrzut dysku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Zrzuć fizyczny dysk z nośnikiem do pamięci wewnętrznej. Zostanie zapisany jako plik obrazu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Playlisty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Tutaj pojawi się skanowana zawartość."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Skanuj zawartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Pokaż menu pulpitu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Wyłącz tryb kiosku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Aktualizacja online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Pobierz dodatki, komponenty i treści do RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Gra online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Dołącz lub obsługuj sesję gry online."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Ustawienia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Konfiguruj program."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informacja"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Wyświetl informacje o systemie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Konfiguracje"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Zarządzaj i twórz pliki konfiguracyjne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Pomoc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Dowiedz się więcej o tym, jak działa program."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Uruchom RetroArch ponownie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Uruchom ponownie program."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Zamknij RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Zamknij program."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Pobierz rdzeń..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Pobierz i zainstaluj rdzeń z narzędzia do aktualizacji online."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Zainstaluj lub przywróć rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Uruchom procesor wideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Uruchom zdalny Retro pad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Katalog startowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Pliki do pobrania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Przeglądaj archiwum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Załaduj archiwum"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Ulubione"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Muzyka"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Tutaj pojawi się muzyka, która została wcześniej odtworzona."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Tutaj pojawią się obrazy, które wcześniej były oglądane."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Tutaj będą wyświetlane filmy, które zostały wcześniej odtworzone."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Pobierz rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Aktualizuj zainstalowane rdzenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Zaktualizuj wszystkie zainstalowane rdzenie do najnowszej dostępnej wersji."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Zaktualizuj miniatury"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Pobierz kompletny pakiet miniatur dla wybranego systemu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Aktualizator miniatur list odtwarzania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Program do pobierania treści"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Zaktualizuj podstawowe pliki informacyjne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Zaktualizuj zasoby"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Zaktualizuj kody"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Zaktualizuj bazy danych"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Zaktualizuj nakładki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Zaktualizuj shadery GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Zaktualizuj shadery CG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Zaktualizuj Shadery Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Informacje podstawowe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Wyświetl informacje dotyczące aplikacji/rdzenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Informacje o dysku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Wyświetl informacje o włożonych dyskach multimedialnych."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Informacje o sieci"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Pokaż interfejsy sieciowe i powiązane adresy IP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Informacje o systemie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Pokaż informacje dotyczące konkretnego urządzenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Menedżer bazy danych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Zobacz bazy danych."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Menedżer kursorów"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Wyświetl poprzednie wyszukiwania."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Nazwa rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Etykieta rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Nazwa systemu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Producent systemu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Kategoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Autor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Uprawnienia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Licencja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Obsługiwane rozszerzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Wymagane API grafiki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Firmware(y)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING,
   "Brakujący"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT,
   "Obecny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPTIONAL,
   "Opcjonalny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REQUIRED,
   "Wymagany"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Usuń rdzeń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Usuń ten rdzeń z dysku."
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Data Builda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Wersja Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Kompilator"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Model CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Funkcje procesora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Architektura procesora:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Rdzeń procesora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CPU_CORES,
   "Ilość rdzeni procesora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Identyfikator Frontend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "System operacyjny Frontend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "Poziom Oceny Retro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Źródło prądu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Sterownik kontekstowy wideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Wyświetl szerokość metryczną (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Wyświetl wysokość metryczną (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Wyświetl DPI metryczne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "Obsługa LibretroDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Obsługa nakładek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Obsługa interfejsu poleceń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Obsługa interfejsu dowodzenia sieciowego"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Wsparcie Cocoa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "Obsługa PNG (RPNG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "Obsługa JPEG (RJPEG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "Obsługa BMP (RBMP)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "Obsługa TGA (RTGA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "Wsparcie SDL1.2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "Wsparcie SDL2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Wsparcie Vulkan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Wsparcie Metal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "Obsługa OpenGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "Obsługa OpenGL ES"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Przewlekanie wsparcia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "Obsługa KMS/EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Wsparcie dla Udev"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "Obsługa OpenVG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "Wsparcie EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "Wsparcie X11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Obsługa Wayland"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "Wsparcie XVideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "Obsługa ALSA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "Obsługa OSS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "Obsługa OpenAL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "Obsługa OpenSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "Wsparcie RSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "Obsługa RoarAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "Obsługa JACK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "Obsługa PulseAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "Obsługa CoreAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "Obsługa CoreAudio V3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "Wsparcie DirectSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "Obsługa WASAPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "Wsparcie XAudio2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "Obsługa Zlib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "Obsługa 7zip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Obsługa dynamicznej biblioteki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Dynamiczne ładowanie biblioteki libretro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Wsparcie Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "Wsparcie GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "Wsparcie HLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "Obsługa obrazów SDL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "Wsparcie FFmpeg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "Wsparcie dla MPV"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "Obsługa CoreText"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "Wsparcie FreeType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "Wsparcie STB TrueType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Wsparcie Gry online (peer-to-peer)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
   "Obsługa Pythona (obsługa skryptów w modułach cieniujących)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Obsługa Video4Linux2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "Wsparcie Libusb"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Wybór bazy danych"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Nazwa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Opis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Gatunek muzyczny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Wydawca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Deweloper"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Pochodzenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Seria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Ocena TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Ocena magazynu Famitsu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Recenzja magazynu Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Ocena magazynu Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Problem magazynu Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Miesiąc wydania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Rok wydania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Ocena BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Ocena ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Ocena ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Ocena PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Sprzęt ulepszający"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Ocena CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Kod seryjny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Obsługa analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Obsługa wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Tryb kooperacji"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Załaduj konfigurację"
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
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Zapisz bieżącą konfigurację"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Zapisz nową konfigurację"
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Podstawowa kontrola menu"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Przewiń do góry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Przewiń w dół"
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
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Rozpocznij"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Przełącz menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Wyjdź"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Przełącz klawiaturę"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Sterowniki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Zmień sterowniki używane przez system."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Zmień ustawienia wyjścia wideo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Dźwięk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Zmień ustawienia wyjścia audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Sterowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Zmień ustawienia joypada, klawiatury i myszy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Opóźnienie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Zmień ustawienia związane z wideo, dźwiękiem i opóźnieniem sygnału wejściowego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Rdzeń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Zmień ustawienia rdzenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Konfiguracja"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Zmień domyślne ustawienia plików konfiguracyjnych."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Zapisywanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Zmień ustawienia zapisu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Zalogowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Zmień ustawienia rejestrowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Przeglądarka plików"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Manipulacja klatek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Zmień ustawienia przewijania, przyśpieszania i spowalniania gry."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Nagranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Zmień ustawienia nagrywania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Wyświetlacz na ekranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Zmień nakładkę ekranu i nakładkę klawiatury oraz ustawienia powiadomień na ekranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Interfejs użytkownika"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Zmień ustawienia interfejsu użytkownika."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Usługa AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Zmień ustawienia dla usługi AI (Tłumaczenie/TTS/Różne)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Dostępność"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Zmień ustawienia narratora Dostępności."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Zarządzanie energią"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Zmień ustawienia zarządzania energią."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Osiągnięcia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Zmień ustawienia osiągnięć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Sieć"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Skonfiguruj ustawienia serwera i sieci."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Playlisty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Zmień ustawienia playlist."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Użytkownik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Zmień konto, nazwę użytkownika i ustawienia językowe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Szczegóły"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Zmień domyślne katalogi, w których znajdują się pliki."
   )

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Sterownik wejściowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Sterownik wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Sterownik wideo do użycia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Sterownik audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Sterownik audio do użycia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Sterownik audio resamplera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Sterownik resamplera audio do użycia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Sterownik kamery"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Sterownik kamery do użycia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
   "Sterownik Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Sterownik lokalizacji"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Sterownik lokalizacji do użycia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Sterownik menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Sterownik menu do użycia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Sterowniki nagrywania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Zapisz sterownik, którego chcesz użyć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
   "Sterownik MIDI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Sterownik MIDI do użycia."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "Przełączniki rozdzielczości CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Wyjściowe natywne sygnały niskiej rozdzielczości do użytku z wyświetlaczami CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Wyjście"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Zmień ustawienia wyjścia wideo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Tryb pełnoekranowy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Zmień ustawienia trybu pełnoekranowego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Tryb okien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Zmień ustawienia trybu okna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Skalowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Zmień ustawienia skalowania wideo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Synchronizacja"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Zmień ustawienia synchronizacji wideo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Wstrzymaj wygaszacz ekranu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Zapobiega włączaniu wygaszacza ekranu systemu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Wątek wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Poprawia wydajność kosztem opóźnień i częstszego rwania obrazu. Używaj tylko wtedy, gdy nie możesz uzyskać pełnej prędkości w przeciwnym razie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Wstawianie czarnej klatki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Włącz zrzut ekranu GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Zrzuty ekranu z zacienionych materiałów GPU, jeśli są dostępne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Filtrowanie bilinearne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Dodaje lekkie rozmycie obrazu, aby odciąć krawędzie twardych pikseli. Ta opcja ma bardzo mały wpływ na wydajność."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Opóźnienie Auto-Shadera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Opóźnia automatyczne ładowanie shaderów (w ms). Może obejść usterki graficzne podczas korzystania z oprogramowania do przechwytywania ekranu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Filtr wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Zastosuj filtr wideo wykorzystujący CPU.\nUWAGA: Może to spowodować wysokie koszty wydajności. Niektóre filtry wideo mogą działać tylko dla rdzeni o kolorze 32bitowym lub 16bitowym."
   )

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "CRT Przełącz rozdzielczość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Tylko dla wyświetlaczy CRT. Próby użycia dokładnej rozdzielczości rdzenia/gry i częstotliwości odświeżania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "CRT Super rozdzielczość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Przełącz między natywnymi i superszybkimi superrozdzielczościami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Centracja X-Axis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Przejrzyj te opcje, jeśli obraz nie jest wyśrodkowany na wyświetlaczu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Użyj niestandardowej częstotliwości odświeżania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "W razie potrzeby użyj niestandardowej częstotliwości odświeżania określonej w pliku konfiguracyjnym."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Monitoruj indeks"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Określa, który ekran wyświetlacza ma być używany."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Obrót"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Wymusza pewien obrót ekranu. Obrót jest dodawany do rotacji, które ustawia rdzeń."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Orientacja ekranu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Wymusza określoną orientację ekranu z systemu operacyjnego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Indeks GPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Odświeżanie w pionie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Prędkość odświeżania ekranu w pionie. Używane do obliczania odpowiedniej szybkości wejścia audio.\nUWAGA: Zostanie to zignorowane, jeśli włączona jest opcja 'Wideo wielowątkowe'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Szacowana liczba klatek na sekundę na ekranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Dokładna szacowana częstotliwość odświeżania ekranu w Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Ustaw częstotliwość odświeżania raportowanego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "Częstotliwość odświeżania zgłoszona przez sterownik ekranu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Wymuś wyłączenie sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Przymusowe wyłączenie obsługi sRGB FBO. Niektóre sterowniki Intel OpenGL w systemie Windows mają problemy z wideo z obsługą sRGB FBO, jeśli jest włączona. Włączenie tego może obejść to."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Użyj trybu pełnoekranowego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Zacznij w trybie pełnoekranowym. Można zmienić w czasie wykonywania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Tryb pełnoekranowy z pełnym ekranem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Jeśli jest to tryb pełnoekranowy, korzystaj z trybu pełnoekranowego w oknie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Pełna szerokość ekranu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Ustaw niestandardową szerokość w trybie pełnoekranowym bez okien. Pozostawienie go na 0 spowoduje użycie rozdzielczości pulpitu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Pełnoekranowa wysokość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Ustaw niestandardowy rozmiar wysokości dla trybu pełnoekranowego bez okien. Pozostawienie go na 0 spowoduje użycie rozdzielczości pulpitu"
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Skala okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Ustawia rozmiar okna względem głównego rozmiaru wyświetlania. Alternatywnie możesz ustawić szerokość i wysokość okna poniżej dla ustalonego rozmiaru okna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Przezroczystość okna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Pokaż dekoracje okien"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Zapamiętaj położenie i rozmiar okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Pamiętaj o wielkości i pozycji okna, włączenie tej opcji ma pierwszeństwo przed skalą okienkową"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Szerokość okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Ustaw niestandardowy rozmiar szerokości okna wyświetlacza. Pozostawienie go na 0 spowoduje przeskalowanie okna tak dużego, jak to możliwe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Wysokość okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Ustaw niestandardowy rozmiar wysokości okna wyświetlacza. Pozostawienie go na 0 spowoduje przeskalowanie okna tak dużego, jak to możliwe."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Skala całkowita"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Skaluje wideo tylko w krokach całkowitych. Rozmiar bazy zależy od raportowanej przez system geometrii i współczynnika kształtu. Jeśli 'Force Aspect' nie jest ustawione, X/Y będzie liczbą całkowitą skalowaną niezależnie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Proporcja obrazu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Konfiguruj współczynnik kształtu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Wartość zmiennoprzecinkowa dla współczynnika proporcji wideo (szerokość/wysokość), jeśli współczynnik proporcji jest ustawiony na 'Config'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Niestandardowy współczynnik kształtu X Poz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Niestandardowy współczynnik kształtu Y Poz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Niestandardowy współczynnik proporcji Szerokość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Niestandardowa szerokość ekranu, która jest używana, jeśli współczynnik proporcji jest ustawiony na 'Niestandardowy'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Niestandardowy współczynnik proporcji Wysokość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Niestandardowa wysokość ekranu, która jest używana, jeśli współczynnik proporcji jest ustawiony na 'Niestandardowy'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Przytnij Overscan (Przeładuj)"
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Synchronizacja pionowa (V-Sync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Synchronizuje wyjście wideo karty graficznej z częstotliwością odświeżania ekranu. Zalecana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Interwał wymiany pionowej synchronizacji (V-Sync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Używa niestandardowego interwału wymiany dla Vsync. Ustaw, aby efektywnie zmniejszyć o połowę częstotliwość odświeżania monitora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Adaptacyjny Vsync"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Opóźnienie klatki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Zmniejsza opóźnienie kosztem większego ryzyka rwania obrazu. Dodaje opóźnienie po V-Sync (w ms)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Trudna synchronizacja z GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Synchronizacja CPU i GPU. Zmniejsza opóźnienie kosztem wydajności."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Twarde klatki do synchronizacji z GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Określa liczbę klatek, jaką procesor może uruchomić przed GPU, gdy używana jest 'Trudna synchronizacja z GPU'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Synchronizuj z dokładną liczbą klatek na sekundę (G-Sync, FreeSync)"
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Wyjście"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Zmień ustawienia wyjścia audio."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Zmień ustawienia resamplera dźwięku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Synchronizacja"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Zmień ustawienia synchronizacji dźwięku."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Zmień ustawienia urządzeń MIDI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Ustawienia miksera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Zobacz i/lub zmień ustawienia miksera audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Dźwięki menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Wycisz dźwięk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Wycisz/włącz dźwięk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Wycisz mikser dźwięku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Wycisz/włącz dźwięk miksera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Wycisz podczas szybkiego przewijania do przodu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Automatycznie wycisz dźwięk podczas używania szybkiego przewijania do przodu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Poziom głośności dźwięku (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Głośność dźwięku (w dB). 0 dB to normalna głośność i nie jest stosowane żadne wzmocnienie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Poziom głośności miksera audio (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Globalna objętość miksera audio (w dB). 0 dB to normalna głośność i nie jest stosowane żadne wzmocnienie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Plugin dźwięku DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Wtyczka audio DSP przetwarzająca dźwięk przed wysłaniem go do sterownika."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Tryb WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Pozwól sterownikowi WASAPI na przejęcie wyłącznej kontroli nad urządzeniem audio. Jeśli jest wyłączona, zamiast tego użyje trybu wspólnego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Format WASAPI Float"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Użyj formatu float dla sterownika WASAPI, jeśli jest on obsługiwany przez twoje urządzenie audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Współdzielony bufor WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Pośrednia długość bufora (w klatkach), gdy używany jest sterownik WASAPI w trybie współdzielonym."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Włącz dźwięk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Włącz wyjście audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Urządzenie audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Zastąp domyślne urządzenie audio używane przez sterownik. Zależne od sterownika."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Opóźnienie dźwięku (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Pożądane opóźnienie dźwięku w milisekundach. Może nie być honorowane, jeśli sterownik audio nie może zapewnić określonego opóźnienia."
   )

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Jakość resamplera audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Obniż tę wartość, aby faworyzować wydajność/zmniejszyć opóźnienie w stosunku do jakości dźwięku, zwiększaj, jeśli chcesz uzyskać lepszą jakość dźwięku kosztem wydajności/mniejszego opóźnienia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Szybkość wyjścia audio (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Prędkość próbkowania wyjścia audio."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Synchronizacja dźwięku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Synchronizuj dźwięk. Zalecane."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Maksymalne przesunięcie czasowe dźwięku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Maksymalna zmiana częstotliwości wejściowej audio. Zwiększenie to umożliwia bardzo duże zmiany w taktowaniu kosztem niedokładnego nachylenia dźwięku (np. Uruchamianie rdzeni PAL na wyświetlaczach NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Dynamiczna kontrola szybkości audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Pomaga wygładzać niedoskonałości w synchronizacji czasu podczas synchronizowania dźwięku i obrazu. Pamiętaj, że jeśli jest wyłączona, właściwa synchronizacja jest prawie niemożliwa do uzyskania."
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Wejście"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Wybierz urządzenie wejściowe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Wyjście"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Wybierz urządzenie wyjściowe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Głośność"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Ustaw głośność wyjściową (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Odtwarzaj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Rozpocznie odtwarzanie strumienia audio. Po zakończeniu usunie bieżący strumień audio z pamięci."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Odtwarzaj (Zapętlone)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Rozpocznie odtwarzanie strumienia audio. Po zakończeniu zostanie zapętlony i odtworzony ponownie od początku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Odtwarzaj (Sekwencyjny)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Rozpocznie odtwarzanie strumienia audio. Po zakończeniu przeskakuje do następnego strumienia audio w kolejności sekwencyjnej i powtórzy to zachowanie. Przydatny jako tryb odtwarzania albumu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Zatrzymaj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Spowoduje to zatrzymanie odtwarzania strumienia audio, ale nie spowoduje usunięcia go z pamięci. Możesz zacząć grać ponownie, wybierając opcję 'Odtwarzaj'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Usuń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Spowoduje to zatrzymanie odtwarzania strumienia audio i całkowite usunięcie go z pamięci."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Głośność"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Dostosuj głośność strumienia audio."
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Włącz dźwięk menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Włącz lub wyłącz dźwięk menu."
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Maksymalna liczba użytkowników"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Maksymalna liczba użytkowników obsługiwanych przez RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Włącz autoconfig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Wprowadź próg osi przycisku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Jak daleko należy przechylać oś, aby uzyskać naciśnięcie przycisku."
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
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Limit czasu powiązania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Ilość sekund oczekiwania na przejście do następnej więzi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Wiązanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Ilość sekund do przechowywania danych wejściowych, aby je powiązać."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Okres turbo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
   "Cykl zapisu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "Opisuje, jak długi powinien być okres przycisku z włączoną funkcją turbo. Liczby są opisane w klatkach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Tryb Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Wybiera ogólne zachowanie trybu turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DEFAULT_BUTTON,
   "Domyślny przycisk Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DEFAULT_BUTTON,
   "Domyślny aktywny przycisk dla trybu Turbo \"Pojedynczy przycisk\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Reakcja dotykowa / wibracyje"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Zmień ustawienia dotykowego sprzężenia zwrotnego i wibracji."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Sterowanie menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Zmień ustawienia sterowania menu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Skonfiguruj ustawienia skrótu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Skonfiguruj ustawienia dla tego portu."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Wibruj po naciśnięciu klawisza"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Włącz wibracje urządzenia (dla obsługiwanych rdzeni)"
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Zunifikowane sterowanie menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Użyj tych samych elementów sterujących zarówno do menu, jak i do gry. Dotyczy klawiatury."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Zamień przyciski na OK/Anuluj. Wyłączone to orientacja japońskiego przycisku, włączona jest orientacja zachodnia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Menu sterowania wszystkich użytkowników"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Pozwala dowolnemu użytkownikowi kontrolować menu. Jeśli wyłączone, tylko użytkownik 1 może kontrolować menu."
   )

/* Settings > Input > Hotkey Binds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Menu przełączenia gamepad combo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Wczytaj zapis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Zapisz stan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Zamknij RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Slot zapisu +"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Slot zapisu -"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Przewijanie do tyłu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Zaawansowane klatki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Zresetuj grę"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Następny moduł cieniujący"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Poprzedni moduł cieniujący"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Zrób zrzut ekranu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "Wyślij informacje o debugowaniu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Przełącznik wysuwania dysku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Usługa SI"
   )

/* Settings > Input > Port # Binds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Rodzaj urządzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Typ analogowo-cyfrowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Indeks urządzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Indeks myszy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Przycisk B (w dół)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Przycisk Y (po lewej)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Wybierz przycisk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Przycisk Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Przycisk (po prawej)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Przycisk X (u góry)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "Przycisk L (ramię)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "Przycisk R"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "Przycisk L2 (spust)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2 przycisk (spust)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "Przycisk L3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "R3 przycisk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Lewy analog X+ (po prawej)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Lewy analog X- (lewy)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Lewy analog Y+ (dół)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Lewy analog Y- (w górę)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Prawy analog X+ (po prawej)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Prawy analog X- (po lewej)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Prawy analog Y+ (w dół)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Prawy analog Y- (w górę)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Spust"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Przeładowanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Pomocniczy A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Pomocniczy B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Pomocniczy C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Start pistoletu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Wybierz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "D-pad góra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "D-pad dół"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "D-pad lewo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "D-pad prawo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Włącz turbo"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
   "Przejdź do przodu w celu skrócenia czasu oczekiwania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
   "Uruchamiaj logikę rdzeniową lub więcej klatek z wyprzedzeniem, a następnie ładuj stan z powrotem, aby zmniejszyć postrzegane opóźnienie wejściowe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Liczba klatek do uruchomienia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Liczba klatek do uruchomienia. Powoduje problemy z grą, takie jak drganie, jeśli przekroczysz liczbę klatek wystąpią opóźnienia wewnętrzne w grze."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Włącz udostępniony kontekst sprzętu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Nadaj rdzeniom sprzętowym własny prywatny kontekst. Unikaj konieczności przejmowania zmian stanu sprzętu pomiędzy klatkami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Pozwól rdzeniom przełączyć sterownik wideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Atrapa rdzenia przy zatrzymaniu rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Rozpocznij rdzeń automatycznie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Sprawdź brakujące oprogramowanie sprzętowe przed ładowaniem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Przed próbą załadowania zawartości sprawdź, czy wszystkie wymagane oprogramowanie układowe jest obecne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Pozwól na rotacje"
   )

/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Automatycznie ładuj zależne od zawartości opcje rdzenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Domyślnie włącz spersonalizowane opcje rdzenia podczas uruchamiania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Automatyczne zastępowanie plików"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Domyślnie włącz niestandardową konfigurację podczas uruchamiania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Automatycznie ładuj pliki przemapowania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Domyślnie włącz niestandardowe elementy sterujące podczas uruchamiania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Automatycznie załaduj Shadery"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Użyj pliku globalnych opcji podstawowych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Zapisz wszystkie opcje rdzenia w pliku wspólnych ustawień (retroarch-core-options.cfg). Po wyłączeniu opcje dla każdego rdzenia zostaną zapisane w osobnym folderze/pliku specyficznym dla rdzenia w katalogu „Config” RetroArch."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Sortuj stany zachowywania w folderach nazwanych po używanym rdzeniu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Nie zastępuj SaveRAM przy ładowaniu stanu zapisu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Blokuj Zapisz pamięć RAM przed nadpisaniem podczas ładowania stanów zapisu. Może potencjalnie prowadzić do zbugowania gier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Autozapis SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Automatyczne zapisanie stanu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Automatycznie ładuj stan automatycznego zapisywania podczas uruchamiania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Zapisz miniatury"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Kompresja SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Zapisuj nieulotne pliki SaveRAM w zarchiwizowanym formacie. Znacząco zmniejsza rozmiar pliku kosztem (nieistotnego) wydłużenia czasu zapisywania / ładowania. Uwaga: Dotyczy tylko rdzeni, które umożliwiają zapis przez standardowy interfejs SaveRAM libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Kompresja zapisu stanu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Zapisuj pliki stanów w formacie archiwum. Dramatycznie zmniejsza rozmiar pliku kosztem zwiększonego czasu zapisywania/ładowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Napisz zapisuje do treści dir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Napisz zapis stanów do treści dir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Pliki systemowe znajdują się w katalogu treści"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Zapisuj zrzuty ekranu w katalog treści"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Zapisz dziennik środowiska wykonawczego (na rdzeń)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Śledzi, jak długo każdy element treści został uruchomiony, z rekordami oddzielonymi rdzeniem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Zapisz dziennik czasu wykonywania (agregacji)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Śledzi, jak długo każdy element treści został uruchomiony, zapisany jako suma całkowita dla wszystkich rdzeni."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Zalogowanie rozmowy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Włącz lub wyłącz rejestrowanie w terminalu."
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
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Poziom zalogowania rdzenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Ustawia poziom logu dla rdzeni. Jeśli poziom dziennika wydany przez rdzeń jest poniżej tej wartości, jest on ignorowany."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Zaloguj się do pliku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Przekierowuje komunikaty dziennika zdarzeń systemowych do pliku. Wymaga „oznajmiania rejestrowania”, aby włączyć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Pliki dziennika ze znacznikami czasu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Podczas logowania do pliku przekierowuje dane wyjściowe z każdej sesji RetroArch do nowego pliku ze znacznikami czasu. Jeśli opcja jest wyłączona, dziennik jest nadpisywany przy każdym ponownym uruchomieniu RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Liczniki wydajności"
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtruj nieznane rozszerzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Użyj wbudowanego odtwarzacza multimedialnego"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Filtruj według bieżącego rdzenia"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Przewijanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Zarządza ustawieniami przewijania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Licznik czasu klatki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Dostosuj ustawienia wpływające na licznik czasu klatek (aktywne tylko, gdy wątkowe wideo jest wyłączone)."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Maksymalna prędkość odtwarzania zawartości przy użyciu przyśpieszenia (np. 5.0x przy zawartości 60 klatek na sekundę = 300 klatek na sekundę). Ustawienie na 0.0x oznacza brak limitu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Menu obrotowe przepustnicy częstotliwości wyświetlania klatek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Upewnia się, że liczba klatek na sekundę jest ograniczona w menu."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Włącz przewianie do tyłu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Rozmiar bufora przewijania (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "Ilość pamięci (w MB) do zarezerwowania dla bufora przewijania. Zwiększenie tego zwiększy ilość historii przewijania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Krok zmiany rozmiaru bufora (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Za każdym razem, gdy zwiększysz lub zmniejszysz wartość rozmiaru bufora przewijania za pomocą tego interfejsu, zmieni się on o tę wartość"
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Resetuj po przewijaniu do przodu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Zresetuj licznik czasu klatki po przewijaniu do przodu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Zresetuj po załadowaniu stanu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Zresetuj licznik czasu klatki po załadowaniu stanu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Resetuj po zapisaniu stanu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Zresetuj licznik czasu klatki po zapisaniu stanu."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Nagrywanie wątków"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Użyj funkcji Nagrywania po filtrowaniu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Przechwyć obraz po zastosowaniu filtrów (ale nie shaderów). Twój film będzie wyglądał tak fantazyjnie jak to, co widzisz na ekranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Użyj zapisu GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Rejestruje wyjście z zacienionego materiału GPU, jeśli jest dostępne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Tryb przesyłania strumieniowego"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "Port Strumienia UDP"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Nakładka na ekranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Dopasuj ramki i elementy sterujące na ekranie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Układ wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Dostosuj układ wideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Powiadomienia na ekranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Dostosuj powiadomienia na ekranie"
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Wyświetl nakładkę"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Włącz nakładkę."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Ukryj nakładkę w menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Ukryj nakładkę w menu i pokaż ją ponownie po wyjściu z menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
   "Pokaż nakładki na nakładce"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
   "Pokaż wejścia klawiatury/kontrolera na nakładce ekranowej."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
   "Pokaż wejścia portu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
   "Wybierz port dla nakładki, aby usłyszeć, czy opcja Pokaż nakładki na nakładkę jest włączona."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Pokaż kursor myszy z nakładką"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Pokaż kursor myszy podczas korzystania z nakładki ekranowej."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Automatyczny obrót nakładki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Jeśli obsługiwane przez bieżącą nakładkę, automatycznie obracaj układ, aby dopasować orientację/proporcje ekranu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Nakładka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Automatycznie ładuj preferowaną nakładkę"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Krycie nakładki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Krycie wszystkich elementów interfejsu użytkownika nakładki."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Ustawienia nakładki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Wybierz nakładkę z przeglądarki plików."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
   "Skala nakładki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
   "Skala wszystkich elementów interfejsu nakładki."
   )



/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Włącz układ wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Układy wideo są używane dla ramek i innych elementów graficznych."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Ścieżka układu wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Wybierz układ wideo z przeglądarki plików."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Wybrany widok"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Wybierz widok w załadowanym układzie."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Włącz powiadomienia na ekranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Pokaż komunikaty na ekranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Widżety graficzne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Automatycznie zmień rozmiar udekorowanych powiadomień, wskaźników i kontrolek na podstawie aktualnej skali menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Nadpisanie skali widżetów graficznych (Pełny ekran)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Nadpisanie skali widżetów graficznych (Tryb okienkowy)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Wyświetl ilość klatek na sekundę"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Interwał aktualizacji klatek na sekundę (w klatkach)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "Wyświetlanie klatek będzie aktualizowane w ustalonych odstępach czasu (w klatkach)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Pokaż liczbę klatek na wyświetlaczu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Wyświetla bieżącą liczbę klatek na ekranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Wyświetl statystyki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Pokaż techniczne statystyki na ekranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Czcionka powiadomienia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Rozmiar powiadomienia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Określ rozmiar czcionki w punktach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Pozycja X powiadomienia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Określ niestandardową pozycję osi X dla tekstu na ekranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Pozycja Y powiadomienia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Określ niestandardową pozycję osi Y dla tekstu na ekranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Powiadomienie czerwony kolor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Powiadomienie zielony kolor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Powiadomienie niebieski kolor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Włącz tło powiadomień"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Czerwony kolor powiadomienia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Zielony kolor powiadomienia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Niebieski kolor powiadomienia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Krycie tła powiadomienia"
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Wygląd"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Dostosuj ustawienia wyglądu ekranu menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Pokaż ustawienia zaawansowane"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Włącz tryb kiosku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Chroni konfigurację, ukrywając wszystkie ustawienia związane z konfiguracją."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Ustaw hasło do wyłączania trybu kiosku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Podanie hasła podczas włączania trybu kiosku umożliwia późniejsze wyłączenie go z menu, przechodząc do Menu głównego, wybierając Wyłącz tryb kiosku i wprowadzając hasło."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Nawigacja owinięciem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Przewijanie do początku i/lub końca, jeśli granica listy jest osiągnięta poziomo lub pionowo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Jeśli wyłączone, zawartość będzie nadal działać w tle, gdy menu RetroArch jest przełączane."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Wznów zawartość po użyciu stanów zapisu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Wznów zawartość po zmianie dysków"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Obsługa myszy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Obsługa dotyku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Zadania z wątkami"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Wykonuj zadania w oddzielnym wątku."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Wstrzymaj grę, gdy RetroArch nie jest aktywnym oknem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Wyłącz kompozycję pulpitu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Włącz lub wyłącz kompozycję (tylko system Windows)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Przyspieszenie przewijania menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Maksymalna prędkość kursora przy trzymaniu kierunku przewijania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Włącz Companion UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Pasek menu"
   )

/* Settings > User Interface > Views */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Szybkie menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Ustawienia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Pokaż ładowanie rdzenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Pokaż/ukryj opcję 'Załaduj rdzeń'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Pokaż zawartość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Pokaż/ukryj opcję \"Wczytaj zawartość\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Pokaż Załaduj dysk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Pokaż/ukryj opcję 'Załaduj dysk'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Pokaż Zgraj dysk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Pokaż/ukryj opcję 'Zgraj dysk'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Pokaż aktualizacje online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Pokaż/ukryj opcję 'Aktualizacja online'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Pokaż pobierz rdzeń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Pokaż/ukryj możliwość aktualizacji rdzeni (i podstawowych plików informacyjnych)."
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
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Pokaż informacje"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Pokaż/ukryj opcję 'Informacje'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Pokaż konfiguracje"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Pokaż/ukryj opcję 'Konfiguracje'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Pokaż pomoc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Pokaż/ukryj opcję \"Pomoc\"."
   )
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
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Pokaż kartę Ustawienia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Pokaż kartę Ustawień w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Podanie hasła podczas ukrywania karty ustawień pozwala później przywrócić ją z menu, przechodząc do karty Menu główne, wybierając opcję Włącz kartę Ustawienia i wprowadzając hasło."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Pokaż kartę Ulubione"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Pokaż zakładkę Ulubione w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Pokaż kartę Obraz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Pokaż kartę Obrazu w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Pokaż kartę Muzyka"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Pokaż zakładkę Muzyka w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Pokaż kartę Wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Pokaż kartę Video w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Pokaż kartę Gry Online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Pokaż zakładkę Netplay w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Pokaż kartę Historii"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Pokaż kartę ostatnich historii w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "Pokaż kartę Importuj zawartość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "Pokaż kartę treści importu w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Pokaż karty playlist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Pokaż karty playlist w menu głównym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Pokaż datę/czas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Pokazuje aktualną datę i/lub czas w menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Styl daty/godziny"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Zmienia bieżący styl daty i/lub czasu wyświetlany wewnątrz menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Pokaż poziom naładowania baterii"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Pokazuje aktualny poziom naładowania baterii w menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Pokaż nazwę rdzenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Wyświetla bieżącą nazwę rdzenia w menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Pokaż podpowiedzi menu"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Wyświetl ekran startowy"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Pokaż ekran startowy w menu. Po pierwszym uruchomieniu program jest automatycznie ustawiany na false."
   )

/* Settings > User Interface > Views > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Pokaż znów zawartość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Pokaż/ukryj opcję „Wznów treść”."
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
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Pokaż zrzut ekranu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Pokaż/ukryj opcję \"Zrób zrzut ekranu\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Pokaż stan zapisywania/ładowania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Pokaż/ukryj opcje zapisywania/ładowania stanu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Pokaż opcje cofnij zapisanie/załadowanie stanu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Pokaż/ukryj opcje cofania stanu zapisywania/ładowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Pokaż dodaj do ulubionych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Pokaż/ukryj opcję 'Dodaj do ulubionych'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Pokaż Rozpocznij nagrywanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Pokaż/ukryj opcję 'Rozpocznij nagrywanie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Pokaż rozpoczęcie strumieniowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Pokaż / ukryj opcję 'Rozpocznij transmisję'."
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
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Pokaż Resetuj podstawowe skojarzenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Pokaż/ukryj opcję 'Resetuj podstawowe skojarzenie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Pokaż opcje"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Pokaż/ukryj opcję 'Opcje'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Pokaż elementy sterujące"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Pokaż/ukryj opcję 'Sterowanie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Pokaż kody"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Pokaż/ukryj opcję 'Kody'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Pokaż shadery"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Pokaż/ukryj opcję 'Shadery'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Pokaż ustawienia przewijania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Pokaż/ukryj opcje przewijania."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Pokaż/ukryj opcje opóźnień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Pokaż ustawienia opóźnień"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Pokaż/ukryj opcje nakładki."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Pokaż ustawienia nakładek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Pokaż ustawienia układu wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Pokaż/ukryj opcje układu wideo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Pokaż zapis nadpisu rdzenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Pokaż/ukryj opcję 'Zapisz podstawowe przesłonięcia'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Pokaż zapisy gry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Pokaż/ukryj opcję 'Zachowaj pominięcia gry'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Pokaż informacje"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Pokaż/ukryj opcję 'Informacje'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Pokaż Pobierz Miniatury"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Pokaż/ukryj opcję „Pobierz miniatury”."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Pokaż sterownik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Pokaż/ukryj 'Ustawienia sterownika' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Pokaż wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Pokaż/ukryj 'Ustawienia wideo' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Pokaż dźwięk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Pokaż/ukryj 'Ustawienia dźwięku' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Pokaż dane wejściowe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Pokaż/ukryj 'Ustawienia wprowadzania' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Pokaż opóźnienie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Pokaż/ukryj 'Ustawienia opóźnień' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Pokaż rdzeń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Pokaż/ukryj 'Ustawienia rdzenia' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Pokaż konfigurację"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Pokaż/ukryj 'Ustawienia konfiguracji' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Pokaż zapisywanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Pokaż/ukryj 'Ustawienia zapisywania' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Pokaż rejestrowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Pokaż/ukryj 'Ustawienia rejestrowania' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Pokaż Manipulację klatek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Pokaż/ukryj 'Ustawienia manipulacji klatek' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Pokaż nagrywanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Pokaż/ukryj 'Ustawienia nagrywania' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Pokaż wyświetlacz na ekranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Pokaż/ukryj 'Ustawienia wyświetlania na ekranie' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Pokaż interfejs użytkownika"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Pokaż/ukryj 'Ustawienia interfejsu użytkownika' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Pokaż usługę SI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Pokaż/ukryj ustawienia usługi AI na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Pokaż zarządzanie zasilaniem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Pokaż/ukryj 'Ustawienia zarządzania energią' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Pokaż osiągnięcia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Pokaż/ukryj 'Ustawienia Osiągnięć' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Pokaż sieć"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Pokaż/ukryj 'Ustawienia sieciowe' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Pokaż playlisty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Pokaż/ukryj 'Ustawienia playlist' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Pokaż użytkownika"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Pokaż/ukryj 'Ustawienia użytkownika' na ekranie ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Pokaż katalog"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Pokaż/ukryj 'Ustawienia katalogu' na ekranie ustawień."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Współczynnik skali menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Nieprzezroczystość tła"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Nieprzezroczysty bufor klatki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Zmodyfikuj krycie bufora klatki."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Użyj preferowanego motywu kolorystycznego systemu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Użyj motywu kolorystycznego systemu operacyjnego (jeśli jest dostępny) - zastępuje ustawienia kompozycji."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Miniatury"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Typ wyświetlanej miniatury."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Próg skalowania miniatur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Automatycznie skaluj w górę miniaturki o szerokości / wysokości mniejszej niż określona wartość. Poprawia jakość obrazu. Ma umiarkowany wpływ na wydajność."
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
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Gładki tekst paska"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Wyjście usługi AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Pokaż tłumaczenie jako nakładkę tekstową (tryb obrazu) lub odtwarzaj jako tekst na mowę (tryb mowy)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "Adres URL usługi AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Adres URL http: // wskazujący na usługę tłumaczeniową do użycia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Usługa AI włączona"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Włącz usługę AI, aby była uruchamiana po naciśnięciu klawisza skrótu usługi AI."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Wstrzymuje rdzeń podczas tłumaczenia ekranu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Język źródłowy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "Język, z którego usługa będzie tłumaczona. Jeśli ustawione na „Nie przejmuj się”, spróbuje automatycznie wykryć język. Ustawienie go na określony język sprawi, że tłumaczenie będzie dokładniejsze."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Język docelowy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "Język, na który będzie tłumaczona usługa. Jeśli ustawione na „Nie przejmuj się”, domyślnie będzie ustawiony na angielski."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Dostępność włączona"
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Włącz osiągnięcia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Osiągnięcia trybu hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Podwójna ilość zdobytych punktów.\nWyłącza zapisy, cheat-y, przewijanie, pauzy i wolniejsze ruchy dla wszystkich gier.\nPrzełączanie tego ustawienia w czasie pracy zrestartuje grę."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Tabele wyników"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "Tryb Rich Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Wysyła szczegółowy status gry na stronę internetową RetroOsiągnięć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Odznaki osiągnięć"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Włącz lub wyłącz wyświetlanie znaczków na liście osiągnięć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Sprawdź nieoficjalne osiągnięcia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Włącz lub wyłącz nieoficjalne osiągnięcia i/lub funkcje beta do celów testowych."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Osiągnięcia trybu pełnego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Włącz lub wyłącz powiadomienia OSD dla osiągnięć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Automatyczny zrzut ekranu osiągnięć"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Uruchom aktywny"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Rozpocznij sesję z aktywnymi osiągnięciami (nawet tymi, które wcześniej odblokowałeś)."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Upublicznianie sesji online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Czy publicznie ogłaszać gry internetowe. W przypadku braku ustawienia klienci muszą ręcznie łączyć się, zamiast korzystać z publicznego lobby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Użyj serwera przekaźnikowego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Przekaż połączenia sieciowe przez serwer pośredniczący. Przydatne, jeśli host znajduje się za zaporą lub ma problemy z NAT/UPnP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Lokalizacja serwera przekaźnikowego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Wybierz określony serwer przekazujący, którego chcesz użyć. Geograficznie bliższe lokalizacje mają zazwyczaj mniejsze opóźnienie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Adres serwera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Adres hosta, z którym chcesz się połączyć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Port TCP gry online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Port adresu IP hosta. Może to być port TCP lub UDP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Hasło serwera"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Hasło spontaniczne serwera"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Tryb widza gry online"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Zezwalaj na klientów w trybie slave"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Odmów klientów w trybie innym niż slave"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
   "Tryb bezstanowej gry online"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Sprawdź klatki gry online"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Wejściowe klatki opóźnień"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Liczba klatek opóźnienia wejściowego dla netplay do wykorzystania do ukrycia opóźnień sieci. Zmniejsza drgania i sprawia, że gra jest mniej intensywna, kosztem zauważalnego opóźnienia wejścia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Zakres latencji wejściowych klatek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Zakres klatek opóźnień wejściowych, które można wykorzystać do ukrycia opóźnień sieci. Zmniejsza fluktuacje i sprawia, że netplay jest mniej obciążający procesor, kosztem nieprzewidywalnego opóźnienia wejściowego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Gra online NAT Traversal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Podczas hostowania, próbuj słuchać połączeń z publicznego Internetu, używając UPnP lub podobnych technologii, aby uciec z sieci LAN."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Udostępnianie wejścia cyfrowego"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Zażądaj urządzenia %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Żądanie odtworzenia za pomocą danego urządzenia wejściowego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Polecenia sieciowe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Port poleceń sieciowych"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "Polecenia STDIN"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Włącz interfejs poleceń Stdin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Pobieranie miniatur na żądanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Aktualizacja"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Adres URL rdzeni Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "Adres URL do głównego katalogu Updater na kompilatorze Libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Adres URL zasobów Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "Adres URL do katalogu aktualizującego zasoby na kompilatorze Libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Automatycznie wyodrębnij pobrane archiwum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Po pobraniu automatycznie wyodrębnia pliki zawarte w pobranych archiwach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Pokaż eksperymentalne rdzenie"
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Włącz lub wyłącz listę najnowszych odtwarzanych, zdjęć, muzyki i filmów."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Rozmiar listy historii"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Ogranicz liczbę wpisów na liście odtwarzania dla gier, zdjęć, muzyki i filmów."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Ogranicz liczbę wpisów na liście odtwarzania ulubionych. Po osiągnięciu limitu nowe dodatki będą blokowane, dopóki stare wpisy nie zostaną usunięte. Ustawienie wartości -1 pozwala na „nieograniczoną” liczbę (99999) wpisów. OSTRZEŻENIE: Zmniejszenie wartości spowoduje usunięcie istniejących wpisów!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Zezwalaj na zmianę nazw wpisów"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Pozwól użytkownikowi zmieniać nazwy wpisów na listach odtwarzania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Zezwalaj na usuwanie wpisów"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Pozwól użytkownikowi usunąć wpisy z list odtwarzania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Sortuj listy alfabetycznie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Zapisz playlisty w starym formacie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Kompresuj playlisty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Archiwizuj dane playlisty podczas zapisywania na dysku. Zmniejsza rozmiar pliku i czasy ładowania kosztem (pomijalnym) zwiększonego użycia procesora. Może być używany ze starymi lub nowymi listami odtwarzania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Pokaż powiązane rdzenie na listach odtwarzania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Określ, kiedy oznaczyć pozycje playlisty aktualnie z aktualnie powiązanym rdzeniem (jeśli istnieje). UWAGA: To ustawienie jest ignorowane po włączeniu podtytułów playlisty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Pokaż etykiety pomocnicze playlisty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Wyświetla dodatkowe informacje dla każdego wpisu playlisty, takie jak bieżące powiązanie rdzenia i środowisko wykonawcze (jeśli jest dostępne). Ma zmienny wpływ na wydajność."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Rdzeń:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Ostatnia gra:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Czas działania etykiety podrzędnej playlisty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Wybiera typ rekordu dziennika środowiska wykonawczego, który ma być wyświetlany na podtytułach playlisty. (Należy pamiętać, że odpowiedni dziennik środowiska wykonawczego musi być włączony za pomocą menu opcji „Zapisywanie”)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Dopasowanie rozmycia archiwum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Podczas wyszukiwania list odtwarzania dla wpisów powiązanych ze skompresowanymi plikami, dopasuj tylko nazwę pliku archiwum zamiast [nazwa pliku] + [treść]. Włącz to, aby uniknąć duplikatów wpisów historii treści podczas ładowania skompresowanych plików."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Skanuj bez dopasowania podstawowego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Po wyłączeniu zawartość jest dodawana tylko do playlisty, jeśli zainstalowany jest rdzeń, który obsługuje jej rozszerzenie. Włączając to, niezależnie od tego doda do playlisty. W ten sposób możesz zainstalować rdzeń, którego potrzebujesz później po skanowaniu."
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Domyślny rdzeń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Określ rdzeń używany podczas uruchamiania treści poprzez wpis playlisty, który nie posiada istniejącego powiązania rdzenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Resetuj skojarzenia rdzenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Usuń istniejące powiązania dla wszystkich wpisów na playliście."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Tryb wyświetlania etykiet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Zmień sposób wyświetlania etykiet treści na tej liście odtwarzania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Metoda sortowania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Wyczyść playlistę"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Usuń listę odtwarzania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Usuń playlistę z systemu plików."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Prywatność"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Zmień ustawienia prywatności."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Konta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Zarządzaj aktualnie skonfigurowanymi kontami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Nazwa Użytkownika"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Wprowadź tutaj swoją nazwę użytkownika. Będzie to wykorzystywane między innymi do gry internetowej."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Język"
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Zezwalaj na kamerę"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Zezwalaj lub nie zezwalaj na dostęp do kamery przez rdzenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Włącz Discord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Pozwala aplikacji Discorda pokazywać więcej danych o odtwarzanej treści.\nUWAGA: Nie będzie działać z wersją przeglądarki, tylko z natywnym klientem stacjonarnym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Zezwalaj na lokalizację"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Zezwalaj lub nie zezwalaj na dostęp do usług lokalizacyjnych przez rdzenie."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Retro osiągnięcia"
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Nazwa Użytkownika"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Hasło"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Wprowadź hasło do swojego konta RetroAchievements."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "Klucz strumienia YouTube"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Klucz strumienia Twitch"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "Ustawia katalog systemowy. Rdzenie mogą wysyłać zapytania do tego katalogu, aby załadować BIOS, konfiguracje specyficzne dla systemu itp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Pobrane pliki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Zapisz wszystkie pobrane pliki w tym katalogu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Assety"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Dynamiczne tła"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Katalog do przechowywania tapet dynamicznie ładowanych przez menu w zależności od kontekstu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Miniatury"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Dodatkowe miniaturki (boxarty/miscale itp.) Są tutaj przechowywane."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Przeglądarka plików"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Konfiguracja"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Ustawia katalog początkowy dla przeglądarki konfiguracji menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Rdzeń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Katalog, w którym program szuka treści/rdzeni."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Informacje o rdzeniu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Pliki informacji o aplikacji/rdzeniu przechowywane są tutaj ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Baza danych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Bazy danych są przechowywane w tym katalogu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
   "Kursor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
   "Zapisane zapytania są przechowywane w tym katalogu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Oszukane pliki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Pliki do pobrania są tutaj przechowywane."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Filtr wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Katalog, w którym przechowywane są pliki filtrów wideo oparte na procesorze."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Filtr audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Katalog, w którym przechowywane są pliki filtrów DSP audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Moduł cieniujący wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Definiuje katalog, w którym pliki shaderów wideo oparte na GPU są przechowywane dla łatwego dostępu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Wyjście nagrywania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Nagrania zostaną zrzucone do tego katalogu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Konfig. Nagrywania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Nakładka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Układ wideo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Określa katalog, w którym układy wideo są przechowywane dla łatwego dostępu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Zrzut ekranu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Katalog do zrzutu zrzutów ekranu do."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Wprowadź autoconfig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Przemapowanie wejścia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Playlisty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Zapisz wszystkie playlisty w tym katalogu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Dzienniki uruchomia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Zapisz pliki dziennika środowiska wykonawczego do tego katalogu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Zapisz plik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Zapisz wszystkie pliki zapisu w tym katalogu. Jeśli nie jest ustawione, spróbuje zapisać w katalogu roboczym pliku zawartości."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Zapisz stan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Zapisz wszystkie stany zachowania w tym katalogu. Jeśli nie jest ustawione, spróbuje zapisać w katalogu roboczym plików zawartości."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Pamięć podręczna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Jeśli ustawione na katalog, zawartość, która jest czasowo wyodrębniana (np. Z archiwów), zostanie wyodrębniona do tego katalogu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Dzienniki zdarzeń systemowych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Zapisz pliki dziennika zdarzeń systemowych w tym katalogu."
   )

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Dodaj do miksera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Dodaj tę ścieżkę audio do dostępnego gniazda strumienia audio. Jeśli obecnie nie ma wolnych miejsc, zostanie to zignorowane."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Dodaj do miksera i odtwarzaj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Dodaj tę ścieżkę audio do dostępnego gniazda strumienia audio i odtwarzaj. Jeśli obecnie nie ma wolnych miejsc, zostanie to zignorowane."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Połącz się z hostem gry online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Włącza grę sieciową w trybie klienta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Odłącz od hosta gry online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Odłącz aktywne połączenie gry online."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Uruchom hosta gry online"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Włącza funkcję netplay w trybie hosta (serwer)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Zatrzymaj hosta gry"
   )

/* Import content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Skanuj katalog"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Skanuje katalog w poszukiwaniu kompatybilnych plików."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Zeskanuj ten katalog>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Zeskanuj plik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Skanuje zgodny plik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Ręczne skanowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Konfigurowalne skanowanie na podstawie nazw plików zawartości. Nie wymaga zawartości do dopasowania bazy danych."
   )

/* Import content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Dodaj do miksera"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Dodaj do miksera i odtwarzaj"
   )

/* Import content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Katalog zawartości"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Wybiera katalog do skanowania zawartości."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Nazwa systemu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Określ 'nazwę systemu', z którą można powiązać zeskanowane treści. Używane jako nazwa wygenerowanego pliku playlisty i identyfikacji miniatur playlisty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Niestandardowa systemu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Ręcznie określ 'nazwę systemu' dla zeskanowanych treści. Używane tylko wtedy, gdy 'Nazwa systemu' jest ustawiona na '<Niestandardowy>'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Wybierz domyślny rdzeń do użycia podczas uruchamiania zeskanowanych treści."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Rozszerzenia pliku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Skanuj wewnątrz archiwów"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Gdy włączone, pliki archiwalne (.zip, .7z, itp.) będą przeszukiwane pod kątem poprawnej/obsługiwanej zawartości. Może mieć znaczący wpływ na wydajność skanowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Plik Arcade DAT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Wybierz plik listy XML DAT Logiqx lub MAME, aby włączyć automatyczne nazywanie zeskanowanej zawartości arkade (MAME, FinalBurn Neo, itp.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Filtr Arcade DAT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Korzystając z arkadowego pliku DAT, dodawaj zawartość do listy odtwarzania tylko wtedy, gdy znaleziono pasujący wpis pliku DAT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Nadpisz istniejącą playlistę"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Gdy włączone, każda istniejąca playlista zostanie usunięta przed skanowaniem zawartości. Jeśli wyłączone, istniejące wpisy listy odtwarzania są zachowane i tylko zawartość obecnie brakująca w liście odtwarzania zostanie dodana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Rozpocznij skanowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Zeskanuj wybraną zawartość."
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Uruchom"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Uruchom tytuł."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Zmień nazwę"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Zmień nazwę tytułu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Usuń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Usuń ten wpis z playlisty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Dodaj do ulubionych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Dodaj wpis do ulubionych."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Ustaw powiązanie rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Zresetuj domyślny rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informacja"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Zobacz więcej informacji o zawartości."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Pobierz miniatury"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Pobierz zrzut ekranu/okładkę/ekran tytułowy dla bieżącej zawartości. Aktualizuje wszystkie istniejące miniatury."
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Nazwa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Ścieżka do pliku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Czas w grze"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Ostatnia gra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Baza danych"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Wznów zawartość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Wznów aktualnie uruchomioną zawartość i opuść szybkie menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Uruchom ponownie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Ponownie uruchamia zawartość od początku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Zamknij zawartość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Zamyka aktualną zawartość. Wszelkie niezapisane zmiany mogą zostać utracone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Zrób zrzut ekranu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Przechwytuje obraz ekranu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Slot zapisu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Zmienia aktualnie wybrany przedział stanu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Zapisz stan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Zapisz stan w aktualnie wybranym gnieździe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Wczytaj zapis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Załaduj zapisany stan z aktualnie wybranego gniazda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Cofnij załadowanie stanu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Jeśli stan został załadowany, zawartość wróci do stanu sprzed załadowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Cofnij zapisanie stanu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Jeśli stan został nadpisany, zostanie przywrócony do poprzedniego stanu zapisu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Dodaj do ulubionych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Dodaj wpis do ulubionych."
   )
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
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Opcje"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Zmień opcje aktualnie wyświetlanej treści."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Sterowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Zmień ustawienia dla aktualnie wyświetlanej treści."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Kody"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Skonfiguruj kody."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Kontrola dysku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Zarządzanie obrazem dysku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Shadery"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Skonfiguruj shadery w celu polepszenia obrazu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Opcje zastępowania konfiguracji"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Opcje nadpisania konfiguracji globalnej."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Lista osiągnięć"
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Utwórz plik opcji gry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Plik opcji gry"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Załaduj plik przemapowania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Zapisz plik przemapowania rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Usuń plik przemapowania rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Zapisz plik przemapowania zawartości"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Usuń plik przemapowania katalogu zawartości gry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Zapisz plik przemapowania gry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Usuń plik przemapowania gry"
   )

/* Quick Menu > Controls > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Plik przemapowania"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Rozpocznij lub kontynuuj wyszukiwanie kodów"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Załaduj oszukany plik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Załaduj plik kodów i zastąp istniejące kody."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Załaduj plik kodu (dołącz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Załaduj plik kodów i dołącz do istniejących kodów."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Odśwież Kody specyficzne dla gry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Zapisz oszukany plik jako"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Zapisz bieżące kody jako plik składowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Automatyczne stosowanie cheatów podczas ładowania gry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Automatyczne stosowanie kodów podczas ładowania gry."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Zastosuj po przełączeniu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Zastosuj kody natychmiast po przełączeniu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Zatwierdź zmiany"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Zmiany kodu odniosą skutek natychmiast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Oszukać"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Rozpocznij lub zrestartuj wyszukiwanie cheat-ów"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Lewo/Prawo by zmienić rozmiar bitu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
   "Big Endian (grubokońcowość)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Równa się %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Mniejszy niż przed"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Mniejszy lub równy niż przed"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Większy niż przed"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Wiekszy lub równy niż przed"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Równy jak przed"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Nie równy jak przed"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Lewo/Prawo by zmienić wartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Równy jak przed+%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Wyszukaj wartość w pamięci"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Lewo/Prawo by zmienić wartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Równy jak przed-%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Dodaj %u dopasowań do Twojej listy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Usuń mecz #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Utwórz kodu meczu #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Adres meczu: %08X maska: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Oszukane pliki"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Załaduj plik cheat"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Szczegóły cheat-u"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Indeks"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Pozycja indeksu na liście."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Włączony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Opis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Uchwyt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Rozmiar wyszukiwania w pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Typ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Wartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Adres pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Przeglądaj adres: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Maska adresowa pamięci"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Adres maski bitowej, gdy rozmiar wyszukiwania w pamięci <8-bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Liczba Iteracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Adres zwiększa każdą iterację"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Wartość zwiększa każdą iterację"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Wibruj, gdy pamięć"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Wartość wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Port wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Siła wibracji podstawowych"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Czas trwania wibracji podstawowych (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Siła wibracji drugorzędnych"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Czas trwania wibracji drugorzędnych (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Kod"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Dodaj nowy cheat po tym"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Dodaj nowy cheat przed tym"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Skopiuj ten cheat po"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Skopiuj ten cheat przed"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Wysuń dysk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Włóż dysk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Załaduj nowy dysk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Wyjmij bieżący dysk, wybierz nowy dysk z systemu plików, a następnie wstaw go i zamknij wirtualną tackę. UWAGA: To jest starsza funkcja. Zamiast tego zaleca się wczytywanie wielu tytułów dysków za pośrednictwem list odtwarzania M3U, które umożliwiają wybór dysku przy użyciu opcji 'Wyjmij/Włóż dysk' i 'Bieżący indeks dysków'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Indeks dysku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Wybierz bieżący dysk z listy dostępnych obrazów. Dysk zostanie załadowany po wybraniu 'Włóż dysk'."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Shadery Video"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Oglądaj pliki modułu cieniującego w poszukiwaniu zmian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Załaduj ustawienia Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Załaduj preset modułu cieniującego. Potok cieniujący zostanie automatycznie skonfigurowany."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
   "Zapisz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "Zapisz ustawienie bieżącego shader-a."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
   "Usuń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "Usuń wstępne ustawienia modułu cieniującego określonego typu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Zatwierdź zmiany"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Zmiany w konfiguracji modułu cieniującego zostaną natychmiast zastosowane. Użyj tej opcji, jeśli zmienisz liczbę przejść dla modułów cieniujących, filtrowanie, skalę FBO itp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Podgląd parametrów modułu cieniującego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Modyfikuje bezpośrednio bieżący moduł cieniujący. Zmiany nie zostaną zapisane w ustawionym pliku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Przepustka Shadera"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "Cieniowanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Filtr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Skala"
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Zapisz ustawienie Shadera jako"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Zapisz bieżące ustawienia modułu cieniującego jako nowe ustawienie domyślne modułu cieniującego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Zapisz ustawienie globalne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Zapisz bieżące ustawienia shader-a jako domyślne ustawienia globalne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Zapis ustawienia podstawowe rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Zapisz ustawienie zawartości katalogu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Zapisz ustawienie gry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Zapisz bieżące ustawienia modułu cieniującego jako ustawienia domyślne dla zawartości."
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Nie znaleziono automatycznych ustawień cieniowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Usuń globalne ustawienie wstępne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Usuń ustawienie globalne, używane przez całą zawartość i wszystkie rdzenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Usuń ustawienia wstępne rdzenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Usuń ustawienie wstępne rdzenia, używane przez całą zawartość uruchomioną z aktualnie załadowanym rdzeniem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Usuń ustawienie katalogu treści"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Usuń ustawienie wstępne katalogu treści, używane przez całą zawartość w bieżącym katalogu roboczym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Usuń ustawienie gry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Usuń ustawienie wstępne gry, używane tylko dla konkretnej gry, o której mowa."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Brak parametrów modułu cieniującego."
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Zapisz przesłonięcia rdzenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Zapisuje plik konfiguracji zastąpienia, który będzie obowiązywał dla wszystkich treści załadowanych tym rdzeniem. Ma pierwszeństwo przed główną konfiguracją."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Zapisz nadpisania katalogu zawartości"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Zapisuje plik konfiguracji zastępowania, który będzie obowiązywał dla całej zawartości ładowanej z tego samego katalogu, co bieżący plik. Będzie mieć pierwszeństwo przed główną konfiguracją."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Zapisz nadpisania gry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Zapisuje plik konfiguracji zastąpienia, który będzie dotyczył tylko bieżącej treści. Ma pierwszeństwo przed główną konfiguracją."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Brak osiągnięć do wyświetlenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Wstrzymaj tryb osiągnięć hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Wstrzymaj osiągnięcia na bieżącej sesji (To działanie umożliwi zapisywanie, cheat-y, przewijanie, wstrzymanie i spowolnienie ruchu)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Wznów tryb osiągnięć hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Wznów osiągnięcia dla bieżącej sesji (ta akcja wyłączy zapisywanie stanów, cheat-y, przewijanie, wstrzymanie i spowolnienie ruchu, a także zresetuje bieżącą grę)."
   )

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "RetroAchievements hash"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Wpis bazy danych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Pokaż informacje bazy danych dla bieżącej zawartości"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Brak wpisów do wyświetlenia."
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Brak dostępnych rdzeni."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Brak opcji podstawowych rdzenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Brak dostępnych podstawowych informacji."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Brak ulubionych."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Brak historii."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Brak zdjęć."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Brak muzyki."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Brak filmów."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Brak informacji."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Brak dostępnych pozycji na liście odtwarzania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Nie znaleziono ustawień."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Nie znaleziono sieci."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Bez rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Szukaj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Z powrotem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Nadrzędny katalog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Nie znaleziono katalogu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Nie ma plików."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Wybierz plik"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Nieznany kompilator"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Podziel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Zahacz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Głosuj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Udostępnianie wejścia analogowego"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Maks."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Średni"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Brak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Brak preferencji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Odbijanie lewo/prawo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Przewijanie w lewo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Tryb obrazu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Tryb mowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Tryb Narratora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Historia i Ulubione"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Wszystkie playlisty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "WYŁ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Historia i Ulubione"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Zawsze"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Nigdy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Na rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Zagregowane"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Naładowany"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Ładowanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Rozładowywanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Użyj tego katalogu>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Ścieżka treści>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Domyślny>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Żaden>"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
   "Retro klawiatura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD,
   "Retro pad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Brak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Nieznany"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Przytrzymaj Start (2 sekundy)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "W dół + Wybierz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Wyłączone>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Zmiany"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Nie zmienia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Zwiększa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Zmniejsza się"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= Wartości wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= Wartości wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< Wartości wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> Wartości wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Zwiększa o wartość wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Zmniejsza o wartość wibracji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Wszystkie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Wyłączone>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Ustaw wartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Zwiększ o wartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Zmniejsz o wartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Uruchom następny chet, jeśli wartość = pamięć"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Uruchom następny chet, jeśli wartość != pamięć"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Uruchom następny chet, jeśli wartość < pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Uruchom następny chet, jeśli wartość > pamięć"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-bit, maksymalna wartość = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-bit, maksymalna wartość = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-bit, maksymalna wartość = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-bit, maksymalna wartość = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-bit, maksymalna wartość = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-bit, maksymalna wartość = 0xFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Systemowe ustawienie domyślne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Alfabetycznie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Brak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Pokaż pełne etykiety"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Usuń () treść"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Usuń [] treść"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Usuń () i []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Zachowaj region"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Zachowaj indeks płyty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Zachowaj indeks regionu i dysku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Systemowe ustawienie domyślne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Okładki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Zrzuty ekranu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Ekrany tytułowe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "Normalnie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Szybko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "Włącz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "Wyłącz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Tak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Nie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Prawdziwe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Fałszywy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Włączone"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Wyłączony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "Nie dotyczy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Zablokowany"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Odblokowany"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Nieoficjalne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Nieobsługiwane"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Brak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Liniowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Najbliższy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Katalog zawartości>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Niestandardowy>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Nieokreślone>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Lewy analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Prawy Analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "(Klucz: %s)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Mysz 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Mysz 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Mysz 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Mysz 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Mysz 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Wcześnie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "Normalny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Późno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "RRRR-MM-DD GG:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "RRRR-MM-DD GG:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "RRRR-MM-DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "RRRR-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "DD-MM-RRRR GG:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "DD-MM-RRRR GG:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "MM-DD GG:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "MM-DD-RRRR"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "DD-MM-RRRR GG:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "DD-MM-RRRR GG:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "DD-MM GG:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "DD-MM-RRRR"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS,
   "GG:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM,
   "GG:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "RRRR-MM-DD GG:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "RRRR-MM-DD GG:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "MM-DD-RRRR GG:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "MM-DD-RRRR GG:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "MM-DD GG:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "DD-MM-RRRR GG:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "DD-MM-RRRR GG:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "DD-MM GG:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM,
   "GG:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM,
   "GG:MM (AM/PM)"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Włącz grubość wypełniacza tła"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Zwiększ grubość wzoru szachownicy tła menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Włącz wypełnienie granicy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Grubość wypełnienia obramowania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Zwiększa rozmiar wzoru szachownicy w tle obramowania."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Wyświetl obramowanie menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Użyj układu pełnej szerokości"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Zmień rozmiar i pozycję wpisów menu, aby jak najlepiej wykorzystać dostępną przestrzeń ekranu. Wyłącz to, aby użyć klasycznego układu dwóch kolumn o stałej szerokości."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Menu filtra liniowego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Dodaje lekkie rozmycie do menu, aby usunąć krawędzie twardych pikseli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Wewnętrzne skalowanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Rozwiń interfejs menu przed rysowaniem do ekranu. Gdy używany jest z włączonym 'Filtrem liniowym menu', usuwa artefakty skalowania (nierównomierne piksele), zachowując ostry obraz. Ma znaczący wpływ na wydajność, która wzrasta wraz ze wzrostem poziomu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Współczynnik proporcji menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Wybierz współczynnik proporcji menu. Współczynniki panoramiczne zwiększają poziomą rozdzielczość interfejsu menu. (Może wymagać ponownego uruchomienia, jeśli opcja „Zablokuj proporcje menu” jest wyłączona)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Zablokuj współczynnik proporcji menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Gwarantuje, że menu jest zawsze wyświetlane z właściwym współczynnikiem kształtu. Jeśli wyłączone, szybkie menu zostanie rozciągnięte, aby dopasować się do aktualnie załadowanej zawartości."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Motyw koloru menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Wybierz inny motyw koloru. Wybranie opcji „Niestandardowe” umożliwia korzystanie z plików predefiniowanych kompozycji menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Niestandardowe ustawienia motywu menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Wybierz predefiniowany motyw menu z przeglądarki plików."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Efekty cienia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Włącz cienie dla tekstu menu, ramek i miniatur. Ma skromny wpływ na wydajność."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Animacja tła"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Włącz efekt animacji cząstek tła. Ma znaczący wpływ na wydajność."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Prędkość animacji w tle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Dostosuj szybkość animacji efektu cząstek tła."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Pokaż miniatury na playliście"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Włącz wyświetlanie miniatur przeskalowanych w dół podczas przeglądania list odtwarzania. Gdy wyłączone, 'Wierzchnia miniatura' może nadal być przełączona na pełny ekran przez naciśnięcie RetroPad Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Wierzchnia miniatura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Typ miniatury wyświetlanej w prawym górnym rogu playlisty. Ta miniaturka może być przełączona na pełny ekran, naciskając RetroPad Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Spodnia miniatura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Typ miniatury do wyświetlenia w prawym dolnym rogu playlisty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Zamień miniatury"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Zamienia pozycje wyświetlania \"Wierzchniej miniatury\" i \"Spodniej miniatury”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Metoda redukcji skali miniatury"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Metoda Resamplingu używana podczas kurczenia dużych miniatur, aby dopasować do wyświetlacza."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Opóźnienie miniatury (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Stosuje opóźnienie czasowe pomiędzy wybraniem elementu playlisty a załadowaniem powiązanych miniatur. Ustawianie tej wartości na poziomie co najmniej 256 ms umożliwia szybkie przewijanie bez opóźnień nawet na najwolniejszych urządzeniach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Rozszerzone wsparcie ASCII"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Włącz wyświetlanie niestandardowych znaków ASCII. Wymagane dla kompatybilności z niektórymi nieangielskimi językami zachodnimi. Ma umiarkowany wpływ na wydajność."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Najbliższy sąsiad (szybki)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "Dwuliniowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (wolny)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Brak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "Automatyczny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (wyśrodkowany)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (wyśrodkowany)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "WYŁĄCZONE"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Dopasuj ekran"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Skala całkowita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Wypełnij ekran (rozciągnięty)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Niestandardowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Klasyczny czerwony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Klasyczny pomarańczowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Klasyczny żółty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Klasyczny zielony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Klasyczny niebieski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Klasyczny fiolet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Klasyczny szary"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Starszy czerwony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Ciemny fiolet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Niebieska północ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Złoty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektryczny błękit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Zielone jabłko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Czerwień wulkaniczna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Gruvbox jasny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Zhakuj rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
   "One ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized jasny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango jasny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "WYŁĄCZ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Śnieg (lekki)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Śnieg (ciężki)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Deszcz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "Wir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Pole gwiazdy"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Miniatury po lewej stornie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Typ miniatury do wyświetlenia po lewej stronie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Dynamiczne tło"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Dynamicznie ładuj nową tapetę w zależności od kontekstu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Animacja pozioma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Włącz animację poziomą menu. Zwiększy wydajność."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Podświetlanie ikony poziomej animacji"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Animacja, która wyzwala się podczas przewijania między kartami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animacja Przesuń w górę/w dół"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animacja, która wyzwala się podczas poruszania się w górę lub w dół."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Menu główne animacji Otwiera/Zamyka"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animacja, która wyzwala podczas otwierania podmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Menu czynnika alfa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Czcionka menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Wybierz inną główną czcionkę, która ma być używana w menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Kolor czcionki czerwony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Kolor czcionki zielony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Kolor czcionki niebieski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Układ menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Wybierz inny układ interfejsu XMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Motyw ikon menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Wybierz inny motyw dla ikony. Zmiany zaczną obowiązywać po ponownym uruchomieniu programu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Włącz cienie ikony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Animowany efekt tłą"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Wybierz animowany efekt tła. Może być intensywnie wykorzystujący GPU w zależności od efektu. Jeśli wydajność jest niezadowalająca, wyłącz to lub powróć do prostszego efektu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Kolor menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Wybierz inny motyw gradientu tła."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Miniatury dyspozycji pionowej"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Wyświetl miniaturę po prawej stronie ekranu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Współczynnik skali miniatur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Zmniejsz rozmiar miniaturki poprzez skalowanie maksymalnej dozwolonej szerokości."
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Włącz kartę Ustawienia"
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Wstążka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Prosty śnieg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Śnieg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Płatek śniegu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Niestandardowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Monochromia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Odwrócona monochromia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Systematyczny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
   "Piksel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
   "RetroSystem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
   "Pixel art"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Automatyczne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Automatyczny odwrócony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Zielone jabłko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Jasny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Poranny błękit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Ciemny fiolet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektryczny błękit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Złoty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Czerwone dziedzictwo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Niebieska północ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Zwykły"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Podmorski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Czerwień wulkaniczna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Limonkowy zielony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Pikachu żółty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Sześcian fioletowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Rodzinny czerwony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Płonący gorący"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Lodowy zimny"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Zwiń pasek boczny"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Niech lewy pasek boczny zawsze zwinięty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Przytnij nazwy playlist"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Kolor menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Wybierz inny motyw koloru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "podstawowy odcień bieli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "podstawowy odcień czerni"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Druga miniatura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Użyj paska tekstu dla metadanych zawartości"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Gdy włączony, każdy element metadanych zawartości wyświetlany na prawym pasku bocznym playlisty (powiązany rdzeń, czas odtwarzania) będzie zajmował pojedynczą linię; ciągi przekraczające szerokość paska bocznego będą wyświetlane jako przewijany tekst. Po wyłączeniu, każdy element metadanych zawartości będzie wyświetlany statycznie, zawinięty i zajmujący tyle wierszy, ile jest wymagane."
   )

/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Ikony menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Włącz/wyłącz ikony menu pokazane po lewej stronie wpisów menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Optymalizuj układ poziomy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Automatycznie dopasuj układ menu, aby lepiej pasował do ekranu podczas używania orientacji ekranu poziomego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Pokaż pasek nawigacji"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Wyświetlaj stałe skróty nawigacji na ekranie. Umożliwia szybkie przełączanie pomiędzy kategoriami menu. Zalecane dla urządzeń ekranu dotykowego."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Automatycznie przesuń pasek nawigacyjny na prawą stronę ekranu, gdy używasz orientacji wyświetlania poziomego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Kolor menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Wybierz inny motyw gradientu tła."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Animacja przejścia menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Włącz płynne efekty animacji podczas nawigacji pomiędzy różnymi poziomami menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Widok miniatury w trybie pionowym"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Określ widok miniatur playlisty podczas używania orientacji wyświetlania w trybie pionowym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Widok Miniatury poziomej"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Określ widok miniatur playlisty podczas używania orientacji wyświetlania poziomego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Pokaż drugorzędną miniaturę na liście widoków"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Rysuj tła miniatur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Umożliwia wypełnianie nieużywanego miejsca w obrazach miniatur jednolitym tłem. Zapewnia to jednolity rozmiar wyświetlania wszystkich obrazów, poprawiając wygląd menu podczas oglądania mieszanych miniatur zawartości o różnych podstawowych wymiarach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Pierwsza miniatura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Główny typ miniatury do powiązania z każdym wpisem playlisty. Zazwyczaj służy jako ikona zawartości."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Drugorzędna miniatura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Pomocniczy typ miniaturek powiązany z każdym wpisem playlisty. Wykorzystanie zależy od bieżącego trybu widoku miniatur playlisty."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Niebieski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Niebiesko szary"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Ciemny niebieski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Zielony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "NVIDIA Shield"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Czerwony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Żółty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Material UI ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozone ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Cutie niebieski"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Cutie cyjan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Cutie zielony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Cutie pomarańczowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Cutie różowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Cutie fioletowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Cutie czerwony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "Automatyczny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Zanikanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Slajd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "WYŁĄCZ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "WYŁĄCZ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Lista (mała)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Lista (średnia)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Podwójna ikona"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "WYŁ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Lista (mała)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Lista (średnia)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Lista (duża)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Pulpit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "WYŁĄCZONE"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "WŁĄCZONE"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Wyklucz widoki miniatur"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Informacje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Plik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Załaduj rdzeń..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Wyładuj rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "&Wyjdź"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Edytuj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Szukaj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Widok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Zamknięte Docki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Parametry cieniowania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Ustawienia..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Zapamiętaj pozycje doku:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Zapamiętaj geometrię okna:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Zapamiętaj ostatnią kartę przeglądarki treści:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Motyw:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Domyślne ustawie systemowe>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Ciemny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Niestandardowy..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Ustawienia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Narzędzia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Pomoc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "O RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Dokumentacja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Wczytaj niestandardowy rdzeń..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Załaduj rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Ładowanie rdzenia..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Nazwa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Wersja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Playlista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Przeglądarka plików"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Do góry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Przeglądarka treści"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Okładka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Zrzut ekranu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Ekran tytułu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Wszystkie playlisty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Informacje o rdzeniu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Zapytaj>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informacje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Ostrzeżenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Błąd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Błąd sieci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Uruchom program ponownie, aby zmiany odniosły skutek."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 elementów"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Upuść obraz tutaj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Nie pokazuj tego ponownie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Zatrzymaj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Powiązany rdzeń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Ukryte playlisty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Ukryj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Kolor podświetlenia:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Wybierz..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Wybierz kolor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Wybierz motyw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Motyw niestandardowy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Ścieżka do pliku jest pusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Plik jest pusty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Nie można otworzyć pliku do odczytu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Nie można otworzyć pliku do zapisu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Plik nie istnieje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Zaproponuj najpierw załadowany rdzeń:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Powiększ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Widok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Ikony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Lista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Oczyść"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Postęp:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "Maksymalna liczba wpisów na liście \"Wszystkie playlisty\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "Maksymalna liczba wpisów na siatce \"Wszystkie playlisty\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Pokaż ukryte pliki i foldery:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Nowa playlista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Wprowadź nową nazwę playlisty:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Usuń playlistę"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Zmień nazwę playlisty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Czy na pewno chcesz usunąć playlistę \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Pytanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Nie można usunąć pliku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Nie można zmienić nazwy pliku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Zbieranie listy plików..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Dodawanie plików do playlisty..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Wpis playlisty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Nazwa:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Ścieżka:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Rdzeń:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Baza danych:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Rozszerzenia:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(rozdzielone spacjami; domyślnie obejmuje wszystkie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Filtruj wewnątrz archiwów"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(używany do wyszukiwania miniatur)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Czy na pewno chcesz usunąć element \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Proszę najpierw wybrać pojedynczą playlistę."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Usuń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Dodaj wpis..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Dodaj plik(i)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Dodaj folder..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Edytuj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Wybierz pliki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Wybierz folder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<wiele>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Błąd podczas aktualizacji wpisu playlisty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Proszę wypełnić wszystkie wymagane pola."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "Aktualizuj RetroArch (nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch został zaktualizowany. Uruchom ponownie aplikację, aby zmiany stały się skuteczne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Aktualizacja nie powiodła się."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Współtwórcy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Bieżący shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Przesuń w dół"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Przesuń w górę"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Załaduj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Zapisz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Usuń"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Usuń hasła"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Zastosuj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Dodaj przejście"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Wyczyść wszystkie przejścia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Brak przejść shader-a."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Resetuj przejście"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Zresetuj wszystkie przejścia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Resetuj parametr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Pobierz miniatury"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Pobieranie jest już w toku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Rozpocznij na liście odtwarzania:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Miniatura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Limit pamięci podręcznej miniatur:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Limit rozmiaru miniatury dla akcji przeciągnij i upuść:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Pobierz wszystkie miniatury"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Cały system"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Ta playlista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Pobieranie miniatur zakończone sukcesem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Sukces: %1 Niepowodzenie: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Opcje rdzenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Zresetuj wszystko"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Aktualizacja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Konta Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Lista punktów klienta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Turbo/Martwa strefa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
   "Retro osiągnięcia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Liczniki rdzeniowe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Nie wybrano dysku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Liczniki frontendu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Menu poziome"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Ukryj niezwiązane podstawowe deskryptory wejściowe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Wyświetl etykiety deskryptorów wejściowych"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Nakładka na ekranie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Załaduj ostatnie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Wybierz zawartość z listy historii ostatnio uruchamianych."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Podsystemy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Nie znaleziono hostów gry online."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Brak liczników wydajności."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Brak playlist"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "Dostępny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Opcje cheat-u"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Rozpocznij lub kontynuuj wyszukiwanie cheat-u"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Uruchom muzykę"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "sekundy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Rozpocznij rdzeń"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Zacznij rdzeń bez zawartości."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Sugerowane rdzenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Nie można odczytać skompresowanego pliku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Użytkownik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Maksymalne obrazy swapchain"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Informuje sterownik wideo, aby jawnie użył określonego trybu buforowania."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Modyfikuje domyślne ustawienie modułu cieniującego w menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Ustawienia Shader"
   )
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
   "Rozpocznij"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Przezwisko: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Znaleziono zgodną zawartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Startowy gong"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "Auto. Współczynnik proporcji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Włącz System BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
   "Niestandardowy współczynnik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Włącz nagrywanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "Zapisz wyjścia jako..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "Zapisz nagrania w katalogu wyjściowym"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "Zobacz mecz #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "Wybierz dopasowanie do wyświetlenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "Wymuś współczynnik proporcji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "Wybierz z playlisty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME,
   "Wznów"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME,
   "Wznów aktualnie uruchomioną zawartość i opuść szybkie menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "Zobacz listę %u dopasowań"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "Utwórz kod z tego dopasowania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "Usuń to dopasowanie"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "Nieprzezroczystość stopki"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "Zmodyfikuj krycie grafiki stopki."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "Nieprzezroczystość nagłówka"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "Zmodyfikuj krycie grafiki nagłówka."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Włącz grę online"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "Rozpocznij zawartość"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Ścieżka historii treści"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Identyfikator wyświetlania wyjścia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Wybierz port wyjściowy podłączony do wyświetlacza CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Pomoc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Rozwiązywanie problemów audio/wideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "Ładowanie zawartości"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "Skanowanie w poszukiwaniu treści"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "Co to jest rdzeń?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "Wyślij informacje o debugowaniu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
   "Wysyła informacje diagnostyczne o twoim urządzeniu i konfiguracji RetroArch na nasze serwery w celu analizy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Ustawienia bazy danych"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Opóźnij klatki gry online"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Zeskanuj sieć lokalną"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Wyszukaj i połącz się z hostami gry online w sieci lokalnej."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Włącz klienta gry online"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Widz gry online włączone"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Opis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "Ogranicz maksymalną prędkość działania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "Rozpocznij wyszukiwanie nowego cheat-kodu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "Rozpocznij wyszukiwanie nowego cheat-a. Liczba bitów może zostać zmieniona."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Kontynuuj wyszukiwanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Kontynuuj wyszukiwanie nowego chet-a."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Lista osiągnięć (Hardcore)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Szczegóły cheat-u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Zarządza ustawieniami szczegółów cheat-a."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Rozpocznij lub kontynuuj wyszukiwanie kodu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Rozpocznij lub kontynuuj wyszukiwanie kodu chet-a."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Oszukane przepustki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Zwiększ lub zmniejsz ilość kodów."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Lewy analog X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Lewy analog Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Prawo analog X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Prawo analog Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Rozpocznij lub kontynuuj wyszukiwanie cheat-u"
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
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Baza danych - Filtr: Pochodzenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Baza danych - Filtr: Franczyza"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Baza danych - Filtr: Ocena ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Baza danych - Filtr: Ocena ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Baza danych - Filtr: Ocena PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Baza danych - Filtr: Ocena CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Baza danych - Filtr: Ocena BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Baza danych - Filtr: Maks. Liczba użytkowników"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Baza danych - Filtr: Data wydania na miesiąc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Baza danych - Filtr: Data wydania na rok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Baza danych - Filtr: Problem z magazynem Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Baza danych - Filtr: Ocena magazynu Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Informacje o bazie danych"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Konfiguracja"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Pliki do pobrania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Ustawienia gry online"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Obsługa Slangu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "Obsługuje renderowanie do tekstury (shadery wieloprzebiegowe) OpenGL/Direct3D"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Zawartość"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "Zwykle ustawiane przez programistów, którzy pakują aplikacje libretro/RetroArch, aby wskazywały na zasoby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Zapytać"
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

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "W Menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "W Grze"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "W Grze (Wstrzymano)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Gra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Wstrzymano"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Gra online rozpocznie się po załadowaniu zawartości."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Nie można znaleźć odpowiedniego pliku rdzenia lub treści, załaduj ręcznie."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Sterownik grafiki nie jest kompatybilny z bieżącym sterownikiem wideo w RetroArch, powrót do sterownika %s. Uruchom ponownie RetroArch, aby zmiany odniosły skutek."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Instalacja rdzenia zakończona sukcesem"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Instalacja rdzenia nie powiodła się"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Naciśnij pięć razy w prawo, aby usunąć wszystkie cheat-y."
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "Nie udało się zapisać informacji debugowania."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_DEBUG_INFO,
   "Nie udało się wysłać informacji o debugowaniu na serwer."
   )
MSG_HASH(
   MSG_SENDING_DEBUG_INFO,
   "Wysyłanie informacji o debugowaniu..."
   )
MSG_HASH(
   MSG_SENT_DEBUG_INFO,
   "Wysłano informacje o debugowaniu na serwer. Twój numer identyfikacyjny to %u."
   )
MSG_HASH(
   MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
   "Naciśnij jeszcze dwa razy, aby przesłać informacje diagnostyczne do zespołu RetroArch."
   )
MSG_HASH(
   MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
   "Naciśnij jeszcze raz, aby przesłać informacje diagnostyczne do zespołu RetroArch."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Globalna objętość miksera audio"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Zakończono skanowanie gry online."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Przepraszamy, niezaimplementowane: rdzenie, które nie wymagają treści, nie mogą uczestniczyć w grze sieciowej."
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
   "Połączenie z: \"%s \" \""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Połączenie z: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Mapowanie portu zakończone sukcesem"
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Mapowanie portu nie powiodło się"
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Ustawianie dysku w zasobniku"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Oczekiwanie na klienta..."
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
   MSG_DISCORD_CONNECTION_REQUEST,
   "Czy chcesz zezwolić na połączenie od użytkownika:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Niepoprawne hasło"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" rozłączono"
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
   "Uczestnik sieciowy \"%s\" wstrzymano"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Twój pseudonim został zmieniony na \"%s\""
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
   MSG_FETCHING_CORE_LIST,
   "Pobieranie listy rdzeni..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Nie udało się pobrać listy rdzeni!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "Najnowsza wersja już zainstalowana: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Aktualizacja rdzenia: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Pobieranie rdzenia: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Wyodrębnianie rdzenia: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Rdzeń zainstalowany: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Skanowanie rdzeni..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Sprawdzanie rdzeni: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Wszystkie zainstalowane rdzenie są w najnowszej wersji"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "rdzenie zaktualizowane: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Resetowanie rdzeni: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Rdzenie zresetowane: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Czyszczenie playlisty: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Playlista wyczyszczona: "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Dodano do ulubionych"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Nie udało się dodać ulubionych: playlista pełna"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Zestaw rdzenia: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "Kojarzenie wejścia do playlisty zostało zresetowane."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Dołączony dysk"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Nie udało się dodać dysku"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Aplikacja Dir"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Stosowanie zmian w kodzie."
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Zastosuj Shader"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Wyciszenie Dźwięku."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Dźwięk nie jest wyciszony."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Błąd podczas zapisywania pliku autoconfig."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Plik Autoconfig został pomyślnie zapisany."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Nie można zainicjować autozapisu."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Automatycznie zapisz stan do"
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Blokowanie nadpisywania SRAM"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Wprowadzanie interfejsu poleceń na porcie"
   )
MSG_HASH(
   MSG_BYTES,
   "bajty"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Nie można określić nowej ścieżki konfiguracji. Użyj bieżącego czasu."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Tryb Hardcore włączony, stan zapisu i przewijanie do tyłu były wyłączone."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "Porównując ze znanymi magicznymi liczbami..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Skompilowany z API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Nie skonfigurowano katalogu konfiguracyjnego. Nie można zapisać nowej konfiguracji."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Połączony z"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "Zawartość CRC32 różni się. Nie można używać różnych gier."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Pominięto ładowanie treści. Implementacja załaduje ją samodzielnie."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Rdzeń nie obsługuje stanów zapisywania."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Plik opcji rdzenia został pomyślnie utworzony."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Nie można znaleźć następnego sterownika"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Nie można znaleźć zgodnego systemu."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Nie można znaleźć prawidłowej ścieżki danych"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "Nie można otworzyć ścieżki danych"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "Nie można odczytać pliku zawartości"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Nie można odczytać nagłówka filmu."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Nie można odczytać stanu z filmu."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "Niezgodność sumy kontrolnej CRC32 między plikiem treści a zapisaną sumą kontrolną w nagłówku pliku odtwarzania. Powtórka najprawdopodobniej zsynchronizuje się podczas odtwarzania."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Podano niestandardowy czas"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Dekompresja już trwa."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Dekompresja nie powiodła się."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Wykryty obszar widoku"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Nie znaleziono poprawki treści."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Odłącz urządzenie od poprawnego portu."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Zamknięte"
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Wyrzucony"
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Ściąganie"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "Index"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Pobieranie nie udane"
   )
MSG_HASH(
   MSG_ERROR,
   "Błąd"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Rdzeń Libretro wymaga treści, ale nic nie zostało dostarczone."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Rdzeń Libretro wymaga specjalnych treści, ale żadne nie zostały dostarczone."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Core nie obsługuje VFS i ładowanie z lokalnej kopii nie powiodło się"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Błąd podczas analizowania argumentów."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Błąd podczas zapisywania pliku opcji podstawowych."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Błąd podczas zapisywania pliku remap."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Błąd podczas usuwania pliku przemapowania."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Błąd podczas zapisywania ustawienia modułu cieniującego."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Aplikacja zewnętrzna Dir"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Wyodrębnianie"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Wyodrębnianie pliku"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Nie powiodło się zapisanie konfiguracji do"
   )
MSG_HASH(
   MSG_FAILED_TO,
   "Nie udało się"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Nie udało się przyjąć przychodzącego widza."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Nie można przydzielić pamięci na poprawioną zawartość..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Nie można zastosować modułu cieniującego."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Nie udało się zastosować ustawienia shadera:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Nie powiodło się wiązanie gniazda."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Nie można utworzyć katalogu."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Nie można wyodrębnić treści ze skompresowanego pliku"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Nie udało się uzyskać pseudonimu od klienta."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Nie udało się załadować"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Nie udało się wczytać treści"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Nie udało się załadować pliku filmowego"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Nie udało się załadować nakładki."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Nie udało się załadować stanu z"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Nie udało się otworzyć rdzenia libretro"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Nie udało się załatać"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Nie można odebrać nagłówka od klienta."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Nie udało się odebrać pseudonimu."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Nie udało się odebrać pseudonimu z hosta."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Nie można odebrać rozmiaru pseudonimu z hosta."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "Nie można odebrać danych SRAM z hosta."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Nie udało się usunąć dysku z zasobnika."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Nie udało się usunąć pliku tymczasowego"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "Nie udało się zapisać SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Nie udało się zapisać stanu do"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Nie udało się wysłać pseudonimu."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Nie udało się wysłać rozmiaru pseudonimu."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Nie udało się wysłać pseudonimu do klienta."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Nie udało się wysłać pseudonimu do hosta."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "Nie udało się wysłać danych SRAM do klienta."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Nie można uruchomić sterownika audio. Będzie kontynuowany bez dźwięku."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Nie można uruchomić nagrywania filmu."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Nie można rozpocząć nagrywania."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Nie udało się zrobić zrzutu ekranu."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Nie udało się cofnąć stanu obciążenia."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Nie udało się cofnąć stanu zapisania."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Nie udało się wyłączyć dźwięku."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Błąd krytyczny odebrany w"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Nie znaleziono pliku"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Znajdź stan automatycznego zapisywania w"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Znaleziono etykietę dysku"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Znaleziono pierwszą ścieżkę danych w pliku"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Znaleziono ostatni stan automatu"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Znaleziony moduł cieniujący"
   )
MSG_HASH(
   MSG_FRAMES,
   "Klatki"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Opcje gry: podstawowe opcje zależne od danej gry"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Otrzymałem nieprawidłowy indeks dysku."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Sprawdź stan myszy"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Skup się na grze"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Nie skupiaj się na grze"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Rdzeń Libretro jest renderowany sprzętowo. Musi także korzystać z nagrywania po cieniowaniu."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "Skomplikowana suma kontrolna nie pasuje do CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Wejdź w kod"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Wprowadź nazwę kodu"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Wprowadź wstępnie ustawioną nazwę pliku"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Zmień nazwę tytułu"
   )
MSG_HASH(
   MSG_INTERFACE,
   "Interfejs"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Pamięć wewnętrzna"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Magazyn wymienny"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Nieprawidłowy rozmiar pseudonimu."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "w bajtach"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "w megabajtach"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "w gigabajtach"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "jest skompilowany przeciwko innej wersji libretro niż ta implementacja libretro."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Frontend dla libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Załadowany stan z gniazda #%d."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Załadowany stan z gniazda #-1 (automatyczny)."
   )
MSG_HASH(
   MSG_LOADING,
   "Ładuję"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Brak jednego lub więcej plików oprogramowania układowego"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Ładowanie pliku zawartości"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Ładowanie pliku historii"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Ładowanie pliku ulubionych"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Ładowanie stanu"
   )
MSG_HASH(
   MSG_MEMORY,
   "Pamięć"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
   "Plik filmu nie jest prawidłowym plikiem BSV1."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Wygląda na to, że format filmu ma inną wersję serializera. Najprawdopodobniej zawiedzie."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Zakończono odtwarzanie filmu."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Zatrzymywanie nagrywania filmu."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Nie udało się zainicjować gry sieciowej."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Bez zawartości, zaczynając sztuczny rdzeń."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Żaden stan zapisu nie został jeszcze nadpisany."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Żaden stan nie został jeszcze załadowany."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Błąd podczas zapisywania zastąpień."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Przesłonięcia zostały pomyślnie zapisane."
   )
MSG_HASH(
   MSG_PAUSED,
   "Wstrzymane."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Czytanie pierwszej ścieżki danych..."
   )
MSG_HASH(
   MSG_RECEIVED,
   "Odebrane"
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Nagrywanie zakończyło się z powodu zmiany rozmiaru."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Nagrywanie do"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Przekierowuję plik oszustów do"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Przekierowanie pliku zapisu do"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Przekierowuję stan zapisywania do"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Plik przemapowania został pomyślnie zapisany."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Plik przemapowania został pomyślnie usunięty."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Usunięto dysk z zasobnika."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Usuwanie tymczasowego pliku zawartości"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Ponowne uruchamianie nagrywania z powodu ponownego uruchomienia sterownika."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Przywrócono stary stan zapisywania."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Shadery: przywracanie domyślnego ustawienia modułu cieniującego do"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Przywracanie zapisywania katalogu plików do"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Przywracanie katalogu stanu zapisu do"
   )
MSG_HASH(
   MSG_REWINDING,
   "Przewijanie."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Inicjowanie bufora przewijania z rozmiarem"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Nie można zainicjować buforu przewijania. Przewijanie zostanie wyłączone."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "Implementacja wykorzystuje nagrany dźwięk. Nie można użyć do przewijania."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Osiągnięty koniec bufora przewijania."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Zapisano nową konfigurację do"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Zapisany stan do gniazda #%d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Zapisany stan do gniazda #-1 (auto)."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Zapisano pomyślnie do"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Zapisywanie typu pamięci RAM"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Zapisywanie stanu"
   )
MSG_HASH(
   MSG_SCANNING,
   "Skanowanie"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Skanowanie katalogu zakończone"
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Wysyłam polecenie"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Kilka łatek jest jawnie zdefiniowanych, ignorując wszystko..."
   )
MSG_HASH(
   MSG_SHADER,
   "Cieniowanie"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Preset Shadera został pomyślnie zapisany."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Pomijanie obciążenia SRAM."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Zwolnione tempo."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Szybko na przód."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Zwolnij tempo."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM nie zostanie zapisany."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Rozpoczynanie odtwarzania filmu."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Rozpoczęcie nagrywania filmu na"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Wielkość państwa"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Stan gniazda"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Zrzut ekranu."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Zrzut ekranu zapisany"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Osiągnięcie odblokowane"
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Zmień typ miniatury"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Miniatury pełnoekranowe"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Przełącz metadane"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Brak dostępnych miniatur"
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Naciśnij ponownie, aby wyjść..."
   )
MSG_HASH(
   MSG_TO,
   "do"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Anulować stan zapisu."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Cofanie stanu zapisu"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Nieznany"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Anulowano."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Polecenie nierozpoznane"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Używanie nazwy rdzenia dla nowej konfiguracji."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Używanie rdzenia manekinowego libretro. Pomijanie nagrania."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Podłącz urządzenie z prawidłowego portu."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Odłączanie urządzenia od portu"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Ponowne uruchamianie..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Wyłączanie..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Wersja API libretro"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Obliczenie wielkości wyświetlania nie powiodło się! Będzie nadal korzystać z nieprzetworzonych danych. Prawdopodobnie to nie zadziała ..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY,
   "Wirtualna taca dysku."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "wysuń"
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "zamknij"
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
   MSG_WIFI_SCAN_COMPLETE,
   "Skanowanie Wi-Fi zostało zakończone."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Skanowanie sieci bezprzewodowych..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Skanowanie w poszukiwaniu hostów gry online..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Przygotowanie do skanowania zawartości..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Wprowadź hasło"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Hasło poprawne."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Hasło niepoprawne."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Wprowadź hasło"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Hasło poprawne."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Hasło niepoprawne."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "Przeładowanie konfiguracji zostało załadowane."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Załadowano plik przemapowania gry."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Załadowano plik z przemapowaniem katalogu zawartości."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Załadowano plik przemapowania rdzenia."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "RunAhead został wyłączony, ponieważ ten rdzeń nie obsługuje zapisywania stanów."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Nie udało się zapisać stanu. RunAhead został wyłączony."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Nie udało się załadować stanu. RunAhead został wyłączony."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Nie udało się utworzyć drugiej instancji. RunAhead wykorzystuje teraz tylko jedną instancję."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Skanowanie pliku zakończone"
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Pomyślnie rozpoczęto wyszukiwanie cheat-ów"
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Nie udało się uruchomić wyszukiwania cheat-ów"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Wyszukiwanie nie zostało zainicjowane/rozpoczęte"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Nowa liczba dopasowań = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "Dodano %u dopasowań"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Nie udało się dodać dopasowania"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Utworzono kod z dopasowania"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Nie udało się utworzyć kodu"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Usunięte dopasowanie"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Nie wystarczający pokój. Całkowita liczba cheat-ów, które możesz mieć, to 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Nowy cheat dodany na górze listy."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Nowy cheat dodany na dole listy."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Wszystkie cheat-y usunięte."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Nowy cheat dodany przed tym cheat-em."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Nowy cheat dodany po tym cheat-cie."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Cheat skopiowany przed tym cheat-em."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Cheat skopiowany po tym cheat-cie."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Cheat usunięty."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "Skonfigurowane w porcie:"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Nie udało się ustawić dysku"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Nie udało się ustawić ostatnio używanego dysku..."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Wczytano stan zapisu. Tryb osiągnięć Hardcore jest wyłączony dla bieżącej sesji."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Cheat został aktywowany. Tryb osiągnięć hardcore jest wyłączony dla bieżącej sesji."
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "Najniższa"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Niższa"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "Normalna"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Wyższa"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Najwyższa"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Ostrzeżenie: Brakujące zasoby, użyj aktualizacji online, jeśli dostępne"
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Zrzut dysku..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Napęd %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Proszę najpierw załadować rdzeń."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Nie udało się odczytać z dysku. Zrzut przerwany."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Nie udało się zapisać na dysku. Zrzut przerwany."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Żaden dysk nie jest umieszczony w napędzie."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Pomyślnie usunięto ustawienie shader-a."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Błąd podczas usuwania ustawienia shader-a."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Wybrano nieprawidłowy plik arcade DAT"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "Wybrany plik arcade DAT jest zbyt duży (niewystarczająca wolna pamięć)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Nie udało się załadować pliku arcade DAT (nieprawidłowy format?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Nieprawidłowa konfiguracja skanowania ręcznego"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Nie wykryto poprawnej zawartości"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Skanowanie zawartości: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Skanowanie: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Czyszczenia wpisów M3U: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Skanowanie zakończone: "
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Zaktualizuj Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Nazwa frontendu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Wersja Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Restart"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Podziel Joy-Con"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
   "Status wewnętrznej pamięci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Nadpisanie skali widżetów graficznych"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Rozdzielczość ekranu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Zamknąć"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Włącz dostęp do plików zewnętrznych"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Otwórz w Windows ustawienia uprawnień dostępu do plików"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Otwórz..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Otwórz inny katalog używając systemowego selektora plików"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Filtr migotania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Gamma wideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Włącz filtr miękki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Skanuje sieci bezprzewodowe i nawiązuje połączenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Migotanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "Ustaw VI szerokość ekranu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Korekta przeskalowania (góra)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Dostosuj przeskanowanie ekranu poprzez zmniejszenie rozmiaru obrazu o określoną liczbę skanowanych linii (pobranych od góry ekranu). Uwaga: Może wprowadzić artefakty skalowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Korekta przeskalowania (dół)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Dostosuj przeskanowanie ekranu poprzez zmniejszenie rozmiaru obrazu o określoną liczbę skanowanych linii (pobranych od dołu ekranu). Uwaga: Może wprowadzić artefakty skalowania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Trwały tryb wydajności"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Użyj trybu PAL60"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Uruchom RetroArch ponownie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Zablokuj klatki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Preferuj sterowanie dotykiem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Włącz dotyk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Włącz małą klawiaturę"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Limit czasu bloku wejściowego"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Liczba milisekund oczekiwania na pobranie pełnej próbki wejściowej, użyj go, jeśli masz problemy z jednoczesnymi naciśnięciami przycisku (tylko Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Pokaż restart"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Pokaż/ukryj opcję 'Restart'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Pokaż wyłączenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Pokaż/ukryj opcję 'Wyłącz'."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
   "Podkręcanie GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE,
   "Podkręcanie lub odkręcenie GPU Switch-a"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_BACKLIGHT_CONTROL,
   "Jasność ekranu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_BACKLIGHT_CONTROL,
   "Zwiększanie lub zmniejszanie jasności ekranu Switch-a"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT_RCM,
   "Uruchom ponownie do RCM"
   )
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "Podkręcanie CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Podkręcanie CPU Switch-a"
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
   "Włącz Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Włącz lub wyłącz bluetooth."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Usługi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Zarządzaj usługami na poziomie systemu operacyjnego."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "Włącz SAMBA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Włącz lub wyłącz udostępnianie sieciowe folderów."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "Włącz SSH"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Włącz lub wyłącz zdalny dostęp do wiersza poleceń."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Punkt dostępu Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Włącz lub wyłącz punkt dostępu Wi-Fi."
   )
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Wyłączanie punktu dostępu Wi-Fi."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Odłączanie od Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Punkt dostępu Wi-Fi jest już uruchomiony"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Punkt dostępu Wi-Fi nie jest uruchomiony"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Uruchamianie punktu dostępu Wi-Fi z SSID =% si kluczem dostępu=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Nie można utworzyć pliku konfiguracyjnego punktu dostępu Wi-Fi."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Niepoprawny plik konfiguracyjny - nie można znaleźć APNAME lub PASSWORD w %s"
   )
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Skala myszy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Dostosuj skalę x/y dla prędkości broni świetlnej Wiimote."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "Skalowanie RGA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "Skalowanie RGA i filtrowanie bicubic. Może wpływać na widżety."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Specyficzne skalowanie kontekstu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Skalowanie kontekstu sprzętowego (jeśli dostępne)."
   )
#endif
#if defined(_3DS)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "Dolny ekran 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Włącz wyświetlanie informacji o stanie na dolnym ekranie. Wyłącz, aby wydłużyć czas pracy baterii i zwiększyć wydajność."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "Tryb wyświetlania 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Wybiera pomiędzy trybami wyświetlania 3D i 2D. W trybie '3D' piksele są kwadratowe a podczas przeglądania szybkiego menu zastosowany jest efekt głębi. Tryb „2D” zapewnia najlepszą wydajność."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (efekt siatki pikseli)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (wysoka rozdzielczość)"
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Skanowanie zakończone.<br><br>\nW celu poprawnego skanowania treści, musisz:\n<ul><li>mieć już pobrany kompatybilny rdzeń</li>\n<li>zaktualizować \"Pliki informacji rdzenia\" za pomocą aktualizacji online</li>\n<li>mieć zaktualizowane \"Bazy Danych\" za pomocą aktualizacji online</li>\n<li>zrestartować RetroArch, jeśli którakolwiek z powyższych operacji właśnie została wykonana</li></ul>\nNa koniec, zawartość musi pasować do istniejących baz danych z <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">tutaj</a>. Jeśli nadal nie działa, rozważ <a href=\"https://www.github.com/libretro/RetroArch/issues\">wysłanie zgłoszenia błędu</a>."
   )
#endif

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

MSG_HASH(
      MSG_COMPILER,
      "Компилятор"
      )
MSG_HASH(
      MSG_UNKNOWN_COMPILER,
      "Неизвестный компилятор"
      )
MSG_HASH(
      MSG_NATIVE,
      "Native"
      )
MSG_HASH(
      MSG_DEVICE_DISCONNECTED_FROM_PORT,
      "Устройство отключено от порта"
      )
MSG_HASH(
      MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
      "Неизвестная команда netplay"
      )
MSG_HASH(
      MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
      "Файл уже существует. Сохранение в резервном буфере"
      )
MSG_HASH(
      MSG_GOT_CONNECTION_FROM,
      "Установлено соединение с: \"%s\""
      )
MSG_HASH(
      MSG_GOT_CONNECTION_FROM_NAME,
      "Установлено соединение с: \"%s (%s)\""
      )
MSG_HASH(
      MSG_PUBLIC_ADDRESS,
      "Публичный адрес"
      )
MSG_HASH(
      MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
      "Нет аргументов и встроенного меню, отображается справка..."
      )
MSG_HASH(
      MSG_SETTING_DISK_IN_TRAY,
      "Установка диска в привод"
      )
MSG_HASH(
      MSG_WAITING_FOR_CLIENT,
      "Ожидание клиента..."
      )
MSG_HASH(
      MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
      "Вы покинули игру"
      )
MSG_HASH(
      MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
      "Вы присоединились под именем %u"
      )
MSG_HASH(
      MSG_NETPLAY_ENDIAN_DEPENDENT,
      "Выбранное ядро не поддерживает архитектуру netplay между этими системами"
      )
MSG_HASH(
      MSG_NETPLAY_PLATFORM_DEPENDENT,
      "Выбранное ядро не поддерживает архитектуру netplay"
      )
MSG_HASH(
      MSG_NETPLAY_ENTER_PASSWORD,
      "Введите пароль от сервера netplay:"
      )
MSG_HASH(
      MSG_NETPLAY_INCORRECT_PASSWORD,
      "Неверный пароль"
      )
MSG_HASH(
      MSG_NETPLAY_SERVER_NAMED_HANGUP,
      "\"%s\" отключился"
      )
MSG_HASH(
      MSG_NETPLAY_SERVER_HANGUP,
      "Клиент netplay отключен"
      )
MSG_HASH(
      MSG_NETPLAY_CLIENT_HANGUP,
      "Netplay отключен"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
      "У вас недостаточно прав, чтобы играть"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
      "Недостаточно свободных слотов для игры"
      )
MSG_HASH(
      MSG_NETPLAY_CANNOT_PLAY,
      "Невозможно переключиться в режим игры"
      )
MSG_HASH(
      MSG_NETPLAY_PEER_PAUSED,
      "Пир netplay \"%s\" приостановил игру"
      )
MSG_HASH(
      MSG_NETPLAY_CHANGED_NICK,
      "Ваш ник изменился на \"%s\""
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
      "Предоставьте аппаратно-рендерированным ядрам собственный контекст. Избегайте принятия изменений состояния оборудования между кадрами."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
      "Enable horizontal animation for the menu. This will have a performance hit."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_SETTINGS,
      "Настройка параметров, относящихся к внешнему виду экрана меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
      "Синхронизируйте CPU и GPU. Это улучшит производительность."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_THREADED,
      "Повышает производительность за счёт понижения количества кадров в секунду. Использовать только тогда, когда другие способы не сработали."
      )
MSG_HASH(
      MSG_AUDIO_VOLUME,
      "Громкость звука"
      )
MSG_HASH(
      MSG_AUTODETECT,
      "Определять автоматически"
      )
MSG_HASH(
      MSG_AUTOLOADING_SAVESTATE_FROM,
      "Автозагрузка из"
      )
MSG_HASH(
      MSG_CAPABILITIES,
      "Возможности"
      )
MSG_HASH(
      MSG_CONNECTING_TO_NETPLAY_HOST,
      "Подключение к netplay"
      )
MSG_HASH(
      MSG_CONNECTING_TO_PORT,
      "Подключение к порту"
      )
MSG_HASH(
      MSG_CONNECTION_SLOT,
      "Слот подключения"
      )
MSG_HASH(
      MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
      "К сожалению, данная функция не работает: ядра, не запрашивающие контент, не могут участвовать в netplay."
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
      "Пароль"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
      "Аккаунт Cheevos"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
      "Имя пользователя"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
      "Аккаунты"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
      "Аккаунт лист Endpoint"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
      "Достижения"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
      "Список достижений"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
      "Список достижений (хардкор)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
      "Результат сканирования"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
      "Конфигурации"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ADD_TAB,
      "Импорт содержимого"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
      "Сетевая игра"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
      "Спрашивать"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
      "Ресурсы"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
      "Блокировка фреймов"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
      "Аудиоустройство"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
      "Аудиодрайвер"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
      "Аудиоплагин DSP"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
      "Включить звук"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
      "Аудиофильтр"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
      "Turbo/Deadzone"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
      "Задержка звука (ms)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
      "Максимальный тайминг синхронизации аудио-сигнала"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
      "Отключить звук"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
      "Частота аудиовыхода (КГц)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
      "Динамический контроль скорости звука"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
      "Аудиоресэмплер"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
      "Аудио"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
      "Синхронизация звука"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
      "Уровень громкости звука (дБ)"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
      "Эксклюзивный режим WASAPI"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
      "Формат WASAPI с плавающей точкой"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
      "Длина общего буфера WASAPI"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
      "Интервал автосохранений"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
      "Переопределить файлы загрузки"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
      "Загружать файлы переопределения автоматически"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
      "Автоматическая загрузка преднастройки шейдеров"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
      "Назад"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
      "Подтвердить"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
      "Инфо"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
      "Выйти"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
      "Прокрутить вниз"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
      "Прокрутить вверх"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
      "Старт"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
      "Переключить клавиатуру"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
      "Переключить меню"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
      "Базовые элементы управления меню"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
      "Подтвердить"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
      "Инфо"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
      "Выйти"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
      "Прокрутить вверх"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
      "По умолчанию"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
      "Переключить клавиатуру"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
      "Переключить меню"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
      "Не перезаписывать SaveRAM при загрузке сохранений"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
      "Включить Bluetooth"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
      "Создание ботов URL"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
      "Кэш"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
      "Разрешить использовать камеру"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
      "Драйвер камеры"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT,
      "Чит"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
      "Применить изменения чита"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
      "Чит-файл"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
      "Чит-файл"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
      "Загрузить чит-файл"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
      "Сохранить чит-файл как:"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
      "Кол-во доступных чит-кодов"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
      "Описание"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Достижения в хардкорном режиме"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ACHIEVEMENTS,
      "Заблокированные достижения:"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
      "Заблокировано"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
      "Достижения"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
      "Тестовые неофициальные достижения"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
      "Режим подробного протоколирования достижений"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ACHIEVEMENTS,
      "Разблокированные достижения:"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
      "Разблокирован"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
      "Закрыть"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIG,
      "Конфигурация"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
      "Загрузить конфигурацию"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
      "Конфигурация"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
      "Сохранить конфигурацию и выйти"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
      "База данных контента"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
      "Контент"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
      "Размер истории")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
      "Разрешить удалить контент с плейлиста")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
      "Быстрые настройки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
      "Загрузки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
      "Загрузки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
      "Читы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
      "Основной счётчик")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
      "Показать название ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
      "Информация о ядре")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
      "Авторы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
      "Категории")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
      "Символ ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
      "Имя ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
      "Микропрограммы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
      "Лицензии")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
      "Разрешения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
      "Поддерживаемые расширения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
      "Разработчик системы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
      "Эмулирует системы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
      "Элементы управления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_LIST,
      "Загрузить ядро")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
      "Опции")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
      "Ядро")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
      "Автоматически запускать ядро")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
      "Автоматически извлечь загруженный архив")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
      "URL билдбота ядер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
      "Обновить ядро")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
      "Обновление")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
      "Архитектура CPU:")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CPU_CORES,
      "Ядра CPU:")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
      "Курсор")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
      "Менеджер курсоров")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
      "Дополнительное значение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
      "Менеджер баз данных")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
      "Выбор баз данных")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
      "Удалить")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FAVORITES,
      "Избранное")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
      "<Каталог содержимого>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
      "<По умолчанию>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
      "<Нет>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
      "Каталог не найден.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
      "Директории")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS,
      "Статус диска в приводе")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
      "Добавить образ диска")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_INDEX,
      "Индекс диска")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
      "Управление диском")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DONT_CARE,
      "Не важно")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
      "Загрузки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
      "Загрузка ядра...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
      "Загрузка содержимого")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE,
      "Разрешить переопределение DPI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE,
      "Переопределение DPI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
      "Драйвер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
      "Загрузка заглушки при выключении ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
      "Проверять отсутствие прошивки перед загрузкой")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
      "Динамические обои")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
      "Каталог с динамическими обоями")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
      "Включить достижения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FALSE,
      "Выключено")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
      "Максимальная рабочая скорость")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_SHOW,
      "Вывести значение FPS на экран")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
      "Ограничить максимальную скорость работы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
      "Настройка частоты кадров")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
      "Внешние счетчики")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
      "Автоматически загружать основные параметры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
      "Создать файл параметров игры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
      "Файл параметров игры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP,
      "Помощь")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
      "Устранение проблем с аудио/видео")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
      "Изменение обложки виртуального геймпада")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
      "Основные элементы управления меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_LIST,
      "Помощь")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
      "Загрузка содержимого")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
      "Сканирование содержимого")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
      "Что такое ядро?")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
      "История запуска")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
      "История")
MSG_HASH(MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
      "Горизонтальное меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
      "Изображения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INFORMATION,
      "Информация")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
      "Информация")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
      "Аналогово-цифровой тип")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
      "Все пользователи управляют меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
      "Левый аналоговый стик - ось X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
      "Левый аналоговый стик - ось X (влево)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
      "Левый аналоговый стик - ось X+ (вправо)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
      "Левый аналоговый стик - ось Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
      "Левый аналоговый стик - ось Y- (вверх)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
      "Левый аналоговый стик - ось Y+ (вниз)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
      "Правый аналоговый стик - ось X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
      "Правый аналоговый стик - ось X- (влево)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
      "Правый аналоговый стик - ось X + (вправо)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
      "Правый аналоговый стик - ось Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
      "Правый аналоговый стик - ось Y- (вверх)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
      "Правый аналоговый стик - ось Y+ (вниз)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
      "Gun Trigger")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
      "Gun Reload")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
      "Gun Aux A")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
      "Gun Aux B")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
      "Gun Aux C")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
      "Gun Start")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
      "Gun Select")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
      "Gun D-pad Up")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
      "Gun D-pad Down")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
      "Gun D-pad Left")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
      "Gun D-pad Right")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
      "Автоматическая настройка включена")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
      "Поменять кнопки OK и Отмена")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
      "Привязать всё")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
      "Привязать всё по умолчанию")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
      "Тайм-аут привязки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
      "Скрыть несвязанные входные дескрипторы ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
      "Отображать ярлыки дескрипторов ввода")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
      "Индекс устройства")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
      "Тип устройства")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
      "Индекс мыши")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
      "Драйвер ввода")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
      "Рабочий цикл")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
      "Привязка горячих клавиш")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
      "Клавиатура и геймпад включены")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
      "Кнопка A ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
      "Кнопка B ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
      "Крестовина ВНИЗ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
      "Кнопка L2 ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
      "Кнопка L3")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
      "Кнопка L ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
      "Крестовина ВЛЕВО")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
      "Кнопка R2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
      "Кнопка R3")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
      "Кнопка R")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
      "Крестовина ВПРАВО")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
      "Кнопка SELECT")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
      "Кнопка START")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
      "Крестовина ВВЕРХ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
      "Кнопка X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
      "Кнопка Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_KEY,
      "(Клавиша: %s)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
      "Левая кнопка мыши")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
      "Правая кнопка мыши")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
      "Средняя кнопка мыши")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
      "Кнопка мыши 4")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
      "Кнопка мыши 5")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
      "Колесо вверх")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
      "Колесо вниз")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
      "Колесо влево")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
      "Колесо вправо")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
      "Тип отображения клавиатуры для геймпада")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
      "Максимум пользователей")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
      "Комбинация вызова меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
      "Индекс чита -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
      "Индекс чита +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
      "Переключить читы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
      "Извлечь диск")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
      "Следующий диск")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
      "Предыдущий диск")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
      "Включить горячие клавиши")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
      "Перемотка удержанием")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
      "Включить перемотку вперед")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
      "Дополнительные фреймы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
      "На весь экран")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
      "Захват мыши")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
      "Переключить игровой фокус")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
      "Загрузить сохраненную игру")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
      "Отобразить меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
      "Записать видео")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
      "Заглушить звук")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
      "Переключить режим игры/наблюдателя Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
      "Показать экранную клавиатуру")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
      "Следующий оверлей")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
      "Пауза")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
      "Выйти из RetroArch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
      "Сбросить игру")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
      "Реверс")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
      "Сохранить игру")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
      "Сделать скриншот")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
      "Следующий шейдер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
      "Предыдущий шейдер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
      "Замедлить")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
      "Слот сохранения -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
      "Слот сохранения +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
      "Громкость -")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
      "Громкость +")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
      "Отобразить оверлей")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
      "Скрыть оверлей в меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
      "Задержка опроса устройств")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
      "Ранняя")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
      "Поздняя")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
      "Нормальная")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
      "Предпочительно переднее касание")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
      "Переопределение ввода")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
      "Включить замену привязок")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
      "Сохранить автоматическую настройку")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
      "Ввод")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
      "Включить малую клавиатуру")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
      "Включить касание")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
      "Включить турборежим")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
      "Период турборежима")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
      "Привязки ввода пользователя %u")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
      "Состояние внутренней памяти")
MSG_HASH(MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
      "Преднастроенные контроллеры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
      "Драйвер геймпада")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
      "Сервисы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
      "Китайский (Простой)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
      "Китайский (Традициональный)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
      "Голландский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
      "Английский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
      "Эсперанто")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
      "Французский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
      "Немецкий")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
      "Итальянский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
      "Японский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
      "Корейский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_POLISH,
      "Польский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL,
      "Португальский (Бразилия)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL,
      "Португальский (Португалия)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
      "Русский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
      "Испанский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE,
      "Вьетнамский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_ARABIC,
      "Aрабский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_GREEK,
      "Греческий")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LANG_TURKISH,
      "Турецкий")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
      "Левый аналоговый стик")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
      "Ядро")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
      "Информация о ядре")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
      "Уровень ведения журнала")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LINEAR,
      "Линейный")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
      "Загрузить архив")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
      "Загрузить последние")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
      "Загрузить контент")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_DISC,
      "Load Disc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMP_DISC,
      "Dump Disc")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_STATE,
      "Загрузить состояние")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
      "Разрешить определение местоположения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
      "Местоположение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
      "Журналирование")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
      "Ведение журнала")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MAIN_MENU,
      "Главное меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MANAGEMENT,
      "Настройки баз данных")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
      "Цветовая тема меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
      "Синий")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
      "Серо-голубой")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
      "Тёмно-синий")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
      "Зелёный")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
      "NVIDIA Shield")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
      "Красный")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
      "Жёлтый")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
      "Непрозрачность нижнего колонтитула")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
      "Непрозрачность заголовка")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
      "Драйвер меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
      "Максимальная частота кадров меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
      "File Browser")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
      "Линейный фильтр меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
      "Horizontal Animation")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
      "Меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
      "Задний фон")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
      "Непрозрачность фона")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MISSING,
      "Отсутствует")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MORE,
      "...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
      "Поддержка мыши")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
      "Мультимедиа")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
      "Музыка")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
      "Фильтрация неизвестных расширений")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
      "Цикличная прокрутка")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NEAREST,
      "Ближайший")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY,
      "Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
      "Разрешать клиентов в режиме Slave")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
      "Проверка фреймов Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
      "Кадры задержки ввода")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
      "Диапазон кадров задержки ввода")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
      "Задержка кадров Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
      "Отключено")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
      "Включить Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
      "Подключение к Netplay-хосту")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
      "Запустить Netplay-хост")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
      "Остановить Netplay-хост")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
      "Адрес сервера")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
      "Сканировать локальную сеть")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
      "Включить клиент Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
      "Имя пользователя")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
      "Пароль сервера")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
      "Публично анонсировать Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
      "Запретить клиентов не в режиме Slave")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
      "Настройки Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
      "Режим наблюдателя Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
      "Режим без состояния Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
      "Пароль сервера режима Spectate-Only")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
      "Включить режим зрителя в Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
      "Порт Netplay TCP / UDP")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
      "Пересечение NAT в Netplay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
      "Сетевые команды")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
      "Сетевой командный порт")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
      "Информация о сети")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
      "Сетевой геймпад")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
      "Сетевой удаленный базовый порт")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
      "Сеть")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO,
      "Нет")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NONE,
      "Отсутствует")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
      "N/A")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
      "Никаких достижений для показа.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE,
      "Без ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
      "Нет доступных ядер.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
      "Нет информации о ядре.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
      "Нет доступных опций ядра.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
      "Нет записей для отображения.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
      "История недоступна.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
      "Нет информации.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_ITEMS,
      "Нет элементов.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
      "Сетевые хосты не найдены.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
      "Сети не найдены.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
      "Нет счетчиков производительности.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
      "Нет плейлистов.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
      "Нет доступных записей в плейлисте.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
      "Настройки не найдены.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
      "Нет параметров шейдера.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OFF,
      "ВЫКЛ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ON,
      "ВКЛ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONLINE,
      "Онлайн")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
      "Онлайн-обновление")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
      "Экранное отображение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
      "Оверлей")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
      "Экранные уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
      "Обзор архива")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OPTIONAL,
      "Опционально")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY,
      "Оверлей")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
      "Автозагрузка оверлея")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
      "Оверлей")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
      "Непрозрачность оверлея")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
      "Преднастройка оверлея")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
      "Масштаб оверлея")
MSG_HASH(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
      "Оверлей")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
      "Использовать режим PAL60")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
      "Родительский каталог")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
      "Пауза при активации меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
      "Не работать в фоновом режиме")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
      "Счетчики производительности")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
      "Плейлисты")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
      "Плейлисты")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
      "Плейлисты")
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
      "Поддержка Touch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PORT,
      "Порт")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PRESENT,
      "Отображать")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
      "Конфиденциальность")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
      "Выход из RetroArch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
      "Поддерживается аналоговый сигнал")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
      "Рейтинг BBFC")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
      "Рейтинг CERO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
      "Кооператив поддерживается")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32,
      "CRC32")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
      "Описание")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
      "Разработчик")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
      "Проблема журнала Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
      "Рейтинг журнала Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
      "Обзор журнала Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
      "Рейтинг ELSPA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
      "Оборудование для улучшения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
      "Рейтинг ESRB")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
      "Рейтинг журнала Famitsu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
      "Франчайзинг")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
      "Жанр")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5,
      "MD5")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
      "Имя")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
      "Происхождение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
      "Рейтинг PEGI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
      "Издатель")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
      "Месяц выхода")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
      "Год выхода")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
      "Rumble поддерживается")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
      "Серия")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
      "SHA1")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
      "Запустить игру")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
      "Рейтинг TGDB")
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(MENU_ENUM_LABEL_VALUE_REBOOT,
      "Перезагрузка (RCM)")
#else
MSG_HASH(MENU_ENUM_LABEL_VALUE_REBOOT,
      "Перезагрузка")
#endif
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
      "Конфигурация записи")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
      "Каталог с записями")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
      "Запись")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
      "Загрузка конфигурации записи...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
      "Драйвер записи")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
      "Включить запись")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_PATH,
      "Сохранить запись вывода как...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
      "Сохранить записи в выходном каталоге")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE,
      "Файл переназначения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
      "Загрузить файл переназначения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
      "Сохранить файл переопределения ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
      "Сохранить файл переопределения игры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REQUIRED,
      "Необходимые")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
      "Перезапуск")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
      "Перезапустить RetroArch")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESUME,
      "Продолжить")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
      "Продолжить")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
      "Ретро-клавиатура")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROPAD,
      "Ретро-геймпад")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
      "Ретро-геймпад с аналогами")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
      "Достижения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
      "Включить перемотку назад")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
      "Гранулярность перемотки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
      "Перемотка назад")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
      "Браузер файлов")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
      "Конфигурация")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
      "Показ приветствия")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
      "Правый аналог")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN,
      "Запустить")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
      "Запустить")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
      "Включить SAMBA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
      "Сохранения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
      "Сохранить индекс автосостояния")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
      "Автозагрузка")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
      "Автосохранение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
      "Сохранения состояний")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
      "Обложка к сохранениям")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
      "Сохранить текущую конфигурацию")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "Сохранить переопределения ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "Сохранить переопределения игры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
      "Сохранить новую конфигурацию")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_STATE,
      "Сохранить состояние")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
      "Сохранения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
      "Сканировать каталог")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_FILE,
      "Сканировать файл")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
      "<Сканировать этот каталог>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
      "Скриншот")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
      "Разрешение экрана")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SEARCH,
      "Поиск")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SECONDS,
      "секунды")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SETTINGS,
      "Настройки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
      "Настройки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER,
      "Шейдер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
      "Применение изменений шейдера")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
      "Шейдеры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
      "Лента")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
      "Лента (упрощённая)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
      "Простой снег")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
      "Снег")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
      "Показать дополнительные настройки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
      "Показать скрытые файлы и директории")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHUTDOWN,
      "Завершение работы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
      "Коэффициент замедленного движения ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
      "Сохранить сортировку в папках")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
      "Сортировать сохранения в папках")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
      "SSH включён")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_CORE,
      "Запустить ядро")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
      "Запустить удаленный ретро-геймпад")
MSG_HASH(MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
      "Запустить видеопроцессор")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATE_SLOT,
      "Слот состояния")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATUS,
      "Статус")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
      "Команды stdin")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
      "Рекомендуемые ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
      "Заблокировать скринсейвер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
      "Включен режим BGM")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
      "System/BIOS")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
      "Системная информация")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
      "Поддержка 7zip")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
      "Поддержка ALSA")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
      "Дата компиляции")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
      "Поддержка Cg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
      "Поддержка Cocoa")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
      "Поддержка командного интерфейса")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
      "Поддержка CoreText")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
      "Особенности CPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
      "Количество точек на дюйм (DPI) монитора")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
      "Высота дисплея (мм)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
      "Ширина дисплея (мм)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
      "Поддержка DirectSound")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
      "Поддержка WASAPI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
      "Поддержка динамических библиотек")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
      "Загрузка динамических библиотек во время выполнения")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
      "Поддержка EGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
      "Поддержка OpenGL/Direct3D рендеринга для текстур")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
      "Поддержка FFmpeg")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
      "Поддержка FreeType")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
      "Поддержка STB TrueType")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
      "Идентификатор внешнего интерфейса")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
      "Имя внешнего интерфейса")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
      "Операционная система")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
      "Версия Git")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
      "Поддержка GLSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
      "Поддержка HLSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
      "Поддержка JACK")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
      "поддержка KMS/EGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
      "Версия Lakka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
      "Поддержка LibretroDB")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
      "Поддержка Libusb")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
      "Поддержка Netplay (peer-to-peer)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
      "Поддержка сетевого командного интерфейса")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
      "Поддержка геймпада в сети")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
      "Поддержка OpenAL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
      "Поддержка OpenGL ES")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
      "Поддержка OpenGL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
      "Поддержка OpenSL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
      "Поддержка OpenVG")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
      "Поддержка OSS")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
      "Поддержка Overlay")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
      "Источник питания")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
      "Заряжена")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
      "Заряжается")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
      "Разряжается")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
      "Нет источника")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
      "Поддержка PulseAudio")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
      "Поддержка Python (поддержка скриптов в шейдерах)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
      "Поддержка BMP(RBMP)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
      "Уровень RetroRating")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
      "Поддержка JPEG (RJPEG)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
      "Поддержка RoarAudio")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
      "Поддержка PNG(RPNG)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
      "Поддержка RSound")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
      "Поддержка TGA(RTGA)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
      "Поддержка SDL2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
      "Поддержка изображений SDL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
      "Поддержка SDL1.2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
      "Поддержка Slang")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
      "Поддержка Threading")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
      "Поддержка Udev")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
      "Поддержка Video4Linux2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
      "Драйвер контекстного видео")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
      "Поддержка Vulkan")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
      "Поддержка Wayland")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
      "Поддержка X11")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
      "Поддержка XAudio2")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
      "Поддержка XVideo")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
      "Поддержка Zlib")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
      "Сделать скриншот")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
      "Реализованные задачи")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS,
      "Миниатюры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
      "Миниатюры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
      "Обновление ядер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
      "Бокс-арты")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
      "Скриншоты")
MSG_HASH(MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
      "Экраны заголовков")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
      "Показать дату/время")
MSG_HASH(MENU_ENUM_LABEL_VALUE_TRUE,
      "True")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
      "UI Companion включён")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
      "Включить UI Companion при загрузке")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
      "Строка меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
      "Не удалось прочитать сжатый файл.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
      "Отменить загрузку")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
      "Отменить сохранение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UNKNOWN,
      "Неизвестно")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
      "Обновитель")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
      "Обновить ресурсы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
      "Обновить профили автоматической настройки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
      "Обновление Cg шейдеров")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
      "Обновить читы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
      "Обновить основные информационные файлы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
      "Обновить базы данных")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
      "Обновить GLSL шейдеры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
      "Обновить Lakka")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
      "Обновить оверлеи")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
      "Обновить сленговые шейдеры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER,
      "Игрок")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
      "Интерфейс пользователя")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
      "Язык")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
      "Пользователь")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
      "Использовать встроенный просмотрщик изображений")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
      "Использовать встроенный медиаплеер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
      "<Использовать этот каталог>")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
      "Разрешить вращение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
      "Настройки соотношения сторон")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
      "Автоотношение сторон")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
      "Соотношение сторон")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
      "Вставка чёрного кадра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
      "Обрезка обрезки (перезагрузка)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
      "Отключить компоновку рабочего стола")
#if defined(_3DS)
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
      "Нижний экран 3DS")
#endif
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
      "Видеодрайвер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
      "Видеофильтр")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
      "Видеофильтр")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
      "Мерцающий фильтр")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
      "Включить экранные уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
      "Шрифт экранного уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
      "Размер уведомлений на экране")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
      "Форсировать пропорции")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
      "Принудительное отключение sRGB FBO")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
      "Задержка кадра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
      "Полноэкранный режим")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
      "Видеогамма")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
      "Использовать GPU Recording")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
      "GPU Screenshot включен")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
      "Принудительная синхронизация GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
      "Количество кадров принудительной синхронизации GPU")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
      "Максимальное количество образов свопчейнов")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
      "Позиция уведомления на экране по оси X")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
      "Позиция уведомления на экране по оси Y")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
      "Индекс монитора")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
      "Использовать запись после фильтра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
      "Вертикальная частота обновления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
      "Оценочная частота экрана")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
      "Возвращать частоту обновления дисплея")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
      "Вращение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
      "Масштабирование окна")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
      "Целочисленное масштабирование")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
      "Видео")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
      "Шейдеры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
      "Шейдерные проходы")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
      "Предварительный просмотр параметров шейдера")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
      "Загрузить предварительную настройку шейдера")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
      "Сохранить предустановку шейдера как")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
      "Сохранить предварительную настройку ядра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
      "Сохранить предварительную настройку игры")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
      "Включить Hardware Shared Context")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
      "Билинейная фильтрация")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
      "Включён мягкий фильтр")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
      "Интервал свопинга по вертикальной синхронизации")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
      "Видео")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
      "Многопоточное видео")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
      "Дефликер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
      "Высота пользовательского видового экрана")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
      "Пользовательская ширина видового экрана")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
      "Пользовательский видовой экран X Поз.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
      "Пользовательский видовой экран Y Поз.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
      "Установить ширину экрана VI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
      "Вертикальная синхронизация (Vsync)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
      "Оконный полноэкранный режим")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
      "Ширина окна")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
      "Высота окна")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
      "Полноэкранная ширина")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
      "Полноэкранная высота")
MSG_HASH(MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
      "Драйвер Wi-Fi")
MSG_HASH(MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
      "Wi-Fi")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
      "Меню Alpha Factor")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
      "Menu Font Red Color")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
      "Menu Font Green Color")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
      "Menu Font Blue Color")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_FONT,
      "Шрифт")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
      "Пользовательский")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
      "FlatUI")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
      "Монохромный")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
      "Монохромный Inverted")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
      "Систематический")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
      "NeoActive")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
      "Пиксель")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
      "RetroActive")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
      "Retrosystem")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
      "Dot-Art")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
      "Цветная тема меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
      "Зеленое яблоко")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
      "Тёмный")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
      "Тёмно-фиолетовый")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
      "Электрический синий")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
      "Золотой")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
      "Красный")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
      "Тёмно-синий")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
      "Обычный")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
      "Водные глубины")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
      "Вулканический красный")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
      "Шейдерный фон меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR,
      "Масштаб в меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
      "Отображение теней у иконок")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
      "Показать вкладку История просмотров")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
      "Показать вкладку Импорт контента")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
      "Показать вкладку Изображение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
      "Показать вкладку Музыка")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
      "Показать вкладку Настройки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
      "Показать вкладку Видео")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
      "Показать вкладку Сетевая игра")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
      "Menu Layout")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_THEME,
      "Тема значка меню")
MSG_HASH(MENU_ENUM_LABEL_VALUE_YES,
      "Да")
MSG_HASH(MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
      "Предустановка шейдера")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
      "Включение или отключение достижений. Для получения дополнительной информации посетите страницу http://retroachievements.org")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
      "Включение или отключение неофициальных достижений и/или бета-функций в целях тестирования.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Включить или отключить сохранения, читы, перемотку назад, перемотку вперед, паузу и замедленное воспроизведение для всех игр.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
      "Включить или отключить экран OSD для достижений.")
MSG_HASH(MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
      "Изменить драйвера для этой системы.")
MSG_HASH(MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
      "Изменить настройки достижений.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_SETTINGS,
      "Изменить настройки ядра.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
      "Изменить настройки записи.")
MSG_HASH(MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
      "Измените настройки отображения перекрытия, наложения клавиатуры и уведомлений на экране.")
MSG_HASH(MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
      "Измените настройки перемотки, быстрой перемотки и замедленного воспроизведения.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
      "Измените настройки сохранения.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
      "Изменить настройки ведения журнала.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
      "Изменить настройки пользовательского интерфейса.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_SETTINGS,
      "Изменить учетные записи, имя пользователя и язык.")
MSG_HASH(MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
      "Изменить настройки конфиденциальности.")
MSG_HASH(MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
      "Изменение каталогов по умолчанию для этой системы.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
      "Изменить настройки списков воспроизведения.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
      "Настроить параметры сервера и сети.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
      "Сканировать содержимое и добавить в базу данных.")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
      "Настройка параметров аудиовыхода.")
MSG_HASH(MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
      "Включить или отключить Bluetooth.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
      "Сохраняет изменения в файле конфигурации при выходе.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
      "Измените настройки по умолчанию для файлов конфигурации.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
      "Управление и создание файлов конфигурации.")
MSG_HASH(MENU_ENUM_SUBLABEL_CPU_CORES,
      "Количество ядер CPU.")
MSG_HASH(MENU_ENUM_SUBLABEL_FPS_SHOW,
      "Отображает текущую частоту кадров в секунду на экране.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
      "Настроить параметры горячих клавиш.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
      "Комбинация кнопок геймпада для переключения меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
      "Настройки управления для джойстика, клавиатуры и мыши.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
      "Настройки элементов управления для этого пользователя.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
      "Включить или отключить ведение журнала в терминале.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY,
      "Присоединиться или создать сеанс Netplay.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
      "Поиск и подключение к серверу по локальной сети.")
MSG_HASH(MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
      "Показать информацию о ядре, сети и системе.")
MSG_HASH(MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
      "Загрузите надстройки, компоненты и содержимое для RetroArch.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
      "Включить или отключить сетевой доступ к папкам.")
MSG_HASH(MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
      "Управление службами операционной системы.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
      "Показывать скрытые файлы/каталоги внутри файлового менеджера.")
MSG_HASH(MENU_ENUM_SUBLABEL_SSH_ENABLE,
      "Включить или отключить удаленный доступ к командной строке.")
MSG_HASH(MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
      "Запрещается активация скринсейвера вашей системы.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
      "Устанавливает размер окна относительно размера окна просмотра. Кроме того, вы можете установить ширину и высоту окна ниже для фиксации размера окна.")
MSG_HASH(MENU_ENUM_SUBLABEL_USER_LANGUAGE,
      "Устанавливает язык интерфейса.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
      "Вставляет черную рамку между кадрами. Полезно для пользователей с экранами с частотой 120 Гц, которые хотят проигрывать материал с частотой 60 Гц с устраненным ореолом.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
      "Уменьшает задержку за счет более высокого риска заикания видео. Добавляет задержку после V-Sync (в мс).")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
      "Устанавливает, сколько кадров процессор может запустить перед графическим процессором при использовании Hard GPU Sync.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
      "Указывает видеодрайверу, какой режим буферизации использовать.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
      "Выбирает, какой экран дисплея использовать.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
      "Точная оценка частоты обновления экрана в Гц.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
      "Частота обновления дисплея возвращенная драйвером дисплея.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
      "Настройка параметров вывода видео.")
MSG_HASH(MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
      "Сканирует беспроводные сети и устанавливает соединение.")
MSG_HASH(MENU_ENUM_SUBLABEL_HELP_LIST,
      "Узнайте больше о том, как это работает.")
MSG_HASH(MSG_APPENDED_DISK,
      "Вставлен диск")
MSG_HASH(MSG_APPLICATION_DIR,
      "Каталог приложений")
MSG_HASH(MSG_APPLYING_CHEAT,
      "Применение читов.")
MSG_HASH(MSG_APPLYING_SHADER,
      "Применён шейдер")
MSG_HASH(MSG_AUDIO_MUTED,
      "Звук откл.")
MSG_HASH(MSG_AUDIO_UNMUTED,
      "Звук вкл.")
MSG_HASH(MSG_AUTOCONFIG_FILE_ERROR_SAVING,
      "Ошибка при сохранении файла автоматической настройки.")
MSG_HASH(MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
      "Файл автоматической настройки успешно сохранен.")
MSG_HASH(MSG_AUTOSAVE_FAILED,
      "Ошибка автосохранения.")
MSG_HASH(MSG_AUTO_SAVE_STATE_TO,
      "Процесс автосохранения")
MSG_HASH(MSG_BLOCKING_SRAM_OVERWRITE,
      "Перезапись SRAM запрещена.")
MSG_HASH(MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
      "Вывод командного интерфейса на порт")
MSG_HASH(MSG_BYTES,
      "Байт")
MSG_HASH(MSG_CANNOT_INFER_NEW_CONFIG_PATH,
      "Вывод командного интерфейса на порт.")
MSG_HASH(MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
      "Хардкорный режим включен, ваши сохранения и функция перемотки отключены.")
MSG_HASH(MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
      "Сравнение с известными магическими числами...")
MSG_HASH(MSG_COMPILED_AGAINST_API,
      "Скомпилирован против API")
MSG_HASH(MSG_CONFIG_DIRECTORY_NOT_SET,
      "Не задана папка хранения настроек. Невозможно сохранить конфигурацию.")
MSG_HASH(MSG_CONNECTED_TO,
      "Подключен к")
MSG_HASH(MSG_CONTENT_CRC32S_DIFFER,
      "Содержимое CRC32 различается. Нельзя использовать разные игры.")
MSG_HASH(MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
      "Загрузка контента пропущена, реализация будет загружена сама по себе.")
MSG_HASH(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
      "Ядро не поддерживает быстрые сохранения.")
MSG_HASH(MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
      "Файл основных настроек успешно создан.")
MSG_HASH(MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
      "Не удалось найти следующий драйвер")
MSG_HASH(MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
      "Не удалось найти совместимую систему.")
MSG_HASH(MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
      "Не удалось найти дорожку данных")
MSG_HASH(MSG_COULD_NOT_OPEN_DATA_TRACK,
      "Не удалось открыть дорожку данных")
MSG_HASH(MSG_COULD_NOT_READ_CONTENT_FILE,
      "Не удалось прочитать файл контента")
MSG_HASH(MSG_COULD_NOT_READ_MOVIE_HEADER,
      "Не удалось прочитать заголовок видеоролика.")
MSG_HASH(MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
      "Не удалось прочитать состояние видеоролика.")
MSG_HASH(MSG_CRC32_CHECKSUM_MISMATCH,
      "Ошибка контрольной суммы CRC32 между файлом содержимого и контрольной суммой сохраненного содержимого в заголовке файла воспроизведения), что, скорее всего, приведет к рассинхронизации при воспроизведении.")
MSG_HASH(MSG_CUSTOM_TIMING_GIVEN,
      "Задано ручное значение тайминга.")
MSG_HASH(MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
      "Декомпрессия уже выполняется.")
MSG_HASH(MSG_DECOMPRESSION_FAILED,
      "Ошибка декомпрессии.")
MSG_HASH(MSG_DETECTED_VIEWPORT_OF,
      "Обнаружено окно проекции")
MSG_HASH(MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
      "Не найден правильный патч для контента.")
MSG_HASH(MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
      "Отключите устройство от действительного порта.")
MSG_HASH(MSG_DISK_CLOSED,
      "Закрыто")
MSG_HASH(MSG_DISK_EJECTED,
      "Выброс")
MSG_HASH(MSG_DOWNLOADING,
      "Прогресс загрузки")
MSG_HASH(MSG_DOWNLOAD_FAILED,
      "Загрузка не удалась")
MSG_HASH(MSG_ERROR,
      "Ошибка")
MSG_HASH(MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
      "Ядру Libretro требуется контент, но ничего не было предоставлено.")
MSG_HASH(MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
      "Ядру Libretro требуется особый контент, но его не было.")
MSG_HASH(MSG_ERROR_PARSING_ARGUMENTS,
      "Ошибка при анализе аргументов.")
MSG_HASH(MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
      "Ошибка при сохранении файла основных опций.")
MSG_HASH(MSG_ERROR_SAVING_REMAP_FILE,
      "Ошибка при сохранении файла переопределения.")
MSG_HASH(MSG_ERROR_SAVING_SHADER_PRESET,
      "Ошибка сохранения пресетов шейдера.")
MSG_HASH(MSG_EXTERNAL_APPLICATION_DIR,
      "Внешний каталог приложений")
MSG_HASH(MSG_EXTRACTING,
      "Извлечение")
MSG_HASH(MSG_EXTRACTING_FILE,
      "Извлечение файла")
MSG_HASH(MSG_FAILED_SAVING_CONFIG_TO,
      "Ошибка сохранения конфигурации в")
MSG_HASH(MSG_FAILED_TO,
      "Ошибка")
MSG_HASH(MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
      "Не удалось принять зрителя.")
MSG_HASH(MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
      "Не удалось выделить память для пропатченного содержимого ...")
MSG_HASH(MSG_FAILED_TO_APPLY_SHADER,
      "Не удалось применить шейдер")
MSG_HASH(MSG_FAILED_TO_BIND_SOCKET,
      "Не удалось привязать сокет.")
MSG_HASH(MSG_FAILED_TO_CREATE_THE_DIRECTORY,
      "Не удалось создать каталог.")
MSG_HASH(MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
      "Не удалось извлечь содержимое из сжатого файла")
MSG_HASH(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
      "Не удалось получить псевдоним от клиента.")
MSG_HASH(MSG_FAILED_TO_LOAD,
      "Ошибка загрузки")
MSG_HASH(MSG_FAILED_TO_LOAD_CONTENT,
      "Не удалось загрузить контент")
MSG_HASH(MSG_FAILED_TO_LOAD_MOVIE_FILE,
      "Не удалось загрузить файл записи.")
MSG_HASH(MSG_FAILED_TO_LOAD_OVERLAY,
      "Ошибка загрузки оверлея.")
MSG_HASH(MSG_FAILED_TO_LOAD_STATE,
      "Ошибка загрузки сохранения из")
MSG_HASH(MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
      "Не удалось открыть ядро libretro")
MSG_HASH(MSG_FAILED_TO_PATCH,
      "Не удалось пропатчить")
MSG_HASH(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
      "Не удалось получить заголовок от клиента.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME,
      "Не удалось получить никнейм.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
      "Не удалось получить никнейм с узла.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
      "Не удалось получить размер никнейма от хоста.")
MSG_HASH(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
      "Не удалось получить данные SRAM с хоста.")
MSG_HASH(MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
      "Невозможно извлечь диск.")
MSG_HASH(MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
      "Ошибка удаления временного файла.")
MSG_HASH(MSG_FAILED_TO_SAVE_SRAM,
      "Ошибка сохранения SRAM")
MSG_HASH(MSG_FAILED_TO_SAVE_STATE_TO,
      "Ошибка записи сохранения в")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME,
      "Не удалось отправить никнейм.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_SIZE,
      "Не удалось отправить размер никнейма.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
      "Не удалось отправить никнейм клиенту.")
MSG_HASH(MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
      "Не удалось отправить никнейм на хост.")
MSG_HASH(MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
      "Не удалось отправить данные SRAM клиенту.")
MSG_HASH(MSG_FAILED_TO_START_AUDIO_DRIVER,
      "Не удалось запустить звуковой драйвер. Будет продолжено без звука.")
MSG_HASH(MSG_FAILED_TO_START_MOVIE_RECORD,
      "Невозможно начать запись видео.")
MSG_HASH(MSG_FAILED_TO_START_RECORDING,
      "Невозможно начать запись.")
MSG_HASH(MSG_FAILED_TO_TAKE_SCREENSHOT,
      "Невозможно создать скриншот.")
MSG_HASH(MSG_FAILED_TO_UNDO_LOAD_STATE,
      "Не удалось отменить загрузку.")
MSG_HASH(MSG_FAILED_TO_UNDO_SAVE_STATE,
      "Не удалось отменить сохранение.")
MSG_HASH(MSG_FAILED_TO_UNMUTE_AUDIO,
      "Не удалось включить звук.")
MSG_HASH(MSG_FATAL_ERROR_RECEIVED_IN,
      "Получена критическая ошибка")
MSG_HASH(MSG_FILE_NOT_FOUND,
      "Файл не найден")
MSG_HASH(MSG_FOUND_AUTO_SAVESTATE_IN,
      "Найдено автосохранение в")
MSG_HASH(MSG_FOUND_DISK_LABEL,
      "Метка найденного диска")
MSG_HASH(MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
      "Найдена первая дорожка данных в файле")
MSG_HASH(MSG_FOUND_LAST_STATE_SLOT,
      "Найдено последний слот состояния")
MSG_HASH(MSG_FOUND_SHADER,
      "Обнаружен шейдер")
MSG_HASH(MSG_FRAMES,
      "Рамки")
MSG_HASH(MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
      "Параметры игры: параметры ядра, определенные для игры")
MSG_HASH(MSG_GOT_INVALID_DISK_INDEX,
      "Задан неверный номер диска.")
MSG_HASH(MSG_GRAB_MOUSE_STATE,
      "Режим перехвата мыши")
MSG_HASH(MSG_GAME_FOCUS_ON,
      "Игровой фокус включен")
MSG_HASH(MSG_GAME_FOCUS_OFF,
      "Игровой фокус выключен")
MSG_HASH(MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
      "Ядро использует аппаратный рендеринг. Включите запись с GPU.")
MSG_HASH(MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
      "Завышенная контрольная сумма не соответствует CRC32.")
MSG_HASH(MSG_INPUT_CHEAT,
      "Исходный чит")
MSG_HASH(MSG_INPUT_CHEAT_FILENAME,
      "Имя чита")
MSG_HASH(MSG_INPUT_PRESET_FILENAME,
      "Имя настроек")
MSG_HASH(MSG_INPUT_RENAME_ENTRY,
      "Переименовать заголовок")
MSG_HASH(MSG_INTERFACE,
      "Интерфейс")
MSG_HASH(MSG_INTERNAL_STORAGE,
      "Внутреннее хранилище")
MSG_HASH(MSG_REMOVABLE_STORAGE,
      "Внешнее хранилище")
MSG_HASH(MSG_INVALID_NICKNAME_SIZE,
      "Недопустимый размер никнейма.")
MSG_HASH(MSG_IN_BYTES,
      "В байтах")
MSG_HASH(MSG_IN_GIGABYTES,
      "В гигабайтах")
MSG_HASH(MSG_IN_MEGABYTES,
      "В мегабайтах")
MSG_HASH(MSG_LIBRETRO_ABI_BREAK,
      "Скомпилировано для другой версии libretro.")
MSG_HASH(MSG_LIBRETRO_FRONTEND,
      "Внешний интерфейс для libretro")
MSG_HASH(MSG_LOADED_STATE_FROM_SLOT,
      "Загружено сохранение из слота #%d.")
MSG_HASH(MSG_LOADED_STATE_FROM_SLOT_AUTO,
      "Загружено сохранение из слота #-1 (auto).")
MSG_HASH(MSG_LOADING,
      "Загрузка")
MSG_HASH(MSG_FIRMWARE,
      "Отсутствуют один или несколько файлов прошивки")
MSG_HASH(MSG_LOADING_CONTENT_FILE,
      "Загружен файл контента")
MSG_HASH(MSG_LOADING_HISTORY_FILE,
      "Загрузка файла истории")
MSG_HASH(MSG_LOADING_STATE,
      "Загружено сохранение")
MSG_HASH(MSG_MEMORY,
      "Память")
MSG_HASH(MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
      "Видеофайл не является допустимым файлом BSV1.")
MSG_HASH(MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
      "Формат фильма, похоже, отличается от версии сериализатора.")
MSG_HASH(MSG_MOVIE_PLAYBACK_ENDED,
      "Достигнут конец записи.")
MSG_HASH(MSG_MOVIE_RECORD_STOPPED,
      "Запись остановлена.")
MSG_HASH(MSG_NETPLAY_FAILED,
      "Ошибка запуска сетевой игры.")
MSG_HASH(MSG_NO_CONTENT_STARTING_DUMMY_CORE,
      "Нет содержимого, запуск фиктивного ядра.")
MSG_HASH(MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
      "Сохраненное состояние не было перезаписано.")
MSG_HASH(MSG_NO_STATE_HAS_BEEN_LOADED_YET,
      "Состояние еще не загружено.")
MSG_HASH(MSG_OVERRIDES_ERROR_SAVING,
      "Ошибка сохранения переопределений.")
MSG_HASH(MSG_OVERRIDES_SAVED_SUCCESSFULLY,
      "Переопределения сохранены успешно.")
MSG_HASH(MSG_PAUSED,
      "Пауза вкл.")
MSG_HASH(MSG_PROGRAM,
      "RetroArch")
MSG_HASH(MSG_READING_FIRST_DATA_TRACK,
      "Чтение первой дорожки данных ...")
MSG_HASH(MSG_RECEIVED,
      "Получено")
MSG_HASH(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
      "Размеры окна были изменены. Запись остановлена.")
MSG_HASH(MSG_RECORDING_TO,
      "Запись в")
MSG_HASH(MSG_REDIRECTING_CHEATFILE_TO,
      "Файл с чит-кодами перенаправлен в")
MSG_HASH(MSG_REDIRECTING_SAVEFILE_TO,
      "Файл карты памяти перенаправлен в")
MSG_HASH(MSG_REDIRECTING_SAVESTATE_TO,
      "Файл сохранения перенаправлен в")
MSG_HASH(MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
      "Файл перекодировки успешно сохранен.")
MSG_HASH(MSG_REMOVED_DISK_FROM_TRAY,
      "Диск извлечён.")
MSG_HASH(MSG_REMOVING_TEMPORARY_CONTENT_FILE,
      "Удалён временный файл контента")
MSG_HASH(MSG_RESET,
      "Сброс")
MSG_HASH(MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
      "Реинициализация драйвера. Запись перезапущена.")
MSG_HASH(MSG_RESTORED_OLD_SAVE_STATE,
      "Восстановлено старое состояние сохранения.")
MSG_HASH(MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
      "Шейдеры: восстановление стандартных настроек шейдера")
MSG_HASH(MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
      "Возврат каталога карт памяти на")
MSG_HASH(MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
      "Возврат каталога сохранений к")
MSG_HASH(MSG_REWINDING,
      "Перемотка.")
MSG_HASH(MSG_REWIND_INIT,
      "Инициализация буфера перемотки с размером")
MSG_HASH(MSG_REWIND_INIT_FAILED,
      "Ошибка создания буфера перемотки. Перемотка будет отключена.")
MSG_HASH(MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
      "Ядро использует многопоточный звук. Перемотка невозможна.")
MSG_HASH(MSG_REWIND_REACHED_END,
      "Достигнут предел буфера перемотки.")
MSG_HASH(MSG_SAVED_NEW_CONFIG_TO,
      "Сохранено новое имя конфигурации")
MSG_HASH(MSG_SAVED_STATE_TO_SLOT,
      "Сохранено в слот #%d.")
MSG_HASH(MSG_SAVED_STATE_TO_SLOT_AUTO,
      "Сохранено в слот #-1 (auto).")
MSG_HASH(MSG_SAVED_SUCCESSFULLY_TO,
      "Успешно сохранено в")
MSG_HASH(MSG_SAVING_RAM_TYPE,
      "Запись RAM")
MSG_HASH(MSG_SAVING_STATE,
      "Сохранено")
MSG_HASH(MSG_SCANNING,
      "Сканирование")
MSG_HASH(MSG_SCANNING_OF_DIRECTORY_FINISHED,
      "Сканирование папки завершено")
MSG_HASH(MSG_SENDING_COMMAND,
      "Отправка команды")
MSG_HASH(MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
      "Несколько патчей явно определены, игнорируя все...")
MSG_HASH(MSG_SHADER,
      "Шейдер")
MSG_HASH(MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
      "Предварительная установка шейдера успешно сохранена.")
MSG_HASH(MSG_SKIPPING_SRAM_LOAD,
      "Пропуск загрузки SRAM.")
MSG_HASH(MSG_SLOW_MOTION,
      "Замедление.")
MSG_HASH(MSG_FAST_FORWARD,
      "Перемотка вперед.")
MSG_HASH(MSG_SLOW_MOTION_REWIND,
      "Замедленная перемотка.")
MSG_HASH(MSG_SRAM_WILL_NOT_BE_SAVED,
      "Невозможно сохранить SRAM.")
MSG_HASH(MSG_STARTING_MOVIE_PLAYBACK,
      "Воспроизведение записи.")
MSG_HASH(MSG_STARTING_MOVIE_RECORD_TO,
      "Запись видео в")
MSG_HASH(MSG_STATE_SIZE,
      "Размер сохранения")
MSG_HASH(MSG_STATE_SLOT,
      "Слот сохранения")
MSG_HASH(MSG_TAKING_SCREENSHOT,
      "Скриншот сохранён.")
MSG_HASH(MSG_TO,
      "в")
MSG_HASH(MSG_UNDID_LOAD_STATE,
      "Отключить состояние загрузки.")
MSG_HASH(MSG_UNDOING_SAVE_STATE,
      "Отмена сохранения состояния")
MSG_HASH(MSG_UNKNOWN,
      "Неизвестно.")
MSG_HASH(MSG_UNPAUSED,
      "Пауза откл.")
MSG_HASH(MSG_UNRECOGNIZED_COMMAND,
      "Неизвестная команда")
MSG_HASH(MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
      "Использование имени ядра для новой конфигурации.")
MSG_HASH(MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
      "Используется фиктивное ядро. Запись не производится.")
MSG_HASH(MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
      "Подключите устройство к действительному порту.")
MSG_HASH(MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
      "Отключение устройства от порта")
MSG_HASH(MSG_VALUE_REBOOTING,
      "Перезагрузка ...")
MSG_HASH(MSG_VALUE_SHUTTING_DOWN,
      "Выключение...")
MSG_HASH(MSG_VERSION_OF_LIBRETRO_API,
      "Версия API libretro")
MSG_HASH(MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
      "Ошибка расчёта размеров окна проекции. Будут использованы необработанные данные. Возможны ошибки.")
MSG_HASH(MSG_VIRTUAL_DISK_TRAY,
      "Виртуальный лоток cd-привода")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
      "Требуемая задержка звука в миллисекундах. Может не работать если драйвер не может обеспечить заданную задержку.")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MUTE,
      "Отключить/включить звук.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
      "Помогает синхронизировать аудио и видео. Имейте в виду, что если отключено, правильную синхронизацию получить почти невозможно."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
      "Разрешить или запретить ядрам доступ к камере."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
      "Разрешить или запретить ядрам доступ к службам определения местоположения."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
      "Максимальное количество пользователей, поддерживаемых RetroArch."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
      "Изменение скорости опроса устройств. Значения «Ранняя» или «Поздняя» могут привести к меньшей задержке в зависимости от конфигурации."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
      "Позволяет любому пользователю управлять меню. Если отключено, только пользователь 1 может управлять меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
      "Громкость звука (в дБ). 0 дБ - это нормальный уровень громкости, при этом не применяется усиление."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
      "Разрешить драйверу WASAPI получать полный контроль над аудиоустройством. При отключении будет использоваться общий режим."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
      "Использовать формат с плавающей точкой для драйвера WASAPI, если он поддерживается вашим аудиоустройством."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
      "Длина промежуточного буфера (в кадрах) при использовании драйвера WASAPI в общем режиме."
      )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Синхронизировать звук. Рекомендуется."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Количество секунд ожидания до перехода к следующей привязке."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Описывает период, в котором переключаются турбокнопки. Значение описывается в кадрах."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "Описывает, как долго должен действовать период турбокнопки. Значение описывается в кадрах."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Синхронизирует видеовыход видеокарты с частотой обновления экрана. Рекомендуется."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Разрешить или запретить ядрам поворачивать экран. Полезно когда экран поворачивается вручную."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Некоторые ядра могут иметь функцию отключения. Если она включена, это предотвратит остановку RetroArch, вместо этого он загрузит фиктивное ядро."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Перед загрузкой контента проверьте, все ли необходимые микропрограммы присутствуют."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Частота обновления экрана. Используется для расчета подходящей скорости ввода аудиосигнала. Игнорируется если включено потоковое видео."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Включить вывод звука."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Максимальное изменение скорости ввода аудиосигнала. Возможно, вы захотите увеличить его, чтобы обеспечить очень большие изменения во времени, например, запуск PAL-ядер на дисплеях NTSC за счет неточности звукового поля."
   )
MSG_HASH(
   MSG_FAILED,
   "Не удалось"
   )
MSG_HASH(
   MSG_SUCCEEDED,
   "Удалось"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "Не настроено"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "Не настроен, используется резервный"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Список курсоров баз данных"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "База данных - фильтр: разработчик"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "База данных - фильтр: издатель"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Отключено"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Включено"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Путь к истории контента"
   )
MSG_HASH(
      MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
      "База данных - фильтр: по происхождению")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
      "База данных - фильтр: по франчайзу")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
      "База данных - фильтр: по рейтингу ESRB")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
      "База данных - фильтр: по рейтингу ELSPA")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
      "База данных - фильтр: по рейтингу PEGI")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
      "База данных - фильтр: по рейтингу CERO")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
      "База данных - фильтр: по рейтингу BBFC")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
      "База данных - фильтр: по кол-во игроков")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
      "База данных - фильтр: Вышедшие по месяцам")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
      "База данных - фильтр: Вышедшие по годам")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
      "База данных - фильтр: ошибки журнела Edge")
MSG_HASH(MENU_ENUM_LABEL_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
      "База данных - фильтр: рейтинг журнала Edge")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
      "Информация базы данных")
MSG_HASH(MSG_WIFI_SCAN_COMPLETE,
      "Сканирование Wi-Fi успешно завершено.")
MSG_HASH(MSG_SCANNING_WIRELESS_NETWORKS,
      "Сканирование беспроводных сетей...")
MSG_HASH(MSG_NETPLAY_LAN_SCAN_COMPLETE,
      "Сканирование Netplay завершено.")
MSG_HASH(MSG_NETPLAY_LAN_SCANNING,
      "Сканирование хостов Netplay...")
MSG_HASH(MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
      "Ставить на паузу, когда окно теряет фокус.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
      "Включить или отключить композицию (только для Windows).")
MSG_HASH(MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
      "Включить или отключить показ недавно запущенных игр, изображений, музыки и видео.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
      "Ограничить количество записей в последнем плейлисте для игр, изображений, музыки и видео.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
      "Унифицировать управление в меню")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
      "Использовать такое же управление в меню, как в игре (для клавиатуры).")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
      "Показывать сообщения на экране.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
      "Включить удаленного пользователя %d")
MSG_HASH(MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
      "Показать заряд батареи")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SELECT_FILE,
      "Выбрать файл")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
      "Выбрать из плейлиста.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FILTER,
      "Фильтр")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCALE,
      "Масштаб")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
      "Netplay заработает, когда вы запустите игру.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
      "Не удается найти требуемое ядро или файл контента, загрузите его вручную.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
      "Просмотр URL"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_URL,
      "Путь URL"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_BROWSE_START,
      "Старт"
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
      "Боке")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
      "Обновить список комнат")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
      "Имя: %s")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
      "Имя (lan): %s")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
      "Найден совместимый контент")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
      "Отрезает несколько пикселей вокруг краев экрана, обычно оставленными пустыми разработчиками, которые иногда содержат лишь мусор.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
      "Добавляет небольшое смазывание к изображению, чтобы избавиться от острых пикселей. Эта настройка мало влияет на производительность.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FILTER,
      "Применяется видеофильтр, работающий на CPU. ПРИМЕЧАНИЕ: Возможна высокая нагрузка на производительность. Некоторые фильтры могут работать только на некоторых ядрах, использующих 32-битную или 16-битную палитру.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
      "Введите имя пользователя от вашего аккаунта Retro Achievements.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
      "Введите пароль от вашего аккаунта Retro Achievements.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
      "Введите ваш никнейм. Он будет использоваться в сессиях Netplay и т.д.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
      "Производить запись после применения фильтров (но не шейдеров). Записанное видео будет так же красиво как и во время игры.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_LIST,
      "Выберите, какое ядро использовать.")
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
      "Выберите, какой контент запустить.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
      "Показать сетевые интерфейсы и их IP адреса.")
MSG_HASH(MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
      "Показать информацию об устройстве.")
MSG_HASH(MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
      "Закрыть программу.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
      "Изменить размер ширины для окна дисплея. Если вы поменяете параметр на 0, то программа будет пытаться масштабировать окно как можно больше.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
      "Изменить размер высоты для окна дисплея. Если вы поменяете параметр на 0, то программа будет пытаться масштабировать окно как можно больше.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
      "Установить свой размер ширины для неоконного полноэкранного режима. Если оставить его значение на 0, то будет использовано разрешение рабочего стола.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
      "Установить свой размер высоты для неоконного полноэкранного режима. Если оставить его значение на 0, то будет использовано разрешение рабочего стола.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
      "Изменить положение оповещение по горизонтали.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
      "Изменить положение оповещение по вертикали.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
      "Изменить размер шрифта уведомления.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
      "Спрятать наложения в меню интерфейса, и показывать снова после выхода из него.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
      "Сканированный контент появится здесь."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
      "Масштабирование видео с целочисленным интервалом. Базовый размер зависит от системной геометрии экрана и соотношения сторон. Если 'Принудительная установка' не включена, значения X/Y будут изменяться независимо друг от друга."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
      "Вывод скриншотов с затененным материалом с помощью GPU, если доступно."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
      "Принудительно задает определенный поворот экрана. Он добавляется к поворотам, уже заданным ядром."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
      "Принудительно отключить поддержку sRGB FBO. Некоторые драйвера Intel OpenGL на Windows испытывают проблемы с включенной поддержкой sRGB FBO. Включение этой настройки может обойти эту проблему."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
      "Запускать в полноэкранном режиме."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
      "Включить полноэкранный режим. Советуем использовать оконный полноэкранный режим."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
      "Записывает вывод с затененным материалом с помощью GPU, если доступно."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
      "Сохранение производится в следующий индекс. Загрузка будет произведена из сохранения с высшим индексом."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
      "Блокировать сохранение ОЗУ от перезаписи при загрузке сохранений. Может привести к багам в играх."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
      "Максимальная скорость перемотки контента (например, 5.0x для 60 FPS, т.е. ограничение 300 FPS). Если установлен на 0.0x - скорость не ограничена."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
      "При снижении скорости контент будет замедляться в соответствии с заданными факторами/настройками."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_REWIND_ENABLE,
      "Включить перемотку. Это может снижать производительность при воспроизведении."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
      "При перемотке на определенное количество кадров, вы можете перематывать несколько кадров сразу, увеличивая скорость перемотки."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
      "Устанавливает уровень ведения журнала для ядер. Если уровень ведения журнала, заданный ядром, ниже его значения, то он игнорируется."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
      "Включить счетчики производительности для RetroArch (и ядер)."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
      "Автоматически сохранять игру при выходе. RetroArch загрузит это сохранение, если включена 'Автозагрузка'."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
      "При запуске игры загружать автоматически сохраненное состояние."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
      "Показывать миниатюры файлов сохранений в меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
      "Автоматически сохраняет состояние через заданный промежуток времени. Интервал измеряется в секундах. Значение 0 отключает автосохранение."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
      "Если включено, перезаписывает привязки ввода переназначенными для текущего ядра."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
      "Включить автоматическое обнаружение ввода. Будет производена попытка автоматической настройки геймпадов Plug-and-Play."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
      "Поменять местами кнопки OK/Отмена. Отключено — японское расположение кнопок, включено — западное расположение."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
      "Если включено, контент будет запущен в фоне при включении меню RetroArch."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
      "Используемый видеодрайвер."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
      "Используемый аудиодрайвер."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_DRIVER,
      "Используемый драйвер ввода. Может быть принудительно изменен видеодрайвером."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
      "Используемый драйвер геймпада."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
      "Используемый драйвер аудиоресэмплера."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
      "Используемый драйвер камеры."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
      "Используемый драйвер местоположения."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_DRIVER,
      "Используемый драйвер меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_RECORD_DRIVER,
      "Используемый драйвер записи."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_WIFI_DRIVER,
      "Используемый драйвер Wi-Fi."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
      "Фильтровать файлы в файловом менеджере по поддерживаемым расширениям."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
      "Выберите изображение для установки в качестве обоев меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
      "Динамически загружать новые обои в зависимости от контекста."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
      "Перебивать аудиоустройство по умолчанию, используя аудиодрайвер. Эта настройка зависит от драйвера."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
      "Аудиоплагин DSP, обрабатывающий аудио до отправки драйверу."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
      "Частота дискретизации аудиовывода."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
      "Непрозрачность всех элементов пользовательского интерфейса наложения."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
      "Масштаб всех элементов пользовательского интерфейса наложения."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
      "Включить наложение."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
      "Выберите наложение из браузера файлов."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
      "Адрес хоста для подключения."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
      "Порт IP-адреса хоста. Может быть порт TCP или UDP."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
      "Пароль для подключения к Netplay-хосту. Используется только в режиме хоста."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
      "Анонсировать игру netplay публично. Если не установлено, клиенту придется подключаться вручную, не используя публичное лобби."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
      "Пароль для подключения к хосту netplay только с правами наблюдателя. Используется только в режиме хоста."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
      "Начинать netplay в режиме наблюдателя."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
      "Разрешать подключения в режиме slave. Клиенты в режиме slave требуют немного вычислительной мощности со своей стороны, но задержка сети может значительно увеличиться."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
      "Запрещать подключения не в режиме slave. Не рекомендуется, за исключением очень быстрых сетей с очень слабыми машинами."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
      "Запускать netplay в режиме, не требующем сохранения. Если включено, потребуется очень быстрая сеть, но netplay будет без заиканий, поскольку нет повторов."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
      "Частота кадров, с которой netplay будет проверять синхронизацию хоста и клиента."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
      "В режиме хоста, пытаться слушать подключения из публичного Интернета, используя UPnP или похожие технологии для избежания локальных сетей."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
      "Включить интерфейс команды stdin."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
      "Включить поддержку мыши в главном меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_POINTER_ENABLE,
      "Включить поддержку touch в главном меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_THUMBNAILS,
      "Вид обложки к играм."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
      "Показывать текущую дату и/или время в меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
      "Показывать текущий уровень заряда батареи в меню."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
      "Переход к началу и/или концу списка, если достигнута горизонтальная или вертикальная граница списка."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
      "Запустить netplay на хосте (сервере)."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
      "Включить netplay в режиме клиента.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
      "Отключить активное соединение Netplay.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
      "Сканирует каталог для поиска совместимых файлов.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_FILE,
      "Сканирует совместимый файл.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
      "Использует заданный интервал обновления для вертикальной синхронизации. Установите для эффективной частоты обновления монитора."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
      "Сортировать карты памяти в каталогах, названные после использования ядра."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
      "Сортировать файлы сохранений в каталогах, названные после использования ядра."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
      "URL каталога обновлений ядра на билдботе Libretro.")
MSG_HASH(MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
      "URL каталога обновлений содержимого на билдботе Libretro.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
      "После загрузки, автоматически извлекать файлы из загруженных архивов."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
      "Сканировать для поиска новых комнат.")
MSG_HASH(MENU_ENUM_SUBLABEL_DELETE_ENTRY,
      "Удалить эту запись из плейлиста.")
MSG_HASH(MENU_ENUM_SUBLABEL_INFORMATION,
      "Просмотреть больше информации о содержимом.")
MSG_HASH(MENU_ENUM_SUBLABEL_RUN,
      "Запустить содержимое.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
      "Задает настройки файлового менеджера.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
      "Включить измененное управление по умолчанию при запуске."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
      "Включить измененную конфигурацию по умолчанию при запуске."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
      "Включить измененные настройки ядра по умолчанию при запуске.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_ENABLE,
      "Показывать название ядра в главном меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
      "Просмотр баз данных.")
MSG_HASH(MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
      "Просмотр предыдущих запросов.")
MSG_HASH(MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
      "Сделать скриншот экрана.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
      "Выход из игры. Все несохраненные файлы, возможно, пропадут."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_STATE,
      "Загрузить сохранение из текущего выбранного слота.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_STATE,
      "Сохранить в текущий выбранный слот.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESUME,
      "Продолжить контент, запущенный в данный момент, и покинуть быстрое меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESUME_CONTENT,
      "Продолжить контент, запущенный в данный момент, и покинуть быстрое меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_STATE_SLOT,
      "Изменяет выбранный слот сохранения.")
MSG_HASH(MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
      "При загруженном сохранении, контент изменит свое состояние на предшествующее загруженному.")
MSG_HASH(MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
      "Если сохранение было перезаписано, будет произведен откат на предшествующее.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
      "Сервис Retro Achievements. Для дополнительной информации, посетите страницу http://retroachievements.org"
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
      "Управляет настроенными профилями."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
      "Управление настройками перемотки.")
MSG_HASH(MENU_ENUM_SUBLABEL_RESTART_CONTENT,
      "Перезапустить игру.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
      "Сохраняет файл настроек, который будет применен ко всему контенту, загруженному с этим ядром. Он будет иметь более высокий приоритет над основной конфигурацией.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
      "Сохраняет файл настроек, который будет применен только к текущему контенту. Он будет иметь более высокий приоритет над основной конфигурацией.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
      "Настроить чит-коды.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
      "Настроить шейдеры для визуального улучшения изображения.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
      "Изменить управление для контента, запущенного в данный момент.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_OPTIONS,
      "Изменить настройки для контента, запущенного в данный момент.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
      "Показывать расширенные настройки для опытных пользователей (скрыто по умолчанию).")
MSG_HASH(MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
      "Выполнять задачи в отдельном потоке.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
      "Разрешить пользователю удалять отдельные записи из плейлистов.")
MSG_HASH(MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
      "Устанавливает каталог System. Ядра могут запрашивать его для загрузки BIOS, прошивок, системных настроек и т.д.")
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
      "Установить начальный каталог для файлового браузера.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CONTENT_DIR,
      "Обычно настраивается разработчиками, составляющими комплекты приложений libretro/RetroArch для указания содержимого."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
      "Каталог для хранения обоев, динамически загружающихся в меню в зависимости от контекста.")
MSG_HASH(MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
      "Дополнительные миниатюры (бокс-арты/другие изображения и т.д.) хранятся здесь."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
      "Каталог с настройками.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
      "Количество кадров задержки ввода для netplay для сокрытия задержки сети. Уменьшает лаги и делает netplay менее требовательным к CPU, ценой значительной задержки ввода.")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
      "Диапазон кадров задержки ввода для netplay для сокрытия задержки сети. Уменьшает лаги и делает netplay менее требовательным к CPU, ценой непредсказуемой задержки ввода.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_CYCLE_TRAY_STATUS,
      "Зацикливать текущий диск. Если вставлен диск, он будет извлечен. Если диск не вставлен, он будет вставлен.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_INDEX,
      "Изменить индекс диска.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_OPTIONS,
      "Управление образа диска.")
MSG_HASH(MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
      "Выберите образ диска для загрузки.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
      "Убедитесь, что частота кадров ограничена внутри меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_LAYOUT,
      "Select a different layout for the XMB interface.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_THEME,
      "Выберите другую тему для значка. Изменения заработают после перезагрузки.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
      "Включить тени для всех значков. Это приведет к незначительной нагрузке.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
      "Выберите другую тему цветового градиента.")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
      "Изменить прозрачность обоев интерфейса.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
      "Выберите другую тему цветового градиента.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
      "Выберите анимированный фоновый эффект. Может быть требовательным к GPU в зависимости от эффекта. Если производительность не тянет на это, то либо выключите это, либо вернитесь к более простому эффекту.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_FONT,
      "Выбрать основной шрифт для меню интерфейса.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
      "Показать вкладку Изображения в главном меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
      "Показать вкладку Музыка в главном меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
      "Показать вкладку Видео в главном меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
      "Показать вкладку Сетевая игра в главном меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
      "Показать вкладку Настройка в главном меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
      "Показать вкладку История просмотров в главном меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
      "Показать вкладку Импорт контента в главном меню.")
MSG_HASH(MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
      "Показать начальный экран. Этот параметре автомотически выключается после первого включения.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
      "Изменение прозрачность графического заголовка.")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
      "Изменение прозрачности графика нижнего колонтитула.")
MSG_HASH(MENU_ENUM_SUBLABEL_DPI_OVERRIDE_ENABLE,
      "Меню интерфейса обычно сам подстраивается. Если вы хотите изменить DPI под свой вкус, то включите эту функцию.")
MSG_HASH(MENU_ENUM_SUBLABEL_DPI_OVERRIDE_VALUE,
      "Установить свой размер масштабирования. ПРИМЕЧАНИЕ: Вы должны включить 'DPI Override', чтобы эти изменения заработали.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
      "Сохранять все закаченные файлы в этой папке.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
      "Сохранить все переназначенные элементы управления в этой папке.")
MSG_HASH(MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
      "Местоположение ядер.")
MSG_HASH(MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
      "Здесь хранятся файлы информации контента/ядра.")
MSG_HASH(MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
      "Если игровой джойстик подключен, то джойстик будет автомотически подстроен, если существует подходящий файл автоматической настройки.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
      "Сохранять все плейлисты в выбранной папке.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
      "Контент извлеченный из архивов будет временно размещен в этой папке."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
      "Сохраненные запросы находятся в этом каталоге.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
      "Базы данных находятся в этом каталоге."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
      "Это местоположение запрашивается по умолчанию, когда интерфейсы меню пытаются найти загружаемые ресурсы (ассеты) и т.д."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
      "Выберите папку для внутриигровых сохранений. Если путь к папке не задан, то будет использован рабочий каталог.")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
      "Выберите папку для сохрания состояний. Если путь к папке не задан, то будет использован рабочий каталог.")
MSG_HASH(MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
      "Выберите папку, где будут сохраняться ваши скриншоты.")
MSG_HASH(MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
      "Выберите папку, где будут находиться ваши наложения (Overlays).")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
      "Выберите папку, где будут находиться ваши чит-файлы."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
      "Выберите папку, где будут находиться файлы звуковых фильтр DSP."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
      "Выберите папку, где будут находиться фильтры, обрабатываемые GPU."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
      "Выберите папку, где будут находиться шейдеры, обрабатываемые GPU.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
      "Выберите папку, где будут сохраняться ваши записи.")
MSG_HASH(MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
      "Выберите папку, где будут находиться ваши записи конфигураций.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
      "Выберите шрифт для красивого отображения оповещений.")
MSG_HASH(MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
      "Изменения конфигурации шейдера вступят в силу немедленно. Используйте это, если вы изменили количество используемых шейдеров, фильтров, шкалу и т.д.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
      "Повышайте или уменьшайте кол-во используемых шейдеров pipeline. Вы можете привязать отдельный шейдер к каждому pipeline конвейера и настроить его масштаб и фильтрацию."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
      "Выберите предустановленный шейдер. Pipeline шейдера будет автоматически настроен.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
      "Сохраните текущие настройки шейдера в качестве нового предустановленного шейдера.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
      "Сохраните текущие настройки шейдера как настройки по умолчанию для этого игры/ядра.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
      "Сохраните текущие настройки шейдера в качестве настроек по умолчанию для игры.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
      "Изменяет текущий шейдер напрямую. Изменения не будут сохранены в файл предварительной настройки.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
      "Изменяет настройки шейдера, которая в настоящее время используется в меню.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
      "Увеличение или уменьшение количества используемых чит-кодов."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
      "Изменения чит-кодов заработают немедленно.")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
      "Открыть чит-файл."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
      "Сохранить ниже указанные чит-коды в чит-файл."
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
      "Быстрый доступ к настройкам игры.")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_INFORMATION,
      "Просмотреть полную информаию о ядре + удалить ядро.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
      "Значение плавающей точки для соотношения сторон видео (ширина / высота), используется, если для параметра «Соотношение сторон» установлено значение «Config».")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
      "Настройка высоты окна экрана, которая используется, если для параметра «Соотношение сторон» установлено значение «Пользовательский».")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
      "Настройка ширины окна экрана, которая используется, если для параметра «Соотношение сторон» установлено значение «Пользовательский».")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
      "Смещение окна экрана для определения положения оси X окна просмотра. Она игнорируются, если включена функция «Целочисленная шкала». Оно будет автоматически центрирован.")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
      "Настройка смещения окна экрана для определения положения оси Y окна просмотра. Она игнорируются, если включена функция «Целочисленное масштабирование». Он будет автоматически центрирован.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
      "Использовать релейный сервер")
MSG_HASH(MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
      "Передать сетевые соединения через сервер «man-in-the-midle». Будет полезным, если хост находится за брандмауэром или имеет проблемы NAT / UPnP.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
      "Добавить в микшер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
      "Add to mixer and play")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
      "Добавить в микшер")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
      "Add to mixer and play")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
      "Фильтрировать по текущему ядру")
MSG_HASH(MSG_AUDIO_MIXER_VOLUME,
      "Общий уровень громкости звукового микшера")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
      "Громкость звукового микшера (в дБ). По умолчанию стоит 0 дБ и коэффициент усиления не применяется.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
      "Уровень громкости звукового микшера (дБ)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
      "Отключить звуковой микшер")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
      "Отключить / Включить звуковой микшер.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
      "Показать меню Онлайн-обновление")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
      "Показать/скрыть опцию 'Онлайн-обновление'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
      "Элементы главного экрана")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
      "Показать или скрыть элементы на экране меню.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
      "Показать меню Обновление ядра")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
      "Показать/скрыть возможность обновлять ядра (и информационные файлы ядер).")
MSG_HASH(MSG_PREPARING_FOR_CONTENT_SCAN,
      "Подготовка к сканированию...")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CORE_DELETE,
      "Удалить ядро")
MSG_HASH(MENU_ENUM_SUBLABEL_CORE_DELETE,
      "Удаление ядра с устройства.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
      "Переименовать игру")
MSG_HASH(MENU_ENUM_LABEL_RENAME_ENTRY,
      "Переименовать")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
      "Прозрачность кадра")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
      "Модификация прозрачности кадров.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
      "Избранные")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
      "Любой контент, который вы добавите в «Избранное», появится в этом списке.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
      "Музыка")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_MUSIC,
      "Любая музыка, которая была ранее прослушана, появится в этом разделе.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
      "Фотографии")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_IMAGES,
      "Любая фотография, которая ранее была просмотрена, появится в этом разделе.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
      "Видео")
MSG_HASH(MENU_ENUM_SUBLABEL_GOTO_VIDEO,
      "Любое видео, которое ранее было просмотрено, появится в этом разделе.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
      "Иконки интерфейса")
MSG_HASH(MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
      "Отображать или скрывать иконки интерфейса, отображающиеся в левой части меню.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
      "Включить вкладку Настройки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
      "Задать пароль для включения вкладки Настройки")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
      "Введите пароль")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
      "Пароль верен.")
MSG_HASH(MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
      "Пароль неверен.")
MSG_HASH(MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
      "Включает вкладку Настройки. Для появления вкладки требуется перезапуск.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
      "Применение пароля при скрытии вкладки Настройки может позже восстановить ее из меню. Для этого нужно перейти на вкладку Главное меню, выбрать Включить вкладку Настройки и ввести пароль.")
MSG_HASH(MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
      "Разрешить пользователю переименовывать записи в плейлистах.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
      "Разрешить переименовывать записи")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
      "Показать Загрузить ядро")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
      "Показать/скрыть настройку 'Загрузить ядро'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
      "Показать Загрузить содержимое")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
      "Показать/скрыть настройку 'Загрузить содержимое'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
      "Показать Информация")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
      "Показать/скрыть настройку 'Информация'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
      "Показать Настройки")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
      "Показать/скрыть настройку 'Настройки'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
      "Показать Помощь")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
      "Показать/скрыть настройку 'Помощь'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
      "Показать Выйти из RetroArch")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
      "Показать/скрыть настройку 'Выйти из RetroArch'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
      "Показать Перезагрузить")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
      "Показать/скрыть настройку 'Перезагрузить'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
      "Show Shutdown")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
      "Show/hide the 'Shutdown' option.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
      "Быстрое меню")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
      "Показать/скрыть элементы на экране быстрого меню.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
      "Показать Сделать скриншот")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
      "Показать/скрыть настройку 'Сделать скриншот'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
      "Показать Сохранить/Загрузить")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
      "Показать/скрыть настройки для сохранения/загрузки.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
      "Показать отменить сохранение/загрузку")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
      "Показать/скрыть настройки для отмены сохранения загрузки.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
      "Показать Добавить в избранное")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
      "Показать/скрыть настройку 'Добавить в избранное'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
      "Show Start Recording")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
      "Show/hide the 'Start Recording' option.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
      "Show Start Streaming")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
      "Show/hide the 'Start Streaming' option.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
      "Show Reset Core Association")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
      "Show/hide the 'Reset Core Association' option.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
      "Показать Настройки")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
      "Показать/скрыть настройку 'Настройки'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
      "Показать Управление")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
      "Показать/скрыть настройку 'Управление'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
      "Показать Чит-коды")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
      "Показать/скрыть настройку 'Чит-коды'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
      "Показать Шейдеры")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
      "Показать/скрыть настройку 'Шейдеры'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
      "Показать Сохранить переопределения ядра")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
      "Показать/скрыть настройку 'Сохранить переопределения ядра'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
      "Показать Сохранить переопределения игры")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
      "Показать/скрыть настройку 'Сохранить переопределения игры.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
      "Показать Информация")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
      "Показать/скрыть настройку 'Информация'.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
      "Отключить режим киоска")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
      "Отключает режим киоска. Для достижения нужного эффекта требуется перезапуск.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
      "Включить режим киоска")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
      "Защищает конфигурацию путем скрытия всех настроек. Необходим перезапуск.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
      "Задать пароль для отключения режима киоска")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
      "Для отключения режима киоска перейдите в главное меню, выберите 'Отключить режим киоска' и введите пароль.")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD,
      "Введите пароль")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
      "Пароль верен.")
MSG_HASH(MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
      "Пароль неверен.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
      "Автоматически добавлять контент в плейлист")
MSG_HASH(MENU_ENUM_SUBLABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
      "Автоматически сканировать загруженный контент и добавлять его в плейлист.")
MSG_HASH(MSG_SCANNING_OF_FILE_FINISHED,
      "Сканирование файла завершено")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
      "Качество аудио ресемплера")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
      "Уменьшение значения повлечет ухудшение качества звука, но уменьшение задержки и улучшение производительности.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
      "Отобразить статистику")
MSG_HASH(MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
      "Отображать на экране техническую статистику.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
      "Отображать настройки перемотки")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
      "Показать/скрыть настройки перемотки.")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
      "Показать/скрыть настройки задержек.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
      "Показать настройки задержек")
MSG_HASH(MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
      "Показать/скрыть настройки оверлея.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
      "Показать настройки оверлея")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
      "Включить звук в меню")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
      "Включить или выключить звук в меню.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
      "Настройки микшера")
MSG_HASH(MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
      "Посмотреть и/или изменить настройки микшера.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
      "Переопределения")
MSG_HASH(MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
      "Опции для переопределения глобальной конфигурации.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
      "Запустит воспроизведение звукового потока. После завершения удалит звуковой поток из памяти.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
      "Запустит воспроизведение звукового потока. После окончания воспроизведет трек с начала.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
      "Запустит воспроизведение звукового потока. После окончания будет последовательно запускать звуковые потоки, а затем воспроизведет первый поток.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
      "Остановит аудио поток но не удалит его из памяти. Вы можете воспроизвести его снова выбрав 'Воспроизвести'.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
      "Остановит аудио поток и удалит его из памяти.")
MSG_HASH(MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
      "Изменить громкость аудио потока.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
      "Добавить этот трек в доступный аудио слот. ")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
      "Добавить этот трек в доступный аудио слот и воспроизвести его.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
      "Воспроизвести")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
      "Воспроизвести (Зацикленно)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
      "Воспроизвести (Последовательно)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
      "Стоп")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
      "Удалить")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
      "Громкость")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
      "Текущее ядро")
MSG_HASH(MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
      "Очистить")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
      "In-Menu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
      "In-Game")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
      "In-Game (Paused)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
      "Playing")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
      "Paused")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
      "Включить Discord")
MSG_HASH(MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
      "Включить или отключить поддержку Discord. Работает только с нативным клиентом.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
      "Задержка")
MSG_HASH(MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
      "Изменить настройки относящиеся к задержке видео, аудио и ввода.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
      "Обгон для уменьшения задержки")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
      "Количество кадров для обгона")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
      "Использовать вторую инстанцию для обгона")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
      "Убрать уведомления об обгоне")
MSG_HASH(
      MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
      "Обрабатывать логику ядра перед рендером 1 или более кадров для уменьшения задержки ввода."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
      "Количество кадров обгона. Может вызвать дрожание при большом значении."
      )
MSG_HASH(
      MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
      "Скрывает предупреждение при использовании обгона когда ядро не поддерживает сохранения."
      )

MSG_HASH(
      MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
      "Запускать вторую копию RetroArch для обгона. Исправляет проблемы со звуком во время загрузки."
      )

MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
      "Включить заполнитель границ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
      "Включить толщину заполнителя границ")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
      "Включить фон заполнителя границ")
MSG_HASH(MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
      "Только для CRT дисплеев. Пытается использовать точное разрешение и частоту обновления ядра/игры.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
      "CRT SwitchRes")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
      "Switch among native and ultrawide super resolutions."
      )
MSG_HASH(MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
      "CRT Super Resolution")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
      "Показывать клавиатуру/контроллер на оверлее.")
MSG_HASH(MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
      "Select the port for the overlay to listen to if Show Inputs On Overlay is enabled.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
      "Показывать управление на оверлее")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
      "Показать прослушиваемый порт управления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
      "Отображать количество кадров вместе с FPS")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
      "Красный цвет уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
      "Зеленый цвет уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
      "Синий цвет уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
      "Фон уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
      "Красный цвет фона уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
      "Зеленый цвет фона уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
      "Синий цвет фона уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
      "Непрозрачность фона уведомления")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
      "Добавить в избранное")
MSG_HASH(MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
      "Добавить в избранное")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
      "Добавить пункт в избранное.")
MSG_HASH(MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
      "Добавить пункт в избранное.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
      "Добавить переопределение для директории")
MSG_HASH(MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
      "Сохраняет файл настроек, который будет применен только к контенту в текущей директории. Он будет иметь более высокий приоритет над основной конфигурацией.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
      "Наблюдать за изменениями шейдеров")
MSG_HASH(MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
      "Автоматически применять изменения внесенные в шейдер.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
      "Сохранить пресет для контента в директории")
MSG_HASH(MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
      "Применяет текущие настройки шейдера для всего контента в текущей директории.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
      "Сохранить переопределение ввода для директории")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
      "Записывать сохранения в директорию с контентом")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
      "Записывать внутриигровые сохранения в директорию с контентом")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
      "Системные файлы в директории с контентом")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
      "Записывать скриншоты в директорию с контентом")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
      "Избранные")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
      "Непрозрачность окна")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
      "Показывать оформление окна")
MSG_HASH(MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
      "Забыть использованное ядро")
MSG_HASH(MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
      "Отображать меню на рабочем столе при запуске")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
      "Отображать меню на рабочем столе (перезапуск)")
MSG_HASH(MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
      "Местоположение релейного сервера")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
      "Включить замедленное движение")
MSG_HASH(MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
      "Включить меню рабочего стола")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
      "Таблица лидеров"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
      "Значки достижений"
      )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
      "Автоматический скриншот"
      )
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
      "Включить или выключить внутриигровую таблицу лидеров. Не работает в хардкорном режиме.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
      "Включить или выключить отображение значков в списке достижений.")
MSG_HASH(MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
      "Автоматически сделать скриншот при получении достижения.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
      "Power Management")
MSG_HASH(MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
      "Change power management settings.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
      "Sustained Performance Mode")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
      "mpv support")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
    "Adaptive Vsync"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
    "V-Sync is enabled until performance falls below the target refresh rate. Can minimize stuttering when performance falls below realtime, and can be more energy efficient."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
    "CRT SwitchRes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
    "Output native, low-resolution signals for use with CRT displays."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
    "Cycle through these options if the image is not centered properly on the display."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
    "X-Axis Centering"
    )
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
      "Use a custom refresh rate specified in the config file if needed.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
      "Use Custom Refresh Rate")
MSG_HASH(
      MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
      "Select the output port connected to the CRT display.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
      "Output Display ID")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
    "Start Recording"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
    "Starts recording."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
    "Stop Recording"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
    "Stops recording."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
    "Start Streaming"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
    "Starts streaming."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
    "Stop Streaming"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
    "Stops streaming."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
    "Recording toggle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
    "Streaming toggle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
    "Record Quality"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
    "Stream Quality"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_URL,
    "Streaming URL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
    "UDP Stream Port"
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
      "Twitch Stream Key")
MSG_HASH(MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
      "YouTube Stream Key")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
      "Streaming Mode")
MSG_HASH(MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
      "Title of Stream")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
    "Split Joy-Con"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
    "Reset To Defaults"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
    "Reset the current configuration to default values."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
    "OK"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
      "Цветовая тема меню")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
    "Basic White"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
    "Basic Black"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
    "Select a different color theme."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "Use preferred system color theme")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
      "Use your operating system's color theme (if any) - overrides theme settings.")
MSG_HASH(MSG_RESAMPLER_QUALITY_LOWEST,
      "Lowest")
MSG_HASH(MSG_RESAMPLER_QUALITY_LOWER,
      "Lower")
MSG_HASH(MSG_RESAMPLER_QUALITY_NORMAL,
      "Normal")
MSG_HASH(MSG_RESAMPLER_QUALITY_HIGHER,
      "Higher")
MSG_HASH(MSG_RESAMPLER_QUALITY_HIGHEST,
      "Highest")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
    "No music available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
    "No videos available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
    "No images available."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
    "No favorites available."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
      "Remember Window Position and Size")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
    "CoreAudio support"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
    "CoreAudio V3 support"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
      "Menu Widgets")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
      "Video Shaders")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
      "Scan without core match")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
      "When disabled, content is only added to playlists if you have a core installed that supports its extension. By enabling this, it will add to playlist regardless. This way, you can install the core you need later on after scanning.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
      "Animation Horizontal Icon Highlight")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
      "Animation Move Up/Down")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
      "Animation Main Menu Opens/Closes")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
    "Disc Information"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISC_INFORMATION,
    "View information about inserted media discs."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
    "Frontend Logging Level"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
    "Sets log level for the frontend. If a log level issued by the frontend is below this value, it is ignored."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
      "Framerate Update Interval (in frames)")
MSG_HASH(MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
      "Framerate display will be updated at the set interval (in frames).")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Show Restart Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Show/hide the 'Restart Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Show Close Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Show/hide the 'Close Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Show Resume Content"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Show/hide the 'Resume Content' option."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
    "Show Input"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
    "Show or hide 'Input Settings' on the Settings screen."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
    "AI Service"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
    "Change settings for the AI Service (Translation/TTS/Misc)."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
      "AI Service Output")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
      "AI Service URL")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
      "AI Service Enabled")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
      "Pauses gameplay during translation (Image mode), or continues to run (Speech mode)")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
      "A http:// url pointing to the translation service to use.")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
      "Enable AI Service to run when the AI Service hotkey is pressed.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
      "Target Language")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
      "The language the service will translate to. If set to 'Don't Care', it will default to English.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
      "Source Language")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
      "The language the service will translate from. If set to 'Don't Care', it will attempt to auto-detect the language. Setting it to a specific language will make the translation more accurate.")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CZECH,
    "Czech"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DANISH,
    "Danish"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWEDISH,
   "Swedish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CROATIAN,
   "Croatian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CATALAN,
   "Catalan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN,
   "Bulgarian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BENGALI,
   "Bengali"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BASQUE,
   "Basque"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI,
   "Azerbaijani"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN,
   "Albanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS,
   "Afrikaans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN,
   "Estonian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FILIPINO,
   "Filipino"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FINNISH,
   "Finnish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GALICIAN,
   "Galician"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN,
   "Georgian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GUJARATI,
   "Gujarati"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE,
   "Haitian Creole"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
   "Hebrew"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HINDI,
   "Hindi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN,
   "Hungarian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC,
   "Icelandic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN,
   "Indonesian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_IRISH,
   "Irish"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_KANNADA,
   "Kannada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATIN,
   "Latin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATVIAN,
   "Latvian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN,
   "Lithuanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN,
   "Macedonian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALAY,
   "Malay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALTESE,
   "Maltese"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN,
   "Norwegian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_PERSIAN,
   "Persian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN,
   "Romanian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SERBIAN,
   "Serbian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVAK,
   "Slovak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN,
   "Slovenian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWAHILI,
   "Swahili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TAMIL,
   "Tamil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TELUGU,
   "Telugu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_THAI,
   "Thai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN,
   "Ukrainian"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_URDU,
   "Urdu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_WELSH,
   "Welsh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_YIDDISH,
   "Yiddish"
   )
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_DISC,
      "Load a physical media disc. You should first select the core (Load Core)  you intend to use with the disc.")
MSG_HASH(MENU_ENUM_SUBLABEL_DUMP_DISC,
      "Dump the physical media disc to internal storage. It will be saved as an image file.")
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Image Mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Speech Mode"
   )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
      "Remove")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
      "Remove shader presets of a specific type.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Remove Global Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Remove the Global Preset, used by all content and all cores.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Remove Core Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Remove the Core Preset, used by all content ran with the currently loaded core.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Remove Content Directory Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Remove the Content Directory Preset, used by all content inside the current working directory.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Remove Game Preset")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Remove the Game Preset, used only for the specific game in question.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
      "Frame Time Counter")
MSG_HASH(MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
      "Adjust settings influencing the frame time counter (only active when threaded video is disabled).")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
      "Use modern decorated animations, notifications, indicators and controls instead of the old text only system.")

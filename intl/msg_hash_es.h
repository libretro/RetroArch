#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
    "Overclockear GPU"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE,
    "Ajusta la velocidad de la GPU."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_BACKLIGHT_CONTROL,
    "Brillo de pantalla"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_BACKLIGHT_CONTROL,
    "Ajusta el brillo de la pantalla."
    )
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
    "Overclockear CPU"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
    "Acelera la CPU."
    )
#endif
MSG_HASH(
    MSG_COMPILER,
    "Compilador"
    )
MSG_HASH(
    MSG_UNKNOWN_COMPILER,
    "Compilador desconocido"
    )
MSG_HASH(
    MSG_NATIVE,
    "Nativo"
    )
MSG_HASH(
    MSG_DEVICE_DISCONNECTED_FROM_PORT,
    "Dispositivo desconectado del puerto"
    )
MSG_HASH(
    MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
    "Se ha recibido un comando de juego en red desconocido"
    )
MSG_HASH(
    MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
    "El archivo ya existe. Guardándolo en el búfer de respaldo"
    )
MSG_HASH(
    MSG_GOT_CONNECTION_FROM,
    "Conectado con: \"%s\""
    )
MSG_HASH(
    MSG_GOT_CONNECTION_FROM_NAME,
    "Conectado con: \"%s (%s)\""
    )
MSG_HASH(
    MSG_PUBLIC_ADDRESS,
    "Asignación de puertos completada"
    )
MSG_HASH(
    MSG_UPNP_FAILED,
    "Error al asignar puertos"
    )
MSG_HASH(
    MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
    "No se pasaron argumentos y no hay menú integrado, mostrando ayuda..."
    )
MSG_HASH(
    MSG_SETTING_DISK_IN_TRAY,
    "Introduciendo disco en bandeja"
    )
MSG_HASH(
    MSG_WAITING_FOR_CLIENT,
    "Esperando al cliente..."
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
    "Has abandonado la partida"
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
    "Has accedido como el jugador %u"
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
    "Has accedido con el dispositivo de entrada %.*s"
    )
MSG_HASH(
    MSG_NETPLAY_PLAYER_S_LEFT,
    "El jugador %.*s ha abandonado la partida"
    )
MSG_HASH(
    MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
    "%.*s se ha unido como el jugador %u"
    )
MSG_HASH(
    MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
    "%.*s se ha unido con los dispositivos de entrada %.*s"
    )
MSG_HASH(
    MSG_NETPLAY_NOT_RETROARCH,
    "Una conexión de juego en red ha fallado porque el usuario no utilizaba RetroArch o utilizaba una versión antigua de RetroArch."
    )
MSG_HASH(
    MSG_NETPLAY_OUT_OF_DATE,
    "El cliente del juego en red utiliza una versión antigua de RetroArch. No se puede conectar con el cliente."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_VERSIONS,
    "ADVERTENCIA: Un cliente de juego en red está utilizando una versión diferente de RetroArch. En caso de problemas, se recomienda usar la misma versión."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_CORES,
    "Un cliente de juego en red está utilizando una versión distinta del núcleo. No se puede conectar con el cliente."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
    "ADVERTENCIA: Un cliente de juego en red está utilizando una versión distinta del núcleo. En caso de problemas, se recomienda usar la misma versión."
    )
MSG_HASH(
    MSG_NETPLAY_ENDIAN_DEPENDENT,
    "Este núcleo no es compatible con el juego en red entre diferentes arquitecturas de sistemas"
    )
MSG_HASH(
    MSG_NETPLAY_PLATFORM_DEPENDENT,
    "Este núcleo no es compatible con el juego en red entre diferentes sistemas"
    )
MSG_HASH(
    MSG_NETPLAY_ENTER_PASSWORD,
    "Introducir la contraseña del servidor de juego en red:"
    )
MSG_HASH(
    MSG_DISCORD_CONNECTION_REQUEST,
    "¿Quieres permitir que este usuario se conecte?"
    )
MSG_HASH(
    MSG_NETPLAY_INCORRECT_PASSWORD,
    "Contraseña incorrecta"
    )
MSG_HASH(
    MSG_NETPLAY_SERVER_NAMED_HANGUP,
    "\"%s\" se ha desconectado"
    )
MSG_HASH(
    MSG_NETPLAY_SERVER_HANGUP,
    "Un cliente de juego en red se ha desconectado"
    )
MSG_HASH(
    MSG_NETPLAY_CLIENT_HANGUP,
    "Te has desconectado del juego en red"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
    "No tienes permiso para jugar"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
    "No hay espacios para jugadores disponibles"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
    "Los dispositivos de entrada solicitados no están disponibles"
    )
MSG_HASH(
    MSG_NETPLAY_CANNOT_PLAY,
    "No se puede cambiar al modo de juego"
    )
MSG_HASH(
    MSG_NETPLAY_PEER_PAUSED,
    "El cliente de juego en red \"%s\" está pausado"
    )
MSG_HASH(
    MSG_NETPLAY_CHANGED_NICK,
    "Tu apodo pasa a ser a \"%s\""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
    "Otorga un contexto privado a los núcleos renderizados por hardware. Así se evita tener que asumir cambios en el estado del hardware entre fotogramas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
    "Activa la animación horizontal en el menú. Afectará al rendimiento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SETTINGS,
    "Ajusta la apariencia del menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
    "Fuerza la sincronía entre CPU y GPU. Reduce la latencia a costa del rendimiento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_THREADED,
    "Mejora el rendimiento a costa de perder latencia y tener algunos tirones. Usar solo si no puede obtener máxima velocidad de otra manera."
    )
MSG_HASH(
    MSG_AUDIO_VOLUME,
    "Volumen de audio"
    )
MSG_HASH(
    MSG_AUTODETECT,
    "Autodetectar"
    )
MSG_HASH(
    MSG_AUTOLOADING_SAVESTATE_FROM,
    "Cargando guardado rápido desde"
    )
MSG_HASH(
    MSG_CAPABILITIES,
    "Capacidades"
    )
MSG_HASH(
    MSG_CONNECTING_TO_NETPLAY_HOST,
    "Conectando al servidor de juego en red"
    )
MSG_HASH(
    MSG_CONNECTING_TO_PORT,
    "Conectando al puerto"
    )
MSG_HASH(
    MSG_CONNECTION_SLOT,
    "Lugar de conexión"
    )
MSG_HASH(
    MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
    "Lo sentimos, este sistema no está implementado: los núcleos que no requieren contenido no pueden participar en el juego en red."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
    "Contraseña"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
    "Cuenta de logros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
    "Nombre de usuario"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
    "Cuentas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
    "Fin de la lista de cuentas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
    "RetroAchievements"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
    "Logros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
    "Pausar el modo Hardcore de logros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
    "Reanudar el modo Hardcore de logros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
    "Logros (Hardcore)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
    "Buscar contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
    "Configuración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TAB,
    "Importar contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
    "Salas de juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
    "Preguntar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
    "Recursos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
    "Bloquear fotogramas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
    "Dispositivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
    "Audio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
    "Plugin DSP"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
    "Audio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
    "Filtro de audio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
    "Turbo/zona muerta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
    "Latencia de audio (ms)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
    "Variación máxima de sincronía de audio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
    "Silenciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
    "Frecuencia de muestreo (Hz)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
    "Control dinámico de frecuencia de audio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
    "Remuestreo de audio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
    "Audio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
    "Sincronización"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
    "Ganancia de volumen (dB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
    "Modo WASAPI exclusivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
    "Formato WASAPI de coma flotante"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
    "Tamaño del búfer compartido de WASAPI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
    "Intervalo de autoguardados SaveRAM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
    "Autocargar archivos de personalización"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
    "Autocargar archivos de reasignación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
    "Usar archivo de opciones globales del núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
    "Guarda toda la configuración de los núcleos en un archivo común (retroarch-core-options.cfg). Si se desactiva esta opción, se guardará la configuración de cada núcleo en una carpeta o archivo único por núcleo, dentro de la carpeta «Config» de RetroArch."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
    "Autocargar ajustes de shaders"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
    "Atrás"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
    "Confirmar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
    "Información"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
    "Salir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
    "Desplazar hacia abajo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
    "Desplazar hacia arriba"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
    "Iniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
    "Mostrar teclado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
    "Mostrar menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
    "Controles básicos del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
    "Confirmar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
    "Información"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
    "Salir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
    "Desplazar hacia arriba"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
    "Valores predeterminados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
    "Mostrar teclado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
    "Mostrar menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
    "No sobrescribir SaveRAM al cargar un guardado rápido"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
    "Activar Bluetooth"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
    "URL de recursos del Buildbot"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
    "Caché"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
    "Permitir cámara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
    "Cámara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT,
    "Truco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
    "Aplicar cambios"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
    "Buscar un truco nuevo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
    "Continuar búsqueda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
    "Archivo de trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
    "Archivo de trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
    "Cargar archivo de trucos (Reemplazar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
    "Cargar archivo de trucos (Añadir)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
    "Guardar archivo de trucos como"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
    "Pasadas de trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
    "Descripción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Modo Hardcore"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
    "Tablas de clasificación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
    "Insignias de logros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
    "Bloqueado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
    "No compatible"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
    "RetroAchievements"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
    "Probar logros no oficiales"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
    "No oficial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
    "Desbloqueado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
    "Hardcore"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
    "Modo informativo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
    "Capturar pantalla automáticamente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
    "Cerrar contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIG,
    "Configurar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
    "Cargar configuración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
    "Configuración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
    "Guardar configuración al salir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
    "Base de datos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
    "Contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
    "Tamaño del historial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
    "Tamaño del listado de favoritos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
    "Limita la cantidad de elementos de la lista de reproducción. Cuando se alcance el límite, no se podrán añadir más entradas hasta que se eliminen otras. Utiliza el valor -1 para tener un número «ilimitado» (99999) de entradas. ADVERTENCIA. ¡Si se reduce este valor, se borrarán las entradas ya existentes!"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
    "Permitir la eliminación de entradas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
    "Menú rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
    "Descargas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
    "Descargas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
    "Trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
    "Contadores de núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
    "Mostrar nombre del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
    "Información del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
    "Autores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
    "Categorías"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
    "Etiqueta del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
    "Nombre del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
    "Firmware(s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
    "Licencia(s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
    "Permisos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
    "Extensiones compatibles"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
    "Fabricante del sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
    "Nombre del sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
    "APIs gráficas necesarias"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
    "Controles"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_LIST,
    "Cargar núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
    "Instalar o restaurar un núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
    "Error al instalar el núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
    "El núcleo se ha instalado correctamente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
    "Opciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
    "Iniciar un núcleo automáticamente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
    "Extraer automáticamente los archivos descargados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
    "URL de núcleos de Buildbot"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
    "Actualizador de núcleos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
    "Actualizador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
    "Arquitectura de CPU:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CPU_CORES,
    "Núcleos de CPU:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
    "Cursor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
    "Gestor de cursores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
    "Relación personalizada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
    "Gestor de bases de datos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
    "Seleccionar bases de datos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
    "Eliminar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FAVORITES,
    "Carpeta inicial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
    "<Carpeta de contenido>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
    "<Predeterminada>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
    "<Ninguna>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
    "No se ha encontrado la carpeta."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
    "Carpeta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_INDEX,
    "Índice de disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
    "Control de disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DONT_CARE,
    "No importa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
    "Descargas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
    "Descargar núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
    "Descargador de contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
    "Actualizar núcleos instalados"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
    "Actualiza todos los núcleos instalados a sus últimas versiones."
    )
MSG_HASH(
    MSG_FETCHING_CORE_LIST,
    "Obteniendo lista de núcleos..."
    )
MSG_HASH(
    MSG_CORE_LIST_FAILED,
    "¡Error al obtener la lista de núcleos!"
    )
MSG_HASH(
    MSG_LATEST_CORE_INSTALLED,
    "Última versión ya instalada: "
    )
MSG_HASH(
    MSG_UPDATING_CORE,
    "Actualizando núcleo: "
    )
MSG_HASH(
    MSG_DOWNLOADING_CORE,
    "Descargando núcleo: "
    )
MSG_HASH(
    MSG_EXTRACTING_CORE,
    "Extrayendo núcleo: "
    )
MSG_HASH(
    MSG_CORE_INSTALLED,
    "Núcleo instalado: "
    )
MSG_HASH(
    MSG_SCANNING_CORES,
    "Escaneando núcleos..."
    )
MSG_HASH(
    MSG_CHECKING_CORE,
    "Comprobando núcleo: "
    )
MSG_HASH(
    MSG_ALL_CORES_UPDATED,
    "Todos los núcleos instalados están al día"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
    "Escala del menú"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
    "Aplica un valor global a la escala del menú. Sirve para aumentar o reducir el tamaño de la interfaz de usuario."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
    "Controladores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
    "Cargar vacío al cerrar núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
    "Comprobar si falta el firmware antes de cargar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
    "Fondo de pantalla dinámico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
    "Fondos de pantalla dinámicos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
    "Activar logros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FALSE,
    "Desactivado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
    "Velocidad máxima de ejecución"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
    "Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FPS_SHOW,
    "Mostrar FPS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
    "Incluir datos de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
    "Limitar velocidad máxima de ejecución"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
    "Sincronizar FPS al contenido (G-Sync, FreeSync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
    "Limitador de fotogramas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
    "Contadores de la interfaz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
    "Usar opciones específicas según el contenido y el núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
    "Crear archivo de opciones del juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
    "Guardar archivo de opciones del juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP,
    "Ayuda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
    "Solucionar problemas de audio/vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
    "Cambiar el mando virtual superpuesto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
    "Controles básicos del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_LIST,
    "Ayuda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
    "Cargando contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
    "Buscando contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
    "¿Qué es un núcleo?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
    "Listado del historial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
    "Historial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
    "Menú horizontal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
    "Imágenes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INFORMATION,
    "Información"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
    "Información"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
    "Tipo de analógico a digital"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
    "Todos los usuarios pueden controlar el menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
    "Analógico izq. X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
    "Analógico izq. X- (izquierda)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
    "Analógico izq. X+ (derecha)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
    "Analógico izq. Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
    "Analógico izq. Y- (arriba)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
    "Analógico izq. Y+ (abajo)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
    "Analógico der. X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
    "Analógico der. X- (izquierda)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
    "Analógico der. X+ (derecha)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
    "Analógico der. Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
    "Analógico der. Y- (arriba)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
    "Analógico der. Y+ (abajo)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
    "Arma: Gatillo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
    "Arma: Recargar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
    "Arma: Aux A"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
    "Arma: Aux B"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
    "Arma: Aux C"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
    "Arma: Start"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
    "Arma: Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
    "Arma: Cruceta arriba"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
    "Arma: Cruceta abajo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
    "Arma: Cruceta izquierda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
    "Arma: Cruceta derecha"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
    "Activar autoconfiguración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
    "Umbral de entrada de los ejes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
    "Zona muerta analógica"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
    "Sensibilidad analógica"
    )
#ifdef GEKKO
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
    "Escala del ratón"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
    "Ajusta la escala X/Y para la velocidad de las lightguns usando el mando de Wii."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
    "Menú: cambiar botones para Confirmar y Cancelar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
    "Asignar todo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
    "Asignar valores por defecto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
    "Asignar tiempo límite"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
    "Asignar (mantener)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
    "Asignar tiempo límite para bloquear"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
    "Ocultar descripciones de entrada sin asignar de los núcleos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
    "Mostrar etiquetas de descripción de entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
    "Índice del dispositivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
    "Tipo de dispositivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
    "Índice de ratón"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
    "Entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
    "Ciclo de trabajo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
    "Asignar teclas rápidas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
    "Activar asignación de teclado/mando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
    "Botón A (derecha)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
    "Botón B (abajo)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
    "Cruceta abajo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
    "Botón L2 (LT)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
    "Botón L3 (Pulsar analógico izq.)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
    "Botón L1 (LB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
    "Cruceta izquierda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
    "Botón R2 (RT)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
    "Botón R3 (Pulsar analógico der.)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
    "Botón R1 (RB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
    "Cruceta derecha"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
    "Botón Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
    "Botón Start"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
    "Cruceta arriba"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
    "Botón X (arriba)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
    "Botón Y (izquierda)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_KEY,
    "(Tecla: %s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
    "Ratón 1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
    "Ratón 2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
    "Ratón 3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
    "Ratón 4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
    "Ratón 5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
    "Rueda arriba"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
    "Rueda abajo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
    "Rueda izquierda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
    "Rueda derecha"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
    "Tipo de asignación de teclado/mando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
    "Número máximo de usuarios"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
    "Combinación para mostrar el menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
    "Índice de trucos -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
    "Índice de trucos +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
    "Activar truco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
    "Expulsar disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
    "Siguiente disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
    "Disco anterior"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
    "Activar teclas rápidas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
    "Mantener para avance rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
    "Avance rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
    "Avanzar fotograma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
    "Enviar datos de depuración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
    "Mostrar FPS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
    "Crear una partida de juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
    "Pantalla completa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
    "Capturar ratón"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
    "Prioridad al juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
    "Menú de escritorio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
    "Cargar guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
    "Mostrar menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
    "Grabar repetición de partida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
    "Silenciar audio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
    "Juego en red: cambiar modo juego/espectador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
    "Teclado en pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
    "Siguiente superposición"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
    "Pausar"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
    "Reiniciar RetroArch"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
    "Cerrar RetroArch"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
    "Reiniciar juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
    "Rebobinar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
    "Detalles de truco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
    "Iniciar o reanudar búsqueda de trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
    "Guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
    "Capturar pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
    "Siguiente shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
    "Shader anterior"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
    "Tecla a mantener para cámara lenta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
    "Cámara lenta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
    "Posición de guardado rápido anterior"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
    "Siguiente posición de guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
    "Bajar volumen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
    "Subir volumen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
    "Mostrar superposición"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
    "Ocultar superposición en el menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
    "Mostrar entradas en la superposición"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
    "Mostrar cursor del ratón en la superposición"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
    "Autorotar superposición"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
    "Si la superposición lo permite, rotarla para que coincida con la orientación y relación de aspecto de la pantalla."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
    "Puerto de escucha para entradas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
    "Sistema de sondeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
    "Temprano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
    "Tardío"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
    "Normal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
    "Preferir pantalla táctil frontal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
    "Reasignación de entrada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
    "Reasignar controles en este núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
    "Guardar autoconfiguración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
    "Controles"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
    "Activar teclado pequeño"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
    "Activar pantalla táctil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
    "Turbo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
    "Periodo del turbo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
    "Modo turbo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DEFAULT_BUTTON,
    "Botón predeterminado del turbo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
    "Controles del puerto %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
    "Latencia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
    "Estado del almacenamiento interno"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
    "Autoconfiguración de controles"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
    "Mando"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
    "Servicios"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
    "Chino (Simplificado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
    "Chino (Tradicional)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
    "Holandés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
    "Inglés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
    "Esperanto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
    "Francés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
    "Alemán"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
    "Italiano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
    "Japonés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
    "Coreano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_POLISH,
    "Polaco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL,
    "Portugués (Brasil)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL,
    "Portugués (Portugal)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
    "Ruso"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
    "Español"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE,
    "Vietnamita"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ARABIC,
    "Árabe"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GREEK,
    "Griego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_TURKISH,
    "Turco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
    "Analógico izquierdo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
    "Información del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
    "Nivel de registro del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LINEAR,
    "Lineal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
    "Cargar archivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
    "Cargar archivos recientes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
    "Selecciona un contenido de la lista de contenidos recientes."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
    "Cargar contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_DISC,
    "Cargar disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DUMP_DISC,
    "Volcar disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_STATE,
    "Cargar guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
    "Activar localización"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
    "Localización"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
    "Registros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
    "Verbosidad del registro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
    "Copiar registro a archivo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_TO_FILE,
    "Redirige los mensajes de registro del sistema a un archivo. La opción «Verbosidad del registro» debe estar activada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
    "Fechar archivos de registro"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
    "Al enviar los registros a un archivo, redirige los registros de cada sesión de RetroArch a un archivo nuevo, marcando su fecha. Al desactivar esta opción se sobrescribirá el archivo de registro cada vez que se reinicie RetroArch."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MAIN_MENU,
    "Menú principal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANAGEMENT,
    "Ajustes de bases de datos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
    "Color del tema del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
    "Azul"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
    "Azul gris"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
    "Azul oscuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
    "Verde"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
    "Shield"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
    "Rojo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
    "Amarillo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI,
    "Material UI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
    "Material UI (oscuro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
    "Ozone (oscuro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD,
    "Nórdico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
    "Gruvbox (oscuro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
    "Solarized (oscuro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
    "Cutie azul"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
    "Cutie turquesa"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
    "Cutie verde"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
    "Cutie naranja"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
    "Cutie rosa"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
    "Cutie morado"
    )
 MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
    "Cutie rojo"
    )
  MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY,
    "Virtual Boy"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
    "Transiciones de menú animadas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
    "Activa los efectos de animación al navegar entre las distintas opciones del menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
    "Automáticas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
    "Fundido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
    "Desplazamiento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
    "Desactivar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
    "Miniaturas en modo vertical"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
    "Especifica la forma de mostrar las miniaturas de las listas de reproducción con la orientación vertical."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
    "Miniaturas en modo horizontal"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
    "Especifica la forma de mostrar las miniaturas de las listas de reproducción con la orientación horizontal."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
    "Desactivar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
    "Lista (pequeña)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
    "Lista (mediana)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
    "Dos iconos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
    "Desactivar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
    "Lista (pequeña)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
    "Lista (mediana)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
    "Lista (grande)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
    "Opacidad del pie de página"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
    "Opacidad del encabezado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
    "Menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
    "Limitar FPS del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
    "Explorador de archivos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
    "Filtro lineal del menú"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
    "Suaviza ligeramente el menú para eliminar los bordes pixelados."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
    "Animación horizontal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
    "Apariencia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
    "Fondo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
    "Opacidad del fondo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MISSING,
    "Faltante"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MORE,
    "..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
    "Soporte para ratón"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
    "Multimedia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
    "Música"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
    "Filtrar extensiones desconocidas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
    "Navegación en bucle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NEAREST,
    "Más cercano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY,
    "Juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
    "Permitir clientes en modo esclavo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
    "Juego en red: comprobar fotogramas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
    "Latencia en fotogramas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
    "Rango de latencia en fotogramas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
    "Juego en red: retrasar fotogramas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
    "Desconectar del servidor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
    "Activar juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
    "Conectar a un servidor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
    "Iniciar servidor de juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
    "Terminar juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
    "Dirección del servidor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
    "Buscar red local"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
    "Cliente de juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
    "Nombre de usuario"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
    "Contraseña del servidor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
    "Anunciar servidor de forma pública"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
    "Solicitar dispositivo %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
    "Bloquear clientes sin modo esclavo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
    "Configurar juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
    "Compartir entrada analógica"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
    "Máx."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
    "Promedio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
    "Compartir entrada digital"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
    "Compartir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
    "Reservar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
    "Votar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
    "Nada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
    "Sin preferencia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
    "Juego en red: modo espectador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
    "Juego en red: modo sin estados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
    "Contraseña del servidor para espectadores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
    "Juego en red: activar espectadores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
    "Juego en red: puerto TCP"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
    "Juego en red: NAT traversal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
    "Comandos de red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
    "Puerto de comandos de red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
    "Información de red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
    "Mando en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
    "Puerto de base remota de red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
    "Descarga de miniaturas automática"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
    "Descarga automáticamente las miniaturas mientras se navega por las listas de reproducción. Provoca una bajada importante en el rendimiento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
    "Red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO,
    "No"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NONE,
    "Ninguno"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
    "No disponible"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
    "No hay logros que mostrar."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE,
    "Sin núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
    "No hay núcleos disponibles."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
    "No hay información del núcleo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
    "No hay opciones del núcleo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
    "No hay entradas disponibles."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
    "No hay historial disponible."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
    "No hay información disponible."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ITEMS,
    "No hay elementos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
    "No se han encontrado anfitriones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
    "No se han encontrado redes."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
    "No hay contadores de rendimiento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
    "No hay listas de reproducción."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
    "No hay entradas en la lista de reproducción."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
    "No se ha encontrado una configuración."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
    "No se han encontrado presets de shaders."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
    "No hay parámetros de shaders."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OFF,
    "OFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ON,
    "ON"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONLINE,
    "En línea"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
    "Actualizador en línea"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
    "Información en pantalla (OSD)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
    "Superposiciones"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
    "Ajusta los controles en pantalla y los marcos."
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
    "Diseño de vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
    "Ajusta el diseño de vídeo."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Notificaciones"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Ajusta las notificaciones en pantalla."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
    "Explorar archivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OPTIONAL,
    "Opcional"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY,
    "Superposiciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
    "Autocargar superposición preferida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
    "Superposiciones"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
    "Diseño de vídeo"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
    "Opacidad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
    "Preset"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
    "Escala"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
    "Superposición de pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
    "Usar modo PAL60"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
    "Carpeta superior"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
    "Permitir acceso externo a archivos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
    "Abre la configuración de permisos de acceso a archivos de Windows."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
    "Abrir..."
)
MSG_HASH(
    MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
    "Abre otra carpeta mediante el explorador de archivos del sistema."
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
    "Pausar al activar el menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
    "Reanudar contenido tras utilizar un guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
    "Cierra el menú y reanuda el contenido actual de forma automática tras seleccionar «Guardado rápido» o  «Cargar guardado rápido» en el menú rápido. Si se desactiva esta opción, puede mejorar el rendimiento de los guardados rápidos en dispositivos lentos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
    "Pausar al pasar a segundo plano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
    "Contadores de rendimiento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
    "Listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
    "Listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
    "Listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
    "Gestionar listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
    "Permite hacer tareas de mantenimiento en la lista seleccionada (p. ej.: asignar o reiniciar asociaciones de núcleos predeterminadas)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
    "Núcleo predeterminado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
    "Especifica el núcleo a utilizar cuando se inicie un contenido de una lista de reproducción que no tenga una asociación de núcleo ya existente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
    "Reiniciar asociaciones de núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
    "Elimina las asociaciones de núcleo en todos los elementos de la lista de reproducción."
    )
MSG_HASH(
    MSG_PLAYLIST_MANAGER_RESETTING_CORES,
    "Reiniciando núcleos: "
    )
MSG_HASH(
    MSG_PLAYLIST_MANAGER_CORES_RESET,
    "Núcleos reiniciados: "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Visualización de etiquetas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Cambia la forma de mostrar las etiquetas de contenido en esta lista."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
    "Mostrar todo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
    "Quitar paréntesis ()"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
    "Quitar corchetes []"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
    "Quitar paréntesis y corchetes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
    "Preservar región"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
    "Preservar índice de disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
    "Preservar región e índice de disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
    "Valores predeterminados del sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
    "Soporte táctil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PORT,
    "Puerto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PRESENT,
    "Presente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
    "Privacidad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS,
    "MIDI"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
    "Reiniciar RetroArch"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
    "Cerrar RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
    "Reiniciar RetroArch"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
    "Elemento de la base de datos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
    "Muestra la información de la base de datos sobre el contenido actual."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
    "Soporte de analógico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
    "Clasificación de la BBFC"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
    "Clasificación de la CERO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
    "Juego cooperativo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32,
    "CRC32"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
    "Descripción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
    "Desarrollador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
    "Edición de la revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
    "Puntuación de la revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
    "Análisis de la revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
    "Clasificación de la ELSPA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
    "Hardware de mejora"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
    "Clasificación de la ESRB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
    "Puntuación de la revista Famitsu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
    "Franquicia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
    "Género"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5,
    "MD5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
    "Nombre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
    "Origen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
    "Clasificación de PEGI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
    "Distribuidora"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
    "Mes de lanzamiento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
    "Año de lanzamiento"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
    "Soporte de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
    "Serie"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
    "SHA1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
    "Ejecutar contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
    "Clasificación de TGDB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
    "Nombre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
    "Ruta del archivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
    "Base de datos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
    "Tiempo de juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
    "Última partida"
    )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REBOOT,
    "Reiniciar (RCM)"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REBOOT,
    "Reiniciar"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
    "Configuración de grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
    "Salida de grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
    "Grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
    "Configuración de grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
    "Configuración de streaming"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
    "Grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
    "MIDI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
    "Soporte para grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_PATH,
    "Guardar grabación como..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
    "Usar carpeta de salida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE,
    "Archivo de reasignación de controles"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
    "Cargar archivo de reasignación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
    "Guardar reasignación para el núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
    "Carpeta de reasignaciones según contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
    "Guardar reasignación para el juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
    "Borrar reasignación del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
    "Borrar reasignación del juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
    "Borrar carpeta de reasignaciones de juegos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REQUIRED,
    "Necesario"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
    "Reiniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESUME,
    "Reanudar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
    "Reanudar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
    "RetroKeyboard"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROPAD,
    "RetroPad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
    "RetroPad con analógicos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
    "Logros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
    "Rebobinado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
    "Aplicar después de cambiar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
    "Autoaplicar trucos al cargar el juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
    "Nivel de detalle del rebobinado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
    "Tamaño del búfer de rebobinado (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
    "Tamaño del intervalo de ajuste del búfer (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
    "Rebobinado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
    "Opciones de trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
    "Detalles del truco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
    "Iniciar o reanudar búsqueda de trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
    "Explorador de archivos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
    "Configuración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
    "Mostrar pantalla de inicio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
    "Analógico derecho"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
    "Agregar a Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
    "Agregar a Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
    "Descargar miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
    "Descarga miniaturas para el contenido actual. Actualizará cualquier miniatura ya existente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
    "Asignar asociación de núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
    "Restablecer asociación de núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN,
    "Iniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
    "Iniciar"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
    "Activar SAMBA"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
    "Partidas guardadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
    "Indizar automáticamente la posición"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
    "Cargar guardado rápido automáticamente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
    "Guardado rápido automático"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
    "Guardados rápidos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
    "Miniaturas de guardados rápidos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
    "Guardar configuración actual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
    "Guardar personalizaciones del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Carpeta de personalizaciones de contenidos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
    "Guardar personalizaciones del juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
    "Guardar configuración nueva"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_STATE,
    "Guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
    "Guardado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
    "Buscar carpeta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_FILE,
    "Buscar archivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
    "<Buscar en esta carpeta>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
    "Capturas de pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
    "Resolución de pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SEARCH,
    "Buscar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SECONDS,
    "segundos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS,
    "Ajustes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
    "Ajustes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER,
    "Shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
    "Aplicar cambios"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
    "Shaders"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
    "Ribbon"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
    "Ribbon (simplificado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
    "Nieve simple"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
    "Nieve"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
    "Mostrar ajustes avanzados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
    "Mostrar archivos y carpetas ocultos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHUTDOWN,
    "Apagar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
    "Velocidad de cámara lenta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
    "Reducir latencia de forma predictiva"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
    "Fotogramas a predecir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
    "Segunda instancia de predicción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
    "Ocultar advertencias de predicción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
    "Ordenar partidas guardadas por carpetas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
    "Ordenar guardados rápidos por carpetas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
    "Escribir guardados rápidos en la carpeta del contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
    "Escribir partidas guardadas en la carpeta del contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
    "Guardar archivos del sistema en la carpeta del contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
    "Escribir capturas de pantalla en la carpeta del contenido"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
    "Activar SSH"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_CORE,
    "Iniciar núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
    "Iniciar RetroPad remoto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
    "Iniciar procesador de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATE_SLOT,
    "Posición de guardado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATUS,
    "Estado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
    "Comandos stdin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
    "Núcleos sugeridos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
    "Suspender salvapantallas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
    "Música del sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
    "Sistema/BIOS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
    "Información del sistema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
    "Soporte de 7zip"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
    "Soporte de ALSA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
    "Fecha de compilación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
    "Soporte de Cg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
    "Soporte de Cocoa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
    "Soporte de interfaz por comandos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
    "Soporte de CoreText"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
    "Modelo de CPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
    "Características de CPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
    "DPI de pantalla (métrico)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
    "Alto de pantalla (en milímetros)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
    "Ancho de pantalla (en milímetros)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
    "Soporte de DirectSound"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
    "Soporte de WASAPI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
    "Soporte de librerías dinámicas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
    "Carga dinámica en tiempo real de librerías libretro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
    "Soporte de EGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
    "Soporte de render-to-texture OpenGL/Direct3D (shaders multipasos)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
    "Soporte de FFmpeg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
    "Soporte de FreeType"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
    "Soporte de STB TrueType"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
    "Identificador de interfaz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
    "Nombre de la interfaz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
    "S.O. de la interfaz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
    "Versión de Git"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
    "Soporte de GLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
    "Soporte de HLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
    "Soporte de JACK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
    "Soporte de KMS/EGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
    "Versión de Lakka"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
    "Soporte de LibretroDB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
    "Soporte de Libusb"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
    "Soporte de juego en red (peer-to-peer)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
    "Soporte de interfaz de red por comandos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
    "Soporte de mando en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
    "Soporte de OpenAL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
    "Soporte de OpenGL ES"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
    "Soporte de OpenGL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
    "Soporte de OpenSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
    "Soporte de OpenVG"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
    "Soporte de OSS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
    "Soporte de superposiciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
    "Fuente de alimentación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
    "Cargada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
    "Cargando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
    "Descargando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
    "No hay una fuente"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
    "Soporte de PulseAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
    "Soporte de Python (scripts para shaders)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
    "Soporte de BMP (RBMP)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
    "Nivel de RetroRating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
    "Soporte de JPEG (RJPEG)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
    "Soporte de RoarAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
    "Soporte de PNG (RPNG)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
    "Soporte de RSound"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
    "Soporte de TGA (RTGA)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
    "Soporte de SDL2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
    "Soporte de imágenes SDL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
    "Soporte de SDL1.2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
    "Soporte de Slang"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
    "Soporte multihilo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
    "Soporte de Udev"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
    "Soporte de Video4Linux2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
    "Controlador de contexto de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
    "Soporte de Vulkan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
    "Soporte de Metal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
    "Soporte de Wayland"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
    "Soporte de X11"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
    "Soporte de XAudio2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
    "Soporte de XVideo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
    "Soporte de Zlib"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
    "Capturar pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
    "Tareas en hilos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS,
    "Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
    "Miniatura superior"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
    "Miniatura principal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
    "Miniaturas a la izquierda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
    "Miniaturas inferiores"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
    "Segunda miniatura"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
    "Miniatura secundaria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
    "Posición vertical de miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
    "Escala de miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
    "Reduce el tamaño de las miniaturas mediante un ancho máximo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
    "Umbral de escalado de miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
    "Escala automáticamente las miniaturas a un ancho/alto inferior al valor especificado. Mejora la calidad de la imagen y tiene un pequeño coste de rendimiento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
    "Mostrar miniaturas en lista de reproducción"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
    "Muestra las miniaturas de forma secuencial en las listas de reproducción. Al desactivar esta opción, todavía se puede mostrar la miniatura superior pulsando el botón RetroPad Y."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
    "Intercambiar miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
    "Cambia la posición de las miniaturas superior e inferior."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
    "Retraso de las miniaturas (ms)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
    "Aplica un retardo entre el momento en el que se selecciona un elemento de una lista de reproducción y la carga de su miniatura correspondiente. Un valor mínimo de 256 ms elimina cualquier retraso, incluso en los dispositivos más lentos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
    "Método de escalado de miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
    "Selecciona el método para reescalar las miniaturas más grandes para que entren en pantalla."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
    "Vecino más próximo (rápido)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
    "Bilineal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
    "Sinc/Lanczos3 (lento)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
    "Ninguno"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
    "Automático"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X2,
    "x2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X3,
    "x3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X4,
    "x4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X5,
    "x5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X6,
    "x6"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X7,
    "x7"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X8,
    "x8"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X9,
    "x9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_4_3,
    "4:3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9,
    "16:9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
    "16:9 (centrada)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10,
    "16:10"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
    "16:10 (centrada)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
    "Desactivar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
    "Ajustar a pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
    "Escala con valores enteros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
    "Miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
    "Actualizador de miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
    "Descargar el paquete completo de miniaturas para el sistema seleccionado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
    "Actualizar miniaturas de listas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
    "Descarga las miniaturas concretas de cada elemento de la lista de reproducción seleccionada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
    "Cajas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
    "Capturas de pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
    "Pantallas de título"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
    "Mostrar fecha y hora"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
    "Estilo de fecha y hora"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
    "Cambia la forma en que se muestra la fecha y hora."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HMS,
    "AAAA-MM-DD HH:MM:SS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HM,
    "AAAA-MM-DD HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MDYYYY,
    "MM-DD-AAAA HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS,
    "HH:MM:SS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HM,
    "HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_DM_HM,
    "DD/MM HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MD_HM,
    "MM/DD HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HMS_AM_PM,
    "AAAA-MM-DD HH:MM:SS (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HM_AM_PM,
    "AAAA-MM-DD HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MDYYYY_AM_PM,
    "MM-DD-AAAA HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS_AM_PM,
    "HH:MM:SS (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HM_AM_PM,
    "HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_DM_HM_AM_PM,
    "DD/MM HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MD_HM_AM_PM,
    "MM/DD HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
    "Animación de textos largos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
    "Selecciona el método de desplazamiento horizontal para los textos largos en los menús."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
    "Alternar izda./dcha."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
    "Desplazar hacia la izda."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
    "Velocidad de textos largos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
    "Indica la velocidad de animación de los textos largos en los menús."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
    "Suavizar movimiento de textos largos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
    "Utiliza una animación suave para mostrar los textos largos en los menús. Afecta levemente al rendimiento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
    "Esquema de color del menú"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
    "Selecciona un esquema de color. Utiliza «Personalizado» para utilizar archivos de esquemas de menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
    "Esquema de menú personalizado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
    "Selecciona un esquema de menús en el explorador de archivos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
    "Personalizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
    "Rojo clásico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
    "Naranja clásico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
    "Amarillo clásico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
    "Verde clásico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
    "Azul clásico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
    "Violeta clásico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
    "Gris clásico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
    "Rojo legado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
    "Violeta oscuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
    "Azul medianoche"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
    "Dorado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
    "Azul eléctrico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
    "Verde manzana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
    "Rojo volcánico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
    "Laguna"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER,
    "Programador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA,
    "Drácula"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
    "Algodón de azúcar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI,
    "Interfaz plana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
    "Gruvbox oscuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
    "Gruvbox claro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
    "Hackeando el kernel"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD,
    "Nórdico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NOVA,
    "Nova"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
    "Uno (oscuro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT,
    "Noche pálida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
    "Solarizado (oscuro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
    "Solarizado (claro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
    "Tango (oscuro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
    "Tango (claro)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ZENBURN,
    "Zenburn"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ANTI_ZENBURN,
    "Anti-Zenburn"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLUX,
    "Flux"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TRUE,
    "Activado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
    "Asistente de interfaz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
    "Ejecutar al inicio el asistente de la interfaz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
    "Mostrar menú de escritorio al inicio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
    "Activar menú de escritorio (reiniciar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
    "Barra de menús"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
    "No se ha podido leer el archivo comprimido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
    "Deshacer carga rápida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
    "Deshacer guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNKNOWN,
    "Desconocido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
    "Actualizador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
    "Actualizar recursos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
    "Actualizar perfiles de control"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
    "Actualizar shaders Cg"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
    "Actualizar trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
    "Actualizar archivos de información de núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
    "Actualizar bases de datos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
    "Actualizar shaders GLSL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
    "Actualizar Lakka"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
    "Actualizar superposiciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
    "Actualizar shaders Slang"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER,
    "Usuario"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_KEYBOARD,
    "Teclado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
    "Interfaz de usuario"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
    "Idioma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
    "Usuario"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
    "Usar visor de imágenes integrado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
    "Usar visor de medios integrado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
    "<Usar esta carpeta>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
    "Permitir rotación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
    "Configurar relación de aspecto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
    "Relación de aspecto automática"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
    "Relación de aspecto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
    "Insertar fotogramas negros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
    "Recortar overscan (reiniciar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
    "Desactivar composición de escritorio"
    )
#if defined(_3DS)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
    "Pantalla inferior de 3DS"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
    "Mostrar información de estado en la pantalla inferior. Desactiva esta opción para mejorar la duración de la batería y el rendimiento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
    "Modo de pantalla de 3DS"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
    "Selecciona el modo de pantalla 2D o 3D. En el modo 3D, los píxeles serán rectangulares y se aplicará un efecto de profundidad al usar el menú rápido. El modo 2D es el mejor para el rendimiento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_3D,
    "3D"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D,
    "2D"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400x240,
    "2D (Efecto de rejilla de píxeles)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800x240,
    "2D (Alta resolución)"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
    "Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
    "Filtro de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
    "Filtro de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
    "Filtro de parpadeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
    "Notificaciones en pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
    "Fuente de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
    "Tamaño de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
    "Forzar relación de aspecto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
    "Forzar desactivación del FBO sRGB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
    "Retraso de fotogramas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
    "Retraso de shaders automáticos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
    "Iniciar en pantalla completa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
    "Gamma de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
    "Activar grabación de GPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
    "Capturas de pantalla de GPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
    "Sincronía estricta de GPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
    "Fotogramas para sincronía estricta de GPU"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
    "Máximo de imágenes en swap chain"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
    "Posición X de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
    "Posición Y de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
    "Índice del monitor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
    "Activar grabación con filtros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
    "Frecuencia de actualización"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
    "Frecuencia estimada del monitor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
    "Frecuencia declarada por la pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
    "Rotación de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
    "Orientación de pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
    "Escala en ventana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
    "Hilos de grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
    "Escalar usando enteros"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
    "Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
    "Shader de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
    "Pasadas del shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
    "Parámetros de shaders"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
    "Cargar preset de shaders"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
    "Guardar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
    "Guardar preset de shaders como..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
    "Guardar preset global"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
    "Guardar preset para el núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
    "Guardar preset de directorio de contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
    "Guardar preset para el juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
    "Activar contexto compartido por HW"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
    "Filtrado bilineal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
    "Filtro de suavizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
    "Intervalo de intercambio de Vsync"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
    "Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
    "Vídeo multihilo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
    "Filtro antiparpadeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
    "Alto de la resolución personalizada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
    "Ancho de la resolución personalizada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
    "Posición X de resolución personalizada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
    "Posición Y de resolución personalizada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
    "Asignar ancho de interfaz visual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
    "Corrección de overscan (superior)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
    "Ajusta el recorte del overscan reduciendo un número concreto de líneas (a partir de la parte superior de la pantalla). Aviso: Puede provocar defectos de escalado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
    "Corrección de overscan (inferior)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
    "Ajusta el recorte del overscan reduciendo un número concreto de líneas (a partir de la parte inferior de la pantalla). Aviso: Puede provocar defectos de escalado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
    "Sincronía vertical (Vsync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
    "Pantalla completa en ventana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
    "Ancho de la ventana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
    "Alto de la ventana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
    "Ancho en pantalla completa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
    "Alto en pantalla completa"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
    "Activar diseño de vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
    "Los diseños de vídeo se utilizan para poner marcos y otras imágenes."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
    "Ruta de diseños de vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
    "Selecciona un diseño de vídeo desde el explorador de archivos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
    "Vista seleccionada"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
    "Selecciona una vista del diseño cargado."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
    "Controlador wifi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
    "Wifi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
    "Transparencia del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
    "Color rojo de la fuente del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
    "Color verde de la fuente del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
    "Color azul de la fuente del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_FONT,
    "Fuente del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
    "Personalizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
    "FlatUI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
    "Monochrome"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
    "Monochrome invertido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
    "Systematic"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
    "NeoActive"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
    "Pixel"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
    "RetroActive"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
    "Retrosystem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
    "Dot-Art"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
    "Automática"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
    "Automática invertida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
    "Color del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
    "Verde manzana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
    "Oscuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
    "Claro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
    "Azul mañana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
    "Sunbeam"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
    "Violeta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
    "Azul eléctrico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
    "Dorado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
    "Rojo legado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
    "Azul medianoche"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
    "Simple"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
    "Bajo el mar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
    "Rojo volcánico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
    "Canal de shaders del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
    "Sombras de iconos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
    "Mostrar pestaña Historial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
    "Mostrar pestaña Importar contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
    "Mostrar pestañas de listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
    "Mostrar pestaña Favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
    "Mostrar pestaña Imágenes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
    "Mostrar pestaña Música"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
    "Mostrar pestaña Configuración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
    "Mostrar pestaña Vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
    "Mostrar pestaña Juego en red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
    "Disposición del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_THEME,
    "Tema de iconos del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_YES,
    "Sí"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
    "Preset de shader"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
    "Compite para conseguir logros hechos a medida para los juegos clásicos.\n"
    "Para más información, visita http://retroachievements.org."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
    "Activa los logros no oficiales y/o beta para probarlos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Duplica los puntos conseguidos a costa de desactivar las funciones de guardado rápido, trucos, rebobinado, pausa y cámara lenta en todos los juegos.\n"
    "Si cambias este ajuste en mitad de una partida, el juego se reiniciará."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
    "Utiliza tablas de clasificación específicas para cada juego.\n"
    "Esta opción no surtirá efecto si se ha desactivado el modo Hardcore."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
    "Muestra insignias en la lista de logros."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
    "Muestra información adicional en las notificaciones."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
    "Captura la pantalla automáticamente al obtener un logro."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
    "Cambia los controladores usados por el sistema."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
    "Cambia las opciones de los logros."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_SETTINGS,
    "Cambia las opciones de los núcleos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
    "Cambia las opciones de grabación."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
    "Cambia las opciones de notificaciones, controles en pantalla y marcos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
    "Cambia las opciones de rebobinado, aceleración de fotogramas y cámara lenta."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
    "Cambia las opciones de guardado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
    "Cambia las opciones de registro."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
    "Cambia las opciones de la interfaz de usuario."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_SETTINGS,
    "Cambia las opciones de cuentas, nombre de usuario y el idioma."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
    "Cambia las opciones de privacidad."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
    "Cambia las opciones de MIDI."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
    "Cambia las carpetas predeterminadas para los archivos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
    "Cambia las opciones de las listas de reproducción."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
    "Configura las opciones del servidor y de red."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
    "Busca contenidos y los incluye en la base de datos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
    "Cambia las opciones de salida de audio."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
    "Activa o desactiva la función de Bluetooth."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
    "Guarda los cambios en el archivo de configuración al salir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
    "Cambia los valores predeterminados de los archivos de configuración."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
    "Administra los archivos de configuración."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CPU_CORES,
    "La cantidad de núcleos que tiene la CPU."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FPS_SHOW,
    "Muestra la velocidad de FPS en pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
    "Muestra el contador de fotogramas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MEMORY_SHOW,
    "Añade el indicador de memoria usada/total en el contador de FPS/fotogramas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
    "Configura las teclas rápidas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
    "Asigna una combinación de botones en el mando para mostrar el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
    "Cambia las opciones de mando, teclado y ratón."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
    "Configura los controles para este puerto."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
    "Cambia las opciones relacionadas con la latencia de vídeo, audio y control."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
    "Registra los eventos a la terminal o a un archivo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY,
    "Permite unirse o crear una sesión de juego en red."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
    "Buscar sesiones de juego en red en la red local."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
    "Muestra la información del sistema."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
    "Descarga componentes y contenido adicional para RetroArch."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
    "Activa o desactiva el compartido de carpetas en red mediante el protocolo SMB."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
    "Administra los servicios del sistema operativo."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
    "Muestra archivos y carpetas ocultos en el explorador de archivos."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_SSH_ENABLE,
    "Utiliza SSH para acceder a la línea de comandos de forma remota."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
    "Evita que se active el protector de pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
    "Cambia el tamaño de la ventana para que sea relativo al núcleo. También puedes fijar un tamaño de ventana más abajo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_LANGUAGE,
    "Cambia el idioma de la interfaz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
    "Introduce fotogramas negros intermedios. Útil para usuarios con pantallas de 120 Hz que quieren eliminar el efecto ghosting en contenidos que se reproducen a 60 Hz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
    "Reduce la latencia a costa de un mayor riesgo de tirones. Agrega un retraso posterior a la Vsync en milisegundos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
    "Reduce la carga automática de shaders en milisegundos. Puede sortear los fallos gráficos al utilizar programas de captura de pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
    "Ajusta la cantidad de fotogramas que puede predecir la CPU de la GPU al utilizar la sincronía estricta de GPU."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
    "Informa al controlador de vídeo que utilice un modo de búfer concreto."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
    "Selecciona la pantalla a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
    "Muestra la estimación exacta de la frecuencia de actualización de la pantalla en hercios."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
    "Muestra la frecuencia de actualización según el controlador de pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
    "Cambia las opciones de salida de vídeo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
    "Busca redes inalámbricas para conectarse a ellas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HELP_LIST,
    "Muestra información sobre el funcionamiento del programa."
    )
MSG_HASH(
    MSG_ADDED_TO_FAVORITES,
    "Favorito agregado"
    )
MSG_HASH(
    MSG_ADD_TO_FAVORITES_FAILED,
    "Error al añadir favorito: la lista de reproducción está llena."
    )
MSG_HASH(
    MSG_SET_CORE_ASSOCIATION,
    "Núcleo asignado: "
    )
MSG_HASH(
    MSG_RESET_CORE_ASSOCIATION,
    "Se ha restablecido la asociación del núcleo de la entrada de la lista."
    )
MSG_HASH(
    MSG_APPENDED_DISK,
    "Disco en cola"
    )
MSG_HASH(
    MSG_APPLICATION_DIR,
    "Carpeta de la aplicación"
    )
MSG_HASH(
    MSG_APPLYING_CHEAT,
    "Aplicando trucos."
    )
MSG_HASH(
    MSG_APPLYING_SHADER,
    "Aplicando shader"
    )
MSG_HASH(
    MSG_AUDIO_MUTED,
    "Audio silenciado."
    )
MSG_HASH(
    MSG_AUDIO_UNMUTED,
    "Silencio desactivado."
    )
MSG_HASH(
    MSG_AUTOCONFIG_FILE_ERROR_SAVING,
    "Error al guardar el archivo de autoconfiguración."
    )
MSG_HASH(
    MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
    "El archivo de autoconfiguración ha sido guardado."
    )
MSG_HASH(
    MSG_AUTOSAVE_FAILED,
    "No se puede inicializar el autoguardado."
    )
MSG_HASH(
    MSG_AUTO_SAVE_STATE_TO,
    "Autoguardar en:"
    )
MSG_HASH(
    MSG_BLOCKING_SRAM_OVERWRITE,
    "Bloqueando sobrescritura de SaveRAM"
    )
MSG_HASH(
    MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
    "Iniciando la línea de comandos en el puerto"
    )
MSG_HASH(
    MSG_BYTES,
    "bytes"
    )
MSG_HASH(
    MSG_CANNOT_INFER_NEW_CONFIG_PATH,
    "No se puede deducir la nueva ruta de configuración. Utilizando la fecha actual."
    )
MSG_HASH(
    MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Modo Hardcore activado, guardado rápido y rebobinado desactivados."
    )
MSG_HASH(
    MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
    "Comparando con números mágicos conocidos..."
    )
MSG_HASH(
    MSG_COMPILED_AGAINST_API,
    "Compilado para la API"
    )
MSG_HASH(
    MSG_CONFIG_DIRECTORY_NOT_SET,
    "Carpeta de configuración no establecida. No se puede guardar la configuración."
    )
MSG_HASH(
    MSG_CONNECTED_TO,
    "Conectado a"
    )
MSG_HASH(
    MSG_CONTENT_CRC32S_DIFFER,
    "El CRC32 de los contenidos no es idéntico. No se pueden usar juegos distintos."
    )
MSG_HASH(
    MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
    "Carga de contenido omitida. La implementación usará la suya."
    )
MSG_HASH(
    MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
    "El núcleo no soporta guardados rápidos."
    )
MSG_HASH(
    MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
    "El archivo de opciones del núcleo ha sido creado."
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
    "No se encuentra otro controlador"
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
    "No se encuentra un sistema compatible."
    )
MSG_HASH(
    MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
    "No se encuentra una pista de datos válida"
    )
MSG_HASH(
    MSG_COULD_NOT_OPEN_DATA_TRACK,
    "No se puede abrir la pista de datos"
    )
MSG_HASH(
    MSG_COULD_NOT_READ_CONTENT_FILE,
    "No se puede leer el contenido"
    )
MSG_HASH(
    MSG_COULD_NOT_READ_MOVIE_HEADER,
    "No se puede leer la cabecera de la película."
    )
MSG_HASH(
    MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
    "No se puede leer el estado de la película."
    )
MSG_HASH(
    MSG_CRC32_CHECKSUM_MISMATCH,
    "El CRC32 del contenido no concuerda con el de la repetición. Es muy probable que la reproducción se desincronice."
    )
MSG_HASH(
    MSG_CUSTOM_TIMING_GIVEN,
    "Timing personalizado provisto"
    )
MSG_HASH(
    MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
    "Descomprimiendo."
    )
MSG_HASH(
    MSG_DECOMPRESSION_FAILED,
    "Error al descomprimir."
    )
MSG_HASH(
    MSG_DETECTED_VIEWPORT_OF,
    "Ventana detectada:"
    )
MSG_HASH(
    MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
    "No se encontró un parche válido."
    )
MSG_HASH(
    MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
    "Desconecte el dispositivo desde un puerto válido."
    )
MSG_HASH(
    MSG_DISK_CLOSED,
    "Cerrado"
    )
MSG_HASH(
    MSG_DISK_EJECTED,
    "Expulsado"
    )
MSG_HASH(
    MSG_DOWNLOADING,
    "Descargando"
    )
MSG_HASH(
    MSG_INDEX_FILE,
    "índice"
    )
MSG_HASH(
    MSG_DOWNLOAD_FAILED,
    "Error al descargar."
    )
MSG_HASH(
    MSG_ERROR,
    "Error"
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
    "El núcleo Libretro necesita contenido, pero no fue provisto."
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
    "El núcleo Libretro necesita un contenido especial, pero no fue provisto."
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
    "El núcleo no es compatible con VFS y no se pudo cargar una copia local."
)
MSG_HASH(
    MSG_ERROR_PARSING_ARGUMENTS,
    "Error al analizar argumentos."
    )
MSG_HASH(
    MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
    "Error al guardar el archivo de opciones del núcleo."
    )
MSG_HASH(
    MSG_ERROR_SAVING_REMAP_FILE,
    "Error al guardar el archivo de reasignación."
    )
MSG_HASH(
    MSG_ERROR_REMOVING_REMAP_FILE,
    "Error al eliminar el archivo de reasignación."
    )
MSG_HASH(
    MSG_ERROR_SAVING_SHADER_PRESET,
    "Error al guardar el preset de shaders."
    )
MSG_HASH(
    MSG_EXTERNAL_APPLICATION_DIR,
    "Carpeta externa a la aplicación"
    )
MSG_HASH(
    MSG_EXTRACTING,
    "Extrayendo"
    )
MSG_HASH(
    MSG_EXTRACTING_FILE,
    "Extrayendo archivo"
    )
MSG_HASH(
    MSG_FAILED_SAVING_CONFIG_TO,
    "Error al guardar configuración en"
    )
MSG_HASH(
    MSG_FAILED_TO,
    "Error:"
    )
MSG_HASH(
    MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
    "Error al aceptar al espectador."
    )
MSG_HASH(
    MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
    "Error al reservar memoria para el contenido parcheado..."
    )
MSG_HASH(
    MSG_FAILED_TO_APPLY_SHADER,
    "Error al aplicar el shader."
    )
MSG_HASH(
    MSG_FAILED_TO_APPLY_SHADER_PRESET,
    "Error al aplicar el preset de shaders:"
    )
MSG_HASH(
    MSG_FAILED_TO_BIND_SOCKET,
    "Error al asignar el socket."
    )
MSG_HASH(
    MSG_FAILED_TO_CREATE_THE_DIRECTORY,
    "Error al crear la carpeta."
    )
MSG_HASH(
    MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
    "Error al extraer el contenido del archivo comprimido"
    )
MSG_HASH(
    MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
    "Error al obtener el apodo del cliente."
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD,
    "Error al cargar"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_CONTENT,
    "Error al cargar contenido"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_MOVIE_FILE,
    "Error al cargar película"
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_OVERLAY,
    "Error al cargar la superposición."
    )
MSG_HASH(
    MSG_FAILED_TO_LOAD_STATE,
    "Error al cargar el guardado rápido de"
    )
MSG_HASH(
    MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
    "Error al abrir el núcleo Libretro"
    )
MSG_HASH(
    MSG_FAILED_TO_PATCH,
    "Error al parchear"
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
    "Error al recibir el encabezado desde el cliente."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME,
    "Error al recibir el apodo."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
    "Error al recibir el apodo del servidor."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
    "Error al recibir el tamaño del apodo del servidor."
    )
MSG_HASH(
    MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
    "Error al recibir los datos SaveRAM del servidor."
    )
MSG_HASH(
    MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
    "Error al expulsar el disco de la bandeja."
    )
MSG_HASH(
    MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
    "Error al eliminar el archivo temporal"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_SRAM,
    "Error al guardar SaveRAM"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_STATE_TO,
    "Error al guardar en"
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME,
    "Error al enviar el apodo."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_SIZE,
    "Error al enviar el tamaño del apodo."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
    "Error al enviar el apodo al cliente."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
    "Error al enviar el apodo al servidor."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
    "Error al enviar datos SaveRAM al cliente."
    )
MSG_HASH(
    MSG_FAILED_TO_START_AUDIO_DRIVER,
    "Error al iniciar el controlador de audio. Se continuará en silencio."
    )
MSG_HASH(
    MSG_FAILED_TO_START_MOVIE_RECORD,
    "Error al iniciar clip de grabación."
    )
MSG_HASH(
    MSG_FAILED_TO_START_RECORDING,
    "Error al iniciar grabación."
    )
MSG_HASH(
    MSG_FAILED_TO_TAKE_SCREENSHOT,
    "Error al capturar pantalla."
    )
MSG_HASH(
    MSG_FAILED_TO_UNDO_LOAD_STATE,
    "Error al deshacer carga."
    )
MSG_HASH(
    MSG_FAILED_TO_UNDO_SAVE_STATE,
    "Error al deshacer guardado."
    )
MSG_HASH(
    MSG_FAILED_TO_UNMUTE_AUDIO,
    "Error al restablecer el audio."
    )
MSG_HASH(
    MSG_FATAL_ERROR_RECEIVED_IN,
    "Error fatal recibido en"
    )
MSG_HASH(
    MSG_FILE_NOT_FOUND,
    "Archivo no encontrado"
    )
MSG_HASH(
    MSG_FOUND_AUTO_SAVESTATE_IN,
    "Autoguardado localizado en"
    )
MSG_HASH(
    MSG_FOUND_DISK_LABEL,
    "Encontrada la etiqueta del disco"
    )
MSG_HASH(
    MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
    "Encontrada la primer pista de datos del archivo"
    )
MSG_HASH(
    MSG_FOUND_LAST_STATE_SLOT,
    "Encontrada la última posición de guardado rápido"
    )
MSG_HASH(
    MSG_FOUND_SHADER,
    "Encontrado el shader"
    )
MSG_HASH(
    MSG_FRAMES,
    "Fotogramas"
    )
MSG_HASH(
    MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
    "Opciones específicas del juego y del núcleo encontradas en"
    )
MSG_HASH(
    MSG_GOT_INVALID_DISK_INDEX,
    "Índice de disco incorrecto."
    )
MSG_HASH(
    MSG_GRAB_MOUSE_STATE,
    "Capturar estado del ratón"
    )
MSG_HASH(
    MSG_GAME_FOCUS_ON,
    "Activar prioridad al juego"
    )
MSG_HASH(
    MSG_GAME_FOCUS_OFF,
    "Desactivar prioridad al juego"
    )
MSG_HASH(
    MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
    "El núcleo Libretro se renderiza por hardware. Las grabaciones deben tener shaders aplicados."
    )
MSG_HASH(
    MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
    "El CRC32 inflado no concuerda."
    )
MSG_HASH(
    MSG_INPUT_CHEAT,
    "Introducir truco"
    )
MSG_HASH(
    MSG_INPUT_CHEAT_FILENAME,
    "Introducir nombre de archivo de truco"
    )
MSG_HASH(
    MSG_INPUT_PRESET_FILENAME,
    "Introducir nombre de archivo del preset"
    )
MSG_HASH(
    MSG_INPUT_RENAME_ENTRY,
    "Renombrar título"
    )
MSG_HASH(
    MSG_INTERFACE,
    "Interfaz"
    )
MSG_HASH(
    MSG_INTERNAL_STORAGE,
    "Almacenamiento interno"
    )
MSG_HASH(
    MSG_REMOVABLE_STORAGE,
    "Almacenamiento extraíble"
    )
MSG_HASH(
    MSG_INVALID_NICKNAME_SIZE,
    "Tamaño de apodo incorrecto."
    )
MSG_HASH(
    MSG_IN_BYTES,
    "en bytes"
    )
MSG_HASH(
    MSG_IN_GIGABYTES,
    "en gigabytes"
    )
MSG_HASH(
    MSG_IN_MEGABYTES,
    "en megabytes"
    )
MSG_HASH(
    MSG_LIBRETRO_ABI_BREAK,
    "está compilado para otra versión de Libretro"
    )
MSG_HASH(
    MSG_LIBRETRO_FRONTEND,
    "Interfaz de usuario para Libretro"
    )
MSG_HASH(
    MSG_LOADED_STATE_FROM_SLOT,
    "Guardado rápido n.º %d cargado."
    )
MSG_HASH(
    MSG_LOADED_STATE_FROM_SLOT_AUTO,
    "Guardado rápido #-1 (autom.) cargado."
    )
MSG_HASH(
    MSG_LOADING,
    "Cargando"
    )
MSG_HASH(
    MSG_FIRMWARE,
    "Faltan archivos de firmware"
    )
MSG_HASH(
    MSG_LOADING_CONTENT_FILE,
    "Cargando contenido"
    )
MSG_HASH(
    MSG_LOADING_HISTORY_FILE,
    "Cargando historial"
    )
MSG_HASH(
    MSG_LOADING_FAVORITES_FILE,
    "Cargando favoritos"
    )
MSG_HASH(
    MSG_LOADING_STATE,
    "Cargando guardado rápido"
    )
MSG_HASH(
    MSG_MEMORY,
    "Memoria"
    )
MSG_HASH(
    MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
    "La grabación no es un archivo BSV1 válido."
    )
MSG_HASH(
    MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
    "El formato de la grabación parece tener una versión diferente de serializer. Probablemente dará error."
    )
MSG_HASH(
    MSG_MOVIE_PLAYBACK_ENDED,
    "La reproducción ha finalizado."
    )
MSG_HASH(
    MSG_MOVIE_RECORD_STOPPED,
    "Deteniendo grabación."
    )
MSG_HASH(
    MSG_NETPLAY_FAILED,
    "Error al iniciar juego en red."
    )
MSG_HASH(
    MSG_NO_CONTENT_STARTING_DUMMY_CORE,
    "No hay contenido, iniciando núcleo vacío."
    )
MSG_HASH(
    MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
    "No se ha sobrescrito ningún guardado rápido."
    )
MSG_HASH(
    MSG_NO_STATE_HAS_BEEN_LOADED_YET,
    "No se ha cargado un guardado rápido."
    )
MSG_HASH(
    MSG_OVERRIDES_ERROR_SAVING,
    "Error al guardar la personalización."
    )
MSG_HASH(
    MSG_OVERRIDES_SAVED_SUCCESSFULLY,
    "La personalización ha sido guardada."
    )
MSG_HASH(
    MSG_PAUSED,
    "En pausa."
    )
MSG_HASH(
    MSG_PROGRAM,
    "RetroArch"
    )
MSG_HASH(
    MSG_READING_FIRST_DATA_TRACK,
    "Leyendo la primera pista de datos..."
    )
MSG_HASH(
    MSG_RECEIVED,
    "recibido"
    )
MSG_HASH(
    MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
    "Grabación terminada debido al cambio de tamaño."
    )
MSG_HASH(
    MSG_RECORDING_TO,
    "Grabando en"
    )
MSG_HASH(
    MSG_REDIRECTING_CHEATFILE_TO,
    "Redirigiendo archivo de trucos a"
    )
MSG_HASH(
    MSG_REDIRECTING_SAVEFILE_TO,
    "Redirigiendo archivo de guardado a"
    )
MSG_HASH(
    MSG_REDIRECTING_SAVESTATE_TO,
    "Redirigiendo archivo de guardado rápido a"
    )
MSG_HASH(
    MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
    "La reasignación ha sido guardada."
    )
MSG_HASH(
    MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
    "La reasignación ha sido eliminada."
    )
MSG_HASH(
    MSG_REMOVED_DISK_FROM_TRAY,
    "El disco ha sido retirado de la bandeja."
    )
MSG_HASH(
    MSG_REMOVING_TEMPORARY_CONTENT_FILE,
    "Eliminando el contenido temporal"
    )
MSG_HASH(
    MSG_RESET,
    "Reiniciar"
    )
MSG_HASH(
    MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
    "Reiniciando grabación por reinicio del controlador."
    )
MSG_HASH(
    MSG_RESTORED_OLD_SAVE_STATE,
    "Restaurado antiguo guardado rápido."
    )
MSG_HASH(
    MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
    "Shaders: restaurado el preset por defecto en"
    )
MSG_HASH(
    MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
    "Revirtiendo la carpeta de guardado a"
    )
MSG_HASH(
    MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
    "Revirtiendo la carpeta de guardado rápido a"
    )
MSG_HASH(
    MSG_REWINDING,
    "Rebobinando."
    )
MSG_HASH(
    MSG_REWIND_INIT,
    "Iniciando búfer de rebobinado de"
    )
MSG_HASH(
    MSG_REWIND_INIT_FAILED,
    "Error al iniciar el buffer. El rebobinado se desactivará."
    )
MSG_HASH(
    MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
    "La implementación usa audio multihilo. No se puede rebobinar."
    )
MSG_HASH(
    MSG_REWIND_REACHED_END,
    "Se ha alcanzado el final del buffer de rebobinado."
    )
MSG_HASH(
    MSG_SAVED_NEW_CONFIG_TO,
    "Guardada nueva configuración en"
    )
MSG_HASH(
    MSG_SAVED_STATE_TO_SLOT,
    "Guardado rápido en la posición %d terminado."
    )
MSG_HASH(
    MSG_SAVED_STATE_TO_SLOT_AUTO,
    "Guardado rápido en la posición #-1 (autom.) terminado."
    )
MSG_HASH(
    MSG_SAVED_SUCCESSFULLY_TO,
    "Datos guardados en"
    )
MSG_HASH(
    MSG_SAVING_RAM_TYPE,
    "Guardando RAM"
    )
MSG_HASH(
    MSG_SAVING_STATE,
    "Guardado rápido"
    )
MSG_HASH(
    MSG_SCANNING,
    "Buscando"
    )
MSG_HASH(
    MSG_SCANNING_OF_DIRECTORY_FINISHED,
    "Búsqueda de carpeta finalizada."
    )
MSG_HASH(
    MSG_SENDING_COMMAND,
    "Enviando comando"
    )
MSG_HASH(
    MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
    "Hay varios parches definidos explícitamente, ignorando todos..."
    )
MSG_HASH(
    MSG_SHADER,
    "Shader"
    )
MSG_HASH(
    MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
    "Preset de shaders guardado."
    )
MSG_HASH(
    MSG_SKIPPING_SRAM_LOAD,
    "Omitiendo carga de SRAM."
    )
MSG_HASH(
    MSG_SLOW_MOTION,
    "Cámara lenta."
    )
MSG_HASH(
    MSG_FAST_FORWARD,
    "Avance rápido."
    )
MSG_HASH(
    MSG_SLOW_MOTION_REWIND,
    "Rebobinado lento."
    )
MSG_HASH(
    MSG_SRAM_WILL_NOT_BE_SAVED,
    "No se guardará la SRAM."
    )
MSG_HASH(
    MSG_STARTING_MOVIE_PLAYBACK,
    "Reproduciendo película."
    )
MSG_HASH(
    MSG_STARTING_MOVIE_RECORD_TO,
    "Iniciando grabación de película en"
    )
MSG_HASH(
    MSG_STATE_SIZE,
    "Tamaño del guardado"
    )
MSG_HASH(
    MSG_STATE_SLOT,
    "Posición de guardado"
    )
MSG_HASH(
    MSG_TAKING_SCREENSHOT,
    "Capturando pantalla"
    )
MSG_HASH(
    MSG_SCREENSHOT_SAVED,
    "Pantalla capturada"
    )
MSG_HASH(
    MSG_ACHIEVEMENT_UNLOCKED,
    "Logro desbloqueado"
    )
MSG_HASH(
    MSG_CHANGE_THUMBNAIL_TYPE,
    "Cambiar tipo de miniatura"
    )
MSG_HASH(
    MSG_NO_THUMBNAIL_AVAILABLE,
    "No hay miniaturas disponibles"
    )
MSG_HASH(
    MSG_PRESS_AGAIN_TO_QUIT,
    "Vuelve a pulsar para salir..."
    )
MSG_HASH(
    MSG_TO,
    "en"
    )
MSG_HASH(
    MSG_UNDID_LOAD_STATE,
    "Carga rápida deshecha."
    )
MSG_HASH(
    MSG_UNDOING_SAVE_STATE,
    "Deshaciendo guardado rápido."
    )
MSG_HASH(
    MSG_UNKNOWN,
    "Desconocido"
    )
MSG_HASH(
    MSG_UNPAUSED,
    "Pausa terminada."
    )
MSG_HASH(
    MSG_UNRECOGNIZED_COMMAND,
    "Comando no reconocido"
    )
MSG_HASH(
    MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
    "Usando el nombre del núcleo para la nueva configuración."
    )
MSG_HASH(
    MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
    "Usando núcleo vacío. Omitiendo grabación."
    )
MSG_HASH(
    MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
    "Conecta el dispositivo a un puerto válido."
    )
MSG_HASH(
    MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
    "Desconectando el dispositivo del puerto"
    )
MSG_HASH(
    MSG_VALUE_REBOOTING,
    "Reiniciando..."
    )
MSG_HASH(
    MSG_VALUE_SHUTTING_DOWN,
    "Apagando..."
    )
MSG_HASH(
    MSG_VERSION_OF_LIBRETRO_API,
    "Versión de la API Libretro"
    )
MSG_HASH(
    MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
    "¡Error al calcular el tamaño de la ventana! Se continuará utilizando datos en bruto. Probablemente no funcionará bien..."
    )
MSG_HASH(
    MSG_VIRTUAL_DISK_TRAY,
    "bandeja de discos virtual."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
    "Selecciona la latencia de audio deseada en milisegundos. Este valor puede ser ignorado si el controlador no puede generar dicha latencia."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MUTE,
    "Silencia el audio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
    "Ayuda a suavizar imperfecciones de timing al sincronizar audio y vídeo. Cuidado: Si se desactiva esta opción, es casi imposible tener una sincronía correcta."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
    "Permite que los núcleos puedan acceder a la cámara."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
    "Permite que los núcleos puedan acceder a los servicios de localización."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
    "Indica el número máximo de usuarios que puede tener RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
    "Influencia cómo se realiza el sondeo de control dentro de RetroArch. «Temprano» o «Tardío» pueden reducir la latencia en función de tu configuración."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
    "Permite que cualquier usuario pueda controlar el menú. Si esta opción está desactivada, solo podrá hacerlo el usuario 1."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
    "Ajusta el volumen de audio (en dB). 0 dB es el volumen normal, sin ganancia alguna."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
    "Permite que el controlador WASAPI tome el control exclusivo del dispositivo  de audio. Si se desactiva esta opción se usará el modo compartido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
    "Usa el formato de coma flotante para el controlador WASAPI, si es compatible con tu dispositivo de audio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
    "Asigna el tamaño del búfer intermedio (en fotogramas) al usar el controlador WASAPI en modo compartido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SYNC,
    "Sincroniza el audio. Recomendado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
    "Ajusta la distancia a la que debe llegar un eje para que el botón se considere presionado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
    "Ajusta la cantidad de segundos a esperar hasta la siguiente asignación."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
    "Ajusta la cantidad de segundos que debe mantenerse pulsada una entrada para asignarla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
    "Describe el periodo entre pulsación de los botones con turbo (en fotogramas)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
    "Describe la duración de la pulsación de los botones turbo (en fotogramas)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
    "Selecciona el comportamiento básico del modo turbo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_TURBO_DEFAULT_BUTTON,
    "El botón individual predeterminado para el modo turbo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
    "Sincroniza la señal de vídeo de la tarjeta gráfica con la frecuencia de actualización de pantalla. Recomendado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
    "Permite a los núcleos rotar la pantalla. Al desactivar esta opción, se ignorarán las peticiones de rotación. Es útil cuando se puede rotar la pantalla de forma manual."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
    "Algunos núcleos tienen una opción de apagado. Activa esta opción para evitar que RetroArch se cierre y, en su lugar, cargue un núcleo vacío."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
    "Verifica que el firmware necesario esté disponible antes de cargar el contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
    "Ajusta la frecuencia de actualización vertical de tu pantalla para calcular la velocidad de audio.\n"
    "Nota: Este valor se ignorará si la opción Vídeo multihilo está activada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
    "Activa la salida de audio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
    "Ajusta la variación máxima en la velocidad de audio. Un valor alto permite grandes cambios de timing a costa de alterar el tono del audio (p. ej.: núcleos PAL en pantallas NTSC)."
    )
MSG_HASH(
    MSG_FAILED,
    "Error"
    )
MSG_HASH(
    MSG_SUCCEEDED,
    "Proceso terminado"
    )
MSG_HASH(
    MSG_DEVICE_NOT_CONFIGURED,
    "no configurado"
    )
MSG_HASH(
    MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
    "no configurado, usando respaldo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
    "Lista de cursores de la base de datos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
    "Filtro de base de datos: desarrollador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
    "Filtro de base de datos: distribuidora"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISABLED,
    "Desactivar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ENABLED,
    "Activar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
    "Ruta del historial"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
    "Filtro de base de datos: origen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
    "Filtro de base de datos: franquicia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
    "Filtro de base de datos: clasificación de la ESRB"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
    "Filtro de base de datos: clasificación de la ELSPA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
    "Filtro de base de datos: clasificación de PEGI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
    "Filtro de base de datos: clasificación de la CERO"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
    "Filtro de base de datos: clasificación de la BBFC"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
    "Filtro de base de datos: n.º máximo de usuarios"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
    "Filtro de base de datos: lanzamiento por mes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
    "Filtro de base de datos: lanzamiento por año"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
    "Filtro de base de datos: número de la revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
    "Filtro de base de datos: puntuación de la revista Edge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
    "Información de base de datos"
    )
MSG_HASH(
    MSG_WIFI_SCAN_COMPLETE,
    "Búsqueda de señales wifi finalizada."
    )
MSG_HASH(
    MSG_SCANNING_WIRELESS_NETWORKS,
    "Buscando redes inalámbricas..."
    )
MSG_HASH(
    MSG_NETPLAY_LAN_SCAN_COMPLETE,
    "Búsqueda de sesiones de juego en red finalizada."
    )
MSG_HASH(
    MSG_NETPLAY_LAN_SCANNING,
    "Buscando sesiones de juego en red..."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
    "Pausa la partida si RetroArch no es la ventana activa."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
    "Los gestores de pantalla utilizan la composición para aplicar efectos visuales o ventanas que no respondan, entre otras cosas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
    "Permite tener listas de juegos, imágenes, música y vídeos utilizados recientemente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
    "Limita el número de entradas en la lista de reproducción de elementos recientes."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
    "Controles de menú unificados"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
    "Usa los mismos controles para el menú y el juego. Esta opción también se aplica al teclado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
    "Pulsar Salir dos veces"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
    "Pulsa la tecla rápida Salir dos veces para salir de RetroArch."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
    "Muestra mensajes en pantalla."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
    "Usuario remoto %d activado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
    "Mostrar carga de batería"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
    "Mostrar subetiquetas en el menú"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
    "Muestra información adicional sobre la entrada de menú seleccionada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SELECT_FILE,
    "Seleccionar archivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
    "Seleccionar de la lista de reproducción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILTER,
    "Filtro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCALE,
    "Escala"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
    "El juego en red comenzará cuando se cargue el contenido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
    "No se encontró el núcleo o contenido, hay que cargarlo manualmente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
    "Ir a URL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_URL,
    "Dirección URL"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_START,
    "Iniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
    "Bokeh"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
    "Snowflake"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
    "Actualizar lista de salas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
    "Apodo: %s"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
    "Apodo (lan): %s"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
    "Contenido compatible encontrado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
    "Corta unos píxeles de los bordes de la imagen que los desarrolladores suelen dejar en blanco o con basura."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
    "Aplica un ligero desenfoque a la imagen para eliminar los bordes de los píxeles. Esta opción apenas afecta al rendimiento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FILTER,
    "Aplica un filtro de vídeo mediante la CPU.\n"
    "Nota: Puede tener un alto coste de rendimiento. Algunos filtros solo funcionan con núcleos que usan 16 o 32 bits de color."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
    "Introduce el nombre de usuario de tu cuenta de RetroAchievements."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
    "Introduce la contraseña de tu cuenta de RetroAchievements."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
    "Introduce tu apodo. Se utilizará en las sesiones de juego en red y en otros sitios."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
    "Captura la imagen con los filtros (pero sin shaders). La imagen se mostrará tal y como aparezca en pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_LIST,
    "Selecciona el núcleo a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_START_CORE,
    "Ejecuta el núcleo sin un contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
    "Instala un núcleo mediante el actualizador en línea."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
    "Instala o restaura un núcleo desde la carpeta de descargas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
    "Selecciona un contenido a ejecutar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
    "Muestra las interfaces de red e IPs asociadas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
    "Muestra información específica del dispositivo."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Reinicia el programa."
    )
#else
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Salir del programa."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
    "Reinicia el programa."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
    "Establece el ancho de la ventana."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
    "Establece el alto de la ventana."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
    "Recuerda el tamaño y la posición de la ventana, ignorando el valor de Escala en ventana."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
    "Establece el ancho en pantalla completa. En caso de no asignar un valor, se usará la resolución del escritorio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
    "Establece el alto en pantalla completa. En caso de no asignar un valor, se usará la resolución del escritorio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
    "Especifica la posición sobre el eje X para el texto en pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
    "Especifica la posición sobre el eje Y para el texto en pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
    "Especifica el tamaño de la fuente de letra en puntos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
    "Oculta la superposición en el menú y la vuelve a mostrar al salir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
    "Muestra las pulsaciones de los controles en pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
    "Selecciona el puerto que se utilizará al activar la opción «Mostrar entradas en la superposición»."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
    "Muestra el cursor del ratón al utilizar una superposición."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
    "Aquí aparecerán los contenidos que coincidan con la base de datos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
    "Limita el escalado de vídeo a múltiplos enteros. El tamaño base dependerá de la geometría del sistema y la relación de aspecto. Si la opción «Forzar aspecto» está desactivada, los valores X e Y serán escalados individualmente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
    "Las capturas de pantalla mostrarán la imagen con shaders de la GPU si es posible."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
    "Fuerza una cierta rotación de la pantalla. La rotación se añade a la impuesta por el núcleo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
    "Fuerza una cierta orientación de la pantalla respecto a la del sistema operativo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
    "Fuerza la desactivación del soporte de FBO sRGB. Ayuda a algunos controladores OpenGL de Intel que tienen problemas de vídeo en Windows."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
    "Arranca RetroArch en pantalla completa. Esta opción puede ser cambiada en cualquier momento o anulada por un argumento de línea de comandos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
    "Da prioridad al uso de una ventana sin bordes."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
    "Graba la salida de la GPU con shaders, si está disponible."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
    "Al hacer un guardado rápido, se incrementará el índice de forma automática antes de guardar. Al cargar contenido, el índice será el mayor existente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
    "Evita que se sobrescriba la Save RAM al cargar un guardado rápido. Puede provocar fallos en los juegos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
    "Ajusta la velocidad máxima con la que se ejecutará un contenido al usar el avance rápido (p. ej.: 5.0x para un juego de 60 fps = 300 fps). Si el valor es 0, no habrá límite."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
    "Al usar la cámara lenta, el contenido se ralentizará según el factor especificado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
    "Ejecuta la lógica del núcleo uno o más fotogramas por adelantado y luego carga un guardado rápido para reducir la latencia de entrada percibida."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
    "Ajusta el número de fotogramas a predecir. Si el número de fotogramas retrasados supera al valor interno del juego, puede causar tirones."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
    "Indica el número de milisegundos a esperar para conseguir una muestra completa de entrada. Usar si tienes problemas para pulsar varios botones a la vez (solo en Android)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
    "Utiliza una segunda instancia del núcleo para predecir los cambios. Previene los problemas de audio causados al cargar guardados rápidos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
    "Oculta el mensaje de advertencia que aparece al usar la reducción de latencia predictiva si el núcleo no es compatible con los guardados rápidos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_ENABLE,
    "¿Te has equivocado? Rebobina y vuelve a intentarlo.\n"
    "Ten en cuenta que afectará al rendimiento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
    "Aplica los trucos nada más cambiarlos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
    "Aplica automáticamente los trucos nada más cargar el juego."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
    "Indica la cantidad de veces en las que se aplicará el truco.\n"
    "Usar con las otras dos opciones de iteración para cubrir regiones grandes de memoria."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
    "Tras cada «Número de iteraciones», se incrementará la dirección de memoria con este valor multiplicado por el tamaño de búsqueda de memoria."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
    "Tras cada «Número de iteraciones», se incrementará el valor con esta cantidad."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
    "Rebobina un número determinado de fotogramas a la vez para aumentar la velocidad del rebobinado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
    "Asigna una cantidad de memoria (en MB) a reservar para el búfer de rebobinado. Este valor cambiará la longitud del historial de rebobinado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
    "Cada vez que aumentes o disminuyas el tamaño del búfer de rebobinado, cambiará en función de esta cantidad."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_IDX,
    "Posición en el índice de la lista."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
    "Bitmask de la dirección cuando el tamaño de la búsqueda de memoria es menor a 8 bits."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
    "Selecciona una coincidencia."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
    "Izquierda/Derecha para cambiar el bit-size."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
    "Izquierda/Derecha para cambiar el valor."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LTE,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GTE,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQ,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_NEQ,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
    "Izquierda/Derecha para cambiar el valor."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
    "Izquierda/Derecha para cambiar el valor."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_MATCHES,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_VIEW_MATCHES,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_CREATE_OPTION,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DELETE_OPTION,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN,
    "Big endian  : 258 = 0x0102,\n"
    "Little endian : 258 = 0x0201"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
    "Asigna el nivel de registro de los núcleos. Si el valor de un registro del núcleo es inferior a este, será ignorado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
    "Activa los contadores de rendimiento para RetroArch y sus núcleos.\n"
    "Su información puede ayudar a localizar cuellos de botella en el sistema y a perfilar el rendimiento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
    "Hace un guardado rápido al salir de RetroArch. Se cargará automáticamente si la opción «Cargar guardado rápido automáticamente» está activada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
    "Carga el guardado rápido automático al arrancar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
    "Muestra miniaturas de los guardados rápidos en el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
    "Guarda automáticamente la memoria no volátil Save RAM a intervalos regulares (en segundos). Esta función se desactiva seleccionando 0."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
    "Al activar esta opción, se utilizará la reasignación de controles para el núcleo actual en vez de la reasignación estándar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
    "Activa la detección automática de controles, al estilo «Plug and Play»."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
    "Intercambia los botones de Confirmar y Cancelar. Desactivar para usar el estilo japonés o activar para usar el estilo occidental."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
    "Desactiva esta opción para que el contenido siga ejecutándose cuando estés en el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
    "Controlador de vídeo a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
    "Controlador de audio a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_DRIVER,
    "Controlador de entrada a usar. El controlador de vídeo puede forzar uno distinto."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
    "Controlador de mando a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
    "Remuestreador de audio a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
    "Controlador de cámara a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
    "Controlador de localización a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_DRIVER,
    "Controlador de menú a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORD_DRIVER,
    "Controlador de grabación a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_DRIVER,
    "Controlador MIDI a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_WIFI_DRIVER,
    "Controlador wifi a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
    "Muestra únicamente los archivos con extensiones conocidas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
    "Selecciona una imagen para usar de fondo en el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
    "Carga un fondo de pantalla según el contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
    "Fuerza el dispositivo a usar por el controlador de audio. Depende del controlador."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
    "Plugin de audio DSP que procesa el audio antes de enviarlo al controlador."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
    "Velocidad de muestreo de la salida de audio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
    "Controla la transparencia de la superposición."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
    "Controla el tamaño de la superposición."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
    "Las superposiciones sirven para aplicar marcos y mostrar controles en pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
    "Selecciona una superposición en el explorador de archivos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
    "Indica la dirección IP del servidor a conectar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
    "Indica el puerto del servidor a conectar. Puede ser TCP o UDP."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
    "Indica una contraseña a pedir a los clientes que se conecten al estar en modo servidor."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
    "Permite anunciar las sesiones de juego en red de forma pública, de lo contrario, los clientes deberán conectarse manualmente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
    "Contraseña a pedir a los espectadores que se conecten al estar en modo servidor."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
    "Selecciona si se debe iniciar el juego en red en modo espectador."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
    "Indica si se permiten conexiones en modo esclavo. El modo esclavo consume muy pocos recursos, pero afectará en gran medida a la latencia de red."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
    "Forzar conexiones que utilicen el modo esclavo. No se recomienda su uso salvo en redes muy rápidas con máquinas poco potentes."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
    "Indica si se puede ejecutar el juego en red en un modo que no permita guardados rápidos. Necesita una red muy rápida, pero se eliminarán los tirones provocados por el rebobinado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
    "La frecuencia en fotogramas con la que se verificará si el servidor y el cliente están sincronizados."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
    "Cuando se es un servidor, buscar conexiones desde Internet mediante UPnP o tecnologías similares con las que salir de redes locales."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
    "Activa la interfaz de comandos stdin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
    "Permite controlar el menú con el ratón."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_POINTER_ENABLE,
    "Permite controlar el menú con la pantalla táctil."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS,
    "Tipo de miniaturas a mostrar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
    "Indica las miniaturas que se mostrarán en la esquina superior derecha de las listas de reproducción. Podrán mostrarse a pantalla completa pulsando el botón RetroPad Y."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
    "Miniatura principal relacionada con las entradas en las listas de reproducción. Suele ser el icono del contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
    "Tipo de miniaturas a mostrar en la parte izquierda de la pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
    "Tipo de miniaturas a mostrar en la esquina inferior derecha de las listas de reproducción."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
    "Sustituye el panel de metadatos por otra miniatura."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
    "Miniatura auxiliar asociada con las entradas en las listas de reproducción. Se utilizará en función del modo de miniaturas que esté seleccionado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
    "Muestra la miniatura izquierda bajo la que se encuentra en la parte derecha de la pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
    "Muestra la fecha actual en el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
    "Muestra el nivel de batería actual en el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
    "Manda la lista de vuelta al principio cuando se llegue al final horizontal o vertical."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
    "Crea un servidor de juego en red."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
    "Introduce la dirección del servidor de juego de red para conectarte como cliente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
    "Desconecta una sesión activa de juego en red."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
    "Busca contenidos de la base de datos en una carpeta."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_FILE,
    "Busca un archivo de la base de datos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
    "Utiliza un valor personalizado para el intervalo de la sincronía vertical, reduciendo a la mitad la frecuencia de actualización del monitor."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
    "Ordena los archivos de guardado usando carpetas nombradas por núcleo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
    "Ordena los guardados rápidos usando carpetas nombradas por núcleo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
    "Solicita jugar con el dispositivo de entrada indicado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
    "URL de la carpeta del actualizador de núcleos en el buildbot Libretro."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
    "URL de la carpeta de recursos en el buildbot Libretro."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
    "Extrae automáticamente los archivos después de descargarlos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
    "Busca salas nuevas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DELETE_ENTRY,
    "Elimina esta entrada de la lista de reproducción."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INFORMATION,
    "Muestra más información sobre el contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
    "Agrega la entrada a tus favoritos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
    "Agrega la entrada a tus favoritos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN,
    "Ejecuta el contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
    "Configura el explorador de archivos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
    "Carga los controles personalizados al inicio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
    "Carga la configuración personalizada al inicio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
    "Carga la configuración de núcleos personalizada por defecto al inicio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_ENABLE,
    "Muestra el nombre del núcleo actual en el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
    "Muestra las bases de datos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
    "Muestra las últimas búsquedas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
    "Captura una imagen de la pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
    "Cierra el contenido actual. Se perderá cualquier progreso."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_STATE,
    "Carga un guardado rápido de la posición seleccionada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_STATE,
    "Genera un guardado rápido en la posición seleccionada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESUME,
    "Reanuda la ejecución del contenido y abandona el menú rápido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESUME_CONTENT,
    "Reanuda la ejecución del contenido y abandona el menú rápido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STATE_SLOT,
    "Cambia la posición actual de guardado rápido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
    "Si se carga un guardado rápido, el contenido volverá al estado previo a la carga."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
    "Si se sobrescribió un guardado rápido, volverá a tener los datos previos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
    "Servicio de RetroAchievements. Para más información, visita http://retroachievements.org."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
    "Administra las cuentas de usuario."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
    "Administra las opciones de rebobinado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
    "Administra la información de los trucos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
    "Inicia o reanuda una búsqueda de trucos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_CONTENT,
    "Reinicia el contenido desde el principio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
    "Guarda un archivo de personalización que se aplicará a todo el contenido cargado con este núcleo. Tendrá prioridad sobre la configuración principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Guarda un archivo de configuraciones que aplicará a todo el contenido cargado desde el mismo directorio que el archivo actual. Tendrá prioridad sobre el archivo principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
    "Guarda un archivo de personalización que se aplicará solo a este contenido cargado. Tendrá prioridad sobre la configuración principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
    "Configura los trucos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
    "Configura los shaders para mejorar la imagen."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
    "Cambia los controles para el contenido cargado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_OPTIONS,
    "Cambia las opciones del contenido cargado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
    "Muestra opciones avanzadas (ocultas por defecto)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
    "Ejecuta otras tareas en un hilo separado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
    "Permite que el usuario pueda eliminar entradas de las listas de reproducción."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
    "Establece la carpeta del sistema. Los núcleos la utilizan para cargar BIOS, configuraciones de sistema, etc."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
    "Establece la carpeta inicial del explorador de archivos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_DIR,
    "Esta carpeta suele ser utilizada por desarrolladores que empaquetan aplicaciones libretro/RetroArch para indicar la ubicación de los recursos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
    "Carpeta donde se guardan los fondos de pantalla dinámicos según el contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
    "Carpeta de miniaturas adicionales (cajas, capturas, etc.)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
    "Establece la ubicación inicial del explorador del menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
    "Indica el número de fotogramas de entrada a usar para ocultar la latencia durante el juego en red. Reduce los tirones y el uso de la CPU a costa de incrementar la latencia de entrada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
    "Indica el rango de fotogramas de entrada a usar para ocultar la latencia durante el juego en red. Reduce los tirones y el uso de la CPU a costa de incrementar la latencia de entrada."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISK_OPTIONS,
    "Administra las imágenes de disco."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
    "Limita los FPS del menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
    "Evita desviarse del timing que solicita el núcleo. Usar con pantallas con una frecuencia de actualización variable, G-Sync, FreeSync."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_LAYOUT,
    "Selecciona otra disposición para la interfaz XMB."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_THEME,
    "Selecciona otro tema de iconos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
    "Muestra sombras en todos los iconos.\n"
	"Afecta levemente al rendimiento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
    "Selecciona otro color para el fondo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
    "Modifica la opacidad del fondo de pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
    "Selecciona otro color para el fondo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
    "Selecciona un fondo animado. Ciertos efectos pueden consumir recursos de la GPU. Si no estás contento con el rendimiento, desactiva esta opción o utiliza un efecto más sencillo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_FONT,
    "Selecciona el tipo de letra usado en el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
    "Muestra la pestaña de favoritos en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
    "Muestra la pestaña de imágenes en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
    "Muestra la pestaña de música en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
    "Muestra la pestaña de vídeo en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
    "Muestra la pestaña de juego en red en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
    "Muestra la pestaña de configuración en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
    "Muestra la pestaña de historial en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
    "Muestra la pestaña de importar contenido en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
    "Muestra las pestañas de las listas de reproducción en el menú principal."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
    "Muestra la pantalla de inicio en el menú. Esta opción se desactiva automáticamente tras iniciar el programa por primera vez."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
    "Modifica la opacidad del encabezado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
    "Modifica la opacidad del pie de página."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
    "En esta carpeta se guardan los archivos descargados."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
    "En esta carpeta se guardan las reasignaciones de controles."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
    "En esta carpeta se buscan los contenidos o los núcleos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
    "En esta carpeta se guardan los archivos de información de aplicación y de núcleos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
    "Al conectar un mando, se configurará automáticamente si existe un archivo de configuración correspondiente dentro de esta carpeta."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
    "En esta carpeta se guardan las listas de reproducción."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
    "Si seleccionas una carpeta, los contenidos (p. ej.: de otros archivos) se extraerán temporalmente en ella."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
    "En esta carpeta se guarda el historial de búsquedas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
    "En esta carpeta se guardan las bases de datos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
    "Esta carpeta es el lugar predeterminado en el que las interfaces de los menús buscarán los recursos que necesiten."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
    "En esta carpeta se almacenarán todos los archivos de guardado. Si no hay una carpeta asignada, se intentarán guardar en la carpeta del contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
    "En esta carpeta se almacenarán todos los guardados rápidos. Si no hay una carpeta asignada, se intentarán guardar en la carpeta del contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
    "En esta carpeta se guardan las capturas de pantalla."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
    "En esta carpeta se guardan las superposiciones."
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
    "En esta carpeta se guardan los diseños de vídeo."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
    "En esta carpeta se guardan los archivos de trucos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
    "En esta carpeta se guardan los filtros DSP de audio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
    "En esta carpeta se guardan los filtros de vídeo basados en CPU."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
    "En esta carpeta se guardan los filtros de vídeo basados en GPU."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
    "En esta carpeta se guardarán las grabaciones."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
    "En esta carpeta se guardan las configuraciones de grabación."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
    "Selecciona otra fuente de letra para las notificaciones."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
    "Los cambios en la configuración de shaders tendrán efecto inmediatamente. Utiliza esta opción al cambiar la cantidad de pasadas, el filtrado, etc."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
    "Cambia la cantidad de pasadas de shaders. Puedes asignar shaders individualmente a cada pasada y configurar su escalado y filtrado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
    "Carga un preset de shaders. Será configurado automáticamente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
    "Guarda el preset de shaders actual."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
    "Guarda la configuración actual de shaders como un preset nuevo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
    "Guarda la configuración actual de shaders para este núcleo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
    "Guarda la configuración actual de shaders para todos los archivos del directorio de contenido actual."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
    "Guarda la configuración actual de shaders para este contenido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
    "Guarda la configuración actual de shaders para todos los contenidos."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
    "Modifica el shader actual directamente. No se guardarán los cambios en el preset."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
    "Modifica el preset de shader que se usa actualmente en el menú."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
    "Cambia la cantidad de trucos a usar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
    "Los cambios en los trucos tendrán efecto inmediatamente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
    "Inicia la búsqueda de un truco nuevo. Se puede cambiar el número de bits."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
    "Reanuda la búsqueda de un truco nuevo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
    "Carga un archivo de trucos reemplazando los existentes."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
    "Carga un archivo de trucos agregándolos a los existentes."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
    "Guarda los trucos actuales en un archivo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
    "Accede rápidamente a todas las opciones del juego."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_INFORMATION,
    "Muestra información pertinente a la aplicación/núcleo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
    "Valor en coma flotante (ancho/alto) de la relación de aspecto si su valor es «Personalizado»."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
    "Altura personalizada de la imagen. Se utiliza si el valor de la relación de aspecto es «Personalizado»."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
    "Ancho personalizado de la imagen. Se utiliza si el valor de la relación de aspecto es «Personalizado»."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
    "Diferencial de posición respecto al eje X de la pantalla. Si «Escala con valores enteros» está activado, este valor será ignorado y la imagen se centrará automáticamente."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
    "Diferencial de posición respecto al eje X de la pantalla. Si «Escala con valores enteros» está activado, este valor será ignorado y la imagen se centrará automáticamente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
    "Usar servidor de retransmisión"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
    "Envía las conexiones de juego en red a través de otro servidor (man-in-the-middle). Útil si el servidor está tras un cortafuegos o tiene problemas de NAT/UPnP."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
    "Ubicación del servidor de retransmisión"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
    "Elige un servidor concreto. Las ubicaciones geográficamente cercanas suelen tener una latencia más baja."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
    "Agregar al mezclador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
    "Agregar al mezclador y reproducir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
    "Agregar al mezclador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
    "Agregar al mezclador y reproducir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
    "Filtrar por núcleo actual"
    )
MSG_HASH(
    MSG_AUDIO_MIXER_VOLUME,
    "Volumen del mezclador de audio global"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
    "Volumen del mezclador de audio global (en dB). 0 dB es el volumen normal, sin ganancia alguna."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
    "Volumen del mezclador de audio (dB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
    "Silenciar mezclador de audio"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
    "Silencia el mezclador de audio."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
    "Mostrar Actualizador en línea"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
    "Muestra la opción «Actualizador en línea»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
    "Mostrar el antiguo actualizador de miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
    "Permite descargar los paquetes de miniaturas con el sistema antiguo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
    "Vistas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
    "Muestra u oculta elementos del menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
    "Mostrar Actualizador de núcleos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
    "Permite actualizar núcleos y archivos asociados."
    )
MSG_HASH(
    MSG_PREPARING_FOR_CONTENT_SCAN,
    "Preparando búsqueda de contenido..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_DELETE,
    "Borrar núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_DELETE,
    "Elimina este núcleo del disco."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
    "Opacidad del framebuffer"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
    "Modifica la opacidad del framebuffer."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
    "Favoritos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
    "Aquí aparecerán los contenidos agregados a Favoritos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
    "Música"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_MUSIC,
    "Aquí aparecerá la música que se haya reproducido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
    "Imagen"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_IMAGES,
    "Aquí aparecerán las imágenes que se hayan visualizado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
    "Vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_VIDEO,
    "Aquí aparecerán los vídeos que se hayan reproducido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
    "Iconos del menú"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
    "Muestra iconos a la izquierda de las entradas del menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
    "Optimizar disposición en modo horizontal"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
    "Ajusta automáticamente la disposición del menú para que se adapte a la pantalla con la orientación horizontal."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
    "OFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
    "ON"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
    "Excluir imágenes en miniatura"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
    "Autorotar barra de navegación"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
    "Desplaza automáticamente la barra de navegación a la parte derecha de la pantalla con la orientación horizontal."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
    "Mostrar miniaturas secundarias en las listas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
    "Permite mostrar una miniatura secundaria al utilizar los formatos tipo lista de miniaturas en las listas de reproducción. Aviso: Este ajuste sólo tendrá efecto cuando la pantalla tenga el ancho necesario para mostrar dos miniaturas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
    "Generar fondos de miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
    "Permite cubrir el espacio no utilizado en las miniaturas con un fondo plano. Esto garantiza que todas las imágenes tengan un tamaño uniforme, mejorando la apariencia de los menús al ver miniaturas de contenidos y dimensiones base distintos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
    "Pestaña de opciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
    "Establecer contraseña para activar la pestaña de opciones"
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
    "Introduce una contraseña"
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
    "Contraseña correcta."
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
    "Contraseña incorrecta."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
    "Activa la pestaña de opciones. Es necesario reiniciar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
    "Permite usar una contraseña para ocultar la pestaña de opciones. Esto permite restaurarla desde el menú principal activando «Pestaña de opciones»."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
    "Permite al usuario cambiar el nombre de las entradas en las listas de reproducción."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
    "Permitir renombrar entradas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RENAME_ENTRY,
    "Cambia el título de esta entrada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
    "Cambiar nombre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
    "Mostrar Cargar núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
    "Muestra la opción «Cargar núcleo»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
    "Mostrar Cargar contenido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
    "Muestra la opción «Cargar contenido»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
    "Mostrar Cargar disco"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
    "Muestra la opción «Cargar disco»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
    "Mostrar Volcar disco"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
    "Muestra la opción «Volcar disco»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
    "Mostrar Información"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
    "Muestra la opción «Información»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
    "Mostrar Configuración"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
    "Muestra la opción «Configuración»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
    "Mostrar Ayuda"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
    "Muestra la opción «Ayuda»."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "Mostrar Reiniciar RetroArch"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "Muestra la opción «Reiniciar RetroArch»."
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "Mostrar Cerrar RetroArch"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "Muestra la opción «Cerrar RetroArch»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
    "Mostrar Reiniciar RetroArch"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
    "Muestra la opción «Reiniciar RetroArch»."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
    "Mostrar Reiniciar"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
    "Muestra la opción «Reiniciar»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
    "Mostrar Apagar"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
    "Muestra la opción «Apagar»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
    "Menú rápido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
    "Muestra elementos del menú rápido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
    "Mostrar Captura de pantalla"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
    "Muestra la opción «Captura de pantalla»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
    "Mostrar Guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
    "Muestra la opción «Guardado rápido»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
    "Mostrar Deshacer guardado rápido/carga rápida"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
    "Muestra las opciones que permiten deshacer la carga y el guardado rápido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
    "Mostrar Agregar a favoritos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
    "Muestra la opción «Agregar a favoritos»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
    "Mostrar Comenzar grabación"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
    "Muestra la opción «Comenzar grabación»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
    "Mostrar Comenzar streaming"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
    "Muestra la opción «Comenzar streaming»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
    "Mostrar Asignar asociación de núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
    "Muestra la opción «Asignar asociación de núcleo»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
    "Mostrar Restablecer asociación de núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
    "Muestra la opción «Restablecer asociación de núcleo»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
    "Mostrar Opciones"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
    "Muestra la opción «Opciones»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
    "Mostrar Controles"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
    "Muestra la opción «Controles»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
    "Mostrar Trucos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
    "Muestra la opción «Trucos»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
    "Mostrar Shaders"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
    "Muestra la opción «Shaders»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
    "Mostrar Guardar personalizaciones del núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
    "Muestra la opción «Guardar personalizaciones de núcleo»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
    "Mostrar Guardar personalizaciones de juego"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
    "Muestra la opción «Guardar personalizaciones de juego»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
    "Mostrar Información"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
    "Muestra la opción «Información»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
    "Mostrar Descargar miniaturas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
    "Muestra la opción «Descargar miniaturas»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
    "Fondo de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
    "Color rojo del fondo de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
    "Color verde del fondo de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
    "Color azul del fondo de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
    "Opacidad del fondo de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
    "Desactivar modo quiosco"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
    "Desactiva el modo quiosco. Es necesario reiniciar."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
    "Modo quiosco"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
    "Evita que se pueda modificar la configuración."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
    "Establecer contraseña para desactivar modo quiosco"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
    "Al usar una contraseña para activar el modo quiosco, es posible desactivarlo desde el menú principal desde «Desactivar modo quiosco»."
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD,
    "Introduce una contraseña"
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
    "Contraseña correcta."
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
    "Contraseña incorrecta."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
    "Color rojo de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
    "Color verde de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
    "Color azul de notificaciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
    "Mostrar contador de fotogramas"
    )
MSG_HASH(
    MSG_CONFIG_OVERRIDE_LOADED,
    "Configuración personalizada cargada."
    )
MSG_HASH(
    MSG_GAME_REMAP_FILE_LOADED,
    "Archivo de reasignaciones de juego cargado."
    )
MSG_HASH(
    MSG_CORE_REMAP_FILE_LOADED,
    "Archivo de reasignaciones de núcleo cargado."
    )
MSG_HASH(
    MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
    "La reducción predictiva fue desactivada porque este núcleo no es compatible con guardados rápidos."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
    "Error al guardar rápidamente. La reducción predictiva ha sido desactivada."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
    "Error al cargar el guardado rápido. La reducción predictiva ha sido desactivada."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
    "Error al crear la segunda instancia. La reducción predictiva solo usará una a partir de este momento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
    "Autoagregar contenidos a listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
    "Busca automáticamente los contenidos para que aparezcan en las listas de reproducción."
    )
MSG_HASH(
    MSG_SCANNING_OF_FILE_FINISHED,
    "Archivo escaneado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
    "Opacidad de la ventana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
    "Calidad del muestreo de audio"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
    "Baja este valor para favorecer el rendimiento y la latencia a costa de la calidad o súbelo para mejorar la calidad a costa de perder rendimiento y latencia."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
    "Comprobar cambios en shaders"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
    "Aplica automáticamente los cambios hechos a los archivos shader del disco."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
    "Mostrar decoraciones de ventanas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
    "Mostrar estadísticas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
    "Muestra las estadísticas técnicas en pantalla."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
    "Activar relleno de borde"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
    "Muestra el borde del menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
    "Activar ancho del relleno de borde"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
    "Aumenta la aspereza del efecto de cuadrícula del borde del menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
    "Ancho del relleno de fondo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
    "Aumenta la aspereza del efecto de cuadrícula del fondo del menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
    "Fijar relación de aspecto del menú"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
    "Preserva la relación de aspecto correcta del menú. Al desactivar esta opción, se estirará el menú rápido para adaptarse al contenido cargado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
    "Reescalado interno"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
    "Reescala el menú antes de mostrarlo en pantalla. Si se utiliza cuando «Filtro lineal del menú» está activado, se eliminarán los defectos de escalado (píxeles no proporcionales) manteniendo una imagen nítida. Afecta al rendimiento en función del nivel de reescalado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
    "Relación de aspecto del menú"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
    "Selecciona la relación de aspecto del menú. Una relación panorámica aumenta la resolución horizontal del menú (Puede ser necesario reiniciar si la opción «Fijar relación de aspecto del menú» está desactivada)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
    "Usar disposición de ancho completo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
    "Reescala y reubica las entradas del menú para aprovechar al máximo el espacio disponible. Desactivar para utilizar la disposición clásica de doble columna."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
    "Sombras"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
    "Mostrar sombras para los textos, bordes y miniaturas del menú. Afecta levemente al rendimiento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
    "Animación de fondo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
    "Activa el efecto de partículas animadas de fondo. Afecta medianamente al rendimiento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
    "Desactivar"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
    "Nieve (Escasa)"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
    "Nieve (Fuerte)"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
    "Lluvia"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
    "Vórtice"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
    "Estrellas"
)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
    "Velocidad de animación del fondo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
    "Ajusta la velocidad de los efectos de partículas de la animación de fondo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
    "Soporte para ASCII extendido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
    "Permite mostrar caracteres ajenos al estándar ASCII. Es necesario para la compatibilidad con ciertos idiomas occidentales. Afecta moderadamente al rendimiento."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
    "Para pantallas CRT. Intenta usar la resolución y frecuencia de actualización exactas del núcleo/juego."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
    "SwitchRes para CRT"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
    "Alterna entre las resoluciones nativas y las ultrapanorámicas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
    "Ultraresolución CRT"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
    "Mostrar opciones de rebobinado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
    "Muestra las opciones de rebobinado."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
    "Muestra las opciones de latencia."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
    "Mostrar opciones de latencia"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
    "Muestra las opciones de superposición."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
    "Mostrar opciones de superposición"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
    "Mostrar opciones de diseño de vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
    "Muestra las opciones de diseño de vídeo."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
    "Mezclador de audio"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
    "Reproduce varias pistas de audio a la vez, incluso dentro del menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
    "Opciones del mezclador"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
    "Modifica las opciones del mezclador de audio."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_INFO,
    "Información"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
    "&Archivo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
    "&Cargar núcleo..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
    "&Descargar núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
    "&Salir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
    "&Editar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
    "&Buscar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
    "&Ver"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
    "Docks cerrados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
    "Parámetros del shader"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
    "&Opciones..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
    "Recordar las posiciones de dock:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
    "Recordar la geometría de la ventana:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
    "Recordar la última pestaña de contenido:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
    "Tema:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
    "<Por defecto>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
    "Oscuro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
    "Personalizado..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
    "Opciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
    "Herramien&tas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
    "A&yuda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
    "Acerca de RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
    "Documentación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
    "Cargar núcleo personalizado..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
    "Cargar núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
    "Cargando núcleo..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NAME,
    "Nombre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
    "Versión"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
    "Listas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
    "Explorador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
    "Inicio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
    "Arriba"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
    "Explorador de contenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
    "Caja"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
    "Captura de pantalla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
    "Pantalla de título"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
    "Todas las listas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
    "Información del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
    "<Preguntarme>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
    "Información"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_WARNING,
    "Advertencia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ERROR,
    "Error"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
    "Error de red"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
    "Por favor, reinicia el programa para aplicar los cambios."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOG,
    "Registro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
    "%1 elemento(s)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
    "Arrastra una imagen aquí"
    )
#ifdef HAVE_QT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
    "Búsqueda finalizada.<br><br>\n"
    "Para que un contenido pueda ser identificado, es necesario lo siguiente:\n"
    "<ul><li>Tener un núcleo compatible descargado</li>\n"
    "<li>Tener los archivos de información de núcleos actualizados</li>\n"
    "<li>Tener las bases de datos actualizadas</li>\n"
    "<li>Reiniciar RetroArch si has usado el actualizador en línea para actualizar alguno de estos elementos</li></ul>\n"
    "Por último, el contenido debe coincidir con las bases de datos que se encuentran <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">aquí</a>. Si aún no consigues resultados, puedes <a href=\"https://www.github.com/libretro/RetroArch/issues\">enviar un informe de error.</a>"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
    "Mostrar menú de escritorio"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_WIMP,
    "Abre el menú de escritorio."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
    "No volver a mostrar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_STOP,
    "Detener"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
    "Asociar núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
    "Listas ocultas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_HIDE,
    "Ocultar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
    "Color de resaltado:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
    "&Elegir..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
    "Seleccionar color"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
    "Seleccionar tema"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
    "Tema personalizado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
    "La ruta al archivo está en blanco."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
    "El archivo está vacío."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
    "No se pudo leer el archivo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
    "No se pudo escribir el archivo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
    "El archivo no existe."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
    "Sugerir el primer núcleo a cargar:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ZOOM,
    "Zoom"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW,
    "Vista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
    "Iconos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
    "Lista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
    "Personalizaciones"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
    "Opciones para anteponer configuraciones personalizadas sobre las globales."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
    "Inicia la reproducción de audio. Al finalizar, será borrado de la memoria."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
    "Inicia la reproducción de audio. Al finalizar, será reproducido de nuevo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
    "Inicia la reproducción de audio. Al finalizar, se continuará con el siguiente, útil para álbumes."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
    "Detiene la reproducción sin borrar el audio de la memoria. Puedes continuar la reproducción."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
    "Detiene la reproducción y borra el audio de la memoria."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
    "Ajusta el volumen del audio."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
    "Agrega esta pista de audio a una casilla. Si no hay disponibles, se ignorará."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
    "Agrega esta pista de audio a una casilla y la reproduce. Si no hay disponibles, se ignorará."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
    "Reproducir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
    "Reproducir (Repetir)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
    "Reproducir (Secuencial)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
    "Detener"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
    "Quitar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
    "Volumen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
    "Núcleo actual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
    "Limpiar"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
    "Pausa los logros en esta sesión (Se activarán los guardados rápidos, los trucos, el rebobinado, la pausa y la cámara lenta)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
    "Reanuda los logros en esta sesión (Se desactivarán los guardados rápidos, los trucos, el rebobinado, la pausa y la cámara lenta)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
    "En el menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
    "Dentro del juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
    "Dentro del juego (Pausado)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
    "Jugando"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
    "Pausado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
    "Discord Rich Presence"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
    "Permite que la aplicación Discord pueda mostrar más información sobre el contenido que estés utilizando.\n"
    "NOTA: Esta función solo funcionará con el cliente de escritorio, no con la versión web."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
    "Entrada"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_INPUT,
    "Selecciona un dispositivo de entrada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
    "Salida"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
    "Selecciona un dispositivo de salida."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
    "Volumen"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_VOLUME,
    "Ajusta el volumen de salida (en porcentaje)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
    "Energía"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
    "Cambia las opciones de energía."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
    "Modo de rendimiento sostenido"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
    "Soporte de mpv"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
    "Índice"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
    "Ver coincidencia n.º "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
    "Coincidir dirección: %08X Máscara: %02X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
    "Crear coincidencia de truco n.º "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
    "Borrar coincidencia n.º "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
    "Examinar dirección: %08X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
    "Descripción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
    "Activado"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
    "Truco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
    "Manipulador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
    "Tamaño de la memoria de búsqueda"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
    "Tipo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
    "Valor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
    "Dirección de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
    "Máscara de la dirección de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
    "Vibrar cuando memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
    "Valor de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
    "Puerto de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
    "Fuerza primaria de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
    "Duración (ms) de la vibración primaria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
    "Fuerza secundaria de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
    "Duración (ms) de la vibración secundaria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
    "Número de iteraciones"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
    "Incrementar valor con cada iteración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
    "Incrementar dirección con cada iteración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
    "Agregar un truco nuevo después de este"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
    "Agregar un truco nuevo antes de este"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
    "Copiar este truco después"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
    "Copiar este truco antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
    "Borrar este truco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
    "Emulador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_RETRO,
    "RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
    "<Desactivado>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
    "Establecer valor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
    "Aumentar por valor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
    "Disminuir por valor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
    "Ejecutar siguiente truco si el valor es igual a la memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
    "Ejecutar siguiente truco si el valor es distinto a la memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
    "Ejecutar el siguiente truco si el valor es menor a la memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
    "Ejecutar el siguiente truco si el valor es mayor a la memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
    "<Desactivado>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
    "Cambios"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
    "No cambia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
    "Aumenta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
    "Disminuye"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
    "Igual al valor de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
    "Distinto al valor de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
    "Menor al valor de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
    "Mayor al valor de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
    "Incrementa por el valor de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
    "Disminuye por el valor de vibración"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
    "1 bit, valor máx. = 0x01"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
    "2 bits, valor máx. = 0x03"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
    "4 bits, valor máx. = 0x0F"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
    "8 bits, valor máx. = 0xFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
    "16 bits, valor máx. = 0xFFFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
    "32 bits, valor máx. = 0xFFFFFFFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_0,
    "1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_1,
    "2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_2,
    "3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_3,
    "4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_4,
    "5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_5,
    "6"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_6,
    "7"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_7,
    "8"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_8,
    "9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_9,
    "10"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_10,
    "11"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_11,
    "12"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_12,
    "13"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_13,
    "14"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_14,
    "15"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_15,
    "16"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_16,
    "Todos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
    "Iniciar o continuar búsqueda de trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
    "Iniciar o reiniciar búsqueda de trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
    "Buscar valores de memoria"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
    "Agregar %u coincidencias a tu lista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
    "Ver lista de %u coincidencias"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
    "Crear truco a partir de esta coincidencia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
    "Borrar esta coincidencia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
    "Agregar nuevo truco al principio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
    "Agregar nuevo truco al final"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
    "Borrar todos los trucos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
    "Recargar trucos específicos del juego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
    "Igual a %u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
    "Menos que antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
    "Más que antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
    "Menos o igual que antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
    "Más o igual que antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
    "Igual que antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
    "Distinto que antes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
    "Igual que antes + %u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
    "Igual que antes - %u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
    "Iniciar o continuar búsqueda de trucos"
    )
MSG_HASH(
    MSG_CHEAT_INIT_SUCCESS,
    "Búsqueda de trucos iniciada"
    )
MSG_HASH(
    MSG_CHEAT_INIT_FAIL,
    "Error al iniciar búsqueda de trucos"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_NOT_INITIALIZED,
    "La búsqueda no ha sido iniciada"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_FOUND_MATCHES,
    "Número de coincidencias = %u"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
    "Big Endian"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
    "Agregadas %u coincidencias"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
    "Error al agregar coincidencias"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
    "Truco creado a partir de coincidencia"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
    "Error al crear truco"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
    "Borrar coincidencia"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
    "No hay suficiente espacio. El máximo es 100 trucos."
    )
MSG_HASH(
    MSG_CHEAT_ADD_TOP_SUCCESS,
    "Nuevo truco agregado al inicio de la lista."
    )
MSG_HASH(
    MSG_CHEAT_ADD_BOTTOM_SUCCESS,
    "Nuevo truco agregado al final de la lista."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
    "Presiona derecha cinco veces para borrar todos los trucos."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_ALL_SUCCESS,
    "Todos los trucos fueron borrados."
    )
MSG_HASH(
    MSG_CHEAT_ADD_BEFORE_SUCCESS,
    "Nuevo truco agregado antes de este."
    )
MSG_HASH(
    MSG_CHEAT_ADD_AFTER_SUCCESS,
    "Nuevo truco agregado después de este."
    )
MSG_HASH(
    MSG_CHEAT_COPY_BEFORE_SUCCESS,
    "Truco copiado antes de este."
    )
MSG_HASH(
    MSG_CHEAT_COPY_AFTER_SUCCESS,
    "Truco copiado después de este."
    )
MSG_HASH(
    MSG_CHEAT_DELETE_SUCCESS,
    "Truco borrado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
    "Progreso:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
    "N.º máximo de entradas en lista de «Todas las listas»:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
    "N.º máximo de entradas en rejilla de «Todas las listas»:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
    "Mostrar elementos ocultos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
    "Lista de reproducción nueva"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
    "Introduce el nombre de la lista nueva:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
    "Borrar lista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
    "Renombrar lista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
    "¿Seguro que quieres borrar la lista «%1»?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_QUESTION,
    "Pregunta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
    "No se pudo borrar el archivo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
    "No se pudo renombrar el archivo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
    "Cargando lista de archivos..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
    "Agregando archivos a la lista..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
    "Entrada de la lista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
    "Nombre:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
    "Ruta:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
    "Núcleo:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
    "Base de datos:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
    "Extensiones:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
    "(separar con espacios; incluye a todas por defecto)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
    "Filtrar archivos internos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
    "(para buscar miniaturas)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
    "¿Seguro que quieres borrar «%1»?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
    "Primero debes elegir una lista."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DELETE,
    "Borrar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
    "Agregar entrada..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
    "Agregar archivo(s)..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
    "Agregar carpeta..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_EDIT,
    "Editar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
    "Seleccionar archivos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
    "Seleccionar carpeta"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
    "<múltiples>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
    "Error al actualizar la entrada de la lista."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
    "Rellena todos los campos obligatorios."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
    "Actualizar RetroArch (nightly)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
    "RetroArch ha sido actualizado. Reinicie la aplicación para aplicar los cambios."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
    "Error al actualizar."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
    "Contribuyentes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
    "Shader actual"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
    "Mover abajo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
    "Mover arriba"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_LOAD,
    "Cargar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SAVE,
    "Guardar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_REMOVE,
    "Quitar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
    "Quitar pasadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_APPLY,
    "Aplicar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
    "Agregar pasada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
    "Quitar todas las pasadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
    "No hay pasadas de shader."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
    "Restablecer pasada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
    "Restablecer todas las pasadas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
    "Restablecer parámetro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
    "Descargar miniatura"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
    "Ya hay una descarga en curso."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
    "Empezar por esta lista:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
    "Miniatura"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
    "Límite de caché de miniaturas:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
    "Descargar todas las miniaturas"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
    "Sistema completo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
    "Esta lista"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
    "Miniaturas descargadas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
    "Obtenidas: %1 Fallos: %2"
    )
MSG_HASH(
    MSG_DEVICE_CONFIGURED_IN_PORT,
    "Configurado en puerto:"
    )
MSG_HASH(
    MSG_FAILED_TO_SET_DISK,
    "Error al asignar disco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
    "Opciones del núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
    "Vsync adaptativa"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
    "La sincronía vertical estará activada hasta que el rendimiento sea inferior al necesario para mantener la frecuencia de actualización de la pantalla.\n"
    "Esta opción reduce los tirones provocados al bajar los FPS y tiene una mayor eficiencia energética."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
    "SwitchRes en CRT"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
    "Generar señales de vídeo de baja resolución para pantallas CRT."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
    "Modifica esta opción si la imagen no está centrada en la pantalla."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
    "Centrado del eje X"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
    "Utiliza una frecuencia de actualización personalizada especificada en el archivo de configuración."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
    "Usar frecuencia personalizada"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
    "Selecciona el puerto de salida conectado a la pantalla CRT."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
    "ID de la pantalla de salida"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
    "Comenzar grabación"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
    "Comienza la grabación."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
    "Detener grabación"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
    "Detiene la grabación."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
    "Comenzar streaming"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
    "Comienza el streaming."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
    "Detener streaming"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
    "Detiene el streaming."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
    "Comenzar/detener grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
    "Comenzar/detener streaming"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
    "Servicio de IA"
    )
MSG_HASH(
    MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
    "Se ha cargado un guardado rápido y se han desactivado los logros Hardcore en esta sesión. Reinicia para volver a habilitarlos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
    "Calidad de grabación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
    "Calidad del stream"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_URL,
    "URL del stream"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
    "Puerto UDP para el stream"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH,
    "Twitch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE,
    "YouTube"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
    "Clave de streaming de Twitch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
    "Clave de streaming de YouTube"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
    "Modo de streaming"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
    "Título del stream"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
    "Joy-Con separados"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
    "Restablecer valores predeterminados"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
    "Cambia la configuración actual a los valores predeterminados."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
    "Aceptar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
    "Color del tema del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
    "Blanco básico"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
    "Negro básico"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
    "Selecciona otro color."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
    "Ocultar barra lateral"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
    "Mantener la barra lateral izquierda oculta en todo momento."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
    "Partir nombres de lista de reproducción"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
    "Quita los nombres de los sistemas de las listas de reproducción (p. ej.: mostrará «PlayStation» en lugar de «Sony - PlayStation»). Es necesario reiniciar."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
    "Reducir textos largos de los metadatos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
    "Al activar esta opción, cada elemento de los metadatos de un contenido que se muestren en la barra derecha de las listas de reproducción (núcleo asociado, tiempo de juego...) ocupará únicamente una línea, las cadenas que superen el ancho de la barra se desplazarán automáticamente. Al desactivar esta opción, cada elemento de los metadatos se mostrará de forma estática, extendiéndose las líneas que sean necesarias."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
    "Usar los colores del sistema"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
    "Utiliza la paleta de colores del sistema operativo (si está disponible)."
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_LOWEST,
    "Muy baja"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_LOWER,
    "Baja"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_NORMAL,
    "Normal"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_HIGHER,
    "Alta"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_HIGHEST,
    "Ultra"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
    "No hay música disponible."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
    "No hay vídeos disponibles."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
    "No hay imágenes disponibles."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
    "No hay favoritos disponibles."
    )
MSG_HASH(
    MSG_MISSING_ASSETS,
    "ADVERTENCIA: Faltan recursos, utiliza el actualizador en línea si está disponible"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
    "Recordar posición y tamaño de la ventana"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HOLD_START,
    "Mantener Start (2 segundos)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
    "Usar el antiguo formato de listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
    "Mostrar núcleos asociados en las listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
    "Indica si se deben asociar entradas en las listas de reproducción con el núcleo actual asociado (si existe). AVISO: Este ajuste se ignorará si se activan las subetiquetas en las listas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
    "Historial y favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
    "Siempre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
    "Nunca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
    "Historial y favoritos"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
    "Todas las listas de reproducción"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
    "Desactivar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
    "Ordenar listas por orden alfabético"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
    "Ordena las listas de contenidos por orden alfabético. Esta opción no se aplica a los historiales de elementos reproducidos recientemente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
    "Sonidos del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
    "Activar sonido de confirmación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
    "Activar sonido de cancelación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
    "Activar sonido de notificación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
    "Activar música de fondo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
    "Abajo + Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
    "Tu controlador de gráficos no es compatible con el controlador de vídeo actual de RetroArch, que pasa a utilizar %s. Reinicia RetroArch para aplicar los cambios."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
    "Soporte de CoreAudio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
    "Soporte de CoreAudio V3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
    "Guardar registro de ejecución (por núcleo)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
    "Registra el tiempo de ejecución de cada contenido, separando los registros según el núcleo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
    "Guardar registro de ejecución (acumulativo)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
    "Registra el tiempo de ejecución de cada contenido de forma conjunta entre todos los núcleos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
    "Registros de ejecución"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
    "En esta carpeta se guardan los registros de ejecución."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
    "Mostrar subetiquetas de listas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
    "Muestra información adicional en cada entrada de las listas de reproducción, como la asociación de núcleos (si está disponible). Afecta al rendimiento de forma variable."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
    "Núcleo:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
    "Tiempo de juego:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
    "Última partida:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
    "Ejecución de subetiquetas de listas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
    "Selecciona el tipo de registro de ejecución que se mostrará en las subetiquetas de las listas de reproducción (El registro correspondiente debe ser activado en las opciones de Guardado)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
    "Por núcleo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
    "Acumulativo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
    "Formato de «Última partida» en subetiquetas de listas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
    "Selecciona el formato de fecha y hora para la información de la última partida jugada al mostrar el registro de ejecución. Nota: Las opciones AM/PM afectarán levemente al rendimiento en algunas plataformas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_YMD_HMS,
    "AAAA/MM/DD - HH:MM:SS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_YMD_HM,
    "AAAA/MM/DD - HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_MDYYYY,
    "MM/DD/AAAA - HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_DM_HM,
    "DD/MM - HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_MD_HM,
    "MM/DD - HH:MM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_YMD_HMS_AM_PM,
    "AAAA/MM/DD - HH:MM:SS (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_YMD_HM_AM_PM,
    "AAAA/MM/DD - HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_MDYYYY_AM_PM,
    "MM/DD/AAAA - HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_DM_HM_AM_PM,
    "DD/MM - HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE_MD_HM_AM_PM,
    "MM/DD - HH:MM (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
    "Búsqueda difusa de archivos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
    "Al buscar entradas asociadas a archivos comprimidos en las listas de reproducción, solo se buscará una coincidencia en el nombre del archivo, en vez de [nombre de archivo]+[contenido]. Activar para evitar duplicados en el historial al cargar archivos comprimidos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
    "Enviar información de depuración"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_DEBUG_INFO,
    "Error al guardar la información de depuración."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_DEBUG_INFO,
    "Error al enviar la información de depuración al servidor."
    )
MSG_HASH(
    MSG_SENDING_DEBUG_INFO,
    "Enviando información de depuración..."
    )
MSG_HASH(
    MSG_SENT_DEBUG_INFO,
    "Información enviada al servidor. Tu identificador es %u."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
    "Envía información de diagnóstico sobre tu dispositivo y tu configuración de RetroArch a nuestros servidores para su posterior análisis."
    )
MSG_HASH(
    MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
    "Vuelve a pulsar el botón dos veces para enviar la información de diagnóstico al equipo de RetroArch."
    )
MSG_HASH(
    MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
    "Vuelve a pulsar el botón una última vez para enviar la información de diagnóstico al equipo de RetroArch."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
    "Vibrar al pulsar un botón"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
    "Activar vibración de dispositivo (en núcleos compatibles)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_DIR,
    "Registros del sistema"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_DIR,
    "En esta carpeta se guardan los archivos de registros del sistema."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
    "Widgets del menú"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
    "Shaders de vídeo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
    "Buscar sin usar coincidencias del núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
    "Al desactivar esta opción, solo se añadirán contenidos a las listas de reproducción si se ha instalado un núcleo compatible con sus extensiones. Al activar esta opción se añadirán a la lista de reproducción sin hacer distinciones. Así podrás instalar un núcleo necesario después de buscar contenidos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
    "Resaltado de iconos horizontales en animación"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
    "Animación de subida/bajada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
    "Animación al abrir/cerrar menú principal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
    "Índice de la GPU"
    )
MSG_HASH(
    MSG_DUMPING_DISC,
    "Volcando disco..."
    )
MSG_HASH(
    MSG_DRIVE_NUMBER,
    "Unidad %d"
    )
MSG_HASH(
    MSG_LOAD_CORE_FIRST,
    "Primero debes cargar un núcleo."
    )
MSG_HASH(
    MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
    "Error al leer desde la unidad. Se ha abortado el volcado."
    )
MSG_HASH(
    MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
    "Error al escribir a disco. Se ha abortado el volcado."
    )
MSG_HASH(
    MSG_NO_DISC_INSERTED,
    "No hay un disco en la unidad."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
    "Información del disco"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISC_INFORMATION,
    "Muestra la información de los discos introducidos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET,
    "Reiniciar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
    "Reiniciar todo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
    "Nivel de registro de la interfaz"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
    "Ajusta el nivel de registro de la interfaz. Si el valor de un registro es inferior a este, será ignorado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
    "Intervalo de actualización de FPS (fotogramas)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
    "Ajusta la velocidad de actualización del contador de FPS en pantalla (en fotogramas)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Mostrar Reiniciar contenido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
    "Muestra la opción «Reiniciar contenido»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Mostrar Cerrar contenido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "Muestra la opción «Cerrar contenido»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Mostrar Reanudar contenido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
    "Muestra la opción «Reanudar contenido»."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
    "Opciones"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
    "Muestra elementos de la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
    "Mostrar Controles"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
    "Muestra la opción «Controles» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
    "Accesibilidad"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
    "Cambia la configuración del narrador de accesibilidad."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
    "Servicio de IA"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
    "Cambia la configuración del servicio de IA (Traducción/texto a voz/etc.)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
    "Formato de salida del servicio de IA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
    "URL del servicio de IA"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
    "Activar servicio de IA"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
    "Muestra la traducción como una superposición de texto (Imagen) o reproducir una conversión de texto a voz (modo Voz)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
    "Indica una dirección URL que lleve al servicio de traducción que quieras utilizar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
    "Inicia el servicio de IA cuando se pulse la tecla rápida del servicio de IA."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
    "Idioma de destino"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
    "Indica el idioma de destino de la traducción. En caso de seleccionar «No importa» se traducirá a inglés."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
    "Idioma de origen"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
    "El idioma que utilizará el servicio de traducción. En caso de seleccionar «No importa», se intentará detectar el idioma automáticamente. Al seleccionar un idioma concreto, la traducción será más precisa."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CZECH,
    "Checo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DANISH,
    "Danés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SWEDISH,
    "Sueco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CROATIAN,
    "Croata"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CATALAN,
    "Catalán"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN,
    "Búlgaro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_BENGALI,
    "Bengalí"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_BASQUE,
    "Vasco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI,
    "Azerbaiyán"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN,
    "Albanés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS,
    "Africaans"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN,
    "Estonio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_FILIPINO,
    "Filipino"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_FINNISH,
    "Finlandés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GALICIAN,
    "Gallego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN,
    "Georgiano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GUJARATI,
    "Gujarati"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE,
    "Criollo haitiano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
    "Hebreo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_HINDI,
    "Hindi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN,
    "Húngaro"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC,
    "Islandés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN,
    "Indonesio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_IRISH,
    "Irlandés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_KANNADA,
    "Kannada"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_LATIN,
    "Latín"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_LATVIAN,
    "Letón"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN,
    "Lituano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN,
    "Macedonio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_MALAY,
    "Malayo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_MALTESE,
    "Maltés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN,
    "Noruego"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PERSIAN,
    "Persa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN,
    "Rumano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SERBIAN,
    "Serbio"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SLOVAK,
    "Eslovaco"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN,
    "Esloveno"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SWAHILI,
    "Suajili"
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
    "Tailandés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN,
    "Ucraniano"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_URDU,
    "Urdu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_WELSH,
    "Galés"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_YIDDISH,
    "Yídish"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
    "Mostrar Controladores"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
    "Muestra la opción «Controladores» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
    "Mostrar Vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
    "Muestra la opción «Vídeo» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
    "Mostrar Audio"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
    "Muestra la opción «Audio» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
    "Mostrar Latencia"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
    "Muestra la opción «Latencia» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
    "Mostrar Núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
    "Muestra la opción «Núcleo» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
    "Mostrar Configuración"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
    "Muestra la opción «Configuración» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
    "Mostrar Guardado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
    "Muestra la opción «Guardado» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
    "Mostrar Registros"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
    "Muestra la opción «Registros» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
    "Mostrar Limitador de fotogramas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
    "Muestra la opción «Limitador de fotogramas» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
    "Mostrar Grabación"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
    "Muestra la opción «Grabación» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
    "Mostrar Información en pantalla (OSD)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
    "Muestra la opción «Información en pantalla (OSD)» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
    "Mostrar Interfaz de usuario"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
    "Muestra la opción «Interfaz de usuario» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
    "Mostrar Servicio de IA"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
    "Muestra la opción «Servicio de IA» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
    "Mostrar Energía"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
    "Muestra la opción «Energía» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
    "Mostrar Logros"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
    "Muestra la opción «Logros» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
    "Mostrar Red"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
    "Muestra la opción «Red» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
    "Mostrar Listas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
    "Muestra la opción «Listas de reproducción» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
    "Mostrar Usuario"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
    "Muestra la opción «Usuario» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
    "Mostrar Carpeta"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
    "Muestra la opción «Carpeta» en la pantalla de Opciones."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_DISC,
    "Carga un medio físico. Antes de nada, deberías cargar el núcleo que pretendes utilizar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DUMP_DISC,
    "Vuelca los contenidos del medio físico al almacenamiento interno. Se guardarán como un archivo de imagen."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
    "Modo Imagen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
    "Modo Voz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
    "Modo Narrador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
    "Eliminar"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
    "Elimina los presets de shaders de tipos concretos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
    "Eliminar preset global"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
    "Elimina el preset global, usado por todos los contenidos y núcleos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
    "Eliminar preset de núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
    "Elimina el preset del núcleo, usado por todos los contenidos compatibles con el núcleo cargado actualmente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
    "Eliminar preset de carpeta de contenidos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
    "Elimina el preset de la carpeta de contenidos, usado por todos los contenidos de la carpeta usada actualmente."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
    "Eliminar preset de juego"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
    "Elimina el preset de juego, usado solo por el juego seleccionado."
    )
MSG_HASH(
    MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
    "Preset de shaders eliminado."
    )
MSG_HASH(
    MSG_ERROR_REMOVING_SHADER_PRESET,
    "Error al eliminar el preset de shaders."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
    "Contador de duración de fotogramas"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
    "Modifica la configuración del contador de duración de fotogramas (solo se activará al desactivar el vídeo multihilo)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
    "Reiniciar tras usar avance rápido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
    "Reinicia el contador de duración de fotogramas al usar el avance rápido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
    "Reiniciar tras cargar guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
    "Reinicia el contador de duración de fotogramas al cargar un guardado rápido."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
    "Reiniciar tras guardado rápido"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
    "Reinicia el contador de duración de fotogramas al generar un guardado rápido."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
    "Utiliza animaciones, notificaciones, indicadores y controles con decoración moderna en vez del antiguo sistema de texto simple."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
    "Indica la animación que aparece al desplazarte entre pestañas."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
    "Indica la animación que aparece al desplazarte hacia arriba o abajo."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
    "Indica la animación que aparece al abrir un submenú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
    "Borrar lista de reproducción"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
    "Punto de acceso wifi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
    "Activa o desactiva el punto de acceso wifi."
    )
MSG_HASH(
    MSG_LOCALAP_SWITCHING_OFF,
    "Desconectando el punto de acceso wifi."
    )
MSG_HASH(
    MSG_WIFI_DISCONNECT_FROM,
    "Desconectando de la señal wifi «%s»"
    )
MSG_HASH(
    MSG_LOCALAP_ALREADY_RUNNING,
    "Ya se ha iniciado un punto de acceso wifi"
    )
MSG_HASH(
    MSG_LOCALAP_NOT_RUNNING,
    "El punto de acceso wifi no está en funcionamiento"
    )
MSG_HASH(
    MSG_LOCALAP_STARTING,
    "Iniciando punto de acceso wifi con el SSID «%s» y la contraseña «%s»"
    )
MSG_HASH(
    MSG_LOCALAP_ERROR_CONFIG_CREATE,
    "No se ha podido crear un archivo de configuración para el punto de acceso wifi."
    )
MSG_HASH(
    MSG_LOCALAP_ERROR_CONFIG_PARSE,
    "Archivo de configuración incorrecto: no se han podido encontrar los valores APNAME o PASSWORD en %s"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
    "Permitir a los núcleos cambiar el controlador de vídeo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
    "Permite que los núcleos fuercen un controlador de vídeo distinto al que esté en uso."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
    "Alternar pausa del servicio de IA"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
    "Pausa el núcleo mientras se traduce la pantalla."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
    "Escaneo manual"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
    "Escaneado configurable mediante nombres de contenidos. No necesita que los contenidos coincidan con los de la base de datos."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
    "Carpeta de contenidos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
    "Selecciona una carpeta a escanear."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
    "Nombre del sistema"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
    "Indica un «nombre del sistema» a asociar a los contenidos escaneados. Se utiliza para nombrar la lista de reproducción generada y para identificar sus miniaturas."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
    "Nombre de sistema personalizado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
    "Indica un «nombre del sistema» para contenidos escaneados. Solo se utilizará si la opción Nombre del sistema está configurada a Personalizado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
    "Núcleo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
    "Selecciona el núcleo que se utilizará para iniciar contenidos escaneados de forma predeterminada."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
    "Extensiones de archivo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
    "Una lista separada con espacios de los tipos de archivo que se incluirán en el escaneo. En caso de estar en blanco, incluirá todos los archivos; si se ha indicado un núcleo, incluirá todos los archivos compatibles con el núcleo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
    "Escanear dentro de archivos"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
    "Al activar esta opción, se buscarán contenidos válidos o compatibles dentro de archivos contenedores (.zip, .7z, etc.). Podría afectar al rendimiento del escaneado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
    "Archivo DAT de arcade"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
    "Selecciona un archivo DAT XML de Logiqx o MAME para nombrar automáticamente los contenidos de arcade escaneados (para MAME, FinalBurn Neo, etc.)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
    "Sobrescribir lista existente"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
    "Al activar esta opción se borrará cualquier lista de reproducción existente antes de escanear contenidos. Al desactivarla, se preservarán las listas ya existentes y solo se añadirán aquellos contenidos que no se encuentren en la lista."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
    "Comenzar escaneado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
    "Escanea el contenido seleccionado."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
    "<Carpeta de contenidos>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
    "<Personalizado>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
    "<Sin especificar>"
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
    "Se ha seleccionado un archivo DAT de arcade no válido."
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
    "El archivo DAT de arcade es demasiado grande (no hay memoria suficiente)"
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
    "Ha habido un error al cargar el archivo DAT de arcade (¿el formato no es válido?)"
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
    "La configuración del escaneo manual no es válida."
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
    "No se han detectado contenidos válidos."
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_START,
    "Escaneando contenido: "
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
    "Escaneando: "
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_END,
    "Escaneo finalizado: "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
    "Activar accesibilidad"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
    "Activa o desactiva el narrador de accesibilidad en los menús."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
    "Ajusta la velocidad de lectura del narrador."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
    "Velocidad de lectura del narrador"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
    "Escalado"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
    "Cambia los ajustes de escalado de vídeo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
    "Pantalla completa"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
    "Cambia los ajustes del modo en pantalla completa."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
    "Ventana"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
    "Cambia los ajustes del modo ventana."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
    "Salida"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
    "Cambia los ajustes de la salida de vídeo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
    "Sincronización"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
    "Cambia los ajustes de sincronización de vídeo."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
    "Salida"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
    "Cambia los ajustes de la salida de audio."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
    "Sincronización"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
    "Cambia los ajustes de sincronización de audio."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
    "Remuestreo"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
    "Cambia los ajustes del remuestreo de audio."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
    "Controles del menú"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
    "Cambia los ajustes de control del menú."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
    "Respuesta háptica/vibración"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
    "Cambia los ajustes de la respuesta háptica y la vibración."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_GONG,
    "Iniciar gong"
    )


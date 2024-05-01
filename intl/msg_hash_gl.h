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
   "Menú principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Configuracións"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favoritos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Historial"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Imaxes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Música"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Vídeos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Xogo en liña"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Explorar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Núcleos sen Contido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importar o contido"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Menú rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Acceso rápido ás configuracións relevantes durante o xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Cargar un núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Escolla que núcleo empregar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Busca unha implementación do núcleo de libretro. O lugar onde se inicia o navegador depende da ruta do directorio principal. Se está en branco, comezará na raíz.\nlf Core Directory é un directorio, o menú empregarao como cartafol superior. Se Core Directory é unha ruta completa, comezará no cartafol onde está o ficheiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Cargar contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Escolle que contido executar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Busca contido. Para cargar contido, necesitas un \"Núcleo\" para usar e un ficheiro de contido.\nPara controlar onde comeza o menú a buscar contido, configura o \"Directorio do navegador de ficheiros\". Se non se define, comezará en root.\nO navegador filtrará as extensións para o último conxunto de núcleos en \"Cargar núcleo\" e utilizará ese núcleo cando se cargue o contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Cargar un disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Carge un disco de medio físico. Primeiro escolla o núcleo (Cargar un núcleo) a empregar co disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Facer unha copia do disco"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Copia o disco de medios físico ó almacenamento interno. Gardarase como un arquivo de imaxe."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Expulsar Disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Expulsar Disco do lector físico."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Listaxes de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "O contido atopado coincidente coa base de datos aparecerá aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Importar o contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Crea e anova as listas de reproducción facendo unha procura de contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Amosar o menú do escritorio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Abre a vista de escritorio tradicional."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Desactivar o modo de quiosco (requírese reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Amosa todas os axustes relacionados coas configuracións."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Actualizador en liña"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Descarga módulos de extensión, compoñentes e contido para RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Xogo en liña"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Unirse ou hospedar unha sesión de xogo en rede."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Configuración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Configurar o programa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Información"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Amosar á información do sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Ficheiro de configuración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Xestiona e crea os ficheiros de configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Axuda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Saiba máis sobre o funcionamento do programa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Reiniciar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Reinicie a aplicación RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Saír"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Saia da aplicación RetroArch. Gardar a configuración ao saír está activado."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Sair da aplicación RetroArch. Gardar a configuración ao saír está desactivado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Saír de RetroArch. Matar o programa de calquera xeito (SIGKILL, etc.) finalizará RetroArch sen gardar a configuración en ningún caso. En Unix-like, SIGINT/SIGTERM permite unha desinicialización limpa que inclúe gardar a configuración se está activada."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Descargar un núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Descarga e instala un núcleo dende o actualizador en liña."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Instalar ou restaurar un núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Instala ou restaura un núcleo dende o directorio de Descargas."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Iniciar o procesador de vídeo"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Iniciar RetroPad remoto"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Directorio inicial"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Descargas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Abrir un arquivo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Cargar un arquivo"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "O contido engadido a \"Favoritos\" aparecerá aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Música"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "A música reproducida anteriormente aparecerá aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Imaxes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "As imaxes visualizadas anteriormente aparecerán aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Vídeos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Os vídeos reproducidos anteriormente aparecerán aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Explorar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Follear todos os contidos coincidentes ca base de datos mediante unha interface de búsqueda categorizada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Núcleos sen Contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Os núcleos instalados que poden funcionar sen cargar contido aparecerán aquí."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Descargador de núcleos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Actualizar os núcleos instalados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Anova todos os núcleos instalados á última versión dispoñible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Mudar os núcleos pola versión da Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Substitúe todos os núcleos orixinais e os manualmente instalados coa última versión da Play Store, cando estean dispoñibles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Anovador de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Descarga o paquete completo de miniaturas para o sistema escollido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Anovador das miniaturas das listas de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Descarga miniaturas para as entradas da playlist escollida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Descargador de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Descargar contido gratuito dende o núcleo seleccionado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Descargador de Arquivos do sistema para o Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Descarga os ficheiros auxiliares do sistema necesarios para un funcionamento correcto/óptimo do núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Anovar os ficheiros de información sobre os núcleos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Anovar os recursos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Anovar os perfís dos telemandos de xogo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Anovar as trampas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Anovar as bases de datos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Anovar as superposicións"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Anovar os sombreadores GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Anovar os sombreadores Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Anovar os sombreadores Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Información do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Amosa a información relativa á aplicación ou o núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Información sobre o disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Mostra información sobre os discos inseridos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Información da rede"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Amosar a(s) interface(s) de rede e IPs asociadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Información do sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Mostra información específica do dispositivo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Xestor de bases de datos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Ves as bases de datos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Xestor de cursores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Amosa buscas anteriores."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Nome do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Etiqueta do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Versión do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Nome do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Fabricante do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Categorías"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Autor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Permisos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Licenza"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Extensións compatibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Interfaces de programación de aplicacións gráficas requiridas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Camiño completo do núcleo"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Soporte do Slot de gardado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Ningún"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Básico (Gardar/Cargar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Serializado (Gardar/Cargar, Retroceder)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Determinista (Gardar/Cargar, Retroceder, Run-Ahead, Netplay)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Microprograma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "- Nota: \"Os ficheiros do sistema están no directorio de contido\" está activado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "- Buscando en: '%s'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Non se atopa, Requirido:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Non se atopa, Opcional:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Atopado, Requirido:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Atopado, Opcional:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Bloquear o núcleo instalado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Coutar a modificación do núcleo actualmente instalado. Pode ser empregado para evitar anovacións non desexadas cando o contido precisa ter instalada unha versión específica do núcleo (p. ex. conxuntos de ROMs arcade)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Excluir do menú 'Nucleos sen contido'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Evita que este núcleo se mostre na pestaña/menú \"Núcleos sen contido\". Só se aplica cando o modo de visualización está configurado como \"Personalizado\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Borrar núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Borrar este núcleo do disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Facer unha copia de seguridade do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Crear unha copia de seguridade arquivada do núcleo instalado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Restaurar unha copia de seguridade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Instalar unha versión anterior do núcleo dende unha lista de copias de seguridade arquivadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Borrar unha copia de seguridade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Borrar un ficheiro da lista de copias de seguridade arquivadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[Automático]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Data de compilación"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "Versión de RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Versión de Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Compilador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Modelo de CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Características da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Arquitectura da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Núcleos da CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT dispoñible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Indentificador da interface"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Sistema operativo da interface"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "Nivel RetroRating"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Fonte de alimentación"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Controlador de contexto de vídeo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Largura da pantalla (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Altura da pantalla (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "PPP da pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "Compatibilidade con LibretroDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Compatibilidade con sobreposicións"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Compatibilidade con liña de ordes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Compatibilidade con liña de ordes por rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Compatibilidade con telemandos de xogo en rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Compatibilidade con Cocoa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "Compatibilidade con PNG (RPNG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "Compatibilidade con JPEG (RJPEG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "Compatibilidade con BMP (RBMP)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "Compatibilidade con TGA (RTGA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "Compatibilidade con SDL 1.2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "Compatibilidade con SDL 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Compatibilidade con Vulkan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Compatibilidade con Metal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "Compatibilidade con OpenGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "Compatibilidade con OpenGL ES"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Compatibilidade con multifío"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "Compatibilidade con KMS/EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Compatibilidade con udev"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "Compatibilidade con OpenVG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "Compatibilidade con EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "Compatibilidade con X11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Compatibilidade con Wayland"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "Compatibilidade con XVideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "Compatibilidade con ALSA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "Compatibilidade con OSS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "Compatibilidade con OpenAL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "Compatibilidade con OpenSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "Compatibilidade con RSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "Compatibilidade con RoarAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "Compatibilidade con JACK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "Compatibilidade con PulseAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "Compatibilidade con CoreAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "Compatibilidade con CoreAudio V3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "Compatibilidade con DirectSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "Compatibilidade con WASAPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "Compatibilidade con XAudio2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "Compatibilidade con zlib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "Compatibilidade con 7zip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Compatibilidade con bibliotecas dinámicas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Cargando dinámicamente a librería de Libretro en tempo de execución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Compatibilidade con Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "Compatibilidade con GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "Compatibilidade con HLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "Compatibilidade con imáxes SDL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "Compatibilidade con FFmpeg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "Compatibilidade con mpv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "Compatibilidade con CoreText"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "Compatibilidade con FreeType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "Compatibilidade con STB TrueType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Compatibilidade con xogo en rede (entre pares)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Compatibilidade con Video4Linux2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "Compatibilidade con libusb"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Selección de base de datos"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Nome"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Descrición"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Xénero"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Logros"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Categoría"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Lingua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Rexión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Exclusivo de Consola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Exclusivo de Plataforma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Puntuación"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Multimedia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Controles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Estilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Mecánicas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Narrativa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "Paso"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Perspectiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Axustes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Editor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Desenvolvedor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Orixe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Franquía"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Clasificación TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Valoración según Famitsu Magazine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Revisión de Edge Magazine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Valoración según Edge Magazine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Tema de Edge Magazine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Mes de Lanzamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Ano de lanzamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Valoración según BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Valoración según ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Valoración según ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Clasificación PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Millora no hardware"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Valoración según CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Número de serie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analóxico soportado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Vibración soportada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Cooperación soportada"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Cargar configuración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Carga a configuración existente e substitúe os valores actuais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Gardar a configuración actual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Sobrescribir o ficheiro de configuración actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Gardar a nova configuración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Garda a configuración actual nun ficheiro separado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Restablecer aos valores predefinidos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Restablece a configuración actual aos valores predeterminados."
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Menú dos controis básicos"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Desprazar cara a arriba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Desprazar cara a abaixo"
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
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Iniciar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Activa o menú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Saír"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Activa o teclado"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Controladores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Cambiar os controladores empregados polo sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Cambia os axustes da saída de video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Son"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Cambiar a configuración de entrada/saída de audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Cambia os axustes do mando, teclado e rato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Cambia os axustes relacionados co video, audio e a latencia de entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Cambia os axustes do núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Configuración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Cambiar os axustes por defecto nos arquivos de configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Gardando"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Cambiar os axustes de gardado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Sincronización na nube"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Cambiar a configuración de sincronización na nube."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Activa a sincronización na nube"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Tenta sincronizar configuracións, sram e estados cun provedor de almacenamento na nube."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Sincronización da nube destrutiva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Cando está desactivado, os ficheiros móvense a un cartafol de copia de seguranza antes de sobrescribilos ou eliminalos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Backend de sincronización na nube"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Que protocolo de rede de almacenamento na nube utilizar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "URL de almacenamento na nube"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "O URL do punto de entrada da API ao servizo de almacenamento na nube."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Nome de usuario"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "O teu nome de usuario para a túa conta de almacenamento na nube."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Contrasinal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "O teu contrasinal para a túa conta de almacenamento na nube."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Rexistro de eventos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Cambia os axustes do rexistro de eventos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Explorador de ficheiros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Cambiar a configuración do explorador de ficheiros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Ficheiro de configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Ficheiro de arquivo comprimido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Gravando ficheiro de configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Ficheiro do cursor da base de datos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Ficheiro de configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Ficheiro predefinido do sombreador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Arquivo shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Remapear ficheiros de controis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Arquivo de trucos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Arquivo de superposición."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Arquivo de base de datos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "Ficheiro de fontes TrueType."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Arquivo simple."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Vídeo. Selecciónao para abrir este ficheiro co reprodutor de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Música. Selecciónao para abrir este ficheiro co reprodutor de música."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Arquivo de imaxe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Imaxe. Selecciónao para abrir este ficheiro co visor de imaxes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Libretro core. Seleccionando isto asociarase este núcleo ao xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Libretro core. Seleccione este ficheiro para que RetroArch cargue este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Directorio. Selecciónao para abrir este directorio."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Cambiar os axustes de retroceso, avance rápido e cámara lenta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Gravación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Cambiar os axustes da grabación."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Información en pantalla (OSD)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Cambiar a superposición da pantalla e do teclado, e configuración das notificacións en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Interface de usuario"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Cambiar as configuracións da interface de usuario."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Servizo de intelixencia artificial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Cambiar os axustes para a Intelixencia Artificial (Tradución/TTS/Misc)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Accesibilidade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Cambiar os axustes para o narrador de Accesibilidade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Xestión enerxética"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Cambia as configuracións da xestión enerxética."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Logros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Cambia as configuracións dos logros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Rede"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Cambiar os axustes da rede e do servidor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Listaxes de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Cambia as configuracións das listaxes de reproducción."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Usuario"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Cambia os axustes das contas, o nome de usuario e a lingua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Directorio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Cambia os directorios predeterminados onde se atopan os ficheiros."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Mapeamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Multimedia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Rendemento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Son"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Especificacións"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Almacenamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Temporalización"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Cambiar a configuración relacionada con Steam."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Controlador de entrada a empregar. Algúns controladores de vídeo forzan o uso doutro controlador de entrada diferente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "O controlador udev le eventos evdev para compatibilidade de teclado. Tamén admite as devolucións de chamada do teclado, os ratos e os paneles táctiles.\nPor defecto na maioría das distribucións, os nodos /dev/input só son root (modo 600). Podes configurar unha regra udev que faga accesibles para non root."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "O controlador de entrada de linuxraw require un TTY activo. Os eventos do teclado lense directamente desde o TTY, o que o fai máis sinxelo, pero non tan flexible como udev. Non se admiten ratos, etc. Este controlador usa a API de joystick máis antiga (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Controlador de entrada. O controlador de vídeo pode forzar un controlador de entrada diferente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Telemando de xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Controlador para usar. (O reinicio é necesario)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "Controlador de driver DirectInput."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "Controlador de dispositivo de interface humana de baixo nivel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Controlador Raw Linux, usa a API de joystick heredada. Use udev no seu lugar se é posible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Controlador de Linux para controladores conectados ao porto paralelo mediante adaptadores especiais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Controlador de controlador baseado en bibliotecas SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Controlador de controlador con interface udev, xeralmente recomendado. Usa a recente API de joypad evdev para compatibilidade con joystick. Admite conexión en quente e feedback forzado.\nPor defecto na maioría das distribucións, os nodos /dev/input son só root (modo 600). Podes configurar unha regra udev que faga accesibles para non root."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "Controlador de controlador XInput. Principalmente para controladores Xbox."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Controlador de video a usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "Controlador OpenGL 1.x. Versión mínima necesaria: OpenGL 1.1. Non admite sombreadores. Use controladores OpenGL posteriores no seu lugar, se é posible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "Controlador OpenGL 2.x. Este controlador permite que os núcleos de libretro GL se utilicen ademais dos núcleos renderizados por software. Versión mínima necesaria: OpenGL 2.0 ou OpenGLES 2.0. Admite o formato de sombreado GLSL. Use o controlador glcore no seu lugar, se é posible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "Controlador OpenGL 3.x. Este controlador permite que os núcleos de libretro GL se utilicen ademais dos núcleos renderizados por software. Versión mínima necesaria: OpenGL 3.2 ou OpenGLES 3.0+. Admite o formato Slang Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Condutor Vulkan. Este controlador permite utilizar núcleos Vulkan de libretro ademais dos núcleos renderizados por software. Versión mínima necesaria: Vulkan 1.0. Admite sombreadores HDR e Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "Controlador renderizado por software SDL 1.2. O rendemento considérase que non é óptimo. Considere usalo só como último recurso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "Controlador renderizado por software SDL 2. O rendemento das implementacións básicas de libretro renderizadas por software depende da implementación SDL da túa plataforma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Controlador de metal para plataformas Apple. Admite o formato Slang Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Controlador Direct3D 8 sen compatibilidade con sombreadores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Controlador Direct3D 9 con soporte para o antigo formato Cg Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Controlador direct3D 9 con soporte para o formato shader HLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Controlador Direct3D 10 con soporte para o formato Slang Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Controlador Direct3D 11 con soporte para HDR e o formato Slang Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Controlador Direct3D 12 con soporte para HDR e o formato Slang Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "Controlador DispmanX. Usa a API DispmanX para a GPU Videocore IV en Raspberry Pi 0..3. Sen superposición nin soporte para sombreadores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "Controlador LibCACA. Produce caracteres en lugar de gráficos. Non recomendado para uso práctico."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Un controlador de vídeo Exynos de baixo nivel que usa o bloque G2D en Samsung Exynos SoC para operacións blit. O rendemento dos núcleos renderizados por software debe ser óptimo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Controlador de vídeo DRM simple. Este é un controlador de vídeo de baixo nivel que usa libdrm para escalar o hardware mediante superposicións de GPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Un controlador de vídeo Sunxi de baixo nivel que usa o bloque G2D no SoCs Allwinner."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Controlador de Wii U. Admite slang shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Cambiar controlador. Admite o formato de sombreado GLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "Driver OpenVG. Usa a API de gráficos vectoriales 2D acelerada por hardware de OpenVG."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "Controlador GDI. Usa unha interface de Windows herdada. Non recomendado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Controlador de vídeo actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Son"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Controlador de Audio a usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "Controlador RSound para sistemas de audio na rede."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Controlador Legacy Open Sound System."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Controlador ALSA predeterminado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "Controlador ALSA con soporte para rosca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "Controlador ALSA implementado sen dependencias."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "Controlador do sistema de son RoarAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "Controlador OpenAL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "Controlador OpenSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "Controlador DirectSound. DirectSound úsase principalmente desde Windows 95 ata Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Controlador de API de sesión de audio de Windows. WASAPI úsase principalmente desde Windows 7 e superior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "Controlador PulseAudio. Se o sistema usa PulseAudio, asegúrate de usar este controlador en lugar de, e.g. ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Controlador do kit de conexión de audio Jack."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Micrófono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Controlador de micrófono para usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Micrófono Resampler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Controlador de resampler de micrófono para usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Marcos de bloque de micrófono"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Remostreador de audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Controlador de remostreador de Audio a usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Windowed Sinc Implementación."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Implementación de coseno complicado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Implementación de remuestreo máis próxima. Este remuestreador ignora a configuración de calidade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Cámara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Controlador de cámara a usar."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Controlador bluetooth a usar."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Controlador Wi-Fi a usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Localización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Localización do controlador a usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Controlador de menú para usar. (O reinicio é necesario)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB é unha GUI de RetroArch que semella un menú de consola da sétima xeración. Pode admitir as mesmas funcións que Ozone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone é a GUI predeterminada de RetroArch na maioría das plataformas. Está optimizado para navegar cun controlador de xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI é unha sinxela GUI integrada para RetroArch. Ten os requisitos de rendemento máis baixos entre os controladores de menú e pódese usar en pantallas de baixa resolución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "Nos dispositivos móbiles, RetroArch usa a IU móbil, MaterialUI, de forma predeterminada. Esta interface está deseñada en torno a pantallas táctiles e dispositivos punteiros, como un rato/bola de seguimento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Gravación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Controlador de gravación a usar."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Controlador MIDI a usar."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "Selector de Resolución para CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Saída nativa, sinais de baixa resolución para usar con monitores CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Saída"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Cambiar as configuracións de saída de Vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Modo pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Cambiar a configuración do modo pantalla completa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Modo fiestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Cambiar as configuración do modo fiestra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Escalado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Cambiar as configuracións do escalado de vídeo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Cambiar as configuracións de vídeo HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Sincronización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Cambiar as configuracións de sincronización de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Suspender Salvapantallas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Previr que se active o salvapantallas do sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Suspende o protector de pantalla. É unha suxestión que non necesariamente ten que ser respectada polo controlador de vídeo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Millora o rendemento a costa da latencia e que o vídeo vaia a saltos. Úsao só se a velocidade completa non se pode obter doutra maneira."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Usa un controlador de vídeo enroscado. Usalo pode mellorar o rendemento polo posible custo da latencia e máis tartamudeo de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Inserción de Black Frame"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "Insira cadro(s) negros entre cadros. Pode reducir moito o desenfoque de movemento emulando a exploración CRT, pero a costa do brillo. Non combine con Intervalo de intercambio > 1, sub-fotogramas, Retraso de fotogramas ou Sincronizar a velocidade de fotogramas de contido exacto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Insire cadro(s) negros entre cadros para mellorar a claridade do movemento. Use só a opción designada para a súa frecuencia de actualización da pantalla actual. Non se debe usar con frecuencias de actualización que non sexan múltiplos de 60 Hz, como 144 Hz, 165 Hz, etc. Non combines con Intervalo de intercambio > 1, subfotogramas, Retraso de fotogramas ou Sincronización con frecuencia de fotogramas de contido exacto. Deixar o sistema VRR activado está ben, pero non esa configuración. Se[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_120,
   "1 - Para a frecuencia de actualización da pantalla de 120 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_180,
   "2 - Para a frecuencia de actualización da pantalla de 180 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_240,
   "3 - Para a frecuencia de actualización da pantalla de 240 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_300,
   "4 - Para a frecuencia de actualización da pantalla de 300 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_360,
   "5 - Para a frecuencia de actualización da pantalla de 360 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_420,
   "6 - Para a frecuencia de actualización da pantalla de 420 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_480,
   "7 - Para a frecuencia de actualización da pantalla de 480 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_540,
   "8 - Para a frecuencia de actualización da pantalla de 540 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_600,
   "9 - Para a frecuencia de actualización da pantalla de 600 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_660,
   "10 - Para a frecuencia de actualización da pantalla de 660 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_720,
   "11 - Para a frecuencia de actualización da pantalla de 720 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_780,
   "12 - Para a frecuencia de actualización da pantalla de 780 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_840,
   "13 - Para a frecuencia de actualización da pantalla de 840 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_900,
   "14 - Para a frecuencia de actualización da pantalla de 900 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_960,
   "15 - Para a frecuencia de actualización da pantalla de 960 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Inserción de cadros negros - cadros escuros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Axuste o número de cadros negros na secuencia total de exploración BFI. Máis é maior claridade de movemento, menos é maior brillo. Non é aplicable a 120 Hz xa que só hai 1 marco BFI para traballar en total. A configuración superior á posible limitarache ao máximo posible para a frecuencia de actualización que elixiches."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Axusta o número de fotogramas que se mostran na secuencia bfi que son negros. Máis cadros negros aumentan a claridade do movemento pero reducen o brillo. Non se aplica a 120 Hz xa que só hai un cadro extra de 60 Hz, polo que debe ser negro, se non, BFI non estaría activo en absoluto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Subcadros de sombreadores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "Insira marco(s) de sombreado extra entre cadros. Permite aos sombreadores facer efectos que se executan a un fps superior á taxa de contido real. Debe configurarse na pantalla actual Hz. Non combines con Intervalo de intercambio > 1, BFI, Retraso de fotogramas ou Sincronizar a velocidade de fotogramas de contido exacto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Insire cadro(s) de sombreado extra entre os fotogramas para os posibles efectos de sombreado que estean deseñados para executarse máis rápido que a taxa de contido. Use só a opción designada para a súa frecuencia de actualización da pantalla actual. Non debe usarse con frecuencias de actualización que non sexan múltiplos de 60 Hz, como 144 Hz, 165 Hz, etc. Non combine con Intervalo de intercambio > 1, BFI, Retraso de fotogramas ou Sincronización con frecuencia de fotogramas de contido [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_120,
   "2 - Para a frecuencia de actualización da pantalla de 120 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_180,
   "3 - Para a frecuencia de actualización da pantalla de 180 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_240,
   "4 - Para a frecuencia de actualización da pantalla de 240 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_300,
   "5 - Para a frecuencia de actualización da pantalla de 300 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_360,
   "6 - Para a frecuencia de actualización da pantalla de 360 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_420,
   "7 - Para a frecuencia de actualización da pantalla de 420 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_480,
   "8 - Para a frecuencia de actualización da pantalla de 480 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_540,
   "9 - Para a frecuencia de actualización da pantalla de 540 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_600,
   "10 - Para a frecuencia de actualización da pantalla de 600 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_660,
   "11 - Para a frecuencia de actualización da pantalla de 660 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_720,
   "12 - Para a frecuencia de actualización da pantalla de 720 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_780,
   "13 - Para a frecuencia de actualización da pantalla de 780 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_840,
   "14 - Para a frecuencia de actualización da pantalla de 840 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_900,
   "15 - Para a frecuencia de actualización da pantalla de 900 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_960,
   "16 - Para a frecuencia de actualización da pantalla de 960 Hz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Captura de pantalla da GPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Simulación de liña de escaneo rodante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "Simula unha liña de escaneo en rotación básica sobre varios subfotogramas dividindo a pantalla verticalmente e representando cada parte da pantalla segundo cantos subfotogramas haxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Simula unha liña de escaneo en rotación básica sobre varios subfotogramas dividindo a pantalla verticalmente cara arriba e representando cada parte da pantalla segundo cantos subfotogramas haxa desde a parte superior da pantalla cara abaixo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Captura de pantalla dos sombreados da GPU, se están dispoñibles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Filtrado Bilinear"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Engade un lixeiro borroso á imaxe para suavizar os bordos dos píxeles duros. Esta opción ten moi pouco impacto no rendemento. Debe estar desactivado se se usan sombreadores."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Interpolación de imaxes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Elixe o método de interpolación de imaxes cando se está escalando contido a través da IPU interna. 'Bicúbica' ou 'Bilinear' é o recomendado cando se usan filtros de video alimentados pola CPU. Esta opción non ten efecto no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bicúbico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Veciño máis próximo"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Interpolación de imaxe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Elixe o método de interpolación de imaxe cando 'Escala de Números Enteiros' estea desactivado. 'Veciño máis próximo' ten o menor efecto no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Veciño máis próximo"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Retraso do sombreado automático"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Retardo do autocargado de sombras (en ms). Pode traballar sobre os fallos gráficos cando se usa algún programa de 'captura de pantalla'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Filtro de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Aplica un filtro de vídeo alimentado pola CPU. Isto podría ter un custo elevado no rendemento. Algúns filtros de vídeo poderían só traballar en núcleos que usen 16 ou 32 bits de cor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Aplicar un filtro de vídeo alimentado por CPU. Pode ter un alto custo de rendemento. É posible que algúns filtros de vídeo só funcionen para núcleos que usan cor de 32 ou 16 bits. Pódense seleccionar bibliotecas de filtros de vídeo vinculadas dinámicamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Aplicar un filtro de vídeo alimentado por CPU. Pode ter un alto custo de rendemento. É posible que algúns filtros de vídeo só funcionen para núcleos que usan cor de 32 ou 16 bits. Pódense seleccionar bibliotecas de filtros de vídeo incorporadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Elimina o filtro de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Desactiva calquera filtro de vídeo alimentado pola CPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Activa a pantalla completa sobre o notch en dispositivos Android e iOS"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "Selector de Resolución para CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Só para pantallas CRT. Tenta usar a resolución e tasa de refresco exactas para cada núcleo/xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "Super Resolución en CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Cambia entre super resolucións nativas ou ultraanchas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Centrado do eixo X"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Vai xirando sobre estas opcións se a imaxe non está correctamente centrada na pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Axuste de porche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Vai xirando sobre estas opcións para axustar os axustes de porche para cambiar o tamaño de imaxe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Use o menú de alta resolución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Elixe o modo alta resolución para usar con menús de alta resolución cando non se cargou ningún contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Tasa de refresco personalizada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Usa unha tasa de refresco especificada no arquivo de configuracion se o precisa."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Índice de monitores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Elixe qué monitor usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Que monitor preferir. 0 (predeterminado) significa que non se prefire ningún monitor en particular, 1 ou superior (1 é o primeiro monitor), suxire que RetroArch use ese monitor en particular."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Optimizar para Wii U GamePad (requírese reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Usa unha escala exacta de 2x do GamePad como ventana gráfica. Desactive para mostrar a resolución nativa da TV."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Rotación de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Forza unha rotación do vídeo. A rotación é engadida ás rotacións que establece o núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Orientación da Pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Forza a orientación da pantalla dende o sistema operativo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Índice de GPUs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Elixe qué tarxeta gráfica usar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Offset horizontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Forza na pantalla un determinado offset horizontal. Este offset é aplicado globalmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Offset vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Forza na pantalla un determinado offset vetical. Este offset é aplicado globalmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Tasa de refresco vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Tasa de refresco vertical da túa pantalla. Usado para calcular a adecuada tasa de entrada de audio.\n Isto será ignorado se 'Threaded Video' está habilitado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Taxa de actualización da pantalla estimada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "A taxa de actualización estimada da pantalla en Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "A frecuencia de actualización precisa do teu monitor (Hz). Isto úsase para calcular a taxa de entrada de audio coa fórmula:\naudio_input_rate = taxa de entrada do xogo * frecuencia de actualización de visualización / frecuencia de actualización do xogo\nSe o núcleo non indica ningún valor, asumiranse os valores predeterminados NTSC para a compatibilidade.\nEste valor debería permanecer próximo. a 60 Hz para evitar grandes cambios de ton. Se o teu monitor non funciona a 60 Hz ou preto d[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Establece a taxa de actualización da pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "A taxa de actualización segundo a informada polo controlador da pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Interruptor automático da taxa de actualización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Cambia automaticamente a frecuencia de actualización da pantalla en función do contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Só no modo exclusivo de pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Todos os modos de pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Limiar PAL de taxa de actualización automática"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Taxa de actualización máxima para considerarse PAL."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Tasa de refresco vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Establece a frecuencia de actualización vertical da pantalla. \"50 Hz\" permitirá un vídeo suave ao executar contido PAL."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Forzar desactivar sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Desactivar pola forza a compatibilidade con sRGB FBO. Algúns controladores Intel OpenGL en Windows teñen problemas de vídeo cos FBO sRGB. Activar isto pode funcionar."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Comeza no modo de pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Comeza en pantalla completa. Pódese cambiar en tempo de execución. Pódese anular mediante un interruptor de liña de comandos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Modo de pantalla completa con xanela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Se a pantalla completa, prefire usar unha xanela de pantalla completa para evitar o cambio de modo de visualización."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Ancho da pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Establece o tamaño de ancho personalizado para o modo de pantalla completa sen xanela. Se non o configuras, utilizarase a resolución do escritorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Altura de pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Establece o tamaño de altura personalizado para o modo de pantalla completa sen xanela. Se non o configuras, utilizarase a resolución do escritorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Forzar resolución en UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Forzar a resolución ao tamaño de pantalla completa, se se establece en 0, empregarase un valor fixo de 3840 x 2160."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Escala de ventá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Establece o tamaño da xanela co múltiplo especificado do tamaño da ventana principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Opacidade da xanela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Establece a transparencia da xanela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Mostrar decoración da xanela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Mostrar a barra de título e os bordos da xanela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Mostrar a barra de menús"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Mostrar a barra de menú da xanela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Lembra a posición e o tamaño da xanela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Mostra todo o contido nunha xanela de tamaño fixo de dimensións especificadas por \"Ancho da xanela\" e \"Altura da xanela\", e garda o tamaño e a posición da xanela actual ao pechar RetroArch. Cando estea desactivado, o tamaño da xanela establecerase de forma dinámica en función da \"Escala da xanela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Usa o tamaño da xanela personalizado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Mostra todo o contido nunha xanela de tamaño fixo de dimensións especificadas por \"Ancho da xanela\" e \"Alto da xanela\". Cando estea desactivado, o tamaño da xanela establecerase de forma dinámica en función da \"Escala da xanela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Ancho da xanela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Establece o ancho personalizado para a xanela de visualización."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Altura da xanela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Establece a altura personalizada para a xanela de visualización."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Ancho máximo da xanela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Establece o ancho máximo da xanela cando cambie o tamaño automaticamente en función da \"Escala da xanela\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Altura máxima da xanela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Establece a altura máxima da xanela cando cambie o tamaño automaticamente en función da \"Escala da xanela\"."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Escala enteira"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Escala o vídeo só en pasos enteiros. O tamaño base depende da xeometría e relación de aspecto informadas polo sistema. Se non se define a \"Forzar relación de aspecto\", X/Y escalarase enteiro de forma independente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_OVERSCALE,
   "Sobreescala de escala enteira"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_OVERSCALE,
   "Forza a escala de enteiros a redondear cara arriba ao seguinte número enteiro maior en lugar de redondear abaixo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Relación de aspecto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Establecer a relación de aspecto da pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Configurar a relación de aspecto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Valor de punto flotante para a relación de aspecto do vídeo (ancho/alto)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Configuración"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Núcleo proporcionado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Cheo"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Mantér a relación de aspecto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Mantén relacións de aspecto de píxeles de 1:1 ao escalar o contido a través da IPU interna. Se está desactivado, as imaxes estenderanse para encher toda a pantalla."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Relación de aspecto personalizada (posición X)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Desprazamento da ventana gráfica personalizada que se usa para definir a posición do eixe X da ventana gráfica.\nIgnoraranse se se activa a \"Escala enteira\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Relación de aspecto personalizada (posición Y)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Desfase personalizado da ventana gráfica que se usa para definir a posición do eixe Y da ventana gráfica.\nIgnoraranse se está activada a \"Escala enteira\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Relación de aspecto personalizada (ancho)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Ancho da vista personalizada que se usa se a Relación de aspecto está definida como \"Relación de aspecto personalizada\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Relación de aspecto personalizada (altura)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Altura da ventana gráfica personalizada que se usa se a Relación de aspecto está definida como \"Relación de aspecto personalizada\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Recortar sobreexploración (requírese reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Recorta algúns píxeles ao redor dos bordos da imaxe que normalmente deixan en branco os desenvolvedores que ás veces tamén conteñen píxeles de lixo."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "Activa o HDR"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Activa o HDR se a pantalla o admite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Luminancia máxima"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Establece a luminancia máxima (en cd/m2) que pode reproducir a túa pantalla. Consulta os RTings para coñecer a luminancia máxima da túa pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Luminancia branca de papel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Establece a luminancia na que debe ser o branco do papel, é dicir, o texto lexible ou a luminancia na parte superior do rango SDR (rango dinámico estándar). Útil para axustarse ás diferentes condicións de iluminación do seu entorno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "Contraste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "Control de gamma/contraste para HDR. Toma as cores e aumenta o rango global entre as partes máis brillantes e as partes máis escuras da imaxe. Canto maior sexa o contraste HDR, maior será esta diferenza, mentres que canto menor sexa o contraste, máis lavada será a imaxe. Axuda aos usuarios a adaptar a imaxe ao seu gusto e o que cren que se ve mellor na súa pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Expandir Gamut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Unha vez que o espazo de cor se converta en espazo lineal, decida se debemos usar unha gama de cores ampliada para chegar a HDR10."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Sincronización vertical (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Sincroniza o vídeo de saída da tarxeta gráfica coa taxa de actualización da pantalla. Recomendado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Intervalo de intercambio VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Use un intervalo de intercambio personalizado para VSync. Reduce efectivamente a frecuencia de actualización do monitor polo factor especificado. \"Automático\" define o factor en función da frecuencia de fotogramas informada polo núcleo, proporcionando un ritmo de fotogramas mellorado cando se executa, por exemplo. Contido de 30 fps nunha pantalla de 60 Hz ou contido de 60 fps nunha pantalla de 120 Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "VSync adaptativo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "VSync está habilitado ata que o rendemento cae por debaixo da taxa de actualización obxectivo. Pode minimizar o tartamudeo cando o rendemento cae por debaixo do tempo real e ser máis eficiente enerxéticamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Retraso de fotogramas (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Reduce a latencia ao custo dun maior risco de tartamudeo de vídeo. Engade un atraso en milisegundos despois da presentación do vídeo e antes do cadro principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Establece cantos milisegundos hai que atrasar despois da presentación do vídeo antes de executar o núcleo. Pode reducir a latencia a costa dun maior risco de tartamudeo. O máximo é %d."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Retraso de fotograma automático"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Reduce temporalmente o \"Retraso de fotogramas\" efectivo para evitar caídas de fotogramas futuras. O punto de partida é a metade do tempo cando o \"Retraso de fotograma\" é 0."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Diminúe temporalmente o \"Retraso de fotogramas\" efectivo ata que a taxa de actualización obxectivo sexa estable. A medición comeza a partir da metade do tempo de fotograma cando o \"Retraso de fotograma\" é 0. Por exemplo. 8 para NTSC e 10 para PAL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "eficaz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Hard GPU Sincronización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Sincroniza a CPU e a GPU. Reduce a latencia ao custo do rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Hard GPU sincronización de Fotogramas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Establece cantos fotogramas pode executar a CPU por diante da GPU ao usar \"Hard GPU Sincronización\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Establece cantos fotogramas pode executar a CPU por diante da GPU cando se utiliza \"GPU Hard Sincronización\". O máximo é 3.\n 0: sincronizar coa GPU inmediatamente.\n 1: sincronizar co fotograma anterior.\n 2: etc..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Sincronización coa taxa de fotogramas do contido exacto (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Non hai desviación do tempo solicitado principal. Use para pantallas de frecuencia de actualización variable (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Sincronizar coa taxa de fotogramas de contido exacto. Esta opción é o equivalente a forzar a velocidade x1 aínda que permite un avance rápido. Sen desviación da frecuencia de actualización do núcleo solicitada, nin control dinámico da taxa de son."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Saída"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Cambia os axustes da saída de son."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Micrófono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Cambiar a configuración de entrada de audio."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Remostrador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Cambiar a configuración do remostreador de audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Sincronización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Acalar a configuración de sincronización de audio."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Cambiar a configuración MIDI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Mesturador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Cambiar a configuración do mesturador de audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Sons do menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Cambiar a configuración de son do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Acalar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Acalar o audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Acalar o mesturador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Acalar o audio do mesturador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Respecta o modo silencioso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Acala todo o audio en modo silencioso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Acalar ao avanzar rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Acalar automaticamente o audio ao utilizar o avance rápido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Acelerar ao avanzar rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Acelera o audio ao avanzar rápido. Evita crepitar pero cambia de ton."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Ganancia de volume (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Volume de audio (en dB). 0 dB é o volume normal e non se aplica ningunha ganancia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Volume de audio, expresado en dB. 0 dB é o volume normal, onde non se aplica ningunha ganancia. A ganancia pódese controlar durante o tempo de execución con Input Volume Up / Input Volume Down."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Ganancia de volume do mesturador (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Volume do mesturador de audio global (en dB). 0 dB é o volume normal e non se aplica ningunha ganancia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Complemento DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Complemento DSP de audio que procesa o audio antes de envialo ao controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Eliminar o complemento DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Descarga calquera complemento DSP de audio activo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Modo exclusivo WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Permitir que o controlador WASAPI asuma o control exclusivo do dispositivo de audio. Se está desactivado, utilizará o modo compartido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "WASAPI Formato flotante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Use o formato flotante para o controlador WASAPI, se é compatible co seu dispositivo de audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Lonxitude do búfer compartido WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "A lonxitude intermedia do búfer (en fotogramas) cando se usa o controlador WASAPI en modo compartido."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Son"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Activa a saída de audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Dispositivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Anular o dispositivo de audio predeterminado que usa o controlador de audio. Isto depende do controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Anular o dispositivo de audio predeterminado que usa o controlador de audio. Isto depende do controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Valor do dispositivo PCM personalizado para o controlador ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Valor de ruta personalizado para o controlador OSS (por exemplo, /dev/dsp)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Valor do nome de porto personalizado para o controlador JACK (por exemplo, system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Enderezo IP personalizado dun servidor RSound para o controlador RSound."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Latencia de audio (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Latencia de audio desexada en milisegundos. Pode non ser honrado se o controlador de audio non pode proporcionar unha latencia determinada."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Micrófono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Activa a entrada de audio nos núcleos compatibles. Non ten sobrecarga se o núcleo non está usando un micrófono."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Dispositivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Anular o dispositivo de entrada predeterminado que usa o controlador do micrófono. Isto depende do controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Anular o dispositivo de entrada predeterminado que usa o controlador do micrófono. Isto depende do controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Calidade do remuestreador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Reduce este valor para favorecer o rendemento/menor latencia fronte á calidade do audio, aumenta para obter unha mellor calidade de audio a costa do rendemento/menor latencia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Taxa de entrada predeterminada (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Frecuencia de mostraxe de entrada de audio, que se usa se un núcleo non solicita un número específico."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Latencia da entrada do audio (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Latencia de entrada de audio desexada en milisegundos. Pode non ser honrado se o controlador do micrófono non pode proporcionar unha latencia determinada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Modo exclusivo WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Permitir que RetroArch asuma o control exclusivo do dispositivo de micrófono cando use o controlador de micrófono WASAPI. Se está desactivado, RetroArch usará o modo compartido no seu lugar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "WASAPI Formato flotante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Use o formato flotante para o controlador WASAPI, se é compatible co seu dispositivo de audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Lonxitude do búfer compartido WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "A lonxitude intermedia do búfer (en fotogramas) cando se usa o controlador WASAPI en modo compartido."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Calidade do remuestreador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Reduce este valor para favorecer o rendemento/menor latencia fronte á calidade do audio, aumenta para obter unha mellor calidade de audio a costa do rendemento/menor latencia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Taxa de saída (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Frecuencia de mostraxe de saída de audio."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Sincronización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Sincronizar audio. Recomendado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Sesgo de tempo máximo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "O cambio máximo na taxa de entrada de audio. Aumentar isto permite cambios moi grandes no tempo a costa dun tono de audio impreciso (por exemplo, executar núcleos PAL en pantallas NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Sesgo máximo de tempo de audio.\nDefine o cambio máximo na taxa de entrada. Quizais queiras aumentar isto para permitir cambios moi grandes no tempo, por exemplo executando núcleos PAL en pantallas NTSC, a costa dun tono de audio impreciso.\nA taxa de entrada defínese como:\ntaxa de entrada * (1,0 +/- (tempo máximo sesgado))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Control dinámico da taxa de audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Axuda a suavizar as imperfeccións de tempo ao sincronizar audio e vídeo. Teña en conta que, se está desactivado, a sincronización adecuada é case imposible de conseguir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Establecer isto en 0 desactiva o control da taxa. Calquera outro valor controla a delta do control da taxa de audio.\nDefine a cantidade de taxa de entrada que se pode axustar de forma dinámica. A taxa de entrada defínese como:\ntaxa de entrada * (1,0 +/- (delta de control de taxa))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Seleccione o dispositivo de entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Establece o dispositivo de entrada (específico do controlador). Cando se axuste a \"Desactivado\", a entrada MIDI desactivarase. Tamén se pode escribir o nome do dispositivo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Saída"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Seleccione o dispositivo de saída."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Establece o dispositivo de saída (específico do controlador). Cando se define en \"Desactivado\", a saída MIDI desactivarase. Tamén se pode escribir o nome do dispositivo.\nCando a saída MIDI está activada e o núcleo e o xogo/a aplicación admiten a saída MIDI, algúns ou todos os sons (depende do xogo/aplicación) xeraranse polo dispositivo MIDI. En caso de controlador MIDI \"nulo\", isto significa que eses sons non serán audibles."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Establecer o volume de saída (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Fluxo do mesturador #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Reproducir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Iniciarase a reprodución do fluxo de audio. Unha vez rematado, eliminará o fluxo de audio actual da memoria."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Xogar (en bucle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Iniciarase a reprodución do fluxo de audio. Unha vez rematado, volverá a reproducir a pista desde o principio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Xogar (secuencial)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Iniciarase a reprodución do fluxo de audio. Unha vez rematado, saltará á seguinte emisión de audio en orde secuencial e repetirá este comportamento. Útil como modo de reprodución de álbums."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Parar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Isto deterá a reprodución do fluxo de audio, pero non o eliminará da memoria. Pódese iniciar de novo seleccionando \"Reproducir\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Borrar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Isto deterá a reprodución do fluxo de audio e eliminarase por completo da memoria."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Axusta o volume do fluxo de audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Estado: N/A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Estado: Detido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Estado: Reproducindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Estado: Reproducindo (en bucle)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Estado: Reproducindo (Secuencial)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Mesturador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Reproduce fluxos de audio simultáneos mesmo no menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Activa o son \"OK\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Activa o son \"Cancelar\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Activa o son \"Aviso\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Activa o son \"BGM\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Activa os sons de \"Desprazamento\""
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Máximo Usuarios"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Cantidade máxima de usuarios admitidos por RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Comportamento de votación (requírese reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Inflúe como se realiza a votación de entrada en RetroArch. Axustándoo a \"Cece\" ou \"Tardía\" pode producir menos latencia, dependendo da túa configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Inflúe na forma en que se realiza a votación de entrada dentro de RetroArch.\nA principios: a enquisa de entrada realízase antes de que se procese o fotograma.\nNormal: a enquisa de entrada realízase cando se solicita o sondeo.\nTarde: a votación de entrada realízase na primeira solicitude de estado de entrada por fotograma.\\ nSeleccionándoo como \"cedo\" ou \"Tardía\" pode producir menos latencia, dependendo da túa configuración. Ignorarase ao usar netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Remapear controis para este núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Anular os enlaces de entrada cos enlaces reasignados definidos para o núcleo actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Auto configurar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Configura automaticamente controladores que teñen un perfil, estilo Plug-and-Play."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Desactivar as teclas de acceso rápido de Windows (requírese reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Manteña as combinacións Win-key dentro da aplicación."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Seleccione o teclado físico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Usa este dispositivo como teclado físico e non como gamepad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Se RetroArch identifica un teclado de hardware como algún tipo de mando de xogos, esta configuración pódese usar para forzar a RetroArch a tratar o dispositivo identificado incorrectamente como un teclado.\nIsto pode ser útil se estás tentando emular un ordenador nalgún dispositivo Android TV e tamén posúe un teclado físico que se pode conectar á caixa."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Entrada de sensor auxiliar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Captura automática do rato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Activa a captura do rato no foco da aplicación."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Activar automaticamente o modo \"Game Focus\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "ACENDER"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Detectar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Pausa o contido cando o controlador se desconecta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Pausa o contido cando calquera controlador estea desconectado. Retomar con Inicio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Limiar do eixe do botón de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Ata onde se debe inclinar un eixe para que se prema un botón ao usar \"Analóxico a dixital\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Zona morta analóxica"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Ignora os movementos do stick analóxico por debaixo do valor da zona morta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Sensibilidade analóxica"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Axusta a sensibilidade dos sticks analóxicos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Tempo de espera de vinculación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Cantidade de segundos para esperar ata pasar á seguinte vinculación."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Manter vinculación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Cantidade de segundos para manter unha entrada para vinculala."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Preme o teclado, o rato ou o controlador"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Solta teclas e botóns!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Tempo de espera"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Manter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Período Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "O período (en fotogramas) no que se preme os botóns activados para turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
   "Ciclo de traballo turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "O número de fotogramas do período Turbo durante os que se manteñen pulsados os botóns. Se este número é igual ou superior ao período turbo, os botóns nunca se soltarán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Modo Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Seleccione o comportamento xeral do modo turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Clásico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Clásico (alternar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Botón único (alternar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Botón único (manteña premida)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Modo clásico, operación con dous botóns. Mantén premido un botón e toca o botón Turbo para activar a secuencia do comunicado de prensa.\nO botón Turbo pódese asignar en Configuración/Entrada/Controis do porto 1."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Modo de alternancia clásico, operación con dous botóns. Mantén premido un botón e toca o botón Turbo para activar o turbo para ese botón. Para desactivar o turbo: manteña premido o botón e preme de novo o botón Turbo.\nO botón Turbo pódese asignar en Configuración/Entrada/Controis do porto 1."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Alternar modo. Preme o botón Turbo unha vez para activar a secuencia de lanzamento de prensa para o botón predeterminado seleccionado, preme de novo para desactivalo.\nO botón Turbo pódese asignar en Configuración/Entrada/Controis do porto 1."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Modo de espera. A secuencia de lanzamento de prensa para o botón predeterminado seleccionado está activa mentres se manteña premido o botón Turbo.\nO botón Turbo pódese asignar en Configuración/Entrada/Controis do porto 1.\nPara emular a función de disparo automático da era do ordenador doméstico, configura Turbo e os botóns predeterminados deben ser os mesmos que o botón de disparo do joystick."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DEFAULT_BUTTON,
   "Botón Turbo predeterminado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DEFAULT_BUTTON,
   "Botón activo predeterminado para o modo Turbo \"Botón único\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALLOW_TURBO_DPAD,
   "Permitir indicacións do Turbo D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALLOW_TURBO_DPAD,
   "Se está activada, as entradas direccionais dixitais (tamén coñecidas como d-pad ou 'hatswitch') poden ser turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Turbo fogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Cambia a configuración do turbo fogo.\nNota: a función turbo require asignar un botón turbo ao teu dispositivo de entrada no menú \"Controis do porto X\" correspondente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Retroalimentación/Vibración háptica"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Cambia a configuración de vibración e retroalimentación háptico."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Controis do menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Cambiar a configuración do control do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Teclas de acceso rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Cambia a configuración e as asignacións das teclas de acceso rápido, como cambiar o menú durante o xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Vinculacións de RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Cambia como se asigna o RetroPad virtual a un dispositivo de entrada físico. Se un dispositivo de entrada se recoñece e se configura automaticamente correctamente, probablemente os usuarios non necesiten usar este menú.\nNota: para cambios de entrada específicos do núcleo, use no seu lugar o submenú \"Controis\" do menú rápido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro usa unha abstracción de gamepad virtual coñecida como \"RetroPad\" para comunicarse desde interfaces (como RetroArch) ata núcleos e viceversa. Este menú determina como se asigna o RetroPad virtual aos dispositivos de entrada físicos e cales son os portos de entrada virtuais que ocupan estes dispositivos.\nSe un dispositivo de entrada físico é recoñecido e autoconfigurado correctamente, probablemente os usuarios non teñan que usar este menú en absoluto, e para o núcleo. -cambi[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Controis do porto %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Cambia como se asigna o RetroPad virtual ao teu dispositivo de entrada físico para este porto virtual."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Cambia as asignacións de entrada específicas do núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Solución alternativa de desconexión de Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Solución alternativa para os controladores que se desconectan e se reconectan. Impide a 2 xogadores cos controladores idénticos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Confirmar peche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "Require que se prema dúas veces a tecla Saír para saír de RetroArch."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Vibrar ao presionar a tecla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Activar a vibración do dispositivo (para núcleos compatibles)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Resistencia á vibración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Especifique a magnitude dos efectos de retroalimentación háptica."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Controis de menú unificados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Usa os mesmos controis tanto para o menú como para o xogo. Aplícase ao teclado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Desactivar o botón de información"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Se está activado, ignoraranse as pulsacións do botón Información."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Desactivar o botón de busca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Se está activado, ignoraranse as pulsacións do botón Buscar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Botóns Intercambiar menú Aceptar e Cancelar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Cambiar os botóns para Aceptar/Cancelar. Desactivada é a orientación do botón xaponés, activada é a orientación occidental."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Botóns de desprazamento de cambio de menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Intercambiar botóns para desprazarse. Desactivado despraza 10 elementos con L/R e alfabeticamente con L2/R2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Menú de control de todos os usuarios"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Permitir a calquera usuario controlar o menú. Se está desactivado, só o usuario 1 pode controlar o menú."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Activar tecla de acceso rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Cando estea asignada, a tecla \"Habilitar tecla de acceso rápido\" debe manterse antes de que se recoñeza calquera outra tecla de acceso rápido. Permite asignar os botóns do controlador ás funcións das teclas rápidas sen afectar a entrada normal. Asignar o modificador só ao controlador non o requirirá para as teclas rápidas do teclado, e viceversa, pero ambos os modificadores funcionan para ambos os dispositivos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Se esta tecla de acceso rápido está ligada a calquera teclado, botón de alegría ou eixo de alegría, todas as outras teclas de acceso rápido desactivaranse a non ser que tamén se manteña esta tecla de acceso rápido ao mesmo tempo.\nIsto é útil para implementacións centradas en RETRO_KEYBOARD que consultan unha gran área do teclado, onde se atopa. non é desexable que as teclas de acceso rápido se poñan no camiño."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Retardo de activación de teclas de acceso rápido (fotogramas)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Engade un atraso nos fotogramas antes de que se bloquee a entrada normal despois de premer a tecla \"Habilitar tecla de acceso rápido\" asignada. Permite capturar a entrada normal desde a tecla \"Habilitar tecla de acceso rápido\" cando se asigna a outra acción (por exemplo, \"Seleccionar\" de RetroPad)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Combinación de tipos de dispositivo de teclas rápidas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Bloquea todas as teclas de acceso rápido dos tipos de dispositivos do teclado e do controlador se calquera dos tipos ten \"Habilitar teclas de acceso rápido\" configurada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Alternar menú (Combo de controlador)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Combinación de botóns do controlador para cambiar o menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Alternar menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Cambia a visualización actual entre menú e contido en execución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Saír (Combo de controlador)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Combinación de botóns do controlador para saír de RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Saír"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Pecha RetroArch, garantindo que todos os datos gardados e os ficheiros de configuración se lavan no disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Pechar contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Pecha o contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Restablecer contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Reinicia o contido actual dende o principio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Avance rápido (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Cambia entre a velocidade de avance rápido e a normal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Avance rápido (manteña)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Permite o avance rápido cando se manteña. O contido execútase á velocidade normal cando se solta a tecla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Cámara lenta (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Cambia entre a cámara lenta e a velocidade normal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Cámara lenta (manteña)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Permite a cámara lenta cando se manteña. O contido execútase á velocidade normal cando se solta a tecla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Rebobinar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Rebobina o contido actual mentres se mantén premida a tecla. A 'Soporte para rebobinar' debe estar activada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "En pausa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Cambia o contido en execución entre estados en pausa e non pausa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Avance de Fotograma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Avanza o contido nun fotograma cando está en pausa."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Silenciar audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Activa/desactiva a saída de audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Subir volume"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Aumenta o nivel de volume de audio de saída."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Baixar o volume"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Diminúe o nivel de volume de audio de saída."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Estado de carga"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Carga o estado gardado do espazo seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Gardar Estado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Garda o estado no slot seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Gardar na seguinte ranura de estado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Incrementa o índice de ranura de estado de gardar seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Gardar na anterior ranura de estado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Reduce o índice de ranura de estado de gardar seleccionado actualmente."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Expulsión de disco (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Se a bandexa do disco virtual está pechada, ábrea e elimina o disco cargado. En caso contrario, insire o disco seleccionado actualmente e pecha a bandexa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Disco seguinte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Aumenta o índice do disco seleccionado actualmente. A bandexa do disco virtual debe estar aberta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Disco anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Reduce o índice do disco seleccionado actualmente. A bandexa do disco virtual debe estar aberta."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Sombreadores (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Activa/desactiva o sombreador seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Seguinte Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Carga e aplica o seguinte ficheiro predefinido de sombreadores na raíz do directorio \"Sombreadores de vídeo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Shader anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Carga e aplica o ficheiro predefinido do sombreador anterior na raíz do directorio \"Sombreadores de vídeo\"."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Trucos (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Activa/desactiva o truco seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Próximo índice de trucos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Aumenta o índice de trucos seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Índice de trucos anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Reduce o índice de trucos seleccionado actualmente."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Facer captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Captura unha imaxe do contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Gravación (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Inicia/detén a gravación da sesión actual nun ficheiro de vídeo local."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Transmisión en tempo real (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Inicia/detén a transmisión da sesión actual a unha plataforma de vídeo en liña."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Reproducir a Repetición"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Reproduce o ficheiro gardado dende a ranura escollida actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Reproducir a gravación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Reproduce o ficheiro gardado dende a ranura escollida actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Deter a gravación/reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Detén a gravación/reprodución da reprodución actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "Próxima ranura de repetición"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Aumenta o índice da ranura de reprodución seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Ranura de repetición anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Reduce o índice da ranura de reprodución seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Colle o rato (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Colle ou solta o rato. Cando se colle, o cursor do sistema está oculto e confinado á xanela de visualización de RetroArch, mellorando a entrada relativa do rato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Foco do xogo (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Activa/desactiva o modo \"Game Focus\". Cando o contido ten o foco, as teclas de acceso rápido están desactivadas (a entrada completa do teclado pásase ao núcleo en execución) e colle o rato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Pantalla completa (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Cambia entre os modos de visualización de pantalla completa e de xanela."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Menú do escritorio (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Abre a interface de usuario de escritorio complementaria WIMP (Windows, Iconas, Menús, Pointer)."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Sincronizar coa taxa de fotogramas do contido exacto (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Activa/desactiva a sincronización coa taxa de fotogramas de contido exacta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Correr por diante (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Activa/desactiva correr por diante."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Fotogramas preventivos (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Activa/desactiva os fotogramas preventivos."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Mostrar FPS (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Activa/desactiva o indicador de estado de \"fotogramas por segundo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Mostrar estatísticas técnicas (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Activa/desactiva a visualización das estatísticas técnicas en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Superposición de teclado (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Activa/desactiva a superposición do teclado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Seguinte superposición"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Cambia ao seguinte deseño dispoñible da superposición en pantalla activa actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Servizo de intelixencia artificial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Captura unha imaxe do contido actual para traducir e/ou ler en voz alta calquera texto en pantalla. O \"Servizo AI\" debe estar activado e configurado."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Ping de Netplay (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Activa/desactiva o contador de ping para a sala de netplay actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Aloxamento de Netplay (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Activa/desactiva o hospedaxe de netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Netplay Play/Spectate Mode (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Cambia a sesión actual de netplay entre os modos \"xogar\" e \"espectador\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Chat de Netplay Player"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Envía unha mensaxe de chat á sesión actual de netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Activar/desactivar Netplay Fade Chat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Alterna entre mensaxes de chat de netplay esvaecidas e estáticas."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "Enviar información de depuración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SEND_DEBUG_INFO,
   "Envía información de diagnóstico sobre o teu dispositivo e a configuración de RetroArch aos nosos servidores para a súa análise."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Tipo de dispositivo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Especifica o tipo de controlador emulado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Tipo analóxico a dixital"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Use o stick analóxico especificado para a entrada de D-Pad. Os modos \"forzados\" anulan a entrada analóxica nativa principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Mapear o stick analóxico especificado para a entrada de D-Pad.\nSe o núcleo ten compatibilidade analóxica nativa, a asignación de D-Pad desactivarase a menos que se seleccione unha opción \"(Forzada)\".\nSe o mapeo de D-Pad é forzada, o núcleo non recibirá ningunha función analóxica. entrada desde o stick especificado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Índice de dispositivos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "O controlador físico recoñecido por RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Porto mapeado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Especifica que porto principal recibirá entrada do porto do controlador frontend %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Establecer todos os controis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Asigne todas as direccións e botóns, un despois do outro, na orde en que aparecen neste menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Restablecer os controis predeterminados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Borra a configuración de ligazón de entrada aos seus valores predeterminados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Gardar o perfil do controlador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Garda un ficheiro de configuración automática que se aplicará automaticamente sempre que se detecte de novo este controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Índice do rato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "O rato físico recoñecido por RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Botón B (abaixo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Botón Y (esquerda)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Seleccione o botón"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Botón de inicio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "D-Pad arriba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "D-Pad abaixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "D-Pad esquerda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "D-Pad dereita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Botón A (dereita)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Botón X (arriba)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "Botón L (ombreiro)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "Botón R (ombreiro)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "Botón L2 (disparador)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "Botón R2 (disparador)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "Botón L3 (polgar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "Botón R3 (polgar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Esquerda Analóxica X+ (dereita)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "X analóxico esquerdo (esquerda)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Analóxico esquerdo Y+ (abaixo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Analóxico esquerdo Y- (arriba)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Analóxico dereito X+ (dereito)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Analóxico dereito X- (esquerda)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Analóxico dereito Y+ (abaixo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Analóxico dereito Y- (arriba)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Gatillo de arma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Recarga de armas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Arma Auxiliar A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Arma Auxiliar B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Arma Auxiliar C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Inicio do arma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Selección de armas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Arma D-Pad arriba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Arma D-Pad abaixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Arma D-Pad esquerda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Arma D-Pad dereita"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[adiante non dispoñible]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "O núcleo actual é incompatible con adiante debido á falta de soporte determinista para o estado de gardado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
   "Run-ahead para reducir a latencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
   "Execute a lóxica central un ou máis fotogramas por diante e despois cargue o estado de novo para reducir o atraso de entrada percibido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Número de fotogramas para correr adiante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "O número de fotogramas que se van adiante. Causa problemas de xogo, como tremores, se se supera o número de fotogramas de atraso internos do xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
   "Use a segunda instancia para Run-Ahead"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
   "Use unha segunda instancia do núcleo de RetroArch para avanzar. Evita problemas de audio debido ao estado de carga."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Ocultar avisos de adianto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Oculta a mensaxe de aviso que aparece ao usar Run-Ahead e o núcleo non admite os estados de gardar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_UNSUPPORTED,
   "[Fotogramas preventivos non dispoñibles]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_UNSUPPORTED,
   "O núcleo actual é incompatible con fotogramas preventivos debido á falta de soporte determinista para o estado de gardar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_ENABLE,
   "Executar fotogramas preventivos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_ENABLE,
   "Volve executar a lóxica principal coa entrada máis recente cando o estado do controlador cambie. Máis rápido que Run-Ahead, pero non evita problemas de audio que poden ter os núcleos cos estados de carga."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Número de fotogramas preventivos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "O número de fotogramas para volver executar. Causa problemas de xogo, como tremores, se se supera o número de fotogramas de atraso internos do xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_HIDE_WARNINGS,
   "Ocultar avisos de fotogramas preventivos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_HIDE_WARNINGS,
   "Oculta a mensaxe de aviso que aparece cando un núcleo é incompatible cos fotogramas preventivos."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Contexto compartido de hardware"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Dar aos núcleos renderizados por hardware o seu propio contexto privado. Evita ter que asumir cambios de estado do hardware entre fotogramas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Permitir que os núcleos cambien o controlador de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Permitir que os núcleos cambien a un controlador de vídeo diferente ao cargado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Cargar Dummy ao apagar o núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Algúns núcleos teñen unha función de apagado; cargar un núcleo ficticio evitará que RetroArch se apague."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Algúns núcleos poden ter unha función de apagado. Se se deixa desactivada esta opción, seleccionar o procedemento de apagado provocaría o peche de RetroArch.\nAo activar esta opción, cargarase un núcleo ficticio para que permanezamos dentro do menú e RetroArch non se apagará."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Iniciar un núcleo de forma automática"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Comprobe se falta firmware antes de cargar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Comproba se todo o firmware necesario está presente antes de tentar cargar contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE,
   "Algúns núcleos poden necesitar ficheiros de firmware ou bios. Se esta opción está activada, RetroArch non permitirá iniciar o núcleo se falta algún elemento de firmware obrigatorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Categorías de opcións do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Permitir que os núcleos presenten opcións en submenús baseados en categorías. NOTA: O núcleo debe recargarse para que os cambios teñan efecto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Ficheiros de información básica da caché"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Manter unha caché local persistente da información básica instalada. Reduce moito os tempos de carga en plataformas con acceso lento ao disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Omite as funcións de gardar estados da información do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Especifica se ignorar as capacidades de estado de salvamento da información básica, o que permite probar con funcións relacionadas (adiante, rebobinar, etc.)."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Recarga sempre o núcleo ao executar contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Reinicie RetroArch ao iniciar contido, aínda que o núcleo solicitado xa estea cargado. Isto pode mellorar a estabilidade do sistema, a costa de aumentar os tempos de carga."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Permitir a rotación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Permitir que os núcleos establezan a rotación. Cando se desactiva, ignóranse as solicitudes de rotación. Útil para configuracións que xiran manualmente a pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Xestionar núcleos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Realiza tarefas de mantemento sen conexión nos núcleos instalados (copia de seguridade, restauración, eliminación, etc.) e consulta a información do núcleo."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Xestionar núcleos"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Instala ou desinstala núcleos distribuídos a través de Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Instalar o núcleo"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Desinstalar o núcleo"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "Mostrar \"Xestionar núcleos\""
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Mostra a opción \"Xestionar núcleos\" no menú principal."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Instalando o Núcleo: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "O núcleo desinstalarase ao saír de RetroArch."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "O núcleo estase descargando"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Gardar a configuración ao saír"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Garda os cambios no ficheiro de configuración ao saír."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Garda os cambios no ficheiro de configuración ao saír. Útil para os cambios realizados no menú. Sobrescribe o ficheiro de configuración, os #include e os comentarios non se conservan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Garda os ficheiros de reasignación ao saír"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Garda os cambios en calquera ficheiro de reasignación de entrada activo ao pechar contido ou saír de RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Cargar opcións básicas específicas de contido automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Carga opcións básicas personalizadas de forma predeterminada ao inicio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Cargar ficheiros de anulación automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Carga a configuración personalizada ao inicio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Cargar ficheiros de reasignación automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Carga controis personalizados ao inicio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Cargar ficheiros de índice de disco inicial automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "Cambia ao último disco usado ao iniciar contido multidisco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Cargar presets de Shader automaticamente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Use o ficheiro de opcións básicas globais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Garda todas as opcións principais nun ficheiro de configuración común (retroarch-core-options.cfg). Cando estea desactivado, as opcións para cada núcleo gardaranse nun cartafol/ficheiro separado específico do núcleo no directorio \"Configuracións\" de RetroArch."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Ordenar os gardados en cartafoles polo nome do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Ordena os ficheiros gardados en cartafoles co nome do núcleo utilizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Ordenar os estados de gardar en cartafoles polo nome do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Ordena os estados de gardado en cartafoles co nome do núcleo utilizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Ordenar os gardados en cartafoles por directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Ordena os ficheiros gardados en cartafoles co nome do directorio no que se atopa o contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Ordenar estados gardados en cartafoles por directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Ordena os estados de gardado en cartafoles co nome do directorio no que se atopa o contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Non sobrescriba SaveRAM ao cargar o estado de gardar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Bloquee que SaveRAM non se sobrescriba ao cargar estados de gardar. Podería levar a xogos con erros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Bloquee que SaveRAM non se sobrescriba ao cargar estados de gardar. Podería levar a xogos con erros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Garda automaticamente a SaveRAM non volátil nun intervalo regular (en segundos)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Garda automaticamente a SRAM non volátil nun intervalo regular. Esta opción está desactivada por defecto a non ser que se estableza o contrario. O intervalo mídese en segundos. Un valor de 0 desactiva o gardado automático."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Reproducir Intervalo de punto de control"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Marca automaticamente o estado do xogo durante a gravación da repetición nun intervalo regular (en segundos)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Garda automaticamente o estado do xogo durante a gravación da repetición nun intervalo regular. Esta opción está desactivada por defecto a non ser que se estableza o contrario. O intervalo mídese en segundos. Un valor de 0 desactiva a gravación do punto de control."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Incremento do índice de estado de gardar automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Antes de facer un estado de gardado, o índice de estado de gardado aumenta automaticamente. Ao cargar contido, o índice establecerase co índice existente máis alto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Incrementa o índice de reprodución automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Antes de facer unha repetición, o índice de repetición aumenta automaticamente. Ao cargar contido, o índice establecerase co índice existente máis alto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Incremento automático máximo de estados de gardar para manter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Limite o número de estados de gardar que se crearán cando se active \"Incrementar o índice de estado de gardar automaticamente\". Se se supera o límite ao gardar un novo estado, eliminarase o estado existente co índice máis baixo. Un valor de \"0\" significa que se rexistrarán estados ilimitados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Máximo de repeticións de incremento automático para manter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Limite o número de repeticións que se crearán cando se active \"Incrementar índice de reprodución automaticamente\". Se se supera o límite ao gravar unha nova reprodución, eliminarase a reprodución existente co índice máis baixo. Un valor de \"0\" significa que se gravarán repeticións ilimitadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Estado de gardado automático"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Fai automaticamente un estado de gardado cando o contido estea pechado. RetroArch cargará automaticamente este estado de gardado se \"Cargar estado automaticamente\" está activado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Estado de carga automática"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Carga automaticamente o estado de gardado automático ao iniciar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Gardar miniaturas do estado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Mostra as miniaturas dos estados de gardado no menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Compresión SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Escribe ficheiros SaveRAM non volátiles nun formato arquivado. Reduce drasticamente o tamaño do ficheiro a costa dun aumento (despreciable) dos tempos de aforro/carga.\nSó se aplica aos núcleos que permiten gardar a través da interface SaveRAM libretro estándar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Gardar compresión de estado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Escribe ficheiros de estado de gardar nun formato arquivado. Reduce drasticamente o tamaño do ficheiro a costa de aumentar os tempos de aforro/carga."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Ordena as capturas de pantalla en cartafoles segundo o directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Ordena as capturas de pantalla en cartafoles co nome do directorio no que se atopa o contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Escribir gardados no directorio de contidos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Usa o directorio de contido como directorio de ficheiros de gardar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Escribir estados de gardar no directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Usa o directorio de contido como directorio de estado de gardar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Os ficheiros do sistema están no directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Use o directorio de contido como directorio do sistema/BIOS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Escribe capturas de pantalla no Directorio de contidos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Usa o directorio de contido como directorio de capturas de pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Gardar rexistro de tempo de execución (por núcleo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Fai un seguimento do tempo que leva cada elemento de contido, con rexistros separados por núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Gardar rexistro de tempo de execución (agregado)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Fai un seguimento do tempo que leva cada elemento de contido, rexistrado como o total agregado en todos os núcleos."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Verbosidade de rexistro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Rexistra eventos nun terminal ou ficheiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Nivel de rexistro de frontend"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Establece o nivel de rexistro para o frontend. Se un nivel de rexistro emitido pola interface está por debaixo deste valor, ignorarase."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Establece o nivel de rexistro para os núcleos. Se un nivel de rexistro emitido por un núcleo está por debaixo deste valor, ignórase."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Establece o nivel de rexistro para os núcleos de libretro (GET_LOG_INTERFACE). Se un nivel de rexistro emitido por un núcleo de libretro está por debaixo do nivel de libretro_log, ignorarase. Os rexistros de DEBUG sempre se ignoran a menos que se active o modo detallado (--verbose).\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (Depuración)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Aviso)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (Erro)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Iniciar sesión no ficheiro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Redirixe as mensaxes do rexistro de eventos do sistema ao ficheiro. Require que a \"Verbosidade de rexistro\" estea activada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Ficheiros de rexistro de marca de tempo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Ao iniciar sesión no ficheiro, redirixe a saída de cada sesión de RetroArch a un novo ficheiro con marca de tempo. Se está desactivado, o rexistro sobrescríbese cada vez que se reinicie RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Contadores de rendemento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Contadores de rendemento para RetroArch e núcleos. Os datos dos contadores poden axudar a determinar os pescozos de botella do sistema e mellorar o rendemento."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Mostrar ficheiros e directorios ocultos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Mostra ficheiros e directorios ocultos no Explorador de ficheiros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtrar extensións descoñecidas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtra os ficheiros que se mostran no Explorador de ficheiros polas extensións compatibles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Usa o reprodutor multimedia integrado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Filtrar por núcleo actual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Lembra o último directorio de inicio usado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Abre o explorador de ficheiros na última localización utilizada ao cargar contido do directorio de inicio. Nota: a localización restablecerase ao valor predeterminado ao reiniciar RetroArch."
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Rebobinar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Cambiar a configuración de rebobinado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Contador de tempo de fotogramas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Cambia a configuración que inflúe no contador de tempo de fotogramas.\nSó activo cando o vídeo en conversas está desactivado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Taxa de avance rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "A velocidade máxima á que se executará o contido ao utilizar o avance rápido (por exemplo, 5.0x para contido de 60 fps = límite de 300 fps). Se se establece en 0,0x, a relación de avance rápido é ilimitada (sen límite de FPS)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "A velocidade máxima á que se executará o contido ao utilizar o avance rápido. (Por exemplo, 5.0 para contido de 60 fps => límite de 300 fps).\nRetroArch irá inactiva para garantir que non se supere a velocidade máxima. Non confíe nesta tapa para ser perfectamente precisa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Frameskip de avance rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Saltar fotogramas segundo a taxa de avance rápido. Isto aforra enerxía e permite o uso da limitación de cadros de terceiros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Velocidade de cámara lenta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "A velocidade coa que se reproducirá o contido cando se utiliza a cámara lenta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Velocidade de fotogramas do menú de aceleración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Asegúrate de que a taxa de fotogramas está limitada mentres está dentro do menú."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Rebobinaxe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Volver a un punto anterior do xogo recente. Isto provoca un grave golpe de rendemento ao xogar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Rebobinar fotogramas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "O número de fotogramas a rebobinar por paso. Os valores máis altos aumentan a velocidade de rebobinado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Tamaño do búfer de rebobinado (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "A cantidade de memoria (en MB) a reservar para o búfer de rebobinado. Aumentar isto aumentará a cantidade de historial de rebobinado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Paso de tamaño do búfer de rebobinado (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Cada vez que se aumente ou diminúe o valor do tamaño do búfer de rebobinado, cambiará nesta cantidade."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Restablecer despois do avance rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Restablece o contador de tempo de fotograma despois do avance rápido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Restablecer despois do estado de carga"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Restablece o contador de tempo de fotograma despois de cargar un estado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Restablecer despois de gardar o estado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Restablece o contador de tempo de fotograma despois de gardar un estado."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Calidade da gravación"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Baixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "Medio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Alto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Sen perdas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM de alta calidade"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Configuración de gravación personalizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Fíos de gravación"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Use a gravación do filtro posterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Captura a imaxe despois de aplicar filtros (pero non sombreadores). O vídeo terá un aspecto tan elegante como o que ves na túa pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Usa a gravación da GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Grava a saída do material sombreado da GPU, se está dispoñible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Modo de transmisión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Calidade de transmisión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Baixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "Medio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Alto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Configuración de transmisión personalizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Título do fluxo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "URL do fluxo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "Porto de fluxo UDP"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Superposición en pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Axusta os marcos e os controis en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Deseño de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Axustar o deseño do vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Notificacións en pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Axustar as notificacións en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Visibilidade de notificacións"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Alterna a visibilidade de tipos específicos de notificacións."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Superposición de visualización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "As superposicións úsanse para os bordos e os controis en pantalla."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Mostrar superposición detrás do menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Mostra a superposición detrás en vez de diante do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Ocultar a superposición no menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Oculta a superposición mentres está dentro do menú e móstraa de novo ao saír do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Ocultar a superposición cando o controlador está conectado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Oculta a superposición cando un controlador físico está conectado no porto 1 e móstrao de novo cando o controlador estea desconectado."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Oculta a superposición cando un controlador físico está conectado no porto 1. A superposición non se restaurará automaticamente cando se desconecte o controlador."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Mostrar entradas en superposición"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Mostra as entradas rexistradas na superposición en pantalla. \"Tocado\" destaca os elementos de superposición nos que se preme ou se fai clic. \"Físico (controlador)\" destaca a entrada real que se pasa aos núcleos, normalmente desde un controlador/teclado conectado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Tocado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Físico (controlador)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Configura a entrada de arma lixeira enviada desde a superposición."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Rato superposto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Configure a entrada do rato enviada desde a superposición. Nota: os toques con 1, 2 e 3 dedos envían clics no botón esquerdo, dereito e medio."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "Predefinido de superposición de teclado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Seleccione unha superposición de teclado no Explorador de ficheiros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Superposición de teclado de escala automática"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Axusta a superposición do teclado á súa relación de aspecto orixinal. Desactiva para estirar á pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Opacidade de superposición do teclado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Opacidade de todos os elementos da interface de usuario da superposición do teclado."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Porto de arma lixeira"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Establece o porto principal para que reciba a entrada da arma lixeira de superposición."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Calquera"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Activar ao tocar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Enviar entrada de activación coa entrada de punteiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Retardo de disparo (fotogramas)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Retrasa a entrada do disparador para permitir que o cursor se mova. Este atraso tamén se usa para esperar a que se realice o reconto multitáctil correcto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Entrada de 2 toques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Seleccione a entrada para enviar cando hai dous punteiros na pantalla. O retardo de disparo debe ser distinto de cero para distinguilo doutra entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Entrada de 3 toques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Seleccione a entrada para enviar cando aparezan tres punteiros na pantalla. O retardo de disparo debe ser distinto de cero para distinguilo doutra entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Entrada de 4 toques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Selecciona a entrada para enviar cando hai catro punteiros na pantalla. O retardo de disparo debe ser distinto de cero para distinguilo doutra entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Permitir fóra da pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Permite apuntar fóra dos límites. Desactivar para fixar o obxectivo fóra da pantalla ao bordo do límite."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Velocidade do rato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Axusta a velocidade de movemento do cursor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Prema longa para arrastrar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Mantén presionada a pantalla para comezar a presionar un botón."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Limiar de presión longa (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Axusta o tempo de espera necesario para unha pulsación longa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Toca dúas veces para arrastrar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Toca dúas veces a pantalla para comezar a premer un botón no segundo toque. Engade latencia aos clics do rato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Limiar de dobre toque (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Axusta o tempo permitido entre toques cando detectes un toque dobre."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Limiar para pasar o dedo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Axusta o intervalo de deriva permitido ao detectar unha pulsación ou un toque longos. Expresado como unha porcentaxe da dimensión de pantalla máis pequena."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Activar o deseño de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Os deseños de vídeo úsanse para os marcos e outras obras de arte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Ruta de deseño de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Seleccione un deseño de vídeo no Explorador de ficheiros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Vista seleccionada"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Seleccione unha vista dentro do deseño cargado."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Notificacións en pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Mostra mensaxes en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Widgets gráficos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Usa animacións decoradas, notificacións, indicadores e controis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Escala os widgets gráficos automaticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Redimensiona automaticamente as notificacións, os indicadores e os controis decorados en función da escala do menú actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Anulación de escala de widgets gráficos (pantalla completa)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Aplique unha substitución manual do factor de escala ao debuxar widgets de visualización en modo de pantalla completa. Só se aplica cando \"Escala automaticamente widgets gráficos\" está desactivado. Pódese usar para aumentar ou diminuír o tamaño das notificacións, indicadores e controis decorados independentemente do propio menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Anulación de escala de widgets gráficos (en ventás)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Intervalo de actualización da taxa de fotogramas (en fotogramas)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "A visualización da velocidade de fotogramas actualizarase no intervalo establecido en fotogramas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Mostrar conta de fotogramas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Mostra o reconto de fotogramas actual na pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Mostrar estatísticas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Mostra estatísticas técnicas en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Mostrar o uso da memoria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Mostra a cantidade total e utilizada de memoria no sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Intervalo de actualización de uso da memoria (en fotogramas)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "A visualización do uso da memoria actualizarase no intervalo establecido en fotogramas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Mostrar ping de Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Mostra o ping para a sala de netplay actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Notificación de inicio \"Cargar contido\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Mostra unha breve animación de comentarios de inicio ao cargar contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Notificacións de conexión de entrada (configuración automática)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Notificacións de códigos de trucos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Mostra unha mensaxe en pantalla cando se apliquen códigos de trucos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Notificacións de parches"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Mostra unha mensaxe en pantalla ao aplicar parches suaves ás ROMs."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Mostra unha mensaxe en pantalla ao conectar/desconectar dispositivos de entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Introducir notificacións cargadas de reasignación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Mostra unha mensaxe en pantalla ao cargar ficheiros de remap de entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Config Anular notificacións cargadas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Mostra unha mensaxe en pantalla ao cargar ficheiros de anulación de configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Notificacións iniciais do disco restaurado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Mostra unha mensaxe en pantalla ao restaurar automaticamente ao iniciar o último disco usado de contido multidisco cargado mediante listas de reprodución M3U."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Notificacións de control de disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Mostra unha mensaxe na pantalla ao inserir e expulsar discos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Gardar notificacións estatais"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Notificacións de avance rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Mostra un indicador en pantalla cando avance rápido contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Notificacións de captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Mostra unha mensaxe na pantalla ao facer unha captura de pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Persistencia de notificación de captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Define a duración da mensaxe da captura de pantalla en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Rápido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Moi rápido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Instantáneo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Captura de pantalla Efecto Flash"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Mostra un efecto de parpadeo branco na pantalla coa duración desexada ao facer unha captura de pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "ON (rápido)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Notificacións de taxas de actualización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Mostra unha mensaxe en pantalla ao configurar a frecuencia de actualización."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Notificacións extra de Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Mostra mensaxes en pantalla de netplay non esenciais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Notificacións só no menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Mostra notificacións só cando o menú estea aberto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Fonte de notificación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Seleccione o tipo de letra para as notificacións en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Tamaño da notificación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Especifique o tamaño da fonte en puntos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Posición de notificación (horizontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Especifique a posición do eixe X personalizada para o texto en pantalla. 0 é o bordo esquerdo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Posición de notificación (vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Especifique a posición personalizada do eixe Y para o texto en pantalla. 0 é o bordo inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Cor da notificación (vermello)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Establece o valor vermello da cor do texto OSD. Os valores válidos están entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Cor da notificación (verde)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Establece o valor verde da cor do texto OSD. Os valores válidos están entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Cor da notificación (azul)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Establece o valor azul da cor do texto OSD. Os valores válidos están entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Antecedentes da notificación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Activa unha cor de fondo para o OSD."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Cor de fondo da notificación (vermello)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Establece o valor vermello da cor de fondo da OSD. Os valores válidos están entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Cor de fondo da notificación (verde)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Establece o valor verde da cor de fondo do OSD. Os valores válidos están entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Cor de fondo da notificación (azul)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Establece o valor azul da cor de fondo do OSD. Os valores válidos están entre 0 e 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Opacidade do fondo da notificación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Establece a opacidade da cor de fondo da OSD. Os valores válidos están entre 0,0 e 1,0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Visibilidade dos elementos do menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Alterna a visibilidade dos elementos do menú en RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Aparencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Cambiar a configuración da aparencia da pantalla do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "Icona da aplicación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Cambiar icona da app."
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "Aspecto da pantalla inferior 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Cambiar a configuración da aparencia da pantalla inferior."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Amosar axustes avanzados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Amosar axustes avanzados para usuarios avanzados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Modo Quiosco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Protexe a configuración ocultando todos os axustes relacionados coa configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Establecer o contrasinal para desactivar o modo quiosco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Ao proporcionar un contrasinal ao activar o modo quiosco fai posible desactivalo posteriormente desde o menú, accedendo ao menú principal, seleccionando Desactivar modo quiosco e introducindo o contrasinal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Navegación envolvente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Pausa o contido cando o menú estea activo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Pausa o contido en execución se o menú está activo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Retomar o contido despois de usar os estados de gardar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Pecha automaticamente o menú e retoma o contido despois de gardar ou cargar un estado. Desactivar isto pode mellorar o rendemento do estado de gardado en dispositivos moi lentos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Retomar o contido despois de cambiar os discos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Pecha automaticamente o menú e retoma o contido despois de inserir ou cargar un novo disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Saír ao pechar contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Sae automaticamente de RetroArch ao pechar o contido. A 'CLI' só sae cando o contido se inicia a través da liña de comandos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Tempo de espera do salvapantallas do menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Mentres o menú está activo, aparecerá un protector de pantalla despois do período de inactividade especificado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Menú Salvapantallas Animación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Activa un efecto de animación mentres o salvapantallas do menú está activo. Ten un impacto modesto no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Neve"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Campo de estrelas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Axusta a velocidade do efecto de animación do salvapantallas do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Compatibilidade con rato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Permitir que o menú se controle cun rato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Toca Soporte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Permitir que o menú se controle cunha pantalla táctil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Tarefas roscadas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Realiza tarefas nun fío separado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Pausa o contido cando non está activo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Pausa o contido cando RetroArch non é a xanela activa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Desactivar a composición do escritorio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Os xestores de fiestras usan a composición para aplicar efectos visuais, detectar ventás que non responden, entre outras cousas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Desactivar forzadamente a composición. A desactivación só é válida en Windows Vista/7 polo momento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Aceleración de desprazamento do menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Velocidade máxima do cursor ao manter unha dirección para desprazarse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Retraso de desprazamento do menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Atraso inicial en milisegundos ao manter unha dirección para desprazarse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Inicia UI Companion ao arrancar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Inicie o controlador complementario da interface de usuario no arranque (se está dispoñible)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Menú de escritorio (reinicio necesario)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Abre o menú Escritorio ao iniciar"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Menú rápido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Alterna a visibilidade dos elementos do menú no menú rápido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Configuración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Alterna a visibilidade dos elementos do menú no menú Configuración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Mostrar \"Cargar núcleo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Mostra a opción \"Cargar núcleo\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Mostrar \"Cargar contido\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Mostra a opción \"Cargar contido\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Mostrar \"Cargar disco\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Mostra a opción \"Cargar disco\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Mostrar \"Dump Disc\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Mostra a opción \"Dump Disc\" no menú principal."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Mostra a opción \"Expulsar disco\" no menú principal."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Mostrar \"Actualizador en liña\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Mostra a opción \"Actualizador en liña\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Mostrar \"Descargador principal\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Mostra a posibilidade de actualizar núcleos (e ficheiros de información básica) na opción \"Actualizador en liña\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Mostrar o \"Actualizador de miniaturas\" herdado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Mostra a entrada para descargar paquetes de miniaturas legados na opción \"Actualizador en liña\"."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Mostra a opción \"Información\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Mostrar \"Ficheiro de configuración\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Mostra a opción \"Ficheiro de configuración\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Mostrar \"Axuda\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Mostra a opción \"Axuda\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Mostrar \"Saír de RetroArch\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Mostra a opción \"Saír de RetroArch\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Mostrar \"Reiniciar RetroArch\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Mostra a opción \"Reiniciar RetroArch\" no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Mostrar \"Configuración\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Mostra o menú \"Configuración\". (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Establecer o contrasinal para activar \"Configuración\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Ao proporcionar un contrasinal ao ocultar a pestana de configuración, fai posible restauralo posteriormente desde o menú, indo á pestana Menú principal, seleccionando \"Activar a pestana de configuración\" e introducindo o contrasinal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Mostrar \"Favoritos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Mostra o menú \"Favoritos\". (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Mostrar \"Imaxes\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Mostra o menú \"Imaxes\". (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Mostrar \"Música\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Mostra o menú \"Música\". (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Mostrar \"Vídeos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Mostra o menú \"Netplay\". (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Mostrar \"Historial\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Mostra o menú do historial recente. (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "Mostrar \"Importar contido\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "Mostra o menú \"Importar contido\". (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH( /* FIXME can now be replaced with MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD */
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Mostrar \"Importar contido\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Mostra unha entrada \"Importar contido\" dentro do menú principal ou do submenú das listas de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Menú principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Menú Listas de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Mostrar \"Listas de reprodución\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Mostra as listas de reprodución. (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Mostrar \"Explorar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Mostra a opción do explorador de contido. (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Mostrar \"Núcleos sen contido\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Especifique o tipo de núcleo (se o hai) para mostrar no menú \"Núcleos sen contido\". Cando se define como \"Personalizado\", a visibilidade do núcleo individual pódese alternar a través do menú \"Xestionar núcleos\". (Requírese reiniciar en Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Todo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "De un só uso"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Mostrar a data e a hora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Mostra a data e/ou hora actual dentro do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Estilo de data e hora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Cambiar o estilo de data e/ou hora actual móstrase dentro do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Separador de datas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Especifique o carácter para usar como separador entre os compoñentes ano/mes/día cando se mostre a data actual dentro do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Mostrar o nivel de batería"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Amosa o nivel de batería actual dentro do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Amosar o nome principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Mostrar o nome do núcleo actual dentro do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Mostrar subetiquetas do menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Mostra información adicional para os elementos do menú."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Mostrar a pantalla de inicio"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Mostrar a pantalla de inicio no menú. Establécese automaticamente como falso despois de que o programa se inicie por primeira vez."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Mostrar \"Resumo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Mostra a opción de contido do currículo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Mostrar \"Reiniciar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Mostra a opción de reiniciar contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Mostrar \"Pechar contido\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Mostra a opción \"Pechar contido\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Mostrar o submenú \"Gardar estados\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Mostrar as opcións de estado de gardar nun submenú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Mostrar \"Gardar/Cargar estado\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Mostra as opcións para gardar/cargar un estado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "Mostrar \"Controis de repetición\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "Mostra as opcións para gravar/reproducir ficheiros de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Mostrar \"Desfacer gardar/cargar estado\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Mostra as opcións para desfacer o estado de gardar/cargar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Mostrar \"Opcións Núcleo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Mostra a opción \"Opcións Núcleo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Mostrar \"Opcións de descarga no disco\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Mostra a entrada \"Opcións de descarga no disco\" no menú \"Opcións > Xestionar opcións básicas\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Mostrar controis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Mostra a opción \"Controis\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Mostrar \"Facer captura de pantalla\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Mostra a opción \"Tomar captura de pantalla\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Mostrar \"Iniciar gravación\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Mostrar \"Iniciar gravación\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Mostrar \"Iniciar a emisión\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Mostra a opción \"Iniciar a emisión\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Mostrar \"Superposición en pantalla\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Mostra a opción \"Superposición en pantalla\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Mostrar \"Disposición do vídeo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Mostrar a opción \"Diseño de vídeo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Mostrar \"Latencia\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Mostra a opción \"Latencia\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Mostrar \"Rebobinar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Mostra a opción \"Rebobinar\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Mostrar \"Gardar substitucións básicas\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Mostra a opción \"Gardar substitucións básicas\" no menú \"Anulacións\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Mostrar \"Gardar substitucións do directorio de contido\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Mostra a opción \"Gardar substitucións do directorio de contido\" no menú \"Anulacións\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Mostrar \"Gardar as substitucións do xogo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Mostra a opción \"Gardar substitucións do xogo\" no menú \"Anulacións\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Mostrar \"Cheats\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Mostra a opción \"Cheats\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Mostrar \"Shaders\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Mostra a opción 'Shaders'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Mostrar \"Engadir a favoritos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Mostra a opción \"Engadir a favoritos\"."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "Engadir á lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "Engade o contido a unha lista de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "Crear unha nova lista de reprodución"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "Engadida á lista de reprodución"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Produciuse un erro ao engadir á lista de reprodución: lista de reprodución chea"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "Produciuse un erro ao engadir á lista de reprodución: lista de reprodución chea."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Mostrar \"Set Core Association\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Mostra a opción \"Establecer asociación de núcleos\" cando o contido non se estea a executar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Mostrar \"Restablecer asociación de núcleos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Mostra a opción \"Restablecer asociación principal\" cando o contido non se estea executando."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Mostrar \"Descargar miniaturas\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Mostra a opción \"Descargar miniaturas\" cando o contido non se estea a executar."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Mostra a opción \"Información\"."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Mostrar \"Controladores\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Mostrar a configuración de \"Controladores\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Mostrar \"Vídeo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Mostrar a configuración de \"Vídeo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Mostrar \"Audio\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Mostrar a configuración de \"Audio\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Mostrar \"Entrada\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Mostrar a configuración de \"Entrada\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Mostrar \"Latencia\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Mostrar a configuración de \"Latencia\"."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Mostrar a configuración do \"nucleo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Mostrar \"Configuración\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Mostrar os axustes de \"Configuración\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Mostrar \"Gardar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Mostrar a configuración de \"Gardar\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Mostrar \"Rexistro\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Mostrar a configuración de \"Registro\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Mostrar \"Explorador de ficheiros\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Mostrar a configuración do \"Explorador de ficheiros\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Mostrar \"Acelerador de fotogramas\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Mostrar a configuración de \"Acelerador de fotogramas\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Amosar a gravación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Amosar a configuración de \"Grabación\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Mostrar \"visualización en pantalla\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Mostrar a configuración de \"Visualización en pantalla\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Mostrar \"Interface de usuario\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Mostrar a configuración da \"Interface de usuario\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Mostrar \"Servizo de IA\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Mostrar a configuración do \"Servizo AI\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Mostrar \"Accesibilidade\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Mostrar a configuración de \"Accesibilidade\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Mostrar \"Xestión de enerxía\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Mostrar a configuración de \"Xestión de enerxía\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Amosar \"Logros\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Amosar a configuración de \"Logros\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Amosar \"Rede\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Amosar a configuración de \"Rede\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Mostrar \"Listas de reprodución\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Mostrar a configuración de \"Listas de reprodución\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Mostrar \"Usuario\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Mostrar a configuración de \"Usuario\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Mostrar \"Directorio\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Mostrar a configuración do \"Directorio\"."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "Mostrar 'Steam'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "Mostrar a configuración de \"Steam\"."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Factor de escala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Escala o tamaño dos elementos da interface de usuario no menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Imaxe de fondo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Seleccione unha imaxe para definir como fondo do menú. As imaxes manuais e dinámicas anularán o \"Tema de cor\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Opacidade do fondo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Modifica a opacidade da imaxe de fondo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Opacidade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Modifique a opacidade do fondo do menú predeterminado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Usa o tema de cor do sistema preferido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Use o tema de cor do sistema operativo (se o hai). Anula a configuración do tema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Miniatura primaria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Tipo de miniatura a mostrar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Limiar de ampliación de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Aumenta automaticamente as imaxes en miniatura cunha anchura/alto menor que o valor especificado. Mellora a calidade da imaxe. Ten un impacto moderado no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Animación de texto ticker"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Seleccione o método de desprazamento horizontal usado para mostrar texto longo do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Velocidade do texto do ticker"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Velocidade de animación ao desprazar o texto longo do menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Texto suave"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Use unha animación de desprazamento suave ao mostrar texto de menú longo. Ten un pequeno impacto no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Lembra a selección ao cambiar as pestanas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Lembra a posición anterior do cursor nas pestanas. RGUI non ten pestanas, pero Listas de reprodución e Configuración compórtanse como tales."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Sempre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Só para listas de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Só para o menú principal e a configuración"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Saída do servizo de IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Mostra a tradución como superposición de texto (modo de imaxe), reproduce como texto a voz (voz) ou usa un narrador do sistema como NVDA (narrador)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "URL do servizo AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Un URL http:// que apunta ao servizo de tradución que se vai utilizar."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Activa o servizo AI para que se execute cando se preme a tecla de acceso rápido AI Service."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Pausa durante a tradución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Pausa o núcleo mentres se traduce a pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Lingua de orixe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "O idioma do que se traducirá o servizo. Se se define como \"Predeterminado\", tentará detectar automaticamente o idioma. Se o configura nun idioma específico, a tradución será máis precisa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Lingua de destino"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "O idioma ao que se traducirá o servizo. \"Predeterminado\" é inglés."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Accesibilidade activada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Activa o texto a voz para axudar na navegación por menús."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Velocidade de texto a voz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "A velocidade da voz de texto a voz."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Logros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Consigue logros en xogos clásicos. Para obter máis información, visite \"https://retroachievements.org\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Modo Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Desactiva as trampas, o rebobinado, a cámara lenta e os estados de gardado de carga. Os logros obtidos no modo hardcore están marcados de xeito exclusivo para que poidas mostrar aos demais o que conseguiches sen as funcións de asistencia do emulador. Cambiar esta configuración no tempo de execución reiniciarase o xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Táboa de liderado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Periódicamente envía información contextual do xogo ao sitio web de RetroAchievements. Non ten efecto se o \"Modo Hardcore\" está activado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Insignias de logros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Mostrar distintivos na Lista de logros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Proba logros non oficiais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Usa logros non oficiais e/ou funcións beta para probas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Desbloquear Son"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Reproduce un son cando se desbloquea un logro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Captura de pantalla automática"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Fai automaticamente unha captura de pantalla cando se consiga un logro."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Modo de novo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Comeza a sesión con todos os logros activos (incluso os desbloqueados anteriormente)."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Aparencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Cambia a posición e as compensacións das notificacións de logros en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Posición"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Establece a esquina/borde da pantalla desde a que aparecerán as notificacións de logros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Arriba - esquerda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "Arriba-centro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Arriba-dereita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Abaixo esquerda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Abaixo no centro"
   )

/* Settings > Achievements > Visibility */


/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "O porto do enderezo IP do host. Pode ser un porto TCP ou UDP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Máximo de conexións simultáneas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "O número máximo de conexións activas que o host aceptará antes de rexeitar outras novas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Limitador de ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "A latencia de conexión máxima (ping) que aceptará o host. Establéceo en 0 sen límite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Contrasinal do servidor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "O contrasinal utilizado polos clientes que se conectan ao host."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Spectador do servidor - Só Contrasinal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "O contrasinal utilizado polos clientes que se conectan ao host como espectador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Modo Spectador Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Inicia o xogo en rede no modo espectador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Indica se debes iniciar o xogo en rede no modo espectador. Se se define como verdadeiro, a reprodución en rede estará no modo espectador ao iniciarse. Sempre é posible cambiar de modo máis tarde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Desvanece Chat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Desvanece as mensaxes de chat co paso do tempo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Cor do chat (alcume)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Formato: #RRGGBB ou RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Cor do chat (mensaxe)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Formato: #RRGGBB ou RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Permitir a pausa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Permite aos xogadores facer unha pausa durante o xogo en rede."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Permitir clientes en modo escravo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Permitir conexións en modo escravo. Os clientes en modo escravo requiren moi pouca potencia de procesamento por cada lado, pero sufrirán significativamente a latencia da rede."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Non permitir clientes en modo non escravo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Non permitir conexións non en modo escravo. Non recomendado excepto para redes moi rápidas con máquinas moi débiles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Netplay comproba Fotogramas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "A frecuencia (en fotogramas) que netplay verificará que o host e o cliente están sincronizados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "A frecuencia en fotogramas coa que netplay verificará que o host e o cliente están sincronizados. Na maioría dos núcleos, este valor non terá ningún efecto visible e pódese ignorar. Con núcleos non determinstic, este valor determina a frecuencia con que se sincronizarán os pares de netplay. Con núcleos con erros, establecer este valor en calquera valor distinto de cero causará graves problemas de rendemento. Establécese a cero para non realizar comprobacións. Este valor só se usa n[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Fotogramas de latencia de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "O número de fotogramas de latencia de entrada que netplay debe usar para ocultar a latencia da rede. Reduce o nerviosismo e fai que a reprodución en rede sexa menos intensiva en CPU, a costa dun retraso de entrada notable."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "O número de fotogramas de latencia de entrada que netplay se utiliza para ocultar a latencia da rede.\nCando está en netplay, esta opción atrasa a entrada local, de xeito que o fotograma que se está a executar estea máis preto dos fotogramas que se reciben da rede. Isto reduce o nerviosismo e fai que o xogo en rede sexa menos intensivo en CPU, pero ao prezo dun retraso de entrada notable."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Rango de fotogramas de latencia de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "O intervalo de fotogramas de latencia de entrada que se poden usar para ocultar a latencia da rede. Reduce o nerviosismo e fai que a reprodución en rede sexa menos intensiva en CPU, a costa dun atraso de entrada imprevisible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "O intervalo de fotogramas de latencia de entrada que pode usar netplay para ocultar a latencia da rede.\nSe se establece, netplay axustará o número de fotogramas de latencia de entrada de forma dinámica para equilibrar o tempo da CPU, a latencia de entrada e a latencia da rede. Isto reduce o nerviosismo e fai que o xogo en rede sexa menos intensivo en CPU, pero ao prezo dun retardo de entrada imprevisible."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Durante o hospedaxe, intente escoitar as conexións da Internet pública, utilizando UPnP ou tecnoloxías similares para escapar das LAN."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Compartir entrada dixital"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Solicitar o dispositivo %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Solicita xogar co dispositivo de entrada indicado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Comandos de rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Porto de comando de rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "RetroPad de rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Porto base de RetroPad de rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Usuario %d Network RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "Comandos stdin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "interface de comando stdin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Descargas de miniaturas baixo demanda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Descarga automaticamente as miniaturas que faltan mentres buscas listas de reprodución. Ten un grave impacto no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Configuración do actualizador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Acceda á configuración básica do actualizador"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL do directorio de actualización principal no buildbot de libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "URL de recursos de Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL para o directorio de actualización de activos no buildbot de libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Extrae automaticamente o arquivo descargado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Despois da descarga, extrae automaticamente os ficheiros contidos nos arquivos descargados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Mostrar núcleos experimentais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Inclúe núcleos \"experimentais\" na lista do Core Downloader. Estes son normalmente só para fins de desenvolvemento/proba, e non se recomendan para uso xeral."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Núcleos de copia de seguranza ao actualizar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Crea automaticamente unha copia de seguridade de todos os núcleos instalados ao realizar unha actualización en liña. Permite unha recuperación sinxela a un núcleo que funcione se unha actualización introduce unha regresión."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Tamaño do historial de copia de seguranza do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Especifique cantas copias de seguranza xeradas automaticamente quere manter para cada núcleo instalado. Cando se alcance este límite, ao crear unha nova copia de seguranza mediante unha actualización en liña, eliminarase a copia de seguranza máis antiga. As copias de seguranza manuais do núcleo non se ven afectadas por esta configuración."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Historial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Mantén unha lista de reprodución de xogos, imaxes, música e vídeos usados recentemente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Tamaño do historial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Limita o número de entradas na lista de reprodución recente para xogos, imaxes, música e vídeos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Tamaño favorito"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Limita o número de entradas na lista de reprodución \"Favoritos\". Unha vez alcanzado o límite, impediranse novas incorporacións ata que se eliminen as antigas. Establecer un valor de -1 permite entradas \"ilimitadas\".\nADVERTENCIA: Se reduce o valor, borraranse as entradas existentes!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Permitir cambiar o nome das entradas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Permitir que as entradas das listas de reprodución cambien de nome."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Permitir eliminar entradas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Permitir que se eliminen as entradas da lista de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Ordenar as listas de reprodución alfabeticamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Ordena as listas de reprodución de contido en orde alfabética, excluíndo as listas de reprodución \"Historial\", \"Imaxes\", \"Música\" e \"Vídeos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Garda listas de reprodución usando o formato antigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Escribe listas de reprodución utilizando un formato de texto plano depreciado. Cando está desactivada, as listas de reprodución están formateadas con JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Comprimir listas de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Arquiva os datos da lista de reprodución ao escribir no disco. Reduce o tamaño do ficheiro e os tempos de carga a costa dun aumento (insignificante) do uso da CPU. Pódese usar con listas de reprodución de formato antigo ou novo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Mostrar núcleos asociados nas listas de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Especifica cando etiquetar as entradas das listas de reprodución co núcleo asociado actualmente (se o hai).\nEsta configuración ignórase cando se activan as subetiquetas das listas de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Mostrar subetiquetas da lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Mostra información adicional para cada entrada da lista de reprodución, como a asociación central actual e o tempo de execución (se está dispoñible). Ten un impacto variable no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Mostrar iconas específicas de contido no Historial e Favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Mostra iconas específicas para cada entrada do historial e das listas de reprodución favoritas. Ten un impacto variable no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Núcleo:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Tempo de execución:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Último xogado:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "segundo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "segundos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "minuto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "minutos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "hora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "horas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "día"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "días"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "semana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "semanas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "mes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "meses"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "ano"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "anos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "hai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Mostrar o índice de entradas da lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Mostra os números de entrada ao ver as listas de reprodución. O formato de visualización depende do controlador de menú seleccionado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Tempo de execución da sub-etiqueta da lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Seleccione que tipo de rexistro de rexistro de execución quere mostrar nas subetiquetas das listas de reprodución.\nO rexistro de tempo de execución correspondente debe estar activado a través do menú de opcións \"Gardar\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Estilo de data e hora \"Última reprodución\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Establece o estilo da data e da hora que se mostra para a información da marca de tempo \"Última reprodución\". As opcións \"(AM/PM)\" terán un pequeno impacto no rendemento nalgunhas plataformas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Correspondencia de arquivo difuso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Cando busque entradas asociadas a ficheiros comprimidos nas listas de reprodución, coincida só co nome do ficheiro de arquivo en lugar de [nome do ficheiro]+[contido]. Activa esta opción para evitar entradas duplicadas do historial de contido ao cargar ficheiros comprimidos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Escanear sen Core Match"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Permitir que o contido sexa dixitalizado e engadido a unha lista de reprodución sen un núcleo instalado que o admita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "A exploración verifica CRC sobre posibles duplicados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "Ás veces, as ISO duplican series, especialmente con títulos de PSP/PSN. Confiar só no serial ás veces pode facer que o escáner poña contido no sistema incorrecto. Isto engade unha comprobación CRC, que ralentiza considerablemente a exploración, pero pode ser máis precisa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Xestionar listas de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Realiza tarefas de mantemento das listas de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Listas de reprodución portátiles"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Cando está activado, e tamén se selecciona o directorio \"Explorador de ficheiros\", o valor actual do parámetro \"Explorador de ficheiros\" gárdase na lista de reprodución. Cando a lista de reprodución se carga noutro sistema no que estea activada a mesma opción, o valor do parámetro \"Navegador de ficheiros\" compárase co valor da lista de reprodución; se é diferente, os camiños das entradas da lista de reprodución corrixiranse automaticamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "Use nomes de ficheiro para a correspondencia de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "Cando estea activado, atopará miniaturas polo nome do ficheiro da entrada, en lugar da súa etiqueta."
   )
   MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "Xestionar"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Núcleo predeterminado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Especifique o núcleo que se utilizará ao iniciar contido a través dunha entrada de lista de reprodución que non teña unha asociación de núcleo existente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Restablecer asociacións básicas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Elimina as asociacións principais existentes para todas as entradas das listas de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Modo de visualización de etiquetas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Cambia como se mostran as etiquetas de contido nesta lista de reprodución."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Determina como se ordenan as entradas nesta lista de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Limpar lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Valida as asociacións principais e elimina as entradas non válidas e duplicadas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Actualizar lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Engade novo contido e elimina entradas non válidas repetindo a última operación \"Escaneado manual\" utilizada para crear ou editar a lista de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Eliminar lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Eliminar a lista de reprodución do sistema de ficheiros."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Privacidade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Cambiar a configuración de privacidade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Contas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Xestiona as contas configuradas actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Nome de usuario"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Introduza aquí o seu nome de usuario. Usarase para sesións de netplay, entre outras cousas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Idioma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Establece o idioma da interface de usuario."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Localiza o menú e todas as mensaxes en pantalla segundo o idioma que seleccionaches aquí. Require un reinicio para que os cambios teñan efecto.\nMóstrase a integridade da tradución xunto a cada opción. No caso de que non se implemente un idioma para un elemento do menú, volvemos ao inglés."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Permitir cámara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Permitir que os núcleos accedan á cámara."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Discord Rich Presenza"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Permite que a aplicación Discord mostre datos sobre o contido reproducido.\nSó dispoñible co cliente de escritorio nativo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Permitir localización"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Permite que os núcleos accedan á túa localización."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Consigue logros en xogos clásicos. Para obter máis información, visite \"https://retroachievements.org\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Detalles de inicio de sesión para a túa conta de RetroAchievements. Visita retroachievements.org e rexístrate para obter unha conta gratuíta.\nDespois de rematar o rexistro, debes introducir o nome de usuario e o contrasinal en RetroArch."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Nome de usuario"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Introduza o nome de usuario da súa conta de RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Contrasinal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Introduce o contrasinal da túa conta de RetroAchievements. Lonxitude máxima: 255 caracteres."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "Chave de fluxo de YouTube"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Clave de transmisión de Twitch"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Chave de fluxo de Facebook Gamming"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Sistema/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "Neste directorio gárdanse BIOS, ROM de arranque e outros ficheiros específicos do sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Descargas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Os ficheiros descargados almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Activos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Os recursos de menú utilizados por RetroArch almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Fondos dinámicos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "As imaxes de fondo utilizadas no menú gárdanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Neste directorio gárdanse as imaxes do cadro, as capturas de pantalla e as miniaturas da pantalla de título."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Explorador de ficheiros"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Establecer o directorio de inicio para o explorador de ficheiros."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Ficheiro de configuración"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "O ficheiro de configuración predeterminado almacénase neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Núcleos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Os núcleos de Libretro almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Información do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Neste directorio gárdanse os ficheiros de información da aplicación/núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Bases de datos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "As bases de datos gárdanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Arquivo de trucos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Os ficheiros de trampas almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Filtros de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Os filtros de vídeo baseados na CPU almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Filtros de son"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Os filtros DSP de audio almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Sombreadores de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Os sombreadores de vídeo baseados na GPU almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Gravacións"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "As gravacións gárdanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Configuración de gravación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "As configuracións de gravación almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Superposicións"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "As superposicións almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Superposicións de teclado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "As superposicións de teclado gárdanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Disposicións de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Os deseños de vídeo almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Capturas de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "As capturas de pantalla almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Perfís de controlador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Neste directorio gárdanse os perfís de controladores utilizados para configurar automaticamente os controladores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Remapas de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Neste directorio gárdanse as reasignacións de entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Listaxes de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "As listas de reprodución gárdanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Lista de reprodución de favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Garda a lista de reprodución Favoritos neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Lista de reprodución do historial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Garda a lista de reprodución do historial neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Lista de reprodución de imaxes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Garda a lista de reprodución do Historial de imaxes neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Lista de reprodución de música"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Garda a lista de reprodución de música neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Lista de reprodución de vídeos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Garda a lista de reprodución de vídeos neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Rexistros de execución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Os rexistros de execución almacénanse neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Gardar ficheiros"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Garda todos os ficheiros gardados neste directorio. Se non se define, tentarase gardar dentro do directorio de traballo do ficheiro de contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Garda todos os ficheiros gardados (*.srm) neste directorio. Isto inclúe ficheiros relacionados como .rt, .psrm, etc... Isto será substituído por opcións explícitas da liña de comandos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Salvar Estados"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Os estados de gardar e as repeticións almacénanse neste directorio. Se non se define, tentará gardalos no directorio onde se atopa o contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Caché"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "O contido arquivado extrairase temporalmente neste directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Rexistros de eventos do sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Os rexistros de eventos do sistema almacénanse neste directorio."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Activa a presenza enriquecida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Comparte o teu estado actual en RetroArch en Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Formato de contido enriquecido Presente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Decida que información relacionada co contido en execución se compartirá."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Contido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Nome do núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Nome do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Contido (nome do sistema)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Contido (nome do núcleo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Contido (Nome do sistema - Nome do núcleo)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Engadir ao Mesturador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Engade esta pista de audio a un espazo de emisión de audio dispoñible.\nSe non hai espazos dispoñibles actualmente, ignorarase."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Engadir ao Mesturador e xogar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Engade esta pista de audio a un espazo de emisión de audio dispoñible e reproduceo.\nSe non hai espazos dispoñibles neste momento, ignorarase."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Anfitrión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Conéctate ao Netplay do Anfitrión"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Introduza o enderezo do servidor de netplay e conéctese no modo cliente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Desconectar de Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Desconecta unha conexión de netplay activa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Filtros do vestíbulo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Só cuartos conectables"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Só núcleos instalados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Salas con contrasinal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Actualizar a lista de hosts de Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Actualizar a lista de hosts de Netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Actualizar a lista LAN de Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Busca hosts de netplay na LAN."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Inicia Netplay do Anfitrión"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Inicia a reprodución en rede no modo Anfitrión (servidor)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Detén Netplay do Anfitrión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "Cliente de patada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Expulsa a un cliente da túa habitación aloxada actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "Cliente Prohibido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Prohibe a un cliente da túa sala aloxada actualmente."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Escanear directorio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Analiza un directorio en busca de contido que coincida coa base de datos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Escanear este directorio>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "Seleccione isto para buscar contido no directorio actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Buscar ficheiro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Analiza un ficheiro para buscar contido que coincida coa base de datos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Busqueda manual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Exploración configurable en función dos nomes dos ficheiros de contido. Non require contido para coincidir coa base de datos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "Buscar"
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Engadir ao Mesturador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Engadir ao Mesturador e xogar"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Directorio de contidos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Seleccione un directorio para buscar contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Nome do sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Especifique un \"nome do sistema\" co que asociar o contido dixitalizado. Utilízase para nomear o ficheiro de lista de reprodución xerado e para identificar as miniaturas das listas de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Nome do sistema personalizado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Especifique manualmente un \"nome do sistema\" para o contido dixitalizado. Só se usa cando \"Nome do sistema\" está definido como \"<Personalizado>\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Núcleo predeterminado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Seleccione un núcleo predeterminado para usar ao iniciar o contido dixitalizado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Extensións de ficheiro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Lista de tipos de ficheiros para incluír na exploración, separados por espazos. Se está baleiro, inclúe todos os tipos de ficheiros ou, se se especifica un núcleo, todos os ficheiros admitidos polo núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Escanear recursivamente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Cando estea activado, todos os subdirectorios do 'Directorio de contido' especificado incluiranse na exploración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Escanear dentro dos arquivos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Cando estea activado, buscarase contido válido/compatible nos ficheiros de arquivo (.zip, .7z, etc.). Pode ter un impacto significativo no rendemento da exploración."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Arcade Arquivo DAT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Seleccione un ficheiro Logiqx ou MAME List XML DAT para activar a denominación automática do contido arcade dixitalizado (MAME, FinalBurn Neo, etc.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Filtro DAT Arcade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Cando se utiliza un ficheiro DAT arcade, só se engadirá contido á lista de reprodución se se atopa unha entrada de ficheiro DAT correspondente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Sobrescribir a lista de reprodución existente"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Cando estea activado, eliminarase calquera lista de reprodución existente antes de escanear o contido. Cando se desactiva, consérvanse as entradas das listas de reprodución existentes e só se engadirá o contido que falte na lista de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Validar entradas existentes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Cando estea activado, verificaranse as entradas de calquera lista de reprodución existente antes de escanear contido novo. Eliminaranse as entradas que se refiran a contido que falta e/ou ficheiros con extensións non válidas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Iniciar análise"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Escanear o contido seleccionado."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Iniciando lista..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Ano de lanzamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Número de xogadores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Rexión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Etiqueta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Buscar nome..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Amosar todo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Filtros adicionáis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Todos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Filtros adicionáis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u elementos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Desenvolvedor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Editor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Ano de lanzamento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Por número de xogadores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Por Xénero"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Logros"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Por categoría"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Lingua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Rexión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Exclusivo de Consola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Exclusivo de Plataforma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Rumble"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Por puntuación"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Por Media"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Controis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Estilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Mecánicas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Por Narrativa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "Ao ritmo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Perspectiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Axustes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Por Visual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Vehicular"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Por Orixe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Por franquía"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Por etiqueta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Nome do sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Establecer filtro de intervalo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Ver"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Gardar como Vista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Elimina esta vista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Introduza o nome da nova vista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "A vista xa existe co mesmo nome"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "A vista gardouse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "Eliminouse a vista"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Executar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Inicia o contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Renomear"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Renomear o título da entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Borrar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Elimina esta entrada da lista de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Engadir a favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Engade o contido a \"Favoritos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Establecer Asociación Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Establece o núcleo asociado a este contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Resstablecer Asociación Núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Restablece o núcleo asociado a este contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Información"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Ver máis información sobre o contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Descargar miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Descarga imaxes en miniatura de captura de pantalla/arte da caixa/pantalla de título para o contido actual. Actualiza as miniaturas existentes."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Núcleo actual"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Nome"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Ruta do ficheiro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Núcleo"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Reproducir a Repetición"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Reproduce o ficheiro gardado dende a ranura escollida actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Reproducir a gravación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Reproduce o ficheiro gardado dende a ranura escollida actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Deter a gravación/reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Engadir a favoritos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Engade o contido a \"Favoritos\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Salvar Estados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Logros"
   )

/* Quick Menu > Options */


/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Gardar opcións de xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Garda as opcións principais que só se aplicarán ao contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Eliminar Opcións de xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Elimina as opcións principais que só se aplicarán ao contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Gardar opcións do directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Garda as opcións fundamentais que se aplicarán a todo o contido cargado desde o mesmo directorio que o ficheiro actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Eliminar opcións do directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Elimina as opcións principais que se aplicarán a todo o contido cargado desde o mesmo directorio que o ficheiro actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Ficheiro de opcións activos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "O ficheiro de opcións actual en uso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Opcións de restablecemento"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Establece todas as opcións básicas cos valores predeterminados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Borrar as opcións no disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Forzar que a configuración actual se escriba no ficheiro de opcións activo. Asegura que as opcións se conserven no caso de que un erro principal cause un peche incorrecto da interface."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Crear ficheiro de opcións de xogo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Gardar o ficheiro de opcións de xogo"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Xestionar ficheiros de reasignación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Cargar, gardar ou eliminar ficheiros de remap de entrada para o contido actual."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Ficheiro de reasignación activo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "O ficheiro de reasignación actual en uso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Cargar ficheiro de reasignación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Cargar e substituír as asignacións de entrada actuais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Gardar o ficheiro de reasignación como"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Garda as asignacións de entrada actuais como un novo ficheiro de reasignación."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Gardar o ficheiro Core Remap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Garda un ficheiro de reasignación que se aplicará a todo o contido cargado con este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Eliminar o ficheiro Core Remap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Elimina o ficheiro de reasignación que se aplicará a todo o contido cargado con este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Gardar o ficheiro de reasignación do directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Garda un ficheiro de reasignación que se aplicará a todo o contido cargado desde o mesmo directorio que o ficheiro actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Eliminar o ficheiro de reasignación do directorio de contido do xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Elimina o ficheiro de reasignación que se aplicará a todo o contido cargado desde o mesmo directorio que o ficheiro actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Gardar o ficheiro de reasignación do xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "Garda un ficheiro de reasignación que só se aplicará ao contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Eliminar o ficheiro de reasignación do xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "Elimina o ficheiro de reasignación que só se aplicará ao contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Restablecer a asignación de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Establece todas as opcións de reasignación de entrada aos valores predeterminados."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Actualizar o ficheiro de reasignación de entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Sobrescribe o ficheiro de remapeamento activo coas opcións de reasignación de entrada actuais."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Remapear ficheiro"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Inicia ou continúa a busca de trampas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT,
   "Analiza a memoria para crear novos trucos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Cargar ficheiro trucos (substituír)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Cargue un ficheiro de trampas e substitúa os trucos existentes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Cargar ficheiro cheat (anexar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Cargue un ficheiro de trampas e engádeo ós trucos existentes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Recargar trucos específicos do xogo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Gardar o ficheiro de trampas como"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Garda os trucos actuais como un ficheiro de trucos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Engade un novo truco na parte superior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Engade un novo truco ao fondo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Eliminar todos os trucos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Aplicar trucos automaticamente durante a carga do xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Aplica trucos automaticamente cando se carga o xogo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Aplicar despois de alternar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Aplique trucos inmediatamente despois de alternar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Aplicar os cambios"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Os cambios de trucos terán efecto inmediatamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Trucos"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Inicia ou reinicia a busca de trucos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Prema Esquerda ou Dereita para cambiar o tamaño de bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Preme á esquerda ou á dereita para cambiar o valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Igual a %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Menos que antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Menor ou igual que antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Maior que antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Maior ou igual que antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Igual a Antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Non igual a Antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Preme á esquerda ou á dereita para cambiar o valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Igual a Antes de +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Busca valores en memoria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Preme á esquerda ou á dereita para cambiar o valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Igual a Antes de -%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Engade as %u coincidencias á túa lista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Crear coincidencia de código #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Enderezo de coincidencia: %08X Máscara: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Ficheiro de trucos (substituír)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Ficheiro de trucos (anexar)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Detalles de trucos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Índice"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Posición de trucos na lista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Activado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Descrición"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Manexador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Tamaño da busca de memoria"
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
   "Enderezo de memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Buscar o enderezo: %08X"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Endereza a máscara de bits cando o tamaño da busca de memoria < 8 bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Número de iteracións"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "O número de veces que se aplicará o truco. Use coas outras dúas opcións de \"Iteración\" para afectar a grandes áreas de memoria."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Aumenta o enderezo en cada iteración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Despois de cada iteración, o \"Enderezo da memoria\" aumentarase por este número veces o \"Tamaño da busca de memoria\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Aumento de valor en cada iteración"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Despois de cada iteración, o \"Valor\" aumentarase nesta cantidade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Rumble Cando Memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Código"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Engade un novo truco despois disto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Engade un novo truco antes disto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Copia este truco despois"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Copia este truco antes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Eliminar este truco"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Expulsar Disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Abre a bandexa do disco virtual e elimina o disco cargado actualmente. Se se activa \"Pausa o contido cando o menú está activo\", é posible que algúns núcleos non rexistren os cambios a menos que o contido se retome durante uns segundos despois de cada acción de control do disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Inserir disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Insira o disco correspondente ao 'Índice de disco actual' e pecha a bandexa do disco virtual. Se se activa \"Pausa o contido cando o menú está activo\", é posible que algúns núcleos non rexistren os cambios a menos que o contido se retome durante uns segundos despois de cada acción de control do disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Cargar disco novo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Expulsa o disco actual, selecciona un novo disco do sistema de ficheiros, despois insértao e pecha a bandexa do disco virtual.\nNOTA: esta é unha función antiga. En cambio, recoméndase cargar títulos de varios discos a través de listas de reprodución M3U, que permiten a selección de discos mediante as opcións \"Expulsar/Inserir disco\" e \"Índice de disco actual\"."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Seleccione un novo disco do sistema de ficheiros e insírelo sen pechar a bandexa do disco virtual.\nNOTA: Esta é unha función antiga. En cambio, recoméndase cargar títulos de varios discos a través de listas de reprodución M3U, que permiten a selección de discos mediante a opción \"Índice de disco actual\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Índice actual do disco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Escolla o disco actual da lista de imaxes dispoñibles. O disco cargarase cando se seleccione \"Inserir disco\"."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Sombreadores de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Activa a canalización do sombreador de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Consulta os ficheiros de Shader para ver os cambios"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Aplicar automaticamente os cambios feitos aos ficheiros de sombreadores no disco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Observa os ficheiros do sombreador para ver novos cambios. Despois de gardar os cambios nun sombreador no disco, recompilarase automaticamente e aplicarase ao contido en execución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Lembra o último directorio de Sombreadores usado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Abre o Explorador de ficheiros no último directorio utilizado ao cargar as opcións predeterminadas de sombreadores e pases."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Cargar preaxuste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Cargar un preset de sombreado. A canalización do sombreado configurarase automaticamente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Carga directamente un preset de sombreado. O menú do sombreador actualízase en consecuencia.\nO factor de escala que se amosa no menú só é fiable se a configuración predeterminada usa métodos de escala sinxelos (é dicir, escala da fonte, mesmo factor de escala para X/Y)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Antepoñer predefinido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Anteponer o predefenido ao predefinido cargado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Antepoñer predefinido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Engadir preselección ao predefinido cargado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
   "Gardar preaxuste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "Garda a configuración predeterminada do sombreador actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
   "Eliminar predefinido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "Eliminar un predefinido de sombreado automático."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Aplicar os cambios"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Os cambios na configuración do sombreado terán efecto inmediatamente. Use isto se cambiou a cantidade de pases de sombreado, filtrado, escala FBO, etc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "Despois de cambiar a configuración do sombreador, como a cantidade de pases de sombreado, o filtrado ou a escala de FBO, úsao para aplicar cambios.\nCambiar esta configuración de sombreado é unha operación algo cara polo que hai que facelo de forma explícita.\nCando aplicas sombreadores, a configuración do sombreado é gardouse nun ficheiro temporal (retroarch.slangp/.cgp/.glslp) e cargouse. O ficheiro persiste despois de saír de RetroArch e gárdase no directorio Shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Parámetros do sombreador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Modifica directamente o sombreador actual. Os cambios non se gardarán no ficheiro predefinido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Pases de Sombreadores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Aumenta ou diminúe a cantidade de pases de pipeline shader. Pódense unir sombreadores separados a cada paso de canalización e configurar a súa escala e filtrado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch permite mesturar e combinar varios sombreadores con pases de sombreado arbitrarios, con filtros de hardware personalizados e factores de escala.\nEsta opción especifica o número de pases de sombreado a usar. Se estableces isto en 0 e usas Aplicar cambios de sombreado, usas un sombreador \"en branco\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "Sombreador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "Camiño ao sombreador. Todos os sombreadores deben ser do mesmo tipo (por exemplo, Cg, GLSL ou Slang). Establece o Directorio de sombreadores para definir onde comeza o navegador a buscar os sombreadores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Filtro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "Filtro de hardware para este pase. Se se define como \"Predeterminado\", o filtro será \"Lineal\" ou \"Máis próximo\", dependendo da configuración de \"Filtrado bilineal\" en Configuración de vídeo."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Escala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "Escala para este pase. O factor de escala acumúlase, é dicir, 2x para o primeiro pase e 2x para o segundo pase, dará unha escala total de 4x.\nSe hai un factor de escala para o último pase, o resultado amplíase á pantalla co filtro predeterminado, dependendo da configuración de Filtrado bilineal. en Configuración de vídeo.\nSe se define como \"Predeterminado\", utilizarase a escala 1x ou a extensión a pantalla completa, dependendo de se non é a última pasada ou non."
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Presets simples"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Garda un preset de sombreado que ten unha ligazón ao preset orixinal cargado e inclúe só os cambios de parámetro que fixeches."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Gardar sombreado predefinido como"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Garda a configuración actual do sombreador como un novo sombreador predefinido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Gardar predefinido global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Garda a configuración actual do sombreador como a configuración global predeterminada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Gardar Núcleo predefinido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Garda a configuración actual do sombreador como predeterminada para este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Gardar o predefinido do directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Garda a configuración actual do sombreador como predeterminada para todos os ficheiros do directorio de contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Gardar xogo predefinido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Garda a configuración actual do sombreador como a configuración predeterminada para o contido."
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Non se atoparon presets de sombreadores automáticos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Eliminar predefinido global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Elimina o predefinido global, usado por todo o contido e todos os núcleos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Eliminar Núcleo predefinido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Elimina a configuración predeterminada do núcleo, utilizada por todo o contido executado co núcleo cargado actualmente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Eliminar o predefinido do directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Elimina a configuración predeterminada do directorio de contido, utilizada por todo o contido do directorio de traballo actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Eliminar xogo predefinido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Elimina o xogo predefinido, usado só para o xogo específico en cuestión."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Sen parámetros de sombreado"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "Ficheiro de anulación activo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "O ficheiro de substitución actual en uso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "Cargar o ficheiro de substitución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "Cargar e substituír a configuración actual."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "Garda a configuración actual como un novo ficheiro de substitución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Gardar substitucións básicas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Garda un ficheiro de configuración de substitución que se aplicará a todo o contido cargado con este núcleo. Terá prioridade sobre a configuración principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Eliminar as anulacións do núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Elimina o ficheiro de configuración de anulación que se aplicará a todo o contido cargado con este núcleo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Gardar substitucións do directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Garda un ficheiro de configuración de substitución que se aplicará a todo o contido cargado desde o mesmo directorio que o ficheiro actual. Terá prioridade sobre a configuración principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Eliminar substitucións do directorio de contido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Elimina o ficheiro de configuración de substitución que se aplicará a todo o contido cargado desde o mesmo directorio que o ficheiro actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Gardar substitucións do xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Garda un ficheiro de configuración de substitución que só se aplicará ao contido actual. Terá prioridade sobre a configuración principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Eliminar as anulacións do xogo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Elimina o ficheiro de configuración de substitución que só se aplicará ao contido actual."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Erro de rede"
)

/* Quick Menu > Information */


/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "Dacordo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Directorio principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "Volve ao directorio principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Directorio non atopado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Sen elementos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Seleccionar ficheiro"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Compilador descoñecido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Compartido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Agarre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Votar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Compartir entrada analóxica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Máxima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Media"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Ningún"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Sen preferencia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Rebote á esquerda/dereita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Desprazar cara á esquerda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Modo de imaxe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Modo de fala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Modo Narrador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Historia e Favoritos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Todas as listas de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Historia e Favoritos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Sempre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Nunca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Por Núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Engadido"
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
   "Non hai fontes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Usar este directorio>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "Seleccione isto para configurar este como o directorio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Directorio de contido>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Predeterminado>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Ningún>"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
   "Teclado retro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD,
   "Pad retro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad con analóxico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Ningún"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Descoñecido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Abaixo + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Mantén presionado Inicio (2 segundos)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Mantén pulsado Seleccionar (2 segundos)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Abaixo + Seleccionar"
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
   "Non cambia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Aumentar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Reducir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= Valor de estrondo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= Valor de Estrondo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< Valor de Estrondo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> Valor de Estrondo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Aumenta por Valor de Estrondo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Diminúe por Valor de Estrondo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Todo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Desactivado>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Establecer en Valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Aumentar por valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Diminuír por valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Executa o seguinte truco se valor = memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Executar seguinte Cheat If Value != Memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Executa o seguinte truco se o valor < Memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Executar seguinte Cheat If Value > Memoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Emulador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1 bit, valor máximo = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2 bits, valor máximo = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Alfabeticamente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Ningún"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Mostrar etiquetas completas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Eliminar () contido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Eliminar [] Contido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Eliminar () e []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Manter a rexión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Manter o índice do disco"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Manteña a rexión e o índice do disco"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Sistema por defecto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Titulo en Pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Rápido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "ACENDER"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Sí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Non"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Verdadeiro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Falso"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Activado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Desactivado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Bloqueado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Desbloqueado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
   "Extremo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Non oficial"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Non compatible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Desbloqueado recentemente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Case alí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Retos Activos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Só rastrexadores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Só notificacións"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Por defecto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Lineal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Contido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Directorio de contido>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Personalizado>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Non especificado>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Esquerda Analóxica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Analóxico dereito"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Analóxico esquerdo (forzado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Analóxico dereito (forzado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "Chave %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Rato 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Rato 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Rato 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Rato 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Rato 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Roda do rato cara arriba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Roda do rato cara abaixo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Roda do rato á esquerda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Roda do rato á dereita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Cedo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Tarde"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Relación de aspecto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Tema predefinido personalizado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Seleccione un tema de menú predefinido no Explorador de ficheiros."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Transparencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Activa a visualización en segundo plano do contido en execución mentres o menú rápido está activo. A desactivación da transparencia pode alterar as cores do tema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Efectos de sombra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Activa as sombras para o texto do menú, os bordos e as miniaturas. Ten un impacto modesto no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Animación de fondo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Activa o efecto de animación de partículas de fondo. Ten un impacto significativo no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Velocidade de animación de fondo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Axuste a velocidade dos efectos de animación de partículas de fondo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Animación de fondo do salvapantallas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Mostra o efecto de animación de partículas de fondo mentres o protector de pantalla do menú está activo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Mostrar miniaturas da lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Activa a visualización de miniaturas en liña reducidas mentres visualizas listas de reprodución. Conmutable con RetroPad Select. Cando está desactivada, as miniaturas aínda se poden cambiar a pantalla completa con RetroPad Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Miniatura superior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Tipo de miniatura para mostrar na parte superior dereita das listas de reprodución. Este tipo de miniaturas pódese cambiar premendo o RetroPad Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Miniatura inferior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Tipo de miniatura para mostrar na parte inferior dereita das listas de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Intercambiar miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Cambia as posicións de visualización de \"Miniatura superior\" e \"Miniatura inferior\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Método de redución de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Método de remuestreo que se usa ao reducir as miniaturas grandes para que se axusten á pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Retraso de miniatura (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Aplica un atraso de tempo entre a selección dunha entrada da lista de reprodución e a carga das súas miniaturas asociadas. Establecer este valor nun valor de polo menos 256 ms permite un desprazamento rápido e sen atrasos incluso nos dispositivos máis lentos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Compatibilidade con ASCII estendido"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Activa a visualización de caracteres ASCII non estándar. Necesario para a compatibilidade con certas linguas occidentais non inglesas. Ten un impacto moderado no rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Iconas de cambio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Use iconas en lugar de texto ON/OFF para representar as entradas de configuración do menú \"interruptor de alternancia\"."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Veciño máis próximo (rápido)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (Lento)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Ningún"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (Centrado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (Centrado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (Centrado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (Centrado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (Centrado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Axustar Pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Escala enteira"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Pantalla de recheo (estirada)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Vermello clásico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Laranxa clásico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Amarelo clásico"
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
   "Violeta clásica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Gris clásico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Vermello Legado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Morado Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Azul medianoite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Dourado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Azul eléctrico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Apple Verde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vermello Volcánico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Lagoa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER,
   "Brogramas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "Seda de fadas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI,
   "Plana UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Caixa de minería Escura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Caixa de Minería Clara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackeando o núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD,
   "Nórdico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
   "Ozono Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT,
   "Noite pálida"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Escuridade solarizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Escuridade Clara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Escuridade Tango"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLUX,
   "Fluxo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dinámico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Gris Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Gris Claro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Neve lixeira"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Neve (pesado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Choiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Campo de estrelas"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Miniatura secundaria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Tipo de miniatura para mostrar á esquerda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Fondo dinámico"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Carga dinámicamente un novo fondo de pantalla dependendo do contexto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Animación horizontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Activa a animación horizontal para o menú. Isto terá un éxito de performance."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Destacado da icona horizontal de animación"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "A animación que se activa ao desprazarse entre as pestanas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animación Mover arriba/abaixo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "A animación que se activa ao subir ou baixar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Cor da fonte (verde)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Cor da fonte (azul)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Deseño"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Seleccione un deseño diferente para a interface XMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Tema de iconas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Seleccione un tema de icona diferente para RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Iconas de cambio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "Use iconas en lugar de texto ON/OFF para representar as entradas de configuración do menú \"interruptor de alternancia\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Efectos de sombra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Debuxa sombras para iconas, miniaturas e letras. Isto terá un éxito de actuación menor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Canalización de sombreadores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Seleccione un efecto de fondo animado. Pode ser intensivo en GPU dependendo do efecto. Se o rendemento non é satisfactorio, desactívao ou volve a un efecto máis sinxelo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Cor do tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Selecciona un tema de cor de fondo diferente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Disposición vertical de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Mostra a miniatura esquerda debaixo da dereita, no lado dereito da pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Factor de escala de miniaturas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Reduce o tamaño da visualización das miniaturas escalando o ancho máximo permitido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Factor de desvanecemento vertical"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Mostrar cabeceira do título"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Marxe do título"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Desfase horizontal da marxe do título"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Activar a pestana Configuración (requírese reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Mostra a pestana Configuración que contén a configuración do programa."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Fita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Fita (simplificado)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Neve sinxela"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Neve"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Folerpa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Personalizado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
   "UI plana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUX,
   "UX Plana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Monocromo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Monocromo invertido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Sistemático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
   "Píxel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
   "Retrosistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
   "Punto-Art"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Automático"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Automático invertido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DAITE,
   "De cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Apple Verde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Luz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Mañana azul"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Raio de sol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Morado Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Azul eléctrico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Dourado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Vermello Legado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Azul medianoite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Imaxe de fondo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Baixo o Mar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vermello Volcánico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Verde Lima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Pikachu Amarelo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Cubo Morado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Familia Vermella"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Quente ardente"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_NORD,
   "Nórdico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Caixa de minería Escura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackeando o núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Escuridade solarizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Escuridade Clara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Miniatura secundaria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Factor de escala de miniaturas"
   )

/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Iconas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Iconas de cambio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "Use iconas en lugar de texto ON/OFF para representar as entradas de configuración do menú \"interruptor de alternancia\"."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Selecciona un tema de cor de fondo diferente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Miniatura secundaria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Tipo auxiliar de miniatura para asociar a cada entrada da lista de reprodución. O uso depende do modo de visualización de miniaturas da lista de reprodución actual."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Azul"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Azul Gris"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Azul escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Verde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "Escudo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Vermello"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Amarelo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI,
   "IU material"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Material UI Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozono Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD,
   "Nórdico"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Caixa de minería Escura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Escuridade solarizada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Azul Lindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Cian Lindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Verde Lindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Laranxa Lindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Rosa Lindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Morado Lindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Vermello Lindo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY,
   "Neno Virtual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackeando o núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Gris Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Luz Gris"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Esvaecido"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Diapositivas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Lista (pequena)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Lista (Medio)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Icona dual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Lista (pequena)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Lista (Medio)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Lista (Longa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Escritorio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "Apagado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "ACENDER"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Excluír vistas en miniatura"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Información"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Ficheiro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Cargar un núcleo..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Descargar un Núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "S&air"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Editar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Atopar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Ver"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Peiraos pechados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Parámetros do sombreador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Configuracións..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Lembra as posicións do peirao:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Lembra a xeometría da xanela:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Lembra a última pestana do navegador de contido:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Tema:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Sistema por defecto>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Escuro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Personalizado..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Configuración"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Ferramentas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Axuda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "Sobre RetroArch"
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
   "Cargar un núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Cargando núcleo..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Nome"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Versión"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Listaxes de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Explorador de ficheiros"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Superior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Subir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Navegador de contidos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Titulo en Pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Todas as listas de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Información do Núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Pregúntame>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Información"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Aviso"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Erro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Erro de rede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Reinicie o programa para que os cambios teñan efecto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Rexistro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 elementos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Deixa a imaxe aquí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Non amosar isto de novo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Parar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Núcleo asociado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Listas de reproducción agochadas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Agochar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Cor de realce:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Escoller..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Seleccionar cor"
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
   "O camiño do ficheiro está en branco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "O ficheiro está baleiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Non se puido abrir o ficheiro para ler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Non se puido abrir o ficheiro para escribir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "O ficheiro non existe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Suxire primeiro o núcleo cargado:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Vista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Iconas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Listaxe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Limpar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Progreso:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "Entradas máximas da lista \"Todas as listas de reprodución\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "Entradas máximas da grella \"Todas as listas de reprodución\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Mostrar ficheiros e cartafoles ocultos:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Nova lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Introduce o nome da nova lista de reprodución:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Eliminar lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Cambiar o nome da lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Está seguro que quere eliminala lista de reprodución \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Pregunta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Non se puido eliminar o ficheiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Non se puido cambiar o nome do ficheiro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Recopilando lista de ficheiros..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Engadindo ficheiros á lista de reprodución..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Entrada na lista de reprodución"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Nome:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Núcleo:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Borrar"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Contadores de núcleos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Non se seleccionou ningún disco"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Contadores frontend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Menú horizontal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Ocultar descriptores de entrada de núcleo sen vincular"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Mostrar etiquetas de descritores de entrada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Superposición en pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Historial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Selecciona contido da lista de reprodución do historial recente."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Cando se carga o contido, as combinacións de contido e o núcleo de libretro gárdanse no historial.\nO historial gárdase nun ficheiro no mesmo directorio que o ficheiro de configuración de RetroArch. Se non se cargou ningún ficheiro de configuración no inicio, o historial non se gardará nin se cargará e non existirá no menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Subsistemas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Acceda á configuración do subsistema para o contido actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Contido actual: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Non se atoparon Anfitrións de netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Non se atoparon clientes de netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Sen contadores de rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Sen listas de reprodución."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Conectado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "En liña"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "Porto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Porto %d Nome do dispositivo: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Nome de visualización do dispositivo: %s\nNome da configuración do dispositivo: %s\nVID/PID do dispositivo: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Detalles de trucos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Inicia ou continúa a busca de trampas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Xogar en Media Player"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "segundos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Iniciar núcleo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Inicia o núcleo sen contido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Núcleos suxeridos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Non se puido ler o ficheiro comprimido."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Usuario"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Use o visor de imaxes integrado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Max Swapchain Imaxes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Dille ao controlador de vídeo que use de forma explícita un modo de almacenamento en búfer especificado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Cantidade máxima de imaxes de intercambio. Isto pode indicarlle ao controlador de vídeo que utilice un modo de almacenamento en búfer de vídeo específico.\nBuffer único - 1\nDobre almacenamento en búfer - 2\nTriple almacenamento en búfer - 3\nEstablecer o modo de almacenamento en búfer correcto pode ter un gran impacto na latencia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "Cadeas de intercambio esperables"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Sincroniza a CPU e a GPU. Reduce a latencia ao custo do rendemento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Latencia máxima de fotogramas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Dille ao controlador de vídeo que use de forma explícita un modo de almacenamento en búfer especificado."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Modifica a configuración predeterminada do sombreador que se usa actualmente no menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Preaxuste do sombreado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Preaxuste do sombreado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "Preaxuste do sombreado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Explorar URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "URL Ruta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Iniciar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Alcume: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Buscando contido compatible..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "Non se atopou ningún núcleo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "Non se atoparon playlists"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Atopouse contido compatible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "Produciuse un erro ao localizar o contido coincidente por CRC ou polo nome de ficheiro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Comeza Gong"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "Relación de aspecto automática"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Compatibilidade con gravación"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Xogo en liña"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Axuda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "Limpar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "Enviar información de depuración"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Descrición"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Detalles de trucos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Inicia ou continúa a busca de trampas"
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Inicia ou continúa a busca de trampas"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Descargas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Compatibilidade con Slang"
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
   "Saír"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Desprazar cara a arriba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Valores predeterminados"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Activa o teclado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Activa o menú"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "No menú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "No xogo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "No xogo (en pausa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "En xogo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "En pausa"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netplay comezará cando se cargue o contido."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "O contido debe cargarse antes de iniciar a netplay."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Non se puido atopar un ficheiro básico ou de contido adecuado, cárgueo manualmente."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "O teu controlador de gráficos non é compatible co controlador de vídeo actual en RetroArch, volvendo ao controlador %s. Reinicie RetroArch para que os cambios teñan efecto."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Instalación do núcleo exitosa"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Fallou a instalación do núcleo"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Preme á dereita cinco veces para eliminar todos os trucos."
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "Produciuse un erro ao gardar a información de depuración."
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "En xogo"
   )

MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "O núcleo non é compatible co gardado rápido."
   )
MSG_HASH(
   MSG_ERROR,
   "Erro"
   )
MSG_HASH(
   MSG_SHADER,
   "Sombreador"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Descoñecido"
   )

/* Lakka */


/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Reiniciar RetroArch"
   )
#ifdef HAVE_LIBNX
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Compartir cartafoles na rede empregando o protocolo SMB."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Empregar SSH para acceder á liña de comandos de forma remota."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Punto de acceso wifi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Activar ou desactivar o punto de acceso wifi."
   )
#ifdef HAVE_LAKKA_SWITCH
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Apagando o punto de acceso wifi."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Desconectando da wifi \"%s\""
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
#endif
#ifdef UDEV_TOUCH_SUPPORT
#endif
#ifdef HAVE_ODROIDGO2
#else
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "Pantalla inferior da 3DS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (Alta definición)"
   )
#endif
#ifdef HAVE_QT
#endif

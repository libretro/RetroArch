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
   "Configuració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favorits"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Historial"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Imatges"
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
   "Joc en línia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Explora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Nuclis sense continguts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importa contingut"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Menú ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Accedeix ràpidament a totes les opcions relacionades amb la partida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Carrega un nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Selecciona quin nucli utilitzar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "Tanca el nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Allibera el nucli carregat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Cerca una implementació d'un nucli de libretro. La ubicació d'inici del navegador depèn de la ruta del Directori de Nuclis. En cas de trobar-se en blanc, començarà al directori arrel.\nSi el Directori de Nuclis és un directori, el menú l'usarà com a carpeta inicial. Altrament, si és una ruta completa, el navegador s'iniciarà a la carpeta on es trobi el fitxer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Carrega contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Selecciona quin contingut iniciar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Cerca continguts. Per a carregar-ne, necessites un «nucli» i un fitxer de contingut.\nPer a configurar a on comença el menú a cercar els continguts, cal definir el «Directori de navegació de fitxers». Si no està configurat, aquest començarà a l'arrel.\nEl navegador filtrarà extensions per a l'últim nucli triat en «Carrega un nucli», i emprarà aquell nucli quan el contingut es carregui."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Carrega un disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Carrega un disc físic multimèdia. Primer cal seleccionar el nucli (Carrega un nucli) a utilitzar amb el disc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Aboca un disc"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Aboca un disc físic multimèdia a l'emmagatzematge intern. Es desarà com un fitxer d'imatge."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Expulsa el disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Expulsa el disc de la unitat física de CD/DVD."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "El contingut que coincideixi amb la base de dades apareixerà aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Importa contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Crea i actualitza les llistes de reproducció escanejant contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Mostra el menú d'escriptori"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Obre el menú tradicional d'escriptori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Deshabilita el mode Quiosc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Mostra totes les opcions de configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Actualitzador en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Baixa complements, components i contingut per al RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Joc en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Organitza o uneix-te a una sessió de joc en línia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Configuració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Configura el programa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Mostra la informació del sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Fitxer de configuració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Gestiona i crea fitxers de configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Ajuda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Assabenta't del funcionament del programa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Reinicia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Reinicia el RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Tanca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Tanca el RetroArch. La configuració es desarà en sortir."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Tanca el RetroArch. La configuració no es desarà en sortir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Tanca el RetroArch. Matar el programa de manera forçada (amb SIGKILL, etc.) finalitzarà el RetroArch sense desar la configuració. En sistemes Unix, SIGINT/SIGTERM permet un tancament net, el que inclou que es desi la configuració si està activat."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Baixa un nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Baixa i instal·la un nucli des de l'actualitzador en línia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Instal·la o restaura un nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Instal·la o restaura un nucli des del directori «Baixades»."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Inicia el processador de vídeo"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Inicia un RetroPad remot"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Directori d'inici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Descàrregues"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Explora el fitxer comprimit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Carrega el fitxer comprimit"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favorits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "El contingut afegit a «Favorits» apareixerà aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Música"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "La música reproduïda anteriorment apareixerà aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Imatges"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Les imatges visualitzades anteriorment apareixeran aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Vídeos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Els vídeos reproduïts anteriorment apareixeran aquí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Explora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Navega pels continguts que coincideixin amb la base de dades mitjançant una interfície de cerca categoritzada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Nuclis sense continguts"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Aquí apareixeran els nuclis instal·lats que poden operar sense carregar contingut."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Baixador de nuclis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Actualitza els nuclis instal·lats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Actualitza tots els nuclis instal·lats a la darrera versió disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Canvia els nuclis a les versions de Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Substitueix tots els nuclis heretats i instal·lats manualment per les darreres versions de la Play Store, si hi són disponibles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Actualitzador de miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Baixa el paquet complet de miniatures per al sistema seleccionat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Actualitzador de miniatures de llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Baixa les miniatures de les entrades de la llista de reproducció seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Descarregador de contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Baixa contingut gratuït per al nucli seleccionat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Baixador de fitxers de sistema dels nuclis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Baixa fitxers del sistema auxiliars necessaris per a un funcionament correcte o òptim del nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Actualitza els fitxers d'informació del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Actualitza els recursos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Actualitza els perfils dels controladors"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Actualitza els trucs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Actualitza les bases de dades"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Actualitza les superposicions"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Actualitza els shaders GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Actualitza els shaders Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Actualitza els shaders Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Informació del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Mostra la informació relativa a l'aplicació/nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Informació del disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Mostra la informació dels discs multimèdia inserits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Informació de la xarxa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Mostra la interfície de xarxa i les adreces IP associades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Informació del sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Mostra la informació específica del dispositiu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Gestor de bases de dades"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Mostra les bases de dades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Gestor del cursor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Mostra les cerques anteriors."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Nom del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Etiqueta del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Versió del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Nom del sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Fabricant del sistema"
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
   "Llicència"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Extensions compatibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "API de gràfics requerides"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Ruta completa"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Compatibilitat amb el desament"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Cap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Bàsic (Desar/Carregar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Serialitzat (Desar/Carregar, Rebobinar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Determinista (Desar/Carregar, Rebobinar, Avançar, Joc en línia)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Microprogramari"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "- Nota: «Els fitxers del sistema són al directori del contingut» es troba activat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "- S'està mirant en: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Manca, requerit:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Manca, opcional:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Present, requerit:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Present, opcional:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Bloca el nucli instal·lat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Evita la modificació del nucli instal·lat actualment. Es podria utilitzar per a evitar actualitzacions no desitjades quan el contingut requereix una versió específica del nucli (p. ex., conjunts de ROM d'arcade) o les modificacions que faci el nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Exclou del menú «Nuclis sense continguts»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Evita que el nucli es mostri a la secció «Nuclis sense continguts». Només s'aplica quan el mode de visualització es troba configurat com a «Personalitzat»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Elimina el nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Elimina aquest nucli del disc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Còpia de seguretat del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Crea una còpia de seguretat del nucli instal·lat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Restaura la còpia de seguretat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Instal·la una versió anterior del nucli des d'una llista de còpies de seguretat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Elimina la còpia de seguretat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Elimina un fitxer de la llista de còpies de seguretat."
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Data de compilació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "Versió del RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Versió del Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Compilador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Model de CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Característiques de la CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Arquitectura de la CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Nuclis de la CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT disponible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUNDLE_IDENTIFIER,
   "Identificador del paquet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Identificador del processador d'accés"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Sistema operatiu del processador d'accés"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "Nivell del RetroRating"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Font d'alimentació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Controlador de context de vídeo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Amplada de la pantalla (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Alçada de la pantalla (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Densitat de píxels de la pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "Compatibilitat amb LibretroDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Compatibilitat amb superposicions"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Compatibilitat amb interfície d'ordres"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Compatibilitat amb interfície d'ordres de xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Compatibilitat amb controlador de xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Compatibilitat amb Cocoa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "Compatibilitat amb PNG (RPNG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "Compatibilitat amb JPEG (RJPEG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "Compatibilitat amb BMP (RBMP)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "Compatibilitat amb TGA (RTGA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "Compatibilitat amb SDL 1.2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "Compatibilitat amb SDL 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D8_SUPPORT,
   "Compatibilitat amb el Direct3D 8"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D9_SUPPORT,
   "Compatibilitat amb el Direct3D 9"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D10_SUPPORT,
   "Compatibilitat amb el Direct3D 10"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D11_SUPPORT,
   "Compatibilitat amb el Direct3D 11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D12_SUPPORT,
   "Compatibilitat amb el Direct3D 12"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GDI_SUPPORT,
   "Compatibilitat amb GDI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Compatibilitat amb Vulkan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Compatibilitat amb Metal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "Compatibilitat amb OpenGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "Compatibilitat amb OpenGL ES"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Compatibilitat amb multifil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "Compatibilitat amb KMS/EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Compatibilitat amb udev"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "Compatibilitat amb OpenVG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "Compatibilitat amb EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "Compatibilitat amb X11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Compatibilitat amb Wayland"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "Compatibilitat amb XVideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "Compatibilitat amb ALSA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "Compatibilitat amb OSS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "Compatibilitat amb OpenAL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "Compatibilitat amb OpenSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "Compatibilitat amb RSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "Compatibilitat amb RoarAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "Compatibilitat amb JACK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "Compatibilitat amb PulseAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PIPEWIRE_SUPPORT,
   "Suport de PipeWire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "Compatibilitat amb CoreAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "Compatibilitat amb CoreAudio V3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "Compatibilitat amb DirectSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "Compatibilitat amb WASAPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "Compatibilitat amb XAudio2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "Compatibilitat amb zlib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "Compatibilitat amb 7zip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZSTD_SUPPORT,
   "Suport per Zstandard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Compatibilitat amb biblioteques dinàmiques"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Càrrega dinàmica en temps d'execució de biblioteques de libretro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Compatibilitat amb Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "Compatibilitat amb GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "Compatibilitat amb GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "Compatibilitat amb imatges SDL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "Compatibilitat amb FFmpeg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "Compatibilitat amb mpv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "Compatibilitat amb CoreText"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "Compatibilitat amb FreeType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "Compatibilitat amb STB TrueType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Compatibilitat amb el joc en xarxa (Peer-to-Peer)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Compatibilitat amb Video4Linux2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SSL_SUPPORT,
   "Compatibilitat amb SSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "Compatibilitat amb libusb"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Selecció de base de dades"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Nom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Descripció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Gènere"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Assoliments"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Categoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Llengua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Regió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Exclusiu de consola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Exclusiu de plataforma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Puntuació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Mitjà"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Estil artístic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Tipus de joc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Narrativa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "Ritme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Perspectiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Ambientació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Estil visual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Vehicle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Editora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Desenvolupadora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Origen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Franquícia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Valoració de TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Valoració de la revista Famitsu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Revisió de la revista Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Valoració de la revista Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Número de la revista Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Mes de publicació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Any de publicació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Clasificació BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Clasificació ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Clasificació ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Clasificació PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Maquinari de millora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Clasificació CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Sèrie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Compatible amb controls analògics"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Compatible amb vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Compatible amb el joc cooperatiu"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Carrega configuració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Carrega la configuració existent i reemplaça els valors actuals."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Desa la configuració actual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Sobreescriu el fitxer de configuració actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Desa una configuració nova"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Desa la configuració actual en un fitxer nou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_AS_CONFIG,
   "Desa la configuració com a"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG,
   "Desa la configuració actual com el fitxer de configuració predeterminada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_MAIN_CONFIG,
   "Desa la configuració principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG,
   "Desa la configuració actual com a configuració principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Restaura els valors predefinits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Restableix la configuració actual als valors predeterminats."
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Controls bàsics del menú"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Moure cap amunt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Moure cap a baix"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Confirmar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "Informació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Iniciar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Fer aparèixer el menú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Sortir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Fer aparèixer el teclat"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Controladors"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Canvia els controladors usats pel sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Canvia les opcions de sortida de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Àudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Canvia les opcions d'entrada i sortida de l'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Canvia les opcions de comandaments, teclat i ratolí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latència"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Canvia les opcions relatives a la latència del vídeo, l'àudio i l'entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Canvia les opcions del nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Configuració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Canvia les opcions predefinides dels fitxers de configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Desament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Canvia les opcions de desament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Sincronització al núvol"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Canvia les opcions de sincronització al núvol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Activa la sincronització al núvol"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Prova de sincronitzar les configuracions, l'sram i els estats a un proveïdor d'emmagatzematge al núvol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Sincronització al núvol destructiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Sincronitza: Desaments/Estats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Sincronitza: Fitxers de configuració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Sincronitza: Miniatures"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Sincronitza: Fitxers del sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "En activar-ho, els desaments/estats se sincronitzaran amb el núvol."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "En activar-ho, els fitxers de configuració se sincronitzaran amb el núvol."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "En activar-ho, les imatges de miniatures se sincronitzaran amb el núvol. No es recomana, en general, excepte per a col·leccions grans de miniatures personalitzades. És una millor opció el baixador de miniatures."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "En activar-ho, els fitxers del sistema se sincronitzaran amb el núvol. Això pot augmentar substancialment el temps de sincronització; utilitzeu-ho amb compte."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "En desactivar-ho, els fitxers es mouen a una carpeta de còpia de seguretat abans de sobreescriure's o eliminar-se."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Rerefons de la sincronització al núvol"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Quin protocol de xarxa de l'emmagatzematge al núvol utilitzar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "URL de l'emmagatzematge al núvol"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "L'URL del punt d'entrada de l'API del servei d'emmagatzematge al núvol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Nom d’usuari"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "El nom d'usuari del teu compte d'emmagatzematge al núvol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Contrasenya"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "La contrasenya del teu compte d'emmagatzematge al núvol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Registres"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Canvia les opcions del registre."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Navegador de fitxers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Canvia la configuració del navegador de fitxers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Fitxer de configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Fitxer comprimit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Fitxer de configuració de l'enregistrament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Fitxer de cursors de la base de dades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Fitxer de configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Fitxer predefinit del shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Fitxer del shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Fitxer de reassignació del controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Fitxer de trucs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Fitxer de superposició."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Fitxer de base de dades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "Fitxer de tipografia TrueType."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Fitxer de text sense format."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Vídeo. Selecciona-ho per a obrir aquest fitxer amb el reproductor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Música. Selecciona-ho per a obrir aquest fitxer amb el reproductor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Fitxer d'imatge."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Imatge. Selecciona-ho per a obrir aquest fitxer amb el visor d'imatges."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Nucli Libretro. Seleccionar-ho associarà aquest nucli amb el joc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Nucli Libretro. Selecciona el fitxer perquè el RetroArch carregui aquest nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Directori. Seleccioneu-ho per a obrir aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Regulador de fotogrames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Canvia les opcions de rebobinat, avançament ràpid i càmera lenta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Enregistrament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Canvia les opcions d'enregistrament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Visualització en pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Canvia les superposicions de visualització i del teclat, i les opcions de notificacions en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Interfície d'usuari"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Canvia les opcions de la interfície d'usuari."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Servei d'IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Canvia les opcions del servei d'IA (traducció/text a veu/etc.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Accessibilitat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Canvia les opcions del narrador d'accessibilitat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Gestió d'energia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Canvia les opcions de la gestió d'energia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Assoliments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Canvia les opcions dels assoliments."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Xarxa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Canvia les opcions del servidor i la xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Canvia les opcions de les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Usuari"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Canvia la configuració de la privacitat, el compte i el nom d'usuari."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Directoris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Canvia els directoris predefinits on es troben els fitxers."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Assignacions"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Multimèdia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Rendiment"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "So"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Especificacions"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Emmagatzematge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Temporalització"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Canvia les opcions relacionades amb Steam."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Controlador d'entrada a utilitzar. Alguns controladors de vídeo en forcen un diferent. (Es requereix reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "El controlador udev llegeix els esdeveniments evdev per a la compatibilitat del teclat. També és compatible amb les trucades del teclat, ratolí i panells tàctils.\nDe manera predefinida en la majoria de les distribucions, els nodes /dev/input són només per a root (mode 600). Pots definir una regla udev que els faci accessibles a altres usuaris."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "El controlador d'entrada linuxraw necessita un TTY actiu. Els esdeveniments de teclat es llegeixen directament des de TTY, el què ho fa més senzill, però no tan flexible com l'udev. Els ratolins, etc. no són compatibles de cap de manera. Aquest controlador utilitza l'antiga API de palanques de control (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Controlador d'entrada. El controlador de vídeo podria forçar-ne un altre diferent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Comandament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Controlador de comandament a utilitzar (cal reiniciar)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "Controlador de comandaments DirectInput."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "Controlador de dispositiu d'interfície humana de baix nivell."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Controlador Linux cru, utilitza l'antiga API de palanques de control. Fes servir millor udev."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Controlador Linux per a comandaments connectats a un port paral·lel mitjançant adaptadors especials."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Controlador de comandament basat en biblioteques SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Controlador de comandament amb interfície udev, recomanat en general. Utilitza l'API recent de palanques de control evdev. És compatible amb la connexió en calent i la retroacció forçada.\nDe manera predefinida en la majoria de les distribucions, els nodes /dev/input són només per a root (mode 600). Pots definir una regla udev que els faci accessibles a altres usuaris."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "Controlador de comandaments XInput. Majorment, per a comandaments XBox."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Controlador de vídeo que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "Controlador OpenGL 1.x. Versió mínima requerida: OpenGL 1.1. No és compatible amb els shaders. Millor fes servir els últims controladors OpenGL si és possible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "Controlador OpenGL 2.x. Aquest controlador permet que s'utilitzin els nuclis libretro GL a més dels nuclis de renderitzat per programari. Versió mínima requerida: OpenGL 2.0 o OpenGLES 2.0. És compatible amb el format de shaders GLSL. Millor fes servir el controlador glcore si és possible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "Controlador OpenGL 3.x. Aquest controlador permet que s'utilitzin els nuclis libretro GL a més dels nuclis de renderitzat per programari. Versió mínima requerida: OpenGL 3.2 o OpenGLES 3.0+. És compatible amb el format de shaders Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Controlador Vulkan. Aquest controlador permet que s'utilitzin els nuclis libretro Vulkan a més dels nuclis de renderitzat per maquinari. Versió mínima requerida: Vulkan 1.0. És compatible amb els shaders HDR i Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "Controlador de renderitzat per programari SDL 1.2. Es considera que el seu rendiment no és l'òptim. Considera fer-lo servir només com a últim recurs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "Controlador de renderitzat per programari SDL 2. El rendiment de les implementacions del nucli de renderitzat per programari libretro depèn de la teva implementació de la plataforma SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Controlador Metal per a plataformes Apple. És compatible amb el format de shaders Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Controlador Direct3D 8 sense compatibilitat amb els shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Controlador Direct3D 9 amb compatibilitat amb l'antic format de shaders Cg."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Controlador Direct3D 9 amb compatibilitat amb el format de shaders HLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Controlador Direct3D 10 amb compatibilitat amb el format de shaders Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Controlador Direct3D 11 amb compatibilitat amb HDR i amb el format de shaders Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Controlador Direct3D 12 amb compatibilitat amb HDR i amb el format de shaders Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "Controlador DispmanX. Utilitza l'API DispmanX per al Videocore IV GPU a les Raspberry Pi 0..3. Sense compatibilitat amb superposicions ni shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "Controlador LibCACA. Produeix una eixida amb caràcters en compte d'amb gràfics. No es recomana per a l'ús pràctic."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Un controlador de vídeo Exynos de baix nivell que utilitza el bloc G2D del SoC de Samsung Exynos per a les operacions blit. El rendiment dels nuclis renderitzats per programari hauria de ser òptim."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Controlador de vídeo DRM simple. És un controlador de vídeo de baix nivell que utilitza libdrm per a fer l'escalat per maquinari, fent servir superposicions de la GPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Un controlador de vídeo Sunxi de baix nivell que utilitza el bloc G2D dels SoC Allwinner."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Controlador de la Wii U. És compatible amb els shaders Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Controlador de la Switch. És compatible amb el format de shaders GLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "Controlador OpenVG. Utilitza l'API de gràfics vectorials 2D accelerats per maquinari OpenVG."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "Controlador GDI. Utilitza una interfície de Windows antiga. No es recomana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Controlador de vídeo actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Àudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Controlador d'àudio que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "Controlador RSound per a sistemes d'àudio en xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Controlador antic Open Sound System."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Controlador predefinit ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "Controlador ALSA amb compatibilitat amb multifil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "Controlador ALSA implementat sense dependències."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "Controlador del sistema de so RoarAudio."
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
   "Controlador DirectSound. El DirectSound s'utilitza principalment per a Windows 95 i Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Controlador de l'API del Windwos Audio Session (WASAPI). El WASAPI s'utilitza principalment per a Windows 7 i superiors."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "Controlador PulseAudio. Si el sistema utilitza PulseAudio, assegura't de fer servir aquest controlador en compte de, p. ex., l'ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "Controlador PipeWire. Si el sistema utilitza PipeWire, assegura't de fer servir aquest controlador en compte de, p. ex., el PulseAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Controlador del Jack Audio Connection Kit (JACK)."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Micròfon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Controlador de micròfon que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Remostreig del micròfon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Controlador del remostreig del micròfon que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Marcs de blocs del micròfon"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Remostreig d'àudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Controlador de remostreig d'àudio que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Implementació de sinus cardinal (Windowed-Sinc)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Implementació de cosinus de convolució (Convoluted Cosine)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Implementació de remostreig més propera. Aquest remostreig ignora l'opció de qualitat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Càmera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Controlador de càmera que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Controlador Bluetooth que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Controlador Wi-Fi que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Ubicació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Controlador d'ubicació que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Controlador de menú que es farà servir (cal reiniciar)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "L'XMB és una interfície gràfica per al RetroArch que s'assembla al menú d'una consola de 7a generació. És compatible amb les mateixes funcions que l'Ozone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "L'Ozone és la interfície gràfica predefinida del RetroArch a la majoria dels dispositius. És optimitzada per a navegar-hi amb un comandament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "L'RGUI és una interfície gràfica simple integrada al RetroArch. És el controlador de menú que menys requisits de rendiment demana, i es pot utilitzar en pantalles de resolució baixa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "En dispositius mòbils, el RetroArch utilitza de manera predefinida la interfície mòbil, MaterialUI. Aquesta s'ha dissenyat per a les pantalles tàctils i els dispositius senyaladors, com ara ratolins o trackballs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Enregistrament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Controlador d'enregistrament que es farà servir."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Controlador MIDI que es farà servir."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "SwitchRes per a monitors CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Emet senyals de vídeo natius de baixa resolució per a pantalles de tub de raigs catòdics (CRT)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Sortida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Canvia les opcions de la sortida de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Mode de pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Canvia les opcions del mode de pantalla completa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Mode finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Canvia les opcions del mode finestra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Escalat d'imatge"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Canvia les opcions de l'escalat del vídeo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Canvia les opcions de vídeo de l'HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Sincronització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Canvia les opcions de sincronització de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Suspensió de l'estalvi de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Evita que s'activi l'estalvi de pantalla del sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Suspèn l'estalvi de pantalla. És una petició que el controlador de vídeo no té per què respectar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Vídeo multifil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Millora el rendiment a costa de la latència i de més entretallament del vídeo. Useu només si no es pot assolir la velocitat màxima de cap altra manera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Utilitza un controlador de vídeo multifil. En usar-lo, podria millorar el rendiment a costa de la latència i més entretallament del vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Inserció de fotogrames en negre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "ATENCIÓ: El parpelleig ràpid podria causar una persistència de la imatge en algunes pantalles. Utilitza-ho sota el teu propi risc. // Insereix fotogrames en negre entre fotogrames. Pot reduir molt el desenfocament per moviment emulant l'escaneig d'un monitor CRT, en detriment de la brillantor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Insereix fotogrames en negre entre fotogrames per a millorar la claredat del moviment. Useu només l'opció designada per a la freqüència d'actualització actual de la pantalla. No ho feu servir per a freqüències d'actualització que no siguin múltiples de 60 Hz, com ara 144 Hz, 165 Hz, etc. No ho combineu amb intervals d'intercanvi > 1, subfotogrames, retard dels fotogrames ni amb la sincronització a la velocitat de fotogrames del contingut exacta. Es pot deixar activada l'opció de VRR ([...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Inserció de fotogrames en negre (BFI) - Fotogrames foscos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Ajusta el nombre de fotogrames en negre a la seqüència total d'escaneig de sortida. Més significa una claredat de moviment més alta; menys significa una brillantor més alta. No aplicable a 120 Hz atès que només s'introduirà 1 fotograma en negre. Els paràmetres més alts del que és possible us limitarà al màxim possible de la freqüència d'actualització que trieu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Ajusta el nombre de fotogrames mostrats en la seqüència d'inserció de fotogrames en negre. Més fotogrames en negre augmenta la claredat, però redueix la brillantor. No es pot aplicar a 120 Hz atès que només hi ha un fotograma extra als 60 Hz, i per això cal que sigui en negre, ja que altrament la inserció de fotogrames en negre no seria activa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Subfotogrames d'ombreig"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "AVÍS: El parpelleig ràpid pot causar errors gràfics en algunes pantalles. Fes servir aquesta opció sota la teva responsabilitat // Simula una línia d'escaneig en moviment bàsica sobre múltiples subfotogrames dividint la pantalla en vertical i renderitzant cada part d'acord amb quants subfotogrames hi ha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Insereix fotogrames d'ombreig extra entre fotogrames per a qualsevol efecte d'ombreig que estigui designat per a executar-se més ràpid que la freqüència del contingut. Useu només l'opció designada per a la freqüència d'actualització actual de la pantalla. No ho feu servir per a freqüències d'actualització que no siguin múltiples de 60 Hz, com ara 144 Hz, 165 Hz, etc. No ho combineu amb intervals d'intercanvi > 1, BFI, retard dels fotogrames ni amb la sincronització a la velocitat d[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Captura de pantalla de la GPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Simulació de la línia d'escaneig en moviment"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "AVÍS: El parpelleig ràpid pot causar errors gràfics en algunes pantalles. Fes servir aquesta opció sota la teva responsabilitat // Simula una línia d'escaneig en moviment bàsica sobre múltiples subfotogrames dividint la pantalla en vertical i renderitzant cada part d'acord amb quants subfotogrames hi ha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Simula una línia d'escaneig en moviment bàsica sobre múltiples subfotogrames dividint la pantalla en vertical i renderitzant cada part d'acord amb quants subfotogrames hi ha des del capdamunt cap avall."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Les captures de pantalla prendran la GPU amb el material d'ombreig si es troba disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Filtre bilineal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Afegeix un difuminat lleuger a la imatge per a suavitzar les vores dels píxels. Aquesta opció té molt poc impacte en el rendiment. S'hauria de desactivar si useu ombreig."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Interpolació de la imatge"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Especifiqueu el mètode d'interpolació d'imatges quan s'escali el contingut a la IPU interna. Es recomana «Bicúbica» o «Bilineal» quan es fan servir filtres de vídeo de CPU. Aquesta opció no té cap impacte sobre el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bicúbica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Bilineal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Veí més proper"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Interpolació de la imatge"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Especifiqueu el mètode d'interpolació d'imatges quan es desactiva l'opció «Escalat enter». L'opció «Veí més proper» és la que té menys impacte sobre el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Veí més proper"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Semilineal"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Retard de l'ombreig automàtic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Retarda la càrrega automàtica de l'ombreig (en ms). Pot resoldre algunes errades gràfiques quan s'utilitza programari per capturar la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Filtre de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Aplica un filtre de vídeo per CPU. Pot tenir un gran impacte sobre el rendiment. Alguns filtres de vídeo podrien només funcionar amb nuclis que usen color en 32 o 16 bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Aplica un filtre de vídeo per CPU. Pot tenir un gran impacte sobre el rendiment. Alguns filtres de vídeo podrien només funcionar amb nuclis que usen color en 32 o 16 bits. Es poden seleccionar biblioteques de filtres de vídeo enllaçades de manera dinàmica."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Aplica un filtre de vídeo per CPU. Pot tenir un gran impacte sobre el rendiment. Alguns filtres de vídeo podrien només funcionar amb nuclis que usen color en 32 o 16 bits. Es poden seleccionar biblioteques de filtres de vídeo integrades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Elimina el filtre de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Elimina qualsevol filtre de vídeo per CPU actiu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Activa la pantalla completa sobre l'osca als dispositius Android i iOS"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
   "Fes servir la memòria intermèdia pels arguments de Metal (Cal reiniciar)"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS,
   "Pots provar d'augmentar el rendiment si fas servir la memòria intermèdia pels arguments de Metal. Alguns nuclis necessiten tenir aquesta opció activada. Aquesta opció pot fer malbé alguns shaders, sobretot en maquinari més antic o versions del sistema operatiu anteriors."
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "SwitchRes per a monitors CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Només per a pantalles CRT. Intenta usar la resolució i freqüència d'actualització exacta del nucli/joc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "Súper-resolució CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Canvia entre súper-resolucions nativa i ultra-allargada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Centrar en l'horitzontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Rota entre aquestes opcions si la imatge no se centra correctament a la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Mida horitzontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Roteu entre aquestes opcions per ajustar la configuració horitzontal per canviar la mida de la imatge."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_VERTICAL_ADJUST,
   "Centrat vertical"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Utilitza el menú d'alta resolució"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Canvia al mode de línia d'alta resolució amb menús d'alta resolució quan no hi ha contingut carregat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Freqüència d'actualització personalitzada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Utilitza, si cal, una freqüència d'actualització personalitzada especificada en el fitxer de configuració."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Índex de monitor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Seleccioneu quina pantalla utilitzar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Monitor preferit. 0 (predefinit) significa que no es prefereix cap monitor en particular, 1 i següents (sent 1 el primer monitor) suggereix al RetroArch utilitzar eixe monitor en particular."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Optimitza per al comandament de la Wii U (cal reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Utilitza un escalat exacte de 2x del controlador a la subàrea de visualització. Deshabiliteu per mostrar a la resolució nativa de la TV."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Rotació del vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Força una certa rotació del vídeo. Aquesta rotació s'afegeix a les que permet el nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Orientació de la pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Força una certa orientació de la pantalla des del sistema operatiu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Índex de la GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Selecciona quina targeta gràfica utilitzar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Desplaçament horitzontal de la pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Força un cert desplaçament horitzontal al vídeo. S'aplica globalment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Desplaçament vertical de la pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Força un cert desplaçament vertical al vídeo. S'aplica globalment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Freqüència d'actualització vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Freqüència d'actualització vertical de la pantalla. S'usa per a calcular una freqüència d'àudio adient.\nS'ignorarà si s'activa «Vídeo multifil»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Freqüència d'actualització estimada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "La freqüència d'actualització precisa estimada de la pantalla en Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "La freqüència exacta d'actualització del teu monitor (en Hz). Serveix per calcular la freqüència d'entrada d'àudio com:\nfreqüència d'entrada d'àudio = velocitat entrada del joc * freqüència d'actualització del monitor/freqüència d'actualització del joc.\nSi el nucli no indica un valor, s'assumeix el format NTSC per motius de compatibilitat.\nAquest valor serà similar a 60 Hz per evitar canvis en el to. Si el teu monitor no és compatible amb 60 Hz, desactiva la sincronia vertica[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Estableix la freqüència d'actualització notificada per la pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "La freqüència d'actualització que notifica el controlador de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Canvia automàticament la freqüència d'actualització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Canvia automàticament la freqüència d'actualització de la pantalla en funció del contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Només en mode de pantalla completa exclusiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Només en mode de finestra a pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Tots els modes de pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Llindar de freqüència d'actualització PAL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "La taxa de refresc màxima considerada com a PAL."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Freqüència d'actualització vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Estableix la freqüència d'actualització vertical de la pantalla. «50 Hz» permetrà un vídeo fluid quan s'executi contingut PAL."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Desactiva forçadament el FBO sRGB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Força la desactivació del suport sRGB del FBO. Amb Windows alguns controladors OpenGL d'Intel tenen problemes de vídeo amb l'FBO sRGB. Activant això es podria resoldre."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Visualització en pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Visualització en pantalla completa. Es pot canviar durant l'execució. Es pot sobreescriure amb un paràmetre de la consola de comandes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Mode de finestra a pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Amb pantalla completa, prefereix usar una finestra a pantalla completa per evitar el canvi del mode de pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Amplada de la pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Estableix una mida personalitzada de l'amplada per mode de pantalla completa sense finestra. S'utilitzarà la resolució de l'escriptori si no s'estableix."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Alçada de la pantalla completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Estableix una mida personalitzada de l'alçada per mode de pantalla completa sense finestra. S'utilitzarà la resolució de l'escriptori si no s'estableix."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Força la resolució amb UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Força la resolució de la mida de pantalla completa. S'usarà un valor fix de 3840 x 2160 si s'estableix a 0."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Escalat de la finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Estableix la mida de la finestra al múltiple especificat de la mida de la subàrea de visualització del nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Opacitat de la finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Estableix la transparència de la finestra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Mostra les decoracions de finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Mostra la barra de títol i les vores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Mostra la barra de menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Mostra la barra del menú de la finestra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Recorda la posició i mida de la finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Mostra tot el contingut dins una finestra de mida fixa amb les dimensions especificades per 'Amplada de la finestra' i 'Alçada de la finestra', i desa la mida i posició actual de la finestra en tancar RetroArch. Si es deshabilita la mida de la finestra s'establirà dinàmicament en funció de l'\"Escalat de la finestra\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Utilitza una mida de finestra personalitzada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Mostra tot el contingut dins una finestra de mida fixa amb les dimensions especificades per 'Amplada de la finestra' i 'Alçada de la finestra'. Si es deshabilita la mida de la finestra s'establirà dinàmicament en funció de l'\"Escalat de la finestra\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Amplada de la finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Estableix l'amplada personalitzada de la finestra de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Alçada de la finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Estableix l'alçada personalitzada de la finestra de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Amplada màxima de la finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Estableix l'amplada màxima de la finestra de la pantalla quan redimensiona automàticament en funció de l'\"Escalat de la finestra\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Alçada màxima de la finestra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Estableix l'alçada màxima de la finestra de la pantalla quan redimensiona automàticament en funció de l'\"Escalat de la finestra\"."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Escalat d'enter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Limita l'escalat del vídeo a múltiples enters. La mida base dependrà de la geometria del sistema i de la relació d'aspecte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "Eix d'escala entera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Escala l'alçada i l'amplada. Els escalats irregulars s'aplicaran només a les imatges d'alta resolució."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Escalat per nombre enter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Augmenta o disminueix l'escala del següent valor enter. 'Intel·ligent' disminueix l'escala si la imatge queda retallada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "Reduïr escala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "Augmentar l'escala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Intel·ligent"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Relació d'aspecte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Estableix la relació d'aspecte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Configura la relació d'aspecte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Valor de coma flotant per la relació d'aspecte del vídeo (amplada / alçada)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Configuració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Proporcionada pel nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Completa"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Manté la relació d'aspecte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Manté relacions d'aspecte d'1:1 píxel en escalar el contingut via l'IPU interna. Si es desactiva, les imatges s'estiraran fins a omplir la pantalla completa."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Relació d'aspecte personalitzat (posició X)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Desplaçament personalitzat per definir la posició en l'eix X de l'àrea de visualització."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Relació d'aspecte personalitzat (posició Y)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Desplaçament personalitzat per definir la posició en l'eix Y de l'àrea de visualització."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Compensa l'eix X del punt d'anclatge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Compensa l'eix X del punt d'anclatge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Compensa l'eix Y del punt d'anclatge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Compensa l'eix Y del punt d'anclatge"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Posició horitzontal de contingut si l'àrea de visualització és més ampla que el contingut. 0.0 per l'extrem esquerre, 0.5 per centrat, 1.0 per per l'extrem dret."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Estableix la posició vertical del contingut si l'àrea de visualització és més alta que el contingut. 0,0 per l'extrem superior, 0,5 per a centrar, 1,0 per l'extrem inferior."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Compensació de l'eix X del punt d'ancoratge de l'àrea de visualització (orientació vertical)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Compensació de l'eix X del punt d'ancoratge de l'àrea de visualització (orientació vertical)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Compensació de l'eix Y del punt d'ancoratge de l'àrea de visualització (orientació vertical)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Compensació de l'eix Y del punt d'ancoratge de l'àrea de visualització (orientació vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Posició horitzontal de contingut si l'àrea de visualització és més ampla que el contingut. 0.0 per l'extrem esquerre, 0.5 per centrat, 1.0 per per l'extrem dret."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Estableix la posició vertical del contingut si l'àrea de visualització és més alta que el contingut. 0,0 per l'extrem superior, 0,5 per a centrar, 1,0 per l'extrem inferior. (Orientació vertical)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Relació d'aspecte personalitzat (amplada)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Amplada personalitzada de la subàrea de visualització que s'usa si la relació d'aspecte s'estableix a 'Relació d'aspecte personalitzada'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Relació d'aspecte personalitzat (alçada)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Alçada personalitzada de la subàrea de visualització que s'usa si la relació d'aspecte s'estableix a 'Relació d'aspecte personalitzada'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Retalla el sobreescombrat (Es requereix reinici)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Retalla uns quants píxels sobre les vores de la imatge deixats en blanc normalment pels desenvolupadors i que de vegades també contenen píxels brossa."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "Activa l'HDR"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Activa l'HDR si la pantalla ho suporta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Pic de luminància"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Estableix el pic de luminància (en cd/m2) que pot reproduir la vostra pantalla. Consulteu RTings pel pic de luminància de la vostra pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Luminància de blancs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Estableix la luminància a la que hauria d'estar un paper blanc, per exemple un text llegible, o luminància al cim del rang SDR (Standard Dynamic Range). Útil per ajustar a diferents condicions de llum del vostre entorn."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "Control de gamma/contrast per a HDR. Agafa els colors i incrementa el rang general entre les parts més fosques i més brillants de la imatge. Quant més alt és el contrast HDR, més gran es torna la diferència, mentre que quan més baix és el contrast, més desgastada es torna la imatge. Ajuda als usuari a ajustar la imatge al seu gust i al que sembla veure's millor a la seva pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Expandeix l'escala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Una vegada convertit l'espai de colors a un espai lineal, decideix si s'hauria d'usar una escala de color ampliada per a obtenir HDR10."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Sincronització vertical (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Sincronitza la sortida de vídeo de la targeta gràfica a la freqüència d'actualització de la pantalla. Recomanat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Interval d'intercanvi de VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Usa un interval d’intercanvi personalitzat per la sincronització vertical. Redueix la freqüència d’actualització del monitor pel factor especificat. «Automàtic» estableix el factor depenent dels fotogrames per segon que indica el nucli, proporcionant una millora en la taxa de fotogrames quan per exemple s’executa un contingut a 30 fps en una pantalla a 60 Hz o un contingut a 60 fps en una pantalla a 120 Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Automàtic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "VSync adaptatiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "La sincronització vertical roman activa fins que el rendiment cau per davall de la freqüència d'actualització establerta. Pot minimitzar l'entretallament quan el rendiment cau per davall del temps real, i pot ser més energèticament eficient. No és compatible amb 'Retard de fotogrames'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Retard dels fotogrames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Redueix la latència a canvi d'augmentar la probabilitat que es produeixen estrebades de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Selecciona quants mil·lisegons passen entre l'execució del nucli i la presentació de la imatge. Redueix la latència a canvi d'augmentar el risc d'estrebades.\nEls valors iguals o superiors a 20 es consideren com un percentatge de duració dels fotogrames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Retard automàtic de fotograma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Ajusta el retard de fotogrames real de forma dinàmica."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Intentar mantenir el desfasament de fotogrames objectiu i minimitzar els fotogrames perduts. Si el desfasament de fotogrames està establert a 0 (automàtic), el punt de partida serà de 3/4 parts de la duració dels fotogrames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Automàtic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "real(s)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Sincronització dura de GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Sincronització rígida de la CPU i GPU. Redueix la latència a cost de rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Fotogrames de sincronització dura de GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Estableix quants fotogrames de CPU poden anar per davant de la GPU quan s'usa 'Sincronització dura de GPU'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Escull quants fotogrames pot anar la CPU per davant de la GPU si es fa servir l'opció 'Sincronia estricta de CPU'. El valor màxim és 3.\n 0: Sincronitza immediatament a la GPU.\n 1: Sincronitza amb el fotograma anterior.\n 2: ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Sincronització a la velocitat de fotogrames del contingut exacta (S-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Sense desviació de la cadència demanada pel nucli. Useu-ho per a pantalles amb freqüències d'actualització variable (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Sincronització a la velocitat de fotogrames del contingut exacta. Aquesta opció és l'equivalent a forçar una velocitat de x1 permetent alhora l'avançament ràpid. No produeix alteracions en la velocitat d'actualització sol·licitada pel nucli, ni en el Control de velocitat dinàmic."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Sortida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Canvia les opcions de sortida de l'àudio."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Micròfon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Canvia la configuració d'entrada d'àudio."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Re-mostreig"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Canvia les opcions de remostreig de l'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Sincronització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Canvia les opcions de sincronització de l'àudio."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Canvia les opcions del MIDI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Mesclador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Canviar les opcions del mesclador d'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Sons del menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Canvia les opcions dels sons del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Silencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Silencia l'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Silencia el mesclador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Silencia l'àudio del mesclador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Respecta el mode silenciós"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Silencia l'àudio en fer servir el mode silenci."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Silencia l'àudio en l'avançament ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Silencia automàticament el so quan s’usa l’avançament ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Accelerar l'àudio durant l'avançament ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Accelera l'àudio a l'emprar l'avançament ràpid. Defugirá sorolls en l'àudio però canviant la seva tonalitat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Silencia l'àudio en rebobinar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Silencia automàticament el so quan s’usa el rebobinat ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Guany de volum (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Volum del so (en dB). 0 dB és el volum normal, sense cap guany afegit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Volum de l'àudio, expressat en decibels dB. 0 dB és el volum normal, quan no s'aplica cap guany. El guany es pot controlar en temps real amb Pujar/Baixar volum."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Guany de volum del mesclador (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Volum del mesclador global de so (en dB). 0 dB és el volum normal, sense cap guany afegit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Connector DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Connector DSP d’àudio que processa el so abans d’enviar-lo al controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Treu el connector DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Desactiva qualsevol connector DSP d’àudio activat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Mode WASAPI exclusiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Permet al controlador WASAPI prendre el control exclusiu del dispositiu d’àudio. Si es desactiva aquesta opció, serà usat en mode compartit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Format WASAPI de coma flotant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Usa el format de coma flotant per al controlador WASAPI si és admès pel dispositiu d’àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Mida de la memòria intermèdia compartida de WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Mida (en fotogrames) de la memòria intermèdia quan s’usa el controlador WASAPI en mode compartit."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Àudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Habilita la sortida d’àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Dispositiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Força el dispositiu que fa servir el controlador d’àudio. Això depèn del controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Força el dispositiu que fa servir el controlador d’àudio. Això depèn del controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Valor personalitzat del dispositiu PCM pel controlador ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Valor personalitzat per la ruta del controlador OSS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Valor personalitzat del nom del port pel controlador JACK (per exemple: system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Direcció IP personalitzada en un servidor RSound pel controlador RSound."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Latència de l'àudio (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Latència màxima d'àudio en mil·lisegons. El controlador intentarà mantenir la latència real en un 50% d'aquest valor. Aquesta opció no es complirà si el controlador d'àudio no pot produir aquesta latència."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Micròfon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Activa l'entrada d'àudio en aquells nuclis que siguin compatibles. Si el nucli no fa servir el micròfon, no augmenta la càrrega de la CPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Dispositiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Força el dispositiu que fa servir el controlador de micròfon. Això depèn del controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Força el dispositiu que fa servir el controlador de micròfon. Això depèn del controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Qualitat del re-sampleig"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Baixeu aquest valor per a afavorir el rendiment/baixa latència sobre la qualitat de l'àudio, augmenteu-lo per a una millor qualitat d'àudio a expenses de rendiment/baixa latència."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Freqüència d'entrada predeterminada (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Indica la freqüència de mostreig de l'entrada d'àudio. S'utilitzarà si els nuclis no demanen un valor concret."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Latència de l'entrada de so (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Escull la latència per l'entrada d'àudio en mil·lisegons. Aquest valor pot ser ignorat si el controlador del micròfon no pot generar aquesta latència."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Mode WASAPI exclusiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Permet a RetroArch a prendre el control del dispositiu de micròfon quan es fa servir el controlador de micròfon WASAPI. Si aquesta opció està deshabilitada, RetroArch farà servir el mode compartit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Format WASAPI de coma flotant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Fes servir el format de coma flotant pel controlador WASAPI, si és compatible amb el teu dispositiu d'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Mida de la memòria intermèdia compartida de WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Mida (en fotogrames) de la memòria intermèdia quan s’usa el controlador WASAPI en mode compartit."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Qualitat del re-sampleig"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Baixeu aquest valor per a afavorir el rendiment/baixa latència sobre la qualitat de l'àudio, augmenteu-lo per a una millor qualitat d'àudio a expenses de rendiment/baixa latència."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Freqüència de sortida (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Freqüència de mostreig de la sortida d'àudio."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Sincronització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Sincronitza l'àudio. Recomanat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Desviació màxima de sincronització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "El canvi màxim en la freqüència d'entrada d'àudio. Augmentar això activa canvis molt grans en la sincronització a cost d'un to d'àudio imprecís (ex. executar nuclis PAL a pantalles NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "La variació màxima de la sincronia d'àudio.\nDefineix la variació màxima de la freqüència d'entrada. Es pot augmentar el valor per canviar la sincronia (per exemple, si executes nuclis PAL en pantalles NTSC) a canvi de canviar el to de l'àudio.\nLa freqüència d'entrada es calcula com:\nfreqüència d'entrada * (1.0 +/- (variació màxima de sincronia))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Control dinàmic de freqüència d'àudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Ajuda a mitigar imperfeccions en la sincronització de l'àudio i vídeo. Teniu en compte que si es desactiva és pràcticament impossible obtenir una sincronització correcta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Establir aquest valor a 0 per desactivar el control de la freqüència. Qualsevol altre valor canviarà el valor delta de control de la freqüència d'àudio.\nDefineix la quantitat de freqüències d'entrada que poden ajustar-se dinàmicament. La freqüència d'entrada es defineix com:\nFreqüències d'entra * (1.0 +/- (delta de control de freqüència))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Seleccioneu el dispositiu d'entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Escull el dispositiu d'entrada. Si està seleccionat a 'Off', l'entrada MIDI estarà deshabilitada. El nom del dispositiu es pot introduir de forma manual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Sortida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Seleccioneu el dispositiu de sortida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Escull el dispositiu de sortida (específic del controlador). La sortida de so MIDI es desactivarà si aquesta opció està desactivada. També es pot introduir el nom del dispositiu manualment.\nSi s'activa la sortida MIDI i el nucli/joc/aplicació son compatibles amb la sortida MIDI, alguns o tots els sons es generaran pel dispositiu MIDI. Si es selecciona un controlador MIDI 'null', aquests sons no es reproduiran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Volum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Estableix el volum de sortida (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Mescla de la retransmissió #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Reprodueix"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Iniciarà la reproducció del flux d'àudio. En acabar, s'eliminarà de la memòria el flux actual d'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Reprodueix (bucle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Iniciarà la reproducció del flux d'àudio. En acabar, farà un bucle i reproduirà de nou la pista des del principi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Reprodueix (Seqüència)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Iniciarà la reproducció del flux d'àudio. En acabar, saltarà al següent flux d'àudio en ordre seqüencial i repetirà aquest comportament. Útil com un mode de reproducció d'àlbum."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Atura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Aturarà la reproducció del flux d'àudio, però no l'eliminarà de la memòria. Es pot iniciar de nou seleccionant 'Reprodueix'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Elimina"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Aturarà la reproducció completament del flux d'àudio i l'eliminarà completament de la memòria."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Volum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Ajusta el volum del flux d'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Estat: no disponible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Estat: aturat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Estat: reproduint"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Estat: Reproduint (en bucle)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Estat: Reproduint (Seqüencial)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Mesclador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Reprodueix fluxos d'àudio simultanis fins i tot al menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Activa el so de 'OK'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Activa el so de 'Cancel·la'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Activa el so de 'Notifica'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Activa el so de 'BGM'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Activa el so de desplaçament"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Usuaris màxims"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "El número màxim d'usuaris suportat per RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Sistema de votació (Es requereix reinici)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Afecta com RetroArch realitza el sondeig d'entrada. Depenent de la vostra configuració, establir-ho a 'Prest' o 'Tard' pot significar menys latència."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Influeix en la forma de testejar el senyal d'entrada a RetroArch.\nPrimerenc: El test es fa abans de processar el fotograma.\nNormal: El test es fa quan es sol·liciti.\nTardà: el test es fa després de la primera petició de l'estat d'entrada de cada fotograma.\nSi es selecciona Primerenc o Tardà, es pot reduir la latència. Aquesta configuració no tindrà efecte durant una sessió de joc en xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Re-mapeja els controls d'aquest nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Sobreescriu les assignacions d'entrada amb les establertes per a aquest nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Endreça les assignacions per cada controlador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Les assignacions s'aplicaran només al controlador actiu en el qual s'han desat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Auto-configura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Configura automàticament els controladors que tenen un perfil, a l'estil Plug-and-Play."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Deshabilita les combinacions de tecles de Windows (Es requereix reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Manté les combinacions de tecles Windows dins l'aplicació."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Selecciona el teclat físic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Fes servir aquest dispositiu per un teclat físic i no per un controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Si RetroArch identifica un teclat físic com algun tipus de controlador, aquí pots obligar a configurar el dispositiu com un teclat.\nÉs útil si vols intentar emular un ordinador en un dispositiu tipus Android TV i també tens un teclat físic que es pot connectar al dispostiu."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Sensor d'entrada auxiliar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Activa l'entrada des dels sensors d'acceleròmetre, giroscopi i luminància, si està suportat pel maquinari actual. Pot tenir un impacte sobre el rendiment i/o un increment de consum energètic en algunes plataformes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Captura el ratolí automàticament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Activa la captura del ratolí quan l'aplicació té focus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Auto-activa el mode 'Focus de joc'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Activa sempre el mode 'Focus de joc' en iniciar i reprendre el contingut. Si s'estableix a 'Detecta', l'opció s'activarà si el nucli actual implementa la funcionalitat de crida de retorn del frontal de teclat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "Desactiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "Activa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Detecta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Posa en pausa el contingut quan es desconnecti el comandament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Posa en pausa el contingut quan qualssevol controlador es desconnecti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Llindar de l'eix del botó d'entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Quant s'ha d'inclinar per esdevenir una pulsació de botó quan s'utilitza 'Analògic a Digital'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Zona morta analògica"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Ignora els moviments de la palanca analògica per devall del valor de la zona morta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Sensibilitat analògica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Sensibilitat de l'acceleròmetre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Sensibilitat del giroscopi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Ajusta la sensibilitat de les palanques analògiques."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Ajusta la sensibilitat de l'acceleròmetre."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
  "Ajusta la sensibilitat del giroscopi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Temps d'espera d'assignació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "La quantitat de segons que s'espera abans de procedir a la següent assignació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Sosteniment d'assignació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "La quantitat de segons que se sosté una entrada per a assignar-la."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Clica al teclat, ratolí o controlador"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Deixa anar totes les tecles i botons!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Temps d'espera superat"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Mantingueu premut"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Disparador de turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE,
   "En desactivar aquesta opció, s'aturaran totes les opcions de turbo/tir automàtic."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Període de turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Indica la duració entre fotogrames quan els botons amb turbo es cliquen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DUTY_CYCLE,
   "Cicle de treball del turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE,
   "El nombre de fotogrames que es premen els botons del període de turbo. Si aquest número és igual o major que el període de turbo, els botons mai s'amollaran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF,
   "Mig cicle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Mode turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Selecciona el comportament general del mode turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Clàssic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Clàssic (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Botó simple (alternar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Botó simple (Mantenir)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Mode clàssic de maneig amb dos botons. Manté clicat un botó i clica el botó Turbo al mateix temps per activar la seqüència.\nEl turbo es pot assignar mitjançant la configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Ús clàssic dels dos botons. Manté clicat un botó i clica a la vegada el botó de turbo per activar el mode turbo. Desactivar turbo: manté clicat el botó i torna a clicar el botó turbo.\nEl botó turbo es pot assignar dins de la configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Mode d'alternança. Clica al botó Turbo una vegada per activar la seqüència de clicar i alliberar el botó seleccionat i torna a clicar el botó Turbo per desactivar-lo.\nEl turbo pot assignar-se a la configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Mode de bloqueig de botó. LA seqüència de clicar i soltar el botó predeterminat seleccionat es mantindrà activa sempre que es mantingui clicat el botó de turbo.\nEl turbo pot assignar-se a configuració.\nSi vols emular el mètode de tir automàtic d'èpoques anteriors, assigna els botons de turbo i predeterminat per a què siguin el mateix botó del controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "Assignació del turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "L'assignació que activarà el turbo en el RetroPad. Si està buit, s'utilitzarà l'assignació específica del port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "Botó del turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "El botó que es farà servir pel turbo en el mode de botó dedicat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "Permet direccions amb la creueta amb turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "Permet que els botons digitals de direcció (coneguts com a creueta) puguin fer servir el mode turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Disparador de turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Canvia les opcions per disparar amb turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Vibració/Resposta Hàptica"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Canvia les opcions de vibració i resposta hàptica."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Controls de menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Canvia les opcions de control del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Dreceres de teclat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Canvia la configuració i l'assignació de tecles d'accés ràpid, així com les combinacions de tecles per mostrar o amagar el menú durant la partida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Assignacions dels RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Canvia les assignacions del RetroPad virtual respecte a un dispositiu d'entrada físic. Si es reconeix i configura automàticament un dispositiu d'entrada correctament, els usuaris no necessitaran aquest menú.\nNota: per fer canvis específics segons cada nucli, fes servir el menú 'Controls' del menú ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro fa servir un controlador virtual conegut com 'RetroPad' per comunicar els senyals entre els nuclis i els frontends i viceversa. Aquest menú determina com s'assignen els senyals dels controladors físics al RetroPad virtual i quins ports fa servir cada dispositiu.\nSi un controlador físic és reconegut i configurat automàticament, els usuaris no hauran d'utilitzar aquest menú en cap moment, i per canvis propis de cada nucli, caldria fer servir el menú ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Controls del port %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Canvia com les entrades del RetroPad virtual són assignades a les entrades del dispositiu d'entrada físic per aquest port virtual."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Canvia les assignacions d'entrada específiques pel nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Solució per les desconnexions d’Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Solució temporal per la desconnexió intermitent dels controladors. Evita que hi hagi 2 jugadors amb controladors idèntics."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Confirma que vols Sortir/Tancar/Reiniciar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "Requereix que la drecera de tecla per Sortir/Tancar/Reiniciar s'hagi de clicar dues vegades."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Vibra en prémer la tecla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Activa la vibració del dispositiu (per a nuclis suportats)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Intensitat de la vibració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Especifica la magnitud dels efectes de resposta hàptica."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Controls de menú unificats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Useu els mateixos controls per a ambdós, el menú i el joc. Aplica al teclat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "El menú inverteix els botons d'OK i Cancel·la"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Inverteix els botons d'OK/Cancel·la. Desactivat correspon a l'orientació japonesa, activat correspon a l'orientació occidental."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Intercanvia els botons de desplaçament dels menús"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Intercanvia els botons de desplaçament. En desactivar aquesta opció, els botons L/R mouran 10 elements i els botons L2/R2 aniran en ordre alfabètic."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Tots els usuaris controlen el menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Permet que qualsevol usuari controli el menú. Si es desactiva, només l'usuari 1 pot controlar el menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SINGLECLICK_PLAYLISTS,
   "Llistes de reproducció en un sol clic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SINGLECLICK_PLAYLISTS,
   "Omet 'Executar' del menú quan s'executen les entrades de la llista de reproducció. Clica la creueta digital mentre es clica el botó OK per accedir al menú 'Executa'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ALLOW_TABS_BACK,
   "Permet tornar enrere des de les seleccions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ALLOW_TABS_BACK,
   "Torna al menú principal des de les seccions o des de la barra lateral en clicar el botó Enrere."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Acceleració del desplaçament del menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Velocitat màxima del cursor quan es manté una direcció de desplaçament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Retard del desplaçament del menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Retard inicial en mil·lisegons quan es manté una direcció per desplaçar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Inhabilita el botó d’informació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Evita que aparegui el menú d'informació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Inhabilita el botó de cerca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Evita que aparegui el menú de cerca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Deshabilita analògic esquerre en el menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Evita que el joystick analògic esquerre pugui moure el menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Deshabilita l'analògic dret en els menús"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Evita que el joystick analògic dret pugui moure el menú. El joystick analògic dret canvia les miniatures de les llistes de reproducció."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Activa Dreceres de Teclat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Si la tecla per activar tecles ràpides està assignada, és necessari mantenir-la clicada abans de clicar qualsevol altra tecla ràpida. Això permet assignar botons dels controladors a funcions de tecles ràpides sense influenciar a les entrades normals. Si s'assigna la tecla a un sol comandament, aquest comandament no serà imprescindible per les tecles ràpides del teclat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Si aquesta tecla ràpida està assignada a un teclat, un botó o l'eix d'un controlador, la resta de tecles ràpides es desactivaran a no ser que aquesta tecla sigui clicada al mateix temps.\nAixò serveix per implementacions centrades en RETRO_KEYBOARD, que fan servir una gran part del teclat i en les que no vols que les tecles ràpides interfereixin amb el funcionament normal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Retard de l'activació de les dreceres de teclat (fotogrames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Afegeix un retard en fotogrames abans que l'entrada normal es bloquegi després de polsar la tecla 'Activa dreceres de teclat' assignada. Permet que es capturi l'entrada normal de la tecla 'Activa dreceres de teclat' quan es mapeja a una altra acció (ex. Selecciona al RetroPad)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Fusionar tipus de dispositiu per a tecles ràpides"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Bloqueja totes les dreceres de teclat dels teclats i controladors si qualsevol dels dispositius té un botó o tecla per activar les dreceres de teclat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Commuta el menú (combinació de botons)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Combinació de botons del controlador per a activar el menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Commuta el menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Alterna entre mostrar el menú o el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Sortir (combinació de botons)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Combinació de botons del controlador per sortir de RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Sortir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Tanca el RetroArch, assegurant-se que totes les dades desades i els fitxers de configuració es passen a disc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Tanca contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Tanca el contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Resetejant el contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Reinicia al principi el contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Avanç ràpid (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Canvia entre la velocitat normal i la d'avanç ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Avanç ràpid (manté)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Activa l'avanç ràpid quan es manté. El contingut s'executa a velocitat normal quan s'amolla la tecla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Càmera lenta (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Canvia entre càmera lenta i velocitat normal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Càmera lenta (manté)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Activa la càmera lenta quan es manté. El contingut s'executa a velocitat normal quan s'amolla la tecla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Rebobina"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Rebobina el contingut actual mentre es polsa la tecla. Ha d'estar activat 'Suporta rebobinat'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Fer pausa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Alterna entre aturar o reprendre el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Avançar fotograma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Avança un fotograma el contingut en pausar."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Silenciar l'àudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Canvia entre activada/desactivada la sortida d'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Apuja el Volum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Augmenta el nivell de volum de la sortida d'àudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Abaixa el Volum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Disminueix el nivell de volum de la sortida d'àudio."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Carrega estat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Carrega un estat desat de la posició actualment seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Desa estat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Desa un estat a la posició actualment seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Següent ranura de desat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Incrementa l'índex de la posició de l'estat desat que està seleccionat actualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Posició anterior de desat ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Disminueix l'índex de la posició de l'estat desat que està seleccionat actualment."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Expulsa el disc (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "I una safata virtual de disc es troba tancada, l'obre i elimina el disc carregat. Altrament, insereix el disc seleccionat actualment i tanca la safata."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Disc Següent"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Incrementa l'índex del disc seleccionat actualment. La safata de disc virtual ha d'estar oberta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Disc Anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Disminueix l'índex del disc seleccionat actualment. La safata de disc virtual ha d'estar oberta."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Shaders (Alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Activa o desactiva el shader seleccionat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_HOLD,
   "Shaders (Mantenir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD,
   "Manté el shader seleccionat activat o desactivat mentre la tecla és clicada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Següent shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Carrega i aplica el següent fitxer predefinit de shader dins l'arrel del directori de 'Shaders de Vídeo'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Shader Anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Carrega i aplica l'anterior fitxer predefinit de shader dins l'arrel del directori de 'Shaders de Vídeo'."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Trampes (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Canvia entre activada/desactivada la trampa seleccionada actualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Següent Índex de Trampes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Augmenta l'índex de la trampa seleccionada actualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Anterior Índex de Trampes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Disminueix l'índex de la trampa seleccionada actualment."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Fes una captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Captura una imatge del contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Enregistra (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Inicia/Atura l'enregistrament de la sessió actual a un fitxer de vídeo local."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Transmet (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Inicia/Atura la transmissió de la sessió actual a una plataforma de vídeo en línia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Reproduir repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Reprodueix el fitxer de repetició des de la posició seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Enregistrar la repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Enregistra un fitxer de repetició a la posició seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Atura l'enregistrament/repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Atura la gravació/reproducció de la repetició actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Desa el punt de control de repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Marca un punt de control a la repetició que s'està reproduint."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Punt de control de repetició anterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Retrocedeix la repetició al punt de control anterior automàtic o desat manualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Punt de control de repetició posterior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Avança la repetició al següent punt de control automàtic o desat manualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "Següent posició de repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Augmenta l'índex de la posició de la repetició seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Ranura prèvia de desat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Disminueix l'índex de la posició de la repetició seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_TURBO_FIRE_TOGGLE,
   "Pulsació de turbo (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE,
   "Alterna el mode d'activació del turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Agafa el Ratolí (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Agafa o allibera el ratolí. En agafar-lo, s'amaga i confina el cursor del sistema a la finestra de visualització del Retroarch, millorant l'entrada de ratolí relativa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Focus de Joc (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Activa o desactiva el mode 'Focus de Joc'. Quan el contingut té el focus es desactiven les dreceres de teclat (l'entrada completa de teclat es passa al nucli que s'executa) i es captura el ratolí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Pantalla completa (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Canvia entre els modes de pantalla completa i pantalla en finestra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Menú d'Escriptori (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Obre la interfície d'usuari d'escriptori WIMP (Finestres, Icones, Menús, Punter) complementària."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Sincronització a la velocitat de fotogrames del contingut exacta (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Canvia a activat/desactivat la sincronització a fotograma de contingut exacte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Anticipa (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Canvia a activat/desactivat l'anticipació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Fes servir fotogrames preventius (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Activa o desactiva el sistema de fotogrames preventius."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Mostra els FPS (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Canvia entre activat/desactivat l'indicador d'estat dels 'fotogrames per segon'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Mostra Estadístiques Tècniques (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Canvia entre activada/desactivada la visualització en pantalla de les estadístiques tècniques."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Superposició de teclat (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Activa o desactiva la superposició del teclat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Següent Superposició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Canvia a la següent disposició disponible de la superposició activa sobre la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Servei d'IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Captura una imatge del contingut actual i tradueix o llegeix en veu alta els textos que hi hagi.Cal haver activat i configurat el servei d'IA."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Latència en el joc en xarxa (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Canvia entre activat/desactivat el comptador de ping de l'habitació de netplay actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Allotjament de Netplay (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Canvia entre activat/desactivat l'allotjament de netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Mode Jugador/Espectador de Netplay (commuta)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Commuta la sessió actual de netplay entre els modes 'espectador' i 'jugador'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Xat de Jugador de Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Envia un missatge de xat a la sessió actual de netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Amaga el xat del joc en xarxa (alternar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Canvia entre els missatges de xat estàtics o esvaïts."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Tipus de dispositiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Específica el tipus de controlador a emular."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Tipus d’analògic a digital"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Fes servir un joystick analògic específic per l'entrada de la creueta digital. 'Forçat' ignoren les entrades analògiques natives del nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Assigna el joystick analògic especificat com a senyal d'entrada de la creueta.\nSi el nucli suporta el control analògic de forma nativa, les assignacions de la creueta s'anul·laran a no ser que s'hagin forçat.\nEn forçar les assingaciona de creuete, el nucli no rebrà senyal de cap entrada analògica del joystick especificat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Índex del dispositiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "El controlador físic tal qual el reconeix RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Dispositiu reservat per aquest jugador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Aquest controlador serà assignat a aquest jugador seguint el mode de reserva."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "Sense reserva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "Preferit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "Reservat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "Tipus de reserva del dispositiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "Preferit: Si el controlador específic està disponible, s'assignarà a aquest jugador. Reservat: No s'assignarà cap controlador a aquest jugador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Port assignat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Especifica quin dels ports del nucli rebrà els senyals d'entrada del port del comandament %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Configura tots els controls"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Assigna totes les direccions i botons, un després de l'altre, en l'ordre en que apareixen en aquest menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Restableix els controls per defecte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Restableix la configuració de les assignacions als valors predeterminats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Desa el perfil de controlador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Desa un fitxer d'autoconfiguració que s'aplicarà automàticament cada cop que es detecti aquest controlador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Índex del ratolí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "El ratolí físic tal qual el reconeix RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Botó B (avall)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Botó Y (esquerra)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Botó Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Botó Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "Creueta amunt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "Creueta avall"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "Creueta esquerra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "Creueta dreta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Botó A (dreta)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Botó X (amunt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "Botó L (lateral)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "Botó R (lateral)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "Botó L2 (gallet)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "Botó R2 (gallet)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "Botó L3 (polzes)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "Botó R3 (polzes)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Analògic esquerre X+ (dreta)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Analògic esquerre X- (esquerra)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Analògic esquerre Y+ (avall)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Analògic esquerre Y- (amunt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Analògic dret X+ (dreta)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Analògic dret X- (esquerra)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Analògic dret Y+ (avall)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Analògic dret Y- (amunt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Gallet de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Recàrrega de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Auxiliar A de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Auxiliar B de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Auxiliar C de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Botó 'Start' de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Botó 'Select' de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Creueta amunt de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Creueta avall de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Creueta esquerra de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Creueta dreta de la pistola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO,
   "Disparador de turbo"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Reducció predictiva de latència no disponible]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "El nucli actual és imcompatible amb la reducció predictiva de latència per mancar de suport de desats ràpids determinístics."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE,
   "Reducció predictiva de latència"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Nombre de fotogrames a la reducció predictiva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Determina el nombre de fotogrames a executar de bestreta. Poden haver fluctuacions de senyal si el nombre de fotogrames endarrerits supera al valor intern del joc."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "Executa lògiques addicionals del nucli per reduir la latència. Una instància executa la lògica fins al següent fotograma i torna a carregar l'estat actual. Una segona instància només per a vídeo per evitar errors amb l'estat de l'àudio. Fotogrames preventius executa els fotogrames anteriors amb accions d'entrada noves quan cal millorar l'eficàcia."
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Executa lògiques addicionals del nucli per reduir la latència. Una instància executa la lògica fins al següent fotograma i torna a carregar l'estat actual. Fotogrames preventius executa els fotogrames anteriors amb accions d'entrada noves quan cal millorar l'eficàcia."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "Mode d'instància única"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "Mode de doble instància"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   "Mode de fotogrames preventius"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Amagar avisos de la reducció predictiva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Amaga el missatge d'avís que es mostra a l'usar la reducció predictiva de latència si el nucli no es compatible amb els desats ràpids."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Nombre de fotogrames preventius"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Determina el nombre de fotogrames a repetir. Poden haver fluctuacions de senyal si el nombre de fotogrames endarrerits supera al valor intern del joc."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Activar context compartit per maquinari"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Dóna un context privat als nuclis renderitzats per maquinari. Això estalvia traginar canvis en l'estat del maquinari entre fotogrames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Permetre als nuclis canviar el controlador de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Permet que els nuclis canviïn el controlador de vídeo a un de diferent al que s'estigui fent servir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Carregar un nucli buit al tancar-ne un altre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Existeixen nuclis amb opció de tancat. Activant aquesta opció, RetroArch carregarà un nucli buit evitant que es tanqui soles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Diversos nuclis tenen la funció pròpia d'apagada. Si desactives aquesta opció, en apagar el nucli, també s'apaga RetroArch. \nAl contrari, en apagar el nucli es carregarà un nucli buit. Així, podràs seguir dins del menú sense tancar RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Iniciar nucli automàticament"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Comprovar si manca el microprogramari (firmware) abans de carregar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Comprova que el microprogramari necessari estigui disponible abans de carregar el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE,
   "Diversos nuclis poden necessitar fitxers de firmware o de bios. Si aquesta opció està activada, RetroArch no permetrà executar el nucli si algun d'aquests fitxers està absent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Categories en les opcions dels nuclis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Permet als nuclis oferir les opcions a submenús agrupats per categories. NOTA: cal tornar a carregar el nucli per a que els canvis facin efecte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Memòria cau d'arxius d'informació de nuclis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Manté una memòria interna local amb informació sobre els nuclis instal·lats. Redueix molt els temps de càrrega en maquinari amb velocitats baixes d'accés al disc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Ometent la informació del nucli sobre desats ràpids"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Especifica si es pot ignorar la informació que té el nucli sobre les seves capacitats per desats ràpids per experimentar amb les seves característiques relacionades (reducció predictiva de latència, rebobinat,...)."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Recarrega sempre el nucli en executar contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Reinicia RetroArch quan s'inicia contingut, encara que el nucli ja s'hagi carregat. Això augmenta l'estabilitat del sistema a canvi d'augmentar el temps de càrrega."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Permet la rotació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Permet que els nuclis puguin girar la pantalla. Si està desactivat, les peticions de girar la pantalla seran ignorades. Útil per configuracions que necessiten girar la pantalla manualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Gestionar nuclis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Executa tasques de manteniment en els nuclis instal·lats (còpia de seguretat, eliminació,...) de forma local i mostra informació dels nuclis."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Gestionar nuclis"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Instal·la o desinstal·la nuclis distribuïts a través de Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Instal·lar nucli"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Desinstal·lar un nucli"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "Mostra 'Administrar nuclis'"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Mostra l’opció «Gestiona els nuclis» al menú principal."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Instal·lant el nucli: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "El nucli serà desinstal·lat quan es tanqui RetroArch."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "El nucli s'està descarregant ara mateix"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Desa la configuració en sortir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "S'han desat els canvis al fitxer de configuració en tancar RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Desa els canvis al fitxer de configuració en sortir. Això és útil pels canvis fets en el menú. Sobreescriu el fitxer de configuració, #include's i comentaris no es desen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Desa els fitxers d'assignació en sortir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Desa els canvis de qualsevol fitxer d'assignació d'entrada actiu en tancar un contingut o en sortir de RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Carrega automàticament les opcions del nucli pel contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Carrega les opcions personalitzades del nucli per defecte en iniciar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Carrega els fitxers de personalització automàticament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Carrega la configuració personalitzada en arrancar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Carrega automàticament fitxers de remapeig"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Carrega els controls personalitzats a l'inici."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Carrega automàticament els fitxers d'índexs de disc inicial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "En executar contingut de diversos dics, es carregarà l'últim disc utilitzat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Carrega els fitxers de shaders automàticament"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Fes servir el fitxer global d'opcions del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Desa tota la configuració dels nuclis en un únic fitxer comú (retroarch-core-options.cfg). Si aquesta opció no està activada, les opcions de cada nucli es desaran en fitxers separats en les carpetes que marqui la configuració de RetroArch."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Endreça els fitxers desats per carpetes amb el nom del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Endreça els fitxers desats en carpetes que continguin el nom del nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Ordena els estats desats en carpetes amb el nom del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Ordena els estats desats en carpetes amb el nom del nucli utilitzat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Endreça els fitxers desats per carpetes amb el nom del contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Endreça els fitxers de desat mitjançant carpetes amb els noms on es troba el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Endreça els estats desats en carpetes amb el nom del contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Endreça els desats ràpids mitjançant carpetes amb el nom on es troba el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "No sobreescriguis la SaveRAM en carregar un desat ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Evita que se sobreescrigui la memòria SaveRAM en carregar un desat ràpid. Pot provocar errors en alguns jocs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Interval entre desats automàtics de la SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Desa automàticament la SaveRAM no volàtil a intervals marcats (en segons)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Desa automàticament la memòria no volàtil SRAM de manera regular. L'interval es mesura en segons. Si assignes un valor de 0, el desat es desactiva."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Interval de punts de control de repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Marca automàticament i a intervals regulars (en segons) l'estat del joc a mesura que s'enregistra una repetició."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Desa automàticament l'estat del joc a intervals regulars durant la reproducció de la repetició. Aquesta opció està desactivada per defecte. L'interval es mesura en mil·lisegons. Un valor de 0 deshabilita el punt de control de la gravació."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_DESERIALIZE,
   "Indica si cal desserialitzar els punts de control emmagatzemats en les repeticions durant la reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_DESERIALIZE,
   "Desserialitza els punts de control de repetició"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_DESERIALIZE,
   "Indica si cal desserialitzar els punts de control emmagatzemats en les repeticions durant la reproducció. La majoria de nuclis han de tenir activada aquesta opció, però alguns poden mostrar comportaments estranys en desserialitzar el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Augmenta automàticament l'índex de l'estat desat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Abans de desar un estat, l'índex augmenta automàticament. En carregar el contingut, l'índex s'assignarà al valor més alt possible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Augmenta automàticament l'índex de repeticions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Abans de fer una repetició, l'índex augmenta automàticament. En carregar el contingut, l'índex s'assignarà al valor més alt possible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Màxim de desats ràpids autoincrementats a mantenir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Limita el nombre de desats ràpids que es crearan quan s’habiliti l’opció «Incrementa l’índex de desat ràpid automàticament». Si se sobrepassa el límit quan es crea un nou desat ràpid s’esborrarà el desat ràpid amb l'índex més baix. Un valor de «0» significa que es crearan desats ràpids sense límit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Nombre màxim de repeticions automàtiques a conservar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Limita el nombre de repeticions que es crearan quan s’habiliti l’opció «Incrementa l’índex de repeticions automàticament». Si se sobrepassa el límit quan es crea una nova repetició s’esborrarà la repetició amb l'índex més baix. Un valor de «0» significa que es crearan repeticions sense límit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Desat ràpid automàtic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Desa un estat automàticament quan es tanca el contingut. L'estat desat es carrega en carregar el contingut si l'opció 'Carrega l'estat automàticament' està activada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Carrega estat automàticament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Carrega automàticament el desat ràpid automàtic a l’inici."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Miniatures de desats ràpids"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Mostra miniatures dels estats desats al menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Compressió de SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Escriu els fitxers de SaveRAM no volàtil en un format d’arxiu comprimit. Redueix dràsticament la mida de fitxer a canvi d’incrementar (de forma insignificant) els temps de desar i carregar.\nNomés s’aplica als nuclis que permeten desar mitjançant la interfície estàndard de libretro per a SaveRAM."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Compressió dels estats desats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Escriu els fitxers d’estat desat en un format d’arxiu comprimit. Redueix dràsticament la mida de fitxer a canvi d’incrementar els temps de desar i carregar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Organitza les captures de pantalla en carpetes per directori de contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Organitza les captures de pantalla en carpetes que prenen el nom del directori on hi ha el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Desa les partides al directori del contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Fes servir la carpeta de continguts com a carpeta per desar fitxers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Desa els estats al directori del contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Fes servir la carpeta de contingut com a carpeta d'estats desats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Els fitxers de sistema són al directori del contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Fes servir la carpeta de contingut com a carpeta pel Sistema/BIOS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Desa les captures al directori del contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Fes servir la carpeta de contingut com a carpeta de captures de pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Desa un registre de temps d’execució (per nucli)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Manté un seguiment de quanta estona s’ha executat cada contingut, amb registres separats per nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Desa un registre de temps d’execució (agregat)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Manté un seguiment de quanta estona s’ha executat cada contingut, enregistrat com l’agregat total entre tots els nuclis."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Nivell de detall dels registres"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Enregistra els esdeveniments a un terminal o fitxer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Nivell de registre del front"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Estableix el nivell de registre pel front. Si un nivell de registre emès pel front es troba per sota d’aquest valor, s’ignorarà."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Nivell de registre del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Estableix el nivell de registre pels nuclis. Si un nivell de registre emès per un nucli es troba per sota d’aquest valor, s’ignorarà."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Estableix el nivell de registre dels nuclis libretro (GET_LOG_INTERFACE). Si el nivell de registre d'un nucli libretro es troba per sota del nivell indicat a libretro_log, s'ignorarà. Els registres DEBUG són sempre ignorats a no ser que estigui activat el mode de verbositat (--verbose).\nDEBUG = 0\nINFO  = 1\nWARN  = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (Depurador)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (Informació)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Avís)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Desa el registre en un fitxer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Redirigeix els missatges de registre d’esdeveniments del sistema a un fitxer. Requereix que l’opció «Nivell de detall dels registres» estigui activada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Afegeix marques de temps als fitxers de registre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Quan es desi el registre en un fitxer, redirigeix la sortida de cada sessió de RetroArch a un fitxer nou amb marca de temps. Si es desactiva, se sobreescriu el registre cada cop que es reinicia el RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Comptadors de rendiment"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Comptadors de rendiment pel RetroArch i els nuclis. Les dades dels comptadors poden ajudar a determinar colls d’ampolla del sistema i afinar el rendiment."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Mostra fitxers i carpetes ocults"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Mostra fitxers ocults i carpetes ocultes en l'explorador de fitxers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtrar extensions desconegudes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Mostra només fitxers que tinguin extensions conegudes a l'explorador de fitxers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Filtra pel nucli actual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "Filtra els fitxers que es mostren a l'explorador de fitxers pel nucli actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Recorda l'última carpeta on has desat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Obre l'explorador de fitxers a l'última localització que s'ha fet servir quan es carrega contingut des de la carpeta inicial. Nota: La localització torna al seu valor predeterminat en reiniciar RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SUGGEST_ALWAYS,
   "Suggereix nuclis sempre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS,
   "Suggereix tots els nuclis disponibles quan es carrega un nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Fes servir el reproductor integrat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Fes servir el visualitzador d’imatges integrat"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Rebobina"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Canvia la configuració del rebobinat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Comptador de duració dels fotogrames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Canvia les opcions del comptador de duració de fotogrames.\nNomés en funcionament quan estigui desactivat el vídeo multifil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Taxa d'avançament ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "La velocitat màxima a la qual s'executa el contingut en un avançament ràpid (per exemple, 5.0x per un contingut a 60 fps serà 300 fps). Si el valor 0, l'avançament ràpid anirà a la màxima velocitat possible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "Indica la velocitat màxima a la qual s'executarà el contingut durant l'avançament ràpid (per exemple, 5.0 per un contingut a 60 fps => 300 fps).\nRetroArch passarà a segon pla per assegurar que no se supera la velocitat màxima. Aquest límit no és completament precís."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Omet fotogrames en l'avançament ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Omet fotogrames segons la velocitat d'avançament ràpid. Estalvia energia i permet l'ús de limitadors de fotogrames externs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Velocitat de càmera lenta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "Ajusta la velocitat a la qual es reprodueix el contingut si es fa servir càmera lenta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Limita la velocitat de fotogrames al menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "S’assegura que la velocitat de fotogrames dins del menú està limitada."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Suporta rebobinat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Torna a un punt anterior de la teva partida més recent. Afecta molt al rendiment de la partida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Rebobinar fotogrames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "El nombre de fotogrames a rebobinar en cada pas. Valors alts augmenten la velocitat de rebobinat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Mida de la memòria intermèdia del rebobinat (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "La quantitat de memòria (en MB) reservada per la memòria de rebobinat. Augmentar aquest valor, augmentarà la quantitat de rebobinat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Mida de la memòria intermèdia dels passos de rebobinat (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Cada vegada que s'augmenta o disminueix el valor de la mida de la memòria del rebobinat, canviarà en funció d'aquesta quantitat."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Reiniciar després d'utilitzar l'avançament ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Reinicia el comptador de duració de fotogrames després d'un avançament ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Reiniciar després de carregar un desat ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Reinicia el comptador de duració de fotogrames després de carregar un estat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Reiniciar després de generar un desat ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Reinicia el comptador de duració de fotogrames després de desar un estat."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Qualitat de l'enregistrament"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Baix"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "Mig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Alt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Sense pèrdues"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM (ràpida)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "Alta qualitat WebM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Configuració personalitzada de gravació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Fils per a l'enregistrament"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Grava amb els filtres aplicats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Captura la imatge després d'aplicar els filtres (però no els shaders). El vídeo es veurà tan elegant com el veieu a la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Fes servir l'enregistrament per GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Enregistra la sortida de la GPU amb shaders, si està disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Mode de retransmissió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Qualitat de la retransmissió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Baix"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "Mig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Alt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Configuració personalitzada de retransmissió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Emetre títol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "URL de transmissió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "Port de transmissió UDP"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Superposició en Pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Ajustant els marcs i els controls en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Disposició del vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Ajusta la disposició de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Notificacions en pantall"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Configura les notificacions en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Visibilitat de les notificacions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Canvia la visibilitat de determinades notificacions."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Superposició de la Visualització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Les superposicions s'utilitzen pels controls i contorns sobre la pantalla."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Mostra la Superposició darrere el Menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Mostra la superposició darrere en lloc de sobre el menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Amaga la Superposició al Menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Amaga la superposició mentre siguem dins el menú, i mosta'l de nou en sortir del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Ocultar superposició al connectar un comandament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Amaga la superposició quan un controlador físic en connecti al port 1, i el mostra de nou quan es desconnecta el controlador."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Amaga la superposició quan un controlador físic en connecti al port 1. No es restaurarà automàticament la superposició quan es desconnecti el controlador."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Mostra les Entrades a la Superposició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Mostra a la superposició en pantalla les entrades registrades. 'Tocat' destaca els elements de la supersposició que es polsen/cliquen. 'Físic (controlador)' destaca l'entrada real passada als nuclis, habitualment des d'un teclat/controlador connectat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Tàctil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Físic (Controlador)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Mostra les tecles polsades del port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Selecciona el port del dispositiu d'entrada a monitorar quan s'estableix 'Mostra les Entrades a la Superposició' a 'Físic (controlador)'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Mostra el Cursor amb la Superposició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Mostra el cursor del ratolí mentre s'utilitzi una superposició en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Rota automàticament la Superposició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Si està suportat per la superposició actual, es rotarà automàticament la disposició per coincidir la relació d'aspecte amb l'orientació de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Escala automàticament la Superposició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Ajusta automàticament l'escala de la superposició i de l'espaiat dels elements de la IU per coincidir la relació d'aspecte amb l'orientació de la pantalla. Produeix els millors resultats amb superposicions de controladors."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Sensibilitat diagonal de la creueta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Ajusta la mida de les zones diagonals. Per tenir una simetría en les 8 direccions, selecciona 100%."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Sensibilitat de la sobreposició ABXY"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Ajusta la mida de les zones superposades en el bloc dels botons d'acció. Per obtenir una simetria en les 8 direccions, selecciona el 100%."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Zona de centrat dels gatells analògics"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "El senyal d'entrada dels joysticks analògics serà relativa a la posició del primer clic si es fa dins d'aquesta zona."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Superposició"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Carrega Automàticament la Superposició Preferida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "Es dona preferència a la càrrega de superposicions segons el nom del sistema en comptes de fer servir la configuració predeterminada. Aquesta opció no es tindrà en compte si s'ha establert una personalització per les superposicions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Opacitat de la Superposició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Opacitat de tots els elements de la IU de la superposició."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Superposició predefinida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Selecciona una superposició en l'explorador de fitxers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "Escala de superposició (mode horitzontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Dimensiona els elements de la interfície d'usuari quan es fa servir una orientació horitzontal de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Relació d'aspecte de la superposició (mode horitzontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Aplica un factor de correcció de la relació d’aspecte a la superposició quan la pantalla està orientada horitzontalment. Els valors positius augmenten l’amplada efectiva de la superposició mentre que els negatius la redueixen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Separació horitzontal de superposició (mode horitzontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Si la configuració actual ho permet, ajusta l'espai entre els elements de la interfície de les meitats esquerra i dreta d'una superposició en fer servir l'a orientació horitzontal de pantalla. Els valors positius augmenten la separació entre les dues meitats i els valors negatius redueixen la separació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Separació vertical de superposició (mode horitzontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Si la configuració actual ho permet, ajusta l'espai entre els elements de la interfície de les meitats superior i inferior d'una superposició en fer servir l'a orientació horitzontal de pantalla. Els valors positius augmenten la separació entre les dues meitats i els valors negatius redueixen la separació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "Desplaçament X de superposició (mode horitzontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Desplaça la superposició en l'eix horitzontal en fer servir una orientació de pantalla horitzontal. Els valors positius desplacen la superposició cap a la dreta i els valors negatius, cap a l'esquerre."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Desplaçament Y de superposició (mode horitzontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Desplaça la superposició en l'eix vertical en fer servir una orientació de pantalla horitzontal. Els valors positius desplacen la superposició cap a dalt i els valors negatius, cap a baix."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "Escala de la superposició (mode vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Dimensiona els elements de la interfície d'usuari quan es fa servir una orientació vertical de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Relació d'aspecte de superposició (Mode vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Aplica un factor de correcció de la relació d’aspecte a la superposició quan la pantalla està orientada verticalment. Els valors positius augmenten l’alçada efectiva de la superposició mentre que els negatius la redueixen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "Separació vertical de superposició (mode vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Si la configuració actual ho permet, ajusta l'espai entre els elements de la interfície de les meitats esquerra i dreta d'una superposició en fer servir l'a orientació vertical de pantalla. Els valors positius augmenten la separació entre les dues meitats i els valors negatius redueixen la separació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Separació vertical de superposició (mode vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Si la configuració actual ho permet, ajusta l'espai entre els elements de la interfície de les meitats superior i inferior d'una superposició en fer servir l'a orientació vertical de pantalla. Els valors positius augmenten la separació entre les dues meitats i els valors negatius redueixen la separació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "Desplaçament X de superposició (mode vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Desplaça la superposició en l'eix horitzontal en fer servir una orientació de pantalla vertical. Els valors positius desplacen la superposició cap a la dreta i els valors negatius, cap a l'esquerre."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "Desplaçament Y de superposició (mode vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Desplaça la superposició en l'eix vertical en fer servir una orientació de pantalla vertical. Els valors positius desplacen la superposició cap a dalt i els valors negatius, cap a baix."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "Superposició de teclat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "Selecciona i ajusta una superposició de teclat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "Activa la superposició per lightgun, ratolí i punter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "Fes servir tots els tocs tàctils que no cliquin els controls per crear accions d'entrada dels dispositius d'entrada del nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "Superposa la pistola de llum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Configura l'entrada de la pistola de llum enviada a través de la superposició."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Superposa el ratolí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Configura el senyal d'entrada del ratolí que es transmeten a la superposició. Nota: 1-, 2-, i 3- tocs de dit es transmeten com clic esquerre, dret i mig del ratolí."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "Ajustament de la superposició del teclat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Selecciona una superposició de teclat en l'explorador de fitxers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Escala automàticament la posició del teclat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Ajusta la superposició de teclat a la relació d'aspecte original. Desactiva aquesta opció per estirar el teclat per tota la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Opacitat del teclat superposat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Opacitat dels elements de la interfície de la superposició de teclat."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Port de la pistola de llum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Selecciona el port del nucli que rebrà les accions d'entrada de la pistola de llum."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Qualsevol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Dispara en tocar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Envia un senyal del joystick en conjunt a la del punter."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Retard en el tret (fotogrames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Estableix un retard en el senyal d'entrada per donar temps al desplaçament del ratolí. Aquest retard també serveix per comptar correctament els tocs tàctils amb diversos dits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Entrada de 2 dits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Escull l'entrada que s'enviarà si hi ha dos punters en pantalla. El retard del gatell ha de tenir un valor diferent de 0 per poder separar els dos senyals."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Entrada de 3 dits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Escull l'entrada que s'enviarà si hi ha tres punters en pantalla. El retard del gatell ha de tenir un valor diferent de 0 per poder separar els dos senyals."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Entrada de 4 dits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Escull l'entrada que s'enviarà si hi ha quatre punters en pantalla. El retard del gatell ha de tenir un valor diferent de 0 per poder separar els dos senyals."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Permet apuntar fora de la pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Permet apuntar fora dels límits de la pantalla. Desactiva aquesta opció per limitar l'apuntat a la vora de la pantalla."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Velocitat del ratolí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Ajusta la velocitat del ratolí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Mantén polsat per arrossegar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Fes un clic llarg a la pantalla per mantenir clicat el botó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Llindar de la pulsació llarga (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Ajusta el temps necessari per considerar una pulsació llarga."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Doble toc per arrossegar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Fes doble clic a la pantalla per començar a mantenir clicat un botó amb el segon clic. Afegirà un retard als clics del ratolí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Llindar de la doble pulsació (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Ajusta el marge de temps entre clics pel qual es detectarà un doble clic."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Llindar de lliscament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Ajusta el rang de moviment permès a l'hora de detectar una pulsació llarga o un toc. Es representa com un percentatge de la dimensió més petita de la pantalla."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Activa la disposició de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Les disposicions de vídeo es fan servir per posar marcs i altres imatges."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Ruta de les disposicions del vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Selecciona una disposició de vídeo des de l'explorador de fitxers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Seleccionar la vista"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Selecciona una vista de la disposició carregada."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Notificacions en pantall"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Mostra missatges en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Ginys gràfics"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Utilitza animacions, notificacions, indicacions i controls decoratius."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Escalar widgets gràfics automàticament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Canvia automàticament la mida de les notificacions decorades, indicadors i controls segons l’escala actual del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Personalitza l'escala dels widgets gràfics (pantalla completa)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Força un factor d’escalat manual quan es dibuixen ginys de visualització en mode de pantalla completa. Només s’aplica quan l’opció «Escala automàticament els ginys gràfics» està desactivada. Es pot fer servir per augmentar o disminuir la mida de les notificacions decorades, indicadors i controls independentment del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Personalitza l'escala dels widgets gràfics (finestra)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Força un factor d’escalat manual quan es dibuixen ginys de visualització en mode de finestra. Només s’aplica quan l’opció «Escala automàticament els ginys gràfics» està desactivada. Es pot fer servir per augmentar o disminuir la mida de les notificacions decorades, indicadors i controls independentment del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Mostra la velocitat de fotogrames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Mostra el nombre actual de fotogrames per segon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Interval d'actualització de fotogrames (en fps)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "El comptador de fotogrames en pantalla s'actualitzarà a la velocitat assignada en fotogrames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Mostra el comptador de fotogrames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Mostra en pantalla el comptador de fotogrames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Mostra les estadístiques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Mostrar les estadístiques tècniques en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Mostrar l'ús de la memòria "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Mostra la quantitat de memòria que es fa servir i la memòria total del sistema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Interval d'actualització de l'ús de memòria (en fotogrames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "El comptador de memòria en pantalla s'actualitzarà a la velocitat assignada a fotogrames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Mostra el ping del joc en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Mostra el ping de la sala de joc en línia actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Notificació d'inici en carregar contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Mostra una breu animació per indicar que s'ha iniciat un contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Notificacions de connexions d'entrada (configuració automàtica)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Notificacions dels errors d'entrada (autoconfiguració)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Notificacions dels trucs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Mostra un missatge en pantalla en aplicar trucs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Notificacions de correccions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Mostra un missatge en pantalla al corregir ROM al vol."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Mostra un missatge en pantalla quan es connectin o desconnectin dispositiu d'entrada."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Mostra un missatge en pantalla quan el dispositiu d'entrda no es pot configurar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Notificacions de la càrrega de reassignacions d'entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Mostra un missatge en pantalla en carregar fitxers d'assignació d'entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Notificacions de càrrega de configuracions personalitzades"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Mostra un missatge en pantalla en carregar fitxers de configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Notificacions en restaurar un disc d'inci"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Mostra un missatge en pantalla quan restableix automàticament a l’arrencada el darrer disc usat d’un contingut multidisc carregat mitjançant llistes M3U."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Notificacions de control de disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Mostra un missatge en pantalla en inserir o extreure discs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Notificacions dels estat desats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "Mostra un missatge en pantalla en desar i carregar desats ràpids."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Notificacions d’avanç ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Mostra un indicador en pantalla quan s’avança ràpidament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Notificacions de captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Mostra un missatge en pantalla quan es fa una captura."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Durada de la notificació de captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Defineix la durada del missatge en pantalla de captura."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Ràpida"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Molt ràpida"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Instantània"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Efecte de flaix de captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Mostra un efecte de flaix blanc en pantalla amb la durada desitjada quan es fa una captura."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "Activat (normal)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "Activat (ràpid)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Notificacions de la freqüència d’actualització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Mostra un missatge en pantalla quan s’estableix la freqüència d’actualització."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Notificacions addicionals del joc en xarxa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Mostra en pantalla missatges no essencials del joc en xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Notificacions només al menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Mostra notificacions només quan el menú és obert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Tipus de lletra de les notificacions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Selecciona el tipus de lletra per les notificacions en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Mida de les notificacions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Especifica la mida de la tipografia en punts. Si es fan servir els widgets, la mida només s'aplicarà a les estadístiques en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Posició de les notificacions (horitzontal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Especifica una posició x personalitzada del text en pantalla respecte a l'eix X. 0 és la vora esquerra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Posició de les notificacions (vertical)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Especifica una posició personalitzada del text en pantalla respecte a l'eix Y. 0 és la vora inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Color de les notificacions (vermell)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Ajusta el valor del vermell del color del text en els missatges en pantalla. Els valors vàlids són de 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Color de les notificacions (verd)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Ajusta el valor del verd del color del text en els missatges en pantalla. Els valors vàlids són de 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Color de les notificacions (blau)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Ajusta el valor del blau del color del text en els missatges en pantalla. Els valors vàlids són de 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Fons de les notificacions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Activa un color de fons pels missatges en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Color de fons de les notificacions (vermell)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Ajusta el valor del vermell del fons dels missatges en pantalla. Els valors vàlids són de 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Color de fons de les notificacions (verd)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Ajusta el valor del verd del fons dels missatges en pantalla. Els valors vàlids són de 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Color de fons de les notificacions (blau)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Ajusta el valor del blau del fons dels missatges en pantalla. Els valors vàlids són de 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Opacitat del fons de les notificacions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Ajusta l'opacitat del fons dels missatges en pantalla. Els valors vàlids estan entre 0,0 i 1,0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Aparença"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Canvia les opcions de l'aparença del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Visibilitat dels elements del menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Commuta la visibilitat dels elements del menú al RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Posa en pausa el contingut quan el menú estigui actiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Pausa el contingut si el menú està actiu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Posa en pausa el contingut quan no estigui actiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Posa en pausa el contingut quan el RetroArch no sigui la finestra activa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Surt en tancar el contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Tanca RetroArch automàticament en tancar contingut. 'CLI' tanca només si el contingut es carrega per la línia d'ordres."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Reprendre contingut després d'utilitzar un desat ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Tanca automàticament el menú i reprèn el contingut després de desar o carregar un estat. Deshabilitar aquesta opció pot millorar el rendiment en maquinari molt lent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Reprendre contingut al canviar de disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Tanca el menú i reprèn el contingut automàticament després d'inserir o carregar un nou disc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Navegació en bucle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Quan s'arribi al final horitzontal o vertical d'una llista, es tornarà a l'altre extrem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Mostra opcions avançades"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Mostra les opcions avançades per a usuaris experts."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Mode Quiosc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Protegeix la configuració amagant la configuració relacionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Estableix una contrasenya per desactivar el mode quiosc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Estableix una contrasenya per mostrar el mode quiosc, permetent deshabilitar-la des del menú principal seleccionant 'Deshabilitar el mode quiosc' i introduint la contrasenya."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Compatibilitat amb ratolí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Permet controlar el menú amb un ratolí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Compatibilitat amb pantalla tàctil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Permet controlar el menú amb una pantalla tàctil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Tasques multifil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Executa tasques en un fil a part."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Temps d'espera de l'estalvi de pantalla del menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Si un menú està actiu, es mostrarà un estalvi de pantalla si hi ha un temps d'inactivitat especificat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Animació de l'estalvi de pantalla del menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Habilita un efecte d’animació mentre l’estalvi de pantalla del menú està actiu. Afecta moderadament el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Neu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Estrelles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX,
   "Vòrtex"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Velocitat d’animació de l’estalvi de pantalla del menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Ajusta la velocitat de l’efecte d’animació de l’estalvi de pantalla del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Desactiva la composició d’escriptori"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Els gestors de finestres usen la composició per aplicar efectes visuals i detectar finestres que no responen, entre altres coses."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Força la desactivació de la composició. Només funciona per Windows Vista o Windows 7."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Executa la interfície d'usuari en iniciar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Inicia el controlador de l'assistent de la interfície en iniciar (si està disponible)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Menú d'escriptori (Es requereix reinici)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Obre el menú d’escriptori a l’inici"
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "Aparença de la pantalla inferior de 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Canvia l'ajustament de l'aparença de la pantalla inferior."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "Icona de l'Aplicació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Canvia la icona de l'aplicació."
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Menú Ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Canvia la visibilitat dels elements al menú ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Opcions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Canvia la visibilitat dels elements al menú d'Opcions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Mostra “Carrega el nucli”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Mostra l’opció “Carrega el nucli” al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Mostra “Carrega el contingut”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Mostra l’opció “Carrega el contingut” al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Mostra “Carrega el disc”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Mostra l’opció “Carrega el disc” al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Mostra “Bolca el disc”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Mostra l’opció “Bolca el disc” al menú principal."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "Mostra “Expulsa el disc”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Mostra l’opció “Expulsa el disc” al menú principal."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Mostra “Actualitzador en línia”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Mostra l’opció “Actualitzador en línia” al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Mostra “Baixador de nuclis”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Mostra la capacitat d’actualitzar nuclis (i fitxers d’informació de nuclis) a l’opció “Baixador de nuclis”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Mostra l’“Actualitzador de miniatures” antic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Mostra l’entrada per baixar paquets de miniatures antics a l’opció “Actualitzador en línia”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Mostra «Informació»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Mostra l’opció «Informació» al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Mostra «Fitxer de configuració»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Mostra l’opció «Fitxer de configuració» al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Mostra «Ajuda»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Mostra l’opció «Ajuda» al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Mostra «Surt del RetroArch»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Mostra l’opció «Surt del RetroArch» al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Mostra «Reinicia el RetroArch»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Mostra l’opció «Reinicia el RetroArch» al menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Mostra 'Opcions'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Mostra el menú «Configuració»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Establir contrasenya per activar la secció 'Opcions'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Estableix una contrasenya per ocultar la pestanya 'Opcions', permetent restaurar-la des del menú principal seleccionant 'Mostrar la pestanya d'Opcions' i introduint la contrasenya."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Mostra «Favorits»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Mostra el menú «Preferits»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES_FIRST,
   "Mostra primer els preferits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES_FIRST,
   "Mostra el menú «Preferits» abans que «Historial»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Mostrar Imatges"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Mostra el menú «Imatges»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Mostrar 'Música'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Mostra el menú «Música»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Mostra «Vídeos»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Mostra el menú «Vídeos»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Mostra «Joc en línia»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Mostra el menú «Joc en xarxa»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Mostra «Historial»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Mostra el menú amb l'historial d'ítems recents."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Mostra «Importa contingut»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Mostra l'entrada «Importa contingut» dins del menú principal o les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Menú principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Menú de llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Mostra «Llistes de reproducció»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Mostra les llistes de reproducció en el menú principal. S'ignorarà aquesta opció a GLUI si s'activen les seccions de la llista de reproducció i de la barra de navegació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "Mostra les pestanyes de les llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "Mostra les seccions de les llistes de reproducció. No afecta RGUI. Cal activar la barra de navegació a GLUI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Mostra «Explora»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Mostra l'opció d'explorador de continguts."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Mostra «Nuclis sense continguts»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Especifica el tipus de nucli (si n'hi ha cap) per mostrar al menú «Nuclis sense continguts». En establir-ho com a «Personalitzat», la visibilitat individual dels nuclis es pot canviar mitjançant el menú «Gestiona els nuclis»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Tots"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Un sol ús"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Mostra la data i l’hora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Mostra la data i/o l’hora actuals al menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Estil de la data i l’hora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Canvia l’estil amb què es mostren la data i/o l’hora actuals al menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Separador de dates"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Especifica el caràcter a usar com a separador entre els components any, mes i dia quan es mostra la data actual al menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Mostra el nivell de la bateria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Mostra el nivell actual de la bateria al menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Mostra el nom del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Mostra el nom del nucli actual al menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Mostra subetiquetes al menú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Mostra informació addicional pels elements del menú."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Mostra la pantalla inicial"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Mostra la pantalla inicial al menú. Aquesta opció es desactiva automàticament quan el programa s’inicia per primera vegada."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Mostra “Reprèn”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Mostra l’opció de reprendre el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Mostra “Reinicia”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Mostra l’opció de reiniciar el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Mostra “Tanca el contingut”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Mostra l’opció “Tanca el contingut”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Mostra el submenú de desat ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Mostra les opcions de desat ràpid en un submenú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Mostra “Desa/Carrega l’estat”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Mostra les opcions per desar o carregar estats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "Mostra els controls de repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "Mostra les opcions per enregistrar o reproduir fitxers de repetició."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Mostra “Desfés el desament/càrrega d’estat”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Mostra les opcions per desfer el desament o càrrega d’estat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Mostra «Opcions del nucli»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Mostra l’opció «Opcions del nucli»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Mostra «Escriu les opcions al disc»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Mostra l’entrada «Escriu les opcions al disc» al menú «Opcions > Gestiona les opcions del nucli»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Mostra «Controls»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Mostra l’opció «Controls»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Mostra “Fes una captura de pantalla”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Mostra l’opció “Fes una captura de pantalla”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Mostra “Inicia l’enregistrament”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Mostra l’opció “Inicia l’enregistrament”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Mostra «Inicia la transmissió»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Mostra l’opció «Inicia la transmissió»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Mostra «Superposició en pantalla»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Mostra l’opció «Superposició en pantalla»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Mostra «Disposició del vídeo»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Mostra l’opció «Disposició del vídeo»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Mostra «Latència»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Mostra l’opció «Latència»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Mostra «Rebobina»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Mostra l'opció 'Rebobinar'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Mostra «Desa els nuclis forçats»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Mostra l'opció 'Desar personalitzacions del nucli' dins del menú Personalitzacions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Mostra l'opció 'Desar la personalització de carpetes de continguts'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Mostra l'opció 'Desa la personalització de les carpetes de contingut' dins del menú Personalitzacions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Mostra desar les personalitzacions del joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Mostra l'opció 'Desar personalitzacions del nucli' dins del menú Personalitzacions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Mostrar trucs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Mostra l'opció 'Trucs'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Mostrar Shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Mostra l’opció de «Shaders»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Mostra «Afegeix als favorits»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Mostra l’opció «Afegeix als favorits»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Mostra afegir a una llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Mostra l'opció 'Afegir a una llista de reproducció'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Mostra «Estableix l’associació de nucli»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Mostra l’opció «Estableix l’associació de nucli» quan no s’executa el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Mostra «Restableix l’associació de nucli»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Mostra l’opció «Restableix l’associació de nucli» quan no s’executa el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Mostra «Baixa les miniatures»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Mostra l’opció «Baixa les miniatures» quan no s’executa el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Mostra «Informació»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Mostra l’opció «Informació»."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Mostra «Controladors»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Mostra les opcions de controladors."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Mostra «Vídeo»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Mostra les opcions de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Mostra «Àudio»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Mostra les opcions d’«Àudio»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Mostra «Entrada»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Mostra les opcions d’«Entrada»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Mostra «Latència»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Mostra les opcions de «Latència»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Mostra «Nucli»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Mostra les opcions del «Nucli»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Mostra «Configuració»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Mostra les opcions de «Configuració»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Mostra «Desament»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Mostra les opcions de «Desament»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Mostra «Registre»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Mostra les opcions de «Registre»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Mostra «Navegador de fitxers»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Mostra les opcions de «Navegador de fitxers»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Mostra 'Regulació de fotogrames'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Mostra la configuració de 'Regulació de fotogrames'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Mostra «Enregistrament»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Mostra la configuració d'enregistrament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Mostra 'Presentació en pantalla'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Mostra l'opció 'Visualització en pantalla'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Mostrar 'Interfície d'usuari'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Mostra la configuració 'Interfície d'usuari'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Mostra «Servei d'IA»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Mostra la configuració de 'Servei AI'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Mostra 'Accesibilitat'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Mostra la configuració d'accessibilitat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Mostra 'Administració d'energia'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Mostra la configuració de 'Gestió energia'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Mostra 'Assoliments'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Mostra la configuració dels assoliments."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Mostrar Xarxa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Mostra la configuració de xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Mostra «Llistes de reproducció»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Mostra la configuració de les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Mostrar 'Usuari'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Mostra 'Configuració d'usuari'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Mostra «Directori»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Mostra la configuració de les carpetes."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "Mostrar 'Steam'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "Mostra la configuració de Steam."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Factor d'escala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Escala la mida dels elements de la interfície d'usuari en el menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Imatge de fons"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Escull una imatge com a fons del menú. Les imatges manuals i dinàmiques tindran preferència sobre el tema de colors."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Opacitat del fons"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Modifica l'opacitat de la imatge de fons."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Opacitat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Modifica l'opacitat de fons del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Fes servir el tema de colors del sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Fes servir el tema de color del sistema operatiu (si n'hi ha). S'ignoraran la configuració del tema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Miniatura principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Tipus de miniatures que es mostrarà."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Llindar d'escalat de miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Escala automàticament les imatges amb una amplada/alçada més petita que el valor especificat. Millora la qualitat de la imatge. Té un efecte moderat en el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Fons de les miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Permet cobrir l'espai no utilitzat en les miniatures amb fons pla. Això garanteix que totes les imatges tinguin una mida uniforme, millorant visualment els menús en veure miniatures de continguts i mides diferents."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Animació de text en moviment"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Selecciona el mètode desplaçament horitzontal pels texts del menú que siguin molt llargs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Velocitat dels texts en moviment"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "La velocitat de l'animació del text en moure's en el menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Suavitza el textos en moviment"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Mostra el text dels menús, que siguin molt llargs, desplaçant-lo amb una animació suau. Afecta lleugerament al rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Recordar selecció al canviar entre seccions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Recorda la posició anterior del ratolí a les pestanyes. RGUI no té pestanyes però les llistes de reproducció i la configuració tindran el mateix comportament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Sempre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Només per les llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Només pel menú principal i la configuració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_STARTUP_PAGE,
   "Pàgina d'inici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_STARTUP_PAGE,
   "Pàgina d'inici del menú en iniciar."
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Sortida del servei d’IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Mostra les traduccions com una imatge superposada (Mode Imatge), àudio directe (Veu) o utilitza una veu del sistema, com NVDA (Narració)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "URL del servei d’IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Un URL http:// apuntant al servei de traducció que s’usarà."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Servei d’IA habilitat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Habilita el servei d’IA perquè s’executi quan es premi la tecla de drecera del servei d’IA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Pausa durant la traducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Posa en pausa el nucli mentre es tradueix la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Llengua original"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "La llengua des de la qual traduirà el servei. Si és «Per defecte» s’intentarà detectar automàticament la llengua. Si es tria una llengua específica la traducció serà més acurada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Llengua final"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "La llengua a la qual traduirà el servei. «Per defecte» és anglès."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Activa l’accessibilitat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Activa la conversió de text a parla per ajudar en la navegació pels menús."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Velocitat de la conversió de text a parla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "La velocitat de la veu de la conversió de text a parla."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Assoliments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Guanya assoliments en jocs clàssics. Per a més informació visiteu «https://retroachievements.org»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Mode expert"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Desactiva els trucs, el rebobinat, la càmera lenta i la càrrega de desats ràpids. Els assoliments guanyats en el mode expert es marquen de forma única perquè pugueu mostrar als altres el que heu aconseguit sense ajuda de les funcionalitats especials de l’emulador. Si es commuta aquesta opció durant l’execució es reiniciarà el joc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Taules de classificació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "Presència rica"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Envia periòdicament informació contextual sobre el joc al web de RetroAchievements. No té cap efecte si el «mode expert» està activat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Insígnies dels assoliments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Mostra insígnies a la llista d’assoliments."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Prova assoliments no oficials"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Usa assoliments no oficials i/o característiques en beta amb finalitats de verificació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "So de desbloqueig"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Reprodueix un so quan es desbloqueja un assoliment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Captura de pantalla automàtica"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Fa una captura de pantalla automàticament quan es desbloqueja un assoliment."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Mode bis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Comença la sessió amb tots els assoliments actius (fins i tot els que ja estan desbloquejats)."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Aparença"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Canvia la posició i desplaçaments de les notificacions d’assoliments en pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Posició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Estableix la cantonada/vora de la pantalla des d’on apareixeran les notificacions d’assoliments."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "A dalt a l’esquerra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "A dalt al centre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "A dalt a la dreta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "A baix a l’esquerra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "A baix al centre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "A baix a la dreta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Separació alineada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Estableix si les notificacions d’assoliments s’haurien d’alinear amb altres tipus de notificacions en pantalla. Desactiveu-ho per definir valors de separació/posició manuals."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "Separació horitzontal manual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Distància des de la vora esquerra/dreta de la pantalla, que pot compensar l’overscan de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "Separació vertical manual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Distància des de la vora superior/inferior de la pantalla, que pot compensar l’overscan de la pantalla."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "Visibilitat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "Canvia quins missatges i elements de pantalla es mostraran. Aquesta opció no desactiva la funcionalitat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "Resum d'inici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "Mostra informació sobre el joc que s'està carregat i del progrés de l'usuari.\n'Tots els jocs identificats' mostrarà el resum d'aquells jocs que no tenen assoliments publicats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "Tots els jocs identificats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "Jocs amb assoliments"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "Desbloqueja les notificacions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "Mostra una notificació si un assoliment es desbloqueja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "Notificacions de jocs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "Mostra una notificació quan s'hagin desbloquejat tots els assoliments d'un joc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Activa els indicadors de desafiament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Mostra indicadors en pantalla quan es puguin desbloquejar certs assoliments."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Indicador de progrés"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Mostra un indicador en pantalla quan es facin progressos per aconseguir certs assoliments."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "Missatges d'inici de la taula de classificació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "Mostra una descripció d'una taula de classificació quan s'activi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Missatges enviats a la taula de classificació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Mostra un missatge amb el valor enviat en completar l'intent d'entrar en una taula de classificació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Missatges d'error en la taula de classificació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Mostra un missatge si falla un intent d'entrar en una taula de classificació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Seguiment de la taula de classificació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Mostra informació del seguiment en pantalla amb el valor actualitzat de les taules de classifcació actives."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Missatges d'inici de sessió"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Mostra missatges relacionats amb l'inici de sessió al compte de RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Missatges detallats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Mostra diagnòstic addicional i missatges d'error."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Anuncia el joc en línia públicament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Anuncia les partides de joc en línia públicament. Si no està activat els clients s’han de connectar manualment en comptes d’usar el vestíbul públic."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Fes servir un servidor intermediari"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Redirigeix les connexions de joc en línia a través d'un servidor intermediari. És útil si l’amfitrió es troba darrere d’un tallafoc o té problemes de NAT/UPnP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Ubicació del servidor intermediari"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Escull un servidor intermediari concret. Les localitzacions més properes acostumen a tenir una menor latència."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Direcció del servidor intermediari personalitzat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Aquí pots introduir la direcció del teu servidor intermediari personalitzat. Format: Adreça o adreça/port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "Nord-amèrica (Costa est, USA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "Europa Occidental"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "Sud-amèrica (Sud-est, Brasil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Sud-est Àsia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Direcció del servidor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Indica l'adreça IP del servidor al qual cal connectar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Port TCP per al joc en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Indica el port del servidor a connectar. Pot ser TCP o UDP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Nombre màxim de connexions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Indica el nombre màxim de connexions actives que admet el servidor abans de rebutjar una nova connexió."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Limitació del ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "Indica la latència (ping) màxima que acceptarà el servidor d'altres connexions. Selecciona 0 per desactivar el límit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Contrasenya del servidor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Indica la contrasenya que han de fer servir els clients que es connecten al servidor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Contrasenya del servidor per espectadors"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Indica la contrasenya que han de fer servir els clients que es connecten al sevidor com a espectadors."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Mode espectador del joc en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Comença el joc en línia en mode espectador."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Indica si cal començar una sessió de joc en xarxa en el mode espectador. Si aquesta opció està activada, el joc en xarxa començarà en el mode espectador. Es pot canviar el mode més endavant."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Amagar xat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Fes desaparèixer els missatges del xat en un temps determinat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Color del xat (Sobrenom)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Format: #RRGGBB o RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Color del xat (Missatges)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Format: #RRGGBB o RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Permet la pausa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Permet als jugadors posar la partida en pausa durant el joc en línia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Permet clients en mode esclau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Permet les connexions en el mode esclau. Els clients en mode esclau necessiten poca potència de processament en el seu maquinari, però patiran significativament si hi ha latència a la xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Bloqueja els clients sense el mode esclau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Desactiva les connexions que no estiguin en mode esclau. No es recomana fer-ho excepte en xarxes molt ràpides amb maquinari molt lent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Fotogrames de comprovació de joc en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "La freqüència (en fotogrames) amb la qual es comprovarà que l’amfitrió i el client estan sincronitzats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "Indica la freqüència (en fotogrames) amb el que el servidor del joc en xarxa verificarà la sincronia entre els clients. Aquest valor no tindrà efectes visibles en gran part dels nuclis i pot ser ignorat. En nuclis no deterministes determinarà l'interval de temps per sincronitzar entre clients. En nuclis amb errors, pot produir errors greus en el rendiment si el valor és diferent de 0. Selecciona 0 per no haver de fer comprovacions. Aquest valor només el fa servir el servidor del joc en xa[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Fotogrames de latència d'entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "El nombre de fotogrames de latència d’entrada per al joc en línia que s’usarà per amagar la latència de la xarxa. Redueix la fluctuació de senyal i l’ús de CPU del joc en línia a canvi d’introduir una latència d’entrada considerable."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "El nombre de fotogrames de retard en l'entrada que farà servir el joc en xarxa per amagar la latència de la xarxa.\nAquesta opció retarda l'entrada local en el joc en xarxa perquè el fotograma executat més proper als fotogrames que es rebin de la xarxa, reduint les distorsions visuals i el consum de la CPU, però genera un retard en la entrada visible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Rang de fotogrames de latència d'entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "El rang de fotogrames de latència d’entrada que es poden fer servir per amagar la latència de la xarxa. Redueix la fluctuació de senyal i l’ús de CPU del joc en línia a canvi d’una latència d’entrada imprevisible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "El rang de fotogrames de retard d'entrada que pot utilitzar el joc en xarxa per amagar la latència de la xarxa.\nSi aquesta opció està activada, el joc en xarxa ajustarà dinàmicament el nombre de fotogrames de retard d'entrada per equilibrar l'ús de la CPU; el retard d'entrada i latència de xarxa. Redueix les distorsions visuals i el consum de la CPU, però genera un retard entre les entrades/controladors."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Camí del NAT del joc en xarxa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "En executar un servidor, se cercaran les connexions des d'internet mitjançant UPnP o tecnologies similars amb les quals sortir de les xarxes locals."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Compartir entrada digital"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Sol·licita el dispositiu %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Sol·licitud per jugar amb el dispositiu d'entrada indicat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Ordres de xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Port de comandes de xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "RetroPad de xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Port base de RetroPad a la xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "RetroPad de xarxa de l'usuari %d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "Comandes stdin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Interfície de comandes de l'entrada estàndard."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Descàrrega les miniatures sota demanda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Descarrega automàticament les miniatures que falten quan es navega per les llistes de reproducció. Afecta molt al rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Configuració de l'actualitzador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Accedeix a la configuració d'actualització del nucli"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "URL dels nuclis del Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "Indica l'adreça web URL per actualitzar els nuclis de buildbot de libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Adreça web de recursos de buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "Indica l'adreça web URL per actualitzar els nuclis de buildbot de libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Extreu automàticament els fitxers descarregats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Extreu els continguts dels fitxers després de descarregar-los."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Mostra els nuclis experimentals"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Inclou nuclis experimentals a la llista de nuclis per descarregar. Aquests nuclis solen estar en desenvolupament i solen tenir funcions de testatge, i no es recomanen per l'ús general."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Desar una còpia dels nuclis en actualitzar-los"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Crea automàticament una còpia de seguretat dels nuclis instal·lats quan es faci una actualització en línia. Així es pot recuperar fàcilment un nucli funcional en cas que una actualització inclogui una regressió."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Mida de l'historial de còpies de seguretat del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Especifica la quantitat de còpies de seguretat automàtiques que es desaran per cada nucli instal·lat. En arribar a aquest límit, la següent còpia de seguretat produïda eliminarà la còpia de seguretat més antiga. Les còpies de seguretat manuals no es veuen afectades per aquesta opció."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Historial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Permet tenir llistes de jocs, imatges, música i vídeos utilitzats recentment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Mida del historial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Limita el nombre d'entrades a la llista de reproducció recent per jocs, imatges, música i vídeos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Mida dels favorits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Limita el nombre d'entrades a la llista 'Preferits'. Un cop s'arriba al límit, les noves incorporacions no s'afegiran fins que altres entrades siguin eliminades. Posar un valor de -1 per tenir un valor d'entrades il·limitat.\nAVÍS: Si es disminueix aquest valor, s'eliminaran les entrades existents!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Permet reanomenar els elements"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Permet canviar el nom dels elements de les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Permet eliminar els elements"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Permet eliminar els elements de les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Ordena les llistes de reproducció alfabèticament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Mostra contingut a les llistes de reproducció en ordre alfabètic, excloent les llistes de l'historial, imatges, música i vídeos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Desa les llistes de reproducció en el format antic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Desa les llistes de reproducció en el format de text simple. Si es desactiva aquesta opció, les llistes faran servir el format JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Comprimeix les llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Arxiva les llistes de reproducció i desa-les al disc. Redueix la mida del fitxer i els temps de càrrega a canvi d'augmentar (lleugerament) el consum de CPU. Es poden comprimir llistes amb el format antic o bé amb el nou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Mostra els nuclis associats a les llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Especifica si cal associar elements de la llista de reproducció amb el nucli actual (si existeix).\nAquesta opció s'ignorarà si les subetiquetes a les llistes estan actives."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Mostra subetiquetes de les llistes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Mostra informació addicional per cada entrada de la llista de reproducció, com el nucli associat i el temps de jocs (si està disponible). Té un impacte variable en el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Mostra icones específiques dels continguts a l'historial i als preferits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Mostra una icona específica per cada element de les llistes de reproducció de l'historial i preferits. Afecta el rendiment de forma variable."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Nucli:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Durada:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Darrera partida:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_PLAY_COUNT,
   "Comptador de reproduccions:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "segon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "segons"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "minut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "minuts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "hora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "hores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "dia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "dies"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "setmana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "setmanes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "mes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "mesos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "any"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "anys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "abans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Mostar índexs en les llistres de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Mostra el nombre d'entrades en visualitzar llistes de reproducció. El format depen del controlador del menú seleccionat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Etiqueta de temps de joc a les llistes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Escull quin tipus de registre del temps de joc es mostra a la llista de reproducció. \nCal activar el registre dins de la configuració."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Estil de data i temps de 'Última reproducció'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Escull l'estil de la data i l'hora que es mostren a l'etiqueta 'Última partida'. Les opcions AM/PM tenen un petit impacte en el rendiment en algunes plataformes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Cerca aproximada de fitxers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Quan es fa una cerca en els fitxers comprimits d'una llista de reproducció, només es cercarà la coincidència en el nom del fitxer. Això evita que apareguin duplicats en l'historial."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Cerca sense coincidència de nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Permet cercar contingut i afegir-lo a una llista de reproducció encara que no hi hagi un nucli instal·lat que l’admeti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "Comprova els duplicats mitjançant les sumes de verificació CRC"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "A vegades, els fitxers ISO tenen nombres de sèrie duplicats, sobretot en contingut de PSP/PSN. En dependre únicament del nombre de sèrie, el sistema pot cercar contingut en el sistema erroni. Aquesta opció afegeix una comprovació mitjançant sumes de verificació CRC, això disminueix la velocitat de la cerca però fa que sigui més precisa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Gestiona les llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Du a terme tasques de manteniment a les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Llistes de reproducció portàtils"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Quan s’activa i també se selecciona el directori del «navegador de fitxers», el valor actual del paràmetre «navegador de fitxers» es desa a la llista de reproducció. Quan es carrega la llista en un altre sistema on s’hagi activat la mateixa opció, el valor del paràmetre «navegador de fitxers» es compara amb el valor de la llista de reproducció; si són diferents els camins de les entrades de la llista s’arreglen automàticament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "Fes servir els noms dels fitxers per cercar miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "Si està activat, es cercaran miniatures mitjançant el nom del fitxer de l'element i no en la seva etiqueta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "Permet tots els tipus d'matge per les miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "En activar aquesta opció, es poden afegir miniatures locals amb tots els tipus d'imatge compatibles amb RetroArch (com jpeg). Pot afectar lleugerament al rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "Administrar"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Nucli per defecte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Especifica el nucli que es farà servir per executar contingut que no té associat un nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Reinicia les associacions de nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Eliminar totes les associacions als nuclis existents pels elements de les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Presentació d'etiquetes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Canvia la forma de presentar les etiquetes de contingut per aquesta llista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Mètode d'ordenació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Determina com les entrades s'ordenen en aquesta llista de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Neteja la llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Confirma les associacions a nuclis i elimina els elements invàlids i duplicats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Actualitza la llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Afegeix nou contingut i elimina entrades invàlides amb l'opció 'Escaneig manual'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Esborrar llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Elimina la llista de reproducció del sistema de fitxers."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Privacitat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Canvia la configuració de privacitat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Compte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Administra els comptes d'usuari configurats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Nom d’usuari"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Introdueix aquí el teu nom d'usuari. Aquest nom es farà servir pel joc en xarxa i altres coses."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Llengua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Estableix la llengua de la interfície d’usuari."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Tradueix el menú i tots els missatges en pantalla a l'idioma que has seleccionat. Cal reiniciar per poder aplicar els canvis.\nNota:Al costat de cada idioma, es mostrarà el grau de finalització de cada traducció. Si algun element del menú no està traduït a l'idioma seleccionat, es mostrarà en anglès."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Permetre la càmera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Tots els nuclis tenen accés a la càmera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Presència enriquida de Discord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Permet que l'app Discord mostri més informació sobre el contingut que s'està executant. \nNomés funcionarà amb el client d'escriptori de Discord."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Permet la localització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Permet als nuclis accedir a la teva localització."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Guanya assoliments en jocs clàssics. Per a més informació visiteu «https://retroachievements.org»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Detalls d'inici de sessió pel teu compte de RetroAchievements. Ves a retroachievements.org per aconseguir un compte gratuït.\nDesprés de fer el registre correctament, cal posar el nom d'usuari i la contrasenya a RetroArch."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Nom d’usuari"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Entreu el vostre nom d’usuari del compte de RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Contrasenya"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Entreu la contrasenya del vostre compte de RetroAchievements. Longitud màxima: 255 caràcters."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "Clau de transmissió de Youtube"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Clau de transmissió de Twitch"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Clau de retransmissió de Facebook Gaming"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Sistema/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "Les BIOS, ROM d’arrencada i altres fitxers específics de sistema es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Descàrregues"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Els fitxers baixats es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Recursos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Els recursos de menú usats pel RetroArch es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Fons dinàmics"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Les imatges de fons dels menús es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Les miniatures de l’art de les capses, captures i pantalla de títol es desen en aquest directori."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Directori d'inici"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Selecciona la carpeta inicial de l'explorador de fitxers."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Fitxers de configuració"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "El fitxer de configuració predeterminat s'ha desat en aquesta carpeta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Nuclis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Els nuclis de libretro es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Informació del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Els fitxers d’informació de l’aplicació i els nuclis es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Bases de dades"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Les bases de dades es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Arxius de trucs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "En aquesta carpeta es desaran els fitxers de trucs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Filtres de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Els filtres de vídeo basats en CPU es desen en aquesta carpeta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Filtres d'àudio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Els filtres d'àudio DSP s'emmagatzemen en aquesta carpeta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Shaders de Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Els shaders de vídeo basats en GPU s'emmagatzemen dins aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Enregistraments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Les gravacions es desaran en aquesta carpeta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Configuracions dels enregistraments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Les configuracions d’enregistrament es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Superposicions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Les superposicions es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Superposicions del teclat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "En aquesta carpeta es desen les superposicions de teclat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Disposicions de vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Les disposicions de vídeo es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Captures de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Les captures de pantalla es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Perfils de controlador"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Els perfils de controlador per configurar automàticament els controladors es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Reassignacions d’entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Les reassignacions d’entrada es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Les llistes de reproducció es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Llista de reproducció dels favorits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Desa la llista de reproducció de favorits en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Llista de reproducció de l’historial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Desa la llista de reproducció de l’historial en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Llista de reproducció d’imatges"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Desa la llista de reproducció d’imatges en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Llista de reproducció de música"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Desa la llista de reproducció de música en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Llista de reproducció de vídeos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Desa la llista de reproducció de vídeos en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Registres d’execució"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Els registres d’execució es desen en aquest directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Desar fitxers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Desa tots els estats desats en aquesta carpeta. Si no hi ha una carpeta assignada, es desaran en la carpeta del contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Desa tots els fitxers d'estats desats (*.srm) en aquesta carpeta. Això inclou fitxers relacionats com .rt, .psrm,... Aquesta carpeta serà ignorada si es fa servir la línia d'ordres."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Estats desats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Els estats desats i les repeticions es desaran en aquesta carpeta. Si no hi ha cap carpeta seleccionada, s'intentarà desar en la mateixa carpeta on es troba el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Memòria cau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "En aquesta carpeta es desen temporalment els continguts dels fitxers comprimits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Registres d'esdeveniments del sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "El registre d'esdeveniments del sistema es desen en aquesta carpeta."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Activa Rich Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Comparteix el teu estat actual dins de RetroArch a través de Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Format de continguts de presència enriquida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Decideix quina informació dels continguts es compartirà."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Contingut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Nom del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Nom del sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Contingut (Nom del sistema)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Contingut (Nom del nucli)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Contingut (Nom del sistema - Nom del nucli)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Afegir a la mescla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Afegeix aquesta pista d'àudio a un espai de seqüències d'àudio.\nSi no hi ha espai disponible, s'ignorarà."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Afegir a la mescla i reproduir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Afegeix aquesta pista d'àudio a un espai de seqüències d'àudio i reprodueix-la.\nSi no hi ha espai disponible, s'ignorarà."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Amfitrió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Connecta amb l'amfitrió del joc en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Introduïu l’adreça del servidor de joc en línia i connecteu en mode client."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Desconnecta de l'amfitrió del joc en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Desconnecta una sessió activa del joc en xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Filtres de vestíbul"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Mostra només les sales disponibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Només pels nuclis instal·lats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Sales amb contrasenya"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Actualitza la llista d’amfitrions de joc en línia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Cerca amfitrions de joc en línia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Actualitza la llista d’amfitrions de joc en línia a la xarxa local"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Cerca amfitrions de joc en línia a la xarxa local."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Inicia un servidor de joc en xarxa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Inicia el joc en xarxa en el mode servirdor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Atura el servidor de joc en xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "Expulsar client"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Expulsa al client de la teva sala."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "Vetar client"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Prohibeix l'accés a un client a la teva sala."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Cerca en un directori"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Cerca contingut que coincideixi amb la base de dades en un directori."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Cerca en aquest directori>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "Selecciona aquesta opció per cerca contingut a la carpeta actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Cerca en un fitxer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Cerca contingut que coincideixi amb la base de dades en un fitxer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Cerca manual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Cerca configurable basada en noms de fitxer de contingut. No cal que el contingut coincideixi amb la base de dades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "Escanejar"
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Afegir a la mescla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Afegir a la mescla i reproduir"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Directori de contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Selecciona un directori on cercar continguts."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Nom del sistema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Especifica un «nom de sistema» amb el qual associar els continguts trobats. Es fa servir per donar nom a la llista de reproducció generada i per identificar-ne les miniatures."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Nom de sistema personalitzat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Especifica manualment un «nom de sistema» pels continguts trobats. Només es fa servir quan el «Nom de sistema» és «<Personalitzat>»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Nucli per defecte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Selecciona un nucli per defecte pels continguts trobats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Extensions de fitxer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Llista de tipus de fitxer que s’inclouran a la cerca, separats amb espais. Si és buida inclou tots els tipus de fitxer, o tots els fitxers admesos pel nucli si aquest s’ha especificat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Cerca recursiva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Quan s’activa inclou a la cerca tots els subdirectoris del «Directori de continguts» especificat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Cerca dins dels arxius"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Quan s’activa, se cercaran continguts vàlids o admesos dins dels fitxers d’arxiu (.zip, .7z, etc.). Pot afectar significativament el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Fitxer DAT d’arcade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Escull un fitxer XML DAT de Logiqx o MAME per anomenar automàticament els continguts identificats (MAME, FinalNBurn Neo, etç)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Filtre DAT d'arcade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Si es fa servir un fitxer DAT, el contingut només s'afegirà a la llista de reproducció si coincideix amb la informació del fitxer DAT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Sobreescriu la llista de reproducció existent"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Si s’activa s’esborrarà qualsevol llista de reproducció existent abans de cercar continguts. Si es desactiva es mantenen les entrades de la llista i només s’hi afegeixen els continguts que falten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Valida les entrades existents"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Si s’activa es verificaran les entrades de qualsevol llista de reproducció existent abans de cercar continguts nous. S’esborraran les entrades que facin referència a continguts desapareguts i/o fitxers amb extensions no vàlides."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Comença la cerca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Cerca els continguts seleccionats."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "S’està inicialitzant la llista..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Any de publicació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Nombre de jugadors"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Regió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Etiqueta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Cerca pel nom..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Mostra'ls tots"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Filtre addicional"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Tots"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Afegeix un filtre addicional"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u elements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Per desenvolupador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Per editor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Per any de publicació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Per nombre de jugadors"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Per gènere"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Per assoliments"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Per categoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Per llengua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Per regió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Per exclusivitat de consola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Per exclusivitat de plataforma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Per vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Per puntuació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Per suport"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Per controls"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Per estil artístic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Per mecànica de joc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Per narrativa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "Per ritme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Per perspectiva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Per ambientació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Per estil visual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Per vehicles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Per origen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Per franquícia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Per etiqueta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Per nom del sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Estableix el filtre de rang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Vista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Desa com a vista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Suprimeix aquesta llista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Introduïu el nom de la nova vista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Ja hi ha una vista amb el mateix nom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "S’ha desat la vista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "S’ha suprimit la vista"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Executa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Inicia el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Canvia el nom"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Canvia el títol de l’entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Elimina"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Elimina l’entrada de la llista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Afegeix als favorits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Afegeix el contingut a «Favorits»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "Afegir a la llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "Afegeix el contingut a una llista de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "Crea una nova llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "Crea una nova llista de reproducció i afegeix aquest element a la nova llista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Estableix l’associació del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Estableix el nucli associat amb aquest contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Restableix l’associació del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Restableix el nucli associat amb aquest contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Visualitza més informació sobre el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Baixa miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Baixa imatges de captures de pantalla/art de la capsa/pantalla de títol pel contingut actual. Actualitza les miniatures existents."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Nucli actual"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Nom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Camí al fitxer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Entrada: %lu/%lu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Nucli"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Temps de joc"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Darrera partida"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Bases de dades"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Continua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Reprèn el contingut i abandona el Menú Ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Reinicia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Reinicia el contingut des del principi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Tanca contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Tanca el contingut. Es perdrà qualsevol progrés no desat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Fes una captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Captura una imatge de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Posició de desat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Canvia la posició actual de desat ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Desa estat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Desa un estat a la posició actualment seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVE_STATE,
   "Desa un estat a la ranura seleccionada. Nota: Els estats desats no es poden compartir i no funcionaran amb altres versions del nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Carrega estat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Carrega un estat desat des de la posició actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_STATE,
   "Carrega un estat desat des de la ranura seleccionada. Nota: No funcionarà si l'estat es va desar amb una versió anterior del nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Desfer càrrega ràpida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Si s'ha carregat un estat desat, el contingut tornarà a l'estat previ a la càrrega."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Desfer desat ràpid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Si s'ha sobreescrit un desat ràpid, tornarà a tenir les dades prèvies."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "Posició de repitició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "Canvia la posició actual de desat ràpid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Reproduir repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Reprodueix el fitxer de repetició des de la posició seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Enregistrar la repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Enregistra un fitxer de repetició a la posició seleccionada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Atura l'enregistrament/repetició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "Atura la gravació/reproducció de la repetició actual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Afegeix als favorits"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Afegeix el contingut a «Favorits»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Inicia enregistrament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Inicia la gravació de vídeo de la partida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Atura l'enregistrament"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Atura la gravació de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Inicia la transmissió"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Inicia una retransmissió en el destí seleccionat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Atura la retransmissió"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Acabar la retransmissió."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Estats desats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Accedeix a les opcions dels desats ràpids."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Opcions del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Canvia les opcions pel contingut."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Canvia els controls del contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Trucs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Configura els trucs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Control de disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Gestió de la imatge de disc."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Configura shaders per millorar visualment la imatge."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Excepcions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Opcions per definir excepcions a la configuració global."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Assoliments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Vegeu els assoliments i les opcions relacionades."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Gestiona les opcions del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Desa o elimina excepcions a les opcions pel contingut actual."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Desa les opcions del joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Desa les opcions del nucli que s'aplicarà al contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Elimina les opcions del joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Elimina les opcions del nucli que s'aplicaran només al contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Desa les opcions del directori de contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Desa les opcions del nucli que s'aplicaran a tot el contingut carregat de la mateixa carpeta que el fitxer actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Elimina les opcions del directori de continguts"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Esborra les opcions del nucli que s'aplicaran a tots els continguts que es carreguin des de la mateixa carpeta que el fitxer actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Fitxer d'opcions actiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "El fitxer d'opcions que està ara mateix en ús."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Reinicia les opcions del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Reinicia totes les opcions del nucli als valors predeterminats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Escriu les opcions al disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Força que s’escriguin les opcions actuals al fitxer d’opcions actiu. S’assegura que les opcions es conservin en cas que un error del nucli provoqués una aturada incorrecta del front."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Crea el fitxer d’opcions del joc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Desa el fitxer d’opcions del joc"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Gestiona els fitxers de reassignació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Carrega, desa o elimina els fitxers d'assignació d'entrada del contingut actual."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Fitxer de reassignació actiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "El fitxer de reassignació està en ús actualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Carrega el fitxer de reassignació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Carrega i substitueix les assignacions actuals d'entrada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Anomena i desa el fitxer de reassignació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Desa la configuració actual d'assignacions d'entrada en un nou fitxer d'assignacions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Desa el fitxer de mapeig del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Desa un fitxer d'assignació que s'aplicarà a tots els continguts que es carreguin amb aquest nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Elimina els fitxers d'assignació del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Elimina el fitxer d'assignació que s'aplicarà a tots els cotinguts que es carreguin amb aquest nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Desa la personalització a la carpeta de fitxers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Desa un fitxer d'assignacions que s'aplicaran a tot el contingut carregat de la mateixa carpeta que el fitxer actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Elimina el fitxer d'assignació de les carpetes de contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Elimina un fitxer d'assignacions que s'aplicaran a tot el contingut carregat de la mateixa carpeta que el fitxer actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Desa el fitxer de mapeig del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "Desa un fitxer d'assignació que s'aplicarà al contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Elimina els fitxers d'assignació del joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "Elimina el fitxer d'assignació que s'aplicarà al contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Reinicia les assignacions d'entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Estableix totes les opcions de les assignacions als valors predeterminats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Actualitza el fitxer d'assignacions d'entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Sobreescriu el fitxer d'assignació actiu amb les opcions actuals d'assignació d'entrada."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Fitxer de reassignació"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Comença o continua la cerca de trucs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT,
   "Escaneja la memòria per trobar nous trucs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Carrega el fitxer de trucs (Reemplaça)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Carrega un fitxer de trucs i reemplaça els trucs existents."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Carrega fitxers de trucs (afegir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Carrega un fitxer de trucs i afegeix-los als trucs actuals."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Torna a carregar els trucs específics del joc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Anomena i desa el fitxer de trucs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Desa els trucs actuals en un fitxer de trucs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Afegeix un nou truc a la part superior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Afegeix un nou truc al final"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Elimina tots els trucs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Aplica els trucs automàticament durant en carregar un joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Aplica els trucs automàticament quan es carrega el joc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Aplica en activar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Aplica els trucs immediatament després d'activar-los."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Aplica els canvis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Els canvis en els trucs tindran efecte immediat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Trucs"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Inicia o reinicia una cerca de trucs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Clica esquerra o dreta per canviar la mida en bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Clica esquerra o dreta per canviar el valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Igual a %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Menys que abans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Inferior o igual a l'anterior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Més grans que l'anterior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Més gran o igual que l'anterior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Iguala a l'anterior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Diferents que l'anterior"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Clica esquerra o dreta per canviar el valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Iguals a l'anterior +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Cerca valors en la memòria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Clica esquerra o dreta per canviar el valor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Iguals a l'anterior -%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Afegeix %u coincidències a la llista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Suprimeix la coincidència #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Crea una coincidència del truc #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Direcció de coincidència: %08X Mask: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Fitxer de trucs (Reemplaça)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Fitxer de trucs (afegir)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Detalls del truc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Índex"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Posició del truc a la llista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Habilitat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Descripció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Mida de la memòria de cerca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Tipus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Adreça de memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Examina l'adreça: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Adreça de memòria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Màscara de bits de la direcció si la mida de la cerca en memòria és menor a 8 bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Nombre d'iteracions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "El nombre de vegades que un truc pot ser aplicat. Fes servir amb les altres dues opcions d'iteració per cobrir grans regions de memòria."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Augmenta la direcció en cada iteració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Després de cada iteració, s'augmentarà l'adreça de memòria amb aquest valor multiplicat per la mida de la cerca de memòria."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Augmentar el valor en cada iteració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Després de cada iteració, el 'Valor' augmentarà en aquesta quantitat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Vibra si la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Valor de la vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Port de vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Força de la vibració primària"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Duració (ms) de la vibració primària"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Força de la vibració secundària"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Duració (ms) de la vibració secundària"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Codi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Afegeix un nou truc després d'aquest"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Afegeix un nou truc després d'aquest"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Copia aquest truc per més endavant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Copia aquest truc per més endavant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Elimina aquest truc"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Expulsar disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Obre la safata del disc virtual i extreu el disc que s'estava carregant. Si l'opció 'Fes pausa al contingut quan el menú estigui actiu' està activada, alguns nuclis no poden registrar canvis fins que hagin passat uns segons amb el contingut reprès i després de cada acció de control de discs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Insereix disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Insereix un disc i tanca la safata virtual. Si l'opció 'Fes pausa al contingut quan el menú estigui actiu' està activada, alguns nuclis no poden registrar canvis fins que hagin passat uns segons amb el contingut reprès i després de cada acció de control de discs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Carrega un nou disc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Expulsa el disc actual, selecciona un nou disc des del sistema de fitxers, insereix-lo i tanca la safata virtual del disc.\nNOTA: Aquesta és una característica heretada. Es recomana carregar contingut de diversos discs a les llistes de reproducció M3U, les quals permeten seleccionar discs mitjançant les opcions 'Expulsar disc', 'Introduir disc' i 'Índex del disc actual'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Escull un nou disc des del sistema de fitxers i insereix-lo sense tancar la safata virtual de disc.\nNOTA: Aquesta és una característica heretada. Es recomana carregar títols de diversos discs fent servir les llistes de reproducció M3U, les quals permeten seleccionar discs mitjançant l'opció 'Índex del disc actual'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Índex del disc actual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Escull el disc d'una llista de disc disponibles. El disc es carregarà en clicar 'Insereix disc'."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Shaders de Vídeo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Activa el procés de shaders de vídeo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Comprova els canvis en els shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Aplica els canvis al fitxer de shaders en el disc de manera automàtica."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Cerca canvis en els fitxers dels shaders. Un cop hagis desat els canvis d'un shader en el disc, el shader es recompilarà automàticament i s'aplicarà el contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Recorda l'última carpeta de shaders que s'ha fet servir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Obre l'explorador de fitxers en l'última carpeta que es va obrir per carregar ajustaments i shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Carregar ajust predeterminat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Carrega un shader predefinit. El pipeline de shaders es configurarà automàticament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Carrega directament una configuració de shaders. El menú de shaders s'actualitzarà.\nEl factor d'escala que es mostra en el menú serà correcte només si es fa servir mètodes d'escalat simples."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Anteposar el preajust"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Anteposa el preajust actual al preajust carregat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Afegeix el preajust"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Annexa l'ajustament actual en carregar-lo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_MANAGER,
   "Gestiona els fitxers predeterminats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_MANAGER,
   "Desa o elimina la configuració de shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_FILE_INFO,
   "Fitxer predeterminat actiu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_FILE_INFO,
   "Un shader predeterminat actualment en ús."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Aplica els canvis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Els canvis de la configuració de shaders tindran efecte immediat. Fes servir aquesta opció en canviar la quantitat de passades, filtrat,..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "Després de canviar la configuració de shaders (com el nombre de passades, el filtratge, l'escala...) pots fer servir aquesta opció per aplicar els canvis.\nCanviar aquesta configuració és una opció que consumeix molts recursos, així que cal fer-la de forma explícita.\nEn aplicar els shaders, es desarà la configuració del menú en un fitxer temporal(retroarch.slangp/.cgp/.glslp) i es carregaran. El fitxer no s'eliminarà en tancar RetroArch i es desarà a la carpeta de shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Paràmetres del shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Modifica el shader actual directament. Els canvis no es desaran en el fitxer d'ajustament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Nombre de passades del shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Augmenta o disminueix la quantitat de passes de canalització de shaders. Shaders separats es poden vincular a cada passa de canalització i configurar el seu escalat i filtrat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch permet combinar diversos shaders amb passades arbitràries, filtres personals de maquinari i factors d'escala.\nAquesta opció especifica la quantitat de passades de shaders que es faran servir. Si esculls 0 i després 'Aplicar canvis en els shaders', faràs servir un shader 'en blanc'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "Ruta cap a la carpeta de shaders. Tots els shaders han de ser del mateix tipus (per exemple, Cg, GLSL o Slang). Canvia el directori de shaders per indicar on el navegador començarà a cercar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Filtre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "Filtre de maquinari que s'aplicarà. Si està seleccionat a 'Predeterminat', el filtre serà lineal o més proper en funció de 'Filtre bilineal', en la configuració del vídeo."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Escala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "Ajusta l'escala per aquesta passada. El factor d'escala és acumulatiu, per exemple: 2x per a la primera passada i 2x per a la segona serà una escala global de 4x.\nSi l'última passada té un factor d'escala, el resultat final estirarà per tota la pantalla amb el filtre predeterminat segons la configuració del filtre bilineal (en la configuració de vídeo).\nSi has escollit 'Irellevant', es farà servir l'escala 1x o bé s'estirarà a pantalla completa, en funció de si és l'última passad[...]"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Predefinicions simples"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Desa una predefinició de shader que té un enllaç a la predefinició carregada originalment i inclou només els paràmetres canviats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Desa l'ajust predeterminat actual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Desa l'ajust predeterminat de shaders actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Desa ajust predeterminat com"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Desa la configuració actual del shader com una nova predefinició de shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Desa com a predefinició global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Desa la configuració actual de shaders com a configuració global predeterminada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Desa com a predefinició del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Desa la configuració actual del shader com a predeterminada per aquest nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Desa com a predefinició del directori de contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Desa la configuració actual dels shaders com a predeterminada per tots els fitxers a la carpeta de continguts actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Desa com a predefinició del joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Desa la configuració actual de shaders per aquest contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "No s’han trobat predefinicions de shader automàtiques"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Suprimeix la predefinició global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Suprimeix la predefinició global, usada per tots els continguts i tots els nuclis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Suprimeix la predefinició del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Suprimeix la predefinició del nucli, usada per tots els continguts executats amb el nucli actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Suprimeix la predefinició del directori de contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Suprimeix la predefinició del directori de contingut, usada per tots els continguts dins del directori actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Suprimeix la predefinició del joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Suprimeix la predefinició del joc, usada només pel joc en qüestió."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "No hi ha configuració pels shaders"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "Fitxer de personalització activat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "El fitxer de trucs està en ús actualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "Carrega un fitxer personalitzat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "Carrega i substitueix la configuració actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "Anomena i desa les personalitzacions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "Desa la configuració actual en un nou fitxer de personalització."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Desa les excepcions del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Desa un fitxer de configuració d’excepcions que s’aplicarà a tot el contingut carregat amb aquest nucli. Tindrà prioritat sobre la configuració principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Elimina les personalitzacions del nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Esborra el fitxer de configuracions personalitzades que s'aplicarà a tots els continguts que es carreguin amb aquest nucli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Desa la personalització de directori de continguts"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Desa un fitxer de configuració d’excepcions que s’aplicarà a tot el contingut carregat des de la mateixa carpeta que el fitxer actual. Tindrà prioritat sobre la configuració principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Elimina la personalització de les carpetes de contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Elimina el fitxer de configuració personalitzada que s'aplicarà a tot el contingut carregat de la mateixa carpeta que el fitxer actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Desa les excepcions del joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Desa un fitxer de configuració d’excepcions que s’aplicarà només al contingut actual. Tindrà prioritat sobre la configuració principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Elimina les personalitzacions del joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Elimina el fitxer de configuració personalitzada que s'aplicarà només al contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "Descarrega la personalització"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "Reinicia totes les opcions als valors de la configuració global."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "No hi ha assoliments per mostrar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Cancel·la la pausa del mode expert d’assoliments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Deixa el mode expert d’assoliments activat per la sessió actual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Cancel·la la represa del mode expert d’assoliments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Deixa el mode expert d’assoliments desactivat per la sessió actual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "Reprèn el mode expert d’assoliments desactivat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "Has de recarregar el nucli per reprendre el mode expert d'assoliments"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Posa en pausa el mode expert d’assoliments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Posa en pausa el mode Hardcore pels assoliments en aquesta sessió. Aquest canvi activa els trucs, rebobinat, mode càmera lenta i carregar estats desats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Reprèn el mode expert d’assoliments"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Reprèn el mode hardcore en els assoliments per aquesta sessió. Això deshabilitarà els trucs, el rebobinat, la càmera lenta, poder carregar estats desats i reiniciar el joc actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "El servidor de RetroAchievements no està disponible"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Un o més desbloquejos d'assoliments no han arribat al servidor. S'intentarà reenviar la informació mentre tinguis RetroArch obert."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "El servidor de RetroAchievements no està disponible. Es seguirà intentant la connexió fins que es confirmi la comunicació o fins que es tanqui l'aplicació."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "Totes les sol·licituds pendents han sigut sincronitzades amb el servidor de RetroAchievements."
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "S'està identificant el joc"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "S'estan obtenint les dades del joc"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "S'està iniciant la sessió"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "No s'ha iniciat la sessió"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Error de xarxa"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Joc desconegut"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "No es pot activar els assoliments amb aquest nucli"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "Hash de RetroAchievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Element de la base de dades"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Mostra informació de la base de dades pel contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "No hi ha elements disponibles"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "No hi ha nuclis disponibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "No hi ha opcions del nucli disponibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "No hi ha informació del nucli disponible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "No hi ha còpies de seguretat del nucli disponibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "No hi ha preferits disponibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "No hi ha historial disponible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "No hi ha imatges disponibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "No hi ha música disponible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "No hi ha vídeos disponibles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "No hi ha informació disponible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "No hi ha elements disponibles a la llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "No s'ha trobat cap configuració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "No s'han trobat dispositius amb Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "No s'ha trocat cap xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Cap nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Cerca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "Intercanvia les miniatures"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RANDOM_SELECT,
   "Selecció aleatòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Enrere"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "D'acord"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Directori pare"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "Torna al directori superior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "No s'ha trobat la carpeta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "No hi ha elements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Seleccionar fitxer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180º"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270º"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Compilador desconegut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Compartit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Exclusió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Majoria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Compartir l'entrada analògica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Màxim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Mitjana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Cap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Sense preferències"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Rebota a esquerra i dreta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Desplaçar cap a l'esquerra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Mode d'imatge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Mode veu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Mode narrador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Historial i Preferits"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Totes les llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "No"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Historial i Preferits"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Sempre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Mai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Per nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Acumulatiu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Carregada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Carregant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Descarregant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "No hi ha font"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Fes servir aquesta adreça>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "Selecciona aquesta opció per establir aquesta carpeta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Directori de continguts>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Per defecte>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Cap>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad amb analògic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Cap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Desconegut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Avall + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Mantén Start (2 segons)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Mantén Select (2 segons)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Avall + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Desactivat>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Canviï"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "No canviï"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Augmenti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Disminueixi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= valor de vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "≠ valor de vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< valor de vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> valor de vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Augmenti per valor de vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Disminueixi per valor de vibració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Tots"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Desactivat>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Assigna valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Augmenta en Valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Disminueix en Valor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Executa el següent truc si el valor és idèntic al de la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Executa el següent truc si el valor és diferent del de la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Executa el següent truc si el valor és inferior al de la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Executa el següent truc si el valor és superior al de la memòria"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Emulador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Bit, Valor màxim = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-Bit, Valor màxim = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-Bit, Valor màxim = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-Bit, Valor màxim = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16 Bits, valor màxim = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32 bits, Valor màxim = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Configuració predeterminada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Alfabètic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Cap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Mostra les etiquetes completes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Elimina el contingut entre parèntesis ( )"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Elimina el contingut entre claudàtors [ ]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Elimina el contingut entre ( ) i [ ]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Preservar la regió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Preserva l'índex del disc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Manté la regió i l'índex del disc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Configuració predeterminada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Caràtula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Captura de pantall"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Pantalla de títol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "Logotip de contingut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Ràpida"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "Activa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "No"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Sí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Cert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Fals"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Habilitat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Deshabilitat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "N/D"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Bloquejat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Desbloquejat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
   "Expert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "No oficial"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "No compatible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Desblocat recentment"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Gairebé allà"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Desafiaments actius"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Només rastrejadors"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Només notificacions"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Per defecte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Lineal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Més proper"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Contingut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Directori de continguts>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Personalitzat>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<No especificat>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Analògic esquerre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Analògic dret"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Analògic esquerre (forçat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Analògic dret (forçat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "Tecla %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Ratolí 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Ratolí 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Ratolí 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Ratolí 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Ratolí 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Roda del ratolí cap amunt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Roda del ratolí cap avall"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Roda del ratolí cap a l’esquerra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Roda del ratolí cap a la dreta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Aviat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Tard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "AAAA-MM-DD HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "AAAA-MM-DD HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "AAAA-MM-DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "AAAA-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "MM-DD-AAAA HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "MM-DD-AAAA HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "MM-DD-AAAA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "DD-MM-AAAA HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "DD-MM-AAAA HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "DD-MM-AAAA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "AAAA-MM-DD HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "AAAA-MM-DD HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "MM-DD-AAAA HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "MM-DD-AAAA HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "DD-MM-AAAA HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "DD-MM-AAAA HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "Abans"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Gruix del fons"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Augmenta les dimensions de la quadrícula de fons del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Filtre de vores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Bruix de la vora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Augmenta les dimensions de la quadrïcula de la vora del menú."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Mostra la vora del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Utilitzar l'amplada completa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Canvia la longitud i la posició dels elements del menú per aprofitar al màxim l'espai disponible en pantalla. Desactiva aquesta opció per fer servir una disposició clàssica de doble columna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Filtre lineal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Suavitza lleugerament el menú per no es notin les vores pixelades."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Reescalat intern"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Escala la interfície del menú abans de mostrar-la en pantalla. Si l'opció 'Filtre lineal del menú' està activada, elimina els efectes d'escalat (píxels no proporcionals) mantenint una imatge nítida. Té un impacte significatiu en el rendiment que augmenta si ho fa l'escalat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Relació d'aspecte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Seleccioneu la relació d’aspecte del menú. Les relacions de pantalla panoràmica incrementen la resolució horitzontal de la interfície del menú. (Pot caldre reiniciar si l’opció «Bloqueja la relació d’aspecte del menú» està desactivada)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Bloca la relació d'aspecte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Assegura que el menú es vegi sempre amb la relació d’aspecte correcta. Si es desactiva, el menú ràpid s’estirarà per coincidir amb el contingut carregat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Color del tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Selecciona un tema de colors diferent. 'Personalitzat' permet fer servir fitxers d'ajustament de temes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Tema predefinit predeterminat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Selecciona un ajustament del tema del menú en l'explorador de fitxers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Transparència"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Mostra el contingut en el fons del menú ràpid. Desactivar la transparència pot canviar els colors del tema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Efectes d'ombra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Mostra ombres en el text, vores i miniatures del menú. No afecta gaire el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Animació de fons"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Activa un efecte d'animació de partícules en el fons. Té un impacte en el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Velocitat de l'animació de fons"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Ajusta la velocitat dels efectes d'animació de les partícules de fons."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Animació d'estalvi de pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Mostra un efecte d'animació de partícules en el fons mentre l'estalvi de pantalla del menú estigui actiu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Mostra miniatures de les llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Mostra les miniatures de forma seqüencial a les llistes de reproducció. Clica RetroPad Select per activar o desactivar aquesta opció. En desactivar aquesta opció, encara es poden mostrar les miniatures a pantalla completa, clicant al botó RetroPad Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Miniatura superior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Indica el tipus de miniatura que es mostrarà a la part superior dreta de les llistes de reproducció. El tipus escollit es pot canviar clicant al botó RetroPad Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Miniatura inferior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Tipus de miniatura a mostrar a la part superior dreta de la llista de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Intercanvia miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Intercanvia la posició de les miniatures superior e inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Mètode d'escalat de miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Selecciona el mètode de redimensionat perquè les miniatures entrin a la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Retard en les miniatures (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Aplica un retard entre el moment en què se selecciona un element d'una llista de reproducció i la càrrega de la miniatura corresponent. Un valor mínim de 256 ms elimina qualsevol retard, també en els equips més lents."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Suport per ASCII ampliat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Habilita la visualització de caràcters ASCII no estàndards. És necessari per compatibilitat amb certes llengües occidentals no angleses. Afecta moderadament al rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Icones d'interruptors"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Mostra icones en comptes de text per representar aquella configuració del menú que tingui aquestes opcions."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Píxel més proper (Ràpid)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "Bilineal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (Lent)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Cap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "Automàtic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (centrat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (centrat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (Centrat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (centrat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (centrat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "No"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Ajusta a la pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Escalat d'enter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Omple la pantalla (estirat)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Vermell clàssic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Taronja clàssic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Groc clàssic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Verd clàssic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Blau clàssic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Lila clàssic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Gris clàssic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Vermell antic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Lila fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Blau de mitjanit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Daurat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Blau elèctric"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Verd poma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vermell volcànic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Llac"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA,
   "Dràcula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "Cotó de sucre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Grubvox Fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Gruvbox (clar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackejant el kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solaritzat fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solaritzat clar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango clar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dinàmic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Gris fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Gris clar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "No"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Nevada (lleugera)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Nevada (intensa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Pluja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "Vòrtex"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Estrelles"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Segona miniatura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Tipus de miniatura que es mostrarà a l’esquerra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "Miniatura de icona"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "El tipus de miniatura que es mostrarà com a icona en les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Fons dinàmic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Carrega un fons de pantalla dinàmicament segons el context."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Animació horitzontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Activa l’animació horitzontal pel menú. Això afectarà el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Ressalta les icones horitzontals en fer animacions"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Indica l'animació que apareix en moure's entre pestanyes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animació de pujada/baixada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Indica l'animació que apareix en el desplaçament vertical."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animació d'obertura i tancament del menú principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Indica l'animació que apareix en obrir un submenú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Transparència del tema de colors"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Tipografia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Seleccioneu un altre tipus de lletra principal pel menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Color del text (vermell)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Color del text (verd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Color del text (blau)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Disposició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Selecciona la disposició de la interfície XMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Tema d'icones"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Selecciona un tema d'icones per RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Icones d'interruptors"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "Mostra icones en comptes de text per representar aquella configuració del menú que tingui aquestes opcions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Efectes d'ombra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Mostra ombres en totes les icones, miniatures i texts. Afecta lleugerament al rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Canal de shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Escull un fons animat. Alguns efectes poden consumir molts recursos de la GPU. Si el rendiment és baix, desactiva aquesta opció o utilitza un efecte més simple."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Color del tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Selecciona un tema de colors de fons diferent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Disposició vertical de miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Mostra la minatura de l’esquerra a sota de la de la dreta, al costat dret de la pantalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Factor d’escala de les miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Redueix la mida de visualització de les miniatures escalant l’amplada màxima permesa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Factor d’esvaïment vertical"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Mostra la capçalera del títol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Marge del títol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Desplaçament horitzontal del marge del títol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Activa la pestanya de configuració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Mostra la pestanya de configuració que conté els paràmetres del programa."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Cinta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Cinta (simplificada)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Neu simple"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Neu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Flocs de neu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Monocrom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Monocrom invertit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Sistemàtic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
   "Píxel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Automàtic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Automàtic invertit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Verd poma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Clar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Blau matinal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Raig de sol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Lila fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Blau elèctric"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Daurat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Vermell antic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Blau de mitjanit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Imatge de fons"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Submarí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vermell volcànic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Verd llima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Groc Pikachu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Lila Cub"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Vermell familiar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Flamejant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Glaçat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Gris fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Gris clar"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT,
   "Tipografia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT,
   "Seleccioneu un altre tipus de lletra principal pel menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE,
   "Mida de la font"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE,
   "Defineix si la mida de la tipografia en el menú hauria de tenir escala pròpia i si l'escala ha de ser global o, en canvi, tenir valors independents per cada part del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_SEPARATE,
   "Valors separats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Factor d'escala de la mida de la tipografia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Escala la mida de la tipografia linealment al llarg del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Factor d'escala de la mida de la tipografia del títol"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Escala la mida de la tipografia pels títols de la capçalera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Factor d'escala de la mida de la tipografia de la barra esquerra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Escala la mida de la tipografia pel text de la barra esquerra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Factor d'escala de la mida de la tipografia de les etiquetes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Escala la mida de la tipografia per les etiquetes del menú opcions i les entrades de la llista de reproducció. Això afecta també a la mida del text de les caixes d'ajuda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Factor d'escala de la mida de la tipografia de les subetiquetes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Escala la mida de la tipografia per les subetiquetes del menú opcions i les entrades de la llista de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TIME,
   "Factor d'escala de la mida de la tipografia de la marca de temps"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TIME,
   "Escala la mida de la tipografia per l'hora i la data indicades a la part superior dret del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Factor d'escala de la mida de la tipografia del peu de pàgina"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Escala la mida de la tipografia pel text del peu de pàgina. Això afecta també a la mida del text de la barra lateral dreta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Replega la barra lateral"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Manté la barra lateral sempre replegada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Parteix els noms de la llista de reproducció (Es requereix reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Elimina els noms dels fabricants a les llistes de reproducció. Per exemple, 'Sony - PlayStation' passa a ser 'PlayStation'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Endreça les llistes de reproducció després de truncar els noms (Cal reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Les llistes de reproducció s'endrecen per ordre alfabètic eliminant la part del fabricant dels noms."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Segona miniatura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Substitueix el plafó de metadades per una altra miniatura."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Mostra metadades de continguts en moviment"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Si actives aquesta opció, cadascun dels elements de les metadades d'un contingut que es mostren a la part dreta de les llistes de reproducció (nucli associat, temps de joc,...) ocuparà una única fila, i el text que superi l'amplada disponible, es desplaçarà lateralment automàticament. Si desactives aquesta opció, cada element de les metadades es mostrarà de forma estàtica, ocupant les línies que facin falta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Factor d’escala de les miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Ajusta la mida de la barra de miniatures."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_PADDING_FACTOR,
   "Factor del farciment"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_PADDING_FACTOR,
   "Escala la mida horitzontal del farciment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR,
   "Separador de la capçalera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_SEPARATOR,
   "Amplada alternativa per la capçalera i el peu de pàgina."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NONE,
   "Cap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_MAXIMUM,
   "Màxim"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Color del tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Selecciona un tema de colors diferent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Blanc bàsic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Negre bàsic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Grubvox Fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackejant el kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "Dimensió Desconeguda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA,
   "Dràcula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solaritzat fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solaritzat clar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Gris fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Gris clar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN,
   "Pluja lila"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Icones"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Mostra icones a l’esquerra de les entrades del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Icones d'interruptors"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "Mostra icones en comptes de text per representar aquella configuració del menú que tingui aquestes opcions."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Icones de les llistes de reproducció (Es requereix reinici)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Mostra icones del sistema a les llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Optimitza la disposició en mode horitzontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Ajusta automàticament la posició del menú perquè s'adapti a la pantalla amb disposició horitzontal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Mostra la barra de navegació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Mostra sempre un menú de navegació amb accés directe. Permet canviar entre categories de forma ràpida. Recomanat per dispositius que facin servir pantalles tàctils."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Gira automàticament la barra de navegació"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Mou automàticament la barra de navegació a la part dreta de la pantalla si es fa servir l'orientació horitzontal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Color del tema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Selecciona un tema de colors de fons diferent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Animació de transició"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Permet navegar per les diferents opcions del menú amb una animació fluida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Visualització de les miniatures en mode vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Especifica la forma de mostra les miniatures de les llistes de reproducció amb l'orientació vertical."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Miniatures en mode horitzontal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Especifica la forma de mostra les miniatures de les llistes de reproducció amb l'orientació horitzontal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Mostra les miniatures secundàries en les llistes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Mostra una miniatura secundària si es fa servir els formats tipus llista de miniatures a les llistes de reproducció. Aquesta configuració només té efecte si la pantalla té l'amplada suficient per mostrar dues miniatures."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Fons de les miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Permet cobrir l'espai no utilitzat en les miniatures amb fons pla. Això garanteix que totes les imatges tinguin una mida uniforme, millorant visualment els menús en veure miniatures de continguts i mides diferents."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Miniatura principal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Miniatura principal relacionada amb cada element de les llistes de reproducció. Acostuma a ser la icona del contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Segona miniatura"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Miniatura auxiliar relacionada a cada element de la llista de reproducció. Es farà servir en funció del mode de miniatures que estigui seleccionat."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Blau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Verd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "Escut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Vermell"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Groc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Material UI Fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Grubvox Fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solaritzat fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Taronja Cutie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Cutie Rosa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Lila Cutie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackejant el kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Gris fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Gris clar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "Automàtic"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Esfumar-se"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Desplaçament"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "No"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "No"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Llista (petita)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Llista (Mitjana)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "No"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Llista (petita)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Llista (Mitjana)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Llista (gran)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Escriptori"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "No"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "Activa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Exclou les imatges en miniatura"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Informació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Fitxer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "Carrera un nucli..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "Descarregar nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "&Sortida"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Edita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Cerca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Veure"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Finestres tancades"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Paràmetres del shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "Configuració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Recorda la posició de les finestres acoblades:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Recorda la geometria de la finestra:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Recorda l'última pestanya de l'explorador de continguts:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Tema:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Predefinit pel sistema>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Fosc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Personalitzat..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Opcions"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Eines"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Ajuda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "Sobre RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Documentació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Carrega el nucli personalitzat..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Carregar Nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Carregant el nucli..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Nom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Versió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Navegador de fitxers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "A dalt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Amunt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Explorador de continguts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Caràtula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Captura de pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Pantalla de títol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO,
   "Logotip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Totes les llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Informació del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Pregunta'm>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Advertència"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Error de xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Cal reiniciar el programa per a què els canvis tinguin efecte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Registre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 elements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Arrossega aquí una imatge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "No mostris una altra vegada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Atura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Nucli associat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Llistes de reproducció ocultes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Amagar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Color destacat:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "Escollir..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Seleccionar el color"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Seleccionar el tema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Tema personalitzat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "L'adreça del fitxer està buida."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "L'arxiu està buit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "No es pot obrir el fitxer per llegir-lo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "No es pot obrir el fitxer per escriure-hi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "El fitxer no existeix."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Suggereix el nucli carregat com a primera opció:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Ampliar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Veure"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Icones"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Llista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Netejar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Progrés:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "Nombre màxim d'entrades a 'Totes les llistes de reproducció':"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "Nombre màxim d'entrades a 'Totes les llistes de reproducció':"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Mostra fitxers i carpetes ocults:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Nova llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Introdueix el nom d'una llista nova:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Esborrar llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Reanomena la llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Estàs segur que vols esborrar la llista de reproducció \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Qüestió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "No es pot esborrar aquest fitxer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "No es pot canviar el nom a aquest fitxer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Creant llista de fitxers..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Afegint fitxers a la llista de reproducció..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Element de la llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Nom:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Ruta:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Nucli:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Bases de dades:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(separades amb espais; per defecte les inclou totes)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Filtra dins dels arxius"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(es fa servir per trobar miniatures)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Segur que voleu esborrar l’element «%1»?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Trieu una sola llista de reproducció primer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Esborra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Afegeix una entrada..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Afegeix fitxers..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Afegeix una carpeta..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Edita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Selecciona fitxers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Selecciona una carpeta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<múltiples>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "S’ha produït un error en actualitzar l’entrada de la llista de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Ompliu tots els camps obligatoris."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "Actualitza el RetroArch (nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "El RetroArch s’ha actualitzat correctament. Reinicieu l’aplicació per aplicar els canvis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Ha fallat l’actualització."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Col·laboradors"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Shader actual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Mou avall"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Mou amunt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Carrega"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Desa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Elimina"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Elimina passades"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Aplica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Afegeix una passada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Elimina totes les passades"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "No hi ha passades de shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Restableix la passada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Restableix totes les passades"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Restableix el paràmetre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Descarrega la miniatura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Ja hi ha una baixada en curs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Comença a la llista de reproducció:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Miniatura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Límit de la memòria cau de miniatures:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Límit de la mida de miniatures arrosegades:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Descarrega totes les miniatures"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Sistema sencer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Aquesta llista de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Les miniatures s'han descarregat correctament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Obtingudes: %1 Fallades: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Opcions del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Reiniciar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Restableix-ho tot"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Opcions de l'actualitzador de nuclis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Comptes Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Punt de connexió amb la llista de comptes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Turbo/zona morta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Comptadors dels nuclis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "No heu seleccionat cap disc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Comptadors de la interfície"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Menú horitzontal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Amaga les descripcions de nuclis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Mostrar etiquetes amb descripcions d'entrada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Superposició en Pantalla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Historial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Selecciona el contingut des d'una llista de reproducció recent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Quan es carrega un contingut, es desarà a l'historial juntament amb les combinacions de nuclis de libretro.\nL'historial es desa en un fitxer situat a la mateixa carpeta on estigui el fitxer de configuració de RetroArch. Si no s'ha carregat un fitxer de configuració en iniciar RetroArch, no es desarà ni carregarà l'historial i l'opció desapareixerà del menú principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "Multimèdia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Subsistemes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Accedeix a la configuració dels subsistemes pel contingut actual."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Contingut actual: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "No s'han trobat servidors del joc en xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "No s'han trobat clients del joc en xarxa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "No hi ha comptadors de rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "No hi ha llistes de reproducció."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Connectat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "En línia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Port %d Nom dispositiu: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Nom per mostrar del dispositiu: %s\nNom de configuració del dispositiu: %s\n VID/PID del dispositiu: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Configuració dels trucs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Comença o continua la cerca de trucs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Reprodueix en el reproductor de mèdia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "segons"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Inicia el nucli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Inicia el nucli sense contingut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Nuclis suggerits"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "No es pot llegir el fitxer comprimit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Usuari"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "Teclat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Nombre màxim d'imatges en la cadena d'intercanvi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Avisa al controlador de vídeo que faci servir un mode de búfer concret."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Nombre màxim d'imatges en swap chain. Això indica al controlador de vídeo que faci servir un búfer de vídeo concret.\nBúfer simple: 1\nBúfer doble: 2\nBúfer triple: 3\nEscull el búfer més apropiat per millorar la latència."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "Cadenes d'intercanvi en espera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Sincronització rígida de la CPU i GPU. Redueix la latència a cost de rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Latència de fotograma màxima"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Avisa al controlador de vídeo que faci servir un mode de búfer concret."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Modifica el preajust de shaders que utilitza el menú actualment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Shader predefinit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Shader predefinit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "Shader predefinit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Navegar per la URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "Direcció URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Iniciar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Sobrenom: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Cercant per contingut compatible..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "No s’ha trobat cap nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "No s'han trobat llistes de reproducció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "S'ha trobat contingut compatible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "Error en localitzar el contingut corresponent segons el seu CRC o el nom del fitxer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Iniciar gong"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "Sobrenom (LAN): %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "Estat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Sistema BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Compatibilitat amb l'enregistrament"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "Desa la gravació com a..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "Desa les gravacions a la carpeta assignada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "Veure coincidència #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "Selecciona la coincidència que vols veure."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "Mostra la llista de coincidències"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "Crea un truc a partir d'aquesta coincidència"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "Elimina aquesta coincidència"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "Opacitat del peu de pàgina"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "Modifica l'opacitat del peu de pàgina."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "Opacitat de l'encapçalament"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "Modifica l'opacitat de la capçalera de la pàgina."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Joc en línia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "Iniciar contingut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Ruta de l'historial de continguts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "ID de la pantalla de sortida"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Selecciona el port de sortida connectat a la pantalla CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Ajuda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "Netejar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Solució de problemes d'àudio/vídeo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "Canvia el controlador virtual superposat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "S’està carregant el contingut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "Cercant continguts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "Què és un nucli?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Configuració de la base de dades"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Retardar fotogrames durant el joc en xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Explora la xarxa local"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Cerca amfitrions de joc a la xarxa local i s’hi connecta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Client de joc en xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Espectador al joc en línia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Descripció"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "Limita la velocitat màxima d’execució"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "Cerca un truc nou"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "Cerca un truc nou. Es pot canviar el nombre de bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Continua la cerca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Continua cercant un truc nou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Assoliments (expert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Detalls del truc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Gestiona la configuració dels detalls dels trucs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Comença o continua la cerca de trucs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Comença o continua la cerca de trucs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Passades de trucs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Augmenta o redueix la quantitat de trucs."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Analògic esquerre X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Analògic esquerre Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Analògic dret X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Analògic dret Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Comença o continua la cerca de trucs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Llista de cursors de la base de dades"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Base de dades - Filtre: desenvolupador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Base de dades - Filtre: editor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Base de dades - Filtre: origen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Base de dades - Filtre: franquícia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Base de dades - Filtre: qualificació ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Base de dades - Filtre: qualificació ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Base de dades - Filtre: qualificació PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Base de dades - Filtre: qualificació CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Base de dades - Filtre: qualificació BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Base de dades - Filtre: màxim d’usuaris"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Base de dades - Filtre: mes de publicació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Base de dades - Filtre: any de publicació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Base de dades - Filtre: número de la revista Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Base de dades - Filtre: valoració de la revista Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_GENRE,
   "Base de dades - Filtre: Gènere"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_REGION,
   "Base de dades - Filtre: Regió"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Informació de la base de dades"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Configuració"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Descàrregues"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Configuració del joc en xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Compatibilitat amb Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "Compatibilitat amb OpenGL/Direct3D render-to-texture (shaders multipas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Contingut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "Generalment definit pels desenvolupadors que empaqueten les aplicacions de libretro/RetroArch per apuntar als actius."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Pregunta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Controls bàsics del menú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Confirma/D’acord"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "Informació"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "Sortir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Moure cap amunt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Valors predeterminats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Fer aparèixer el teclat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Fer aparèixer el menú"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "Al menú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "Al joc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "Al joc (en pausa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Jugant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "En pausa"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "El joc en línia començarà quan s’hagi carregat el contingut."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "El contingut s'ha de carregar abans d'iniciar el joc en xarxa."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "No s’ha pogut trobar cap nucli o fitxer de contingut adequats, carregueu-los manualment."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "El vostre controlador de gràfics no és compatible amb el controlador de vídeo actual del RetroArch, s’usarà el controlador %s com a alternativa. Reinicieu el RetroArch per aplicar els canvis."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "S’ha instal·lat el nucli correctament"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Ha fallat la instal·lació del nucli"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Premeu dreta cinc vegades per esborrar tots els trucs."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Volum del mesclador d’àudio global"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "S’ha completat la cerca de partides en línia."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Ho sentim, aquest sistema no està implementat: els nuclis que no requereixen continguts, no poden fer servir. el joc en xarxa."
   )
MSG_HASH(
   MSG_NATIVE,
   "Nativa"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "S’ha rebut una ordre de joc en línia desconeguda"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Aquest fitxer ja existeix. Desant-lo a la memòria cau de còpia de seguretat"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "S’ha rebut una connexió de: «%s»"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "S’ha rebut una connexió de: «%s (%s)»"
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "L'assignació de ports pel joc en xarxa s'ha completat"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "La xarxa externa té una adreça privada o compartida. Podries utilitzar un servidor intermedi."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Error en assignar ports UPnP pel joc en xarxa"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "No s'ha introduït arguments i no hi ha menú integrant, mostrant ajuda..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Introduint disc a la safata"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Esperant al client..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "No es pot connectar a la teva sala des d'internet."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Has deixat el joc"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "T'has unit com a jugador %u"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "Has accedit amb el dispositiu d'entrada %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "El jugador %.*s ha deixat el joc"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s s'ha unit com el jugador %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s ha accedit amb el dispositiu %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYERS_INFO,
   "%d jugador(s)"
   )
MSG_HASH(
   MSG_NETPLAY_SPECTATORS_INFO,
   "%d jugador(s) (%d esperant)"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Una connexió de joc en xarxa ha fallat perquè el client no feia servir RetroArch o feia servir una versió anterior de RetroArch."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Un client del joc en xarxa fa servir una versió antiga de RetroArch. No es pot connectar amb el client."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "AVÍS: Un client del joc en xarxa està fent servir una versió diferent de RetroArch. En cas de trobar errors, es recomana fer servir la mateixa versió."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Un client del joc en xarxa està fent servir un nucli diferent. No es pot connectar amb el client."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "AVÍS: Un client del joc en xarxa està fent servir una versió diferent del nucli. En cas que es trobin errors, es recomana fer servir la mateixa versió."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Aquest nucli no suporta el joc en xarxa entre aquestes plataformes"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Aquest nucli no és compatible amb el joc en xarxa entre diferents plataformes"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Introdueix la contrasenya del servidor de joc:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Escriu el missatge per iniciar la sessió:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Vols permetre la connexió de l'usuari:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Contrasenya incorrecta"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" s'ha desconnectat"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Un client de joc de xarxa s'ha desconnectat"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Joc en xarxa desconnectat"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "No tens permís per jugar"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "No hi ha lloc per més jugadors"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "Els dispositius d'entrada sol·licitats no estan disponibles"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "No es pot canviar el mode de joc"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "El client del joc en xarxa \"%s\" està en pausa"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "El teu sobrenom ha canviat a \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "Client expulsat: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "Error en expulsar al client: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "Client prohibit: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "Error en vetar el client: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Jugant"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "Espectador"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "Dispositius"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "Compatibilitat amb el xat"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "Alentiments provocats"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Volum d'àudio"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Detecció automàtica"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Capacitats"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Connectant al servidor de joc en xarxa"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "S’està connectant al port"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Ranura de connexió"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "S’està recuperant la llista de nuclis..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "No s’ha pogut recuperar la llista de nuclis!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "La darrera versió ja està instal·lada: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "S’està actualitzant el nucli: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "S’està baixant el nucli: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "S’està extraient el nucli: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "S’ha instal·lat el nucli: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "No s’ha pogut instal·lar el nucli: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "S’estan cercant nuclis..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "S’està comprovant el nucli: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Tots els nuclis instal·lats estan actualitzats"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "S’han canviat tots els nuclis admesos a les versions de la Play Store"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "Nuclis actualitzats: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "Nuclis omesos: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "S’ha inhabilitat l’actualització del nucli. El nucli està bloquejat: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "S’estan reiniciant els nuclis: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "S’han reiniciat els nuclis: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "S’està netejant la llista de reproducció: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "S’ha netejat la llista de reproducció: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Ha fallat l’actualització. La llista de reproducció no conté cap registre de cerca vàlid: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Ha fallat l’actualització. El directori de contingut no és vàlid o no hi és: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Ha fallat l’actualització. El nom de sistema no és vàlid o no hi és: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Ha fallat l’actualització. El nucli no és vàlid: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Ha fallat l’actualització. El fitxer DAT d’arcade no és vàlid o no hi és: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Ha fallat l’actualització. El fitxer DAT d’arcade és massa gran (no hi ha prou memòria): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "S'ha afegit als favorits"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Error en afegir preferits: la llista de reproducció està plena"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "S'ha afegit a la llista de reproducció"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Error en afegir a la llista de reproducció: la llista de reproducció està plena"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Nucli definit: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "S'ha restablert l'associació del nucli amb l'element de la llista."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Disc afegit"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Error en afegir el disc"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Directori de l’aplicació"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "S’estan aplicant els canvis de trucs."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "S’està aplicant el pedaç: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "S’està aplicant el shader"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "S’ha silenciat l’àudio."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "S’ha activat l’àudio."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "S’ha produït un error desant el perfil del controlador."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "S’ha desat correctament el perfil del controlador."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "El perfil de controlador s'ha desat a la carpeta corresponent com:\n\"%s\""
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "No s’ha pogut inicialitzar el desament automàtic."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Desa l’estat automàticament a"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "S’està iniciant la interfície de comandes al port"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "No es pot inferir el nou camí de configuració. Useu l’hora actual."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "S’està comparant amb nombres màgics coneguts..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Compilat amb l’API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "No s’ha establert el directori de configuració. No es pot desar la nova configuració."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "S’ha connectat a"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "Els CRC32 dels continguts són diferents. No es poden usar jocs diferents."
   )
MSG_HASH(
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "El servidor està executant un joc diferent."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "El vostre ping és massa alt per aquest amfitrió."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "S’ha omès la càrrega de contingut. La implementació el carregarà pel seu compte."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "El nucli no admet estats desats."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
   "El nucli no té suport per Disc Control."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "S’ha creat correctament el fitxer d’opcions del nucli."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "S’ha eliminat correctament el fitxer d’opcions del nucli."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Totes les opcions del nucli s’han restablert als valors predeterminats."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "S’han desat les opcions del nucli a:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "No s’han pogut desar les opcions del nucli a:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "No s'ha trobat un altre controlador"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "No s'ha pogut trobar un sistema compatible."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "No s'ha trobat una pista de dades vàlida"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "No es pot obrir la pista de dades"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "No es pot llegir el contingut"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "No es pot llegir la capçalera de la repetició."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "No es pot llegir l'estat des de la repetició."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "La suma CRC32 del fitxer no coincideix amb la suma de la repetició. És probable que la repetició no es mostri sincronitzada."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Temps personalitzat donat"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "La descompressió ja està en curs."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "La descompressió ha fallat."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "S’ha detectat una àrea de visualització de"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "No s’ha trobat cap pedaç de contingut vàlid."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Desconnecta el dispositiu d’un port vàlid."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "S’ha tancat la safata de disc virtual."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "S’ha expulsat la safata de disc virtual."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "S’està baixant"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "índex"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Ha fallat la baixada"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "El nucli de libretro necessita contingut, però no n’heu donat cap."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "El nucli de libretro necessita contingut especial, però no n’heu donat cap."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "El nucli no és compatible amb VFS i ha fallat la càrrega des d’una còpia local"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "S’ha produït un error en analitzar els arguments."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "S’ha produït un error en desar el fitxer d’opcions del nucli."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "S’ha produït un error en suprimir el fitxer d’opcions del nucli."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Error en desar el fitxer de reassignació."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Error en eliminar el fitxer d'assignació."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Error en desar el preajust de shaders."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Directori extern de l'aplicació"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Extraient"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Extraient el fitxer"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Error desant la configuració a"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Error en acceptar a l'espectador."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "No s’ha pogut assignar memòria pel contingut apedaçat..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "No s’ha pogut aplicar el shader."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "No s’ha pogut aplicar el valor predefinit del shader:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "No s’ha pogut vincular el sòcol."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "No s’ha pogut crear el directori."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "No s’ha pogut extreure el contingut del fitxer comprimit"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "No s’ha pogut obtenir el sobrenom del client."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Error en carregar."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Error en carregar contingut."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_FROM_PLAYLIST,
   "Error en carregar des d'una llista de reproducció."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Error en carregar el fitxer de vídeo."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "No s’ha pogut carregar la superposició."
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "No s'ha configurat la superposició de teclat."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "No s’ha pogut carregar l’estat de"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "No s’ha pogut obrir el nucli de libretro"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "No s’ha pogut aplicar el pedaç"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "No s’ha pogut rebre la capçalera del client."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "No s’ha pogut rebre el sobrenom."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "No s’ha pogut rebre el sobrenom de l’amfitrió."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "No s’ha pogut rebre la mida del sobrenom de l’amfitrió."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "No s’han pogut rebre les dades de SRAM de l’amfitrió."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "No s’ha pogut treure el disc de la safata."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "No s’ha pogut eliminar el fitxer temporal"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "No s’ha pogut desar la SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "No s’ha pogut carregar la SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "No s’ha pogut desar l’estat a"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "No s’ha pogut enviar el sobrenom."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "No s’ha pogut enviar la mida del sobrenom."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "No s’ha pogut enviar el sobrenom al client."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "No s'ha pogut enviar el sobrenom a l'amfitrió."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "No s'han pogut enviar les dades d'SRAM al client."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "No s'ha pogut iniciar el controlador d'àudio. Es continuarà sense so."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "No s'ha pogut iniciar l'enregistrament de la pel·lícula."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "No s'ha pogut iniciar l'enregistrament."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "No s'ha pogut fer la captura de pantalla."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "No s'ha pogut desfer la càrrega d'estat."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "No s'ha pogut desfer el desament d'estat."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "No s'ha pogut activar el so."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "S'ha produït un error fatal a"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "No s'ha trobat el fitxer"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "S'ha trobat un estat desat automàtic a"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "S'ha trobat l'etiqueta del disc"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "S'ha trobat la primera pista de dades al fitxer"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "S'ha trobat el darrer espai d'estat"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "S'ha trobat l'última posició de repetició"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "No prové de la gravació actual"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "No és compatible amb les repiticions"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_FUTURE_STATE,
   "No es pot carregar l'estat posterior durant la reproducció"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_WRONG_TIMELINE,
   "Error de línia de temps errònia durant la reproducció"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_OVERWRITING_REPLAY,
   "Línia de temps errònia; sobreescrivint la gravació"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT,
   "Cerca enrere"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT_FAILED,
   "Error en la cerca enrere"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT,
   "Cerca endavant"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT_FAILED,
   "Error en la cerca endavant"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME,
   "Cerca completada"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME_FAILED,
   "Error en la cerca"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "S'ha trobat el shader"
   )
MSG_HASH(
   MSG_FRAMES,
   "Fotogrames"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "S'han trobat les opcions de nucli específiques del joc a"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "S'han trobat les opcions de nucli específiques de la carpeta a"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "S'ha obtingut un índex de disc no vàlid."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Captura l'estat del ratolí"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Focus al joc activat"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Focus al joc desactivat"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "La renderització d'aquest nucli es fa per maquinari. Els enregistraments han de tenir shaders aplicats."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "La suma de verificació inflada no coincideix amb el CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Introduir truc"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Introdueix el nom del fitxer de trucs"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Introdueix el nom del fitxer"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "Introdueix el nom del fitxer de personalització"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "Introdueix el nom del fitxer de reassignació"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Reanomenar títol"
   )
MSG_HASH(
   MSG_INTERFACE,
   "Interfície"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Emmagatzematge intern"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Emmagatzematge extraïble"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "La mida del sobrenom no és vàlida."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "en bytes"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "en megabytes"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "en gigabytes"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "està compilat per una versió diferent de libretro que aquesta compilació de libretro."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Interfície d'usuari per Libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Carrega el desat ràpid %d."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Carrega l'estat des de la ranura #-1 (Automàticament)."
   )
MSG_HASH(
   MSG_LOADING,
   "Carregant"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Falten diversos fitxers de firmware"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Carregant el contingut del fitxer"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Carregant el fitxer de l'historial"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Carregant el fitxer de preferits"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Carregant l'estat"
   )
MSG_HASH(
   MSG_MEMORY,
   "Memòria"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "El fitxer de repetició d'entrada no és un fitxer vàlid."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "El format de repetició d'entrada sembla tenir una versió diferent. Probablement serà un error."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "La reproducció s'ha acabat."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Aturant la gravació de la partida."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Error en iniciar el joc en xarxa."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "El nucli no admet joc en xarxa."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "No hi ha contingut, iniciant un nucli buit."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "No s'ha sobreescrit cap desat ràpid."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "No s'ha carregat encara el desat ràpid."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Error en desar la personalització."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "Error en eliminar les personalitzacions."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "S'ha desat la personalització correctament."
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "S'han eliminat les personalitzacions."
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "S'han descarregat les personalitzacions correctament."
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "No hi ha res a desar. No s'han desat les personalitzacions."
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "La funció desat està bloquejada. Les personalitzacions estan actives."
   )
MSG_HASH(
   MSG_PAUSED,
   "En pausa."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Llegint la primera pista de dades..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Gravació acabada pel canvi de mida de la finestra."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Enregistrant a"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Redirigint el fitxer de trucs a"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Redirigint fitxer de desat a"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Redirigint el fitxer d'estats desats a"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "El fitxer de personalització s'ha desat correctament."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "S'ha eliminat el fitxer d'assignació."
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "Totes les opcions d'entrada han sigut restablertes."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Disc eliminat de la safata."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Eliminant el contingut temporal"
   )
MSG_HASH(
   MSG_RESET,
   "Reiniciar"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Reiniciant la gravació a causa del reinici del controlador."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Restaurant un estat desat anterior."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Shaders: Restaurant l'ajust predeterminat a"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Revertint la carpeta de fitxers de desat a"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Revertint la carpeta de fitxers de desats a"
   )
MSG_HASH(
   MSG_REWINDING,
   "Retrocedint."
   )
MSG_HASH(
   MSG_REWIND_BUFFER_CAPACITY_INSUFFICIENT,
   "Capacitat de memòria intermèdia insuficient."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "No està disponible el rebobinat perquè aquest nucli no té suport d'estat desats serialitzats."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Inicialitzant la memòria cau de rebobinat amb la mida"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Error en iniciar la memòria intermèdia de rebobinat. La funció de rebobinat es desactivarà."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "La implementació de l'àudio fa servir multifil. No es pot rebobinar."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "S'ha arribat al final de la memòria intermèdia de rebobinat."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Desa la configuració a"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Estat desat a la ranura #%d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Desat l'estat a la ranura #-1 (Automàtic)."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Desat correctament a"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Desant el tipus de memòria RAM"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Desant estat"
   )
MSG_HASH(
   MSG_SCANNING,
   "Escanejant"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "S'ha acabat l'escaneig del directori."
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Enviant comanda"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Hi ha diversos trucs definits explícitament, ignorant-los tots..."
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "S'ha desat la personalització de shaders correctament."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "A càmera lenta."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Avançament ràpid."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Repetició a càmera lenta."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Ignora la càrrega de SRAM."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "No es desarà la SRAM."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "S’està blocant la sobreescriptura de SRAM"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Reproduint la repetició."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Iniciant gravació de la repetició a"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Mida de l'estat"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Posició de desat"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "Posició de repitició"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "S'està fent una captura de pantalla."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "S'ha desat la captura de pantalla"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Assoliment desbloquejat"
   )
MSG_HASH(
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "Assoliment poc freqüent assolit"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "Intent d'entrar a la taula de classificació iniciat"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "Error en l'intent d'accedir a la taula de classificacions"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "Enviat %s per %s" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "Rang: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "Millor: %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Canvia tipus miniatura"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Miniatures a pantalla complerta"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Mostra metadates"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "No hi ha miniatures disponibles"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "Ja s'han cercat totes les possibles caràtules descarregables per aquest element de la llista de reproducció."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Premeu una altra vegada per sortir..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_CLOSE_CONTENT,
   "Clica una altra vegada per tancar el contingut..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_RESET,
   "Clica una altra vegada per reiniciar..."
   )
MSG_HASH(
   MSG_TO,
   "a"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "S'ha desfet la càrrega del desat ràpid."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Desfer desat ràpid"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Desconegut"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Fi de la pausa."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "S'ha rebut una comanda no reconeguda: \"%s\". \n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Fes servir el nom del nucli per la nova configuració."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Fent servir un nucli buit. Ometent la gravació."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Connecta el dispositiu a un port vàlid."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Desconnecta el dispositiu del port"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Reiniciant..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "S'està apagant..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Versió de la API de Libretro"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Error en calcular la mida de l'àrea de visualització! Se seguirà calculant amb les dades en brut. És possible que hi hagi errors..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Error en obrir la safata de discs virtual."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Error en tancar la safata de discs virtual."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Carrega automàticament el desat ràpid des de"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "Error en carregar automàticament l'estat desat des de \"%s\"."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "S'ha carregat correctament l'estat desat des de \"%s\"."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "configurat al port"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "Configurat %s al port %u"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "desconnectat del port"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "Desconnectat %s del port %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "Sense configurar"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "No ha sigut configurat %s (%u/%u)"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "no està configurat, es fa servir l'opció secundària"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "No ha sigut configurat %s (%u/%u) usant l'opció secundària"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "S’ha completat la cerca de Bluetooth."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "S’ha eliminat l’emparellament. Reinicieu el RetroArch per tornar a connectar o emparellar un dispositiu."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "S’ha completat la cerca de Wi-Fi."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "S’estan cercant dispositius Bluetooth..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "S’estan cercant xarxes sense fil..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "S’està activant la Wi-Fi..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "S’està desactivant la Wi-Fi..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "S’està desconnectant la Wi-Fi..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "S’estan cercant amfitrions pel joc en xarxa..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "S’està preparant per cercar contingut..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Introduïu la contrasenya"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Contrasenya correcta."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Contrasenya incorrecta."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Introduïu la contrasenya"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Contrasenya correcta."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Contrasenya incorrecta."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "S'ha carregat la configuració personalitzada."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "S'ha carregat el fitxer d'assignacions del joc."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Carregat el fitxer d'assignació de la carpeta de continguts."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "S'ha carregat el fitxer d'assignacions del nucli."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "Les opcions d'assignació d'entrada s'han desat a:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "Error en desar les opcions d'assignació d'entrada a:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Reducció predictiva de latència activada. Fotogrames de latència eliminats: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "La reducció predictiva de latència predictiva s'ha activat mitjançant una segona instància. Fotogrames de latència eliminats: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Desactiva l'execució anticipada."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "La reducció predictiva de latència ha sigut desactivada perquè aquest nucli no és compatible amb els desats ràpids."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "La reducció predictiva de latència no està disponible perquè el nucli no suporta els desats ràpids."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Error en desar l'estat. La reducció predictiva de latència ha sigut desactivada."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Error en carregar l'estat. La reducció predictiva de latència ha sigut desactivada."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Error en carregar la segona instància. La reducció predictiva de latència només farà servir una instància a partir d'ara."
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "Sistema de fotogrames preventiu activat. Fotogrames de latència eliminats: %u."
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "Sistemes de fotogrames preventiu desactivat."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "El sistema de fotogrames preventiu ha sigut desactivat perquè aquest nucli no és compatible amb els desats ràpids."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "No està disponible el sistema de fotogrames preventius perquè aquest nucli no suporta els desats ràpids."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "Error en assignar memòria pel sistema de fotogrames preventius."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "Error en desar estat. El sistema de fotogrames preventius s'ha desactivat."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "Error en carregar estat. El sistema de fotogrames preventius s'ha desactivat."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Cerca de fitxers finalitzada."
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "S'ha iniciat correctament la cerca de trucs."
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "No és possible iniciar la cerca de trucs."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "No s'ha iniciat la cerca."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Nou nombre de coincidències = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "S'han afegit %u coincidències."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "No s'han pogut afegir coincidències."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "S'ha creat un truc a partir de la coincidència."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "No s’ha pogut crear el codi."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Coincidència suprimida."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "No queda espai. El màxim són 100 trucs simultanis."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "S’ha afegit un nou truc al capdamunt de la llista."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "S’ha afegit un nou truc al capdavall de la llista."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "S’han suprimit tots els trucs."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "S’ha afegit un nou truc abans d’aquest."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "S’ha afegit un nou truc després d’aquest."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "S’ha copiat un truc abans d’aquest."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "S’ha copiat un truc després d’aquest."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "S’ha suprimit el truc."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Error en seleccionar el disc."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Error en establi l'últim disc que s'ha fet servir."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "Error en connectar amb el client."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Error en connectar al servidor."
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "El servidor de joc està ple."
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "No se't permet entrar en aquest servidor."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Error en rebre l'encapçalament del servidor."
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "Assoliments: Has iniciat sessió com a \"%s\"."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Heu de posar en pausa o desactivar el mode expert d’assoliments per carregar desats ràpids."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_SAVEFILE_PREVENTED_BY_HARDCORE_MODE,
   "Heu de posar en pausa o desactivar el mode expert d’assoliments per carregar desats srm."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "S’ha carregat un desat ràpid. S’ha desactivat el mode expert d’assoliments per la sessió actual."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "S’ha activat un truc. S’ha desactivat el mode expert d’assoliments per la sessió actual."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST,
   "Assoliments Hardcore canviats pel servidor."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST,
   "El servidor de joc en xarxa necessita una actualització. Els assoliments Hardcore han sigut desactivats per aquesta sessió."
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "Completat %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "S’ha activat el mode expert d’assoliments; s’han desactivat els desats ràpids i el rebobinat."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "Aquest joc no té assoliments."
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "S'han activat tots els assoliments %d en aquesta sessió"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "S'han activat els %d assoliments no oficials"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "Has desbloquejat %d de %d assoliments"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "%d no compatible"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_WARNING,
   "S'han trobat assoliments que no són compatibles. Proveu amb un nucli diferent o actualitzeu el RetroArch."
)
MSG_HASH(
   MSG_CHEEVOS_RICH_PRESENCE_SPECTATING,
   "Expectant %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "Mode Hardcore en pausa. No es pot ajustar manualment el retard entre fotogrames de vídeo."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL,
   "Mode Hardcore en pausa. No es permet un valor d'intercanvi de sincronització vertical superior a 1."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION,
   "Mode Hardcore en pausa. No es poden inserir fotogrames negres."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED,
   "Mode Hardcore pausat. No es pot canviar: %s=%s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE,
   "Mode Hardcore en pausa. No pots desbloquejar assoliments Hardcore de %s amb %s"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "RetroAchievements: No es pot identificar el joc."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "Error en carregar el joc a Assoliments: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "Error en canviar els medis a RetroAchievements: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_LOGIN_TOKEN_EXPIRED,
   "L'inici de sessió a RetroAchievements ha expirat. Reintrodueix, si us plau, la teva contrasenya i recarrega el joc."
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "Més baix"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Més baix"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Més alt"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Més alt"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Avís: falten recursos, fes servir l'actualitzador en línia si està disponible."
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Atenció: falten tipografies per la llengua seleccionada, useu l’actualitzador en línia si està disponible."
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Avís: Llengua no suportada - Canviant a anglès."
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Abocant el disc..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Unitat %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Carrega un nucli en primer lloc, si us plau."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Error en llegir des de la unitat. S'ha avortat el procés."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Error en escriure al disc. S'ha avortat el procés."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "No hi ha un disc dins del lector."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "S'ha eliminat la configuració de shaders."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Error en eliminar la configuració de shaders."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Has seleccionat un fitxer DAT de arcade no vàlid."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "El fitxer de dades DAT seleccionat és massa gran (memòria lliure insuficient)."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Error en carregar el fitxer DAT (format invàlid?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Configuració de cerca manual no vàlida."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "No s'ha detectat contingut vàlid."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Escanejant contingut: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Comprovant els elements actuals: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Escanejant: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Netejant les etiquetes M3U: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Cerca finalitzada: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Cercant al nucli: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Ja existeix una còpia de seguretat del nucli instal·lat: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Còpia de seguretat del nucli: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Eliminant còpies de seguretat obsoletes: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Còpia de seguretat del nucli completada: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "La còpia de seguretat del nucli seleccionat ja està instal·lada: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Restaurant els nuclis: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "S'ha restaurat el nucli: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "El nucli seleccionat ja està instal·lat: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Instal·lant el nucli: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Instal·lació completa del nucli: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "El nucli seleccionat no és vàlid: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Error en crear la còpia de seguretat del nucli: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Error en restaurar el nucli: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Ha fallat la instal·lació del nucli: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Restauració de nucli desactivada, el nucli està protegit: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "S’ha inhabilitat la instal·lació del nucli. El nucli està bloquejat: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Error en bloquejar el nucli: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Error en desbloquejar el nucli: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "No s'ha pogut eliminar el nucli de la llista «Nuclis sense continguts»: "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "No s'ha pogut afegir el nucli a la llista «Nuclis sense continguts»: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "S’ha inhabilitat l’eliminació del nucli. El nucli està bloquejat: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "El mode de vídeo no és compatible"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "No es pot escriure a la carpeta d'informació del nucli: es desactivarà la memòria intermèdia d'informació del nucli"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "S'ha trobat un desat ràpid a"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Carregant un desat ràpid des de"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "Error en entrar al GameMode"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "Error en entrar a GameMode - comprova que el servei de GameMode està instal·lat i en funcionament"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "S'ha activat la sincronització a la velocitat de fotogrames del contingut exacta."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "S'ha desactivat la sincronització a la velocitat de fotogrames del contingut exacta."
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "S'ha canviat la freqüència d'actualització a %s Hz."
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Actualitzar Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Nom de la interfície visual"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Versió de Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Reiniciar"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Separa els Joy-Con"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Personalitza l'escala de widgets gràfics"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Força un factor d’escalat manual quan es dibuixen ginys de visualització en mode de finestra. Només s’aplica quan l’opció «Escala automàticament els ginys gràfics» està desactivada. Es pot fer servir per augmentar o disminuir la mida de les notificacions decorades, indicadors i controls independentment del menú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Resolució de pantalla"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_FORMAT_NO_DESC,
   "%u×%u"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_FORMAT_DESC,
   "%u×%u - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Resolució de la pantalla: Per defecte"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Resolució de pantalla: %d x %d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Resolució de la pantalla: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Aplicant: predefinit"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Aplicant: %dx%d\nINICI per reiniciar"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Aplicant: %dx%d - %s\nSTART per reiniciar"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Reiniciant a: Predeterminat"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Reiniciant a: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Restablint a: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Selecciona el mode de vídeo (Es requereix reiniciar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Apagar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Permet l'accés a fitxers externs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Obre la configuració de permisos d'accés de fitxers a Windows"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Obre la finestra de configuració de permisos de Windows per activar el permís a broadFileSystemAccess."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Obrir..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Obre una altra carpeta mitjançant l'explorador de fitxers del sistema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Filtre de parpelleig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Gamma del vídeo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Filtre suau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Escaneja dispositius bluetooth i connecta-hi."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Escaneja per xarxes wifi i estableix la connexió."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Activant la WIFI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Connecta a una xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Connecta a una xarxa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Desconnecta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Antipampallugueig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "Estableix l’amplada de la pantalla de la VI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Correcció d’overscan (superior)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Estableix la retallada de la imatge traient un nombre concret de línies (a partir de la part superior de la pantalla). Pot provocar defectes d'escalat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Correcció d’overscan (inferior)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Estableix la retallada de la imatge traient un nombre concret de línies (a partir de la part inferior de la pantalla). Pot provocar defectes d'escalat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Mode de rendiment sostingut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "Rendiment i potència de la CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "Política"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Mode de governança"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Permet ajustar manualment tots els detalls en cada CPU: governador, freqüències, etc. Es recomana només per a usuaris avançats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Rendiment (gestionat)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Mode per defecte i recomanat. Ofereix el màxim rendiment mentre es juga i permet estalviar energia durant les pauses i en els menús."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Personalitzat gestionat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Permet triar quins governadors es faran servir als menús i durant el joc. Durant el joc es recomana fer servir Performance, Ondemand o Schedutil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Màxim rendiment"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Sempre el màxim rendiment: les freqüències més altes per obtenir la millor experiència."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Mínima potència"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Usa la freqüència més baixa disponible per estalviar energia. És útil en dispositius que funcionen amb bateria però el rendiment baixa significativament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Equilibrat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "S’adapta a la càrrega actual. Funciona bé amb la majoria de dispositius i emuladors i ajuda a estalviar energia. Els jocs i nuclis més exigents poden perdre rendiment en alguns dispositius."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Mínima freqüència"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Màxima freqüència"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Mínima freqüència del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Màxima freqüència del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "Governador de la CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Governador del nucli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Governador del menú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "Mode de joc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Pot millorar el rendiment, reduir la latència i arreglar problemes d’àudio crepitant. Necessitareu https://github.com/FeralInteractive/gamemode perquè funcioni."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Activar del mode de joc de Linux pot millorar la latència, corregir errors en l'àudio i maximitzar el rendiment general configurat automàticament la teva CPU i la teva GPU per optimitzar el seu rendiment.\nCal tenir instal·lat el programari GameMode perquè aquesta opció funcioni. Si necessites més informació sobre GameMode, visita la pàgina https://github.com/FeralInteractive/gamemode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Usa el mode PAL60"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Reiniciar RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Surt i reinicia RetroArch. És necessari per activar certes configuracions del menú (per exemple, canviar el controlador del menú)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Bloquejant fotogrames"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Preferència a l'entrada tàctil frontal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "Fes servir el toc frontal en comptes del toc posterior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Tàctil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Configuració del mode teclat del controlador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Tipus de configuració del mode teclat del controlador"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Teclat petit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Temps límit pel bloqueig d'entrada"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Indica el nombre de mil·lisegons a esperar per aconseguir una mostra completa d'entrada. Fes-ho servir en cas de tenir problemes en clicar diversos botons a la vegada (només a Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Mostrar Reiniciar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Mostra l'opció 'Reiniciar'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Mostra 'Apagar'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Mostra l'opció 'Apagar'."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Amb contrasenya"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "Internet (servidor intermediari)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "Internet (No hi ha connexió)"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Estat de l'emmagatzematge intern: Lectura/Escriptura"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Estat de l'emmagatzematge intern: Només lectura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Brillantor de la pantalla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Augmenta o redueix la brillantor de la pantalla."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "Força la CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Augmenta la velocitat de rellotge de la CPU de la Switch."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Determina l’estat del Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Serveis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Gestiona els serveis del sistema operatiu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Comparteix carpetes de xarxa mitjançant el protocol SMB."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Usa SSH per accedir a la línia d’ordres remotament."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Punt d’accés Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Activa o desactiva el punt d’accés Wi-Fi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Zona horària"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Seleccioneu la zona horària per ajustar la data i l’hora a la vostra ubicació."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "Mostra una llista de fusos horaris disponibles. Després de selecciona un fus horari, l'hora i la data s'ajusten a aquest. Es dona per suposat que el rellotge del sistema o del maquinari està posat a UTC."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Opcions per a Nintendo Switch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Administra les opcions específiques per la Nintendo Switch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "Força la CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "Permet que es facin servir altres freqüències de rellotge amb la CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "Suport CEC"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "Habilita el protocol de senyals CEC amb el televisor en el mode televisor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "Deshabilitar bluetooth ERTM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "Desactiva el ERTM del senyal Bluetooth per corregir l'emparellament d'alguns dispositius"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "S’està apagant el punt d’accés Wi-Fi."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "S’està desconnectat de la Wi-Fi «%s»"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "S’està connectant a la Wi-Fi «%s»"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Sense SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "El punt d’accés Wi-Fi ja està actiu"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "El punt d’accés Wi-Fi no està actiu"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "S’està iniciant el punt d’accés Wi-Fi amb SSID=%s i contrasenya=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "No s’ha pogut crear el fitxer de configuració del punt d’accés Wi-Fi."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Fitxer de configuració erroni — no s’han trobat APNAME ni PASSWORD a %s"
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Escala del ratolí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Ajusta l’escala x/y per la velocitat de la pistola de llum del Wiimote."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Escala tàctil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "Ajusta l’escala x/y de les coordenades de la pantalla tàctil per adaptar-les a l’escala de visualització del sistema operatiu."
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "Fes servir el ratolí virtual (VMouse) com a punter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "Activa aquesta opció per enviar els tocs a la pantalla tàctil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "Empra el ratolí virtual (VMouse) com a ratolí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "Activa l'emulació de ratolí virtual fent servir els tocs a la pantalla tàctil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Mode «Touchpad» del ratolí virtual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Activa aquesta opció juntament amb el ratolí per utilitzar la pantalla tàctil com a panell tàctil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Mode «Trackball» del ratolí virtual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Activa aquesta opció juntament amb 'com ratolí' per fer servir la pantalla tàctil com si fos un ratolí de bola, afegint inèrcia al cursor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "Gestos del ratolí virtual"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "Activa els gestos a la pantalla tàctil, incloent-hi clicar, arrossegar i combinacions de dits."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "Escalat RGA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "Escalat d'imatge que fa servir RGA i filtratge bicúbic. Pot fer malbé els widgets."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Escalat segons el context"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Escalat segons el context de maquinari (si està disponible)."
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "Activa l'acceleració de CPU de New3DS i memòria intermèdia L2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "Activa la velocitat màxima de la CPU de New3DS (804 MHz) i la memòria intermèdia L2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "Pantalla inferior de la 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Habilita la visualització de la informació de l'estat a la pantalla inferior. Desactiva aquesta opció per millorar la duració de la bateria i el rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "Mode de pantalla 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Escull entre els modes 3D i 2D. En el mode 3D, els píxels són quadrats i s'aplica un efecte de profunditat quan es veuen en el menú ràpid. El mode 2D dona un millor rendiment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (Efecte de graella de píxels)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (Alta resolució)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Toca aquí per anar al menú de RetroArch"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "No s'han trobat els recursos"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Sense\nDades"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "No hi ha captures de pantalla"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Continuar la partida"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Crea un punt de restauració"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Carrega el punt de restauració"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "Directori de recursos de la pantalla inferior"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "Indica la carpeta de recursos de la pantalla inferior. La carpeta ha de tenir un fitxer \"bottom_menu.png\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "Habilitar la font"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "Mostra la font del menú de la pantalla inferior i les descripcions dels botons, excepte la data dels desats ràpids."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Color vermell de la font"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Ajustant el color vermell de la font de la pantalla inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Color de text verd"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Ajusta el color verd de la font de la pantalla inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Color blau de la font"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Ajusta el color blau de la font de la pantalla inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Opacitat del color de text"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "Ajusta l'opacitat de la font de la pantalla inferior."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "Mida de la font"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "Ajusta l'escala de la font a la pantalla inferior."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Escaneig finalitzat.<br><br>\nPer endreçar correctament el contingut escanejat, cal que:\n<ul><li>tinguis un nucli compatible descarregat</li>\n<li>tinguis els fitxers d'informació del nucli actualitzats a través de l'actualitzador en línia</li>\n<li>tinguis la base de dades actualitzada a través de l'actualitzador en línia</li>\n<li>reiniciar RetroArch si acabes de complir alguna de les condicions anteriors</li></ul>\nFinalment, el contingut ha de coincidir amb la base de dades existent <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">aquí</a>. Si encara no funciona correctament, pots <a href=\"https://www.github.com/libretro/RetroArch/issues\">enviar un error</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "L'ús del ratolí amb la pantalla tàctil està activat"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "Ús del ratolí amb la pantalla tàctil desactivat"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "El micròfon sdl2 necessita un controlador d'àudio sdl2"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "L'accessibilitat de RetroArch està activada. Menú principal, carregar nucli."
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "aturat."
   )
#ifdef HAVE_GAME_AI
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_MENU_OPTION,
   "Anul·lar el jugador de IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_MENU_OPTION,
   "Descripció d'anul·lació del jugador de IA"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_GAME_AI_OPTIONS,
   "IA del joc"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P1,
   "Sobreescriu el p1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P1,
   "Anul·la el jugador 01"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P2,
   "Sobreescriu el p2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P2,
   "Anul·la el jugador 02"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_SHOW_DEBUG,
   "Mostra la depuració"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG,
   "Mostra la depuració"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_GAME_AI,
   "Mostrar «IA del joc»"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_GAME_AI,
   "Mostra l'opció 'IA del joc'."
   )
#endif

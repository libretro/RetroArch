#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

/*
##### NOTE FOR TRANSLATORS ####

PLEASE do NOT modify any `msg_hash_*.h` files, besides `msg_hash_us.h`!

Translations are handled using the localization platform Crowdin:
https://crowdin.com/project/retroarch

Translations from Crowdin are applied automatically and will overwrite
any changes made to the other localization files.
As a result, any submissions directly altering `msg_hash_*.h` files
other than `msg_hash_us.h` will be rejected.
*/

/* Top-Level Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN_MENU,
   "Ĉefa menuo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Agordoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Preferataj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Historio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Bildoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Muziko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Videoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Retludo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Esplori"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Malplenaj kernoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importi enhavon"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Rapida menuo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Rapide aliri ĉiujn rilatajn en-ludajn agordojn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Ŝargi kernon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Elekti kernon uzotan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "Malŝargi kernon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Malteni la ŝargitan kernon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Foliumu por libretron kernon. Kien la retumilo komenciĝas dependas de via Kerna Dosieruja vojo. Se ĝi estus malplena, ĝi komenciĝus en radiko. \nSe Kerna Dosierujo estus dosierujo, la menuo uzus tion por la plej supra dosierujo. Se la Kerna Dosierujo estus plena, ĝi komenciĝus en la dosierujo, kiu la dosiero estas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Ŝargi enhavon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Elekti enhavon lanĉotan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Foliumu por enhavo. Por ŝarĝi enhavon, vi bezonas 'Kernon' por uzi, kaj enhavan dosieron. \nPor kontroli, kie la menuo ekfoliumas por enhavoj, elektu 'Dosieran Retumilan Dosierujon'. Se ĝi ne elektus, ĝi komenciĝus en Radiko.\nLa retumilo filtros por etendaĵoj por la plej freŝa kerno, kiu elektis en 'Ŝarĝi Kernon', kaj uzos tiun kernon, kiam enhavo ŝarĝiĝos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Ŝargi diskon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Ŝargu fizikan enhavan diskon. Unue elektu la kernon (Ŝargi kernon) por uzi kun la disko."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Forĵeti diskon"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Forĵeti la fizikan enhavan diskon al interna stokado. Ĝi estos konservita kiel bilda dosiero."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Elĵeti diskon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Elĵeti la diskon el la fizika CD/DVD ludilo."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Ludlistoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Skanita enhavo, kiu kongruos kun la datumbazo, aperos ĉi tie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Importi enhavon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Krei kaj ĝisdatigi ludlistojn skanante enhavojn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Montri labortablan menuon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Malfermi la tradician labortablan menuon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Malebligi envicigan reĝimon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Montri ĉiujn agordojn rilatantajn kun konfiguro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Reta ĝisdatigilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Elŝuti kromaĵojn, komponantojn kaj enhavojn por RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Retludo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Aliĝi al retluda sesio aŭ gastigi iun."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Agordoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Konfiguri la programon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Montri sistemajn informojn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Konfigura dosiero"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Administri kaj krei konfigurajn dosierojn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Helpo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Lerni pli pri kiel la programo funkcias."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Restartigi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Restartigi aplikaĵon RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Forlasi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Forlasi aplikaĵon RetroArch. Konfigura konservado ĉe forlaso estas ebligita."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Forlasi aplikaĵon RetroArch. Konfigura konservado ĉe forlaso estas malebligita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Forlasi RetroArch. Ĉesigi la programon per ajna severa maniero (SIGKILL, kc) ĉesigos RetroArch sen konservi la konfiguraĵon ajnakaze. En Uniks-similaj sistemoj, SIGINT/SIGTERM ebligas puran forlason, kio inkluzivas konfiguran konservon se ebligita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_NOW,
   "Sinkronigi nun"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_NOW,
   "Permane lanĉi nuban sinkronigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Solvi konfliktojn: konservi lokalaĵojn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Solvi ĉiujn konfliktojn alŝutante lokalajn dosierojn en la servilon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Solvi konfliktojn: konservi servilaĵojn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Solvi ĉiujn konfliktojn elŝutante servilajn dosierojn, anstataŭigante lokalajn kopiojn."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Elŝuiti kernon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Elŝuti kaj instali kernon el la reta ĝisdatigilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Instali aŭ restaŭri kernon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Instali aŭ restaŭri kernon el la dosierujo \"Elŝutaĵoj\"."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Komencigi videtraktilon"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Komenci foran RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Komenca dosierujo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Elŝutaĵoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Foliumi arĥivon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Ŝargi arĥivon"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Preferataj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Enhavo aldonita al \"Preferataj\" aperos ĉi tie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Muziko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Jam ludita muziko aperos ĉi tie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Bildoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Jam viditaj bildoj aperos ĉi tie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Videoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Jam luditaj videoj aperos ĉi tie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Esplori"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Foliumi ĉiun enhavon kongruantan kun la datumbazo per kategoriigita serĉinterfaco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Malplenaj kernoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Instalitaj kernoj, kiuj povas funkcii sen ŝargi enhavon, aperos ĉi tie."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Elŝutilo de kernoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Ĝisdatigi instalitajn kernojn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Ĝisdatigi ĉiujn instalitajn kernojn ĝis la plej lasta disponebla versio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Ŝanĝi kernojn de Switch al versioj de Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Anstataŭigi ĉiujn malnovajn kaj permane instalitajn kernojn kontraŭ la plej lastaj versioj el la Play Store, kiam ajn disponeblaj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Ĝisdatigilo de ludlistaj bildetoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Elŝuti bildetojn por eroj de la elektita ludlisto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Elŝutilo de enhavoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Elŝuti senkostajn enhavojn por la elektita kerno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Elŝutilo de dosieroj de la sistemo de la kernoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Elŝuti helpajn sistemdosierojn bezonatajn por la ĝusta funkciado de la kernoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Ĝisdatigi informajn dosierojn de kernoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Ĝisdatigi havaĵojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Ĝisdatigi regilajn profilarojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Ĝisdatigi trompkodojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Ĝisdatigi datumbazojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Ĝisdatigi supermetaĵojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Ĝisdatigi ombrigilojn GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Ĝisdatigi ombrigilojn Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Ĝisdatigi ombrigilojn Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Kernaj informoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Vidi informojn aktualajn por la aplikaĵo aŭ kerno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Diskaj informoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Vidi informojn pri enigitaj aŭdvidaĵaj diskoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Retaj informoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Vidi retaj interfacoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Informoj de sistemo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Vidi informojn specifajn de la aparato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Administrilo de datumbazoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Vidi datumbazojn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Administrilo de kursoroj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Vidi antaŭajn serĉojn."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Nomo de la kerno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Etikedo de la kerno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Versio de la kerno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Nomo de la sistemo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Fabrikinto de la sistemo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Kategorioj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Kreinto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Permesoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Permesilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Subtenataj kromaĵoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Grafikaj API bezonataj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Plena dosierindiko"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Subteno de rapidkonservado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Neniu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Baza (konservi/ŝargi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Seriigita (konservi/ŝargi, revolvi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Determinisma (konservi/ŝargi, revolvi, , retludo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Mikroprogramaro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "Noto: la opcio \"Sistemdosieroj estas en Enhavdosierujo\" estas ebligita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "Serĉante en: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Mankanta, bezonata:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Mankanta, opcia:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Estanta, bezonata:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Estanta, opcia:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Ŝlosi instalita kerno"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Malebligu modifon de la kurante instalita kerno. Povas esti uzata por malhelpi nedeziratajn ĝisdatigojn kiam enhavoj bezonos specifan kernan version (ekz. ROM-aroj de Arcade) aŭ ŝanĝojn de la rapidkonserva formato de la propra kerno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Eksigi el menuo \"Senenhavaj kernoj\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Malebligi, ke ĉi tiu kerno estu videbla en la sekcio/menuo \"Senenhavaj kernoj\". Nur aplikiĝas kiam montromodo estas agordita je \"Adaptita\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Forigi kernon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Forigi ĉi tiun kernon el la disko."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Rezerva kerno"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Krei arĥivitan restaŭrkopion de la kurante instalita kerno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Restaŭri restaŭrkopion"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Instali antaŭan version de la kerno el listo de arĥivitaj restaŭrkopioj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Forigi restaŭrkopiojn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Forigi dosieron el la listo de arĥivitaj restaŭrkopioj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[Aǔtomate]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Dato de muntado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "Versio de RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Versio de Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Programtradukilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Modelo de CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Trajtoj de CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Arĥitekturo de CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Kernoj de CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT disponebla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUNDLE_IDENTIFIER,
   "Identigilo de pakaĵo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Identigilo de fasado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Operaciumo de fasado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Kurentofonto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Pelilo de videokunteksto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Larĝo de bildo (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Alto de bildo (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   ""
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Nomo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Priskribo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Ĝenro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Akiroj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Kategorio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Lingvo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Regiono"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Ekskluzivaĵo de ludkonzolo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Ekskluzivaĵo de platformo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Poentaro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Komunikilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Regiloj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Artstilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Ludspeco"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Rakontspeco"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "Ludritmo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Perspektivo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Etoso"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Vidigstilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Veturilspeco"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Eldonisto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Programisto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Origino"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Franĉizo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Rangigo de TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Rangigo de revuo Famitsu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Recenzo de revuo Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Rangigo de revuo Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Numero de revuo Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Monato de eldondato"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Jaro de eldondato"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Rangigo de BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Rangigo de ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Rangigo de ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Rangigo de PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Pliboniga aparataro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Rangigo de CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Seria numero"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Subteno de analoga regilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   ""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Subteno de kunhelpa ludo"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Ŝargi konfiguron"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Ŝargi ekzistantan konfiguron kaj anstataŭigi kurantajn valorojn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Konservi kurantan konfiguron"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Anstataŭigi kurantan konfiguran dosieron."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Konservi novan konfiguron"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Konservi kurantan konfiguron en aparta dosiero."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_AS_CONFIG,
   "Konservi konfiguron kiel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG,
   "Konservi kurantan konfiguron kiel propra konfigura dosiero."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_MAIN_CONFIG,
   "Konservi ĉefan konfiguron"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG,
   "Konservi ĉefan konfiguron kiel ĉefa konfiguro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Restarigi defaŭltajn valorojn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Restarigi kurantan konfiguron kun defaŭltajn valorojn."
   )

/* Main Menu > Help */

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Rulumi supren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Rulumi malsupren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Konfirmi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "Informojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Starti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Baskuligi menuon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Forlasi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Baskuligi klavaron"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Peliloj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Ŝanĝi pelilojn uzatajn de la sistemo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Videa pelilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Ŝanĝi la agordojn pri videa eligo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Sona pelilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Ŝanĝi la agordojn pri sona enigo kaj eligo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Eniga pelilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Ŝanĝi la agordojn pri regilo, klavaro kaj muso."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Respondotempo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Ŝanĝi agordojn rilatantajn al respondotempo de video, sono kaj enigo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Kerno"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Ŝanĝi agordojn pri la kernoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Konfiguro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Ŝanĝi la defaŭltajn agordojn por la konfiguraj dosieroj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Konservado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Ŝanĝi la agordojn pri konservado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Sinkronigo kun la nubo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Ŝanĝi agordojn pri sinkronigo kun la nubo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Ebligi nubsinkronigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Provi sinkronigi konfigurojn, SRAM kaj statojn al provizanto de nuba konservejo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Detrua nubsinkronigo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Sinkronigi: konservaĵojn/statojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Sinkronigi: konfigurajn dosierojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Sinkronigi: bildetojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Sinkronigi: sistemaj dosieroj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "Kiam ebligita, konservaĵoj/statoj estos sinkronigitaj al la nubo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "Kiam ebligita, konfiguraj dosieroj estos sinkronigitaj al la nubo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "Kiam ebligita, bildetoj estos sinkronigitaj al la nubo. Ĝenerale, ne rekomendata krom por grandaj kolektoj el propraj bildetoj; alikaze, la bildeta elŝutilo estas pli bona elekto."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "Kiam ebligita, sistemaj dosieroj estos sinkronigitaj al la nubo. Ĉi tio povas grave pliigi la tempon bezonata por sinkronigi; uzu singarde."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Kiam malebligita, dosieroj estas movitaj al restaŭrkopia dosiero antaŭ ol esti anstataŭigitaj aŭ forigitaj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE,
   "Sinkroniga reĝimo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_MODE,
   "Aŭtomata: sinkronigi ĉe komenciĝo de RetroArch kaj kiam kernoj estas malŝargitaj.Permana: nur sinkronigi kiam la butono \"Sinkronigi nun\" estas permane ekagigita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_AUTOMATIC,
   "Aŭtomata"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_MANUAL,
   "Permana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Nubsinkroniĝa servilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Kiun nubsinkroniĝan retan protokolon uzi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "Nubkonserveja URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "La URL por la enirejo de la API al la nubkonserveja servo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Uzantnomo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "Via uzantnomo por via nubkonserveja konto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Pasvorto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "Via pasvorto por via nubkonserveja konto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ACCESS_KEY_ID,
   "Identigilo de alirklavo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ACCESS_KEY_ID,
   "La identigilo de via alirklavo por via nubkonserveja konto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SECRET_ACCESS_KEY,
   "Sekreta alirklavo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SECRET_ACCESS_KEY,
   "Via sekreta alirklavo por via nubkonserveja konto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_S3_URL,
   "URL de S3"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_S3_URL,
   "La URL de la finpunkto de S3 de via nuba konservejo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Protokolado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Ŝanĝi agordojn pri protokolado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Dosieresplorilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Ŝanĝi agordojn pri la dosieresplorilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Konfigura dosiero."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Densigita arĥiva dosiero."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Konfigura dosiero por registradoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Dosiero de datumbazo de kursoroj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Dosiero de konfiguraĵoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Dosiero de antaŭagordoj de ombrigilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Dosiero de ombrigilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Dosiero de reasignado de regiloj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Dosiero de trompaĵo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Dosiero de supermetaĵo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Dosiero de datumbazo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "Dosiero de tiparo TrueType."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Simpla dosiero."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Video. Elektu ĝin por malfermi ĉi tiun dosieron per la filmetoludilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Muziko. Elektu ĝin por malfermi ĉi tiun dosieron per la muzikludilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Bilda dosiero."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Bildo. Elektu ĝin por malfermi ĉi tiun dosieron per la bildmontrilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Kerno de Libretro. Elekti ĉi tion ligos ĉi tiun kernon al la ludo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Kerno de Libretro. Elektu ĉi tiun dosieron por farigi RetroArch ŝargi la kernon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Dosierujo. Elektu ĝin por malfermi ĉi tiun dosierujon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Limigo de filmertrafikon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Ŝanĝi agordojn pri revolvi, rapidpluigi kaj malrapidpluigi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Registrado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Ŝanĝi agordojn pri registri."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Surekrana montraĵo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Ŝanĝi agordojn pri surmetaĵoj bildaj kaj klavaraj, kaj surekranaj sciigoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Fasado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Ŝanĝi agordojn pri la fasado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "AI-servo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Ŝanĝi agordojn pri la AI-servo (traduko/parolsintezo/ks)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Alireblo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Ŝanĝi agordojn pri la alirebla rakontilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Kurentomastrumado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Ŝanği agordojn pri kurentomastrumado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Akiroj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Ŝanĝi agordojn pri akiroj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Reto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Ŝanĝi agordojn pri servilo kaj reto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Ludlistoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Ŝanĝi agordojn pri ludlisto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Uzanto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Ŝanĝi agordojn pri privateco, konto kaj uzantnomo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Dosierujo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Ŝanĝi la defaŭltajn dosierujojn, kie dosieroj estas lokataj."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS,
   "Kodumoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Mapado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Komunikilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Rendimento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Sono"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Specifoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Konservejo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Sistemo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Tempomezurado"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_SETTINGS,
   ""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Ŝanĝi agordojn rilatantajn al Steam."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Enigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Eniga pelilo uzota. Kelkaj videaj peliloj devigas certan enigan pelilon. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "La pelilo udev legas eventojn evdev por subteno de klavaro. Ĝi ankaŭ subtenas klavarajn revokojn, musojn kaj tuŝplatojn.\nLa plej multo el la distribuaĵoj defaŭlte havas la nodojn /dev/input en reĝimoj por nur root (reĝimo 600). Oni povas konfiguri regulon, por ke ili estu alireblaj al aliaj uzantoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "La eniga pelilo linuxraw bezonas aktivan TTY. Klavaraj eventoj estas legataj rekte el la TTY, kio faras ĝin pli simpla, sed ne tiel fleksebla kiel udev. Musoj, kc., tute ne estas subtenataj. Ĉi tiu pelilo uzas la pli malnovan API por la stirstango (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   ""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Regilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Regila pelilo uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "Pelilo de regilo de DirectInput."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "Aparata pelilo de homa interfaco de malalta nivelo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Pelilo de Linux uzanta krudajn datumojn, uzas malnovan API de stirstango. Anstataŭe, uzu udev se eble."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Pelilo de LInux por regiloj konektitaj al paralela konektejo per specialaj adaptiloj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Regila pelilo bazita sur bibliotekoj SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Regila pelilo kun interfaco udev, ĝenerale rekomendata. Uzas la lasta stirstanga API evdev por subteno de stirstango. Ĝi subtenas dumkure permutebladon kaj preman respondreagon.\nDefaŭlte en la plej multo de la distribuaĵoj, nodoj de /dev/input estas nur por root (reĝimo 600). Oni povas agordi regulon de udev, kio faros ĉi tiujn alireblaj por ne-root."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "Regila pelilo XInput. Ĉefe por regiloj de XBox."
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Videa pelilo uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "Pelilo de OpenGL 1.x. Minimuma versio bezonata: OpenGL 1.1. Ne subtenas ombrigilojn. Anstataŭe, uzu postajn pelilojn OpenGL se eble."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "Pelilo de OpenGL 2.x. Ĉi tiu pelilo ebligas al kernoj de libretro GL esti uzataj aldone al kernoj bildigitaj per programaro. Minimuma versio bezonata: OpenGL 2.0 aŭ OpenGLES 2.0. Subtenas la formaton de ombrigilo GLSL. Anstataŭe, uzu pelilon glcore se eble."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "Pelilo de OpenGL 3.x. Ĉi tiu pelilo ebligas al kernoj de libretro GL esti uzataj aldone al kernoj bildigataj per programaro. Minimuma versio bezonata: OpenGL 3.2 aŭ OpenGLES 3.0+. Subtenas la formaton de ombrigilo Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Pelilo de Vulkan. Ĉi tiu pelilo ebligas al kernoj de libretro Vulkan esti uzataj aldone al kernoj bildigataj per programaro. Minimuma versio bezonata: Vulkan 1.0. Subtenas ombrigilojn HDR kaj Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "Pelilo bildigita per programaro de SDL 1.2. Rendimento estas taksata kiel esti suboptimuma. Konsideru uzi ĝin nur lastrimede."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "Pelilo bildigita per programaro de SDL 2. Rendimento por realigoj de kernoj de libretro bildigitaj per programaro dependas de la realigo de SDL de via platformo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Metala pelilo por platformoj de Apple. Subtenas la ombrigilan formaton Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Pelilo de Direct3D 8 sen subteno de ombrigiloj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Pelilo de Direct3D 9 kun subteno por la malnova formato de ombrigilo Cg."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Pelilo de Direct3D 9 kun subteno por la formato de ombrigilo HLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Pelilo de Direct3D 10 kun subteno por la formato de ombrigilo Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Pelilo de Direct3D 11 kun subteno por la formatoj de ombrigilo HDR kaj Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Pelilo de Direct3D 12 kun subteno por la formato de ombrigilo HDR kaj Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "Pelilo de DispmanX. Uzas la API DispmanX por la GPU Videocore IV en Raspberry Pi 0..3. Neniu subteno por surmetaĵoj aŭ ombrigiloj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "Pelilo de LibCACA. Produktas signa eligo anstataŭ grafikoj. Ne rekomendata por praktika uzado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Malaltnivela videa pelilo de Exynos, kiu uzas la bloko G2D en Samsung Exynos SoC por operacioj \"blit\". Rendimento por kernoj bildigitaj per programaro devus esti optimuma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Simpla videa pelilo de DRM. Ĉi tiu estas malaltnivela videa pelilo uzante libdrm por aparatara skalado uzante supermetaĵoj de GPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Malaltnivela videa pelilo de Sunxi uzanta la bloko G2D en Allwinner SoCs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Pelilo de Wii U. Subtenas ombrigilojn Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Pelilo de Switch. Subtenas la ombrigila formato GLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "Pelilo de OpenVG. Uzas API de 2D vektoraj grafikoj plirapidigitaj per aparataro de OpenVG."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "Pelilo de GDI. Uzas malnovan interfaco de Windows. Ne rekomendata."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Kuranta videa pelilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Sono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Sona pelilo uzota. Bezonas restartigo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "Pelilo de RSound por retkonektitaj sonaj sistemoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Malnova pelilo de Open Sound System."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Defaŭlta pelilo de ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "Pelilo de ALSA kun subteno al fanigado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "Pelilo de ALSA realigita sen dependaĵoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "Pelilo de sonsistemo de RoarAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "Pelilo de OpenAL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "Pelilo de OpenSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "Pelilo de DirectSound. DirectSound estas uzata ĉefe de Windows 95 ĝis Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Pelilo de Windows Audio Session API. WASAPI estas ĉefe uzata de Windows 7 kaj plue."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "Pelilo de PulseAudio. Se la sistemo uzas PulseAudio, certiĝu, ke vi uzu ĉi tiun pelilon anstataŭ ekz. ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "Pelilo de PipeWire. Se la sistemo uzas PipeWire, certiĝu, ke vi uzu ĉi tiun pelilon anstataŭ ekz. PulseAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Pelilo de Jack Audio Connection Kit."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Mikrofono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Mikrofona pelilo uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Mikrofona resonpecigado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Pelilo de mikrofona resonpecigado uzota."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Resonpecigado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Resonpecigada pelilo uzota."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Realigo en fenestro je sinc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Implikita kosinusa realigo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Plej proksima resonpeciga realigo. Ĉi tiu resonpecigilo ignoras la agordon pri kvalito."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Kamerao"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Kameraa pelilo uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Pelilo de Bluetooth uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Pelilo de Wi-Fi uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Loko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Pelilo de loko uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Menuo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Menua pelilo uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB estas grafika interfaco de RetroArch, kiu ŝajnas kvazaŭ menuo de konsolo de 7-a generacio. Ĝi povas subteni la samajn trajtojn kiel Ozone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone estas la defaŭlta grafika interfaco de RetroArcho sur la plej multo de platformoj. Ĝi estas optimumigita por navigado per ludregilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI estas simpla integrita grafika interfaco por RetroArch. Ĝi havas la plej malaltajn rendimentajn postulojn inter la menuaj peliloj, kaj povas esti uzata sur malaltdistingivaj ekranoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "Sur porteblaj aparatoj, RetroArch defaŭlte uzas la poŝtelefonan uzantinterfaco, MaterialUI. Ĉi tiu interfaco estas dezajnita je tuŝekranaj kaj montrilaj aparatoj, kiel muso aŭ stirglobo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Registro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Registra pelilo uzota. Bezonas restartigon."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Pelilo de MIDI uzota. Bezonas restartigon."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "SwitchRes por CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Generas malaltdistingivaj videajn signalojn por ekranoj de CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Eligo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Ŝanĝi agordojn pri videa eligo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Plenekrana reĝimo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Ŝanĝi agordojn pri la plenekrana reĝimo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Enfenestra reĝimo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Ŝanĝi agordojn pri la enfenestra reĝimo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Skalado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Ŝanĝi agordojn pri videa skalado."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Ŝanĝi agordojn pri video rilatantaj al HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Sinkronigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Ŝanĝi agordojn pri videa sinkronigo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_ENABLE,
   "Apliki videan filtrilon. Eblas, ke la videa pelilo ne respektu ĉi tion."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_ENABLE,
   "Ebligi videan filtrilon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Halteti ekrankurtenon."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Preventu la aktivigon de ekrankurtenon de via sistemo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Haltetas la ekrankurtenon. Eblas, ke la videa pelilo ne respektu ĉi tion."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Fadenigita video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Plibonigas rendimenton kontraŭ atendotempo kaj videa \"mikrobalbutado\". Uzu ĝin nur se plena rapideco ne povas esti atingita alikaze."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Uzi pelilon de fadenigita video. Uzi ĉi tio povas plibonigi rendimenton, eble kontraŭ respondotempo kaj pli videa \"mikrobalbutado\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Enmeto de nigraj filmeroj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   ""
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   ""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Enmego de nigraj filmeroj - malhelaj filmeroj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Alĝustigas la nombron de nigraj filmeroj enmetotaj en la totala sinsekvo de \"scan-out\". Ju pli alta valoro, des pli movklareco; ju malpli alta valoro, des pli brileco. Ne aplikebla je 120Hz, ĉar nur ekzistas 1 nigra filmero enmetota. Agordoj pli alta ol ebla limigos vin al la maksimume ebla por via elektita ofteco de aktualigo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Alĝustigas la nombron de tute nigraj filmeroj montrotaj en la sinsekvo de enmeto de nigraj filmeroj. Ju pli da nigraj filmeroj, des pli da movklareco sed malpli da brileco. NE aplikebla je 120Hz ĉar nur ekzistas unu totala aldona filmero de 60Hz, do ĝi devos esti nigra, alikaze enmeto de nigraj fotogramoj estus tute ne aktiva."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Ombrigilaj subfilmeroj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "AVERTO: Rapida flagro povus kaŭzi bilda daŭro (\"image presistence\") en kelkaj ekranoj. Uzu ĝin je via propra risko. // Imitas bazan rulantan skanlinion sur pluraj subfilmeroj dividante la ekrano supren vertikale kaj bildigante ĉiu parto de la ekrano laŭ kiom da subfilmeroj estas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Enmetas kromajn ombrigilajn filmerojn inter filmeroj por ĉiuj eblaj ombrigilaj efikoj designitaj por ruli pli rapide ol la enhava ofteco. Uzu nur opcion designitan por la ofteco de aktualigo de via ekrano. Ne uzota je oftecoj de aktualigo, kiuj ne estas obloj de 60Hz, kiel 144Hz, 165Hz, kc. Ne kombinu ĝin kun intervalo de permuto > 1, enmeto de nigrajn filmerojn, filmera prokrasto aŭ sinkronigi al ĝusta filmerofteco de enhavo. Lasi la VRR de sistemo estas bone, sed ne estas uzebla kun ĉi ti[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Imito de rulanta skanlinio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "AVERTO: Rapida flagro povus kaŭzi bildan daŭron (\"image presistence\") en kelkaj ekranoj. Uzu ĝin je via propra risko. // Imitas bazan rulantan skanlinion sur pluraj subfilmeroj dividante la ekranon supren vertikale kaj bildigante ĉiun parton de la ekrano laŭ kiom da subfilmeroj estas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Imitas bazan rulantan skanlinion sur pluraj subfilmeroj dividante la ekranon supren vertikale kaj bildigante ĉiun parton de la ekrano laŭ kiom da subfilmeroj estas de la plej supro de la ekrano malsupren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Dulineara filtrado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Milde malklarigi la bildon por malakrigi akrajn bilderajn eĝojn. Ĉi tiu opcio malmulte influas rendimenton. Devus esti malebligita se uzante ombrigiloj."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Bilda interpolado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Specifi metodon de bilda interpolado kiam skalante enhavon per la ena IPU. \"Dukuba\" aŭ \"Dulineara\" estas rekomendata kiam uzante videajn filtrilojn funkciigate de CPU. Ĉi tiu opcio havas nenian influon al rendimento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Dukuba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Dulineara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Plej proksima najbaro"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Bilda interpolado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Specifi metodon de bilda interpolado kiam \"Entejra skalado\" estas malebligita. \"Plej proksima najbaro\" malplli influas la rendimenton."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Plej proksima najbaro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Duon-lineara"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Prokrasti aŭtomatan ombrigilon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Prokrasti aŭtomate ŝargi ombrigilojn (je ms). Povas solvi grafikajn missignalojn kiam uzante \"ekrankopian\" programaron."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Videa filtrilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Apliki videan filtrilon funkciiganta de CPU. Povas multe influi rendimenton. Kelkaj videaj filtriloj povas funkcii nur por kernoj, kiuj uzas 32-bitan aŭ 16-bitan koloradon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Apliki videan filtrilon funkciiganta de CPU. Povas multe influi rendimenton. Kelkaj videaj filtriloj povas funkcii nur por kernoj, kiuj uzas 32-bitan aŭ 16-bitan koloradon. Dinamike ligitaj videfiltrilaj bibliotekoj estas elekteblaj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Apliki videan filtrilon funkciiganta de CPU. Povas multe influi rendimenton. Kelkaj videaj filtriloj povas funkcii nur por kernoj, kiuj uzas 32-bitan aŭ 16-bitan koloradon. Integritaj videfiltrilaj bibliotekoj estas elekteblaj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Forigi videan filtrilon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Malŝargi ajnan aktivan videan filtrilon funkciigita de CPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Ebligi plenekranan reĝimon super \"noĉo\" en aparatoj Android kaj iOS"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
   "Uzi argumentajn bufrojn de Metal (bezonas rekomencon)"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS,
   "Provi plibonigi rendimenton uzante argumentajn bufrojn de Metal. Kelkaj kernoj povas bezoni ĉi tion. Ĉi tio povas \"rompi\" kelkajn ombrigilojn, ĉefe en kadukaj aparataroj aŭ malnovaj versioj de operaciumoj."
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "SwitchRes por CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Nur por ekranoj de CRT. Provas uzi ĝustajn distingivon kaj ofteco de aktualigo de la kerno/ludo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "Superdistingivo de CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Interŝanĝi inter indiĝenaj kaj ultralarĝaj superdistingivoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Horizontala centerigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Ŝanĝu ĉi tiun opcion se la bildo ne estas ĝuste centerigita en la ekrano."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Horizontala grando"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Ŝanĝu ĉi tiun opcion por alĝustigi la horizontalajn agordojn por ŝanĝi la bildan grandon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_VERTICAL_ADJUST,
   "Vertikala centrigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_VERTICAL_ADJUST,
   "Ŝanĝu ĉi tiun opcion se la bildo ne estas ĝuste centrigita en la ekrano."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Uzi altdistingivan menuon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Ŝanĝi al altdistingiva reĝimlinio (\"modeline\") por uzi kun altdistingivaj menuoj kiam nenia enhavo estas ŝargita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Propra ofteco de aktualigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Uzi propran oftecon de aktualigo specifitan en la konfigura dosiero se bezonata."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Ekrana indekso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Elekti la ekranan indekson uzotan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Kiun ekranon preferi. 0 (defaŭlte) signifas nenian apartan ekranon estas preferata, 1 kaj plie (1 por la unua ekrano) sugestas al RetroArch uzi tiun apartan ekranon."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Optimumigi por la GamePad de Wii U (bezonas rekomencon)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Uzi ĝuste duoblan skaladon de la GamePad kiel la vidujo. Malebligu por montri je la indiĝena distingivo de la TV."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Videa rotacio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Devigas certan rotacion de la video. La rotacio estas aldonita al rotacioj agorditaj de la kerno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Ekrana orientigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Devigas certan orientigon de la ekrano de la operaciumo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Indekso de GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Elekti la grafikan kardon uzotan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Ekrana horizontala deŝovo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Devigas certan deŝovon horizontale sur la video. La deŝovo estas aplikata ĉie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Ekrana vertikala deŝovo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Devigas certa deŝovo vertikale sur la video. La deŝovo estas aplikata ĉie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Vertikala ofteco de aktualigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Vertikala ofteco de aktualigo de via ekrano. Uzata por kalkuli ĝustan oftecon de aŭdia enigo. Ĉi tio estos ignorata se \"Fadenigita video\" estas ebligita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Pritaksita ekrana ofteco de aktualigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "La fidela pritaksita ofteco de aktualigo de la ekrano je Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "La fidela ofteco de aktualigo de via ekrano (Hz). Ĉi tio estas uzata por kalkuli oftecon de aŭdia enigo per la formulo:\nofteco de aŭdia enigo = ofteco de luda enigo * ofteco de ekrana aktualigo / ofteco de luda aktualigo\nSe la kerno ne raportas ajnajn valorojn, oni supozos la defaŭltajn valorojn de NTSC por retrokongruo.\nĈi tiu valoro devus esti ĉirkaŭ 60Hz por eviti grandajn ŝanĝojn en la tono. Se via ekrano ne rulas ĉe aŭ ĉirkaŭ 60Hz, malebligu VSync, kaj lasu ĉi tion ĉe ĝia[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Agordi ekran-raportitan ofteco de aktualigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "La ofteco de aktualigo laŭ raportite de la ekrana pelilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Aŭtomate ŝanĝi oftecon de aktualigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Aŭtomate interŝanĝi ekranan oftecon de aktualigo laŭ kuranta enhavo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Nur en ekskluzive plenekrana reĝimo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Nur en enfenestre plenekrana reĝimo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Ĉiuj plenekranaj reĝimoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Sojlo de aŭtomata ofteco de aktualigo al PAL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Maksimuma ofteco de aktualigo konsiderata kiel PAL."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Vertikala ofteco de aktualigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Agordi vertikalan oftecon de aktualigo de la ekrano. \"50Hz\" ebligos glatan videon kiam rulante enhavon de PAL."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Devigi malebligon de sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Devige malebligi subtenon de sRGB FBO. Kelkaj peliloj de Intel OpenGL en Windows havas videajn problemojn kun sRGB FBO-oj. Ebligi ĉi tion povas mildigi ilin."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Penekrana reĝimo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Montri en plenekrana reĝimo. Povas esti ŝanĝita plenumtempe. Povas esti nuligita per komanda linio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Enfenestra plenekrana reĝimo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Se plenekrane, preferi uzi plenekranan fenestron por preventi ŝanĝojn en la ekrana reĝimo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Larĝo dum plenekrane"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Agordi la propran larĝon por la neenfenestra plenekrana reĝimo. Lasi ĝin neagordita uzos la distingivon de la labortablon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Plenekrana alto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Agordi la propran alton por la neenfenestra plenekrana reĝimo. Lasi ĝin neagordita uzos la distingivon de la labortablo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Devigi distingivon ĉe UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Devigi la distingivon al la plenekrana grando. Se agordita je 0, fiksita valoro de 3840 × 2160 estos uzata."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Enfenestra skalado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Agordi la fenestra grando al la specifita oblo de la grando de la videjo de la kerno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Maldiafaneco de fenestro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Agordi la maldiafanecon de la fenestro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Montri fenestrajn ornamojn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Montri la titolan breton kaj eĝojn de la fenestro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Montri menubreton"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   ""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Memori fenestran lokon kaj grandon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Montri ĉiujn enhavojn en fenestro de fiksita grando, kies dimensioj estos specifitaj de la valoroj de \"Fenestra larĝo\" kaj \"Fenestra alto\", kaj konservi la kurantajn grandon kaj lokon de la fenestro ĉe fermo de RetroArch. Kiam malebligita, la fenestra grando estos dinamike fiksita laŭ la valoro de \"Enfenestra skalado\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Uzi propran fenestran grandon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Montri ĉiun enhavon en fenestro de fiksita grando, kies dimensioj estas specifita de la valoroj de \"Fenestra larĝo\" kaj \"Fenestra alto\". Kiam malebligite, la fenestra grando estos dinamike agordita laŭ la valoro de \"Enfenestra skalado\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Fenestra larĝo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Agordi la propran larĝon por la fenestro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Fenestra alto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Agordi la propran alton por la fenestro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Maksimuma fenestra larĝo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Agordi la maksimuman larĝon de la fenestro kiam aŭtomate regrandigante laŭ la valoro de \"Enfenestra skalado\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Maximuma fenestra alto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Agordi la maksimuman alton de la fenestro kiam aŭtomate regrandigante laŭ la valoro de \"Enfenestra skalado\"."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Entjera skalado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Skali videon nur je entjeraj paŝoj. La baza grando dependas de geometrio raportita de la kerno kaj de la bilda proporcio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "Akso de entjera skalado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Skali ĉu alton ĉu larĝon ĉu ambaŭ. Duonaj paŝoj aplikiĝas nur ĉe altdistingivaj fontoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Speco de entjera skalado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Rondigi ĉu supren ĉu malsupren al la sekva entjero. \"Inteligente\" reduktas la skaladon kiam la bildo estas tro stucita, kaj, ĉu la marĝenoj estu tro grandaj, malebligas la entjeran skaladon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "Subskalado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "Superskalado"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Inteligente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Bilda proporcio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Agordi la bildan proporcion."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Konfiguri bildan proporcion"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Glitpunkta valoro por videa proporcio (larĝo / alto)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Konfigura"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Provizita de kerno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Propra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Plena"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Konservi bildan proporcion"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Konservi la proporciojn bilderajn 1:1 kiam skalante enhavon per la ena IPU. Se malebligita, bildoj estos streĉitaj por plenigi la tutan ekranon."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Propran bildan proporcion (pozicio X)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Propra videja deŝovo uzata por difini la pozicion de la X-akso de la videjo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Propra bilda proporcio (posicio Y)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Propra videja deŝovo uzata por difini la pozicion de akso-Y de la videjo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Videja biaso X por la ankropunkto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Videja biaso X por la ankropunkto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Videja biaso Y por la ankropunkto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Videja biaso Y por la ankropunkto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Horizontala pozicio de la enhavo kiam la videjo estas pli larĝa ol la larĝo de la enhavo. 0.0 estas plej maldekstre, 0.5 estas centro, 1.0 estas plej dekstre."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Vertikala pozicio de la enhavo kiam la videjo estas pli alta ol la alto de la enhavo. 0.0 estas plej supre, 0.5 estas centro, 1.0 estas plej malsupre."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Videja biaso X por la ankropunkto (vertikala orientiĝo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Videja biaso X por la ankropunkto (vertikala orientiĝo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Videja biaso Y por la ankropunkto (vertikala orientiĝo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Videja biaso Y por la ankropunkto (vertikala orientiĝo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Horizontala pozicio de la enhavo kiam la videjo estas pli larĝa ol la larĝo de la enhavo. 0.0 estas plej maldekstre, 0.5 estas centro, 1.0 estas plej dekstre. Vertikala orientiĝo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Vertikala pozicio de la enhavo kiam la videjo estas pli alta ol la alto de la enhavo. 0.0 estas plej supre, 0.5 estas centro, 1.0 estas plej malsupre. Vertikala orientiĝo."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Propra bilda proporcio (larĝo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Propra videja larĝo uzata se la Bilda proporcio estas agordita al \"Propra bilda proporcio\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Propra bilda proporcio (alto)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Propra videja alto uzata se la Bilda proporcio estas agordita al \"Propra bilda proporcio\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Stuci superskanadon (bezonas rekomenciĝon)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Fortranĉas kelkajn bilderojn ĉirkaŭ la eĝoj de la bildo, kiuj kutime estas malplenaj pro la programistoj, kaj kelkfoje ankaŭ enhavas senvalorajn bilderojn."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Agordi la eligan reĝimon de HDR se la ekrano subtenas tion. Rimarku: scRGB povus glatigi rigoraj ombrigilaj maskoj de CRT ĉar la operaciuma kunmetilo konvertas al HDR10 post kiam la masko estas aplikita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MODE_OFF,
   "Malebligita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HDR_BRIGHTNESS_NITS,
   "Brilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HDR_BRIGHTNESS_NITS,
   "Brilo de la menuo je cd/m² (nits) kiam uzante ekranon de HDR. Nur videbla kiam HDR estas ebligita en Agordoj > Video > HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Brilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Agordas la bril-nivelon de HDR en nits. Uzu ĝin kombine kun la fizikaj bril-agordoj de via ekrano. Por komenci, agordu ĉi tion je 80 kaj la brilo de via ekrano je la maksimumo. Alternative, agordi ĉi tion je la maksimumaj nits de via ekrano kaj malaltigu la brilon de via ekrano ĝis la bildo ŝajnos bone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Pliigo de koloro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Uzas la plena kolor-gamo de via ekrano por krei pli helan, pli saturan bildon. Por koloroj pli fidelaj al la origina dezajno de la ludo, agordu ĉi tion je Preciza."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_ACCURATE,
   "Preciza"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_EXPANDED,
   "Plivastigita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_WIDE,
   "Larĝa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_SUPER,
   "Supera"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SCANLINES,
   "Skanlinioj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SCANLINES,
   "Ebligi skanlinioj de HDR. Skanlinioj estas la ĉefa kialo por uzi HDR en RetroArch, ĉar preciza realigo de skanlinio malŝaltas la plej multon de la ekrano kaj HDR restaŭras iom el tiu perdita brilo. Se vi bezonas regi viajn skanliniojn pli, rigardu tajloritajn ombrigilojn, kiujn RetroArch havigas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "Subbildera aranĝo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "Elekti la subbilderan aranĝon de via erkano. Ĉi tio nur influas skanliniojn. Se vi tute ne konas la subbilderan aranĝo de via ekrano, vidu je Rtings.com por la \"subpixel layout\" de via ekrano."
   )


/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Vertikala sinkronigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Sinkronigi la eligitan videon de la grafika karto kun la ofteco de aktualigo de la ekrano. Rekomendata."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Intervalo de interŝanĝo de vertikala sinkronigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Uzi propran intervalon de interŝanĝo por vertikala sinkronigo. Efektive malpliigas la oftecon de aktualigo de la ekrano je la specifita faktoro. \"Aŭtomate\" agordas la faktoron laŭ la ofteco de filmeroj raportitaj de la kerno, plibonigante ritmon de filmero kiam rulante, ekzemple, enhavon de 30 fps en ekrano de 60 Hz, aŭ enhavon de 60 fps en ekrano de 120 Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Aŭtomate"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Adaptiĝema vertikala sinkronigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "Vertikala sinkronigo estas ebligita ĝis kiam rendimento falas sub la celan oftecon de aktualigo. Povas minimumigi \"mikrobalbutadon\" kiam rendimento falas sub realan tempon, kaj povas esti pli energi-efika. Ne kongrua kun \"Filmera prokrasto\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCANLINE_SYNC,
   "Skanlinia sinkronigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCANLINE_SYNC,
   "Sinkronigi videan prezenton kun la pozicioj de la skanlinioj. Malpliigas respondotempon kontraŭ pli da risko de bilda sekciiĝo. Oni bezonas malebligi la vertikalan sinkronigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Filmera prokrasto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Malpliigas respondotempon kontraŭ pli da risko de videa \"mikrobalbutado\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Agordas la tempon je milisekundoj inter la lanĉo de la kerno kaj la prezento de la bildo. Malpliigas respondotempon kontraŭ pli da risko de \"mikrobalbutado\".\nValoroj 20 kaj pliaj estos konsiderataj kiel procento de la daŭro je filmeroj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Aŭtomata filmera prokrasto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Alĝustigi efikan \"Filmeran prokraston\" dinamike."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Provi teni la deziratan celon por \"Filmera prokrasto\" kaj minimumigi perdon de filmeroj. Komenca punkto estas 3/4 el la filmera tempo kiam \"Filmera prokrasto\" estas 0 (aŭtomate)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_TIME_SAMPLE_GATED,
   "Specimenigi filmeran tempon nur en stabila stato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_TIME_SAMPLE_GATED,
   "Limigas la specimenigon de la opcio \"Pritaksita ekrana ofteco de aktualigo\" al filmeroj, kiam enhavo rulas pure (ne en menuo, ne paŭzite, ne rapidpluigite... Daŭro en normala situacio). La diagnoza lego fariĝas reala signalo kontraŭ pli malrapida konverĝo post ŝargo de enhavo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Aŭtomata"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "efika(j)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Rigida sinkronigo de GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Rigide sinkronigi la CPU kun la GPU. Malpliigas la respondotempon kontraŭ rendimento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Filmeroj de rigida sinkronigo de GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Agordu kiom da filmeroj la CPU povas uzi antaŭtempe de la GPU kiam uzante \"Rigida sinkronigo de GPU\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Agordi kiom da filmeroj la CPU povas uzi antaŭtempe de la GPU kiam uzante \"Rigida sinkronigo de GPU\". La maksimuma valoro estas 3.\n 0: tuj sinkronigi kun la GPU.\n 1: Sinkronigi kun la antaŭa filmero.\n 2: kc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Sinkronigi al ĝusta enhava filmer-ofteco (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Nenia dekliniĝo de la intervalo postulata de la kerno. Uzi kun ekranoj kun variiĝema ofteco de aktualigo (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Sinkronigi la filmer-oftecon laŭ la enhavo. Ĉi tiu opcio egalas al devigi rapidon de ×1 dum ankoraŭ ebligante rapidpluigon. Ne estos dekliniĝoj el la ofteco de aktualigo postulata de la kerno, nek dinamika rego de ofteco de sono."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Eligo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Ŝanĝi agordojn pri sona eligo."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Mikrofono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Ŝanĝi agordojn pri sona enigo."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Sinkronigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Ŝanĝi agordojn pri sona sinkronigo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Ŝanĝi agordojn pri MIDI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Sonmiksilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Ŝanĝi agordojn pri la sonmiksilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Menuaj sonoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Ŝanĝi agordojn pri la menuaj sonoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Silentigi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Silentigi sonojn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Silentigi sonmiksilon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Silentigi la sonmiksilon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Respekti silentan reĝimon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Silentigi ĉiujn sonojn en Silenta reĝimo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Silentigi sonon kiam rapidpluigante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Aŭtomate silentigi sonon kiam uzante rapidpluigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Rapidpluigi sonon kun rapidpluigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Rapidpluigi sonon kiam rapidpluigante la ludon. Evitas kraketadon sed ŝanĝas la tonon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Silentigi sonon revolvante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Aŭtomate silentigi sonon kiam revolvante la ludon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Gajno de laŭteco (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Laŭteco (en dB). 0 dB estas normala laŭteco, sen gajno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Laŭteco, esprimita en dB. 0 dB estas normala laŭteco, kie nenia gajno estas aplikita. Gajno povas esti alĝustigita rulante per Eniga plilaŭtigo / Eniga malplilaŭtigo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Sonmiksila gajno de laŭteco (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Ĉiea sonmiksila laŭteco (en dB). 0 dB estas normala laŭteco, sen ajna gajno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Kromaĵo de DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Kromaĵo de sona DSP, kiu prilaboras sonon antaŭ ol ĝi estos sendita al la pelilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Forigi kromaĵon de DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Malŝargi ajnan aktivan sonan kromaĵon de DSP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Reĝimo ekskluzivo por WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Ebligi al la pelilo de WASAPI preni ekskluzivan regon de la sonaparato. Se malebligite, ĝi uzos la kunhavigita reĝimo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Glitpunkta formato de WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Uzi la glitpunktan formaton por la pelilo de WASAPI, se subtenata de via sonaparato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Grando de la kunhavigita bufro de WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "La intera bufra grando (je filmeroj) kiam uzante la kunhavigitan reĝimon de la pelilo de WASAPI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ASIO_CONTROL_PANEL,
   "Malfermi stirpanelon de ASIO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ASIO_CONTROL_PANEL,
   "Malfermi la stirpanelon de la pelilo ASIO por konfiguri aparatan enkursigon kaj bufrajn agordojn."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Sono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Ebligi sona eligo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Aparato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Transpasi la defaŭltan sonan aparaton, kiun la sonan pelilon uzas. Ĉi tiu dependas de la pelilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Transpasi la defaŭltan sonan aparaton, kiun la sona pelilo uzas. Ĉi tio dependas de la pelilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Propra valoro de la aparato PCM por la pelilo ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Propra valoro de dosierindiko por la pelilo OSS (ekz. /dev/dsp)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Propra valoro de pordo-nomo por la pelilo JACK (ekz. system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Propra IP-adreso de servilo RSound por la pelilo RSound."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Sona respondotempo (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Maksimuma sona respondotempo je milisekundoj. La pelilo celas konservi kurantan respondotempon en 50% de ĉi tiu valoro. Eblas, ke ĉi tio ne estu respektata se la sona pelilo ne povas provizi la agorditan respondotempon."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Mikrofono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Ebligi enigon de sono en subtenataj kernoj. Se la kerno ne uzas mikrofonon, ne pluigos la ŝargon de la CPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Aparato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Transpasi la defaŭltan enigan aparaton, kiun la mikrofona pelilo uzas. Ĉi tio dependas de la pelilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Transpasi la defaŭltan enigan aparaton, kiun la mikrofona pelilo uzas. Ĉi tio dependas de la pelilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Resonpeciga kvalito"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Malpliigu ĉi tiun valoron por pli da rendimento kaj malpli da respondotempo kontraŭ sona kvalito; pliigu ĝin por pli da sona kvalito kontraŭ rendimento kaj malpliigo de respondotempo. "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Defaŭlta ofteco de enigo (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Ofteco de sonpecigo de enigo, uzata se kerno ne postulas specifan nombron."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Respondotempo de sona enigo (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Dezirata respondotempo de sona enigo je milisekundoj. Povus ne esti respektata se la mikrofona pelilo ne povas provizi la respondotempon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Reĝimo ekskluzivo por WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Ebligi al RetroArch preni ekskluzivan regadon de la mikrofona aparato kiam uzante la mikrofonan pelilon WASAPI. Se malebligite, RetroArch uzos la kunhavigatan reĝimon anstataŭe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Glitpunkta formato de WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Uzi glitpunktan enigon por la pelilo WASAPI, se subtenata de via sona aparato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Grando de la kunhavigita bufro de WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "La intera bufra grando (je filmeroj) kiam uzante la kunhavigitan reĝimon de la pelilo WASAPI."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Resonpeciga kvalito"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Malpliigu ĉi tiun valoron por favori rendimenton kaj malpli respondotempon pli ol sona kvalito. Pliigu ĝin por pli bona sona kvalito kontraŭ rendimento kaj malpli da respondotempo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Ofteco de eligo (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Ofteco de sonpeciga eligo."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Sinkronigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Sinkronigi sonojn. Rekomendata."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Maksimuma sinkroniga varieco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "La maksimuma ŝanĝo en ofteco de sona enigo. Pliigi ĉi tion ebligas tre grandajn ŝanĝojn sinkronigante kontraŭ malprecizaj sontonoj (ekz. rulante kernojn PAL sur ekranoj NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Maksimuma sinkroniga varieco de sono.\nDifinas la maksimuman ŝanĝon en ofteco de enigo. Vi eble volos pliigi ĉi tion por ebligi grandajn ŝanĝojn sinkronigante, ekzemple rulante kernojn PAL sur ekranojn NTSC, kontraŭ malprecizaj sontonoj.\nOfteco de enigo estas difinita kiel:\nofteco de enigo × (1.0 +/− (maks. sinkroniga varieco))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Dinamika regado de sona ofteco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Helpas glatigi malperfektaĵoj sinkronigante sonon kaj videon. Estu singarda pri tio, ke se malebligita, estas preskaŭ neeble akiri ĝustan sinkronigon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Agordante ĉi tion je 0 malebligas regadon de ofteco. Alia ajn valoro regas la delton de regado de sona ofteco.\nDifinas kiom da einga ofteco povas esti alĝustigita dinamike. Eniga ofteco × (1.0 +/− (delto de regado de ofteco))."
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Enigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Elekti enigan aparaton."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Agordas la enigan aparaton (specifa de la pelilo). Kiam agordita je \"Malŝaltita\", la enigo MIDI estos malebligita. Oni ankaŭ povas tajpi la nomon de la aparato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Eligo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Elekti eligan aparaton."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Agordas la eligan aparaton (specifa de la pelilo). Kiam agordita je \"Malŝatita\", la eligo MIDI estos malebligita. Oni ankaŭ povas tajpi la nomon de la aparato.\nKiam eligo MIDI estas ebligita kaj la kerno kaj la ludo aŭ aplikaĵo subtenas eligon MIDI, iom aŭ ĉiom el la sonoj (dependante de la ludo aŭ aplikaĵo) estos generita de la aparato MIDI. Kaze de \"senvalora\" pelilo MIDI, tiam tiuj sonoj estos neaŭdeblaj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Laŭteco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Agordi eligan laŭtecon (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Sonmiksila fluo #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Ludi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Komencos ludadon de la sonfluo. Kiam finite, ĝi forigos la kurantan sonfluon el memoro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Ludi (ripetade)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Komencos ludadon de la sonfluo. Kiam finite, ĝi ripetos la kanton denove el la komenco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Ludi (sinsekve)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Komencos ludadon de la sonfluo. Kiam finite, ĝi saltos al la sekva sonfluo laŭ sinsekva ordo kaj ripetos ĉi tiun agmanieron. Utila kiel reĝimo por ludi albumojn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Haltigi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Ĉi tio haltigos ludadon de la sonfluo, sed ne forigos ĝin el la memoro. Ĝi povas esti rekomencita elektante \"Ludi\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Forigi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Ĉi tio forigos ludadon de la sonfluo kaj tute forigos ĝin el la memoro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Laŭteco"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Alĝustigi la laŭtecon de la sonfluo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Stato: neaplikebla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Stato: Haltigita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Stato: Ludate"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Stato: Ludate (ripetade)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Stato: Ludate (sinsekve)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Sonmiksilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Ludi samtempe sonfluojn eĉ en la menuo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Ebligi sonon de \"Bone\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Ebligi sonon de \"Rezigni\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Ebligi sonon de \"Avizo\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Ebligi sonon \"Fona muziko\""
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Ebligi sonojn de \"Rulumado\""
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   ""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Maksimuma kvanto da uzantoj subtenataj de RetroArch. Bezonas rekomencon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Sondada konduto (bezonas rekomencon)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Influas kiel enigsondado estas farata en RetroArch. Agordante ĝin al \"Frua\" aŭ \"Malfrua\" povas rezulti en malpli da respondotempo, dependante de via konfiguro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Influas kiel enigsondado estas farata en RetroArch.\nFrua - Enigsondado estas plenumigita antaŭ ol la filmero estas prilaborita.\nNormala - Enigsondado estas plenumigita kiam la sondado estas postulita.\nMalfrua - Enigsondado estas plenumigita ĉe la unua postulo de stato per filmero.\nAgordante ĝin je \"Frua\" aŭ \"Malfrua\" povas rezulti en malpli da respondotempo, dependante de via konfiguro. Ĝi estos ignorata kiam uzante retludo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Reasigni regilojn por ĉi tiu kerno"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Transpasi la enigajn klavasignadojn kontraŭ la reasignitaj klavasignoj por la kuranta kerno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Ordigi reasignadojn laŭ ludregilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Reasignadoj nur estos aplikitaj al la aktiva ludregilo, en kiu ili estis konservitaj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Aŭtomata konfiguro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Aŭtomate konfiguras regilojn, kiuj havas profilon, kvazaǔ \"Plug-and-Play\"."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Malebligi fulmoklavojn de Windows (bezonas rekomencon)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Konservi kombinaĵojn, kiuj uzas la vindozan klavon, en la aplikaĵo."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Elekti fizikan klavaron"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Uzi ĉi tiun aparaton kiel fizika klavaro kaj ne kiel ludregilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Se RetroArch identigas aparataran klavaron kiel ia ludregilo, ĉi tiu agordo povas esti uzata por devigi RetroArch trakti la misidentigila aparato kiel klavaro.\nĈi tio estas utila se vi provas imiti komputilon en ia aparato Android TV kaj ankaŭ posedas fizikan klavaron, kiu povas esti alligita al la aparato."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Akcesora sentila enigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Ebligi enigon el plirapidiga mezurilo, \"gyroscope\" kaj lumiga sentiloj, se subtenate de via kuranta aparataro. Povus influi la rendimenton, kaj/aŭ pliigi kurentuzadon en kelkaj platformoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Aŭtomate kapti muson"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Ebligi kaptadon de muso kiam la aplikaĵo estas fronte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Aŭtomate ebligi reĝimon \"Koncentriĝi en ludo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Ĉiam ebligi reĝimon \"Koncentriĝi en ludo\" kiam lanĉante kaj daŭrigante enhavon. Kiam agordite je \"Detekti\", ĉi tiu opcio estos ebligita se la kuranta kerno realigas funkciaron de fasada klavara revoko."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "Malŝaltita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "Ŝaltita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Detekti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Paŭzi enhavon pro malkonekto de regilo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Paŭzi enhavon kiam ajna regilo estas malkonektita. Uzu \"Start\" por daŭrigi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Sojlo de enigo de butonaj aksoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Kiel malproksime akso devas esti inklinita por rezulti en premo de butono kiam uzante \"Bitigilo\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Analoga malviva zono"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Ignori analogajn stirstangajn movojn sub la valoro de malviva zono."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Analoga sentemo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Plirapidig-mezurila sentemo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Sentemo de \"Gyroscope\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Alĝustigi la sentemon de analogaj stirstangoj."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Alĝustigi la sentemon de la plirapidiga mezurilo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ORIENTATION,
   "Sentila orientigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ORIENTATION,
   "Rotacii la aksojn de la plirapidiga mezurilo kaj la \"gyroscope\" por kongrui la orientigon de la aparato."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SENSOR_ORIENTATION_AUTO,
   "Aŭtomate"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Alĝustigi la sentemon de la \"Gyroscope\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Atendotempo por la klavasignado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Kiomo da sekundoj atendi antaŭ ol procedi al la sekva klavasignado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Teno de asignado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Kvanto da sekundoj teni enigon por asigni ĝin."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Premu klavaron, muson aŭ regilon"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Maltenu klavojn kaj butonojn!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Atendotempo"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Teni"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Ada aktivigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE,
   "Malebligite haltigas ĉian operaciojn pri ada aktivigo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Periodo de ada aktivigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "La periodo je filmeroj kiam butonoj kun ada aktivigo estas premataj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DUTY_CYCLE,
   "Ciklo de laborado de la ada aktivigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE,
   "La nombro da filmeroj de la Periodo de ada aktivigo, dum kiuj la butonoj estas premataj. Se ĉi tiu nombro estas egala aŭ pli ol la Periodo de ada aktivigo, la butonoj neniam estos maltenitaj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF,
   "Duono da ciklo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Reĝimo de ada aktivigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Elekti la ĝeneralan konduton de la reĝimo de ada aktivigo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Klasika"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Klasika (baskuligi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Ununura butono (baskuligi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Ununura butono (teni)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Reĝimo klasika, dubutona operacio. Tenu butonon kaj premetu la butonon \"Turbo\" por aktivigi la sinsekvo premi-malteni.\nLa butono \"Turbo\" povas esti asignita ĉe Agordoj/Eniga pelilo/Retroregilaj asignoj/Regiloj de konektejo X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Reĝimo baskuligi klasika, dubutona operacio. Tenu butonon kaj premetu la butonon \"Turbo\" por ebligi la ada aktivigo de tiu butono. Por malebligi la ada aktivigo: tenu la butonon kaj premu la butonon \"Turbo\" denove.\nLa butono \"Turbo\" povas esti asignita ĉe Agordoj/Eniga pelilo/Retroregilaj asignoj/Regiloj de konektejo X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Reĝimo baskuligi. Premu la butonon unufoje \"Turbo\" por aktivigi la sinsekvo premi-malteni de la elektita defaŭlta butono; premu ĝin denove unufoje por malŝalti ĝin.\nLa butono \"Turbo\" povas esti asignita ĉe Agordoj/Eniga pelilo/Retroregilaj asignoj/Regiloj de konektejo X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Reĝimo teni. La sinsekvo premi-malteni de la elektita defaŭlta butono estas aktiva dum la butono \"Turbo\" estas tenata.\nLa butono \"Turbo\" povas esti asignita ĉe Agordoj/Eniga pelilo/Retroregilaj asignoj/Regiloj de konektejo X.\nPor imiti la funkcion de ada aktivigo de la erao de hejma komputilo, agordu la Asignadon kaj la Butonon al la sama butono de la stirstango."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "Asignado de \"Turbo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "La asigno, kiu aktivigos la adan aktivigon en la RetroPad. Se lasite vakua, ĝi uzos la asignon specifa de la konektejo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "Butono \"Turbo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "Cela butono \"Turbo\" en reĝimo \"Ununura butono\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "Ebligi direktojn de la direktkruco por \"Turbo\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "Se ebligite, ciferecaj direktaj enigoj povas esti ade aktivigataj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Ada aktivigo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Ŝanĝi agordojn pri ada aktivigo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Tuŝ-retrokuplado/vibrado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Ŝanĝi agordojn pri tuŝ-retrokuplado kaj vibrado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_SETTINGS,
   "Mov-/lum- sentiloj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_SETTINGS,
   "Ŝanĝi agordojn pri la plirapidiga mezurilo, \"gyroscope\" kaj lumigo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Menuaj regiloj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Ŝanĝi agordojn pri la menuaj regiloj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Fulmoklavoj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Ŝanĝi agordojn kaj asignadoj por fulmoklavoj. Ekzemple, kiel baskuligi la menuon dum ludado."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Asignoj de RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Ŝanĝi kiel la virtuala RetroPad estas asignita al fizika eniga aparato. Se eniga aparato estas agnoskita kaj aŭtomate konfigurita ĝuste, uzantoj eble ne bezonos uzi ĉi tiun menuon.\nNote: por ŝanĝoj de enigo specifaj de la kerno, anstataŭe uzu la submenuon \"Regiloj\" de la Rapida menuo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro uzas abstraktaĵon de virtuala ludregilo, konata kiel la \"RetroPad\", por komuniki inter fasadoj (kiel RetroArch) kaj kernoj. Ĉi tiu menuo determinas kiel la virtuala RetroPad estas asignita al la fizikaj enigaj aparatoj, kaj tiuj virtualaj enigaj konektejoj, kiujn ĉi tiuj aparatoj okupas.\n Se fizika eniga aparato estas agnoskita kaj aŭtomate konfigurita ĝuste, uzantoj eble ne bezonos uzi ĉi tiun menuon, kaj por ŝanĝoj de enigo specifaj de kerno, ili devus uzi anstataŭe la sub[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Regiloj de konektejo %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Ŝanĝi la asignojn de la virtuala RetroPad al via fizika eniga aparato por ĉi tiu virtuala konektejo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Ŝanĝi asignojn de enigo specifa de kerno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Provizora solvo de malkonekto de Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Provizora solvo por regiloj malkonektante kaj rekonektante. Malhelpas 2 ludantojn kun samaj regiloj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_QUIT,
   "Konfirmi forlason"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_QUIT,
   "Postuli, ke la fulmoklavo Forlasi estu premata dufoje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_CLOSE,
   "Konfirmi fermi enhavon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_CLOSE,
   "Postuli, ke la fulmoklavo Fermi enhavon estu premata dufoje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_RESET,
   "Konfirmi rekomencigi enhavon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_RESET,
   "Postuli, ke la fulmoklavo Rekomencigi enhavon estu premata dufoje."
   )


/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Vibri ĉe premi klavon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Ebligi vibradon de aparato (por subtenataj kernoj)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Forto de vibrado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Specifi la magnitudon de tuŝ-retrokupladaj efikoj."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Unuigitaj menuaj regiloj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Uzi la samajn regilojn por kaj la menuo kaj la ludo. Aplikiĝas al la klavaro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Interŝanĝi butonojn Bone kaj Rezigni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Interŝanĝi la butonojn Bone kaj Rezigni. Malebligita estas la butona orientigo japana; ebligita estas la orientigo okcidenta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Interŝanĝi butonojn rulumi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Interŝanĝi butonojn por rulumi. Malebligita rulumas 10 erojn per L/R kaj alfabete per L2/R2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Ĉiuj uzantoj regas menuon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Ebligi al ĉiuj uzantoj regi la menuon. Se malebligita, nur Uzanto 1 povas regi la menuon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SINGLECLICK_PLAYLISTS,
   "Ludlistoj per ununura alklako"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SINGLECLICK_PLAYLISTS,
   "Preterlasi la menuon \"Lanĉi\" kiam lanĉante ludlisterojn. Premu la direktokrucon dum tenante Bone por atingi la menuon \"Lanĉi\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ALLOW_TABS_BACK,
   "Ebligi reeniri el sekcioj"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ALLOW_TABS_BACK,
   "Reeniri al ĉefa menuo el sekcioj aŭ flankpanelo kiam premante Reen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Plirapidigo de rulumado"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Maksimuma rapido de la kursoro kiam tenante direkton por rulumi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Ruluma prokrasto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Komenca prokrasto je milisekundoj kiam tenante direkton por rulumi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Malebligi butonon Informo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Malhelpi la funkciadon de menuaj informoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Malebligi butonon serĉi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Malhelpi la funkciadon de menua serĉo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Malebligi maldekstran analogan en menuo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Malhelpi la enigon de la maldekstra analoga stirstango en la menuo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Malebligi dekstran analogan en menuo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Malhelpi la enigon de la dekstra analoga stirstango. Dekstra analoga stirstango ŝanĝas la bildetojn en ludlistoj."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Klavo Ebligi fulmoklavojn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Kiam asignita, la klavo \"Ebligi fulmoklavojn\" devas esti tenata antaŭ ol ajnaj aliaj fulmoklavoj estu agnoskitaj. Ebligas regilaj butonoj estu asignitaj al fulmoklavaj funkcioj sen influi normalan enigon. Asigni la modifilon nur al regilo ne necesigos ĝin por klavaraj fulmoklavoj, kaj inverse, sed ambaŭ modifiloj funkcioj por ambaŭ aparatoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Se ĉi tiu fulmoklavo estas asignita al klavaro, stirstanga butono aŭ stistanga akso, ĉiuj aliaj fulmoklavoj estos malebligitaj krom ĉi tiu fulmoklavo ankaŭ estas tenata samtempe.\nĈi tio estas utila por realigoj centritaj en RETRO_KEYBOARD, kiuj bezonas grandan areon de la klavaro, kaj kie oni ne deziras, ke fulmoklavoj ĝenu la normalan funkciadon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Prokrasto por Ebligi fulmoklavojn (filmeroj)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Aldoni prokraston je filmeroj antaŭ ol normala enigo estu barita post premi la asignitan klavon \"Ebligi fulmoklavojn\". Ebligas, ke normala enigo de la klavo \"Ebligi fulmoklavojn\" estu kaptita kiam ĝi estas asignita al alia ago (ekz. \"Select\" de RetroPad)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Kunfandi specojn de fulmoklava aparato"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Bari ĉiujn fulmoklavojn de kaj klavaroj kaj regiloj, se iu havas agordita \"Ebligi fulmoklavojn\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Agnoski nur fulmoklavojn de ludanto 1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Fulmoklavoj estas asignitaj al kerna konektejo 1, eĉ se kerna konektejo 1 estas reasignita al malsama uzanto. Notu: klavaraj fulmoklavoj ne funkcios se kerna konektejo 1 estas reasignita al ia ajn uzanto ol 1 (klavara enigo estas de uzanto 1)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Baskuligi menuon (kombinaĵo de regilo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Butonkombinaĵo de regilo por baskuligi menuon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Baskuligi mebuo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Interŝanĝas la kurantan ekranon inter menuo kaj enhavo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Forlasi (regila kombinaĵo)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Butonkombinaĵo de regilo por forlasi RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Forlasi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Fermas RetroArch, konservante ĉiujn konservan datumon kaj konfigurajn dosierojn en la disko."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Fermi enhavon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Fermi la kurantan enhavon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Rekomencigi enhavon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Rekomencigi la kurantan enhavon ekde la komenco."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Rapidpluigi (baskuligi)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Interŝanĝas inter rapidpluigita kaj normala rapidoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Rapidpluigi (teni)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Ebligas rapidpluigon kiam tenate. Enhavo rulas je normala rapido kiam la klavon estas tenata."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Malrapidpluigi (baskuligi)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Interŝanĝas inter malrapidpluigita kaj normala rapidoj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Malrapidpluigi (teni)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Ebligas malrapidpluigo kiam tenate. Enhavo rulas je normala rapido kiam la klavo estas maltenata."
   )








MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "AI-servo"
   )


/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO,
   "Ada aktivigo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOLD,
   "Teni"
   )

/* Settings > Latency */

#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
#endif

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Enable Hardware Shared Context"
   )
#ifndef HAVE_DYNAMIC
#endif
#ifdef HAVE_MIST







#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Enable customized core options by default at startup."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Enable customized configuration by default at startup."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Enable customized controls by default at startup."
   )

/* Settings > Saving */


/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Enable or disable logging to the terminal."
   )

/* Settings > File Browser */


/* Settings > Frame Throttle */


/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Rewind Enable"
   )

/* Settings > Frame Throttle > Frame Time Counter */


/* Settings > Recording */


/* Settings > On-Screen Display */


/* Settings > On-Screen Display > On-Screen Overlay */


MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Kaŝu interkovro dum konekto de ludregilo"
   )
#if defined(ANDROID)
#endif

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */


/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Onscreen Notification Font"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Onscreen Notification Size"
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Daŭrigu enhavon post uzado de rapida savo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Daŭrigu enhavon post ŝanĝado de diskoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Enable Kiosk Mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "UI Companion Enable"
   )
#ifdef _3DS
#endif

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Rapida menuo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Agordoj"
   )
#ifdef HAVE_LAKKA
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Ĉefa Menuo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Montri 'Malplenaj kernoj'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Tuta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Unu uzo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Propra"
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */


/* Settings > User Interface > Views > Settings */



/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Memori elekton kiam ŝanĝante inter sekcioj"
   )

/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Enable Achievements"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Enable or disable unofficial achievements and/or beta features for testing purposes."
   )

/* Settings > Achievements > Appearance */


/* Settings > Achievements > Visibility */


/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Netplay TCP/UDP Port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Enable stdin command interface."
   )

/* Settings > Network > Updater */


/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Historio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Enable or disable recent playlist for games, images, music, and videos."
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Forigi ludliston"
   )

/* Settings > User */


/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Enable Discord"
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Retro Achievements"
   )

/* Settings > User > Accounts > RetroAchievements */


/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > User > Accounts > Kick */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Elŝutaĵoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Dynamic Wallpapers"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Komenca dosierujo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Ludlistoj"
   )

#ifdef HAVE_MIST
/* Settings > Steam */



#endif

/* Music */

/* Music > Quick Menu */


/* Netplay */

MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Enables netplay in client mode."
   )

/* Netplay > Host */


/* Import Content */


/* Import Content > Scan File */


/* Import Content > Content Scan */


/* Explore tab */

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Rename the title of the entry."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Forigi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informo"
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Nomo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Kerno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Content Database"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Close"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Enable Achievements"
   )

/* Quick Menu > Options */


/* Quick Menu > Options > Manage Core Options */


/* Quick Menu > Controls */


/* Quick Menu > Controls > Manage Remap Files */


/* Quick Menu > Controls > Manage Remap Files > Load Remap File */


/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Load Cheat File"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Load a cheat file."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Apply Cheat Changes"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */


/* Quick Menu > Cheats > Load Cheat File (Replace) */


/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Priskribo"
   )

/* Quick Menu > Disc Control */


/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Apply Shader Changes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Preview Shader Parameters"
   )


/* Quick Menu > Shaders > Shader Parameters */


/* Quick Menu > Overrides */


/* Quick Menu > Achievements */


/* Quick Menu > Information */


/* Miscellaneous UI Items */


/* Settings Options */


/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Bilda proporcio"
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Entjera skalado"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Dynamic Wallpaper"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Select a different theme for the icon. Changes will take effect after you restart the program."
   )

/* XMB: Settings Options */


/* Ozone: Settings > User Interface > Appearance */




/* MaterialUI: Settings > User Interface > Appearance */


/* MaterialUI: Settings Options */


/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Informojn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Preview Shader Parameters"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Agordoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&help"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Ŝargi kernon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Nomo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Ludlistoj"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Dosieresplorilo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Kerno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Haltigi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Forigi ludliston"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Content Database:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Forigi"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
   "Retro Achievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Historio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Uzanto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Rigide sinkronigi la CPU kun la GPU. Malpliigas la respondotempon kontraŭ rendimento."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Starti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "System BGM Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "help"
   )

/* Discord Status */


/* Notifications */

MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Atendante klienton..."
   )

MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "CRC32 checksum mismatch between content file and saved content checksum in replay file header) replay highly likely to desync on playback."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Movie format seems to have a different serializer version. Will most likely fail."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Movie playback ended."
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Viewport size calculation failed! Will continue using raw data. This will probably not work right ..."
   )


/* Lakka */


/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Soft Filter Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_SETTINGS,
   "Bluetooth Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
   "Wi-Fi Driver"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Touch Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Small Keyboard Enable"
   )
#ifdef HAVE_LIBNX
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
   "Bluetooth Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "SAMBA Enable"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Enable or disable network sharing of your folders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "SSH Enable"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Enable or disable remote command line access."
   )
#ifdef HAVE_RETROFLAG
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAFESHUTDOWN_ENABLE,
#ifdef HAVE_RETROFLAG_RPI5
   "Retroflag Safe Shutdown"
#else
   "Retroflag Safe Shutdown (Reboot required)"
#endif
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAFESHUTDOWN_ENABLE,
#ifdef HAVE_RETROFLAG_RPI5
   "For use with compatible Retroflag case."
#else
   "For use with compatible Retroflag case. Reboot is required when changing."
#endif
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
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
   "3DS Fundo Ekrano"
   )
#endif
#ifdef HAVE_QT
#endif
#ifdef HAVE_GAME_AI





#endif
#ifdef HAVE_SMBCLIENT
#endif

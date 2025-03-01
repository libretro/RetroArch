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
   "Галоўнае меню"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Налады"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Упадабанае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Гісторыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Відарысы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Музыка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Відэа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Сеткавая гульня"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Агляд"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Аўтаномныя ядры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Імпарт змесціва"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Хуткае меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Хуткі доступ да ўсіх адпаведных налад у гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Загрузка ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Выбраць ядро на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Выбар ядра libretro. Пры праглядзе браўзэр адчыняе шлях, паказаны для каталога захоўвання ядраў. Калі шлях не зададзены, прагляд пачынаецца з каранёвага каталога.\nКалі каталог захоўвання ядраў з'яўляецца тэчкай, меню будзе выкарыстоўваць яе ў якасці тэчкі верхняга ўзроўня. Ка[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Загрузка змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Выбраць змесціва на запуск."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Агледзець змесціва. Для загрузкі неабходны 'ядро' ды файл са змесцівам.\nЗадайце 'каталог браўзера файлаў', дзе меню пачне агляд змесціва. Калі не зададзена, будзе пачынацца з кораню.\nБраўзер будзе фільтраваць пашырэнні згодна з апошнім ядром, выбраным праз 'Загрузіць ядро', [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Загрузка дыска"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Загрузіць фізічны медыядыск. Спачатку выберыце адпаведнае дыску ядро («Загрузіць ядро»)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Дамп дыска"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Стварыць дамп медыядыска ва ўнутраным сховішчы. Ён будзе захаваны як файл вобразу."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Выняць дыск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Вымае дыск з фізічнага CD/DVD-прывада."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Плэй-лісты"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Тут адлюстроўваецца знойдзенае змесціва, адпаведнае базе даных."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Імпарт змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Стварыць ці абнавіць плэй-лісты па знойдзенаму змесціву."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Паказ меню працоўнага стала"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Адкрывае традыцыйнае меню працоўнага стала."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Адключыць рэжым кіёска (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Паказаць усе адпаведныя канфігурацыі налады."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Анлайнавы абнаўляльнік"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Сцягнуць дадатковыя кампаненты ды змесціва на RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Сеткавая гульня"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Далучыцца або стварыць сеанс сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Налады"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Наладзіць праграму."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Звесткі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Адлюстраваць сістэмныя звесткі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Файл канфігурацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Кіраваць ды ствараць файлы канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Даведка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Даведацца больш пра тое, як працуе праграма."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Перазапуск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Перазапусціць праграму RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Выхад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Выйсці з праграмы RetroArch. Захаванне канфігурацыі пры выхадзе ўключана."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Выйсці з праграмы RetroArch. Захаванне канфігурацыі пры выхадзе выключана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Выхад з праграмы RetroArch. Закрыццё праграмы любым прымусовым спосабам (SIGKILL і г. д.) выгружае RetroArch без захавання канфігурацыі. На Unix-падобных сістэмах SIGINT/SIGTERM дазваляе выканаць чыстую дэініцыялізацыю з захаваннем канфігурацыі, калі дадзеная налада ўключаная."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Сцягванне ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Сцягнуць ды ўсталяваць ядро праз анлайнавы абнаўляльнік."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Усталяванне або аднаўленне ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Усталяваць або аднавіць ядро з каталога 'Спампоўкі'."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Запуск відэапрацэсара"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Запусціць аддалены RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Пачатковы каталог"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Спампоўкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Агляд архіва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Загрузіць архіў"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Упадабанае"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Тут адлюстроўваецца змесціва, дададзенае да 'Упадабанае'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Музыка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Тут адлюстроўваецца музыка, якая калісьці прайгравалася."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Відарысы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Тут адлюстроўваюцца відарысы, якія былі калісьці прагледжаныя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Відэа"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Тут адлюстроўваецца відэа, якое калісьці прайгравалася."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Агляд"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Агляд усяго адпаведнага базе даных змесціва праз інтэрфейс пошуку па катэгорыям."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Аўтаномныя ядры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Тут адлюстроўваюцца ўсталяваныя ядры, якія могуць працаваць без загрузкі змесціва."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Сцягвальнік ядраў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Абнавіць усталяваныя ядры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Абнавіць усе ўсталяваныя ядры да апошніх даступных версій."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Замена ядраў версіямі з Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Замяніць усе ўстарэлыя ды ўсталяваныя ўручную ядры на апошнія даступныя версіі з Play Store."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Абнаўляльнік мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Сцягнуць поўны пакет мініяцюр на выбраную сістэму."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Абнаўляльнік мініяцюр плэй-лістоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Сцягнуць мініяцюры да запісаў выбранага плэй-ліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Сцягвальнік змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Сцягвае вольнае змесціва да выбранага ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Сцягвальнік сістэмных файлаў ядраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Сцягнуць дапаможныя сістэмныя файлы, патрэбных для слушнай/найлепшай працы ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Абнавіць файлы звестак ядраў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Абнавіць рэсурсы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Абнавіць профілі кантролераў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Абнавіць чыт-коды"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Абнавіць базы даных"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Абнавіць накладкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Абнавіць шэйдары GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Абнавіць шэйдары Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Абнавіць шэйдары Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Звесткі пра ядро"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Праглядзець датычныя да праграмы/ядра звесткі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Звесткі пра дыск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Праглядзець звесткі пра ўстаўленыя медыядыскі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Звесткі пра сеціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Праглядзець сеткавыя інтэрфейсы з асацыяванымі адрасамі IP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Звесткі пра сістэму"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Праглядзець пэўныя звесткі пра прыладу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Кіраванне базамі даных"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Праглядзець базу даных."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Кіраванне курсорамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Праглядзець папярэднія пошукі."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Назва ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Цэтлік ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Версія ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Назва сістэмы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Вырабнік сістэмы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Катэгорыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Аўтар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Дазволы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Ліцэнзія"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Падтрымліваюцца пашырэнні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Патрэбныя графічныя API"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Поўны шлях"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Падтрымка захавання стану"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Няма"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Базавая (захаванне/загрузка)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Серыялізаваная (захаванне/загрузка, перамотка назад)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Дэтэрмінаваная (захаванне/загрузка, перамотка назад, забяганне, сеткавая гульня)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Прашыўка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "- Заўвага: уключана опцыя «Сістэмныя файлы ў каталогу змесціва»."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "- Прагляд у: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Адсутнічае, неабходнае:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Адсутнічае, неабавязкова:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Маецца, неабходнае:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Маецца, неабавязкова:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Блакаванне ўсталяванага ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Забараняе змену ўсталяванага ядра. Выключае непажаданыя абнаўленні, калі кантэнту патрабуецца пэўная версія ядра (напрыклад для аркадных набораў ROM)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Выключыць з меню 'Аўтаномныя ядры'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Адключае паказ ядра ва ўкладцы/меню 'Аўтаномныя ядры'. Ужываецца толькі пры выбары рэжыму адлюстравання 'Уручную'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Выдаленне ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Прыбраць гэтае ядро з дыска."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Стварэнне рэзервовай копіі ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Стварыць архіваваную рэзервовую копію бягучага ўсталяванага ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Аднаўленне з рэзервовай копіі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Усталяваць папярэднюю версію ядра са спісу рэзервовых копій."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Выдаленне рэзервовай копіі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Прыбраць файл са спісу архіваваных рэзервовых копій."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[Аўта]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Дата зборкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "Версія RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Версія Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Кампілятар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Мадэль ЦП"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Уласцівасці ЦП"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Архітэктура ЦП"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Ядраў ЦП"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "Даступны JIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Ідэнтыфікатар вонкавага інтэрфейсу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Аперацыйная сістэма"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "Ацэнка RetroRating"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Крыніца сілкавання"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Драйвер кантэксту відэа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Шырыня дысплэя (мм)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Вышыня дысплэя (мм)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "DPI дысплэя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "Падтрымка LibretroDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Падтрымка накладкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Падтрымка каманднага інтэрфейсу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Падтрымка сеткавага каманднага інтэрфейсу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Падтрымка сеткавага кантролера"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Падтрымка Cocoa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "Падтрымка PNG (RPNG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "Падтрымка JPEG (RJPEG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "Падтрымка BMP (RBMP)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "Падтрымка TGA (RTGA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "Падтрымка SDL 1.2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "Падтрымка SDL 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D8_SUPPORT,
   "Падтрымка Direct3D 8"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D9_SUPPORT,
   "Падтрымка Direct3D 9"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D10_SUPPORT,
   "Падтрымка Direct3D 10"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D11_SUPPORT,
   "Падтрымка Direct3D 11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D12_SUPPORT,
   "Падтрымка Direct3D 12"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GDI_SUPPORT,
   "Падтрымка GDI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Падтрымка Vulkan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Падтрымка Metal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "Падтрымка OpenGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "Падтрымка OpenGL ES"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Падтрымка шматпаточнасці"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "Падтрымка KMS/EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Падтрымка udev"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "Падтрымка OpenVG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "Падтрымка EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "Падтрымка X11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Падтрымка Wayland"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "Падтрымка XVideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "Падтрымка ALSA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "Падтрымка OSS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "Падтрымка OpenAL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "Падтрымка OpenSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "Падтрымка RSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "Падтрымка RoarAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "Падтрымка JACK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "Падтрымка PulseAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PIPEWIRE_SUPPORT,
   "Падтрымка PipeWire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "Падтрымка CoreAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "Падтрымка CoreAudio V3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "Падтрымка DirectSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "Падтрымка WASAPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "Падтрымка XAudio2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "Падтрымка zlib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "Падтрымка 7zip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Падтрымка дынамічных бібліятэк"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Дынамічная загрузка бібліятэкі libretro падчас выканання"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Падтрымка Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "Падтрымка GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "Падтрымка HLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "Падтрымка SDL Image"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "Падтрымка FFmpeg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "Падтрымка mpv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "Падтрымка CoreText"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "Падтрымка FreeType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "Падтрымка STB TrueType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Падтрымка сеткавай гульні (аднарангавай)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Падтрымка Video4Linux2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SSL_SUPPORT,
   "Падтрымка SSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "Падтрымка libusb"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Выбар базы даных"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Назва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Апісанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Жанр"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Дасягненні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Катэгорыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Мова"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Рэгіён"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Эксклюзіў кансолі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Эксклюзіў платформы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Балы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Носьбіт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Кіраванне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Мастацкі стыль"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Гульнявы працэс"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Наратыў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "Тэмп"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Перспектыва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Сетынг"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Візуалізацыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Машынізацыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Выдавец"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Распрацоўнік"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Паходжанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Франшыза"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Рэйтынг TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Рэйтынг часопіса Famitsu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Агляд часопіса Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Рэйтынг часопіса Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Нумар часопіса Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Месяц выхаду"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Год выхаду"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Рэйтынг BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Рэйтынг ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Рэйтынг ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Рэйтынг PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Апаратныя ўдасканаленні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Рэйтынг CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Серыйны нумар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Падтрымка аналагавага ўводу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Падтрымка груку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Падтрымка кааперацыі"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Загрузіць канфігурацыю"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Загрузіць актуальную канфігурацыю ды замяніць бягучыя значэнні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Захаваць бягучую канфігурацыю"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Перазапісаць бягучы файл канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Захаваць новую канфігурацыю"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Захаваць бягучую канфігурацыю ў асобны файл."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Скінуць да прадвызначанага"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Скінуць бягучую канфігурацыю да прадвызначаных значэнняў."
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Базавае кіраванне меню"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Прагортка ўгору"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Прагортка ўніз"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Пацвярджэнне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "Звесткі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Старт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Выклік меню"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Выхад"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Выклік клавіятуры"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Драйверы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Змяніць драйверы, ужытыя сістэмай."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Відэа"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Змяніць налады вываду відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Аўдыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Змяніць налады ўводу/вываду гуку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Увод"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Змяніць налады кантролера, клавіятуры, мышы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Латэнтнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Змяніць налады, датычныя латэнтнасці відэа, гуку ды ўводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Ядро"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Змяніць налады ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Канфігурацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Змяніць прадвызначаныя налады для файлаў канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Захаванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Змяніць налады захоўвання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Воблачная сінхранізацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Змяніць налады воблачнай сінхранізацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Уключэнне воблачнай сінхранізацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Спрабаваць сінхранізаваць файлы канфігурацый і захаванняў з сэрвісам воблачнага сховішча."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Дэструктыўная воблачная сінхранізацыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Сінхранізацыя: захаванні/станы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Сінхранізацыя: файлы канфігурацыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Сінхранізацыя: эскізы мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Сінхранізацыя: сістэмныя файлы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "Калі ўключана, захаванні/станы будуць сінхранізавацца з воблакам."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "Калі ўключана, файлы канфігурацыі будуць сінхранізавацца з воблакам."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "Калі ўключана, эскізы мініяцюр будуць сінхранізавацца з воблакам. У большасці выпадкаў не рэкамендуецца, апрача багатых калекцый уласных эскізаў мініяцюр; інакш сцягванне мініяцюр - лепшы выбар."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "Калі ўключана, сістэмныя файлы будуць сінхранізавацца з воблакам. Гэта можа значна павялічыць час на сінхранізацыю; выкарыстоўвайце ўсвядомлена."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Калі адключана, файлы будуць перасунутыя ў тэчку рэзервовай копіі перад іх перазапісам ці выдаленнем."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Бэкенд воблачнай сінхранізацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Які сеткавы пратакол воблачнага сховішча выкарыстоўваць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "URL воблачнага сховішча"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "URL-адрас на пункт увахода API у воблачнае сховішча."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Імя карыстальніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "Вашае імя карыстальніка да вашага ўліковага запісу воблачнага сховішча."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Пароль"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "Ваш пароль да вашага ўліковага запісу воблачнага сховішча."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Вядзенне журнала"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Змяніць налады вядзення журнала."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Файлавы браўзер"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Змяніць налады файлавага браўзера."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Файл канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Сціснуты архіўны файл."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Запіс файла канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Файл базы даных курсораў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Файл канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Файл набору налад шэйдара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Файл шэйдара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Файл прызначэнняў кіравання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Файл з чыт-кодамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Файл накладкі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Файл базы даных."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "Файл шрыфту TrueType."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Просты файл."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Відэа. Выберыце, каб адкрыць гэты файл праз прайгравальнік відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Музыка. Выберыце, каб адкрыць гэты файл праз прайгравальнік музыкі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Файл відарыса."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Відарыс. Выберыце, каб адкрыць гэты файл праз праглядальнік відарысаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Ядро libretro. Выберыце, каб спалучыць гэтае ядро з гульнёй."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Ядро libretro. Выберыце гэты файл каб RetroArch загружаў гэтае ядро."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Каталог. Выберыце, каб адкрыць гэты каталог."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Рэгуляванне кадраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Змена параметраў перамоткі назад, наперад і запаволенага руху."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Запіс"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Змяніцы налады запісу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Адлюстраванне на экране"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Змяніць накладку на дысплэй і клавіятуру, а таксама налады апавяшчэнняў на экране."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Карыстальніцкі інтэрфейс"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Змяніць налады карыстальніцкага інтэрфейса."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Сэрвіс ШІ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Змяніць налады сэрвісу ШІ (пераклад, СМТ ды г.д.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Спецыяльныя магчымасці"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Змяніць налады экраннага дыктара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Кіраванне сілкаваннем"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Змяніць налады кіравання сілкаваннем."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Дасягненні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Змяніць налады дасягненняў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Сеціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Змяніць налады сервера ды сеціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Плэй-лісты"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Змяніць налады плэй-ліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Карыстальнік"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Змяніць налады ўліковага запісу, імя карыстальніка ды мовы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Каталог"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Змяніць прадвызначаныя каталогі змяшчэння файлаў."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS,
   "Хакі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Супастаўленні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Носьбіт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Прадукцыйнасць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Гук"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Характарыстыкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Сховішча"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Сістэма"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Таймінг"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Змяніць датычныя да Steam налады."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Увод"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Драйвер увода на выкарыстанне. Некаторыя драйверы відэа прымусова ўжываюць іншыя драйверы ўвода."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "Драйвер udev счытвае падзеі evdev для падтрымкі клавіятуры. Таксама маецца падтрымка зваротнага выкліку клавіятуры, мышак і тачпадаў.\nПа змаўчанні ў большасці дыстрыбутываў узлы /dev/input даступныя толькі пры наяўнасці root-правоў (рэжым 600). Вы можаце ўсталяваць udev для доступу да я[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "Для драйвера ўводу linuxraw патрабуецца актыўны TTY. Падзеі клавіятуры счытваюцца напрамую з TTY, што робіць драйвер прасцейшым, але меней гнуткім, чым udev. Падтрымка мышак і іншых прылад цалкам адсутнічае. Драйвер выкарыстоўвае састарэлы joystick API (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Драйвер увода. Драйвер відэа можа прымусова вызначаць іншы драйвер увода."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Кантролер"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Драйвер кантролера на выкарыстанне. (Патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "Драйвер кантролера DirectInput."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "Нізкаўзроўневы драйвер Human Interface Device."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Драйвер Raw Linux, ужывае састарэлы joystick API. Па магчымасці, замест яго ўжывайце udev."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Драйвер Linux для кантролераў, падлучаных да паралельнага порта праз спецыяльныя адаптары."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Драйвер кантролера, грунтаваны на бібліятэках SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Рэкамендаваны драйвер кантролера з інтэрфейсам udev, які выкарыстоўвае для падтрымкі джойсцікаў апошнюю версію joypad API. Падтрымлівае гарачае падлучэнне і аддачу.\nПа змаўчанні ў большасці дыстрыбутываў вузлы /dev/input даступныя толькі пры наяўнасці root-правоў (mode 600). Вы можаце на[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "Драйвер кантролера XInput. Нацэлены на кантролеры XBox."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Відэа"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Драйвер відэа на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "Драйвер OpenGL 1.x. Патрабуе версію:OpenGL не менш за 1.1. Не падтрымлівае шэйдары. Па магчымасці, замест яго ўжывайце пазнейшыя драйверы OpenGL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "Драйвер OpenGL 2.x. Гэты драйвер дазваляе ўжываць GL ядры ў дадатак да ядраў з праграмным рэндэрынгам. Мінімальная патрэбная версія: OpenGL 2.0 або OpenGLES 2.0. Падтрымлівае фармат шэйдараў GLSL. Па магчымасці, замест яго ўжывайце драйвер glcore."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "Драйвер OpenGL 3.x. Гэты драйвер дазваляе ўжываць GL ядры ў дадатак да ядраў з праграмным рэндэрынгам. Мінімальная патрэбная версія: OpenGL 3.2 або OpenGLES 3.0+. Падтрымлівае фармат шэйдараў Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Драйвер Vulkan. Гэты драйвер дазваляе ўжываць GL ядры ў дадатак да ядраў з праграмным рэндэрынгам. Мінімальная патрэбная версія: Vulkan 1.0. Падтрымлівае фарматы шэйдараў HDR ды Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "Драйвер SDL 1.2 з праграмным рэндэрынгам. Забяспечвае не аптымальную прадукцыйнасць. Рэкамендуецца выкарыстоўваць у апошнюю чаргу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "Драйвер SDL 2 з праграмным рэндэрынгам. Прадукцыйнасць ядраў libretro з праграмным рэндэрынгам залежыць ад рэалізацыі SDL на платформе якая выкарыстоўваецца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Драйвер Metal на платформы Apple. Падтрымлівае фармат шэйдараў Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Драйвер Direct3D 8 без падтрымкі шэйдараў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Драйвер Direct3D 9 з падтрымкай старога фармату шэйдараў Cg."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Драйвер Direct3D 9 з падтрымкай фармату шэйдараў HLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Драйвер Direct3D 10 з падтрымкай фармату шэдараў Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Драйвер Direct3D 11 з падтрымкай HDR ды фармату шэйдараў Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Драйвер Direct3D 12 з падтрымкай HDR ды фармату шэйдараў Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "Драйвер DispmanX. Ужывае DispmanX API на Videocore IV GPU у Raspberry Pi 0..3. Накладкі ды шэйдары не падтрымліваюцца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "Драйвер LibCACA. Стварае сімвальны вывад замест графічнага. Не рэкамендуецца дзеля практычнага выкарыстання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Нізкаўзроўневы драйвер Exynos, які ўжывае блок G2D на аперацыі блітавання ў аднакрыштальных сістэмах Samsung Exynos. Забяспечвае аптымальную прадукцыйнасць у ядрах з праграмным рэндэрынгам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Просты драйвер відэа DRM. Гэты нізкаўзроўневы драйвер відэа ўжывае libdrm для апаратнага маштабавання з дапамогай накладак графічнага працэсара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Нізкаўзроўневы драйвер відэа Sunxi, які ўжывае блок G2D у аднакрыштальных сістэмах Allwinner."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Драйвер Wii U. Падтрымлівае шэйдары Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Драйвер Switch. Падтрымлівае фармат шэйдараў GLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "Драйвер OpenVG. Ужывае графічны API вектарнай 2D-графікі з апаратнай акселерацыяй OpenVG."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "Драйвер GDI. Ужывае састарэлы інтэрфейс Windows. Не рэкамендуецца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Бягучы драйвер відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Аўдыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Гукавы драйвер на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "Драйвер RSound для сеткавых аўдыясістэм."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Устарэлы драйвер Open Sound System."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Прадвызначаны драйвер ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "ALSA драйвер з падтрымкай шматпаточнасці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "Драйвер ALSA, рэалізаваны без залежнасцяў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "Драйвер гукавой сістэмы RoarAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "Драйвер OpenAL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "Драйвер OpenSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "Драйвер DirectSound. DirectSound выкарыстоўваецца пераважна з Windows 95 па Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Драйвер Windows Audio Session API. WASAPI выкарыстоўваецца пераважна з Windows 7 і вышэй."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "Драйвер PulseAudio. Калі ў сістэме выкарыстоўваецца PulseAudio, пераканайцеся, што ўжыты гэты драйвер замест накшталт ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "Драйвер PipeWire. Калі ў сістэме выкарыстоўваецца PipeWire, пераканайцеся, што ўжыты гэты драйвер замест накшталт PulseAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Драйвер Jack Audio Connection Kit."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Мікрафон"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Драйвер мікрафона на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Перадыскрэтызацыя мікрафона"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Драйвер перадыскрэтызацыі мікрафона на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Фрэймаў у аўдыёблоку мікрафона"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Перадыскрэтызацыя гуку"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Драйвер перадыскрэтызацыі гуку на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Рэалізацыя Windowed Sinc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Рэалізацыя Convoluted Cosine."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Рэалізацыя найбліжэйшай перадыскрэтызацыі. Гэтая перадыскрэтызацыя ігнаруе налады якасці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Камера"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Драйвер камеры на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Драйвер Bluetooth на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Драйвер Wi-Fi на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Месцазнаходжанне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Драйвер месцазнаходжання на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Драйвер меню на выкарыстанне. (Патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB гэта графічны інтэрфейс RetroArch, які выглядае як меню кансолей 7 пакалення. Па функцыянальнасці аналагічны Ozone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone гэта стандартны графічны інтэрфейс RetroArch на большасці платформаў. Ён аптымізаваны для навігацыі праз гульнявы кантролер."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI гэта просты ўбудаваны графічны інтэрфейс для RetroArch. Сярод драйвераў меню ён мае найніжэйшыя патрабаванні да прадукцыйнасці і прыгодны для экранаў з нізкай раздзяляльнасцю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "На мабільных прыладах RetroArch па змаўчанні выкарыстоўвае мабільны карыстальніцкі інтэрфейс MaterialUI. Дадзены інтэрфейс распрацаваны для сэнсарных экранаў і ўказальных прылад тыпу мыш/трэкбол."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Запіс"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Драйвер запісу на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Драйвер MIDI на выкарыстанне."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "Змена разрознення ЭПТ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Вывад уласных сігналаў нізкай раздзяляльнасці на выкарыстанне разам з ЭПТ-дысплэямі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Вывад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Змяніць налады вываду відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Поўнаэкранны рэжым"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Змяніць налады поўнаэкраннага рэжыму."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Аконны рэжым"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Змяніць налады аконнага рэжыму."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Маштабаванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Змяніць налады маштабавання відэа."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Змяніць налады HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Сінхранізацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Змяніць налады сінхранізацыі відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Прыпыніць ахоўнік экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Прадухіліць актыўнасць вашага сістэмнага ахоўніка экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Прыпыняе ахоўнік экрана. Служыць падказкай драйверу відэа; неабавязкова павінна выконвацца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Відэа асобным патокам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Паляпшае прадукцыйнасць коштам латэнтнасці ды большай перарывістасці відэа. Выкарыстоўвайце толькі пры немажлівасці дасягнення поўнай хуткасці іншым шляхам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Выкарыстоўваць паточны драйвер відэа. Можа палепшыць прадукцыйнасць коштам мажлівай латэнтнасці ды большай перарывістасці відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Устаўка чорнага кадра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Устаўляе паміж кадрамі кадр(ы) чорнага колеру для падвышэння выразнасці руху. Выкарыстоўвайце значэнне толькі для бягучай частаты абнаўлення. Не дастасавальна з частатой абнаўлення не кратнай 60 Гц, напрыклад 144 Гц, 165 Гц і г. д. Не ўключайце адначасова з інтэрвалам абнаўлен[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_120,
   "1 – для частаты абнаўлення экрану 120 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_180,
   "2 – для частаты абнаўлення экрану 180 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_240,
   "3 – для частаты абнаўлення экрану 240 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_300,
   "4 – для частаты абнаўлення экрану 300 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_360,
   "5 – для частаты абнаўлення экрану 360 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_420,
   "6 – для частаты абнаўлення экрану 420 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_480,
   "7 – для частаты абнаўлення экрану 480 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_540,
   "8 – для частаты абнаўлення экрану 540 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_600,
   "9 – для частаты абнаўлення экрану 600 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_660,
   "10 – для частаты абнаўлення экрану 660 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_720,
   "11 – для частаты абнаўлення экрану 720 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_780,
   "12 – для частаты абнаўлення экрану 780 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_840,
   "13 – для частаты абнаўлення экрану 840 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_900,
   "14 – для частаты абнаўлення экрану 900 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_960,
   "15– для частаты абнаўлення экрану 960 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Устаўлянне чорнага кадра - зацямнёныя кадры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Рэгулюе колькасць чорных кадраў у агульнай паслядоўнасці разгорткі BFI. Падвышэнне значэння паляпшае выразнасць руху, паніжэнне павялічвае яркасць. Не дастасавальна да 120 Гц, паколькі пры гэтым даступны толькі адзін кадр для апрацоўкі BFI. Пры перавышэнні наладкі будзе ўста[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Рэгулюе колькасць кадраў, якія адлюстроўваюцца чорнымі ў паслядоўнасці BFI. Большая колькасць чорных кадраў павялічвае выразнасць руху, але змяншае яркасць. Не дастасавальна для 120 Гц з-за наяўнасці толькі аднаго лішняга кадра ў 60 Гц, які павінен быць чорным, інакш BFI не будз[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Падкадры шэйдэра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Устаўляе паміж кадрамі дадатковы кадр(ы) шэйдара для ўсіх шэйдарных эфектаў з частатой вышэй частаты кантэнту. Выкарыстоўвайце значэнне толькі для бягучай частаты абнаўлення. Не дастасавальна з частатой абнаўлення не кратнай 60 Гц, напрыклад 144 Гц, 165 Гц і г. д. Не ўключайце [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_120,
   "2 – для частаты абнаўлення экрану 120 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_180,
   "3 – для частаты абнаўлення экрану 180 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_240,
   "4 – для частаты абнаўлення экрану 240 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_300,
   "5 – для частаты абнаўлення экрану 300 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_360,
   "6 – для частаты абнаўлення экрану 360 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_420,
   "7 – для частаты абнаўлення экрану 420 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_480,
   "8 – для частаты абнаўлення экрану 480 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_540,
   "9 – для частаты абнаўлення экрану 540 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_600,
   "10 – для частаты абнаўлення экрану 600 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_660,
   "11 – для частаты абнаўлення экрану 660 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_720,
   "12 – для частаты абнаўлення экрану 720 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_780,
   "13 – для частаты абнаўлення экрану 780 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_840,
   "14 – для частаты абнаўлення экрану 840 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_900,
   "15 – для частаты абнаўлення экрану 900 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_960,
   "16– для частаты абнаўлення экрану 960 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Здымак экрана графічным працэсарам"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Сімуляцыя плывучага радка разгорткі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Імітуе просты плавальны радок разгорткі па-над некалькімі падкадрамі шляхам дзялення экрана па вертыкалі і адмалёўкі кожнай яго часткі зыходзячы з колькасці падкадраў ад верха да нізу экрана."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Калі падтрымліваецца, рабіць здымкі экрану пасля апрацоўкі GPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Білінейная фільтрацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Дадае невялікае размыццё выявы для згладжвання рэзкасці пікселяў. Практычна не ўплывае на прадукцыйнасць."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Інтэрпаляцыя выявы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Вызначыць метад інтэрпаляцыі выявы пры маштабаванні кантэнту праз унутраную адзінку апрацоўкі выявы. Пры выкарыстанні апрацоўчых ЦА відэафільтраў прапануецца 'Бікубічная' або 'Білінейная'. Не ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Бікубічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Білінейная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Метад бліжэйшага суседа"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Інтэрпаляцыя выявы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Вызначыць метад інтэрпаляцыі выявы пры выключанай функцыі 'Цэлалікавы маштаб'. 'Метад бліжэйшага суседа' мае найменшы ўплыў на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Метад бліжэйшага суседа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Паўлінейная"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Затрымка аўташэйдара"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Затрымка (у мс) аўтаматычнай загрузкі шэйдараў. Можа выправіць графічныя збоі пры выкарыстоўванні праграм захопу экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Відэафільтр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Ужыць апрацоўчы ЦП відэафільтр. Можа значна знізіць прадукцыйнасць. Некаторыя відэафільтры працуюць толькі з ядрамі, якія выкарыстоўваюць 32- або 16-бітны колер."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Ужыць апрацоўчы ЦП відэафільтр. Можа значна знізіць прадукцыйнасць. Некаторыя відэафільтры працуюць толькі з ядрамі, якія выкарыстоўваюць 32- або 16-бітны колер. Могуць быць выбраныя дынамічна звязаныя бібліятэкі відэафільтраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Ужыць апрацоўчы ЦП відэафільтр. Можа значна знізіць прадукцыйнасць. Некаторыя відэафільтры працуюць толькі з ядрамі, якія выкарыстоўваюць 32- або 16-бітны колер. Могуць быць выбраныя ўбудаваныя бібліятэкі відэафільтраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Адкінуць відэафільтр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Выгрузіць любы актыўны апрацоўчы ЦП відэафільтр."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Дадаць засечку ў поўнаэкранным рэжыме на прыладах Android ды iOS"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "Змена разрознення ЭПТ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Толькі для ЭПТ. Спрабуе выкарыстоўваць раздзяляльнасць ды частату абнаўлення акурат ядру/гульне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "Суперраздзяляльнасць ЭПТ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Пераключэнне паміж роднымі ды ўльтрашырокімі суперраздзяляльнасцямі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Цэнтраванне па восі X"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Прайдзіце праз гэтыя наладкі, калі выява не належным чынам адцэнтравана на дысплэі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Карэкцыя імпульсу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Прайдзіце праз гэтыя наладкі, каб наладзіць карэкцыю імпульсу для змены памеру выявы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Выкарыстоўваць меню высокай раздзяляльнасці"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Пераключацца на модлайн высокага раздзялення пры адсутнасці загружанага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Уласная частата абнаўлення"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Ужываць уласную частату абнаўлення, адзначаную ў файле канфігурацыі."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Індэкс манітора"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Выбраць, які экран дысплэя будзе выкарыстоўвацца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Прыярытэт манітора. 0 (па змаўчанні) азначае, што перавага манітора выключана. 1 і вышэй (дзе 1 азначае першы манітор) паказвае RetroArch, што прыярытэт у дадзенага манітора."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Аптымізаваць для Wii U GamePad (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Выкарыстоўваць дакладнае 2x маштабаванне пры вывадзе на GamePad. Адключыце для адлюстравання ў натыўным раздзяленні ТВ."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Паварот выявы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Прымушае ўжыць пэўны паварот выявы. Дадаецца да паваротаў, што задае ядро."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Арыентацыя экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Прымушае ўжыць пэўную арыентацыю экрана паводле аперацыйнай сістэмы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Індэкс графічнага працэсара"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Выбраць графічную картку на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Гарызантальны зрух экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Прымушае ўжыць пэўнае зрушэнне выявы па гарызанталі. Ужываецца глабальна."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Вертыкальны зрух экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Прымушае ўжыць пэўнае зрушэнне выявы па вертыкалі. Ужываецца глабальна."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Вертыкальная частата абнаўлення"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Вертыкальная частата абнаўлення вашага экрана. Выкарыстоўваецца для падліку адпаведнай частаты ўводнага гуку.\nІгнаруееца, калі ўключана 'Відэа асобным патокам'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Ацэнка частаты абнаўлення экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Дакладная ацэнка частаты абнаўлення экрана ў Гц."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "Дакладная частата абнаўлення экрана у (Гц). Ужываецца для вылічэння частаты гуку па формуле:\nчастата_гуку = частата ўводу гульні * частата абнаўлення экрана /\nчастата абнаўлення гульні. Калі ядро ​​не паведамляе значэнняў, то для сумяшчальнасці прымяняюцца стандартныя зн[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Вызначаная дысплэем частата абнаўлення"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "Частата абнаўлення, нададзеная драйверам дысплэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Аўтаматычнае пераключэнне частаты абнаўлення"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Пераключаць частату абнаўлення экрана на падставе бягучага змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Толькі ў эксклюзіўным поўнаэкранным рэжыме"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Толькі ў аконным поўнаэкранным рэжыме"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Усе поўнаэкранныя рэжымы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Парог аўтаматычнай частаты абнаўлення PAL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Максімальная частата абнаўлення, якую падтрымлівае PAL."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Вертыкальная частата абнаўлення"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Задаць вертыкальную частату абнаўлення экрана. '50 Гц' дазволіць плаўную выяву пры запуску змесціва PAL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE_60HZ,
   "60 Гц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE_50HZ,
   "50 Гц"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Прымусовае адключэнне sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Прымусова адключыць падтрымку sRGB FBO. Некаторыя драйверы Intel OpenGL на Windows маюць праблемы з відэа праз sRGB FBO. Уключэнне гэтай налады можа дапамагчы вырашыць тыя праблемы."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Запуск у поўнаэкранным рэжыме"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Запусціць на ўвесь экран. Можа быць зменена падчас выканання. Можа перавызначацца параметрам каманднага радка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Аконны поўнаэкранны рэжым"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Аддаваць перавагу акну на ўвесь экран над поўным экранам, каб прадухіліць змену рэжыму дысплэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Поўнаэкранная шырыня"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Задаць уласны памер шырыні для неаконнага поўнаэкраннага рэжыму. Пры пакінутым нявызначаным будзе ўжыта раздзяляльнасць працоўнага стала."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Поўнаэкранная вышыня"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Задаць уласны памер вышыні для неаконнага поўнаэкраннага рэжыму. Пры пакінутым нявызначаным будзе ўжыта раздзяляльнасць працоўнага стала."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Прымусовая раздзяляльнасць на UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Прымусовая раздзяляльнасць на памер усяго экрана; пры зададзеным 0 будзе ўжыта фіксаванае значэнне 3840 x 2160."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Маштаб акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Змяняе памеры акна, цягам ужывання зададзенага множніка да вобласці прагляду ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Непразрыстасць акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Задаць празрыстасць акна."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Паказваць афармленне акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Паказваць панэль назвы акна з межамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Паказваць панэль меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Паказваць меню акна."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Памятаць пазіцыю і велічыню акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Паказваць усё змесціва ў акне фіксаванай велічыні, згодна з адзначанымі значэннямі 'Шырыня акна' ды 'Вышыня акна', і захоўваць бягучыя велічыню ды пазіцыю акна падчас выхаду з RetroArch. Калі адключана, велічыня акна будзе задавацца дынамічна, грунтуючыся на значэнні 'Маштаб ак[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Уласная велічыня акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Паказваць усё змесціва ў акне фіксаванай велічыні, згодна з адзначанымі значэннямі 'Шырыня акна' ды 'Вышыня акна. Калі адключана, велічыня акна будзе задавацца дынамічна, грунтуючыся на значэнні 'Маштаб акна'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Шырыня акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Задаць уласную шырыню акна дысплэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Вышыня акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Задаць уласную вышыню акна дысплэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Максімальная шырыня акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Задаць максімальную шырыню акна дысплэя пры аўтаматычнай змене памераў, грунтуючыся на значэнні 'Маштаб акна'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Максімальная вышыня акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Задаць максімальную вышыню акна дысплэя пры аўтаматычнай змене памераў, грунтуючыся на значэнні 'Маштаб акна'."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Цэлалікавы маштаб"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Змяняць маштаб выявы толькі з цэлымі крокамі. Базавыя памеры залежаць ад геаметрыі і суадносін бакоў, перададзеныя ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "Вось цэлалікавага маштабу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Змяняць маштаб па вышыні ці па шырыні, або адразу па вышыні ды шырыні. Паўкрокі ўжываюцца толькі на крыніцах з высокай раздзяляльнасцю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Цэлалікавае маштабаванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Акругліць у меншы або большы бок да наступнага цэлага ліку. 'Разумна' заніжае маштаб пры празмернай абрэзцы выявы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "Заніжэнне маштабу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "Завышэнне маштабу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Разумнае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Суадносіны бакоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Задаць суадносіны бакоў дысплэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Канфігурацыя суадносін бакоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Дробнае значэнне для суадносін бакоў малюнка (шырыня / вышыня)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Канфігурацыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Прапанаваныя ядром"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Уласныя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Поўныя"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Захоўваць прапорцыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Захоўваць піксельныя прапорцыі 1:1 пры маштабаванні кантэнту ўбудаваным апрацоўшчыкам выявы. Калі выключана, выява будзе расцягнута на ўвесь экран."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Уласныя суадносіны бакоў (пазіцыя X)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Ўстаноўка зрушэння для вызначэння становішча вобласці прагляду па восі X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Уласныя суадносіны бакоў (пазіцыя Y)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Ўстаноўка зрушэння для вызначэння становішча вобласці прагляду па восі Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Зрух вобласці прагляду па X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Зрух вобласці прагляду па X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Зрух вобласці прагляду па Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Зрух вобласці прагляду па Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Ўстаноўка зрушэння вобласці прагляду па гарызанталі (калі шырыня перавышае вышыню кантэнту). 0.0 адпавядае леваму краю, 1.0 - праваму."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Ўстаноўка зрушэння вобласці прагляду па вертыкалі (калі вышыня перавышае вышыню кантэнту). 0.0 адпавядае леваму краю, 1.0 - праваму."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Зрух вобласці прагляду па X (партрэтны рэжым)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Зрух вобласці прагляду па X (партрэтны рэжым)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Зрух вобласці прагляду па Y (партрэтны рэжым)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Зрух вобласці прагляду па Y (партрэтны рэжым)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Ўстаноўка зрушэння вобласці прагляду па гарызанталі (калі шырыня перавышае вышыню кантэнту). 0.0 адпавядае леваму краю, 1.0 - праваму. Для (партрэтнага рэжыму)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Ўстаноўка зрушэння вобласці прагляду па вертыкалі (калі вышыня перавышае вышыню кантэнту). 0.0 адпавядае леваму краю, 1.0 - праваму. Для (партрэтнага рэжыму)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Уласныя суадносіны бакоў (шырыня)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Ручная ўстаноўка шырыні вобласці выявы для карыстальніцкіх суадносін бакоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Уласныя суадносіны бакоў (вышыня)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Ручная ўстаноўка вышыні вобласці выявы для карыстальніцкіх суадносін бакоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Кадраваць вылеты разгорткі (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Зразае некалькі пікселяў па краі карцінкі, якія, як правіла, не выкарыстоўваюцца пры распрацоўцы і могуць уносіць скажэнні."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "Уключыць HDR"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Уключыць HDR, калі падтрымліваецца дысплэем."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Пікавая яркасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Ўстаноўка максімальнай яркасці (у кд/м2), якую можа выдаваць дысплей. Значэнне пікавай яркасці дысплея гледзіце на рэсурсе RTings."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Яркасць белага аркушу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Усталёўвае яркасць, пры якой тэкст можна добра чытаць на белым аркушы або яркасць у канцы шкалы SDR (стандартнага дынамічнага дыяпазону). Выкарыстоўваецца для падладкі пад розныя ўмовы асвятлення."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "Кантраст"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "Налада гамы/кантраснасці HDR. Пашырае дыяпазон паміж самымі светлымі і самымі цёмнымі часткамі выявы. Чым вышэй кантраст HDR, тым мацней розніца і чым ніжэй кантраст, тым больш размытай будзе выява. Дазваляе дасягнуць найлепшай якасці карцінкі на экране."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Пашыраная каляровая гама"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Пасля пераўтварэння каляровай прасторы ў лінейнае вызначае, ці трэба ўжываць пашыраную каляровую гаму для HDR10."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Вертыкальная сінхранізацыя (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Сінхранізаваць вывад відэа графічнай карткі з частатой абнаўлення экрана. Рэкамендуецца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Інтэрвал абмену VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Ручная ўстаноўка інтэрвалу падпампоўкі VSync. Паніжае частату экрана на зададзены каэфіцыент. У рэжыме 'Аўтаматычна' значэнне выбіраецца з частаты абнаўлення ядра, што паляпшае размеркаванне кадраў кантэнту ў 30 кад/с на дысплеях 60 Гц або кантэнту ў 60 кад/с на дысплеях 120 Гц."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Аўта"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Адаптыўны VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "VSync будзе адключацца пры зніжэнні прадукцыйнасці ніжэй мэтавай частаты абнаўлення. Дапамагае ліквідаваць запавольванні пры пагаршэнні прадукцыйнасці і падвышае энергаэфектыўнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Затрымка кадра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Скарачае латэнтнасць коштам большай рызыкі заікання відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Устанаўлівае колькасць мілісекунд бяздзейнасці перад запускам ядра пасля вываду малюнка. Памяншае запазненне, але можа ўплываць на плыўнасць.\nЗначэнне вышэй або роўнае 20 апрацоўваецца як адсотак ад часу кадра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Аўтаматычная затрымка кадраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Дынамічная падладка дзеючай 'Затрымкі кадра'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Спрабаваць захоўваць мэтавую 'Затрымку кадра' і мінімізаваць выпадзенне кадраў. Пры значэнні 'Затрымкі кадра' 0 (аўтаматычна) пачатковая кропка роўная 3/4 часу кадра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Аўта"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "дзеючых"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Жорсткая сінхранізацыя з ГП"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Жорстка сінхранізаваць цэнтральны працэсар з графічным. Скарачае латэнтнасць за кошт прадукцыйнасці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Кадры жорсткай сінхранізацыі з ГП"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Задаць колькасць кадраў, якіх можа апрацаваць цэнтральны працэсар апераджаючы графічны пры выкарыстанні функцыі 'Жорсткая сінхранізацыя з ГП'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Задаць колькасць кадраў, якіх можа апрацаваць цэнтральны працэсар апераджаючы графічны пры выкарыстанні функцыі 'Жорсткая сінхранізацыя з ГП'. Максімальнае -- 3.\n 0: Неадкладная сінхранізацыя з ГП.\n 1: Сінхранізацыя з папярэднім кадрам.\n 2: І г.д."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Сінхранізацыя з частатой кадраў змесціва (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Выключае адхіленні ад таймінгу ядра. Ужываецца для манітораў з зменнай частатой абнаўлення (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Сінхранізацыя з дакладнай кадравай частатой кантэнту. Эквівалентна прымусовай усталёўцы хуткасці на 1x, але з магчымасцю паскоранай перамоткі. Выключае адхіленне ад запытанай ядром частаты абнаўлення і не выкарыстоўвае дынамічнае кіраванне частатой гуку."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Вывад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Змяніць налады вываду гуку."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Мікрафон"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Змяніць налады ўводу гуку."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Перадыскрэтызацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Змяніць налады перадыскрэтызацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Сінхранізацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Змяніць налады сінхранізацыі гуку."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Змяніць налады MIDI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Мікшар"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Змяніць налады аўдыямікшара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Гукі меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Змяніць налады гукаў меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Прыглушыць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Адключыць гук."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Адключыць мікшар"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Адключыць аўдыямікшар."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Выконваць бясшумны рэжым"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Адключыць увесь гук у бясшумным рэжыме."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Перамотка наперад з адключаным гукам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Аўтаматычна адключаць гук пры выкарыстанні перамотцы наперад."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Перамотка наперад з паскораным гукам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Паскараць гук пры перамотцы наперад. Прадухіляе патрэскванне, але змяняе вышыню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Перамотка назад з адключанымі гукам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Аўтаматычна адключаць гук пры выкарыстанні перамотцы назад."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Узмацненне гучнасці (дБ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Гучнасць гуку (у дБ). Значэнню 0 дБ адпавядае нармальная гучнасць без ужывання ўзмацнення."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Гучнасць гуку ў дб. Значэнне 0 дб адпавядае нармальнай гучнасці без узмацнення. Узровень узмацнення можна змяняць падчас гульні клавішамі Павялічыць гучнасць / Паменшыць гучнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Узмацненне гучнасці мікшара (дБ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Глабальная гучнасць аўдыямікшара (у дБ). Значэнню 0 дБ адпавядае нармальная гучнасць без ужывання ўзмацнення."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Убудова DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Убудова DSP гуку, якая апрацоўвае гук перад адпраўкай у драйвер."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Адкінуць убудову DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Выгрузіць усялякія актыўныя гукавыя ўбудовы DSP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Эксклюзіўны рэжым WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Дазволіць драйверу WASAPI атрымаць эксклюзіўны кантроль над прыладай гуку. Калі адключана, то будзе выкарыстоўвацца абагулены рэжым."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Фармат WASAPI з плаваючай кропкай"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Калі падтрымліваецца гукавой прыладай, выкарыстоўваць для драйвера WASAPI фармат з плаваючай кропкай."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Памер абагуленага буфера WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Даўжыня прамежкавага буфера (у кадрах) пры выкарыстанні драйвера WASAPI у абагуленым рэжыме."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Гук"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Уключыць вывад гуку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Прылада"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Змяніць прадвызначаную аўдыяпрыладу, якую ўжывае гукавы драйвер. Залежыць ад драйвера."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Змяніць прадвызначаную аўдыяпрыладу, якую ўжывае гукавы драйвер. Залежыць ад драйвера."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Карыстальніцкае значэнне прылады PCM для драйвера ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Карыстальніцкае значэнне шляху для драйвера OSS (напр. /dev/dsp)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Карыстальніцкае значэнне назвы порта для драйвера JACK (напр. system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Уласны адрас IP сервера RSound для драйвера RSound."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Латэнтнасць гуку (мс)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Максімальная латэнтнасць гуку ў мілісекундах. Драйвер імкнецца падтрымліваць фактычную латэнтнасць на ўзроўні 50% ад гэтага значэння. Не будзе прытрымлівацца, калі гукавы драйвер не зможа забяспечыць атрыманае значэнне."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Мікрафон"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Уключыць увод гуку ў ядрах, якія гэта падтрымліваюць. Не мае накладных выдаткаў, калі ядро не выкарыстоўвае мікрафон."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Прылада"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Змяніць прадвызначаную прыладу ўводу, якую ўжывае драйвер мікрафона. Залежыць ад драйвера."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Змяніць прадвызначаную прыладу ўводу, якую ўжывае драйвер мікрафона. Залежыць ад драйвера."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Якасць перадыскрэтызацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Нізкія значэнні павялічваюць прадукцыйнасць/зніжаюць затрымку, але пагаршаюць якасць гуку. Высокія паляпшаюць якасць гуку, але змяншаюць прадукцыйнасць/павялічваюць адставанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Прадвызначаная ўваходная частата (Гц)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Частата дыскрэтызацыі гуку ўваходнага сігналу; выкарыстоўваецца, калі ядро не запрошвае пэўную велічыню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Латэнтнасць уваходнага гуку (мс)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Пажаданая латэнтнасць уводу гуку ў мілісекундах. Можа не ўлічвацца, калі драйвер мікрафона не зможа забяспечыць зададзенае значэнне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Эксклюзіўны рэжым WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Дазволіць, каб RetroArch атрымаў эксклюзіўны кантроль над прыладай мікрафона пры выкарыстанні драйвера мікрафона WASAPI. Калі адключана, RetroArch будзе выкарыстоўваць абагулены рэжым."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Фармат WASAPI з плаваючай кропкай"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Выкарыстоўваць увод з плаваючай кропкай для драйвера WASAPI, калі падтрымліваецца гукавой прыладай."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Памер абагуленага буфера WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Даўжыня прамежкавага буфера (у кадрах) пры выкарыстанні драйвера WASAPI у абагуленым рэжыме."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Якасць перадыскрэтызацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Нізкія значэнні павялічваюць прадукцыйнасць/зніжаюць затрымку, але пагаршаюць якасць гуку. Высокія паляпшаюць якасць гуку, але змяншаюць прадукцыйнасць/павялічваюць адставанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Выходная частата (Гц)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Частата дыскрэтызацыі аўдыявываду."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Сінхранізацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Сінхранізаваць гук. Рэкамендавана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Максімальны зрух таймінгу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Максімальнае адхіленне частаты аўдыёсігналу. Павышэнне значэння моцна ўплывае на змены таймінгу, але прыводзіць да недакладнай вышыні гуку (напрыклад пры запуску PAL-кантэнту на NTSC-экранах)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Максімальнае адхіленне таймінгу гуку.\nВызначае максімальную змену зыходнай частаты. Павышэннем значэння можна дабіцца значных адхіленняў таймінга, напрыклалад пры запуску PAL-ядзер на NTSC-дысплеях, але гэта паўплывае на дакладнасць вышыні гуку.\nРазлік зыходнай частаты:\nз[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Дынамічны кантроль частаты гуку"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Дапамагае згладжваць адхіленні ад таймінгу пры сінхранізацыі гуку і выявы. Калі выключана, дасягненне дакладнай сінхранізацыі практычна немагчыма."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Ўстаноўка на 0 адключае кіраванне частатой. Іншыя значэнні ўплываюць на адхіленне частаты гуку.\nВызначае межы змены зыходнай частаты пры дынамічнай падладцы. Разлічваецца наступным чынам:\\зыходная частата * (1.0 +/- (адхіленне кіравання частатой))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Увод"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Выбраць прыладу ўводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Задае прыладу ўводу (адмыслова драйверу). Пры зададзеным 'Выкл' увод MIDI будзе адключаны. Таксама можна пазначаць назву прылады."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Вывад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Выбраць прыладу вываду."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Задае прыладу вываду (адмыслова драйверу). Пры зададзеным 'Выкл' вывад MIDI будзе адключаны. Таксама можна пазначаць назву прылады..\nПры ўключаным вывадзе MIDI разам з ядром ды гульнёй/праграмай з падтрымкай MIDI, некаторыя або ўсе гукі (у залежнасці ад гульні/праграмы) будуць утв[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Гучнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Задаць узровень гучнасці (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Плынь мікшара #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Прайграць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Пачне прайграванне гукавой плыні. Па завяршэнні бягучыя гукавая плынь будзе прыбрана з памяці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Прайграваць (цыклічна)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Пачне прайграванне гукавой плыні. Цыклічна, па завяршэнні, трэк будзе зноў прайгравацца з пачатку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Прайграць (паслядоўна)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Пачне прайграванне гукавой плыні. Па завяршэнні будзе пераход у паслядоўным парадку да наступнай гукавой плыні з паўторам дзеянняў. Карысна для рэжыму прайгравання альбомаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Спыніць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Спыніць прайграванне гукавой плыні, але не прыбіраць яе з памяці. Можа быць запушчана зноў пры выбары 'Прайграць'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Пазбавіцца"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Спыніць прайграванне гукавой плыні ды цалкам прыбраць яе з памяці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Гучнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Дапасаваць гучнасць гукавой плыні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Стан: н/д"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Стан: спынена"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Стан: прайграецца"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Стан: прайграецца (зацыклена)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Стан: прайграецца (паслядоўна)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Мікшар"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Прайграваць гукавыя плыні адначасова нават у меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Уключыць гук 'Згода'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Уключыць гук 'Скасаванне'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Уключыць гук 'Перасцярога'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Уключыць гук фонавай музыкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Уключыць гук 'Прагортка'"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Максімум карыстальнікаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Максімальная колькасць карыстальнікаў, якіх падтрымае RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Рэжым апытання (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Уплывае на тое, як у RetroArch праводзіцца апытанне ўводу. У залежнасці ад бягучай канфігурацыі, усталёўка 'Ранні' ці 'Позні' можа паменшыць затрымку ўводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Уплывае на тое, як RetroArch праводзіць апытанне ўводу.\nРанні - перад апрацоўкай кадра.\nЗвычайны - пры фактычным запыце на апытанне ўводу.\nПозні - пры першым запыце стану ўводу для кожнага кадра.\nУ залежнасці ад канфігурацыі, рэжымы \"Ранні\" або \"Позні\" могуць зніжаць затрымку ў[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Пераназначыць элементы кіравання для гэтага ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Перавызначыць прывязкі ўводу пераназначаным наборам прывязак, устаноўленымі для бягучага ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Сартаванне пераназначэнняў па геймпадам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Пераназначэнні будуць ужывацца толькі да актыўнага геймпада, да якога яны былі захаваныя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Аўтаканфігурацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Аўтаматычна наладзіць кантролеры, якія маюць профілі, падобна Plug-and-Play."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Адключыць гарачыя клавішы Windows (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Калі ўключана, забараняе ў праграме спалучэнні з клавішай Windows."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Выбар фізічнай клавіятуры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Выкарыстоўваць гэтую прыладу як фізічную клавіятуру, а не як геймпад."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Калі RetroArch вызначае фізічную клавіятуру як геймпад, дадзеная настройка дазваляе RetroArch прымусова апазнаваць такую ​​прыладу як клавіятуру.\nМожа быць карысна ў тых выпадках, калі вы спрабуеце эмуляваць кампутар на прыладзе з Android TV і выкарыстоўваеце фізічную клавіятуру, я[...]"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Увод праз дадатковыя адчувальных элементаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Пры падтрымцы прыладай уключае ўвод з акселерометра, гіраскопа і датчыка асветленасці. Можа ўплываць на прадукцыйнасць і энергаспажыванне на шэрагу платформ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Аўтаматычны захоп мышы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Уключаць захоп мышы, калі праграма ў фокусе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Аўтаматычнае ўключэнне рэжыму 'Гульнявы фокус'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Заўсёды ўключаць рэжым 'Гульнявога фокусу' пры запуску і аднаўленні кантэнту. Пры выбары значэння 'Вызначаць' настройка будзе ўключана толькі калі ў бягучым ядры рэалізавана падтрымка зваротнага выкліку клавіятуры франтэндам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "УКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Выявіць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Прыпыненне змесціва пры адлучэнні кантролера"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Прыпыняць змесціва пры адлучэнні любога кантролера. Узнаўляць пры націсканні на Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Зрушэнне восі для націску кнопкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Значэнне адхілення восі пры якім будзе фіксавацца націск кнопкі, калі выкарыстоўваецца 'Аналага-лічбавы' рэжым."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Мёртвая зона аналагавага ўвода"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Ігнараваць адхіленні аналагавага стыку ніжэй значэння мёртвай зоны."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Чуласць аналагавага ўвода"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Настройка адчувальнасці аналагавых стыкаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Тайм-аўт прызначэння"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Колькасць секунд чакання перад пераходам да наступнага прызначэння."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Утрыманне для прызначэння"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Колькасць секунд утрымання ўводу для яго прызначэння."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Націсніце кнопку кантролера, мышы або клавіятуры"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Адпусціце клавішы ды кнопкі!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Тайм-аўт"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Утрымлівайце"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Турба-кнопкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Перыяд турба"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Рэжым турба"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Выбар паводзін турбарэжыму."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Класічны"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Класічны (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Адна кнопка (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Адна кнопка (Утрымліванне)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Турба-кнопкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Тактыльная аддача/вібрацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Налады тактыльнай аддачы і вібрацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Кіраванне меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Змяніць налады кіравання меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Гарачыя клавішы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Змяніць налады ды прызначэнні гарачых клавіш, накшталт выкліку меню падчас гульнявога працэсу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Прызначэнні RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Налада прывязак віртуальнага RetroPad да фізічнай прылады ўводу. Калі прылада ўводу карэктна распазнана і наладжана, выкарыстанне дадзенага меню не патрабуецца.\nДля змены прывязак уводу асобных ядраў выкарыстоўвайце пункт 'Кіраванне' у хуткім меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Для сувязі ядраў і франтэндаў (напр. RetroArch) Libretro выкарыстоўвае віртуальную абстракцыю геймпада 'RetroPad'. Дадзенае меню дазваляе наладзіць прывязкі RetroPad да фізічных прылад уводу і супаставіць ім віртуальныя парты ўводу.\nКалі прылада ўводу карэктна распазнана і папярэдне нал[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Кантроль порта %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Налада прывязак віртуальнага RetroPad да фізічнай прылады ўводу для дадзенага парта."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Змена прывязак уводу для бягучага ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Абыходны спосаб адлучэння Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Рашэнне праблемы з адлучэннем ды паўторным падлучэннем кантролераў. Перашкаджае прызначэнню аднолькавых кантролераў для абодвух гульцоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Пацвярджэнне выхаду"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "Патрабаваць двайное націсканне гарачай клавішы выхаду, каб выйсці з RetroArch."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Вібрацыя пры націсканні кнопак"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Уключыць вібрацыю прылады (для ядраў з падтрымкай)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Інтэнсіўнасць вібрацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Настройка велічыні эфекту тактыльнай аддачы."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Уніфікаванае кіраванне меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Выкарыстоўваць аднолькавае кіраванне як для меню, так і для гульняў. Ужываецца да клавіятуры."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Адключэнне кнопкі даведкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Калі ўключана, націсканні кнопкі даведкі будуць ігнаравацца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Адключэнне кнопкі пошуку"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Калі ўключана, націсканні кнопкі пошуку будуць ігнаравацца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Адключыць левы аналагавы стык у меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Прадухіліць пераход па меню левым аналагавымі стыкам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Адключыць правы аналагавы стык у меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Прадухіліць пераход па меню правым аналагавымі стыкам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Замена кнопак кіравання меню Згода ды Скасаванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Змяняе месцамі кнопкі OK/Адмена. Пры адключэнні дзейнічае японская раскладка, пры ўключэнні - заходняя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Замена кнопак прагорткі меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Змяняе месцамі кнопкі скроллінга. L/R для пераходу па алфавіце, L2/R2 для пракруткі па 10 элементаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Кіраванне меню ўсімі карыстальнікамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Дазволіць любым карыстальнікам кіраваць меню. Калі выключана, толькі Карыстальнік 1 здольны кіраваць меню."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Уключальнік гарачых клавіш"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Утрымлівайце дадзеную кнопку для актывацыі гарачых клавіш. Дазваляе прызначаць дадатковыя дзеянні, не ўплываючы на ​​стандартныя. Прывязка мадыфікатара толькі да геймпада не закранае спалучэнні клавіятуры і наадварот, у той час як абодва мадыфікатара працуюць для абед[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Калі дадзеная гарачая клавіша прывязана да клавіятуры, кнопкам ці восям кантролера, усе іншыя гарачыя клавішы спрацоўваюць толькі пры яе націску.\nКарысна для арыентаваных на RETRO_KEYBOARD ядраў, якія шырока выкарыстоўваюць клавіятуру, дзе непажаданыя спрацоўванні гарачых кла[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Затрымка ўключэння гарачых клавіш (у кадрах)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Дадае затрымку ў кадрах перад блакіроўкай стандартнага ўводу пасля націску кнопкі 'Актыватар гарачых клавіш'. Дазваляе перахапляць стандартны ўвод з кнопкі 'Актыватар гарачых клавіш', калі на яе прызначанае іншае дзеянне (напрыклад RetroPad 'Select')."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Аб'яднанне тыпаў прылад гарачых клавіш"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Блакуе ўсе гарачыя клавішы клавіятуры і кантролера, калі для кожнага тыпу прылад уключаны 'Актыватар гарачых клавіш'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Выклік меню (камбінацыя кантролера)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Камбінацыя кнопак кантролера для выкліку меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Выклік меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Пераключае бягучы дысплэй паміж меню ды змесцівам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Выхад (камбінацыя кантролера)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Камбінацыя кнопак кантролера для выхаду з RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Выхад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Закрывае RetroArch з гарантыяй, што ўсе захаваныя даныя ды файлы канфігурацыі скінуты на дыск."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Закрыць змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Закрывае бягучае змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Скінуць змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Зноўку запускае бягучае змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Перамотка наперад (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Пераключэнне паміж перамоткай наперад ды звычайнай хуткасцю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Перамотка наперад (утрыманне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Уключае перамотку наперад падчас утрымання. Пры адцісканні змесціва выконваецца са звычайнай хуткасцю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Запаволены рух (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Пераключэнне паміж запаволенай ды звычайнай хуткасцю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Запаволены рух (утрыманне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Уключае запаволеных рух падчас утрымання. Пры адцісканні змесціва выконваецца са звычайнай хуткасцю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Перамотка назад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Перамотка назад бягучага змесціва пры ўтрыманні кнопкі. Мае быць уключана 'Падтрымка перамоткі назад'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Паўза"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Пераключае змесціва паміж станамі з прыпыненнем ды без."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Пакадравы запуск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Запускае кантэнт па адным кадры на паўзе."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Адключэнне гуку"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Уключае/выключае вывад гуку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Павялічыць гучнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Павялічвае ўзровень гучнасці выходнага гуку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Паменшыць гучнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Паменшае ўзровень гучнасці выходнага гуку."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Загрузіць стан"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Загружае захаваны стан з бягучага выбранага слота."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Захаваць стан"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Захоўвае стан у выбраны на дадзены момант слот."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Наступны слот захавання стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Павялічвае бягучы індэкс слота захавання стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Папярэдні слот захавання стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Паніжае бягучы індэкс слота захавання стану."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Выняць дыск (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Калі віртуальны латок прывада зачынены, адчыняе яго і вымае загружаны дыск. У адваротным выпадку, устаўляе абраны дыск і закрывае латок."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Наступны дыск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Павялічвае бягучы індэкс дыска. Віртуальны латок дыска мае быць адкрытым."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Папярэдні дыск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Паніжае бягучы індэкс дыска. Віртуальны латок дыска мае быць адкрытым."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Шэйдары (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Уключае/выключае бягучы выбраны шэйдар."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Наступны шэйдар"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Загружае і ўжывае наступны файл набору налад шэйдара ў каранёвым каталозе 'Графічныя шэйдары'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Папярэдні шэйдар"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Загружае і ўжывае папярэдні файл набору налад шэйдара ў каранёвым каталозе 'Графічныя шэйдары'."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Чыт-коды (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Уключае/адключае абраны чыт-код."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Наступны чыт-код"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Павялічвае бягучы індэкс чыт-кода."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Папярэдні чыт-код"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Паніжае бягучы індэкс чыт-кода."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Стварыць здымак экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Захоплівае выяву бягучага змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Запіс (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Пачынае/спыняе запіс бягучага сеанса ў лакальны відэафайл."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Трансляцыя (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Пачынае/спыняе трансляцыю бягучага сеанса на анлайнавай відэаплатформе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Прайгаць паўтор"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Прайграць файл паўтору з бягучага выбранага слота."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Запісаць паўтор"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Запісаць файл паўтору ў бягучы выбраны слот."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Спыніць запіс/паўтор"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Спыняе запіс/прайграванне бягучага паўтору."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "Наступны слот паўтору"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Павялічвае бягучы індэкс слота паўтору."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Папярэдні слот паўтору"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Паніжае бягучы індэкс слота паўтору."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Захоп мышы (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Захапляе або вызваляе мыш. Пры захопе сістэмны курсор схаваны ды абмежаваны акном RetroArch, паляпшаючы адносны ўвод мышы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Гульнявы фокус (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Пераключае рэжым 'Гульнявы ​​фокус'. Пры атрыманні кантэнтам фокусу адключаюцца гарачыя клавішы (увод з клавіятуры перадаецца ядру) і адбываецца захоп мышы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "На ўвесь экран (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Пераключае паміж поўнаэкранным і аконным рэжымамі дысплэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Меню працоўнага стала (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Выклікае дапаможны інтэрфейс WIMP (Windows, Icons, Menus, Pointer)."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Сінхранізацыя з частатой кадраў змесціва (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Уключае/адключае сінхранізацыю з частатой кадраў змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Забяганне (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Уключае/выключае забяганне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Папераджальныя кадры (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Уключае/выключае папераджальныя кадры."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Паказ кадр/с (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Уключае/выключае індыкатар частаты кадраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Паказ тэхнічнай статыстыкі (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Уключае/выключае адлюстраванне на экране тэхнічнай статыстыкі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Накладка клавіятуры (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Уключае/выключае накладку клавіятуры."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Наступная накладка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Пераход да наступнага даступнага ўвазе бягучага аверлэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Сэрвіс ШІ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Захапляе выяву бягучага кантэнту для перакладу і/або агучвае любы тэкст на экране. 'AI-сэрвіс' павінен быць уключаны і наладжаны."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Пінг сеткавай гульні (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Уключае/адключае адлюстраванне пінга для бягучай сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Хостынг сеткавай гульні (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Уключае/выключае хостынг сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Сеткавая гульня/рэжым назіральніка (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Перамыкае бягучы сеанс Netplay паміж рэжымамі 'гульня' і 'назіральнік'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Сеткавы чат"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Адпраўка паведамленняў у чат бягучага сеансу сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Згасанне чата (пераключэнне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Пераключэнне паміж згасаючымі і статычнымі паведамленнямі чата."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "Адпраўка адладачных звестак"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SEND_DEBUG_INFO,
   "Адпраўляе дыягнастычныя звесткі пра вашую прыладу ды канфігурацыю RetroArch на нашы серверы для аналізу."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Тып прылады"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Вызначае тып эмуляванага кантролера."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Аналага-лічбавы рэжым"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Указаны аналагавы джойсцік будзе працаваць як D-Pad. Рэжымы 'Прымусова' замяшчае натыўны аналагавы ўвод ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Прывязвае паказаны аналагавы джойсцік да D-Pad.\nКалі ядро ​​натыўна падтрымлівае аналагавы ўвод, прывязка да D-Pad будзе працаваць толькі ў рэжыме '(Прымусова)'.\nЗ прымусовай прывязкай да D-Pad ядро ​​не атрымлівае падзеі ўводу з паказанага аналагавага джойсціка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Індэкс прылады"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "Фізічны кантролер, апазнаны RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Прылада для дадзенага гульца"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Абраны кантролер будзе прысвоены бягучаму гульцу паводле рэжыму рэзервавання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "Без рэзервавання"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "Пажаданы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "Зарэзервавана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "Тып рэзервавання прылады"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "Аддаваць перавагу: пры падлучэнні ўказанай прылады яно будзе прысвоена дадзенаму гульцу. Рэзерваваць: не прысвойваць іншыя кантролеры дадзенаму гульцу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Порт прывязкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Усталёўвае які порт ядра атрымлівае падзеі ўводу ад порта кантролера франтэнда %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Назначыць кіраванне цалкам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Па чарзе прызначце ўсе кнопкі і восі ў парадку іх з'яўлення ў дадзеным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Скінуць да прадвызначанага"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Вяртае налады прызначэнняў уводу да прадвызначаных значэнняў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Захаваць профіль кантролера"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Стварыць файл аўтаканфігурацыі, які будзе аўтаматычна прымяняцца пры кожным падключэнні кантролера."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Індэкс мышы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "Фізічная мыш, распазнаная RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Кнопка B (унізе)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Кнопка Y (злева)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Кнопка Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Кнопка Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "D-Pad уверх"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "D-Pad уніз"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "D-Pad улева"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "D-Pad управа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Кнопка A (справа)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Кнопка X (угары)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "Кнопка L"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "Кнопка R"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "Кнопка L2 (трыгер)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "Кнопка R2 (трыгер)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "Кнопка L3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "Кнопка R3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Левы аналагавы стык X+ (направа)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Левы аналагавы стык X- (налева)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Левы аналагавы стык Y+ (уніз)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Левы аналагавы стык Y- (уверх)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Правы аналагавы стык X+ (направа)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Правы аналагавы стык X- (налева)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Правы аналагавы стык Y+ (уніз)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Правы аналагавы стык Y- (уверх)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Пісталет курок"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Пісталет перазарадка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Пісталет дадатковая A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Пісталет дадатковая B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Пісталет дадатковая C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Пісталет Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Пісталет Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Пісталет D-Pad уверх"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Пісталет D-Pad ўніз"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Пісталет D-Pad налева"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Пісталет D-Pad направа"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Забяганне недаступнае]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "Бягучае ядро ​​несумяшчальнае з забяганнем праз адсутнасць дэтэрмінаванай падтрымкі захавання стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE,
   "Забяганне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Колькасць кадраў забягання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Колькасць кадраў забягання. Выклікае нестабільнасць геймплэя пры завышэнні колькасці кадраў унутрагульнявой затрымкі."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "Запускае дадатковую логіку ядра, каб скараціць затрымку. Адзіны асобнік сутыкае будучы кадр, затым перазагружае бягучы стан. Другі трымае толькі пад відэа асобнік ядра на будучым кадры, каб пазбегнуць праблем са станам гуку. Папераджальныя кадры могуць запускаць мінулыя [...]"
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Запускае дадатковую логіку ядра, каб скараціць затрымку. Адзіны асобнік сутыкае будучы кадр, затым перазагружае бягучы стан. Папераджальныя кадры могуць дзеля эфектыўнасці запускаць мінулыя кадры з новым уводам."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "Рэжым адзінага асобніка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "Рэжым другога асобніка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   "Рэжым папераджальных кадраў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Хаваць папярэджанні забягання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Хаваць папярэджанні, якія паказваюцца пры выкарыстанні забягання з ядром без падтрымкі захавання станаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Колькасць папераджальных кадраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Колькасць кадраў для паўторнай апрацоўкі. Выклікае няроўнасць геймплэя, калі перавышана колькасць кадраў унутрагульнявой затрымкі."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Абагулены апаратны кантэкст"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Даваць прыватны кантэкст для ядраў з апаратнай адмалёўкай. Здымае неабходнасць прадбачыць змены стану прылады паміж кадрамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Дазвол ядрам пераключаць драйвер відэа"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Дазволіць ядрам пераключацца да іншага драйвера відэа замест бягучага загружанага."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Загружаць заглушку пры прыпыненні ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Пры прыпыненні ядра загружаць пустое ядро ​​для прадухілення выхаду з RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Асобныя ядры могуць мець функцыю прыпынку. Калі дадзеная опцыя адключаная, выклік працэдуры прыпынку будзе прыводзіць да зачынення RetroArch.\nПры ўключанай опцыі замест гэтага будзе загружацца пустое ядро, якое дазваляе застацца ў меню і не зачыняць RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Запускаць ядро аўтаматычна"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Правяраць наяўнасць біасаў перад загрузкай"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Правяраць перад загрузкай кантэнту наяўнасць усіх патрэбных мікрапраграм."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE,
   "Для некаторых ядраў патрабуюцца файлы мікрапраграм ці Bios. Калі дадзеная опцыя ўключаная, RetroArch блакуе запуск ядра пры адсутнасці неабходных сістэмных файлаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Катэгорыі опцый ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Дазваляць ядрам адлюстроўваць опцыі катэгорыямі ў асобным меню. Неабходны перазапуск ядра для ўжывання змен."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Кэшаванне файлаў звестак ядраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Захоўваць файлы з інфармацыяй аб ядрах у лакальным кэшы. Істотна паскарае загрузку на платформах з павольным доступам да памяці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Абыход магчымасцяў захаванняў ва ўласцівасцях ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Вызначае, ці варта ігнараваць уласцівасці захавання стану ў звестках ядра, што дазваляе эксперыментаваць з адпаведнымі функцыямі (забяганне, перамотка назад і г. д.)."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Заўсёды перазагружаць ядро ​​пры запуску кантэнту"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Пры запуску кантэнту перазагружаць RetroArch, нават калі патрэбнае ядро ​​ўжо загружана. Можа павысіць стабільнасць сістэмы, але павялічвае час загрузкі."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Дазвол павароту"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Дазволіць ядрам задаваць кручэнне. Калі выключана, запыты на кручэнне ігнаруюцца. Выкарыстоўваецца ў канфігурацыях з ручной усталёўкай павароту экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Кіраванне ядрамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Прагляд звестак і выкананне дзеянняў з усталяванымі ядрамі (рэзерваванне, аднаўленне, выдаленне і г. д.)."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Кіраванне ядрамі"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Усталяваць ці выдаліць ядры, якія распаўсюджваюцца праз Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Усталяваць ядро"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Выдаліць ядро"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "Паказ 'Кіраванне ядрамі'"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Паказваць опцыю 'Кіраванне ядрамі' ў галоўным меню."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Усталяванне ядра: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "Ядро будзе выдалена пры выхадзе з RetroArch."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "Ядро зараз сцягваецца"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Захаванне канфігурацыі падчас выхаду"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Захоўвае змены ў файл канфігурацыі падчас выхаду."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Захоўваць змены ў файл канфігурацыі пры выхадзе з праграмы. Карысна для ўнесеных у меню змен. Перазапісвае файл канфігурацыі, не захоўваючы радкі з каментарамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Захаванне файлаў пераназначэнняў пры выхадзе"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Захоўваць змены ў любым уваходным файле пераназначэнняў пры закрыцці змесціва або пры выхадзе з RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Аўтаматычная загрузка адмысловых змесціву опцый ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Загружаць дапасаваныя опцыі ядра па змаўчанні пры запуску."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Аўтаматычная загрузка файлаў перавызначэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Загружаць дапасаваную канфігурацыю пры запуску."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Аўтаматычная загрузка файлаў пераназначэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Загружаць дапасаванае кіраванне пры запуску."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Аўтаматычная загрузка файлаў індэксаў пачатковага дыска"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "Пераходзіць да апошняга выкарыстанага дыска пры запуску шматдыскавага змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Загружаць наборы налад шэйдараў аўтаматычна"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Выкарыстанне глабальнага файла опцый ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Захаваць усе опцыі ядра ў агульны файл налад (retroarch-core-options.cfg). Калі адключана, опцыі для кожнага ядра будуць захоўвацца ў асобны файл адмысловай ядру тэчкі ўнутры каталога 'Канфігурацыя' RetroArch."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Сартаванне захаванняў па тэчкам з назваў ядраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Рассартоўваць файлы захаванняў па тэчкам з назвай выкарыстаных ядраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Сартаванне захаванняў станаў па тэчкам з назваў ядраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Рассартоўваць захаванні станаў па тэчкам з назвай выкарыстаных ядраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Сартаванне захаванняў па тэчкам каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Рассартоўваць файлы захаванняў па тэчкам з назвай каталогаў, дзе размешчана змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Сартаванне захаванняў станаў па тэчкам каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Рассартоўваць захаванні станаў па тэчкам з назвай каталогаў, дзе размешчана змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Не перапісваць SaveRAM пры загрузцы хуткіх захаванняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Блакаваць перазапіс SaveRAM пры загрузцы хуткіх захаванняў. Можа прыводзіць да памылак у гульнях."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Інтэрвал аўтазахавання SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Аўтаматычна захоўваць энерганезалежную памяць з зададзеным інтэрвалам (у секундах)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Аўтаматычна захоўвае энерганезалежную SRAM з зададзеным інтэрвалам. Па змаўчанні выключана. Інтэрвал вымяраецца ў секундах. Значэнне 0 адключае аўтазахаванне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Інтэрвал кантрольнай кропкі паўтору"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Пры запісе паўтору стан гульні аўтаматычна захоўваецца праз роўныя інтэрвалы (у секундах)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Аўтаматычна захоўвае стан гульні праз роўныя інтэрвалы пры запісе паўтору. Па змаўчанні адключана, калі не перавызначана іншымі наладамі. Інтэрвал вымяраецца ў секундах. Значэнне 0 адключае запіс кантрольных кропак."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Аўтаматычнае павялічванне індэкса захавання стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Аўтаматычна падвышаць нумар слота перад стварэннем хуткага захавання. Пры загрузцы кантэнту будзе абраны найвышэйшы даступны нумар слота."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Аўтаматычна падвышаць слот паўтору"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Аўтаматычна падвышаць нумар слота перад стварэннем запісу паўтору. Пры загрузцы кантэнту будзе абраны найвышэйшы даступны нумар слота."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Максімум аўтазахаванняў пры павышэнні слота"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Максімальны лік захаванняў, якія ствараюцца пры ўключанай опцыі 'Аўтаматычна падвышаць слот захавання'. Пры перавышэнні значэння новае захаванне выдаліць існае захаванне з найменшым індэксам. Значэнне '0' здымае абмежаванне на колькасць захаванняў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Максімум паўтораў для захавання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Максімальны лік паўтораў, якія захоўваюцца калі ўключаная опцыя 'Аўтаматычна падвышаць слот паўтору'. Пры перавышэнні значэння запіс новага паўтору выдаліць існы запіс з найменшым індэксам. Значэнне '0' здымае абмежаванне на колькасць запісаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Аўтаматычнае захаванне стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Аўтаматычна ствараць захаванне стану падчас закрыцця змесціва. Гэты стан будзе загружаны пры запуску, калі ўключана 'Аўтаматычная загрузка стану'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Аўтаматычная загрузка стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Аўтаматычна загружаць стан аўтаматычнага захавання пры запуску."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Мініяцюры станаў захавання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Паказваць у меню мініяцюры станаў захавання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Сціскаць SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Запісваць файлы SaveRAM у сціснутым фармаце. Істотна памяншае памер файлаў, трохі павялічваючы час загрузкі/захавання.\nПрацуе толькі для ядраў, якія выкарыстоўваюць стандартны інтэрфейс libretro для захавання SaveRAM."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Сцісканне захавання стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Запісваць файлы захаванняў стану ў сціснутым фармаце. Істотна памяншае памер файла, але павялічвае час загрузкі/захавання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Сартаванне здымкаў экрана па тэчкам каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Рассартоўваць здымкі экрана па тэчкам з назвай каталогаў, дзе размешчана змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Запіс захаванняў у каталог змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Выкарыстоўваць каталог змесціва як каталог файлаў захавання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Запіс захаванняў стану ў каталог змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Выкарыстоўваць каталог змесціва як каталог захаванняў стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Сістэмныя файлы ў каталозе змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Выкарыстоўваць каталог змесціва як сістэмны/BIOS каталог."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Запіс здымкаў экрана ў каталог змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Выкарыстоўваць каталог змесціва як каталог здымкаў экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Адсочваць час запуску (па ядры)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Весці запіс часу запуску кантэнту з падзелам па ядрах."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Адсочваць час запуску (агульнае)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Весці запіс агульнага часу запуску кантэнту на ўсіх ядрах."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Вядзенне журнала"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Заносіць падзеі ў тэрмінал або файл."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Узровень вядзення журнала франтэнда"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Задае ўзровень вядзення журнала франтэнда. Паведамленні франтэнда ніжэй гэтага ўзроўню ігнаруюцца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Узровень вядзення журнала ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Задае ўзровень вядзення журнала ядраў. Паведамленні ядра ніжэй гэтага ўзроўню ігнаруюцца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Усталёўвае ўзровень дэталізацыі лога для ядраў libretro (GET_LOG_INTERFACE). Калі ўзровень лога, які паведамляецца ядром ніжэй узроўня libretro_log, то ён ігнаруецца. DEBUG-логі ігнаруюцца заўсёды, толькі калі не ўключаны падрабязны рэжым (--verbose).\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (Адладка)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (Звесткі)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Папярэджанні)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (Памылкі)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Пісаць журнал у файл"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Перанакіроўваць паведамленні журнала сістэмных падзей у файл. Патрабуе, каб было ўключана 'Вядзенне журнала'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Файлы журналаў з адбіткамі часу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Пры запісванні журналу ў файл перанакіроўвае вывад з кожнага сеанса RetroArch у новы файл з адбіткам часу. Калі адключана, журнал будзе перазапісвацца пры кожным перазапуску RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Лічыльнікі прадукцыйнасці"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Лічыльнікі прадукцыйнасці для RetroArch ды ядраў. Звесткі лічыльнікаў могуць дапамагчы вызначыць вузкія месцы сістэмы ды файна наладзіць прадукцыйнасць."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Паказ схаваных файлаў ды каталогаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Паказваць схаваныя файлы ды каталогі ў файлавым браўзеры."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Фільтраванне невядомых пашырэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Фільтраваць паказ файлаў файлавым браўзерам паводле падтрымкі пашырэння."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Фільтраванне па бягучаму ядру"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "Фільтраваць паказ файлаў файлавым браўзерам па бягучаму ядру."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Памятаць апошні выкарыстаны пачатковы каталог"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Адкрываць файлавы браўзер на апошнім выкарыстаным месцы пры загрузцы змесціва з пачатковага каталога. Заўвага: месца будзе скінута да прадвызначанага пры перазапуску RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Выкарыстанне ўбудаванага медыяпрайгравальніка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Убудаваны прагляд малюнкаў"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Перамотка назад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Змяніць налады перамоткі назад."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Лічыльнік часу кадра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Настройкі лічыльніка часу кадра.\nАктыўна толькі пры ўключэнні опцыі 'Рэндэрынг у асобным патоку'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Хуткасць перамоткі наперад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Максімальная частата кантэнту пры паскарэнні (напрыклад, 5.0x для кантэнту ў 60 FPS = максімум 300 FPS). Значэнне 0.0x прыбірае мяжу паскарэння (без абмежавання FPS)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "Максімальная частата кантэнту пры выкарыстанні паскарэння (напрыклад 5.0 для кантэнту ў 60 fps => абмежаванне ў 300 fps).\nRetroArch будзе пераходзіць у рэжым сну, каб гарантавана не перавышаць максімальную частату. Дадзенае абмежаванне не забяспечвае абсалютную дакладнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Прапусканне кадраў пры перамотцы наперад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Прапускаць кадры ў адпаведнасці з хуткасцю перамоткі наперад. Эканоміць энергію і дазваляе выкарыстоўваць старонняе абмежаванне кадраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Паказчык запаволенага руху"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "Хуткасць змесціва пры выкарыстанні функцыі запаволенага руху."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Рэгуляванне частаты кадраў меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Пераконваецца ў абмежаванні частаты кадраў у меню."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Падтрымка перамоткі назад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Зварот да папярэдняй кропкі гульнявога працэсу. Істотна ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Кадры перамоткі назад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Колькасць кадраў перамоткі назад на крок. Больш высокія значэнні павялічваюць хуткасць перамоткі назад."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Памер буфера перамоткі (МБ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "Аб’ём памяці (у МБ), які трэба зарэзерваваць для буфера перамоткі. Павелічэнне гэтай велічыні павялічыць даўжыню гісторыі перамоткі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Крок памеру буфера перамоткі (МБ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Кожны раз, калі значэнне памеру буфера перамоткі павялічваецца або памяншаецца, яно будзе змяняцца на гэтую велічыню."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Скіданне пасля перамоткі наперад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Скідаць лічыльнік часу кадра пасля перамоткі наперад."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Скіданне пасля загрузкі стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Скідаць лічыльнік часу кадра пасля загрузкі стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Скіданне пасля захавання стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Скідаць лічыльнік часу кадра пасля захавання стану."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Якасць запісу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Уласная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Нізкая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "Сярэдняя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Высокая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Без стратаў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM найхутчэйшы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM найлепшы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Уласная канфігурацыя запісу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Колькасць патокаў запісу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Запіс пасля фільтрацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Захопліваць выяву пасля ўжывання фільтраў (але не шэйдараў). Запісанае відэа будзе выглядаць гэтак жа, як на экране."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Запіс з дапамогай ГП"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Пры магчымасці запісваць выяву пасля апрацоўкі графічным працэсарам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Рэжым трансляцыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "Лакальны"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Уласны"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Якасць трансляцыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Уласная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Нізкая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "Сярэдняя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Высокая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Уласная канфігурацыя трансляцыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Назва стрыму"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "URL стрыму"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "UDP-порт стрыму"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Накладка на экране"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Налады рамак і экраннага кіравання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Макет экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Налады макета экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Экранныя апавяшчэнні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Дапасаваць экранныя апавяшчэнні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Бачнасць апавяшчэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Настройка бачнасці асобных тыпаў апавяшчэнняў."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Паказваць аверлэй"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Аверлэі выкарыстоўваюцца для адлюстравання рамак і экраннага кіравання."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Паказваць аверлэй за меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Адлюстроўваць меню па-над аверлэем, а не наадварот."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Хаваць аверлэй у меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Адключаць адлюстраванне аверлэя пры ўваходзе ў меню і аднаўляць пры выхадзе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Хаваць накладку пры падлучэнні кантролера"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Хаваць накладку пры падлучэнні фізічнага кантролера да порта 1 і ўключаць зноў пры адлучэнні."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Хаваць накладку пры падлучэнні фізічнага кантролера да порта 1. Накладка не будзе аўтаматычна адноўленая пры адлучэнні геймпада."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Паказваць націскі на аверлэі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Адлюстраванне падзей уводу на экранным аверлэе. У рэжыме 'З сэнсарнага экрана' элементы аверлэя падсвятляюцца пры фактычным націску/тыку. У рэжыме 'З фізічнага кантролера' падсвятляюцца падзеі ўводу, якія атрымліваюцца ядром ад падлучанага кантролера/клавіятуры."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "З сэнсарнага экрана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Фізічны (Кантролер)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Паказваць націскі з порта"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Пазначце порт праслухоўвання прылады ўводу, калі для опцыі 'Паказваць націскі на аверлэе' абрана значэнне 'Фізічны (Кантролер)'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Адлюстроўваць курсор мышы з аверлэем"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Паказваць курсор мышы пры выкарыстанні экраннага накладання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Аўтапаварот аверлэя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Калі падтрымліваецца бягучым аверлэем, паварочваць макет паводле арыентацыі экрана/суадносін бакоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Аўтамаштабаванне аверлэя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Аўтаматычна падладжваць маштаб і адлегласць паміж элементамі аверлэя пад прапорцыі экрана. Дае лепшы вынік з аверлэямі кантролераў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Адчувальнасць дыяганаляў D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Настройка памеру зон дыяганаляў. Усталюйце на 100% для сіметрыі па 8 напрамках."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Адчувальнасць перакрыцця ABXY"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Настройка памеру зон перакрыцця для блока кнопак. Усталюйце на 100% для сіметрыі па 8 напрамках."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Зона перацэнтроўкі аналагавага ўвода"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Увод аналагавы стыку будзе адлічвацца адносна першага дотыку пры націсканні ў гэтай зоне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Накладка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Аўтазагрузка аверлэя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "Загружаць накладкі на аснове назвы сістэмы, перш чым вяртаць прадвызначаны набор налад. Будзе ігнаравацца, калі зададзена перавызначэнне на папярэдні набор налад накладкі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Непразрыстасць накладкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Настройка празрыстасці элементаў аверлэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Набор налад накладкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Выбар аверлэя ў браўзеры файлаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "Маштаб аверлэя (ландшафт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Змяняе памер усіх элементаў аверлэя ў ландшафтным рэжыме экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Падладка памераў аверлэя (ландшафт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Ужывае каэфіцыент карэкцыі суадносін бакоў для аверлэеў у ландшафтным рэжыме экрана. Павялічвае (ці памяншае) значэнне шырыні аверлэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Дзяленне аверлэя па гарызанталі (ландшафт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Калі падтрымліваецца бягучым макетам, падладжвае адлегласць паміж элементамі левай і правай частак аверлэя ў ландшафтнай арыентацыі экрана. Станоўчыя значэнні павялічваюць падзел паловаў, у той час як адмоўныя памяншаюць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Дзяленне аверлэя па вертыкалі (ландшафт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Калі падтрымліваецца бягучым макетам, падладжвае адлегласць паміж элементамі верхняй і ніжняй частак аверлэя ў ландшафтнай арыентацыі экрана. Станоўчыя значэнні павялічваюць падзел паловаў, у той час як адмоўныя памяншаюць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "Зрух аверлэя па X (ландшафт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Зрух аверлэя па гарызанталі ў ландшафтным рэжыме экрана. Станоўчыя значэнні зрушваюць аверлэй направа, адмоўныя налева."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Зрух аверлэя па Y (ландшафт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Зрух аверлэя па вертыкалі ў ландшафтным рэжыме экрана. Станоўчыя значэнні зрушваюць аверлэй уверх, адмоўныя - уніз."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "Маштаб аверлэя (партрэт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Змяняе памер усіх элементаў аверлэя ў партрэтным рэжыме экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Падладка памераў аверлэя (партрэт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Ужывае каэфіцыент карэкцыі суадносін бакоў для аверлэеў у партрэтным рэжыме экрана. Павялічвае (ці памяншае) значэнне шырыні аверлэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "Дзяленне аверлэя па гарызанталі (партрэт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Калі падтрымліваецца бягучым макетам, падладжвае адлегласць паміж элементамі левай і правай частак аверлэя ў партрэтнай арыентацыі экрана. Станоўчыя значэнні павялічваюць падзел паловаў, у той час як адмоўныя памяншаюць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Дзяленне аверлэя па вертыкалі (партрэт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Калі падтрымліваецца бягучым макетам, падладжвае адлегласць паміж элементамі верхняй і ніжняй частак аверлэя ў партрэтнай арыентацыі экрана. Станоўчыя значэнні павялічваюць падзел паловаў, у той час як адмоўныя памяншаюць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "Зрух аверлэя па X (партрэт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Зрух аверлэя па гарызанталі ў партрэтным рэжыме экрана. Станоўчыя значэнні зрушваюць аверлэй направа, адмоўныя налева."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "Зрух аверлэя па Y (партрэт)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Зрух аверлэя па вертыкалі ў партрэтным рэжыме экрана. Станоўчыя значэнні зрушваюць аверлэй уверх, адмоўныя - уніз."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "Аверлэй клавіятуры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "Выбар і падладка аверлэя клавіятуры."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "Накладка светлавога пісталета, мышы ды ўказальніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "Выкарыстоўваць любыя дотыкі не па кнопках аверлэя для перадачы ядру падзей уводу з прылады."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "Накладка светлавога пісталета"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Настройка падзей уводу светлавога пісталета, якія адпраўляюцца з аверлэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Накладка мышы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Настройка падзей уводу мышы, якія адпраўляюцца з аверлэя. Заўвага: адзіночныя, падвойныя і трайныя дотыкі перадаюць націскі левай, правай і сярэдняй кнопкамі мышы."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "Набор налад накладкі клавіятуры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Выбар аверлэя клавіятуры ў браўзеры файлаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Аўтамаштабаванне аверлэя клавіятуры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Падладка аверлэя клавіятуры пад зыходны фармат. Адключыце для расцягу на ўвесь экран."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Бачнасць аверлэя клавіятуры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Бачнасць усіх элементаў інтэрфейсу аверлэя клавіятуры."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Порт светлавога пісталета"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Порт ядра, які атрымлівае падзеі ўводу светлавога пісталета з аверлэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Любы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Націск курка пры дотыку"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Перадаваць націскі курка, цягам выкарыстання ўводу з паказальніка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Затрымка курка (у кадрах)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Затрымка спрацоўвання курка, каб даць час для зрушэння курсора. Таксама выкарыстоўваецца для рэгістрацыі правільнай колькасці адначасовых націскаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Увод пры дотыку двума пальцамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Выбар падзеі ўводу пры дотыку экрана двума пальцамі. Для правільнага спрацоўвання іншых дзеянняў затрымка курка не павінна раўняцца нулю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Увод пры дотыку трыма пальцамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Выбар падзеі ўводу пры дотыку экрана трыма пальцамі. Для слушнага спрацоўвання іншых дзеянняў затрымка курка не павінна раўняцца нулю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Увод пры дотыку чатырма пальцамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Выбар падзеі ўводу пры дотыку экрана чатырма пальцамі. Для слушнага спрацоўвання іншых дзеянняў затрымка курка не павінна раўняцца нулю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Дазволіць прыцэл па-за экранам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Дазваляе выхад прыцэла за экран. Адключыце, каб прыцэльванне абмяжоўвалася ўнутранымі межамі."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Хуткасць мышы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Налада хуткасці перамяшчэння курсора."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Перацягваць доўгім націскам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Зрабіце доўгі націск па экране для ўтрымлівання кнопкі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Затрымка доўгага націску (мс)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Час утрымлівання, неабходны для доўгага націску."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Перацягваць падвойным націскам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Двойчы краніце экран для ўтрымлівання кнопкі пасля другога націску. Дадае затрымку да клікаў мышшу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Затрымка падвойнага націску (мс)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Дапушчальная затрымка паміж дотыкамі для апрацоўкі падвойнага націску."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Допуск націску"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Дапушчальнае адхіленне пры вызначэнні доўгага ці адзіночнага націску. Выяўляецца ў адсотках меншага памеру экрана."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Уключыць макеты экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Макеты выкарыстоўваюцца для адлюстравання рамак і іншых элементаў афармлення."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Шлях да макета экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Выбар макета экрана ў браўзеры файлаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Абраны выгляд"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Абярыце выгляд у загружаным макеце."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Экранныя апавяшчэнні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Вывад паведамленняў на экран."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Графічныя віджэты"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Уключыць палепшаныя анімацыі, апавяшчэнні, паказальнікі і элементы кіравання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Аўтамаштабаванне віджэтаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Аўтаматычна змяняць памеры апавяшчэнняў, індыкатараў і элементаў кіравання паводле зададзенага маштабу меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Маштаб віджэтаў (поўнаэкранны рэжым)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Ручная ўстаноўка каэфіцыента маштабавання для віджэтаў у поўнаэкранным рэжыме. Улічваецца толькі калі выключана 'Аўтамаштабаванне віджэтаў'. Дазваляе змяняць памер апавяшчэнняў, індыкатараў і элементаў кіравання незалежна ад зададзенага маштабу меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Маштаб віджэтаў (аконны рэжым)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Ручная ўстаноўка каэфіцыента маштабавання для віджэтаў у аконным рэжыме. Улічваецца толькі калі выключана 'Аўтамаштабаванне віджэтаў'. Дазваляе змяняць памер апавяшчэнняў, індыкатараў і элементаў кіравання незалежна ад зададзенага маштабу меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Адлюстраванне частаты кадраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Адлюстроўваць бягучаю колькасць кадраў за секунду."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Інтэрвал абнаўлення паказу частаты кадраў (у кадрах)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "Адлюстраванне частаты кадраў будзе абнаўляцца з зададзеным інтэрвалам у кадрах."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Адлюстраванне колькасці кадраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Адлюстроўваць на экране бягучую колькасць кадраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Адлюстраванне статыстыкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Адлюстроўваць на экране тэхнічную статыстыку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Адлюстраванне выкарыстання памяці"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Адлюстроўваць скарыстаны ды агульны аб'ём памяці ў сістэме."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Інтэрвал абнаўлення паказу выкарыстання памяці (у кадрах)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "Адлюстраванне выкарыстання памяці будзе абнаўляцца з зададзеным інтэрвалам у кадрах."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Паказваць пінг netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Адлюстроўваць пінг для бягучага пакоя netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Апавяшчэнне аб запуску кантэнту"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Паказваць кароткую анімацыю пры загрузцы кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Апавяшчэнне пра злучэнне прылады ўводу (аўтаканфігурацыя)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Апавяшчэнні пра чыт-коды"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Адлюстроўваць паведамленне пры загрузцы чыт-кодаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Апавяшчэнне аб патчы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Адлюстроўваць паведамленне, калі да ROM ужыты софт-патч."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Адлюстроўваць паведамленне пры злучэнне/адлучэнне прылад уводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Апавяшчэнне аб загрузцы раскладак кіравання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Адлюстроўваць паведамленне пры загрузцы файлаў раскладак кіравання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Апавяшчэнне аб загрузцы перавызначэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Паказваць паведамленне пры загрузцы перавызначэння канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Апавяшчэнне аб загрузцы абранага дыска"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Адлюстроўваць паведамленне пры аўтаматычным запуску апошняга выкарыстанага дыска з дапамогай M3U-плэйліста для мультыдыскавага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Апавяшчэнне аб аперацыях з дыскам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Адлюстроўваць паведамленне пры ўстаўцы і выманні дыска."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Апавяшчэнні пра захаванне стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "Адлюстроўваць на экране паведамленне пры захаванні ды загрузцы станаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Апавяшчэнні пра перамотку наперад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Адлюстроўваць на экране індыкатар пры перамотцы змесціва наперад."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Апавяшчэнні пра здымкі экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Адлюстроўваць паведамленне пры захаванні скрыншота."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Працягласць апавяшчэння здымку экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Вызначыць працягласць адлюстравання паведамлення здымку экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "Звычайная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Хуткая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Вельмі хуткая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Імгненная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Эфект успышкі здымка экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Адлюстраванне на экране эфекту ўспышкі з зададзенай працягласцю пры стварэнні скрыншота."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "УКЛ (звычайна)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "УКЛ (хутка)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Апавяшчэнне аб частаце абнаўлення"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Адлюстроўваць паведамленне пры змене частаты абнаўлення."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Дадатковыя апавяшчэнні Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Адлюстроўваць другарадныя сеткавыя паведамленні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Апавяшчэнні толькі ў меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Адлюстроўваць апавяшчэння толькі калі адкрыта меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Шрыфт апавяшчэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Выбраць шрыфт для апавяшчэнняў на экране."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Памер апавяшчэнняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Палажэнне апавяшчэння (па гарызанталі)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Вызначае становішча тэксту на экране па восі X. 0 адпавядае мяжы злева."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Палажэнне апавяшчэння (па вертыкалі)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Вызначае становішча тэксту на экране па восі Y. 0 адпавядае ніжняй мяжы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Колер апавяшчэнняў (чырвоны)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Ўстаноўка чырвонага колеру для тэкставых паведамленняў на экране. Дыяпазон дапушчальных значэнняў ад 0 да 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Колер апавяшчэнняў (зялёны)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Ўстаноўка зялёнага колеру для тэкставых паведамленняў на экране. Дыяпазон дапушчальных значэнняў ад 0 да 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Колер апавяшчэнняў (сіні)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Ўстаноўка сіняга колеру для тэкставых паведамленняў на экране. Дыяпазон дапушчальных значэнняў ад 0 да 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Фон апавяшчэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Уключае каляровы фон для паведамленняў на экране."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Колер фону апавяшчэнняў (чырвоны)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Ўстаноўка чырвонага колеру для фону паведамленняў. Дыяпазон дапушчальных значэнняў ад 0 да 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Колер фону апавяшчэнняў (зялёны)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Ўстаноўка зялёнага колеру для фону паведамленняў. Дыяпазон дапушчальных значэнняў ад 0 да 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Колер фону апавяшчэнняў (сіні)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Ўстаноўка сіняга колеру для фону паведамленняў. Дыяпазон дапушчальных значэнняў ад 0 да 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Непразрыстасць фону апавяшчэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Ўстаноўка празрыстасці фону тэкставых паведамленняў. Дыяпазон дапушчальных значэнняў ад 0.0 да 1.0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Бачнасць пунктаў меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Змяніць адлюстраванне элементаў меню RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Выгляд"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Змяніць налады выгляду экраннага меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "Значок праграмы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Змяніць значок праграмы."
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "Адлюстраванне ніжняга экрана 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Налады знешняга выгляду ніжняга экрана."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Паказ пашыраных налад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Адлюстраванне пашыраных налад для дасведчаных карыстальнікаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Рэжым кіёска"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Абараняе наладу, хаваючы доступ да ўсіх параметраў канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Пароль для адмены рэжыму кіёска"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Калі рэжым кіёска ўключаны, усталёўка пароля дазваляе адключыць яго з меню. Для гэтага абярыце ў галоўным меню 'Адключыць рэжым кіёска' і ўвядзіце пароль."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Цыклічная пракрутка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Пераходзіць у пачатак/канец спісу, калі дасягнута гарызантальная ці вертыкальная мяжа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Паўза пры выкліку меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Прыпыняць змесціва пры выкліку меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Аднаўляць кантэнт пасля захавання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Аўтаматычна зачыняць меню і аднаўляць кантэнт пасля захавання або загрузкі стану. Адключэнне налады павялічвае хуткасць захавання для слабых прылад."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Аднаўляць кантэнт пры змене дыска"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Аўтаматычна зачыняць меню і аднаўляць кантэнт, калі быў устаўлены ці загружаны новы дыск."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Выхад па закрыцці змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Аўтаматычна зачыняць RetroArch пры прыпыненні кантэнту. Інтэрфейс каманднага радка будзе зачынены толькі калі кантэнт быў запушчаны праз яго."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Тайм-аўт застаўкі меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Пры выкліку меню запускаць захавальнік экрана пасля зададзенага перыяду бяздзейнасці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Анімацыя застаўкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Паказваць анімацыю пры запуску застаўкі. Нязначна ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Снег"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Зорнае поле"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX,
   "Вір"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Хуткасць анімацыі застаўкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Настройка хуткасці эфекту анімацыі застаўкі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Падтрымка мышы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Уключае навігацыю ў меню з дапамогай мышы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Падтрымка дотыкаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Уключае навігацыю ў меню з дапамогай тачскрына."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Задачы асобным патокам"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Выконваць задачы ў асобным патоку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Паўза пры страце фокусу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Прыпыняць кантэнт пры страце фокусу акна RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Адключыць кампазіцыю працоўнага стала"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Кампазіцыя выкарыстоўваецца сродкамі Windows для адлюстравання візуальных эфектаў, вызначэння неактыўных вокнаў і г. д."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Прымусовае адключэнне кампазіцыі. У наш час працуе толькі ў Windows Vista/7."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Паскарэнне прагорткі меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Максімальная хуткасць курсора пры ўтрыманні пракруткі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Затрымка прагорткі меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Пачатковая затрымка ў мілісекундах пры ўтрыманні напрамку пракруткі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Дадатковы інтэрфейс"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Запуск дадатковага інтэрфейсу пры загрузцы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Запускаць дадатковы драйвер карыстальніцкага інтэрфейсу пры загрузцы (калі даступна)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Меню працоўнага стала (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Адкрываць меню працоўнага стала падчас запуску"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Хуткае меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Змяніць адлюстраванне элементаў хуткага меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Налады"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Выбар элементаў меню налад якія адлюстроўваюцца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Паказ 'Загрузіць ядро'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Паказваць опцыю 'Загрузіць ядро' ў галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Паказ 'Загрузіць змесціва'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Паказваць опцыю 'Загрузіць змесціва' ў галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Паказ 'Загрузіць дыск'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Паказваць опцыю 'Загрузіць дыск' у галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Паказ 'Дамп дыска'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Паказваць опцыю 'Дамп дыска' ў галоўным меню."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "Паказ 'Выняць дыск'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Паказваць опцыю 'Выняць дыск' у галоўным меню."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Паказ 'Анлайнавы абнаўляльнік'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Паказваць опцыю 'Анлайн абнаўляльнік' у галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Паказ 'Сцягвальнік ядраў'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Паказваць опцыі для спампоўвання ядраў (і інфармацыйных файлаў) у 'Анлайн-абнаўленнях'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Паказваць састарэлы 'Абнаўляльнік мініяцюр'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Паказваць опцыю для спампоўкі пакетаў мініяцюр у 'Анлайн-абнаўленнях'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Паказ 'Звесткі'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Паказваць опцыю 'Звесткі' ў галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Паказ 'Файл канфігурацыі'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Паказваць опцыю 'Файл канфігурацыі' ў галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Паказ 'Даведка'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Паказваць опцыю 'Даведка' ў галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Паказ 'Выйсці з RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Паказваць опцыю 'Выйсці з RetroArch' у галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Паказ 'Перазапусціць RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Паказваць опцыю 'Перазапусціць RetroArch' у галоўным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Паказ 'Налады'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Паказваць меню 'Налады'. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Пароль для адлюстравання ўкладкі 'Налады'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Усталяванне пароля дазваляе аднавіць доступ да ўкладкі 'Налады', калі яна была ўтоена. Для гэтага абярыце ў галоўным меню радок 'Уключыць укладку Налады' і ўвядзіце пароль."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Паказ 'Упадабанае'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Паказваць меню 'Упадабанае'. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Паказ 'Відарысы'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Паказваць меню 'Відарысы'. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Паказ 'Музыка'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Паказваць меню 'Музыка'. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Паказ 'Відэа'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Паказваць меню 'Відэа'. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Паказ 'Сеткавая гульня'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Паказваць меню 'Сеткавая гульня'. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Паказ 'Гісторыя'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Паказваць меню нядаўняй гісторыі. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "Паказ 'Імпартаваць змесціва'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "Паказваць меню 'Імпартаваць змесціва'. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH( /* FIXME can now be replaced with MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD */
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Паказ 'Імпартаваць змесціва'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Паказваць опцыю 'Імпарт кантэнту' у галоўным меню ці на ўкладцы плэйлістоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Галоўнае меню"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Меню плэй-лістоў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Паказ 'Плэй-лісты'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Паказваць плэй-лісты ў галоўным меню. Ігнаруецца ў GLUI, калі ўключаныя карткі плэй-лістоў ды панэлі навігацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "Паказ картак плэй-лістоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "Паказваць карткі плэй-лістоў. Не мае ўплыву на RGUI. Панэль навігацыі мае быць уключаная ў GLUI. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Паказ 'Агляд'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Паказваць опцыю даследчыка змесціва. (Патрабуецца перазапуск на Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Паказ 'Аўтаномныя ядры'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Тыпы ядраў (пры іх наяўнасці), якія адлюстроўваюцца ў меню 'Аўтаномныя ядры'. У рэжыме 'Уручную' бачнасць кожнага ядра можна наладзіць у меню 'Кіраванне ядрамі' (патрабуецца перазапуск для Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Усе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Адзінкавыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Уласныя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Паказ даты і часу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Паказваць у меню бягучую дату ды/або час."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Фармат даты і часу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Выбраць стыль адлюстравання даты і/або часу ў меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Раздзяляльнік даты"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Вызначыць сімвал для ўжывання ў якасці раздзяляльніка паміж складнікамі год/месяц/дзень, калі бягучая дата паказваецца ў меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Паказ узроўню акумулятара"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Паказваць у меню бягучы ўзровень акумулятара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Паказ назвы ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Паказваць у меню назву бягучага ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Паказ тлумачэнняў да меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Паказваць дадатковыя звесткі па пунктах меню."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Адлюстраванне застаўкі"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Паказваць застаўку пры запуску. Гэтая ўласцівасць прадвызначаецца адключанай пры запуску ўпершыню."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Паказ 'Працягнуць'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Паказваць опцыю працягвання змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Паказ 'Перазапусціць'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Паказваць опцыю перазапуску змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Паказ 'Закрыць змесціва'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Паказваць опцыю 'Закрыць змесціва'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Паказваць падменю 'Захаванні'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Адлюстроўваць опцыі захаванняў у асобным меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Паказваць 'Захаваць/Загрузіць'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Паказваць опцыі захавання/загрузкі стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "Паказваць 'Кіраванне паўторамі'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "Паказваць опцыі для запісу/прайгравання файлаў паўтору."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Паказваць 'Адмяніць захаванне/загрузку'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Паказваць опцыі адмены захавання/загрузкі стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Паказ 'Опцыі ядра'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Паказваць опцыю 'Опцыі ядра'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Паказ 'Скінуць опцыі на дыск'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Паказваць пункт 'Скінуць опцыі на дыск' у меню 'Опцыі > Кіраванне опцыямі ядра'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Паказ 'Кіраванне'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Паказваць опцыю 'Кіраванне'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Паказ 'Стварыць здымак экрана'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Паказваць опцыю 'Стварыць здымак экрана'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Паказ 'Пачаць запіс'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Паказваць опцыю 'Пачаць запіс'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Паказ 'Пачаць трансляцыю'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Паказваць опцыю 'Пачаць трансляцыю'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Паказваць 'Аверлэі'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Паказваць опцыю 'Аверлэі'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Паказваць 'Макет экрана'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Паказваць опцыю 'Макет экрана'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Паказваць 'Затрымка'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Паказваць опцыю 'Затрымка'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Паказ 'Перамотка назад'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Паказваць опцыю 'Перамотка назад'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Паказ 'Захаваць перавызначэнні ядра'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Паказваць опцыю 'Захаваць перавызначэнні ядра' ў меню 'Перавызначэнні'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Паказ 'Захаваць перавызначэнні каталога змесціва'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Паказваць опцыю 'Захаваць перавызначэнні каталога змесціва' ў меню 'Перавызначэнні'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Паказ 'Захаваць перавызначэнні гульні'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Паказваць опцыю 'Захаваць перавызначэнні гульні' ў меню 'Перавызначэнні'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Паказваць 'Чыт-коды'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Паказваць опцыю 'Чыт-коды'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Паказ 'Шэйдары'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Паказваць опцыю 'Шэйдары'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Паказ 'Дадаць да ўпадабанага'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Паказваць опцыю 'Дадаць да ўпадабанага'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Паказ 'Дадаць да плэй-ліста'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Паказваць опцыю 'Дадаць да плэй-ліста'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Паказ 'Задаць сувязь з ядром'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Паказваць опцыю 'Задаць сувязь з ядром', калі не выконваецца змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Паказ 'Скінуць сувязь з ядром'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Паказваць опцыю 'Скінуць сувязь з ядром', калі не выконваецца змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Паказ 'Сцягнуць мініяцюры'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Паказваць опцыю 'Сцягнуць мініяцюры', калі не выконваецца змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Паказ 'Звесткі'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Паказваць опцыю 'Звесткі'."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Паказ 'Драйверы'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Паказваць налады 'Драйверы'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Паказ 'Відэа'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Паказваць налады 'Відэа'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Паказ 'Аўдыя'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Паказваць налады 'Аўдыя'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Паказ 'Увод'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Паказваць налады 'Увод'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Паказ 'Латэнтнасць'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Паказваць налады 'Латэнтнасць'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Паказ 'Ядро'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Паказваць налады 'Ядро'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Паказ 'Канфігурацыя'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Паказваць налады 'Канфігурацыі'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Паказ 'Захаванне'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Паказваць налады 'Захаванне'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Паказ 'Вядзенне журнала'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Паказваць налады 'Вядзенне журнала'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Паказ 'Файлавы браўзер'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Паказваць налады 'Файлавы браўзер'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Паказ 'Рэгуляванне кадраў'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Паказваць налады 'Рэгуляванне кадраў'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Паказ 'Запіс'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Паказваць налады 'Запіс'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Паказ 'Адлюстраванне на экране'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Паказваць налады 'Адлюстраванне на экране'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Паказ 'Карыстальніцкі інтэрфейс'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Паказваць налады 'Карыстальніцкі інтэрфейс'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Паказ 'Сэрвіс ШІ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Паказваць налады 'Сэрвіс ШІ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Паказ 'Спецыяльныя магчымасці'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Паказваць налады 'Спецыяльныя магчымасці'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Паказ 'Кіраванне сілкаваннем''"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Паказваць налады 'Кіраванне сілкаваннем''."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Паказ 'Дасягненні'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Паказваць налады 'Дасягненні'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Паказ 'Сеціва'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Паказваць налады 'Сеціва'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Паказ 'Плэй-лісты'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Паказваць налады 'Плэй-лісты'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Паказ 'Карыстальнік'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Паказваць налады 'Карыстальнік'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Паказ 'Каталог'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Паказваць налады 'Каталог'."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "Паказ 'Стрым'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "Паказваць налады 'Стрым'."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Маштабны каэфіцыент"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Змяняе памер элементаў карыстальніцкага інтэрфейсу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Фонавы відарыс"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Вызначыць відарыс у якасці фону меню. Зададзеныя ўручную ды дынамічныя маюць перавагу над 'Колеравая тэма'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Непразрыстасць фону"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Змяніць непразрыстасць фонавага відарысу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Непразрыстасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Змяніць непразрыстасць прадвызначанага фону меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Ужыванне пажаданай сістэмнай колеравай тэмы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Ужываць колеравую тэму аперацыйнай сістэмы (калі існуе). Перавызначае налады тэмы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Першасная мініяцяюра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Тып мініяцюр для адлюстравання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Парог апскейлінга мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Аўтаматычна маштабаваць эскізы з шырынёй/вышынёй менш зададзенага значэння. Падвышае якасць малюнка, умерана ўплываючы на ​​прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Анімацыя бягучага радка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Метад скролінга, які выкарыстоўваецца для адлюстравання доўгіх радкоў тэксту ў меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Хуткасць бягучага радка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Хуткасць пракруткі доўгіх радкоў тэксту ў меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Згладжванне бягучага радка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Выкарыстоўваць гладкую анімацыю пракруткі пры адлюстраванні доўгіх тэкставых радкоў у меню. Амаль не ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Запамінаць выбар пры змене картак"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Запамінаць пазіцыю курсора ў картках. RGUI не мае картак, але плэй-лісты ды налады паводзяць сябе гэтаксама."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Заўсёды"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Толькі для плэй-лістоў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Толькі для галоўнага меню ды налад"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Рэжым вываду сэрвісу ШІ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Паказваць пераклад праз накладку тэксту (рэжым выявы), прайграваць праз сінтэз маўлення (маўленне) або выкарыстоўваць сістэмны дыктар накшталт NVDA (дыктар)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "URL сэрвісу ШІ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Адрас http:// URL, які накіроўвае на сэрвіс перакладу на выкарыстанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Выклік сэрвісу ШІ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Уключыць запуск сэрвісу ШІ пры націсканні прывязанай гарачай клавішы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Паўза падчас перакладу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Прыпыняць ядро пакуль перакладаецца экран."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Мова арыгінала"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "Мова, з якой будзе ажыццяўляцца пераклад. Калі выбрана 'Па змаўчанні', сэрвіс будзе спрабаваць вызначыць мову аўтаматычна. Устаноўка пэўнай мовы павышае дакладнасць перакладу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Мэтавая мова"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "Мова, на якую будзе ажыццяўляцца пераклад. Па змаўчанні выкарыстоўваецца англійская."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Уключыць спецыяльныя магчымасці"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Упаўнаважыць сінтэз маўлення, каб дапамагчы ў навігацыі па меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Хуткасць сінтэзу маўлення"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Хуткасць сінтэзаванага голасу."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Дасягненні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Здабывайце дасягненні ў класічных гульнях. Для атрымання падрабязных звестак наведайце 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Рэжым хардкору"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Адключае чыты, перамотку, запаволены рух ды загрузку захаванняў стану. Дасягненні ў рэжыме хардкора маюць спецыяльную пазнаку, каб паказаць іншым гульцам вашыя навыкі без магчымасцяў эмулятара. Пераключэнне наладкі падчас гульні прывядзе да яе перазапуску."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Дошкі лідараў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "Пашыраны статус"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Перыядычная адпраўка кантэкстнай інфармацыі па гульні на сайт RetroAchievements. Не працуе, калі ўключаны 'Рэжым хардкора'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Значкі дасягненняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Адлюстроўваць значкі ў спісе дасягненняў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Тэставыя неафіцыйныя дасягненні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Выкарыстоўваць неафіцыйныя дасягненні ды/або бэта функцыі ў мэтах тэставання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Гук раскрыцця"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Прайграваць гук пры раскрыцці дасягнення."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Аўтаматычны здымак экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Аўтаматычна ствараць здымак экрана пры набыцці дасягнення."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Рэжым перагульвання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Пачынаць сеанс з актывацыяй усіх дасягненняў (нават з раней раскрытымі)."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Выгляд"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Настройка размяшчэння і зрушэнняў для наэкранных апавяшчэнняў аб дасягненнях."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Размяшчэнне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Выбар кута/краю экрана, у якім будуць адлюстроўвацца апавяшчэнні аб дасягненнях."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Зверху злева"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "Зверху ў цэнтры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Зверху справа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Знізу злева"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Унізе па цэнтры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "Знізу справа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Выраўнаваны водступ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Уключае выраўноўванне апавяшчэнняў аб дасягненнях з іншымі паведамленнямі. Адключыце для ручной усталёўкі значэнняў водступу/становішча."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "Ручны гарызантальны водступ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Адлегласць ад левага/правага краю экрана для кампенсацыі вылетаў разгорткі дысплея."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "Ручны вертыкальны водступ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Адлегласць ад верхняга/ніжняга краю экрана для кампенсацыі вылетаў разгорткі дысплея."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "Бачнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "Настройка элементаў і апавяшчэнняў, якія адлюстроўваюцца на экране. Не ўплывае на працу функцый."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "Зводка пры запуску"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "Паказваць інфармацыю па загружанай гульні і бягучы прагрэс карыстальніка.\nУ рэжыме 'Для ўсіх апазнаных гульняў' будзе адлюстроўвацца зводка па гульнях без даступных дасягненняў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "Усе распазнаныя гульні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "Гульні з дасягненнямі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "Апавяшчэнні пра раскрыццё"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "Паказвае апавяшчэнне пры раскрыцці дасягнення."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "Апавяшчэнні пра майстэрства"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "Паказвае апавяшчэнне пры раскрыцці ўсіх дасягненняў гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Індыкатары актыўных выпрабаванняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Паказваць на экране значкі дасягненняў, для якіх выконваюцца ўмовы атрымання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Індыкатар хода выканання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Паказваць на экране індыкатар пры прагрэсе ў атрыманні дасягнення."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "Паведамленні пра адкрыццё дошкі лідараў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "Паказвае апісанне дошкі лідараў, калі яна становіцца актыўнай."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Паведамленні пра ўнясенне на дошку лідараў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Паказвае паведамленне з унесеным на дошку лідараў значэннем пры паспяховай спробе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Паведамленні пра няўдачу з дошкай лідараў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Паказвае паведамленне пры няўдалай спробе трапіць на дошку лідараў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Лічыльнік табліцы лідараў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Выводзіць на экран бягучы рахунак для актыўнай табліцы лідараў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Паведамленні пра ўваход"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Паказваць паведамленні, звязаныя з доступам да акаўнта RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Падрабязныя паведамленні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Паказвае дадатковыя дыягнастычныя паведамленні ды памылкі."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Публічны анонс сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Уключае анансаванне сеансаў сеткавай гульні. Калі выключана, кліенты далучаюцца не праз лобі, а ўручную."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Выкарыстоўваць сервер-пасярэднік"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Пераадрасаванне злучэнняў праз прамежкавы сервер. Карысна ў тых выпадках, калі хост выкарыстоўвае брандмаўэр ці маюцца праблемы з NAT/UPnP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Лакацыя сервера-пасярэдніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Выбар вызначанага рэлейнага сервера. Лакацыі, якія знаходзяцца геаграфічна бліжэй звычайна маюць меншую затрымку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Ручны ўвод адраса сервера-пасярэдніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Увядзіце адрас вашага прамежкавага сервера. Фармат: адрас або адрас|порт."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "Паўночная Амерыка (усходняе ўзбярэжжа, ЗША)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "Заходняя Еўропа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "Паўднёвая Амерыка (паўднёвы ўсход, Бразілія)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Паўднёва-Усходняя Азія"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_5,
   "Усходняя Азія (Чхунчхон, Паўднёвая Карэя)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Уласная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Адрас сервера"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Адрас хаста для злучэння."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Порт TCP сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Порт IP-адрасы хаста. Можа быць портам TCP ці UDP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Максімум адначасовых злучэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Максімальная колькасць актыўных злучэнняў, якая будзе прымаць хост да адмовы новым."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Абмежаванне пінга"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "Максімальная сеткавая затрымка (пінг), прымальная для хаста. Значэнне 0 здымае абмежаванне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Пароль сервера"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Пароль, які выкарыстоўваецца для злучэння кліентаў з хастом."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Пароль сервера ў рэжыме назіральніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Пароль для кліентаў, якія злучаюцца з хастом ў рэжыме назіральніка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Рэжым назіральніка сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Пачынаць сеткавую гульню ў рэжыме назіральніка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Ўстаноўка рэжыму назіральніка для сеансаў сеткавай гульні. Пры ўключэнні сеткавая гульня будзе запускацца ў рэжыме назіральніка. Можа быць зменена ў любы час."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Згасанне чата"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Згасаць паведамленні чата з цягам часу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Колер чата (мянушка)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Фармат: #RRGGBB або RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Колер чата (паведамленне)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Фармат: #RRGGBB або RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Дазвол паўзы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Дазваляць гульцу прыпыняцца падчас сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Дазваляць кліентаў у slave-рэжыме"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Дазваляць злучэнні ў slave-рэжыме. Slave-кліенты патрабуюць вельмі мала вылічальнай магутнасці, але сеткавая затрымка для іх істотна вышэй."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Забараняць кліентаў не ў slave-рэжыме"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Забараняць злучэнні не ў slave-рэжыме. Рэкамендуецца ўключаць толькі для вельмі хуткіх сетак са слабымі машынамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Кадры праверкі сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "Частата (у кадрах), з якой праводзіцца праверка сінхранізацыі хаста і кліента."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "Частата ў кадрах, з якой звяраецца сінхранізацыя кліента і хаста. Для большасці ядраў не дае бачнага эфекту і можа ігнаравацца. Для недэтэрмінаваных ядраў вызначае перыядычнасць сінхранізацыі сеткавых піраў. З нестабільнымі ядрамі ўстаноўка да значэнняў, выдатным ад нул[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Кадры затрымкі ўводу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Колькасць кадраў затрымкі ўводу для маскавання запазнення сеткавай гульні. Падвышае плыўнасць і памяншае нагрузку на працэсар, але ўносіць адчувальную затрымку ўводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Колькасць кадраў затрымкі ўводу для маскавання запазнення падчас сеткавай гульні.\nУносіць лакальную затрымку ўводу для максімальнай сінхранізацыі паміж бягучым кадрам і кадрамі, якія атрымліваюцца з сеткі. Падвышае плыўнасць і змяншае нагрузку на CPU, але павялічвае зат[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Дыяпазон кадраў затрымкі ўводу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Дыяпазон кадраў затрымкі ўводу для згладжвання запазнення сеткі. Зніжае разсінхранізацыі і патрабаванні сеткавай гульні да прадукцыйнасці за кошт непрадказальнай затрымкі ўводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Дыяпазон затрымкі ўводу для згладжвання запазнення сеткі.\nДынамічна падладжвае кадры затрымкі ўводу для балансу паміж працэсарным часам, затрымкай уводу і запазненнем сеткі. Падвышае плыўнасць і змяншае нагрузку на CPU, але непрадказальна ўплывае на затрымку ўводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Абыход NAT у Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "У рэжыме хаста спрабаваць слухаць злучэнні з публічнага Інтэрнэту, выкарыстоўваючы UPnP або падобныя пратаколы, каб пазбегнуць лакальных сетак."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Перадача лічбавага ўводу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Запатрабаванне прылады %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Запрошваць гульню з дадзенай прыладай уводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Сеткавыя каманды"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Сеткавы камандны порт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "Сеткавы RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Базавы порт сеткавага RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Сеткавы RetroPad карыстальніка %d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "Каманды stdin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Камандны інтэрфейс stdin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Сцягванне мініяцюр па патрабаванні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Аўтаматычна сцягваць адсутныя мініяцюры пры аглядзе плэй-лістоў. Значна ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Налады абнаўляльніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Дазвол да налад абнаўляльніка ядраў"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Спасылка для загрузкі ядраў з білдбота"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "Сеткавы адрас тэчкі з абнаўленнямі ядраў на білдбоце Libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Спасылка для загрузкі рэсурсаў з білдбота"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "Сеткавы адрас тэчкі з абнаўленнямі рэсурсаў на білдбоце Libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Аўтаматычная распакаванне спампаваных архіваў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Аўтаматычна распакоўваць спампаваныя архівы пасля заканчэння спампоўкі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Паказваць эксперыментальныя ядры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Адлюстроўваць у спісе сцягвальніка 'эксперыментальныя' ядры. Звычайна яны выкарыстоўваюцца для адладкі/тэставання і не рэкамендаваныя дзеля сталага выкарыстання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Рэзервовае капіраванне ядраў пры абнаўленні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Аўтаматычна ствараць рэзервовыя копіі ўсталяваных ядраў пры анлайн-абнаўленні. У выпадку рэгрэсу забяспечвае просты адкат да працоўнай версіі ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Памер гісторыі рэзервовых копій ядраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Настройка колькасці аўтаматычных рэзервовых копій для кожнага ўсталяванага ядра. Калі дасягнуты мяжа, стварэнне новай рэзервовай копіі пры абнаўленні ядра прыводзіць да выдалення самай ранняй копіі. Не ўплывае на рэзервовыя копіі, захаваныя ўручную."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Гісторыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Весці плэй-ліст нядаўна скарыстаных гульняў, відарысаў, музыкі ды відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Памер гісторыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Абмежаваць колькасць запісаў у плэй-лістах гісторыі гульняў, відарысаў, музыкі ды відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Памер упадабанага"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Максімальны лік запісаў для плэйліста 'Абранае'. Пры дасягненні межавага значэння новыя запісы не будуць дадавацца да выдалення старых. Значэнне '-1' здымае абмежаванне на колькасць запісаў.\nУВАГА: памяншэнне значэння выдаліць існыя запісы!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Дазвол змены назвы запісаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Дазволіць змяняць назву запісаў плэй-ліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Дазвол пазбаўлення запісаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Дазволіць прыбіраць запісы плэй-ліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Алфавітнае сартаванне плэй-лістоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Размяшчаць плэйлісты з кантэнтам у алфавітным парадку, за выключэннем плэйлістоў 'Гісторыя запуску', 'Выявы', 'Музыка' і 'Відэа'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Захаванне плэй-лістоў у старым фармаце"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Запісваць плэй-лісты ў састарэлым фармаце простага тэксту. Калі выключана, плэй-лісты будуць фарматаваныя ў JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Сціск плэй-лістоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Сціскаць плэйлісты пры запісе ў памяць. Памяншае памер файлаў і час загрузкі за кошт нязначнага павышэння нагрузкі на працэсар. Працуе для плэйлістоў у старым і новым фарматах."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Паказваць прывязкі ядраў у плэйлістах"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Дадаваць для запісаў у плэйлістах тэг прывязкі ядра (пры наяўнасці).\nІгнаруецца, калі ўключана адлюстраванне пазнак у плэйлістах."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Паказваць пазнакі ў плэйлістах"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Паказваць дадатковыя звесткі на кожны запіс плэй-ліста накшталт бягучай сувязі з ядром ды час за гульнёй (калі даступна). Па-рознаму ўплыве на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Паказваць значкі кантэнту ў Гісторыі запуску і Абраным"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Паказваць адмысловыя значкі на кожны запіс плэй-ліста гісторыі ды ўпадабанага. Па-рознаму ўплыве на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Ядро:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Часу за гульнёй:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Апошні запуск:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "секунда"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "секунд"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "хвіліна"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "хвілін"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "гадзіна"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "гадзін"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "дзень"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "дзён"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "тыдзень"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "тыдняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "месяц"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "месяцаў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "год"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "гадоў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "таму"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Паказваць нумар запісу ў плэйлісце"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Адлюстроўваць нумары запісаў пры праглядзе плэйлістоў. Фармат вываду залежыць ад бягучага драйвера меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Пазнака часу запуску ў плэйлісце"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Выбар тыпу часу запуску, які адлюстроўваецца ў пазнаках плэйліста.\nАдпаведная опцыя павінна быць уключана ў наладах 'Захаванняў'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Фармат даты і часу апошняга запуску"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Фармат даты/часу пры адлюстраванні інфармацыі аб апошнім запуску. На некаторых платформах пазнакі \"(да/пасля поўдня)\" могуць крыху ўплываць на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Недакладнае супастаўленне архіваў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Пры пошуку запісаў у плэйлістах, асацыіраваных з архівамі, шукаць супадзенні толькі па імю файла з архівам замест [імя файла]+[кантэнт]. Уключыце, каб пазбегнуць з'яўлення паўторных запісаў у гісторыі запуску пры загрузцы сціснутых файлаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Сканаванне без супастаўлення ядру"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Дазволіць сканіраванне і даданне кантэнту ў плэйліст без устаноўленых сумяшчальных ядраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "Сканаваць з праверкай CRC на магчымыя дублікаты"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "Часам серыйныя нумары выяў могуць дублявацца, у прыватнасці для гульняў PSP/PSN. Сканаванне толькі па серыйных нумарах можа прыводзіць да супастаўлення кантэнту з няправільнай сістэмай. Дадзеная опцыя дадае праверку CRC, якая адчувальна запавольвае сканаванне, але робіць яго[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Кіраванне плэй-лістамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Зрабіць справы па вядзенню плэй-лістоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Партатыўныя плэй-лісты"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Калі ўключана і зададзены шлях да каталога 'Браўзер файлаў', значэнне захоўваецца ў плэйлісце. Пры загрузцы плэйліста на іншай сістэме з уключанай опцыяй, значэнне 'Браўзер файлаў' параўноўваецца са значэннем у плэйлісце і калі яны адрозніваюцца, шляхі ў плэйлісце аўтамат[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "Выкарыстоўваць назвы файлаў для пошуку мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "Калі ўключана, пошук мініяцюр замест назваў запісаў робіцца па імёнах файлаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "Дазвол усіх, што маюць падтрымку, тыпаў відарысаў для мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "Калі ўключана, лакальныя мініяцюры можна дадаваць ва ўсіх тыпаў відарысаў, якія падтрымлівае RetroArch (напрыклад, jpeg). Можа мець невялікі ўплыў на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "Кіраваць"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Прадвызначанае ядро"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Вызначыць ядро для запуску змесціва праз запіс плэй-ліста, што не мае існай сувязі з ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Скінуць сувязь з ядром"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Прыбраць усе актуальныя сувязі з ядром для ўсіх запісаў плэй-ліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Рэжым адлюстравання метак"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Наладзіць адлюстраванне метак для кантэнту ў бягучым плэйлісце."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Парадак сартавання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Вызначае парадак сартавання запісаў у бягучым плэйлісце."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Ачысціць плэй-ліст"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Спраўдзіць сувязі з ядром і прыбраць хібныя запісы ды дублікаты."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Абнавіць плэй-ліст"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Дадаць новы кантэнт і выдаліць няправільныя запісы шляхам паўтору апошняй аперацыі 'Ручное сканаванне', скарыстанай для стварэння або змены плэйліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Выдаліць плэй-ліст"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Прыбраць плэй-ліст з файлавай сістэмы."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Прыватнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Змяніць налады прыватнасці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Уліковыя запісы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Кіраванне наладамі акаўнтаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Імя карыстальніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Увядзіце тут сваё імя карыстальніка. Яно будзе выкарыстоўвацца, між іншым, для сеансаў сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Мова"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Задаць мову карыстальніцкага інтэрфейса."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Лакалізацыя меню і наэкранных паведамленняў у адпаведнасці з абранай мовай. Неабходны перазапуск для ўжывання змен.\nКаля кожнай опцыі паказаны прагрэс перакладу. Для элементаў меню без перакладу выкарыстоўваецца англійская мова."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Дазвол камеры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Дазволіць ядрам доступ да камеры."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Статус актыўнасці Discord (Rich Presence)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Дазваляць дадатку Discord паказ звестак аб бягучым кантэнце.\nДаступна толькі для афіцыйнага настольнага кліента."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Дазвол адсочвання месцазнаходжання"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Дазволіць ядрам доступ да месцазнаходжання."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Здабывайце дасягненні ў класічных гульнях. Для атрымання падрабязных звестак наведайце 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Дадзеныя для ўваходу ва ўліковы запіс RetroAchievements. Для стварэння бясплатнага ўліковага запісу наведайце retroachievements.org.\nПасля завяршэння рэгістрацыі ўвядзіце імя карыстальніка і пароль у RetroArch."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Імя карыстальніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Увядзіце імя карыстальніка для акаўнта RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Пароль"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Увядзіце пароль ад вашага акаўнта RetroAchievements. Максімальная даўжыня: 255 сымбаляў."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "Ключ стрыму YouTube"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Ключ стрыму Twitch"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Ключ стрымінгу Facebook Gaming"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Сістэмныя файлы/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "Каталог захоўвання вобразаў BIOS, загрузачных ПЗУ і іншых сістэмных файлаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Спампоўкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Сцягнутыя файлы захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Рэсурсы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Каталог захоўвання рэсурсаў меню RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Дынамічныя фоны"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Каталог захоўвання фонавых малюнкаў, якія выкарыстоўваюцца ў меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Мініяцюры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Каталог захоўвання мініяцюр вокладак, скрыншотаў і тытульных экранаў."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Пачатковы каталог"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Задаць пачатковы каталог для файлавага браўзера."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Файлы канфігурацыі"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Прадвызначаны файл канфігурацыі захоўваецца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Ядры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Ядры libretro захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Звесткі ядраў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Каталог захоўвання інфармацыйных файлаў ядраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Базы даных"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Базы даных захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Файлы з чыт-кодамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Каталог захоўвання файлаў з чыт-кодамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Відэафільтры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Каталог захоўвання графічных CPU-фільтраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Аўдыяфільтры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Каталог захоўвання гукавых DSP-фільтраў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Графічныя шэйдары"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Каталог захоўвання GPU-шэйдараў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Запісы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Запісы захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Канфігурацыі запісу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Канфігурацыі запісу захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Накладкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Накладкі захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Накладкі клавіятур"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "Накладкі клавіятур захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Макеты экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Каталог захоўвання макетаў экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Здымкі экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Здымкі экрана захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Профілі кантролераў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Каталог захоўвання профіляў для аўтаматычнай наладкі прылад уводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Прывязкі ўводу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Каталог захоўвання перавызначэнняў прывязак уводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Плэй-лісты"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Плэй-лісты захоўваюцца ў гэтым каталозе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Плэй-ліст упадабанага"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Захоўваць плэй-ліст упадабанага ў гэты каталог."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Плэй-ліст гісторыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Захоўваць плэй-ліст гісторыі ў гэты каталог."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Плэй-ліст відарысаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Захоўваць плэй-ліст відарысаў у гэты каталог."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Плэй-ліст музыкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Захоўваць плэй-ліст музыкі ў гэты каталог."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Плэй-ліст відэа"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Захоўваць плэй-ліст відэа ў гэты каталог."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Логі запуску"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Каталог захоўвання логаў запуску."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Файлы захаванняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Каталог захоўвання ўнутрагульнявых захаванняў. Калі не зададзена, для стварэння захаванняў будзе выкарыстоўвацца працоўны каталог кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Ствараць усе файлы захаванняў (*.srm) у дадзеным каталогу, уключаючы злучаныя файлы .rt, .psrm і інш. Можа быць перавызначана аргументамі каманднага радка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Захаванні станаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Каталог захоўвання запісаў паўтораў і хуткіх захаванняў. Калі не зададзена, файлы будуць захоўвацца ў каталогу з кантэнтам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Кэш"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Каталог часовага захоўвання кантэнту, вынятага з архіваў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Логі сістэмных падзей"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Каталог захоўвання логаў сістэмных падзей."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Уключыць Rich Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Дзяліцеся вашым бягучым статусам RetroArch у Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Фармат кантэнту Rich Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Выбраць, якія звесткі пра змесціва будуць перададзеныя."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Змесціва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Назва ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Назва сістэмы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Змесціва (назва сістэмы)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Змесціва (назва ядра)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Змесціва (назва сістэмы - назва ядра)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Дадаць у мікшар"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Дадаць гукавую дарожку ў даступны аўдыё-струменевы слот.\nНе ўлічваецца, калі няма даступных слатоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Дадаць у мікшар і прайграць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Дадаць гукавую дарожку ў даступны аўдыё-струменевы слот і прайграць.\nНе ўлічваецца, калі няма даступных слатоў."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Хост"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Злучыцца з хастом сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Увесці адрас сервера сеткавай гульні і злучыцца ў рэжыме кліента."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Адлучыцца ад хаста сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Адлучыць актыўнае злучэнне сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Фільтраванне лобі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Толькі злучальныя пакоі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Толькі ўсталяваныя ядры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Пакоі пад паролем"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Абнавіць спіс сеткавых хастоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Пошук хастоў сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Абнавіць спіс лакальных хастоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Пошук лакальных хастоў сеткавай гульні."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Запусціць хост сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Запуск Netplay у рэжыме хаста (сервера)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Спыніць Netplay-хост"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "Прагнаць кліента"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Адключыць кліента ад бягучага пакоя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "Забаніць кліента"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Забаніць кліента ў бягучым пакоі."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Сканаваць каталог"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Сканіраванне каталога для пошуку сумяшчальнага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Сканаваць гэты каталог>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "Абярыце, каб запусціць пошук кантэнту ў дадзеным каталогу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Сканаваць файл"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Сканіраванне файла для пошуку сумяшчальнага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Ручное сканіраванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Налада сканавання на аснове імёнаў файлаў кантэнту. Не патрабуе супадзення з базамі даных."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "Сканаваць"
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Дадаць у мікшар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Дадаць у мікшар і прайграць"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Каталог змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Выбар каталога для пошуку кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Назва сістэмы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Пазначце 'назву сістэмы', з якой будзе звязаны сканаваны кантэнт. Выкарыстоўваецца ў якасці імя плэйліста і для ідэнтыфікацыі мініяцюр."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Дапасаваная назва сістэмы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Пазначце ўручную 'назву сістэмы' для сканаванага кантэнту. Ужываецца толькі калі для опцыі 'Назва сістэмы' абрана '<Ручны ўвод>'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Прадвызначанае ядро"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Выбраць і зрабіць прадвызначаным ядро для запуску знойдзенага змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Пашырэнні файлаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Тыпы файлаў для сканавання, запісаныя праз прабел. Калі не зададзена, уключае файлы ўсіх тыпаў або, калі паказана ядро, усе файлы, якія падтрымліваюцца ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Рэкурсіўнае сканаванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Калі ўключана, у сканаванне будуць дададзены ўсе падкаталогі ў абраным 'Каталогу кантэнту'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Сканаванне зместу архіваў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Калі ўключана, у пошук сумяшчальнага кантэнту будуць дададзены файлы архіваў (.zip, .7z і г.д.). Можа істотна ўплываць на хуткасць сканавання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Аркадны DAT-файл"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Загрузка спісу Logiqxc або MAME з XML DAT-файла для аўтаматычнага перайменавання знойдзенага аркаднага кантэнту (MAME, FinalBurn Neo і г. д.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Аркадны DAT-фільтр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Пры выкарыстанні аркаднага DAT-файла кантэнт будзе дададзены ў плэйліст толькі пры супадзенні з запісамі ў DAT-файле."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Перазапісаць існы плэй-ліст"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Пры ўключэнні налады існы плэйліст будзе выдалены перад сканаваннем кантэнту. Калі выключана, у плэйліст дадаюцца толькі новыя запісы, а існуючыя застаюцца без зменаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Спраўджанне існых запісаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Калі ўключана, запісы любога існага плэй-ліста будуць праверацца перад сканаваннем новага змесціва. Запісы, якія спасылаюцца на адсутнае змесціва, ды/або файлы з несапраўднымі пашырэннямі будуць выдаленыя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Пачаць сканаванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Сканаваць выбранае змесціва."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Стварэнне спісу..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Год выхаду"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Колькасць гульцоў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Рэгіён"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Цэтлік"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Пошук па назве ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Паказаць усе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Дадатковы фільтр"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Усе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Дадаць дадатковы фільтр"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "запісаў: %u"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Па распрацоўніку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Па выдаўцу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Па году выхаду"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Па колькасці гульцоў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Па жанру"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Па падтрымцы дасягненняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Па катэгорыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Па мове"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Па рэгіёну"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Па эксклюзіве кансолі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Па эксклюзіве платформы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Па падтрымцы вібрацыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Па балам"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Па носьбіту"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Па прыладам уводу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Па мастацкаму стылю"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Па гульнявому працэсу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Па наратыву"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "Па тэмпу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Па перспектыве"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Па сетынгу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Па візуалізацыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Па машынізацыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Па краіне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Па франшызе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Па цэтліку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Па назве сістэмы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Задаць фільтр дыяпазону"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Выгляд"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Захаваць як выгляд"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Выдаліць гэты выгляд"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Увесці назву новага выгляду"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Выгляд з такой назвай ужо існуе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "Выгляд быў захаваны"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "Выгляд быў выдалены"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Запусціць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Запусціць змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Змяніць назву"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Змяніць назву гэтага запісу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Прыбраць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Прыбраць гэты запіс з плэй-ліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Дадаць да ўпадабанага"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Дадаць змесціва да 'Упадабанае'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "Дадаць да плэй-ліста"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "Дадаць змесціва да плэй-ліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "Стварыць новы плэйлiст"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "Стварыць новы плэйліст і дадаць у яго бягучы запіс."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Задаць сувязь з ядром"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Задаць сувязь гэтага змесціва з ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Скінуць сувязь з ядром"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Скінуць сувязь гэтага змесціва з ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Звесткі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Прагляд пашыранай інфармацыі па кантэнце."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Сцягнуць мініяцюры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Сцягнуць мініяцюры здымка экрана/бокс-арта/тытульнага экрана бягучага змесціва. Абнаўляе ўсе наяўныя мініяцюры."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Бягучае ядро"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Назва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Шлях да файла"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Запіс: %lu/%lu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Ядро"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Час у гульні"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Апошні запуск"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Базы даных"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Працягнуць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Закрыць хуткае меню і аднавіць змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Перазапусціць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Перазапусціць загружаны кантэнт."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Закрыць змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Закрыць змесціва. Любыя незахаваныя змены могуць быць страчаныя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Стварыць здымак экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Зрабіць здымак экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Слот стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Змяніць бягучы слот захавання стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Захаваць стан"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Захаваць стан у бягучы абраны слот."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Загрузіць стан"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Загрузіць захаваны стан з бягучага абранага слота."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Адмяніць загрузку стану"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Вяртанне да стану кантэнту да моманту загрузкі захавання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Адмяніць захаванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Вяртанне да папярэдняга захавання, калі яно было перазапісана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "Слот паўтору"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "Змяніць бягучы слот захавання стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Прайгаць паўтор"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Прайграць файл паўтору з бягучага выбранага слота."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Запісаць паўтор"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Запісаць файл паўтору ў бягучы выбраны слот."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Спыніць запіс/паўтор"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "Спыняе запіс/прайграванне бягучага паўтору"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Дадаць да ўпадабанага"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Дадаць змесціва да 'Упадабанае'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Пачаць запіс"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Пачаць запіс відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Спыніць запіс"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Спыніць запіс відэа."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Пачаць трансляцыю"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Пачаць трансляцыю на мэтавым рэсурсе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Спыніць трансляцыю"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Завершыць стрым."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Захаванні станаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Доступ да опцый захаванняў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Опцыі ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Змена опцый для змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Кіраванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Змяніць кіраванне для змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Чыт-коды"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Наладзіць чыт-коды."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Кіраванне дыскамі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Кіраванне вобразамі дыскаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Шэйдары"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Настройкі шэйдараў для візуальнага паляпшэння малюнка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Перавызначэнні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Налады пераазначэнняў глабальнай канфігурацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Дасягненні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Прагляд дасягненняў і злучаных налад."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Кіраванне опцыямі ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Захаваць або адкінуць перавызначэнні опцый для бягучага змесціва."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Захаваць опцыі гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Захоўвае опцыі ядра, якія прымяняюцца толькі для бягучага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Адкінуць опцыі гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Выдаляе опцыі ядра, якія прымяняюцца толькі для бягучага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Захаваць опцыі каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Захоўвае опцыі ядра, якія прымяняюцца для ўсяго кантэнту ў каталогу з бягучым файлам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Адкінуць опцыі каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Выдаляе опцыі ядра, якія прымяняюцца для ўсяго кантэнту ў каталогу з бягучым файлам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Загружаны файл опцый"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "Бягучы файл опцый які выкарыстоўваецца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Скінуць опцыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Задаць усім опцыям ядра прадвызначаныя значэнні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Скінуць опцыі на дыск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Прымусова запісаць наладкі ў бягучы файл опцый. Забяспечвае захаванне опцый пры некарэктным зачыненні франтэнда з-за збою ядра."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Стварыць файл параметраў гульні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Захаваць файл параметраў гульні"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Кіраванне прывязкамі ўводу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Загрузка, захаванне або выдаленне прывязак уводу для бягучага кантэнту."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Загружаны файл прывязак"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "Бягучы файл прывязак які выкарыстоўваецца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Загрузіць прывязкі ўводу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Загрузка і замена бягучых прывязак уводу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Захаваць файл прывязак як"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Захаванне бягучых прывязак уводу ў новы файл."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Захаваць прывязкі ўводу для ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Захаванне файла прывязак, які будзе прымяняцца для ўсяго кантэнту, загружанага з бягучым ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Выдаліць файл прывязак ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Выдаленне файла прывязак, які прымяняецца для ўсяго кантэнту, загружанага з бягучым ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Захаваць прывязкі ўводу для кантэнту ў тэчцы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Захаванне файла прывязак, які будзе прымяняцца для ўсяго змесціва, загружанага з каталога з бягучым кантэнтам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Выдаліць файл прывязак каталога кантэнту"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Выдаленне файла прывязак, які ўжываецца для ўсяго змесціва, загружанага з каталога з бягучым кантэнтам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Захаваць прывязкі ўводу для гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "Захаванне файла прывязак, які будзе прымяняцца толькі для бягучага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Выдаліць файл прывязак гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "Выдаленне файла прывязак, які прымяняецца толькі для бягучага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Скінуць прывязкі ўводу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Зварот усіх прывязак уводу да стандартных значэнняў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Абнавіць файл прывязак уводу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Запісвае бягучыя налады прывязак уводу ў загружаны файл прывязак."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Файл раскладкі ўводу"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Пачаць або працягнуць пошук чыт-кода"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT,
   "Сканаваць памяць для стварэння новых чыт-кодаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Загрузіць файл з чыт-кодамі (замяніць)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Загрузіць файл з чыт-кодамі і замяніць актуальная чыты."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Загрузіць файл з чыт-кодамі (дадаць)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Загрузіць файл з чыт-кодамі і дадаць ды актуальных чытоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Перазагрузіць чыт-коды для гульні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Захаваць файл з чыт-кодамі як"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Захаваць бягучыя чыты ў файл з чыт-кодамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Дадаць чыт-код у пачатак спісу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Дадаць чыт-код у канец спісу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Выдаліць усе чыт-коды"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Аўтазагрузка чыт-кодаў пры запуску гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Аўтаматычна прымяняць чыт-коды пры запуску гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Ужываць пры пераключэнні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Ужываць чыт-коды адразу пасля ўключэння."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Ужыць змены"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Неадкладна прымяніць змены чыт-кодаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Чыт-код"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Пачаць або перазапусціць пошук чыт-кода"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Націсніце налева або направа, каб змяніць значэнне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Націсніце налева або направа, каб змяніць значэнне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Роўных %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Менш чым да"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Менш чым ці роўных да"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Больш чым да"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Больш чым ці роўных да"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Роўных да"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Ня роўных да"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Націсніце налева або направа, каб змяніць значэнне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Роўных да +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Пошук у памяці значэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Націсніце налева або направа, каб змяніць значэнне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Роўных да -%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Дадаць %u супадзенняў у спіс"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Выдаліць супадзенне #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Стварыць код супадзення #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Адрас супадзення: %08X Маска: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Файл з чыт-кодамі (замяніць)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Файл з чыт-кодамі (дадаць)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Дэталі чыт-кода"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Індэкс"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Нумар пазіцыі ў спісе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Уключана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Апісанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Апрацоўнік"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Памер значэння ў памяці"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Тып"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Значэнне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Адрас у памяці"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Прагляд адраса: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Адрасная маска памяці"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Адрас бітавай маскі, калі памер значэння ў памяці < 8-біт."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Колькасць ітэрацый"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Вызначае, колькі разоў будзе ўжыты чыт-код. Выкарыстоўвайце разам з іншымі параметрамі 'ітэрацый' для ахопу вялікіх абласцей памяці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Падвышаць адрас з кожнай ітэрацыяй"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "З кожнай ітэрацыяй 'Адрас у памяці' будзе падвышацца на 'Памер значэння ў памяці'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Падвышаць значэнне з кожнай ітэрацыяй"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Пасля кожнай ітэрацыі 'Значэнне' будзе павялічвацца на паказаны лік."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Уключаць аддачу, калі памяць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Значэнне аддачы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Порт аддачы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Сіла асноўнай аддачы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Працягласць асноўнай аддачы (мс)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Сіла другаснай аддачы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Працягласць другаснай аддачы (мс)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Код"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Дадаць новы чыт-код пасля бягучага"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Дадаць новы чыт-код перад бягучым"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Скапіяваць бягучы чыт-код пасля"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Скапіяваць бягучы чыт-код перад"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Выдаліць бягучы чыт-код"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Выняць дыск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Адкрыць віртуальны латок прывада і выняць бягучы дыск. Калі ўключаная опцыя 'Паўза пры выкліку меню', некаторым ядрам пасля любых аперацый з дыскамі патрабуецца на некалькі секунд аднавіць кантэнт для рэгістрацыі змен."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Уставіць дыск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Загрузіць дыск з бягучага азначніка і зачыніць латок віртуальнага прывада. Калі ўключаная опцыя 'Паўза пры выкліку меню', некаторым ядрам пасля любых аперацый з дыскамі патрабуецца на некалькі секунд аднавіць кантэнт для рэгістрацыі змен."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Загрузіць новы дыск"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Выміце бягучы дыск, абярыце з памяці іншы, устаўце і зачыніце латок прывада.\nГэтая функцыя з'яўляецца састарэлай. Гульні на некалькіх дысках рэкамендуецца загружаць праз M3U-плэйлісты, якія дазваляюць мяняць дыскі пры дапамозе опцый 'Выняць дыск' і 'Азначнік дыска'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Выбраць новы дыск з памяці і ўставіць яго без закрыцця латка.\nУВАГА: гэта функцыя састарэлая. Гульні на некалькіх дысках рэкамендуецца загружаць праз M3U-плэйлісты, якія дазваляюць мяняць дыскі пры дапамозе опцый 'Выняць дыск' і 'Азначнік дыска'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Індэкс бягучага дыска"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Абярыце бягучы дыск са спісу даступных вобразаў. Дыск будзе загружаны пры выбары 'Уставіць Дыск'."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Графічныя шэйдары"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Уключае канвеер графічных шэйдараў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Адсочваць змены файлаў шэйдара"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Аўтаматычна прымяняць змены, унесеныя ў файлы шэйдара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Адсочваць змены ў файлах шэйдара. Пасля захавання змен шэйдара на дыск ён будзе аўтаматычна перакампіляваны і ўжыты да змесціва."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Запамінаць шлях да каталога шэйдараў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Адкрываць браўзер файлаў у апошнім выкарыстаным каталогу пры загрузцы прасэтаў і праходаў шэйдараў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Загрузіць набор налад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Загрузка перадусталёўкі шэйдара. Пайплайн шэйдара будзе настроены аўтаматычна."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Неадкладная загрузка прасэту шэйдара з абнаўленнем меню.\nЯкі адлюстроўваецца ў меню каэфіцыент маштабавання сапраўдны толькі ў тым выпадку, калі ў прасэце выкарыстоўваюцца простыя метады маштабавання (напрыклад маштабаванне крыніцы, роўныя каэфіцыенты маштабавання дл[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Дадаць набор налад спераду"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Дадаць набор налад перад бягучымі загружанымі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Дадаць набор налад ззаду"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Дадаць набор налад па-за бягучымі загружанымі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
   "Захаваць набор налад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "Захаваць бягучы набор налад шэйдара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
   "Прыбраць набор налад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "Прыбраць аўтаматычны набор налад шэйдара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Ужыць змены"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Неадкладнае ўжыванне змен канфігурацыі шэйдара. Выкарыстоўвайце пры змене колькасці праходаў шэйдара, рэжыму фільтрацыі, маштабу FBO і г. д."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "Опцыя выкарыстоўваецца пры змене колькасці праходаў, метадаў фільтрацыі і маштаба FBO.\nЗмяняйце дадзеныя налады шэйдара ўважліва, таму што гэта можа паўплываць на прадукцыйнасць.\nПасля ўжывання шэйдараў налады захоўваюцца ў часовы файл (retroarch.slang/.cgp/.glslp) і загружаюцца. Фай[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Параметры шэйдара"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Налада загружанага шэйдара. Змены не будуць захаваны ў файл прэсета."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Праходы шэйдара"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Павялічыць або паменшыць колькасць праходаў у пайплайне шэйдара. Да кожнага праходу пайплайна можа быць прывязаны свой шэйдар, з наладай маштабу і рэжыму фільтрацыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch дазваляе змешваць і супастаўляць розныя шэйдары з адвольным лікам праходаў, апаратнымі фільтрамі і каэфіцыентамі маштабавання.\nДадзеная налада вызначае колькасць выкарыстаных праходаў шэйдара. Пры ўсталёўцы значэння на 0 і ўжыванні змен будзе выкарыстоўвацца 'пус[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "Шэйдар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "Шлях да шэйдара. Усе шэйдары павінны быць аднаго тыпу (напр. Cg, GLSL ці Slang). Пазначце ў наладах каталог шэйдараў, каб змяніць тэчку, з якой браўзер будзе пачынаць агляд шэйдараў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Фільтр"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "Апаратны фільтр для дадзенага праходу. Калі выбрана 'Па змаўчанні', будзе выкарыстоўвацца 'Лінейны' або 'Найбліжэйшы' фільтр, у залежнасці ад усталёўкі 'Білінейнае фільтраванне' у наладах малюнка."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Маштаб"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "Маштаб для бягучага праходу. Множнікі маштабавання складаюцца, напрыклад 2x для першага праходу і 2x для другога ў суме даюць множнік 4x.\nПры наяўнасці множніка ў апошнім праходзе выніковая выява расцягваецца на ўвесь экран з фільтрам па змаўчанні, зыходзячы са значэння 'Біл[...]"
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Простыя наборы налад"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Захоўваць прасэты шэйдараў у выглядзе спасылкі на арыгінальны прасэт з наборам параметраў, змененых карыстальнікам."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Захаваць набор налад шэйдара як"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Захаваць бягучыя налады шэйдара ў новы прасэт."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Захаваць глабальны набор налад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Захаваць бягучыя наладкі шэйдара ў якасці глабальнага прасэту па змаўчанні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Захаваць набор налад для ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Захоўвае бягучыя налады шэйдара ў якасці прасэта па змаўчанні для загружанага ядра."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Захаваць набор налад для каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Захаваць бягучыя наладкі шэйдара ў якасці стандартных для ўсіх файлаў у тэчцы кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Захаваць набор налад для гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Захаваць бягучыя наладкі шэйдара ў якасці стандартных для загружанага кантэнту."
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Аўтаматычныя прасэты шэйдараў не знойдзены"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Прыбраць глабальны набор налад"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Прыбраць глабальны набор налад, які выкарыстоўваецца ўсім змесцівам і ўсімі ядрамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Прыбраць набор налад для ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Прыбраць набор налад для ядра, які выкарыстоўваецца ўсім змесцівам, што запускаецца з бягучым загружаным ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Прыбраць набор налад для каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Прыбраць набор налад для каталога змесціва, які выкарыстоўваецца ўсім змесцівам унутры бягучага рабочага каталога."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Прыбраць набор налад для гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Прыбраць набор налад для гульні, які выкарыстоўваецца толькі для канкрэтнай гульні."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Няма параметраў шэйдара"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "Загружаны файл пераазначэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "Бягучы файл пераазначэнняў які выкарыстоўваецца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "Загрузіць файл перавызначэнняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "Загружае і замяшчае бягучую канфігурацыю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "Захаваць перавызначэнні як"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "Захаванне бягучай канфігурацыі ў новы файл пераазначэнняў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Захаваць перавызначэнні ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Стварыць файл пераазначэння налад, які будзе прымяняцца да ўсяго кантэнту, які загружаецца з бягучым ядром. Мае прыярытэт над асноўнай канфігурацыяй."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Адкінуць перавызначэнні ядра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Выдаляе перавызначэнне канфігурацыі, якое прымяняецца для ўсяго кантэнту, загружанага з бягучым ядром."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Захаваць перавызначэнні каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Стварыць файл пераазначэння налад, які будзе прымяняцца для ўсяго каталога з якога быў загружаны бягучы кантэнт. Мае прыярытэт над асноўнай канфігурацыяй."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Адкінуць перавызначэнні каталога змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Выдаляе перавызначэнне канфігурацыі, якое прымяняецца для ўсяго кантэнту ў каталогу з бягучым файлам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Захаваць перавызначэнні гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Стварыць файл пераазначэння налад, які будзе прымяняцца толькі да бягучага кантэнту. Мае прыярытэт над асноўнай канфігурацыяй."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Адкінуць перавызначэнні гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Выдаляе перавызначэнне канфігурацыі, якое прымяняецца толькі для бягучага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "Выгрузіць перавызначэнне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "Скінуць усе опцыі да значэнняў глабальнай канфігурацыі."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Няма даступных дасягненняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Адмена паўзы хардкорнага рэжыму дасягненняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Пакінуць хардкорны рэжым дасягненняў уключаным для бягучага сеансу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Адмена працягу хардкорнага рэжыму дасягненняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Пакінуць хардкорны рэжым дасягненняў адключаным для бягучага сеансу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Паўза хардкорнага рэжыму дасягненняў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Прыпыняе хардкор рэжым дасягненняў у бягучым сеансе. Гэтае дзеянне актывуе чыт-коды, перамотку, запавольваны рух ды загрузку станаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Працягнуць дасягненні ў рэжыме хардкора"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Аднаўляе дасягненні ў хардкор рэжыме на бягучы сеанс. Гэтае дзеянне адключае чыт-коды, перамотку назад, запавольванне, загрузку захаванняў стану і перазапускае гульню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Сервер RetroAchievements недаступны"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Адно ці некалькі раскрытых дасягненняў не падхапіліся серверам. Спробы будуць паўтарацца пакуль праграма будзе заставацца адкрытай."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "Сервер RetroAchievements недаступны. Спробы будуць працягвацца да поспеху або да выхаду з праграмы."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "Усе неапрацаваныя запыты паспяхова сінхранізаваныя з серверам RetroAchievements."
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "Ідэнтыфікацыя гульні"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "Атрыманне даных гульні"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "Запуск сеанса"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Уваход не выкананы"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Памылка сеціву"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Невядомая гульня"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Дасягненні не могуць быць актываваны з дадзеным ядром"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "Хэш RetroAchievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Запіс базы даных"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Прагляд звестак базы даных па бягучаму змесціву."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Няма запісаў для адлюстравання"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Няма даступных ядраў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Няма даступных опцый ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Звесткі пра ядро адсутнічаюць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Няма даступных рэзервовых копій ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Упадабанае адсутнічае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Гісторыя адсутнічае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Відарысы адсутнічаюць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Музыка адсутнічае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Відэа адсутнічае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Звесткі адсутнічаюць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Запісы плэй-ліста адсутнічаюць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Налады не знойдзеныя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Прылады Bluetooth не знойдзеныя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Сеткі не знойдзены"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Без ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Пошук"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "Пераключыць эскізы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RANDOM_SELECT,
   "Выпадковы выбар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Назад"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "Згода"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Бацькоўскі каталог"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "Вярнуцца да бацькоўскага каталога."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Каталог не знойдзены"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Няма элементаў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Выбраць файл"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_NORMAL,
   "Звычайная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90°"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180°"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270°"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_NORMAL,
   "Звычайная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90°"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180°"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270°"
   )

/* Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_95_PLUS,
   ">95 %"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_75_PLUS,
   "75-95 %"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_50_PLUS,
   "50-74 %"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_25_PLUS,
   "25-49 %"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LNG_COMPLETION_25_MINUS,
   "<25 %"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Невядомы кампілятар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Абагульненне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Дужанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Галасаванне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Перадача аналагавага ўводу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Максімум"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Сярэдняе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Няма"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Без пераваг"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Дрэйф налева/направа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Пракруціць улева"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Рэжым выявы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Рэжым маўлення"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Рэжым дыктара"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Гісторыя ды ўпадабанае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Усе плэй-лісты"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Гісторыя ды ўпадабанае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Заўсёды"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Ніколі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Па ядры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Агульная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Зараджана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Зараджаецца"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Разраджаецца"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "Няма крыніцы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Выкарыстоўваць гэты каталог>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "Абярыце для ўстаноўкі ў якасці каталога."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Каталог змесціва>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Па змаўчанні>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Няма>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad з аналагавым уводам"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Няма"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Невядомы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Уніз + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Утрымліваць Start (2 секунды)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Утрымліваць Select (2 секунды)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Уніз + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Адключана>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Змяняе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Не змяняе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Павялічвае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Памяншае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= значэнню груку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= значэнню груку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< значэння груку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> значэння груку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Павялічвае на значэнне груку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Памяншае на значэнне груку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Усе"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Адключана>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Задаць значэнне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Павялічыць на значэнне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Паменшыць на значэнне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Ужываць наступны чыт-код калі значэнне = памяці"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Ужываць наступны чыт-код калі значэнне != памяці"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Ужываць наступны чыт-код калі значэнне < памяці"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Ужываць наступны чыт-код калі значэнне > памяці"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Эмулятар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-біт, максімальнае значэнне = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-біт, максімальнае значэнне = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-біт, максімальнае значэнне = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-біт, максімальнае значэнне = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-біт, максімальнае значэнне = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-біт, максімальнае значэнне = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Прадвызначаны сістэмай"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Па алфавіце"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Няма"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Паказваць поўныя пазнакі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Прыбраць ()"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Прыбраць []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Прыбраць () ды []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Пакінуць рэгіён"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Пакінуць нумар дыска"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Пакінуць рэгіён і нумар дыска"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Прадвызначаны сістэмай"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Бокс-арт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Здымак экрана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Тытульны экран"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "Лагатып змесціва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "Звычайна"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Хутка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "УКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Так"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Не"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Так"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Не"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Уключана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Адключана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "Н/Д"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Нераскрытае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Раскрытыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
   "Хардкор"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Неафіцыйнае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Не падтрымліваецца"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Нядаўна раскрытыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Амаль гатова"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Актыўныя выпрабаванні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Толькі трэкеры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Толькі апавяшчэнні"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Прадвызначана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Лінейны"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Бліжэйшы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Галоўнае"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Змесціва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Каталог змесціва>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Ручны ўвод>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Нявызначанае>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Левы аналагавы стык"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Правы аналагавы стык"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Левы аналагавы стык (прымусова)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Правы аналагавы стык (прымусова)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "Клавіша %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Кнопка мышы 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Кнопка мышы 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Кнопка мышы 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Кнопка мышы 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Кнопка мышы 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Колца мышы ўверх"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Колца мышы ўніз"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Колца мышы ўлева"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Колца мышы ўправа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Ранні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "Звычайна"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Позні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "ГГГГ-ММ-ДД ГГ:ХХ:СС"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "ГГГГ-ММ-ДД ГГ:ХХ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "ГГГГ-ММ-ДД"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "ГГГГ-ММ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "ММ-ДД-ГГГГ ГГ:ХХ:СС"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "ММ-ДД-ГГГГ ГГ:ХХ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "ММ-ДД ГГ:ХХ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "ММ-ДД-ГГГГ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD,
   "ММ-ДД"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "ДД-ММ-ГГГГ ГГ:ХХ:СС"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "ДД-ММ-ГГГГ ГГ:ХХ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "ДД-ММ ГГ:ХХ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "ДД-ММ-ГГГГ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM,
   "ДД-ММ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS,
   "ГГ:ХХ:СС"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM,
   "ГГ:ХХ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "ГГГГ-ММ-ДД ГГ:ХХ:СС (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "ГГГГ-ММ-ДД ГГ:ХХ (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "ММ-ДД-ГГГГ ГГ:ХХ:СС (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "ММ-ДД-ГГГГ ГГ:ХХ (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "ММ-ДД ГГ:ХХ (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "ДД-ММ-ГГГГ ГГ:ХХ:СС (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "ДД-ММ-ГГГГ ГГ:ХХ (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "ДД-ММ ГГ:ХХ (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM,
   "ГГ:ХХ:СС (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM,
   "ГГ:ХХ (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "Таму"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Шчыльнасць фону"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Павялічвае зярністасць клятчастага фону меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Арнамент меню"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Шчыльнасць арнаменту"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Павялічвае зярністасць клятчастага арнаменту меню."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Адлюстроўваць арнамент меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Макет па шырыні акна"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Змяняць памер і становішча элементаў меню дзеля аптымальнага выкарыстання месца. Адключыце, каб выкарыстоўваць стандартны макет у два слупка фіксаванай шырыні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Лінейны фільтр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Дадае невялікае размыццё ў меню для згладжвання пікселяў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Унутраны апскейлінг"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Маштабаваць інтэрфейс меню да вывадана экран. Пры выкарыстанні з опцыяй 'Лінейнае фільтраванне меню' прадухіляе артэфакты маштабавання (няроўныя пікселі), захоўвае выразнасць малюнка. Павелічэнне параметру істотна ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Суадносіны бакоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Выбар суадносін бакоў меню. Шырокаэкранныя фарматы павялічваюць гарызантальнае раздзяленне інтэрфейсу. (Можа спатрэбіцца перазагрузка, калі ўключана 'Забарона змены фармату меню')"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Зафіксаваць прапорцыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Забяспечвае адлюстраванне меню з правільнымі суадносінамі бакоў. Калі выключана, хуткае меню будзе расцягнута ў адпаведнасці з загружаным кантэнтам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Колеравая тэма"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Выбар каляровай тэмы. Значэнне 'Карыстальніцкі' дазваляе загружаць прэсэты афармлення меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Кастомны шаблон меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Выбар тэмы меню ў браўзеры файлаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Празрыстасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Адлюстроўваць у фоне змесціва пры выкліку хуткага меню. Адключэнне празрыстасці можа змяніць колеры тэмы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Эфекты ценю"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Уключае адкідванне ценяў для тэксту, рамак і мініяцюр. Умерана ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Анімацыя фону"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Уключыць эфект анімаваных часціц для фону. Істотна ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Хуткасць анімацыі фону"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Настройка хуткасці анімацыі эфекту часціц."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Фонавая анімацыя застаўкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Паказваць аніміраваны эфект часціц пры запуску застаўкі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Паказваць мініяцюры плэй-лістоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Уключае адлюстраванне сціснутых эскізаў пры праглядзе плэйлістоў. Пераключэнне ажыццяўляецца кнопкай RetroPad Select. Калі выкл., прагляд мініяцюр усё роўна даступны кнопкай RetroPad Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Верхняя мініяцюра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Тып мініяцюры, якая адлюстроўваецца ў правай верхняй частцы плэйліста. Для цыклічнага пераключэння мініяцюры націсніце RetroPad Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Ніжняя мініяцюра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Тып мініяцюры, якая адлюстроўваецца ў правым ніжнім куце плэйліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Памяняць месцамі эскізы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Змяняе месцамі становішча верхняй і ніжняй мініяцюр."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Метад сціску мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Метад рэсэмплінга, які выкарыстоўваецца для падганяння буйных мініяцюр пад памеры экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Затрымка мініяцюр (мс)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Затрымка часу паміж вылучэннем запісу плэйліста і падгрузкай прывязаных мініяцюр. Значэнні 256 мс і больш забяспечваюць плыўную пракрутку нават на самых слабых прыладах."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Пашыраная падтрымка ASCII"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Уключыць адлюстраванне нестандартных ASCII-знакаў. Неабходна для сумяшчальнасці з некаторымі заходнімі мовамі. Умерана ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Значкі пераключальніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Выкарыстоўваць значок замест ВКЛ/ВЫКЛ для абазначэння пераключальніка налад у меню."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Бліжэйшага суседа (хуткі)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "Білінейны"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (павольны)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Няма"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "Аўта"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (цэнтравана)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (цэнтравана)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (цэнтравана)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (цэнтравана)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (цэнтравана)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "Аўта"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Упісаць у экран"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Цэлалікавы маштаб"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Запаўняць экран (расцягваць)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Уласная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Чырвоная класічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Аранжавая класічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Жоўтая класічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Зялёная класічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Сіняя класічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Фіялетавая класічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Шэрая класічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Чырвоная старая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Цёмна-фіялетавы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Паўночна-сіняя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Залацістая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Светла-сіняя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Зялёны яблык"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Чырвоная вулканічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Лагуна"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Цёмная Gruvbox"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Светлая Gruvbox"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
   "One цёмная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized цёмная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized светлая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Цёмная Tango"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Светлая Tango"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Дынамічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Цёмна-шэрая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Светла-шэрая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Снег (невялікі)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Снег (моцны)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Дождж"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "Вір"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Зорнае поле"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Другарадная мініяцюра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Тып мініяцюры для адлюстравання злева."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "Значок мініяцюры"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "Тып значка мініяцюры, які адлюстроўваецца ў плэй-лісце."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Дынамічны фон"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Дынамічна загружаць новы малюнак фону ў залежнасці ад кантэксту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Гарызантальная анімацыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Уключае анімацыю гарызантальных пераходаў у меню. Падвышае нагрузку на прыладу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Гарызантальная анімацыя выбару абразкоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Эфект анімацыі пры прагортцы картак."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Анімацыя перамяшчэння уверх/уніз"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Эфект анімацыі пры руху ўверх ці ўніз."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Анімацыя ўваходу/выхаду ў галоўнае меню"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Эфект анімацыі пры пераходзе ў падменю."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Празрыстасць каляровай тэмы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Шрыфт"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Выбраць асноўны шрыфт для меню інтэрфейсу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Колер шрыфта (чырвоны)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Колер шрыфта (зялёны)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Колер шрыфта (сіні)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Макет"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Выбар іншага макета для інтэрфейсу XMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Тэма значкоў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Выбар іншай тэмы абразкоў для RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Значкі пераключальніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "Выкарыстоўваць значок замест ВКЛ/ВЫКЛ для абазначэння пераключальніка налад у меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Эфекты ценю"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Уключыць адкідванне ценяў для значкоў, мініяцюр і літар. Нязначна ўплывае на прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Пайплайн шэйдара"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Выбар эфекту анімацыі фону. У залежнасці ад налады можа істотна нагружаць GPU. У выпадку нізкай прадукцыйнасці адключыце або абярыце менш складаны эфект."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Колеравая тэма"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Выбар каляровай тэмы задняга фону."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Вертыкальнае размяшчэнне мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Адлюстроўваць левую мініяцюру пад правай, у правай частцы экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Каэфіцыент маштабавання мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Памяншае памер мініяцюр настройкай максімальнага значэння шырыні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Велічыня вертыкальнага згасання"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Назва ў загалоўку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Водступ загалоўка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Зрух загалоўка па гарызанталі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Уключыць укладку 'Налады' (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Паказаць укладку 'Налады', якая змяшчае наладкі праграмы."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Жычка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Стужка (простая)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Просты снег"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Снег"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
   "Баке"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Сняжынкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Уласная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Манахромная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Інвертаваная манахромная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Сістэматычная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Аўтаматычная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Інвертаваная аўтаматычная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Зялёны яблык"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Цёмная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Светлая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Ранішняя сінь"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Сонечны прамень"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Цёмна-фіялетавая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Светла-сіняя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Залацістая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Чырвоная старая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Паўночна-сіняя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Фонавы відарыс"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Марская глыбіня"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Чырвоная вулканічная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Зялёны лайм"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Фіялетавая GameCube"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Чырвоная Famicom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Палымяна-гарачая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Халодна-лёдавая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDGAR,
   "Мідгар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Цёмна-шэрая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Светла-шэрая"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Згортваць бакавую панэль"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Заўсёды згортваць бакавую панэль злева."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Скарачаць назвы плэй-лістоў (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Не паказваць назвы кампаній у плэйлістах. Напрыклад, 'Sony - PlayStation' будзе пераўтворана ў 'PlayStation'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Сартаваць плэй-лісты пасля скарачэння назваў (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Плэйлісты будуць перасартаваны ў алфавітным парадку пасля выдалення назваў кампаній."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Другарадная мініяцюра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Замяняе вобласць метададзеных дадатковай мініяцюрай."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Скролінг метададзеных кантэнту"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Калі ўключана, метададзеныя ў правай частцы плэйліста (прывязка ядра, час працы) займаюць адзін радок; запісы больш шырыні бакавой панэлі будуць адлюстраваны бягучым радком. Калі выключана, усе метададзеныя будуць адлюстраваны статычна, займаючы неабходную колькасць рад[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Каэфіцыент маштабавання мініяцюр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Змяняе памер вобласці мініяцюр."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Колеравая тэма"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Выбраць іншую колеравую тэму."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Базавая белая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Базавая чорная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Цёмная Gruvbox"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "Змрочная зона"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solarized цёмная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized светлая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Цёмна-шэрая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Светла-шэрая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SELENIUM,
   "Сэлен"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Значкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Адлюстроўваць значкі злева ад элементаў меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Значкі пераключальніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "Выкарыстоўваць значок замест ВКЛ/ВЫКЛ для абазначэння пераключальніка налад у меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Значкі плэй-лістоў (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Паказваць значкі сістэм для плэйлістоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Аптымізаваць альбомны макет"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Аўтаматычна падладжваць макет меню для аптымальнага адлюстравання ў альбомнай арыентацыі экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Паказваць панэль навігацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Уключае адлюстраванне панэлі з ярлыкамі навігацыі. Дазваляе хутка перамыкацца паміж укладкамі меню. Рэкамендавана для прылад з сэнсарным экранам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Аўтапаварот панэлі навігацыі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Аўтаматычна перамяшчаць панэль навігацыі направа пры выкарыстанні ландшафтнай арыентацыі экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Колеравая тэма"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Выбар каляровай тэмы задняга фону."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Анімацыя пераходаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Уключае эфект плыўнай анімацыі пры пераходзе паміж рознымі ўзроўнямі меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Тып эскізаў у партрэтным рэжыме"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Выбар адлюстравання эскізаў плэйліста ў партрэтнай арыентацыі экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Тып эскізаў у альбомным рэжыме"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Выбар адлюстравання эскізаў плэйліста ў альбомнай арыентацыі экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Дадатковы эскіз у рэжыме спісу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Уключае дадатковы эскіз пры праглядзе плэйліста ў рэжыме 'Спіс'. Настройка прымяняецца толькі калі шырыні экрана дастаткова для адлюстравання двух эскізаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Дадаваць фон да эскізаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Уключае запаўненне вольнай прасторы вакол мініяцюр суцэльным фонам. Забяспечвае адзіную памернасць для ўсіх малюнкаў, паляпшаючы адлюстраванне меню пры праглядзе эскізаў рознага памеру."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Першасная мініяцяюра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Асноўны тып эскіза, які адлюстроўваецца для запісаў у плэйлістах. Выкарыстоўваецца ў якасці значкоў кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Другарадная мініяцюра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Уключае дапаможны эскіз для запісаў у плэйлістах. Выкарыстанне залежыць ад абранага рэжыму адлюстравання мініяцюр у плэйлістах."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Блакітная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Шэра-блакітная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Цёмна-блакітная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Зялёная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "NVIDIA Shield"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Чырвоная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Жоўтая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Цёмная Material UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Цёмная Ozone"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Цёмная Gruvbox"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized цёмная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Прыемная блакітная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Прыемная бірузовая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Прыемная зялёная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Прыемная аранжавая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Прыемная ружовая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Прыемная пурпурная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Прыемная чырвоная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Цёмна-шэрая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Светла-шэрая"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "Аўта"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Згасанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Слізгаценне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Спіс (невялікі)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Спіс (сярэдні)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Падвойны эскіз"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Спіс (невялікі)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Спіс (сярэдні)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Спіс (вялікі)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Працоўны стол"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "ВЫКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "УКЛ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Выключаючы плэйлісты"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Звесткі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Файл"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Загрузіць ядро..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Выгрузіць ядро"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "Вы&хад"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Рэдагаваць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Пошук"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Выгляд"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Закрытыя панэлі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Параметры шэйдара"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Налады..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Запамінаць месца стыкоўкі:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Запамінаць памеры акна:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Запамінаць апошнюю адкрытую ўкладку браўзера кантэнту:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Тэма:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Па змаўчанні>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Цёмная"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Уласная..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Налады"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Інструменты"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Даведка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "Пра RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Дакументацыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Загрузіць знешняе ядро..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Загрузка ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Загрузка ядра..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Назва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Версія"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Плэй-лісты"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Файлавы браўзер"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Верх"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Угару"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Агляд змесціва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Бокс-арт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Здымак экрана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Тытульны экран"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO,
   "Лагатып"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Усе плэй-лісты"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Ядро"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Звесткі ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Запытваць>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Звесткі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Папярэджанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Памылка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Памылка сеціву"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Калі ласка, перазапусціце праграму для прымянення змен."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Лог"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 аб’ектаў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Перацягніце відарыс сюды"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Больш не паказваць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Спыніць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Звязаць з ядром"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Схаваныя плэй-лісты"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Схаваць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Колер вылучэння:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Выбар..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Выбраць колер"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Выбраць тэму"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Карыстальніцкая тэма"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Шлях да файла пусты."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Файл пусты."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Немагчыма адкрыць файл для чытання."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Немагчыма адкрыць файл на запіс."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Файл не існуе."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Прапаноўваць першым загружанае ядро:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Маштаб"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Выгляд"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Значкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Спіс"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Ачысціць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Ход выканання:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "Максімум запісаў у спісе \"Усе плэйлісты\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "Максімум запісаў у сетцы \"Усе плэйлісты\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Паказваць схаваныя файлы і папкі:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Новы плэй-ліст"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Калі ласка, увядзіце імя новага плэйліста:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Выдаліць плэй-ліст"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Змяніць назву плэй-ліста"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Вы ўпэўненыя, што жадаеце выдаліць плэйліст \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Пытанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Немагчыма выдаліць файл."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Немагчыма змяніць назву файла."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Атрыманне спісу файлаў..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Дадаванне файлаў да плэй-ліста..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Запіс плэй-ліста"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Назва:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Шлях:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Ядро:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Базы даных:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Пашырэнні:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(праз прагал; па змаўчанні ўключаны ўсе)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Фільтраваць змесціва архіваў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(выкарыстоўваецца для пошуку мініяцюр)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Вы ўпэўнены, што жадаеце выдаліць %1?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Калі ласка, спачатку абярыце адзін плэйліст."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Выдаліць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Дадаць запіс..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Дадаць файл(ы)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Дадаць тэчку..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Рэдагаваць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Выбраць файлы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Выбраць тэчку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<некалькі>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Памылка абнаўлення запісу плэй-ліста."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Калі ласка, запоўніце ўсе патрэбныя палі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "Абнавіць RetroArch (начная зборка)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch паспяхова абноўлены. Калі ласка, перазапусціце праграму для прымянення змен."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Не атрымалася абнавіць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Удзельнікі праекта"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Бягучы шэйдар"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Перамясціць ніжэй"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Перамясціць вышэй"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Загрузіць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Захаваць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Прыбраць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Выдаліць праходы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Ужыць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Дадаць праход"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Выдаліць усе праходы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Няма праходаў шэйдара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Скінуць праход"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Скінуць усе праходы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Скінуць параметр"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Сцягнуць мініяцюры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Сцягванне ўжо выконваецца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Пачатковы плэйліст:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Мініяцюра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Памер кэша мініяцюр:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Ліміт памеру Drag-n-Drop-мініяцюр:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Сцягнуць усе мініяцюры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Для сістэмы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Для плэйліста"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Мініяцюры паспяхова сцягнутыя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Паспяхова: %1 Не атрымалася: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Опцыі ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Скінуць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Скінуць усё"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Налады абнаўленняў ядраў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Акаўнт Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Канчатковая кропка спісу акаўнтаў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Турба/Мёртвая зона"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Лічыльнікі ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Дыск не выбраны"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Лічыльнікі франтэнда"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Гарызантальнае меню"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Хаваць непрывязаныя дэскрыптары ўводу ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Паказваць подпісы дэскрыптараў уводу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Накладка на экране"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Гісторыя"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Выбраць змесціва з плэй-ліста апошняй гісторыі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Пасля загрузкі кантэнту камбінацыя кантэнту і ядра заносіцца ў гісторыю.\nГісторыя захоўваецца ў файл у адным каталогу з файлам канфігурацыі RetroArch. Калі пры запуску няма загружаных канфігурацый, гісторыя не захоўваецца/загружаецца і будзе недаступная з галоўнага меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "Мультымедыя"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Падсістэмы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Доступ да налад падсістэмы для бягучага кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Бягучае змесціва: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Сеткавыя хасты не знойдзены."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Сеткавыя кліенты не знойдзены."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Няма лічыльнікаў прадукцыйнасці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Няма плэй-лістоў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Злучаны"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "Анлайн"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "Порт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Порт %d назва прылады: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Імя прылады, якое адлюстроўваецца: %s\nФайл канфігурацыі прылады: %s\nVID/PID прылады: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Налады Чытоў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Пачаць або працягнуць пошук чыт-кода"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Прайграць з дапамогай медыяпрайгравальніка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "секунд"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Запусціць ядро"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Запуск ядра без кантэнту."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Прапанаваныя ядры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Не атрымалася прачытаць сціснуты файл."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Карыстальнік"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "Клавіятура"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Максімум малюнкаў у свопчэйне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Задае дакладны рэжым буферызацыі, які выкарыстоўваецца відэадрайверам."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Максімум малюнкаў у свопчэйне. Паказвае відэадрайверу патрэбны рэжым буферызацыі.\nАдзінарная буферызацыя - 1\nПадвойная буферызацыя - 2\nТрайная буферызацыя - 3\nВыбар правільнага рэжыму буферызацыі можа істотна паўплываць на затрымку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "Свопчэйны з чаканнем"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Жорстка сінхранізаваць цэнтральны працэсар з графічным. Скарачае латэнтнасць за кошт прадукцыйнасці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Максімальная затрымка кадра"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Задае дакладны рэжым буферызацыі, які выкарыстоўваецца відэадрайверам."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Змяняе параметры бягучага прасэту шэйдара."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Набор налад шэйдара"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Набор налад шэйдара"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "Набор налад шэйдара"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Прагляд URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "Шлях URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Старт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Мянушка: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Пошук сумяшчальнага кантэнту..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "Ядра не знойдзены"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "Плэй-лісты не знойдзены"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Выяўлены сумяшчальны кантэнт"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "Не атрымалася выявіць кантэнт, прыдатны па CRC або імя файла"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Запуск Gong"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "Аўтасуадносіны бакоў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "Мянушка (LAN): %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "Стан"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Фонавы гук сістэмы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
   "Ручная ўстаноўка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Падтрымка запісу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "Захаваць запіс як..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "Захаваць запісы ў выходным каталогу"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "Прагляд супадзення #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "Выбар супадзення для прагляду."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "Фарсіраваць прапорцыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "Выбраць з плэйліста"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "Прагляд спісу %u супадзенняў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "Стварыць код з супадзення"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "Выдаліць супадзенне"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "Бачнасць ніжняй паласы"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "Налада празрыстасці ніжняй паласы."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "Непразрыстасць загалоўка"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "Налада празрыстасці паласы загалоўка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Сеткавая гульня"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "Запусціць змесціва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Шлях да гісторыі змесціва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Дысплей для вываду"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Выбраць порт вываду, што падлучаны да дысплэя ЭПТ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Даведка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "Ачысціць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Вырашэнне праблем аўдыя/відэа"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "Змена аверлэя экраннага геймпада"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "Загрузка змесціва"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "Пошук кантэнту"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "Што ёсць ядро?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "Адпраўка адладачных звестак"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
   "Адпраўляе інфармацыю аб прыладзе і канфігурацыі RetroArch на нашы серверы для аналізу."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Налады базы даных"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Затрымка кадраў сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Сканаваць лакальнае сеціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Пошук і злучэнне з лакальнымі хастамі сеткавай гульні."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Кліент сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Назіральнік сеткавай гульні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Апісанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "Ліміт максімальнай хуткасці працы"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "Пачаць пошук новага чыт-кода"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "Пачаць пошук новага чыт-кода. Колькасць бітаў можа быць змененая."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Працягнуць пошук"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Працягнуць пошук новага чыт-кода."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Дасягненні (хардкор)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Дэталі чыт-кода"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Кіруе падрабязнымі наладамі чыт-кода."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Пачаць або працягнуць пошук чыт-кода"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Пачаць або працягнуць пошук чыт-кода."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Лік чыт-кодаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Павялічыць або паменшыць колькасць чыт-кодаў."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Левы аналагавы стык - вось X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Левы аналагавы стык - вось Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Правы аналагавы стык - вось X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Правы аналагавы стык - вось Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Пачаць або працягнуць пошук чыт-кода"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Спіс базы дадзеных курсораў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "База дадзеных - фільтр: распрацоўнік"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "База дадзеных - фільтр: выдавец"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "База дадзеных - фільтр: краіна"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "База дадзеных - фільтр: франшыза"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "База дадзеных - фільтр: рэйтынг ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "База дадзеных - фільтр: рэйтынг ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "База дадзеных - фільтр: рэйтынг PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "База дадзеных - фільтр: рэйтынг CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "База даных - фільтр: рэйтынг BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "База даных - фільтр: максімум карыстальнікаў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "База даных - фільтр: па месяцу выхаду"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "База даных - фільтр: па году выхаду"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "База даных - фільтр: нумар часопіса Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "База даных - фільтр: рэйтынг часопіса Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Звесткі з базы даных"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Канфігурацыя"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Спампоўкі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Налады Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Падтрымка Slang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "Падтрымка рэндэрынгу ў тэкстуру для OpenGL/Direct3D (сумяшчэнне шэйдараў)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Змесціва"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "Шлях да рэсурсаў звычайна ўсталёўваецца распрацоўнікамі дадаткаў libretro/RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Запытваць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Базавыя элементы кіравання меню"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Пацвердзіць/OK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "Звесткі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "Выхад"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Прагортка ўгору"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Прадвызначана"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Выклік клавіятуры"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Выклік меню"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "У меню"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "У гульні"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "У гульні (паўза)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Гуляе ў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Прыпынена"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netplay запусціцца пасля загрузкі кантэнту."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "Перад запускам сеткавай гульні неабходна загрузіць кантэнт."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Не атрымалася знайсці падыходнае ядро ​​ці файл кантэнту; загрузіце ўручную."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Графічны драйвер сістэмы несумяшчальны з бягучым відэадрайверам RetroArch. Будзе выкарыстаны драйвер %s. Калі ласка, перазапусціце RetroArch для прымянення змен."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Паспяховае ўсталяванне ядра"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Няўдалае ўсталяванне ядра"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Націсніце направа пяць разоў для выдалення ўсіх чыт-кодаў."
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "Не атрымалася захаваць адладачныя звесткі."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_DEBUG_INFO,
   "Не атрымалася адаслаць адладачныя звесткі на сервер."
   )
MSG_HASH(
   MSG_SENDING_DEBUG_INFO,
   "Адпраўка адладачных звестак..."
   )
MSG_HASH(
   MSG_SENT_DEBUG_INFO,
   "Звесткі для адладкі паспяхова адпраўлены на сервер. Ваш ID-нумар: %u."
   )
MSG_HASH(
   MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
   "Націсніце яшчэ два разы для адпраўкі звестак камандзе RetroArch."
   )
MSG_HASH(
   MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
   "Націсніце яшчэ раз для адпраўкі звестак камандзе RetroArch."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Агульны ўзровень гучнасці гукавога мікшэра"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Сканаванне Netplay завершана."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Нажаль, дадзеная функцыя не працуе: ядры, не якія запытваюць кантэнт, не могуць удзельнічаць у netplay."
   )
MSG_HASH(
   MSG_NATIVE,
   "Родная"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Атрымана невядомая каманда netplay"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Файл ужо існуе. Захаванне ў рэзервовы буфер"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "Атрымана злучэнне з: \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Атрымана злучэнне з: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Порт для сеткавай гульні паспяхова прывязаны"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "Вонкавая сетка мае прыватны ці агульны адрас. Рэкамендуецца выкарыстоўваць сервер перасылкі."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Не ўдалося прывязаць порт UPnP для сеткавай гульні"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "Адсутнічаюць аргументы і ўбудаванае меню, спампоўваецца даведка..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Ўстаноўка дыска ў прывад"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Чаканне кліента..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "З вашым пакоем нельга злучыцца праз інтэрнэт."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Вы пакінулі гульню"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "Вы падключыліся ў якасці Гульца %u"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "Вы падключыліся з прыладамі ўводу %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "Гулец %.*s пакінуў гульню"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s падключыўся ў якасці Гульца %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s падключыўся з прыладамі ўводу %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Спроба злучэння з сеткавай гульнёй не атрымалася, бо ўдзельнік не запусціў RetroArch або выкарыстоўвае старую версію RetroArch."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Злучэнне немагчыма з прычыны запуску старой версіі RetroArch удзельнікам сеткавай гульні."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "УВАГА: удзельнік Netplay выкарыстоўвае іншую версію RetroArch. Пры ўзнікненні праблем, выкарыстоўвайце аднолькавыя версіі."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Злучэнне немагчыма з прычыны запуску іншага ядра ўдзельнікам сеткавай гульні."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "УВАГА: удзельнік Netplay выкарыстоўвае іншую версію ядра. Пры ўзнікненні праблем, выкарыстоўвайце аднолькавыя версіі."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Гэтае ядро не падтрымлівае сеткавую гульню паміж гэтымі платформамі"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Гэтае ядро не падтрымлівае сеткавую гульню паміж разнастайнымі платформамі"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Увядзіце пароль сервера netplay:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Паведамленне ў сеткавы чат:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Дазволіць злучэнне карыстальніку:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Няправільны пароль"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" адлучыўся"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Кліент сеткавай гульні адлучыўся"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Сеткавая гульня адлучана"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "Вы не маеце дазволу на гульню"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "Няма свабодных слотаў для гульцоў"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "Запытаныя прылады ўводу недаступныя"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Немагчыма пераключыцца ў рэжым гульні"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Сеткавы пір \"%s\" пастаўлены на паўзу"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Вашая мянушка зменена на \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "Кліент выгнаны: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "Не ўдалося выгнаць кліента: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "Кліент забанены: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "Не атрымалася забараніць кліента: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Гуляе ў"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "У рэжыме гледача"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "Прылады"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "З падтрымкай чата"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "Выклікана запавольванняў"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Гучнасць"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Аўтавызначэнне"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Магчымасці"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Злучэнне з хастом сеткавай гульні"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Злучэнне з портам"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Слот злучэння"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "Атрыманне спіса ядраў..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Не атрымалася набыць спіс ядраў!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "Ужо ўсталявана апошняя версія: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Абнаўленне ядра: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Сцягванне ядра: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Выманне ядра: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Усталявана ядро: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Не атрымалася ўсталяваць ядро: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Сканаванне ядраў..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Праверка ядра: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Усе ўсталяваныя ядры апошніх версій"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Усе ядры якія падтрымліваюцца зменены на версіі з Play Store"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "абноўлена ядраў: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "прапушчана ядраў: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Абнаўленне ядра адключана - ядро заблакаванае: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Скіданне ядраў: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Скінута ядраў: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Ачыстка плэй-ліста: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Плэй-ліст ачышчаны: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Абнаўленне не атрымалася - плэй-ліст не змяшчае сапраўдных запісаў сканавання: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Абнаўленне не атрымалася - каталог змесціва няправільны або адсутнічае: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Абнаўленне не атрымалася - назва сістэмы няправільная або адсутнічае: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Абнаўленне не атрымалася - няправільнае ядро: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Памылка абнаўлення - няправільны/адсутны аркадны DAT-файл: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Памылка абнаўлення - занадта вялікі аркадны DAT-файл (недастаткова памяці): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Дададзена да ўпадабанага"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Не ўдалося дадаць да ўпадабанага: плэй-ліст запоўнены"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "Дададзена да плэй-ліста"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Не ўдалося дадаць да плэй-ліста: плэй-ліст запоўнены"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Прызначана ядро: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "Сувязь з ядром у плэй-лісце была скінутая."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Устаўлены дыск"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Не ўдалося дадаць дыск"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Каталог праграмы"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Чыт-коды загружаныя."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Ужыты патч: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Ужыванне шэйдара"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Гук адключаны."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Гук уключаны."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Памылка захавання профіля кантролера."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Профіль кантролера паспяхова захаваны."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "Профіль захаваны ў каталог з профілямі кантролераў як\n\"%s\""
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Памылка аўтазахавання."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Аўтаматычнае захаванне стану ў"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Выснова каманднага інтэрфейсу на порт"
   )
MSG_HASH(
   MSG_BYTES,
   "байтаў"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Не ўдалося вызначыць шлях да новай канфігурацыі. Выкарыстоўвайце бягучы час."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "Параўнанне з вядомымі магічнымі лікамі..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Скампіляваны паводле API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Каталог канфігурацый не зададзены. Не ўдалося захаваць новую канфігурацыю."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Злучаны з"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "CRC32 кантэнту адрозніваюцца. Нельга выкарыстоўваць розныя гульні."
   )
MSG_HASH(
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "Хост пачаў іншую гульню."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "Пінг з вамі занадта вялікі для хаста."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Загрузка кантэнту прапушчана, рэалізацыя будзе загружаная сама па сабе."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Ядро не падтрымлівае захаванне станаў."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Файл опцый ядра паспяховы створаны."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "Файл опцый ядра паспяховы прыбраны."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Усе опцыі ядра скінуты да прадвызначаных."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "Опцыі ядра захаваныя ў:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "Не атрымалася захаваць опцыі ядра ў:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Не ўдалося знайсці ніякі наступны драйвер"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Не ўдалося знайсці сумяшчальную сістэму."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Не ўдалося знайсці сапраўдны трэк даных"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "Не ўдалося адкрыць трэк даных"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "Не ўдалося прачытаць фал змесціва"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Не ўдалося прачытаць загаловак відэароліка."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Немагчыма прачытаць захаванне з відэа."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "Несупадзенне CRC32 файла кантэнту і сумы кантэнту ў загалоўку файла паўтору. Высокая верагоднасць разсінхранізацыі пры прайграванні паўтору."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Зададзена ручное значэнне таймінгу"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Дэкампрэсія ўжо выконваецца."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Памылка дэкампрэсіі."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Выяўлена вобласць прагляду"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Не знойдзены слушны патч для кантэнту."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Адлучыць прыладу ад сапраўднага порта."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Зачынены латок віртуальнага cd-прывада."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Адкрыты латок віртуальнага cd-прывада."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Сцягванне"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "пазіцыя"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Памылка пры сцягванні"
   )
MSG_HASH(
   MSG_ERROR,
   "Памылка"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Не прадстаўлены кантэнт, неабходны ядру libretro."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Не прадстаўлены адмысловы кантэнт, неабходны ядру libretro."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Ядро не падтрымлівае VFS, загрузка з лакальнай копіі не атрымалася"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Памылка разбору аргументаў."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Памылка захавання файла опцый ядра."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "Не атрымалася выдаліць файл опцый ядра."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Памылка захавання файла прывязак уводу."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Памылка выдалення файла прывязак уводу."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Памылка пры захаванні набору налад шэйдара."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Вонкавы каталог праграмы"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Выманне"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Выманне файла"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Не ўдалося захаваць канфігурацыю ў"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Не атрымалася падключыць назіральніка."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Не атрымалася вылучыць памяць для прапатчанага змесціва..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Не атрымалася ўжыць шэйдар."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Не атрымалася ўжыць набор налад шэйдара:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Не атрымалася прывязаць сокет."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Не атрымалася стварыць каталог."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Не атрымалася выняць кантэнт са сціснутага файла"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Не ўдалося атрымаць ад кліента мянушку."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Не атрымалася загрузіць"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Не атрымалася загрузіць змесціва"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Не атрымалася загрузіць файл запісу"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Не атрымалася загрузіць накладку."
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "Не абраны аверлэй клавіятуры."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Не атрымалася загрузіць стан з"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Не ўдалося адкрыць ядро libretro"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Не атрымалася прапатчыць"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Не ўдалося атрымаць загаловак ад кліента."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Не ўдалося атрымаць мянушку."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Не ўдалося атрымаць ад хаста мянушку."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Не ўдалося атрымаць ад хаста памер мянушкі."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "Не атрымалася атрымаць дадзеныя SRAM з боку хаста."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Не ўдалося вымаць дыск з латка."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Не ўдалося прыбраць часовы файл"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "Не атрымалася захаваць SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "Не атрымалася загрузіць SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Не атрымалася захаваць у"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Не атрымалася адправіць мянушку."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Не атрымалася адправіць памер мянушкі."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Не атрымалася адправіць кліенту мянушку."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Не атрымалася адправіць хасту мянушку."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "Не атрымалася адправіць дадзеныя SRAM кліенту."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Не атрымалася запусціць аўдыядрайвер. Будзе працягнута без гуку."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Не атрымалася пачаць запіс відэа."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Не атрымалася пачаць запіс."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Не атрымалася стварыць здымак экрана."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Не атрымалася адмяніць загрузку."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Не атрымалася адмяніць захаванне."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Не атрымалася ўключыць гук."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Атрымана крытычная памылка"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Файл не знойдзены"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Знойдзена аўтазахаванне ў"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Пазнака знойдзенага дыска"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Знойдзена першая дарожка дадзеных у файле"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Дасягнуты апошні слот захавання"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "Дасягнуты апошні слот паўтору"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "Не ад бягучага запісу"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "Несумяшчальна з паўторам"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Знойдзены шэйдар"
   )
MSG_HASH(
   MSG_FRAMES,
   "Кадры"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Адмысловыя гульні опцыі ядра знойдзены ў"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Адмысловыя тэчцы опцыі ядра знойдзены ў"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Атрыманы нерэчаісны індэкс дыску."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Рэжым перахопу мышы"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Гульнявы фокус уключаны"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Гульнявы фокус выключаны"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Ядро выкарыстоўвае апаратны рэндэрынг. Уключыце запіс з GPU."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "Падвышаная кантрольная сума не адпавядае CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Зыходны чыт"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Імя файла з чыт-кодамі"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Імя файла прэсета"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "Увядзіце імя файла пераазначэнняў"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "Увядзіце імя файла прывязак"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Змяніць назву"
   )
MSG_HASH(
   MSG_INTERFACE,
   "Інтэрфейс"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Унутранае сховішча"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Здымнае сховішча"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Няправільны памер мянушкі."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "у байтах"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "у мегабайтах"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "у гігабайтах"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "скампіляванае для версіі libretro, адрознай ад бягучай."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Франтэнд для libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Загружана захаванне са слота #%d."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Загружана захаванне са слота #-1 (Аўта)."
   )
MSG_HASH(
   MSG_LOADING,
   "Загрузка"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Адзін ці некалькі файлаў мікрапраграм адсутнічаюць"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Загрузка змесціва"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Загрузка файла гісторыі"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Загрузка файла абранага"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Загрузка захавання"
   )
MSG_HASH(
   MSG_MEMORY,
   "Памяць"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "Файл з запісам націскаў не з'яўляецца правільным файлам ПАЎТОРА."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Фармат запісу паўтору мае іншую версію серыялізатара. Высокая верагоднасць адмовы."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Завершана ўзнаўленне запісу паўтору."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Прыпыненне запісу."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Памылка запуску сеткавай гульні."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "Ядро не падтрымлівае сеткавую гульню."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Кантэнт адсутнічае, запуск фіктыўнага ядра."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Няма перазапісаных захаванняў."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Няма загружаных захаванняў."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Памылка падчас захавання перавызначэнняў."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "Памылка падчас пазбаўлення перавызначэнняў."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Перавызначэнні паспяхова захаваныя."
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "Перавызначэнні паспяхова адкінутыя."
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "Перавызначэнні паспяхова выгружаныя."
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "Няма чаго захоўваць. Перавызначэнні не захаваныя."
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "Без захавання. Дзейнічаюць перавызначэнні."
   )
MSG_HASH(
   MSG_PAUSED,
   "Прыпынена."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Чытанне пачатковай дарожкі дадзеных..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Адмена запісу праз змену памеру акна."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Запіс у"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Файл з чыт-кодамі перанакіраваны ў"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Файл карты памяці перанакіраваны ў"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Файл захавання перанакіраваны ў"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Файл прывязак уводу паспяхова захаваны."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Файл прывязак уводу паспяхова выдалены."
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "Налады прывязак скінуты да стандартных значэнняў."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Дыск выняты з латка."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Выдалены часовы файл кантэнту"
   )
MSG_HASH(
   MSG_RESET,
   "Скіданне"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Запіс запушчаны зноў праз пераініцыялізацыю драйвера."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Адноўлена старое захаванне."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Шэйдары: адноўлены стандартны прасэт шэйдара"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Вяртанне каталога карт памяці ў"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Зварот каталога захаванняў да"
   )
MSG_HASH(
   MSG_REWINDING,
   "Перамотка."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "Перамотка назад недаступная праз адсутнасць у ядры серыялізаванай падтрымкі захавання стану."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Ініцыялізацыя буфера перамоткі з памерам"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Памылка стварэння буфера перамоткі. Перамотка будзе адключаная."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "Ядро выкарыстоўвае асобны паток для гуку. Перамотка немагчымая."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Дасягнута мяжа буфера перамоткі."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Новая канфігурацыя захавана ў"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Захавана ў слот #%d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Стан захавана ў слот #-1 (Аўта)."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Паспяхова захавана ў"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Запіс RAM"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Стварэнне захавання"
   )
MSG_HASH(
   MSG_SCANNING,
   "Сканаванне"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Сканіраванне каталога завершана."
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Адпраўка каманды"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Некалькі патчаў відавочна вызначаны, ігнаруючы ўсё..."
   )
MSG_HASH(
   MSG_SHADER,
   "Шэйдар"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Набор налад шэйдара паспяхова захаваны."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Запаволены рух."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Перамотка наперад."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Запаволеная перамотка назад."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Пропуск загрузкі SRAM."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "Немагчыма захаваць SRAM."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Забарона перазапісу SRAM"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Прайграванне запісу."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Запіс відэа ў"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Памер захавання"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Слот захавання"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "Слот паўтору"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Стварэнне здымка экрана."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Здымак экрана захаваны"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Раскрыта дасягненне"
   )
MSG_HASH(
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "Раскрыта рэдкае дасягненне"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "Спроба трапіць у табліцу лідараў пачата"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "Не ўдалося трапіць у табліцу лідараў"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "Адпраўлена %s для %s" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "Ранг: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "Найлепшы вынік: %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Выбар тыпу мініяцюры"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Поўнаэкранныя эскізы"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Прагляд метададзеных"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Няма даступных мініяцюр"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "Усе магчымыя спампоўкі мініяцюр для гэтага запісу плэй-ліста ўжо апрабаваныя."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Націсніце зноў каб выйсці..."
   )
MSG_HASH(
   MSG_TO,
   "у"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Загрузка захавання адменена."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Адмена захавання стану"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Невядомы"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Паўза адключана."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Атрымана невядомая каманда \"%s\".\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Выкарыстоўваецца імя ядра для новай канфігурацыі."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Выкарыстоўваецца фіктыўнае ядро ​​libretro. Запіс не робіцца."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Злучыць прыладу ад сапраўднага порта."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Адлучэнне прылады ад порта"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Перазагрузка..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Выключэнне..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Версія API libretro"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Не ўдалося разлічыць памеры вобласці прагляду. Будуць скарыстаны неапрацаваныя дадзеныя. Магчымы памылкі..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Не ўдалося адкрыць латок віртуальнага cd-прывада."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Не атрымалася зачыніць латок віртуальнага cd-прывада."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Аўтазагрузка захавання з"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "Памылка аўтаматычнай загрузкі захавання стану з \"%s\"."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "Загружана аўтазахаванне з \"%s\"."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "падключаны да порта"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s падключаны да порта %u"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "адлучаны ад порта"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s адлучаны ад порта %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "не наладжаны"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) не наладжаны"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "не наладжаны, выкарыстоўваецца рэзерв"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) не наладжаны, выкарыстоўваецца рэзерв"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Пошук bluetooth завершаны."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "Спалучэнне выдаленае. Перазапусціце RetroArch для паўторнага злучэння/спалучэння."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Пошук Wi-Fi завершаны."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Пошук прылад bluetooth..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Сканаванне бяздротавых сетак..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Уключэнне Wi-Fi..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Адключэнне Wi-Fi..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Адлучэнне ад Wi-Fi..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Пошук хастоў сеткавай гульні..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Падрыхтоўка да пошуку кантэнту..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Увядзіце пароль"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Слушны пароль."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Памылковы пароль."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Увядзіце пароль"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Слушны пароль."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Памылковы пароль."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "Загружана пераазначэнне канфігурацыі."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Загружаны прывязкі ўводу для гульні."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Загружаны прывязкі ўводу для каталога кантэнту."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Загружаны прывязкі ўводу для ядра."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "Налады прывязак уводу захаваны ў:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "Не атрымалася захаваць налады прывязак уводу ў:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Забяганне ўключана. Выдалена кадраў затрымкі: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Уключана забяганне з другім асобнікам. Выдалена кадраў затрымкі: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Забяганне выключана."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Забяганне выключана, бо ядро не падтрымлівае захаванне станаў."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "Забяганне недаступна праз адсутнасць у ядры дэтэрмінаванай падтрымкі захавання стану."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Не ўдалося стварыць захаванне. Забяганне адключана."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Не атрымалася загрузіць захаванне. Забяганне адключана."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Не ўдалося стварыць другі асобнік. Забяганне будзе выкарыстоўваць толькі адзін экзэмпляр."
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "Папераджальныя кадры ўключаны. Кадраў затрымкі прыбрана: %u."
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "Папераджальныя кадры выключаныя."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Папераджальныя кадры выключаныя, бо бягучае ядро ​​не падтрымлівае захаванне станаў."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "Папераджальныя кадры недаступныя праз адсутнасць у ядры дэтэрмінаванай падтрымкі захавання стану."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "Не ўдалося вылучыць памяць для кадраў апярэджання."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "Не ўдалося стварыць стан захавання. Папераджальныя кадры выключаныя."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "Не атрымалася загрузіць стан. Папераджальныя кадры выключаныя."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Сканаванне файла завершана."
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Паспяхова пачаты пошук чыт-кодаў."
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Не ўдалося запусціць пошук чыт-кодаў."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Пошук не быў ініцыялізаваны/запушчаны."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Новых супадзенняў = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "Дададзена %u супадзенняў."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Не ўдалося дадаць супадзенні."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Створаны код з супадзення."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Не ўдалося стварыць код."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Супадзенне выдаленае."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Недастаткова месца. Максімальная колькасць адначасовых чытоў - 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Новы чыт-код дададзены ў пачатак спісу."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Новы чыт-код дададзены ў канец спісу."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Усе чыты выдаленыя."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Новы чыт-код дададзены перад бягучым."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Новы чыт-код дададзены пасля бягучага."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Чыт-код скапіяваны перад бягучым."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Чыт-код скапіяваны пасля бягучага."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Чыт выдалены."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Не ўдалося ўсталяваць дыск."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Не атрымалася ўсталяваць апошні загружаны дыск."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "Не ўдалося злучыцца з кліентам."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Не ўдалося злучыцца з хастом."
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "Сеткавы хост запоўнены."
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "Вас забанілі на дадзеным хасту."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Не ўдалося атрымаць загаловак хаста."
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "RetroAchievements: уваход як \"%s\"."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Загрузка станаў патрабуе адключэння ці прыпынення хардкор рэжыму дасягненняў."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Загружана захаванне стану. Хардкор рэжым дасягненняў у бягучым сеансе адключаны."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Актываваны чыт-код. Хардкор рэжым дасягненняў у бягучым сеансе адключаны."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST,
   "Хардкор рэжым дасягненняў зменены хастом."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST,
   "Трэба абнавіць хост сеткавай гульні. Хардкор рэжым дасягненняў у бягучым сеансе адключаны."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "Завершана %s"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "Атрымана %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Уключаны хардкор рэжым, захаванне стану ды перамотка назад адключаныя."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "Гэтая гульня не мае дасягненняў."
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "Усе %d дасягненняў актываваныя для гэтага сеанса"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "Актывавана %d неафіцыйных дасягненняў"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "Вы маеце %d сярод %d раскрытых дасягненняў"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "%d не падтрымліваюцца"
)
MSG_HASH(
   MSG_CHEEVOS_RICH_PRESENCE_SPECTATING,
   "Назіраюць %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "Хардкод прыпынены. Налада ручной затрымкі кадраў не дазваляецца."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL,
   "Хардкор прыпынены. Інтэрвал абмену vsync вышэйшы за 1 не дазваляецца."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION,
   "Хардкор прыпынены. Устаўка чорнага кадра не дазваляецца."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED,
   "Хардкор прыпынены. Недазволеная налада: %s=%s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE,
   "Хардкор прыпынены. Немажліва атрымаць хардкорныя дасягненні для %s з выкарыстаннем %s"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "RetroAchievements: не ўдалося апазнаць гульню."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "Не атрымалася загрузіць RetroAchievements на гульню: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "Не атрымалася змяніць медыя RetroAchievements: %s"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "Найніжэйшая"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Ніжэйшая"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "Звычайная"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Вышэйшая"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Найвышэйшая"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Увага: адсутнічаюць рэсурсы, скарыстайцеся Анлайн-абнаўленнем."
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Увага: адсутнічаюць шрыфты для абранай мовы, скарыстайцеся Анлайн-абнаўленнем."
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Увага: мова не падтрымліваецца - выкарыстоўваецца англійская."
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Капіяванне дыска..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Дыск %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Калі ласка, спачатку загрузіце ядро."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Памылка чытання дыска. Капіяванне спынена."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Памылка запісу ў памяць. Капіяванне спынена."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Адсутнічае дыск у прывадзе."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Набор налад шэйдара паспяхова прыбраны."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Памылка пры зняцці набору налад шэйдара."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Абраны няслушны аркадны DAT-файл."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "Абраны аркадны DAT-файл занадта вялікі (недастаткова вольнай памяці)."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Памылка загрузкі аркаднага DAT-файла (няслушны фармат?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Няслушная канфігурацыя ручнога сканавання."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Не знойдзены кантэнт які падтрымліваецца."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Сканаванне кантэнту: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Праверка бягучых запісаў: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Сканаванне: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Ачыстка запісаў M3U: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Сканаванне завершана: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Сканаванне ядра: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Рэзервовая копія ўсталяванага ядра ўжо створана: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Рэзерваванне ядра: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Выдаленне састарэлых копій: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Рэзерваванне ядра завершана: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "Абраная рэзервовая копія ядра ​​ўжо ўсталявана: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Аднаўленне ядра: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Завершана аднаўленне ядра: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "Абраны файл ядра ўжо ўсталяваны: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Усталяванне ядра: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Устаноўка ядра завершана: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "Абраны няправільны файл ядра: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Памылка рэзервавання ядра: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Не ўдалося аднавіць ядро: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Не атрымалася ўсталяваць ядро: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Аднаўленне не ўдалося - ядро ​​заблакавана: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "Устаноўка не ўдалася - ядро ​​заблакавана: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Не ўдалося заблакаваць ядро: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Не атрымалася разблакаваць ядро: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "Не атрымалася выдаліць ядро ​​са спісу 'Аўтаномныя ядры': "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "Не атрымалася дадаць ядро ​​ў спіс 'Аўтаномныя ядры': "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "Выдаленне немагчыма - ядро ​​заблакавана: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Непадтрымліваемы відэарэжым"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "Памылка запісу ў каталог з інфа-файламі ядраў - кэш інфа-файлаў будзе адключаны"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "Знойдзена ўваходнае захаванне ў"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Загрузка ўваходнага захавання з"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "Не ўдалося ўвайсці ў GameMode"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "Не атрымалася ўвайсці ў GameMode - пераканайцеся, што дэман GameMode усталяваны/запушчаны"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "Уключана сінхранізацыя з дакладнай частатой кантэнту."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "Адключана сінхранізацыя з дакладнай частатой кантэнту."
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "Частата абнаўлення ўсталявана на %s Гц."
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Абнавіць Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Найменне франтэнда"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Версия Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Перазапуск"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Падзяліць Joy-Con"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Замяшчэнне маштабу віджэтаў"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Ручная ўстаноўка каэфіцыента маштабавання пры адлюстраванні віджэтаў. Улічваецца толькі калі выключана 'Аўтамаштабаванне віджэтаў'. Дазваляе змяняць памер апавяшчэнняў, індыкатараў і элементаў кіравання незалежна ад зададзенага маштабу меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Раздзяляльнасць экрана"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Дазвол экрана: па змаўчанні"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Раздзяляльнасць экрана: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Раздзяляльнасць экрана: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Ужыванне: па змаўчанні"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Ужыванне: %dx%d\nSTART для скіду"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Ужыванне: %dx%d - %s\nSTART для скіду"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Вяртанне на: па змаўчанні"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Вяртанне на: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Вяртанне на: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Выбраць рэжым адлюстравання (патрабуецца перазапуск)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Выключыць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Пашыраны доступ да памяці"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Адкрыць налады доступу да файлаў у Windows"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Адкрыйце налады дазволаў Windows, каб уключыць магчымасць broadFileSystemAccess."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Адкрыць..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Адкрыць яшчэ адну дырэкторыю з дапамогай сістэмнага выбару файлаў"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Мігатлівы фільтр"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Відэагама"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Мяккі фільтр"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Пошук і падлучэнне bluetooth-прылад."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Пошук і злучэнне з бяздротавымі сеткамі."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Уключыць Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Злучыцца з сеткай"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Злучыцца з сеткай"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Адлучыць"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Дэфлікер"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "Устанавіць шырыню экрана VI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Карэкцыя разгорткі (верх)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Настройка абрэзкі вылетаў разгорткі памяншэннем малюнка на паказаны лік сканлайнаў (уверсе экрана). Магчыма з'яўленне артэфактаў выявы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Карэкцыя разгорткі (ніз)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Настройка абрэзкі вылетаў разгорткі памяншэннем малюнка на паказаны лік сканлайнаў (унізе экрана). Магчыма з'яўленне артэфактаў выявы."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Рэжым устойлівай прадукцыйнасці"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "Сілкаванне і прадукцыйнасць CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "Палітыка"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Рэжым рэгулявання"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Уручную"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Дазваляе вырабляць настройку ўсіх параметраў CPU: рэжым, частаты і г. д. Толькі для дасведчаных карыстальнікаў."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Прадукцыйнасць (кіраваны)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Стандартны і рэкамендаваны рэжым. Максімальная прадукцыйнасць падчас гульні і захаванне энергіі пры прыпынку ці навігацыі па меню."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Які наладжваецца"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Дазваляе выбіраць рэгулятары для меню і геймплэя. Прадукцыйнасць, Па запыце або Schedutil рэкамендаваны для геймплэя."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Максімальная прадукцыйнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Падтрымліваць максімальную прадукцыйнасць: гранічныя частоты для найлепшага выніку."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Мінімальная магутнасць"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Выкарыстоўваць самую нізкую частату для эканоміі рэсурсаў. Карысна пры працы ад акумулятара, але прадукцыйнасць значна зменшыцца."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Збалансаваны"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Адаптуецца да бягучай нагрузкі. Падыходзіць для большасці прылад і эмулятараў, дапамагаючы захаваць рэсурсы. На некаторых прыладах можа выклікаць падзенне прадукцыйнасці ў патрабавальных гульнях і ядрах."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Мінімальная частата"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Максімальная частата"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Мінімальная частата ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Максімальная частата ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "Рэгулятар CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Рэгулятар ядра"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Рэгулятар меню"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "Рэжым гульні"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Можа паляпшаць прадукцыйнасць, змяншаць затрымку ўводу і прыбіраць скажэнні гуку. Для працы запатрабуецца ўсталяваць https://github.com/FeralInteractive/gamemode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Уключэнне Linux GameMode можа зменшыць затрымку, выправіць праблемы з гукам і павысіць агульную прадукцыйнасць шляхам аўтаналадкі CPU і GPU для аптымальнай эфектыўнасці.\nПатрэбна ўстаноўка праграмнага забеспячэння GameMode. Наведайце https://github.com/FeralInteractive/gamemode для атрымання інфармацы[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Выкарыстоўваць рэжым PAL60"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Перазапуск RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Выхад і перазапуск RetroArch. Патрабуецца для прымянення некаторых налад (напрыклад пры змене драйвера меню)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Фрэймаў у блоку"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Пераважна папярэдні дотык"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "Выкарыстоўваць пярэднюю сэнсарную панэль замест задняй."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Уключыць дотык"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Супастаўленне клавіятуры з кантролерам"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Тып супастаўлення клавіятуры з кантролерам"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Малая клавіятура"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Тайм-аўт блакавання ўводу"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Час чакання (у мс) да атрымання поўнай сукупнасці ўводу. Выкарыстоўвайце пры праблемах з адначасовым націскам кнопак (толькі для Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Паказваць 'Перазагрузка'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Паказваць опцыю 'Перазагрузка'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Паказваць 'Выключэнне'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Паказваць опцыю 'Выключэнне'."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Закрыта паролем"
   )
MSG_HASH(
   MSG_INTERNET,
   "Сеціва"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "Сеціва (пераадрасацыя)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "Інтэрнэт (без злучэння)"
   )
MSG_HASH(
   MSG_LOCAL,
   "Лакальны"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Статус унутранага сховішча: чытанне/запіс"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Статус унутранага сховішча: толькі чытанне"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Яркасць экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Павялічыць ці паменшыць яркасць экрана."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "Разгон СPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Разгон працэсара Switch."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Вызначае стан Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Службы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Упраўленне службамі аперацыйнай сістэмы."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Адкрыць доступ да сеткавых тэчак праз пратакол SMB."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Выкарыстоўваць SSH для выдаленага доступу да каманднага радка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Кропка доступу Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Уключыць/адключыць кропку доступу Wi-Fi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Часавы пояс"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Абярыце часавы пояс для падладкі даты і часу пад ваша месцазнаходжанне."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "Адлюстроўвае спіс даступных часавых паясоў. Пасля выбару часавога пояса час і дата карэктуюцца ў адпаведнасці з абраным часавым поясам. Мяркуецца, што сістэмныя/апаратныя гадзіны ўсталёўваюцца па UTC."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Налады Nintendo Switch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Кіраванне наладамі для Nintendo Switch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "Разгон СPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "Уключыць разгон частот CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "Падтрымка CEC"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "Уключае CEC для сувязі з ТБ пры падключанай док-станцыі"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "Адключыць Bluetooth ERTM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "Адключае Bluetooth ERTM для выпраўлення спалучэння на некаторых прыладах"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Адключэнне кропкі доступу Wi-Fi."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Адлучэнне ад Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Злучэнне з Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Няма SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Кропка доступу Wi-Fi ужо ўключана"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Кропка доступу Wi-Fi не ўключана"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Запуск кропкі доступу Wi-Fi з SSID=%s і паролем=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Не атрымалася стварыць файл канфігурацыі кропкі доступу Wi-Fi."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Няправільны файл канфігурацыі - адсутнічае APNAME або PASSWORD у %s"
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Крок мышы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Ўстаноўка маштабу па x/y для змены хуткасці Wiimote."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Маштаб тачскрыну"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "Падладка x/y каардынат тачскрыну для адпаведнасці маштабу дысплэя ў сістэме."
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "Touch VMouse у рэжыме ўказальніка"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "Уключыце для перадачы падзей дотыку пры ўводзе з сэнсарнага экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "Touch VMouse у рэжыме мышы"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "Уключае эмуляцыю віртуальнай мышкі з дапамогай падзей дотыку пры ўводзе з сэнсарнага экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Touch VMouse у рэжыме тачпада"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Уключыце сумесна з рэжымам мышы, каб выкарыстоўваць сэнсарны экран у якасці тачпада."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Touch VMouse у рэжыме трэкбола"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Уключыце сумесна з рэжымам мышы, каб выкарыстоўваць сэнсарны экран у якасці трэкбола з інэрцыяй паказальніка."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "Жэсты сэнсарнай VMouse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "Уключае сэнсарныя жэсты, такія як дотык адным пальцам, перацягванне і гартанне."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "RGA-маштабаванне"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA-маштабаванне і бікубічная фільтрацыя. Можа скажаць віджэты."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Маштабаванне па тыпе кантэксту"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Апаратнае маштабаванне кантэксту (калі даступна)."
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "Уключыць частату / L2 кэш New 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "Актывуе тактавую частату (804 Mhz) і L2 кэш New 3DS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "Ніжні экран 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Уключае вывад на ніжнім экране інфармацыі аб статусе. Адключыце для эканоміі зарада назапашвальніка і павышэння прадукцыйнасці."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "Рэжым экрана 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Выбар паміж 3D ці 2D рэжымам экрана. У рэжыме '3D' пікселі квадратныя і прымяняецца эфект глыбіні пры праглядзе хуткага меню. Рэжым '2D' забяспечвае лепшую прадукцыйнасць."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (эфект піксельнай сеткі)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (высокая раздзяляльнасць)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Дакраніцеся да тачскрына, каб перайсці\nу меню RetroArch"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "Рэсурс(ы) не знойдзены"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Няма\nДадзеных"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "Няма\nСкрыншота"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Працягнуць гульню"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Стварыць кропку\nаднаўлення"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Загрузіць\nкропку\nаднаўлення"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "Каталог рэсурсаў ніжняга экрана"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "Каталог рэсурсаў для ніжняга экрана. Мае змяшчаць \"bottom_menu.png\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "Уключыць шрыфт"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "Адлюстроўвае шрыфт ніжняга меню. Уключыце, каб бачыць апісанне кнопак на ніжнім экране. Адкідае даты захаванняў стану."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Чырвоны колер шрыфта"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Рэгулюе чырвоны колер шрыфта ніжняга экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Зялёны колер шрыфта"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Рэгулюе зялёны колер шрыфта ніжняга экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Сіні колер шрыфта"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Рэгулюе сіні колер шрыфта ніжняга экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Празрыстасць колеру шрыфта"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "Рэгулюе празрыстасць шрыфта ніжняга экрана."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "Памер шрыфта"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "Змяняе памер шрыфта ніжняга экрана."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Сканаванне завершана.<br><br>\nДля слушнага сканавання кантэнту неабходна:\n<ul><li>загрузіць сумяшчальнае ядро</li>\n<li>абнавіць інфармацыйныя файлы ядраў</li>\n<li>абнавіць базы даных</li>\n<li>перазапусціце RetroArch, калі быў выкананы любы з пералічаных пунктаў</li></ul>\nЗмесціва мусіць адпавядаць запісам існых баз даных <a href=\"https://docs.libretro. com/guides/roms-playlists-thumbnails/#sources\">па спасылцы</a>. Калі сканаванне па-ранейшаму не працуе <a href=\"https://www.github.com/libretro/RetroArch/issues\">адпраўце справаздачу пра памылку</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "Уключана кіраванне мышшу з тачскрыну"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "Адключана кіраванне мышшу з тачскрыну"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "Мікрафон sdl2 патрабуе гукавога драйвера sdl2"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "Уключаны адмысловыя магчымасці RetroArch. Галоўнае меню загрузіць ядро."
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "спынены."
   )
#ifdef HAVE_GAME_AI




MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_SHOW_DEBUG,
   "Паказ адладкі"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG,
   "Паказваць адладку"
   )

#endif

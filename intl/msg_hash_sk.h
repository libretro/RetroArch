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
   "Hlavné menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Nastavenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Obľúbené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "História"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Obrázky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Hudba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Videá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Online hranie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Prehľadávať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Bezobsahové jadrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importovať obsah"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Rýchla ponuka"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Rýchly prístup ku všetkým dôležitým nastaveniam v hre."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Načítať jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Vyberte jadro, ktoré chcete použiť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "Odnačítať jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Uvoľniť načítané jadro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Prehliadať implementáciu libretro core. Kde prehliadač začne, závisí od cesty Adresár core. Ak je prázdna, začne v koreni.\nAk je Adresár core priečinkom, menu ho použije ako horný priečinok. Ak je Adresár core úplnou cestou, začne v priečinku, kde sa súbor nachádza."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Načítať obsah"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Vyberte, ktorý obsah chcete spustiť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Prehliadať obsah. Na načítanie obsahu potrebujete 'Core' a súbor obsahu.\nAk chcete určiť, kde menu začne hľadať obsah, nastavte 'Adresár prehliadača súborov'. Ak nie je nastavený, začne v koreni.\nPrehliadač filtruje prípony pre posledný core nastavený v 'Načítať Core' a tento core použije pri načítaní obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Načítať disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Vložte fyzický disk. Najprv vyberte jadro (Načítať jadro), ktoré chcete použiť s diskom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Kopírovať disk"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Skopíruje disk fyzického média do internej pamäte. Uloží sa ako obrazový súbor."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Vysunúť disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Vysunie disk z fyzickej DVD mechaniky."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Zoznamy hier"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Tu sa zobrazí naskenovaný obsah podľa údajov uvedených v databáze."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Importovať obsah"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Vytvorte a aktualizujte zoznamy hier skenovaním jeho obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Zobraziť ponuku pracovnej plochy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Otvorí klasickú ponuku na pracovnej ploche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Zakázať režim kiosku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Zobraziť všetky úpravy súvisiace s konfiguráciou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Online aktualizácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Stiahnite si doplnky, súčasti a obsah pre RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Online hranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Pripojte sa k online hre alebo sa staňte hostiteľom relácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Nastavenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Nastavenie programu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Zobrazí systémové informácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Súbor s nastaveniami"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Spracujte a vytvárajte konfiguračné súbory."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Nápoveda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Získajte viac informácií ako program funguje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Reštart"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Reštartovať aplikáciu RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Ukončiť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Ukončiť aplikáciu RetroArch. Uloženie konfigurácie pri ukončení je povolené."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Ukončiť aplikáciu RetroArch. Uloženie konfigurácie pri ukončení je vypnuté."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Ukončí RetroArch. Ukončenie programu akýmkoľvek tvrdým spôsobom (SIGKILL, atď.) ukončí RetroArch bez uloženia konfigurácie. Na unixoch SIGINT/SIGTERM umožňujú čisté ukončenie vrátane uloženia konfigurácie, ak je povolené."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_NOW,
   "Synchronizovať teraz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_NOW,
   "Manuálne spustiť cloudovú synchronizáciu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Riešenie konfliktov: Ponechať lokálne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Vyrieši všetky konflikty nahraním lokálnych súborov na server."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Riešenie konfliktov: Ponechať na serveri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Vyrieši všetky konflikty stiahnutím súborov zo servera, ktoré nahradia lokálne kópie."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Stiahnuť jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Stiahnite si a nainštalujte jadro z online aktualizácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Inštalácia alebo obnovenie jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Nainštalujte alebo obnovte jadro z adresára \" Stiahnuté súbory\"."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Spustiť videoprocesor"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Spustiť Diaľkový RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Hlavný adresár"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Sťahovanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Prehľadávať archív"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Načítať archív"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Obľúbené"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Obsah pridaný do \"Obľúbené\" sa zobrazí tu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Hudba"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Hudba ktorá bola naposledy prehrávaná sa zobrazí tu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Obrázky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Obrázky ktoré boly naposledy zobrazené sa zobrazí tu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Videa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Videa ktoré boli naposledy prehrávané sa zobrazí tu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Preskúmať"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Prehľadať obsah ktorý sa zhoduje s databázou prostredníctvom kategorizovaného vyhľadavácieho rozhrania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Bezobsahové jadrá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Tu sa zobrazia nainštalované jadrá, ktoré môžu pracovať bez samotného načítania obsahu."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Stiahnutie Jadier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Aktualizácia Stiahnutých Jadier"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Aktualizácia všetkých nainštalovaných jadier na najnovšiu dostupnú verziu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Prepnúť na jadrá z Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Nahradí všetky staršie a ručne inštalované jadrá najnovšími verziami z Play Store, ak sú dostupné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Aktualizácia Zoznamu Miniatúr"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Stiahnutie miniatúr pre položky vo vybranom zozname."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Stiahnutie Obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Stiahnuť slobodný obsah pre vybrané jadro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Stiahnutie jadra systému"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Stiahnite si pomocné systémové súbory potrebné na správnu/optimálnu prevádzku jadra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Aktualizácia Info Sůborov Jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Aktualizácia Assets"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Aktualizácia Profilov Ovládačov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Aktualizácia Cheatov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Aktualizácia Databáz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Aktualizácia Prekrívacich Prvkov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Aktualizácia GLSL Shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Aktualizácia Cg Shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Aktualizácia Slang Shaders"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Informácie Jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Zobraziť informácie týkajúce sa aplikácie/jadra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Informácie disku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Zobraziť informácie vloženého CD/DVD disku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Sieťové informácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Zobraziť sieťové zariadenia a súvisiace IP adresy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Systémové informácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Zobraziť informácie špecifické pre zariadenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Správca databázy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Zobraziť databázy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Správca kurzoru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Zobraziť predchádzajúce vyhľadávania."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Názov Jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Štítok Jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Verzia jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Názov systému"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Výrobca systému"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Kategórie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Autori"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Povolenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Licencia(ie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Podporované rozšírenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Požadované grafické API"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Celá cesta"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Podporované stavy uloženia hry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Žiadne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Základný (Uložiť/Načítať)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Rozšírené (uložiť/načítať, vrátiť sa späť)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Kompletné (uložiť/načítať, vrátiť sa späť, skočiť dopredu, hrať po sieti)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Firmvér(y)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "Poznámka: 'Systémové súbory sú v adresári obsahu' je povolené."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "Hľadám v: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Chýba; požadované:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Chýba; voliteľné:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Teraz požadované:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Teraz voliteľné:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Uzamknúť Nainštalované Jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Zabrániť úpravám aktuálne nainštalovaného core. Možno použiť na zabránenie nechceným aktualizáciám, keď obsah vyžaduje konkrétnu verziu core (napr. sady Arcade ROM) alebo keď sa zmení formát save state samotného core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Vylúčiť z menu 'Cores bez obsahu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Zabrániť zobrazeniu tohto jadra na karte/ponuke 'Jadrá bez obsahu'. Platí iba v prípade, že je režim zobrazenia nastavený na 'Vlastný'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Vymazať Jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Vymaže zvolené jadro z disku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Zálohovať Jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Vytvorí sa archivovaná záloha aktuálne nainštalovaného jadra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Obnova Zálohy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Nainštalujte predchádzajúca verzia jadra, zo zoznamu archivovaných záloh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Vymazať Zálohu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Odstráni sa súbor zo zoznamu archivovaných záloh."
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Dátum zostavenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "Verzia RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Verzia Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Kompiler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Model CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Vlastnosti CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Architektúra CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "CPU jadrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT dostupné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUNDLE_IDENTIFIER,
   "Identifikátor balíka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Identifikátor klientskeho rozhrania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Operačný Systém"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Zdroj Energie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Šírka Obrazovky (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Výška Obrazovky (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "DPI Obrazovky"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Názov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Popis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Žáner"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Úspechy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Kategória"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Jazyk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Región"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Exkluzívne pre konzolu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Exkluzívne pre platformu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Skóre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Médiá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Ovládanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Hrateľnosť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Naratív"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "Pokrok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Perspektíva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Nastavenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Vzhľad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Vozidlo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Vydavateľ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Vývojár"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Pôvod"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Franšíza"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "TGDB Hodnotenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Famitsu Magazine Hodnotenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Edge Magazine Recenzia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Edge Magazine Hodnotenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Edge Magazine Problém"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Mesiac dátumu vydania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Rok dátumu vydania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "BBFC Hodnotenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "ESRB Hodnotenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "ELSPA Hodnotenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "PEGI Hodnotenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Vylepšovací Hardvér"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "CERO Hodnotenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Sériové Č."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analógová Podpora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Podpora Vibrácíi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Co-op Podpora"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Načítať konfiguráciu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Načítať aktuálnu konfiguráciu a nahradiť aktuálne hodnoty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Uložiť Aktuálne Nastavenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Prepísať aktuálny súbor s nastaveniami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Uložiť Nové Nastavenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Uložiť súčasné nastavenia do samostatného súboru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_AS_CONFIG,
   "Uložiť konfiguráciu ako"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG,
   "Uložiť aktuálnu konfiguráciu ako vlastný konfiguračný súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_MAIN_CONFIG,
   "Uložiť hlavnú konfiguráciu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG,
   "Uložiť aktuálnu konfiguráciu ako hlavnú konfiguráciu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Resetovať do Predvolených Nastavení"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Obnoviť súčasné nastavenie na predvolené hodnoty."
   )

/* Main Menu > Help */

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Posunúť hore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Posunúť dolu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Potvrdiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Štart"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Otvoriť / Zatvoriť hlavné menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Ukončiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Otvoriť / Zatvoriť Klávesnicu"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Ovládače"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Zmeniť ovládače používané systémom."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Zmena nastavení video výstupu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Zvuk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Zmena vstupno-výstupných nastavení."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Vstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Zmeniť nastavenia ovládača, klávesnice a myši."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Odozva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Zmeňte nastavenia týkajúce sa videa, zvuku a vstupného oneskorenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Zmena nastavenia jadra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Konfigurácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Zmena predvolených nastavení konfiguračných súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Ukladanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Zmena nastavenia ukladania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Cloud synchronizácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Zmení nastavenia cloudovej synchronizácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Povoliť cloud synchronizáciu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Pokúsiť sa synchronizovať konfigurácie, sram a stavy s poskytovateľom cloudového úložiska."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Deštruktívna cloudová synchronizácia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Synchronizácia: Uloženia/stavy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Synchronizácia: Konfiguračné súbory"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Synchronizácia: Náhľady"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Synchronizácia: Systémové súbory"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "Keď je povolené, uloženia/stavy sa synchronizujú s cloudom."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "Keď je povolené, konfiguračné súbory sa synchronizujú s cloudom."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "Keď je povolené, náhľadové obrázky sa synchronizujú s cloudom. Vo všeobecnosti sa neodporúča okrem veľkých zbierok vlastných náhľadov; inak je lepšou voľbou sťahovač náhľadov."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "Keď je povolené, systémové súbory sa synchronizujú s cloudom. Toto môže výrazne predĺžiť čas synchronizácie; používajte s opatrnosťou."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Keď je vypnuté, súbory sa pred prepísaním alebo zmazaním presunú do záložného priečinka."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE,
   "Režim synchronizácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_MODE,
   "Automatická: synchronizácia pri štarte RetroArchu a pri uvoľňovaní cores. Manuálna: synchronizácia len pri manuálnom spustení tlačidla 'Synchronizovať teraz'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_AUTOMATIC,
   "Automaticky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_MANUAL,
   "Ručne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Backend cloudovej synchronizácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Aký sieťový protokol cloudového úložiska použiť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "URL cloudového úložiska"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "URL adresa pre vstupný bod API služby cloudového úložiska."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Meno používateľa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "Vaše používateľské meno pre účet cloudového úložiska."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Heslo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "Vaše heslo pre účet cloudového úložiska."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ACCESS_KEY_ID,
   "ID prístupového kľúča"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ACCESS_KEY_ID,
   "Vaše ID prístupového kľúča pre účet cloudového úložiska."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SECRET_ACCESS_KEY,
   "Tajný prístupový kľúč"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SECRET_ACCESS_KEY,
   "Váš tajný prístupový kľúč pre účet cloudového úložiska."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_S3_URL,
   "Vaša S3 endpoint URL pre cloudové úložisko."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Záznam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Zmena nastavení záznamu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Správca súborov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Zmení nastavenia prehliadača súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Súbor s nastaveniami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Komprimovaný archívny súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Súbor s nastaveniami nahrávania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Súbor kurzoru databázy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Súbor s nastaveniami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Súbor nastavenia shadera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Súbor shadera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Súbor premapovania ovládačov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Súbor cheatu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Súbor prekrytia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Databázový súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "Súbor písma TrueType."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Čistý súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Video. Jeho výberom otvoríte tento súbor pomocou videoprehrávača."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Hudba. Jeho výberom otvoríte tento súbor v prehrávači hudby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Súbor obrázku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Obrázok. Jeho výberom otvoríte tento súbor v prehliadači obrázkov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Jadro Libretro. Výberom tejto možnosti bude toto jadro priradené k hre."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Jadro Libretro. Ak tento súbor vyberiete, RetroArch toto jadro načíta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Adresár. Jeho výberom tento adresár otvoríte."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Zmena nastavení pretočenia, zrýchlenia a spomalenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Záznam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Zmena nastavenia záznamu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Upraviť zobrazenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Zmena prekrytia displeja, prekrytia klávesnice a nastavenia upozornení na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Užívateľské rozhranie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Zmena nastavení užívateľského rozhrania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Služba AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Zmeniť nastavenia AI služby (preklad/prevod textu na reč/ostatné)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Zjednodušenie prístupu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Zmeniť nastavenia moderátora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Správa napájania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Zmeniť nastavenia správy napájania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Úspechy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Zmeniť nastavenia úspechov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Sieť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Zmeniť nastavenia servera a siete."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Zmeniť nastavenia zoznamov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Používateľ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Zmení nastavenia súkromia, účtu a používateľského mena."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Priečinok"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Zmeniť východzie zložky umiestnenia súborov."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS,
   "Hacky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Mapovanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Médiá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Výkon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Zvuk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Špecifikácie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Úložisko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Systém"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Časovanie"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Zmeniť nastavenia súvisiace so Steamom."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Vstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Aký vstupný ovládač použiť. Niektoré video ovládače vynútia iný vstupný ovládač. (Vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "Ovládač udev číta evdev udalosti pre podporu klávesnice. Podporuje aj klávesnicový callback, myši a touchpady.\nV predvolenom nastavení vo väčšine distribúcií sú uzly /dev/input prístupné iba pre roota (mód 600). Môžete nastaviť pravidlo udev, ktoré ich sprístupní non-root používateľom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "Vstupný ovládač linuxraw vyžaduje aktívne TTY. Udalosti klávesnice sa čítajú priamo z TTY, čo ho zjednodušuje, ale nie je tak flexibilný ako udev. Myši atď. nie sú vôbec podporované. Tento ovládač používa staršie joystick API (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Vstupný ovládač. Video ovládač môže vynútiť iný vstupný ovládač."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Ovládač"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Aký ovládač pre kontrolery použiť. (Vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "Ovládač pre kontrolery DirectInput."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "Nízkoúrovňový ovládač pre Human Interface Device."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Surový Linux ovládač, používa staršie joystick API. Ak je to možné, použite udev."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Linux ovládač pre kontrolery pripojené cez paralelný port pomocou špeciálnych adaptérov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Ovládač pre kontrolery založený na knižniciach SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Ovládač pre kontrolery s rozhraním udev, vo všeobecnosti odporúčaný. Používa nedávne evdev joypad API pre podporu joysticku. Podporuje hotplugging a force feedback.\nV predvolenom nastavení vo väčšine distribúcií sú uzly /dev/input prístupné iba pre roota (mód 600). Môžete nastaviť pravidlo udev, ktoré ich sprístupní non-root používateľom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "Ovládač pre kontrolery XInput. Hlavne pre kontrolery XBox."
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Použiť ovládač videa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "OpenGL 1.x ovládač. Minimálna požadovaná verzia: OpenGL 1,1. Nepodporuje shadery. Ak je to možné, použite novšie OpenGL ovládače."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "OpenGL 2.x ovládač. Tento ovládač umožňuje použitie libretro GL cores popri softvérovo renderovaných cores. Minimálna požadovaná verzia: OpenGL 2,0 alebo OpenGLES 2,0. Podporuje formát shaderov GLSL. Ak je to možné, použite ovládač glcore."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "OpenGL 3.x ovládač. Tento ovládač umožňuje použitie libretro GL cores popri softvérovo renderovaných cores. Minimálna požadovaná verzia: OpenGL 3,2 alebo OpenGLES 3,0+. Podporuje formát shaderov Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Ovládač Vulkan. Tento ovládač umožňuje použitie libretro Vulkan cores popri softvérovo renderovaných cores. Minimálna požadovaná verzia: Vulkan 1,0. Podporuje HDR a Slang shadery."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "SDL 1,2 softvérovo renderovaný ovládač. Výkon je považovaný za suboptimálny. Zvážte jeho použitie len ako poslednú možnosť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "SDL 2 softvérovo renderovaný ovládač. Výkon pre softvérovo renderované implementácie libretro core závisí od SDL implementácie vašej platformy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Ovládač Metal pre platformy Apple. Podporuje formát shaderov Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Ovládač Direct3D 8 bez podpory shaderov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Ovládač Direct3D 9 s podporou starého formátu shaderov Cg."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Ovládač Direct3D 9 s podporou formátu shaderov HLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Ovládač Direct3D 10 s podporou formátu shaderov Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Ovládač Direct3D 11 s podporou HDR a formátu shaderov Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Ovládač Direct3D 12 s podporou HDR a formátu shaderov Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "Ovládač DispmanX. Používa DispmanX API pre Videocore IV GPU v Raspberry Pi 0..3. Bez podpory overlayov a shaderov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "Ovládač LibCACA. Produkuje znakový výstup namiesto grafiky. Neodporúča sa pre praktické použitie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Nízkoúrovňový video ovládač Exynos, ktorý používa blok G2D v Samsung Exynos SoC pre operácie blit. Výkon pre softvérovo renderované cores by mal byť optimálny."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Obyčajný video ovládač DRM. Toto je nízkoúrovňový video ovládač používajúci libdrm pre hardvérové škálovanie pomocou GPU overlayov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Nízkoúrovňový video ovládač Sunxi, ktorý používa blok G2D v Allwinner SoC."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Wii U ovládač. Podporuje Slang shadery."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Switch ovládač. Podporuje formát shaderov GLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "Ovládač OpenVG. Používa OpenVG hardvérovo akcelerované 2D vektorové grafické API."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "Ovládač GDI. Používa staršie Windows rozhranie. Neodporúča sa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Aktuálny video ovládač."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Zvuk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Použiť ovládač zvuku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "Ovládač RSound pre sieťové zvukové systémy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Starší ovládač Open Sound System."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Predvolený ALSA ovládač."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "ALSA ovládač s podporou vlákien."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "ALSA ovládač implementovaný bez závislostí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "Ovládač zvukového systému RoarAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "OpenAL ovládač."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "OpenSL ovládač."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "Ovládač DirectSound. DirectSound sa používa hlavne od Windows 95 po Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Ovládač Windows Audio Session API. WASAPI sa používa hlavne od Windows 7 a vyššie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "Ovládač PulseAudio. Ak systém používa PulseAudio, použite tento ovládač namiesto napr. ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "Ovládač PipeWire. Ak systém používa PipeWire, použite tento ovládač namiesto napr. PulseAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Ovládač Jack Audio Connection Kit."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Mikrofón"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Ovládač mikrofónu, ktorý bude použitý."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Resamplovač mikrofónu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Aký mikrofónový resampler ovládač použiť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Snímky mikrofónnych blokov"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Vzorkovač zvuku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Použiť ovládač vzorkovača zvuku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Implementácia Windowed Sinc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Konvoluntná implementácia kosínusu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Najjednoduchšia implementácia prevzorkovania. Tento resampler ignoruje nastavenie kvality."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Kamera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Použiť ovládač kamery."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Používaný ovládač Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Používaný ovládač Wi-Fi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Poloha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Použiť ovládač polohy."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Aký ovládač menu použiť. (Vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB je grafické rozhranie RetroArchu, ktoré vyzerá ako menu konzol 7. generácie. Podporuje rovnaké funkcie ako Ozone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone je predvolené GUI RetroArchu na väčšine platforiem. Je optimalizované pre navigáciu pomocou herného kontroleru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI je jednoduché vstavané GUI pre RetroArch. Má najnižšie nároky na výkon spomedzi ovládačov menu a možno ho použiť na obrazovkách s nízkym rozlíšením."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "Na mobilných zariadeniach RetroArch v predvolenom nastavení používa mobilné UI MaterialUI. Toto rozhranie je navrhnuté pre dotykové obrazovky a polohovacie zariadenia, ako myš/trackball."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Nahrávanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Použiť ovládač nahrávania."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Použiť ovládač MIDI."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Výstup signálu nízkeho rozlíšenia na použitie s CRT obrazovkami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Výstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Zmena nastavení video výstupu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Režim na celú obrazovku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Zmena nastavení režimu na celú obrazovku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Režim v Okne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Zmena nastavení režimu v okne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Škálovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Zmena nastavenia škálovania."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Zmena nastavenia videa HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Synchronizácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Zmena nastavenia synchronizácie videa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Pozastaviť šetrič obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Zabrániť aktivácii systémového šetriča obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Pozastaví šetrič obrazovky. Je to nápoveda, ktorú video ovládač nemusí nutne rešpektovať."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Vláknové Video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Zlepšuje výkon za cenu oneskorenia a vyššieho zasekávania videa. Používajte, iba ak nemôžete získať plnú rýchlosť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Použiť vláknový video ovládač. Použitie môže zlepšiť výkon za možnú cenu latencie a väčšieho video trhania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Vloženie Čierneho Rámu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "VAROVANIE: Rýchle blikanie môže na niektorých displejoch spôsobiť pretrvávanie obrazu. Používate na vlastné riziko // Vkladá čiernu snímku (snímky) medzi snímky. Môže výrazne redukovať motion blur emuláciou skenovania CRT, ale za cenu jasu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Vkladá čiernu snímku (snímky) medzi snímky pre lepšiu jasnosť pohybu. Použite iba možnosť určenú pre vašu aktuálnu obnovovaciu frekvenciu displeja. Nepoužívajte pri obnovovacích frekvenciách, ktoré nie sú násobkami 60 Hz, ako 144 Hz, 165 Hz atď. Nekombinujte so Swap Interval > 1, sub-frames, Frame Delay alebo Sync to Exact Content Framerate. Systémové VRR môžete nechať zapnuté, len nie toto nastavenie. Ak si všimnete -akékoľvek- dočasné pretrvávanie obrazu, mali[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Vkladanie čiernych snímok - Tmavé snímky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Upraví celkový počet čiernych snímok v sekvencii BFI scan out. Viac znamená vyššiu jasnosť pohybu, menej znamená vyšší jas. Neuplatňuje sa pri 120 Hz, keďže k dispozícii je celkovo iba 1 BFI snímka. Nastavenia vyššie ako možné vás obmedzia na maximum možné pre vašu zvolenú obnovovaciu frekvenciu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Upraví počet snímok zobrazených v sekvencii BFI, ktoré sú čierne. Viac čiernych snímok zvyšuje jasnosť pohybu, ale znižuje jas. Neuplatňuje sa pri 120 Hz, keďže k dispozícii je celkovo iba jedna extra 60 Hz snímka, takže musí byť čierna, inak by BFI nebolo vôbec aktívne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Pod-snímky shaderov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "VAROVANIE: Rýchle blikanie môže na niektorých displejoch spôsobiť pretrvávanie obrazu. Používate na vlastné riziko // Simuluje základný rolujúci skenovací riadok cez viacero pod-snímok rozdelením obrazovky vertikálne a vykresľovaním každej časti obrazovky podľa počtu pod-snímok."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Vkladá extra shader snímku (snímky) medzi snímky pre prípadné shader efekty navrhnuté pre rýchlejší beh než frekvencia obsahu. Použite iba možnosť určenú pre vašu aktuálnu obnovovaciu frekvenciu displeja. Nepoužívajte pri obnovovacích frekvenciách, ktoré nie sú násobkami 60 Hz, ako 144 Hz, 165 Hz atď. Nekombinujte so Swap Interval > 1, BFI, Frame Delay alebo Sync to Exact Content Framerate. Systémové VRR môžete nechať zapnuté, len nie toto nastavenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Simulácia rolujúceho skenovacieho riadku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "VAROVANIE: Rýchle blikanie môže na niektorých displejoch spôsobiť pretrvávanie obrazu. Používate na vlastné riziko // Simuluje základný rolujúci skenovací riadok cez viacero pod-snímok rozdelením obrazovky vertikálne a vykresľovaním každej časti obrazovky podľa počtu pod-snímok."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Simuluje základný rolujúci skenovací riadok cez viacero pod-snímok rozdelením obrazovky vertikálne a vykresľovaním každej časti obrazovky podľa počtu pod-snímok zhora nadol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Bilineárne Filtrovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Pridá ľahké rozmazanie obrazu pre zjemnenie tvrdých okrajov pixelov. Táto možnosť má veľmi malý vplyv na výkon. Pri použití shaderov by mala byť vypnutá."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Interpolácia obrazu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Vybrať metódu interpolácie obrazu pri škálovaní obsahu pomocou interného IPU. Pri použití videofiltrov poháňané procesorom sa odporúča „bikubická“ alebo „bilineárna“. Táto voľba nemá žiadny vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bikubická"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Bilineárna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Najbližší sused"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Interpolácia obrazu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Určuje metódu interpolácie obrazu, keď je vypnutá funkcia \"Integer Scale (Celočíselné zväčšenie)\". Najmenší vplyv na výkon má interpolácia \"Najbližší sused\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Najbližší sused"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Semi-lineárne"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Auto-Shader Oneskorenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Oneskoriť automatické načítavanie shaderu (v ms). Môže obísť grafické chyby pri používaní softvéru na zachytávania obrazovky."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Použiť videofilter poháňaný CPU. Môže mať veľký dopad na výkon. Niektoré videofiltre môžu fungovať iba s jadrami, ktoré používajú 32-bitovú alebo 16-bitovú hĺbku farby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Aplikuje video filter poháňaný CPU. Môže to byť za vysokú cenu výkonu. Niektoré video filtre môžu fungovať len pre cores používajúce 32-bitové alebo 16-bitové farby. Možno vybrať dynamicky linkované knižnice video filtrov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Aplikuje video filter poháňaný CPU. Môže to byť za vysokú cenu výkonu. Niektoré video filtre môžu fungovať len pre cores používajúce 32-bitové alebo 16-bitové farby. Možno vybrať vstavané knižnice video filtrov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Odstrániť videofilter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Zrušiť všetky aktívne videofiltre poháňané CPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Povoliť celú obrazovku cez výrez na zariadeniach Android a iOS"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
   "Použiť Metal Argument Buffers (vyžaduje reštart)"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS,
   "Pokúsi sa zlepšiť výkon použitím Metal argument bufferov. Niektoré cores to môžu vyžadovať. Toto môže pokaziť niektoré shadery, hlavne na starom hardvéri alebo OS verziách."
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Iba pre CRT displeje. Pokúša sa použiť presné rozlíšenie jadra/hry a obnovovaciu frekvenciu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Prepínanie medzi natívnym a ultraširokým super rozlíšením."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Horizontálne vycentrovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Ak obraz nie je správne vycentrovaný na displeji, môžete prenastaviť tieto možnosti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Horizontálna veľkosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Cyklujte cez tieto možnosti pre úpravu horizontálnych nastavení a zmenu veľkosti obrazu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_VERTICAL_ADJUST,
   "Vertikálne vycentrovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_VERTICAL_ADJUST,
   "Ak obraz nie je správne vycentrovaný na displeji, môžete prenastaviť tieto možnosti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Použiť menu vo vysokom rozlíšení"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Prepnúť na modeline s vysokým rozlíšením pre použitie s menu vo vysokom rozlíšení, keď nie je načítaný žiaden obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Vlastná obnovovacia frekvencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "V prípade potreby použite vlastnú obnovovaciu frekvenciu uvedenú v konfiguračnom súbore."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Vybrať obrazovku, ktorá sa použije."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Aký monitor preferovať. 0 (predvolené) znamená, že žiaden konkrétny monitor nie je preferovaný; 1 a vyššie (1 je prvý monitor) odporúča RetroArchu používať konkrétny monitor."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Optimalizovať pre Wii U GamePad (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Použiť presnú 2x mierku GamePadu ako viewport. Vypnite pre zobrazenie v natívnom TV rozlíšení."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Video Rotácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Vynúti určitú rotáciu videa. Rotácia sa pridá k rotáciám, ktoré nastavuje jadro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Orientácia Obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Vynúti určitú orientáciu obrazovky z operačného systému."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Vybrať, ktorú grafickú kartu použiť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Vodorovný posun obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Vynúti určitý horizontálny posun videa. Posun sa aplikuje globálne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Zvislý posun obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Vynúti určitý vertikálny posun videa. Posun sa aplikuje globálne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Vertikálna Obnovovacia Frekvencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Vertikálna obnovovacia frekvencia obrazovky. Použije sa na výpočet vhodného vzorkovania vstupu audia.\nIgnorované, ak je povolené 'Vláknové video'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Odhadovaná Miera Obnovenia Obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Odhadovaná obnovovacia frekvencia obrazovky v Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "Presná obnovovacia frekvencia vášho monitora (Hz). Používa sa na výpočet vstupnej frekvencie zvuku podľa vzorca:\naudio_input_rate = vstupná frekvencia hry * obnovovacia frekvencia displeja / obnovovacia frekvencia hry\nAk core neoznámi žiadne hodnoty, pre kompatibilitu sa predpokladajú predvolené hodnoty NTSC.\nTáto hodnota by mala zostať blízko 60 Hz, aby sa predišlo veľkým zmenám výšky tónu. Ak váš monitor nebeží na alebo blízko 60 Hz, vypnite VSync a túto hodnotu[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Nastaviť obnovovaciu frekvenciu hlásenú obrazovkou"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "Obnovovacia frekvencia hlásená ovládačom obrazu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Automatické prepnutie obnovovacej frekvencie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Automaticky prepne obnovovaciu frekvenciu obrazovky podľa aktuálneho obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Iba v exkluzívnom režime celej obrazovky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Iba v okenovom režime celej obrazovky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Všetky celoobrazovkové režimy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Prah PAL automatickej obnovovacej frekvencie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Maximálna obnovovacia frekvencia, ktorá sa má považovať za PAL."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Vertikálna obnovovacia frekvencia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Nastaviť vertikálnu obnovovaciu frekvenciu. '50 Hz' umožní plynulé video pre obsah v režime PAL."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Natvrdo zakázať sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Vynúti vypnutie podpory sRGB FBO. Niektoré ovládače Intel OpenGL na Windows majú video problémy so sRGB FBO. Povolenie môže problém obísť."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Režim celej obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Zobraziť na celú obrazovku. Možno zmeniť za behu. Možno prepísať parametrom príkazového riadku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Maximalizované okno"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Ak v režime na celú obrazovku, uprednostniť okno na celú obrazovku a zabrániť prepínaniu zobrazovacích režimov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Šírka na celú obrazovku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Nastaviť vlastnú šírku pre celoobrazovkový režim bez okna. Prázdne použije rozlíšenie pracovnej plochy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Výška na celú obrazovku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Nastaviť vlastnú výšku pre celoobrazovkový režim bez okna. Prázdne použije rozlíšenie pracovnej plochy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Vynútiť rozlíšenie na UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Vynútiť rozlíšenie na celú obrazovku, ak nastavené na 0, použije sa pevná hodnota 3840 x 2160."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Škálovanie okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Nastaviť veľkosť okna na zadaný násobok veľkosti viewportu core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Priehľadnosť okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Nastaviť priehľadnosť okna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Zobraziť dekorácie okien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Zobraziť záhlavie a okraje okna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Zobraziť panel ponuky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Zobraziť panel menu okna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Zapamätať pozíciu a veľkosť okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Zobraziť všetok obsah v okne s pevnou veľkosťou určenou hodnotami 'Šírka okna' a 'Výška okna' a uložiť aktuálnu veľkosť a polohu okna pri zatvorení RetroArchu. Keď je vypnuté, veľkosť okna sa nastaví dynamicky na základe 'Mierka okna'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Použiť vlastnú veľkosť okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Zobraziť všetok obsah v okne s pevnou veľkosťou určenou hodnotami 'Šírka okna' a 'Výška okna'. Keď je vypnuté, veľkosť okna sa nastaví dynamicky na základe 'Mierka okna'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Šírka okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Nastaviť vlastnú šírku okna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Výška okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Nastaviť vlastnú výšku okna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Maximálna šírka okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Nastaví maximálnu šírku zobrazovacieho okna pri automatickej zmene veľkosti na základe 'Mierka okna'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Maximálna výška okna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Nastaví maximálnu výšku zobrazovacieho okna pri automatickej zmene veľkosti na základe 'Mierka okna'."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Celočíselné škálovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Škálovať video iba v celočíselných krokoch. Základná veľkosť závisí od geometrie a pomeru strán hlásených core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "Os celočíselnej mierky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Škálovať buď výšku alebo šírku, alebo obe. Polovičné kroky sa uplatňujú iba na zdroje s vysokým rozlíšením."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Spôsob celočíselnej mierky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Zaokrúhliť nadol alebo nahor na najbližšie celé číslo. 'Smart' prejde na podškálovanie, keď je obraz príliš orezaný, a nakoniec sa vráti k nececelému škálovaniu, ak sú podškálovacie okraje príliš veľké."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Inteligentné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Pomer strán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Nastaviť pomer strán displeja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Nastavený pomer strán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Hodnota s pohyblivou desatinnou čiarkou pre pomer strán videa (šírka / výška)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Konfigurácia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Poskytnuté jadrom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Plné"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Zachovať pomer strán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Zachovať pomer pixelov 1:1 pri škálovaní obsahu cez interný IPU. Keď je vypnuté, obrazy sa natiahnu na vyplnenie celého displeja."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Vlastný pomer strán (pozícia X)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Posun vlastného viewportu používaný na definovanie polohy viewportu na osi X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Vlastný pomer strán (pozícia Y)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Posun vlastného viewportu používaný na definovanie polohy viewportu na osi Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Sklon kotvy viewportu X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Sklon kotvy viewportu X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Sklon kotvy viewportu Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Sklon kotvy viewportu Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Horizontálna poloha obsahu, keď je viewport širší než šírka obsahu. 0,0 je úplne vľavo, 0,5 je v strede, 1,0 je úplne vpravo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Vertikálna poloha obsahu, keď je viewport vyšší než výška obsahu. 0,0 je hore, 0,5 je v strede, 1,0 je dole."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Sklon kotvy viewportu X (orientácia na výšku)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Sklon kotvy viewportu X (orientácia na výšku)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Sklon kotvy viewportu Y (orientácia na výšku)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Sklon kotvy viewportu Y (orientácia na výšku)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Horizontálna poloha obsahu, keď je viewport širší než šírka obsahu. 0,0 je úplne vľavo, 0,5 je v strede, 1,0 je úplne vpravo. (Orientácia na výšku)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Vertikálna poloha obsahu, keď je viewport vyšší než výška obsahu. 0,0 je hore, 0,5 je v strede, 1,0 je dole. (Orientácia na výšku)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Vlastný pomer strán (šírka)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Šírka priezoru ak je vybraná možnosť 'Vlastný pomer strán'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Vlastný pomer strán (výška)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Vlastná výška viewportu, ktorá sa použije, ak je Pomer strán nastavený na 'Vlastný pomer strán'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Orezať overscan (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Odreže niekoľko pixelov okolo okrajov obrazu, ktoré vývojári zvyčajne nechávajú prázdne a ktoré niekedy obsahujú aj odpadové pixely."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Nastaví režim výstupu HDR, ak ho displej podporuje. Poznámka: scRGB môže zjemniť prísne masky CRT shaderov, pretože OS kompozítor po aplikácii masky konvertuje na HDR10."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MODE_OFF,
   "Vypnúť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HDR_BRIGHTNESS_NITS,
   "Jas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HDR_BRIGHTNESS_NITS,
   "Jas menu v cd/m2 (nity) pri použití HDR displeja. Viditeľné iba ak je HDR povolené v Nastavenia > Video > HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Jas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Nastaví úroveň jasu HDR v nitoch. Použite v kombinácii s nastaveniami fyzického jasu vášho displeja. Ako východiskový bod nastavte na 80 a jas displeja na maximum. Alternatívne nastavte na max nity vášho displeja a znížte jas displeja, kým nebude obraz vyzerať správne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Zvýraznenie farieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Použije plný farebný rozsah vášho displeja na vytvorenie jasnejšieho a saturovanejšieho obrazu. Pre farby vernejšie pôvodnému dizajnu hry nastavte na Accurate."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_ACCURATE,
   "Presné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_EXPANDED,
   "Rozšírené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_WIDE,
   "Široké"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SCANLINES,
   "Viditeľné riadky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SCANLINES,
   "Povolí HDR scanlines. Scanlines sú hlavným dôvodom pre použitie HDR v RetroArchu, keďže presná implementácia scanline vypína väčšinu obrazovky a HDR obnovuje časť strateného jasu. Ak potrebujete väčšiu kontrolu nad scanlines, pozrite sa na vlastné shadery, ktoré RetroArch poskytuje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "Rozloženie subpixelov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "Vyberte rozloženie subpixelov vášho displeja, toto ovplyvňuje iba scanlines. Ak neviete, aké je rozloženie subpixelov vášho displeja, pozrite Rtings.com pre 'subpixel layout' vášho displeja"
   )


/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Vertikálna synchronizácia (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Synchronizovať video výstup grafickej karty s obnovovacou frekvenciou obrazovky. Odporúčané."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Interval výmeny VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Použiť vlastný interval výmeny pre VSync. Efektívne zníži obnovovaciu frekvenciu monitora o zadaný faktor. 'Auto' nastaví faktor na základe snímkovej frekvencie hlásenej core, čo poskytuje lepšie tempo snímok pri spustení napr. 30 fps obsahu na 60 Hz displeji alebo 60 fps obsahu na 120 Hz displeji."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Adaptívny VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "VSync je povolený, kým výkon neklesne pod cieľovú obnovovaciu frekvenciu. Môže minimalizovať trhanie pri poklese výkonu pod reálny čas a byť energeticky efektívnejší. Nekompatibilné s 'Frame Delay'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCANLINE_SYNC,
   "Synchronizácia scanline"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCANLINE_SYNC,
   "Synchronizuje prezentáciu videa s polohou skenovacieho riadku. Znižuje latenciu za cenu vyššieho rizika tearingu. VSync musí byť vypnutý."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Trvanie snímky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Znižuje latenciu za cenu vyššieho rizika trhania videa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Nastaví, koľko milisekúnd spať pred spustením core po prezentácii videa. Znižuje latenciu za cenu vyššieho rizika trhania.\nHodnoty 20 a vyššie sa považujú za percentá času snímky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Automatické trvanie snímky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Dynamicky upravovať efektívne 'Frame Delay'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Pokúsi sa udržať požadovaný cieľ 'Frame Delay' a minimalizovať vynechané snímky. Východiskový bod je 3/4 času snímky, keď je 'Frame Delay' 0 (Auto)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "efektívne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Pevná synchronizácia GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Napevno synchronizovať CPU a GPU. Znižuje latenciu na úkor výkonu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Snímky pre pevnú synchronizáciu GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Nastaví, koľko snímok môže CPU bežať pred GPU pri použití 'Tvrdá synchronizácia GPU'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Nastaví, koľko snímok môže CPU bežať pred GPU pri použití 'GPU Hard Sync'. Maximum je 3.\n 0: Synchronizovať s GPU okamžite.\n 1: Synchronizovať s predchádzajúcou snímkou.\n 2: Atď ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Synchronizovať na presnú rýchlosť sníkov obsahu (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Žiadna odchýlka od časovania požadovaného core. Použite pre obrazovky s premenlivou obnovovacou frekvenciou (G-Sync, FreeSync, HDMI 2,1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Synchronizovať s presnou frekvenciou obsahu. Táto možnosť je ekvivalentom vynútenia x1 rýchlosti pri zachovaní možnosti rýchleho prevíjania. Žiadna odchýlka od obnovovacej frekvencie požadovanej core, žiadne dynamické riadenie frekvencie zvuku."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Výstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Zmena nastavení audio výstupu."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Mikrofón"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Zmení nastavenia vstupu zvuku."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Synchronizácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Zmení nastavenia synchronizácie zvuku."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Zmeniť MIDI nastavenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Zmiešavač"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Zmení nastavenia mixéra zvuku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Zvuky ponuky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Zmení nastavenia zvuku menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Stlmiť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Stlmiť zvuk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Stlmiť mixér"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Stlmiť zvuk zmiešavača."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Rešpektovať tichý režim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Stlmiť všetok zvuk v tichom režime."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Stlmenie zvuku pri rýchlom prevíjaní"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Automatické stlmenie zvuku pri použití rýchleho posunu vpred."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Zrýchlenie zvuku pri rýchlom prevíjaní"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Zrýchli zvuk pri rýchlom prevíjaní vpred. Predchádza praskaniu, ale posúva výšku tónu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Stlmenie zvuku pri spätnom prevíjaní"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Automaticky stlmí zvuk pri použití spätného prevíjania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Zosilnenie hlasitosti (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Hlasitosť zvuku (v dB). 0 dB je normálna hlasitosť a nepoužíva sa pri nej žiadne zosilnenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Hlasitosť zvuku vyjadrená v dB. 0 dB je normálna hlasitosť, kde sa neaplikuje žiadne zosilnenie. Zosilnenie možno za behu ovládať pomocou Vstup hlasitosť hore / Vstup hlasitosť dole."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Zosilnenie hlasitosti zmiešavača (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Globálna hlasitosť zmiešavača zvuku (v dB). 0 dB je normálna hlasitosť a nepoužíva sa pri nej žiadne zosilnenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "DPS zásuvný modul"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Zásuvný modul DSP zvuku, ktorý spracováva zvuk pred jeho odoslaním do ovládača."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Odstrániť DSP rozšírenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Odobrať aktívne zvukové DSP rozšírenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI exkluzívny mód"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Umožniť ovládaču WASAPI prevziať výhradnú kontrolu nad zvukovým zariadením. Ak je vypnuté, použije namiesto toho zdieľaný režim."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Formát WASAPI s pohyblivou desatinnou čiarkou"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Použije formát s pohyblivou desatinnou čiarkou pre ovládač WASAPI, ak ho vaše zvukové zariadenie podporuje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Dĺžka zdieľanej vyrovnávacej pamäte WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Veľkosť medzipamäte (v rámcoch) pri použití ovládača WASAPI v zdieľanom režime."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ASIO_CONTROL_PANEL,
   "Otvoriť ovládací panel ASIO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ASIO_CONTROL_PANEL,
   "Otvorí ovládací panel ovládača ASIO na konfiguráciu smerovania zariadenia a nastavení bufferov."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Zvuk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Povoliť výstup zvuku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Zariadenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Prepísať predvolené zvukové zariadenie, ktoré používa ovládač zvuku. Závisí od ovládača zvuku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Prepísať predvolené zvukové zariadenie, ktoré používa ovládač zvuku. Závisí od ovládača zvuku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Vlastná hodnota PCM zariadenia pre ovládač ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Vlastná hodnota cesty pre ovládač OSS (napr. /dev/dsp)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Vlastná hodnota názvu portu pre ovládač JACK (napr. system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Vlastná IP adresa servera RSound pre ovládač RSound."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Oneskorenie zvuku (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Maximálna latencia zvuku v milisekundách. Ovládač sa snaží udržať skutočnú latenciu na 50 % tejto hodnoty. Nemusí byť dodržané, ak ovládač zvuku nedokáže poskytnúť danú latenciu."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Mikrofón"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Povolí vstup zvuku v podporovaných cores. Nemá žiadnu réžiu, ak core nepoužíva mikrofón."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Zariadenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Prepíše predvolené vstupné zariadenie, ktoré používa ovládač mikrofónu. Závisí od ovládača."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Prepíše predvolené vstupné zariadenie, ktoré používa ovládač mikrofónu. Závisí od ovládača."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Kvalita prevzorkovania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Znížením tejto hodnoty uprednostníte výkon/nižšiu latenciu pred kvalitou zvuku, zvýšením získate lepšiu kvalitu zvuku na úkor výkonu/nižšej latencie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Predvolená vstupná frekvencia (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Vzorkovacia frekvencia vstupu zvuku, použije sa, ak core nepožaduje konkrétnu hodnotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Latencia vstupu zvuku (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Požadovaná latencia vstupu zvuku v milisekundách. Nemusí byť dodržané, ak ovládač mikrofónu nedokáže poskytnúť danú latenciu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "WASAPI exkluzívny mód"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Umožní RetroArchu prevziať exkluzívnu kontrolu nad zariadením mikrofónu pri použití ovládača mikrofónu WASAPI. Pri vypnutí RetroArch použije zdieľaný režim."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Formát WASAPI s pohyblivou desatinnou čiarkou"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Použiť vstup s pohyblivou desatinnou čiarkou pre ovládač WASAPI, ak ho vaše zvukové zariadenie podporuje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Dĺžka zdieľanej vyrovnávacej pamäte WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Veľkosť medzipamäte (v rámcoch) pri použití ovládača WASAPI v zdieľanom režime."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Kvalita prevzorkovania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Znížením tejto hodnoty uprednostníte výkon/nižšiu latenciu pred kvalitou zvuku, zvýšením získate lepšiu kvalitu zvuku na úkor výkonu/nižšej latencie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Výstupná frekvencia (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Výstupná vzorkovacia frekvencia zvuku."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Synchronizácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Synchronizuje zvuk. Odporúča sa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Maximálny časový posun"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Maximálna frekvenčná odchýlka zvukového signálu. Zvýšenie hodnoty výrazne ovplyvňuje zmeny časovania, ale výsledkom je nepresná výška tónu (napríklad pri spustení obsahu PAL na obrazovkách NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Maximálne skreslenie časovania zvuku.\nDefinuje maximálnu zmenu vstupnej frekvencie. Túto hodnotu môžete chcieť zvýšiť pre veľmi veľké zmeny v časovaní, napríklad pri spustení PAL cores na NTSC displejoch, za cenu nepresnej výšky tónu zvuku.\nVstupná frekvencia je definovaná ako:\nvstupná frekvencia * (1,0 +/- (max skreslenie časovania))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Dynamické regulovanie rýchlosti zvuku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Pomáha odstrániť nedokonalosti v načasovaní pri synchronizácii zvuku a videa. Uvedomte si, že ak je vypnutá, správnu synchronizáciu je takmer nemožné dosiahnuť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Nastavenie tejto hodnoty na 0 vypne kontrolu frekvencie. Akákoľvek iná hodnota ovláda deltu kontroly frekvencie zvuku.\nDefinuje, ako veľmi sa môže vstupná frekvencia dynamicky upraviť. Vstupná frekvencia je definovaná ako:\nvstupná frekvencia * (1,0 +/- (delta kontroly frekvencie))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Vstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Zvoliť vstupné zariadenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Nastaví vstupné zariadenie (špecifické pre ovládač). Pri nastavení na 'Vypnuté' bude MIDI vstup vypnutý. Možno tiež zadať názov zariadenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Výstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Zvoliť výstupné zariadenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Nastaví výstupné zariadenie (špecifické pre ovládač). Pri nastavení na 'Vypnuté' bude MIDI výstup vypnutý. Možno tiež zadať názov zariadenia.\nKeď je MIDI výstup povolený a core a hra/aplikácia podporujú MIDI výstup, niektoré alebo všetky zvuky (závisí od hry/aplikácie) budú generované MIDI zariadením. V prípade ovládača 'null' MIDI to znamená, že tieto zvuky nebudú počuteľné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Hlasitosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Nastaviť hlasitosť výstupu (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Mixér prúd #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Spustiť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Spustí prehrávanie audio streamu. Po dokončení odstráni aktuálny zvukový tok z pamäte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Prehrávať (v slučke)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Spustí prehrávanie audio streamu. Po skončení prehrávania vytvorí slučku a prehráva skladbu znova od začiatku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Prehrávať (v poradí)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Spustí prehrávanie audio streamu. Po dokončení prejde na ďalší zvukový tok v postupnom poradí a toto správanie zopakuje. Použiteľné ako režim prehrávania albumu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Zastaviť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Toto zastaví prehrávanie audio streamu, ale neodstráni ho z pamäte. Môžete ho znova spustiť výberom položky „Prehrať“."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Odstrániť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Toto zastaví prehrávanie audio streamu a úplne ho odstráni z pamäte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Hlasitosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Nastavuje hlasitosť audio streamu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Stav: N/A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Stav: Zastavené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Stav: Hrá sa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Stav: Prehráva sa (slučka)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Stav: Prehráva sa (sekvenčne)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Zmiešavač"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Prehrávať súčasne zvukové toky aj v ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Povoliť zvukový efekt potvrdenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Povoliť zvukový efekt zrušenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Povoliť zvukový efekt oznámenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Povoliť zvuk hudby na pozadí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Povoliť zvuky 'Scroll'"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Maximum užívateľov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Maximálny počet užívateľov podporovaný v RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Správanie pri pollingu (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Ovplyvňuje, ako sa v RetroArchu vykonáva polling vstupu. Nastavenie na 'Skoré' alebo 'Neskoré' môže viesť k nižšej latencii, v závislosti od vašej konfigurácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Ovplyvňuje, ako sa vykonáva polling vstupu v RetroArchu.\nSkoré - Polling vstupu sa vykoná pred spracovaním snímky.\nNormálne - Polling vstupu sa vykoná, keď je vyžiadaný.\nNeskoré - Polling vstupu sa vykoná pri prvej požiadavke na stav vstupu na snímku.\nNastavenie na 'Skoré' alebo 'Neskoré' môže viesť k nižšej latencii, v závislosti od vašej konfigurácie. Pri použití netplay sa ignoruje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Zmeniť ovládanie pre toto jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Prepíše zmeny pomocou upravených volieb pre aktuálne jadro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Triediť remapy podľa gamepadu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Remapy sa použijú iba na aktívny gamepad, v ktorom boli uložené."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Automatická konfigurácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Automaticky konfigurovať herné ovládače, pre ktoré existuje profil, štýl Plug-and-Play."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Vypnúť horúce klávesy Windows (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Zachytiť tieto klávesové kombinácie v aplikácii."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Vybrať fyzickú klávesnicu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Použiť toto zariadenie ako fyzickú klávesnicu, nie ako gamepad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Ak RetroArch identifikuje hardvérovú klávesnicu ako určitý druh gamepadu, toto nastavenie možno použiť na vynútenie, aby RetroArch s nesprávne identifikovaným zariadením zaobchádzal ako s klávesnicou.\nMôže to byť užitočné, ak sa snažíte emulovať počítač na nejakom Android TV zariadení a vlastníte aj fyzickú klávesnicu, ktorú možno k zariadeniu pripojiť."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Vstup prídavného senzoru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Povoliť vstup zo senzorov zrýchlenia, naklonenia a osvetlenia, ak ich podporuje súčasný hardvér. Môže mať dopad na výkon a/alebo zvýšiť spotrebu na niektorých platformách."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Automatické zachytenie kurzora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Povoliť zachytenie kurzora myši pri označení aplikácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Automaticky povoliť režim 'Zameranie na hru'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Povolí režim \"Priorita hry\" pri automatickom spustení a obnovení obsahu. Výberom možnosti \"Detegovať\" sa táto možnosť povolí, ak aktuálne jadro implementuje funkciu spätného volania klávesnice."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "ZAP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Zistiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Pozastaviť obsah pri odpojení kontroleru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Pozastaví obsah pri odpojení akéhokoľvek kontroleru. Pokračujte tlačidlom Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Odchýlka osi pri stlačení tlačidla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Ako ďaleko musí byť os naklonená, aby vyústila do stlačenia tlačidla pri použití 'Analóg na digitálne'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Analógová mŕtva zóna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Ignorovať pohyby analógovej páčky pod hodnotou mŕtvej zóny."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Analógová citlivosť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Citlivosť akcelerometra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Citlivosť gyroskopu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Upraví citlivosť analógových páčok."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Upraví citlivosť akcelerometra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ORIENTATION,
   "Orientácia senzora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ORIENTATION,
   "Otočí osi akcelerometra a gyroskopu tak, aby zodpovedali orientácii zariadenia."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Upraví citlivosť gyroskopu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Časový limit párovania tlačidla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Čas v sekundách, kým sa prejde na ďalšie tlačidlo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Čas stlačenia tlačidla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Počet sekúnd, počas ktorých sa má tlačidlo podržať, aby sa spárovalo."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Stlačte klávesu, myš alebo kontroler"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Uvoľnite klávesy a tlačidlá!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Časový limit"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Držať"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Rýchlo streľba"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE,
   "Vypnuté zastaví všetky operácie turbo streľby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Turbo režim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Perióda v snímkach, počas ktorej sú stlačené tlačidlá s povoleným turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DUTY_CYCLE,
   "Pracovný cyklus turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE,
   "Počet snímok z periódy turbo, počas ktorých sú tlačidlá držané. Ak je toto číslo rovnaké alebo vyššie ako perióda turbo, tlačidlá sa nikdy neuvoľnia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF,
   "Polovičná perióda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Turbo mód"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Zvoľte všeobecné správanie režimu Turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Klasické"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Klasický (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Jedno tlačidlo (prepnúť)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Jedno tlačidlo (držať)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Klasický režim, dvojtlačidlová operácia. Podržte tlačidlo a klepnite na tlačidlo turbo na aktiváciu sekvencie stlačenie-uvoľnenie.\nPriradenie turbo možno nastaviť v Nastavenia/Vstup/Ovládače portu X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Klasický prepínací režim, dvojtlačidlová operácia. Podržte tlačidlo a klepnite na tlačidlo turbo na povolenie turbo pre toto tlačidlo. Pre vypnutie turbo: podržte tlačidlo a stlačte tlačidlo turbo znova.\nPriradenie turbo možno nastaviť v Nastavenia/Vstup/Ovládače portu X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Prepínací režim. Stlačte tlačidlo Turbo raz na aktiváciu sekvencie stlačenie-uvoľnenie pre vybrané predvolené tlačidlo, stlačte ho znova na vypnutie.\nPriradenie turbo možno nastaviť v Nastavenia/Vstup/Ovládače portu X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Režim držania. Sekvencia stlačenie-uvoľnenie pre vybrané predvolené tlačidlo je aktívna, kým je držané tlačidlo Turbo.\nPriradenie turbo možno nastaviť v Nastavenia/Vstup/Ovládače portu X.\nPre emuláciu funkcie autofire z éry domácich počítačov nastavte Bind a Tlačidlo na rovnaké strelné tlačidlo joysticku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "Priradenie turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "Priradenie RetroPad pre aktiváciu turbo. Prázdne použije priradenie špecifické pre port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "Tlačidlo Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "Cieľové tlačidlo turbo v režime 'Jedno tlačidlo'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "Turbo povolí smery D-Padu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "Ak je povolené, digitálne smerové vstupy (známe aj ako d-pad alebo 'hatswitch') môžu mať turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Rýchlo streľba"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Zmení nastavenia turbo streľby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Haptická spätná väzba/vibrácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Zmení nastavenia haptickej spätnej väzby a vibrácií."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_SETTINGS,
   "Senzory pohybu/svetla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_SETTINGS,
   "Zmení nastavenia akcelerometra, gyroskopu a osvetlenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Ovládacie prvky ponuky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Zmení nastavenia ovládania menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Klávesové skratky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Zmení nastavenia a priradenia horúcich kláves, ako napríklad prepnutie menu počas hrania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Priradenia RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Zmení, ako je virtuálny RetroPad mapovaný na fyzické vstupné zariadenie. Ak je vstupné zariadenie rozpoznané a správne autokonfigurované, používatelia toto menu pravdepodobne nepotrebujú.\nPoznámka: pre zmeny vstupu špecifické pre core použite radšej podmenu 'Ovládače' v Quick Menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro používa abstrakciu virtuálneho gamepadu známu ako 'RetroPad' na komunikáciu medzi frontendmi (ako RetroArch) a cores a naopak. Toto menu určuje, ako je virtuálny RetroPad mapovaný na fyzické vstupné zariadenia a ktoré virtuálne vstupné porty tieto zariadenia obsadzujú.\nAk je fyzické vstupné zariadenie rozpoznané a správne autokonfigurované, používatelia toto menu pravdepodobne vôbec nepotrebujú a pre zmeny vstupu špecifické pre core by mali použiť podmenu 'Ov[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Ovládače portu %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Zmení, ako je virtuálny RetroPad mapovaný na vaše fyzické vstupné zariadenie pre tento virtuálny port."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Zmení mapovanie vstupu špecifické pre core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Workaround pre odpájanie Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Workaround pre odpájanie a opätovné pripájanie kontrolerov. Bráni hraniu 2 hráčov s identickými kontrolermi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_QUIT,
   "Potvrdiť ukončenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_QUIT,
   "Vyžadovať dvojité stlačenie horúcej klávesy Ukončiť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_CLOSE,
   "Potvrdiť zatvorenie obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_CLOSE,
   "Vyžadovať dvojité stlačenie horúcej klávesy Zatvoriť obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_RESET,
   "Potvrdiť reset obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_RESET,
   "Vyžadovať dvojité stlačenie horúcej klávesy Resetovať obsah."
   )


/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Vibrovať pri stlačení"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Povoliť vibrovanie zariadenia (pre podporované jadrá)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Intenzita vibrácií"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Určuje silu efektov haptickej spätnej väzby."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Zjednotené ovládanie ponuky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Použiť rovnaké ovládanie pre menu aj hru. Týka sa klávesnice."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Prehodiť tlačidlá OK a Zrušiť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Prehodí tlačidlá OK/Zrušiť. Vypnuté je japonská orientácia tlačidiel, povolené je západná orientácia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Prehodiť tlačidlá rolovania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Prehodí tlačidlá pre rolovanie. Pri vypnutí roluje 10 položiek pomocou L/R a abecedne pomocou L2/R2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Všetci používatelia ovládajú ponuku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Umožniť ktorémukoľvek používateľovi ovládať menu. Pri vypnutí môže menu ovládať iba Používateľ 1."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SINGLECLICK_PLAYLISTS,
   "Zoznamy skladieb na jeden klik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SINGLECLICK_PLAYLISTS,
   "Preskočiť menu 'Spustiť' pri spúšťaní položiek zoznamu skladieb. Stlačte D-Pad a držte OK pre prístup k menu 'Spustiť'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ALLOW_TABS_BACK,
   "Povoliť návrat z kariet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ALLOW_TABS_BACK,
   "Vrátiť sa do hlavného menu z kariet/bočného panelu pri stlačení Späť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Akcelerácia rolovania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Maximálna rýchlosť kurzora pri držaní smeru rolovania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Oneskorenie rolovania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Počiatočné oneskorenie v milisekundách pri držaní smeru rolovania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Zakázať tlačidlo Info"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Zabrániť funkcii info menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Zakázať tlačidlo Hľadať"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Zabrániť funkcii vyhľadávania menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Vypnúť ľavú analógovú páčku v menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Zabrániť vstupu ľavej analógovej páčky v menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Vypnúť pravú analógovú páčku v menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Zabrániť vstupu pravej analógovej páčky v menu. Pravá analógová páčka prepína náhľady v zoznamoch skladieb."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Povoliť skratku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Pri priradení musí byť kláves 'Povoliť horúce klávesy' držaná pred rozpoznaním akýchkoľvek iných horúcich kláves. Umožňuje mapovanie tlačidiel kontroleru na funkcie horúcich kláves bez ovplyvnenia normálneho vstupu. Priradenie modifikátora iba na kontroler ho nebude vyžadovať pre klávesové horúce klávesy a naopak, ale oba modifikátory fungujú pre obe zariadenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Ak je táto horúca kláves priradená klávesnici, joybutton alebo joyaxis, všetky ostatné horúce klávesy budú vypnuté, kým nie je táto kláves súčasne držaná.\nToto je užitočné pre RETRO_KEYBOARD-orientované implementácie, ktoré sa dotazujú veľkej oblasti klávesnice, kde nie je žiaduce, aby horúce klávesy zavadzali."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Oneskorenie povolenia horúcej klávesy (snímky)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Pridať oneskorenie v snímkach pred zablokovaním normálneho vstupu po stlačení priradenej klávesy 'Povoliť horúce klávesy'. Umožňuje zachytiť normálny vstup z klávesy 'Povoliť horúce klávesy', keď je mapovaná na inú akciu (napr. RetroPad 'Select')."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Zlúčenie typu zariadenia horúcich kláves"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Zablokovať všetky horúce klávesy z klávesnice aj kontroleru, ak má jeden z typov nastavené 'Povoliť horúce klávesy'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Horúce klávesy nasledujú hráča 1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Horúce klávesy sú priradené portu 1 core, aj keď je port 1 core remapovaný na iného používateľa. Poznámka: klávesové horúce klávesy nebudú fungovať, ak je port 1 core remapovaný na akéhokoľvek používateľa > 1 (klávesový vstup je od používateľa 1)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Prepnutie menu (kombinácia kontroleru)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Kombinácia tlačidiel kontroleru na prepnutie menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Prepnutie ponuky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Prepína aktuálne zobrazenie medzi menu a obsahom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Ukončenie (kombinácia kontroleru)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Kombinácia tlačidiel kontroleru na ukončenie RetroArchu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Ukončiť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Ukončí RetroArch, zaistí zapísanie všetky úložných dát a nastavení na disk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Zavrieť obsah"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Zatvorí aktuálny obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Vynulovať obsah"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Reštartuje aktuálny obsah od začiatku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Rýchly posun vpred (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Prepína medzi rýchlym posunom vpred a normálnou rýchlosťou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Rýchly posun vpred (držanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Povolí rýchly posun vpred počas držania. Obsah beží normálnou rýchlosťou pri uvoľnení klávesy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Spomalený pohyb (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Prepína medzi spomaleným pohybom a normálnou rýchlosťou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Spomalený pohyb (držanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Povolí spomalený pohyb počas držania. Obsah beží normálnou rýchlosťou pri uvoľnení klávesy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Previnúť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Pretáča aktuálny obsah späť počas držania klávesy. 'Podpora pretáčania' musí byť povolená."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Pauza"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Prepína obsah medzi pozastaveným a nepozastaveným stavom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Posun o snímku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Posunie obsah o jednu snímku, keď je pozastavený."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Stlmiť zvuk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Zapne/vypne výstup zvuku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Zvýšiť hlasitosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Zvýši úroveň hlasitosti výstupu zvuku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Znížiť hlasitosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Zníži úroveň hlasitosti výstupu zvuku."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Načítať stav"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Načíta uložený stav z aktuálne vybraného slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Uložiť stav"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Uloží stav do aktuálne vybraného slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Ďalší slot na uloženie stavu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Zvýši index aktuálne zvoleného slotu na ukladanie stavu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Predchádzajúci slot na uloženie stavu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Zníži index aktuálne zvoleného slotu na ukladanie stavu."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Vysunutie disku (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Ak je virtuálny zásobník disku zatvorený, otvorí ho a odstráni načítaný disk. Inak vloží aktuálne vybraný disk a zatvorí zásobník."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Ďalší disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Zvýši aktuálne vybraný index disku a vykoná oneskorené vloženie, ak je virtuálny zásobník disku zatvorený."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Predošlý disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Zníži aktuálne vybraný index disku a vykoná oneskorené vloženie, ak je virtuálny zásobník disku zatvorený."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Shadery (prepnúť)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Zapne/vypne aktuálne vybraný shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_HOLD,
   "Shadery (držanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD,
   "Drží aktuálne vybraný shader zapnutý/vypnutý počas stláčania klávesy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Ďalší shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Načíta a aplikuje ďalší súbor predvoľby shaderov v koreni adresára 'Video Shadery'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Predošlý shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Načíta a aplikuje predchádzajúci súbor predvoľby shaderov v koreni adresára 'Video Shadery'."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Cheaty (prepnúť)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Zapne/vypne aktuálne vybraný cheat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Ďalší index cheatu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Zvýši aktuálne vybraný index cheatu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Predchádzajúci index cheatu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Zníži aktuálne vybraný index cheatu."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Urobiť screenshot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Zachytí obraz aktuálneho obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Nahrávanie (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Spustí/zastaví nahrávanie aktuálnej relácie do lokálneho video súboru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Streamovanie (prepnúť)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Spustí/zastaví streamovanie aktuálnej relácie na online video platformu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Prehrať opakovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Prehrať súbor replay z aktuálne vybraného slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Nahrať replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Nahrá replay súbor do aktuálne vybraného slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Zastaviť nahrávanie/prehrávanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Zastaví nahrávanie/prehrávanie aktuálneho replay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Uložiť kontrolný bod replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Vytvorí kontrolný bod v aktuálne prehrávanom replay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Predchádzajúci kontrolný bod replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Pretočí replay späť na predchádzajúci automaticky alebo manuálne uložený kontrolný bod."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Ďalší kontrolný bod replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Pretočí replay vpred na ďalší automaticky alebo manuálne uložený kontrolný bod."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "Ďalší záznamový slot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Zvýši index aktuálne vybraného záznamového slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Predchádzajúci záznamový slot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Zníži index aktuálne vybraného záznamového slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_TURBO_FIRE_TOGGLE,
   "Turbo streľba (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE,
   "Zapne/vypne turbo streľbu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Zachytenie myši (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Zachytí alebo uvoľní myš. Pri zachytení sa systémový kurzor skryje a obmedzí na okno RetroArchu, čo zlepšuje relatívny vstup myši."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Zameranie hry (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Zapne/vypne režim 'Zameranie hry'. Keď má obsah zameranie, horúce klávesy sú vypnuté (úplný klávesový vstup je odovzdávaný spustenému core) a myš je zachytená."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Celá obrazovka (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Prepína medzi režimom na celú obrazovku a zobrazení v okne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Desktop menu (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Otvorí pridružené WIMP (Okná, Ikony, Menu, Ukazovateľ) desktop používateľské rozhranie."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Synchronizácia s presnou frekvenciou obsahu (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Zapne/vypne synchronizáciu s presnou frekvenciou obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Run-Ahead (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Zapne/vypne Run-Ahead."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Preemptívne snímky (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Zapne/vypne preemptívne snímky."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Zobraziť FPS (prepnúť)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Zapne/vypne indikátor stavu 'snímok za sekundu'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Zobraziť technické štatistiky (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Zapne/vypne zobrazenie technických štatistík na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Klávesnicový overlay (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Zapne/vypne klávesnicový overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Ďalšie prekrytie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Prepne na ďalšie dostupné rozloženie aktuálne aktívneho overlay na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Služba AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Zachytí obraz aktuálneho obsahu na preklad a/alebo nahlas prečítanie textu na obrazovke. 'AI Service' musí byť povolený a nakonfigurovaný."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Netplay ping (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Zapne/vypne počítadlo pingu pre aktuálnu netplay miestnosť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Hostovanie netplay (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Zapne/vypne hostovanie netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Netplay režim hrania/divák (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Prepne aktuálnu netplay reláciu medzi režimom 'hranie' a 'divák'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Netplay chat hráča"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Pošle chatovú správu do aktuálnej netplay relácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Netplay miznúci chat (prepínanie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Prepína medzi miznúcimi a statickými netplay chatovými správami."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Typ zariadenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Určuje typ emulovaného kontroleru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Typ analógového na digitálne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Použije zadanú analógovú páčku pre vstup D-Padu. Režimy 'Forced' prepíšu natívny analógový vstup core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Mapuje zadanú analógovú páčku pre vstup D-Padu.\nAk má core natívnu podporu analógu, mapovanie D-Padu bude vypnuté, pokiaľ nie je vybraná možnosť '(Forced)'.\nAk je mapovanie D-Padu vynútené, core nedostane žiaden analógový vstup zo zadanej páčky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Číslo zariadenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "Fyzický kontroler rozpoznaný RetroArchom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Zariadenie rezervované pre tohto hráča"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Tento kontroler bude pridelený tomuto hráčovi podľa režimu rezervácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "Žiadna rezervácia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "Preferované"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "Rezervované"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "Typ rezervácie zariadenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "Preferred: ak je zadané zariadenie prítomné, bude pridelené tomuto hráčovi. Reserved: žiaden iný kontroler nebude pridelený tomuto hráčovi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Mapovaný port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Určuje, ktorý port core dostane vstup z portu kontroleru frontendu %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Nastaviť všetko ovládanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Priraďte všetky smery a tlačidlá jeden po druhom v poradí, v akom sa zobrazujú v tomto menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Vynulovať na predvolené ovládanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Vymaže nastavenia priradenia vstupu na ich predvolené hodnoty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Uložiť profil kontroleru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Uloží súbor autokonfigurácie, ktorý sa automaticky aplikuje vždy, keď je tento kontroler znova rozpoznaný."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Číslo myši"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "Fyzická myš rozpoznaná RetroArchom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Tlačidlo B (dolu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Tlačidlo Y (vľavo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Tlačidlo Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Tlačidlo Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "Smer. ovl. nahor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "Smer. ovl. nadol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "Smer. ovl. doľava"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "Smer. ovl. doprava"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Tlačidlo A (vpravo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Tlačidlo X (hore)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "L tlačidlo (rameno)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "R tlačidlo (rameno)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "R2 tlačidlo (spúšťač)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2 tlačidlo (spúšťač)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "L3 tlačidlo (palec)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "R3 tlačidlo (palec)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Ľavý analóg X+ (vpravo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Ľavý analóg X- (vľavo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Ľavý analóg Y+ (dolu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Ľavý analóg Y- (hore)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Pravý analóg X+ (vpravo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Pravý analóg X- (vľavo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Pravý analóg Y+ (dolu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Pravý analóg Y- (hore)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Spúšťač zbrane"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Nabitie zbrane"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Pomocná zbraň A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Pomocná zbraň B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Pomocná zbraň C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Spustenie zbrane"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Výber zbrane"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Zbraň D-Pad hore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Zbraň D-Pad dole"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Zbraň D-Pad vľavo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Zbraň D-Pad vpravo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO,
   "Rýchlo streľba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOLD,
   "Držať"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Run-Ahead nedostupné]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "Aktuálny core je nekompatibilný s run-ahead kvôli chýbajúcej podpore deterministického save state."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Počet snímok pre Run-Ahead"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Počet snímok dopredu. Spôsobuje problémy v hraní ako jitter, ak sa prekročí počet vnútorných lag snímok hry."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "Spustí dodatočnú logiku core na zníženie latencie. Single Instance beží na budúcu snímku, potom načíta aktuálny stav. Second Instance udržuje video-only inštanciu core na budúcej snímke, aby sa predišlo problémom so stavom zvuku. Preemptive Frames v prípade potreby spúšťa minulé snímky s novým vstupom kvôli efektivite."
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Spustí dodatočnú logiku core na zníženie latencie. Single Instance beží na budúcu snímku, potom načíta aktuálny stav. Preemptive Frames v prípade potreby spúšťa minulé snímky s novým vstupom kvôli efektivite."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "Režim Single Instance"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "Režim Second Instance"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   "Režim Preemptive Frames"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Skryť varovania Run-Ahead"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Skryje varovaciu správu, ktorá sa zobrazuje pri použití Run-Ahead, keď core nepodporuje save states."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Počet preemptívnych snímok"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Počet snímok na opätovné spustenie. Spôsobuje problémy v hraní ako jitter, ak sa prekročí počet vnútorných lag snímok hry."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Hardvérový zdieľaný kontext"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Dať hardvérovo renderovaným cores ich vlastný súkromný kontext. Predchádza nutnosti predpokladať zmeny stavu hardvéru medzi snímkami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Povoliť cores prepínať video ovládač"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Umožní cores prepnúť na iný video ovládač než aktuálne načítaný."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Načítať dummy core pri ukončení core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Niektoré cores majú funkciu vypnutia, načítanie dummy core zabráni vypnutiu RetroArchu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Niektoré cores môžu mať funkciu vypnutia. Ak je táto možnosť ponechaná vypnutá, výber procedúry vypnutia by spôsobil vypnutie RetroArchu.\nPovolenie tejto možnosti namiesto toho načíta dummy core, takže zostaneme v menu a RetroArch sa nevypne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Spustiť jadro automaticky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Kategórie možností core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Umožní cores prezentovať možnosti v podmenu založených na kategóriách. POZNÁMKA: Pre aplikovanie zmien sa musí core znova načítať."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Cache info súborov core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Udržiavať trvalú lokálnu cache informácií o nainštalovaných cores. Výrazne znižuje časy načítania na platformách s pomalým prístupom k disku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Obísť funkcie save states z info core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Určuje, či sa majú ignorovať schopnosti save state z info core, čo umožňuje experimentovať so súvisiacimi funkciami (run ahead, rewind, atď.)."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Vždy znova načítať core pri spustení obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Reštartovať RetroArch pri spustení obsahu, aj keď je požadovaný core už načítaný. Môže to zlepšiť stabilitu systému za cenu dlhších časov načítania."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Povoliť otáčanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Umožní cores nastaviť rotáciu. Pri vypnutí sa požiadavky na rotáciu ignorujú. Užitočné pre nastavenia, ktoré obrazovku otáčajú manuálne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Spravovať jadrá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Vykonať offline údržbu na nainštalovaných cores (záloha, obnova, mazanie atď.) a zobraziť informácie o core."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Spravovať jadrá"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Inštalovať alebo odinštalovať cores distribuované cez Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Nainštalovať jadro"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Odinštalovať jadro"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "Zobraziť 'Spravovať jadrá'"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Zobraziť možnosť 'Spravovať cores' v hlavnom menu."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Inštalujem jadro: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "Core sa odinštaluje pri ukončení RetroArchu."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "Jadro sa práve sťahuje"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Uložiť nastavenie pri ukončení"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Pri ukončení uložiť zmeny do súboru s nastaveniami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Uložiť zmeny do konfiguračného súboru pri ukončení. Užitočné pre zmeny vykonané v menu. Prepíše konfiguračný súbor, #include's a komentáre nie sú zachované."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_MINIMAL,
   "Uložiť minimálnu konfiguráciu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_MINIMAL,
   "Uložiť iba nastavenia, ktoré sa líšia od predvolených."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_MINIMAL,
   "Keď je povolené, ukladajú sa iba konfiguračné hodnoty, ktoré sa zmenili oproti predvoleným hodnotám. Výsledkom je menší a lepšie spravovateľný konfiguračný súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Uložiť premapovanie pri ukončení"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Uloží zmeny do akéhokoľvek aktívneho input remap súboru pri zatváraní obsahu alebo ukončení RetroArchu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Automaticky načítať možnosti core špecifické pre obsah"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Načíta vlastné možnosti core v predvolenom nastavení pri štarte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Automaticky načítať override súbory"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Načíta vlastnú konfiguráciu pri štarte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Automaticky načítať remap súbory"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Načíta vlastné ovládanie pri štarte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Automaticky načítať počiatočné indexy diskov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "Pri spustení obsahu s viacerými diskami prepnúť na naposledy použitý disk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Automaticky načítať predvoľby shaderov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Použiť globálny súbor možností core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Uloží všetky možnosti core do spoločného súboru nastavení (retroarch-core-options.cfg). Pri vypnutí sa možnosti pre každý core uložia do samostatného priečinka/súboru špecifického pre core v adresári 'Configs' RetroArchu."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Súbor uloženia: triediť do priečinkov podľa názvu core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Triediť súbory uloženia do priečinkov pomenovaných podľa použitého core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Save state: triediť do priečinkov podľa názvu core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Triediť save states do priečinkov pomenovaných podľa použitého core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Súbor uloženia: triediť do priečinkov podľa adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Triediť súbory uloženia do priečinkov pomenovaných podľa adresára, v ktorom sa obsah nachádza."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Save state: triediť do priečinkov podľa adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Triediť save states do priečinkov pomenovaných podľa adresára, v ktorom sa obsah nachádza."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Súbor uloženia: neprepisovať SaveRAM pri načítaní save state"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Zabráni prepísaniu SaveRAM pri načítaní save states. Môže to viesť k chybám v hrách."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Súbor uloženia: interval automatického ukladania SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Automaticky uložiť non-volatile SaveRAM v pravidelnom intervale (v sekundách)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Automaticky uloží non-volatile SRAM v pravidelnom intervale. V predvolenom nastavení je vypnuté, ak nie je nastavené inak. Interval sa meria v sekundách. Hodnota 0 vypne automatické ukladanie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Replay: interval kontrolných bodov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Automaticky vytvárať záložky stavu hry počas nahrávania replay v pravidelnom intervale (v sekundách)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Automaticky uloží stav hry počas nahrávania replay v pravidelnom intervale. V predvolenom nastavení je vypnuté, ak nie je nastavené inak. Interval sa meria v sekundách. Hodnota 0 vypne nahrávanie kontrolných bodov."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_DESERIALIZE,
   "Či sa majú deserializovať kontrolné body uložené v replay počas bežného prehrávania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_DESERIALIZE,
   "Replay: deserializácia kontrolných bodov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_DESERIALIZE,
   "Či sa majú deserializovať kontrolné body uložené v replay počas bežného prehrávania. Pre väčšinu cores by malo byť zapnuté, niektoré však pri deserializácii obsahu môžu vykazovať trhané správanie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Save state: automaticky zvyšovať index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Pred vytvorením save state sa index save state automaticky zvýši. Pri načítaní obsahu sa index nastaví na najvyšší existujúci index."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Replay: automaticky zvyšovať index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Pred vytvorením replay sa index replay automaticky zvýši. Pri načítaní obsahu sa index nastaví na najvyšší existujúci index."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Save state: maximálny počet auto-zvýšení na uchovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Obmedzí počet save states, ktoré sa vytvoria pri zapnutej možnosti 'Automaticky zvyšovať index'. Ak sa pri ukladaní nového stavu prekročí limit, existujúci stav s najnižším indexom sa odstráni. Hodnota '0' znamená, že sa zaznamenajú neobmedzené stavy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Replay: maximálny počet auto-zvýšení na uchovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Obmedzí počet replays, ktoré sa vytvoria pri zapnutej možnosti 'Automaticky zvyšovať index'. Ak sa pri nahrávaní nového replay prekročí limit, existujúci replay s najnižším indexom sa odstráni. Hodnota '0' znamená, že sa zaznamenajú neobmedzené replays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Save state: automatické ukladanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Automaticky vytvorí save state pri zatvorení obsahu. Tento save state sa načíta pri štarte, ak je zapnutá možnosť 'Auto Load'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Save state: automatické načítanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Automaticky načíta auto save state pri štarte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Save state: náhľady"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Zobraziť náhľady save states."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Súbor uloženia: kompresia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Zapisovať non-volatile SaveRAM súbory v archivovanom formáte. Drasticky znižuje veľkosť súboru za cenu (zanedbateľne) zvýšeného času ukladania/načítania.\nPlatí len pre cores, ktoré umožňujú ukladanie cez štandardné libretro SaveRAM rozhranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Save state: kompresia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Zapisovať súbory save state v archivovanom formáte. Drasticky znižuje veľkosť súboru za cenu zvýšeného času ukladania/načítania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Súbor uloženia: zapisovať do adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Použiť adresár obsahu ako adresár pre súbory uloženia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Save state: zapisovať do adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Použiť adresár obsahu ako adresár pre save state."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Systémové súbory sú v adresári obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Použiť adresár obsahu ako System/BIOS adresár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Screenshot: triediť do priečinkov podľa adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Triediť screenshoty do priečinkov pomenovaných podľa adresára, v ktorom sa obsah nachádza."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Screenshot: zapisovať do adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Použiť adresár obsahu ako adresár screenshotov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Screenshot: použiť GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Snímka obrazovky zachytená z tieňového materiálu GPU, ak je k dispozícii."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Uložiť log času behu (na core)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Sledovať, ako dlho každý kúsok obsahu bežal, so záznamami oddelenými podľa core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Uložiť log času behu (súhrnne)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Sledovať, ako dlho každý kúsok obsahu bežal, zaznamenané ako súhrnný celkový čas naprieč všetkými cores."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Podrobnosť záznamu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Zaznamenávať udalosti do terminálu alebo súboru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Úroveň záznamu frontendu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Nastavte úroveň logovania pre frontend. Ak je úroveň logu vydaná frontendom pod touto hodnotou, ignoruje sa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Úroveň záznamu jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Nastavte úroveň logovania pre cores. Ak je úroveň logu vydaná core pod touto hodnotou, ignoruje sa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Nastaví úroveň logovania pre libretro cores (GET_LOG_INTERFACE). Ak je úroveň logu vydaná libretro core pod úrovňou libretro_log, ignoruje sa. DEBUG logy sa vždy ignorujú, pokiaľ nie je aktivovaný verbose režim (--verbose).\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (ladenie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (info)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (upozornenie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (chyba)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Zaznamenať do súboru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Presmeruje správy systémových udalostí do súboru. Vyžaduje povolené 'Verbozita logovania'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Časové pečiatky log súborov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Pri logovaní do súboru presmeruje výstup z každej relácie RetroArchu do nového časovo-pečiatkovaného súboru. Ak je vypnuté, log sa prepíše pri každom reštarte RetroArchu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Počítadlá výkonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Počítadlá výkonu pre RetroArch a cores. Údaje počítadiel môžu pomôcť určiť úzke miesta systému a doladiť výkon."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Zobraziť skryté súbory a adresáre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Zobraziť skryté súbory a adresáre v prehliadači súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtrovať neznáme prípony"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtrovať súbory zobrazené v prehliadači súborov podľa podporovaných prípon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Filtrovať podľa aktuálneho core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "Filtrovať súbory zobrazené v prehliadači súborov podľa aktuálneho core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Pamätať si naposledy použitý počiatočný adresár"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Otvoriť prehliadač súborov v naposledy použitom umiestnení pri načítaní obsahu z počiatočného adresára. Poznámka: umiestnenie sa pri reštarte RetroArchu obnoví na predvolené."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SUGGEST_ALWAYS,
   "Vždy navrhovať cores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS,
   "Navrhne dostupné cores aj keď je core manuálne načítaný."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Použiť vstavaný prehrávač médií"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_BUILTIN_PLAYER,
   "Zobraziť v prehliadači súborov súbory podporované prehrávačom médií."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Použiť vstavaný prehliadač obrázkov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_BUILTIN_IMAGE_VIEWER,
   "Zobraziť v prehliadači súborov súbory podporované prehliadačom obrázkov."
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Previnúť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_SETTINGS,
   "Zmení nastavenia spätného prevíjania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Počítadlo času snímky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Zmení nastavenia ovplyvňujúce počítadlo času snímky.\nAktívne iba pri vypnutom vláknovom videu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Frekvencia rýchleho posunu vpred"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Maximálna frekvencia, akou sa obsah spustí pri rýchlom posune vpred (napr. 5,0x pre obsah 60 fps = limit 300 fps). Pri nastavení 0,0x je pomer rýchleho posunu vpred neobmedzený (žiadny limit FPS)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "Maximálna frekvencia, akou sa obsah spustí pri rýchlom posune vpred. (Napr. 5,0 pre obsah 60 fps => limit 300 fps).\nRetroArch sa uspí, aby zaistil, že sa neprekročí maximálna frekvencia. Nespoliehajte sa na úplnú presnosť tohto limitu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Vynechávanie snímok rýchleho posunu vpred"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Preskakovať snímky podľa frekvencie rýchleho posunu vpred. Šetrí energiu a umožňuje použitie obmedzovača snímok od tretích strán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Frekvencia spomaleného pohybu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "Frekvencia, akou sa bude obsah prehrávať pri použití spomaleného pohybu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Obmedziť snímkovú frekvenciu menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Zaistí, že snímková frekvencia je obmedzená v menu."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Podpora pretáčania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Vrátiť sa do predchádzajúceho bodu nedávneho hrania. Spôsobuje vážne zníženie výkonu počas hrania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Snímky pretáčania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Počet snímok na pretočenie na krok. Vyššie hodnoty zvyšujú rýchlosť pretáčania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Veľkosť bufferu pretáčania (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "Množstvo pamäte (v MB) rezervované pre buffer pretáčania. Zvýšenie zvýši množstvo histórie pretáčania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Krok veľkosti bufferu pretáčania (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Pri každom zvýšení alebo znížení hodnoty veľkosti bufferu pretáčania sa zmení o túto sumu."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Resetovať po rýchlom posune vpred"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Resetuje počítadlo času snímky po rýchlom posune vpred."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Resetovať po načítaní stavu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Resetuje počítadlo času snímky po načítaní stavu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Resetovať po uložení stavu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Resetuje počítadlo času snímky po uložení stavu."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Kvalita nahrávania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Nízke"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "Stredné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Vysoké"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Bezstratové"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM rýchle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM vysoká kvalita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Vlastná konfigurácia nahrávania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Vlákna nahrávania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Použiť nahrávanie po filtroch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Zachytí obraz po aplikácii filtrov (ale nie shaderov). Video bude vyzerať tak pekne ako to, čo vidíte na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Použiť GPU nahrávanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Zaznamenávať výstup GPU shadovaného materiálu, ak je dostupný."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Režim streamovania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_FACEBOOK,
   "Hranie na Facebooku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "Miestne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Kvalita streamu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Nízke"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "Stredné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Vysoké"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Vlastná konfigurácia streamovania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Názov streamu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "URL adresa vysielania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "Port streamu UDP"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Prekrytie na obrazovke"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Upraviť rámčeky a ovládače na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Rozloženie videa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Upraviť rozloženie videa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Upozornenia na obrazovke"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Upraviť upozornenia na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Viditeľnosť upozornení"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Prepínať viditeľnosť konkrétnych typov oznámení."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Zobraziť prekrytie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Overlaye sa používajú pre okraje a ovládače na obrazovke."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Zobraziť overlay za menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Zobraziť overlay za menu, namiesto pred menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Skryť overlay v menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Skryje overlay počas pobytu v menu a opäť ho zobrazí pri opustení menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Skryť overlay pri pripojení kontroleru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Skryje overlay, keď je v porte 1 pripojený fyzický kontroler, a opäť ho zobrazí pri odpojení kontroleru."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Skryje overlay, keď je v porte 1 pripojený fyzický kontroler. Pri odpojení kontroleru sa overlay automaticky neobnoví."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Zobraziť vstupy na prekrytí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Zobraziť zachytené vstupy na prekrytí obrazovky. 'Dotykové' zvýrazni prvky prekrytie, ktoré boli stlačené/kliknuté. 'Fyzické (herný ovládač)' zvýrazní skutočný vstup poslaný jadru, zvyčajne z pripojeného herného ovládača/klávesnice."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Dotykové"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Fyzické (herný ovládač)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Zobraziť vstupy z portu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Zvoliť port vstupného zariadenia na sledovanie, keď je 'Zobraziť vstupy na prekrytí' nastavené na 'Fyzické (herný ovládať)'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Zobraziť kurzor myši s overlayom"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Zobraziť kurzor myši pri použití overlay na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Automaticky otáčať overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Ak to aktuálny overlay podporuje, automaticky otočí rozloženie tak, aby zodpovedalo orientácii/pomeru strán obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Automaticky škálovať overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Automaticky upraví mierku overlay a rozostup UI prvkov tak, aby zodpovedal pomeru strán obrazovky. Najlepšie výsledky dosahuje s overlay kontroleru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Citlivosť diagonál D-Padu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Upraví veľkosť diagonálnych zón. Pre 8-smerovú symetriu nastavte na 100 %."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Citlivosť prekrytia ABXY"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Upraví veľkosť zón prekrytia v diamantovom usporiadaní hlavných tlačidiel. Pre 8-smerovú symetriu nastavte na 100 %."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Zóna recentrovania analógu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Vstup analógovej páčky bude relatívny k prvému dotyku, ak sa stlačí v tejto zóne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Prekrytie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Automaticky načítať preferovaný overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "Uprednostniť načítanie overlayov na základe názvu systému pred predvolenou predvoľbou. Ak je nastavený override pre overlay predvoľbu, ignoruje sa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Nepriehľadnosť overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Nepriehľadnosť všetkých UI prvkov overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Predvoľba overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Vyberte overlay z prehliadača súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(Krajinka) Mierka overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Mierka všetkých UI prvkov overlay pri použití krajinkovej orientácie zobrazenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(Krajinka) Úprava pomeru strán overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Aplikuje korekčný faktor pomeru strán na overlay pri použití krajinkovej orientácie zobrazenia. Kladné hodnoty zvyšujú (záporné znižujú) efektívnu šírku overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Krajinka) Horizontálne oddelenie overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Ak to aktuálna predvoľba podporuje, upraví rozostup medzi UI prvkami v ľavej a pravej polovici overlay pri použití krajinkovej orientácie zobrazenia. Kladné hodnoty zvyšujú (záporné znižujú) oddelenie oboch polovíc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Krajinka) Vertikálne oddelenie overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Ak to aktuálna predvoľba podporuje, upraví rozostup medzi UI prvkami v hornej a dolnej polovici overlay pri použití krajinkovej orientácie zobrazenia. Kladné hodnoty zvyšujú (záporné znižujú) oddelenie oboch polovíc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Krajinka) Posun overlay X"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Horizontálny posun overlay pri použití krajinkovej orientácie zobrazenia. Kladné hodnoty posúvajú overlay vpravo; záporné vľavo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Krajinka) Posun overlay Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Vertikálny posun overlay pri použití krajinkovej orientácie zobrazenia. Kladné hodnoty posúvajú overlay nahor; záporné nadol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(Portrét) Mierka overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Mierka všetkých UI prvkov overlay pri použití portrétovej orientácie zobrazenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(Portrét) Úprava pomeru strán overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Aplikuje korekčný faktor pomeru strán na overlay pri použití portrétovej orientácie zobrazenia. Kladné hodnoty zvyšujú (záporné znižujú) efektívnu výšku overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Portrét) Horizontálne oddelenie overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Ak to aktuálna predvoľba podporuje, upraví rozostup medzi UI prvkami v ľavej a pravej polovici overlay pri použití portrétovej orientácie zobrazenia. Kladné hodnoty zvyšujú (záporné znižujú) oddelenie oboch polovíc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Portrét) Vertikálne oddelenie overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Ak to aktuálna predvoľba podporuje, upraví rozostup medzi UI prvkami v hornej a dolnej polovici overlay pri použití portrétovej orientácie zobrazenia. Kladné hodnoty zvyšujú (záporné znižujú) oddelenie oboch polovíc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Portrét) Posun overlay X"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Horizontálny posun overlay pri použití portrétovej orientácie zobrazenia. Kladné hodnoty posúvajú overlay vpravo; záporné vľavo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Portrét) Posun overlay Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Vertikálny posun overlay pri použití portrétovej orientácie zobrazenia. Kladné hodnoty posúvajú overlay nahor; záporné nadol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "Klávesnicový overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "Vybrať a upraviť klávesnicový overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "Povoliť svetelnú zbraň, myš a ukazovateľ overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "Použiť akékoľvek dotykové vstupy nestláčajúce ovládače overlay na vytvorenie vstupu polohovacieho zariadenia pre core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "Svetelná zbraň overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Konfigurovať vstup svetelnej zbrane odoslaný z overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Myš overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Konfigurovať vstup myši odoslaný z overlay. Poznámka: 1-, 2- a 3-prstové klepnutia odosielajú kliknutia ľavým, pravým a stredným tlačidlom."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "Predvoľba klávesnicového overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Vyberte klávesnicový overlay z prehliadača súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Automaticky škálovať klávesnicový overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Upraviť klávesnicový overlay na jeho pôvodný pomer strán. Pri vypnutí sa natiahne na obrazovku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Nepriehľadnosť klávesnicového overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Nepriehľadnosť všetkých UI prvkov klávesnicového overlay."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Port svetelnej zbrane"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Nastaviť port core, ktorý má prijímať vstup zo svetelnej zbrane overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Všetky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Spúšťač pri dotyku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Posielať vstup spúšťača spolu so vstupom ukazovateľa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Oneskorenie spúšťača (snímky)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Oneskorenie vstupu spúšťača, aby kurzor mal čas pohnúť sa. Toto oneskorenie sa tiež používa na čakanie na správny počet multi-touch dotykov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "2-dotykový vstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Vyberte vstup, ktorý sa má odoslať pri dvoch ukazovateľoch na obrazovke. Oneskorenie spúšťača by malo byť nenulové, aby sa odlíšilo od iného vstupu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "3-dotykový vstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Vyberte vstup, ktorý sa má odoslať pri troch ukazovateľoch na obrazovke. Oneskorenie spúšťača by malo byť nenulové, aby sa odlíšilo od iného vstupu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "4-dotykový vstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Vyberte vstup, ktorý sa má odoslať pri štyroch ukazovateľoch na obrazovke. Oneskorenie spúšťača by malo byť nenulové, aby sa odlíšilo od iného vstupu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Povoliť mimo obrazovku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Povoliť mierenie mimo hraníc. Pri vypnutí sa mierenie mimo obrazovky obmedzí na okraj v hraniciach."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Rýchlosť myši"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Upraviť rýchlosť pohybu kurzora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Dlhé stlačenie pre potiahnutie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Dlho stlačte obrazovku na začatie držania tlačidla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Prah dlhého stlačenia (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Upraviť čas držania potrebný pre dlhé stlačenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Dvojité klepnutie pre potiahnutie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Dvojito klepnite na obrazovku na začatie držania tlačidla pri druhom klepnutí. Pridáva latenciu k kliknutiam myšou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Prah dvojitého klepnutia (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Upraviť povolený čas medzi klepnutiami pri detekcii dvojitého klepnutia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Prah swipe gesta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
   "Alt 2-dotykový vstup"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
   "Použiť druhý dotyk ako tlačidlo myši pri ovládaní kurzora."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Upraviť povolený rozsah driftu pri detekcii dlhého stlačenia alebo klepnutia. Vyjadrené ako percento menšieho rozmeru obrazovky."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Upozornenia na obrazovke"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Zobrazovať správy na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Grafické widgety"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Použiť dekorované animácie, oznámenia, indikátory a ovládače."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Automaticky škálovať grafické widgety"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Automaticky meniť veľkosť dekorovaných oznámení, indikátorov a ovládačov podľa aktuálnej mierky menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Prepísanie mierky grafických widgetov (celá obrazovka)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Aplikuje manuálne prepísanie mierky pri vykresľovaní zobrazovacích widgetov v režime celej obrazovky. Platí len pri vypnutom 'Automaticky škálovať grafické widgety'. Možno použiť na zväčšenie alebo zmenšenie veľkosti dekorovaných oznámení, indikátorov a ovládačov nezávisle od menu samotného."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Prepísanie mierky grafických widgetov (okno)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Aplikuje manuálne prepísanie mierky pri vykresľovaní zobrazovacích widgetov v režime okna. Platí len pri vypnutom 'Automaticky škálovať grafické widgety'. Možno použiť na zväčšenie alebo zmenšenie veľkosti dekorovaných oznámení, indikátorov a ovládačov nezávisle od menu samotného."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Zobraziť snímkovú frekvenciu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Zobraziť aktuálne snímky za sekundu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Interval aktualizácie snímkovej frekvencie (v snímkach)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "Zobrazenie snímkovej frekvencie sa bude aktualizovať v nastavenom intervale v snímkach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Zobraziť počet snímok"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Zobraziť aktuálny počet snímok na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Zobraziť štatistiku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Zobraziť technické štatistiky na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Zobraziť využitie pamäte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Zobraziť použité a celkové množstvo pamäte v systéme."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Interval aktualizácie použitia pamäte (v snímkach)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "Zobrazenie použitia pamäte sa bude aktualizovať v nastavenom intervale v snímkach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Zobraziť netplay ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Zobraziť ping pre aktuálnu netplay miestnosť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Štartovacie oznámenie 'Načítať obsah'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Zobraziť stručnú spätnú animáciu pri spustení pri načítaní obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Oznámenia pripojenia vstupu (autoconfig)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Oznámenia zlyhania vstupu (autoconfig)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Oznámenia cheat kódov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Zobraziť správu na obrazovke pri aplikovaní cheat kódov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Oznámenia patchov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Zobraziť správu na obrazovke pri soft-patchovaní ROM."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Zobraziť správu na obrazovke pri pripájaní/odpájaní vstupných zariadení."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Zobraziť správu na obrazovke, keď sa vstupné zariadenia nedali nakonfigurovať."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Oznámenia o načítaní vstupných remap súborov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Zobraziť správu na obrazovke pri načítaní vstupných remap súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Oznámenia o načítaní config override"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Zobraziť správu na obrazovke pri načítaní súborov override konfigurácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Oznámenia o obnovení počiatočného disku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Zobraziť správu na obrazovke pri automatickom obnovení naposledy použitého disku obsahu s viacerými diskami načítaného cez M3U zoznamy pri spustení."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Oznámenia ovládania disku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Zobraziť správu na obrazovke pri vkladaní a vysúvaní diskov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Oznámenia save state"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "Zobraziť správu na obrazovke pri ukladaní a načítaní save states."
   )
MSG_HASH( /* FIXME: Rename config key and msg hash */
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Oznámenia obmedzenia snímok"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Zobraziť indikátor na obrazovke, keď je aktívny rýchly posun vpred, spomalený pohyb alebo pretáčanie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Oznámenia screenshotov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Zobraziť správu na obrazovke pri vytváraní screenshotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Trvanie oznámenia screenshotu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Definovať trvanie správy screenshotu na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "Normálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Rýchlo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Veľmi rýchle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Okamžite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Efekt blesku screenshotu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Zobraziť biely blikajúci efekt na obrazovke s požadovaným trvaním pri vytváraní screenshotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "Zapnuté (normálne)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "Vypnuté (rýchle)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Oznámenia obnovovacej frekvencie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Zobraziť správu na obrazovke pri nastavovaní obnovovacej frekvencie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Extra netplay oznámenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Zobraziť nedôležité netplay správy na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Oznámenia iba v menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Zobraziť oznámenia iba keď je menu otvorené."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Písmo upozornení"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Vybrať písmo pre oznámenia na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Veľkosť upozornení"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Určiť veľkosť písma v bodoch. Pri použití widgetov má táto veľkosť vplyv iba na zobrazenie štatistík na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Pozícia oznámenia (horizontálna)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Určiť vlastnú polohu na osi X pre text na obrazovke. 0 je ľavý okraj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Pozícia oznámenia (vertikálna)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Určiť vlastnú polohu na osi Y pre text na obrazovke. 0 je spodný okraj."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Farba oznámenia (červená)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Nastaví červenú zložku farby OSD textu. Platné hodnoty sú medzi 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Farba oznámenia (zelená)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Nastaví zelenú zložku farby OSD textu. Platné hodnoty sú medzi 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Farba oznámenia (modrá)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Nastaví modrú zložku farby OSD textu. Platné hodnoty sú medzi 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Pozadie oznámenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Povolí farbu pozadia pre OSD."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Farba pozadia oznámenia (červená)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Nastaví červenú zložku farby pozadia OSD. Platné hodnoty sú medzi 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Farba pozadia oznámenia (zelená)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Nastaví zelenú zložku farby pozadia OSD. Platné hodnoty sú medzi 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Farba pozadia oznámenia (modrá)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Nastaví modrú zložku farby pozadia OSD. Platné hodnoty sú medzi 0 a 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Nepriehľadnosť pozadia oznámenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Nastaví nepriehľadnosť farby pozadia OSD. Platné hodnoty sú medzi 0,0 a 1,0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Vzhľad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Zmeniť nastavenia vzhľadu obrazovky menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Viditeľnosť položky ponuky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Prepínať viditeľnosť položiek menu v RetroArchu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Pozastaviť obsah keď je menu aktívne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Pozastaví obsah, ak je menu aktívne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Pozastaviť obsah keď nie je aktívne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Pozastaví obsah, keď RetroArch nie je aktívne okno."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Ukončiť pri zavretí obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Automaticky ukončiť RetroArch pri zatvorení obsahu. 'CLI' ukončí iba ak bol obsah spustený cez príkazový riadok."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Pokračovať v obsahu po použití save states"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Automaticky zatvoriť menu a pokračovať v obsahu po uložení alebo načítaní stavu. Vypnutie môže zlepšiť výkon save state na veľmi pomalých zariadeniach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Pokračovať v obsahu po zmene diskov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Automaticky zatvoriť menu a pokračovať v obsahu po vložení alebo načítaní nového disku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Wrap-around navigácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Wrap-around na začiatok a/alebo koniec, keď sa horizontálne alebo vertikálne dosiahne hranica zoznamu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Zobraziť pokročilé nastavenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Zobraziť pokročilé nastavenia pre skúsených používateľov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Režim kiosku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Chráni nastavenie skrytím všetkých nastavení súvisiacich s konfiguráciou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Nastaviť heslo pre vypnutie kiosk režimu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Zadanie hesla pri zapínaní kiosk režimu umožňuje neskôr ho z menu vypnúť, prejdením do hlavného menu, výberom Vypnúť kiosk režim a zadaním hesla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Podpora myši"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Umožniť ovládanie menu myšou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Podpora dotyku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Umožniť ovládanie menu dotykovou obrazovkou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Vláknové úlohy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Vykonávať úlohy v samostatnom vlákne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Časový limit šetriča obrazovky menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Pokiaľ je menu aktívne, šetrič obrazovky sa zobrazí po zadanom období neaktivity."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Animácia šetriča obrazovky menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Povolí animačný efekt počas aktívneho šetriča obrazovky menu. Má mierny vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Sneh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Hviezdičkové pole"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX,
   "Vír"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Rýchlosť animácie šetriča obrazovky menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Upraviť rýchlosť animačného efektu šetriča obrazovky menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Vypnúť kompozíciu pracovnej plochy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Window manažéri používajú kompozíciu na aplikáciu vizuálnych efektov, detekciu nereagujúcich okien a iné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Vynútene vypnúť kompozíciu. Vypnutie je platné iba na Windows Vista/7."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "UI sprievodca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Spustiť UI sprievodcu pri štarte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Spustí pomocný ovládač používateľského rozhrania pri štarte (ak je dostupný)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Desktop menu (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Otvoriť desktop menu pri štarte"
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "Vzhľad spodnej obrazovky 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Zmeniť nastavenia vzhľadu spodnej obrazovky."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "Ikona aplikácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Zmeniť ikonu aplikácie."
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Rýchle Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Prepnúť viditeľnosť položiek ponuky v Rýchlej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Nastavenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Prepnúť viditeľnosť položiek ponuky v ponuke Nastavenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Zobraziť 'Načítať jadro'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Zobraziť voľbu 'Načítať jadro' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Zobraziť 'Načítať obsah'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Zobraziť voľbu 'Načítať obsah' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Zobraziť 'Načítať disk'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Zobraziť voľbu 'Načítať disk' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Zobraziť 'Skopírovať disk'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Zobraziť voľbu 'Skopírovať disk' v hlavnej ponuke."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "Zobraziť 'Vysunúť disk'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Zobraziť voľbu 'Vysunúť disk' v hlavnej ponuke."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Zobraziť 'Online aktualizácie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Zobraziť voľbu 'Online aktualizácie' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Zobraziť 'Stiahnuť jadrá'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Zobraziť možnosť aktualizovať jadrá (a info súbory jadier) vo voľbe 'Online aktualizácie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Zobraziť 'Informácia'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Zobraziť voľbu 'Informácia' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Zobraziť 'Konfiguračný súbor'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Zobraziť voľbu 'Konfiguračný súbor' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Zobraziť 'Pomoc'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Zobraziť voľbu 'Pomocník' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Zobraziť 'Ukončiť RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Zobraziť voľbu 'Ukončiť RetroArch' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Zobraziť 'Reštartovať RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Zobraziť voľbu 'Reštartovať RetroArch' v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Zobraziť 'Nastavenia'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Zobraziť menu 'Nastavenia'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Nastaviť heslo pre povolenie 'Nastavenia'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Zadaním hesla pri skrytí karty nastavení je možné ju neskôr obnoviť z ponuky tak, že prejdete na kartu Hlavná ponuka, vyberiete možnosť „Povoliť kartu Nastavenia“ a zadáte heslo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Zobraziť 'Obľúbené'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Zobraziť menu 'Obľúbené'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES_FIRST,
   "Zobraziť obľúbené ako prvé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES_FIRST,
   "Zobraziť 'Obľúbené' pred 'História'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Zobraziť 'Obrázky'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Zobraziť menu 'Obrázky'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Zobraziť 'Hudba'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Zobraziť menu 'Hudba'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Zobraziť 'Videá'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Zobraziť menu 'Videá'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Zobraziť 'Sieťová hra'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Zobraziť menu 'Netplay'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Zobraziť 'História'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Zobraziť menu nedávnej histórie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Zobraziť 'Importovať obsah'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Zobraziť položku 'Importovať obsah' v hlavnom menu alebo zoznamoch skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Hlavné Menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Ponuka hracích zoznamov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Zobraziť 'Hracie zoznamy'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Zobraziť zoznamy skladieb v hlavnom menu. V GLUI sa ignoruje, ak sú povolené karty zoznamov skladieb a navbar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "Zobraziť karty zoznamov skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "Zobraziť karty zoznamov skladieb. Neovplyvňuje RGUI. V GLUI musí byť navbar povolený."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Zobraziť \"Preskúmať'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Zobraziť možnosť prieskumníka obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Zobraziť 'Bezobsahové jadrá'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Určiť typ core (ak nejaký), ktorý sa má zobraziť v menu 'Cores bez obsahu'. Pri nastavení 'Custom' možno viditeľnosť jednotlivých cores prepínať cez menu 'Spravovať cores'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Všetko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Jedno použitie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Zobraziť dátum a čas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Zobraziť aktuálny dátum a/alebo čas v menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Štýl dátumu a času"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Zmeniť štýl, akým je aktuálny dátum a/alebo čas zobrazený v menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Oddeľovač dátumu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Určiť znak, ktorý sa má použiť ako oddeľovač medzi komponentami rok/mesiac/deň pri zobrazení aktuálneho dátumu v menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Zobraziť úroveň batérie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Zobraziť aktuálnu úroveň batérie v menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Zobraziť názov jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Zobraziť názov aktuálneho jadra v ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Zobraziť pod-popisy menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Zobraziť dodatočné informácie pre položky menu."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Zobraziť štartovaciu obrazovku"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Zobraziť úvodnú obrazovku v menu. Pri prvom spustení programu sa táto možnosť automaticky nastaví na false."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Zobraziť 'Pokračovať'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Zobraziť možnosť pokračovania v obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Zobraziť 'Reset'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Zobraziť možnosť resetu obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Zobraziť 'Zatvoriť obsah'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Zobraziť možnosť zatvorenia obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Zobraziť podmenu 'Save states'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Zobraziť možnosti save state v podmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Zobraziť 'Save/Load State'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Zobraziť možnosti pre uloženie/načítanie stavu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "Zobraziť 'Ovládače Replay'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "Zobraziť možnosti pre nahrávanie/prehrávanie replay súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Zobraziť 'Vrátiť uloženie/načítanie stavu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Zobraziť možnosti pre vrátenie uloženia/načítania stavu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Zobraziť 'Voľby jadra'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Zobraziť voľbu 'Voľby jadra'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Zobraziť 'Vyprázdniť možnosti na disk'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Zobraziť položku 'Vyprázdniť možnosti na disk' v menu 'Možnosti > Spravovať možnosti core'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Zobraziť 'Ovládanie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Zobraziť voľbu 'Ovládanie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Zobraziť 'Urobiť snímku obrazovky'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Zobraziť voľbu 'Vytvoriť snímku obrazovky'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Zobraziť 'Spustiť záznam'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Zobraziť voľbu 'Spustiť záznam'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Zobraziť 'Spustiť streamovanie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Zobraziť voľbu 'Spustiť streamovanie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Zobraziť 'On-Screen prekrytie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Zobraziť voľbu 'On-Screen prekrytie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Zobraziť 'Video rozloženie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Zobraziť voľbu 'Video rozloženie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Zobraziť 'Latencia'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Zobraziť voľbu 'Latencia'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Zobraziť 'Pretočiť'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Zobraziť možnosť 'Spätné prevíjanie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Zobraziť 'Uložiť prepisy jadier'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Zobraziť možnosť 'Uložiť override core' v menu 'Override'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Zobraziť 'Uložiť override adresára obsahu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Zobraziť možnosť 'Uložiť override adresára obsahu' v menu 'Override'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Zobraziť 'Uložiť prepisy hier'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Zobraziť možnosť 'Uložiť override hry' v menu 'Override'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Zobraziť 'Cheaty'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Zobraziť voľbu 'Cheaty'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Zobraziť 'Shadery'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Zobraziť voľbu 'Shadery'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Zobraziť 'Pridať do obľúbených'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Zobraziť voľbu 'Pridať do obľúbených'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Zobraziť 'Pridať do zoznamu skladieb'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Zobraziť možnosť 'Pridať do zoznamu skladieb'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Zobraziť 'Nastaviť asociáciu jadra'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Zobraziť možnosť 'Nastaviť priradenie core', keď nebeží obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Zobraziť 'Vynulovať asociáciu jadra'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Zobraziť možnosť 'Resetovať priradenie core', keď nebeží obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Zobraziť 'Stiahnuť miniatúry'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Zobraziť možnosť 'Stiahnuť náhľady', keď nebeží obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Zobraziť 'Informácia'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Zobraziť voľbu 'Informácia'."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Zobraziť 'Ovládače'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Zobraziť nastavenia 'Ovládače'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Zobraziť 'Video'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Zobraziť nastavenia 'Video'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Zobraziť 'Zvuk'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Zobraziť nastavenia 'Audio'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Zobraziť 'Vstup'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Zobraziť nastavenia 'Vstup'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Zobraziť 'Latencia'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Zobraziť nastavenia 'Latencia'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Zobraziť 'Jadro'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Zobraziť nastavenia 'Jadro'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Zobraziť 'Nastavenie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Zobraziť nastavenia 'Nastavenie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Zobraziť 'Ukladanie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Zobraziť nastavenia 'Ukladanie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Zobraziť 'Záznam'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Zobraziť nastavenia 'Záznam'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Zobraziť 'Prehliadač súborov'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Zobraziť nastavenia 'Prehliadač súborov'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Zobraziť 'Obmedzenie snímok'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Zobraziť nastavenia 'Obmedzenie snímok'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Zobraziť 'Záznam'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Zobraziť nastavenia 'Záznam'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Zobraziť 'On-Screen zobrazenie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Zobraziť nastavenia 'On-Screen zobrazenie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Zobraziť 'Používateľské rozhranie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Zobraziť nastavenia 'Používateľské rozhranie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Zobraziť 'AI Service'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Zobraziť nastavenia 'AI Service'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Zobraziť 'Prístupnosť'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Zobraziť nastavenia 'Prístupnosť'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Zobraziť 'Správa napájania'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Zmeniť nastavenia správy napájania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Zobraziť 'Úspechy'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Zobraziť nastavenia 'Úspechy'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Zobraziť 'Sieť'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Zobraziť nastavenia 'Sieť'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Zobraziť 'Hracie zoznamy'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Zobraziť nastavenia 'Hracie zoznamy'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Zobraziť 'Používateľ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Zobraziť nastavenia 'Používateľ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Zobraziť 'Adresár'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Zobraziť nastavenia 'Adresár'."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "Zobraziť 'Steam'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "Zobraziť nastavenia 'Stream'."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Mierkový faktor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Škáluje veľkosť prvkov používateľského rozhrania v menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Obrázok pozadia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Vyberte obrázok ako pozadie menu. Manuálne a dynamické obrázky prepíšu 'Farebnú tému'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Nepriehľadnosť pozadia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Upraviť nepriehľadnosť obrázku pozadia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Nepriehľadnosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Upraviť nepriehľadnosť predvoleného pozadia menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Použiť preferovanú systémovú farebnú tému"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Použiť farebnú tému operačného systému (ak existuje). Prepisuje nastavenia témy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Primárna miniatúra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Typ náhľadu na zobrazenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Prah upscalingu náhľadov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Automaticky upscalovať obrázky náhľadov so šírkou/výškou menšou ako zadaná hodnota. Zlepšuje kvalitu obrazu. Má mierny vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Pozadia náhľadov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Povolí vyplnenie nepoužitého priestoru v obrázkoch náhľadov pevným pozadím. Zaisťuje jednotnú veľkosť zobrazenia pre všetky obrázky, čo zlepšuje vzhľad menu pri prezeraní zmiešaných náhľadov obsahu s rôznymi základnými rozmermi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Animácia rolujúceho textu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Vyberte spôsob horizontálneho rolovania na zobrazenie dlhého textu menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Rýchlosť rolujúceho textu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Rýchlosť animácie pri rolovaní dlhého textu menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Plynulý rolujúci text"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Použiť plynulú animáciu rolovania pri zobrazení dlhého textu menu. Má malý vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Pamätať si výber pri zmene kariet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Zapamätať si predchádzajúcu polohu kurzora v kartách. RGUI nemá karty, ale Zoznamy skladieb a Nastavenia sa správajú podobne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Vždy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Len pre hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Iba pre Hlavné menu a Nastavenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_STARTUP_PAGE,
   "Úvodná stránka"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_STARTUP_PAGE,
   "Počiatočná stránka menu pri štarte."
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Výstup služby AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Zobraziť preklad ako textový overlay (Image Mode), prehrávať ako Text-To-Speech (Speech), alebo použiť systémový čítač ako NVDA (Narrator)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_BACKEND,
   "Backend AI Service"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_BACKEND,
   "Vyberte, ktorý prekladový backend sa má použiť. HTTP používa vzdialený server na nakonfigurovanej URL. Apple používa OCR a preklad priamo na zariadení (macOS/iOS)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "URL služby AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "URL http:// adresa ukazujúca na prekladovú službu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "AI služba povolená"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Povoliť spustenie AI Service, keď je stlačená horúca kláves AI Service."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Pozastaviť počas prekladu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Pozastaviť core, kým sa preloží obrazovka."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Zdrojový jazyk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "Jazyk, z ktorého služba prekladá. Pri nastavení 'Default' sa pokúsi automaticky detegovať jazyk. Nastavenie konkrétneho jazyka spresní preklad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Cieľový jazyk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "Jazyk, do ktorého služba prekladá. 'Default' je angličtina."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Povoliť prístupnosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Povolí Text-to-Speech na pomoc pri navigácii v menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Rýchlosť Text-to-Speech"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Rýchlosť hlasu Text-to-Speech."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Úspechy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Získavajte achievementy v klasických hrách. Viac informácií na 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Hardcore režim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Vypne cheaty, spätné prevíjanie, spomalený pohyb a načítanie save states. Achievementy získané v hardcore režime sú jedinečne označené, takže môžete ostatným ukázať, čo ste dosiahli bez pomocných funkcií emulátora. Prepnutie tohto nastavenia za behu reštartuje hru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Rebríčky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Pravidelne odosiela kontextové informácie o hre na webovú stránku RetroAchievements. Nemá efekt, ak je povolený 'Hardcore režim'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Odznaky achievementov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Zobraziť odznaky v zozname achievementov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Testovať neoficiálne achievementy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Použiť neoficiálne achievementy a/alebo beta funkcie na testovacie účely."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Zvuk odomknutia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Prehrať zvuk pri odomknutí achievementu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Automatický screenshot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Automaticky vytvoriť screenshot pri získaní achievementu."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Encore režim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Spustiť reláciu so všetkými achievementami aktívnymi (aj predtým odomknutými)."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Vzhľad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Zmeniť polohu a posuny oznámení achievementov na obrazovke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Poloha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Nastaví roh/okraj obrazovky, z ktorého sa budú objavovať oznámenia achievementov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Vľavo hore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "V strede hore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Hore vpravo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Vľavo dolu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Dole v strede"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "Dole vpravo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Zarovnaný odsadenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Nastavte, či sa majú oznámenia achievementov zarovnávať s inými typmi oznámení na obrazovke. Vypnite na manuálne nastavenie hodnôt odsadenia/polohy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "Manuálne horizontálne odsadenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Vzdialenosť od ľavého/pravého okraja obrazovky, ktorá môže kompenzovať overscan displeja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "Manuálne vertikálne odsadenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Vzdialenosť od horného/dolného okraja obrazovky, ktorá môže kompenzovať overscan displeja."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "Viditeľnosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "Zmeniť, ktoré správy a prvky na obrazovke sa zobrazia. Nevypína funkčnosť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "Súhrn pri štarte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "Zobrazí informácie o načítavanej hre a aktuálnom postupe používateľa.\n'Všetky identifikované hry' zobrazí súhrn aj pre hry bez zverejnených achievementov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "Všetky identifikované hry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "Hry s achievementami"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "Oznámenia odomknutia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "Zobrazí oznámenie pri odomknutí achievementu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "Oznámenia majstrovstva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "Zobrazí oznámenie pri odomknutí všetkých achievementov pre hru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Indikátory aktívnych výziev"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Zobrazí indikátory na obrazovke, kým sa môžu získať určité achievementy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Indikátor postupu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Zobrazí indikátor na obrazovke pri postupe smerom k určitým achievementom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "Správy o začatí leaderboardu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "Zobrazí popis leaderboardu, keď sa stane aktívnym."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Správy o odoslaní leaderboardu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Zobrazí správu s odoslanou hodnotou pri dokončení pokusu o leaderboard."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Správy o zlyhaní leaderboardu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Zobrazí správu pri zlyhaní pokusu o leaderboard."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Sledovače leaderboardu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Zobrazí sledovače na obrazovke s aktuálnou hodnotou aktívnych leaderboardov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Správy prihlasovania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Zobrazí správy súvisiace s prihlasovaním do účtu RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Verbozné správy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Zobrazí dodatočné diagnostické a chybové správy."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Verejne oznamovať netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Či sa majú netplay hry verejne oznamovať. Pri vypnutí sa klienti musia pripájať manuálne, namiesto použitia verejnej lobby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Použiť relay server"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Presmerovať netplay pripojenia cez man-in-the-middle server. Užitočné, ak je hostiteľ za firewallom alebo má problémy s NAT/UPnP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Lokácia relay servera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Vyberte konkrétny relay server na použitie. Geograficky bližšie lokácie majú zvyčajne nižšiu latenciu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Vlastná adresa relay servera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Sem zadajte adresu vášho vlastného relay servera. Formát: adresa alebo adresa|port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "Severná Amerika (východné pobrežie, USA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "Západná Európa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "Južná Amerika (juhovýchod, Brazília)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Juhovýchodná Ázia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Adresa servera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Adresa hostiteľa pre pripojenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Netplay TCP port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Port hostiteľskej IP adresy. Môže byť TCP alebo UDP port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Maximálny počet súčasných pripojení"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Maximálny počet aktívnych pripojení, ktoré hostiteľ akceptuje predtým, než odmietne nové."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Obmedzovač pingu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "Maximálna latencia spojenia (ping), ktorý hostiteľ ešte akceptuje. Nastavte na 0 pre neobmedzenú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Heslo servra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Heslo používané klientami pri pripájaní k hostiteľovi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Heslo servera len pre divákov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Heslo používané klientami pri pripájaní k hostiteľovi ako divák."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Netplay režim diváka"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Spustiť netplay v režime diváka."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Či sa má netplay spustiť v režime diváka. Pri nastavení true bude netplay pri spustení v režime diváka. Režim možno vždy zmeniť neskôr."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Blednutie chatu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Postupne stmavovať chatové správy v čase."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Farba chatu (prezývka)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Formát: #RRGGBB alebo RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Farba chatu (správa)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Formát: #RRGGBB alebo RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Povoliť pozastavenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Povoliť hráčom pozastaviť hru počas netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Povoliť klientov v slave režime"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Povoliť pripojenia v slave režime. Klienti v slave režime vyžadujú veľmi málo výpočtového výkonu na oboch stranách, ale výrazne trpia sieťovou latenciou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Zakázať klientov mimo slave režimu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Zakázať pripojenia, ktoré nie sú v slave režime. Neodporúča sa okrem prípadov veľmi rýchlych sietí s veľmi slabými strojmi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Snímky kontroly netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "Frekvencia (v snímkach), ako často netplay overí, že hostiteľ a klient sú synchronizovaní."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "Frekvencia v snímkach, s akou netplay overí, že hostiteľ a klient sú synchronizovaní. Pri väčšine cores nemá táto hodnota viditeľný efekt a možno ju ignorovať. Pri nedeterministických cores táto hodnota určuje, ako často sa netplay peers zosynchronizujú. Pri chybných cores nastavenie nenulovej hodnoty spôsobí vážne problémy s výkonom. Nastavte na nulu na vypnutie kontrol. Táto hodnota sa používa iba na netplay hostiteľovi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Snímky vstupnej latencie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Počet snímok vstupnej latencie, ktoré netplay použije na skrytie sieťovej latencie. Redukuje jitter a robí netplay menej náročným na CPU za cenu znateľného input lagu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Počet snímok vstupnej latencie, ktoré netplay použije na skrytie sieťovej latencie.\nPočas netplay táto možnosť oneskoruje lokálny vstup tak, aby spustená snímka bola bližšie k snímkam prijímaným zo siete. Redukuje jitter a robí netplay menej náročným na CPU za cenu znateľného input lagu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Rozsah snímok vstupnej latencie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Rozsah snímok vstupnej latencie, ktoré možno použiť na skrytie sieťovej latencie. Redukuje jitter a robí netplay menej náročným na CPU za cenu nepredvídateľného input lagu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Rozsah snímok vstupnej latencie, ktoré môže netplay použiť na skrytie sieťovej latencie.\nPri nastavení netplay dynamicky upraví počet snímok vstupnej latencie na vyváženie CPU času, vstupnej latencie a sieťovej latencie. Redukuje jitter a robí netplay menej náročným na CPU za cenu nepredvídateľného input lagu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "NAT prechod netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Pri hostovaní sa pokúsi prijímať pripojenia z verejného internetu pomocou UPnP alebo podobných technológií na obídenie LAN."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Zdieľanie digitálneho vstupu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Požiadať zariadenie %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Požiadať o hru s daným vstupným zariadením."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Sieťové príkazy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Sieťový command port"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "Sieťový RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Základný port sieťového RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Sieťový RetroPad používateľa %d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "stdin príkazy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "stdin príkazové rozhranie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Sťahovanie náhľadov na požiadanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Automaticky stiahnuť chýbajúce náhľady pri prehliadaní zoznamov skladieb. Má vážny vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Nastavenia aktualizátora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Prístup k nastaveniam aktualizátora cores"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "URL Buildbot Cores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL adresa adresára aktualizátora cores na libretro buildbot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "URL Buildbot Assets"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL adresa adresára aktualizátora assetov na libretro buildbot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Automaticky extrahovať stiahnutý archív"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Po stiahnutí automaticky extrahovať súbory obsiahnuté v stiahnutých archívoch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Zobraziť experimentálne cores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Zahrnúť 'experimentálne' cores v zozname Core Downloader. Tieto sú zvyčajne určené iba na vývojové/testovacie účely a neodporúčajú sa na bežné použitie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Zálohovať cores pri aktualizácii"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Automaticky vytvoriť zálohu nainštalovaných cores pri online aktualizácii. Umožňuje jednoduchý návrat k fungujúcemu core, ak aktualizácia spôsobí regresiu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Veľkosť histórie záloh core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Určiť, koľko automaticky generovaných záloh sa má pre každý nainštalovaný core uchovať. Pri dosiahnutí tohto limitu vytvorenie novej zálohy cez online aktualizáciu odstráni najstaršiu zálohu. Manuálne zálohy core týmto nastavením nie sú ovplyvnené."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "História"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Udržiavať zoznam skladieb nedávno použitých hier, obrázkov, hudby a videí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Veľkosť histórie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Obmedziť počet položiek v nedávnom zozname skladieb pre hry, obrázky, hudbu a videá."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Veľkosť obľúbených"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Obmedziť počet položiek v zozname skladieb 'Obľúbené'. Po dosiahnutí limitu sa nové pridania zablokujú, kým sa neodstránia staré položky. Hodnota -1 umožňuje 'neobmedzený' počet položiek.\nVAROVANIE: Zníženie hodnoty odstráni existujúce položky!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Povoliť premenovanie položiek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Povoliť premenovanie položiek zoznamu skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Povoliť odstránenie položiek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Povoliť odstránenie položiek zoznamu skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Zoradiť hracie zoznamy abecedne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Triediť zoznamy skladieb obsahu v abecednom poradí, okrem zoznamov 'História', 'Obrázky', 'Hudba' a 'Videá'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Uložiť zoznamy skladieb v starom formáte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Zapisovať zoznamy skladieb v zastaralom textovom formáte. Pri vypnutí sa zoznamy skladieb formátujú pomocou JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Komprimovať zoznamy skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Archivovať dáta zoznamu skladieb pri zapisovaní na disk. Znižuje veľkosť súboru a časy načítania za cenu (zanedbateľne) zvýšeného použitia CPU. Možno použiť so starým aj novým formátom zoznamov skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Zobraziť priradené cores v zoznamoch skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Určiť, kedy označiť položky zoznamu skladieb aktuálne priradeným core (ak nejaký).\nToto nastavenie sa ignoruje pri zapnutých pod-popisoch zoznamu skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Zobraziť pod-popisy zoznamu skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Zobraziť dodatočné informácie pre každú položku zoznamu skladieb, ako napríklad aktuálne priradenie core a čas behu (ak je dostupný). Má variabilný vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Zobraziť ikony špecifické pre obsah v histórii a obľúbených"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Zobraziť konkrétne ikony pre každú položku zoznamu histórie a obľúbených. Má variabilný vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Jadro:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Čas behu:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Naposledy hrané:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_PLAY_COUNT,
   "Počet hraní:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "sekunda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "sekúnd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "minúta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "minúty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "hodina"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "hodiny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "deň"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "dní"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "týždeň"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "týždne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "mesiac"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "mesiacov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "rok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "roky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "vzad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Zobraziť index položiek zoznamu skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Zobraziť čísla položiek pri prezeraní zoznamov skladieb. Formát zobrazenia závisí od aktuálne zvoleného ovládača menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Čas behu pod-popisu zoznamu skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Vyberte, aký typ záznamu logu času behu sa má zobraziť v pod-popisoch zoznamu skladieb.\nZodpovedajúci log času behu musí byť povolený cez menu možností 'Ukladanie'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Štýl dátumu a času 'Naposledy hrané'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Nastavte štýl dátumu a času zobrazeného pre časovú pečiatku 'Naposledy hrané'. Možnosti '(AM/PM)' budú mať na niektorých platformách malý vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Voľná zhoda archívov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Pri vyhľadávaní položiek v zoznamoch skladieb spojených s komprimovanými súbormi zhodovať len názov archívneho súboru namiesto [názov súboru]+[obsah]. Povoľte na zabránenie duplicitným položkám histórie obsahu pri načítavaní komprimovaných súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Hľadať bez zhody jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Povoliť skenovanie a pridanie obsahu do zoznamu skladieb bez nainštalovaného podporujúceho core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "Skenovanie kontroluje CRC pri možných duplikátoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "Niektoré ISO duplikujú sériové čísla, hlavne pri PSP/PSN tituloch. Spoliehanie sa iba na sériové číslo môže spôsobiť, že skener vloží obsah do nesprávneho systému. Toto pridáva CRC kontrolu, ktorá výrazne spomalí skenovanie, ale môže byť presnejšia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Spravovať hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Vykonať údržbu na zoznamoch skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Prenosné hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Pri zapnutí, a keď je zvolený aj adresár 'Prehliadač súborov', sa aktuálna hodnota parametra 'Prehliadač súborov' uloží do zoznamu skladieb. Pri načítaní zoznamu skladieb na inom systéme s rovnakou možnosťou sa hodnota parametra 'Prehliadač súborov' porovná s hodnotou zo zoznamu skladieb; ak sa líšia, cesty položiek zoznamu skladieb sa automaticky opravia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "Použiť názvy súborov pre zhodu náhľadov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "Pri zapnutí sa náhľady budú vyhľadávať podľa názvu súboru položky, namiesto jej popisku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "Povoliť všetky podporované typy obrázkov pre náhľady"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "Pri zapnutí možno pridávať lokálne náhľady vo všetkých typoch obrázkov, ktoré RetroArch podporuje (napríklad jpeg). Môže to mať mierny vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "Spravovať"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Predvolené jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Určiť core, ktorý sa použije pri spustení obsahu cez položku zoznamu skladieb, ktorá nemá existujúce priradenie core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Resetovať priradenia core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Odstrániť existujúce priradenia core pre všetky položky zoznamu skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Režim zobrazenia popisku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Zmení, ako sa zobrazujú popisky obsahu v tomto zozname skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Metóda triedenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Určuje, ako sa položky v tomto zozname skladieb triedia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Vyčistiť hrací zoznam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Overí priradenia core a odstráni neplatné a duplicitné položky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Obnoviť hrací zoznam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Pridá nový obsah a odstráni neplatné položky opakovaním operácie skenovania obsahu naposledy použitej na vytvorenie alebo úpravu zoznamu skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Vymazať hrací zoznam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Odstrániť zoznam skladieb zo systému súborov."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Súkromie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Zmeniť nastavenia súkromia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Účty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Spravovať aktuálne nakonfigurované účty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Meno používateľa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Sem zadajte vaše používateľské meno. Bude použité okrem iného aj pre netplay relácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Jazyk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Nastaví jazyk používateľského rozhrania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Lokalizuje menu a všetky správy na obrazovke podľa zvoleného jazyka. Pre uplatnenie zmien je potrebný reštart.\nÚplnosť prekladu sa zobrazí pri každej možnosti. V prípade, že jazyk nie je pre položku menu implementovaný, použije sa angličtina."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Povoliť kameru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Povoliť cores prístup ku kamere."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Umožní aplikácii Discord zobrazovať údaje o prehrávanom obsahu.\nDostupné iba s natívnym desktop klientom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Povoliť polohu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Povoliť cores prístup k vašej polohe."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Získavajte achievementy v klasických hrách. Viac informácií na 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Prihlasovacie údaje pre váš účet RetroAchievements. Navštívte retroachievements.org a zaregistrujte sa zadarmo.\nPo registrácii zadajte používateľské meno a heslo do RetroArchu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK,
   "Hranie na Facebooku"
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Meno používateľa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Zadajte používateľské meno vášho účtu RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Heslo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Zadajte heslo vášho účtu RetroAchievements. Maximálna dĺžka: 255 znakov."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "YouTube stream key"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Twitch stream key"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Facebook Gaming stream key"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Systém/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOSy, boot ROM-y a iné systémovo špecifické súbory sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Prevzaté"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Stiahnuté súbory sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Assety"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Menu assety používané RetroArchom sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Dynamické pozadia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Obrázky pozadia použité v menu sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Miniatúry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Náhľady obalov, screenshotov a titulných obrazoviek sú uložené v tomto adresári."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Hlavný adresár"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Nastaviť počiatočný adresár pre prehliadač súborov."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Konfiguračné súbory"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Predvolený konfiguračný súbor je uložený v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Jadrá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Libretro cores sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Informácie o jadre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Súbory informácií o aplikácii/core sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Databázy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Databázy sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Súbory cheatov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Cheat súbory sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Video filtre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Video filtre založené na CPU sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Zvukové filtre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Audio DSP filtre sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Video shadery"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Video shadery založené na GPU sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Nahrávky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Nahrávky sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Konfigurácie nahrávania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Konfigurácie nahrávania sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Prekrytia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Overlaye sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Prekrytia klávesnice"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "Klávesnicové overlaye sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Rozloženie videa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Video rozloženia sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Snímky obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Screenshoty sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Profily herného ovládača"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Profily kontrolerov používané na automatickú konfiguráciu kontrolerov sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Premapovania vstupu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Vstupné remapy sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Zoznamy skladieb sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Zoznam obľúbených"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Uložiť zoznam Obľúbených do tohto adresára."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Hrací zoznam histórie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Uložiť zoznam Histórie do tohto adresára."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Hrací zoznam obrázkov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Uložiť zoznam Histórie obrázkov do tohto adresára."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Hrací zoznam hudby"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Uložiť zoznam Hudby do tohto adresára."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Hrací zoznam videa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Uložiť zoznam Videí do tohto adresára."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Logy času behu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Logy času behu sú uložené v tomto adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Súbory uloženia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Uložiť všetky súbory uloženia do tohto adresára. Ak nie je nastavené, pokúsi sa uložiť do pracovného adresára súboru obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Uložiť všetky súbory uloženia (*.srm) do tohto adresára. Zahŕňa to súvisiace súbory ako .rt, .psrm atď. Toto bude prepísané explicitnými parametrami príkazového riadku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Stavy uloženia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Save states a replays sú uložené v tomto adresári. Ak nie je nastavené, pokúsi sa ich uložiť do adresára, kde sa nachádza obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Vyrovnávacia pamäť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Archivovaný obsah bude dočasne extrahovaný do tohto adresára."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Logy systémových udalostí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Logy systémových udalostí sú uložené v tomto adresári."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Povoliť Rich Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Zdieľať váš aktuálny stav v RetroArchu na Steame."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Formát obsahu Rich Presence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Rozhodnite, aké informácie súvisiace s obsahom sa budú zdieľať."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Obsah"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Názov Jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Názov systému"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Obsah (názov systému)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Obsah (názov jadra)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Obsah (názov systému - názov jadra)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Pridať do zmiešavača"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Pridá túto zvukovú stopu do dostupného slotu zvukového streamu.\nAk momentálne nie sú dostupné žiadne sloty, ignoruje sa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Pridať do mixéra a prehrať"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Pridá túto zvukovú stopu do dostupného slotu zvukového streamu a prehrá ju.\nAk momentálne nie sú dostupné žiadne sloty, ignoruje sa."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Hostiteľ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Prihlásiť sa k hostiteľovi sieťovej hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Zadať adresu netplay servera a pripojiť sa v režime klienta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Odpojiť od netplay hostiteľa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Odpojí aktívne netplay pripojenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Filtre lobby"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Iba pripojiteľné miestnosti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Iba nainštalované cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Heslom chránené miestnosti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Obnoviť zoznam netplay hostiteľov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Vyhľadať netplay hostiteľov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Obnoviť zoznam netplay LAN"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Vyhľadať netplay hostiteľov v LAN."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Spustiť netplay hostiteľa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Spustí netplay v režime hostiteľa (servera)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Zastaviť netplay hostiteľa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "Vykopnúť klienta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Vyhodiť klienta z aktuálne hostovanej miestnosti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "Zablokovať klienta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Zablokovať klienta z aktuálne hostovanej miestnosti."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Prehľadať adresár"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Skenuje adresár pre obsah, ktorý zodpovedá databáze."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Prehľadať tento adresár>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "Vyberte na skenovanie aktuálneho adresára pre obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Prehľadať súbor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Skenuje súbor pre obsah, ktorý zodpovedá databáze."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Skenovanie obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Konfigurovateľné skenovanie založené na názvoch súborov obsahu a/alebo zhode s databázou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "Vyhľadať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD,
   "Metóda skenovania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_METHOD,
   "Automaticky alebo vlastné s detailnými možnosťami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB,
   "Kontrola databázy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_USE_DB,
   "Strict pridá iba položky zhodujúce sa so záznamom v databáze, Loose pridá aj súbory so správnou príponou bez CRC/sériového čísla, Custom DAT kontroluje voči používateľom poskytnutému XML súboru namiesto databáz, None ignoruje databázy a používa iba prípony súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT,
   "Databáza na zhodu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DB_SELECT,
   "Zhodovanie možno obmedziť na jednu konkrétnu databázu alebo na úplne prvú zhodujúcu sa databázu, čím sa skenovanie zrýchli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_TARGET_PLAYLIST,
   "Zoznam skladieb na aktualizáciu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_TARGET_PLAYLIST,
   "Výsledky sa pridajú do tohto zoznamu skladieb. V prípade Auto - Any sa môžu aktualizovať viaceré systémové zoznamy skladieb. Custom bez referencie na databázu nepripojí položky k žiadnej databáze v zozname skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SINGLE_FILE,
   "Skenovať jeden súbor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SINGLE_FILE,
   "Skenuje iba jeden súbor namiesto adresára. Po zmene tejto položky znovu vyberte umiestnenie obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_OMIT_DB_REF,
   "Preskočiť referencie na databázu zo zoznamu skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_OMIT_DB_REF,
   "V prípade vlastného názvu zoznamu skladieb vždy použiť názov zoznamu skladieb na vyhľadávanie náhľadov, aj keď bola nájdená zhoda s databázou."
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Pridať do zmiešavača"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Pridať do mixéra a prehrať"
   )

/* Import Content > Content Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Umiestnenie obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Vyberte adresár (alebo súbor) na skenovanie obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Cieľový hrací zoznam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Názov vygenerovaného súboru zoznamu skladieb, ktorý sa tiež používa na identifikáciu náhľadov zoznamu skladieb. Automatické nastavenie použije rovnaký názov ako zhodujúca sa databáza alebo adresár obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Vlastný názov zoznamu skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Vlastný názov zoznamu skladieb pre skenovaný obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Predvolené jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Vyberte predvolený core, ktorý sa použije pri spustení skenovaného obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Prípony súborov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Zoznam typov súborov, ktoré sa zahrnú do skenovania, oddelené medzerami. Ak je prázdny, zahŕňa všetky typy súborov, alebo ak je špecifikovaný core, všetky súbory podporované core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Hľadať rekurzívne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Pri zapnutí sa do skenovania zahrnú všetky podadresáre zadaného 'Adresára obsahu'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Hľadať v archívoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Pri zapnutí sa archívne súbory (.zip, .7z, atď.) prehľadajú na platný/podporovaný obsah. Môže to mať výrazný vplyv na výkon skenovania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Arcade DAT súbor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Vyberte Logiqx alebo MAME List XML DAT súbor na povolenie automatického pomenovávania skenovaného arcade obsahu (MAME, FinalBurn Neo, atď.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Filter Arcade DAT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Pri použití arcade DAT súboru sa do zoznamu skladieb pridá obsah iba ak sa nájde zhodný záznam v DAT súbore."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Prepísať existujúci zoznam skladieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Pri zapnutí sa pred skenovaním obsahu odstráni akýkoľvek existujúci zoznam skladieb. Pri vypnutí sa existujúce položky zoznamu skladieb zachovajú a pridá sa iba obsah, ktorý v zozname skladieb chýba."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Overiť existujúce položky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Pri zapnutí sa položky v akomkoľvek existujúcom zozname skladieb pred skenovaním nového obsahu overia. Položky odkazujúce na chýbajúci obsah a/alebo súbory s neplatnými príponami sa odstránia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Spustiť vyhľadanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Prehľadať vybraný obsah."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Inicializácia zoznamu..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Rok vydania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Počet hráčov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Región"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Štítok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Hľadať názov..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Ukázať všetko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Ďalší filter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Všetko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Pridať ďalší filter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u položiek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Podľa vývojára"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Podľa vydavateľa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Podľa roku vydania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Podľa počtu hráčov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Podľa žánru"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Podľa úspechov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Podľa kategórie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Podľa jazyka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Podľa regiónu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Podľa exkluzivity konzoly"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Podľa exkluzivity platformy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Podľa vibrácie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Podľa bodov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Podľa média"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Podľa ovládania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Podľa štýlu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Podľa hrateľnosti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Podľa naratívu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "Podľa pokroku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Podľa perspektívy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Podľa nastavenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Podľa vizuálu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Podľa vozidiel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Podľa pôvodu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Podľa franšízy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Podľa značky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Podľa názvu systému"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Nastaviť rozsahový filter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Zobrazenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Uložiť ako pohľad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Vymazať tento pohľad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Zadajte názov nového pohľadu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Pohľad s rovnakým názvom už existuje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "Pohľad bol uložený"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "Pohľad bol odstránený"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Spustiť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Spustiť obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Premenovať"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Premenovať názov položky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Odstrániť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Odstrániť túto položku zo zoznamu skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Pridať do obľúbených"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Pridať obsah do 'Obľúbených'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "Pridať do herného zoznamu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "Pridať obsah do zoznamu skladieb."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "Vytvoriť nový herný zoznam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "Vytvoriť nový zoznam skladieb a pridať doň aktuálnu položku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Nastaviť priradenie jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Nastaviť core priradený tomuto obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Resetovať priradenie core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Resetovať core priradený tomuto obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informácie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Zobraziť viac informácií o obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Stiahnuť miniatúry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Stiahnuť screenshot/obal/náhľady titulnej obrazovky pre aktuálny obsah. Aktualizuje existujúce náhľady."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Aktuálne jadro"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Názov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Cesta k súboru"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Položka: %lu/%lu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Jadro"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Čas hrania"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Naposledy hrané"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Databáza"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Pokračovať"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Pokračovať v obsahu a opustiť Quick Menu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Spustiť soft reset. RetroPad Start spustí hard reset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Zavrieť obsah"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Zatvoriť obsah. Akékoľvek neuložené zmeny môžu byť stratené."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Urobiť screenshot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Zachytiť obraz obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Uložený stav"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Zmena aktuálne vybraného slotu so stavom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Uložiť stav"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Uložiť stav do aktuálne vybraného slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVE_STATE,
   "Uložiť stav do aktuálne vybraného slotu. Poznámka: save states zvyčajne nie sú prenosné a nemusia fungovať s inými verziami tohto core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Načítať stav"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Načítať uložený stav z aktuálne vybraného slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_STATE,
   "Načítať uložený stav z aktuálne vybraného slotu. Poznámka: nemusí fungovať, ak bol stav uložený s inou verziou core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Vrátiť načítanie stavu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Ak bol stav načítaný, obsah sa vráti do stavu pred načítaním."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Vrátiť uloženie stavu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Ak bol stav prepísaný, vráti sa k predchádzajúcemu save state."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "Záznamový slot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "Zmena aktuálne vybraného slotu so stavom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Prehrať opakovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Prehrať súbor replay z aktuálne vybraného slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Nahrať replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Nahrá replay súbor do aktuálne vybraného slotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Zastaviť nahrávanie/prehrávanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "Zastaví nahrávanie/prehrávanie aktuálneho replay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Pridať do obľúbených"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Pridať obsah do 'Obľúbených'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Spustiť zaznamenávanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Spustiť nahrávanie videa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Zastaviť nahrávanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Zastaviť nahrávanie videa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Spustiť streamovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Spustiť streamovanie do zvoleného cieľa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Ukončiť stream"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Ukončiť stream."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Stavy uloženia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Prístup k možnostiam save state."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Voľby jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Zmeniť možnosti pre obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Ovládanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Zmeniť ovládače pre obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Cheaty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Nastaviť cheat kódy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Ovládanie disku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Správa obrazov diskov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Shadery"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Nastaviť shadery na vizuálne vylepšenie obrazu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Prepísania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Možnosti pre prepísanie globálnej konfigurácie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Úspechy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Zobraziť achievementy a súvisiace nastavenia."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Spravovať voľby jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Uložiť alebo odstrániť override možností pre aktuálny obsah."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Uložiť voľby hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Uložiť možnosti core, ktoré sa použijú iba pre aktuálny obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Odstrániť voľby hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Odstrániť možnosti core, ktoré sa použijú iba pre aktuálny obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Uložiť možnosti adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Uložiť možnosti core, ktoré sa použijú pre všetok obsah načítaný z rovnakého adresára ako aktuálny súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Odstrániť možnosti adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Odstrániť možnosti core, ktoré sa použijú pre všetok obsah načítaný z rovnakého adresára ako aktuálny súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Aktívny súbor volieb"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "Aktuálne použitý súbor možností."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Vynulovať voľby jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Nastaviť všetky možnosti aktuálneho core na predvolené hodnoty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Vyprázdniť možnosti na disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Vynútiť zápis aktuálnych nastavení do aktívneho súboru možností. Zaisťuje, že možnosti sú zachované v prípade, že chyba core spôsobí nesprávne ukončenie frontendu."
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Spravovať remap súbory"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Načítať, uložiť alebo odstrániť vstupné remap súbory pre aktuálny obsah."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Aktívny súbor premapovania"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "Aktuálne použitý remap súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Načítať remap súbor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Načítať a nahradiť aktuálne mapovania vstupu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Uložiť remap súbor ako"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Uložiť aktuálne mapovania vstupu ako nový remap súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Uložiť remap súbor core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Uložiť remap súbor, ktorý sa použije pre všetok obsah načítaný s týmto core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Odstrániť remap súbor core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Odstrániť remap súbor, ktorý sa použije pre všetok obsah načítaný s týmto core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Uložiť remap súbor adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Uložiť remap súbor, ktorý sa použije pre všetok obsah načítaný z rovnakého adresára ako aktuálny súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Odstrániť remap súbor adresára obsahu hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Odstrániť remap súbor, ktorý sa použije pre všetok obsah načítaný z rovnakého adresára ako aktuálny súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Uložiť remap súbor hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "Uložiť remap súbor, ktorý sa použije iba pre aktuálny obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Odstrániť remap súbor hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "Odstrániť remap súbor, ktorý sa použije iba pre aktuálny obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Resetovať mapovanie vstupu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Nastaviť všetky možnosti remapovania vstupu na predvolené hodnoty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Aktualizovať remap súbor vstupu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Prepísať aktívny remap súbor aktuálnymi možnosťami remapovania vstupu."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Súbor premapovania"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Spustiť alebo pokračovať vo vyhľadávaní cheatov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT,
   "Skenovať pamäť na vytvorenie nových cheatov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Načítať súbor cheatov (nahradiť)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Načítať súbor cheatov a nahradiť existujúce cheaty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Načítať súbor cheatov (pripojiť)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Načítať súbor cheatov a pripojiť k existujúcim cheatom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Znova načítať cheaty pre konkrétnu hru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS,
   "Znova načítať všetky existujúce cheaty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Uložiť súbor cheatov ako"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Uložiť aktuálne cheaty ako súbor cheatov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Pridať nový cheat na začiatok"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP,
   "Pridať cheat na začiatok zoznamu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Pridať nový cheat na koniec"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM,
   "Pridať cheat na koniec zoznamu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Vymazať všetky cheaty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL,
   "Vyčistiť zoznam cheatov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Automaticky aplikovať cheaty pri načítaní hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Automaticky aplikovať cheaty pri načítaní hry."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Aplikovať po prepnutí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Aplikovať cheat okamžite po prepnutí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Použiť zmeny"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Zmeny cheatov sa prejavia okamžite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Cheaty"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Spustiť alebo reštartovať vyhľadávanie cheatov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Stlačením vľavo alebo vpravo zmeníte veľkosť bitov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
   "Endianita vyššieho bitu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Stlačením vľavo alebo vpravo zmeníte hodnotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Rovné %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Menšie ako predtým"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Menšie alebo rovné ako predtým"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Väčšie ako predtým"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Väčšie alebo rovné ako predtým"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Rovné ako predtým"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Nerovné ako predtým"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Stlačením vľavo alebo vpravo zmeníte hodnotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Rovné ako predtým +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Hľadať hodnoty v pamäti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Stlačením vľavo alebo vpravo zmeníte hodnotu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Rovné ako predtým -%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Pridať %u zhôd do zoznamu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Vymazať zhodu #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Vytvoriť kódovú zhodu #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Adresa zhody: %08X Maska: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Súbor cheatov (nahradiť)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Súbor cheatov (pripojiť)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Podrobnosti cheatu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Pozícia cheatu v zozname."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Povolené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Popis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Spracovateľ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Veľkosť pamäťového vyhľadávania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Typ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Hodnota"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Pamäťová adresa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Prehliadať adresu: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Maska pamäťovej adresy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Bitová maska adresy, keď je veľkosť pamäťového vyhľadávania < 8 bitov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Počet iterácií"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Počet, koľkokrát sa cheat aplikuje. Použite spolu s ďalšími dvoma možnosťami 'Iterácia' na ovplyvnenie veľkých oblastí pamäte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Zvýšenie adresy pri každej iterácii"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Po každej iterácii sa 'Pamäťová adresa' zvýši o tento počet krát 'Veľkosť pamäťového vyhľadávania'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Zvýšenie hodnoty pri každej iterácii"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Po každej iterácii sa 'Hodnota' zvýši o túto sumu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Vibrovať pri pamäti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Hodnota vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Port vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Sila primárnych vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Trvanie primárnych vibrácií (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Sila sekundárnych vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Trvanie sekundárnych vibrácií (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Kód"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Pridať nový cheat za tento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Pridať nový cheat pred tento"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Skopírovať tento cheat za"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Skopírovať tento cheat pred"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Odstrániť tento cheat"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Aktuálny index disku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Vyberte aktuálny disk zo zoznamu dostupných obrazov. Virtuálny zásobník disku môže ostať zatvorený."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Vysunúť disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Otvoriť virtuálny zásobník disku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Vložte disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Zatvoriť virtuálny zásobník disku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Načítať nový disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Vyberte nový disk zo systému súborov a pridajte ho do zoznamu indexov.\nPOZNÁMKA: Toto je staršia funkcia. Pre tituly s viacerými diskami sa odporúča použiť M3U zoznamy skladieb."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Video shadery"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Povoliť pipeline video shaderov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Sledovať shader súbory pre zmeny"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Automaticky aplikovať zmeny vykonané v shader súboroch na disku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Sleduje shader súbory pre nové zmeny. Po uložení zmien shaderu na disk sa automaticky znovu skompiluje a aplikuje na obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Pamätať si naposledy použitý adresár shaderov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Otvoriť prehliadač súborov v naposledy použitom adresári pri načítaní predvolieb a passov shaderov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Načítať predvoľbu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Načítať predvoľbu shadera. Pipeline shaderov sa automaticky nastaví."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Načíta predvoľbu shadera priamo. Menu shaderov sa zodpovedajúcim spôsobom aktualizuje.\nFaktor mierky zobrazený v menu je spoľahlivý iba ak predvoľba používa jednoduché metódy škálovania (t.j. zdrojové škálovanie, rovnaký faktor mierky pre X/Y)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Pridať predvoľbu na začiatok"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Pridať predvoľbu na začiatok aktuálne načítanej predvoľby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Pridať predvoľbu na koniec"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Pridať predvoľbu na koniec aktuálne načítanej predvoľby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_MANAGER,
   "Spravovať predvoľby"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_MANAGER,
   "Uložiť alebo odstrániť predvoľby shaderov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_FILE_INFO,
   "Aktívny súbor predvoľby"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_FILE_INFO,
   "Aktuálne použitá predvoľba shaderov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Použiť zmeny"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Zmeny v konfigurácii shaderov sa prejavia okamžite. Použite, ak ste zmenili počet shader passov, filtrovanie, FBO mierku atď."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "Po zmene nastavení shaderov, ako počet shader passov, filtrovanie, FBO mierka, použite na aplikáciu zmien.\nZmena týchto nastavení shaderov je relatívne náročná operácia, takže sa musí vykonať explicitne.\nPri aplikácii shaderov sa nastavenia shaderov uložia do dočasného súboru (retroarch.slangp/.cgp/.glslp) a načítajú. Súbor pretrváva po ukončení RetroArchu a uloží sa do adresára shaderov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Parametre shadera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Upraviť aktuálny shader priamo. Zmeny sa neuložia do súboru predvoľby."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Shader passy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Zvýšiť alebo znížiť počet passov pipeline shaderov. K každému passu pipeline možno priradiť samostatné shadery a nakonfigurovať mierku a filtrovanie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch umožňuje miešať a kombinovať rôzne shadery s ľubovoľnými shader passmi, s vlastnými hardvérovými filtrami a faktormi mierky.\nTáto možnosť určuje počet shader passov, ktoré sa použijú. Ak je nastavené na 0 a použijete Aplikovať zmeny shaderov, použijete 'prázdny' shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "Cesta k shaderu. Všetky shadery musia byť rovnakého typu (t.j. Cg, GLSL alebo Slang). Nastavte adresár shaderov, kde má prehliadač začať hľadať shadery."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Filtrovať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "Hardvérový filter pre tento pass. Ak je nastavené 'Default', filter bude buď 'Linear' alebo 'Nearest' v závislosti od nastavenia 'Bilineárne filtrovanie' v nastaveniach videa."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Škálovať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "Mierka pre tento pass. Faktor mierky sa kumuluje, t.j. 2x pre prvý pass a 2x pre druhý pass dáva celkovú mierku 4x.\nAk je faktor mierky pre posledný pass, výsledok sa natiahne na obrazovku s predvoleným filtrom v závislosti od nastavenia Bilineárne filtrovanie v nastaveniach videa.\nAk je nastavené 'Default', použije sa buď mierka 1x alebo natiahnutie na celú obrazovku v závislosti od toho, či ide o posledný pass."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Jednoduché predvoľby"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Uložiť predvoľbu shadera s odkazom na pôvodne načítanú predvoľbu, ktorá obsahuje iba zmeny parametrov, ktoré ste vykonali."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Uložiť aktuálnu predvoľbu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Uložiť aktuálnu predvoľbu shadera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Uložiť predvoľbu ako"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Uložiť aktuálne nastavenia shaderov ako novú predvoľbu shadera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Uložiť globálnu predvoľbu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Uložiť aktuálne nastavenia shaderov ako predvolené globálne nastavenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Uložiť predvoľbu jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Uložiť aktuálne nastavenia shaderov ako predvolené pre tento core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Uložiť predvoľbu adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Uložiť aktuálne nastavenia shaderov ako predvolené pre všetky súbory v aktuálnom adresári obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Uložiť predvoľbu hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Uložiť aktuálne nastavenia shaderov ako predvolené nastavenia pre obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Žiadne automatické predvoľby shaderov nenájdené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Odstrániť globálnu predvoľbu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Odstrániť globálnu predvoľbu, ktorú používajú všetky obsahy a všetky cores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Odstrániť predvoľbu jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Odstrániť predvoľbu core, ktorú používa všetok obsah spustený s aktuálne načítaným core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Odstrániť predvoľbu adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Odstrániť predvoľbu adresára obsahu, ktorú používa všetok obsah v aktuálnom pracovnom adresári."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Odstrániť predvoľbu hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Odstrániť predvoľbu hry, ktorá sa používa iba pre konkrétnu hru."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Žiadne parametre shaderov"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "Aktívny override súbor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "Aktuálne použitý override súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "Načítať override súbor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "Načítať a nahradiť aktuálnu konfiguráciu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "Uložiť prepísania ako"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "Uložiť aktuálnu konfiguráciu ako nový súbor s prepismi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Uložiť prepisy jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Uloží súbor s prepisom konfigurácie, ktorý sa použije pre všetok obsah načítaný s týmto jadrom. Má prednosť pred hlavnou konfiguráciou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Odstrániť prepisy jadra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Odstráni súbor s prepisom konfigurácie, ktorý sa použije pre všetok obsah načítaný s týmto jadrom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Uložiť prepisy adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Uloží súbor s prepisom konfigurácie, ktorý sa použije pre všetok obsah načítaný z rovnakého adresára ako aktuálny súbor. Má prednosť pred hlavnou konfiguráciou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Odstrániť prepisy adresára obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Odstráni súbor s prepisom konfigurácie, ktorý sa použije pre všetok obsah načítaný z rovnakého adresára ako aktuálny súbor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Uložiť prepisy hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Uloží súbor s prepisom konfigurácie, ktorý sa použije iba pre aktuálny obsah. Má prednosť pred hlavnou konfiguráciou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Odstrániť prepisy hry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Odstráni súbor s prepisom konfigurácie, ktorý sa použije iba pre aktuálny obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "Zrušiť prepis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "Obnoviť všetky možnosti na hodnoty globálnej konfigurácie."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Žiadne úspechy na zobrazenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Zrušiť pozastavenie režimu hardcore úspechov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Ponechať režim hardcore úspechov povolený pre aktuálnu reláciu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Zrušiť obnovenie režimu hardcore úspechov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Ponechať režim hardcore úspechov zakázaný pre aktuálnu reláciu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "Obnovenie režimu hardcore úspechov zakázané"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "Pre obnovenie režimu hardcore úspechov musíte znovu načítať jadro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Pozastaviť režim hardcore úspechov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Pozastaví režim hardcore úspechov pre aktuálnu reláciu. Táto akcia povolí cheaty, pretáčanie, spomalený pohyb a načítavanie uložených stavov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Obnoviť režim hardcore úspechov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Obnoví režim hardcore úspechov pre aktuálnu reláciu. Táto akcia zakáže cheaty, pretáčanie, spomalený pohyb a načítavanie uložených stavov a reštartuje aktuálnu hru."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Server RetroAchievements je nedostupný"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Jedno alebo viac odomknutí úspechov sa nedostalo na server. Odomknutia sa budú opakovať, pokiaľ nechávate aplikáciu otvorenú."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "Server RetroAchievements je nedostupný. Bude sa to opakovať, pokiaľ nebude úspech alebo sa aplikácia neukončí."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "Všetky čakajúce požiadavky boli úspešne synchronizované so serverom RetroAchievements."
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "Identifikujem hru"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "Sťahujem dáta o hre"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "Spúštam reláciu"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Neprihlásený"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Chyba siete"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Neznáma hra"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Úspechy nemožno aktivovať s týmto jadrom"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "Hash RetroAchievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Položka databázy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Zobraziť informácie z databázy pre aktuálny obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Žiadne položky na zobrazenie"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Žiadne dostupné jadrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Žiadne dostupné možnosti jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Žiadne dostupné informácie o jadre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Žiadne dostupné zálohy jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Žiadne dostupné obľúbené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Žiadna dostupná história"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Žiadne dostupné obrázky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Hudba nedostupná"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Žiadne dostupné videá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Žiadne dostupné informácie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Žiadne dostupné položky zoznamu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Nenašli sa nastavenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Nenašli sa žiadne zariadenia Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Nebola nájdená žiadna sieť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Žiadne jadro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Hľadať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "Cyklovať náhľady"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RANDOM_SELECT,
   "Náhodný výber"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Späť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Nadradený adresár"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "Prejsť späť do nadradeného adresára."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Priečinok nebol nájdený."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Žiadne položky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Vybrať súbor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_NORMAL,
   "Normálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90 st."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180 st."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270 st."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_NORMAL,
   "Normálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90 st."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180 st."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270 st."
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Neznámy kompiler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Zdielať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Zápasy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Hlasovať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Zdieľanie analógového vstupu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Priemer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Žiadne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Bez preferencie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Odraz vľavo/vpravo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Rolovať vľavo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Režim obrazu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Režim reči"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Režim rozprávača"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "História & Obľúbené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Všetky hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "História & Obľúbené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Vždy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Nikdy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Podľa jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Zoskupiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Nabité"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Nabíjanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Vybíjanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "Bez zdroja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Použiť tento adresár>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "Vyberte toto pre nastavenie ako adresár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<adresár obsahu>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Predvolené>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Žiadne>"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
   "RetroKlávesnica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad s analógom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Žiadne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Neznáme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Dolu + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Podržať Štart (2 sekundy)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Podržať Select (2 sekundy)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Dolu + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Zakázané>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Zmeny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Nemení sa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Zvyšuje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Zníženia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= hodnota vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= hodnota vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< hodnota vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> hodnota vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Zvyšuje sa o hodnotu vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Znižuje sa o hodnotu vibrácií"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Všetko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<zakázané>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Nastaviť na hodnotu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Zvýšiť o hodnotu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Znížiť o hodnotu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Spustiť ďalší cheat, ak hodnota = pamäť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Spustiť ďalší cheat, ak hodnota != pamäť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Spustiť ďalší cheat, ak hodnota < pamäť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Spustiť ďalší cheat, ak hodnota > pamäť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Emulátor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Bit, maximálna hodnota = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-Bit, maximálna hodnota = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-Bit, maximálna hodnota = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-Bit, maximálna hodnota = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-Bit, maximálna hodnota = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-Bit, maximálna hodnota = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Predvolené systémom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Abecedne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Žiadne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Zobraziť úplné popisy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Odstrániť obsah ()"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Odstrániť obsah []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Odstrániť () a []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Zachovať región"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Ponechať index disku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Ponechať región a index disku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Predvolené systémom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Obal krabice"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Snímka obrazovky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Titulná obrazovka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "Logo obsahu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "Normálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Rýchlo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "ZAP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Áno"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Nie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Pravda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Nepravda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Povolené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Zakázané"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Zamknuté"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Odomknuté"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Neoficiálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Nepodporované"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Nedávno odomknuté"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Takmer hotovo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Aktívne výzvy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Iba sledovače"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Iba oznámenia"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Predvolené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Lineárny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Najbližšie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Hlavné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Obsah"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<adresár obsahu>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Vlastné>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Neurčené>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD_AUTO,
   "Plne automatické"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_STRICT,
   "Prísne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_LOOSE,
   "Voľné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT,
   "Vlastný DAT (striktný)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT_LOOSE,
   "Vlastný DAT (voľný)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_NONE,
   "Žiadne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_TARGET_PLAYLIST_CUSTOM,
   "<Vlastné>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Ľavý analóg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Ľavý analóg (vynútené)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Pravý analóg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Pravý analóg (vynútené)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG,
   "Ľavý + pravý analóg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG_FORCED,
   "Ľavý + pravý analóg (vynútené)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG,
   "Twin-Stick analóg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG_FORCED,
   "Twin-Stick analóg (vynútené)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "Kláves %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Myš 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Myš 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Myš 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Myš 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Myš 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Koliesko myši nahor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Koliesko myši nadol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Koliesko myši doľava"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Koliesko myši doprava"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Skoro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "Normálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Neskoro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "Vzad"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Hrúbka výplne pozadia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Zvýšiť hrubosť šachovnicového vzoru pozadia ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Výplň okraja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Hrúbka výplne okraja"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Zvýšiť hrubosť šachovnicového vzoru okraja ponuky."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Zobraziť okraj ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Použiť rozloženie na celú šírku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Zmeniť veľkosť a polohu položiek ponuky tak, aby sa čo najlepšie využil dostupný priestor obrazovky. Vypnite toto pre použitie klasického dvojstĺpcového rozloženia s pevnou šírkou."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Lineárny filter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Pridá miernu rozmazanosť do ponuky pre zjemnenie ostrých hrán pixelov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Vnútorné zväčšenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Zväčší rozhranie ponuky pred vykreslením na obrazovku. Pri použití so zapnutou možnosťou „Lineárny filter ponuky“ odstraňuje artefakty zväčšenia (nerovnomerné pixely) pri zachovaní ostrého obrazu. Má významný vplyv na výkon, ktorý sa zvyšuje s úrovňou zväčšenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Pomer strán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Vyberie pomer strán ponuky. Širokouhlé pomery zvyšujú horizontálne rozlíšenie rozhrania ponuky. (Môže vyžadovať reštart, ak je možnosť „Uzamknúť pomer strán ponuky“ vypnutá)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Uzamknúť pomer strán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Zabezpečí, aby sa ponuka vždy zobrazovala so správnym pomerom strán. Ak je vypnuté, rýchla ponuka bude roztiahnutá tak, aby zodpovedala aktuálne načítanému obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Farebná téma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Vyberte iný farebný motív. Voľba „Vlastný“ povoľuje použitie predvolených súborov motívu ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Vlastná predvoľba motívu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Vyberte predvoľbu motívu ponuky z Prehliadača súborov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Priehľadnosť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Povoliť zobrazenie obsahu na pozadí, kým je aktívna Rýchla ponuka. Vypnutie priehľadnosti môže zmeniť farby motívu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Tieňové efekty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Povoliť tiene pre text ponuky, okraje a náhľady. Má mierny vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Animácia pozadia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Povoliť efekt animácie častíc na pozadí. Má významný vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Rýchlosť animácie pozadia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Nastaviť rýchlosť efektov animácie častíc na pozadí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Animácia pozadia šetriča obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Zobraziť efekt animácie častíc na pozadí, kým je aktívny šetrič obrazovky ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Zobraziť náhľady zoznamu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Povoliť zobrazenie zmenšených náhľadov v texte pri prezeraní zoznamov. Prepínateľné pomocou RetroPad Select. Ak je vypnuté, náhľady možno stále prepnúť na celú obrazovku pomocou RetroPad Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Horná miniatúra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Typ náhľadu, ktorý sa zobrazí v pravom hornom rohu zoznamov. Môže sa cyklovať pomocou pravého analógu hore/vľavo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Spodná miniatúra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Typ náhľadu, ktorý sa zobrazí v pravom dolnom rohu zoznamov. Môže sa cyklovať pomocou pravého analógu dole/vpravo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Vymeniť miniatúry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Vymení polohy zobrazenia „Horný náhľad“ a „Dolný náhľad“."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Metóda zmenšovania náhľadov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Metóda prevzorkovania použitá pri zmenšovaní veľkých náhľadov, aby sa zmestili do zobrazenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Oneskorenie náhľadu (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Aplikuje časové oneskorenie medzi výberom položky zoznamu a načítaním jej priradených náhľadov. Nastavenie na hodnotu aspoň 256 ms umožňuje rýchle posúvanie bez oneskorenia aj na najpomalších zariadeniach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Podpora rozšíreného ASCII"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Povoliť zobrazenie neštandardných ASCII znakov. Vyžadované pre kompatibilitu s niektorými neanglickými západnými jazykmi. Má mierny vplyv na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Vymeniť ikony"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Použiť ikony namiesto textu ON/OFF na zobrazenie položiek nastavení ponuky typu „prepínač“."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Najbližší sused (rýchle)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "Bilineárna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (pomalé)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Žiadny"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (vycentrovať)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (vycentrovať)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (vycentrovať)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (vycentrovať)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (vycentrovať)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Prispôsobiť obrazovke"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Celočíselné škálovanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Vyplniť obrazovku (roztiahnuté)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Klasická červená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Klasická oranžová"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Klasická žltá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Klasická zelená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Klasická modrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Klasická fialová"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Klasická šedá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Tmavofialová"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Polnočná modrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Zlatý"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektrická modrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Jablková zelená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Sopečná červená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Lagúna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA,
   "Drakula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI,
   "Ploché UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackovanie jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT,
   "Bledá noc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ZENBURN,
   "Vypaľovanie zenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dynamické"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Šedá tmavá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Šedá svetlá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Dážď"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "Vír"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Sekundárna miniatúra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Typ náhľadu, ktorý sa zobrazí vľavo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "Náhľad ikony"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "Typ náhľadu ikony zoznamu, ktorý sa má zobraziť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Dynamické pozadie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Dynamicky načítať novú tapetu v závislosti od kontextu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Horizontálna animácia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Povoliť horizontálnu animáciu pre ponuku. Bude to mať dopad na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Animácia zvýraznenia horizontálnej ikony"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Animácia, ktorá sa spustí pri prechádzaní medzi kartami."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animácia pohybu hore/dole"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animácia, ktorá sa spustí pri pohybe hore alebo dole."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animácia otvárania/zatvárania hlavnej ponuky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animácia, ktorá sa spustí pri otváraní vedľajšej ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Faktor priehľadnosti farebného motívu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON,
   "Aktuálna ikona ponuky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_CURRENT_MENU_ICON,
   "Aktuálna ikona ponuky môže byť skrytá, pod horizontálnou ponukou alebo v nadpise hlavičky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NONE,
   "Žiadne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NORMAL,
   "Normálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_TITLE,
   "Nadpis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Písmo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Vyberte iné hlavné písmo pre použitie v ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Farba písma (červená)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Farba písma (zelená)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Farba písma (modrá)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Rozloženie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Vyberte iné rozloženie pre rozhranie XMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Téma ikon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Vyberte inú tému ikon pre RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Vymeniť ikony"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "Použiť ikony namiesto textu ON/OFF na zobrazenie položiek nastavení ponuky typu „prepínač“."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Efekty tieňov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Vykreslí tiene pre ikony, náhľady a písmená. Bude to mať malý dopad na výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Shader pipeline"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Vyberte animovaný efekt pozadia. V závislosti od efektu môže byť náročný na GPU. Ak je výkon nedostatočný, buď to vypnite, alebo sa vráťte k jednoduchšiemu efektu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Farebná téma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Vyberte iný motív farby pozadia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Vertikálne usporiadanie náhľadov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Zobrazí ľavý náhľad pod pravým, na pravej strane obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Mierka zväčšenia miniatúry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Zníži veľkosť zobrazenia náhľadov škálovaním maximálnej povolenej šírky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Faktor vertikálneho prelínania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Zobraziť hlavičku názvu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Okraj názvu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Horizontálny posun okraja názvu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Povoliť kartu Nastavenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Zobrazí kartu Nastavenia obsahujúcu nastavenia programu."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Stuha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Stuha (zjednodušené)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Jednoduchý sneh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Sneh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Snehová vločka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Vlastné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Čiernobiele"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Monochromatický invertovaný"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Systematicky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
   "Retro systém"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Automaticky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Automaticky invertované"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Jablková zelená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Tmavý"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Svetlý"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Ranná modrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Slnečný lúč"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Tmavofialová"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektrická modrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Zlatý"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Klasická červená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Polnočná modrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Obrázok pozadia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Pod morom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Sopečná červená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Limetková zelená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Rodinná červená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Ľadovo chladný"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Šedá tmavá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Šedá svetlá"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT,
   "Písmo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT,
   "Vyberte iné hlavné písmo pre použitie v ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE,
   "Mierka písma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE,
   "Určuje, či má veľkosť písma v ponuke vlastné škálovanie a či sa má škálovať globálne alebo so samostatnými hodnotami pre každú časť ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_GLOBAL,
   "Globálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_SEPARATE,
   "Samostatné hodnoty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Faktor škálovania písma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Lineárne škáluje veľkosť písma v celej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Faktor škálovania písma názvu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Škáluje veľkosť písma pre text názvu v hlavičke ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Faktor škálovania písma ľavého bočného panela"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Škáluje veľkosť písma pre text v ľavom bočnom paneli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Faktor škálovania písma popisov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Škáluje veľkosť písma pre popisy možností ponuky a položiek zoznamu. Tiež ovplyvňuje veľkosť textu v poliach pomocníka."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Faktor škálovania písma podpopisov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Škáluje veľkosť písma pre podpopisy možností ponuky a položiek zoznamu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TIME,
   "Faktor škálovania písma dátumu a času"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TIME,
   "Škáluje veľkosť písma indikátora času a dátumu v pravom hornom rohu ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Faktor škálovania písma päty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Škáluje veľkosť písma textu v päte ponuky. Tiež ovplyvňuje veľkosť textu v pravom bočnom paneli náhľadov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Zbaliť bočný panel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Mať ľavý bočný panel vždy zbalený."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Skrátiť názvy zoznamov (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Odstráni názvy výrobcov zo zoznamov. Napríklad „Sony - PlayStation“ sa stane „PlayStation“."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Triediť zoznamy po skrátení názvu (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Zoznamy budú znovu zoradené v abecednom poradí po odstránení časti názvu výrobcu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Sekundárna miniatúra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Nahradí panel metadát obsahu iným náhľadom."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Použiť posuvný text pre metadáta obsahu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Ak je povolené, každá položka metadát obsahu zobrazená v pravom bočnom paneli zoznamov (priradené jadro, čas hrania) zaberie jeden riadok; reťazce presahujúce šírku bočného panela sa zobrazia ako posuvný text. Ak je vypnuté, každá položka metadát obsahu sa zobrazí staticky, zalomená na toľko riadkov, koľko je potrebné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Mierka zväčšenia miniatúry"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Škáluje veľkosť panela náhľadov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_PADDING_FACTOR,
   "Faktor odsadenia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_PADDING_FACTOR,
   "Škáluje veľkosť horizontálneho odsadenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON,
   "Ikona hlavičky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_ICON,
   "Logo hlavičky môže byť skryté, dynamické v závislosti od navigácie alebo pevne nastavené na klasického invadera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR,
   "Oddeľovač hlavičky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_SEPARATOR,
   "Alternatívna šírka pre oddeľovače hlavičky a päty."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_NONE,
   "Žiadne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_DYNAMIC,
   "Dynamické"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_FIXED,
   "Pevné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NONE,
   "Žiadne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NORMAL,
   "Normálne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_MAXIMUM,
   "Maximálne"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Farebná téma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Vyberte iný farebný motív."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Základná biela"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Základná čierna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackovanie jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA,
   "Drakula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Šedá tmavá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Šedá svetlá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SELENIUM,
   "Selén"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Ikony"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Zobraziť ikony naľavo od položiek ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Vymeniť ikony"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "Použiť ikony namiesto textu ON/OFF na zobrazenie položiek nastavení ponuky typu „prepínač“."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Ikony zoznamov (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Zobraziť ikony špecifické pre systém v zoznamoch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Optimalizovať rozloženie na šírku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Automaticky upraviť rozloženie ponuky tak, aby lepšie pasovalo na obrazovku pri použití orientácie zobrazenia na šírku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Zobraziť navigačný panel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Zobrazí trvalé skratky navigácie ponuky na obrazovke. Umožňuje rýchle prepínanie medzi kategóriami ponuky. Odporúča sa pre dotykové zariadenia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Automaticky otáčať navigačný panel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Automaticky presunúť navigačný panel na pravú stranu obrazovky pri použití orientácie zobrazenia na šírku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Farebná téma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Vyberte iný motív farby pozadia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Animácia prechodu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Povoliť plynulé efekty animácie pri navigácii medzi rôznymi úrovňami ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Zobrazenie náhľadov na výšku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Určuje režim zobrazenia náhľadov zoznamu pri použití orientácie zobrazenia na výšku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Zobrazenie náhľadov na šírku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Určuje režim zobrazenia náhľadov zoznamu pri použití orientácie zobrazenia na šírku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Zobraziť sekundárny náhľad v zobrazeniach zoznamu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Zobrazí sekundárny náhľad pri použití režimov zobrazenia náhľadov zoznamu typu „Zoznam“. Toto nastavenie sa použije iba vtedy, keď má obrazovka dostatočnú fyzickú šírku na zobrazenie dvoch náhľadov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Pozadia náhľadov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Povolí vyplnenie nevyužitého priestoru v obrázkoch náhľadov plnou farbou pozadia. To zabezpečí jednotnú veľkosť zobrazenia pre všetky obrázky a vylepší vzhľad ponuky pri zobrazovaní zmiešaných náhľadov obsahu s rôznymi základnými rozmermi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Primárny náhľad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Hlavný typ náhľadu, ktorý sa priradí ku každej položke zoznamu. Zvyčajne slúži ako ikona obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Sekundárna miniatúra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Pomocný typ náhľadu, ktorý sa priradí ku každej položke zoznamu. Použitie závisí od aktuálneho režimu zobrazenia náhľadov zoznamu."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Modrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Modro šedá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Tmavomodrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Zelená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "Štít"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Červená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Žltá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Roztomilo červená"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hackovanie jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Šedá tmavá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Šedá svetlá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DRACULA,
   "Drakula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Zoslabiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Posunutie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Zoznam (malý)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Zoznam (stredný)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Duálna ikona"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Zoznam (malý)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Zoznam (stredný)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Zoznam (veľký)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Pracovná plocha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "VYP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "ZAP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Vylúčiť zobrazenia náhľadov"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Súbor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Načítať jadro..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "Odnačítať jadro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "&Ukončiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Upraviť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Hľadať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Zobrazenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Zatvorené doky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Parametre shadera"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Nastavenia..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Zapamätať si polohy dokov:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Zapamätať si geometriu okna:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Zapamätať si poslednú kartu prehliadača obsahu:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Téma:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Predvolené systémom>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Tmavý"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Vlastné..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Nastavenia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Nástroje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Nápoveda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "O programe RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Dokumentácia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Načítať vlastné jadro..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Načítať jadro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Načítavam jadro..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Názov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Verzia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Správca súborov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Hore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Hore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Prehliadač obsahu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Obal krabice"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Snímka obrazovky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Titulná obrazovka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Všetky hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Jadro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Informácie o jadre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Opýtať sa>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informácie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Upozornenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Chyba"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Chyba siete"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Pre uplatnenie zmien reštartujte program."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Záznam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 položiek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Sem pretiahnite obrázok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Nabudúce nezobrazovať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Priradiť jadro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Skryté hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Skryť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Zvýrazniť farbu:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "Vybrať..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Vyberte farbu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Vybrať tému"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Vlastná téma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Cesta súboru je prázdna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Súbor je prázdny."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Súbor nebolo možné otvoriť na čítanie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Súbor nebolo možné otvoriť na zápis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Súbor neexistuje."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Najprv navrhnúť načítané jadro:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Priblíženie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Zobrazenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Ikony"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Zoznam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Vyčistiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Priebeh:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "\"Všetky zoznamy\" max. počet položiek zoznamu:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "\"Všetky zoznamy\" max. počet položiek mriežky:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Zobraziť skryté súbory a priečinky:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Nový hrací zoznam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Zadajte názov nového zoznamu:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Vymazať hrací zoznam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Premenovať hrací zoznam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Naozaj chcete odstrániť zoznam \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Otázka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Súbor sa nepodarilo odstrániť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Súbor sa nepodarilo premenovať."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Zhromažďuje sa zoznam súborov..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Pridávajú sa súbory do zoznamu..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Položka hracieho zoznamy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Názov:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Cesta:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Jadro:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Databáza:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Rozšírenia:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(oddelené medzerami; predvolene zahŕňa všetko)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Filtrovať vnútri archívov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(používa sa na hľadanie náhľadov)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Naozaj chcete odstrániť položku \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Najprv vyberte jeden zoznam."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Vymazať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Pridať položku..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Pridať súbor(y)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Pridať priečinok..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Upraviť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Vyberte súbory"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Vybrať priečinok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<viac>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Chyba pri aktualizácii položky zoznamu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Vyplňte všetky povinné polia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "Aktualizovať RetroArch (nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch bol úspešne aktualizovaný. Pre uplatnenie zmien reštartujte aplikáciu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Aktualizácia zlyhala."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Prispievatelia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Aktuálny shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Posunúť nadol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Posunúť hore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Načítať"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Uložiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Odstrániť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Odstrániť priechody"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Použiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Pridať povolenie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Vyčistiť všetky priechody"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Žiadne shader priechody."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Obnoviť priechod"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Obnoviť všetky priechody"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Obnoviť parameter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Stiahnuť miniatúru"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Sťahovanie už prebieha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Spustiť na zozname:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Miniatúra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Limit vyrovnávacej pamäte náhľadov:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Limit veľkosti náhľadu pre drag-n-drop:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Stiahnuť všetky miniatúry"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Celý systém"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Tento hrací zoznam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Náhľady boli úspešne stiahnuté."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Úspešné: %1 Neúspešné: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Voľby jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Obnoviť všetko"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Nastavenia aktualizátora jadier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Účty Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Koncový bod zoznamu účtov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Počítadlá jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Nezistil sa disk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Počítadlá frontendu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Vodorovná ponuka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Skryť nepriradené popisy vstupov jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Zobraziť popisy popisov vstupov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Prekrytie na obrazovke"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "História"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Vyberte obsah zo zoznamu nedávnej histórie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Pri načítaní obsahu sa kombinácie obsahu a jadra libretro ukladajú do histórie.\nHistória sa ukladá do súboru v tom istom adresári ako konfiguračný súbor RetroArch. Ak pri spustení nebol načítaný žiadny konfiguračný súbor, história sa nebude ukladať ani načítavať a nebude existovať v hlavnej ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "Multimédiá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Podsystémy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Prístup k nastaveniam podsystému pre aktuálny obsah."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Aktuálny obsah: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Nenašli sa žiadni hostitelia netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Nenašli sa žiadni klienti netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Žiadne počítadlá výkonu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Žiadne hracie zoznamy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Pripojené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Port %d názov zariadenia: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Zobrazený názov zariadenia: %s\nKonfiguračný názov zariadenia: %s\nVID/PID zariadenia: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Nastavenia cheatov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Spustiť alebo pokračovať vo vyhľadávaní cheatov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Prehrať v mediálnom prehrávači"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "sekúnd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Spustiť jadro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Spustiť jadro bez obsahu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Navrhované jadrá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Komprimovaný súbor sa nepodarilo prečítať."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Používateľ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Max. počet obrázkov swapchain"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Povie video ovládaču, aby explicitne použil zadaný režim buffrovania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Maximálny počet obrázkov swapchain. Toto môže povedať video ovládaču, aby použil konkrétny režim video buffrovania.\nJeden buffer - 1\nDvojitý buffer - 2\nTrojitý buffer - 3\nNastavenie správneho režimu buffrovania môže mať veľký vplyv na oneskorenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "Čakateľné swapchain"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Napevno synchronizovať CPU a GPU. Znižuje latenciu na úkor výkonu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Max. oneskorenie snímky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Povie video ovládaču, aby explicitne použil zadaný režim buffrovania."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Upravuje samotnú predvoľbu shaderu aktuálne používanú v ponuke."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Predvoľba shaderu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Predvoľba shaderu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "Predvoľba shaderu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Prejsť na URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "URL cesta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Štart"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Prezývka: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Hľadá sa kompatibilný obsah..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "Jadro nenájdené"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "Nenašli sa hracie zoznamy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Nájdený kompatibilný obsah"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "Nepodarilo sa nájsť zodpovedajúci obsah ani podľa CRC ani podľa názvu súboru"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "Stav"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Systémová BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Nápoveda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "Vyčistiť"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "V menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "V hre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "V hre (pozastavené)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Hranie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Pozastavené"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netplay sa spustí po načítaní obsahu."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "Pred spustením netplay musí byť načítaný obsah."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Nepodarilo sa nájsť vhodné jadro alebo súbor obsahu, načítajte manuálne."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Váš grafický ovládač nie je kompatibilný s aktuálnym video ovládačom v RetroArch, prepína sa na ovládač %s. Pre uplatnenie zmien reštartujte RetroArch."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Inštalácia jadra bola úspešná"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Inštalácia jadra zlyhala"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Päťkrát stlačte vpravo pre odstránenie všetkých cheatov."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Globálna hlasitosť audio mixéra"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Skenovanie netplay dokončené."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Prepáčte, neimplementované: jadrá, ktoré nevyžadujú obsah, sa nemôžu zúčastniť netplay."
   )
MSG_HASH(
   MSG_NATIVE,
   "Natívne"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Prijatý neznámy netplay príkaz"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Súbor už existuje. Ukladám do záložného bufferu"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "Prijaté pripojenie od: \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Prijaté pripojenie od: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Mapovanie portu netplay úspešné"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "Externá sieť má súkromnú alebo zdieľanú adresu. Zvážte použitie relay servera."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Mapovanie UPnP portu netplay zlyhalo"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "Neboli zadané žiadne argumenty a nie je vstavaná ponuka, zobrazuje sa pomocník..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Vkladanie disku do mechaniky"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Čaká sa na klienta..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "K vašej miestnosti sa nedá pripojiť z internetu."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Opustili ste hru"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "Pripojili ste as ako hráč %u"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "Pripojili ste sa so vstupnými zariadeniami %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "Hráč %.*s opustil hru"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s sa pripojil ako hráč %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s sa pripojil so vstupnými zariadeniami %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYERS_INFO,
   "%d hráč(ov)"
   )
MSG_HASH(
   MSG_NETPLAY_SPECTATORS_INFO,
   "%d hráč(ov) (%d divákov)"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Pokus o pripojenie netplay zlyhal, pretože partner nepoužíva RetroArch alebo používa starú verziu RetroArch."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Partner netplay používa starú verziu RetroArch. Nedá sa pripojiť."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "VAROVANIE: Partner netplay používa inú verziu RetroArch. Ak nastanú problémy, použite rovnakú verziu."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Partner netplay používa iné jadro. Nedá sa pripojiť."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "VAROVANIE: Partner netplay používa inú verziu jadra. Ak nastanú problémy, použite rovnakú verziu."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Toto jadro nepodporuje netplay medzi týmito platformami"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Toto jadro nepodporuje netplay medzi rôznymi platformami"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Zadajte heslo netplay servera:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Zadajte správu chatu netplay:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Chcete povoliť pripojenie od používateľa:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Nesprávne heslo"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" sa odpojil"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Klient netplay sa odpojil"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Netplay odpojené"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "Nemáte povolenie hrať"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "Nie sú žiadne voľné sloty hráča"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "Požadované vstupné zariadenia nie sú dostupné"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Nedá sa prepnúť do herného režimu"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Partner netplay \"%s\" pozastavil"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Vaša prezývka sa zmenila na \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "Klient vykopnutý: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "Vykopnutie klienta zlyhalo: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "Klient zablokovaný: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "Zablokovanie klienta zlyhalo: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Hranie"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "Sleduje"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "Zariadenia"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "Chat podporovaný"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "Spôsobené spomalenia"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Hlasitosť zvuku"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Automatická detekcia"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Schopnosti"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Prihlásiť sa k hostiteľovi sieťovej hry"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Pripájam na port"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Slot pripojenia"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "Načítava sa zoznam jadier..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Nepodarilo sa získať zoznam jadier!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "Najnovšia verzia je už nainštalovaná: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Aktualizujem jadro: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Sťahujem jadro: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Rozbaľuje sa jadro: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Jadro nainštalované:"
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Inštalácia jadra zlyhala: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Skenujú sa jadrá..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Kontrola jadra: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Všetky nainštalované jadrá sú v najnovšej verzii"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Všetky podporované jadrá prepnuté na verzie Play Store"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "Aktualizované jadrá: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "Preskočené jadrá: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Aktualizácia jadra zakázaná - jadro je uzamknuté: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Obnovujú sa jadrá: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Obnovené jadrá: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Čistí sa zoznam: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Zoznam vyčistený: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Obnovenie zlyhalo - zoznam neobsahuje žiadny platný záznam skenovania: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Obnovenie zlyhalo - neplatný/chýbajúci adresár obsahu: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Obnovenie zlyhalo - neplatný/chýbajúci názov systému: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Obnovenie zlyhalo - neplatné jadro: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Obnovenie zlyhalo - neplatný/chýbajúci arkádový DAT súbor: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Obnovenie zlyhalo - arkádový DAT súbor je príliš veľký (nedostatok pamäte): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Pridané k obľúbeným"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Pridanie obľúbenej položky zlyhalo: zoznam je plný"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "Pridané do zoznamu"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Pridanie do zoznamu zlyhalo: zoznam je plný"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Jadro nastavené: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "Priradenie jadra k položke zoznamu bolo obnovené."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Disk pripojený"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Pripojenie disku zlyhalo"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Adresár aplikácie"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Aplikujú sa zmeny cheatov."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Aplikuje sa záplata: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Aplikuje sa shader"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Zvuk stlmený."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Zvuk zapnutý."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Chyba pri ukladaní profilu herného ovládača."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "Profil ovládača uložený ako \"%s\"."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Automatické ukladanie sa nepodarilo inicializovať."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Automatické uloženie stavu do"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Spúšťa sa príkazové rozhranie na porte"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Nedá sa odvodiť nová cesta konfigurácie. Použiť aktuálny čas."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "Porovnáva sa so známymi magickými číslami..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Skompilované oproti API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Adresár konfigurácie nie je nastavený. Nedá sa uložiť nová konfigurácia."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Pripojené k"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "CRC32 obsahu sa líšia. Nedajú sa použiť rôzne hry."
   )
MSG_HASH(
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "Hostiteľ používa inú hru."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "Vaše ping je príliš vysoké pre tohto hostiteľa."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Načítavanie obsahu preskočené. Implementácia ho načíta sama."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Jadro nepodporuje uložené stavy."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATE_UNDO,
   "Jadro nepodporuje vrátenie uloženého stavu."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
   "Jadro nepodporuje ovládanie disku."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Súbor možností jadra bol úspešne vytvorený."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "Súbor možností jadra bol úspešne odstránený."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Všetky možnosti jadra obnovené na predvolené."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "Možnosti jadra uložené do:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "Uloženie možností jadra zlyhalo do:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Nepodarilo sa nájsť žiadny ďalší ovládač"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Nepodarilo sa nájsť kompatibilný systém."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Nepodarilo sa nájsť platnú dátovú stopu"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "Nepodarilo sa otvoriť dátovú stopu"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "Nepodarilo sa prečítať súbor obsahu"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Nepodarilo sa prečítať hlavičku filmu."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Nepodarilo sa prečítať stav z filmu."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "Kontrolný súčet CRC32 sa nezhoduje medzi súborom obsahu a uloženým kontrolným súčtom obsahu v hlavičke súboru replay. Pri prehrávaní replay je vysoko pravdepodobná desynchronizácia."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Zadané vlastné časovanie"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Dekompresia už prebieha."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Dekompresia zlyhala."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Zistené zobrazenie"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Nenašiel sa platný patch obsahu."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Odpojte zariadenie z platného portu."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Virtuálna mechanika disku zatvorená."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Virtuálna mechanika disku otvorená."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Sťahovanie"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Sťahovanie zlyhalo"
   )
MSG_HASH(
   MSG_ERROR,
   "Chyba"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Libretro jadro vyžaduje obsah, ale nebol poskytnutý žiadny."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Libretro jadro vyžaduje špeciálny obsah, ale nebol poskytnutý žiadny."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Jadro nepodporuje VFS a načítanie z lokálnej kópie zlyhalo"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Chyba pri spracovaní argumentov."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Chyba pri ukladaní súboru možností jadra."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "Chyba pri odstraňovaní súboru možností jadra."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Chyba pri ukladaní súboru remapu."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Chyba pri odstraňovaní súboru remapu."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Chyba pri ukladaní predvoľby shaderu."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Adresár externej aplikácie"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Extrahujem"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Rozbaľuje sa súbor"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Uloženie konfigurácie zlyhalo do"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Nepodarilo sa prijať prichádzajúceho diváka."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Nepodarilo sa alokovať pamäť pre patchnutý obsah..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Nepodarilo sa aplikovať shader."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Nepodarilo sa aplikovať predvoľbu shaderu:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Nepodarilo sa naviazať soket."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Nepodarilo sa vytvoriť adresár."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Nepodarilo sa rozbaliť obsah z komprimovaného súboru"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Nepodarilo sa získať prezývku od klienta."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Načítanie zlyhalo."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Obsah sa nepodarilo načítať."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_FROM_PLAYLIST,
   "Nepodarilo sa načítať zo zoznamu."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Nepodarilo sa načítať súbor filmu."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Nepodarilo sa načítať prekrytie."
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "Klávesnicové prekrytie nie je nastavené."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Nepodarilo sa načítať stav z"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Nepodarilo sa otvoriť libretro jadro"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Patch zlyhal"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Nepodarilo sa prijať hlavičku od klienta."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Nepodarilo sa prijať prezývku."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Nepodarilo sa prijať prezývku od hostiteľa."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Nepodarilo sa prijať veľkosť prezývky od hostiteľa."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "Nepodarilo sa prijať dáta SRAM od hostiteľa."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Nepodarilo sa odstrániť disk z mechaniky."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Nepodarilo sa odstrániť dočasný súbor"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "Nepodarilo sa uložiť SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "Nepodarilo sa načítať SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Nepodarilo sa uložiť stav do"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Nepodarilo sa odoslať prezývku."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Nepodarilo sa odoslať veľkosť prezývky."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Nepodarilo sa odoslať prezývku klientovi."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Nepodarilo sa odoslať prezývku hostiteľovi."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "Nepodarilo sa odoslať dáta SRAM klientovi."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Nepodarilo sa spustiť audio ovládač. Bude sa pokračovať bez zvuku."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Nepodarilo sa spustiť záznam filmu."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Nepodarilo sa spustiť nahrávanie."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Nepodarilo sa zachytiť snímok obrazovky."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Nepodarilo sa vrátiť načítanie stavu."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Nepodarilo sa vrátiť uloženie stavu."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Nepodarilo sa zapnúť zvuk."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Prijatá závažná chyba v"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Súbor nenájdený"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Nájdené automatické uloženie stavu v"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Nájdená menovka disku"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Nájdená prvá dátová stopa v súbore"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Nájdený posledný slot so stavom"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "Nájdený posledný slot replay"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "Nie z aktuálneho nahrávania"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "Nekompatibilné s replay"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_FUTURE_STATE,
   "Nedá sa načítať budúci stav počas prehrávania"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_WRONG_TIMELINE,
   "Chyba nesprávnej časovej osi počas prehrávania"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_OVERWRITING_REPLAY,
   "Nesprávna časová os; prepisuje sa nahrávanie"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT,
   "Prehľadávať spätne"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT_FAILED,
   "Posun späť zlyhal"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT,
   "Vyhľadať dopredu"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT_FAILED,
   "Posun vpred zlyhal"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME,
   "Posun dokončený"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME_FAILED,
   "Posun zlyhal"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Nájdený shader"
   )
MSG_HASH(
   MSG_FRAMES,
   "Snímky"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Možnosti jadra špecifické pre hru nájdené v"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Možnosti jadra špecifické pre priečinok nájdené v"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Získaný neplatný index disku."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Stav zachytenia myši"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Zameranie hry zapnuté"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Game Focus vypnutý"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Libretro jadro je hardvérovo renderované. Musí sa použiť aj nahrávanie po shadingu."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "Nafúknutý kontrolný súčet sa nezhoduje s CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Vstupný cheat"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Názov súboru vstupného cheatu"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Názov súboru vstupnej predvoľby"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "Názov súboru vstupného prepisu"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "Názov súboru vstupného remapu"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Premenovať názov"
   )
MSG_HASH(
   MSG_INTERFACE,
   "Rozhranie"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Interné úložisko"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Vymeniteľné úložisko"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Neplatná veľkosť prezývky."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "v bajtoch"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "v megabajtoch"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "v gigabajtoch"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "je skompilované oproti inej verzii libretro než táto implementácia libretro."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Frontend pre libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Stav načítaný zo slotu: %d."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Stav načítaný zo slotu: Auto."
   )
MSG_HASH(
   MSG_LOADING,
   "Načítavanie"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Jeden alebo viac súborov firmvéru chýba"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Načítavam súbor obsahu"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Načítava sa súbor histórie"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Načítava sa súbor obľúbených"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Načítavam stav"
   )
MSG_HASH(
   MSG_MEMORY,
   "Pamäť"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "Vstupný súbor filmu replay nie je platný súbor REPLAY."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Formát vstupného filmu replay zrejme používa inú verziu serializátora. S najväčšou pravdepodobnosťou zlyhá."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Prehrávanie vstupného filmu replay skončilo."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Zastavuje sa záznam filmu."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Nepodarilo sa inicializovať netplay."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "Jadro nepodporuje netplay."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Žiadny obsah, spúšťa sa fiktívne jadro."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Zatiaľ nebol prepísaný žiadny uložený stav."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Zatiaľ nebol načítaný žiadny stav."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Chyba pri ukladaní prepisov."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "Chyba pri odstraňovaní prepisov."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Prepisy boli úspešne uložené."
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "Prepisy boli úspešne odstránené."
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "Prepisy boli úspešne uvoľnené."
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "Nič na uloženie. Prepisy neuložené."
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "Neukladá sa. Prepisy sú aktívne."
   )
MSG_HASH(
   MSG_PAUSED,
   "Pozastavené."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Číta sa prvá dátová stopa..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Nahrávanie ukončené z dôvodu zmeny veľkosti."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Záznam do"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Presmerovanie súboru cheatov do"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Presmerovanie uloženého súboru do"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Presmerovanie uloženého stavu do"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Súbor remapu bol úspešne uložený."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Súbor remapu bol úspešne odstránený."
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "Všetky možnosti remapu vstupu obnovené na predvolené."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Disk odstránený z mechaniky."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Odstraňuje sa dočasný súbor obsahu"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Reštartuje sa nahrávanie z dôvodu reinicializácie ovládača."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Starý uložený stav obnovený."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Shadery: obnovuje sa predvolená predvoľba shaderu na"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Vracia sa adresár uložených súborov na"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Vracia sa adresár uložených stavov na"
   )
MSG_HASH(
   MSG_REWINDING,
   "Pretáčam."
   )
MSG_HASH(
   MSG_REWIND_BUFFER_CAPACITY_INSUFFICIENT,
   "Nedostatočná kapacita bufferu."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "Pretáčanie nie je dostupné, pretože toto jadro nepodporuje serializáciu uloženého stavu."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Inicializuje sa buffer pretáčania s veľkosťou"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Nepodarilo sa inicializovať buffer pretáčania. Pretáčanie bude zakázané."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "Implementácia používa vláknový zvuk. Nedá sa použiť pretáčanie."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Dosiahnutý koniec bufferu pretáčania."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Konfigurácia uložená do"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Stav uložený do slotu: %d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Stav uložený do slotu: Auto."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Úspešne uložené do"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Ukladá sa typ RAM"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Ukladám stav"
   )
MSG_HASH(
   MSG_SCANNING,
   "Prehľadávam"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Skenovanie adresára dokončené."
   )
MSG_HASH(
   MSG_SCANNING_NO_DATABASE,
   "Skenovanie neúspešné, nenašla sa žiadna databáza."
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Posielam príkaz"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Niekoľko patchov je explicitne definovaných, ignorujú sa všetky..."
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Predvoľba shaderu bola úspešne uložená."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Spomalený záber."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Pretočiť dopredu."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Pretáčanie spomalene."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Preskakuje sa načítanie SRAM."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM nebude uložená."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Blokovať prepis SRAM"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Spúšťa sa prehrávanie filmu."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Spúšťa sa záznam filmu do"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Veľkosť stavu"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Slot stavu"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "Prehrať slot"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Zachytáva sa snímok obrazovky."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Snímka uložená"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Úspech odomknutý"
   )
MSG_HASH(
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "Vzácny úspech odomknutý"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "Pokus o rebríček zahájený"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "Pokus o rebríček zlyhal"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "Odoslané %s pre %s" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "Hodnotenie: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "Najlepšie: %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Zmeniť typ miniatúry"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Náhľady na celú obrazovku"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Prepnúť metadáta"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Nedostupný náhľad"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "Všetky možné stiahnutia náhľadov už boli vyskúšané pre túto položku zoznamu."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Stlačte znova pre ukončenie..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_CLOSE_CONTENT,
   "Stlačte znova pre zatvorenie obsahu..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_RESET,
   "Stlačte znova pre reštart..."
   )
MSG_HASH(
   MSG_TO,
   "do"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Načítanie stavu vrátené."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Vracia sa uložený stav"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Neznáme"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Spustené"
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Prijatý nerozpoznaný príkaz \"%s\".\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Používa sa názov jadra pre novú konfiguráciu."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Používa sa fiktívne libretro jadro. Preskakuje sa nahrávanie."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Pripojte zariadenie z platného portu."
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Reštartujem..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Prebieha vypínanie..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Verzia libretro API"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Výpočet veľkosti zobrazenia zlyhal! Bude sa pokračovať s nespracovanými dátami. Toto pravdepodobne nebude fungovať správne..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Nepodarilo sa otvoriť virtuálnu mechaniku disku."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Nepodarilo sa zatvoriť virtuálnu mechaniku disku."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Automaticky sa načíta uložený stav z"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "Automatické načítanie uloženého stavu z \"%s\" zlyhalo."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "Automatické načítanie uloženého stavu z \"%s\" bolo úspešné."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s nakonfigurované v porte %u"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s odpojené z portu %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) nenakonfigurované"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) nenakonfigurované, používa sa záloha"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Prehľadávanie Bluetooth ukončené."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "Párovanie odstránené. Reštartujte RetroArch pre opätovné pripojenie/párovanie."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Prehľadanie Wi-Fi ukončené."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Skenujú sa bluetooth zariadenia..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Skenujú sa bezdrôtové siete..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Povoľujem Wi-Fi..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Vypínam Wi-Fi..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Odpájam Wi-Fi..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Hľadajú sa netplay hostitelia..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Pripravuje sa skenovanie obsahu..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Zadajte heslo"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Heslo je správne."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Nesprávne heslo."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Zadajte heslo"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Heslo je správne."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Nesprávne heslo."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "Prepis konfigurácie načítaný."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Súbor remapu hry načítaný."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Súbor remapu adresára obsahu načítaný."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Súbor remapu jadra načítaný."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "Možnosti remapu vstupu uložené do:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "Uloženie možností remapu vstupu zlyhalo do:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Run-Ahead povolené. Odstránené snímky oneskorenia: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Run-Ahead povolené so sekundárnou inštanciou. Odstránené snímky oneskorenia: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Run-Ahead zakázané."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Run-Ahead bolo zakázané, pretože toto jadro nepodporuje uložené stavy."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "Run-Ahead nie je dostupné, pretože toto jadro nepodporuje deterministické uložené stavy."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Uloženie stavu zlyhalo. Run-Ahead bolo zakázané."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Načítanie stavu zlyhalo. Run-Ahead bolo zakázané."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Vytvorenie druhej inštancie zlyhalo. Run-Ahead teraz použije iba jednu inštanciu."
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "Preemptive Frames povolené. Odstránené snímky oneskorenia: %u."
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "Preemptive Frames zakázané."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Preemptive Frames bolo zakázané, pretože toto jadro nepodporuje uložené stavy."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "Preemptive Frames nie je dostupné, pretože toto jadro nepodporuje deterministické uložené stavy."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "Nepodarilo sa alokovať pamäť pre Preemptive Frames."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "Uloženie stavu zlyhalo. Preemptive Frames bolo zakázané."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "Načítanie stavu zlyhalo. Preemptive Frames bolo zakázané."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Skenovanie súboru dokončené."
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Vyhľadávanie cheatov úspešne spustené."
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Spustenie vyhľadávania cheatov zlyhalo."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Vyhľadávanie nebolo inicializované/spustené."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Nový počet zhôd = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "Pridaných %u zhôd."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Pridanie zhôd zlyhalo."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Kód vytvorený zo zhody."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Vytvorenie kódu zlyhalo."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Zhoda odstránená."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Nedostatok miesta. Maximálny počet súčasných cheatov je 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Nový cheat pridaný na začiatok zoznamu."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Nový cheat pridaný na koniec zoznamu."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Všetky cheaty odstránené."
   )
MSG_HASH(
   MSG_CHEAT_RELOAD_ALL_SUCCESS,
   "Všetky cheaty znovu načítané."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Nový cheat pridaný pred tento."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Nový cheat pridaný za tento."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Cheat skopírovaný pred tento."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Cheat skopírovaný za tento."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Cheat vymazaný."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Nepodarilo sa nastaviť disk."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Nepodarilo sa nastaviť posledný použitý disk."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "Pripojenie ku klientovi zlyhalo."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Pripojenie k hostiteľovi zlyhalo."
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "Netplay hostiteľ je plný."
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "Tento hostiteľ vás zablokoval."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Nepodarilo sa prijať hlavičku od hostiteľa."
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "RetroAchievements: Prihlásený ako \"%s\"."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Pre načítavanie stavov musíte pozastaviť alebo zakázať režim hardcore úspechov."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_SAVEFILE_PREVENTED_BY_HARDCORE_MODE,
   "Pre načítavanie srm uložení musíte pozastaviť alebo zakázať režim hardcore úspechov."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Bol načítaný uložený stav. Režim hardcore úspechov bol zakázaný pre aktuálnu reláciu."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Bol aktivovaný cheat. Režim hardcore úspechov bol zakázaný pre aktuálnu reláciu."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST,
   "Režim hardcore úspechov bol zmenený hostiteľom."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST,
   "Netplay hostiteľ musí byť aktualizovaný. Režim hardcore úspechov zakázaný pre aktuálnu reláciu."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "Zvládnuté %s"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "Dokončené %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Režim hardcore úspechov povolený, uložené stavy a pretáčanie boli zakázané."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "Táto hra nemá žiadne úspechy."
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "Všetkých %d úspechov aktivovaných pre túto reláciu"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "Aktivovaných %d neoficiálnych úspechov"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "Máte odomknutých %d z %d úspechov"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "%d nepodporované"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_WARNING,
   "Zistené nepodporované úspechy. Skúste iné jadro alebo aktualizujte RetroArch."
)
MSG_HASH(
   MSG_CHEEVOS_RICH_PRESENCE_SPECTATING,
   "Sleduje %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "Hardcore pozastavený. Manuálne nastavenie oneskorenia video snímky nie je povolené."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL,
   "Hardcore pozastavený. Vsync swap interval nad 1 nie je povolený."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION,
   "Hardcore pozastavený. Vkladanie čiernych snímok nie je povolené."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED,
   "Hardcore pozastavený. Nastavenie nie je povolené: %s=%s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE,
   "Hardcore pozastavený. Nemôžete získať hardcore úspechy za %s pomocou %s"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "RetroAchievements: Hru sa nepodarilo identifikovať."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "Načítanie hry RetroAchievements zlyhalo: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "Zmena média RetroAchievements zlyhala: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_LOGIN_TOKEN_EXPIRED,
   "Prihlásenie do RetroAchievements vypršalo. Znova zadajte heslo a znovu načítajte hru."
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "Najnižsie"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Nižšie"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "Normálne"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Vyššie"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Najvyššie"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Varovanie: Chýbajúce prostriedky, použite Online aktualizátor, ak je dostupný."
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Varovanie: Chýbajúce písma pre vybraný jazyk, použite Online aktualizátor, ak je dostupný."
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Varovanie: Nepodporovaný jazyk – používa sa angličtina."
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Kopírovanie disku..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Disk %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Najprv načítajte jadro."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Nepodarilo sa čítať z mechaniky. Dump prerušený."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Nepodarilo sa zapisovať na disk. Dump prerušený."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "V mechanike nie je vložený žiadny disk."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Predvoľba shaderu bola úspešne odstránená."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Chyba pri odstraňovaní predvoľby shaderu."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Vybraný neplatný arkádový DAT súbor."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "Vybraný arkádový DAT súbor je príliš veľký (nedostatok voľnej pamäte)."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Nepodarilo sa načítať arkádový DAT súbor (neplatný formát?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Neplatná konfigurácia manuálneho skenovania."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Nezistený žiadny platný obsah."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Prehľadávam obsah: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Kontrolujú sa aktuálne položky: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Prehľadávam: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Čistia sa záznamy M3U: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Prehľadávanie ukončené: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Skenuje sa jadro: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Záloha nainštalovaného jadra už existuje: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Zálohuje sa jadro: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Odstraňujú sa zastarané zálohy: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Záloha jadra dokončená: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "Vybraná záloha jadra je už nainštalovaná: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Obnovujem jadro: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Obnovenie jadra dokončené: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "Vybraný súbor jadra je už nainštalovaný: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Inštalujem jadro: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Inštalácia jadra dokončená: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "Vybraný neplatný súbor jadra: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Záloha jadra zlyhala: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Obnovenie jadra zlyhalo: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Inštalácia jadra zlyhala: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Obnovenie jadra zakázané - jadro je uzamknuté: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "Inštalácia jadra zakázaná - jadro je uzamknuté: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Uzamknutie jadra zlyhalo: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Odomknutie jadra zlyhalo: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "Odstránenie jadra zo zoznamu „Jadrá bez obsahu“ zlyhalo: "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "Pridanie jadra do zoznamu „Jadrá bez obsahu“ zlyhalo: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "Odstránenie jadra zakázané - jadro je uzamknuté: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Nepodporovaný video režim"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "Nedá sa zapisovať do adresára informácií jadra - vyrovnávacia pamäť informácií jadra bude zakázaná"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "Nájdený stav záznamu v"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Načítava sa stav záznamu z"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "Nepodarilo sa vstúpiť do GameMode"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "Nepodarilo sa vstúpiť do GameMode - uistite sa, že je GameMode démon nainštalovaný/spustený"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "Synchronizácia s presnou snímkovou frekvenciou obsahu povolená."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "Synchronizácia s presnou snímkovou frekvenciou obsahu zakázaná."
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "Obnovovacia frekvencia videa zmenená na %s Hz."
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Aktualizovať Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Názov frontendu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Verzia Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Reštart"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Rozdeliť Joy-Con"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Prepis škálovania grafických widgetov"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Použiť manuálny prepis faktora škálovania pri vykresľovaní zobrazovacích widgetov. Aplikuje sa iba vtedy, keď je možnosť „Automaticky škálovať grafické widgety“ zakázaná. Môže sa použiť na zväčšenie alebo zmenšenie veľkosti ozdobených upozornení, indikátorov a ovládacích prvkov nezávisle od samotnej ponuky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Rozlíšenie obrazovky"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Rozlíšenie obrazovky: Predvolené"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Rozlíšenie obrazovky: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Rozlíšenie obrazovky: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Aplikuje sa: Predvolené"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Aplikuje sa: %dx%d\nSTART pre obnovenie"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Aplikuje sa: %dx%d - %s\nSTART pre obnovenie"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Obnovuje sa na: Predvolené"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Obnovuje sa na: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Obnovuje sa na: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Vyberte režim zobrazenia (vyžaduje reštart)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Vypnúť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Povoliť prístup k externým súborom"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Otvoriť nastavenia oprávnení prístupu k súborom Windows"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Otvorte nastavenia oprávnení Windows na povolenie funkcie broadFileSystemAccess."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Otvoriť..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Otvoriť iný adresár pomocou systémového výberu súborov"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Filter blikania"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Video gama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Mäkký filter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Vyhľadať bluetooth zariadenia a pripojiť ich."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Vyhľadať bezdrôtové siete a nadviazať pripojenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Povoliť Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Pripojiť k sieti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Pripojiť k sieti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Odpojiť"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "Nastaviť šírku obrazovky VI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Korekcia overscanu (hore)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Upraví orezanie overscanu zobrazenia znížením veľkosti obrazu o určený počet skenovacích riadkov (z hornej časti obrazovky). Môže spôsobiť artefakty škálovania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Korekcia overscanu (dole)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Upraví orezanie overscanu zobrazenia znížením veľkosti obrazu o určený počet skenovacích riadkov (zo spodnej časti obrazovky). Môže spôsobiť artefakty škálovania."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Režim trvalého výkonu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "Výkon a napájanie CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "Politika"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Riadiaci režim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Ručne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Umožňuje manuálne nastaviť každý detail v každom CPU: governor, frekvencie atď. Odporúča sa iba pre pokročilých používateľov."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Výkon (riadený)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Predvolený a odporúčaný režim. Maximálny výkon počas hrania, šetrenie energie počas pozastavenia alebo prehliadania ponúk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Vlastný riadený"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Umožňuje vybrať, ktoré governory sa použijú v ponukách a počas hry. Performance, Ondemand alebo Schedutil sa odporúčajú počas hry."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Maximálny výkon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Vždy maximálny výkon: najvyššie frekvencie pre najlepší zážitok."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Minimálne napájanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Použiť najnižšiu dostupnú frekvenciu pre šetrenie energie. Užitočné pri zariadeniach napájaných z batérie, ale výkon bude výrazne znížený."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Vyvážené"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Prispôsobuje sa aktuálnemu zaťaženiu. Funguje dobre s väčšinou zariadení a emulátorov a pomáha šetriť energiu. Náročné hry a jadrá môžu na niektorých zariadeniach zaznamenať pokles výkonu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Minimálna frekvencia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Maximálna frekvencia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Minimálna frekvencia jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Maximálna frekvencia jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "Regulátor procesora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Regulátor jadra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Regulátor ponuky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "Herný režim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Môže zlepšiť výkon, znížiť oneskorenie a opraviť praskanie zvuku. Pre fungovanie potrebujete https://github.com/FeralInteractive/gamemode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Povolenie Linux GameMode môže zlepšiť oneskorenie, opraviť praskanie zvuku a maximalizovať celkový výkon automatickým nakonfigurovaním vášho CPU a GPU pre najlepší výkon.\nPre fungovanie musí byť nainštalovaný softvér GameMode. Informácie o tom, ako nainštalovať GameMode, nájdete na https://github.com/FeralInteractive/gamemode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Použiť režim PAL60"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Reštartovať RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Ukončite a potom reštartujte RetroArch. Vyžaduje sa pre aktiváciu niektorých nastavení ponuky (napríklad pri zmene ovládača ponuky)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Blokovať snímky"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Uprednostniť predný dotyk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "Použiť predný dotyk namiesto zadného."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Dotyk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Mapovanie ovládača klávesnice"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Typ mapovania ovládača klávesnice"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Malá klávesnica"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Časový limit blokovania vstupu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Počet milisekúnd čakania na získanie kompletnej vzorky vstupu. Použite, ak máte problémy so súčasnými stlačeniami tlačidiel (iba Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Zobraziť 'Reštart'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Zobraziť voľbu 'Reštart'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Zobraziť 'Vypnúť'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Zobraziť voľbu 'Vypnút'."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Zaheslované"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "Internet (nepripojiteľné)"
   )
MSG_HASH(
   MSG_LOCAL,
   "Miestne"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Stav internej pamäte: čítanie/zápis"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Stav internej pamäte: iba čítanie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Jas obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Zvýšiť alebo znížiť jas obrazovky."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "CPU pretaktovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Pretaktovanie CPU Switcha."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Určuje stav Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Služby"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Spravujte služby na úrovni operačného systému."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Zdieľať sieťové priečinky cez protokol SMB."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Použite SSH pre vzdialený prístup k príkazovému riadku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Prístupový bod Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Povoliť alebo zakázať Wi-Fi prístupový bod."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Časové pásmo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Vyberte časové pásmo pre úpravu dátumu a času podľa vašej polohy."
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
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "Zobrazí zoznam dostupných časových pásiem. Po výbere časového pásma sa čas a dátum upraví podľa vybraného časového pásma. Predpokladá sa, že systémové/hardvérové hodiny sú nastavené na UTC."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Voľby pre Nintendo Switch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Spravovať možnosti špecifické pre Nintendo Switch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "Pretaktovanie CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "Povoliť pretaktovacie frekvencie CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "Podpora pre CEC"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "Povoliť CEC Handshaking s TV pri pripojení do dokovacej stanice"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "Zakázať Bluetooth ERTM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "Zakážte Bluetooth ERTM pre opravu párovania niektorých zariadení"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Vypína sa Wi-Fi prístupový bod."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Odpája sa od Wi-Fi „%s“"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Pripája sa k Wi-Fi „%s“"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Žiadne SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Wi-Fi prístupový bod je už spustený"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Wi-Fi prístupový bod nebeží"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Spúšťa sa Wi-Fi prístupový bod s SSID=%s a Passkey=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Nepodarilo sa vytvoriť konfiguračný súbor Wi-Fi prístupového bodu."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Nesprávny konfiguračný súbor - nepodarilo sa nájsť APNAME alebo PASSWORD v %s"
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Mierka myši"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Upraviť mierku x/y pre rýchlosť svetelnej zbrane Wiimote."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Stupnica dotyku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "Upraviť mierku x/y súradníc dotykovej obrazovky pre prispôsobenie sa škálovaniu zobrazenia na úrovni OS."
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "VMouse dotyk ako ukazovateľ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "Povoliť odovzdávanie udalostí dotyku zo vstupnej dotykovej obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "VMouse dotyk ako myš"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "Povoliť emuláciu virtuálnej myši pomocou udalostí vstupného dotyku."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Režim touchpadu VMouse dotyku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Povoľte spolu s myšou pre využitie dotykovej obrazovky ako touchpadu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Režim trackballu VMouse dotyku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Povoľte spolu s myšou pre využitie dotykovej obrazovky ako trackballu, pridáva zotrvačnosť ukazovateľu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "Gestá VMouse dotyku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "Povoliť gestá dotykovej obrazovky vrátane ťukania, ťukania s ťahaním a švihania prstom."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "RGA škálovanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA škálovanie a bikubické filtrovanie. Môže rozbiť widgety."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Škálovanie špecifické pre kontext"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Hardvérové škálovanie kontextu (ak je dostupné)."
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "Povoliť New3DS Clock / L2 vyrovnávaciu pamäť"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "Povoliť rýchlosť hodín New3DS (804MHz) a L2 vyrovnávaciu pamäť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "Spodná obrazovka 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Povoliť zobrazenie informácií o stave na spodnej obrazovke. Vypnite pre predĺženie životnosti batérie a zlepšenie výkonu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "Režim zobrazenia 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Vyberá medzi 3D a 2D režimami zobrazenia. V režime „3D“ sú pixely štvorcové a pri prezeraní Rýchlej ponuky sa použije efekt hĺbky. Režim „2D“ poskytuje najlepší výkon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (efekt pixelovej mriežky)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (vysoké rozlíšenie)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Dotknite sa dotykovej obrazovky, \naby ste prešli do ponuky Retroarch"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "Prostriedok(y) nenájdené"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Žiadne \núdaje"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "Žiadny\nsnímok obrazovky"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Pokračovať v hre"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Vytvoriť\nbod obnovy"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Načítať \nbod obnovy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "Adresár prostriedkov spodnej obrazovky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "Adresár prostriedkov spodnej obrazovky. Adresár musí obsahovať \"bottom_menu.png\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "Povolenie písma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "Zobraziť písmo spodnej ponuky. Povoľte pre zobrazenie popisov tlačidiel na spodnej obrazovke. Toto vylučuje dátum uloženého stavu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Farba písma červená"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Upraviť červenú farbu písma spodnej obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Farba písma zelená"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Upraviť zelenú farbu písma spodnej obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Farba písma modrá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Upraviť modrú farbu písma spodnej obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Priehľadnosť farby písma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "Upraviť priehľadnosť písma spodnej obrazovky."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "Mierka písma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "Upraviť mierku písma spodnej obrazovky."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Skenovanie je dokončené.<br><br>\nAk chcete, aby bol obsah správne naskenovaný, musíte:\n<ul><li>mať už stiahnuté kompatibilné jadro</li>\n<li>mať aktualizované „Informačné súbory o jadre“ cez Online Updater</li>\n<li>aktualizovať \"Databázy\" cez Online Updater</li>\n<li>reštartovať RetroArch, ak ste práve vykonali čokoľvek z vyššie uvedeného</li></ul>\nNakoniec, obsah sa musí zhodovať s existujúcimi databázami z <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">nachádyajúcimi sa tu</a>. Ak to stále nefunguje, <a href=\"https://www.github.com/libretro/RetroArch/issues\">odošlite hlásenie o chybe</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "Dotyková myš je povolená"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "Dotyková myš je zakázaná"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "sdl2 mikrofón vyžaduje sdl2 audio ovládač"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "RetroArch prístupnosť zapnutá.  Hlavná ponuka načítať jadro."
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "zastavené."
   )
#ifdef HAVE_GAME_AI
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_MENU_OPTION,
   "Prepis AI hráča"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_MENU_OPTION,
   "Podpopis prepisu AI hráča"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_GAME_AI_OPTIONS,
   "Herné AI"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P1,
   "Prepísať p1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P1,
   "Prepísať hráča 01"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P2,
   "Prepísať p2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P2,
   "Prepísať hráča 02"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_SHOW_DEBUG,
   "Zobraziť ladenie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG,
   "Zobraziť ladenie"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_GAME_AI,
   "Zobraziť 'Herné AI'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_GAME_AI,
   "Zobraziť možnosť „Game AI“."
   )
#endif
#ifdef HAVE_SMBCLIENT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SETTINGS,
   "Nastavenia siete SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SETTINGS,
   "Konfigurovať nastavenia sieťového zdieľania SMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_ENABLE,
   "Povoliť SMB klienta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_ENABLE,
   "Povoliť prístup k sieťovému zdieľaniu SMB. Ethernet sa dôrazne odporúča pred Wi-Fi pre spoľahlivejšie pripojenie. Poznámka: zmena týchto nastavení vyžaduje reštart RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SERVER,
   "SMB server"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SERVER,
   "IP adresa alebo názov hostiteľa servera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SHARE,
   "Názov zdieľania SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SHARE,
   "Názov sieťového zdieľania, ku ktorému sa pripojiť."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SUBDIR,
   "SMB podadresár (voliteľné)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SUBDIR,
   "Cesta k podadresáru na zdieľaní."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_USERNAME,
   "SMB meno"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_USERNAME,
   "Používateľské meno pre overenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_PASSWORD,
   "SMB heslo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_PASSWORD,
   "Heslo pre overenie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_WORKGROUP,
   "SMB skupina"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_WORKGROUP,
   "Pracovná skupina alebo názov domény."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_AUTH_MODE,
   "Režim overenia SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_AUTH_MODE,
   "Vyberte overenie použité vo vašom prostredí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_NUM_CONTEXTS,
   "Max. počet pripojení SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_NUM_CONTEXTS,
   "Vyberte maximálny počet pripojení použitých vo vašom prostredí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_TIMEOUT,
   "Časový limit SMB"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_TIMEOUT,
   "Vyberte predvolený časový limit v sekundách."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_BROWSE,
   "Prehľadávať SMB zdieľanie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_BROWSE,
   "Prehľadávať súbory na nakonfigurovanom SMB zdieľaní."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SMB_CLIENT,
   "Zobraziť „SMB klient“"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SMB_CLIENT,
   "Zobraziť nastavenia „SMB klient“."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SMB_SHARE,
   "SMB zdieľanie"
   )
#endif

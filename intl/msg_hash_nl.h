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
   "Hoofdmenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favorieten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Geschiedenis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Afbeeldingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Muziek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Video's"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Ontdekken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Inhoudloze Cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importeer Inhoud"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Snelmenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Snelle toegang tot alle relevante in-game instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Laad core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Selecteer welke core te gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "Ontlaad core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Laat de geladen core los."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Bladeren naar een libretro core implementatie. Waar de browser begint hangt af van uw Core Directory pad. Indien leeg, zal het beginnen in root.\nAls Core Directory een map is, zal het menu die gebruiken als bovenste map. Als Core Directory een volledig pad is, zal het starten in de map waar het bestand zich bevindt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Laad content"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Selecteer welke content te starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Blader naar de inhoud. Om inhoud te laden heb je een 'Core' nodig om te gebruiken en een inhoudsbestand.\nOm te bepalen waar het menu begint te bladeren voor inhoud, stel 'Bestandsbrowserdirectory' in. Als dit niet is ingesteld, zal het starten in hoofdbestand.\nDe browser zal extensies filteren voor de laatste core die is ingesteld in 'Load Core', en die core gebruiken wanneer de inhoud wordt geladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Laad Disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Laad een fysieke mediadisk. Selecteer eerst de core (Laad Core) die met de disk zal worden gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Dump Disk"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Dump de fysieke mediadisk naar interne opslag. Het wordt opgeslagen als een image."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Schijf Uitwerpen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Schijf uitwerpen uit fysieke CD/DVD schijf."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Gescande content die overeenkomt met de database zal hier verschijnen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Importeer Inhoud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Afspeellijsten maken en bijwerken door inhoud te scannen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Bureaubladmenu weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Open het traditionele bureaublad menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Kioskmodus uitschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Toon alle configuratie gerelateerde instellingen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Download add-ons, componenten en inhoud voor RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Netplay Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Doe mee of organiseer een netplay-sessie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Instellingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Configureer het programma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Toon systeeminformatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Configuraties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Beheer en creëer configuratiebestanden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Hulp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Leer meer over hoe het werkt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Opnieuw opstarten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Herstart RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Sluit RetroArch. Configuratie opslaan bij het afsluiten staat aan."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Sluit RetroArch. Configuratie opslaan bij het afsluiten staat uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Sluit RetroArch af. Het programma stoppen op een harde manier (SIGKILL, enz.) zal RetroArch stoppen zonder de configuratie op te slaan. Op Unix-likes kan SIGINT/SIGTERM een schone de-initialisatie toestaan, wat configuratie opslaan bij het afsluiten inhoud als dat aan staat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_NOW,
   "Synchroniseer nu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_NOW,
   "Handmatig cloud synchronisatie activeren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Conflicten oplossen: lokale versie behouden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Los alle conflicten op door lokale bestanden te uploaden naar de server."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Conflicten oplossen: versie van de server behouden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Los alle conflicten op door serverbestanden te downloaden, waardoor lokale kopieën worden vervangen."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Een Core Downloaden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Download en installeer een core van de online updater."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Installer of Herstel een Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Installeer of herstel een core vanuit de 'Downloads' map."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Start Videoprocessor"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Start Externe RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Favorieten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Open Archief"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Laad Archief"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favorieten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Inhoud die u aan 'Favorieten' heeft toegevoegd, wordt hier weergegeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Muziek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Muziek die eerder is afgespeeld, wordt hier weergegeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Afbeeldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Afbeeldingen die eerder zijn bekeken, worden hier weergegeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Video's"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Video's die eerder zijn afgespeeld, worden hier weergegeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Ontdekken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Bekijk alle inhoud die overeenkomt met de database via een gecategoriseerde zoekinterface."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Inhoudloze Cores"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Geïnstalleerde cores die kunnen werken zonder inhoud te laden verschijnen hier."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Geïnstalleerde Cores Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Werk alle geïnstalleerde cores bij naar de nieuwste beschikbare versie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Schakel Cores over naar Play Store Versies"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Vervang alle oudere en handmatig geïnstalleerde cores door de nieuwste versies uit de Play Store, indien beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Afspeellijst Miniatuurbijwerker"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Download miniaturen voor items in de geselecteerde afspeellijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Inhoud-downloader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Download gratis content voor de geselecteerde core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Core-systeembestanden Downloader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Download hulpsysteembestanden die zijn nodig voor correcte/optimale core operatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Core Info Bestanden Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Assets Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Controllerprofielen Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Cheats Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Databases Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Overlays Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "GLSL Shaders Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Cg Shaders Bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Slang Shaders Bijwerken"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Core Informatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Bekijk informatie betreffende de applicatie/core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Schijf Informatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Informatie over ingevoegde media-schijven weergeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Netwerk Informatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Toon netwerkinterface(s) en bijbehorende IP-adressen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Systeeminformatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Bekijk informatie bepaald voor het apparaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Database Beheerder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Databases bekijken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Cursor Beheerder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Vorige zoekopdrachten bekijken."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Core Naam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Core Versie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Systeemnaam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Systeemfabrikant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Categorieën"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Auteurs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Machtigingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Licentie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Ondersteunde Extensies"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Vereiste Graphics API"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Volledig pad"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Onderbrekingspunt-Ondersteuning"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Basis (Opslaan/Laden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Geserialiseerd (Opslaan/Laden, Terugspoelen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Deterministisch (Opslaan/Laden, Terugspoelen, Run-Ahead, Netplay)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "Opmerking: 'Systeembestanden staan in de Inhoudsmap' staat aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "Aan het zoeken in %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Ontbrekend, Noodzakelijk:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Ontbrekend, Optioneel:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Aanwezig, Benodigd:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Aanwezig, Optioneel:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Geïnstalleerde Core Vergrendelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Voorkom wijziging van de momenteel geïnstalleerde core. Kan worden gebruikt om ongewenste updates te voorkomen wanneer de inhoud een specifieke core versie vereist (bijv.  Arcade ROM sets) of als de eigen onderbrekingspunt-indeling van de core veranderd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Uitsluiten van 'inhoudloze Cores' menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Voorkomen dat deze core wordt weergegeven in het menu \"Inhoudloze Cores\". Alleen toepasbaar als de weergavemodus is ingesteld op 'Aangepast'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Verwijder core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Verwijder deze core permanent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Back-up Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Maak een gearchiveerde back-up van de momenteel geïnstalleerde core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Back-up Herstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Installeer een eerdere versie van de core uit een lijst met gearchiveerde back-ups."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Back-up Verwijderen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Verwijder een bestand uit de lijst met gearchiveerde back-ups."
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Build datum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "RetroArch versie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Git versie"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Compilator"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "CPU model"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "CPU Eigenschappen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "CPU Architectuur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT beschikbaar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUNDLE_IDENTIFIER,
   "Bundel id"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Front-end identificatie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Energie bron"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Display metric breedte (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Display metric hoogte (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "DPI Weergeven"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Naam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Omschrijving"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Achievements Lijst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Categorie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Taal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Regio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Console-exclusief"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Platform-exclusief"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Besturingselementen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Kunststijl"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Verhaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Perspectief"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Instelling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Visueel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Voertuigtype"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Uitgever"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Ontwikkelaar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Afkomst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "TGDB-Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Famitsu Magazine Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Edge Magazine Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Edge Magazine Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Edge Magazine Nummer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Verschijningsdatum Maand"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Verschijningsdatum Jaar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "BBFC Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "ESRB Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "ELSPA Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "PEGI Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Hardware-verbeteringen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "CERO Beoordeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Serienummer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analoge besturing ondersteund"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Rumble ondersteund"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Co-op ondersteund"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Configuratie Laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Bestaande configuratie laden en huidige waarden vervangen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Huidige Configuratie Opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Overschrijf het huidige configuratiebestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Nieuwe configuratie opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Huidige configuratie opslaan in apart bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_AS_CONFIG,
   "Configuratie opslaan als"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG,
   "Huidige configuratie opslaan als aangepast configuratiebestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_MAIN_CONFIG,
   "Hoofdconfiguratie opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG,
   "Huidige configuratie opslaan als hoofdconfiguratie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Fabrieksinstellingen resetten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Huidige configuratie terugzetten naar standaardwaarden."
   )

/* Main Menu > Help */

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Omhoog Scrollen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Omlaag Scrollen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Bevestigen/OK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Menu Omschakelen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Toetsenbord Omschakelen"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Stuurprogramma's"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Wijzig stuurprogramma's die door het systeem worden gebruikt."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Instellingen voor video-uitvoer wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Geluid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Wijzig de instellingen voor audio-invoer/uitvoer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Invoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Wijzig controller-, toetsenbord- en muisinstellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latentie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Wijzig de instellingen met betrekking tot video, audio en invoervertraging."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Core-instellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Configuratie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Wijzig standaard instellingen voor configuratiebestanden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Instellingen voor opslaan wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Cloud-synchronisatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Wijzig cloud-synchronisatie instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Zet Cloud-synchronisatie aan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Probeer om configuraties, sram en staat aan naar cloud opslagprovider te synchroniseren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Destructieve Cloud-synchronisatie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Synchronisatie: opgeslagen bestanden/slagstaten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Synchronisatie: Configuratiebestanden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Synchronisatie: Miniatuurafbeeldingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Synchronisatie: Systeembestanden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "Wanneer ingeschakeld, worden opgeslagen bestanden/Onderbrekingspunt naar de cloud gesynchroniseerd."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "Wanneer ingeschakeld, worden configuratiebestanden gesynchroniseerd naar de cloud."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "Indien ingeschakeld, worden miniatuurafbeeldingen naar de cloud gesynchroniseerd. Niet aanbevolen behalve voor grote collecties van aangepaste miniatuurafbeeldingen; anders is de miniatuurdownloader een betere keuze."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "Indien ingeschakeld, zullen systeembestanden worden gesynchroniseerd naar de cloud. Dit kan de tijd die nodig is om te synchroniseren aanzienlijk verlengen; gebruik met voorzichtigheid."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Indien uitgeschakeld, worden bestanden verplaatst naar een backupmap voordat ze worden overschreven of verwijderd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE,
   "Synchronisatiemodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_MODE,
   "Automatisch: Synchroniseer bij het opstarten van RetroArch en wanneer cores ontladen worden. Handmatig: synchroniseren alleen wanneer de knop 'Nu synchroniseren' handmatig geactiveerd is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_AUTOMATIC,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_MANUAL,
   "Handmatig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Cloud-synchronisatie Backend"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Welk netwerkprotocol te gebruiken voor cloudopslag."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "Cloudopslag URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "De URL voor het API-startpunt voor de cloud opslagdienst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Gebruikersnaam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "Je gebruikersnaam voor je cloudopslagaccount."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Wachtwoord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "Uw wachtwoord voor uw cloudopslagaccount."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ACCESS_KEY_ID,
   "Toegangssleutel-ID"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ACCESS_KEY_ID,
   "Je toegangssleutel-ID voor uw cloudopslagaccount."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SECRET_ACCESS_KEY,
   "Geheime toegangssleutel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SECRET_ACCESS_KEY,
   "Je geheime toegangssleutel voor je cloudopslagaccount."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_S3_URL,
   "Je S3 eindpunt URL voor cloudopslag."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Loggen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Logboekinstellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Bestandsbeheer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Instellingen voor bestandsbeheer wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Configuratie bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Gecomprimeerd archief bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Configuratiebestand opnemen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Database cursor bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Configuratie bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Shader preset-bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Shader bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Remap besturingsbestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Cheat bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Overlay bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Database bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "TrueType lettertype bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Gewoon bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Video. Selecteer het om dit bestand te openen met de videospeler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Muziek. Selecteer het om dit bestand te openen met de muziekspeler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Afbeelding bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Afbeelding. Selecteer het om dit bestand te openen met de afbeeldingsviewer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Libretro core. Door dit te selecteren wordt deze core geassocieerd met het spel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Libretro core. Selecteer dit bestand om RetroArch deze core te laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Map. Selecteer het om deze map te openen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Instellingen voor terugspoelen, snel vooruitspoelen en slow motion wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Opname"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Opnameinstellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Onscreen Weergave"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Wijzig de scherm- en toetsenbord-overlay, en instellingen voor meldingen op het scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Gebruikersinterface"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Instellingen voor de gebruikersinterface wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "AI-Service"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Wijzig de instellingen voor de AI-Service (Vertaling/TTS/Misc)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Toegankelijkheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Wijzig de instellingen voor de Toegankelijkheidsverteller."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Energiebeheer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Instellingen voor stroombeheer wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Achievements Lijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Wijzig prestatie-instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Netwerk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Wijzig server- en netwerkinstellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Wijzig afspeellijstinstellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Gebruiker"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Privacy, account en gebruikersnaam instellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Map"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Wijzig de standaardmappen waar bestanden zich bevinden."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Koppeling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Prestatie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Geluid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Opslagruimte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Systeem"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Wijzig de instellingen met betrekking tot Steam."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Invoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Invoerstuurprogramma om te gebruiken. Sommige videostuurprogramma's dwingen een ander invoerstuurprogramma af. (Opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "De udev driver leest evdev gebeurtenissen voor toetsenbord ondersteuning. Het ondersteunt ook callback, muizen en touchpads.\nStandaard in de meeste distro's zijn /dev/input nodes alleen toegankelijk voor root (mode 600). U kunt een udev regel instellen die deze toegankelijk maakt voor niet-root."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "De linuxraw input driver vereist een actieve TTY. Toetsenbord gebeurtenissen worden direct van de TTY gelezen wat het eenvoudiger maakt, maar niet zo flexibel als udev. Muizen, enz. worden helemaal niet ondersteund. Deze driver gebruikt de oudere joystick API (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Invoerstuurprogramma. Het videostuurprogramma kan een ander invoerstuurprogramma afdwingen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Te gebruiken Controller driver (opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "DirectInput controller stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Rawe Linux driver, gebruikt de oude joystick API. Gebruik udev indien mogelijk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Linux stuurprogramma voor controllers aangesloten op een parallelle poort via speciale adapters."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Controller stuurprogramma gebaseerd op SDL-bibliotheken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Controller driver met udev interface, algemeen aanbevolen. Gebruikt de recente evdev joypad API voor joystick ondersteuning. Het ondersteunt hotplugging en force feedback.\nStandaard in de meeste distro's zijn /dev/input nodes alleen root-only (mode 600). Je kan een udev regel instellen die deze toegankelijk maakt voor niet-root."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "XInput controller driver. Vooral voor XBox controllers."
   )

MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "OpenGL 1.x stuurprogramma. Minimale versie vereist: OpenGL 1.1. Ondersteunt geen shaders. Gebruik nieuwere OpenGL stuurprogramma's, indien mogelijk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "OpenGL 2.x stuurprogramma. Dit stuurprogramma staat toe dat libretro GL cores worden gebruikt naast software cores. Minimale versie vereist: OpenGL 2.0 of OpenGLES 2.0. Ondersteunt het GLSL shader formaat. Gebruik liever de glcore driver indien mogelijk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "OpenGL 3.x stuurprogramma. Dit stuurprogramma staat toe dat libretro GL cores worden gebruikt naast software cores. Minimale versie vereist: OpenGL 3.2 of OpenGLES 3.0+. Ondersteunt het Slang shader formaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Vulkan stuurprogramma. Dit stuurprogramma staat toe dat libretro Vulkan cores worden gebruikt naast software cores. Minimale versie vereist: Vulkan 1.0. Ondersteunt HDR en Slang shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "SDL 1.2 software-rendered stuurprogramma. Performance wordt als suboptimaal beschouwd. Overweeg het alleen als laatste redmiddel te gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "SDL 2 software-rendered stuurprogramma. Prestaties voor software libretro core implementaties zijn afhankelijk van jouw platform SDL implementatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Metal stuurprogramma voor Apple platformen. Ondersteunt het Slang shader formaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Direct3D 8 stuurprogramma zonder shader ondersteuning."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Direct3D 9 stuurprogramma met ondersteuning voor het oude Cg shader formaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Direct3D 9 stuurprogramma met ondersteuning voor het HLSL shader formaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Direct3D 10 driver met ondersteuning voor Slang shader formaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Direct3D 11 stuurprogramma met ondersteuning voor HDR en het Slang shader formaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Direct3D 12 stuurprogramma met ondersteuning voor HDR en het Slang shader formaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "DispmanX stuurprogramma. Gebruikt de DispmanX API voor de Videocore IV GPU in Raspberry Pi 0..3. Geen overlay of shader ondersteuning."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "LibCACA stuurprogramma. Produceert de karakteroutput in plaats van afbeeldingen. Niet aanbevolen voor praktisch gebruik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Een low-level Exynos video stuurprogramma die het G2D blok in Samsung Exynos SoC gebruikt voor blit-bewerkingen. Prestaties voor software getoonde cores moeten optimaal zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "DRM-videostuurprogramma. Dit is een video stuurprogramma met behulp van libdrm voor hardwareschalen met behulp van GPU overlays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Een Sunxi video stuurprogramma die het G2D blok in Allwinner SoCs gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Wii U stuurprogramma. Ondersteunt Slang shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Switch stuurprogramma. Ondersteunt het GLSL-shaderformaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "OpenVG stuurprogramma. Gebruikt de OpenVG hardware accelerated 2D vector graphics API."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "GDI stuurprogramma. Gebruikt een oudere Windows interface. Niet aanbevolen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Huidige video stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Geluid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "RSound stuurprogramma voor genetwerkte audiosystemen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Legacy Open Sound System stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Standaard ALSA stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "ALSA stuurprogramma met threading ondersteuning."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "ALSA stuurprogramma geïmplementeerd zonder afhankelijkheden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "RoarAudio geluidssysteem stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "OpenAL stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "OpenSL stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "DirectSound stuurprogramma. DirectSound wordt voornamelijk gebruikt van Windows 95 tot Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Windows Audio Session API stuurprogramma. WASAPI wordt voornamelijk gebruikt vanuit Windows 7 en hoger."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "PulseAudio stuurprogramma. Als het systeem PulseAudio gebruikt, zorg er dan voor dat dit stuurprogramma gebruikt wordt in plaats van bijv. ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "PipeWire stuurprogramma. Als het systeem gebruik maakt van PipeWire, zorg ervoor dat je dit stuurprogramma gebruikt in plaats van bijv. PulseAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Jack Audio Verbindingskit stuurprogramma."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Microfoon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Microfoon Resampler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Te gebruiken microfoonstuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Microfoon block frames"
   )
#endif
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Audio resampler stuurprogramma om te gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Windowed Sinc implementatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Geconvolueerde Cosine implementatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Dichtstbijzijnde resampling implementatie. Deze resampler negeert de kwaliteitsinstelling."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Locatie Driver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Menu driver om te gebruiken. (Opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB is een RetroArch GUI die lijkt op een console menu van de 7e generatie. Het kan dezelfde functies als Ozone ondersteunen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone is de standaard GUI van RetroArch op de meeste platformen. Het is geoptimaliseerd voor navigatie met een spelcontroller."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI is een eenvoudige ingebouwde GUI voor RetroArch. Het heeft de laagste prestatienormen onder de menustuurprogramma's en kan worden gebruikt op de schermen met een lage resolutie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "Op mobiele apparaten maakt RetroArch standaard gebruik van de mobiele UI, MaterialUI. Deze interface is ontworpen rond touchscreen en pointer apparaten, zoals een muis/trackball."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Opname Driver"
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Voer native, lage-resolutie signalen uit voor gebruik met CRT-schermen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Uitvoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Instellingen voor video-uitvoer wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Modus volledig scherm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Instellingen voor modus volledig scherm wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Venstermodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Instellingen voor venstermodus wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Schalen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Video-schalen instellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Wijzig HDR-instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Audio Synchronizatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Video-synchronisatie instellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Onderbreek schermbeveiliging"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Voorkom het inschakelen van schermbeveiliging op je systeem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Schorst de screensaver op. Is een hint dat niet noodzakelijkerwijs hoeft te worden gehonoreerd door de videostuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Video met Schroefdraad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Verbetert prestaties ten koste van latentie en vloeiendheid van het beeld. Gebruik dit alleen wanneer het afspelen op volle snelheid niet anders mogelijk is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Gebruik threaded video stuurprogramma. Gebruik hiervan kan de prestaties verbeteren tegen de mogelijke kosten van vertraging en meer video stottering."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Zwarte Frame Injectie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "WAARSCHUWING: Snelle knippering kan afbeelding inbranden op sommige schermen. Gebruik op eigen risico // Voeg zwarte frame(s) toe tussen frames. Kan bewegingsonscherpte aanzienlijk verminderen door CRT scan te emuleren, maar ten koste van helderheid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Voegt zwarte frame(s) binnen tussen frames toe voor verbeterde bewegingsduidelijkheid. Gebruik alleen de optie voor de vernieuwingsfrequentie van je huidige scherm. Niet voor gebruik bij frequenties die geen veelvoud zijn van 60Hz zoals 144Hz, 165Hz, etc. Niet combineren met Swap Interval > 1, sub-frames, Frame Delay of Synchronisatie naar Exacte Inhoud Vernieuwingsfrequentie . VRR van het systeem aan laten staan is ok, maar die instelling niet. Als je tijdelijke afbeelding retentie ziet, moet j[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Zwarte Frame Injectie - Donkere Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Pas het aantal zwarte frames aan in de totale BFI scan outsequentie. Meer betekendt aan hogere bewegingsduidelijkheid, minder is hogere helderheid. Niet van toepassing bij 120 Hz, aangezien er slechts 1 BFI-frame is om met een totaal te werken. Instellingen hoger dan mogelijk zullen je beperken tot het maximaal mogelijke voor je gekozen vernieuwingsfrequentie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Past het aantal weergegeven frames aan in de Bfi sequentie die zwart zijn. Meer zwarte frames verhoogt bewegingshelderheid maar vermindert de helderheid. Niet van toepassing bij 120hz omdat er maar één totaal 60Hz extra frame is, Het moet dus zwart zijn, anders zou BFI helemaal niet actief zijn."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "WAARSCHUWING: Snelle knipperen kan afbeeldingen in sommige schermen inbranden. Gebruik op eigen risico // Simuleert een standaard scanlijn over meerdere sub-frames door het scherm verticaal te verdelen en elk deel van het scherm weer te geven afhankelijk van hoeveel sub-frames er zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Voegt extra shader frame(s) binnen voor mogelijke shader effecten die zijn ontworpen om sneller te draaien dan de snelheid van de inhoud. Gebruik alleen de optie voor uw huidige ververskoers voor weergave. Niet voor gebruik bij frequenties die geen veelvoud zijn van 60Hz zoals 144Hz, 165Hz, etc. Niet combineren met Swap Interval > 1, BFI, Frame Delay, of synchroniseren naar Exact Content Framerate. VRR van het systeem aan laten staan is ok, maar die instelling niet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Rollende scanlijn simulatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "WAARSCHUWING: Een snelle knippering kan afbeeldingen in sommige schermen inbranden. Gebruik op eigen risico // Simuleert een standaard scanlijn over meerdere sub-frames door het scherm verticaal omhoog te delen en elk deel van het scherm weer te geven afhankelijk van hoeveel sub-frames er zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Simuleert een basis rollende scanlijn over meerdere sub-frames door het scherm verticaal te delen en elk deel van het scherm weer te geven afhankelijk van hoeveel sub-frames er van de bovenkant van het scherm naar beneden zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Bilineaire Filtering"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Voeg een lichte vervaging toe aan de afbeelding om harde pixelranden te verzachten. Deze optie heeft zeer weinig invloed op de prestaties. Wordt beter uitgeschakeld wanneer shaders gebruikt worden."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Beeldinterpolatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Specificeer de methode voor beeldinterpolatie wanneer content door de interne IPU wordt geschaald. 'Bicubisch' of 'Bilineair' worden aangeraden bij gebruik van CPU-aangedreven videofilters. Deze instelling heeft geen impact op prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bicubisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Bilineair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Naaste-buur"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Beeldinterpolatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Specificeer beeldinterpolatiemethode wanneer 'Schalen in gehele getallen' is uitgeschakeld. 'Naaste buur' heeft de minste impact op prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Naaste-buur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Semi-Lineaire"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Auto-Shader Vertraging"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Vertraag automatisch ladende shaders (in ms). Kan grafische storingen omzeilen bij gebruik van 'schermgrijping'-software."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Videofilters"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Pas een CPU-aangedreven videofilter toe. Dit kan hoge prestatiekosten met zich meebrengen. Sommige videofilters werken mogelijk alleen voor cores die 32-bits of 16-bits kleuren gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Een CPU-aangedreven videofilter toepassen. Kan hoge prestatiekosten met zich meebrengen. Sommige video filters werken mogelijk alleen voor cores die 32-bit of 16-bit kleur gebruiken. Dynamisch gelinkte video filter bibliotheken kunnen worden geselecteerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Een CPU-aangedreven videofilter toepassen. Kan hoge prestatiekosten met zich meebrengen. Sommige video-filters werken mogelijk alleen voor cores die 32-bit of 16-bit kleur gebruiken. Ingebouwde video-filterbibliotheken kunnen worden geselecteerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Video Filter Verwijderen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Laad elk actief CPU-aangedreven videofilter af."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Volledig scherm inschakelen over de notch op Android- en iOS-apparaten"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
   "Gebruik Metal-Argumentbuffers (herstart vereist)"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS,
   "Probeer de prestaties te verbeteren door de Metal-argumentbuffers te gebruiken. Voor sommige cores kan dit nodig zijn. Dit kan sommige shaders breken, met name op oude hardware of OS versies."
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Alleen voor CRT-schermen. Pogingen om exacte core-/spelresolutie en verversingssnelheid te gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "CRT Super Resolutie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Schakel tussen native en ultrabrede superresoluties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Horizontale Centering"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Blader door deze opties als de afbeelding niet goed op het scherm is gecentreerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Horizontale grootte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Ga door deze opties heen om de horizontale instellingen aan te passen om de afbeeldingsgrootte te wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_VERTICAL_ADJUST,
   "Verticale Centering"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_VERTICAL_ADJUST,
   "Blader door deze opties als de afbeelding niet goed op het scherm is gecentreerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Gebruik menu met hoge resolutie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Schakel over naar een hoog-resolutiemodel voor gebruik met hoge-resolutie menu's wanneer er geen inhoud wordt geladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Aangepaste Vernieuwingsfrequentie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Gebruik indien nodig een aangepaste vernieuwingsfrequentie die is opgegeven in het configuratiebestand."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Selecteer welk beeldscherm er gebruikt moet worden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Welke monitor te verkiezen. 0 (standaard) betekent dat geen bepaalde monitor de voorkeur heeft, 1 en hoger (1 is eerste monitor), stelt RetroArch voor om die specifieke monitor te gebruiken."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Optimaliseren voor Wii U GamePad (Opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Gebruik een exacte 2x schaal van de GamePad als de weergave. Uitschakelen om op de oorspronkelijke TV-resolutie weer te geven."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Rotatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Dwingt een bepaalde rotatie van de video. De rotatie wordt opgeteld bij rotaties die de core instelt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Schermoriëntatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Dwingt een bepaalde oriëntatie van het scherm af van het besturingssysteem."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Selecteer de grafische kaart die gebruikt moet worden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Scherm Horizontale Offset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Forceert een bepaalde offset horizontaal naar de video. De offset wordt globaal toegepast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Scherm Verticale Offset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Forceert een bepaalde offset verticaal naar de video. De offset wordt globaal toegepast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Verticale Refresh Rate"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Geschatte Scherm Framerate"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "De nauwkeurig geschatte vernieuwingsfrequentie van het scherm in Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "De accurate verversingssnelheid van uw monitor (Hz). Dit wordt gebruikt om audio-invoertarief te berekenen met de formule:\naudio_input_rate = spel-invoersnelheid * toon vernieuwingssnelheid / spel verversingssnelheid\nAls de core geen waarden rapporteert, worden NTSC-standaardwaarden aangenomen voor compatibiliteit.\nDeze waarde moet dicht bij 60Hz blijven om grote pitch wijzigingen te voorkomen. Als je monitor niet wordt uitgevoerd bij of dicht bij 60Hz, schakel dan VSync uit en laat dit op zi[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Stel Display-Gerapporteerde Vernieuwingsfrequentie In"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "De vernieuwingsfrequentie zoals gerapporteerd door het beeldschermstuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Automatisch vernieuwingsfrequentie wisselen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Automatisch vernieuwingsfrequentie wisselen op basis van de huidige inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Alleen in Exclusieve Volle-Scherm Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Alleen in Exclusieve Venster-Scherm Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Alle Volle-Scherm Modi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Automatische vernieuwingsfrequentie PAL-drempel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Maximale vernieuwingsfrequentie die als PAL moet worden beschouwd."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Verticale Refresh Rate"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Stel verticale-verversingssnelheid van het scherm in. '50 Hz' zal een soepele video inschakelen bij het uitvoeren van PAL-inhoud."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "sRGB FBO Geforceerd Uitschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Gewelddadig de sRGB FBO-ondersteuning uit te schakelen. Sommige Intel OpenGL stuurprogramma's op Windows hebben videoproblemen met sRGB FBO's. Als u dit inschakelt, kunt u dit omzeilen."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Volledig Scherm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Weergeef op volledig scherm. Kan worden gewijzigd tijdens runtime. Kan worden overschreven door een opdrachtregelschakelaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Schermvullend venstermodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Schermvullend venstermodus gebruiken tijdens modus volledig scherm om veranderingen in weergavemodus te voorkomen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Breedte op Volledig Scherm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Stel de aangepaste breedtegrootte in voor de modus voor volledig scherm zonder venster. Als het is uitgeschakeld, wordt de desktopresolutie gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Hoogte op Volledig Scherm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Stel de aangepaste hoogtegrootte in voor de modus voor volledig scherm zonder venster. Als het is uitgeschakeld, wordt de desktopresolutie gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Forceer resolutie op UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Forceer de resolutie naar de grote van het volledig scherm, een waarde van 3840 x 2160 zal gebruikt worden wanneer 0 is ingesteld."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Venster Schalering"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Stel de venstergrootte in op de opgegeven veelvoud van de core weergave-grootte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Venster Zichtbaarheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Het venster transparantie instellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Toon Venster Decoraties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Toon venster titelbalk en grenzen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Toon Menubalk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Toon venster menubalk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Onthoudt Venster Positie en Grootte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Toon alle inhoud in een venster met vaste grootte van de afmetingen gespecificeerd door 'Vensterbreedte' en 'Vensterhoogte', en sla de huidige venstergrootte en positie op bij het sluiten van RetroArch. Wanneer uitgeschakeld, zal venstergrootte dynamisch worden ingesteld op basis van de 'Vensterschaal'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Aangepaste Venstergrootte Gebruiken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Toon alle inhoud in een venster met vaste grootte van de afmetingen gespecificeerd door 'Vensterbreedte' en 'Vensterhoogte'. Wanneer uitgeschakeld, zal venstergrootte dynamisch worden ingesteld op basis van de 'Vensterschaal'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Window Breedte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Stel de aangepaste breedte in voor het weergavevenster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Window Hoogte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Stel de aangepaste hoogte in voor het weergavevenster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Maximale Vensterbreedte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Stel de maximale breedte van het weergavevenster in bij het aanpassen van de grootte van het formaat op basis van de 'Vensterschaal'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Maximale Vensterhoogte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Stel de maximale hoogte in van het weergavevenster bij het aanpassen van de grootte van het formaat op basis van de 'Vensterschaal'."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Schalen in gehele getallen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Alleen schaal video in gehele stappen. De basisgrootte is afhankelijk van core-gerapporteerde geometrie en hoogte-breedteverhouding."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "As voor het schalen in gehele getallen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Schaal hoogte, breedte, of beide. Halve stappen zijn alleen van toepassing op bronnen met hoge resolutie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Schaal voor schalen in gehele getallen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Rondt af naar beneden of naar boven naar het volgende geheel getal. 'Slim' schaalt naar onder wanneer afbeelding te veel wordt bijgesneden en valt terug naar niet-integer schalen als de onderschaalmarges te groot zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "Onderschaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "Overschaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Slim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Beeldverhouding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Stel beeldverhouding in."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Beeldverhouding-configuratie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Drijvend punt-waarde voor video beeldverhouding (breedte / hoogte)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Configuratie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Voorzien door core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Volledig"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Beeldverhouding behouden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Behoud 1:1 pixel hoogte-breedteverhoudingen bij het schalen van inhoud via het interne IPU. Indien uitgeschakeld, worden afbeeldingen uitgelekt om het volledige scherm in te vullen."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Aangepaste Beeldverhouding (X-positie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Aangepaste viewport offset gebruikt voor het definiëren van de X-as positie van de viewport."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Aangepaste Beeldverhouding (Y-positie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Aangepaste viewport offset gebruikt voor het definiëren van de Y-as positie van de viewport."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Viewport Anker Voorkeur X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Viewport Anker Voorkeur X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Viewport Anker Voorkeur Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Viewport Anker Voorkeur Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Horizontale positie van inhoud wanneer de viewport breder is dan de breedte van de inhoud. 0.0 is helemaal links, 0.5 is center, 1.0 is helemaal rechts."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Verticale positie van content wanneer de viewport groter is dan de hoogte van de inhoud. 0.0 is vanboven, 0.5 is center, 1.0 is vanonder."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Viewport Anker Voorkeur X (portretoriëntatie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Viewport Anker Voorkeur X (portretoriëntatie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Viewport Anker Voorkeur Y (portretoriëntatie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Viewport Anker Voorkeur Y (portretoriëntatie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Horizontale positie van inhoud wanneer de viewport breder is dan de breedte van de inhoud. 0.0 is helemaal links, 0.5 is center, 1.0 is helemaal rechts.  (portretoriëntatie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Verticale positie van content wanneer de viewport groter is dan de hoogte van de inhoud. 0.0 is vanboven, 0.5 is center, 1.0 is vanonder. (portretoriëntatie)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Aangepaste Beeldverhouding (Breedte)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Aangepaste weergave-breedte die wordt gebruikt als de Beeldverhouding is ingesteld op 'Aangepaste Beeldverhouding'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Aangepaste Beeldverhouding (Hoogte)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Aangepaste weergave-hoogte die wordt gebruikt als de Beeldverhouding is ingesteld op 'Aangepaste Beeldverhouding'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Overscan Bijsnijden (Opnieuw Opstarten Vereist)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Snijdt een paar pixels af langs de randen van de afbeelding die gewoonlijk leeg zijn gelaten door ontwikkelaars die soms ook afvalpixels bevatten."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Stel de HDR uitgangsmodus in als het scherm dit ondersteunt. Opmerking: scRGB kan strenge CRT shader maskers afzwakken, omdat de OS compositor converteert naar HDR10 nadat het masker is toegepast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MODE_OFF,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HDR_BRIGHTNESS_NITS,
   "Helderheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HDR_BRIGHTNESS_NITS,
   "Helderheid van het menu in cd/m2 (nits) wanneer een HDR display wordt gebruikt. Alleen zichtbaar wanneer HDR is ingeschakeld in Instellingen > Video > HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Helderheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Stelt het HDR helderheidsniveau in in nit. Gebruik in combinatie met de fysieke helderheidsinstellingen van je scherp. Voor een startpunt, stel dit in op 80 en de gebruik de maximale helderheid van je scherm. Als alternatief stel je dit in op de max nits van je scherm en zet de helderheid van je scherm omlaag tot het het er goed uit ziet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Kleur Boost"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Gebruikt het volledige kleurbereik van uw scherm om een heldere, verzadigde afbeelding te maken. Voor kleuren die meer trouw zijn aan het originele spelontwerp, stel dit in op Accuraat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_ACCURATE,
   "Accuraat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_EXPANDED,
   "Uitgebreid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT_WIDE,
   "Breed"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SCANLINES,
   "Scanlijnen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SCANLINES,
   "HDR scanlijnen inschakelen. Scanlijnen zijn de belangrijkste reden voor het gebruik van HDR in RetroArch omdat een nauwkeurige scanlijn implementatie het grootste deel van het scherm uitschakelt en HDR een deel van dat verloren helderheid terug herstelt. Als je meer controle over je scanlijnen nodig hebt, kijk dan naar aangepaste shaders die RetroArch biedt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "Subpixel lay-out"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SUBPIXEL_LAYOUT,
   "Selecteer je scherms subpixel lay-out, dit heeft alleen effect op scanlijnen. Als je geen idee hebt wat je sub pixel lay-out is zoek op Rtings.com voor je scherms 'subpixel layout'"
   )


/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Verticale synchronisatie (Vsync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Synchroniseer de video-uitvoer van de grafische kaart met de verversingssnelheid van het scherm. Aanbevolen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "VSync-wisselinterval"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Gebruik een aangepast wisselinterval voor VSync. Vermindert de vernieuwingsfrequentie van de monitor effectief met de opgegeven factor. 'Auto' stelt de factor in op basis van de door de core gerapporteerde framesnelheid, wat zorgt voor een verbeterde framepacing bij het hardlopen van b.v. 30 fps-inhoud op een 60 Hz-scherm of 60 fps-inhoud op een 120 Hz-scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Adaptieve VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "V-Sync is ingeschakeld totdat de prestaties onder de doelvernieuwingsfrequentie komen. Kan haperingen minimaliseren wanneer de prestaties onder realtime vallen en kan energiezuiniger zijn. Niet compatibel met \"Frame Vertraging\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCANLINE_SYNC,
   "Scanlijn Synchronisatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCANLINE_SYNC,
   "Synchroniseer videopresentatie met de scanlijnpositie. Vermindert vertraging ten koste van een hoger risico om te tearen. VSync moet worden uitgeschakeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Frame Vertraging"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Vermindert vertraging ten koste van een hoger risico dat het beeld hapert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Stelt in hoeveel milliseconden slaapstand duurt gaan voordat de core wordt uitgevoerd na de videopresentatie. Vermindert vertraging ten koste van een hoger risico op haperen.\nWaarden 20 en hoger worden behandeld als frames tijd percentages."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Automatisch Frame-vertraging"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Effectieve 'Frame Vertraging' dynamisch aanpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Probeer om het gewenste 'Frame vertraging' doel te behouden en verloren frames te minimaliseren. Beginpunt is 3/4 frame tijd wanneer 'Frame Vertraging' op 0 wordt gezet (Auto)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "effectief"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Harde GPU Synchronisatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Hard-synchronisatie van de CPU en GPU. Vermindert latentie ten koste van prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Harde GPU Sync Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Stel in hoeveel frames de CPU voor kan lopen op de GPU bij gebruik van 'Hard GPU Sync'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Stelt in hoeveel frames CPU voorop GPU kan lopen als je 'GPU Hard Sync' gebruikt. Maximum is 3.\n 0: Synchroniseer onmiddellijk met GPU.\n 1: Synchroniseer naar het vorige frame.\n 2: Etc ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Synchroniseer met Exacte Content Framerate (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Geen afwijking van de core gevraagde timing. Gebruik voor schermen met Variabele Verversingsfrequentie (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Synchroniseer met Exacte Content Framerate. Deze optie staat gelijk aan het forceren van x1 snelheid terwijl snel vooruit gaat nog altijd toegestaan is. Geen afwijking van de core gevraagde vernieuwingssnelheid, geen geluid Dynamisch Ratio Controle."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Uitvoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Instellingen voor audio-uitvoer wijzigen."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Microfoon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Instellingen voor audio-uitvoer wijzigen."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Audio Synchronizatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Audio-synchronisatie instellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "MIDI instellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Mixer Instellingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Instellingen voor de audio-mixer wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Menu Geluiden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Wijzig de menugeluidsinstellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Geluid uitzetten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Geluid dempen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Mixer Dempen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Demp audio van mixer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Respecteer de stille modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Demp alle audio in de stille modus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Geluid dempen bij het vooruitspoelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Audio automatisch dempen bij gebruik van Vooruitspoelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Geluid versnellen bij het vooruitspoelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Versnel de audio wanneer je snel vooruitgaat. Voorkomt kraken maar verschuift de pitch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Geluid dempen bij het terugspoelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Geluid dempen bij het vooruitspoelen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Audio Uitgangsniveau (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Audiovolume (in dB). 0 dB is normaal volume en er wordt geen versterking toegepast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Audiovolume, uitgedrukt in dB. 0 dB is normaal volume, waar geen versterking wordt toegepast. Versterking kan worden gecontroleerd in runtime met invoervolume omhoog/invoervolume omlaag."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Audio Mixer Volume Niveau (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Globale volume voor audio mixer (in dB uitgedrukt). 0 dB is het normale geluidsniveau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Audio DSP Plugin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Audio DSP-plug-in die audio verwerkt voordat het naar het stuurprogramma wordt verzonden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Verwijder DSP Plug-in"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Alle actieve audio DSP plug-ins uitladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI Exclusieve mode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Geef het WASAPI-stuurprogramma de exclusieve controle over het audioapparaat. Indien uitgeschakeld, gebruikt het in plaats daarvan de gedeelde modus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "WASAPI Float formaat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Gebruik float formaat voor de WASAPI-stuurprogramma, indien ondersteund door uw audioapparaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI Gedeelde bufferlengte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "De tussenliggende bufferlengte (in frames) bij gebruik van de WASAPI-stuurprogramma in de gedeelde modus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ASIO_CONTROL_PANEL,
   "Open het ASIO Controlepaneel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ASIO_CONTROL_PANEL,
   "Open het ASIO stuurprogramma Controlepaneel om routering en bufferinstellingen van het apparaat te configureren."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Audio Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Audio-uitvoer inschakelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Audio Apparaat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Overschrijf het standaard audioapparaat dat het audiostuurprogramma gebruikt. Dit is afhankelijk van de stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Overschrijf het standaard audioapparaat dat het audiostuurprogramma gebruikt. Dit is afhankelijk van de stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Aangepaste PCM-apparaatwaarde voor het ALSA-stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Aangepaste padwaarde voor het OSS-stuurprogramma (bijv. /dev/dsp)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Aangepaste portnaamwaarde voor het JACK stuurprogramma (bijv. system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Aangepast IP-adres van een RSound server voor het RSound stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Audio Latentie (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Maximale audio latentie in milliseconden. Het doel van de driver is om de werkelijke vertraging op 50 procent van deze waarde te houden. Kan niet gehonoreerd worden als de audio driver geen vertraging kan geven."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Microfoon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Audio-invoer inschakelen in ondersteunde cores. Heeft geen effect op het processorverbruik als de kern geen microfoon gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Audio Apparaat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Overschrijf het standaard audioapparaat dat het microfoonstuurprogramma gebruikt. Dit is afhankelijk van het stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Overschrijf het standaard audioapparaat dat het microfoonstuurprogramma gebruikt. Dit is afhankelijk van het stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Audio Resampler Kwaliteit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Verlaag deze waarde om de prestatie/lagere latentie te prefereren boven de audiokwaliteit, verhoogt voor betere audiokwaliteit ten koste van de prestatie/lagere latentie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Standaard invoerfrequentie (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Audio-invoer sample snelheid, wordt gebruikt als een core geen specifiek nummer opvraagt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Audio-invoer latentie (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Gewenste audio-invoerlatentie in milliseconden. Wordt mogelijk niet gehonoreerd als het audiostuurprogramma geen bepaalde latentie kan bieden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "WASAPI Exclusieve mode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Geef RetroArch de exclusieve controle over de microfoon wanneer je het WASAPI microfoonstuurporgramma gebruikt. Indien uitgeschakeld zal RetroArch in plaats daarvan de gedeelde modus gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "WASAPI Float formaat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Gebruik zwevendekommagetal invoer voor het WASAPI stuurprogramma, indien ondersteund door uw audioapparaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI Gedeelde bufferlengte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "De tussenliggende bufferlengte (in frames) bij gebruik van de WASAPI-stuurprogramma in de gedeelde modus."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Audio Resampler Kwaliteit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Verlaag deze waarde om de prestatie/lagere latentie te prefereren boven de audiokwaliteit, verhoogt voor betere audiokwaliteit ten koste van de prestatie/lagere latentie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Audio Uitvoer Frequentie (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Samplefrequentie audio-uitvoer."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Audio Synchronizatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Synchroniseer audio. Aangeraden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Audio Maximale Timing Onevenredigheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "De maximale verandering in de audio-invoersnelheid. Door dit te verhogen, kunnen zeer grote veranderingen in timing worden gemaakt ten koste van een onnauwkeurige audiopitch (bijv., door PAL-cores op NTSC-schermen te gebruiken)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Maximale audio timing skew.\nDefinieert de maximale verandering in de invoersnelheid. Misschien wilt u dit verhogen om zeer grote veranderingen in de timing in te schakelen, bijvoorbeeld het draaien van PAL-cores op NTSC schermen, ten koste van een onnauwkeurig audiopitch.\nInput snelheid is gedefinieerd als:\ninvoersnelheid * (1.0 +/- (max timing skew))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Dynamische Audio Rate Control"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Helpt bij het synchroniseren van onvolkomenheden in timing bij het synchroniseren van audio en video. Houd er rekening mee dat, indien uitgeschakeld, een goede synchronisatie bijna onmogelijk te verkrijgen is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Dit instellen op 0 schakelt snelheidscontrole uit. Elke andere waarde controleert audiosnelheid controle delta.\nDefinieert hoeveel invoersnelheid dynamisch kan worden aangepast. Input snelheid wordt gedefinieerd als:\ninvoersnelheid * (1.0 +/- (snelheid controle delta))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Invoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Selecteer het invoerapparaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Stelt het invoerapparaat in (stuurprogramma specifiek). Wanneer ingesteld op 'Uit', wordt MIDI invoer uitgeschakeld. Apparaatnaam kan ook worden ingetypt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Uitvoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Selecteer het uitvoerapparaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Stelt het uitvoerapparaat in (stuurprogramma specifiek). Wanneer ingesteld op 'Uitgeschakeld', wordt MIDI uitvoer uitgeschakeld. Apparaatnaam kan ook worden ingetypt.\nWanneer MIDI uitvoer is ingeschakeld en de core en de game/app ondersteunen MIDI uitvoer, sommige of alle geluiden (afhankelijk van game/app) zullen worden gegenereerd door het MIDI-apparaat. In het geval van 'null' MIDI stuurprogramma, betekent dit dat deze geluiden niet hoorbaar zijn."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Stel het uitvoervolume in (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Afspelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Start het afspelen van de audiostream. Eenmaal voltooid, wordt de huidige audiostream uit het geheugen verwijderd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Afspelen (Loop)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Start het afspelen van de audiostream. Eenmaal voltooid, zal het vanaf het begin een lus maken en de track opnieuw afspelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Afspelen (Sequentieel)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Start het afspelen van de audiostream. Eenmaal voltooid, springt het in volgorde naar de volgende audiostream en herhaalt dit gedrag. Handig als afspeelmodus voor albums."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Stoppen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Hierdoor wordt het afspelen van de audiostream gestopt, maar niet uit het geheugen verwijderd. Het kan opnieuw worden gestart door 'Play' te selecteren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Verwijderen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Dit stopt het afspelen van de audiostream en verwijdert deze volledig uit het geheugen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Pas het volume van de audiostream aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Status: n.v.t."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Status: gestopt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Status: aan het afspelen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Status: aan het afspelen (herhalend)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Status: aan het afspelen (één voor één)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Speel gelijktijdige audiostreams af, zelfs in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Activeer 'OK' Geluid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Activeer 'Annuleren' Geluid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Activeer 'Notificatie' Geluid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Activeer 'BGM' Geluid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Inschakelen 'Scroll' geluiden"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Maximum Aantal Gebruikers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Polling-gedrag (opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Beinvloed hoe invoer polling wordt gedaan in RetroArch. Het instellen op 'Early' of 'Late' kan resulteren in minder latentie, afhankelijk van je configuratie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Beinvloed hoe invoer polling wordt gedaan binnen RetroArch.\nEarly - Invoef polling wordt uitgevoerd voordat het frame wordt verwerkt.\nNormaal - Invoer polling wordt uitgevoerd wanneer polling wordt aangevraagd.\nLate - Invoer polling wordt uitgevoerd op eerste invoer-status verzoek per frame.\nHet instellen op 'Early' of 'Late' kan resulteren in minder latentie, afhankelijk van je configuratie. Wordt genegeerd bij het gebruik van netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Remap de Besturingsemlementen voor Deze Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Overschrijf de invoerbindingen met de opnieuw toegewezen bindingen die zijn ingesteld voor de huidige core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Sorteer remaps op gamepad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Remaps zullen alleen van toepassing zijn op de actieve gamepad waarin ze zijn opgeslagen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Autoconfiguratie Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Automatisch controllers configureren die een profiel, Plug-and-Play stijl hebben."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Windows-sneltoetsen Uitschakelen (Opnieuw Opstarten Vereist)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Houd Win-toetscombinaties in de applicatie."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Selecteer fysiek toetsenbord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Gebruik dit apparaat als fysiek toetsenbord en niet als gamepad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Als RetroArch een hardware toetsenbord identificeert als een soort gamepad, kan deze instelling worden gebruikt om RetroArch te dwingen het verkeerd geïdentificeerde apparaat te behandelen als een toetsenbord.\nDit kan handig zijn als je een computer probeert te emuleren in een Android TV apparaat en je ook een fysiek toetsenbord bezit dat aan de box kan worden gekoppeld."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Hulpsensor-ingang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Schakel invoer van versnellingsmeter-, gyroscoop- en verlichtingssensoren in, indien ondersteund door de huidige hardware. Kan een invloed hebben op de prestatie en/of het stroomverbruik op sommige platforms verhogen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Automatische Muisgrijp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Muisgrijp inschakelen bij applicatie-focus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Modus 'Spel Focus' Automatisch Inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Schakel altijd de modus 'Spel Focus' in bij het starten en hervatten van inhoud. Indien ingesteld op 'Detecteer', wordt de optie ingeschakeld alleen als de huidige core de frontend-toetsenbordcallback-functionaliteit implementeert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "AAN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Detecteer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Inhoud pauzeren wanneer de verbinding met de controller verbreekt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Inhoud pauzeren wanneer een controller wordt losgekoppeld. Hervat met Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Invoerknop Axis Drempel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Hoe ver een as moet worden gekanteld om te resulteren in een druk op de knop bij het gebruik van 'Analoog tot Digitaal'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Analoge Deadzone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Analoge stick bewegingen onder de deadzone waarde negeren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Analoge Gevoeligheid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Gevoeligheid Versnellingsmeter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Gevoeligheid Gyroscoop"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Pas de gevoeligheid van analoge sticks aan."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Pas de gevoeligheid van de versnellingsmeter aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ORIENTATION,
   "Sensororiëntatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ORIENTATION,
   "Draai versnellingsmeter en gyroscoop om overeen te komen met de oriëntatie van het apparaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SENSOR_ORIENTATION_AUTO,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Pas de gevoeligheid van de Gyroscoop aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Binding-Timeout"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Aantal seconden om te wachten tot je doorgaat naar de volgende binding."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Binding-Houd"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Aantal seconden om een invoer vast te houden om deze te binden."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Druk op toetsenbord, muis of controller"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Knoppen loslaten!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Tijdslimiet"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Ingedrukt houden"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Turbo-vuur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE,
   "Indien uitgeschakeld stopt alle turbo operaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Turbo Periode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "De tijd in frames per turbo-ingeschakelde knopinvoer cyclus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DUTY_CYCLE,
   "De tijd in frames hoe lang turbo-ingeschakelde knoppen worden ingedrukt."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE,
   "Het aantal frames uit de Turbo-periode waarvoor de knoppen worden ingedrukt. Als dit aantal gelijk is aan of groter is dan de Turbo-periode, zullen de knoppen nooit loslaten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF,
   "Halve cyclus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Turbo Modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Selecteer het algemene gedrag van de turbomodus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Klassiek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Klassiek (omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Enkelvoudige knop (Schakelen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Enkelvoudige knop (Vasthouden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Klassieke modus, gebruikt twee knoppen. Houd een knop ingedrukt en druk op de Turbo knop om de indrukken-loslaten cyclus te activeren.\nTurbo knop kan worden toegewezen in Instellingen/Invoer/Poort X invoer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Klassieke wissel-modus, gebruikt twee knoppen. Houd een knop ingedrukt en druk op de Turbo knop om turbo voor die knop te activeren. Om turbo terug uit te schakelen: houd de knop ingedrukt en druk opnieuw op de Turbo knop.\nTurbo kan worden toegewezen in Instellingen/Input/Port X invoer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Omschakelmodus. Druk eenmaal op de Turbo knop om de indrukken-loslaten cyclus voor de geselecteerde standaard knop te activeren, druk opnieuw op de knop om deze uit te schakelen.\nTurbo knop kan worden toegewezen in Instellingen/Invoer/Poort X invoer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Vasthoud-modus. De indrukken-loslaten cyclus voor de geselecteerde standaard knop is actief zolang de Turbo knop ingedrukt blijft.\nTurbo bind kan worden toegewezen in Instellingen/Input/Port X invoer.\nOm de autofire functie van het home-computertijdperk te emuleren, zet de invoer en de turbo op dezelfde joystick knop."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "Turbo-invoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "De toewijzinging op de RetroPad die turbo activeert. Een lege waarde gebruikt een invoer specifiek voor de poort."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "Turboknop"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "Turboknop in de \"Enkelvoudige knop\" modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "Turbo sta D-pad richtingen toe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "Indien ingeschakeld, kan digitale richtingsinvoer (ook bekend als d-pad of 'hatswitch') turbo gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Turbo-vuur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Wijzig de turbo-vuur instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Haptische Feedback/Vibratie (Trilsignaal)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Haptische feedback en trillingsinstellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_SETTINGS,
   "Beweging-/lichtsensoren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_SETTINGS,
   "Verander versnellingsmeter, gyroscoop en verlichtingsinstellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Menubesturing"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Wijzig de instellingen voor menubediening."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Sneltoetsen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Verander instellingen en toewijzingen voor sneltoetsen, zoals het inschakelen van het menu tijdens het spelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "RetroPad toewijzingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Wijzigen hoe de virtuele RetroPad wordt toegewezen aan een fysiek invoerapparaat. Als een invoerapparaat herkend en automatisch geconfigureerd is, hoeven gebruikers dit menu waarschijnlijk niet te gebruiken.\nOpmerking: voor core-specifieke invoerwijzigingen, gebruik het Quick Menu's submenu in plaats van dit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro maakt gebruik van een virtuele gamepad abstractie, bekend als de 'RetroPad' om voor de communicatie van frontends (zoals RetroArch) naar cores en vice versa. Dit menu bepaalt hoe de virtuele RetroPad is toegewezen aan de fysieke invoerapparaten en welke virtuele invoerpoorten deze apparaten hebben.\nAls een fysiek invoerapparaat herkend en automatisch geconfigureerd is, hoeven gebruikers dit menu waarschijnlijk helemaal niet te gebruiken en voor core-specifieke invoerwijzigingen, zou he[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Poort %u Besturingselementen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Verander hoe de virtuele RetroPad is toegewezen aan je fysieke invoerapparaat voor deze virtuele poort."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Wijzig core-specifieke invoertoewijzingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Tijdelijke oplossing voor het loskoppelen van Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Tijdelijke oplossing voor het loskoppelen en opnieuw verbinden van controllers. Belemmert 2 spelers met dezelfde controllers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_QUIT,
   "Afsluiten bevestigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_QUIT,
   "Vereist dat de Afsluit-sneltoets tweemaal wordt ingedrukt om RetroArch af te sluiten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_CLOSE,
   "Inhoud sluiten bevestigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_CLOSE,
   "Vereist dat de sneltoets voor inhoud te sluiten twee keer wordt ingedrukt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIRM_RESET,
   "Reset inhoud bevestigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIRM_RESET,
   "Vereist dat de knop om inhoud te resetten twee keer wordt ingedrukt."
   )


/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Trillen bij indrukken knop"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Toestel trillen inschakelen (voor ondersteunde Cores)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Trillingssterkte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Specificeer de omvang van de haptische feedbackeffecten."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Uniforme Menubesturing"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Gebruik dezelfde besturingselementen voor zowel het menu als het spel. Geldt voor het toetsenbord."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Wissel OK en Annuleer Knoppen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Verwissel knoppen voor OK/Annuleren. Uitgeschakeld is de Japanse knoporiëntatie, ingeschakeld is de westerse oriëntatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Wissel scrollknoppen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Wisselknoppen voor scrollen. Uitgeschakeld scrolt u 10 items met L/R en alfabetisch met L2/R2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Alle Gebruikers kunnen de Menu Bedienen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Sta elke gebruiker toe om het menu te bedienen. Indien uitgeschakeld, kan alleen Gebruiker 1 het menu bedienen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SINGLECLICK_PLAYLISTS,
   "Enkele-klik Afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SINGLECLICK_PLAYLISTS,
   "Sla het 'Uitvoeren' menu over bij het starten van afspeellijsten. Druk op D-Pad terwijl je OK ingedrukt houdt om het 'Uitvoeren' menu te openen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ALLOW_TABS_BACK,
   "Sta terug vanaf tabbladen toe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ALLOW_TABS_BACK,
   "Ga terug naar het hoofdmenu vanaf tabbladen/zijbalk als er op terug wordt gedruk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Scrolversnelling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Maximale snelheid van de cursor bij het vasthouden van een richting om te scrollen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Scrolvertraging"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Initiële vertraging in milliseconden bij het vasthouden van een richting om te scrollen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Infoknop uitschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Voorkom menu info functie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Zoekknop uitschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Voorkom menu zoekfunctie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Linkse analoge stick uitschakelen in het menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Voorkom invoer van de linkse analoge stick in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Rechtse analoge stick uitschakelen in het menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Voorkom invoer van de rechtse analoge stick in het menu De rechtse analoge stick scrolt door miniatuurweergaven in afspeellijsten."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Sneltoets Inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Wanneer toegewezen, moet de \"Sneltoets Inschakelen\"-knop worden vastgehouden voordat andere sneltoetsen worden herkend. Hiermee kunnen de controller knoppen worden toegewezen aan sneltoetsfuncties zonder de normale invoer te beïnvloeden. Het toewijzen van deze functie aan controllers vereist alleen het niet toetsenbordsneltoetsen en vice versa, maar beide modifiers werken voor beide apparaten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Als deze sneltoets toegewezen is aan ofwel het toetsenbord, ofwel de joyknop, ofwel de joyas, worden alle andere sneltoetsen uitgeschakeld tenzij deze sneltoets tegelijkertijd is ingedrukt.\nDit is handig voor RETRO_KEYBOARD implementaties die een groot gebied van het toetsenbord opvragen waar het niet wenselijk is dat sneltoetsen in de weg staan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Sneltoets Inschakelen Vertraging (Frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Een vertraging in frames toevoegen voordat normale input wordt geblokkeerd na het indrukken van de toegewezen 'Hotkey Inschakelen' toets. Staat toe dat normale input van de 'Hotkey Inshakelen'-toets wordt opgevangen wanneer deze wordt toegewezen aan een andere actie (bijv. RetroPad 'Selecteren')."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Sneltoetsen van apparaattypes samenvoegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Alle sneltoetsen van zowel toetsenbord als controller apparaattypes blokkeren als één van beide type 'Sneltoets Inschakelen' heeft ingesteld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Sneltoetsen volgen speler 1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Sneltoetsen zijn gebonden aan de core-poort 1, zelfs als core-poort 1 opnieuw wordt toegewezen aan een andere gebruiker. Opmerking: toetsenbordsneltoetsen zullen niet werken als core-poort 1 opnieuw is toegewezen aan een gebruiker > 1 (toetsenbordinvoer is van gebruiker 1)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Menu-schakel (Bedieningscombinatie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Knopcombinatie om het menu in of uit te schakelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Menu-schakel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Schakelt de huidige weergave tussen het menu en de inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Afsluiten (Bedieningscombinatie)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Knopcombinatie om RetroArch af te sluiten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Sluit RetroArch af, zodat alle opgeslagen gegevens en configuratiebestanden naar de schijf worden gespoeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Sluit Inhoud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Sluit de huidige inhoud af."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Inhoud Resetten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Herstart de huidige inhoud vanaf het begin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Vooruitspoelen (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Schakelt tussen snel-vooruit en normale snelheid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Vooruitspoelen (Vasthouden)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Maakt vooruitspoelen mogelijk wanneer het knop is vastgehouden. Inhoud wordt op normale snelheid uitgevoerd wanneer de sleutel wordt losgelaten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Slow-Motion (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Schakelt tussen slow-motion en normale snelheid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Slow-Motion (Vasthouden)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Maakt slow-motion mogelijk wanneer het knop is vastgehouden. Inhoud wordt op normale snelheid uitgevoerd wanneer de sleutel wordt losgelaten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Terugspoelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Spoelt de huidige inhoud terug terwijl de toets wordt vastgehouden. \"Terugspoelen Activeren\" moet zijn ingeschakeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Pauzeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Schakelt inhoud uit tussen gepauzeerde en niet-gepauzeerde staten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Frame-voortgang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Zet de inhoud één frame verder wanneer deze wordt gepauzeerd."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Audio dempen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Schakelt de audio-uitvoer aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Volume Omhoog"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Verhoogt het uitvoervolume."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Volume Omlaag"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Verlaagt het uitvoervolume."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Status Laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Laadt de opgeslagen staat van de huidige geselecteerde slot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Sla de Staat op"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Slaat de staat op in het huidige geselecteerde slot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Volgende Slaagstaat-slot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Verhoogt de momenteel geselecteerde spaarstaat slot index."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Vorige Slaagstaat-slot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Vermindert de momenteel geselecteerde spaarstaat slot index."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Schakel Disk-uitwerp (in/uit)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Als de virtuele schijflade is gesloten, wordt deze geopend en wordt de geladen schijf verwijderd. Anders voegt het de huidige geselecteerde schijf toe en sluit het lade."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Volgende Schijf"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Verhoogt de geselecteerde schijfindex en doet een vertraagde invoer als de virtuele schijflade is gesloten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Vorige Schijf"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Verlaagd de geselecteerde schijfindex en doet een vertraagde invoer als de virtuele schijflade is gesloten."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Shaders (Schakel)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Schakelt de huidig geselecteerde shader aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_HOLD,
   "Shaders (ingedrukt houden)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD,
   "Houdt de huidige geselecteerde shader aan/uit wanneer toets wordt ingedrukt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Volgende shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Laadt en past het volgende shader preset-bestand toe in de hoofdmap van de map 'Video Shaders'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Vorige shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Laadt en past het vorige shader preset-bestand toe in de hoofdmap van de map 'Video Shaders'."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Cheats (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Schakelt de huidige geselecteerde cheat aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Volgende Cheat Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Verhoogt de momenteel geselecteerde cheat index."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Vorige Cheat Index"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Vermindert de momenteel geselecteerde cheat index."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Schermafdruk maken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Maakt een afbeelding van de huidige inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Opname (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Start/stopt de opname van de huidige sessie naar een lokaal videobestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Streaming (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Start/stopt het streamen van de huidige sessie naar een online videoplatform."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Speel Herhaling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Speel een replay bestand af van de huidige geselecteerde slot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Record Herhaling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Neem het harhalingsbestand op naar de geselecteerde slot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Stop opnemen/herhalen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Stopt het opnemen/afspelen van de huidige herhaling."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Sla harhalingsopslagpunt op"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Voegt een opslagpunt toe aan de herhaling die aan het spelen is"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Vorige harhalingsopslagpunt "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Spoelt de herhaling terug tot de vorige automatisch of handmatig opgeslagen opslagpunt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Volgende harhalingsopslagpunt "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Spoelt de herhaling voort tot de volgende automatisch of handmatig opgeslagen opslagpunt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "Volgende harhalingslot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Verhoogt de momenteel geselecteerde harhalingslotindex."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Vorige harhalingslot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Verlaagd de momenteel geselecteerde harhalingslotindex."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_TURBO_FIRE_TOGGLE,
   "Turbo (omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE,
   "Zet turbo aan of uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Grijp de Muis (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Grijpt of laat de muis los. Wanneer het wordt gepakt, is de systeemcursor verborgen en beperkt tot het RetroArch-weergavevenster, waardoor de relatieve muisinvoer wordt verbeterd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Spel Focus (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Schakelt de 'Speel-Focus' modus aan/uit. Als de inhoud de focus heeft, worden sneltoetsen uitgeschakeld (volledige toetsenbordinvoer wordt doorgegeven aan de draaiende core) en de muis wordt vastgepakt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Volledig Scherm (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Schakelt tussen volledig scherm en vensterweergave-modi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Bureaublad Menu (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Opent de Bijbehorende WIMP (Windows, Icons, Menus, Pointer) desktop-gebruikersinterface."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Synchroniseren met de Exacte Inhoudsframerate (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Schakelt synchronisatie naar exacte inhoudsframerate aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Run-Ahead (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Schakelt Run-Ahead aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Preemptive Frames (omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Zet Preemptive Frames aan of uit."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Toon de FPS (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Schakelt de 'frames per seconde' statusindicator aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Technische Statistieken Weergeven (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Schakelt de weergave van technische statistieken op het scherm aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Toetsenbord Overlay (omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Schakelt toetsenbord overlay aan of uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Volgende Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Schakelt over naar de volgende beschikbare opmaak van de momenteel actieve schermoverlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "AI-Service"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Legt een beeld van de huidige inhoud vast om alle tekst op het scherm te vertalen en/of voor te lezen. 'AI Service' moet worden ingeschakeld en geconfigureerd."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Netplay Ping (Schakel)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Schakelt de ping-teller voor de huidige net-play kamer aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Netplay Hosting (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Schakelt netplay-hosting aan/uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Netplay Spel/Toeschouwer-Modus (Omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Schakelt de huidige netplay-sessie tussen de 'play' en 'spectate' modi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Net-play Speler Chat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Stuurt een chatbericht naar de huidige net-play-sessie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Netplay-chat vervagen (omschakelen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Schakelen tussen vervagende en statische net-play-chatberichten."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Apparaattype"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Specificeert het geëmuleerde controllertype."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Analoog naar Digitaal Type"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Gebruik opgegeven analoge stick voor de D-Pad invoer. \"Geforceerde\" modus overschrijft de analoge invoer van de core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Gebruik gespecificeerde analoge stick voor D-Pad invoer. Als de core native analoge ondersteuning heeft, wordt D-Pad toewijzing uitgeschakeld, tenzij een \"'(Geforceerd)\"-optie is geselecteerd. Als D-Pad toewijzing wordt geforceerd, ontvangt de core geen analoge invoer van de opgegeven stick."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Apparaatindex"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "De fysieke controller als herkend door RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Apparaat gereserveerd voor deze speler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Deze controller zal worden toegewezen aan deze speler, volgens de reserveringsmodus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "Geen reservatie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "Geprefereerd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "Gereserveerd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "Apparaat Reservatie Type"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "Geprefereerd: als het opgegeven apparaat aanwezig is, wordt het toegewezen voor deze speler. Gereserveerd: er wordt geen ander regelsysteem toegewezen aan deze speler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Toegewezen Poort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Geeft aan welke core-poort invoer krijgt van frontend controller poort %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Stel Alle Bedieningselementen In"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Wijs alle richtingen en knoppen toe, één na de andere, in de volgorde waarin ze worden weergegeven in dit menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Reset naar Standaard-Besturingselementen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Reset de invoerinstellingen naar de standaard waarden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Sla het Controllerprofiel Op"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Sla een autoconfiguratiebestand op dat automatisch wordt toegepast wanneer deze controller opnieuw wordt gedetecteerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Muisindex"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "De fysieke muis zoals herkend door RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "B knop (down)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Y knop (left)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Select knop"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Start knop"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "Navigatiepad Omhoog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "Navigatiepad Omlaag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "Navigatiepad Links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "Navigatiepad Rechts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "A knop (right)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "X knop (top)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "L knop (shoulder)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "R knop (shoulder)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "L2 knop (trigger)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2 knop (trigger)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "L3 knop (thumb)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "R3 knop (thumb)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Linker Analoog X+ (Rechts)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Linker Analoog X- (Links)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Linker Analoog Y+ (Omlaag)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Linker Analoog Y- (Omhoog)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Rechter Analoog X+ (Rechts)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Rechter Analoog X- (Links)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Rechter Analoog Y+ (Omlaag)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Rechter Analoog Y- (Omhoog)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Wapen Trigger"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Wapen Reload"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Wapen Aux A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Wapen Aux B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Wapen Aux C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Wapen Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Wapen Selectie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Wapen D-Pad Omhoog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Wapen D-Pad Omlaag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Wapen D-Pad Links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Wapen D-Pad Rechts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO,
   "Turbo-vuur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOLD,
   "Ingedrukt houden"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Run-Ahead is Niet Beschikbaar]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "De huidige core is onverenigbaar met run-ahead vanwege een gebrek aan deterministische slaagstaat-ondersteuning."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Aantal Frames om Vooruit te Lopen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Het aantal frames dat vooruit moet lopen. Veroorzaakt gameplay-problemen zoals jitter als het aantal lag-frames in de game wordt overschreden."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "Voer extra core-logica uit om latentie te verminderen. Enkele instantie draait om een toekomstig frame, en laadt daarna de huidige status opnieuw. Tweede instantie houdt een kerninstantie met alleen video op een toekomstig frame om problemen met audiostaten te voorkomen. Preemptive Frames draait oude frames met nieuwe invoer wanneer nodig, voor efficiëntie."
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Voer extra kern-logica uit om latentie te verminderen. Enkele instantie draait om een toekomstige frame, en laadt daarna de huidige status opnieuw. Preemptive Frames draait in het verleden frames met nieuwe invoer wanneer nodig, voor efficiëntie."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "Enkele instantie mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "Tweede instantie mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   " Preemptive Frames-modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Verberg Run-Ahead Waarschuwingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Verberg het waarschuwingsbericht dat verschijnt bij het gebruik van Run-Ahead en de core ondersteunt geen spaarstaten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Aantal preemptive Frames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Het aantal frames dat opnieuw gedaan moet worden. Veroorzaakt gameplay-problemen zoals haperingen als het aantal lag-frames in de game wordt overschreden."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Activeer Gedeelde Hardware Context"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Geef hardware-gerenderde cores hun eigen privé-context. Voorkomt dat er tussen de frames hardwarestatuswijzigingen moeten worden aangenomen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Sta Cores toe om de Video-stuurprogramma te Wisselen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Sta cores toe om over te schakelen naar een ander video-stuurprogramma dan degene die momenteel is geladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Dummy Laden Tijdens Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Sommige cores hebben een afsluitfunctie, het laden van een dummy-core voorkomt de afsluiting van RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Sommige cores hebben mogelijk een uitzetfunctie. Als deze optie is uitgeschakeld, wordt RetroArch uitgeschakeld door de afsluitingsprocedure te selecteren.\nHet inschakelen van deze optie zal een dummy core laden, zodat we in het menu blijven en RetroArch niet wordt uitgeschakeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Automatisch core opstarten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Core-optie Categorieën"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Sta cores toe om huidige opties te presenteren op categorie-gebaseerde submenu's. LET OP: Core moet opnieuw worden geladen om wijzigingen toe te passen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Cache Core Info-bestanden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Zorg voor een permanente lokale cache met geïnstalleerde core informatie. Vermindert de laadtijden aanzienlijk op platforms met langzame schijftoegang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Omzeil core info onderbrekingspunt-functies."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Hiermee geeft je aan of core info moet worden genegeerd, waardoor u kunt experimenteren met gerelateerde functies (vooruit, terugspoelen, enz.)"
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Herlaad de Core Altijd bij \"Inhoud Invoeren\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Start RetroArch opnieuw bij het starten van inhoud, zelfs als de gevraagde core is al geladen. Dit kan de systeemstabiliteit verbeteren, ten koste van langere laadtijden."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Rotatie toestaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Sta de cores toe om de rotatie in te stellen. Indien uitgeschakeld, worden rotatieverzoeken genegeerd. Handig voor opstellingen die het scherm handmatig draaien."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Cores Beheren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Voer offline onderhoudstaken uit op geïnstalleerde cores (back-up, herstel, verwijdering, enz.) en core informatie bekijken."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Cores Beheren"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Installeer of verwijder cores die via Steam worden gedistribueerd."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Core Installeren"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Core Verwijderen"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "'Beheer cores' tonen"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Toon de 'Beheer cores' optie in het Hoofdmenu."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Core installeren: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "De core zal worden verwijderd bij het sluiten van RetroArch."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "De core is momenteel aan het downloaden"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Configuratie Opslaan bij Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Sla wijzigingen op in het configuratiebestand bij afsluiten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Sla wijzigingen in het configuratiebestand op bij het afsluiten. Handig voor wijzigingen in het menu. Overschrijft het configuratiebestand, #include's en opmerkingen worden niet bewaard."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_MINIMAL,
   "Minimale configuratie opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_MINIMAL,
   "Sla alleen instellingen op die verschillen van de standaardwaarden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_MINIMAL,
   "Wanneer ingeschakeld, slaat alleen configuratiewaarden op die veranderd zijn van hun standaardwaarden. Resultaten in een kleiner en beter beheersbaar configuratiebestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Remap-bestanden Opslaan bij Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Sla wijzigingen op in elk actief invoerhertoewijzingsbestand bij het sluiten van inhoud of RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Laad Inhoudsspecifieke Core Opties Automatisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Enable customized core options by default at startup."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Laad Override Bestanden Automatisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Enable customized configuration by default at startup."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Laad Remap Bestanden Automatisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Enable customized controls by default at startup."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Laad initiële schijf index bestanden automatisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "Wissel naar de laatst gebruikte schijf bij het starten van inhoud met meerdere schijven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Laad Shader Presets Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Gebruik Globale Core Opties Bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Sla alle core opties op in een algemeen instellingenbestand (retroarch-core-options.cfg). Indien uitgeschakeld, worden opties voor elke core opgeslagen in een aparte core-specifieke map/bestand in de RetroArch's 'Configs' map."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Opslagbestand: sorteer in mappen op basis naam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Sorteer opslagbestanden in mappen die zijn vernoemd naar de gebruikte core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Onderbrekingspunt: sorteer in mappen op basis van core-naam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Sorteer enderbrekingspunten in mappen die vernoemd zijn naar de gebruikte core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Opslagbestand: sorteer in mappen op basis inhoudmappen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Sorteer de slaag-bestanden in folders vernoemd naar de map waarin de inhoud zich bevindt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Onderbrekingspunt: sorteer in mappen op basis inhoudmappen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Sorteer de onderbrekingspunten in folders vernoemd naar de map waarin de inhoud zich bevindt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Opslagbestand: SaveRAM niet overschrijven tijdens het laden van een onderbrekingspunt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Blokkeer SaveRAM van overschrijving bij het laden van onderbrekingspunten. Kan mogelijk leiden tot buggy-spellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Opslagbestand: interval om SaveRam automatisch op te slaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Sla de niet-vluchtige SaveRAM automatisch op met een regelmatig interval (in seconden)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Slaat de niet-vluchtige SRAM automatisch op op een regelmatige interval. Dit is standaard uitgeschakeld tenzij dit anders wordt ingesteld. Het interval wordt in seconden gemeten. Een waarde van 0 schakelt dit uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Herhaling: opslagpunt-interval"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Automatisch bladwijzer maken van het spel tijdens het afspelen van de opname bij een regelmatige interval (in seconden)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Slaat de staat automatisch op tijdens een herhaling op een regelmatige interval. Dit is standaard uitgeschakeld tenzij dit anders wordt ingesteld. Het interval wordt in seconden gemeten. Een waarde van 0 schakelt dit uit."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_DESERIALIZE,
   "Of opslagpunten gedeserialiseerd moeten worden in replay tijdens het gewoon afspelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_DESERIALIZE,
   "Herhaling: opslagpunt decentraliseren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_DESERIALIZE,
   "Of opslagpunten gedeserialiseerd moeten worden in herhalingen tijdens het gewoon afspelen. Moet worden ingesteld op waar voor de meeste cores, maar sommige kunnen krakkemikkig gedrag vertonen bij het deserialiseren van inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Onderbrekingspunt: index automatisch verhogen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Voordat er een onderbrekingspunt wordt gemaakt, wordt de slagstaatsindex automatisch verhoogd. Bij het laden van inhoud wordt de index ingesteld op de hoogst bestaande index."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Herhaling: index automatisch verhogen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Voordat er een herhaling wordt gemaakt, wordt de herhalingsindex automatisch verhoogd. Bij het laden van inhoud wordt de index ingesteld op de hoogst bestaande index."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Onderbrekingspunt: Maximale automatische verhoging om te bewaren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Beperk het aantal onderbrekingspunten die worden gemaakt wanneer \"index automatisch verhogen\" is ingeschakeld. Als de beperking wordt overschreden bij het opslaan van een nieuwe staat, wordt de bestaande staat met de laagste index verwijderd. Een waarde van '0' betekent dat er onbeperkte staten worden opgenomen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Herlaing: Maximale auto-verhoging om te behouden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Beperk het aantal herhalingen die worden gemaakt wanneer \"index automatisch verhogen\" is ingeschakeld. Als de beperking wordt overschreden bij het opslaan van een nieuwe herhaling, wordt de bestaande staat met de laagste index verwijderd. Een waarde van '0' betekent dat er onbeperkte herhalingen worden opgenomen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Onderbrekingspunt: Automatisch opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Automatisch een onderbrekingspunt wanneer inhoud wordt gesloten. Dit onderbrekingspunt wordt geladen bij het opstarten als 'Auto Laden' is ingeschakeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Onderbrekingspunt: Automatisch laden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Laad de onderbrekingspunt automatisch bij het opstarten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Onderbrekingspunt: miniaturen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Toon miniaturen van onderbrekingspunten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Opslagbestand: comprimeer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Schrijf niet-vluchtige SaveRAM-bestanden in een gearchiveerd formaat. Verkleint de bestandsgrootte drastisch ten koste van (verwaarloosbaar) langere opslag-/laadtijden.\nAlleen van toepassing op cores die opslaan mogelijk maken via de standaard libretro SaveRAM interface."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Onderbrekingspunt: comprimeer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Schrijf onderbrekingspunt-bestanden in een gearchiveerd formaat. Verkleint de bestandsgrootte drastisch ten koste van langere opslag-/laadtijden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Onderbrekingspunt: schrijf naar de inhoudsmap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Gebruik inhoudsmap voor opslagbestanden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Onderbrekingspunt: schrijf naar inhoudsmap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Gebruik inhoudsmap voor Onderbrekingspunten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Systeembestanden staan in de Inhoudsmap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Gebruik inhoudsmap voor systeem/BIOS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Schermafbeelding: sorteer in mappen op basis van de inhoudsmap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Sorteer de schermafbeeldingen in folders vernoemd naar de map waarin de inhoud zich bevindt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Schermopnamen: gebruik inhoudsmap."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Gebruik inhoudsmap voor schermopnamen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Schermafbeelding: gebruik GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Indien beschikbaar nemen screenshots GPU-geschaduwd materiaal op."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Opslaan Runtime Log (Per Core)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Houd bij hoe lang elk item van de inhoud is uitgevoerd, met records gescheiden door core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Opslaan Runtime Log (Aggregaat)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Houd bij hoe lang elk item van de inhoud loopt, opgenomen als het aggregaat totaal over alle cores."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Logging Niveau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Enable or disable logging to the terminal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Front-end Logboekregistratieniveau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Stel het logniveau in voor de front-end. Als een logniveau van de front-end lager is dan deze waarde, wordt het genegeerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Core Logboekregistratieniveau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Stel het logniveau in voor de cores. Als een logniveau van een core lager is dan deze waarde, wordt het genegeerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Stelt logniveau in voor libretro cores (GET_LOG_INTERFACE). Als een logniveau uitgegeven door een libretro core lager is dan libretro_log niveau, wordt het genegeerd. DEBUG logs worden altijd genegeerd tenzij de uitgebreide modus is geactiveerd (--verbose).\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (informatie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Waarschuwing)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (Foutmelding)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Log naar bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Verwijs systeemlogboekberichten naar bestand. Vereist 'Logboek-breedsprakigheid' om ingeschakeld te zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Logbestanden met tijdstempels"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Bij het inloggen naar het bestand, doorverwijzen de uitvoer van elke RetroArch sessie naar een nieuw tijdsplan bestand. Indien uitgeschakeld, wordt de log overschreven telkens wanneer RetroArch opnieuw wordt gestart."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Prestatie Teller"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Prestatie tellers voor RetroArch en cores. Tegengegevens kunnen het systeem bottlenecks en de prestaties bepalen."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Verborgen Bestanden en Mappen tonen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Verborgen bestanden en mappen tonen in de bestandsbrowser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtreer onbekende extensies"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Bestanden die worden weergegeven in de bestandsbrowser filteren op ondersteunde extensies."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Filter op Huidige Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "Bestanden die worden weergegeven in de bestandsbrowser filteren op de huidige core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Onthoud de Laatst Gebruikte Startmap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Open bestandsbrowser op de laatst gebruikte locatie bij het laden van inhoud uit de Start map. Opmerking: Locatie wordt teruggezet naar de standaard waarde bij het opnieuw opstarten van RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SUGGEST_ALWAYS,
   "Altijd cores voorstellen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS,
   "Stel beschikbare cores voor zelfs wanneer een kern handmatig wordt geladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Gebruik ingebouwde media speler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_BUILTIN_PLAYER,
   "Toon bestanden ondersteund door de mediaspeler in de bestandsbrowser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Gebruik ingebouwde afbeeldingsviewer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_BUILTIN_IMAGE_VIEWER,
   "Toon bestanden ondersteund door de afbeeldingsviewer  in de bestandsbrowser."
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Terugspoelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_SETTINGS,
   "Terugspoelinstellingen wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Frame Tijd Teller"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Vooruitspoel-snelheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "De maximale snelheid waartegen de inhoud wordt uitgevoerd bij het gebruik van 'vooruitspoel' (bijv. 5.0x voor 60 fps inhoud = 300 fps cap). Indien ingesteld op 0.0x, is vooruitspoel-verhouding onbeperkt (geen FPS cap)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "De maximale snelheid waartegen inhoud uitgevoerd wordt wanneer je snel vooruitgaat. Bijv. 5.0 voor 60 fps inhoud => maximaal 300 fps.\nRetroArch zal de maximale snelheid niet overschrijden. U kunt er niet op vertrouwen dat deze bovengrens perfect klopt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Vooruitspoel-frameskip"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Frames overslaan volgens de snelheid van het vooruitspoelen. Dit bespaart energie en maakt het gebruik van frame-begrenzing van derden mogelijk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Slow-Motion-snelheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "De snelheid waarmee inhoud wordt afgespeeld wanneer slow-motion wordt gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Beperk de Vernieuwingsfrequentie in het Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Zorgt ervoor dat de framerate wordt beperkt in het menu."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Terugspoelen Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Keer terug naar een vorige punt in recente gameplay. Dit veroorzaakt een ernstige prestatievermindering tijdens het spelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Frames Terugspoelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Het aantal frames om terug te spoelen per stap. Hogere waarden verhogen de terugspoelsnelheid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Terugspoelen Buffergrootte (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "De hoeveelheid geheugen (in MB) om te reserveren voor de terugspoelbuffer. Door dit te verhogen zal de hoeveelheid terugspoelgeschiedenis toenemen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Terugspoelen Buffergrootte-Stap (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Telkens wanneer de waarde van de terugspoelbuffergrootte wordt verhoogd of verlaagd, zal deze met dit bedrag veranderen."
   )

/* Settings > Frame Throttle > Frame Time Counter */


/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Opnamekwaliteit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Laag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Hoog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Verliesloos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM Snel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM hoge kwaliteit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Aangepaste Opname Configuratie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Draden Opnemen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Post Filter Opname Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Neem het beeld op nadat filters (maar geen shaders) zijn toegepast. De video zal er net zo mooi uitzien als wat je op uw scherm ziet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "GPU Opname Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Neem de output van GPU-gearceerd materiaal op, indien beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Streaming Modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "Lokaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Streaming Kwaliteit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Laag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Hoog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Aangepaste Streaming Configuratie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Stream Titel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "UDP Stream Poort"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Op-Scherm Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Schermranden en bedieningselementen op het scherm aanpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Videolay-out"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Videolay-out Aanpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Onscreen meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Pas de Onscreen Meldingen aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Zichtbaarheid van Meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "De zichtbaarheid van specifieke soorten meldingen omschakelen."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Overlay Weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Overlays worden gebruikt voor randen en scherm-besturingsemlementen."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Toon Overlay Achter Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Toon de overlay achter in plaats van voor het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Verberg Overlay In Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Verberg de overlay in het menu, en toon hem weer bij het verlaten van het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Verberg de overlay wanneer een controller is aangesloten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Verberg de overlay wanneer een fysieke controller is aangesloten op poort 1, en toon hem weer wanneer de controller wordt losgekoppeld."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Verberg de overlay wanneer een fysieke controller is aangesloten op poort 1, en toon hem weer wanneer de controller wordt losgekoppeld."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Invoer op Overlay weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Toont geregistreerde ingangen op de overlay op het scherm. 'Aangeraakt' markeert overlay-elementen die zijn ingedrukt/geklikt. Fysieke (Controller)' markeert daadwerkelijke invoer die aan de cores wordt doorgegeven, meestal van een aangesloten controller/toetsenbord."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Aangeraakt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Fysieke (Controller)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Toon ingangen van poort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Selecteer de poort van het invoerapparaat dat moet worden bewaakt wanneer \"Show Inputs on Overlay\" is ingesteld op \"Fysieke (Controller)\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Muiscursor met overlay tonen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Toon de muiscursor bij gebruik van een aan-scherm overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Overlay Auto-roteren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Indien ondersteund door de huidige overlay, roteren de lay-out automatisch om deze aan te passen aan de schermoriëntatie/zichtverhouding."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Auto-Schaal de Overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Past automatisch de schaal van de overlay en de afstand tussen UI-elementen aan de schermverhouding aan. Geeft de beste resultaten met controller-overlays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "D-Pad Diagonale Gevoeligheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Pas de grootte van de diagonale zones aan. Stel in op 100% voor 8-voudige symmetrie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "ABXY Overlappingsgevoeligheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Pas de grootte van de overlappingszones in de gezichtsknopdiamant aan. Stel in op 100% voor 8-voudige symmetrie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Analoog opnieuw centreren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Analoge stick invoer zal relatief zijn ten opzichte van eerste aanraking indien ingedrukt binnen deze zone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Laad geprefeerd overlay autom."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "Prefereer het laden van overlays gebaseerd op systeemnaam voordat u terugvalt naar de standaard voorinstelling. Wordt genegeerd als een overschrijving is ingesteld voor de overlay voorinstelling."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Overlay Transparantie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Doorzichtigheid van alle UI elementen van de overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Overlay preset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Selecteer een overlay uit de bestandsbrowser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(Landschap) Overlay Schaal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Schaal van alle UI-elementen van de overlay bij gebruik van landschapsdisplays."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(Landschap) Overlay Aspect Aanpassing"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Pas een beeldverhouding-correctiefactor toe op de overlay bij gebruik van liggende schermoriëntaties. Positieve waarden vergroten (en negatieve waarden verkleinen) de effectieve breedte van de overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Landschap) Overlay Horizontale Scheiding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Indien ondersteund door de huidige voorinstelling, pas de afstand tussen UI-elementen in de linker- en rechterhelft van een overlay aan bij gebruik van landschapsweergave. Positieve waarden vergroten (en negatieve waarden verkleinen) de scheiding tussen de twee helften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Landschap) Overlay Verticale Scheiding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Indien ondersteund door de huidige voorinstelling, pas de afstand tussen UI-elementen in de bovenste en onderste helften van een overlay aan bij gebruik van landschapsweergave. Positieve waarden vergroten (en negatieve waarden verkleinen) de scheiding tussen de twee helften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Landschap) Overlay X Offset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Horizontale overlay offset bij gebruik van liggende schermoriëntaties. Positieve waarden verschuiven de overlay naar rechts; negatieve waarden naar links."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Landschap) Overlay Y Offset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Verticale overlay offset bij gebruik van liggende schermoriëntaties. Positieve waarden verschuiven de overlay naar boven; negatieve waarden naar beneden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(Portret) Overlay Schaal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Schaal van alle UI-elementen van de overlay bij gebruik van staande schermoriëntaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(Portret) Overlay Aspect Aanpassing"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Pas een beelverhouding-correctiefactor toe op de overlay bij gebruik van staande schermoriëntaties. Positieve waarden verhogen (en negatieve waarden verlagen) de effectieve hoogte van de overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Portret) Overlay Horizontale Scheiding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Indien ondersteund door de huidige voorinstelling, pas de afstand tussen UI-elementen in de linker- en rechterhelft van een overlay aan bij gebruik van staande schermoriëntaties. Positieve waarden vergroten (en negatieve waarden verkleinen) de scheiding tussen de twee helften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Portret) Overlay Verticale Scheiding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Indien ondersteund door de huidige voorinstelling, pas de afstand tussen UI-elementen in de bovenste en onderste helften van een overlay aan bij gebruik van staande schermoriëntaties. Positieve waarden vergroten (en negatieve waarden verkleinen) de scheiding tussen de twee helften."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Portret) Overlay X Offset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Horizontale overlay offset bij gebruik van staande schermoriëntaties. Positieve waarden verschuiven de overlay naar rechts; negatieve waarden naar links."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Staand) Overlay Y Offset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Verticale overlay offset bij gebruik van staande schermoriëntaties. Positieve waarden verschuiven de overlay naar boven; negatieve waarden naar beneden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "Toetsenbord-overlay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "Selecteer en pas een toetsenbord-overlay aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "Overlay Lightgun, muis en Pointer inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "Gebruik aanraakinvoer buiten overlay-bedieningselementen als aanwijzerinvoer voor de core."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Configureer lightgun-invoer verzonden vanuit de overlay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Overlay Muis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Configureer muisinvoer verzonden vanuit de overlay. Opmerking: 1-, 2-- en 3-vinger aanrakingen sturen linker, rechter en middel muisklikken."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Selecteer een toetsenbord overlay uit de bestandsbrowser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Schaal Toetsenbord automatisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Pas toetsenbord overlay aan de originele hoogte-breedteverhouding. Schakel uit om uit te rekken naar het scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Keyboard Overlay ondoorzichtigheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Doorzichtigheid van alle UI-elementen van de overlay."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Lightgun Poort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Stel de core-poort in om invoer van het overlay lightgun ontvangen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Elke"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Activeren bij aanraking"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Stuur trigger invoer met aanwijzerinvoer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Trigger Vertraging (frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Vertraaging triggerinvoer om de cursor te laten bewegen. Deze vertraging wordt ook gebruikt om het juiste aantal aanraakpunten te wachten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Invoer bij 2 aanraakpunten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Selecteer de invoer om te verzenden wanneer twee aanwijzers op het scherm staan. Trigger Vertraging moet geen nul zijn om te onderscheiden van andere input."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Invoer bij 3 aanraakpunten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Selecteer de invoer om te verzenden wanneer drie aanwijzers op het scherm staan. Trigger Vertraging moet geen nul zijn om te onderscheiden van andere input."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Invoer bij 4 aanraakpunten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Selecteer de invoer om te verzenden wanneer vier aanwijzers op het scherm staan. Trigger Vertraging moet geen nul zijn om te onderscheiden van andere input."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Buiten het scherm toestaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Sta toe dat er buiten het scherm gericht wordt. Schakel uit om het mikpunt buiten het scherm naar binnen het scherm te verplaatsen."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Muissnelheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Pas de bewegingssnelheid van de cursor aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Lang indrukken om te slepen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Lang drukken op het scherm om te beginnen met het indrukken van een knop."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Drempel voor lang indrukken (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Pas de tijd aan die nodig is voor een lange druk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Dubbeltikken om te slepen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Dubbel tikken op het scherm om een knop op de tweede tik ingedrukt te houden. Voegt latentie aan muisklikken toe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Dubbeltik drempel (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Pas de toegestane tijd tussen tikken aan bij het detecteren van een dubbele tik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Veeggevoeligheid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
   "Alternative invoer bij 2 aanrakingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_ALT_TWO_TOUCH_INPUT,
   "Tweede aanraken als muisknop gebruiken bij het besturen van de cursor."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Pas het toegestane driftbereik aan bij het detecteren van een lange druk of tik. Uitgedrukt als percentage van de kleinere schermdimensie."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Onscreen Berichten Weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Toon berichten op het scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Grafische Widgets"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Gebruik versierde animaties, meldingen, indicatoren en bedieningsapparaten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Grafische Widgets Automatisch Schalen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Automatisch gedecoreerde meldingen, indicatoren en bedieningen aanpassen op basis van de huidige menuschaal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Grafische Widgets Schaal Overschrijven (Volledig-scherm)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Pas een handmatige schaalfactor toe bij het tekenen van weergavewidgets in volledig scherm. Alleen van toepassing wanneer 'Grafische Widgets Automatisch Schalen' is uitgeschakeld. Kan worden gebruikt om de grootte van versierde meldingen, indicatoren en bedieningselementen onafhankelijk van het menu zelf te vergroten of te verkleinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Grafische Widgets Schaal Overschrijven (Venster)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Pas een handmatige schaalfactor toe bij het tekenen van weergavewidgets in venstermodus. Alleen van toepassing wanneer 'Grafische Widgets Automatisch Schalen' is uitgeschakeld. Kan worden gebruikt om de grootte van versierde meldingen, indicatoren en besturingselementen onafhankelijk van het menu zelf te vergroten of te verkleinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Framerate weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Geef de huidige frames per seconde weer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Framerate Update-interval (In Frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "De framerate-weergave wordt bijgewerkt met het ingestelde interval in frames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Kadertelling weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Toont de huidige kadertelling op het scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Statistieken Tonen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Technische statistieken op het scherm weergeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Geheugengebruik weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Geeft de gebruikte en totale hoeveelheid geheugen op het systeem weer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Update-interval geheugengebruik (in frames)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "De weergave van het geheugengebruik wordt bijgewerkt met het ingestelde interval in frames."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_SHOW,
   "Laat tijd zien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIME_SHOW,
   "De huidige tijd in het voorkeursindeling weergeven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Toon Netplay Ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Toont de ping voor de huidige netplaykamer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "\"Laad Inhoud\" Opstartmelding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Toon een korte lanceeranimatie bij het laden van de inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Invoer (Autoconfig) Verbindingsmeldingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Foutmeldingen invoer (Autoconfiguratie)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Cheatcode Meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Geef een bericht op het scherm wanneer cheatcodes worden toegepast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Patchmeldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Een bericht op het scherm weergeven bij soft-patching van ROM's."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Een bericht op het scherm weergeven bij het aansluiten/ontkoppelen van invoerapparaten."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Toon een melding wanneer invoertoestellen niet konden worden geconfigureerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Invoer Remap-geladen Meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Toon een bericht op het scherm bij het laden van invoer-remap-bestanden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Configuratie Overschrijven Geladen Meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Een bericht op het scherm weergeven bij het laden van configuratie-overschrijvende bestanden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Initiële Schijf Hersteld Meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Een bericht op het scherm weergeven wanneer bij het opstarten automatisch de laatst gebruikte disk van via M3U-afspeellijsten geladen inhoud met meerdere disks wordt hersteld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Schijfbeheer notificaties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Toon een melding bij het invoeren of uitwerpen van schijven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Onderbrekingspuntmeldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "Toon een melding bij het opslaan en laden van onderprekingsppunten."
   )
MSG_HASH( /* FIXME: Rename config key and msg hash */
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Frame throttling-meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Een melding tonen wanneer vooruitspoelen, slow motion of terugspoelen actief is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Schermafbeelding Meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Een schermbericht weergeven tijdens het maken van een schermafbeelding."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Schermafbeelding Melding Volharding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Bepaal de duur van het schermafbeelding schermbericht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "Normaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Snel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Zeer Snel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Schermafbeelding Flash Effect"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Een wit knipperend effect op het scherm weergeven met de gewenste duur bij het maken van een schermafbeelding."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "AAN (Standaard)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "AAN (Snel)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Meldingen over de verversingssnelheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Een bericht op het scherm weergeven bij het instellen van de vernieuwingsfrequentie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Extra Netplay Meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Toon niet-essentiële netplay berichten op het scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Menu-exclusieve Meldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Meldingen alleen weergeven als het menu is geopend."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Onscreen Berichten Font"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Selecteer het lettertype voor meldingen op het scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Onscreen Berichten Grootte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Specificeer de lettergrootte in punten. Als widgets worden gebruikt, heeft dit alleen effect op de statistiekenweergave op het scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "OSD Berichten X-as positie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Geef de aangepaste X-as positie op voor tekst op het scherm. 0 is de linkerrand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "OSD Berichten Y-as positie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Geef de aangepaste Y-as positie op voor tekst op het scherm. 0 is de onderrand"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Meldingskleur (rood)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Stelt de rode waarde van de overlay-tekst in. Geldige waarden zijn tussen 0 en 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Meldingskleur (groen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Stelt de groene waarde van de overlay-tekst in. Geldige waarden zijn tussen 0 en 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Notificatie Kleur (Blauw)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Stelt de blauwe waarde van de overlay-tekst in. Geldige waarden zijn tussen 0 en 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Notificatie Achtergrond"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Schakel een achtergrondkleur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Achtergrondkleur melding (rood)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Stelt de rode waarde van de overlay-achtergrondkleur in. Geldige waarden zijn tussen 0 en 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Achtergrondkleur melding (groen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Stelt de groene waarde van de overlay-achtergrondkleur in. Geldige waarden zijn tussen 0 en 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Achtergrondkleur melding (blauw)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Stelt de blauwe waarde van de overlay-achtergrondkleur in. Geldige waarden zijn tussen 0 en 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Ondoorzichtigheid v/d achtergrond van melding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Hiermee stelt u de ondoorzichtigheid van de overlay-achtergrondkleur in. Geldige waarden zijn tussen 0,0 en 1.0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Uiterlijk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Verander weergave instellingen van het menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Zichtbaarheid Menu-Item"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "De zichtbaarheid van menu-items in RetroArch omschakelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Pauzeer inhoud wanneer het menu actief is"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Pauzeer de inhoud wanneer het menu actief is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Pauzeer Inhoud op inactiviteit "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Pauzeer de inhoud wanneer RetroArch niet het actieve venster is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Sluiten bij sluiten van inhoud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Verlaat RetroArch automatisch bij het sluiten van inhoud. 'CLI' stopt alleen als de inhoud via een command-line-interface wordt gelanceerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Hervat inhoud na het gebruik van onderbrekingspunten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Automatisch het menu sluiten en de inhoud hervatten na het opslaan of laden van een onderbrekingspunt. Uitschakelen hiervan kan het opslaan van de prestaties van onderbrekingspunten op zeer trage apparaten verbeteren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Content hervatten na wijzigen van schijven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Automatisch het menu sluiten en de inhoud hervatten na het invoegen of laden van een nieuwe schijf."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Navigatie Wrap-Around"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Spring naar het begin en/of einde wanneer de ander kant van de lijst wordt bereikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Geavanceerde Instellingen weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Toon geavanceerde instellingen voor power-gebruikers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Kioskmodus Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Beschermt de installatie door alle configuratie gerelateerde instellingen te verbergen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Stel wachtwoord in voor het uitschakelen van Kioskmodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Het inschakelen van een wachtwoord wanneer u de kioskmodus inschakelt, maakt het mogelijk om het later uit te schakelen in het menu, door naar het hoofdmenu te gaan en door Kioskmodus uit te schakelen en het wachtwoord in te voeren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Muis Ondersteuning"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Sta muisbesturing toe in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Touch Ondersteuning"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Sta aanraakschermbesturing toe in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Bedrade Taken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Taken uitvoeren op een aparte thread."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Time-out voor schermbeveiliging in menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Terwijl het menu actief is, wordt de schermbeveiliging ingeschakeld na een bepaalde tijdsduur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Animatie voor schermbeveiliging in menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Schakel een animatie-effect in wanneer de menuscreensaver actief is. Heeft een bescheiden impact op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Sneeuw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Sterrenveld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Snelheid screensaver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "De snelheid van het menuscreensaver animatie-effect aanpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Desktop Compositie Deactiveren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Vensterbeheerders gebruiken venstercompositie om visuele effecten toe te passen, niet-responsieve vensters te detecteren, onder andere"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Forceer het uitschakelen van venstercompositie. Uitschakelen is momenteel alleen geldig op Windows Vista/7."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "UI Companion Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Start UI Companion bij opstarten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Start het User Interface Companion stuurprogamma tijdens het opstarten (indien beschikbaar)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Bureaublad menu (opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Open Bureaublad Menu bij het Opstarten"
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "Uiterlijk onderste 3DS scherm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Instellingen voor uiterlijk onderaan scherm wijzigen."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "App-pictogram"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Wijzig app-pictogram"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Snelmenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "De zichtbaarheid van menu-items in het Snel Menu omschakelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Instellingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "De zichtbaarheid van menu-items in de Instellingen-menu omschakelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Toon 'Laad Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Toon de 'Laad Core' optie in het Hoofdmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Toon 'Inhoud Laden'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Toon de 'Inhoud Laden' optie in het Hoofdmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Toon 'Laad Disc'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Toon de 'Laad Disc' optie in het Hoofdmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Toon 'Dump Disk'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Toon de 'Dump Disk' optie in het Hoofdmenu."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "Toon 'Disc Uitwerpen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "De 'Disc Uitwerpen' optie in het Hoofdmenu weergeven."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Toon \"Online Updater\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Toon de \"Online Updater\" optie in het hoofdmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Toon \"Core-downloader\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Toon de mogelijkheid om cores (en core-info-bestanden) bij te werken in de optie \"Online updater\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Toon 'Informatie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Toon de 'Informatie' optie in het Hoofdmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Toon \"Configuratiebestand\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Toon de \"Configuratiebestand\" in het hoofdmenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Toon \"Help\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Toon de \"Help\" in het hoofdmenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Toon \"Sluit RetroArch\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Toon de 'RetroArch Afsluiten' optie in het Hoofdmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Toon 'Herstart RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Toon de 'RetroArch herstarten' optie in het hoofdmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Toon 'Instellingen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Toon het \"Instellingen\" menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Stel wachtwoord in voor \"Instellingen\" in te schakelen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Een wachtwoord instellen bij het verbergen van het instellingentabblad maakt het mogelijk om dit later in het menu te herstellen, door naar het hoofdmenu tabblad te gaan, door \"Instellingentabblad inschakelen\" te selecteren en het wachtwoord in te voeren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Favorieten Weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Toon het \"Favorieten\" menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES_FIRST,
   "Favorieten eerst weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES_FIRST,
   "Toon \"Favorieten\" voor \"Geschiedenis\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Toon 'Afbeeldingen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Toon het \"Afbeeldingen\"-menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Toon 'Muziek'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Toon het 'Muziek'-menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Toon 'Video's'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Toon het 'Videos'-menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Toon 'Netplay'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Toon het 'Netplay'-item in het hoofdmenu of afspeellijsten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Toon 'Geschiedenis'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Toon het menu voor recente geschiedenis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Toon \"Importeer Inhoud\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Toon de \"Importeer Inhoud\" regel in het hoofdmenu of afspeellijsten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Hoofdmenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Afspeellijsten Menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Toon 'Afspeellijsten'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Toon de afspeellijsten in het hoofdmenu. Genegeerd in GLUI als afspeellijsten en navigatiebalk zijn ingeschakeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "Toon afspeellijsttabbladen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "Toon de afspeellijsttabbladen. Dit heeft geen invloed op RGUI. Navigatiebalk moet ingeschakeld zijn in GLUI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Toon 'Verken'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Laat de optie voor inhoudverkenner zien."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Toon 'Inhoudloze Cores'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Geef het type van de core (indien aanwezig) aan in het menu \"Inhoudloze cores\". Wanneer ingesteld op \"Aangepast\", kan individuele core zichtbaarheid aangezet worden via het \"Beheer Cores\" menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Alle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Eenmalig gebruik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Tijd/datum weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Toon de huidige datum en/of tijd in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Stijl van datum / tijd"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Wijzig hoe de huidige datum en/of tijd wordt weergegeven in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Datumscheidingsteken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Geef het teken op dat u wilt gebruiken als scheidingsteken tussen jaar/maand/dag onderdelen wanneer de huidige datum in het menu wordt weergegeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Toon batterijniveau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Batterijstand weergeven in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Core naam weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Toon de naam van de huidige core in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Toon Menu Sub-Labels"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Toon aanvullende informatie voor menu-items"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Start Scherm Weergeven"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Toon opstartscherm in het menu. Dit wordt automatisch uitgeschakeld nadat het programma voor het eerst start."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Toon 'Hervatten'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Toon de optie om de inhoud te hervatten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Toon \"Reset\"."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Toon de optie om de inhoud opnieuw op te starten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Toon 'Inhoud sluiten'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Toon de optie om inhoud te sluiten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Toon \"Onderbrekingspunt\" submenu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Toon onderbrekingspuntopties in een submenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Toon \"Maak/laad onderbrekingspunt\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Toon de opties om een onderbrekingspunt te maken of laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "Toon \"Herhalingsinstellingen\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "Toon de opties voor opname/afspelen van harhalingsbestanden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Toon \"onderbrekingspunt opslaan/laden ongedaan maken\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Toon de opties om het aanmaken/laden van onderbrekingspunten ongedaan te maken. Start activeert aanmaken/laden wanneer verborgen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Toon \"Core-opties\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Toon de \"Core-opties\" optie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Toon \"Opties op schijf opslaan\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Toon het \"Opties op schijf opslaan\" item in het \"Opties > Beheer core-opties\" menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Toon \"Besturingselementen\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Toon de \"Besturingselementen\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Toon \"Schermafdruk maken\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Toon de \"Schermafdruk maken\" optie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Toon \"Start Opname\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Toon de \"Start Opname\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Toon \"Start Streamen\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Toon de \"Start Streamen\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Toon \"Op-Scherm Overlay\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Toon de \"Op-Scherm Overlay\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Toon \"Videolay-out\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Toon de \"Videolay-out\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Toon \"Latentie\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Toon de \"Latentie\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Toon \"Terugspoelen\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Toon de \"Terugspoelen\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Toon \"Core-overrides opslaan\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Toon de \"Core-overrides opslaan\" optie in het \"Overrides\" menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Toon \"Inhoudsmap-overrides opslaan\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Toon de \"Inhoudsmap-overrides opslaan\" optie in het \"Overrides\" menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Toon \"spel-overrides opslaan\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Toon de \"spel-overrides opslaan\"\" optie in het \"Overrides\" menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Toon \"Cheats\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Toon de \"Cheats\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Toon \"Shaders\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Toon de \"Shaders\" option"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Toon \"Toevoegen aan Favorieten\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Toon de \"Toevoegen aan Favorieten\" optie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Toon \"Voeg toe aan afspeellijst\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Toon de \"Voeg toe aan afspeellijst\" optie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Toon \"Stel gekoppelde core in\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Toon de \"Stel gekoppelde core in\" optie wanneer inhoud niet wordt uitgevoerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Toon \"Reset gekoppelde core\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Toon de \"Reset core-koppeling\" optie wanneer inhoud niet wordt uitgevoerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Toon \"Download miniaturen\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Toon de \"Download miniaturen\" optie wanneer inhoud niet wordt uitgevoerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Toon 'Informatie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Toon de \"Informatie\" optie"
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Toon 'Drivers'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Toon 'Drivers' instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Toon 'Video's'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Toon 'Drivers' instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Toon \"Audio\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Toon \"Audio\" instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Toon \"Invoer\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Toon de \"Invoer\" instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Toon \"Latentie\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Toon de \"Latentie\" instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Toon \"Core\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Toon de \"Core\" instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Toon 'Configuratie'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Toon 'Configuratie' instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Toon \"Opslaan\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Toon de \"Opslaan\" instelingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Toon \"Loggen\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Toon de \"Loggen\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Toon \"Bestandsbeheer\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Toon \"Bestandsbeheer\" instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Toon \"Frame Throttle\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Toon de \"Frame Throttle\" instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Toon \"Opname\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Toon de \"Opname\" instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Toon \"Onscreen Weergave\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Toon de \"Onscreen Weergave\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Toon \"Gebruikersinterface\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Toon de \"Gebruikersinterface\" instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Toon \"AI-Service\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Toon de \"AI-Service\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Toon \"Toegankelijkheid\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Toon de \"Toegankelijkheid\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Toon \"Energiebeheer\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Toon de \"Energiebeheer\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Toon \"Achievements\"."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Toon \"Achievements\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Toon \"Netwerk\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Toon de \"Netwerk\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Toon 'Afspeellijsten'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Toon de \"Afspeellijsten\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Toon 'Gebruiker'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Toon de \"Gebruiker\" instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Toon \"Map\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Toon de \"Map\" instellingen."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "Toon \"Steam\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "Toon de \"Steam\" instellingen."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Schaalfactor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Schaal de grootte van de gebruikersinterface-elementen in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Achtergrondafbeelding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Selecteer een afbeelding om in te stellen als menu-achtergrond. Handmatige en dynamische afbeeldingen overschrijven het 'Kleurthema'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Achtergrond doorzichtigheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Wijzig de ondoorzichtigheid van de achtergrondafbeelding."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Transparantie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Wijzig de ondoorzichtigheid van de standaard menuachtergrond."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Gebruik de themakleuren van het systeem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Gebruik het kleurenthema van het besturingssysteem (indien aanwezig). Overschrijft thema-instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Primaire miniatuur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Type miniatuur om weer te geven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Drempelgrootte om miniaturen te schalen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Automatisch miniatuurafbeeldingen opschalen met een breedte/hoogte kleiner dan de opgegeven waarde. Beeldkwaliteit verbetert de beeldkwaliteit. Dit heeft een gematigde impact op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Miniatuurachtergronden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Maakt het opvullen van ongebruikte ruimte in miniatuurafbeeldingen met een vaste achtergrond mogelijk. Dit zorgt voor een uniforme weergavegrootte voor alle afbeeldingen, verbetert het weergave van het menu bij het bekijken van gemengde inhoudminiaturen met verschillende basisafmetingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Scrollende tekst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Selecteer horizontale scrollmethode die wordt gebruikt om lange tekst in het menu weer te geven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Scrollende tekst snelheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "De animatiesnelheid bij het scrollen van lange tekst in het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Vloeiende scrollende tekst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Gebruik een vloeiende scrollanimatie bij het weergeven van lange menutekst. Heeft een kleine impact op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Onthoud selectie bij wijzigen van tabbladen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Onthoud de vorige positie van de cursor in tabbladen. RGUI heeft geen tabs, maar afspeellijsten en instellingen gedragen zich als zo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Altijd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Alleen voor Afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Alleen voor hoofdmenu en instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_STARTUP_PAGE,
   "Startpagina"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_STARTUP_PAGE,
   "Eerste menu-pagina bij het opstarten."
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "AI-Service Uitvoer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Toon de vertaling als een tekstoverlay (Afbeeldingenmodus), speel als Tekst-naar-spraak (Spraak), of gebruik een systeemverhaling zoals NVDA (Verteller)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_BACKEND,
   "AI-Service backend"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_BACKEND,
   "Selecteer welke vertalingbackend moet worden gebruikt. HTTP gebruikt een externe server via de geconfigureerde URL. Apple gebruikt de OCR en vertaling van het apparaat (macOS/iOS)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "AI-Service URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Een http:// URL die naar de te gebruiken vertaalservice verwijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "AI-Service Ingeschakeld"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "AI-Service inschakelen om uit te voeren wanneer de AI-Service sneltoets wordt ingedrukt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Pauzeer tijdens de vertaling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Pauzeer core terwijl het scherm wordt vertaald."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Brontaal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "De taal waaruit de dienst zal vertalen. Indien ingesteld op \"Standaard\", zal het proberen de taal automatisch te detecteren. Het instellen in een specifieke taal zal de vertaling nauwkeuriger maken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Doeltaal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "De taal waarnaar de dienst zal vertalen. \"Standaard\" is Engels."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Toegankelijkheid inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Schakel Tekst-naar-spraak in om te helpen in de menunavigatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Tekst-naar-spraak snelheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "De snelheid van de tekst-naar-spraak stem."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Achievements Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Verdien achievements in klassieke spellen. Voor meer informatie bezoek \"https://retroachievements.org\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Hardcore-modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Schakelt cheats, terugspoelen, slow motion en onderbrekingsputen uit. Prestaties verdiend in hardcore modus zijn uniek gemarkeerd zodat je anderen kunt laten zien wat je hebt bereikt zonder emulatorondersteuningsfuncties. Als je deze instelling bij runtime inschakelt, wordt het spel opnieuw opgestart."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Ranglijsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Verzend periodiek contextuele spelinformatie naar de RetroAchievements website. Heeft geen effect als 'Hardcore Modus' is ingeschakeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Achievement-badges"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Toon badges in de achievementlijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Test onofficiële achievements"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Zet onofficiële achievements en of/bèta-functies aan of uit voor testdoeleinden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Geluid bij het ontgrendelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Speel een geluid af wanneer een achievement wordt ontgrendeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Automatische schermafbeelding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Maak automatisch een schermafbeelding als een prestatie wordt verdiend."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Encore-modus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Start de sessie met alle achievements actief (zelfs de eerder ontgrendelde)."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Uiterlijk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Wijzig de positie en offsets van achievements op het scherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Positie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Stel de hoek/rand in van het scherm waarop achievement-meldingen worden weergegeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Linksboven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "Middenboven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Rechtsboven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Linksonder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Middenonder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "Rechtsonder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Standaarduitlijning"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Stel in of achievement-meldingen moeten uitgelijnd worden met andere soorten on-screen meldingen. Schakel uit om handmatige padding/positie waarden in te stellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "Manuele horizontale uitlijning"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Afstand van de linker/rechter rand van het scherm, wat overscan van het scherm kan compenseren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "Handmatige verticale opvulling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Afstand van de bovenste/onderste rand van het scherm, wat overscan van het scherm kan compenseren."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "Zichtbaarheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "Verander welke berichten en op het scherm elementen worden weergegeven. Schakelt functionaliteit niet uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "Samenvatting bij het starten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "Alle geïdentificeerde spellen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "Spellen met achievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "Ontgrendelmeldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "Toont een notificatie wanneer een achievement wordt ontgrendeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "Meesterschapmeldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "Toont een notificatie wanneer alle achievements van een spel ontgrendeld zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Actieve uitdagingindicatoren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Toont op het scherm indicatoren terwijl bepaalde achievements kunnen worden verdiend."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Voortgangsindicator"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Toont een indicator op het scherm wanneer er vooruitgang wordt gemaakt in de richting van bepaalde achievements ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "Startberichten voor de ranglijst "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "Toont een beschrijving van een ranglijst wanneer het actief wordt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Bericht bij het verzenden naar een ranglijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Toont een bericht met de waarde die wordt ingediend wanneer een poging voor de ranglijst is voltooid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Bericht als de ranglijst niet behaald is"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Toont een bericht wanneer een ranglijstpoging is mislukt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Ranglijsttrackers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Toont trackers op het scherm met de huidige waarde van actieve ranglijsten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Loginberichten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Toont berichten met betrekking tot de RetroAchievements account login."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Uitgebreide berichten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Toont extra diagnostische en foutberichten."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Publiek Netplay aankondigen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Of je netplay-spellen publiekelijk wilt aankondigen. Indien dit niet is ingesteld moeten cliënten handmatig verbinden in plaats van de publieke lobby te gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Gebruik relay-server"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Stuur netplay verbindingen door een man-in-the-middle server. Handig als de host zich achter een firewall bevindt of NAT/UPnP problemen heeft."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Relay-server locatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Kies een specifieke relay-server om te gebruiken. Geografisch nauwere locaties hebben meestal een lagere latentie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Aangepast relay-server adres"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Voer het adres van uw aangepaste relay-server hier in. Formaat: adres of adres|poort."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "Noord-Amerika (East Coast, USA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "West-Europa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "Zuid-Amerika (Southeast, Brazilië)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Zuidoost-Azië"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Server Adres"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Het adres van de host waar we naartoe verbinden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Netplay TCP/UDP Poort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "De poort van het host IP-adres. Kan een TCP of UDP-poort zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Maximale verbindingen tegelijkertijd"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Het maximale aantal actieve verbindingen dat de host accepteert voordat nieuwe worden geweigerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Pinglemiet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "De maximale verbinding latentie (ping) die de host accepteert. Zet het op 0 voor geen limiet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Serverwachtwoord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Het wachtwoord dat gebruikt wordt door clients die verbinding maken met de host."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Serverwachtwoord voor toeschouwers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Het wachtwoord dat gebruikt wordt door clients die verbinding maken met de host als toeschouwer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Netplaytoeschouwermodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Start netplay in toeschouwermodus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Of netplay moet starten in toeschouwermodus. Indien ingeschakeld zal netplay in de toeschouwermodus staan bij het starten. Het is altijd mogelijk om later de modus te wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Chat vervagen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Vervaag chatberichten na verloop van tijd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Chatkleur (bijnaam)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Chatkleur (bericht)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Pauzeren toestaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Spelers toestaan om te pauzeren tijdens de netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Slave-Modus cleints toestaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Toestaan dat verbindingen in slave-modus. Slave-modus-clients vereisen aan beide zijden zeer weinig processorkracht, maar zullen aanzienlijk lijden onder netwerklatentie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Niet-slave-modus clients weigeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Verbied verbindingen niet in slave-modus. Niet aanbevolen behalve voor zeer snelle netwerken met zeer zwakke machines."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Betplay controleframes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "De frequentie (in frames) dat netplay zal controleren of de host en client gesynchroniseerd zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "De frequentie in frames waarmee netplay zal controleren of de host en client gesynchroniseerd zijn. Met de meeste cores heeft deze waarde geen zichtbaar effect en kan worden genegeerd. Met nondeterminstische cores bepaalt deze waarde hoe vaak de netplay peers gesynchroniseerd zullen worden. Met buggy cores, zal het instellen van dit op een niet-nulwaarde ernstige prestatieproblemen veroorzaken. Zet op nul om geen controles uit te voeren. Deze waarde wordt alleen gebruikt op de netplay host."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Invoerlatentieframes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Het aantal frames van invoerslatentie voor netplay om de netwerkvertraging te verbergen. Vermindert haperingen en maakt netplay minder CPU-intensief, ten koste van merkbare invoervertraging."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Het aantal frames van ingangslatentie voor netplay om de netwerkvertraging te verbergen.\nWanneer in het netplay, vertraagt deze optie lokale input, zodat het frame dat wordt uitgevoerd dichter bij de frames komt die worden ontvangen van het netwerk. Dit vermindert jitter en maakt netplay minder CPU-intensief, maar tegen de prijs van merkbare invoervertraging."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Bereik Invoerlatentieframes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Het scala aan frames van invoerslatentie dat kan worden gebruikt om de netwerklatentie te verbergen. Vermindert haperingen en maakt netplay minder CPU-intensief, ten koste van onvoorspelbare invoervertraging."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Het bereik aan frames van invoerslatentie dat kan worden gebruikt door netplay om de netwerk latentie te verbergen.\nAls dit is ingesteld zal netplay het aantal frames van invoerslatentie  dynamisch aanpassen om CPU tijd in balans te brengen, latency en netwerklatentie  tijd in te voeren. Dit vermindert haperingen en maakt netplay minder CPU-intensief, maar tegen de prijs van onvoorspelbare invoervertraging."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Netplay NAT-traversal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Probeer tijdens het hosten te luisteren naar verbindingen van het openbare internet, door UPnP of soortgelijke technologieën te gebruiken om uit LAN te ontsnappen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Deel digitale invoer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Apparaat %u aanvragen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Verzoek om te spelen met het gegeven invoerapparaat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Netwerk Commando's"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Netwerk Commando Poort"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "Netwerk-RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Basis poort voor de netwerk-RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Gebruiker %d netwerk-RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "stdin Commandos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Enable stdin command interface."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Miniatuurdownloads op aanvraag"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Download ontbrekende miniaturen automatisch tijdens het bladeren door afspeellijsten. Dit heeft een ernstige impact op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Updaterinstellingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Ga naar core-updaterinstellingen"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Buildbot cores URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL naar de coreuupdatermap op de libretro buildbot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Buildbot-assets-URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL naar de Buildbot-assets-updatermap op de libretro buildbot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Automatisch uitpakken van gedownloade archieven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Na het downloaden worden bestanden uit de gedownloade archieven automatisch uitgepakt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Laat experimentele cores zien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "\"Experimentele\" cores opnemen in de Core-downloader-lijst. Deze zijn meestal voor ontwikkelings-/testdoeleinden en worden niet aanbevolen voor algemeen gebruik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Back-up cores tijdens het bijwerken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Automatisch een back-up maken van geïnstalleerde cores bij het uitvoeren van een online update. Maakt een gemakkelijke terugkeer naar een werkende kern mogelijk als een update een regressie introduceert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Core back-up geschiedenis grootte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Specificeert hoeveel automatisch gegenereerde back-ups te houden voor elke geïnstalleerde core. Wanneer deze limiet wordt bereikt, zal het maken van een nieuwe back-up via een online update de oudste back-up verwijderen. Handmatige kern back-ups worden door deze instelling niet beïnvloed."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Geschiedenis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Behoud een afspeellijst van recent gebruikte spellen, afbeeldingen, muziek en video's."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Geschiedenisgrootte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Beperk het aantal items in recente afspeellijst voor spellen, afbeeldingen, muziek en video's."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Grootte favoriete"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Beperk het aantal items in de afspeellijst 'Favorieten'. Zodra de limiet is bereikt, zullen nieuwe toevoegingen worden voorkomen totdat oude worden verwijderd. Het instellen van een waarde van -1 staat een ongelimiteerd aantal toe.\nWAARSCHUWING: Het verlagen van de waarde zal bestaande favorieten verwijderen!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Sta het hernoemen van items toe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Toestaan dat afspeellijstitems worden hernoemd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Sta het verwijderen van items toe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Toestaan dat afspeellijst items worden verwijderd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Sorteer Afspeellijsten alfabetisch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Sorteer afspeellijsten in alfabetische volgorde, met uitzondering van de afspeellijsten \"Geschiedenis\", \"Afbeeldingen\", \"Muziek\" en \"Video''."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Sla afspeellijsten op met oud formaat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Schrijf afspeellijsten met verouderde platte tekst indeling. Indien uitgeschakeld, worden afspeellijsten geformatteerd met JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Comprimeer afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Archiveer afspeellijstdata bij het schrijven naar schijf. Vermindert bestandsgrootte en laadtijden ten koste van (verwaarloosbaar) verhoogd CPU gebruik. Kan met oude of nieuwe formaten afspeellijsten worden gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Toon gekoppelde cores in afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Toon Afspeellijstsublabels"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Toon aanvullende informatie voor elk afspeellijstitem, zoals de huidige gekoppelde core en runtime (indien beschikbaar). Heeft een variabele prestatieimpact."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Toon inhoudsspecifieke iconen in geschiedenis en favorieten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Toon specifieke iconen voor elke geschiedenis en favorieten afspeellijst item. Heeft een variabele impact op prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Looptijd:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Laatst Gespeeld:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_PLAY_COUNT,
   "Aantal keer gespeeld:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "seconde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "secondes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "minuut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "minuten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "uur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "uren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "dag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "dagen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "weken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "maand"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "maanden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "jaar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "jaren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "geleden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Toon afspeellijstitemindex"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Toon itemnummers tijdens het bekijken van afspeellijsten. Weergaveformaat is afhankelijk van het huidige geselecteerde menu-stuurprogramma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Looptijd voor afspeellijstsublabels"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Datum en tijdstijl \"Laatst gespeeld\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Stel de stijl in van de datum en tijd die wordt weergegeven voor 'Laatst Gespeeld' tijdstempel informatie. \"(AM/PM)\" opties zullen een kleine impact hebben op sommige platformen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Gebruik fuzzy matching voor archieven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Bij het zoeken naar afspeellijstitems die gekoppeld zijn aan gecomprimeerde bestanden, matcht alleen de archiefbestandsnaam in plaats van [bestandsnaam]+[inhoud]. Schakel dit in om dubbele inhoudsgeschiedenisitems te voorkomen bij het laden van gecomprimeerde bestanden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Scannen zonder overeenkomende code"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Toestaan dat de inhoud wordt gescand en toegevoegd aan een afspeellijst zonder dat er een core geïnstalleerd is die dit ondersteunt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "Controller CRC op mogelijke duplicaten tijdens het scannen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "Soms hebben ISO's dubbele serienummers, met name met PSP/PSN-titels. Als je uitsluitend serienummers gebruikt, zet de scanner soms de inhoud in het verkeerde systeem. Dit voegt een CRC-controle toe, wat het scannen aanzienlijk vertraagt, maar misschien nauwkeuriger is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Afspeellijsten beheren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Onderhoudstaken uitvoeren op afspeellijsten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Draagbare afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Wanneer ingeschakeld, en de map \"Bestandsbeheer\" ook is geselecteerd, wordt de huidige waarde van parameter \"Bestandsbeheer\" opgeslagen in de afspeellijst. Wanneer de afspeellijst is geladen op een ander systeem waar dezelfde optie is ingeschakeld wordt de waarde van parameter \"Bestandsbeheer\" vergeleken met de waarde van de afspeellijst; bij verschillen worden de paden van de afspeellijst automatisch aangepast."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "Gebruik bestandsnamen voor het matchen van miniaturen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "Indien ingeschakeld, zullen miniaturen worden gevonden op basis van de bestandsnaam van het item, in plaats van het label."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "Alle ondersteunde afbeeldingstypes voor miniaturen toestaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "Wanneer ingeschakeld, kunnen lokale miniaturen worden toegevoegd in alle afbeeldingstypes die worden ondersteund door RetroArch (zoals jpeg). Kan een klein prestatie-effect hebben."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "Beheren"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Staandaardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Specificeer de core die gebruikt moet worden bij het starten van de inhoud via een afspeellijstitem die waar geen bestaande core aan gekoppeld is heeft."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Reset core-koppelingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Verwijder bestaande core-koppelingen voor alle afspeellijstitems."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Labelweergavemodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Verander hoe de inhoudslabels worden weergegeven in deze afspeellijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Sorteermethode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Bepaal hoe items worden gesorteerd in deze afspeellijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Afspeellijst opruimen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Valideer core-koppelingen en verwijder ongeldige en dubbele vermeldingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Vernieuw afspeellijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Voeg nieuwe inhoud toe en verwijder ongeldige items door de inhoudsscanoperatie te herhalen die laatst is gebruikt om de afspeellijst aan te maken of te bewerken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Afspeellijst verwijderen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Verwijder afspeellijst van het bestandssysteem."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Privacyinstelling wijzigen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Beheer momenteel geconfigureerde accounts."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Gebruikersnaam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Voer hier je gebruikersnaam in. Dit zal o.a. gebruikt worden voor netplay sessies."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Taal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Stel de taal van de gebruikersinterface in."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Lokaliseert het menu en alle berichten op het scherm volgens de taal die u hier hebt geselecteerd. Vereist een herstart om de wijzigingen door te voeren.\nVertalingsvolledigheid wordt weergegeven naast elke optie. In het geval dat een taal niet is geïmplementeerd voor een menu-item, vallen we terug op Engels."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Camera Toestaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Geef cores toegang tot de camera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Discord Inschakelen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Locatie toestaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Geef cores toegang tot je locatie."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Verdien achievements in klassieke spellen. Voor meer informatie bezoek \"https://retroachievements.org\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Login gegevens voor je RetroAchievements account. Ga naar retroachievements.org en meld je aan voor een gratis account.\nNadat je klaar bent met registreren, moet je de gebruikersnaam en het wachtwoord invoeren in RetroArch."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Gebruikersnaam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Voer de gebruikersnaam in van uw RetroAchievements-account."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Wachtwoord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Voer het wachtwoord in van uw RetroAchievements-account. Maximale lengte: 255 tekens."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "YouTube streamsleutel"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Twitch streamsleutel"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Facebook Gaming streamsleutel"
   )

/* Settings > User > Accounts > Kick */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Systeem/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOS'en, boot ROMs, en andere systeem specifieke bestanden worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Gedownloade bestanden worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Menu-assets gebruikt door RetroArch worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Dynamische Wallpapers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Achtergrondafbeeldingen die gebruikt worden in het menu worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Miniaturen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Doosafbeelding, schermafdruk en titelscherm miniaturen worden opgeslagen in deze map."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Favorieten"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Stel de startmap in voor bestandsbrowser."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Configuratiebestanden"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Standaard configuratiebestand is opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Libretro cores worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Applicatie/core informatiebestanden worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Databases worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Cheatbestanden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Cheatbestanden worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Videofilters"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "CPU-gebaseerde videofilters worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Audiofilters"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Audio DSP filters worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "GPU-gebaseerde videoshaders worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Opnames"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Opnames worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Opnameconfiguratie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "De opnameconfiguraties worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Overlays worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Toetsenbord-overlays"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "Toetsenbord-overlays worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Videolay-outs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Video lay-outs worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Schermafbeeldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Schermafbeeldingen worden in deze map opgeslagen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Controllerprofielen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Controllerprofielen die worden gebruikt om controllers automatisch te configureren worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Invoer-remaps"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Invoer-remaps worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Afspeellijsten worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Afspeellijst van favoriete"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Sla de afspeellijst van favorieten op in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Geschiedenisafspeellijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Sla de geschiedenisafspeellijst op in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Afbeeldingenafspeellijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Sla de afbeeldingenafspeellijst op in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Muziekafspeellijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Sla de muziekafspeellijst op in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Video-afspeellijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Sla de video-afspeellijst op in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Runtimelogboeken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Runtimelogboeken worden opgeslagen in deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Opgeslagen bestanden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Sla alle opslagbestanden op in deze map. Als dit niet is ingesteld, zal het proberen op te slaan in de werkmap van het bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Sla alle opgeslagen bestanden (*.srm) op in deze map. Dit omvat gerelateerde bestanden zoals .rt, .psrm, enz... Dit zal worden overschreven door expliciete command line opties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Onderbrekingspunten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Onderbrekingspunten en herhalingen worden opgeslagen in deze map. Als dit niet is ingesteld, zullen ze proberen op te slaan in de map waar de inhoud zich bevindt."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Gearchiveerde inhoud wordt tijdelijk uitgepakt naar deze map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Systeemgebeurtenislogboeken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Systeemgebeurtenislogboeken worden opgeslagen in deze map."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Rich Presence inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Deel je huidige status in RetroArch op Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Rich Presence inhoud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Bepaal welke informatie met betrekking tot de inhoud zal worden gedeeld."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Inhoud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Core naam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Systeem naam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Inhoud (systeemnaam)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Inhoud (corenaam)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Inhoud (systeemnaam - corenaam)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Aan audio mixer toevoegen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Aan audio mixer toevoegen en afspelen"
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Verbinding maken met Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Enables netplay in client mode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Verbreek verbinding met Netplay Host"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Verbreek een actieve netplay verbinding."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Lobbyfilters"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Alleen verbindbare kamers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Alleen geïnstalleerde cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Kamers met wachtwoord"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Vernieuw netplay hostlijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Zoeken naar netplay hosts."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Vernieuw netplay LAN-lijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Netplay hosts zoeken op LAN."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Begin met hosten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Start netplay in host modus (server)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Stop met hosten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "Verwijder client"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Verwijder cleint klant van je momenteel gehoste kamer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "Blokkeer client"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Blokkeer een klant uit je momenteel gehoste kamer."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Scan map"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Scant een map voor inhoud die overeenkomt met de database."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Scan Deze Map>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "Selecteer dit om de huidige map voor inhoud te scannen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Scan Een Bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Scant een bestand voor inhoud die overeenkomt met de database."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Inhoudscan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Configureerbare scan gebaseerd op bestandnamen en/of database van inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD,
   "Scanmethode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_METHOD,
   "Automatisch of aangepast met gedetailleerde opties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB,
   "Databasecontrole"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_USE_DB,
   "\"streng\" Zal alleen items toevoegen die overeenkomen met een database-item, \"losjes\" voegt ook bestanden toe met de juiste extensie, maar geen CRC/serial overeenkomst, \"Aangepaste DAT\" controleert tegen een gebruiker verstrekt XML-bestand in plaats van databases, Geen van de databases wordt genegeerd en alleen bestandsextensies gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT,
   "Database voor overeenkomsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DB_SELECT,
   "Overeenkomsten zoeken kan worden beperkt tot een specifieke database, of tot de allereerste database die overeenkomt, om het scannen te versnellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_TARGET_PLAYLIST,
   "Afspeellijst om bij te werken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_TARGET_PLAYLIST,
   "Resultaten worden toegevoegd aan deze afspeellijst. In geval van Auto - Elke, kunnen meerdere afspeellijsten van systeem worden bijgewerkt. Aangepast zonder database verwijzing zal de items niet koppelen aan enige database in de afspeellijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SINGLE_FILE,
   "Scan één bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SINGLE_FILE,
   "Scant slechts één bestand in plaats van een map. Selecteer de locatie van de inhoud opnieuw na het wijzigen van deze invoer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_OMIT_DB_REF,
   "Databaseverwijzingen uit afspeellijst overslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_OMIT_DB_REF,
   "In het geval van aangepaste naam van de afspeellijst altijd de naam van de afspeellijst gebruiken voor zoekfunctie van miniatuur, zelfs als er een overeenkomst in een database was."
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Aan audio mixer toevoegen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Aan audio mixer toevoegen en afspelen"
   )

/* Import Content > Content Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Inhoudslocatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Selecteer een map (of bestand) om te scannen naar inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Doelafspeellijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Naam van het gegenereerde afspeellijstbestand, ook gebruikt voor de identificatie van miniaturen in afspeellijst. Automatische instelling gebruikt dezelfde naam als de overeenkomende database of inhoudsmap."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Naam voor de aangepaste afspeellijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Aangepaste afspeellijstnaam voor gescande inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Staandaardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Selecteer een standaard core om te gebruiken bij het starten van gescande inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Bestandsextensies"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Lijst van bestandstypes om op te nemen in de scan, gescheiden door spaties. Als dit leeg is, bevat dit alle bestandstypes, of als een core is opgegeven, alle bestanden ondersteund door de kern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Recursief scannen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Indien ingeschakeld, zullen alle submappen van de opgegeven \"inhoudsmap\" worden opgenomen in de scan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Scan in gecomprimeerde bestanden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Wanneer ingeschakeld, worden gecomprimeerde bestanden (.zip, .7z, enz.) doorzocht voor geldig/ondersteunde inhoud. Kan een aanzienlijke impact hebben op scanprestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Arcade DAT-bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Selecteer een Logiqx of MAME lijst XML DAT-bestand om automatische vermelding van gescand arcade inhoud mogelijk te maken (MAME, FinalBurn Neo, enz.)."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Wanneer je een arcade DAT-bestand gebruikt, wordt de inhoud alleen toegevoegd aan de afspeellijst als een overeenkomende DAT-bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Bestaande afspeellijst overschrijven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Wanneer ingeschakeld, zal elke bestaande afspeellijst worden verwijderd voordat er gescand wordt. Indien uitgeschakeld, worden bestaande afspeellijstitems behouden en wordt alleen de inhoud toegevoegd die momenteel ontbreekt in de afspeellijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Valideer bestaande items"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Wanneer ingeschakeld, worden de items in een bestaande afspeellijst geverifieerd voordat nieuwe inhoud wordt gescand. Invoer die verwijst naar ontbrekende inhoud en/of bestanden met ongeldige extensies zal worden verwijderd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Start scan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Scan geselecteerde inhoud."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Lijst opstarten..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Jaar van uitgave"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Aantal spelers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Regio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Zoek naam ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Toon alles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Aanvullende filter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Alle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Extra filter toevoegen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u items."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Per ontwikkelaar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Per uitgever"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Per jaar van uitgave"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Per aantal spelers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Per genre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Op achievements "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Per categorie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Per taal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Per regio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Per console-exclusief"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Per platform-exclusief"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Per trilfunctie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Per score"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Per media"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Per besturingselementen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Per kunststijl"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Per gameplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Per verhaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "Per pacing"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Per perspectief"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Per instelling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Per zicht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Per voertuig"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Per oorsprong"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Per franchise"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Per tag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Per systeemnaam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Bereikfilter instellen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Weergave"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Opslaan als weergave"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Deze weergave verwijderen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Naam van nieuwe weergave invoeren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Weergave met dezelfde naam bestaat al"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "Weergave is opgeslagen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "Weergave is verwijderd"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Starten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Start de inhoud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Rename the title of the entry."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Hernoem de titel van het item."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Verwijderen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Dit item uit de afspeellijst verwijderen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Toevoegen aan Favorieten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Voeg de inhoud toe aan \"Favorieten\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "Voeg toe aan afspeellijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "Voeg de inhoud toe aan een afspeellijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "Nieuwe afspeellijst maken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "Maak een nieuwe afspeellijst aan en voeg het huidige item eraan toe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Stel gekoppelde core in"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Stel de core in die gekoppeld is aan deze inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Reset gekoppelde core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Reset de core die gekoppeld is aan deze inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Bekijk meer informatie over de inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Download miniaturen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Download screenshot/doosafbeelding/titelscherm miniatuurafbeeldingen voor de huidige content. Bestaande miniaturen worden bijgewerkt."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Huidige core"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Naam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Bestandspad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Item: %lu/%lu"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Speeltijd"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Laatst Gespeeld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Inhoud Database"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Hervatten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Hervat de inhoud en verlaat het snelmenu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Activeer zachte reset. RetroPad Start activeert een harde reset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Sluit de inhoud. Alle niet-opgeslagen wijzigingen kunnen verloren gaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Neem Schermafdruk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Afbeelding van het scherm maken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Onderbrekingspuntslot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Verander het huidige geselecteerde onderbrekingspuntslot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Status Opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Sla een onderbrekingspunt op naar het huidig geselecteerde slot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVE_STATE,
   "Sla een onderbrekingspunt op naar het huidig geselecteerde slot. Let op: onderbrekingspunt zijn niet draagbaar en werken mogelijk niet met andere versies van deze core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Laad State"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Laad een onderbrekingspunt van huidig geselecteerde slot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_STATE,
   "Laad een onderbrekingspunt van huidig geselecteerde slot. Let op: als het onderbrekingspunt was opgeslagen met een andere versie van de core werkt dit mogelijk niet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Laden van State Ongedaan Maken"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Als een onderbrekingspunt was geladen, zal de inhoud teruggaan naar de status van voor het laden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Maak het onderbrekingspunt ongedaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Als een onderbrekingspunt overschreven was, zal het terugdraaien naar het vorige onderbrekingspunt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "Harhalingsslot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "Verander het huidige geselecteerde onderbrekingspuntslot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Speel Herhaling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Speel een replay bestand af van de huidige geselecteerde slot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Record Herhaling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Neem het harhalingsbestand op naar de geselecteerde slot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Stop opnemen/herhalen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "Stopt het opnemen/afspelen van de huidige herhaling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Toevoegen aan Favorieten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Voeg de inhoud toe aan \"Favorieten\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Start Opname"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Start video-opname"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Stop Opname"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Video-opname stoppen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Start Streamen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Begin met streamen naar de gekozen bestemming."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Stop Streamen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Beëindig stream."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Onderbrekingspunten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Ga naar de onderbrekingspuntopties"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Core-opties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Verander de opties voor de inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Besturingselementen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Wijzig de besturingselementen voor de inhoud."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Stel cheatcodes in."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Disk Beheer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Schijfkopiebeheer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Stel shaders in om de afbeelding visueel te verruimen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Opties om de globale configuratie te overschrijven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Achievements Lijst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Bekijk achievements en gerelateerde instellingen."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Beheer core-opties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Overrides opslaan of verwijderen voor de huidige inhoud."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Opslagbestandopties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Core-opties opslaan die alleen voor de huidige inhoud van toepassing zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Spelopties verwijderen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Verwijder core-opties die alleen voor de huidige inhoud van toepassing zijn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Inhoudsmapopties opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Sla core-opties op die van toepassing zijn op alle inhoud die geladen is vanuit dezelfde map als het huidige bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Verwijder inhoudmap-opties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Verwijder core-opties die van toepassing zijn op alle inhoud die geladen is vanuit dezelfde map als het huidige bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Actief optiesbestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "De huidige optiesbestand in gebruik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Reset Core-opties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Stel alle opties van de huidige core in op standaardwaarden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Opties op schijf opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Forceer de huidige instellingen om naar het actieve optiebestand te worden geschreven. Zorgt ervoor dat de opties worden bewaard in het geval dat een core-fout ervoor zorgt dat de front-end niet goed wordt afgesloten."
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Beheer remap-bestanden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Invoer remap-bestanden voor de huidige inhoud laden, opslaan of verwijderen."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Actief remap-bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "Het huidige remap-bestand in gebruik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Laad Remap Bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Laad en vervang huidige invoertoewijzingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Remap-bestand opslaan als"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Sla de huidige invoertoewijzingen op als een nieuw remap-bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Core Remap Bestand Opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Sla een remap-bestand op dat van toepassing is op alle inhoud geladen met deze core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Verwijder remap-bestand voor de core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Verwijder het remap-bestand dat van toepassing is op alle inhoud geladen met deze core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Sla remap-bestand voor de inhoudsmap op."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Sla een remap-bestand op die van toepassing is op alle uit dezelfde map geladen inhoud als het huidige bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Verwijder remap-bestand voor de inhoudsmap."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Verwijder het remap-bestand dat van toepassing zal zijn op alle uit dezelfde map geladen inhoud als het huidige bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Game Remap Bestand Opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "Sla een remap-bestand op dat alleen van toepassing is voor de huidige inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Verwijder remap-bestand voor het spel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "Verwijder het remap-bestand dat alleen van toepassing is voor de huidige inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Reset invoertoewijzingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Stel alle invoertoewijzingen in op standaard waarden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Update invoer-remap-bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Overschrijf het actieve remap-bestand met de huidige instellingen voor invoeraanpassingen."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Remap-bestand"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Start of ga verder met het zoeken naar cheats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT,
   "Scan geheugen om nieuwe cheats te maken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Laad cheat-bestand (vervangen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Laad een cheat bestand en vervang bestaande cheats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Laad cheat-bestand (toevoegen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Laad een cheat-bestand en voeg toe aan bestaande cheats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Herlaad spel specifieke cheats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS,
   "Herlaad alle bestaande cheats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Cheats Opslaan Als"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Huidige cheats opslaan als een cheat bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Voeg nieuwe cheat toe aan de bovenkant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP,
   "Voeg een cheat toe aan de bovenkant van de lijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Voeg nieuwe cheat toe aan de onderkant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM,
   "Voeg een cheat toe aan de onderkant van de lijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Verwijder alle Cheats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL,
   "Wis de cheatlijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Automatisch cheats toepassen tijdens het laden van een spel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Automatisch cheats toepassen wanneer het spel wordt geladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Toepassen Na Omschakeling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Pas cheat direct toe na het schakelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Cheat wijzigingen toepassen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Veranderingen aan cheat zullen onmiddellijk van kracht worden."
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Start of herstart met het zoeken naar cheats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Druk op links of rechts om van bit-grootte te veranderen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Druk op links of rechts om de waarde te veranderen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Gelijk aan %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Minder dan voorheen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Groter dan of gelijk aan voorheen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Groter dan voorheen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Groter dan of gelijk aan voorheen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Gelijk aan voorheen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Niet gelijk aan voorheen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Druk op links of rechts om de waarde te veranderen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Gelijk aan voorheen +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Doorzoek geheugen voor waarden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Druk op links of rechts om de waarde te veranderen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Gelijk aan voorheen +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "%u overeenkomsten toevoegen aan de lijst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Verwijder overeenkomst #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Maak code aan voor overeenkomst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Adres van overeenkomst: %08X masker: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Cheatbestand (vervangen)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Cheatbestand (toevoegen)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Cheatdetails"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Cheatpositie in lijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Ingeschakeld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Omschrijving"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Uitgevoerd door"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Geheugenzoekgrootte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Waarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Geheugenadres"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Doorzoek adres: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Geheugenadresmasker"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Adres bitmasker bij Geheugenzoekgrootte < 8 bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Aantal iteraties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Het aantal keren dat de cheat zal worden toegepast. Gebruik met de andere twee \"iteratie\" opties om grote gebieden van het geheugen te beïnvloeden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Adres elke iteratie verhogen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Na elke iteratie zal het \"Geheugenadres\" worden verhoogd met dit nummer vermenigvuldigd met \"Geheugenzoekgrootte\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Waarde elke iteratie verhogen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Na elke herhaling wordt de \"Waarde\" met dit aantal verhoogd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Trillen wanneer geheugen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Trilwaarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Trilpoort"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Trillen primaire kracht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Trillen primaire duur (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Trillen secundaire kracht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Trillen secundaire duur (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Voeg nieuwe cheat toe na dit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Voeg nieuwe cheat toe voor dit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Kopieer deze cheat erna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Kopieer deze cheat ervoor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Verwijder deze cheat"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Huidige schijfindex"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Kies huidige schijf uit lijst van beschikbare schijfkopieën. De virtuele schijfbalk kan gesloten blijven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Schijf Uitwerpen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Open het virtuele schijfvak."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Disk Image Toevoegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Sluit het virtuele schijfvak."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Laad nieuwe schijf"
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Video shader pipeline inschakelen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Houd shaderbestanden in de gaten voor wijzigingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Pas automatisch gemaakte wijzigingen van shaderbestanden op de schijf toe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Houd shader-bestanden in de gaten voor nieuwe wijzigingen. Na het opslaan van wijzigingen in een shader op de schijf, zal het automatisch opnieuw gecompileerd worden en toegepast op de inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Onthoud laatst gebruikte shadermap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Open bestandsbrowser in de laatst gebruikte map bij het laden van shader-voorinstellingen en passes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Laad preset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Laad een shader-preset. De shader pipeline zal automatisch worden geïnstalleerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Laad een preset van de shader direct. Het shader-menu wordt aangepast.\nDe schaalfactor die getoond wordt in het menu is alleen betrouwbaar als de voorinstelling gebruik maakt van eenvoudige schaalmethoden. (b.v/ bronschalen, dezelfde schaalfactor voor X/Y)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Preset vooraan toevoegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Voeg de preset vooraan toe aan de huidige geladen voorinstelling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Preset achteraan toevoegen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Voeg de preset achteraan toe aan de huidige geladen voorinstelling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_MANAGER,
   "Presets beheren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_MANAGER,
   "Shader preset opslaan of verwijderen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_FILE_INFO,
   "Actief preset-bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_FILE_INFO,
   "De huidige shader preset in gebruik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Shader Instellingen Toepassen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Wijzigingen in de shader configuratie worden onmiddellijk van kracht. Gebruik dit als je aanpassingen hebt gedaan aan de hoeveelheid shader passes, FBO schaal, enz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "Na het wijzigen van shader-instellingen zoals het aantal shaderpassen, filteren, FBO-schaal, gebruik dit om wijzigingen door te voeren.\nHet veranderen van deze shader-instellingen is een vrij dure operatie, dus het moet expliciet gebeuren.\nWanneer je shaders toepast, worden de shader-instellingen opgeslagen in een tijdelijk bestand (retroarch.slangp/.cgp/.glslp) en geladen. Het bestand blijft bestaan nadat RetroArch is afgesloten en is opgeslagen in de shader-map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Voorbeeldweergave Shader Parameters"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Pas de huidige shader direct aan. Wijzigingen worden niet opgeslagen in het preset-bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Shader passes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Verhoog of verlaag de hoeveelheid van shader pipeline passes. Verschillende shaders kunnen worden gebonden aan elke pipeline pass die wordt en hun de schaal en filtering configureren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch maakt het mogelijk verschillende shaders te mengen met willekeurige shader passes, met aangepaste hardware-filters en schaalfactoren.\nDeze optie specificeert het aantal te gebruiken shader passes. Als je dit op 0 hebt ingesteld en veranderingen aan de shader opslaat, gebruik je een \"blanko\" shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "Pad naar de shader. Alle shaders moeten van hetzelfde type zijn (vb. Cg, GLSL of Slang). Zet de shadermap om in te stellen waar de browser op zoek gaat naar shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "Hardwarefilters voor deze pass. Als \"Standaard\" is ingesteld, zal het filter \"Lineair\" of \"Nearest\" zijn, afhankelijk van \"Bilinear filter\" instellingen onder video-instellingen."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Schaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "Schaal voor deze pass. De schaalfactor accumuleert, d.w.z. 2x voor eerste pas en 2x voor tweede pas geeft je een totale schaal van 4x.\nAls er een schaalfactor is voor de laatste pass, wordt het resultaat uitgerekt naar het scherm met het standaardfilter, Afhankelijk van de Bilinaire filterinstelling onder Video-instellingen.\nAls \"Standaard\" is ingesteld, zal 1x schaal of het volledig scherm worden gebruikt, afhankelijk van of het niet de laatste pass is of niet."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Eenvoudige preset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Sla een shader preset op die een link heeft naar de oorspronkelijk geladen voorinstelling en gebruik alleen de door u aangebrachte parameterwijzigingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Huidige preset opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Sla de huidige shader preset op."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Preset opslaan als"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "De huidige shader-instellingen opslaan als nieuwe shader preset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Globale voorinstelling opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Sla de huidige shader-instellingen op als standaard globale instellingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Core preset opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Sla de huidige shader-instellingen op als de standaard voor deze core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Inhoudsmap-preset opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Sla de huidige shader-instellingen op als standaard voor alle bestanden in de huidige inhoudsmap."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Spel-preset opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Sla de huidige shader-instellingen op als standaardinstellingen voor de inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Geen automatische shader presets gevonden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Globale preset verwijderen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Verwijder de globale preset, gebruikt door alle inhoud en alle cores."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Verwijder core preset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Verwijder de core preset, gebruikt door alle inhoud uitgevoerd met de momenteel geladen kern."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Verwijder inhoudsmap-preset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Verwijder inhoudsmap-preset, die wordt gebruikt door alle inhoud in de huidige werkmap."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Verwijder spel-preset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Verwijder de spel-preset die alleen wordt gebruikt voor het specifieke spel in kwestie."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Geen shader parameters."
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "Actieve override-bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "Het huidige override-bestand in gebruik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "Laad override-bestand"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "Laad en vervang de huidige configuratie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "Overrides opslaan als"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "De huidige configuratie als een nieuw override-bestand opslaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Core-overrides opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Sla een override configuratiebestand op dat van toepassing is op alle inhoud die met deze core is geladen. Dit zal voorrang krijgen boven de hoofdconfiguratie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Verwijder core-overrides"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Verwijder het override-configuratiebestand dat van toepassing is op alle met deze core geladen inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Inhoudmap-overrides opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Sla een override-configuratiebestand op dat van toepassing is op alle uit dezelfde map geladen inhoud als het huidige bestand. Zal voorrang krijgen op de hoofdconfiguratie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Verwijder inhoudmap-overrides"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Verwijder het override-configuratiebestand dat van toepassing is op alle uit dezelfde map geladen inhoud als het huidige bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Spel-overrides opslaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Sla een override-configuratiebestand op dat alleen van toepassing is op de huidige inhoud. Zal voorrang krijgen boven de hoofdconfiguratie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Verwijder spel-overrides"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Verwijder het override-configuratiebestand dat alleen voor de huidige inhoud van toepassing is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "Ontlaad override"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "Reset alle opties naar de globale configuratiewaarden."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Geen achievements om weer te geven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Annuleren hardcore-modus voor achievements pauzeren."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Laat hardcore-modus voor achievements ingeschakeld voor de huidige sessie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Annuleer hardcore-modus voor achievements"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Hardcore-modus voor achievementsuitgeschakeld laten voor de huidige sessie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "hardcore-modus voor achievements kan niet worden hervat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "U moet de kern herladen om hardcore-modus voor achievements te hervatten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Pauzeer hardcore-modus voor achievements."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Pauzeer hardcore-modus voor achievements voor de huidige sessie. Deze actie zal cheats, terugspoelen, slow motion en onderbrekingspunten mogelijk maken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Hhardcore-modus voor achievements hervatten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Hervat hardcore-modus voor achievements voor de huidige sessie. Deze actie zal cheats, terugspoelen, slow motion en onderbrekingspunten uitschakelen en het huidige spel resetten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "RetroAchievements server is onbereikbaar"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Één of meer achievements hebben het niet de server niet bereikt. De ontgrendelingen zullen opnieuw worden geprobeerd zolang u de app open laat."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "RetroAchievements server is onbereikbaar. Zal opnieuw proberen totdat het lukt of de app is gesloten."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "Alle openstaande verzoeken zijn gesynchroniseerd met de RetroAchievements server."
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "Spel aan het identificeren"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "Speldata aan het opladen"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "Sessie aan het starten"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Niet ingelogd"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Netwerkfout"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Onbekend spel"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Achievements kunnen niet worden geactiveerd met deze Core"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Databaseitem"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Toon database-informatie voor huidige inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Geen items om weer te geven"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Geen cores beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Geen core opties beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Geen core informatie beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Geen core-back-ups beschikbaar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Geen favorieten beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Geen geschiedenis beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Geen afbeeldingen beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Geen muziek beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Geen videos beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Informatie is niet beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Geen afspeellijst items beschikbaar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Geen instellingen gevonden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Geen Bluetooth-apparaten gevonden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Geen netwerken gevonden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Geen Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Zoeken:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "Miniaturen wisselen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RANDOM_SELECT,
   "Willekeurige selectie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Terug"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Bovenliggende map"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "Ga terug naar de bovenliggende map."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Map niet gevonden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Geen items."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Selecteer bestand"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_NORMAL,
   "Normaal"
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
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_NORMAL,
   "Normaal"
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
   "Onbekende compiler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Delen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Uitsluiten (XOR)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Stemmen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Deel analoge invoer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Gemiddelde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Geen Voorkeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Stuiter links/rechts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Scroll links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Afbeeldingenmodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Spraakmodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Vertellermodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Geschiedenis & Favorieten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Alle afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Geschiedenis & Favorieten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Altijd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Nooit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Per core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Aggregaat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Opgeladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Opladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Ontladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "Geen bron"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Gebruik deze map>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "Selecteer dit om dit als map in te stellen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Inhoudsmap>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Standaard>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Niets>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad met analoog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Onbekend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Omlaag + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Houd Start vast (2 seconden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Houd Select vast (2 seconden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Omlaag + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Gedeactiveerd>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Veranderd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Niet veranderd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Vermeerderd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Vermindert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= Trilwaarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= Trilwaarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< Trilwaarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> Trilwaarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Vermeerderd met trilwaarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Vermindert met trilwaarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Alle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Gedeactiveerd>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Gezet naar waarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Vermeerderd met waarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Verminderd met waarde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Volgende cheat uitvoeren als waarde = geheugen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Volgende cheat uitvoeren als waarde != geheugen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Volgende cheat uitvoeren als waarde < geheugen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Volgende cheat uitvoeren als waarde > geheugen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Bit, maximale waarde = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-Bit, maximale waarde = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-Bit, maximale waarde = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-Bit, maximale waarde = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-Bit, maximale waarde = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-Bit, maximale waarde = 0xFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Systeemstandaard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Alfabetisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Toon volledige labels"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Verwijder () inhoud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Verwijder [] inhoud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Verwijder () en []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Behoud regio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Behoud schijfindex"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Behoud regio en schijfindex"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Systeemstandaard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Doosafbeelding"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Schermafbeeldingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Titelscherm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "Logo voor inhoud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "Normaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Snel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "AAN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Ja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Nee"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Waar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Niet waar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Ingeschakeld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Uitgeschakeld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "N.v.t"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Vergrendeld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Ontgrendeld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Onofficieel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Niet ondersteund"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Onlangs ontgrendeld"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Bijna klaar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Actieve uitdagingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Alleen trackers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Alleen meldingen"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Standaard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Lineair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Naaste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Hoofd"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Inhoud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Inhoudsmap>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_AUTO,
   "<Automatisch>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Aangepast>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Niet gespecifieerd>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD_AUTO,
   "Volledig automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_METHOD_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_STRICT,
   "Streng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_LOOSE,
   "Losjes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT,
   "Aangepaste DAT (streng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_CUSTOM_DAT_LOOSE,
   "Aangepaste DAT (losjes)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_USE_DB_NONE,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT_AUTO_ANY,
   "<Automatisch/Elke>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DB_SELECT_AUTO_FIRST,
   "<Automatisch/Eerste overeenkomst>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_TARGET_PLAYLIST_AUTO_ANY,
   "<Automatisch/Systeemnaam>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_TARGET_PLAYLIST_CUSTOM,
   "<Automatisch>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Linkse Analoge Stick"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Linker analoog (geforceerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Rechtse Analoge Stick"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Rechter analoog (geforceerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG,
   "Linker + rechter analogen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFTRIGHT_ANALOG_FORCED,
   "Linker + rechter analogen (geforceerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG,
   "Twin-stick analogen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWINSTICK_ANALOG_FORCED,
   "Twin-stick analogen (geforceerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "Sleutel %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Muis 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Muis 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Muis 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Muis 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Muis 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Muiswiel omhoog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Muiswiel omlaag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Muiswiel links"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Muiswiel rechts"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Vroeg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "Normaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Laat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "JJJJ-MM-DD UU:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "JJJJ-MM-DD UU:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "JJJJ-MM-DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "JJJJ-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "MM-DD-JJJJ UU:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "MM-DD-JJJJ UU:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "MM-DD UU:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "MM-DD-JJJJ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "DD-MM-JJJJ UU:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "DD-MM-JJJJ UU:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "DD-MM UU:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "DD-MM-JJJJ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS,
   "UU:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM,
   "UU:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "JJJJ-MM-DD UU:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "JJJJ-MM-DD UU:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "MM-DD-JJJJ UU:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "MM-DD-JJJJ UU:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "MM-DD UU:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "DD-MM-JJJJ UU:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "DD-MM-JJJJ UU:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "DD-MM UU:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM,
   "UU:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM,
   "UU:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "geleden"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Dikte achtergrondvulling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Verhoog de grootte van het schaakbordpatroon op de menuachtergrond."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Randvuller"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Dikte randvuller"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Verhoog de grootte van het schaakbordpatroon op de rand van het menu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Menu-rand weergeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Gebruik de lay-out over de volledige breedte."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Herschalen en positioneer menu-items om het beste gebruik te maken van beschikbare schermruimte. Schakel dit uit om de klassieke lay-out met twee kolommen en vaste breedte te gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Lineaire filter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Voegt een lichte vervaging toe aan het menu om scherpe pixelranden te verzachten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Intern opschalen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "De menu-interface wordt opgeschaald voordat deze op het scherm wordt weergegeven. In combinatie met 'Menu Linear Filter' verwijdert dit schaalartefacten (ongelijkmatige pixels) en zorgt het voor een scherp beeld. Dit heeft een aanzienlijke impact op de prestaties, die toeneemt naarmate het opschalingsniveau hoger is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Beeldverhouding"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Selecteer menu hoogte-breedte. Breedbeeldverhoudingen verhogen de horizontale resolutie van de menu-interface. (vereist mogelijk een herstart als \"Beeldverhouding vastzetten\" is uitgeschakeld)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Beeldverhouding vastzetten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Zorgt ervoor dat het menu altijd wordt weergegeven met de juiste beeldverhouding. Als dit is uitgeschakeld, wordt het snelle menu uitgerokken zodat het overeenkomt met de momenteel geladen inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Kleurthema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Kies een ander kleurenthema. Door 'Aangepast' wordt het menuthema-preset bestanden ingeschakeld."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Aangepaste thema-preset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Selecteer een menu thema voorinstelling in de bestandsbrowser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Doorzichtigheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Schakel de weergave van de achtergrondinhoud in terwijl het snelmenu actief is. Het uitschakelen van doorzichtigheid kan de themakleuren wijzigen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Schaduweffecten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Schakel schaduwen in voor menutekst, randen en miniaturen. Dit heeft een bescheiden impact op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Achtergrondanimatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Schakel achtergrond-deeltjes-animatie in. Heeft een aanzienlijke impact op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Achtergrondanimatiesnelheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Snelheid van de achtergrond deeltjes achtergrond-deeltjes-animatie aanpassen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Achtergrondanimatie voor de screensaver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Toon achtergrond-deeltjes-animatie wanneer de menu-screensaver actief is."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Toon miniaturen in afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Schakel de weergave van verkleinde miniaturen in tijdens het bekijken van afspeellijsten. Inschakelbaar met RetroPad Select. Wanneer deze functie is uitgeschakeld, kunnen miniaturen nog steeds op volledig scherm worden weergegeven met RetroPad Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Miniatuur bovenaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Type miniatuur om weer te geven in de rechterbovenhoek van afspeellijsten. Kan worden gewisseld met de rechter analoge stick omhoog/links."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Miniatuur onderaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Type miniatuur om weer te geven in de rechteronderhoek van afspeellijsten. Kan worden gewisseld met de rechter analoge stick omhoog/links."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Verwissel miniaturen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Wissel de weergaveposities van de \"Miniatuur bovenaan\" en \"Miniatuur onderaan\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Miniatuurverkleiningsmethode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Resampling-methode die wordt gebruikt om grote miniaturen te verkleinen zodat ze op het scherm passen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Miniatuurvertraging (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Hiermee wordt een vertraging ingesteld tussen het selecteren van een item in de afspeellijst en het laden van de bijbehorende miniaturen. Door deze waarde in te stellen op minimaal 256 ms, kunt u zelfs op de traagste apparaten snel en zonder vertraging scrollen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Uitgebreide ASCII-ondersteuning"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Schakel de weergave van niet-standaard ASCII-tekens in. Vereist voor compatibiliteit met bepaalde niet-Engelstalige westerse talen. Heeft een matige impact op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Schakelsymbool"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Gebruik pictogrammen in plaats van AAN/UIT-tekst om \"Schakel\" menu-instellingen weer te geven."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Dichtstbijzijnde buur (snel)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "Bilineair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (traag)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (gecentreerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (gecentreerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (gecentreerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (gecentreerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (gecentreerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Schermvullend"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Schalen in gehele getallen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Schermvullend (uitgerekt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Klassieke rood"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Klassieke Oranje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Klassieke Geel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Klassieke groen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Klassieke blauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Klassieke violet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Klassieke grijs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Oude rood"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Donker Paars"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Middernacht Blauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Goud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektrisch Blauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Appel Groen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vulkanisch Rood"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "Suikerspin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI,
   "Platte UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Gruvbox licht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "De kernel hacken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized Licht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango licht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dynamisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Donkergrijs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Lichtgrijs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Sneeuw (licht)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Sneeuw (zwaar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Regen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Sterrenveld"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Secundaire miniatuur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Type miniatuur om weer te geven aan de linkerkant."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "Pictogramminiatuur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "Type afspeellijstpictogram om weer te geven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Dynamische Wallpaper"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Laad dynamisch een nieuwe achtergrond afhankelijk van de context"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Horizontale animatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Schakel horizontale animatie in voor het menu. Dit zal een negatieve impact hebben op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Animatie horizontale pictogramaccentuering"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "De animatie die speelt bij het scrollen tussen tabbladen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animatie omhoog/omlaag"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "De animatie die speelt bij omhoog of omlaag bewegen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animatie hoofdmenu openen/sluiten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "De animatie die activeert bij het openen van een submenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Alfa-factor voor het kleurthema "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON,
   "Huidig menu-pictogram"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_CURRENT_MENU_ICON,
   "Huidige menu-pictogram kan worden verborgen, onder het horizontale menu of in de titel van de koptekst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NONE,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NORMAL,
   "Normaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_TITLE,
   "Titel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Lettertype"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Selecteer een ander hoofdlettertype dat gebruikt wordt door het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Letterkleur (rood)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Letterkleur (groen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Letterkleur (blauw)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Lay-out"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Selecteer een andere lay-out voor de XMB interface."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Pictogramthema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Selecteer een ander pictogramthema voor RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Schakelsymbool"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "Gebruik pictogrammen in plaats van AAN/UIT-tekst om \"Schakel\" menu-instellingen weer te geven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Schaduweffecten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Teken schaduwen voor pictogrammen, miniaturen en letters. Dit heeft een kleine invloed op de prestaties."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Selecteer een geanimeerd achtergrondeffect. Kan GPU-intensief zijn afhankelijk van het effect. Als de prestaties onvoldoende zijn, schakelt je dit uit of keert je terug naar een eenvoudiger effect."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Kleurthema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Selecteer een ander achtergrondkleurenthema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Miniatuur verticale positionering"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Toon de linkerminiatuur onder de rechtse, aan de rechterkant van het scherm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Miniatuur-schaalfactor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Verklein de miniatuurweergave door de maximaal toegestane breedte te schalen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Verticale vervagingsfactor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_HORIZONTAL_LIST,
   "Toon horizontale lijst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Toon titelheader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Titelmarge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Horizontale verschuiving van de titelmarge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Instellingentabblad inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Toon het instellingentabblad met programma-instellingen."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Lint"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Lint (vereenvoudigd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Simpel Sneeuw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Sneeuw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Sneeuwvlok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Aangepast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Monochroom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Omgekeerd monochroom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Systematisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
   "Retrosysteem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Omgekeerd automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Appel Groen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Licht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Ochtendblauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Zonnestraal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Donker Paars"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektrisch Blauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Goud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Oude rood"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Middernacht Blauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Achtergrondafbeelding"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Onderzee"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vulkanisch Rood"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Limoengroen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Pikachugeel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Kubuspaars"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Familierood"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Gloeiheet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "IJskoud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Grijs donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Grijs Licht"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT,
   "Lettertype"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT,
   "Selecteer een ander hoofdlettertype dat gebruikt wordt door het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE,
   "Lettergrootteschaal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE,
   "Bepaal of de lettergrootte in het menu een eigen schaalwaarde moet hebben, en of het globaal geschaald moet worden of met aparte waarden voor elk deel van het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_GLOBAL,
   "Globaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_SEPARATE,
   "Aparte waarden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Lettergrootteschaalfactor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Pas de lettergrootte lineair aan over het hele menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Lettergrootteschaalfactor voor titels"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Schaal de lettergrootte voor de titeltekst in de menuheader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Lettergrooteschaalfactor voor de zijbalk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Schaal de lettergrootte voor de titeltekst in de zijbalk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Lettergrooteschaalfactor voor labels"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Schaal de lettergrootte voor de labels van menu-opties en afspeellijstitems. Dit heeft ook invloed op de tekstgrootte in de helpvensters."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Lettergrooteschaalfactor voor sublabels"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Schaal de lettergrootte voor de sublabels van menu-opties en afspeellijstitems."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TIME,
   "Lettergrooteschaalfactor voor datums en tijden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TIME,
   "Schaal de lettergrootte voor de tijd en datum in de rechterbovenhoek van het  menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Lettergrooteschaalfactor voor voettekst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Schaal de lettergrootte van de tekst in de voettekst van het menu. Ook van invloed op de tekstgrootte in de rechter miniatuurzijbalk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Vouw de zijbalk in"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Zorg ervoor dat de linkerzijbalk altijd is ingeklapt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SHOW_SIDEBAR,
   "Toon de zijbalk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SHOW_SIDEBAR,
   "Sta navigatie en afspeellijsten in de linkerzijbalk toe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Afspeellijstnamen afkappen (opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Verwijder de namen van de fabrikanten uit de afspeellijsten. Bijvoorbeeld: \"Sony - PlayStation\" wordt \"PlayStation\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Afspeellijsten sorteren na afkapping van de naam (opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Afspeellijsten worden opnieuw gesorteerd in alfabetische volgorde na het verwijderen van de fabriekscomponent van hun namen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Secundaire miniatuur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Vervang de inhoudmetadata paneel met een andere miniatuur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Gebruik scrollende tekst voor metadata van inhoud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Wanneer ingeschakeld, wordt elk item van metadata weergegeven op de rechter zijbalk van afspeellijsten (gekoppelde core, afspeeltijd) in een enkele regel; tekst die de breedte van de zijbalk overschrijd, zal worden weergegeven als scrollende tekst. Indien uitgeschakeld wordt elk item van inhoudsmetadata statisch weergegeven, in zoveel regels als nodig."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Miniatuur-schaalfactor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Schaal het formaat van de miniatuurbalk."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_PADDING_FACTOR,
   "Upvullingsfactor"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_PADDING_FACTOR,
   "Schaal de horizontale opvulgrootte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON,
   "Hoofdpictogram"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_ICON,
   "Het headerlogo kan verborgen worden, dynamisch worden weergegeven afhankelijk van de navigatie, of vastgezet naar klassieke invader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR,
   "Scheidingsteken voor koptekst"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_SEPARATOR,
   "Alternatieve breedte voor kop- en voettekstscheidingstekens."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_NONE,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_DYNAMIC,
   "Dynamisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_FIXED,
   "Vast"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NONE,
   "Geen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NORMAL,
   "Normaal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_MAXIMUM,
   "Maximaal"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Kleurthema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Selecteer een ander kleurenschema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Wit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Zwart"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY,
   "Boysenbes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "De kernel hacken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solarized donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized Licht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Grijs donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Grijs licht"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Pictogrammen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Pictogrammen aan de linkerkant van de menu-items weergeven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Schakelsymbool"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "Gebruik pictogrammen in plaats van AAN/UIT-tekst om \"Schakel\" menu-instellingen weer te geven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Afspeellijstpictogrammen (opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Toon systeemspecifieke pictogrammen in de afspeellijsten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Optimaliseer Landschaps-lay-out"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "De menu-indeling wordt automatisch aangepast aan het scherm bij gebruik van een liggende schermoriëntatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Navigatiebalk weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Toon permanent menunavigatiesnelkoppelingen. Maakt snel schakelen tussen menucategorieën mogelijk. Aanbevolen voor touchscreen apparaten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Automatisch navigatiebalk draaien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Automatisch de navigatiebalk verplaatsen naar de rechterkant van het scherm wanneer liggende schermoriëntaties worden gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Kleurthema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Selecteer een ander achtergrondkleurenthema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Overganganimatie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Schakel vloeiende animatieeffecten in bij navigeren tussen verschillende niveaus van het menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Miniatuurweergave portret"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Specificeer afspeellijst miniatuurafbeeldingsmodus bij gebruik van verticale schermoriëntaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Miniatuurweergave landschap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Geef afspeellijst miniatuurafbeelding weer bij liggende schermoriëntaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Toon secundaire miniatuur in lijstweergave"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Geeft een secondaire miniatuur weer bij gebruik van de miniatuurweergavemodus \"Lijst\" voor afspeellijsten. Deze instelling is alleen van toepassing als het scherm voldoende fysieke breedte heeft om twee miniaturen weer te geven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Miniatuurachtergronden"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Maakt het opvullen van ongebruikte ruimte in miniatuurafbeeldingen met een vaste achtergrond mogelijk. Dit zorgt voor een uniforme weergavegrootte voor alle afbeeldingen, verbetert het weergave van het menu bij het bekijken van gemengde inhoudminiaturen met verschillende basisafmetingen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Primaire miniatuur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Hoofdtype van miniatuur om te koppelen aan el afspeellijstitem. Meestal dient dit als inhoudspictogram."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Secundaire miniatuur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Hulptype miniatuurafbeelding dat aan elk item in de afspeellijst wordt gekoppeld. Het gebruik ervan is afhankelijk van de huidige weergavemodus voor miniatuurafbeeldingen in de afspeellijst."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Blauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Blauw Grijs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Donker Blauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Groen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Rood"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Geel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Material UI donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozone donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Cutie blauw"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Cutie cyaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Cutie groen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Cutie oranje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Cutie roze"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Cutie paars"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Cutie rood"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "De kernel hacken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Grijs donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Grijs licht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "Automatisch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Vervagen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Schuiven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Lijst (Klein)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Lijst (Medium)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Duaal pictogram"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Lijst (Klein)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Lijst (Medium)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Lijst (Groot)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "UIT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "AAN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Sluit miniatuurweergaven uit"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Bestand"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Core Laden..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Ontlaad core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "&Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Bewerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Zoeken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Weergave"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Gesloten dokken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Voorbeeldweergave Shader Parameters"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Instellingen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Onthoud dokposities"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Onthoud venstergeometrie:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Onthoud laatste inhoudsbrowser tabblad:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Thema:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Systemmstandaard>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Donker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Aangepast..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Instellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Gereedschap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&hulp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "Over RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Documentatie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Aangepaste core laden..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Core Laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Core aan het laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Naam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Versie "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Bestandbeheer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Bovenaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Omhoog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Inhoudsbrowser"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Doosafbeelding"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Schermafbeeldingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Titelscherm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Alle afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Vraag me>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informatie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Waarschuwing"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Fout"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Netwerkfout"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Herstart het programma om de wijzigingen van kracht te laten worden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Zet de afbeelding hier neer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Laat dit niet meer zien"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Stoppen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Gekoppelde core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Verborgen afspeellijsten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Verbergen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Accentkleur:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Kiezen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Selecteer kleur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Selecteer thema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Aangepast thema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Bestandspad is leeg."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Bestand is leeg."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Kon het bestand niet openen om te lezen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Kan bestand niet openen om te schrijven."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Bestand bestaat niet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Stel de geladen core eerst voor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Zoomen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Bekijken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Pictogrammen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Lijst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Legen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Voortgang:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "\"Alle afspeellijsten\" maximaal aantal items:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "\"Alle afspeellijsten\" maximaal aantal rasteritems:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Toon verborgen bestanden en mappen:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Nieuwe afspeellijst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Voer een naam in vooer de nieuwe afspeellijst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Afspeellijst verwijderen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Hernoem afspeellijst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Weet je zeker dat je de afspeellijst \"%1\" wilt verwijderen?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Vraag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Kon het bestand niet verwijderen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Kon het bestand niet hernoemen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Lijst van bestanden verzamelen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Bestanden aan het toevoegen aan afspeellijst..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Afspeellijstitem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Naam:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Pad‎:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Inhoud Database:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Extensies:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(gescheiden door spaties; standaard worden alle items opgenomen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Filter in archieven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(gebruikt om miniaturen te vinden)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Weet u zeker dat u het item \"%1\" wilt verwijderen?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Kies eerst een enkele afspeellijst a.u.b.."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Verwijderen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Item toevoegen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Bestand(en) toevoegen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Map Toevoegen..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Bewerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Selecteer bestanden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Selecteer map"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<Meerdere>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Fout bij bijwerken afspeellijstitem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Vul a.u.b. alle vereiste velden in."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "RetroArch bijwerken (nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch is succesvol bijgewerkt. Herstart de applicatie om de wijzigingen door te voeren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Update mislukt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "bijdragers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Huidige shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Omlaag verplaatsen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Omhoog verplaatsen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Laden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Opslaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Verwijderen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Passen verwijderen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Toepassen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Voeg pass toe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Alle passes Wissen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Geen shader passes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Reset pass"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Reset alle passes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Reset parameter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Download miniatuur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Er is al een download bezig."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Start op afspeellijst:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Miniatuur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Miniatuurcachelimiet:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Maximale grootte van de miniatuur bij slepen en neerzetten:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Download alle miniaturen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Hele systeem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Deze afspeellijst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Miniaturen succesvol gedownload."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Geslaagd: %1 Mislukt: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Core-opties"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Reset alles"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Core-updaterinstellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Accounts cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Accounts Lijst Eindpunt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Core Prestatie tellers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Front-end Prestatie Tellers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Horizontale Menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Verberg Ongebonden Core-invoeromschrijvers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Invoerbeschrijving Labels Weergeven"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Op-Scherm Overlay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Geschiedenis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Selecteer inhoud uit de geschiedenislijst."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Wanneer inhoud wordt geladen, worden combinaties van content en libretro core opgeslagen in de geschiedenis.\nDe geschiedenis wordt opgeslagen in een bestand in dezelfde map als het RetroArch-configuratiebestand. Als er bij het opstarten geen configuratiebestand is geladen, wordt de geschiedenis niet opgeslagen of geladen en is deze niet zichtbaar in het hoofdmenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Subsystemen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Ga naar de subsysteeminstellingen voor de huidige inhoud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Huidige inhoud: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Geen netplay hosts gevonden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Geen netplay clients gevonden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Geen prestatie tellers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Geen afspeellijsten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Verbonden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "Poort"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Apparaatnaam poort %d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Apparaatweergavenaam: %s\nAparaatconfiguratienaam: %s\nApparaat VID/PID: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Cheatinstellingen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Start of ga verder met het zoeken naar cheats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Speel in mediaspeler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "secondes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Start core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Begin core zonder inhoud."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Aanbevolen cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Fout opgetreden tijdens lezen van gecomprimeerd bestand."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Gebruiker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "Toetsenbord"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Max Swapchain-afbeeldingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Vertelt de video driver om expliciet een opgegeven bufferingmodus te gebruiken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Maximale hoeveelheid swapchain afbeeldingen. Dit kan de video driver vertellen om een specifieke videobufferingmodus te gebruiken.\nEnkele buffering - 1\nDubbele buffering - 2\nDriedubbele buffering - 3\nHet instellen van de juiste buffering modus kan een grote impact hebben op latentie."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Hard-synchronisatie van de CPU en GPU. Vermindert latentie ten koste van prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Maximale framelatentie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Vertelt de video driver om expliciet een opgegeven bufferingmodus te gebruiken."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Wijzigt de shader preset zelf die momenteel in het menu worden gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Shader preset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Shader preset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "Shader preset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Blader URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "URL-pad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Bijnaam: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Zoeken naar compatibele inhoud..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "Geen core gevonden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "Geen afspeellijsten gevonden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Compatibele inhoud gevonden"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "Kan overeenstemmende inhoud niet vinden met CRC of bestandsnaam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Systeem BGM Activeren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Hulp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "Legen"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "In het menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "In het spel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "In-Game (Gepauzeerd)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Afspelen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Gepauzeerd"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netplay zal starten wanneer inhoud is geladen."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "Inhoud moet worden geladen alvorens netplay te starten."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Kon geen geschikt core- of inhoudsbestand vinden, laad handmatig."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Uw grafische stuurprogramma is niet compatibel met de huidige video driver in RetroArch, die terugvalt op de %s driver. Herstart RetroArch om de wijzigingen door te voeren."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Core-installatie succesvol"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Core-installatie mislukt"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Druk vijf keer op rechts om alle cheats te verwijderen."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Globale volume voor audio mixer"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Netplay scan voltooid."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Sorry, niet geïmplementeerd: cores die geen inhoud vragen kunnen niet deelnemen aan het netplay."
   )
MSG_HASH(
   MSG_NATIVE,
   "Standaard"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Onbekend netplay commando ontvangen"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Bestand bestaat al. Saven naar backup buffer"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "Verbonden met: \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Verbonden met: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Netplay poorttoewijzing succesvol"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "Extern netwerk heeft een privé of gedeeld adres. Overweeg om een relay-server te gebruiken."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Netplay UPnP poorttoewijzing gefaald"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "Geen argumenten opgegeven en geen menu ingebouwd, er wordt hulp weergegeven..."
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Aan het wachten op client..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "Je kamer kan niet met het internet verbonden worden."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Je hebt het spel verlaten"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "Je bent aangemeld als speler %u"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "Je bent aangesloten met invoerapparaten %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "Speler %.*s heeft het spel verlaten"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s heeft zich aangemeld als speler %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s heeft zich aangesloten met invoerapparaten %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYERS_INFO,
   "%d speler(s)"
   )
MSG_HASH(
   MSG_NETPLAY_SPECTATORS_INFO,
   "%d speler(s) (%d toeschouwer(s))"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Een netplay verbinding poging mislukt omdat de peer geen RetroArch draait, of een oude versie van RetroArch."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Een netplay peer draait een oude versie van RetroArch. Kan geen verbinding maken."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "WAARSCHUWING: Een netplay peer gebruikt een andere versie van RetroArch. Als zich problemen voordoen, gebruik dan dezelfde versie."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Een netplay peer is een andere core aan het gebruiken. Kan geen verbinding maken."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "WAARSCHUWING: Een netplay peer  een andere versie van de core aan het gebruiken. Als zich problemen voordoen, gebruik dan dezelfde versie."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Deze core ondersteunt geen netplay tussen deze platforms"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Deze core ondersteunt geen netplay tussen verschillende platforms"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Voer netplay serverwachtwoord in:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Netplay chatbericht invoeren:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Wilt u de verbinding toestaan van de gebruiker:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Onjuist wachtwoord"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" heeft de verbinding verbroken"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Een netplay client heeft de verbinding verbroken"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Netplay verbinding verbroken"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "Je hebt geen toestemming om te spelen"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "Er zijn geen vrije spelerslots"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "De gevraagde invoerapparaten zijn niet beschikbaar"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Kan niet overschakelen naar speelmodus"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Netplay peer \"%s\" onderbroken"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Je bijnaam is veranderd naar \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "Client verwijderd: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "Verwijderen van client \"%s\" mislukt"
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "Klant verbannen: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "Verbannen van client \"%s\" mislukt"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Afspelen"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "Aan het toeschouwen"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "Apparaten"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "Chat ondersteund"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "Slowdowns veroorzaakt"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Geluidsvolume"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Autodetecteren"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Mogelijkheden"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Verbinden met netplay host"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Verbinding maken met port"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Connectie slot"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "Ophalen van de core lijst..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Ophalen core lijst mislukt!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "De laatste versie is al geïnstalleerd: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Core aan het bijwerken: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Core aan het downloaden: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Uitpakken van core: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Core geïnstalleerd: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Core installeren mislukt: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Cores scannen..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Controleren van core: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Alle geïnstalleerde cores zijn de laatste versie"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Alle ondersteunde cores zijn omgeschakeld naar de Play Store-versies"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "Cores bijgewerkt: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "Core overgeslagen: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Core-update uitgeschakeld - core is vergrendeld: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Core aan het resetten:"
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Core gereset: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Afspeellijst opruimen: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Afspeellijst opgeruimd: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Vernieuwen mislukt - afspeellijst bevat geen geldige scan record: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Vernieuwen mislukt - ongeldig/ontbrekende inhoudsmap: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Vernieuwen mislukt - ongeldig/ontbrekende systeemnaam: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Vernieuwen mislukt - ongeldige core: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Vernieuwen mislukt - ongeldig/ontbrekend arcade-DAT-bestand: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Vernieuwen mislukt - arcade-DAT-bestand te groot (onvoldoende geheugen): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Aan favorieten toegevoegd"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Kon favoriet niet toevoegen: afspeellijst vol"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "Toegevoegd aan afspeellijst"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Toevoegen aan afspeellijst mislukt: afspeellijst vol"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Gekoppelde core: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "De core-koppeling voor dit afspeellijstitem is gereset."
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Programmamap"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Wijzigingen van cheat aan het toepassen."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Patch aan het toepassen: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Shader aan het toepassen"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Audio gedempt."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Audio ingeschakeld"
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Fout bij opslaan controllerprofiel."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "Controllerprofiel opgeslagen als\"%s\"."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Kan automatisch opslaan niet initialiseren."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Onderbrekingspunt automatisch opslaan naar"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Opdrachtinterface naar opstarten op poort"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Het nieuwe configuratiepad kan niet worden afgeleid. Gebruik de huidige tijd."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "Aan het vergelijken met bekende magic numbers"
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Gecompileerd met API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Configuratiemap niet ingesteld. Kan nieuwe configuratie niet opslaan."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Verbonden met"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "Inhoud CRC32's verschillen. Kan geen verschillende spellen gebruiken."
   )
MSG_HASH(
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "Host draait een ander spel."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "Je ping is te hoog voor deze host."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Inhoud laden overgeslagen. Implementatie zal het zelf laden."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Core heeft geen onderbrekingspunt-ondersteuning."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATE_UNDO,
   "Kern ondersteunt onderbrekingspunt ongedaan maken niet."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
   "Kern ondersteunt geen schijfbeheer."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Core-optiesbestand succesvol gemaakt."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "Core-optiesbestand succesvol verwijderd."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Alle core-opties herstellen naar standaardinstellingen."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "Core-opties opgeslagen naar:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "Niet gelukt om Core-opties op te slaan naar:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Kon geen volgende driver vinden"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Kon geen compatibele vinden."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Kon geen geldige datatrack vinden"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "Kon geen open datatrack vinden"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "Kon het inhoudsbestand niet lezen"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Kon de filmheader niet lezen"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Kon de staat van de film niet lezen."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "CRC32 checksum mismatch between content file and saved content checksum in replay file header) replay highly likely to desync on playback."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Aangepaste timing gegeven"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Decompressie is al aan de gang."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Decompressie mislukt."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Gedetecteerd weergave van"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Kon geen geldige inhoudspatch vinden."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Apparaat ontkoppeld van een geldige poort."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Bezig met downloaden"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Download mislukt"
   )
MSG_HASH(
   MSG_ERROR,
   "Fout"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Libretro core heeft inhoud nodig, maar dat werd niet gegeven."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Libretro core heeft speciaal inhoud nodig, maar dat werd niet gegeven."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Core ondersteunt geen VFS, en het laden van een lokale kopie is mislukt"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Fout opgetreden tijdens het verwerken van de argumenten."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Fout opgetreden tijdens het opslaan van core opties bestand."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "Fout bij verwijderen core-optiesbestand."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Fout is opgetreden tijdens het opslaan van remap bestand."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Fout bij verwijderen van het remap-bestand."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Fout is opgetreden tijdens het opslaan van shader preset."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Externe Applicatie Dir"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Uitpakken"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Uitpakken van bestand"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Fout is opgetreden tijdens het opslaan van configuratie naar"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Inkomende toeschouwer accepteren mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Geheugen toewijzen voor gepatchte inhoud mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Toepassen van shader mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Niet gelukt om shader preset toe te passen:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Socket binden mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Het aanmaken van de map is mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Het is niet gelukt om de inhoud uit het gecomprimeerde bestand te extraheren."
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Het ophalen van de nickname van de client is mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Laden is mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Inhoud laden mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_FROM_PLAYLIST,
   "Laden vanaf afspeellijst mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Laden van filmbestand mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Overlay laden mislukt."
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "Toetsenbord overlay is niet ingesteld."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Onderbrekingspunt laden mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Libretro core openen mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Patch mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Heads van client ontvangen mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Bijnaam ontvangen mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Bijnaam van host ophalen mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Bijnaamgrootte van host ophalen mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "SRAM data ophalen van de host mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Tijdelijk bestand verwijderen mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "SRAM opslaan mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "SRAM laden mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Mislukt om onderbrekingspunt op te slaan naar"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Bijnaam verzenden mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Bijnaamgrootte verzenden mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Bijnaam naar de client verzenden mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Bijnaam naar de host verzenden mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "SRAM data naar de client verzenden mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Starten van audiostuurprogramma mislukt. Zal verder gaan zonder audio."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Starten van filmrecord mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Kon de opname niet starten."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Schermafbeeldingen maken mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Onderbrekingpunt laden mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Onderbrekingpunt ongedaan maken mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Fout bij inschakelen van audio."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Fatale fout ontvangen in"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Bestand niet gevonden"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Automatisch onderbrekingspunt gevonden in"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Schijflabel gevonden"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Eerste data track gevonden in bestand"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Laatste opslagslot gevonden"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "Laatste herhalingsslot gevonden"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "Niet van huidige opname"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "Niet compatibel met herhaling"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_FUTURE_STATE,
   "Kan geen toekomstige staat laden tijdens het afspelen"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_WRONG_TIMELINE,
   "Verkeerde tijdlijn fout tijdens het afspelen"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_OVERWRITING_REPLAY,
   "Verkeerde tijdlijn; opname overschrijven "
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT,
   "Terugzoeken"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT_FAILED,
   "Terugzoeken is mislukt"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT,
   "Voorwaarts zoeken"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT_FAILED,
   "Voorwaarts zoeken is mislukt"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME,
   "Zoeken voltooid"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME_FAILED,
   "Zoeken mislukt"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Shader gevonden"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Game-specifieke core-opties gevonden in"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Map-specifieke core-opties gevonden in"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Ongeldige disc index."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Haal muisstatus"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Spelfocus aan"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Spelfocus uit"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Libretro core is hardware-geaccelereerd. Must use post-shaded recording as well."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "De controlesom komt niet overeen met de CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Cheat invoeren"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Cheatbestandsnaam invoeren"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Voer een naam in voor het preset-bestand"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "Overrides voor bestandsnaam"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "Remap-bestandsnaam invoeren"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Remap-titel"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Interne Opslag"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Verwijderbare opslag"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Ongeldige bijnaamgrootte"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "is gecompileerd tegen een andere versie van libretro dan deze libretro implementatie."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Front-end voor libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Onderbrekingspunt geladen van slot: %d"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Onderbrekingspunt geladen van slot: Automatisch"
   )
MSG_HASH(
   MSG_LOADING,
   "Laden"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Firmware bestanden ontbreken"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Inhoudsbestand aan het laden"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Geschiedenisbestand aan het laden"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Favorietenbestand laden"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Onderbrekingspunt aan het laden"
   )
MSG_HASH(
   MSG_MEMORY,
   "Geheugen"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "Ingevoerde filmherhaling is geen geldig REPLAY-bestand"
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
   MSG_MOVIE_RECORD_STOPPED,
   "Filmopname aan het stoppen."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Netplay initialiseren mislukt"
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "Kern ondersteunt geen netplay."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Geen inhoud, dummy core wordt gestart"
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Er is nog geen onderbrekingspunt overschreven."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Er is nog geen onderbrekingspunt geladen"
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Fout bij opslaan overrides."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "Fout bij verwijderen overrides."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Overrides succesvol opgeslagen."
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "Overrides succesvol verwijderd."
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "Overrides succesvol ontladen."
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "Niets om op te slaan. Geen overrides opgeslagen."
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "Niet aan het opslaan. Overrides actief."
   )
MSG_HASH(
   MSG_PAUSED,
   "Gepauzeerd."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Eerste datatrack aan het lezen..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Opname beëindigd als gevolg van gewijzigd grootte."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Opnemen naar"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Het cheatbestand wordt omgeleid naar"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Het cheatbestand is omgeleid naar"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Het onderbrekingspunt wordt omgeleid naar"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Bitmap-bestand succesvol opgeslagen."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Bitmap-bestand succesvol verwijderd."
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "Alle remap-opties gereset naar de standaardwaarden"
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Tijdelijk inhoudsbestand aan het verwijderen"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Opname wordt opnieuw gestart door een stuurprogramma herinitialisatie."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Oud onderbrekingspunt geladen."
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Map voor opslagbestanden aan het terugzetten naar"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Onderbrekingspunt map aan het terugzetten naar"
   )
MSG_HASH(
   MSG_REWINDING,
   "Terugspoelen."
   )
MSG_HASH(
   MSG_REWIND_BUFFER_CAPACITY_INSUFFICIENT,
   "Buffercapaciteit onvoldoende."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "Terugspoelen is niet beschikbaar omdat deze core geen geserialiseerde onderbrekingspunt heeft."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Terugspoelbuffer aan het initialiseren met grootte"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Kan de terugspoelbuffer niet initialiseren. Terugspoelen zal uitgeschakeld worden."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "Implementatie gebruikt threaded audio. Kan de terugspoelfunctie niet gebruiken."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Einde bereikt van terugspoel buffer."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Configuratie opgeslagen in"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Onderbrekingspunt opgeslagen in slot: %d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Onderbrekingspunt opgeslagen in slot: Auto."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Met succes opgeslagen in"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "RAM-type aan het opslaan"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Onderbrekingspunt aan het opslaan"
   )
MSG_HASH(
   MSG_SCANNING,
   "Aan het scannen"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Scannen van map voltooid."
   )
MSG_HASH(
   MSG_SCANNING_NO_DATABASE,
   "Scannen mislukt, geen database gevonden."
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Commando aan het verzenden"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Verscheidene patches zijn expliciet gedefinieerd, alles wordt genegeerd..."
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Shader preset succesvol opgeslagen."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Slow motion."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Vooruitspoelen."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Terugspoelen in slow motion"
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "SRAM laden wordt overgeslagen."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM zal niet opgeslagen worden."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "SRAM overschrijven wordt geblokkeerd"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "De film wordt afgespeeld."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Filmopname aan het starten naar"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Onderbrekingspuntgrootte"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Onderbrekingspuntslot"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "Harhalingsslot"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Schermafdruk maken."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Schermafbeeldingen opgeslagen"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Achievement ontgrendeld"
   )
MSG_HASH(
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "Zeldzame achievement ontgrendeld"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "Ranglijstpoging gestart"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "Ranglijstpoging gefaald"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "Diende %s voor %s in" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "Rang: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "Beste: %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Miniatuurtype wijzigen"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Voorbeeldweergave op volledig scherm"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Metagegevens Omschakelen"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Geen miniatuur beschikbaar"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "Alle mogelijke miniatuurdownloads zijn al geprobeerd voor deze afspeellijst."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Druk nogmaals om te stoppen..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_CLOSE_CONTENT,
   "Druk nogmaals om de inhoud te sluiten..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_RESET,
   "Druk nogmaals om te resetten..."
   )
MSG_HASH(
   MSG_TO,
   "naar"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Laden van state ongedaan gemaakt."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Onderbrekingspunt ongedaan maken..."
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Onbekend"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Ongepauzeerd."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Niet-herkend commando \"%s\" ontvangen.\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "De core-naam gebruiken voor de nieuwe configuratie."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Libretro dummy core aan het gebruiken. Opname wordt overgeslagen."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Verbind apparaat vanaf een geldige poort."
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Opnieuw opstarten..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Afsluiten..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Versie van libretro API"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Weergave grootte berekening mislukt! Zal doorgaan met rauw gegevens. Dit werkt waarschijnlijk niet goed..."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Automatisch onderbrekingspunt aan het laden vanaf"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "Automatisch onderbrekingspunt laden vanaf \"%s\" mislukt."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "Automatisch onderbrekingspunt laden vanaf \"%s\" gelukt."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s geconfigureerd in poort %u"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s losgekoppeld van poort %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) niet geconfigureerd"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) niet geconfigureerd, terugvalopties worden gebruikt"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Bluetooth scan voltooid."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "Koppeling verwijderd. Herstart RetroArch opnieuw om opnieuw te verbinden/koppelen."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Wi-Fi scan voltooid."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Bluetooth apparaten aan het scannen..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Draadloze apparaten aan het scannen..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Wi-Fi aan het inschakelen..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Wi-Fi aan het uitschakelen..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Verbinding met Wi-Fi aan het verbreken..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Aan het zoeken naar netplay hosts..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Voorbereiden voor inhoudsscan..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Voer wachtwoord in"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Wachtwoord is correct."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Wachtwoord is incorrect."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Voer Wachtwoord In"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Wachtwoord correct."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Verkeerde wachtwoord."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "Configuratie-override geladen."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Spel-remap-bestand geladen."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Inhoudsmap-remap-bestand geladen."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Core-remap-bestand geladen."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "remap-opties opgeslagen naar:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "Het is niet gelukt om remap-opties op te slaan naar:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Run-Ahead ingeschakeld. Latentieframes verwijderd: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Run-Ahead ingeschakeld met secundaire instantie. Latency frames verwijderd: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Run-Ahead uitgeschakeld."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Run-Ahead is uitgeschakeld omdat deze kern geen onderbrekingpunten ondersteunt."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "Run-Ahead is niet beschikbaar omdat deze core geen ondersteuning biedt voor deterministische onderbrekingspunten."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Onderbrekingspunt aanmaking mislukt. Ren-Ahead is uitgeschakeld."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Onderbrekingspunt aanmaking mislukt. Ren-Ahead is uitgeschakeld."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Het maken van een tweede instantie is mislukt. Run-Ahead zal nu maar één instantie gebruiken."
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "Preemptive Frames ingeschakeld. Latentieframes verwijderd: %u."
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "Preemptive Frames uitgeschakeld."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Preemptive Frames zijn uitgeschakeld omdat deze core geen onderbrekingspunten ondersteund."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "Preemptive Frames niet beschikbaar omdat de core geen deterministische onderbrekingspunten ondersteund."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "Geheugen toewijzen voor Preemptive Frames mislukt."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "Onderbrekingspunt opslaan mislukt. Preemptive Frames is uitgeschakeld."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "Onderbrekingspunt laden mislukt. Preemptive Frames is uitgeschakeld."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Scannen van bestand voltooid."
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Zoeken naar cheats succesvol gestart"
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Het starten van naar het zoeken naar cheats is mislukt."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Zoeken is niet geïnitialiseerd/gestart."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Nieuw aantal overeenkomsten = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "%u overeenkomsten toegevoegd."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Toevoegen van overeenkomsten mislukt."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Code van overeenkomst aangemaakt."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Code aanmaken mislukt."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Overeenkomst verwijderd."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Niet genoeg ruimte. Het maximum aantal gelijktijdige cheats is 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Nieuwe cheat toegevoegd aan de top van de lijst."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Nieuwe cheat toegevoegd aan de onderkant van de lijst."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Alle cheats verwijderd."
   )
MSG_HASH(
   MSG_CHEAT_RELOAD_ALL_SUCCESS,
   "Alle cheats herladen."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Nieuwe cheat toegevoegd voor deze."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Nieuwe cheat toegevoegd na deze."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Cheat gekopieerd voor deze."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Cheat gekopieerd na deze."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Cheat verwijderd."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Virtuele schijflade gesloten."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Virtuele schijflade sluiten mislukt."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Virtuele schijflade geopend."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Virtuele schijflade uitwerpen mislukt."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Schijf uit de lade verwijderd."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Schijf uit de lade verwijderen mislukt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Geen schijf geselecteerd"
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Bijgevoegde schijf: "
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Kon schijf niet toevoegen"
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Schijf in schijflade aan het instellen"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Laatst gebruikte schijf instellen mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "Verbinding met de client mislukt."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Verbinding met de host mislukt."
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "Netplay host vol."
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "Je bent verbannen van deze host."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Header van de host ontvangen mislukt."
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "RetroPrestaties: Ingelogd als \"%s\"."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Je moet hardcore-modus voor achievements pauzeren of uitschakelen om onderbrekingspunten te laden."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_SAVEFILE_PREVENTED_BY_HARDCORE_MODE,
   "Je moet hardcore-modus voor achievements pauzeren of uitschakelen om srm-opslagbestanden te laden."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Er was een onderbrekingspunten geladen. Hardcore-modus voor achievements is uitgeschakeld voor de huidige sessie."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Er was een cheat geactiveerd. Hardcore-modus voor achievements is uitgeschakeld voor de huidige sessie."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST,
   "Hardcore-modus voor achievements is veranderd door de host."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST,
   "Netplay host moet worden bijgewerkt. Hardcore-modus voor achievements is  uitgeschakeld voor huidige sessie."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "%s beheerst"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "%s voltooid"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Hardcore-modus voor achievements is ingeschakeld. Onderbrekingspunten en terugspoelen zijn uitgeschakeld"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "Dit spel heeft geen achievements."
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "Alle %d achievements geactiveerd voor deze sessie"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "Alle %d onofficiële achievements geactiveerd voor deze sessie"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "Je hebt %d van de %d achievements ontgrendeld"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "%d niet ondersteund"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_WARNING,
   "Niet-ondersteunde achievements gedetecteerd. Probeer een andere core of werk RetroArch bij."
)
MSG_HASH(
   MSG_CHEEVOS_RICH_PRESENCE_SPECTATING,
   "%s aan het toeschouwen"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "Hardcore gepauzeerd. Handmatige instelling voor video-vertraging is niet toegestaan."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL,
   "Hardcore onderbroken. Vsync swap interval boven 1 niet toegestaan."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION,
   "Hardcore gepauzeerd. Zwarte Frame Injectie is niet toegestaan."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED,
   "Hardcore gepauzeerd. Instelling niet toegestaan: %s=%s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE,
   "Hardcore gepauzeerd. U kunt geen hardecore achievements verdienen voor %s met %s te gebruiken."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "RetroAchievement: Spel kon niet worden geïdentificeerd."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "RetroAchievements spel laden mislukt: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "RetroAchievements wijzigen van media mislukt: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_LOGIN_TOKEN_EXPIRED,
   "RetroAchievements login verlopen. Voer uw wachtwoord opnieuw in en laad het spel opnieuw."
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "Laagst"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Lager"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "Normaal"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Hoger"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Hoogst"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Waarschuwing: Ontbrekende assets, gebruik de Online Updater indien beschikbaar."
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Waarschuwing: Ontbrekende lettertypen voor de geselecteerde taal, gebruik de Online Updater indien beschikbaar."
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Waarschuwing: niet-ondersteunde taal - Engels wordt gebruikt."
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Disk dumpt..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Schijf %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Laad a.u.b. eerst een kern."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Aflezen van schijf mislukt. Dump afgebroken."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Schrijven naar schijf mislukt. Dump afgebroken."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Er is geen schijf in het station geplaatst."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Shader preset succesvol verwijderd."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Fout bij verwijderen shader."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Ongeldige arcade-DAT-bestand geselecteerd."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "Het geselecteerde arcade-DAT-bestand is te groot (onvoldoende beschikbaar geheugen)."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Arcade-DAT-bestand laden mislukt (ongeldig formaat?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Ongeldige handleidingscanconfiguratie."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Geen geldige inhoud gevonden."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Inhoud aan het scannen: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Huidige items aan het controleren: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Aan het scannen: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "M3U items aan het opruimen: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Scan voltooid: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Core aan het scannen: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Backup van de geïnstalleerde core bestaat al: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Back-up aan het maken van de core: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Oude back-ups aan het verwijderen: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Core-back-up voltooid: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "Geselecteerde core back-up is al geïnstalleerd: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Core aan het herstellen: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Core-herstel voltooid:"
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "Geselecteerd core-bestand is al geïnstalleerd: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Core installeren: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Core-installatie voltooid: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "Ongeldig core-bestand geselecteerd: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Core back-up mislukt: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Core herstellen mislukt: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Core-installatie mislukt: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Core-herstel uitgeschakeld - core is vergrendeld: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "Core-installatie uitgeschakeld - core is vergrendeld: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Core vergrendelen mislukt: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Core ontrgrendelen mislukt: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "Mislukt om core te verwijderen uit de \"Inhoudloze Cores\" lijst: "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "Mislukt om core toe te voegen aan de \"Inhoudloze Cores\" lijst: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "Core-verwijdering uitgeschakeld - core is vergrendeld: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Niet-ondersteunde videomodus"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "Kan niet schrijven naar core-info-map - core-info-cache zal uitgeschakeld worden"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "Startpunt gevonden in "
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Startpunt aan het laden vanaf"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "GameMode gebruiken mislukt"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "GameMode gebruiken mislukt - zorg ervoor dat GameMode daemon is geïnstalleerd/uitgevoerd"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "Synchroniseren met exacte inhoudsframerate ingeschakeld."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "Synchroniseren met exacte inhoudsframerate uitgeschakeld."
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "Video verversingssnelheid gewijzigd naar %s Hz."
   )


/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Lakka bijwerken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Front-end naam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Lakka versie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Herstart"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Gesplitste Joy-Con"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Grafische Widgets Schaal Overschrijven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Pas een handmatige schaalfactor toe bij het tekenen van weergavewidgets. Alleen van toepassing wanneer 'Grafische Widgets Automatisch Schalen' is uitgeschakeld. Kan worden gebruikt om de grootte van versierde meldingen, indicatoren en besturingselementen onafhankelijk van het menu zelf te vergroten of te verkleinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Scherm Resolutie"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Schermresolutie: Standaard"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Schermresolutie: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Schermresolutie: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Toepassen: Standaard"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Toepassen: %dx%d\nSTART om te resetten"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Toepassen: %dx%d - %s\nSTART om te resetten"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Resetten naar: Standaard"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Resetten naar: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Resetten naar: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Selecteer weergavemodus (opnieuw opstarten vereist)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Afsluiten"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Toegang tot externe bestanden inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Open Windows-instellingen voor bestandstoegangsrechten "
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Open Windows permissie-instellingen om breedFileSystemAccess in te schakelen."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Open een andere map met behulp van de systeemsbestandskiezer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Video gamma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Soft Filter Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_SETTINGS,
   "Bluetooth Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Scan naar bluetooth apparaten en verbind ze."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Zoek naar draadloze netwerken en verbind."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Wi-Fi inschakelen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Verbind met netwerk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Verbind met netwerk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Verbinding verbreken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Ontflikker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "VI Scherm Breedte Instellen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Overscan-correctie (top)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Pas de overscan-uitsnijding van het scherm aan door de beeldgrootte te verkleinen met een opgegeven aantal scanlijnen (genomen vanaf de bovenkant van het scherm). Dit kan schaalartefacten veroorzaken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Overscan-correctie (onderkant)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Pas de overscan-uitsnijding van het scherm aan door de beeldgrootte te verkleinen met een opgegeven aantal scanlijnen (genomen vanaf de onderkant van het scherm). Dit kan schaalartefacten veroorzaken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Duurzame prestatiemodus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "CPU-prestaties en kracht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "Beleid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Governing-modus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Handmatig"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Hiermee kunt u handmatig elk detail van elke CPU aanpassen: governor, frequenties, enz. Alleen aanbevolen voor gevorderde gebruikers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Prestaties (beheerd)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Standaard en aanbevolen modus. Maximale prestaties tijdens het afspelen, en energie besparen tijdens het pauzeren of browsen van menu's."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Aangepast beheerd"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Maakt het mogelijk om te kiezen welke governors te gebruiken in menu's en tijdens gameplay. Performance, Ondemand of Schedutil worden aanbevolen tijdens het spel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Maximale prestaties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Altijd maximale prestaties: hoogste frequenties voor beste ervaring."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Minimaal verbruik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Gebruik de laagste beschikbare frequentie om energie te besparen. Nuttig op de energiezuinig apparaten, maar de prestaties zullen aanzienlijk dalen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Gebalanceerd"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Past zich aan de huidige werkbelasting aan. Werkt goed met de meeste apparaten en emulators en helpt energie te besparen. Bij veeleisende games en cores kan de prestatie op sommige apparaten afnemen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Minimale frequentie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Maximale frequentie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Minimale CPU-frequentie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Maximale CPU-frequentie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "CPU governor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Core governor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Menu governor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "GameMode"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Kan de prestaties verbeteren, latentie verminderen en problemen met het kraken van geluid oplossen. Je hebt https://github.com/FeralInteractive/gamemode nodig om dit te laten werken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Het inschakelen van Linux GameMode kan de latentie verbeteren, kraken van geluid oplossen en de algehele prestaties maximaliseren door uw CPU en GPU automatisch te configureren voor de beste prestaties.\nDe GameMode software moet worden geïnstalleerd om dit te laten werken. Zie https://github.com/FeralInteractive/gamemode voor informatie over hoe GameMode kan worden geïnstalleerd."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "PAL60 Mode Activeren"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "RetroArch Opnieuw Opstarten"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Sluit RetroArch af en start het opnieuw op. Dit is nodig voor het activeren van bepaalde menu-instellingen (bijvoorbeeld bij het wijzigen van de menudriver)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Blok Frames"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Voorkeur aanraking aan de voorkant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "Gebruikt aanraakingsinvoer aan de voorkant i.p.v. de achterkant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Touch Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Toetsenbord controller toewijziging"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Toetsenbord controller toewijzigingstype"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Small Keyboard Enable"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "invoers-blok-timeout."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Het aantal milliseconden dat gewacht moet worden om een ​​volledig invoersignaal te ontvangen. Gebruik dit als je problemen ondervindt met gelijktijdige knopdrukken (alleen Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Toon \"Herstart\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Toon de \"Herstart\" optie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Toon \"Afsluiten\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Toon de \"Afsluiten\" optie."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Met wachtwoord"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "Internet (niet verbindbaar)"
   )
MSG_HASH(
   MSG_LOCAL,
   "Lokaal"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Interne opslag status: lezen/schrijven"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Interne opslag status: alleen-lezen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Schermhelderheid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Verhoog of verlaag de schermhelderheid."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "CPU overklok"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Overklok de Switch CPU."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
   "Bluetooth Activeren"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Bepaal de status van Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Beheer services op besturingssysteemniveau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "SAMBA Enable"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Netwerkfolders delen via het SMB-protocol."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "SSH Enable"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Enable or disable remote command line access."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Wi-Fi toegangspunt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Schakel Wi-Fi toegangspunt in of uit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Tijdzone"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Selecteer je tijdzone om de datum en tijd van je locatie aan te passen."
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
   "Toont een lijst met beschikbare tijdzones. Na het selecteren van een tijdzone, worden tijd en datum aangepast aan de geselecteerde tijdzone. Het gaat ervan uit dat systeem/hardware klok op UTC staat."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Nintendo Switch opties"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Beheer Nintendo Switch Specifieke Opties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "CPU overklok"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "CPU-overklok-frequenties inschakelen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "CEC Ondersteuning"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "Schakel CEC-handshake met de tv in tijdens het docken."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "Bluetooth ERTM Uitschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "Schakel Bluetooth ERTM uit om de koppeling van sommige apparaten te repareren"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Wi-Fi toegangspunt aan het uitschakelin."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Verbinding met Wi-Fi \"%s\" aan het verbreken."
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Verbinden aan het maken met Wi-Fi \"%s\""
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Geen SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Wi-Fi toegangspunt is al gestart"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Wi-Fi toegangspunt is niet actief"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Wi-Fi-toegangspunt aan het starten met met SSID=%s en wachtwoord=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Kon het Wi-Fi-toegangspunt-configuratiebestand niet maken."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Verkeerd configuratiebestand - kon APNAME of PASSWORD niet vinden in %s"
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Muisschaal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "X/y schaal aanpassen voor de Wiimote light gun."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Aanrakeningsschaal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "Pas de x/y-schaal van de touchscreencoördinaten aan om rekening te houden met de schaalvergroting van het besturingssysteem."
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "Virtuele muis als Pointer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "Schakel om aanraakgebeurtenissen door te geven vanuit het aanraakscherm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "Virtuele muis als muis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "Schakel virtuele muisemulatie in met aanraakgebeurtenissen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Virtuele muis als touchpad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Activeer samen met de muis om het touchpad scherm te gebruiken als touchpad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Virtuele muis als trackball"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Activeer samen met de muis om het touchscreen als een trackbal te gebruiken, en inertie aan de aanwijzer toe te voegen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "Gebaren voor de virtuele muis."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "Schakel aanraakscherm-gebaren in, waaronder tikken, tikken en slepen, en vegen met de vinger."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "RGA schalen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA schalen en bicubic filteren. Kan widgets breken."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Context specifieke schaling"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Hardwarecontextschaling (indien beschikbaar)."
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "Activeer New3DS kloksnelheid / L2 cache"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "Activeer de New3DS kloksnelheid (804MHz) en L2 cache."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "3DS onderste scherm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Schakel statusinformatie in op het onderste scherm. Schakel uit om de batterijduur te verhogen en de prestaties te verbeteren."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "3DS weergavemodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Selecteert tussen 3D en 2D weergavemodus. In \"3D\" modus, zijn pixels vierkant en een diepte-effect wordt toegepast bij het bekijken van het Snel Menu. \"2D\" modus biedt de beste prestaties."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (pixel-raster-effect)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (hoge Resolutie)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Raak aan voor\nRetroArch-menu"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "Asset(s) niet gevonden"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Geen\ngegevens"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "Geen\nSchermafbeeldingen"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Spel\nhervatten"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Bewaarpunt\ncreëren"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Bewaarpunt\nladen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "Onderste scherm assetmap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "De map met assets voor het onderste scherm moet \"bottom_menu.png\" bevatten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "Lettertype inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "Lettertype van het onderste menu weergeven. Inschakelen om knopbeschrijvingen op het onderste scherm weer te geven. Heeft geen effect op onderbrekingspunten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Letterkleur rood"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Pas de rode kleur van het lettertype onderaan het scherm aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Letterkleur groen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Pas de groene kleur van het lettertype onderaan het scherm aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Letterkleur blauw"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Pas de blauwe kleur van het lettertype onderaan het scherm aan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Doorzichtigheid letters"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "Past de doorzichtigheid aan van letters op het onderste scharm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "Lettergrootteschaal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "Pas de lettergrootte van het onderste scherm aan."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Scan voltooid.<br><br>\nOm deze inhoud correct te scannen, moet je:\n<ul><li>een compatibele core al gedownload hebben</li>\n<li>Up-to-date core-infobestanden hebben via de online updater</li>\n<li>Up-to-date \"Databases\" hebben via de online updater</li>\n<li>RetroArch herstarten wanneer één of meer van de bovenstaande stapppen pas gedaan zijn</li></ul>\nUiteindelijk moet de inhoud overeen komen met bestaande databses van  <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">hier</a>. Als het nog steeds niet werkt, overweeg om<a href=\"https://www.github.com/libretro/RetroArch/issues\">een bugverslag te plaatsen</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "Touch muis is ingeschakeld"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "Touch muis is uitgeschakeld"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "sdl2 microfoon vereist sdl2 audiostuurprogramma"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "Herhaal toegankelijkheid staat aan. Hoofdmenu, laad core."
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "gestopt."
   )
#ifdef HAVE_GAME_AI
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_MENU_OPTION,
   "AI-speler overschrijven"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_GAME_AI_OPTIONS,
   "Spel-AI"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P1,
   "S1 overschrijven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P1,
   "Overschrijf speler 01"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P2,
   "S2 overschrijven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P2,
   "Overschrijf speler 02"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_SHOW_DEBUG,
   "Debuginformatie weergeven"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG,
   "Debuginformatie weergeven"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_GAME_AI,
   "Toon \"Spel-AI\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_GAME_AI,
   "Toon de \"Spel-AI\" optie"
   )
#endif
#ifdef HAVE_SMBCLIENT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SETTINGS,
   "SMB netwerkinstellingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SETTINGS,
   "Configureer de instellingen voor SMB-netwerkshares."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_ENABLE,
   "SMB client inschakelen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_ENABLE,
   "Schakel toegang tot SMB-netwerkshares in. Ethernet wordt sterk aanbevolen boven Wi-Fi voor een betrouwbaardere verbinding. Let op: het wijzigen van deze instellingen vereist een herstart van RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SERVER,
   "SMB-server"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SERVER,
   "Server IP-adres of hostnaam."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SHARE,
   "SMB share naam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SHARE,
   "Naam van de netwerkshare waartoe toegang moet worden verkregen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SUBDIR,
   "SMB submap (optioneel)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_SUBDIR,
   "Pad naar submap op de share."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_USERNAME,
   "SMB gebruikersnaam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_USERNAME,
   "Gebruikersnaam voor authenticatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_PASSWORD,
   "SMB wachtwoord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_PASSWORD,
   "Wachtwoord voor verificatie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_WORKGROUP,
   "SMB werkgroep"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_WORKGROUP,
   "Werkgroep of domeinnaam."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_AUTH_MODE,
   "SMB authenticatiemodus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_AUTH_MODE,
   "Selecteer de authenticatie gebruikt in je omgeving."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_NUM_CONTEXTS,
   "SMB maximale verbindingen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_NUM_CONTEXTS,
   "Selecteer het maximale aantal verbindingen dat in he omgeving wordt gebruikt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_TIMEOUT,
   "SMB time-out"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_TIMEOUT,
   "Selecteer standaard timeout in seconden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_BROWSE,
   "Blader door SMB share"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SMB_CLIENT_BROWSE,
   "Blader door bestanden op de geconfigureerde SMB share."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SMB_CLIENT,
   "Toon \"SMB client\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SMB_CLIENT,
   "Toon de \"SMB client\" instelling"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SMB_CLIENT_SMB_SHARE,
   "SMB share"
   )
#endif

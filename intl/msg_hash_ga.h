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
   "Príomh-Roghchlár"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Socruithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Ceanáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Stair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Íomhánna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Ceol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Físeáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Taiscéal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Croíthe gan ábhar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Iompórtáil Ábhar"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Roghchlár Tapa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Rochtain thapa a fháil ar na socruithe ábhartha go léir sa chluiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Luchtaigh Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Roghnaigh cén croí atá le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "Díluchtaigh an Croi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Scaoil an croí luchtaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Brabhsáil le haghaidh croí chur i bhfeidhm libretro Braitheann an áit a dtosaíonn an brabhsálaí ar chonair d'Eolaire Croí. Má tá sé bán, tosóidh sé sa bhfréamh.\nMás eolaire é an tEolaire Croí, úsáidfidh an roghchlár é sin mar an fillteán barr. Más cosán iomlán é an tEolaire Croíthe, tosóidh sé san fhillteán ina bhfuil an comhad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Luchtaigh Ábhar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Roghnaigh cén t-ábhar le tosú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Brabhsáil le haghaidh ábhair. Chun ábhar a luchtú, teastaíonn 'Croi' uait le húsáid, agus comhad ábhair.\nChun rialú a dhéanamh ar an áit a dtosaíonn an roghchlár ag brabhsáil le haghaidh ábhair, socraigh 'Eolaire Brabhsálaí Comhad'. Mura bhfuil sé socraithe, tosóidh sé sa fhréamh.\nScagfaidh an brabhsálaí síntí amach don croí deireanach a socraíodh i 'Luchtaigh Croi', agus úsáidfidh sé an croí sin nuair a luchtófar ábhar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Luchtaigh Diosca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Luchtaigh diosca meán fisiciúil. Roghnaigh an croí (Luchtaigh Croí) ar dtús le húsáid leis an diosca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Diosca Dumpála"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Dumpáil an diosca meán fisiciúil chuig an stóras inmheánach. Sábhálfar é mar chomhad íomhá."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Díbirt Diosca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Díbirtíonn sé an diosca as an tiomántán fisiceach CD/DVD."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Seinmliostaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Beidh ábhar scanta a mheaitseálann an bunachar sonraí le feiceáil anseo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Iompórtáil Ábhar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Cruthaigh agus nuashonraigh seinmliostaí trí ábhar a scanadh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Taispeáin Roghchlár na Deisce"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Oscail an roghchlár deisce traidisiúnta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Díchumasaigh Mód Ciosc (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Taispeáin na socruithe uile a bhaineann leis an gcumraíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Nuashonraitheoir Ar Líne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Íoslódáil breiseáin, comhpháirteanna agus ábhar le haghaidh RetroArch."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Bí páirteach i seisiún líonraithe nó déan é a óstáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Socruithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Cumraigh an clár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Eolas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Taispeáin faisnéis chórais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Comhad Cumraíochta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Bainistigh agus cruthaigh comhaid chumraíochta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Cabhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Foghlaim tuilleadh faoi conas a oibríonn an clár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Atosaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Atosaigh an feidhmchlár RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Scoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Scoir feidhmchlár RetroArch. Tá sábháil chumraíochta ar scor cumasaithe."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Scoir feidhmchlár RetroArch. Tá sábháil chumraíochta ar scor díchumasaithe. Scoir feidhmchlár RetroArch. Tá sábháil chumraíochta ar scor díchumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Scoir RetroArch. Má mharaíonn tú an clár ar aon bhealach crua (SIGKILL, srl.), cuirfear deireadh le RetroArch gan an chumraíocht a shábháil ar aon nós. Ar chórais Unix, ceadaíonn SIGINT/SIGTERM díthosú glan lena n-áirítear sábháil chumraíochta má tá sé cumasaithe."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Íoslódáil Croi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Íoslódáil agus suiteáil croí ón nuashonraitheoir ar líne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Suiteáil nó Athchóirigh Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Suiteáil nó athchóirigh croí ón eolaire 'Íoslódálacha'."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Tosaigh Próiseálaí Físe"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Tosaigh RetroPad Cianda"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Eolaire Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Íoslódálacha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Brabhsáil Cartlann"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Luchtaigh Cartlann"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Ceanáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Beidh ábhar a chuirtear le 'Is Fearr Liom' le feiceáil anseo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Ceol"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Beidh ceol a seinneadh roimhe seo le feiceáil anseo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Íomhánna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Beidh íomhánna a breathnaíodh roimhe seo le feiceáil anseo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Beidh físeáin a imríodh roimhe seo le feiceáil anseo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Taiscéal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Brabhsáil an t-ábhar go léir a mheaitseálann an bunachar sonraí trí chomhéadan cuardaigh catagóirithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Croíthe gan ábhar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Beidh croíthe suiteáilte ar féidir leo oibriú gan ábhar a luchtú le feiceáil anseo."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Íoslódálaí Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Nuashonraigh Croíthe Suiteáilte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Nuashonraigh na croíthe suiteáilte go léir go dtí an leagan is déanaí atá ar fáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Athraigh Croíthe go Leaganacha Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Cuir na leaganacha is déanaí ón Play Store in ionad na croíthe oidhreachta agus na gcroíleacáin atá suiteáilte de láimh, más féidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Nuashonraitheoir Mionsamhlacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Íoslódáil pacáiste iomlán mionsamhlacha don chóras roghnaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Nuashonróir Mionsamhlacha Seinmliosta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Íoslódáil mionsamhlacha d’iontrálacha sa seinmliosta roghnaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Íoslódálaí Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Íoslódáil ábhar saor in aisce don chroí roghnaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Íosluchtaigh croí chóras comhaid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Íoslódáil comhaid chórais chúnta a theastaíonn le haghaidh croí oibriúchán ceart/optamach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Nuashonraigh Comhaid Faisnéise Lárnacha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Nuashonraigh Sócmhainní"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Nuashonraigh Próifílí Rialaitheora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Nuashonraigh na hAicearraí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Nuashonraigh Bunachair Sonraí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Nuashonraigh Forleagan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Nuashonraigh Scáthadóirí GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Nuashonraigh Scáthóirí Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Nuashonraigh Scáthóirí Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Faisnéis Chroí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Féach ar fhaisnéis a bhaineann leis an bhfeidhmchlár/croí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Faisnéis Diosca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Féach ar fhaisnéis faoi dhioscaí meán atá curtha isteach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Faisnéis Líonra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Féach ar chomhéadan(na) líonra agus ar na seoltaí IP gaolmhara."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Faisnéis Córais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Féach ar fhaisnéis a bhaineann go sonrach leis an bhfeiste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Bainisteoir Bunachar Sonraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Féach ar bhunachair shonraí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Bainisteoir Cúrsóra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Féach ar chuardaigh roimhe seo."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Ainm Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Lipéad Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Leagan Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Ainm an Chórais"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Monaróir Córais"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Catagóirí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Údar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Ceadanna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Ceadúnas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Síneadh Tacaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "API Grafaicí Riachtanacha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Cosán Iomlán"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Sábháil Tacaíocht Stáit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Dada"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Bunúsach (Sábháil/Lódáil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Sraithuimhir (Sábháil/Lódáil, Athchasadh)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Cinntitheach (Sábháil/Lódáil, Athchasadh, Rith Chun Tosaigh, Glansúgradh)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Dochtearraí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "- Nóta: Tá 'Tá Comhaid Chórais san Eolaire Ábhair' cumasaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "- Ag féachaint isteach: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Ar iarraidh, Riachtanach:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Ar iarraidh, Roghnach:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "I láthair, Riachtanach:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "I láthair, Roghnach:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Glasáil Croí Suiteáilte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Cosc a chur ar mhodhnú an chroí atá suiteáilte faoi láthair. Féadfar é a úsáid chun nuashonruithe nach dteastaíonn a sheachaint nuair a bhíonn leagan croí ar leith ag teastáil don ábhar (m. sh. tacair ROM Arcade) nó nuair a athraíonn formáid staid shábháilte an chroí féin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Eisiamh ón Roghchlár 'Croíthe Gan Ábhar'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Cosc a chur ar an croí seo a bheith á thaispeáint sa chluaisín/roghchlár 'Croíthe Gan Ábhar'. Ní bhaineann sé seo ach amháin nuair a bhíonn an modh taispeána socraithe go 'Saincheaptha'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Scrios an Croi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Bain an croí seo den diosca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Croí Cúltaca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Cruthaigh cúltaca cartlannaithe den croí atá suiteáilte faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Athchóirigh Cúltaca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Suiteáil leagan roimhe seo den croí ó liosta cúltaca cartlannaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Scrios Cúltaca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Bain comhad as liosta na gcúltaca cartlannaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[Uathoibríoch]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Dáta Tógála"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "Leagan RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Leagan Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Tiomsaitheoir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Samhail LAP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Gnéithe LAP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Ailtireacht LAP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Croíthe LAP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT Ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUNDLE_IDENTIFIER,
   "Aitheantóir Pacáiste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Aitheantóir Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Córas Comhéadain"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "Leibhéal Rátála Retro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Foinse Cumhachta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Tiománaí Comhthéacs Físeáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Leithead Taispeána (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Airde Taispeána (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Taispeáin DPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "Tacaíocht LibretroDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Tacaíocht Forleagan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Tacaíocht Chomhéadain Ordú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Tacaíocht Chomhéadain Ordú Líonra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Tacaíocht Rialaitheora Líonra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Tacaíocht Cócó"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "Tacaíocht PNG (RPNG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "Tacaíocht JPEG (RJPEG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "Tacaíocht BMP (RBMP)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "Tacaíocht TGA (RTGA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "Tacaíocht SDL 1.2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "Tacaíocht SDL 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D8_SUPPORT,
   "Tacaíocht Direct3D 8"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D9_SUPPORT,
   "Tacaíocht Direct3D 9"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D10_SUPPORT,
   "Tacaíocht Direct3D 10"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D11_SUPPORT,
   "Tacaíocht Direct3D 11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D12_SUPPORT,
   "Tacaíocht Direct3D 12"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GDI_SUPPORT,
   "Tacaíocht GDI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Tacaíocht Vulkan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Tacaíocht Miotail"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "Tacaíocht OpenGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "Tacaíocht OpenGL ES"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Tacaíocht Snáithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "Tacaíocht KMS/EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Tacaíocht udev"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "Tacaíocht OpenVG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "Tacaíocht EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "Tacaíocht X11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Tacaíocht Wayland"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "Tacaíocht XVideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "Tacaíocht ALSA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "Tacaíocht OSS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "Tacaíocht OpenAL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "Tacaíocht OpenSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "Tacaíocht RSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "Tacaíocht RoarAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "Tacaíocht JACK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "Tacaíocht PulseAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PIPEWIRE_SUPPORT,
   "Tacaíocht PipeWire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "Tacaíocht CoreAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "Tacaíocht CoreAudio V3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "Tacaíocht DirectSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "Tacaíocht BAILE"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "Tacaíocht XAudio2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "tacaíocht zlib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "Tacaíocht 7zip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Tacaíocht Leabharlainne Dinimiciúil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Luchtú Dinimiciúil Rith-ama Leabharlann libretro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Tacaíocht Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "Tacaíocht GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "Tacaíocht HLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "Tacaíocht Íomhá SDL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "Tacaíocht FFmpeg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "tacaíocht mpv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "Tacaíocht CoreText"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "Tacaíocht FreeType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "Tacaíocht STB TrueType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Tacaíocht Netplay (Piaraí go Piara)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Tacaíocht Video4Linux2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SSL_SUPPORT,
   "Tacaíocht SSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "tacaíocht libusb"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Roghnú Bunachar Sonraí"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Ainm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Cur síos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Seánra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Éachtaí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Catagóir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Teanga"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Réigiún"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Eisiach don chonsól"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Eisiach don ardán"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Scór"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Meáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Rialuithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Stíl Ealaíne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Imirt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Insint"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "Rithim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Peirspictíocht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Socrú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Amhairc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Feithicleach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Foilsitheoir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Forbróir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Bunús"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Saincheadúnas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Rátáil TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Rátáil Iris Famitsu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Léirmheas ar Iris Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Rátáil Iris Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Eagrán Iris Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Dáta Eisiúna Mí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Dáta Eisiúna Bliain"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Rátáil BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Rátáil ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Rátáil ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Rátáil PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Crua-earraí Feabhsúcháin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Rátáil CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Sraitheach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analógach Tacaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Tacaíodh le Rumble"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Comharchumann Tacaithe"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Cumraíocht Luchtaithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Luchtaigh an chumraíocht atá ann cheana féin agus cuir luachanna reatha ina áit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Sábháil an Chumraíocht Reatha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Forscríobh an comhad cumraíochta reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Sábháil Cumraíocht Nua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Sábháil an chumraíocht reatha i gcomhad ar leith."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_AS_CONFIG,
   "Sábháil Cumraíocht Mar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG,
   "Sábháil an chumraíocht reatha mar chomhad cumraíochta saincheaptha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_MAIN_CONFIG,
   "Sábháil an Phríomhchumraíocht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG,
   "Sábháil an chumraíocht reatha mar phríomhchumraíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Athshocraigh go Réamhshocruithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Athshocraigh an chumraíocht reatha go dtí na luachanna réamhshocraithe."
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Rialuithe Bunúsacha an Roghchláir"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Scrollaigh Suas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Scrollaigh Síos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Deimhnigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "Eolas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Roghchlár a Athrú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Scoir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Éadromaigh an Méarchlár"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Tiománaithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Athraigh na tiománaithe a úsáideann an córas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Físeán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Athraigh socruithe aschuir físe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Fuaim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Athraigh socruithe ionchuir/aschuir fuaime."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Ionchur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Athraigh socruithe an rialtóra, an mhéarchláir agus na luiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Aga folaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Athraigh socruithe a bhaineann le físe, fuaim agus moill ionchuir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Athraigh socruithe croí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Cumraíocht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Athraigh socruithe réamhshocraithe le haghaidh comhaid chumraíochta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Sábháil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Athraigh socruithe sábhála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Sioncrónú Cloud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Athraigh socruithe sioncrónaithe scamall."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Cumasaigh Sioncrónú Cloud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Déan iarracht cumraíochtaí, sram, agus stáit a shioncronú le soláthraí stórála scamall."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Sioncrónú Scamall Milltiúil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Sioncrónaigh: Sábhálacha/Stáit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Sioncrónaigh: Comhaid Chumraíochta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Sioncrónaigh: Íomhánna Mionsamhail"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Sioncrónú: Comhaid Chórais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "Nuair a bheidh sé cumasaithe, déanfar sábhálacha/stáit a shioncronú leis an scamall."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "Nuair a bheidh sé cumasaithe, déanfar comhaid chumraíochta a shioncronú leis an scamall."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "Nuair a bheidh sé cumasaithe, déanfar mionsamhlacha a shioncronú leis an scamall. Ní mholtar é seo go ginearálta ach amháin i gcás bailiúcháin mhóra mionsamhlacha saincheaptha; ar shlí eile is rogha níos fearr é an t-íoslódálaí mionsamhlacha."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "Nuair a bheidh sé cumasaithe, déanfar comhaid chórais a shioncronú leis an scamall. Is féidir leis seo an t-am a thógann sé chun sioncronú a dhéanamh a mhéadú go suntasach; bain úsáid as go cúramach."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Nuair a bhíonn sé díchumasaithe, bogtar comhaid chuig fillteán cúltaca sula ndéantar iad a athscríobh nó a scriosadh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Cúltaca Sioncrónaithe Cloud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Cén prótacal líonra stórála scamall atá le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "URL Stórála Cloud"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "An URL don phointe iontrála API chuig an tseirbhís stórála scamall."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Ainm úsáideora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "D'ainm úsáideora do do chuntas stórála scamall."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Pasfhocal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "Do phasfhocal do do chuntas stórála scamall."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Logáil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Athraigh socruithe logála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Brabhsálaí Comhad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Athraigh socruithe Brabhsálaí Comhad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Comhad cumraíochta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Comhad cartlainne comhbhrúite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Comhad cumraíochta taifeadta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Comhad cúrsóra bunachar sonraí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Comhad cumraíochta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Comhad réamhshocraithe scáthaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Comhad scáthaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Athmhapáil comhad rialuithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Comhad aicearra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Comhad forleagan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Comhad bunachar sonraí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "Comhad cló TrueType."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Comhad simplí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Físeán. Roghnaigh é chun an comhad seo a oscailt leis an seinnteoir físeáin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Ceol. Roghnaigh é chun an comhad seo a oscailt leis an seinnteoir ceoil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Comhad íomhá."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Íomhá. Roghnaigh í chun an comhad seo a oscailt leis an lucht féachana íomhánna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Croílár Libretro. Trí é seo a roghnú, nascfar an croí seo leis an gcluiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Croíthe Libretro. Roghnaigh an comhad seo le go luchtóidh RetroArch an croí seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Eolaire. Roghnaigh é chun an t-eolaire seo a oscailt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Teorainn Fráma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Athraigh socruithe siarfhuála, mearfhuála agus mallghluaiseachta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Taifeadadh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Athraigh socruithe taifeadta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Taispeántas ar an Scáileán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Athraigh forleagan taispeána agus forleagan méarchláir, agus socruithe fógraí ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Comhéadan Úsáideora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Athraigh socruithe comhéadan úsáideora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Seirbhís AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Athraigh socruithe don tSeirbhís AI (Aistriúchán/TTS/Ilghnéitheach)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Inrochtaineacht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Athraigh socruithe don insinteoir Inrochtaineachta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Bainistíocht Cumhachta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Athraigh socruithe bainistíochta cumhachta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Éachtaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Athraigh socruithe éachtaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Líonra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Athraigh socruithe an fhreastalaí agus an líonra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Seinmliostaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Athraigh socruithe seinmliosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Úsáideoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Athraigh socruithe príobháideachais, cuntais agus ainm úsáideora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Eolaire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Athraigh na heolairí réamhshocraithe ina bhfuil comhaid suite."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS,
   "Haiceanna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Mapáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Meáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Feidhmíocht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Fuaim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Sonraíochtaí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Stóráil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Córas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Amú"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Athraigh socruithe a bhaineann le Steam."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Ionchur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Tiománaí ionchuir le húsáid. Éilíonn roinnt tiománaithe físe tiománaí ionchuir difriúil. (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "Léann an tiománaí udev imeachtaí evdev le haghaidh tacaíochta méarchláir. Tacaíonn sé freisin le glaonna ar ais méarchláir, lucha agus ceapa tadhaill. \nDe réir réamhshocraithe i bhformhór na ndáiltí, ní bhíonn ach nóid /dev/input ag fréimhe (mód 600). Is féidir leat riail udev a shocrú a fhágann go bhfuil siad seo inrochtana ag daoine nach fréimhe iad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "Éilíonn tiománaí ionchuir linuxraw TTY gníomhach. Léitear imeachtaí méarchláir go díreach ón TTY rud a fhágann go bhfuil sé níos simplí, ach níl sé chomh solúbtha le udev. Ní thacaítear le lucha, srl., ar chor ar bith. Úsáideann an tiománaí seo an API luamhán stiúrtha níos sine (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Tiománaí ionchuir. D’fhéadfadh an tiománaí físe tiománaí ionchuir difriúil a chur i bhfeidhm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Rialaitheoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Tiománaí rialaitheora le húsáid. (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "Tiománaí rialtóir ionchur díreach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "Tiománaí gléas comhéadain dhaonna íseal-leibhéil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Tiománaí Linux amh, úsáideann sé API luamhán stiúrtha oidhreachta. Bain úsáid as udev ina ionad más féidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Tiománaí Linux le haghaidh rialtóirí atá ceangailte le port comhthreomhar trí oiriúnaitheoirí speisialta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Tiománaí rialtóra bunaithe ar leabharlanna SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Tiománaí rialtóra le comhéadan udev, molta go ginearálta. Úsáideann sé an API luamhán stiúrtha evdev le déanaí le haghaidh tacaíochta luamhán stiúrtha. \nTacaíonn sé le plugáil the agus aiseolas fórsa. De réir réamhshocraithe i bhformhór na ndáiltí, is fréamhacha amháin iad nóid /dev/input (mód 600). Is féidir leat riail udev a shocrú a fhágann go bhfuil siad seo inrochtana ag daoine nach fréamhacha iad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "Tiománaí rialaitheora XInput. Den chuid is mó le haghaidh rialaitheoirí Xbox."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Físeán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Tiománaí físe le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "Tiománaí OpenGL 1.x. An leagan íosta atá ag teastáil: OpenGL 1.1. Ní thacaíonn sé le scáthaitheoirí. Bain úsáid as tiománaithe OpenGL níos déanaí ina ionad, más féidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "Tiománaí OpenGL 2.x. Ceadaíonn an tiománaí seo croíleacáin libretro GL a úsáid sa bhreis ar chroíleacáin a rindreáiltear le bogearraí. An leagan íosta atá ag teastáil: OpenGL 2.0 nó OpenGLES 2.0. Tacaíonn sé leis an bhformáid scáthaithe GLSL. Bain úsáid as tiománaí glcore ina ionad, más féidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "Tiománaí OpenGL 3.x. Ceadaíonn an tiománaí seo croíleacáin libretro GL a úsáid sa bhreis ar chroíleacáin a rindreáiltear le bogearraí. An leagan íosta atá ag teastáil: OpenGL 3.2 nó OpenGLES 3.0+. Tacaíonn sé leis an bhformáid scáthaithe Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Tiománaí Vulkan. Ceadaíonn an tiománaí seo croíleacáin libretro Vulkan a úsáid sa bhreis ar chroíleacáin a rindreáiltear le bogearraí. An leagan íosta atá ag teastáil: Vulkan 1.0. Tacaíonn sé le scáthaitheoirí HDR agus Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "Tiománaí rindreáilte ag bogearraí SDL 1.2. Meastar nach bhfuil an fheidhmíocht is fearr. Smaoinigh ar é a úsáid ach mar rogha dheireanach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "Tiománaí SDL 2 rindreáilte le bogearraí. Tá feidhmíocht maidir le croí chur i bhfeidhm libretro arna rindreáil ag bogearraí ag brath ar chur i bhfeidhm SDL d’ardáin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Tiománaí miotail do ardáin Apple. Tacaíonn sé leis an bhformáid scáthaithe Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Tiománaí Direct3D 8 gan tacaíocht do scáthóirí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Tiománaí Direct3D 9 le tacaíocht don seanfhormáid scáthaithe Cg."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Tiománaí Direct3D 9 le tacaíocht don fhormáid scáthaitheora HLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Tiománaí Direct3D 10 le tacaíocht don fhormáid scáthaithe Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Tiománaí Direct3D 11 le tacaíocht do HDR agus don fhormáid scáthaithe Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Tiománaí Direct3D 12 le tacaíocht do HDR agus don fhormáid scáthaithe Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "Tiománaí DispmanX. Úsáideann sé API DispmanX don GPU Videocore IV i Raspberry Pi 0..3. Gan tacaíocht le haghaidh forleagan ná scáthú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "Tiománaí LibCACA. Ginfidh sé aschur carachtar in ionad grafaicí. Ní mholtar é le haghaidh úsáide praiticiúla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Tiománaí físe Exynos íseal-leibhéil a úsáideann an bloc G2D i Samsung Exynos SoC le haghaidh oibríochtaí blit. Ba cheart go mbeadh an fheidhmíocht is fearr do chroíthe rindreáilte bogearraí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Tiománaí físe DRM simplí. Is tiománaí físe ísealleibhéil é seo a úsáideann libdrm le haghaidh scálú crua-earraí ag baint úsáide as forleagan GPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Tiománaí físe ísealleibhéil Sunxi a úsáideann an bloc G2D i SoCanna Allwinner."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Tiománaí Wii U. Tacaíonn sé le scáthaitheoirí Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Tiománaí lasctha. Tacaíonn sé leis an bhformáid scáthaithe GLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "Tiománaí OpenVG. Úsáideann sé API grafaicí veicteoir 2T luasghéaraithe crua-earraí OpenVG."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "Tiománaí GDI. Úsáideann sé comhéadan seanbhunaithe Windows. Ní mholtar é."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Tiománaí físe reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Fuaim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Tiománaí fuaime le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "Tiománaí rsound le haghaidh córais fuaime líonraithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Tiománaí le haghaidh oidhreachta córas fuaime oscailte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Tiománaí réamhshocraithe ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "Tiománaí ALSA le tacaíocht snáitheála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "Tiománaí ALSA curtha i bhfeidhm gan spleáchais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "Tiománaí do chóras fuaime RoarAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "Tiománaí OpenAL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "Tiománaí OpenSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "Tiománaí DirectSound. Úsáidtear DirectSound den chuid is mó ó Windows 95 go Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Tiománaí API Seisiún Fuaime Windows. Úsáidtear WASAPI den chuid is mó ó Windows 7 agus os a chionn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "Tiománaí PulseAudio. Má úsáideann an córas PulseAudio, déan cinnte an tiománaí seo a úsáid in ionad ALSA, mar shampla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "Tiománaí PipeWire. Má úsáideann an córas PipeWire, déan cinnte an tiománaí seo a úsáid in ionad PulseAudio, mar shampla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Tiománaí do threalamh nasc fuaime jack."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Micreafón"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Tiománaí micreafón le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Athshamplálaí Micreafóin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Tiománaí athshamplála micreafóin le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Frámaí Bloc Micreafóin"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Athshamplóir Fuaime"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Tiománaí athshamplála fuaime le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Cur i bhfeidhm Sinc fuinneoige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Cur i bhfeidhm Cosine casta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "An cur i bhfeidhm athshamplála is gaire. Ní thugann an t-athshamplálaí seo aird ar an socrú cáilíochta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Ceamara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Tiománaí ceamara le húsáid."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Tiománaí Bluetooth le húsáid."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Tiománaí Wi-Fi le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Suíomh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Tiománaí suímh le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Roghchlár"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Tiománaí roghchláir le húsáid. (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "Is comhéadan grafach úsáideora RetroArch é XMB atá cosúil le roghchlár consóil den 7ú glúin. Is féidir leis na gnéithe céanna le Ozone a thacú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Is é Ozone an chomhéadan úsáideora grafach réamhshocraithe de chuid RetroArch ar fhormhór na n-ardán. Tá sé optamaithe le haghaidh nascleanúna le rialtóir cluiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "Is comhéadan úsáideora grafach simplí ionsuite é RGUI do RetroArch. Tá na ceanglais feidhmíochta is ísle aige i measc na dtiománaithe roghchláir, agus is féidir é a úsáid ar scáileáin ísealtaifigh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "Ar ghléasanna soghluaiste, úsáideann RetroArch an comhéadan úsáideora soghluaiste, MaterialUI, de réir réamhshocraithe. Tá an comhéadan seo deartha timpeall ar scáileán tadhaill agus gléasanna pointeora, cosúil le luch/liathróid rianaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Taifead"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Tiománaí taifeadta le húsáid."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Tiománaí MIDI le húsáid."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "Athrú Athraithe CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Aschur comharthaí dúchasacha, ísealtaifigh le húsáid le taispeántais CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Aschur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Athraigh socruithe aschuir físe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Mód Lánscáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Athraigh socruithe mód lánscáileáin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Mód Fuinneoige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Athraigh socruithe mód fuinneoige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Scálú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Athraigh socruithe scálúcháin físe."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Athraigh socruithe HDR físe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Sioncrónú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Athraigh socruithe sioncrónaithe físe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Cuir an Scáileán ar Fionraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Cosc a chur ar scáileáin do chórais ó bheith gníomhach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Cuireann sé an spárálaíscáileáin ar fionraí. Is leid í nach gá don tiománaí físe a chomhlíonadh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Físeán Snáithithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Feabhsaíonn sé feidhmíocht ar chostas moille agus níos mó stad físe. Ná húsáid ach amháin mura féidir an luas iomlán a bhaint amach ar shlí eile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Bain úsáid as tiománaí físe snáithithe. D’fhéadfadh feabhas a chur ar fheidhmíocht leis seo ach d’fhéadfadh moill agus níos mó stad físe a bheith mar thoradh air."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Ionsá Fráma Dubh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "RABHADH: D’fhéadfadh caochadh tapa a bheith ina chúis le buanseasmhacht íomhá ar roinnt taispeántais. Úsáid ar do phriacal féin // Cuir fráma(í) dubha idir frámaí. Is féidir doiléire gluaisne a laghdú go mór trí scanadh CRT a aithris, ach ar chostas gile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Cuireann sé fráma(í) dubha idir frámaí le haghaidh soiléireacht ghluaisne feabhsaithe. Ná húsáid ach an rogha atá ainmnithe do do ráta athnuachana reatha taispeána. Ní le húsáid ag rátaí athnuachana nach n-iolraithe de 60Hz iad amhail 144Hz, 165Hz, etc. Ná cuir le chéile é le Eatramh Malartaithe > 1, fo-fhrámaí, Moill Fráma, nó Sioncrónaigh le Ráta Fráma Ábhair Beacht. Tá sé ceart go leor VRR an chórais a fhágáil ar siúl, ach ní an socrú sin amháin. Má thuga[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Ionsá Fráma Dubh - Frámaí Dorcha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Coigeartaigh líon na bhfrámaí dubha i seicheamh iomlán scanadh BFI. Ciallaíonn níos mó soiléireacht ghluaisne níos airde, ciallaíonn níos lú gile níos airde. Ní bhaineann sé seo le 120hz mar níl ach fráma BFI amháin le hoibriú leis san iomlán. Cuirfidh socruithe níos airde ná mar is féidir teorainn leis an uasmhéid is féidir duit don ráta athnuachana roghnaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Coigeartaíonn sé seo líon na bhfrámaí a thaispeántar sa seicheamh bfi atá dubh. Méadaíonn níos mó frámaí dubha soiléireacht ghluaisne ach laghdaíonn sé gile. Ní bhaineann sé seo le 120hz mar níl ach fráma breise amháin 60hz ann san iomlán, mar sin caithfidh sé a bheith dubh nó ní bheadh ​​BFI gníomhach ar chor ar bith."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Fo-fhrámaí Scáthóra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "RABHADH: D’fhéadfadh caochadh tapa a bheith ina chúis le buanseasmhacht íomhá ar roinnt taispeántais. Úsáid ar do phriacal féin // Insamhladh líne scanadh rollta bhunúsach thar ilfho-fhrámaí tríd an scáileán a roinnt go hingearach agus gach cuid den scáileán a rindreáil de réir líon na bhfo-fhrámaí atá ann."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Cuireann sé fráma(í) scáthaithe breise isteach idir frámaí le haghaidh aon éifeachtaí scáthaithe féideartha atá deartha chun rith níos tapúla ná ráta an ábhair. Ná húsáid ach an rogha atá ainmnithe do ráta athnuachana reatha an taispeána. Ní le húsáid ag rátaí athnuachana nach ilchodacha de 60Hz iad amhail 144Hz, 165Hz, etc. Ná cuir le chéile é le Eatramh Malartaithe > 1, BFI, Moill Fráma, nó Sioncrónaigh le Ráta Fráma Ábhair Beacht. Tá sé ceart go leor VRR [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Scáileán GPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Insamhalta scanlíne rollta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "RABHADH: D’fhéadfadh caochadh tapa a bheith ina chúis le buanseasmhacht íomhá ar roinnt taispeántais. Úsáid ar do phriacal féin // Insamhladh líne scanadh rollta bhunúsach thar ilfho-fhrámaí tríd an scáileán a roinnt go hingearach agus gach cuid den scáileán a rindreáil de réir líon na bhfo-fhrámaí atá ann."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Insamhlaíonn sé líne scanadh rollta bhunúsach thar ilfhofhrámaí tríd an scáileán a roinnt go hingearach agus gach cuid den scáileán a rindreáil de réir líon na bhfofhrámaí atá ann ó bharr an scáileáin anuas."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Gabhann scáileáin ábhar scáthaithe GPU más féidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Scagadh Délíneach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Cuir beagán doiléire leis an íomhá chun imill chrua picteilíní a mhaolú. Is beag tionchar atá ag an rogha seo ar fheidhmíocht. Ba chóir é a dhíchumasú má tá scáthaitheoirí in úsáid."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Idirshuíomh Íomhá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Sonraigh modh idirshuíomh íomhá agus ábhar á scálú tríd an IPU inmheánach. Moltar 'Déchiúbach' nó 'Dálíneach' agus scagairí físe faoi thiomáint ag an LAP in úsáid. Níl aon tionchar ag an rogha seo ar fheidhmíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Déchiúbach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Délíneach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Comharsa is Gaire"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Idirshuíomh Íomhá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Sonraigh modh idirshuíomh íomhá nuair a bhíonn 'Scála Slánuimhreacha' díchumasaithe. Is ag 'Comharsa is Gaire' atá an tionchar feidhmíochta is lú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Comharsa is Gaire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Leathlíneach"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Moill Uath-Scáthúcháin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Moill ar scáthaitheoirí uathluchtaithe (i ms). Is féidir leo dul timpeall ar glitches grafacha agus bogearraí 'scáileáin a thógáil' in úsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Scagaire Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Cuir scagaire físe faoi thiomáint LAP i bhfeidhm. D’fhéadfadh costas ard feidhmíochta a bheith i gceist. D’fhéadfadh roinnt scagairí físe a bheith ag obair ach amháin le croíthe a úsáideann dath 32-giotán nó 16-giotán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Cuir scagaire físe faoi thiomáint LAP i bhfeidhm. D’fhéadfadh costas ard feidhmíochta a bheith i gceist. D’fhéadfadh roinnt scagairí físe a bheith ag obair ach amháin le croíthe a úsáideann dath 32-giotán nó 16-giotán. Is féidir leabharlanna scagairí físe atá nasctha go dinimiciúil a roghnú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Cuir scagaire físe faoi thiomáint LAP i bhfeidhm. D’fhéadfadh costas ard feidhmíochta a bheith leis. D’fhéadfadh roinnt scagairí físe a bheith ag obair ach amháin le croíthe a úsáideann dath 32-giotán nó 16-giotán. Is féidir leabharlanna scagairí físe ionsuite a roghnú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Bain Scagaire Físe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Díluchtaigh aon scagaire físe gníomhach faoi thiomáint ag LAP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Cumasaigh lánscáileán thar an notch i bhfeistí Android agus iOS"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "Athrú Athraithe CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Do thaispeántais CRT amháin. Déantar iarracht an réiteach croí/cluiche agus an ráta athnuachana cruinn a úsáid."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Athraigh idir réiteach sár-dhúchasach agus ultra-leathan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Lárú Cothrománach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Téigh trí na roghanna seo mura bhfuil an íomhá lárnaithe i gceart ar an taispeáint."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Méid Cothrománach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Roghnaigh na roghanna seo chun na socruithe cothrománacha a choigeartú agus méid na híomhá a athrú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_VERTICAL_ADJUST,
   "Lárú Ingearach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Úsáid an Roghchlár Ardtaifigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Athraigh go samhaltán ardtaifigh le húsáid le biachláir ardtaifigh nuair nach bhfuil aon ábhar lódáilte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Ráta Athnuachana Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Bain úsáid as ráta athnuachana saincheaptha atá sonraithe sa chomhad cumraíochta más gá."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Innéacs Monatóireachta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Roghnaigh cén scáileán taispeána le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Cén monatóir is fearr leat. Ciallaíonn 0 (réamhshocraithe) nach fearr aon mhonatóir ar leith, 1 agus os a chionn (1 an chéad mhonatóir), molann sé do RetroArch an monatóir sin a úsáid."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Optamaigh do Wii U GamePad (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Bain úsáid as scála cruinn 2x den GamePad mar radharcphort. Díchumasaigh chun taispeáint ag taifeach dúchasach na teilifíse."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Rothlú Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Cuireann sé rothlú áirithe ar an bhfíseán i bhfeidhm. Cuirtear an rothlú leis na rothluithe a shocraíonn an croí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Treoshuíomh an Scáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Éilíonn sé treoshuíomh áirithe den scáileán ón gcóras oibriúcháin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Innéacs GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Roghnaigh cén cárta grafaicí atá le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Fritháireamh Cothrománach an Scáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Cuireann sé seo iallach ar fhritháireamh áirithe a chur i bhfeidhm go cothrománach ar an bhfíseán. Cuirtear an fhritháireamh i bhfeidhm go domhanda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Fritháireamh Ingearach an Scáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Cuireann sé seo iallach ar fhritháireamh áirithe go hingearach a chur ar an bhfíseán. Cuirtear an fhritháireamh i bhfeidhm go domhanda."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Ráta Athnuachana Ingearach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Ráta athnuachana ingearach do scáileáin. Úsáidtear é seo chun ráta ionchuir fuaime oiriúnach a ríomh. \nDéanfar neamhaird air seo má tá 'Físeán Snáithithe' cumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Ráta Athnuachana Scáileáin Measta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "An ráta athnuachana measta cruinn don scáileáin i Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "An ráta athnuachana cruinn de do mhonatóir (Hz). Úsáidtear é seo chun an ráta ionchuir fuaime a ríomh leis an bhfoirmle:\naudio_input_rate = ráta ionchuir cluiche * ráta athnuachana taispeána / ráta athnuachana cluiche\nMura dtuairiscíonn an croí aon luachanna, glacfar leis go bhfuil réamhshocruithe NTSC ann le haghaidh comhoiriúnachta.\nBa chóir go bhfanfadh an luach seo gar do 60Hz chun athruithe móra páirce a sheachaint. Mura ritheann do mhonatóir ag 60Hz nó gar dó, dích[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Socraigh Ráta Athnuachana Tuairiscithe Taispeána"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "An ráta athnuachana mar a thuairiscigh an tiománaí taispeána."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Lasc Ráta Athnuachana Uathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Athraigh ráta athnuachana an scáileáin go huathoibríoch bunaithe ar an ábhar reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "I Mód Lánscáileáin Eisiach Amháin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "I Mód Lánscáileáin Fuinneoige Amháin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Gach Mód Lánscáileáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Tairseach PAL Ráta Athnuachana Uathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "An ráta athnuachana uasta le meas mar PAL."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Ráta Athnuachana Ingearach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Socraigh ráta athnuachana ingearach an taispeántais. Cumasóidh '50 Hz' físeán réidh agus ábhar PAL á rith."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Díchumasaigh sRGB FBO le Fórsa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Díchumasaigh tacaíocht sRGB FBO go foréigneach. Bíonn fadhbanna físe ag roinnt tiománaithe Intel OpenGL ar Windows le sRGB FBOanna. Is féidir é seo a chumasú chun é a sheachaint."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Tosaigh i Mód Lánscáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Tosaigh i lánscáileán. Is féidir é seo a athrú ag am rithe. Is féidir é a shárú le lasc líne ordaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Mód Lánscáileáin Fuinneoige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Más lánscáileán atá i gceist, is fearr fuinneog lánscáileáin a úsáid chun cosc ​​a chur ar athrú mód taispeána."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Leithead Lánscáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Socraigh an méid leithead saincheaptha don mhodh lánscáileáin neamhfhuinneogach. Má fhágtar gan é socraithe, úsáidfear taifeach an deisce."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Airde Lánscáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Socraigh an méid airde saincheaptha don mhodh lánscáileáin neamhfhuinneogach. Má fhágtar gan é socraithe, úsáidfear taifeach an deisce."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Réiteach fórsa ar UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Fórsaigh an taifeach go méid an scáileáin iomláin, má shocraítear é go 0, úsáidfear luach seasta de 3840 x 2160."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Scála Fuinneogach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Socraigh méid na fuinneoige go dtí an t-iolraí sonraithe de mhéid an radhairc chroí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Teimhneacht na Fuinneoige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Socraigh trédhearcacht na fuinneoige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Maisiúcháin Fuinneoige Taispeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Taispeáin barra teidil agus teorainneacha na fuinneoige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Taispeáin Barra Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Taispeáin barra roghchláir na fuinneoige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Cuimhnigh Suíomh agus Méid na Fuinneoige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Taispeáin an t-ábhar go léir i bhfuinneog de mhéid socraithe de thoisí sonraithe ag 'Leithead na Fuinneoige' agus 'Airde na Fuinneoige', agus sábháil méid agus suíomh reatha na fuinneoige nuair a dhúnfar RetroArch. Nuair a bheidh sé díchumasaithe, socrófar méid na fuinneoige go dinimiciúil bunaithe ar 'Scála Fuinneoige'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Úsáid Méid Fuinneoige Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Taispeáin an t-ábhar go léir i bhfuinneog de mhéid socraithe de thoisí sonraithe ag 'Leithead na Fuinneoige' agus 'Airde na Fuinneoige'. Nuair a bhíonn sé díchumasaithe, socrófar méid na fuinneoige go dinimiciúil bunaithe ar 'Scála Fuinneoige'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Leithead na Fuinneoige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Socraigh an leithead saincheaptha don fhuinneog taispeána."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Airde na Fuinneoige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Socraigh an airde saincheaptha don fhuinneog taispeána."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Uasleithead na Fuinneoige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Socraigh uasleithead na fuinneoige taispeána nuair a athraítear méid go huathoibríoch bunaithe ar 'Scála Fuinneoige'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Airde Uasta na Fuinneoige"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Socraigh uasairde na fuinneoige taispeána nuair a athraítear méid go huathoibríoch bunaithe ar 'Scála Fuinneoige'."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Scála Slánuimhir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Scálú físeán i gcéimeanna slánuimhir amháin. Braitheann an méid bonn ar gheoiméadracht agus ar chóimheas gné a thuairiscítear sa croí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "Ais Scála Slánuimhir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Scálaigh airde nó leithead, nó an airde agus an leithead araon. Ní bhaineann leathchéimeanna ach le foinsí ardtaifigh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Scálú Scála Slánuimhreach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Babhtaigh síos nó suas go dtí an chéad slánuimhir eile. Titeann 'Cliste' go scála faoi bhun an scála nuair a bhíonn an íomhá bearrtha an iomarca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "Tearc-scála"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "Ró-scála"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Cliste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Cóimheas Gnéithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Socraigh cóimheas gné an taispeána."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Cóimheas Gné Cumraíochta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Luach snámhphointe don chóimheas gné físe (leithead / airde)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Cumraíocht"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Croí curtha ar fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Lán"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Coinnigh Cóimheas Gnéithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Coinnigh cóimheasa gné picteilín 1:1 agus ábhar á scálú tríd an IPU inmheánach. Má tá sé díchumasaithe, sínfear íomhánna chun an taispeáint ar fad a líonadh."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Cóimheas Gné Saincheaptha (Suíomh X)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Fritháireamh radhairc saincheaptha a úsáidtear chun suíomh ais-X an radhairc a shainiú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Cóimheas Gné Saincheaptha (Suíomh Y)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Fritháireamh radhairc saincheaptha a úsáidtear chun suíomh ais-Y an radhairc a shainiú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Claonadh Ancaire Radharcphoirt X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Claonadh Ancaire Radharcphoirt X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Claonadh Ancaire Radharcphoirt Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Claonadh Ancaire Radharcphoirt Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Suíomh cothrománach an ábhair nuair a bhíonn an radharcphort níos leithne ná leithead an ábhair. Tá 0.0 ar chlé go mór, tá 0.5 sa lár, agus tá 1.0 ar dheis go mór."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Suíomh ingearach an ábhair nuair a bhíonn an radharcphoirt níos airde ná airde an ábhair. Is é 0.0 an barr, 0.5 an lár, agus 1.0 an bun."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Claonadh Ancaire Radharcphoirt X (Treoshuíomh Portráide)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Claonadh Ancaire Radharcphoirt X (Treoshuíomh Portráide)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Claonadh Ancaire Radharcphoirt Y (Treoshuíomh Portráide)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Claonadh Ancaire Radharcphoirt Y (Treoshuíomh Portráide)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Suíomh cothrománach an ábhair nuair a bhíonn an radharcphoirt níos leithne ná leithead an ábhair. Tá 0.0 ar chlé go mór, tá 0.5 sa lár, tá 1.0 ar dheis go mór. (Treoshuíomh Portráide)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Suíomh ingearach an ábhair nuair a bhíonn an radharcphoirt níos airde ná airde an ábhair. Is é 0.0 an barr, 0.5 an lár, 1.0 an bun. (Treoshuíomh Portráide)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Cóimheas Gné Saincheaptha (Leithead)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Leithead radhairc saincheaptha a úsáidtear má tá an Cóimheas Gné socraithe go 'Cóimheas Gné Saincheaptha'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Cóimheas Gné Saincheaptha (Airde)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Airde radhairc saincheaptha a úsáidtear má tá an Cóimheas Gné socraithe go 'Cóimheas Gné Saincheaptha'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Ró-scanadh Bearrtha (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Gearr cúpla picteilín timpeall imill na híomhá a fhágtar bán de ghnáth ag forbróirí agus a mbíonn picteilíní neamhiomlána iontu uaireanta freisin."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "Cumasaigh HDR"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Cumasaigh HDR má thacaíonn an taispeáint leis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Lonracht Bhuaice"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Socraigh an gile buaic (i cd/m2) is féidir le do thaispeántas a atáirgeadh. Féach RTings le haghaidh gile buaic do thaispeántais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Lonrúlacht Bán Páipéir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Socraigh an gile ag a mbeidh bán páipéir i.e. téacs inléite nó gile ag barr an raoin SDR (Raon Dinimiciúil Caighdeánach). Úsáideach chun coigeartú a dhéanamh do dhálaí soilsithe éagsúla i do thimpeallacht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "Codarsnacht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "Rialú gama/chodarsnachta le haghaidh HDR. Glacann sé na dathanna agus méadaíonn sé an raon foriomlán idir na codanna is gile agus na codanna is dorcha den íomhá. Dá airde an codarsnacht HDR, is ea is mó an difríocht seo, agus dá ísle an codarsnacht, is ea is doiléire a bhíonn an íomhá. Cabhraíonn sé le húsáideoirí an íomhá a choigeartú dá dtaitneamh agus cad is fearr leo a fheiceáil ar a scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Leathnaigh Gamut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Nuair a bheidh an spás datha tiontaithe go spás líneach, cinntigh an gceart dúinn gamut datha leathnaithe a úsáid chun HDR10 a bhaint amach."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Sioncrónú Ingearach (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Sioncrónaigh físeán aschuir an chárta grafaicí le ráta athnuachana an scáileáin. Molta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Eatramh Malartaithe VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Úsáid eatramh malartaithe saincheaptha le haghaidh VSync. Laghdaíonn sé ráta athnuachana an mhonatóra go héifeachtach faoin bhfachtóir sonraithe. Socraíonn 'Auto' an fachtóir bunaithe ar an ráta fráma a thuairiscítear ag an gcroílár, rud a sholáthraíonn luas fráma feabhsaithe nuair a bhíonn ábhar 30 fps á rith ar thaispeántas 60 Hz nó ábhar 60 fps ar thaispeántas 120 Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Uathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "VSync Oiriúnaitheach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "Cumasaítear VSync go dtí go dtiteann an fheidhmíocht faoi bhun an ráta athnuachana sprice. Is féidir leis stad a chur ar an luas nuair a thiteann an fheidhmíocht faoi bhun an ráta fíor-ama, agus a bheith níos éifeachtúla ó thaobh fuinnimh de. Ní chomhoiriúnach le 'Moill Fráma'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Moill Fráma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Laghdaíonn sé moill ar chostas riosca níos airde go mbeidh stad ar an bhfíseán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Socraíonn sé cé mhéad milleasoicind le codladh sula ritheann an croí tar éis cur i láthair físe. Laghdaíonn sé moill ar chostas riosca níos airde stamair.\nDéileáiltear le luachanna 20 agus os a chionn mar chéatadáin ama fráma."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Moill Uathoibríoch Fráma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Coigeartaigh an 'Moill Fráma' éifeachtach go dinimiciúil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Déan iarracht an sprioc atá uait 'Moill Fráma' a choinneáil agus titim fráma a íoslaghdú. Is é an pointe tosaigh 3/4 d'am fráma nuair a bhíonn 'Moill Fráma' 0 (Uathoibríoch)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Uathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "éifeachtach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Sioncrónú Crua GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Sioncrónaigh an LAP agus an GPU go crua. Laghdaíonn sé moill ar chostas feidhmíochta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Frámaí Sioncrónaithe GPU Crua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Socraigh cé mhéad fráma is féidir leis an LAP a rith roimh an GPU agus 'Sincronú GPU Crua' in úsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Socraíonn sé cé mhéad fráma is féidir leis an LAP rith roimh an GPU nuair a úsáidtear 'Sioncrónú Crua GPU'. Is é 3 an t-uasmhéid.\n 0: Sioncrónaigh leis an GPU láithreach.\n 1: Sioncrónaigh leis an bhfráma roimhe seo.\n 2: srl ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Sioncrónaigh le Ráta Fráma Ábhar Beacht (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Gan aon diall ón am a iarradh sa croí. Úsáid le haghaidh scáileáin Ráta Athnuachana Athraitheach (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Sioncrónaigh le Ráta Fráma Beacht an Ábhair. Is ionann an rogha seo agus luas x1 a fhorchur agus luas ar aghaidh á cheadú fós. Gan aon diall ón ráta athnuachana croí a iarradh, gan aon fhuaim i Rialú Ráta Dinimiciúil."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Aschur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Athraigh socruithe aschuir fuaime."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Micreafón"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Athraigh socruithe ionchuir fuaime."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Athshamplóir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Athraigh socruithe athshamplála fuaime."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Sioncrónú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Athraigh socruithe sioncrónaithe fuaime."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Athraigh socruithe MIDI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Meascthóir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Athraigh socruithe meascthóra fuaime."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Fuaimeanna Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Athraigh socruithe fuaime an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Balbhaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Balbhaigh an fhuaim."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Meascthóir Balbhaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Muistigh fuaim an mheascthóra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Meas ar Mhód Ciúin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Balbhaigh an fhuaim go léir i Mód Ciúin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Fuaim Mhéadaithe Ar Aghaidh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Balbhaigh an fhuaim go huathoibríoch agus tú ag úsáid luasghéarú ar aghaidh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Luasghéarú Fuaime Ar Aghaidh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Luasaigh an fhuaim agus tú ag luasghéarú ar aghaidh. Coscann sé scoilteadh ach athraíonn sé an pháirc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Athchasadh Fuaime Balbhaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Balbhaigh an fhuaim go huathoibríoch agus tú ag úsáid aischasadh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Gnóthachan Toirte (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Toirt fuaime (i dB). Is gnáth-thoirt é 0 dB, agus ní chuirtear aon ghnóthachan i bhfeidhm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Toirt fuaime, léirithe i dB. Is gnáth-thoirt é 0 dB, áit nach gcuirtear aon ghnóthachan i bhfeidhm. Is féidir an gnóthachan a rialú le linn an ama rite le Toirt Ionchuir Suas / Toirt Ionchuir Síos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Gnóthachan Toirte Meascthóra (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Toirt dhomhanda an mheascthóra fuaime (i dB). Is gnáth-thoirt é 0 dB, agus ní chuirtear aon ghnóthachan i bhfeidhm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Breiseán DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Breiseán DSP Fuaime a phróiseálann fuaim sula seoltar chuig an tiománaí é."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Bain Breiseán DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Díluchtaigh aon bhreiseán DSP fuaime gníomhach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Mód Eisiach WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Lig don tiománaí WASAPI smacht eisiach a ghlacadh ar an ngléas fuaime. Má tá sé díchumasaithe, úsáidfidh sé mód comhroinnte ina ionad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Formáid Snámh WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Bain úsáid as an bhformáid snámhphointe don tiománaí WASAPI, má thacaíonn do ghléas fuaime leis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Fad Maoláin Chomhroinnte WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "An fad maoláin idirmheánach (i bhfrámaí) agus an tiománaí WASAPI in úsáid i mód comhroinnte."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Fuaim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Cumasaigh aschur fuaime."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Gléas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Sáraigh an gléas fuaime réamhshocraithe a úsáideann an tiománaí fuaime. Braitheann sé seo ar an tiománaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Sáraigh an gléas fuaime réamhshocraithe a úsáideann an tiománaí fuaime. Braitheann sé seo ar an tiománaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Luach gléas PCM saincheaptha don tiománaí ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Luach cosáin saincheaptha don tiománaí OSS (m.sh. /dev/dsp)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Luach ainm calafoirt saincheaptha don tiománaí JACK (m.sh. córas:athsheinm1, córas:athsheinm_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Seoladh IP saincheaptha freastalaí RSound don tiománaí RSound."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Moill Fuaime (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Uasmhéid moille fuaime i milleasoicindí. Tá sé mar aidhm ag an tiománaí an moille iarbhír a choinneáil ag 50% den luach seo. B’fhéidir nach ndéanfar é a chomhlíonadh mura féidir leis an tiománaí fuaime an moille shonraithe a sholáthar."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Micreafón"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Cumasaigh ionchur fuaime i gcroílár tacaithe. Níl aon fhorchostais ann mura bhfuil micreafón á úsáid ag an croí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Gléas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Sáraigh an gléas ionchuir réamhshocraithe a úsáideann tiománaí an mhicreafóin. Braitheann sé seo ar an tiománaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Sáraigh an gléas ionchuir réamhshocraithe a úsáideann tiománaí an mhicreafóin. Braitheann sé seo ar an tiománaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Cáilíocht Athshamplála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Ísligh an luach seo chun feidhmíocht/moill níos ísle a chur chun cinn thar cháilíocht fuaime, méadaigh é chun cáilíocht fuaime níos fearr a fháil ar chostas feidhmíochta/moill níos ísle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Ráta Ionchuir Réamhshocraithe (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Ráta samplach ionchuir fuaime, a úsáidtear mura n-iarrann croí uimhir shonrach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Moill Ionchuir Fuaime (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "An moill ionchuir fuaime atá ag teastáil i milleasoicindí. B’fhéidir nach nglacfar leis mura féidir leis an tiománaí micreafóin an moill shonraithe a sholáthar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Mód Eisiach Baile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Lig do RetroArch smacht eisiach a ghlacadh ar an ngléas micreafóin agus tiománaí micreafóin WASAPI á úsáid. Má tá sé díchumasaithe, úsáidfidh RetroArch mód comhroinnte ina ionad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Formáid Snámh WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Bain úsáid as ionchur snámhphointe don tiománaí WASAPI, má thacaíonn do ghléas fuaime leis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Fad Maoláin Chomhroinnte WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "An fad maoláin idirmheánach (i bhfrámaí) agus an tiománaí WASAPI in úsáid i mód comhroinnte."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Cáilíocht Athshamplála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Ísligh an luach seo chun feidhmíocht/moill níos ísle a chur chun cinn thar cháilíocht fuaime, méadaigh é chun cáilíocht fuaime níos fearr a fháil ar chostas feidhmíochta/moill níos ísle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Ráta Aschuir (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Ráta samplach aschuir fuaime."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Sioncrónú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Sioncrónaigh fuaim. Molta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Uasmhéid Sceabha Ama"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "An t-athrú uasta ar an ráta ionchuir fuaime. Má mhéadaítear an ráta seo, is féidir athruithe an-mhóra a dhéanamh ar an am ar chostas páirc fuaime míchruinn (m.sh. croíthe PAL a rith ar thaispeántais NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Uasmhéid claonta ama fuaime.\nSainmhíníonn sé seo an t-athrú uasta ar an ráta ionchuir. B’fhéidir gur mhaith leat é seo a mhéadú chun athruithe an-mhóra ar an am a chumasú, mar shampla croíthe PAL a rith ar thaispeántais NTSC, ar chostas páirc fuaime míchruinn.\nSainmhínítear an ráta ionchuir mar:\nráta ionchuir * (1.0 +/- (uasmhéid claonta ama))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Rialú Ráta Fuaime Dinimiciúil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Cabhraíonn sé le lochtanna san am a mhaolú agus fuaim agus físeán á sioncrónú. Bíodh a fhios agat, mura bhfuil sé sin ar fáil, go bhfuil sé beagnach dodhéanta sioncrónú ceart a bhaint amach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Má shocraítear seo go 0, díchumasaítear rialú ráta. Rialaíonn aon luach eile deilte rialaithe ráta fuaime.\nSainmhíníonn sé cé mhéad ráta ionchuir is féidir a choigeartú go dinimiciúil. Sainmhínítear ráta ionchuir mar:\nráta ionchuir * (1.0 +/- (deilte rialaithe ráta))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Ionchur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Roghnaigh gléas ionchuir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Socraíonn sé an gléas ionchuir (sainiúil don tiománaí). Nuair a shocraítear é go 'As', díchumasófar ionchur MIDI. Is féidir ainm an ghléis a chlóscríobh freisin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Aschur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Roghnaigh gléas aschuir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Socraíonn sé an gléas aschuir (sainiúil don tiománaí). Nuair a shocraítear é go 'As', díchumasófar aschur MIDI. Is féidir ainm an ghléis a chlóscríobh freisin.\nNuair a bhíonn aschur MIDI cumasaithe agus nuair a thacaíonn an croí agus an cluiche/aip le haschur MIDI, ginfidh an gléas MIDI cuid de na fuaimeanna nó na fuaimeanna uile (ag brath ar an gcluiche/aip). I gcás tiománaí MIDI 'null', ciallaíonn sé seo nach mbeidh na fuaimeanna sin inchloiste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Toirt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Socraigh an toirt aschuir (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Sruth meascthóra #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Seinn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Tosóidh sé ag athsheinm an tsrutha fuaime. Nuair a bheidh sé críochnaithe, bainfear an sruth fuaime reatha as an gcuimhne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Seinn (Lúbtha)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Tosóidh sé ag athsheinm an tsrutha fuaime. Nuair a bheidh sé críochnaithe, déanfaidh sé lúb agus seinnfidh sé an rian arís ón tús."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Seinn (Seicheamhach)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Tosóidh sé ag athsheinm an tsrutha fuaime. Nuair a bheidh sé críochnaithe, léimfidh sé go dtí an chéad sruth fuaime eile in ord seicheamhach agus athdhéanamh sé an t-iompar seo. Úsáideach mar mhodh athsheinm albaim."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Stad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Cuirfidh sé seo stop le hathsheinm an tsrutha fuaime, ach ní bhainfidh sé as an gcuimhne é. Is féidir é a thosú arís trí 'Seinn' a roghnú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Bain"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Cuirfidh sé seo stop le hathsheinm an tsrutha fuaime agus bainfidh sé go hiomlán as an gcuimhne é."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Toirt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Coigeartaigh toirt an tsrutha fuaime."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Stát: N/B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Stát: Stoptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Stát: Ag seinm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Stát: Ag seinm (Lúbtha)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Stádas: Ag seinm (Seicheamhach)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Meascthóir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Seinn sruthanna fuaime comhuaineacha fiú sa roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Cumasaigh Fuaim 'Ceart go leor'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Cumasaigh Fuaim 'Cealaigh'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Cumasaigh Fuaim 'Fógra'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Cumasaigh Fuaim 'BGM'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Cumasaigh Fuaimeanna 'Scrollaigh'"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Uasmhéid Úsáideoirí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Uasmhéid úsáideoirí a dtacaítear leo le RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Iompar Vótaíochta (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Bíonn tionchar aige ar an gcaoi a ndéantar pobalbhreith ionchuir i RetroArch. Má shocraítear é go 'Luath' nó 'Déanach', is féidir go mbeidh moill níos lú ann, ag brath ar do chumraíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Bíonn tionchar aige ar an gcaoi a ndéantar pobalbhreith ionchuir laistigh de RetroArch.\nLuath - Déantar pobalbhreith ionchuir sula ndéantar an fráma a phróiseáil.\nGnáth - Déantar pobalbhreith ionchuir nuair a iarrtar pobalbhreith.\nDéanach - Déantar pobalbhreith ionchuir ar an gcéad iarratas stádais ionchuir in aghaidh an fhráma.\nMá shocraítear é go 'Luath' nó 'Déanach', is féidir go mbeidh moill níos lú mar thoradh air, ag brath ar do chumraíocht. Déanfar neamhaird de[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Athmhapáil Rialuithe don Croíthe seo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Sáraigh na ceangail ionchuir leis na ceangail athmhapáilte atá socraithe don croí reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Sórtáil Athléarscáileanna de réir Gamepad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Ní bheidh feidhm ag athmhapálacha ach amháin maidir leis an gamepad gníomhach inar sábháladh iad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Cumraíocht uathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Cumraíonn sé go huathoibríoch rialtóirí a bhfuil próifíl acu, stíl Breiseán-agus-Súgartha."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Díchumasaigh Eochracha Te Windows (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Coinnigh teaglamaí eochracha Win taobh istigh den fheidhmchlár."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Roghnaigh méarchlár fisiceach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Bain úsáid as an ngléas seo mar mhéarchlár fisiceach agus ní mar gamepad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Má aithníonn RetroArch méarchlár crua-earraí mar chineál éigin gamepad, is féidir an socrú seo a úsáid chun RetroArch a chur iallach an gléas mí-aitheanta a láimhseáil mar mhéarchlár. \nIs féidir leis seo a bheith úsáideach má tá tú ag iarraidh ríomhaire a aithris i ngléas Android TV agus má tá méarchlár fisiceach agat ar féidir é a cheangal leis an mbosca."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Ionchur Braiteora Cúnta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Cumasaigh ionchur ó bhraiteoirí luasghéaraithe, giroscóp agus soilsithe, má thacaíonn an crua-earraí reatha leis. D’fhéadfadh tionchar a bheith aige ar fheidhmíocht agus/nó d’fhéadfadh sé go méadófaí an draenáil cumhachta ar roinnt ardán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Greim Uathoibríoch Luiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Cumasaigh greim luiche ar fhócas an fheidhmchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Cumasaigh Mód 'Fócas Cluiche' go huathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Cumasaigh mód 'Fócas Cluiche' i gcónaí agus ábhar á sheoladh agus á atosú. Nuair a shocraítear é go 'Braith', cuirfear an rogha ar siúl má chuireann an croí reatha feidhmiúlacht athghlaoite méarchláir tosaigh i bhfeidhm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Braith"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Cuir an t-ábhar ar sos nuair a dhícheanglaíonn an rialtóir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Cuir ábhar ar sos nuair a bhíonn aon rialtóir dícheangailte. Lean ar aghaidh le Tosaigh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Tairseach Ais Cnaipe Ionchuir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Cé chomh fada is gá ais a chlaonadh chun go mbrúfar cnaipe agus 'Analógach go Digiteach' in úsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Crios Marbh Analógach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Déan neamhaird de ghluaiseachtaí bata analógacha faoi bhun luach an chrios marbh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Íogaireacht Analógach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Coigeartaigh íogaireacht na bataí analógacha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Am Teorann Ceangail"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Líon na soicindí le fanacht sula dtéann tú ar aghaidh go dtí an chéad cheangal eile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Ceangail Coinnigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Líon na soicindí chun ionchur a shealbhú chun é a cheangal."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Brúigh an méarchlár, an luch nó an rialtóir"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Scaoil eochracha agus cnaipí!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Am críochnaithe"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Coinnigh"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Tine Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE,
   "Cuireann díchumasaithe stad ar gach oibríocht tine turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Tréimhse Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "An tréimhse i bhfrámaí nuair a bhrúitear cnaipí turbo-chumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DUTY_CYCLE,
   "Timthriall Dualgais Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE,
   "Líon na bhfrámaí ón Tréimhse Turbo a bhfuil na cnaipí á gcoinneáil síos. Mura bhfuil an uimhir seo cothrom leis an Tréimhse Turbo nó níos mó ná í, ní scaoilfear na cnaipí choíche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF,
   "Leaththréimhse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Mód Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Roghnaigh iompar ginearálta an mhodha turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Clasaiceach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Clasaiceach (Tógáil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Cnaipe Aonair (Tógáil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Cnaipe Aonair (Coinnigh)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Mód clasaiceach, oibriú dhá chnaipe. Coinnigh cnaipe síos agus tapáil an cnaipe Turbo chun an seicheamh brú-scaoilte a ghníomhachtú. \nIs féidir ceangal Turbo a shannadh i Socruithe/Ionchur/Rialuithe Port X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Mód scoránaigh clasaiceach, oibriú dhá chnaipe. Coinnigh cnaipe síos agus tapáil an cnaipe Turbo chun turbo a chumasú don chnaipe sin. Chun turbo a dhíchumasú: coinnigh an cnaipe síos agus brúigh an cnaipe Turbo arís. \nIs féidir ceangal Turbo a shannadh i Socruithe/Ionchur/Rialuithe Port X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Mód scoránaigh. Brúigh an cnaipe Turbo uair amháin chun an seicheamh brú-scaoilte a ghníomhachtú don chnaipe réamhshocraithe roghnaithe, brúigh arís é chun é a mhúchadh. Is féidir ceangal \nTurbo a shannadh i Socruithe/Ionchur/Rialuithe Port X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Mód coinnigh. Bíonn an seicheamh brú-scaoilte don chnaipe réamhshocraithe roghnaithe gníomhach chomh fada agus a choinnítear an cnaipe Turbo síos.\nIs féidir ceangal Turbo a shannadh i Socruithe/Ionchur/Rialuithe Port X.\nChun feidhm uath-tine ré an ríomhaire baile a aithris, socraigh Ceangail agus Cnaipe don chnaipe tine luamhán stiúrtha céanna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "Ceangal Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "Turbo ag gníomhú ceangal RetroPad. Úsáideann Folamh an ceangal atá sainiúil don chalafort."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "Cnaipe Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "Sprioc cnaipe turbo i mód 'Cnaipe Aonair'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "Ceadaigh Turbo Treoracha D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "Más cumasaithe é, is féidir ionchuir threoracha digiteacha (ar a dtugtar d-pad nó 'hatswitch' freisin) a bheith turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Tine Turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Athraigh socruithe tine turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Aiseolas/Creathadh Haptic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Athraigh socruithe aiseolais haptic agus creatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Rialuithe Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Athraigh socruithe rialaithe roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Eochracha Te"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Athraigh socruithe agus sannadh do theochracha, amhail an roghchlár a athrú le linn an chluiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Ceanglaíonn RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Athraigh an chaoi a mapáiltear an RetroPad fíorúil chuig gléas ionchuir fisiceach. Mura n-aithnítear gléas ionchuir agus má dhéantar é a uathchumrú i gceart, is dócha nach gá d'úsáideoirí an roghchlár seo a úsáid.\nTabhair faoi deara: le haghaidh athruithe ionchuir croí-shonracha, bain úsáid as fo-roghchlár 'Rialuithe' an Roghchláir Thapa ina ionad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Úsáideann Libretro teibí gamepad fíorúil ar a dtugtar an 'RetroPad' chun cumarsáid a dhéanamh ó na tosaigh (cosúil le RetroArch) go croíthe agus a mhalairt. Cinneann an roghchlár seo conas a mhapáiltear an RetroPad fíorúil chuig na gléasanna ionchuir fisiciúla agus cé na calafoirt ionchuir fhíorúla a áitíonn na gléasanna seo.\nMura n-aithnítear gléas ionchuir fisiciúil agus má dhéantar é a uathchumrú i gceart, is dócha nach gá d'úsáideoirí an roghchlár seo a ú[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Rialuithe Port %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Athraigh an chaoi a mapáiltear an RetroPad fíorúil chuig do ghléas ionchuir fisiceach don chalafort fíorúil seo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Athraigh mapálacha ionchuir croí-shonracha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Réiteach ar dhícheangal Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Réiteach sealadach ar rialtóirí ag dícheangal agus ag athcheangal. Cuireann sé bac ar 2 imreoir leis na rialtóirí céanna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Deimhnigh Scoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "Éilítear an eochair the Scoir a bhrú faoi dhó chun RetroArch a scor."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Creathadh ar Bhrú Eochrach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Cumasaigh Creathadh Gléas (Do Croíthe Tacaithe)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Neart Chreathadh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Sonraigh méid na n-éifeachtaí aiseolais haptic."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Rialuithe Roghchláir Aontaithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Úsáid na rialuithe céanna don roghchlár agus don chluiche araon. Baineann sé seo leis an méarchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Díchumasaigh an Cnaipe Eolais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Cosc a chur ar fheidhm eolais an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Díchumasaigh an Cnaipe Cuardaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Cosc a chur ar fheidhm chuardaigh an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Díchumasaigh Analógach Clé sa Roghchlár"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Cosc a chur ar ionchur bata analógach clé an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Díchumasaigh Analógach Deas sa Roghchlár"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Cosc a chur ar ionchur ón mbata analógach ar dheis sa roghchlár. Athraíonn an bata analógach ar dheis mionsamhlacha i seinmliostaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Cnaipí Malartaithe Roghchláir Ceart go leor agus Cealaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Malartaigh cnaipí le haghaidh Ceart go leor/Cealaigh. Is é díchumasaithe treoshuíomh na gcnaipí Seapánacha, is é cumasaithe treoshuíomh an iarthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Cnaipí Scrollaigh Malartaithe Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Malartaigh cnaipí le haghaidh scrollú. Scrollaíonn díchumasaithe 10 mír le C/D agus in ord aibítre le C2/D2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Roghchlár Rialaithe Gach Úsáideoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Lig d'aon úsáideoir an roghchlár a rialú. Mura bhfuil sé seo indéanta, ní féidir ach le hÚsáideoir 1 an roghchlár a rialú."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Cumasaigh Eochracha Te"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Nuair a shanntar í, ní mór an eochair 'Cumasaigh Eochracha Te' a choinneáil síos sula n-aithnítear aon eochracha te eile. Ligeann sé seo do chnaipí rialtóra a bheith mapáilte chuig feidhmeanna eochracha te gan cur isteach ar an ngnáthionchur. Má shanntar an modhnóir don rialtóir amháin, ní bheidh sé ag teastáil le haghaidh eochracha te méarchláir, agus a mhalairt, ach oibríonn an dá mhodhnóir don dá fheiste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Má tá an eochair the seo ceangailte le méarchlár, cnaipe luamháin nó joyaxis, díchumasófar na heochair the eile go léir mura gcoimeádtar an eochair the seo síos ag an am céanna.\nTá sé seo úsáideach le haghaidh cur i bhfeidhm RETRO_KEYBOARD-lárnaithe a dhéanann fiosrúcháin ar limistéar mór den mhéarchlár, áit nach bhfuil sé inmhianaithe go mbeadh eochracha te ag cur isteach ar an mbealach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Moill Cumasaithe Eochracha Te (Frámaí)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Cuir moill leis sna frámaí sula gcuirtear bac ar ionchur gnáth tar éis brú a chur ar an eochair 'Cumasaigh Eochracha Te' atá sannta. Ceadaíonn sé seo ionchur gnáth ón eochair 'Cumasaigh Eochracha Te' a ghabháil nuair a mhapáiltear é chuig gníomh eile (m.sh. 'Roghnaigh' RetroPad)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Cineál Gléas Eochrach Te Cumaisc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Blocáil gach eochair the ó chineálacha gléas méarchláir agus rialtóra araon má tá 'Cumasaigh Eochracha Te' socraithe do cheachtar cineál."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Roghchlár Scoránaigh (Teaglaim Rialaitheora)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Teaglaim cnaipí rialtóra chun an roghchlár a athrú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Roghchlár Scoránaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Athraíonn sé an taispeáint reatha idir an roghchlár agus an t-ábhar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Scoir (Teaglaim Rialaitheora)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Teaglaim cnaipí rialtóra chun RetroArch a scor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Scoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Dúnann sé RetroArch, ag cinntiú go ndéantar na sonraí sábhála agus na comhaid chumraíochta go léir a shruthlú chuig an diosca."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Dúnann sé an t-ábhar reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Athshocraigh an t-ábhar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Atosaíonn sé an t-ábhar reatha ón tús."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Mear-Ar Aghaidh (Tógáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Athraíonn idir luas ar aghaidh agus gnáthluas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Tapáil Ar Aghaidh (Coinnigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Cumasaíonn sé luasghéarú nuair a choimeádtar an eochair. Ritheann an t-ábhar ar luas gnáth nuair a scaoiltear an eochair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Gluaiseacht Mhall (Tógáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Athraíonn idir luas mall agus gnáthluas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Gluaiseacht Mhall (Coinnigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Cumasaíonn sé gluaiseacht mhall nuair a choimeádtar é. Ritheann an t-ábhar ar luas gnáth nuair a scaoiltear an eochair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Athchasadh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Athchasann sé an t-ábhar reatha agus an eochair á coinneáil síos. Ní mór 'Tacaíocht Athchasála' a bheith cumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Sos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Athraíonn sé ábhar idir stáit sosaithe agus neamhshosaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Réamhrá Fráma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Cuireann sé an t-ábhar ar aghaidh fráma amháin nuair a chuirtear ar sos é."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Fuaim Mhúchta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Casann sé aschur fuaime air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Ardaigh an Toirt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Méadaíonn sé leibhéal toirte fuaime aschuir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Imleabhar Síos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Laghdaíonn sé leibhéal toirte fuaime aschuir."
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Luchtaíonn sé an stát sábháilte ón sliotán atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Sábhálann sé an stát sa sliotán atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Sliotán Stáit Sábháil Eile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Méadaíonn sé innéacs an tsliotáin stáit sábhála atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Sliotán Stáit Sábháilte Roimhe Seo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Laghdaíonn sé innéacs an sliotáin stáit sábhála atá roghnaithe faoi láthair."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Díbirt Diosca (Tógáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Má tá an tráidire diosca fíorúil dúnta, osclaítear é agus baintear an diosca luchtaithe as. Seachas sin, cuirtear an diosca atá roghnaithe isteach agus dúntar an tráidire."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "An chéad Diosca eile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Méadaíonn sé innéacs an diosca atá roghnaithe faoi láthair. Ní mór an tráidire diosca fíorúil a bheith oscailte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Diosca Roimhe Seo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Laghdaíonn sé innéacs an diosca atá roghnaithe faoi láthair. Ní mór an tráidire diosca fíorúil a bheith oscailte."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Scáthóirí (Tógáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Casann sé an scáthlán atá roghnaithe faoi láthair air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_HOLD,
   "Scáthóirí (Coinnigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD,
   "Coinníonn sé an scáthlán atá roghnaithe faoi láthair ar siúl/as agus an eochair brúite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "An Chéad Scáthadóir Eile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Luchtaíonn agus cuireann sé an chéad chomhad réamhshocraithe scáthaithe eile i bhfréamh an eolaire 'Video Shaders' i bhfeidhm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Scáthadóir Roimhe Seo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Luchtaíonn agus cuireann sé an comhad réamhshocraithe scáthaithe roimhe seo i bhfréamh an eolaire 'Video Shaders' i bhfeidhm."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Cleasanna (Tóglaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Casann sé an cheat atá roghnaithe faoi láthair air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Innéacs Cheat Eile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Méadaíonn sé an t-innéacs meabhlaireachta atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Innéacs Cheat Roimhe Seo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Laghdaíonn sé an t-innéacs meabhlaireachta atá roghnaithe faoi láthair."
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Gabhann sé íomhá den ábhar reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Taifeadadh (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Tosaíonn/stopann sé taifeadadh an tseisiúin reatha chuig comhad físe áitiúil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Sruthú (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Tosaíonn/stopann sé sruthú an tseisiúin reatha chuig ardán físe ar líne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Seinn Athsheinm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Seinn comhad athsheinm ón sliotán atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Athsheinm Taifeadta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Taifead comhad athsheinm chuig an sliotán atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Stop Taifeadadh/Athsheinm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Stopann sé taifeadadh/sheinm an athsheinm reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "An chéad sliotán athimeartha eile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Méadaíonn sé innéacs an sliotán athimeartha atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Sliotán Athimeartha Roimhe Seo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Laghdaíonn sé innéacs an sliotán athimeartha atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_TURBO_FIRE_TOGGLE,
   "Tine Turbo (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE,
   "Casann sé tine turbo air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Greim Luiche (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Greimíonn nó scaoileann sé an luch. Nuair a ghreimítear air, bíonn cúrsóir an chórais i bhfolach agus teoranta don fhuinneog taispeána RetroArch, rud a fheabhsaíonn ionchur coibhneasta na luiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Fócas Cluiche (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Cuireann sé seo an modh 'Fócas Cluiche' ar siúl/as. Nuair a bhíonn fócas ar an ábhar, díchumasaítear na te-eochracha (cuirtear ionchur iomlán an mhéarchláir chuig an gcroílár atá ag rith) agus glactar leis an luch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Lánscáileán (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Athraíonn sé idir mód taispeána lánscáileáin agus fuinneoige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Roghchlár Deisce (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Osclaíonn sé comhéadan úsáideora deisce WIMP (Windows, Deilbhíní, Biachláir, Pointeoir)."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Sioncrónaigh le Ráta Fráma Ábhar Beacht (Scóránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Athraíonn sé sioncrónú le ráta fráma beacht an ábhair ar siúl/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Rith Chun Tosaigh (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Casann sé Run-Ahead air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Frámaí Réamhghníomhacha (Scóránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Casann sé Frámaí Réamhghníomhacha air/as."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Taispeáin FPS (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Casann sé an táscaire stádais 'frámaí in aghaidh an tsoicind' air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Taispeáin Staitisticí Teicniúla (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Casann sé taispeántas staitisticí teicniúla ar an scáileán air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Forleagan Méarchláir (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Casann sé forleagan méarchláir air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "An Chéad Fhorleagan Eile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Athraíonn sé go dtí an chéad leagan amach eile atá ar fáil den fhorleagan ar an scáileán atá gníomhach faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Seirbhís AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Gabhann sé íomhá den ábhar reatha chun aon téacs ar an scáileán a aistriú agus/nó a léamh os ard. Ní mór 'Seirbhís AI' a chumasú agus a chumrú."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Ping Líonra (Scóránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Casann sé seo an cuntar ping don seomra líonra reatha air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Óstáil Netplay (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Casann sé óstáil netplay air/as."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Mód Súgartha/Breathnóireachta Netplay (Scoránaigh)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Athraíonn sé an seisiún reatha líonraithe idir na modhanna 'imirt' agus 'féachaint'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Comhrá seinnteoir Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Seolann sé teachtaireacht comhrá chuig an seisiún reatha ar an ngréasán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Comhrá Céimnithe Netplay (Toggle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Athraigh idir teachtaireachtaí comhrá netplay atá ag céimniú agus statach."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Cineál Gléas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Sonraíonn sé an cineál rialtóra aithriste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Cineál Analógach go Digiteach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Bain úsáid as an maide analógach sonraithe le haghaidh ionchuir D-Pad. Sáraíonn modhanna 'éigeantacha' croí ionchur analógach dúchais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Mapáil bata analógach sonraithe le haghaidh ionchur D-Pad.\nMá tá tacaíocht dhúchasach analógach ag an croí, díchumasófar mapáil D-Pad mura roghnaítear rogha '(Éignithe)'.\nMá dhéantar mapáil D-Pad a éigeantú, ní bhfaighidh an croílár aon ionchur analógach ón mbata sonraithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Innéacs na nGléasanna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "An rialtóir fisiceach mar a aithníonn RetroArch é."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Gléas In Áirithe don Imreoir seo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Déanfar an rialtóir seo a leithdháileadh don imreoir seo, de réir an mhodha áirithinte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "Gan Áirithint"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "Rogha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "In áirithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "Cineál Áirithinte Gléas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "Rogha: má tá gléas sonraithe i láthair, leithdháilfear don imreoir seo é. In áirithe: ní leithdháilfear aon rialtóir eile don imreoir seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Calafort Mapeáilte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Sonraítear cén croí phort a gheobhaidh ionchur ó phort rialtóra tosaigh %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Socraigh Gach Rialú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Sannadh na treoracha agus na cnaipí uile, ceann i ndiaidh a chéile, san ord ina bhfuil siad le feiceáil sa roghchlár seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Athshocraigh chuig Rialuithe Réamhshocraithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Glan socruithe ceangail ionchuir chuig a luachanna réamhshocraithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Sábháil Próifíl an Rialaitheora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Sábháil comhad uathchumraíochta a chuirfear i bhfeidhm go huathoibríoch aon uair a bhraitear an rialtóir seo arís."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Innéacs Luiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "An luch fhisiciúil mar a aithníonn RetroArch í."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Cnaipe B (Síos)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Cnaipe Y (Ar Chlé)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Roghnaigh Cnaipe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Cnaipe Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "D-Pad Suas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "D-Pad Síos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "D-Pad Ar Chlé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "D-Pad Ar Dheis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Cnaipe A (Ar Dheis)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Cnaipe X (Barr)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "Cnaipe L (Gualainn)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "Cnaipe R (Gualainn)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "Cnaipe L2 (Spreagthóir)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "Cnaipe R2 (Spreagthóir)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "Cnaipe L3 (Ordóg)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "Cnaipe R3 (Ordóg)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Analógach Clé X+ (Deas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Analógach Clé X- (Clé)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Analógach Clé Y+ (Síos)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Analógach Clé Y- (Suas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Analógach Deas X+ (Ar Dheis)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Analógach Deas X- (Clé)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Analógach Deas Y+ (Síos)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Analógach Deas Y- (Suas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Truicear Gunna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Athluchtú Gunna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Gunna Aux A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Gunna Aux B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Gunna Aux C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Tús Gunna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Roghnaigh Gunna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Gunna D-Pad Suas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Gunna D-Pad Síos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Gunna D-Pad ar chlé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Gunna D-Pad Ar dheis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO,
   "Tine Turbo"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Rith Chun Tosaigh Gan Fáil]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "Níl an croí reatha comhoiriúnach le rith chun tosaigh mar gheall ar easpa tacaíochta cinntitheach do staid sábhála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE,
   "Rith Chun Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Líon na bhFrámaí le Rith Chun Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Líon na bhfrámaí le rith chun tosaigh. Is cúis le fadhbanna gameplay amhail crith má sháraítear líon na bhfrámaí moille inmheánacha don chluiche."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "Rith loighic chroí bhreise chun an mhoill a laghdú. Rith Aon-Cheist chuig fráma sa todhchaí, agus ansin athlódálann sé an staid reatha. Coinníonn an Dara Ceist croí-cheanglas físe amháin ag fráma sa todhchaí chun fadhbanna staid fuaime a sheachaint. Rith Frámaí Réamhghníomhacha thar fhrámaí le hionchur nua nuair is gá, ar mhaithe le héifeachtúlacht."
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Rith loighic chroí bhreise chun moill a laghdú. Rith Aon-Chás chuig fráma sa todhchaí, ansin athlódálann sé an staid reatha. Rith Frámaí Réamhghníomhacha thar fhrámaí le hionchur nua nuair is gá, ar mhaithe le héifeachtúlacht."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "Mód Cás Aonair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "Mód Cás Dara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   "Mód Frámaí Réamhghníomhacha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Folaigh Rabhaidh Rith Chun Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Folaigh an teachtaireacht rabhaidh a thaispeántar agus Run-Ahead á úsáid agus ní thacaíonn an croí le stáit sábhála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Líon na bhFrámaí Réamhghníomhacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Líon na bhfrámaí le hathchraoladh. Is cúis le fadhbanna cluiche amhail crith má sháraítear líon na bhfrámaí moille atá inmheánach don chluiche."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Comhthéacs Comhroinnte Crua-earraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Tabhair a gcomhthéacs príobháideach féin do croíthe a rindreáiltear le crua-earraí. Seachnaíonn sé seo an gá glacadh le hathruithe ar staid crua-earraí idir frámaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Ceadaigh do Croíthe an Tiománaí Físeáin a Athrú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Lig do croíthe aistriú go tiománaí físe difriúil ón gceann atá luchtaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Luchtaigh an Bréagán ar Dhúnadh an Chroí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Tá gné múchta ag roinnt croíthe, agus cuirfidh luchtú croíthe fhals cosc ​​ar RetroArch múchadh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "D’fhéadfadh gné múchta a bheith ag roinnt croíthe. Mura bhfuil an rogha seo ar fáil, má roghnaítear an nós imeachta múchta, múchfar RetroArch.\nMá chumasaítear an rogha seo, luchtófar croíthe bréige ina ionad ionas go bhfanfaimid laistigh den roghchlár agus ní mhúchfar RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Tosaigh Croíthe go hUathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Seiceáil le haghaidh Firmware atá ar Iarraidh Sula Luchtaítear"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Seiceáil an bhfuil an dochtearraí riachtanach go léir i láthair sula ndéanann tú iarracht ábhar a luchtú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE,
   "D’fhéadfadh go mbeadh comhaid dochtearraí nó bios ag teastáil ó roinnt croíthe. Mura bhfuil an rogha seo cumasaithe, ní cheadóidh RetroArch an croí a thosú mura bhfuil aon mhíreanna éigeantacha dochtearraí ar iarraidh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Catagóirí Rogha Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Lig do croíthe roghanna a chur i láthair i bhfo-roghchláir bunaithe ar chatagóirí. TABHAIR FAOI DEARA: Ní mór croíthe a athlódáil le go dtiocfaidh athruithe i bhfeidhm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Comhaid Faisnéise Croí-Taisce"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Coimeád taisce áitiúil leanúnach de croí fhaisnéis suiteáilte. Laghdaíonn sé go mór amanna lódála ar ardáin a bhfuil rochtain mall diosca acu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Seachbhóthar na Faisnéise Croí Sábháil Gnéithe Stáit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Sonraíonn sé seo an ndéanfar neamhaird de chumais stáit sábhála faisnéise croí, rud a ligeann duit turgnamh a dhéanamh le gnéithe gaolmhara (rith ar aghaidh, siarchasadh, srl.)."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Athlódáil Croi ar Ábhar Rith i gcónaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Atosaigh RetroArch nuair a sheoltar ábhar, fiú nuair a bhíonn an croí iarrtha luchtaithe cheana féin. D’fhéadfadh sé seo cobhsaíocht an chórais a fheabhsú, ar chostas amanna lódála méadaithe."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Ceadaigh Rothlú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Lig do croíthe rothlú a shocrú. Nuair a bhíonn sé díchumasaithe, déantar neamhaird d'iarratais rothlaithe. Úsáideach do shocruithe a rothlaíonn an scáileán de láimh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Bainistigh Croíthe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Déan tascanna cothabhála as líne ar croíthe suiteáilte (cúltaca, athchóiriú, scriosadh, srl.) agus féach ar fhaisnéis croíthe."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Bainistigh Croíthe"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Suiteáil nó díshuiteáil croíthe a dháiltear trí Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Suiteáil croí"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Díshuiteáil an croí"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "Taispeáin 'Bainistigh croíthe'"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Taispeáin an rogha 'Bainistigh croíthe' sa Phríomh-Roghchlár."
)


MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "Díshuiteálfar an croí nuair a scoirfear RetroArch."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "Tá an croí á íoslódáil faoi láthair"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Sábháil Cumraíocht ar Scoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Sábháil athruithe ar an gcomhad cumraíochta nuair a scoireann tú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Sábháil athruithe ar an gcomhad cumraíochta nuair a scoireann tú. Úsáideach le haghaidh athruithe a dhéantar sa roghchlár. Scríobhann sé seo an comhad cumraíochta thar an gceann, ní choimeádtar #includes agus tuairimí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Sábháil Athmhapáil Comhad ar Scoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Sábháil athruithe ar aon chomhad athmhapála ionchuir gníomhach agus ábhar á dhúnadh nó RetroArch á scor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Luchtaigh Roghanna Croí-Ábhair Shonracha go hUathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Luchtaigh roghanna croí saincheaptha de réir réamhshocraithe ag an am tosaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Luchtaigh Comhaid Sárúcháin go hUathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Luchtaigh cumraíocht saincheaptha ag an am tosaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Luchtaigh Comhaid Athmhapála go hUathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Luchtaigh rialuithe saincheaptha ag an am tosaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Luchtaigh Comhaid Innéacs Diosca Tosaigh go Huathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "Athraigh go dtí an diosca deireanach a úsáideadh agus ábhar ildhiosca á thosú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Luchtaigh Réamhshocruithe Scáthaithe go hUathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Úsáid Comhad Roghanna Croí Domhanda"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Sábháil na roghanna croí go léir i gcomhad socruithe coiteann (retroarch-core-options.cfg). Nuair a bhíonn sé díchumasaithe, sábhálfar roghanna do gach croí i bhfillteán/comhad ar leith atá sainiúil don chroí in eolaire 'Cumraíochtaí' RetroArch."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Sórtáil Sábhálacha i bhFillteáin de réir Ainm Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Sórtáil comhaid shábháilte i bhfillteáin ainmnithe i ndiaidh an chroí a úsáideadh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Sórtáil Stáit Shábháilte i bhFillteáin de réir Ainm Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Sórtáil stáit sábhála i bhfillteáin ainmnithe i ndiaidh an chroí a úsáideadh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Sórtáil Sábhálacha i bhFillteáin de réir Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Sórtáil comhaid shábháilte i bhfillteáin ainmnithe i ndiaidh an eolaire ina bhfuil an t-ábhar suite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Sórtáil Stáit Shábháilte i bhFillteáin de réir Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Sórtáil stáit sábhála i bhfillteáin ainmnithe i ndiaidh an eolaire ina bhfuil an t-ábhar suite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Ná scríobh thar SaveRAM agus an stát sábhála á luchtú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Bac a chur ar SaveRAM ó bheith róscríobhta agus stáit sábhála á luchtú. D’fhéadfadh sé seo cluichí lochtacha a chruthú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Eatramh Uathshábháilte SábháilRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Sábháil an SaveRAM neamh-luaineach go huathoibríoch ag eatramh rialta (i soicindí)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Uath-shábháiltear an SRAM neamh-luaineach ag eatramh rialta. Tá sé seo díchumasaithe de réir réamhshocraithe mura socraítear a mhalairt. Tomhaistear an t-eatramh i soicindí. Díchumasaíonn luach 0 an t-uath-shábháil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Eatramh Seiceála Athsheinm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Cuir leabharmharcanna uathoibríocha ar staid an chluiche le linn taifeadadh athimeartha ag eatramh rialta (i soicindí)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Sábhálann sé staid an chluiche go huathoibríoch le linn taifeadadh athimeartha ag eatramh rialta. Tá sé seo díchumasaithe de réir réamhshocraithe mura socraítear a mhalairt. Tomhaistear an t-eatramh i soicindí. Díchumasaíonn luach 0 taifeadadh seicphointe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Méadaigh Innéacs an Stáit Sábháilte go Huathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Sula ndéantar staid sábhála, méadaítear innéacs an staid sábhála go huathoibríoch. Agus ábhar á luchtú, socrófar an t-innéacs go dtí an t-innéacs is airde atá ann cheana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Méadaigh Innéacs Athsheinm go Huathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Sula ndéantar athsheinm, méadaítear innéacs an athsheinm go huathoibríoch. Agus ábhar á luchtú, socrófar an t-innéacs go dtí an t-innéacs is airde atá ann cheana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Uasmhéid Uathmhéadaithe Sábháilte Stáit le Coinneáil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Cuir teorainn le líon na stát sábhála a chruthófar nuair a bheidh 'Innéacs Stáit Sábhála Méadaithe go Hiomparúil' cumasaithe. Má sháraítear an teorainn agus stát nua á shábháil, scriosfar an stát atá ann cheana leis an innéacs is ísle. Ciallaíonn luach '0' go dtaifeadfar líon neamhtheoranta stát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Uasmhéid Athsheinm Uathmhéadaithe le Coinneáil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Cuir teorainn le líon na n-athsheinm a chruthófar nuair a bhíonn 'Méadaigh Innéacs Athsheinm go Hiomparúil' cumasaithe. Má sháraítear an teorainn agus athsheinm nua á thaifeadadh, scriosfar an t-athsheinm atá ann cheana leis an innéacs is ísle. Ciallaíonn luach '0' go dtaifeadfar athsheinm gan teorainn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Stát Sábháil Uathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Cruthaigh staid sábhála go huathoibríoch nuair a dhúntar ábhar. Luchtófar an staid sábhála seo ag am tosaithe má tá 'Stádas Luchtaithe Uathoibríoch' cumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Stádas Luchtaithe Uathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Luchtaigh an staid uath-shábhála go huathoibríoch ag an am tosaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Sábháil Mionsamhlacha Stáit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Taispeáin mionsamhlacha de stáit sábhála sa roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Comhbhrú SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Scríobh comhaid SaveRAM neamh-luaineacha i bhformáid chartlannaithe. Laghdaíonn sé méid an chomhaid go suntasach ar chostas méadú (neamhbhríoch) ar amanna sábhála/lódála. \nNí bhaineann sé seo ach le croíthe a chuireann ar chumas sábháil tríd an gcomhéadan SaveRAM caighdeánach libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Sábháil Comhbhrú Stáit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Scríobh comhaid stáit sábhála i bhformáid chartlannaithe. Laghdaíonn sé seo méid an chomhaid go mór ar chostas amanna sábhála/lódála méadaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Sórtáil Scáileáin i bhFillteáin de réir Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Sórtáil scáileáin i bhfillteáin ainmnithe i ndiaidh an eolaire ina bhfuil an t-ábhar suite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Scríobh Sábhálacha chuig Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Úsáid eolaire ábhair mar eolaire comhad sábhála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Scríobh Stáit Sábháilte chuig Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Úsáid eolaire ábhair mar eolaire stádais sábhála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Tá Comhaid Chórais san Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Úsáid an t-eolaire ábhair mar eolaire Córais/BIOS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Scríobh Scáileáin chuig Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Úsáid eolaire ábhair mar eolaire scáileáin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Sábháil Log Ama Rith (In aghaidh an Chroí)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Coinnigh súil ar cé chomh fada is atá gach mír ábhair ar siúl, agus na taifid scartha de réir croí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Sábháil Log Ama Rith (Comhliomlán)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Coinnigh súil ar an méid ama a bhfuil gach mír ábhair ag rith, agus é taifeadta mar an t-iomlán comhiomlán ar fud na croíthe go léir."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Focúlacht Logála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Logáil imeachtaí chuig críochfort nó comhad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Leibhéal Logála Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Socraigh leibhéal loga don tosaigh. Má tá leibhéal loga arna eisiúint ag an tosaigh faoi bhun an luacha seo, déantar neamhaird de."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Leibhéal Logála Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Socraigh leibhéal loga do croíthe. Má tá leibhéal loga arna eisiúint ag croíleacán faoi bhun an luacha seo, déantar neamhaird de."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Socraíonn sé leibhéal loga do croíthe libretro (GET_LOG_INTERFACE). Má tá leibhéal loga arna eisiúint ag croí libretro faoi bhun leibhéal libretro_log, déantar neamhaird de. Déantar neamhaird de logaí DEBUG i gcónaí mura ngníomhaítear mód foclach (--verbose).\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (Dífhabhtaigh)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (Eolas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Rabhadh)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (Earráid)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Logáil chuig Comhad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Atreoraigh teachtaireachtaí loga imeachtaí an chórais chuig comhad. Éilíonn sé sin go mbeidh 'Focal Logála' cumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Comhaid Loga Stampa Ama"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Agus tú ag logáil isteach i gcomhad, atreoraigh an t-aschur ó gach seisiún RetroArch chuig comhad nua le stampa ama. Má tá sé díchumasaithe, déantar an log a athscríobh gach uair a atosófar RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Áiritheoirí Feidhmíochta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Áiritheoirí feidhmíochta do RetroArch agus do chroíthe. Is féidir le sonraí cuntair cabhrú le bacainní córais a chinneadh agus feidhmíocht a choigeartú go mion."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Taispeáin Comhaid agus Eolairí Folaithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Taispeáin comhaid agus eolairí folaithe sa Bhrabhsálaí Comhad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Scagaire Síneadh Anaithnid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Scag comhaid atá á thaispeáint sa Bhrabhsálaí Comhad de réir síntí a dtacaítear leo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Scag de réir Croi Reatha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "Scag na comhaid atá á thaispeáint sa Bhrabhsálaí Comhad de réir an croí reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Cuimhnigh ar an Eolaire Tosaigh a Úsáideadh Deireanach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Oscail an Brabhsálaí Comhad ag an suíomh deireanach a úsáideadh agus ábhar á luchtú ón Eolaire Tosaigh. Tabhair faoi deara: Athshocrófar an suíomh go dtí an suíomh réamhshocraithe nuair a atosófar RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SUGGEST_ALWAYS,
   "Mol Croíthe i gCónaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS,
   "Mol croíthe atá ar fáil fiú nuair atá croí luchtaithe cheana féin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Úsáid an Seinnteoir Meán Tógtha Isteach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Úsáid an Breathnóir Íomhánna Tógtha Isteach"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Athchasadh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Athraigh socruithe athchasála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Áireamhán Ama Fráma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Athraigh socruithe a mbíonn tionchar acu ar an gcuntar ama fráma.\nGníomhach amháin nuair a bhíonn físeán snáithithe díchumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Ráta Mear-Ar Aghaidh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "An ráta uasta ag a rithfear ábhar agus luasghéarú á úsáid (e.g., 5.0x le haghaidh ábhar 60 fps = uasteorainn 300 fps). Má shocraítear é go 0.0x, níl aon teorainn leis an gcóimheas luasghéaraithe (gan uasteorainn FPS)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "An ráta uasta ag a rithfear ábhar agus luasghéarú in úsáid. (M.sh. 5.0 le haghaidh ábhar 60 fps => uasteorainn 300 fps).\nRachaidh RetroArch a chodladh lena chinntiú nach sárófar an ráta uasta. Ná bí ag brath ar an uasteorainn seo chun a bheith cruinn go hiomlán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Léim Frámaí Ar Aghaidh go Meandarach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Léimeann sé frámaí de réir an ráta luasghéaraithe. Sábhálann sé seo cumhacht agus ceadaíonn sé úsáid teorannú fráma tríú páirtí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Ráta Gluaiseachta Mall"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "An ráta a sheinnfear ábhar nuair a úsáidtear gluaiseacht mall."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Ráta Fráma Roghchláir Throttle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Cinntíonn sé go bhfuil uasteorainn ar an ráta fráma agus tú istigh sa roghchlár."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Tacaíocht Athchasadh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Fill ar phointe roimhe seo i gcluiche le déanaí. Is cúis le seo buille mór feidhmíochta agus tú ag imirt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Frámaí Athchasadh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Líon na bhfrámaí le hathruáil in aghaidh an chéime. Méadaíonn luachanna níos airde luas an athruithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Méid Maoláin Athchasadh (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "An méid cuimhne (i MB) atá le cur in áirithe don mhaolán athchasála. Má mhéadaítear é seo, méadófar méid na staire athchasála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Céim Méid Maoláin Athchasadh (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Gach uair a mhéadaítear nó a laghdaítear luach mhéid an mhaoláin athchasála, athróidh sé de réir an mhéid seo."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Athshocraigh Tar éis Tapáil Ar Aghaidh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Athshocraigh an cuntar ama fráma tar éis luasghéarú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Athshocraigh Tar éis Staid Luchtaithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Athshocraigh an cuntar ama fráma tar éis stát a lódáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Athshocraigh Tar éis Stádas Sábháil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Athshocraigh an cuntar ama fráma tar éis staid a shábháil."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Cáilíocht Taifeadta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Neamhchaillteach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM go tapa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "Ardchaighdeán WebM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Cumraíocht Taifeadta Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Snáitheanna Taifeadta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Úsáid Taifeadadh Scagaire Iar-"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Gabh an íomhá tar éis na scagairí (ach ní na scáthaithe) a chur i bhfeidhm. Beidh an físeán chomh galánta leis an méid a fheiceann tú ar do scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Úsáid Taifeadadh GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Taifead aschur ábhair scáthaithe GPU más féidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Mód Sruthaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Cáilíocht Sruthaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Cumraíocht Sruthaithe Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Teideal an tSrutha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "URL sruthaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "Calafort Srutha UDP"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Forleagan ar an Scáileán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Coigeartaigh na bezels agus na rialuithe ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Leagan Amach Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Coigeartaigh Leagan Amach Físe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Fógraí ar an Scáileán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Coigeartaigh Fógraí ar an Scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Infheictheacht Fógra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Scoránaigh infheictheacht cineálacha sonracha fógraí."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Forleagan Taispeána"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Úsáidtear forleagan le haghaidh teorainneacha agus rialuithe ar an scáileán."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Taispeáin Forleagan Taobh thiar den Roghchlár"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Taispeáin an forleagan taobh thiar den roghchlár seachas os a chomhair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Folaigh Forleagan sa Roghchlár"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Folaigh an forleagan agus tú istigh sa roghchlár, agus taispeáin arís é nuair a bheidh tú ag fágáil an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Folaigh Forleagan Nuair a Bhíonn an Rialaitheoir Ceangailte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Folaigh an forleagan nuair a bhíonn rialtóir fisiceach ceangailte i bport 1, agus taispeáin arís é nuair a bhíonn an rialtóir dícheangailte."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Folaigh an forleagan nuair a bhíonn rialtóir fisiceach ceangailte i bport 1. Ní athchóireofar an forleagan go huathoibríoch nuair a dhícheanglaítear an rialtóir."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Taispeáin Ionchuir ar Fhorleagan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Taispeáin ionchuir chláraithe ar an bhforleagan ar an scáileán. Le 'Teagmháilte', tugtar aird ar eilimintí forleagan a mbrúitear/a gclicítear orthu. Le 'Fisiciúil (Rialaitheoir)', tugtar aird ar an ionchur iarbhír a chuirtear chuig croíleacáin, de ghnáth ó rialtóir/méarchlár ceangailte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Tadhaíodh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Fisiciúil (Rialaitheoir)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Taispeáin Ionchuir Ón gCalafort"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Roghnaigh port an fheiste ionchuir le monatóireacht a dhéanamh air nuair a bhíonn 'Taispeáin Ionchuir ar Fhorleagan' socraithe go 'Fisiciúil (Rialaitheoir)'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Taispeáin Cúrsóir Luiche le Forleagan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Taispeáin cúrsóir na luiche agus forleagan ar an scáileán in úsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Forleagan Uath-Rothlaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Má thacaíonn an forleagan reatha leis, rothlaítear an leagan amach go huathoibríoch chun go mbeidh sé ag teacht leis an treoshuíomh/cóimheas gné den scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Forleagan Uath-Scála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Coigeartaítear scála forleagan agus spásáil eilimintí an chomhéadain úsáideora go huathoibríoch chun cóimheas gné an scáileáin a mheaitseáil. Is fearr na torthaí a gheofar le forleagan rialaitheora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Íogaireacht Trasnánach an D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Coigeartaigh méid na gcriosanna trasnánacha. Socraigh go 100% le haghaidh siméadracht 8-bhealach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Íogaireacht Forluí ABXY"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Coigeartaigh méid na gcriosanna forluí sa diamant cnaipe aghaidhe. Socraigh go 100% le haghaidh siméadracht 8-bhealach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Crios Athdhírithe Analógach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Beidh ionchur an mhaide analógach coibhneasta leis an gcéad teagmháil má bhrúitear é laistigh den chrios seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Forleagan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Uathlódáil Forleagan is Fearr"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "Is fearr forleagan a luchtú bunaithe ar ainm an chórais sula dtéann tú ar ais chuig an réamhshocrú réamhshocraithe. Déanfar neamhaird de seo má shocraítear sárú don réamhshocrú forleagan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Teimhneacht Forleagan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Teimhneacht gach eilimint den chomhéadan úsáideora den fhorleagan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Réamhshocrú Forleagan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Roghnaigh forleagan ón mBrabhsálaí Comhad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "Scála Forleagan (Tírdhreach)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Scála gach eilimint den chomhéadan úsáideora den fhorleagan agus treoshuíomhanna taispeána tírdhreacha in úsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Coigeartú Gné Forleagan (Tírdhreach)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Cuir fachtóir ceartúcháin cóimheas gné i bhfeidhm ar an bhforleagan agus treoshuíomhanna taispeána tírdhreacha in úsáid. Méadaíonn luachanna dearfacha (agus laghdaíonn luachanna diúltacha) leithead éifeachtach an fhorleagan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Tírdhreach) Forleagan Deighilt Chothrománach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Más féidir leis an réamhshocrú reatha é a choigeartú, is féidir an spásáil idir eilimintí an chomhéadain úsáideora i leath chlé agus dheas forleagan a choigeartú agus treoshuíomhanna taispeána tírdhreacha in úsáid. Méadaíonn luachanna dearfacha (agus laghdaíonn luachanna diúltacha) scaradh an dá leath."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Tírdhreach) Forleagan Deighilt Ingearach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Más féidir leis an réamhshocrú reatha é sin a dhéanamh, coigeartaigh an spásáil idir eilimintí an chomhéadain úsáideora i leath uachtarach agus íochtarach forleagan agus treoshuíomhanna taispeána tírdhreacha in úsáid. Méadaíonn luachanna dearfacha (agus laghdaíonn luachanna diúltacha) scaradh an dá leath."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Tírdhreach) Forleagan X Fritháireamh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Fritháireamh forleagan cothrománach agus treoshuíomhanna taispeána tírdhreacha in úsáid. Bogann luachanna dearfacha an forleagan ar dheis; luachanna diúltacha ar chlé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Tírdhreach) Forleagan Fritháireamh Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Fritháireamh forleagan ingearach agus treoshuíomhanna taispeána tírdhreacha in úsáid. Bogann luachanna dearfacha an forleagan suas; luachanna diúltacha síos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "Scála Forleagan (Portráid)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Scála gach eilimint den chomhéadan úsáideora den fhorleagan agus treoshuíomhanna taispeána portráide in úsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Coigeartú Gné Forleagan (Portráid)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Cuir fachtóir ceartúcháin cóimheas gné i bhfeidhm ar an bhforleagan agus treoshuíomhanna taispeána portráide in úsáid. Méadaíonn luachanna dearfacha (agus laghdaíonn luachanna diúltacha) airde éifeachtach an fhorleagan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Portráid) Forleagan Deighilt Chothrománach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Más féidir leis an réamhshocrú reatha é a choigeartú, is féidir an spásáil idir eilimintí an chomhéadain úsáideora i leath chlé agus dheas forleagan a choigeartú agus treoshuíomhanna taispeána portráide in úsáid. Méadaíonn luachanna dearfacha (agus laghdaíonn luachanna diúltacha) scaradh an dá leath."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Portráid) Scaradh Ingearach Forleagan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Más féidir leis an réamhshocrú reatha é a choigeartú, is féidir an spásáil idir eilimintí an chomhéadain úsáideora i leath uachtarach agus íochtarach forleagan a choigeartú agus treoshuíomhanna taispeána portráide in úsáid. Méadaíonn luachanna dearfacha (agus laghdaíonn luachanna diúltacha) scaradh an dá leath."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Portráid) Forleagan X Fritháireamh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Fritháireamh forleagan cothrománach agus treoshuíomhanna taispeána portráide in úsáid. Bogann luachanna dearfacha an forleagan ar dheis; luachanna diúltacha ar chlé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Portráid) Forleagan Fritháireamh Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Fritháireamh forleagan ingearach agus treoshuíomhanna taispeána portráide in úsáid. Bogann luachanna dearfacha an forleagan suas; luachanna diúltacha síos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "Forleagan Méarchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "Roghnaigh agus coigeartaigh forleagan méarchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "Cumasaigh Forleagan Gunna Solais, Luch, agus Pointeoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "Bain úsáid as aon ionchur tadhaill nach bhfuil ag brú rialuithe forleagan chun ionchur gléas pointeála a chruthú don croí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "Forleagan Gunna Solais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Cumraigh ionchur gunna solais a sheoltar ón bhforleagan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Luch Forleagan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Cumraigh ionchur luiche a sheoltar ón bhforleagan. Tabhair faoi deara: Seoltar cliceáil cnaipe ar chlé, ar dheis agus sa lár le tapanna 1-, 2-, agus 3-mhéar."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "Réamhshocrú Forleagan Méarchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Roghnaigh forleagan méarchláir ón mBrabhsálaí Comhad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Forleagan Méarchláir Uath-Scála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Coigeartaigh forleagan an mhéarchláir go dtí a chóimheas gné bunaidh. Díchumasaigh chun é a shíneadh go dtí an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Teimhneacht Forleagan Méarchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Teimhneacht gach eilimint den chomhéadan úsáideora ar fhorleagan an mhéarchláir."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Calafort Gunna Solais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Socraigh an port croí chun ionchur a fháil ón gunna solais forleagan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Aon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Spreagadh ar Theagmháil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Seol ionchur spreagtha le hionchur pointeora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Moill ar Thruicear (frámaí)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Ionchur spreagtha moille chun am a thabhairt don chúrsóir bogadh. Úsáidtear an mhoill seo freisin chun fanacht leis an gcomhaireamh il-theagmhála ceart."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Ionchur 2-Tadhall"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Roghnaigh ionchur le seoladh nuair a bhíonn dhá phointeoir ar an scáileán. Níor cheart go mbeadh an mhoill spreagtha nialas chun idirdhealú a dhéanamh ó ionchur eile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Ionchur 3-Tadhall"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Roghnaigh ionchur le seoladh nuair a bhíonn trí phointeoir ar an scáileán. Níor cheart go mbeadh an mhoill spreagtha nialas chun idirdhealú a dhéanamh ó ionchur eile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Ionchur 4-Tadhall"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Roghnaigh ionchur le seoladh nuair a bhíonn ceithre phointeoir ar an scáileán. Níor cheart go mbeadh an mhoill spreagtha nialas chun idirdhealú a dhéanamh ó ionchur eile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Ceadaigh Lasmuigh den Scáileán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Ceadaigh díriú lasmuigh de theorainneacha. Díchumasaigh chun díriú lasmuigh den scáileán a theannadh leis an imeall istigh."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Luas na Luiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Coigeartaigh luas ghluaiseacht an chúrsóra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Brúigh Fada le Tarraingt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Brúigh an scáileán go fada chun cnaipe a bhrú síos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Tairseach Brú Fada (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Coigeartaigh an t-am coinneála atá ag teastáil le haghaidh brú fada."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Tapáil faoi dhó le Tarraingt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Tapáil faoi dhó ar an scáileán chun cnaipe a shealbhú ar an dara tapáil. Cuireann sé moill le cliceáil luiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Tairseach Tapála Dúbailte (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Coigeartaigh an t-am ceadaithe idir tapálacha nuair a bhraitear tapáil dhúbailte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Tairseach Svaipeála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Coigeartaigh an raon drifte incheadaithe nuair a bhraitear brú nó tapáil fhada. Léirithe mar chéatadán de thoise níos lú an scáileáin."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Cumasaigh Leagan Amach Físe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Úsáidtear leagan amach físe le haghaidh bezels agus saothar ealaíne eile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Cosán Leagan Amach Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Roghnaigh leagan amach físe ón mBrabhsálaí Comhad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Radharc Roghnaithe"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Roghnaigh radharc laistigh den leagan amach luchtaithe."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Fógraí ar an Scáileán"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Taispeáin teachtaireachtaí ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Giuirléidí Grafaicí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Bain úsáid as beochana maisithe, fógraí, táscairí agus rialuithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Scálaigh Giuirléidí Grafaicí go hUathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Athraigh méid fógraí, táscairí agus rialuithe maisithe go huathoibríoch bunaithe ar scála an roghchláir reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Forshárú Scála Giuirléidí Grafaicí (Lánscáileán)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Cuir sárú fachtóra scálúcháin láimhe i bhfeidhm agus giuirléidí taispeána á dtarraingt i mód lánscáileáin. Ní bhaineann sé seo ach amháin nuair a bhíonn 'Scálú Giuirléidí Grafaicí go hUathoibríoch' díchumasaithe. Is féidir é seo a úsáid chun méid fógraí, táscairí agus rialuithe maisithe a mhéadú nó a laghdú go neamhspleách ar an roghchlár féin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Forshárú Scála Giuirléidí Grafaicí (Fuinneog)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Cuir sárú fachtóra scálúcháin láimhe i bhfeidhm agus giuirléidí taispeána á dtarraingt i mód fuinneoige. Ní bhaineann sé seo ach amháin nuair a bhíonn 'Scálú Giuirléidí Grafaicí go Hoibríoch' díchumasaithe. Is féidir é seo a úsáid chun méid fógraí, táscairí agus rialuithe maisithe a mhéadú nó a laghdú go neamhspleách ar an roghchlár féin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Ráta Fráma Taispeána"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Taispeáin na frámaí in aghaidh an tsoicind reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Eatramh Nuashonraithe Ráta Fráma (I bhFrámaí)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "Déanfar an taispeáint ráta fráma a nuashonrú ag an eatramh socraithe i bhfrámaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Líon na bhFrámaí Taispeána"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Taispeáin an comhaireamh fráma reatha ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Staitisticí Taispeána"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Taispeáin staitisticí teicniúla ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Úsáid Cuimhne Taispeána"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Taispeáin an méid cuimhne atá in úsáid agus an méid iomlán cuimhne ar an gcóras."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Eatramh Nuashonraithe Úsáide Cuimhne (I bhFrámaí)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "Déanfar an taispeáint úsáide cuimhne a nuashonrú ag an eatramh socraithe i bhfrámaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Ping Líonra Taispeána"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Taispeáin an ping don seomra líonra reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Fógra Tosaithe \"Lódáil Ábhar\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Taispeáin beochan aiseolais ghearr ar sheoladh agus ábhar á luchtú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Fógraí Ceangail Ionchuir (Uathchumraíocht)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Fógraí Teipe Ionchuir (Uathchumraíocht)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Fógraí Cód Meallta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Taispeáin teachtaireacht ar an scáileán nuair a chuirtear cóid aicearraí i bhfeidhm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Fógraí Paiste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Taispeáin teachtaireacht ar an scáileán agus ROManna á bpaisteáil go bog."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Taispeáin teachtaireacht ar an scáileán agus gléasanna ionchuir á gceangal/á ndícheangal."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Taispeáin teachtaireacht ar an scáileán nuair nach bhféadfaí gléasanna ionchuir a chumrú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Fógraí Luchtaithe Athmhapála Ionchuir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Taispeáin teachtaireacht ar an scáileán agus comhaid athmhapála ionchuir á lódáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Fógraí Luchtaithe Sáraithe Cumraíochta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Taispeáin teachtaireacht ar an scáileán agus comhaid sáraithe cumraíochta á lódáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Fógraí Athchóirithe Diosca Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Taispeáin teachtaireacht ar an scáileán agus an diosca deireanach a úsáideadh d'ábhar ildhiosca a lódáladh trí seinmliostaí M3U á athchóiriú go huathoibríoch ag an seoladh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Fógraí Rialaithe Diosca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Taispeáin teachtaireacht ar an scáileán agus dioscaí á gcur isteach agus á ndíbirt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Sábháil Fógraí Stáit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "Taispeáin teachtaireacht ar an scáileán agus stáit shábhála á sábháil agus á lódáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Fógraí Tapa Ar Aghaidh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Taispeáin táscaire ar an scáileán agus ábhar á bhogadh ar aghaidh go tapa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Fógraí Scáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Taispeáin teachtaireacht ar an scáileán agus pictiúr scáileáin á thógáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Buanseasmhacht Fógra Scáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Sainigh fad na teachtaireachta scáileáin ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "An-tapa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Meandarach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Éifeacht Splanc Scáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Taispeáin éifeacht splancach bán ar an scáileán leis an fad atá ag teastáil agus pictiúr scáileáin á thógáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "AR (Gnáth)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "AR (Gasta)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Fógraí Ráta Athnuachana"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Taispeáin teachtaireacht ar an scáileán agus an ráta athnuachana á shocrú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Fógraí Breise Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Taispeáin teachtaireachtaí líonra neamhriachtanacha ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Fógraí Roghchláir Amháin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Taispeáin fógraí ach amháin nuair a bhíonn an roghchlár oscailte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Cló Fógra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Roghnaigh an cló le haghaidh fógraí ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Méid an Fhógra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Sonraigh méid an chló i bpointí. Nuair a úsáidtear giuirléidí, ní bhíonn éifeacht ag an méid seo ach amháin maidir le taispeáint staitisticí ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Suíomh Fógra (Cothrománach)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Sonraigh suíomh saincheaptha ais X le haghaidh téacs ar an scáileán. Is é 0 an imeall clé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Suíomh Fógra (Ingearach)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Sonraigh suíomh saincheaptha ais Y le haghaidh téacs ar an scáileán. Is é 0 an imeall bun."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Dath Fógra (Dearg)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Socraíonn sé seo luach dearg dhath téacs an OSD. Tá luachanna bailí idir 0 agus 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Dath Fógra (Glas)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Socraíonn sé luach glas dhath téacs an OSD. Tá luachanna bailí idir 0 agus 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Dath Fógra (Gorm)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Socraíonn sé seo luach gorm dhath téacs an OSD. Tá luachanna bailí idir 0 agus 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Cúlra Fógra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Cumasaíonn sé dath cúlra don OSD."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Dath Cúlra Fógra (Dearg)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Socraíonn sé luach dearg dhath chúlra an OSD. Tá luachanna bailí idir 0 agus 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Dath Cúlra Fógra (Glas)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Socraíonn sé luach glas dath cúlra an OSD. Tá luachanna bailí idir 0 agus 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Dath Cúlra Fógra (Gorm)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Socraíonn sé seo luach gorm dhath chúlra an OSD. Tá luachanna bailí idir 0 agus 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Teimhneacht Chúlra Fógra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Socraíonn sé seo teimhneacht dhath chúlra an OSD. Tá luachanna bailí idir 0.0 agus 1.0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Infheictheacht Míre Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Athraigh infheictheacht míreanna roghchláir i RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Dealramh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Athraigh socruithe cuma scáileáin an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "Deilbhín Aipe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Athraigh Deilbhín an Aipe."
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "Dealramh Bunscáileáin 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Athraigh socruithe cuma bun an scáileáin."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Taispeáin Socruithe Ardleibhéil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Taispeáin socruithe ardleibhéil d'úsáideoirí cumhachtacha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Mód Ciosc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Cosnaíonn sé an socrú trí gach socrú a bhaineann leis an gcumraíocht a cheilt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Socraigh Pasfhocal chun Mód Ciosc a Dhíchumasú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Má sholáthraíonn tú pasfhocal agus mód an chiosc á chumasú, is féidir é a dhíchumasú níos déanaí ón roghchlár, trí dhul go dtí an Príomh-Roghchlár, Díchumasaigh Mód an Chiosc a roghnú agus an pasfhocal a iontráil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Timpeall Loingseoireachta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Timfhilleadh go dtí an tús agus/nó an deireadh má shroichtear teorainn an liosta go cothrománach nó go hingearach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Cuir an t-ábhar ar sos nuair a bhíonn an roghchlár gníomhach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Cuir an t-ábhar ar sos má tá an roghchlár gníomhach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Lean ar aghaidh leis an ábhar tar éis úsáid a bhaint as Stáit Sábháilte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Dún an roghchlár go huathoibríoch agus atosú an t-ábhar tar éis staid a shábháil nó a lódáil. Is féidir feidhmíocht staide sábhála a fheabhsú ar ghléasanna an-mhall má dhíchumasaítear é seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Lean ar aghaidh leis an Ábhar Tar éis Dioscaí a Athrú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Dún an roghchlár go huathoibríoch agus atosú an t-ábhar tar éis diosca nua a chur isteach nó a luchtú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Scoir ar Dhúnadh Ábhar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Scoir RetroArch go huathoibríoch nuair a dhúnfar ábhar. Ní scoireann 'CLI' ach amháin nuair a sheoltar ábhar tríd an líne ordaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Am Teorann Scáileáin Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Cé go bhfuil an roghchlár gníomhach, taispeánfar spárálaíscáileáin tar éis na tréimhse neamhghníomhaíochta sonraithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Beochan Sábhálaí Scáileáin Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Cumasaigh éifeacht beochana agus an spárálaíscáileáin roghchláir gníomhach. Tá tionchar measartha aige ar fheidhmíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Réimse Réalt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Luas Beochana Scáileáin Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Coigeartaigh luas éifeacht beochana scáileáin scáileáin an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Tacaíocht Luiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Lig don roghchlár a bheith á rialú leis an luch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Tacaíocht Tadhaill"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Lig don roghchlár a bheith á rialú le scáileán tadhaill."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Tascanna Snáithithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Déan tascanna ar shnáithe ar leithligh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Cuir Ábhar ar Sos Nuair nach bhfuil sé Gníomhach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Cuir ábhar ar sos nuair nach í RetroArch an fhuinneog ghníomhach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Díchumasaigh Comhdhéanamh Deisce"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Úsáideann bainisteoirí fuinneog comhdhéanamh chun éifeachtaí amhairc a chur i bhfeidhm, fuinneoga neamhfhreagracha a bhrath, i measc rudaí eile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Díchumasaigh comhdhéanamh go foréigneach. Ní féidir é sin a dhíchumasú ach ar Windows Vista/7 faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Luasghéarú Scrollaigh Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Uasluas an chúrsóra agus treo á choinneáil chun scrolla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Moill Scrollaigh Roghchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Moill tosaigh i milleasoicindí agus treo á choinneáil chun scrollú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Comhlach Chomhéadain"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Tosaigh UI Companion ar Tosaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Tosaigh tiománaí chomónta an Chomhéadain Úsáideora ag an tosaithe (más féidir)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Roghchlár Deisce (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Oscail Roghchlár na Deisce ag an Tosaithe"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Roghchlár Tapa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Athraigh infheictheacht na míreanna roghchláir sa Roghchlár Tapa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Socruithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Athraigh infheictheacht na míreanna roghchláir sa roghchlár Socruithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Taispeáin 'Lódáil Croí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Taispeáin an rogha 'Lódáil Croí' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Taispeáin 'Lódáil Ábhar'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Taispeáin an rogha 'Lódáil Ábhar' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Taispeáin 'Luchtaigh Diosca'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Taispeáin an rogha 'Luchtaigh Diosca' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Taispeáin 'Diosca Dumpála'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Taispeáin an rogha 'Dumpáil Diosca' sa Phríomh-Roghchlár."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "Taispeáin 'Díbirt Diosca'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Taispeáin an rogha 'Díbirt Diosca' sa Phríomh-Roghchlár."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Taispeáin 'Nuashonraitheoir Ar Líne'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Taispeáin an rogha 'Nuashonraitheoir Ar Líne' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Taispeáin 'Íoslódálaí Croí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Taispeáin an cumas chun croíthe (agus comhaid eolais croíthe) a nuashonrú sa rogha 'Nuashonraitheoir Ar Líne'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Taispeáin an 'Nuashonraitheoir Mionsamhlacha' Seanré"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Taispeáin an iontráil le haghaidh pacáistí mionsamhlacha oidhreachta a íoslódáil sa rogha 'Nuashonraitheoir Ar Líne'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Taispeáin 'Faisnéis'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Taispeáin an rogha 'Faisnéis' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Taispeáin 'Comhad Cumraíochta'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Taispeáin an rogha 'Comhad Cumraíochta' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Taispeáin 'Cabhair'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Taispeáin an rogha 'Cabhair' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Taispeáin 'Scoir RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Taispeáin an rogha 'Scoir RetroArch' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Taispeáin 'Atosaigh RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Taispeáin an rogha 'Atosaigh RetroArch' sa Phríomh-Roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Taispeáin 'Socruithe'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Taispeáin an roghchlár 'Socruithe'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Socraigh Pasfhocal Chun 'Socruithe' a Chumasú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Má sholáthraíonn tú pasfhocal agus an cluaisín socruithe á cheilt, is féidir é a athchóiriú ón roghchlár níos déanaí, trí dhul go dtí an cluaisín Príomh-Roghchlár, 'Cumasaigh an Cluaisín Socruithe' a roghnú agus an pasfhocal a iontráil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Taispeáin ‘Ceanáin’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Taispeáin an roghchlár ‘Ceanáin’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES_FIRST,
   "Taispeáin Ceanáin ar dtús"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES_FIRST,
   "Taispeáin ‘Favorites’ roimh ‘Stair’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Taispeáin 'Íomhánna'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Taispeáin an roghchlár 'Íomhánna'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Taispeáin 'Ceol'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Taispeáin an roghchlár 'Ceol'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Taispeáin 'Físeáin'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Taispeáin an roghchlár 'Físeáin'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Taispeáin 'Netplay'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Taispeáin an roghchlár 'Netplay'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Taispeáin 'Stair'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Taispeáin an roghchlár staire le déanaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Taispeáin 'Iompórtáil Ábhar'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Taispeáin an iontráil 'Iompórtáil Ábhar' taobh istigh den Phríomh-Roghchlár nó de na Seinmliostaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Príomh-Roghchlár"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Roghchlár Seinmliostaí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Taispeáin 'Seinmliostaí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Taispeáin na seinmliostaí sa Phríomh-Roghchlár. Déantar neamhaird orthu i GLUI má tá na cluaisíní seinmliosta agus an barra nascleanúna cumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "Taispeáin Cluaisíní Seinmliosta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "Taispeáin na cluaisíní seinmliosta. Ní dhéanann sé difear don RGUI. Ní mór an barra nascleanúna a bheith cumasaithe i GLUI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Taispeáin 'Fiosraigh'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Taispeáin an rogha fiosraigh ábhar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Taispeáin 'Croíleácha Gan Ábhar'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Sonraigh an cineál croí (más ann dó) atá le taispeáint sa roghchlár 'Croíthe Gan Ábhar'. Nuair atá sé socraithe go 'Saincheaptha', is féidir infheictheacht croí aonair a athrú tríd an roghchlár 'Bainistigh Croí'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Gach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Aonúsáide"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Taispeáin Dáta agus Am"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Taispeáin an dáta agus/nó an t-am reatha taobh istigh den roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Stíl Dáta agus Ama"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Athraigh an stíl, taispeántar an dáta agus/nó an t-am reatha taobh istigh den roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Deighilteoir Dáta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Sonraigh carachtar le húsáid mar dheighilteoir idir comhpháirteanna bliana/mí/lae nuair a thaispeántar an dáta reatha sa roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Taispeáin Leibhéal na Ceallraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Taispeáin leibhéal reatha na ceallraí taobh istigh den roghchlár."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Taispeáin an rogha 'Sábháil Forbhreathnú Eolaire Ábhair' sa roghchlár 'Forbhreathnú'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Taispeáin 'Sábháil For-riaracháin Cluiche'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Taispeáin an rogha 'Sábháil Forbhreathnuithe Cluiche' sa roghchlár 'Forbhreathnuithe'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Taispeáin na 'hAicearraí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Taispeáin an rogha 'hAicearraí'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Taispeáin 'Scáthadóirí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Taispeáin an rogha 'Scáthóirí'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Taispeáin 'Cuir le Ceanáin'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Taispeáin an rogha 'Cuir le Ceanáin'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Taispeáin 'Cuir le Seinmliosta'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Taispeáin an rogha 'Cuir le Seinmliosta'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Taispeáin 'Socraigh Comhlachas Croí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Taispeáin an rogha 'Socraigh Comhlachas Croí' nuair nach bhfuil ábhar ag rith."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Taispeáin 'Athshocraigh Cumann Croí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Taispeáin an rogha 'Athshocraigh an Comhlachas Croí' nuair nach bhfuil ábhar ag rith."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Taispeáin 'Íoslódáil Mionsamhlacha'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Taispeáin an rogha 'Íoslódáil Mionsamhlacha' nuair nach bhfuil ábhar ag rith."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Taispeáin 'Faisnéis'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Taispeáin an rogha 'Faisnéis'."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Taispeáin 'Tiománaithe'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Taispeáin socruithe 'Tiománaithe'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Taispeáin 'Físeán'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Taispeáin socruithe 'Físeáin'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Taispeáin 'Fuaim'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Taispeáin socruithe 'Fuaime'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Taispeáin 'Ionchur'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Taispeáin socruithe 'Ionchuir'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Taispeáin socruithe 'Aga folaigh'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Taispeáin 'Croí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Taispeáin socruithe 'Croí'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Taispeáin 'Cumraíocht'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Taispeáin socruithe 'Cumraíocht'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Taispeáin 'Ag Sábháil'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Taispeáin socruithe 'Sábháil'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Taispeáin 'Logáil'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Taispeáin socruithe 'Logáil'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Taispeáin 'Brabhsálaí Comhad'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Taispeáin socruithe 'Brabhsálaí Comhad'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Taispeáin 'Laghdú Fráma'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Taispeáin socruithe 'Scóig Fráma'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Taispeáin 'Taifeadadh'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Taispeáin socruithe 'Taifeadadh'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Taispeáin 'Taispeántas Ar an Scáileán'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Taispeáin socruithe 'Taispeántas ar an Scáileán'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Taispeáin 'Comhéadan Úsáideora'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Taispeáin socruithe 'Comhéadan Úsáideora'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Taispeáin 'Seirbhís AI'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Taispeáin socruithe 'Seirbhís AI'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Taispeáin 'Inrochtaineacht'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Taispeáin socruithe 'Inrochtaineachta'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Taispeáin 'Bainistíocht Cumhachta'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Taispeáin 'Seinmliostaí'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Taispeáin socruithe 'Úsáideora'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Taispeáin 'Eolaire'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Taispeáin socruithe 'Eolaire'."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "Taispeáin 'Steam'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "Taispeáin socruithe 'Steam'."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Fachtóir Scála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Scálaigh méid eilimintí comhéadan úsáideora sa roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Íomhá Chúlra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Roghnaigh íomhá le socrú mar chúlra an roghchláir. Sáróidh íomhánna láimhe agus dinimiciúla 'Téama Datha'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Teimhneacht an Chúlra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Modhnaigh teimhneacht na híomhá cúlra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Teimhneacht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Modhnaigh teimhneacht chúlra réamhshocraithe an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Úsáid Téama Dath an Chórais is Fearr"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Úsáid téama dathanna an chórais oibriúcháin (más ann dó). Sáraíonn sé socruithe téama."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Cineál mionsamhail le taispeáint."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Tairseach Uas-scála Mionsamhail"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Déanann sé mionsamhlacha a uasghrádú go huathoibríoch le leithead/airde níos lú ná an luach sonraithe. Feabhsaíonn sé cáilíocht na pictiúr. Tá tionchar measartha aige ar fheidhmíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Cúlraí Mionsamhlacha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Beochan Téacs Ticéid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Roghnaigh an modh scrollaigh chothrománach a úsáidtear chun téacs fada roghchláir a thaispeáint."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Luas Téacs Ticéid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Luas na beochana agus téacs fada roghchláir á scrollú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Téacs Ticéid Réidh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Bain úsáid as beochan scrollaigh réidh agus téacs fada roghchláir á thaispeáint. Tá tionchar beag aige ar fheidhmíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Cuimhnigh ar an Roghnú agus tú ag Athrú Cluaisíní"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Cuimhnigh ar shuíomh an chúrsóra roimhe seo i gcluaisíní. Níl cluaisíní ag RGUI, ach oibríonn Seinmliostaí agus Socruithe ar an gcaoi chéanna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Le haghaidh Seinmliostaí Amháin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Don Phríomh-Roghchlár agus Socruithe Amháin"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Aschur Seirbhíse AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Taispeáin an t-aistriúchán mar fhorleagan téacs (Mód Íomhá), seinn mar Téacs-Go-Caint (Caint), nó bain úsáid as insteoir córais cosúil le NVDA (Insteoir)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "URL Seirbhíse AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "URL http:// ag pointeáil chuig an tseirbhís aistriúcháin atá le húsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Seirbhís AI Cumasaithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Cumasaigh Seirbhís AI le rith nuair a bhrúnn tú an eochair the Seirbhís AI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Sos le linn an Aistriúcháin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Cuir an croí ar sos agus an scáileán á aistriú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Teanga Fhoinseach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "An teanga a n-aistreoidh an tseirbhís uaithi. Má shocraítear é go 'Réamhshocrú', déanfaidh sé iarracht an teanga a bhrath go huathoibríoch. Má shocraítear é go teanga shonrach, beidh an t-aistriúchán níos cruinne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Teanga Spriocdhírithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "An teanga a n-aistreofar chuici ón tseirbhís. Is é an Béarla an rogha réamhshocraithe."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Cumasaigh Inrochtaineacht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Cumasaigh Téacs-go-Caint chun cabhrú le nascleanúint an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Luas Téacs-go-Caint"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "An luas don ghuth Téacs-go-Caint."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Éachtaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Bain amach éachtaí i gcluichí clasaiceacha. Le haghaidh tuilleadh eolais, tabhair cuairt ar 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Mód Crua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Díchumasaíonn sé seo cleasanna, athchasadh, gluaiseacht mhall, agus stáit sábhála luchtaithe. Tá marc uathúil ar éachtaí a thuilltear i mód crua ionas gur féidir leat a thaispeáint do dhaoine eile cad atá bainte amach agat gan ghnéithe cúnaimh aithriseora. Má athraíonn tú an socrú seo ag am rith, atosófar an cluiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Cláir Cheannaireachta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "Láithreacht Shaibhir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Seolann sé faisnéis chomhthéacsúil faoin gcluiche chuig suíomh Gréasáin RetroAchievements go tréimhsiúil. Níl aon éifeacht leis má tá 'Mód Hardcore' cumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Suaitheantais Gnóthachtála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Taispeáin suaitheantais sa Liosta Gnóthachtálacha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Tástáil ar Éachtaí Neamhoifigiúla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Bain úsáid as éachtaí neamhoifigiúla agus/nó gnéithe béite chun críocha tástála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Díghlasáil Fuaime"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Seinn fuaim nuair a dhíghlasáiltear éacht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Scáileán Uathoibríoch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Glac pictiúr scáileáin go huathoibríoch nuair a ghnóthaítear éacht."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Mód Encore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Tosaigh an seisiún agus na héachtaí uile gníomhach (fiú na cinn a díghlasáladh roimhe seo)."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Dealramh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Athraigh suíomh agus fritháireamh fógraí éachtaí ar an scáileán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Seasamh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Socraigh cúinne/imeall an scáileáin as a dtaispeánfar fógraí éachtaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Barr ar Chlé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "Lár Barr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Barr ar Dheis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Bun ar Chlé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Lár an Bhun"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "Bun ar Dheis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Líonta Ailínithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Socraigh an bhfuil fógraí éachtaí ailínithe le cineálacha eile fógraí ar an scáileán. Díchumasaigh chun luachanna líonta/suímh láimhe a shocrú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "Páipéar Cothrománach Láimhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Fad ó imeall clé/deas an scáileáin, rud a fhéadann cúiteamh a dhéanamh as ró-scanadh an taispeána."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "Páipéar Ingearach Láimhe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Fad ó imeall uachtarach/bun an scáileáin, rud a fhéadann cúiteamh a dhéanamh as ró-scanadh an taispeána."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "Infheictheacht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "Athraigh na teachtaireachtaí agus na heilimintí ar an scáileán a thaispeántar. Ní dhíchumasaíonn sé feidhmiúlacht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "Achoimre Tosaithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "Taispeánann sé faisnéis faoin gcluiche atá á luchtú agus dul chun cinn reatha an úsáideora. \nTaispeánfaidh 'Gach Cluiche Aitheanta' achoimre ar chluichí nach bhfuil aon éachtaí foilsithe acu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "Gach Cluiche Aitheanta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "Cluichí le Gnóthachtálacha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "Díghlasáil Fógraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "Taispeánann sé fógra nuair a dhíghlasáiltear éacht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "Fógraí Máistreachta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "Taispeánann sé fógra nuair a dhíghlasáiltear gach éacht i gcluiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Táscairí Dúshláin Ghníomhaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Taispeánann sé táscairí ar an scáileán agus éachtaí áirithe le gnóthachtáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Táscaire Dul Chun Cinn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Taispeánann sé táscaire ar an scáileán nuair a dhéantar dul chun cinn i dtreo éachtaí áirithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "Teachtaireachtaí Tosaigh an Chláir Ceannaireachta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "Taispeánann sé cur síos ar chlár ceannaireachta nuair a bhíonn sé gníomhach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Clár Ceannairí Cuir Teachtaireachtaí Isteach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Taispeánann sé teachtaireacht leis an luach atá á chur isteach nuair a bhíonn iarracht ar chlár ceannaireachta críochnaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Teachtaireachtaí Theipthe ar Chlár Ceannaireachta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Taispeánann sé teachtaireacht nuair a theipeann ar iarracht clár ceannaireachta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Rianaitheoirí na gClár Ceannais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Taispeánann sé rianaitheoirí ar an scáileán leis an luach reatha de na cláir cheannaireachta gníomhacha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Teachtaireachtaí Logála Isteach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Taispeánann sé teachtaireachtaí a bhaineann le logáil isteach i gcuntas RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Teachtaireachtaí Fadtéarmacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Taispeánann sé teachtaireachtaí diagnóiseacha agus earráide breise."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Fógraigh Netplay go Poiblí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Cibé acu cluichí netplay a fhógairt go poiblí nó nach ea. Mura bhfuil sé socraithe, ní mór do chliaint ceangal de láimh seachas an stocaireacht phoiblí a úsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Úsáid Freastalaí Athsheolta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Seol naisc netplay ar aghaidh trí fhreastalaí fear-sa-lár. Úsáideach má tá an t-óstach taobh thiar de bhalla dóiteáin nó má tá fadhbanna NAT/UPnP aige."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Suíomh an Fhreastalaí Athsheolta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Roghnaigh freastalaí athsheachadta ar leith le húsáid. Is gnách go mbíonn moill níos ísle ag suíomhanna atá níos gaire go geografach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Seoladh Freastalaí Athsheolta Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Cuir isteach seoladh do fhreastalaí athsheachadta saincheaptha anseo. Formáid: seoladh nó seoladh|port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "Meiriceá Thuaidh (Cósta Thoir, SAM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "Iarthar na hEorpa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "Meiriceá Theas (Oirdheisceart, an Bhrasaíl)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Oirdheisceart na hÁise"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_5,
   "Oirthear na hÁise (Chuncheon, an Chóiré Theas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Seoladh an Fhreastalaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Seoladh an óstaigh le ceangal leis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Port TCP Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Port sheoladh IP an óstaigh. Is féidir gur port TCP nó UDP é."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Uasmhéid Naisc Chomhuaineacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "An líon uasta naisc ghníomhacha a nglacfaidh an t-óstach leo sula ndiúltóidh sé do chinn nua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Teorannóir Ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "An moill uasta nasc (ping) a nglacfaidh an t-óstach leis. Socraigh go 0 é le haghaidh gan teorainn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Pasfhocal an Fhreastalaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "An focal faire a úsáideann cliaint atá ag nascadh leis an óstach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Pasfhocal Freastalaí Spectate Amháin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "An focal faire a úsáideann cliaint atá ag ceangal leis an óstach mar lucht féachana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Mód Breathnóra Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Tosaigh an súgradh líonra i mód lucht féachana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Cibé acu ar cheart an súgradh líonra a thosú i mód breathnóra. Má shocraítear go fíor, beidh an súgradh líonra i mód breathnóra ar an tús. Is féidir an modh a athrú níos déanaí i gcónaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Comhrá Céimnithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Teachtaireachtaí comhrá a laghdú le himeacht ama."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Dath Comhrá (Leasainm)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Formáid: #RRGGBB nó RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Dath Comhrá (Teachtaireacht)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Formáid: #RRGGBB nó RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Ceadaigh Sos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Lig d’imreoirí sos a chur le linn súgartha ar an ngréasán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Ceadaigh Cliant Mód Sclábhaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Ceadaigh naisc i mód sclábhaí. Ní bhíonn mórán cumhachta próiseála ag teastáil ó chliaint i mód sclábhaí ar cheachtar taobh, ach beidh moill mhór líonra orthu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Cosc a chur ar chliaint nach bhfuil i mód sclábhaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Cosc a chur ar naisc nach bhfuil i mód sclábhaí. Ní mholtar é seo ach amháin i gcás líonraí an-tapa le meaisíní an-laga."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Frámaí Seiceála Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "An mhinicíocht (i bhfrámaí) a fhíoróidh netplay go bhfuil an t-óstach agus an cliant sioncrónaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "An mhinicíocht i bhfrámaí lena n-úsáideann netplay fíorú go bhfuil an t-óstach agus an cliant sioncrónaithe. Le formhór na gcroíleacán, ní bheidh aon éifeacht le feiceáil ag an luach seo agus is féidir neamhaird a dhéanamh air. Le croíleacáin neamhchinntitheacha, cinneann an luach seo cé chomh minic a thabharfar na piaraí netplay i sioncrónú. Le croíleacáin lochtacha, má shocraítear é seo go luach neamh-nialas, beidh fadhbanna feidhmíochta tromchúiseacha mar thoradh[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Frámaí Latency Ionchuir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Líon na bhfrámaí de mhoill ionchuir le húsáid ag netplay chun moill líonra a cheilt. Laghdaíonn sé seo crith agus déanann netplay níos lú dian ar LAP, ar chostas moill ionchuir suntasach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Líon na bhfrámaí de mhoill ionchuir le húsáid ag netplay chun moill líonra a cheilt.\nNuair atá netplay i bhfeidhm, cuireann an rogha seo moill ar ionchur áitiúil, ionas go mbeidh an fráma atá á rith níos gaire do na frámaí atá á bhfáil ón líonra. Laghdaíonn sé seo an crith agus déanann netplay níos lú dian ar LAP, ach ar chostas moille ionchuir suntasach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Raon Frámaí Latency Ionchuir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "An raon frámaí moille ionchuir a fhéadfar a úsáid chun moille líonra a cheilt. Laghdaíonn sé seo crith agus déanann sé súgradh líonra níos lú dian ar LAP, ar chostas moille ionchuir dothuartha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "An raon frámaí moille ionchuir a fhéadfaidh netplay a úsáid chun moill líonra a cheilt.\nMá shocraítear é, déanfaidh netplay líon na bhfrámaí moille ionchuir a choigeartú go dinimiciúil chun am LAP, moill ionchuir agus moill líonra a chothromú. Laghdaíonn sé seo jitter agus déanann netplay níos lú dian ar LAP, ach ar chostas moille ionchuir dothuartha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Trasnú NAT Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Agus tú ag óstáil, déan iarracht éisteacht le haghaidh naisc ón Idirlíon poiblí, ag baint úsáide as UPnP nó teicneolaíochtaí comhchosúla chun éalú ó LANanna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Comhroinnt Ionchuir Dhigitigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Iarr Gléas %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Iarratas chun imirt leis an ngléas ionchuir tugtha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Orduithe Líonra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Port Ordú Líonra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "Líonra RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Port Bonn Líonra RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Úsáideoir %d Líonra RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "orduithe stdin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "comhéadan ordaithe stdin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Íoslódálacha Mionsamhlacha Ar Éileamh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Íoslódáil mionsamhlacha atá ar iarraidh go huathoibríoch agus seinmliostaí á mbrabhsáil. Tá tionchar mór aige ar fheidhmíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Socruithe Nuashonraitheora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Rochtain ar shocruithe croí an nuashonraitheora"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "URL Croíthe Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL chuig an eolaire nuashonraitheora croí ar an bot tógála libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "URL Sócmhainní Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL chuig eolaire nuashonraithe sócmhainní ar an bot tógála libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Easbhaint Uathoibríoch ar Chartlann Íoslódáilte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Tar éis na híoslódála, bainfear na comhaid atá sna cartlanna íoslódáilte amach go huathoibríoch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Taispeáin Croíthe Turgnamhacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Cuir croíthe 'turgnamhacha' san áireamh i liosta Íoslódálaí na croíthe. De ghnáth, is chun críocha forbartha/tástála amháin atá siad seo, agus ní mholtar iad le haghaidh úsáide ginearálta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Croíthe Cúltaca Agus Nuashonrú á Dhéanamh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Cruthaigh cúltaca go huathoibríoch d'aon croíthe suiteáilte agus nuashonrú ar líne á dhéanamh. Cumasaíonn sé seo rolladh ar ais go croíthe atá ag obair go héasca má thugann nuashonrú aischéimniú isteach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Méid Stair Cúltaca Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Sonraigh cé mhéad cúltaca a ghintear go huathoibríoch le coinneáil do gach croí suiteáilte. Nuair a shroichtear an teorainn seo, scriosfar an cúltaca is sine má chruthaítear cúltaca nua trí nuashonrú ar líne. Ní dhéanann an socrú seo difear do chúltacaí croí láimhe."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Stair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Coinnigh seinmliosta de chluichí, íomhánna, ceol agus físeáin a úsáideadh le déanaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Méid na Staire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Cuir teorainn le líon na n-iontrálacha sa seinmliosta le déanaí le haghaidh cluichí, íomhánna, ceoil agus físeáin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Méid is Fearr leat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Cuir teorainn le líon na n-iontrálacha sa seinmliosta 'Is Fearr Liom'. Nuair a shroichtear an teorainn, cuirfear cosc ​​ar bhreiseanna nua go dtí go mbainfear sean-iontrálacha. Má shocraítear luach -1, ceadaítear iontrálacha 'gan teorainn'.\nRABHADH: Scriosfar iontrálacha atá ann cheana má laghdaítear an luach!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Ceadaigh Athainmniú Iontrálacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Ceadaigh athainmniú iontrálacha seinmliosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Ceadaigh Iontrálacha a Bhaint"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Ceadaigh iontrálacha seinmliosta a bhaint."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Sórtáil Seinmliostaí in Aibítre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Sórtáil seinmliostaí ábhair in ord aibítre, gan na seinmliostaí 'Stair', 'Íomhánna', 'Ceol' agus 'Físeáin' a áireamh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Sábháil Seinmliostaí Ag Úsáid Seanfhormáid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Scríobh seinmliostaí ag baint úsáide as formáid téacs simplí atá imithe i léig. Nuair a bhíonn sé díchumasaithe, déantar seinmliostaí a fhormáidiú ag baint úsáide as JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Comhbhrúigh Seinmliostaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Cartlannú sonraí seinmliosta agus scríobh chuig diosca á dhéanamh. Laghdaíonn sé méid an chomhaid agus amanna lódála ar chostas méadú (neamhbhríoch) ar úsáid LAP. Is féidir é a úsáid le seinmliostaí seanfhormáide nó nuafhormáide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Taispeáin Croíthe Ghaolmhara i Seinmliostaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Sonraigh cathain is ceart iontrálacha seinmliosta a chlibeáil leis an croí atá bainteach leis faoi láthair (más ann dó).\nDéantar neamhaird den socrú seo nuair a bhíonn fo-lipéid seinmliosta cumasaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Taispeáin Fo-lipéid Seinmliosta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Taispeáin faisnéis bhreise do gach iontráil seinmliosta, amhail an comhlachas croí reatha agus an t-am rite (más féidir). Tá tionchar athraitheach aige ar fheidhmíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Taispeáin Deilbhíní Ábhar-Shonracha sa Stair agus sna Ceanáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Taispeáin deilbhíní sonracha do gach iontráil sa stair agus sna seinmliostaí is fearr leat. Tá tionchar athraitheach aige ar fheidhmíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Croí:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "soicindí"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Bain seinmliosta as an gcóras comhad."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Príobháideacht"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Athraigh socruithe príobháideachais."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Cuntais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Bainistigh na cuntais atá cumraithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Ainm úsáideora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Cuir isteach d'ainm úsáideora anseo. Úsáidfear é seo le haghaidh seisiúin netplay, i measc rudaí eile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Teanga"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Socraigh teanga an chomhéadain úsáideora."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Déanann sé seo an roghchlár agus na teachtaireachtaí uile ar an scáileán a shuíomh de réir na teanga a roghnaigh tú anseo. Éilíonn sé atosú chun na hathruithe a chur i bhfeidhm. \nTaispeántar iomláine an aistriúcháin in aice le gach rogha. Mura gcuirtear teanga i bhfeidhm do mhír roghchláir, is Béarla an rogha is fearr a úsáidimid."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Ceadaigh Ceamara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Lig do croíthe rochtain a fháil ar an gceamara."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Láithreacht Shaibhir Discord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Lig don aip Discord sonraí a thaispeáint faoin ábhar a imríodh. \nAr fáil leis an gcliant deisce dúchais amháin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Ceadaigh Suíomh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Lig do croíthe rochtain a fháil ar do shuíomh."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Bain amach éachtaí i gcluichí clasaiceacha. Le haghaidh tuilleadh eolais, tabhair cuairt ar 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Sonraí logála isteach do do chuntas RetroAchievements. Tabhair cuairt ar retroachievements.org agus cláraigh le haghaidh cuntas saor in aisce. \nTar éis duit clárú, ní mór duit an t-ainm úsáideora agus an focal faire a ionchur i RetroArch."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Ainm úsáideora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Cuir isteach ainm úsáideora do chuntais RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Pasfhocal"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Cuir isteach focal faire do chuntais RetroAchievements. Fad uasta: 255 carachtar."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "Eochair Sruth YouTube"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Eochair Sruth Twitch"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Eochair Srutha Cearrbhachais Facebook"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Córas/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "Stóráiltear BIOSanna, ROManna tosaithe, agus comhaid eile atá sainiúil don chóras sa chomhadlann seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Íoslódálacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Stóráiltear comhaid íoslódáilte san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Sócmhainní"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Stóráiltear sócmhainní roghchláir a úsáideann RetroArch sa chomhadlann seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Cúlraí Dinimiciúla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Stóráiltear íomhánna cúlra a úsáidtear laistigh den roghchlár sa chomhadlann seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Mionsamhlacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Stóráiltear ealaín bosca, mionsamhlacha scáileáin, agus mionsamhlacha teidil scáileáin sa chomhadlann seo."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Eolaire Tosaigh"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Socraigh Eolaire Tosaigh don Bhrabhsálaí Comhad."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Comhaid chumraíochta"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Stóráiltear an comhad cumraíochta réamhshocraithe san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Croíthe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Stóráiltear croíthe Libretro san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Eolas Croíthe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Stóráiltear comhaid feidhmchláir/eolais croí san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Bunachair Sonraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Stóráiltear bunachair shonraí san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Comhaid Aicearraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Stóráiltear comhaid aicearraí san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Scagairí Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Stóráiltear scagairí físe bunaithe ar LAP sa chomhadlann seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Scagairí Fuaime"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Stóráiltear scagairí DSP fuaime san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Scáthadóirí Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Stóráiltear scáthaitheoirí físe bunaithe ar GPU sa chomhadlann seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Taifeadtaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Stóráiltear taifeadtaí san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Cumraíochtaí Taifeadta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Stóráiltear cumraíochtaí taifeadta san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Forleagain"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Stóráiltear forleaganacha san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Forleagain Méarchláir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "Stóráiltear Forleagain Méarchláir san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Leagan Amach Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Stóráiltear Leagan Amach Físeáin san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Scáileáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Stóráiltear scáileáin sa chomhadlann seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Próifílí Rialaitheora"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Stóráiltear próifílí rialtóra a úsáidtear chun rialtóirí a chumrú go huathoibríoch sa chomhadlann seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Athmhapálacha Ionchuir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Stóráiltear athmhapálacha ionchuir san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Seinmliostaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Stóráiltear seinmliostaí san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Seinmliosta is Ansa liom"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Sábháil seinmliosta na gCeanán sa chomhadlann seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Seinmliosta Staire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Sábháil seinmliosta na Staire chuig an eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Seinmliosta Íomhánna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Sábháil seinmliosta Stair na nÍomhánna chuig an eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Seinmliosta Ceoil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Sábháil an seinmliosta Ceoil chuig an eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Seinmliosta Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Sábháil seinmliosta na bhfíseán chuig an eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Logaí Ama Rith"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Stóráiltear logaí rith-ama san eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Sábháil Comhaid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Sábháil na comhaid sábhála go léir sa chomhadlann seo. Mura socraítear é, déanfar iarracht iad a shábháil laistigh d'eolaire oibre an chomhaid ábhair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Sábháil gach comhad sábhála (*.srm) chuig an eolaire seo. Áirítear leis seo comhaid ghaolmhara ar nós .rt, .psrm, srl... Sárófar é seo le roghanna sainráite líne ordaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Sábháil Stáit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Stóráiltear stáit sábhála agus athsheinmeanna san eolaire seo. Mura socraítear é, déanfar iarracht iad a shábháil san eolaire ina bhfuil an t-ábhar suite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Taisce"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Déanfar ábhar cartlannaithe a bhaint go sealadach chuig an eolaire seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Logaí Imeachtaí Córais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Stóráiltear logaí imeachtaí córais san eolaire seo."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Cumasaigh Láithreacht Shaibhir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Roinn do stádas reatha laistigh de RetroArch ar Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Formáid Ábhar Láithreachta Saibhir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Cinneadh a dhéanamh ar an bhfaisnéis a bhaineann leis an ábhar a roinnfear."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Ábhar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Ainm lárnach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Ainm an chórais"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Ábhar (Ainm an chórais)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Ábhar (Croí ainm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Ábhar (Ainm an chórais - Ainm an chroí)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Cuir le Meascthóir"
   )

/* Netplay */


/* Netplay > Host */


/* Import Content */


/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Cuir le Meascthóir"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Nuair a bheidh sé cumasaithe, scriosfar aon seinmliosta atá ann cheana sula ndéanfar scanadh ar an ábhar. Nuair a bheidh sé díchumasaithe, coimeádtar iontrálacha seinmliosta atá ann cheana agus ní chuirfear leis ach an t-ábhar atá in easnamh ón seinmliosta faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Bailíochtú Iontrálacha atá ann cheana"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Nuair a bheidh sé cumasaithe, déanfar iontrálacha in aon seinmliosta atá ann cheana a fhíorú sula ndéanfar scanadh ar ábhar nua. Bainfear iontrálacha a thagraíonn d'ábhar atá ar iarraidh agus/nó comhaid le síntí neamhbhailí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Tosaigh Scanadh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Scanadh an t-ábhar roghnaithe."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Ag tosú liosta..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Bliain Scaoilte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Líon na nImreoirí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Réigiún"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Clib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Cuardaigh Ainm ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Taispeáin Gach Rud"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Scagaire Breise"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Gach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Cuir Scagaire Breise leis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u Míreanna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Ag Forbróir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "De réir Foilsitheora"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "De réir na Bliana Eisiúna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "De réir Líon na nImreoirí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "De réir Seánra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "De réir Éachtaí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "De réir Catagóire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "De réir Teanga"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "De réir Réigiúin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "De réir Consól eisiach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "De réir eisiach Ardáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "De réir Rumble"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "De réir Scóir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "De réir na Meán"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "De réir Rialuithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "De réir Artstyle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "De réir Gameplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "De réir Insinte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "De réir Pacála"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "De réir Peirspictíochta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "De réir Socrú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "De réir Radhairc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "De réir Feithicle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "De réir Bunús"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "De réir saincheadúnais"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "De réir Clib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "De réir Ainm an Chórais"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Socraigh Scagaire Raoin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Sábháil mar Amharc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Scrios an Radharc seo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Cuir isteach ainm an radhairc nua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Tá an radharc ann cheana féin leis an ainm céanna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "Sábháladh an radharc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "Scriosadh an radharc"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Rith"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Bain"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Eolas"
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Ainm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Croí"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Seinn Athsheinm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Seinn comhad athsheinm ón sliotán atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Athsheinm Taifeadta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Taifead comhad athsheinm chuig an sliotán atá roghnaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Stop Taifeadadh/Athsheinm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Tosaigh ag sruthú chuig an gceann scríbe roghnaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Stop a chur leis an sruthú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Deireadh srutha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Sábháil Stáit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Rochtain ar roghanna stádais sábhála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Roghanna Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Athraigh na roghanna don ábhar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Rialuithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Athraigh na rialuithe don ábhar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Aicearraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Socraigh cóid aicearraí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Rialú Diosca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Bainistíocht íomhá diosca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Scáthaitheoirí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Socraigh scáthaitheoirí chun an íomhá a mhéadú go amhairc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Sáruithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Roghanna chun an chumraíocht dhomhanda a shárú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Éachtaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Féach ar éachtaí agus socruithe gaolmhara."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Bainistigh na Roghanna Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Sábháil nó bain sáruithe roghanna don ábhar reatha."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Roghanna Sábháil Cluiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Sábháil na príomhroghanna a bheidh i bhfeidhm ar an ábhar reatha amháin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Bain Roghanna Cluiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Scrios na príomhroghanna a bheidh i bhfeidhm ar an ábhar reatha amháin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Roghanna Sábháil Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Sábháil roghanna lárnacha a bheidh i bhfeidhm ar an ábhar go léir a lódálfar ón eolaire céanna leis an gcomhad reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Bain Roghanna Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Scrios na roghanna lárnacha a bheidh i bhfeidhm ar an ábhar go léir a lódálfar ón eolaire céanna leis an gcomhad reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Comhad Roghanna Gníomhacha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "An comhad roghanna atá in úsáid faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Athshocraigh na Roghanna Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Socraigh gach rogha den chroílár reatha go luachanna réamhshocraithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Roghanna a Shruthlú go Diosca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Éilíonn scríobh socruithe reatha chuig an gcomhad roghanna gníomhacha. Cinntíonn sé seo go gcoimeádtar roghanna i gcás go mbíonn fabht lárnach ina chúis le múchadh míchuí an taobh tosaigh."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Cruthaigh Comhad Roghanna Cluiche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Sábháil Comhad Roghanna Cluiche"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Bainistigh Comhaid Athmhapála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Luchtaigh, sábháil nó bain comhaid athmhapála ionchuir don ábhar reatha."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Comhad Athmhapála Gníomhach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "An comhad athmhapála atá in úsáid faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Luchtaigh Comhad Athmhapála"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Luchtaigh agus cuir mapálacha ionchuir reatha ina n-áit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Sábháil Athmhapáil Comhad Mar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Sábháil mapálacha ionchuir reatha mar chomhad athmhapála nua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Sábháil Comhad Athmhapála Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Sábháil comhad athmhapála a bheidh i bhfeidhm ar gach ábhar a lódálfar leis an gcroí seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Bain Comhad Athmhapála Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Scrios an comhad athmhapála a bheidh i bhfeidhm ar gach ábhar a luchtófar leis an gcroí seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Sábháil Comhad Athmhapála Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Sábháil comhad athmhapála a bheidh i bhfeidhm ar an ábhar go léir a lódáladh ón eolaire céanna leis an gcomhad reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Bain Comhad Athmhapála Eolaire Ábhar Cluiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Scrios an comhad athmhapála a bheidh i bhfeidhm ar an ábhar go léir a lódáladh ón eolaire céanna leis an gcomhad reatha."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */


/* Quick Menu > Cheats */


/* Quick Menu > Cheats > Start or Continue Cheat Search */


/* Quick Menu > Cheats > Load Cheat File (Replace) */


/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Innéacs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Seasamh aicearra sa liosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Cumasaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Cur síos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Láimhseálaí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Méid Cuardaigh Cuimhne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Cineál"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Luach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Seoladh Cuimhne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Brabhsáil Seoladh: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Masc Seoladh Cuimhne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Masc giotán an seoladh nuair a bhíonn Méid Cuardaigh Cuimhne < 8 ngiotán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Líon na nAthruithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Líon na n-uaireanta a chuirfear an cheat i bhfeidhm. Úsáid leis an dá rogha eile 'Athrá' chun tionchar a imirt ar limistéir mhóra cuimhne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Seoladh Méadú Gach Athrá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Tar éis gach athrá, méadófar an 'Seoladh Cuimhne' faoin uimhir seo iolraithe faoin 'Méid Cuardaigh Cuimhne'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Méadú Luach Gach Athrá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Tar éis gach athrá méadófar an 'Luach' faoin méid seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Creathadh ar chuimhne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Luach Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Port Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Neart Príomhúil Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Fad Príomhúil an Creathadh (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Neart Dara Leibhéal Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Fad Dara Leibhéal an Creathadh (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Cód"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Cuir Aicearra Nua leis Tar éis Seo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Cuir Aicearra Nua leis Roimh Seo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Cóipeáil an Aicearra seo Tar éis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Cóipeáil an Aicearra seo Roimhe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Bain an Aicearra seo"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Díbirt Diosca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Oscail an tráidire diosca fíorúil agus bain an diosca atá luchtaithe faoi láthair. Má tá 'Cuir an t-ábhar ar sos nuair a bhíonn an roghchlár gníomhach' cumasaithe, ní fhéadfaidh roinnt croíleacán athruithe a chlárú mura n-atosófar an t-ábhar ar feadh cúpla soicind tar éis gach gníomh rialaithe diosca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Cuir Diosca isteach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Cuir isteach an diosca a fhreagraíonn don 'Innéacs Diosca Reatha' agus dún an tráidire diosca fíorúil. Má tá 'Cuir an t-ábhar ar sos nuair a bhíonn an roghchlár gníomhach' cumasaithe, ní fhéadfaidh roinnt croíleacán athruithe a chlárú mura n-atosófar an t-ábhar ar feadh cúpla soicind tar éis gach gníomh rialaithe diosca."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Luchtaigh Diosca Nua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Díbirt an diosca reatha, roghnaigh diosca nua ón gcóras comhad ansin cuir isteach é agus dún an tráidire diosca fíorúil.\nNÓTA: Is gné oidhreachta í seo. Moltar ina ionad sin teidil ildhiosca a luchtú trí seinmliostaí M3U, a cheadaíonn roghnú diosca ag baint úsáide as na roghanna 'Díbirt/Cuir isteach Diosca' agus 'Innéacs Diosca Reatha'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Roghnaigh diosca nua ón gcóras comhad agus cuir isteach é gan an tráidire diosca fíorúil a dhúnadh.\nNÓTA: Is gné oidhreachta í seo. Moltar ina ionad sin teidil ildhiosca a luchtú trí seinmliostaí M3U, a cheadaíonn roghnú diosca ag baint úsáide as an rogha 'Innéacs Diosca Reatha'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Innéacs an Diosca Reatha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Roghnaigh an diosca reatha ón liosta d’íomhánna atá ar fáil. Luchtófar an diosca nuair a roghnófar 'Cuir Diosca Isteach'."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Scáthadóirí Físeáin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Cumasaigh píblíne scáthaithe físe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Faire ar Chomhaid Scáthóra le haghaidh Athruithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Cuir athruithe a rinneadh ar chomhaid scáthaithe ar dhiosca i bhfeidhm go huathoibríoch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Bí ag faire amach do chomhaid scáthaithe le haghaidh athruithe nua. Tar éis athruithe ar scáthaithe a shábháil ar dhiosca, déanfar é a aththiomsú go huathoibríoch agus a chur i bhfeidhm ar an ábhar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Cuimhnigh ar an Eolaire Scáthóirí a Úsáideadh Deireanach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Oscail an Brabhsálaí Comhad san eolaire deireanach a úsáideadh agus réamhshocruithe agus pasanna scáthaithe á luchtú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Luchtaigh Réamhshocrú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Luchtaigh réamhshocrú scáthaithe. Socrófar an píblíne scáthaithe go huathoibríoch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Luchtaigh réamhshocrú scáthaithe go díreach. Nuashonraítear roghchlár an scáthaithe dá réir.\nNí bhíonn an fachtóir scálúcháin a thaispeántar sa roghchlár iontaofa ach amháin má úsáideann an réamhshocrú modhanna scálúcháin simplí (i.e. scálú foinse, an fachtóir scálúcháin céanna do X/Y)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Réamhshocrú a Chur Roimh"
   )

/* Quick Menu > Shaders > Save */




/* Quick Menu > Shaders > Remove */


/* Quick Menu > Shaders > Shader Parameters */


/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Bain Sáruithe Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Scrios an comhad cumraíochta sáraithe a bheidh i bhfeidhm ar gach ábhar a lódálfar leis an gcroí seo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Sábháil Sáruithe Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Sábháil comhad cumraíochta sáraithe a bheidh i bhfeidhm ar an ábhar go léir a lódálfar ón eolaire céanna leis an gcomhad reatha. Beidh tosaíocht aige seo thar an bpríomhchumraíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Bain Sáruithe Eolaire Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Scrios an comhad cumraíochta sáraithe a bheidh i bhfeidhm ar gach ábhar a lódálfar ón eolaire céanna leis an gcomhad reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Sábháil Forruithe Cluiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Sábháil comhad cumraíochta sáraithe a bheidh i bhfeidhm ar an ábhar reatha amháin. Beidh tosaíocht aige seo thar an bpríomhchumraíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Bain For-Riaráidí Cluiche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Scrios an comhad cumraíochta sáraithe a bheidh i bhfeidhm ar an ábhar reatha amháin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "Díluchtaigh an Sárú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "Athshocraigh na roghanna go léir go luachanna cumraíochta domhanda."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Gan aon éachtaí le taispeáint"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Cealaigh Sos Éachtaí Mód Crua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Fág mód crua-éachtaí cumasaithe don seisiún reatha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Cealaigh Lean ar aghaidh le Gnóthachtálacha Mód croí crua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Fág mód croí-chrua éachtaí díchumasaithe don seisiún reatha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Sos Éachtaí Mód croí crua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Cuir an mód croí-chroí éachtaí ar sos don seisiún reatha. Cumasóidh an gníomh seo stáit aicearra, athchasadh, gluaiseacht mhall, agus luchtú sábhála."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Mód Crua-Éachtaí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Atosaigh mód croí crua don seisiún reatha. Díchumasóidh an gníomh seo cleasanna, athchasadh, gluaiseacht mhall, agus stáit sábhála luchtaithe agus athshocróidh sé an cluiche reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Ní féidir teacht ar fhreastalaí RetroAchievements"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Níor éirigh le díghlasáil éachta amháin nó níos mó an freastalaí a bhaint amach. Déanfar iarracht eile ar na díghlasálacha chomh fada agus a fhágfaidh tú an aip ar oscailt."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "Ní féidir teacht ar fhreastalaí RetroAchievements. Déanfaidh sé iarracht eile go n-éireoidh leis nó go ndúnfar an aip."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "Tá gach iarratas ar feitheamh sioncrónaithe go rathúil le freastalaí RetroAchievements."
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "Cluiche a aithint"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "Ag fáil sonraí cluiche"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "Seisiún á thosú"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Níl logáilte isteach"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Earráid Líonra"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Cluiche Anaithnid"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Ní féidir éachtaí a ghníomhachtú leis an croí seo"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "Hais RetroAchievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Iontráil Bunachar Sonraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Taispeáin faisnéis bhunachar sonraí don ábhar reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Gan aon Iontrálacha le Taispeáint"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Níl Croíthe ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Níl aon Roghanna Croí ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Níl aon fhaisnéis Croi ar fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Níl aon Chúltacaí Croí ar fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Níl Ceanáin ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Gan Stair ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Níl aon Íomhánna ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Níl Ceol ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Níl Físeáin ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Níl Eolas ar Fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Níl aon iontrálacha seinmliosta ar fáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Níor aimsíodh aon socruithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Níor aimsíodh aon ghléasanna Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Níor aimsíodh aon Líonraí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Gan Croí"
   )

/* Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad le Analógach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Anaithnid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Síos + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Coinnigh Start (2 shoicind)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Coinnigh Select (2 shoicind)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Síos + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Díchumasaithe>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Athruithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Ní Athraíonn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Méaduithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Laghdaíonn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= Luach Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= Luach Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< Luach Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> Luach Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Méadaíonn Luach Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Laghdaíonn Luach Creathadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Gach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Díchumasaithe>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Socraigh go Luach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Méadú de réir Luach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Laghdú de réir Luach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Rith an Chéad Cheat Eile Má Luach = Cuimhne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Rith an Chéad Cheat Eile Má Luach != Cuimhne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Rith an Chéad Cheat Eile Má Luach < Cuimhne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Rith an Chéad Cheat Eile Má Luach> Cuimhne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Aithriseoir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Giotán, Uasluach = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Ealaín Bosca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Seat scáileáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Scáileán Teidil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Cumasaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Ábhar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Luch 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Roth na Luiche Suas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Roth na Luiche Síos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Roth na Luiche ar Chlé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Roth na Luiche ar Dheis"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Go luath"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Go déanach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "BBBB-MM-LL UU:NN:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "BBBB-MM-LL UU:NN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "BBBB-MM-LL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "BBBB-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "MM-LL-BBBB UU:NN:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "MM-LL-BBBB UU:NN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "MM-LL UU::NN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "MM-LL-BBBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD,
   "MM-LL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "LL-MM-BBBB UU:NN:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "LL-MM-BBBB UU:NN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "LL-MM UU:NN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "LL-MM-BBBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM,
   "LL-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS,
   "UU:NN:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM,
   "UU-NN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "BBBB-MM-LL UU:NN:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "BBBB-MM-LL UU:NN (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "MM-LL-BBBB UU:NN:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "MM-LL-BBBB UU:NN (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "MM-LL UU:NN (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "LL-MM-BBBB UU:NN:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "LL-MM-BBBB UU:NN (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "LL-MM UU:NN (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM,
   "UU:NN:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM,
   "UU:NN (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "Ó shin"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Tiús Líontóir Cúlra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Méadaigh garbhacht phatrún seiceála chúlra an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Líontóir Teorann"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Tiús Líontóir Teorann"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Méadaigh garbhacht imeall an roghchláir ar chlár seiceála."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Taispeáin teorainn an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Úsáid Leagan Amach Lánleithead"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Athraigh méid agus suíomh iontrálacha an roghchláir chun an úsáid is fearr a bhaint as an spás scáileáin atá ar fáil. Díchumasaigh é seo chun leagan amach clasaiceach dhá cholún leithead seasta a úsáid."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Scagaire Líneach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Cuireann sé beagán doiléire leis an roghchlár chun imill chrua picteilíní a mhaolú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Uasghrádú Inmheánach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Uasghrádaigh comhéadan an roghchláir sula dtarraingítear chuig an scáileán. Nuair a úsáidtear é agus 'Scagaire Líneach Roghchláir' cumasaithe, baintear déantáin scálúcháin (picteilíní míchothroma) agus coinnítear íomhá ghéar. Tá tionchar suntasach aige ar fheidhmíocht a mhéadaíonn de réir mar a uasghrádaítear an leibhéal uasghrádaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Cóimheas Gnéithe"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Roghnaigh cóimheas gné an roghchláir. Méadaíonn cóimheasa scáileáin leathana taifeach cothrománach chomhéadan an roghchláir. (D’fhéadfadh atosú a bheith ag teastáil má tá 'Glasáil Cóimheas Gné an Roghchláir' díchumasaithe)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Cóimheas Gnéithe a Ghlasáil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Cinntíonn sé seo go bhfuil an cóimheas gné ceart ar an roghchlár i gcónaí. Má tá sé díchumasaithe, sínfear an roghchlár tapa chun go mbeidh sé ag teacht leis an ábhar atá luchtaithe faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Téama Dathanna"
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (Láraithe)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Oiriúnaigh an Scáileán"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Scála Slánuimhir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Líon an Scáileán (Sínte)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Dearg Clasaiceach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Oráiste Clasaiceach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Buí Clasaiceach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Glas Clasaiceach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Gorm Clasaiceach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Violet Clasaiceach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Liath Clasaiceach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Dearg Oidhreachta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Corcra Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Gorm Meán Oíche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Órga"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Gorm Leictreach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Úll Glas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Dearg Bolcánach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Lagún"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "Floss an tSíog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Solas Gruvbox"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Ag Hackáil an Eithne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
   "Aon Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized Solas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Solas Tango"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dinimiciúil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Liath Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Solas Liath"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Sneachta (Éadrom)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Sneachta (Trom)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Báisteach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Réimse Réalt"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Cineál mionsamhail le taispeáint ar chlé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "Mionsamhail Deilbhín"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "Cineál mionsamhail deilbhín seinmliosta le taispeáint."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Cúlra Dinimiciúil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Luchtaigh páipéar balla nua go dinimiciúil ag brath ar an gcomhthéacs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Beochan Cothrománach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Cumasaigh beochan cothrománach don roghchlár. Beidh tionchar aige seo ar fheidhmíocht."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Téama Dathanna"
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Úll Glas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Bhíoma gréine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Corcra Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Gorm Leictreach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Órga"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Dearg Oidhreachta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Gorm Meán Oíche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Íomhá Chúlra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Faoin bhFarraige"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Dearg Bolcánach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Glas Aoil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Pikachu Buí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Ciúb Corcra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Dearg Teaghlaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Te Lasrach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Oighear Fuar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Liath Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Solas Liath"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE,
   "Sainigh an gcaithfidh méid an chló sa roghchlár a scálú féin a bheith aige, agus an gcaithfidh sé a bheith scálaithe go domhanda nó le luachanna ar leithligh do gach cuid den roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_GLOBAL,
   "Domhanda"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_SEPARATE,
   "Luachanna ar leithligh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Fachtóir Scála Cló"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Scálaigh méid an chló go líneach ar fud an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Fachtóir Scála Cló Teidil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Scálaigh méid an chló don téacs teidil i gceanntásc an roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Fachtóir Scála Cló Barra Taobh Clé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Scálaigh méid an chló don téacs sa bharra taoibh clé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Lipéid Fachtóir Scála Cló"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Scálaigh méid an chló do lipéid na roghanna roghchláir agus na n-iontrálacha seinmliosta. Bíonn tionchar aige seo freisin ar mhéid an téacs sna boscaí cabhrach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Fachtóir Scála Cló Fo-lipéid"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Scálaigh méid an chló do na fo-lipéid de roghanna roghchláir agus iontrálacha seinmliosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TIME,
   "Fachtóir Scála Cló Dáta Ama"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TIME,
   "Scálaigh méid cló an táscaire ama agus dáta sa chúinne uachtarach ar dheis den roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Fachtóir Scála Cló Buntásc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Scálaigh méid cló an téacs i mbunús an roghchláir. Bíonn tionchar aige seo freisin ar mhéid an téacs sa bharra taoibh mionsamhlacha ar dheis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Laghdaigh an Barra Taobh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Bíodh an barra taoibh clé fillte i gcónaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Gearr Ainmneacha Seinmliostaí (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Bain ainmneacha na monaróirí as na seinmliostaí. Mar shampla, athraíonn 'Sony - PlayStation' go 'PlayStation'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Sórtáil Seinmliostaí i ndiaidh Gearradh Ainm (Atosú ag teastáil)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Déanfar seinmliostaí a athshórtáil in ord aibítre tar éis an chomhpháirt mhonaróra a bhaint dá n-ainmneacha."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Cuir mionsamhail eile in ionad phainéal meiteashonraí an ábhair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Úsáid Téacs Ticker le haghaidh Meiteashonraí Ábhair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Nuair a bheidh sé cumasaithe, beidh gach mír meiteashonraí ábhair a thaispeántar ar bharra taoibh deas na seinmliostaí (croílár gaolmhar, am imeartha) ar líne amháin; taispeánfar teaghráin a sháraíonn leithead an bharra taoibh mar théacs ticéad scrollaithe. Nuair a bheidh sé díchumasaithe, taispeánfar gach mír meiteashonraí ábhair go statach, fillte chun an oiread línte agus is gá a áitiú."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Scálaigh méid an bharra mionsamhlacha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_PADDING_FACTOR,
   "Fachtóir Líonta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_PADDING_FACTOR,
   "Scálaigh méid an líonadh cothrománach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR,
   "Deighilteoir Ceanntásca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_SEPARATOR,
   "Leithead malartach do dheighilteoirí ceanntásca agus buntásca."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Téama Dathanna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Roghnaigh téama dathanna difriúil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Bán Bunúsach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Dubh Bunúsach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Ag Hackáil an Eithne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "Crios na Twilight"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solarized Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized Solas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Liath Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Solas Liath"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN,
   "Báisteach Corcra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SELENIUM,
   "Seiléiniam"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Taispeáin deilbhíní ar chlé na n-iontrálacha roghchláir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Téama Dathanna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Cúlraí Mionsamhlacha"
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Ag Hackáil an Eithne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Liath Dorcha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Solas Liath"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Eolas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "S&coir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Cuir in Eagar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Cuardaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Amharc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Dugaí Dúnta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Socruithe..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Cuimhnigh suíomhanna na ndugaí:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Cuimhnigh geoiméadracht na fuinneoige:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Cuimhnigh ar an gcluaisín brabhsálaí ábhair deireanach:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Téama:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Réamhshocrú an Chórais>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Saincheaptha..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Socruithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Uirlisí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Cabhair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "Maidir le RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Doiciméadú"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Luchtaigh Croi Saincheaptha..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Luchtaigh Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Croí á lódáil..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Ainm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Leagan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Seinmliostaí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Brabhsálaí Comhad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Barr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Suas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Brabhsálaí Ábhair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Ealaín Bosca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Seat scáileáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Scáileán Teidil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO,
   "Lógó"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Eolas Croíthe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Fiafraigh díom>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Eolas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Rabhadh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Earráid"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Earráid Líonra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Atosaigh an clár le go dtiocfaidh na hathruithe i bhfeidhm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Logáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 mír"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Scaoil íomhá anseo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Ná taispeáin seo arís"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Stad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Comhlach Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Seinmliostaí Folaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Folaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Dath aibhsithe:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Roghnaigh..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Roghnaigh Dath"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Roghnaigh Téama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Téama Saincheaptha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Tá an cosán comhaid bán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Tá an comhad folamh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Níorbh fhéidir an comhad a oscailt le haghaidh léitheoireachta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Níorbh fhéidir an comhad a oscailt le haghaidh scríbhneoireachta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Níl an comhad ann."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Moltar croí a luchtú ar dtús:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Zúmáil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Croí:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Bain"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Ní théann aon scáthóir tharainn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Athshocraigh Pas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Athshocraigh Gach Pas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Athshocraigh Paraiméadar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Íoslódáil mionsamhail"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Tá íoslódáil ar siúl cheana féin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Tosaigh ar seinmliosta:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Mionsamhail"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Teorainn taisce mionsamhlacha:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Teorainn mhéide mionsamhlacha tarraing-agus-titim:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Íoslódáil Gach Mionsamhail"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Córas Iomlán"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "An Seinmliosta seo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Íoslódáladh na mionsamhlacha go rathúil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "D'éirigh leis: %1 Theip air: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Roghanna Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Athshocraigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Athshocraigh Gach Rud"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Socruithe Nuashonraitheora Lárnach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Cuntais Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Críochphointe Liosta na gCuntas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Turbo/Crios Marbh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Áiritheoirí Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Níl aon diosca roghnaithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Áiritheoirí Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Roghchlár Cothrománach"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Folaigh Tuairiscí Ionchuir Croí Neamhcheangailte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Lipéid Tuairiscí Ionchuir Taispeáine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Forleagan ar an Scáileán"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Stair"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Roghnaigh ábhar ó seinmliosta staire le déanaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Nuair a luchtaítear ábhar, sábháiltear teaglamaí croí-ábhar agus libretro sa stair.\nSábháiltear an stair i gcomhad san eolaire céanna leis an gcomhad cumraíochta RetroArch. Mura luchtáladh aon chomhad cumraíochta ag an am tosaithe, ní shábhálfar ná ní luchtófar an stair, agus ní bheidh sí sa phríomh-roghchlár."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "Ilmheáin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Fochórais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Rochtain ar shocruithe fochórais don ábhar reatha."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Ábhar Reatha: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Ní bhfuarthas aon óstach netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Ní bhfuarthas aon chliaint netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Gan aon chomhaireamh feidhmíochta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Gan aon seinmliostaí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Ceangailte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "Ar líne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Port %d Ainm Gléas: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Ainm Taispeána Gléis: %s\nAinm Cumraíochta Gléis: %s\nVID/PID Gléis: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Socruithe Aicearra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Seinn i Seinnteoir Meán"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "soicindí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Tosaigh Croi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Tosaigh croí gan ábhar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Croíthe molta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Ní féidir comhad comhbhrúite a léamh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Úsáideoir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Sioncrónaigh an LAP agus an GPU go crua. Laghdaíonn sé moill ar chostas feidhmíochta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Tosaigh"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Cabhair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Cur síos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Lean ar aghaidh leis an gCuardach"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Lean ort ag cuardach le haghaidh meabhlaireachta nua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Éachtaí (Crua-Chrua)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Bainistíonn sé socruithe sonraí na gcleasanna."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Tosaigh nó lean ar aghaidh le cuardach cód aicearra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Pasanna Aicearraí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Méadaigh nó laghdaigh líon na gceapadóireachtaí."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Analógach Clé X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Analógach Clé Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Analógach Deas X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Analógach Deas Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Liosta Cúrsóra Bunachar Sonraí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Bunachar Sonraí - Scagaire: Forbróir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Bunachar Sonraí - Scagaire: Foilsitheoir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Bunachar Sonraí - Scagaire: Bunús"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Bunachar Sonraí - Scagaire: Saincheadúnas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Bunachar Sonraí - Scagaire: Rátáil ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Bunachar Sonraí - Scagaire: Rátáil ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Bunachar Sonraí - Scagaire: Rátáil PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Bunachar Sonraí - Scagaire: Rátáil CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Bunachar Sonraí - Scagaire: Rátáil BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Bunachar Sonraí - Scagaire: Uasmhéid Úsáideoirí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Bunachar Sonraí - Scagaire: Dáta Eisiúna de réir Míosa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Bunachar Sonraí - Scagaire: Dáta Eisiúna de réir Bliana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Bunachar Sonraí - Scagaire: Eagrán Iris Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Bunachar Sonraí - Scagaire: Rátáil Iris Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Eolas Bunachar Sonraí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Cumraíocht"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Íoslódálacha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Socruithe Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Tacaíocht do Bhéarlagar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "Tacaíocht rindreála-go-uigeacht OpenGL/Direct3D (scáthadóirí ilphas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Ábhar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "De ghnáth socraítear iad ag forbróirí a chuireann aipeanna libretro/RetroArch i bpacáistí chun pointeáil chuig sócmhainní."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Iarr"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Rialuithe bunúsacha roghchláir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Deimhnigh/Ceart go leor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "Eolas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "Scoir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Scrollaigh Suas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Réamhshocruithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Éadromaigh an Méarchlár"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Roghchlár a Athrú"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "Sa Roghchlár"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "Sa Chluiche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "Sa Chluiche (Ar Sos)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Ag imirt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Sosaithe"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Tosóidh Netplay nuair a bheidh an t-ábhar lódáilte."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "Ní mór ábhar a luchtú sula dtosaíonn tú ag imirt ar an ngréasán."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Níorbh fhéidir comhad croí nó ábhair oiriúnach a aimsiú, luchtáil de láimh."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Níl do thiománaí grafaicí comhoiriúnach leis an tiománaí físe reatha i RetroArch, agus úsáidtear an tiománaí %s. Atosaigh RetroArch le go dtiocfaidh na hathruithe i bhfeidhm."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Suiteáil lárnach rathúil"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Theip ar shuiteáil an chroí"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Brúigh ar dheis cúig huaire chun gach aicearraí a scriosadh."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Imleabhar meascthóra fuaime domhanda"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Ag imirt"
   )

MSG_HASH(
   MSG_CAPABILITIES,
   "Cumais"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Ag ceangal le hóstach netplay"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Ag nascadh le port"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Sliotán nasc"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "Ag fáil an liosta croí..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Theip ar an liosta croí a aisghabháil!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "An leagan is déanaí suiteáilte cheana féin: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Croílár a nuashonrú: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Croílár á íoslódáil: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Croí a bhaint amach "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Croí suiteáilte: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Theip ar an gcroí a shuiteáil: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Croíleacáin á scanadh..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Ag seiceáil croí: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Gach croíleacán suiteáilte ag an leagan is déanaí"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Aistríodh na croíleacáin uile a dtacaítear leo chuig leaganacha Play Store"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "croíleacáin nuashonraithe: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "croíleacáin scipeáilte: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Nuashonrú croí díchumasaithe - tá an croí faoi ghlas: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Croíleacáin a athshocrú: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Athshocrú croíleacán: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Glanadh seinmliosta: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Seinmliosta glanta: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Theip ar an athnuachan - níl aon taifead scanadh bailí sa seinmliosta: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Theip ar athnuachan - eolaire ábhair neamhbhailí/ar iarraidh: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Theip ar athnuachan - ainm córais neamhbhailí/ar iarraidh: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Theip ar athnuachan - croí neamhbhailí: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Theip ar athnuachan - comhad DAT stuara neamhbhailí/ar iarraidh: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Theip ar athnuachan - comhad DAT an stua ró-mhór (cuimhne neamhleor): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Curtha le ceanáin"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Theip ar an rogha is fearr leat a chur leis: seinmliosta lán"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "Curtha leis an seinmliosta"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Theip ar chur leis an seinmliosta: seinmliosta lán"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Tacar croí: "
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Diosca curtha leis"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Theip ar an diosca a chur leis"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Eolaire Feidhmchláir"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Ag cur athruithe aicearra i bhfeidhm."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Ag cur paiste i bhfeidhm: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Scáthadóir á chur i bhfeidhm"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Fuaim múchta."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Fuaim díbholgtha."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Earráid ag sábháil phróifíl an rialaitheora."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Sábháileadh próifíl an rialaitheora go rathúil."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "Próifíl rialtóra sábháilte in eolaire Próifílí Rialaitheora mar\n\"%s\""
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Níorbh fhéidir an t-uathshábháil a thosú."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Sábháil uathoibríoch an stáit chuig"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Comhéadan ordaithe a thabhairt suas ar an bport"
   )
MSG_HASH(
   MSG_BYTES,
   "bearta"
   )
MSG_HASH(
   MSG_ERROR,
   "Earráid"
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Theip ar réamhshocrú scáthaithe a chur i bhfeidhm:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Theip ar an soicéad a cheangal."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Theip ar chruthú an eolaire."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Theip ar ábhar a bhaint as comhad comhbhrúite"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Theip ar an leasainm a fháil ón gcliant."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Theip ar an luchtú"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Theip ar an ábhar a lódáil"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Theip ar an gcomhad scannáin a luchtú"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Theip ar an forleagan a lódáil."
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "Níl forleagan méarchláir socraithe."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Theip ar an stát a lódáil ó"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Theip ar chroílár an libretro a oscailt"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Theip ar an bpaisteáil"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Theip ar an gceannteideal a fháil ón gcliant."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Theip ar an leasainm a fháil."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Theip ar an leasainm a fháil ón óstach."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Theip ar mhéid an leasainm a fháil ón óstach."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "Theip ar shonraí SRAM a fháil ón óstach."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Theip ar an diosca a bhaint as an tráidire."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Theip ar an gcomhad sealadach a bhaint"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "Theip ar SRAM a shábháil"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "Theip ar luchtú SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Theip ar an stát a shábháil chuig"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Theip ar an leasainm a sheoladh."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Theip ar mhéid an leasainm a sheoladh."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Theip ar an leasainm a sheoladh chuig an gcliant."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Theip ar an leasainm a sheoladh chuig an óstach."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "Theip ar shonraí SRAM a sheoladh chuig an gcliant."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Theip ar thosú an tiománaí fuaime. Leanfaidh sé ar aghaidh gan fuaim."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Theip ar thosú taifeadadh scannáin."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Theip ar an taifeadadh a thosú."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Theip ar an seat scáileáin a thógáil."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Theip ar an staid lódála a chealú."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Theip ar an staid shábháilte a chealú."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Theip ar an bhfuaim a dhíbholadh."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Earráid mharfach a fuarthas i"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Níor aimsíodh an comhad"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Fuarthas staid uath-shábháil i"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Lipéad diosca aimsithe"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Aimsíodh an chéad rian sonraí ar chomhad"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Fuarthas an sliotán stáit deireanach"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "Fuarthas an sliotán athimeartha deireanach"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "Ní ón taifeadadh reatha"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "Ní comhoiriúnach le hathsheinm"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Scáthadóir aimsithe"
   )
MSG_HASH(
   MSG_FRAMES,
   "Frámaí"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Roghanna croí-shonracha don chluiche a fhaightear ag"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Roghanna croí-shonracha fillteáin a fhaightear ag"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Fuarthas innéacs diosca neamhbhailí."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Stádas greim na luiche"
   )
MSG_HASH(
   MSG_RESET,
   "Athshocraigh"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Anaithnid"
   )

/* Lakka */


/* Environment Specific Settings */

#ifdef HAVE_LIBNX
#endif
#ifdef HAVE_LAKKA
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
#endif
#ifdef HAVE_QT
#endif
#ifdef HAVE_GAME_AI





#endif

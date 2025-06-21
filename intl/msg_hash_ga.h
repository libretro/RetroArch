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
   "Croíleacáin gan ábhar"
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
   "Díluchtaigh an Croí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Scaoil an croí luchtaithe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Brabhsáil le haghaidh cur i bhfeidhm lárnach libretro. Braitheann an áit a dtosaíonn an brabhsálaí ar chonair d'Eolaire Croí. Má tá sé bán, tosóidh sé sa bhfréamh.\nMás eolaire é an tEolaire Croí, úsáidfidh an roghchlár é sin mar an fillteán barr. Más cosán iomlán é an tEolaire Croí, tosóidh sé san fhillteán ina bhfuil an comhad."
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
   "Brabhsáil le haghaidh ábhair. Chun ábhar a luchtú, teastaíonn 'Croílár' uait le húsáid, agus comhad ábhair.\nChun rialú a dhéanamh ar an áit a dtosaíonn an roghchlár ag brabhsáil le haghaidh ábhair, socraigh 'Eolaire Brabhsálaí Comhad'. Mura bhfuil sé socraithe, tosóidh sé sa fhréamh.\nScagfaidh an brabhsálaí síntí amach don chroílár deireanach a socraíodh i 'Luchtaigh Croílár', agus úsáidfidh sé an croílár sin nuair a luchtófar ábhar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Luchtaigh Diosca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Luchtaigh diosca meán fisiciúil. Roghnaigh an croí (Load Core) ar dtús le húsáid leis an diosca."
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
   "Íoslódáil Croílár"
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
   "Croíleacáin gan ábhar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Beidh croíleacáin suiteáilte ar féidir leo oibriú gan ábhar a luchtú le feiceáil anseo."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Íoslódálaí Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Nuashonraigh Croíleacáin Suiteáilte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Nuashonraigh na croíleacáin suiteáilte go léir go dtí an leagan is déanaí atá ar fáil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Athraigh Croíleacáin go Leaganacha Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Cuir na leaganacha is déanaí ón Play Store in ionad na gcroíleacáin oidhreachta agus na gcroíleacáin atá suiteáilte de láimh, más féidir."
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
   "Íoslódálaí Comhad an Chórais Lárnaigh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Íoslódáil comhaid chórais chúnta atá riachtanach le haghaidh oibriú lárnach ceart/optamach."
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
   "Féach ar fhaisnéis a bhaineann leis an bhfeidhmchlár/croílár."
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
   "Eisiamh ón Roghchlár 'Croíleácha Gan Ábhar'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Cosc a chur ar an gcroílár seo a bheith á thaispeáint sa chluaisín/roghchlár 'Croílár Gan Ábhar'. Ní bhaineann sé seo ach amháin nuair a bhíonn an modh taispeána socraithe go 'Saincheaptha'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Scrios an Chroí"
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
   "Cruthaigh cúltaca cartlannaithe den chroílár atá suiteáilte faoi láthair."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Athchóirigh Cúltaca"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Suiteáil leagan roimhe seo den chroílár ó liosta cúltaca cartlannaithe."
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
   "Croíleacáin LAP"
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
   "Athraigh socruithe lárnacha."
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
   "Croílár Libretro. Trí é seo a roghnú, nascfar an croílár seo leis an gcluiche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Croílár Libretro. Roghnaigh an comhad seo le go luchtóidh RetroArch an croílár seo."
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
   "Tiománaí SDL 2 rindreáilte le bogearraí. Braitheann feidhmíocht do chur i bhfeidhm lárnacha libretro rindreáilte le bogearraí ar chur i bhfeidhm SDL d'ardáin."
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
   "Cuir scagaire físe faoi thiomáint LAP i bhfeidhm. D’fhéadfadh costas ard feidhmíochta a bheith i gceist. D’fhéadfadh roinnt scagairí físe a bheith ag obair ach amháin le croíleacáin a úsáideann dath 32-giotán nó 16-giotán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Cuir scagaire físe faoi thiomáint LAP i bhfeidhm. D’fhéadfadh costas ard feidhmíochta a bheith i gceist. D’fhéadfadh roinnt scagairí físe a bheith ag obair ach amháin le croíleacáin a úsáideann dath 32-giotán nó 16-giotán. Is féidir leabharlanna scagairí físe atá nasctha go dinimiciúil a roghnú."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Cuir scagaire físe faoi thiomáint LAP i bhfeidhm. D’fhéadfadh costas ard feidhmíochta a bheith leis. D’fhéadfadh roinnt scagairí físe a bheith ag obair ach amháin le croíleacáin a úsáideann dath 32-giotán nó 16-giotán. Is féidir leabharlanna scagairí físe ionsuite a roghnú."
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
   "Cuireann sé rothlú áirithe ar an bhfíseán i bhfeidhm. Cuirtear an rothlú leis na rothluithe a shocraíonn an croílár."
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
   "Scálú físeán i gcéimeanna slánuimhir amháin. Braitheann an méid bonn ar gheoiméadracht agus ar chóimheas gné a thuairiscítear sa chroílár."
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
   "Gan aon diall ón am a iarradh sa chroílár. Úsáid le haghaidh scáileáin Ráta Athnuachana Athraitheach (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Sioncrónaigh le Ráta Fráma Beacht an Ábhair. Is ionann an rogha seo agus luas x1 a fhorchur agus luas ar aghaidh á cheadú fós. Gan aon diall ón ráta athnuachana lárnach a iarrtar, gan aon fhuaim Rialú Ráta Dinimiciúil."
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
   "Cumasaigh ionchur fuaime i gcroílár tacaithe. Níl aon fhorchostais ann mura bhfuil micreafón á úsáid ag an gcroílár."
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


/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Sioncrónú"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Ionchur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Aschur"
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */


/* Settings > Audio > Menu Sounds */


/* Settings > Input */

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

/* Settings > Input > Haptic Feedback/Vibration */


/* Settings > Input > Menu Controls */


/* Settings > Input > Hotkeys */

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
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Seirbhís AI"
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
   "Bain úsáid as an maide analógach sonraithe le haghaidh ionchuir D-Pad. Sáraíonn móid 'éigeantacha' an t-ionchur analógach dúchasach lárnach."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Mapáil bata analógach sonraithe le haghaidh ionchur D-Pad.\nMá tá tacaíocht dhúchasach analógach ag an gcroílár, díchumasófar mapáil D-Pad mura roghnaítear rogha '(Éignithe)'.\nMá dhéantar mapáil D-Pad a éigeantú, ní bhfaighidh an croílár aon ionchur analógach ón mbata sonraithe."
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
   "Sonraíonn sé cén port lárnach a gheobhaidh ionchur ó phort rialtóra tosaigh %u."
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

/* Settings > Latency */

#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
#endif

/* Settings > Core */

#ifndef HAVE_DYNAMIC
#endif
#ifdef HAVE_MIST


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
   "Taispeáin 'Bainistigh croíleacáin'"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Taispeáin an rogha 'Bainistigh croíleacáin' sa Phríomh-Roghchlár."
)


MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "Díshuiteálfar an croí nuair a scoirfear RetroArch."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "Tá an croílár á íoslódáil faoi láthair"
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

/* Settings > Logging */


/* Settings > File Browser */


/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Athchasadh"
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
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Cáilíocht Sruthaithe"
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

/* Settings > On-Screen Display > On-Screen Overlay */


#if defined(ANDROID)
#endif
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
   "Bain úsáid as aon ionchur tadhaill nach bhfuil ag brú rialuithe forleagan chun ionchur gléas pointeála a chruthú don chroílár."
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
   "Socraigh an port lárnach chun ionchur a fháil ón gunna solais forleagan."
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


/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Fógraí ar an Scáileán"
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

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Roghchlár Tapa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Socruithe"
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
   "Taispeáin an cumas chun croíleacáin (agus comhaid eolais croíleacáin) a nuashonrú sa rogha 'Nuashonraitheoir Ar Líne'."
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
   "Sonraigh an cineál croí (más ann dó) atá le taispeáint sa roghchlár 'Croíleacáin Gan Ábhar'. Nuair atá sé socraithe go 'Saincheaptha', is féidir infheictheacht croí aonair a athrú tríd an roghchlár 'Bainistigh Croí'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Aonúsáide"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Taispeáin Dáta agus Am"
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


/* Settings > User Interface > Appearance */


/* Settings > AI Service */


/* Settings > Accessibility */

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

/* Settings > Network */


/* Settings > Network > Updater */


/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Stair"
   )

/* Settings > Playlists > Playlist Management */


/* Settings > User */


/* Settings > User > Privacy */


/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Bain amach éachtaí i gcluichí clasaiceacha. Le haghaidh tuilleadh eolais, tabhair cuairt ar 'https://retroachievements.org'."
   )

/* Settings > User > Accounts > RetroAchievements */


/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Íoslódálacha"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Eolaire Tosaigh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Seinmliostaí"
   )

#ifdef HAVE_MIST
/* Settings > Steam */



#endif

/* Music */

/* Music > Quick Menu */


/* Netplay */


/* Netplay > Host */


/* Import Content */


/* Import Content > Scan File */


/* Import Content > Manual Scan */


/* Explore tab */

/* Playlist > Playlist Item */

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
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Éachtaí"
   )

/* Quick Menu > Options */


/* Quick Menu > Options > Manage Core Options */


/* - Legacy (unused) */

/* Quick Menu > Controls */


/* Quick Menu > Controls > Manage Remap Files */


/* Quick Menu > Controls > Manage Remap Files > Load Remap File */


/* Quick Menu > Cheats */


/* Quick Menu > Cheats > Start or Continue Cheat Search */


/* Quick Menu > Cheats > Load Cheat File (Replace) */


/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Cur síos"
   )

/* Quick Menu > Disc Control */


/* Quick Menu > Shaders */


/* Quick Menu > Shaders > Save */




/* Quick Menu > Shaders > Remove */


/* Quick Menu > Shaders > Shader Parameters */


/* Quick Menu > Overrides */


/* Quick Menu > Achievements */


/* Quick Menu > Information */


/* Miscellaneous UI Items */


/* Settings Options */


/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Cóimheas Gnéithe"
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Scála Slánuimhir"
   )

/* XMB: Settings > User Interface > Appearance */


/* XMB: Settings Options */


/* Ozone: Settings > User Interface > Appearance */




/* MaterialUI: Settings > User Interface > Appearance */


/* MaterialUI: Settings Options */


/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Eolas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Socruithe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Luchtaigh Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Ainm"
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
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Croí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Eolas"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Forleagan ar an Scáileán"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Stair"
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

/* Unused (Needs Confirmation) */

MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Íoslódálacha"
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
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Éadromaigh an Méarchlár"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Roghchlár a Athrú"
   )

/* Discord Status */


/* Notifications */



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

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
   "메인 메뉴"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "즐겨찾기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "히스토리"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "이미지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "음악"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "비디오"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "넷플레이"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "탐색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "단독 실행 코어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "콘텐츠 가져오기"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "빠른 메뉴"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "관련된 모든 게임 설정에 빠르게 접근합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "코어 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "사용할 코어를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "libretro 코어 파일을 탐색합니다. 탐색기가 시작되는 위치는 코어 디렉토리 경로를 따릅니다. 만약 비어있다면 루트에서 시작됩니다.\n코어 디렉토리가 폴더로 설정되면 메뉴상에서 최상위 폴더로 사용됩니다. 코어 디렉토리가 전체 경로라면 파일이 있는 폴더에서 시작됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "콘텐츠 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "실행할 콘텐츠를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "콘텐츠를 탐색합니다. 콘텐츠를 불러오려면 사용할 '코어'와 콘텐츠 파일이 필요합니다.\n메뉴에서 콘텐츠 탐색을 시작할 위치를 지정하려면 '파일 탐색기 디렉토리'를 설정하십시오. 설정하지 않으면 루트에서 시작됩니다.\n탐색기는 '코어 불러오기'에서 설정한 코어에 맞춰 확장자를 필터해주고 콘텐츠를 불러올때 해당 코어를 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "디스크 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "물리 미디어 디스크를 불러옵니다. 먼저 디스크를 사용할 코어를 불러와야(코어 불러오기) 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "디스크 덤프"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "물리 미디어 디스크를 내부 저장장치에 덤프합니다. 이미지 파일로 저장되게 됩니다."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "디스크 꺼내기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "물리 CD/DVD 드라이브에서 디스크를 꺼냅니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "실행목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "검색된 콘텐츠가 이곳에 표시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "콘텐츠 가져오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "콘텐츠를 검색하여 실행목록을 만들거나 업데이트 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "데스크탑 메뉴 보이기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "데스크탑 메뉴를 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "키오스크 모드를 비활성화합니다. (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "모든 구성 관련 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "온라인 업데이터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "RetroArch용 애드온, 구성요소 및 콘텐츠를 다운로드합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "넷플레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "넷플레이 세션을 개설하거나 참가합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "프로그램을 구성합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "정보"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "코어, 네트워크 및 시스템 정보를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "구성 파일"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "구성 파일을 관리하고 생성합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "도움말"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "프로그램 작동 방법을 설명합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "RetroArch 다시 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "프로그램을 재시작합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "RetroArch 종료"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "프로그램을 종료합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "RetroArch를 종료합니다. 프로그램을 강제 종료(SIGKILL 등)할 경우 RetroArch의 구성 등이 저장되지 않고 종료됩니다. 유닉스 계열 운영체제의 경우, SIGINT/SIGTERM을 사용하면 정상 종료가 가능합니다."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "코어 다운로드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "온라인 업데이터를 통해 코어를 설치합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "코어 설치 또는 복원"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "다운로드 폴더에서 코어를 설치 또는 복원합니다."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "비디오 프로세서 실행"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "원격 RetroPad 실행"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "시작 디렉토리"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "다운로드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "압축파일 탐색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "압축파일 불러오기"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "즐겨찾기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "즐겨찾기로 등록한 콘텐츠가 이곳에 표시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "음악"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "이전에 재생되었던 노래가 이곳에 표시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "이미지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "이전에 열람했던 사진이 이곳에 표시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "비디오"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "이전에 재생되었던 영상들이 이곳에 표시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "탐색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "분류된 검색 인터페이스를 통해 데이터베이스와 일치하는 모든 콘텐츠를 탐색합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "단독 실행 코어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "콘텐츠 없이 단독 실행될 수 있는 코어가 이곳에 표시됩니다."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "코어 다운로더"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "설치된 코어 모두 업데이트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "설치된 코어를 모두 최신 버전으로 업데이트합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "코어를 Play 스토어 버전으로 교체"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "가능한 코어에 한해, 이전에 설치했거나 수동으로 설치된 코어들을 Play 스토어에서 제공하는 최신 버전으로 교체합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "미리보기 업데이터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "선택된 시스템의 미리보기 전체를 다운로드 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "실행목록 미리보기 업데이터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "선택된 실행목록 내 모든 항목의 미리보기를 다운로드합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "콘텐츠 다운로더"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "지정된 코어에서 사용 가능한 무료 콘텐츠를 다운로드합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "코어 시스템 파일 다운로더"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "코어의 올바른/이상적인 작동에 필요한 추가 시스템 파일을 다운로드합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "코어 정보 파일 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "애셋 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "컨트롤러 프로필 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "치트 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "데이터베이스 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "오버레이 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "GLSL 셰이더 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Cg 셰이더 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Slang 셰이더 업데이트"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "코어 정보"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "앱/코어에 관련된 정보를 확인합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "디스크 정보"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "삽입된 미디어 디스크의 정보를 봅니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "네트워크 정보"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "모든 네트워크 인터페이스와 각각의 IP 주소를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "시스템 정보"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "시스템 관련 정보를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "데이터베이스 매니저"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "데이터베이스를 봅니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "커서 매니저"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "이전의 검색 내용을 봅니다."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "코어 이름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "코어 라벨"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "코어 버전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "시스템 이름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "시스템 제조사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "카테고리"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "저자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "권한"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "라이센스"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "지원 확장자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "그래픽 API 필요"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "상태저장 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "기본 (저장/불러오기)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "직렬 (저장/불러오기, 뒤로 감기)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "결정식 (저장/불러오기, 뒤로 감기, 미리 실행, 넷플레이)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "펌웨어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "누락됨, 필수:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "누락됨, 선택:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "존재함, 필수:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "존재함, 선택:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "설치된 코어 잠금"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "현재 설치된 코어의 수정을 방지합니다. 콘텐츠에 특정 버전의 코어가 필요한 경우 원치 않는 업데이트를 방지하는 데 사용할 수 있습니다(예: 아케이드 롬셋)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "'단독 실행 코어' 메뉴에서 제외"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "이 코어가 '단독 실행 코어' 메뉴에 표시되지 않게 합니다. 표시 모드가 '사용자 정의'일 경우에만 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "코어 삭제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "이 코어를 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "코어 백업"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "현재 설치된 코어의 백업을 생성합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "백업 복원"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "이전에 백업해둔 코어를 다시 설치합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "백업 삭제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "이전에 백업해둔 코어를 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[자동]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "빌드 일시"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "RetroArch 버전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Git 버전"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "컴파일러"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "CPU 모델"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "CPU 기능"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "CPU 아키텍처"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "CPU 코어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "프론트엔드 식별자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "프론트엔드 OS"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "RetroRating 수준"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "전원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "비디오 컨텍스트 드라이버"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "화면 너비(mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "화면 높이(mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "화면 DPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "LibretroDB 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "오버레이 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "명령줄 인터페이스 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "네트워크 명령줄 인터페이스 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "네트워크 컨트롤러 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Cocoa 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "PNG 지원 (RPNG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "JPEG 지원 (RJPEG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "BMP 지원 (RBMP)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "TGA 지원 (RTGA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "SDL1.2 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "SDL2 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Vulkan 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Metal 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "OpenGL 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "OpenGL ES 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "스레딩 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "KMS/EGL 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Udev 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "OpenVG 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "EGL 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "X11 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Wayland 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "XVideo 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "ALSA 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "OSS 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "OpenAL 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "OpenSL 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "RSound 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "RoarAudio 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "JACK 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "PulseAudio 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "CoreAudio 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "CoreAudio V3 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "DirectSound 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "WASAPI 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "XAudio2 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "Zlib 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "7zip 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "다이나믹 라이브러리 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "libretro 라이브러리 다이나믹 런타임 로딩"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Cg 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "GLSL 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "HLSL 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "SDL 이미지 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "FFmpeg 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "mpv 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "CoreText 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "FreeType 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "STB TrueType 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "넷플레이 (P2P) 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Video4Linux2 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "Libusb 지원"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "데이터베이스 선택"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "이름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "설명"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "장르"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "도전과제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "카테고리"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "언어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "지역"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "콘솔 독점"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "플랫폼 독점"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "점수"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "미디어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "컨트롤"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "아트 스타일"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "게임플레이"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "내러티브"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "진행 속도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "시점"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "비주얼"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "탑승물"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "배급사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "개발사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "원본"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "프랜차이즈"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "TGDB 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "패미통 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "에지 매거진 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "에지 매거진 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "에지 매거진 기사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "출시 월"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "출시 연도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "BBFC 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "ESRB 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "ELSPA 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "PEGI 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "개선 하드웨어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "CERO 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "시리얼"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "아날로그 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "진동 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "협동모드 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
   "SHA1:"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "구성 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "현재 사용되는 구성을 불러오고 현재 값을 바꿉니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "현재 구성 저장하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "현재 구성 파일을 덮어씁니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "새 구성 저장하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "현재 구성을 별도의 파일에 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "기본값으로 재설정하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "현재 구성을 기본값으로 재설정합니다."
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "기본 메뉴 컨트롤"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "위로 스크롤"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "아래로 스크롤"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "확인"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "정보"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "시작"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "메뉴 전환"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "나가기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "키보드 전환"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "시스템에서 사용되는 드라이버를 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "비디오"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "비디오 출력 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "오디오"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "오디오 입/출력 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "입력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "컨트롤러, 키보드 및 마우스를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "지연시간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "비디오, 오디오 및 입력 지연에 관련된 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "코어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "코어 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "구성"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "구성 파일의 기본 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "저장 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "클라우드 동기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "클라우드 동기화 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "클라우드 동기화 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "설정, SRAM, 상태저장을 클라우드 저장소 제공자에 동기화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "파괴적 클라우드 동기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "비활성화할 경우, 동기화 대상 파일들을 덮어씌우거나 삭제하지 않고 백업 폴더에 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "클라우드 동기화 백엔드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "사용할 클라우드 저장소 네트워크 프로토콜을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "클라우드 저장소 URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "클라우드 저장소 서비스의 API 진입점 URL입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "사용자 이름"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "클라우드 저장소 계정의 사용자명입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "비밀번호"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "클라우드 저장소 계정의 비밀번호입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "로그"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "로그 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "파일 탐색기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "파일 탐색기 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "구성 파일입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "압축 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "녹화 구성 파일입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "데이터베이스 커서 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "구성 파일입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "셰이더 프리셋 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "셰이더 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "컨트롤 파일을 다시 매핑합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "치트 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "오버레이 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "데이터베이스 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "트루타입 폰트 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "일반 파일입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "비디오입니다. 비디오 플레이어로 이 파일을 열려면 이 파일을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "음악입니다. 음악 플레이어에서 이 파일을 열려면 이 파일을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "이미지 파일."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "이미지입니다. 이미지 뷰어에서 이 파일을 열려면 이 파일을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Libretro 코어. 설정하면 현재 코어가 게임에 연결되게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Libretro 코어. RetroArch가 현재 코어를 불러오도록 파일을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "디렉토리. 현재 디렉토리를 열려면 선택하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "프레임 조절"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "뒤로 감기, 빨리 감기, 슬로모션 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "녹화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "녹화 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "온스크린 디스플레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "디스플레이 오버레이 및 키보드 오버레이, 온스크린 알림 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "사용자 인터페이스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "사용자 인터페이스 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "AI 서비스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "AI 서비스(번역/TTS/기타 등등) 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "접근성"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "접근성 나레이터 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "전력 관리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "전력 관리 설정들을 수정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "도전과제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "도전과제 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "네트워크"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "서버 및 네트워크를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "실행목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "실행목록 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "사용자"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "계정, 사용자 이름, 언어 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "디렉토리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "파일이 위치할 기본 디렉토리를 변경합니다."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS,
   "핵"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "매핑"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "미디어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "성능"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "소리"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "스펙"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "저장공간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "시스템"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "타이밍"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Steam 관련 설정을 변경합니다."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "입력 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "사용할 입력 드라이버입니다. 비디오 드라이버에 따라 다른 입력 드라이버를 사용해야 할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "udev 입력 드라이버는 조이스틱 지원에 최신 evdev 조이패드 API를 활용합니다. 핫플러깅과 포스 피드백을 지원합니다.\n키보드 지원에는 evdev 이벤트가 사용됩니다. 키보드 콜백, 마우스, 터치패드를 지원합니다.\n대부분의 디스트로에서 기본적으로, /dev/input 노드는 루트 전용(모드 600)으로 구성되어 있습니다. udev 규칙을 설정하면 이 값들에 비 root 앱들이 접근할 수 있게 할 수 있[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "linuxraw 입력 드라이버는 활성 TTY를 필요로 합니다. 키보드 이벤트를 TTY에서 직접 읽어들이므로 과정이 단순해지지만, udev만큼 유연하지 않습니다. 마우스 등은 지원되지 않습니다. 이 드라이버는 오래 된 조이스틱 API(/dev/input/js*)를 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "입력 드라이버입니다. 비디오 드라이버에 따라 다른 입력 드라이버를 사용해야 할 수도 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "컨트롤러 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "사용할 컨트롤러 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "비디오 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "사용할 비디오 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "OpenGL 1.x 드라이버입니다. 최소 OpenGL 1.1 지원이 필요합니다. 셰이더를 지원하지 않습니다. 가능한 경우 더 최신 버전의 OpenGL 드라이버를 사용하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "OpenGL 2.x 드라이버입니다. 이 드라이버는 소프트웨어 렌더링 코어 뿐만 아니라 libretro GL 코어도 실행할 수 있습니다. 최소 OpenGL 2.0 또는 OpenGLES 2.0 지원이 필요합니다. GLSL 셰이더 포맷을 지원합니다. 가능할 경우 glcore 드라이버를 대신 사용하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "OpenGL 3.x 드라이버입니다. 이 드라이버는 소프트웨어 렌더링 코어 뿐만 아니라 libretro GL 코어도 실행할 수 있습니다. 최소 OpenGL 3.2 또는 OpenGLES 3.0+ 지원이 필요합니다. Slang 셰이더 포맷을 지원합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Vulkan 드라이버입니다. 이 드라이버는 소프트웨어 렌더링 코어 뿐만 아니라 libretro Vulkan 코어도 실행할 수 있습니다. 최소 Vulkan 1.0 지원이 필요합니다. HDR과 Slang 셰이더를 지원합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "SDL 1.2 소프트웨어 렌더링 드라이버입니다. 성능이 좋지 않습니다. 여지가 없을 경우 최후의 수단으로만 사용하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "SDL 2 소프트웨어 렌더링 드라이버입니다. 소프트웨어 렌더링을 사용하는 libretro 코어의 성능은 현재 플랫폼의 SDL 구현 방식에 따라 결정됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Apple 플랫폼 전용 Metal 드라이버입니다. Slang 셰이더 포맷을 지원합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "셰이더 지원이 없는 Direct3D 8 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "오래 된 Cg 셰이더 포맷을 지원하는 Direct3D 9 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "HLSL 셰이더 포맷을 지원하는 Direct3D 9 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Slang 셰이더 포맷을 지원하는 Direct3D 10 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "HDR과 Slang 셰이더 포맷을 지원하는 Direct3D 11 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "HDR과 Slang 셰이더 포맷을 지원하는 Direct3D 12 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "DispmanX 드라이버입니다. 라즈베이 파이 0~3에서 사용되는 Videocore IV GPU의 DispmanX API를 사용합니다. 오버레이와 셰이더를 지원하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "LibCACA 드라이버입니다. 그래픽 대신 문자열을 출력합니다. 일반적인 용도로는 추천하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Samsung Exynos SoC의 G2D 블럭으로 블리팅을 수행하는 저수준 Exynos 비디오 드라이버입니다. 소프트웨어 렌더링을 사용하는 코어에서 최적의 성능을 보여줍니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "DRM 비디오 드라이버입니다. libdrm의 GPU 오버레이로 하드웨어 스케일링을 구현하는 저수준 비디오 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Allwinner SoC의 G2D 블럭을 사용하는 저수준 Sunxi 비디오 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Wii U 드라이버입니다. Slang 셰이더를 지원합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Switch 드라이버입니다. GLSL 셰이더 포맷을 지원합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "OpenVG 드라이버입니다. OpenVG의 하드웨어 가속 2D 벡터 그래픽스 API를 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "GDI 드라이버입니다. Windows의 레거시 인터페이스를 사용합니다. 권장되지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "현재 비디오 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "오디오 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "사용할 오디오 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "네트워크 오디오 시스템을 위한 RSound 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "레거시 Open Sound System 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "기본 ALSA 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "스레딩을 지원하는 ALSA 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "의존성 없이 구현된 ALSA 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "RoarAudio 사운드 시스템 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "OpenAL 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "OpenSL 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "DIrectSound 드라이버입니다. DirectSound는 Windows 95부터 Windows XP까지 주로 사용되었습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Windows Audio Session API 드라이버입니다. WASAPI는 Windows 7 이상에서 주로 사용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "PulseAudio 드라이버입니다. 시스템이 PulseAudio를 사용할 경우, ALSA 등의 드라이버 대신 반드시 이 드라이버를 사용해야 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Jack Audio Connection Kit 드라이버입니다."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "마이크"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "사용할 마이크 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "마이크 리샘플러"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "사용할 마이크 리샘플러 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "마이크 블록 프레임"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "오디오 리샘플러 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "사용할 오디오 리샘플러 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "창문형 Sinc 구현입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "합성형 코사인 구현입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "최근접 리샘플링 구현입니다. 이 리샘플러는 품질 설정을 무시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "카메라 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "사용할 카메라 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_DRIVER,
   "블루투스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "사용할 블루투스 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
   "Wi-Fi 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "사용할 Wi-Fi 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "위치 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "사용할 위치 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "메뉴 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "사용할 메뉴 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB는 어느 7세대 콘솔의 메뉴와 유사한 모습의 RetroArch GUI입니다. Ozone과 같은 기능을 제공합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone은 대부분의 플랫폼에서 RetroArch의 기본 GUI입니다. 게임 컨트롤러를 사용한 내비게이션에 최적화되어 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI는 단순한 빌트인 RetroArch GUI입니다. 성능 요구사항이 가장 낮으며, 저해상도 화면에서도 사용될 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "모바일 장치에서, RetroArch는 모바일 UI인 MaterialUI를 기본 GUI로 사용합니다. 이 인터페이스는 터치스크린 및 마우스/트랙볼 등의 포인팅 디바이스를 염두에 두고 설계되었습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "녹화 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "사용할 녹화 드라이버입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
   "MIDI 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "사용할 MIDI 드라이버입니다."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "CRT 디스플레이에서 사용하는 네이티브 저해상도 신호를 출력합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "출력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "비디오 출력 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "전체 화면 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "전체화면 모드 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "창 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "창 모드 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "비디오 크기 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "비디오 HDR 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "동기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "비디오 동기화 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "화면보호기 동작 차단"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "시스템 화면보호기의 동작을 방지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "화면 보호기를 일시 중단합니다. 비디오 드라이버에서 반드시 따를 필요없는 힌트입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "스레드된 비디오"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "지연시간과 끊김을 감수하면서 성능을 향상시킵니다. 어떻게 해서도 풀 스피드를 얻지 못하는 경우에만 사용하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "스레드된 비디오 드라이버를 사용합니다. 사용시 지연과 더 많은 비디오 끊김 현상이 발생할 수 있지만 성능이 향상될 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "검은 프레임 삽입"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "프레임 사이 검은 프레임을 삽입합니다. 120Hz 디스플레이에서 60Hz 콘텐츠 실행 시 고스트 현상을 줄이는데 유용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "프레임 사이에 검은색 프레임을 삽입합니다. 120Hz 모니터에서 고스팅이 제거된 60Hz 자료를 실행하는 데 유용합니다. 비디오 새로 고침 빈도는 60Hz 모니터상태 그대로 구성해야 합니다(새로 고침 빈도를 2로 나눔)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "GPU 스크린샷 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "가능한 경우 스크린샷을 GPU에서 셰이더 처리된 화면으로 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "이중선형 필터링"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "픽셀 가장자리의 날카로운 부분을 줄이기 위해 이미지에 약간의 흐림 효과를 추가합니다. 이 옵션은 성능에 거의 영향을 주지 않습니다. 셰이더를 사용할 경우 비활성화되어야 합니다."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "이미지 보간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "내부 IPU를 통해 크기조절 시 사용할 이미지 보간 방법을 지정합니다. CPU로 동작하는 비디오 필터 사용시 '쌍입방' 또는 '이중선형'을 추천합니다. 해당 옵션은 퍼포먼스에 영향을 주지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "쌍입방보간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "이중선형보간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "최근접 이웃"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "이미지 보간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "'정수 단위 화면 크기'를 사용하지 않을 때 이미지를 보간할 방식을 지정합니다. '최근접 이웃'이 성능에 가장 영향이 적습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "최근접 이웃"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "반선형"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "자동 셰이더 지연"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "자동 셰이더 로드를 지연(밀리초) 합니다. 화면 캡쳐 소프트웨어의 이상 작동을 막을 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "비디오 필터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "CPU가 처리하는 비디오 필터를 적용합니다. 성능에 큰 희생이 필요할 수 있습니다. 일부 필터는 32비트 또는 16비트 색상을 사용하는 코어에만 적용될 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "CPU로 작동하는 비디오 필터를 적용합니다. 높은 성능 비용이 들 수 있습니다. 일부 비디오 필터는 32비트 또는 16비트 색상을 사용하는 코어에서만 작동할 수 있습니다. 동적으로 연결된 비디오 필터 라이브러리를 선택할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "CPU로 작동하는 비디오 필터를 적용합니다. 높은 성능 비용이 들 수 있습니다. 일부 비디오 필터는 32비트 또는 16비트 색상을 사용하는 코어에서만 작동할 수 있습니다. 내장 비디오 필터 라이브러리를 선택할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "비디오 필터 제거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "활성중인 CPU로 동작하는 비디오 필터를 해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Android 기기에서 전체화면을 노치까지 확장하기"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "CRT 리소스 전환"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "각 코어/게임에 적합한 해상도와 주사율을 사용합니다. CRT 디스플레이 전용 설정입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "CRT 슈퍼 해상도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "기본/고화질 해상도 중 하나를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "X축 센터링"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "이미지가 디스플레이 중앙에 표시되지 않는다면 이 옵션들을 변경해가며 적용해보세요."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "포치 조절"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "이미지 크기를 변경하려면 옵션을 통해 포치 설정 값을 조정하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "고해상도 메뉴 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "불러온 콘텐츠가 없을 때 고해상도 메뉴를 사용할 수 있도록 고해상도 모드라인으로 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "사용자 정의 주사율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "필요한 경우 구성 파일에 지정된 사용자 지정 주사율을 사용합니다."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "모니터 인덱스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "사용할 디스플레이를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "선호하는 모니터입니다. 0(기본값)으로 설정하는 것은 특별히 선호하는 모니터가 없다는 의미이고, 1 이상(1이 첫 번째 모니터)으로 설정하는 것은 RetroArch에게 해당 모니터를 사용할 것을 제안하는 것입니다."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Wii U GamePad에 알맞게 표시 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "GamePad의 정확히 2배 크기를 뷰포트로 사용합니다. 비활성화할 경우 TV의 기본 해상도로 출력됩니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "화면 회전"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "강제로 화면을 회전시킵니다. 코어 기본 회전 설정에 추가로 회전됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "화면 방향"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "OS에서 강제로 화면을 회전하게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "GPU 인덱스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "사용할 그래픽 카드를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "화면 가로 오프셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "화면에 강제로 일정량의 가로 방향 오프셋을 적용합니다. 이 설정은 전역적으로 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "화면 세로 오프셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "화면에 강제로 일정량의 세로 방향 오프셋을 적용합니다. 이 설정은 전역적으로 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "주사율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "화면의 주사율입니다. 적절한 오디오 입력 비율을 계산하는데 사용됩니다.\n '스레드된 비디오' 설정 사용시 무시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "예상되는 화면 주사율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "화면의 정확한 주사율(Hz) 측정치입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "모니터의 정확한 재생 빈도(Hz). 이 값은 다음 공식을 사용하여 오디오 입력 속도를 계산하는 데 사용됩니다.\n오디오 입력 비율 = 게임 입력 속도 * 디스플레이 새로 고침 빈도 / 게임 새로 고침 빈도\n코어가 값을 보고하지 않으면 호환성을 위해 NTSC 기본값이 사용됩니다.\n이 값은 큰 피치 변경을 방지하기 위해 60Hz에 가깝게 유지되어야 합니다. 모니터가 60Hz 또는 60Hz에 가깝게 [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "보고된 화면 주사율 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "디스플레이 드라이버가 보고한 화면 주사율입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "자동 새로고침 빈도 전환"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "현재 콘텐츠에 따라 화면 새로고침 빈도를 자동으로 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "독점 전체화면 모드에서만"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "전체 창화면 모드에서만"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "모든 전체화면 모드에서"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "자동 새로고침 빈도 PAL 임계값"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "PAL로 간주되는 최대 새로고침 빈도입니다."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "주사율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "디스플레이의 주사율을 설정합니다. '50 Hz'로 설정하면 PAL 콘텐츠를 실행할 때 부드럽게 표시할 수 있습니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "sRGB FBO 강제 사용해제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "sRGB FBO 지원을 강제로 비활성화합니다. 일부 인텔 OpenGL 드라이버는 Windows에서 sRGB FBO 사용 시 문제를 일으킵니다. 활성화하여 해당 문제를 우회할 수 있습니다."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "전체 화면 모드로 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "전체화면으로 시작합니다. 실행 중 변경도 가능합니다. 명령줄 스위치로 무시할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "전체 창화면 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "전체화면을 사용할 경우, 화면 모드 변경을 막기 위해 전체 창모드를 선호하도록 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "전체화면 폭"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "전체화면 모드의 사용자 가로 폭을 설정합니다. 설정하지 않으면 바탕화면 해상도를 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "전체화면 높이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "전체화면 모드의 사용자 세로 높이를 설정합니다. 설정하지 않으면 바탕화면 해상도를 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "해상도 강제 (UWP)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "해상도를 전체 화면 크기로 강제합니다. 0으로 설정할 경우 3840 x 2160의 고정된 값이 사용됩니다."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "창 모드 크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "창 크기를 코어의 뷰포트 크기의 배수로 지정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "창 불투명도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "창의 투명도를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "창 꾸밈 보이기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "창에 제목 표시줄과 테두리를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "메뉴 바 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "창에 메뉴 바를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "창 위치와 크기 기억"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "모든 콘텐츠를 '창 너비'와 '창 높이'에서 지정한 크기의 창 안에 표시하고, RetroArch 종료 시 현재 창 크기와 위치를 기억해둡니다. 비활성화할 경우, 창 크기는 '창 모드 크기'에서 지정한 배율에 맞춰 동적으로 설정됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "사용자 정의 창 크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "모든 콘텐츠를 '창 너비'와 '창 높이'에서 지정한 크기의 창 안에 표시합니다. 비활성화할 경우, 창 크기는 '창 모드 크기'에서 지정한 배율에 맞춰 동적으로 설정됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "윈도우 폭"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "표시 창의 사용자 너비 크기를 설정합니다. 값을 0으로 두면 최대 크기로 창을 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "윈도우 높이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "표시 창의 사용자 높이 크기를 설정합니다. 값을 0으로 두면 최대 크기로 창을 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "최대 창 너비"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "창 크기를 '창 모드 크기' 설정에 맞춰 변경할 때 사용할 창의 최대 너비를 지정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "최대 창 높이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "창 크기를 '창 모드 크기' 설정에 맞춰 변경할 때 사용할 창의 최대 높이를 지정합니다."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "정수 단위 화면 크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "비디오의 크기를 정수 단위로만 조정합니다. 기본 크기는 각 시스템이 보고한 크기와 종횡비에 따라 결정되며 '강제 화면비'를 설정하지 않았을 경우 X/Y 크기를 각각 정수 단위로 조절합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_OVERSCALE,
   "정수 단위 오버스케일링"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_OVERSCALE,
   "화면 크기를 조정할 정수 단위 배율을 버림 대신 올림으로 계산합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "화면비"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "디스플레이의 화면비를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "가로 세로 비율 구성하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "화면비를 나타내는 실수 값(너비/높이)입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "구성하기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_SQUARE_PIXEL,
   "1:1 픽셀비 (%u:%u 화면비)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "코어에 따름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "사용자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "모두"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "화변 비율 유지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "내부 IPU를 통한 크지조절 시 1:1 픽셀 비율을 유지합니다. 사용 해제시 이미지가 확대되어 전체 화면을 채우게됩니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "사용자 화면비 X 위치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "뷰포트의 X축 위치를 정의하는 데 사용되는 사용자 뷰포트 오프셋입니다.\n'정수 단위 화면 크기'가 활성화된 경우 무시되고 자동으로 중앙이 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "사용자 화면비 Y 위치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "뷰포트의 Y축 위치를 정의하는 데 사용되는 사용자 뷰포트 오프셋입니다.\n'정수 단위 화면 크기'가 활성화된 경우 무시되고 자동으로 중앙이 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "사용자 화면비 폭"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "화면비가 '사용자 설정'으로 되어있을 시 사용할 사용자 뷰포트 너비입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "사용자 화면비 높이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "화면비가 '사용자 설정'으로 되어있을 시 사용할 사용자 뷰포트 높이입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "오버스캔 잘라내기 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "개발자가 의도적으로 비워두는 화면 가장자리 픽셀을 잘라냅니다."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "HDR 활성화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "지원되는 디스플레이에서 HDR을 활성화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "최대 휘도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "디스플레이가 표시할 수 있는 최대 휘도(cd/m2)를 설정합니다. RTINGS 등의 사이트에서 관련 수치를 확인할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "페이퍼 화이트 휘도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "흰 종이의 휘도(읽을 수 있는 글자 또는 SDR 상태에서의 최대 휘도)를 설정합니다. 주변 환경의 밝기에 맞춰 화면 밝기를 조정하는 데 사용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "명암"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "HDR의 감마/명암을 조절합니다. 어두운 색과 밝은 색 사이의 격차를 증가시킵니다. HDR 명암이 높을수록 해당 격차가 커지며, 낮을수록 화면이 바래집니다. 화면을 사용자가 보기 좋도록 원하는 대로 조정하는 데 도움이 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "색 영역 확장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "색 영역을 선형으로 변환한 후에 HDR10에 대응하는 확장 색 영역을 사용할지 설정합니다."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "수직 동기화 (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "그래픽 카드의 비디오 출력을 디스플레이의 화면 주사율과 동기화시킵니다. 사용이 권장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "수직 동기화 스왑 간격"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "수직 동기화에 사용자 정의 스왑 간격을 사용합니다. 모니터 주사율을 정한 값으로 나누는 것과 같습니다. '자동'은 값을 코어가 보고하는 프레임 레이트에 맞춰 설정하며, 30FPS 콘텐츠를 60FPS 화면에서 실행할 때 또는 60FPS 컨텐츠를 120FPS 화면에서 실행할 때 등의 상황에서 프레임 페이스를 개선합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "자동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "적응형 수직 동기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "설정된 주사율보다 낮은 성능이 감지될 경우 수직 동기화가 자동으로 비활성화됩니다. 퍼포먼스가 떨어질 때 버벅임을 방지할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "프레임 지연 (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "지연 시간을 줄이지만 비디오 버벅거림이 발생할 확률이 높아집니다. 비디오 출력 이후와 코어 프레임 이전 사이에 밀리초 단위의 지연을 추가합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "비디오 프레젠테이션 후 코어를 실행하기 전에 지연할 시간(밀리초) 을 설정합니다. 끊김 현상이 발생할 수 있는 대신 대기 시간을 줄일 수 있습니다. 최대값은 %d입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "자동 프레임 지연"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "필요한 경우 '프레임 지연'을 일시적으로 감소시키며 프레임 누락을 방지합니다. '프레임 지연'이 0일 경우 시작 값은 프레임 간격의 절반입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "목표 프레임 레이트에 안정적으로 도달할 때까지 임시로 '프레임 지연'을 감소시킵니다. 측정은 '프레임 지연'이 0일 때 프레임 시간의 절반에서부터 시작됩니다. (예: NTSC에서는 8, PAL에서는 10)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "자동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "강제 GPU 동기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "CPU와 GPU를 강제 동기화합니다. 성능을 희생해 지연시간을 줄입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "강제 GPU 동기화 프레임"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "'강제 GPU 동기화' 사용 시 GPU에 앞서 CPU가 먼저 계산할 프레임 수를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "'강제 GPU 동기화'를 사용할 때 CPU가 GPU보다 앞서 실행할 수 있는 프레임 수를 설정합니다. 최대값은 3입니다.\n 0: GPU에 바로 동기화.\n 1: 이전 프레임에 동기화.\n 2: 기타 ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "정확한 프레임 동기화 (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "코어에서 요구하는 타이밍에 편차가 발생하지 않게 합니다. 유동적인 화면 주사율을 가진 모니터(G-Sync, FreeSync, HDMI 2.1 VRR)에서 사용할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "정확한 콘텐츠 프레임 속도로 동기화합니다. 이 옵션은 빨리 감기를 허용하면서 x1 속도를 강제하는 것과 동일합니다. 코어에서 지정된 재생 빈도를 벗어난 요청, 사운드 동적 속도 제어등이 불가능해집니다."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "출력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "오디오 출력 설정을 변경합니다."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "마이크"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "오디오 입력 설정을 변경합니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "리샘플러"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "오디오 리샘플러 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "동기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "오디오 동기화 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS,
   "MIDI 드라이버"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "MIDI 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "믹서 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "오디오 설정을 확인하거나 편집합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "메뉴 사운드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "메뉴 소리 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "오디오 음소거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "소리를 끕니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "오디오 믹서 음소거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "믹서 오디오를 음소거합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "빨리 감기 중 음소거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "빨리 감기를 사용할 때 오디오를 음소거합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "빨리 감기 중 오디오도 빨리 감기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "빨리 감기 중 오디오의 속도를 올립니다. 소리 깨짐 효과를 방지하지만 음높이가 올라갑니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "오디오 볼륨 레벨 (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "오디오 볼륨(dB)입니다. 0 dB로 설정 시 증폭이 적용되지 않은 정상 볼륨 상태가 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "dB로 나타낸 오디오 볼륨입니다. 0 dB는 증폭이 적용되지 않은 일반 볼륨입니다. 증폭은 실행 중에 볼륨 높임/볼륨 낮춤 입력으로 조정될 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "오디오 믹서 볼륨 크기 (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "전역 오디오 믹서 볼륨(dB)입니다. 0 dB로 설정 시 증폭이 적용되지 않은 정상 볼륨 상태가 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "오디오 DSP 플러그인"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "드라이버로 보내기 전에 오디오를 처리하는 오디오 DSP 플러그인을 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "DSP 플러그인 제거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "사용중인 오디오 DSP 플러그인을 해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI 단독 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI 드라이버가 오디오 장치에 대한 독점 권한을 갖도록 허용합니다. 비활성 시 공유 모드를 사용하게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "WASAPI 부동소수점 형식"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "오디오 장치가 지원할 경우, WASAPI 드라이버가 부동소수점 형식을 사용하도록 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI 공유 버퍼 길이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI 드라이버를 공유 모드로 사용할 경우 사용할 중간 버퍼의 길이(프레임)입니다."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "오디오 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "오디오 출력을 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "오디오 장치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "오디오 드라이버가 사용할 기본 오디오 장치를 설정합니다. 이 옵션은 드라이버 종속적입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "오디오 드라이버가 사용할 기본 오디오 장치를 설정합니다. 이 옵션은 드라이버 종속적입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "ALSA 드라이버의 사용자 지정 PCM 장치 값입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "OSS 드라이버의 경로 값입니다. (예: /dev/dsp)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "JACK 드라이버의 포트명 값입니다. (예: system:playback1,system:playback_2)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "RSound 드라이버가 존재하는 RSound 서버의 IP 주소입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "오디오 지연 (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "오디오 지연(밀리초)을 설정합니다. 오디오 드라이버가 해당 지연시간을 지원하지 못할 경우 무시될 수 있습니다."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "마이크"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "지원되는 코어에서 오디오 입력을 활성화합니다. 코어가 마이크를 사용하지 않을 경우 이 옵션은 아무 영향도 끼치지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "오디오 장치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "마이크 드라이버가 사용할 기본 입력 장치를 설정합니다. 이 옵션은 드라이버 종속적입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "마이크 드라이버가 사용할 기본 입력 장치를 설정합니다. 이 옵션은 드라이버 종속적입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "오디오 리샘플러 품질"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "값을 낮춰서 오디오 품질 대신 성능 및 반응 속도에 비중을 두거나, 반대로 값을 높여서 성능이나 반응 속도 대신 더 나은 음질에 비중을 둘 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "기본 입력 레이트 (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "오디오 입력의 샘플 레이트입니다. 코어가 특정한 값을 요구하지 않을 경우 사용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "오디오 입력 지연 (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "오디오 입력 지연(밀리초)을 설정합니다. 마이크 드라이버가 해당 지연시간을 지원하지 못할 경우 무시될 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "WASAPI 단독 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "WASAPI 드라이버를 사용할 때 RetroArch가 마이크 장치에 대한 독점 권한을 갖도록 허용합니다. 비활성 시 공유 모드를 사용하게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "WASAPI 부동소수점 형식"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "오디오 장치가 지원할 경우, WASAPI 드라이버가 부동소수점 형식을 사용하도록 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI 공유 버퍼 길이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI 드라이버를 공유 모드로 사용할 경우 사용할 중간 버퍼의 길이(프레임)입니다."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "오디오 리샘플러 품질"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "값을 낮춰서 오디오 품질 대신 성능 및 반응 속도에 비중을 두거나, 반대로 값을 높여서 성능이나 반응 속도 대신 더 나은 음질에 비중을 둘 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "오디오 출력 레이트 (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "오디오 출력 샘플 레이트입니다."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "동기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "오디오를 동기화합니다. 사용이 권장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "오디오 최대 타이밍 왜곡"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "오디오 입력 비율의 최대 변화치입니다. 값을 높이면 오디오 피치를 뒤틀어서 타이밍을 크게 변형할 수 있습니다. (예: NTSC 디스플레이에서 PAL 코어 실행)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "최대 오디오 타이밍 왜곡.\n입력 속도의 최대 변화를 정의합니다. 부정확한 오디오 피치를 갖는 대신 값을 늘려 타이밍을 매우 크게 변경할 수 있습니다. (예: NTSC 디스플레이에서 PAL 코어 실행). \n입력 속도는 다음과 같이 정의됩니다.\n입력 속도 * (1.0 +/- (최대 타이밍 왜곡))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "동적 오디오 레이트 제어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "오디오와 비디오를 동기화 할 때 불완전한 타이밍을 완화시킵니다. 비활성화 시 정상적인 동기화 결과를 얻을 수 없게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "이 값을 0으로 설정하면 레이트 제어가 비활성화됩니다. 다른 모든 값은 오디오 레이트 컨트롤 델타를 제어합니다.\n동적으로 조정할 수 있는 입력 레이트의 양을 정의합니다. 입력 레이트 다음과 같이 정의됩니다. \n입력 레이트 * (1.0 +/- (레이트 제어 델타))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "입력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "입력 장치를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "입력 장치(드라이버 특정)를 설정합니다. '꺼짐'으로 설정하면 MIDI 입력이 비활성화됩니다. 장치 이름도 입력할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "출력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "출력 장치를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "출력 장치(드라이버 특정)를 설정합니다. '꺼짐'으로로 설정하면 MIDI 출력이 비활성화됩니다. 장치 이름도 입력할 수 있습니다.\nMIDI 출력이 활성화되고 코어 및 게임/앱이 MIDI 출력을 지원하는 경우 게임/앱에 따라 MIDI 장치에서 일부 또는 모든 사운드가 출력됩니다. 'null' MIDI 드라이버의 경우 소리가 들리지 않음을 의미합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "볼륨"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "출력 볼륨을 설정합니다(%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "믹서 스트림 #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "재생"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "오디오 스트림을 재생합니다. 재생 후에 오디오 스트림을 메모리에서 제거합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "재생 (반복)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "오디오 스트림을 재생합니다. 재생 후에 다시 반복합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "재생 (연속 재생)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "오디오 스트림을 재생합니다. 재생 후에 다음 오디오 스트림으로 넘어갑니다. 앨범 재생 모드에 유용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "중지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "재생중인 오디오 스트림을 멈춥니다. 오디오가 메모리에서 삭제되지는 않으므로 '재생'을 눌러 다시 재생할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "삭제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "재생되던 오디오 스트림을 멈추고 메모리에서 완전히 제거합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "볼륨"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "오디오 스트림의 볼륨을 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "상태 : 사용불가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "상태 : 정지됨"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "상태 : 재생 중"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "상태 : 재생 중 (반복)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "상태 : 재생 중 (순차)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "메뉴 효과음 활성화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "메뉴 효과음을 활성화/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "'확인' 사운드 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "'취소' 사운드 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "'알림' 사운드 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "배경음악 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "'스크롤' 사운드 활성화"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "최대 사용자"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "RetroArch에서 지원할 최대 사용자 수를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "폴링 방식 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "RetroArch에서 입력 폴링이 수행되는 방식을 설정합니다. 시스템 구성에 따라 '일찍' 또는 '늦게'로 설정 시 지연시간이 줄어들 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "RetroArch에서 입력 폴링이 수행되는 방식을 설정합니다.\n일찍 - 입력 폴링이 프레임 처리 이전에 수행됩니다.\n보통 - 입력 폴링이 폴링 요청이 발생할 때 수행됩니다.\n늦게 - 입력 폴링이 매 프레임에서 처음으로 입력 상태 요청이 발생할 때마다 수행됩니다.\n시스템 구성에 따라 '일찍' 또는 '늦게'로 설정 시 지연시간이 줄어들 수 있습니다. 이 설정은 넷플레이 중에는 무시됩니[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "이 코어에 입력설정 파일 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "재설정된 입력설정을 현재 코어에 설정된 입력보다 우선 적용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "자동구성"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "프로필, 플러그 앤 플레이 스타일을 가진 컨트롤러를 자동으로 구성합니다."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Windows 핫키 비활성화 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "윈도우 키와 함께 입력되는 핫키들을 비활성화합니다."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "물리적 키보드 선택"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "이 장치를 게임패드가 아닌 물리적 키보드로서 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "만일 RetroArch가 하드웨어 키보드를 게임패드로 인식할 경우, 이 설정을 활성화해 RetroArch가 잘못 인식된 해당 장치를 키보드인 것처럼 다루도록 강제할 수 있습니다.\n이 설정은 Android TV 장치에서 컴퓨터를 에뮬레이트하려 하는데 장치에 직접 연결 가능한 물리적 키보드가 존재할 경우 등의 상황에서 유용하게 사용될 수 있습니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "보조 센서 입력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "가능한 경우 가속도계, 자이로스코프, 조도 센서로부터의 입력을 사용합니다. 일부 플랫폼에서는 성능에 영향을 미치거나 전력 사용량이 증가할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "자동 마우스 잡기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "앱이 활성화될 때 자동으로 마우스 잡기를 활성화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "게임 포커스 모드 자동 활성화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "콘텐츠를 불러올 때 게임 포커스 모드를 항상 자동으로 활성화합니다. '자동 감지'로 설정할 경우, 선택된 코어가 키보드 입력을 요구할 경우에만 게임 포커스 모드가 활성화됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "끄기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "켜기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "자동 감지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "컨트롤러 연결 해제 시 콘텐츠 일시정지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "컨트롤러가 연결 해제될 경우 콘텐츠를 일시정지합니다. 시작 버튼을 누르면 일시정지를 해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "버튼 입력 축 임계값"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "'아날로그로 디지털 입력' 옵션 사용 시 지정한 축이 얼마나 기울어져야 버튼 입력으로 인식할 지 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "아날로그 데드존"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "설정한 데드존 값 미만의 아날로그 스틱 움직임은 무시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "아날로그 감도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "아날로그 스틱의 감도를 조정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "입력 만료 시간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "다음 입력 설정으로 넘어가기 전까지 대기하는 시간(초)입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "입력 설정 시간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "입력 설정을 위해 누르고 있어야 하는 시간(초)입니다."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "키보드, 마우스, 또는 컨트롤러를 누르세요"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "키 및 버튼에서 손을 떼세요!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "시간 제한"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "꾹 누르기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "터보 반복 간격"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "터보 설정된 버튼이 눌렸을 때 사용될 입력 간격(프레임)입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
   "터보 입력 시간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "터보 설정된 버튼이 눌렸을 때 사용될 누르고 있을 기간(프레임)입니다. 이 값이 터보 간격 값과 같거나 더 클 경우, 버튼은 눌린 상태로 유지될 것입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "터보 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "터보 모드의 작동 방식을 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "클래식"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "단일 버튼 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "단일 버튼 (홀드)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "클래식 모드, 2버튼 작동. 버튼을 누른 상태에서 터보 버튼을 클릭해 활성화합니다.\n터보 버튼은 설정/입력/포트 1 컨트롤에서 할당할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "토글 모드. 터보 버튼을 한 번 누르면 선택한 기본 버튼의 터보가 활성화되고 다시 한 번 누르면 꺼집니다.\n터보 버튼은 설정/입력/포트 1 컨트롤에서 할당할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "홀드 모드. 터보 버튼을 누르고 있는 동안 선택한 기본 버튼의 터보가 활성화됩니다.\n터보 버튼은 설정/입력/포트 1 컨트롤에서 할당할 수 있습니다.\n가정용 컴퓨터 시대의 자동 발사 기능을 모방하려면 터보 버튼과 기본 버튼을 조이스틱 발사 버튼과 동일하게 설정하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DEFAULT_BUTTON,
   "터보 기본 버튼"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DEFAULT_BUTTON,
   "'단일 버튼' 터보 모드의 기본 터보 활성화 버튼을 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "터보 입력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "터보 발사 설정 변경입니다.\n참고: 터보 기능을 사용하려면 해당 '포트 X 컨트롤' 메뉴에서 터보 버튼을 입력 장치에 매핑해야 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "햅틱 피드백/진동"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "햅틱 피드백 및 진동 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "메뉴 조작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "메뉴 조작 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "핫키 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "게임 플레이 중 메뉴 토글 등 단축키에 대한 설정 및 할당을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "레트로패드 바인딩"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "가상 레트로패드가 물리적 입력 장치에 매핑되는 방식을 변경합니다. 입력 장치가 올바르게 인식되고 자동 구성되면 사용자는 이 메뉴를 사용할 필요가 없습니다.\n참고: 코어별로 입력을 변경하려면 빠른 메뉴의 '컨트롤' 하위 메뉴를 대신 사용하세요."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro는 '레트로패드'로 알려진 가상 게임패드 가상화를 사용하여 프론트엔드(예: RetroArch)에서 코어로, 또는 그 반대로 통신합니다. 이 메뉴는 가상 레트로패드가 물리적 입력 장치에 매핑되는 방식과 이러한 장치가 차지하는 가상 입력 포트를 결정합니다.\n물리적 입력 장치가 올바르게 인식되고 자동 구성되면 사용자는 이 메뉴를 전혀 사용할 필요가 없으며, 코어별 입력 변[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "%u번 포트 입력 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "이 가상 포트의 가상 레트로패드가 물리적 입력 장치에 매핑되는 방식을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "코어별 입력 매핑을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Android 재연결 문제 우회 해결"
   )
MSG_HASH(
   MENU_ENUM_LABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "연결이 끊겼다가 돌아오는 컨트롤러를 위한 우회 방안입니다. 같은 종류의 컨트롤러를 2명의 플레이어로 인식하지 않게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "두 번 눌러 종료"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "종료 핫키를 두 번 눌러야 RetroArch가 종료되도록 합니다."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "키 누를 때 진동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "장치 진동 사용 (지원 코어 한정)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "진동 세기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "햅틱 피드백 효과의 세기를 설정합니다."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "통합 메뉴 컨트롤"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "메뉴와 게임 모두에 동일한 컨트롤을 사용합니다. 키보드에 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "정보 버튼 사용 안 함"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "정보 버튼 입력을 무시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "검색 버튼 사용 안 함"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "검색 버튼 입력을 무시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "확인/취소 버튼 반전"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "확인/취소 버튼을 바꿉니다. 비활성시 일본식 버튼 구성, 활성시 서양식 구성으로 설정됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "메뉴 스크롤 버튼 반전"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "스크롤 버튼 동작을 바꿉니다. 비활성 시 L/R은 10개씩, L2/R2는 알파벳 단위로 스크롤을 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "모든 사용자가 메뉴 조작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "모든 사용자가 메뉴를 제어할 수 있도록 허용합니다. 비활성 시 1번 사용자만 메뉴를 제어할 수 있습니다."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "핫키 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "할당된 경우, 다른 모든 핫키는 '핫키 사용' 키를 누른 채로 입력해야 합니다. 일반 입력에 영향을 끼치지 않으면서 컨트롤러 버튼을 핫키로 할당할 수 있게 합니다. '핫키 사용' 키를 컨트롤러에만 할당할 경우 키보드 핫키는 컨트롤러의 '핫키 사용' 키를 누르지 않아도 사용할 수 있으며, 컨트롤러와 키보드 모두 할당할 경우 두 장치의 '핫키 사용' 키는 두 장치 모두에서 사용[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "현재 핫키가 키보드나 조이스틱 버튼 또는 조이스틱에 할당된 경우 이 핫키와 동시에 누르지 않으면 다른 모든 핫키는 비활성 됩니다. \n이 기능은 키보드의 대부분의 영역을 차지하는 RETRO_KEYBOARD 구현시 유용하며, 핫키가 사용에 방해되는 경우에 유용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "핫키 지연 시간 (프레임)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "핫키를 눌렀을 때 일반 입력이 제한되기 전에 약간의 지연을 추가합니다. 핫키가 다른 동작(예: RetroPad '선택' 버튼)과 겹쳐서 매핑되어 있을 경우 해당 입력이 인식될 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "메뉴 열기/닫기 (컨트롤러 버튼 조합)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "메뉴를 호출하는 버튼 조합입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "메뉴 열기/닫기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "실행중인 콘텐츠와 메뉴 사이를 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "종료 (컨트롤러 버튼 조합)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "RetroArch를 종료하는 버튼 조합입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "나가기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "RetroArch를 닫으면, 모든 저장 데이터 및 구성 파일이 디스크로 저장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "콘텐츠 종료"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "현재 콘텐츠를 종료합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "콘텐츠 다시 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "콘텐츠를 처음부터 다시 시작합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "빨리 감기 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "빠른 속도와 일반 속도 사이를 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "빨리 감기 (홀드)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "누르고 있을 경우 빨리 감기를 사용합니다. 키를 떼면 다시 정상 속도로 실행됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "슬로모션 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "슬로모션과 일반 속도 사이를 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "슬로모션 (홀드)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "누르고 있을 경우 슬로모션을 사용합니다. 키를 떼면 다시 정상 속도로 실행됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "뒤로 감기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "키를 누르고 있는 동안 현재 콘텐츠를 뒤로 되돌립니다. '뒤로 감기 사용'이 반드시 활성화 되어야합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "일시정지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "콘텐츠의 일시정지를 설정/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "프레임 진행"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "일시정지 상태에서 콘텐츠를 한 프레임씩 진행합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "오디오 음소거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "음소거를 사용/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "볼륨 높임"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "오디오 출력의 볼륨 레벨을 증가시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "볼륨 낮춤"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "오디오 출력의 볼륨 레벨을 감소시킵니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "상태 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "현재 선택된 슬롯에서 상태저장 파일을 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "상태 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "현재 선택된 슬롯에 상태저장 파일을 기록합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "다음 상태저장 슬롯"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "선택된 상태저장 슬롯 인덱스를 증가시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "이전 상태저장 슬롯"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "선택된 상태저장 슬롯 인덱스를 감소시킵니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "디스크 꺼내기/삽입하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "가상 디스크 트레이가 닫혀있으면 트레이를 열고 디스크를 제거하고, 반대의 경우 현재 선택된 디스크를 넣고 트레이를 닫습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "다음 디스크"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "다음 디스크를 선택합니다. 가상 디스크 트레이가 열려 있어야 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "이전 디스크"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "이전 디스크를 선택합니다. 가상 디스크 트레이가 열려 있어야 합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "셰이더 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "현재 선택된 셰이더를 활성화/비활성화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "다음 셰이더"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "'비디오 셰이더' 디렉토리에서 다음 셰이더 프리셋을 불러와 적용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "이전 셰이더"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "'비디오 셰이더' 디렉토리에서 이전 셰이더 프리셋을 불러와 적용합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "치트 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "현재 선택된 치트를 활성화/비활성화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "다음 치트 인덱스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "선택된 치트 인덱스를 증가시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "이전 치트 인덱스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "선택된 치트 인덱스를 감소시킵니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "스크린샷 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "현재 콘텐츠의 화면 이미지를 캡쳐합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "녹화 (시작/중지)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "현재 세션을 동영상 파일로 녹화 시작/종료 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "스트리밍 (시작/중지)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "현재 세션 영상을 온라인상으로 스트리밍을 시작/종료 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "리플레이 재생"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "현재 선택된 슬롯에서 리플레이 파일을 재생합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "리플레이 녹화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "현재 선택된 슬롯에 리플레이 파일을 기록합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "녹화/재생 중지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "현재 리플레이의 녹화/재생을 중지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "다음 리플레이 슬롯"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "선택된 리플레이 슬롯의 인덱스를 증가시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "이전 리플레이 슬롯"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "선택된 리플레이 슬롯의 인덱스를 감소시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "마우스 잡기 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "마우스 잡기를 사용하거나 사용해제합니다. 사용 시 시스템 커서가 숨겨지고 마우스 움직임이 RetroArch 창 내로 제한되어 상대적 입력이 향상됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "게임 포커스 모드 (사용/해제)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "'게임 포커스' 모드를 켜거나 끕니다. 게임 포커스 모드가 켜지면 핫키를 사용할 수 없게 되고(모든 키보드 입력이 코어로 전달되고) 마우스가 움직이지 않게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "전체화면 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "전체화면 모드와 창 모드 사이를 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "데스크탑 메뉴 (열기/닫기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "윈도우 데스크탑 인터페이스 메뉴를 실행합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "정확한 프레임 동기화 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "정확한 프레임 동기화를 사용/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "미리 실행 (사용/해제)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "미리 실행 사용 유무를 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "선제 프레임 (사용/해제)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "선제 프레임 기능을 사용/사용해제 합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "FPS 표시 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "'초당 프레임 수' 표시기를 사용/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "기술적 통계 표시 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "온스크린 기술적 통계 표시를 켜거나 끕니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "키보드 오버레이 (토글)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "키보드 오버레이를 켜거나 끕니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "다음 오버레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "현재 활성된 온스크린 오버레이의 다음 사용 가능한 레이아웃으로 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "AI 서비스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "현재 콘텐츠의 이미지를 캡처하여 화면의 텍스트를 번역하거나 소리내어 읽습니다. 'AI 서비스'를 사용하도록 설정하고 구성해야 합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "넷플레이 핑 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "현재 넷플레이 방의 핑 카운터 표시 여부를 켜거나 끕니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "넷플레이 호스트 (켜기/끄기)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "넷플레이 호스트를 사용/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "넷플레이 플레이/관전 전환"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "현재 넷플레이 세션 모드를 '플레이' 또는 '관전'으로 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "넷플레이 플레이어 채팅"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "현재 넷플레이 세션에 대화 메시지를 보냅니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "넷플레이 채팅 페이드 켜기/끄기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "채팅 메시지를 서서히 사라지게 할 것인지 정적으로 표시할 것인지 선택합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "디버그 정보 보내기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SEND_DEBUG_INFO,
   "분석을 위해 장치 및 RetroArch 구성에 대한 진단 정보를 서버로 보냅니다."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "장치 종류"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "에뮬레이트할 컨트롤러의 종류를 지정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "아날로그로 디지털 입력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "지정한 아날로그 스틱을 D-패드 입력에 사용합니다. '강제' 옵션을 사용하면 코어에 보낼 아날로그 입력을 D-패드 입력으로 덮어씁니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "지정한 아날로그 스틱을 방향 패드 대신으로 사용합니다. \n코어가 아날로그 입력을 지원하는 경우, 방향 패드 매핑은 (강제) 옵션을 선택하지 않은 한 비활성화됩니다. \n방향 패드 매핑을 강제하면 코어는 지정한 스틱의 아날로그 입력을 받을 수 없게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "장치 인덱스"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "매핑할 포트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "프론트엔드의 %u번 포트 컨트롤러의 입력을 받을 코어의 포트를 지정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "전체 컨트롤 설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "기본 컨트롤 설정으로 초기화"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "컨트롤러 프로필 저장하기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "마우스 인덱스"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "B 버튼 (하)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Y 버튼 (좌)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "선택 버튼"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "시작 버튼"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "방향 패드 위"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "방향 패드 아래"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "방향 패드 왼쪽"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "방향 패드 오른쪽"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "A 버튼 (우)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "X 버튼 (상)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "L 버튼 (숄더)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "R 버튼 (숄더)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "L2 버튼 (트리거)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2 버튼 (트리거)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "L3 버튼 (엄지)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "R3 버튼 (엄지)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "좌 아날로그 X+ (우)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "좌 아날로그 X- (좌)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "좌 아날로그 Y+ (down)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "좌 아날로그 Y- (up)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "우 아날로그 X+ (우)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "우 아날로그 X- (좌)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "우 아날로그 Y+ (하)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "우 아날로그 Y- (상)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "무기 방아쇠"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "무기 재장전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "무기 보조(Aux) A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "무기 보조 B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "무기 보조 C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "무기 시작버튼"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "무기 선택버튼"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "무기 D-패드 위"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "무기 D-패드 아래"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "무기 D-패드 왼쪽"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "무기 D-패드 오른쪽"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "터보 사용"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[미리 실행 사용 불가]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "이 코어는 결정식 상태저장을 지원하지 않아 미리 실행과 호환되지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
   "미리 실행으로 지연 줄이기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
   "하나 혹은 그 이상의 프레임을 먼저 실행하고 이전 상태로 되돌리는 방식을 통해 입력지연을 줄여줍니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "미리 실행할 프레임 수"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "먼저 실행할 프레임 수를 설정합니다. 게임 자체의 지연 프레임을 넘어설 경우 화면이 떨리는 등 게임플레이에 문제가 생길 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
   "미리 실행에 두 번째 인스턴스 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
   "미리 실행에 두 번째 인스턴스를 사용합니다. 상태저장 불러오기 중에 발생하는 오디오 문제를 해결할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "미리 실행 경고 숨김"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "상태저장을 지원하지 않는 코어에서 미리 실행 사용 시 경고 메시지를 표시하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_UNSUPPORTED,
   "[선제 프레임 기능 사용불가]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_UNSUPPORTED,
   "이 코어는 결정식 상태저장을 지원하지 않아 선제 프레임 기능과 호환되지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_ENABLE,
   "선제 프레임 실행"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_ENABLE,
   "컨트롤러 상태가 변경되면 최근 입력으로 코어 로직을 다시 실행합니다. 미리실행보다 빠르지만 상태 불러오기에서 코어에 발생할 수 있는 오디오 문제를 막지는 못합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "선제 프레임 수"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "다시 실행할 프레임 수를 설정합니다. 게임 자체의 지연 프레임을 넘어설 경우 화면이 떨리는 등 게임플레이에 문제가 생길 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_HIDE_WARNINGS,
   "선제 프레임 경고 숨김"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_HIDE_WARNINGS,
   "선제 프레임 기능 사용시 호환되지 않는 코어에서 보이는 경고 메시지를 숨김니다."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "하드웨어 공유 컨텍스트 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "하드웨어 가속을 사용하는 코어에 독자 컨텍스트를 부여합니다. 프레임간 하드웨어 상태 변화를 추측하지 않아도 되게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "코어의 비디오 드라이버 교체 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "코어가 현재 로드된 비디오 드라이버가 아닌 다른 비디오 드라이버를 사용할 수 있도록 허용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "코어 종료 시 더미 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "종료 기능을 가진 코어가 종료될 때 더미 코어를 불러와 RetroArch까지 종료되는 것을 방지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "일부 코어는 종료 기능을 가지고 있을 수 있습니다. 이 옵션을 비활성화해둘 경우, 해당 종료 기능을 실행하면 RetroArch 전체가 종료됩니다.\n이 옵션을 활성화하면 종료 대신 더미 코어를 불러와 RetroArch가 종료되지 않고 메뉴를 표시할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "자동으로 코어 시작"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "불러오기 전 부족한 펌웨어 체크"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "콘텐츠를 불러오기 전에 필요한 펌웨어가 모두 있는지 확인합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE,
   "일부 코어는 펌웨어 또는 BIOS 파일을 필요로 합니다. 이 옵션을 활성화하면, RetroArch는 필요한 펌웨어 파일이 모두 갖춰지지 않은 코어의 실행을 차단합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "코어 옵션 카테고리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "코어가 옵션을 카테고리에 따른 하위 메뉴 형식으로 표시할 수 있게 합니다. 참고: 변경 사항을 적용하려면 코어를 다시 불러와야 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "코어 정보 파일 캐싱하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "설치된 코어 정보에 대한 로컬 캐시를 구축합니다. 디스크 성능이 좋지 않은 플랫폼에서 불러오는 시간을 크게 줄일 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "코어 정보 상태저장 기능 여부 무시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "코어 정보의 상태저장 기능 제공 여부를 무시할지 설정합니다. 관련 기능(미리 실행, 되감기 등)을 시험해볼 수 있게 됩니다."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "콘텐츠 실행 시 항상 코어를 다시 불러옴"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "필요한 코어를 이미 읽어들인 상태라도 콘텐츠 실행 시 RetroArch를 다시 시작합니다. 불러오는 시간이 증가하는 대신 시스템 안정성을 개선할 수 있습니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "회전 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "코어의 화면 회전 설정을 허용합니다. 비활성화하면 회전 요청을 무시하게 됩니다. 수동으로 화면 회전시 유용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "코어 관리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "설치된 코어의 오프라인 관리 작업(백업, 복원, 삭제 등) 을 수행하고 코어 정보를 확인합니다."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "코어 관리"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Steam을 통해 배포되는 코어를 설치 또는 삭제합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "코어 설치"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "코어 삭제"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "'코어 관리' 표시"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "메인 메뉴에 '코어 관리' 옵션을 표시합니다."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "코어 설치 중: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "RetroArch를 종료할 때 코어가 삭제됩니다."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "코어를 다운로드 중입니다"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "종료 시 구성 저장하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "종료 시 구성 파일의 변경 내용을 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "종료 시 구성 파일에 변경사항을 저장합니다. 메뉴에서 설정을 변경할 경우 유용합니다. 구성 파일을 덮어쓰며, #include와 주석은 보존되지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "종료 시 입력설정 파일 저장하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "콘텐츠를 중단하거나 RetroArch를 종료할 때 입력설정의 변경 사항을 파일에 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "콘텐츠 특정 코어 옵션 자동으로 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "시작 시 커스터마이즈된 코어 옵션을 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "우선 설정 파일 자동으로 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "시작 시 사용자 지정된 구성을 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "입력 설정 파일 자동으로 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "시작 시 사용자 입력설정을 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "셰이더 프리셋 자동으로 불러오기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "전역 코어 옵션 파일 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "모든 코어 옵션을 공통 설정 파일(retroarch-core-options.cfg)에 저장합니다. 비활성화하면 각 코어에 대한 옵션이 RetroArch의 '구성' 디렉터리에 있는 별도의 코어별 폴더/파일에 저장됩니다."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "저장 파일을 코어 이름 별로 폴더에 정렬"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "코어 이름으로 된 폴더안에 세이브파일을 정렬합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "상태저장 파일을 코어 이름 별로 폴더에 정렬"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "코어 이름 폴더별로 저장된 상태저장 파일을 정렬합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "저장 파일을 콘텐츠 디렉토리 별로 폴더에 정렬"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "콘텐츠가 위치한 디렉토리의 이름으로 폴더를 만들어 저장 파일을 정렬합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "상태저장 파일을 콘텐츠 디렉토리 별로 폴더에 정렬"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "콘텐츠가 위치한 디렉토리의 이름으로 폴더를 만들어 상태저장 파일을 정렬합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "상태저장 불러올 때 SaveRAM 덮어쓰지 않기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "상태저장 파일을 불러올 때 SaveRAM을 덮어쓰는 걸 방지합니다. 게임에서 버그가 발생할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "SaveRAM 자동저장 간격"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "비휘발성 SaveRAM을 정기적으로 자동 저장합니다. (초)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "비휘발성 SRAM을 정기적으로 자동 저장합니다. 기본값은 비활성이며 기간은 초 단위입니다. 0으로 설정시 자동저장이 비활성 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "리플레이 체크포인트 간격"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "리플레이 녹화 중에 자동으로 게임의 상태저장을 수행할 시간 간격(초)입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "리플레이 녹화 중에 게임의 상태저장을 일정 간격으로 자동 실행합니다. 기본값은 비활성이며 기간은 초 단위입니다. 0으로 설정시 자동 상태저장이 비활성 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "상태저장 인덱스 자동 증가"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "상태저장을 생성할 때마다 자동으로 인덱스를 증가시킵니다. 불러올 때에는 가장 높은 인덱스의 상태저장을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "리플레이 인덱스 자동 증가"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "리플레이를 생성할 때마다 자동으로 인덱스를 증가시킵니다. 불러올 때에는 가장 높은 인덱스의 리플레이를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "상태저장 인덱스 자동 증가 상한"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "'상태저장 인덱스 자동 증가' 사용 시 활용될 상태저장의 개수 제한을 설정합니다. 상태저장이 개수 제한에 도달할 경우 가장 낮은 인덱스의 상태저장이 삭제됩니다. '0'으로 설정 시 제한을 사용하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "리플레이 인덱스 자동 증가 상한"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "'리플레이 인덱스 자동 증가' 사용 시 활용될 리플레이 개수 제한을 설정합니다. 리플레이 개수 제한에 도달할 경우 가장 낮은 인덱스의 리플레이가 삭제됩니다. '0'으로 설정 시 제한을 사용하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "자동 상태저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "콘텐츠가 종료되면 자동으로 상태저장 파일을 생성합니다. 자동 상태 불러오기가 활성화 되어 있으면 다음 시작시 해당 상태저장을 자동으로 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "상태저장 자동으로 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "시작시 자동으로 상태저장 파일을 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "상태저장 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "메뉴에 상태저장 미리보기를 보입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "SaveRAM 압축"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "비휘발성 SaveRAM 파일을 압축된 형식으로 기록합니다. 저장/불러오기 시간이 늘어나는 대신 파일 크기가 대폭 감소합니다.\n표준 libretro SaveRAM 인터페이스를 통해 저장하는 코어에만 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "상태저장 압축"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "상태저장 파일을 압축된 형식으로 저장합니다. 저장/불러오기 시간이 증가하는 대신 파일 크기를 대폭 줄일 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "스크린샷을 콘텐츠 디렉토리 별로 폴더에 정렬"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "콘텐츠가 위치한 디렉토리의 이름으로 폴더를 만들어 스크린샷 파일을 정렬합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "저장 파일을 콘텐츠 폴더에 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "콘텐츠 디렉토리를 저장 파일 디렉토리로 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "콘텐츠 폴더에 상태저장파일 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "콘텐츠 디렉토리를 저장 상태 디렉토리로 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "콘텐츠 폴더에 시스템 파일 포함"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "콘텐츠 디렉토리를 시스템 디렉토리로 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "스크린샷을 콘텐츠 폴더에 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "콘텐츠 디렉토리를 스크린샷 디렉토리로 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "실행시간 로그 저장 (코어별)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "각각의 콘텐츠 항목을 얼마나 실행했는지 코어별로 기록합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "실행시간 로그 저장 (종합)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "각각의 콘텐츠 항목을 얼마나 실행했는지 모든 코어를 종합해 기록합니다."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "세부정보 로그"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "터미널 또는 파일에 로그를 기록할 것인지 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "프론트엔드 로그 레벨"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "프론트엔드의 로그 수준을 설정합니다. 프론트엔드에서 발생하는 설정값 이하 수준의 로그는 무시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "코어 로그 레벨"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "코어의 로그 수준을 설정합니다. 코어에서 발생하는 설정값 이하 수준의 로그는 무시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "libretro 코어의 로그 수준을 설정합니다(GET_LOG_INTERFACE). libretro 코어에서 발생하는 로그 수준이 libretro_log 설정 값보다 낮을경우 무시됩니다. 디버그 로그는 상세 모드가 활성되지 않은 한 항한 무시됩니다. (--verbose).\nDEBUG = 0\nINFO  = 1\nWARN  = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (디버그)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (정보)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (경고)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (오류)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "파일에 로그 기록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "시스템 이벤트 로그 메시지를 파일로 저장합니다. '세부정보 로그'를 활성화해야 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "시간별 로그 파일 생성"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "각각의 RetroArch 세션마다의 로그를 시간이 표기된 별도의 로그 파일에 저장합니다. 비활성화 시 RetroArch를 실행할 때마다 기존 로그 파일을 덮어쓰게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "성능 카운터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "RetroArch 및 코어의 성능 카운터를 사용합니다. 카운터 데이터는 시스템의 병목 현상 원인 파악 및 시스템과 프로그램의 성능 조정에 사용될 수 있습니다."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "숨겨진 파일 및 폴더 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "파일 탐색기에 숨김 파일 및 폴더를 보입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "알 수 없는 확장자 숨김"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "파일 탐색기에 지원되는 확장자만 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "내장 미디어 플레이어 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "현재 코어에 따라 필터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "최근 사용한 시작 디렉토리 기억"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "시작 디렉토리에서 콘텐츠를 불러올 경우 최근 사용한 위치에서 파일 탐색기를 시작합니다. 참고: RetroArch를 재시작하면 기본 위치로 초기화 됩니다."
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "뒤로 감기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "뒤로 감기 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "프레임 타임 카운터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "프레임 타임 카운터에 영향을 주는 설정을 변경합니다.\n스레드된 비디오가 비활성화되었을 경우에만 사용 가능합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "빨리 감기 비율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "빨리 감기를 사용할 때 콘텐츠가 실행되는 최대 속도입니다. (예: 60fps 콘텐츠의 경우 5.0x = 300fps) 0.0x로 설정하면 빨리 감기 속도는 무제한이 됩니다. (FPS 제한 없음)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "컨텐츠를 빨리 감기 할 때 사용될 최대 실행 속도 배율입니다. (예: 60fps 컨텐츠에 5.0배 => 빨리 감기 시 최대 300fps로 실행)\n이 속도를 넘지 않기 위해 RetroArch는 슬립을 사용하므로, 속도 제한이 정확하지 않을 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "프레임 스킵으로 빨리 감기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "빨리 감기 비율에 따라 프레임을 건너뜁니다. 전력 소모를 줄이고 서드파티 프레임 리미터를 사용할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "슬로모션 비율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "슬로모션을 사용할 때 콘텐츠를 느려지게 할 비율입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "메뉴 프레임레이트 제한"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "메뉴상에 있을 시의 프레임 제한을 설정합니다."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "뒤로 감기 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "최근 게임 플레이의 이전 지점으로 돌아갑니다. 사용 시 심각한 성능 저하가 발생할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "뒤로 감기 프레임"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "한 단계마다 되돌릴 프레임 수입니다. 값이 높을수록 뒤로 감기 속도가 빨라집니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "뒤로 감기 버퍼 크기 (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "되감기 버퍼용으로 예약할 메모리 크기(MB) 입니다. 크기를 늘리면 되감을 수 있는 시간이 늘어납니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "뒤로 감기 버퍼 크기 단계 (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "뒤로 감기 버퍼의 크기를 조절해야할 경우 여기서 지정된 크기만큼씩 변경됩니다."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "빨리 감기 후 초기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "빨리 감기 후 프레임 타임 카운터를 초기화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "상태저장 불러오기 후 초기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "상태저장 불러오기 후 프레임 타임 카운터를 초기화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "상태저장 후 초기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "상태저장 후 프레임 타임 카운터를 초기화합니다."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "녹화 품질"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "사용자 지정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "낮음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "중간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "높음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "무손실"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM 고성능"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM 고화질"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "사용자 지정 녹화 구성"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "녹화 스레드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "필터 적용된 녹화 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "이미지에 필터를 적용하여 캡처합니다(셰이더는 제외). 화면상에 보이는 그대로 멋지게 저장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "GPU 녹화 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "가능한 경우 GPU에서 셰이더 처리된 화면을 녹화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "스트리밍 모드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "로컬"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "사용자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "스트리밍 품질"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "사용자 지정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "낮음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "중간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "높음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "사용자 지정 스트리밍 구성"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "스트림 타이틀"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "스트림 URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "UDP 스트림 포트"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "온스크린 오버레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "베젤과 온스크린 컨트롤을 조정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "비디오 레이아웃"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "비디오 레이아웃을 조정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "온스크린 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "온스크린 알림을 조정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "알림 표시 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "특정 형식의 알림 표시 유무를 설정합니다."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "오버레이 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "오버레이는 테두리 및 온스크린 컨트롤에 사용됩니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "오버레이 메뉴 뒤에 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "오버레이를 메뉴 앞이 아닌 뒤에 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "메뉴에서 오버레이 숨김"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "메뉴 화면에서 오버레이를 숨기고 메뉴 종료 시에 다시 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "컨트롤러 연결 시 오버레이 숨기기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "물리적 컨트롤러가 1번 포트에 연결되면 오버레이를 숨기고, 연결이 해제되면 다시 표시합니다."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "물리적 컨트롤러가 1번 포트에 연결되면 오버레이를 숨깁니다. 컨트롤러를 분리해도 오버레이가 다시 표시되지 않습니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "오버레이에 입력 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "인식된 입력을 온스크린 오버레이에 표시합니다. '터치'는 누르거나 클릭한 오버레이 요소를 강조 표시합니다. '물리 (컨트롤러)'는 연결된 컨트롤러/키보드에서 코어로 전달되는 실제 입력을 강조 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "터치"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "물리 (컨트롤러)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "입력을 표시할 포트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "'오버레이에 입력 표시'를 '물리 (컨트롤러)'로 설정 시 화면에 표시할 입력 포트를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "오버레이에 마우스 커서 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "온스크린 오버레이를 사용할 때 마우스 커서를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "오버레이 자동 회전"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "오버레이가 회전을 지원할 경우 오버레이를 자동 회전하여 화면에 맞춥니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "오버레이 크기 자동 조정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "오버레이 크기와 UI 요소 간 간격을 화면비에 맞춰 자동으로 조정합니다. 컨트롤러 오버레이와 함께 사용하면 좋습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "D-패드 대각선 방향 감도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "대각선 구역의 크기를 조절합니다. 100%로 설정하면 8방향 구역의 크기가 모두 같아집니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "ABXY 겹침 감도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "전면 버튼이 겹치는 부분의 크기를 조절합니다. 100%로 설정하면 8방향 구역의 크기가 모두 같아집니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "오버레이"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "선호 오버레이 자동 불러오기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "오버레이 투명도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "오버레이의 모든 UI 요소에 대한 불투명도를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "오버레이 프리셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "파일 탐색기에서 오버레이를 선택하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(가로) 오버레이 크기 조절"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "가로 화면에서 오버레이에 사용할 모든 UI 구성요소의 크기를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(가로) 오버레이 화면비 조정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "가로 화면에서 사용할 오버레이의 화면비 보정 수치입니다. 값을 키우면 오버레이의 너비가 늘어나고, 값을 줄이면 오버레이의 너비가 줄어듭니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(가로) 오버레이 가로 분할"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "현재 프리셋에서 지원될 경우, 가로 화면에서 사용할 오버레이 UI 구성요소의 왼쪽 절반과 오른쪽 절반 사이 간격의 크기를 조정합니다. 값을 키우면 간격이 늘어나고, 값을 줄이면 간격이 줄어듭니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(가로) 오버레이 세로 분할"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "현재 프리셋에서 지원될 경우, 가로 화면에서 사용할 오버레이 UI 구성요소의 위쪽 절반과 아래쪽 절반 사이 간격의 크기를 조정합니다. 값을 키우면 간격이 늘어나고, 값을 줄이면 간격이 줄어듭니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(가로) 오버레이 X 오프셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "가로 화면에서 사용할 오버레이의 가로 오프셋입니다. 값을 키우면 오버레이가 오른쪽으로 이동하고, 값을 줄이면 오버레이가 왼쪽으로 이동합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(가로) 오버레이 Y 오프셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "가로 화면에서 사용할 오버레이의 세로 오프셋입니다. 값을 키우면 오버레이가 위쪽으로 이동하고, 값을 줄이면 오버레이가 아래쪽으로 이동합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(세로) 오버레이 크기 조절"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "세로 화면에서 오버레이에 사용할 모든 UI 구성요소의 크기를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(세로) 오버레이 화면비 조정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "세로 화면에서 사용할 오버레이의 화면비 보정 수치입니다. 값을 키우면 오버레이의 높이가 늘어나고, 값을 줄이면 오버레이의 높이가 줄어듭니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(세로) 오버레이 가로 분할"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "현재 프리셋에서 지원될 경우, 세로 화면에서 사용할 오버레이 UI 구성요소의 왼쪽 절반과 오른쪽 절반 사이 간격의 크기를 조정합니다. 값을 키우면 간격이 늘어나고, 값을 줄이면 간격이 줄어듭니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(세로) 오버레이 세로 분할"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "현재 프리셋에서 지원될 경우, 세로 화면에서 사용할 오버레이 UI 구성요소의 위쪽 절반과 아래쪽 절반 사이 간격의 크기를 조정합니다. 값을 키우면 간격이 늘어나고, 값을 줄이면 간격이 줄어듭니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(세로) 오버레이 X 오프셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "세로 화면에서 사용할 오버레이의 가로 오프셋입니다. 값을 키우면 오버레이가 오른쪽으로 이동하고, 값을 줄이면 오버레이가 왼쪽으로 이동합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(세로) 오버레이 Y 오프셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "세로 화면에서 사용할 오버레이의 세로 오프셋입니다. 값을 키우면 오버레이가 위쪽으로 이동하고, 값을 줄이면 오버레이가 아래쪽으로 이동합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "키보드 오버레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "키보드 오버레이를 선택하고 조정합니다."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "키보드 오버레이 사전 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "파일 브라우저에서 키보드 오버레이를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "자동 크기 조정 키보드 오버레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "키보드 오버레이를 원래 화면 비율로 조정합니다. 화면으로 늘리려면 비활성화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "키보드 오버레이 불투명도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "키보드 오버레이의 모든 UI 요소의 불투명도입니다."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "비디오 레이아웃 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "비디오 레이아웃은 베젤이나 아트워크 표시에 사용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "비디오 레이아웃 경로"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "비디오 레이아웃을 파일 탐색기에서 선택하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "선택된 보기"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "불러온 레이아웃에서 보기를 선택하십시오."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "온스크린 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "화면에 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "그래픽 위젯"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "개선된 디자인의 애니메이션, 알림, 표시자 및 컨트롤을 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "그래픽 위젯 자동 크기 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "개선된 디잔인의 알림, 표시자 및 컨트롤을 현재의 메뉴 크기에 맞춰 자동으로 크기 조절합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "그래픽 위젯 크기조절 우선설정 (전체 화면)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "전체 화면에서 위젯을 표시할 때 적용할 크기조절 값을 수동으로 강제 적용합니다. '그래픽 위젯 자동 크기 설정'이 비활성화되었을 경우에만 적용됩니다. 알림, 표시, 컨트롤 등의 크기를 메뉴와 상관없이 키우거나 줄일때 사용할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "그래픽 위젯 크기조절 우선설정 (창 모드)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "창 모드에서 위젯을 표시할 때 적용할 크기조절 값을 수동으로 강제 적용합니다. '그래픽 위젯 자동 크기 설정'이 비활성화되었을 경우에만 적용됩니다. 알림, 표시, 컨트롤 등의 크기를 메뉴와 상관없이 키우거나 줄일때 사용할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "FPS 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "화면에 현재 FPS를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "프레임레이트 업데이트 주기 (프레임)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "프레임레이트 표시를 갱신할 프레임 간격입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "프레임 수 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "현재 프레임 수를 화면에 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "통계 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "화면에 전문적인 통계를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "메모리 사용량 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "화면에 시스템의 전체 및 사용된 메모리 양을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "메모리 사용량 업데이트 주기 (프레임)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "메모리 사용량을 갱신할 프레임 간격입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "넷플레이 핑 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "현재 넷플레이 방의 핑을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "\"콘텐츠 불러오기\" 시작 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "콘텐츠를 불러올때 간단한 시작 애니메이션을 보여줍니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "입력 (자동구성) 연결 알림"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "치트 코드 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "치트 코드가 적용되면 화면에 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "패치 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "ROM을 소프트 패치할 때 화면에 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "입력 장치를 연결/연결 해제할 때 화면에 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "입력 설정파일 불러오기 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "입력 리맵 파일을 불러올 때 화면 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "구성 재정의 불러오면 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "구성 재정의 파일을 불러올 때 화면 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "시작 디스크 복원 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "M3U 실행목록을 통해 불러온 마지막으로 사용한 멀티 디스크 콘텐츠 디스크를 실행 시 자동으로 복원할 때 화면에 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "상태저장 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "상태저장을 하거나 불러올 때 화면에 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "빨리 감기 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "콘텐츠를 빨리 감기할 때 화면 표시기를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "스크린샷 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "스크린샷을 찍을 때 화면에 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "스크린샷 알림 지속시간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "화면 스크린샷 메시지의 지속 시간을 정의합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "보통"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "빠르게"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "매우 빠르게"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "순간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "스크린샷 플래시 효과"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "스크린샷을 찍을 때 화면에 원하는 시간동안 흰색 깜박임 효과를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "켬 (보통)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "켬 (빠르게)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "주사율 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "화면의 주사율을 설정할 때 화면에 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "추가 넷플레이 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "중요하지 않은 넷플레이 메시지를 화면에 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "메뉴에서만 알림 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "메뉴가 열려있을 때만 알림을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "OSD 알림 폰트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "화면 알림에 사용할 글꼴을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "OSD 알림 크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "포인트 단위로 폰트 크기를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "OSD 알림 X 위치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "온스크린 텍스트에 대한 사용자 지정 X축 위치를 지정합니다. 0은 왼쪽 가장자리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "OSD 알림 Y 위치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "온스크린 텍스트에 대한 사용자 지정 Y축 위치를 지정합니다. 0은 아래쪽 가장자리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "알림 색 (빨강)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "온스크린 알림 글자 색의 빨간색 값을 설정합니다. 유효한 값은 0에서 255 사이입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "알림 색 (초록)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "온스크린 알림 글자 색의 초록색 값을 설정합니다. 유효한 값은 0에서 255 사이입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "알림 색 (파랑)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "온스크린 알림 글자 색의 파란색 값을 설정합니다. 유효한 값은 0에서 255 사이입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "알림 배경"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "온스크린 알림에 배경 색상을 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "알림 배경 빨간색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "OSD 배경색의 빨간색 값을 설정합니다. 유효한 값은 0에서 255 사이입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "알림 배경 녹색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "OSD 배경색의 녹색 값을 설정합니다. 유효한 값은 0에서 255 사이입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "알림 배경 파란색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "OSD 배경색의 파란색 값을 설정합니다. 유효한 값은 0에서 255 사이입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "알림 배경 투명도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "OSD 배경색의 불투명도를 설정합니다. 유효한 값은 0.0에서 1.0 사이입니다."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "메뉴 항목 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "RetroArch상의 메뉴 항목 표시 유무를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "메뉴"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "메뉴 화면 설정을 변경합니다."
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "3DS 아래 화면 메뉴"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "아래 화면 메뉴의 설정을 변경합니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "고급 설정 보임"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "파워 유저를 위한 고급 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "키오스크 모드 활성화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "모든 구성 관련 설정을 숨겨서 설정을 보호합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "키오스크 모드 비활성화 비밀번호 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "키오스크 모드를 비활성화할때 비밀번호를 설정해 나중에 설정할 수 있게 끔 합니다. 메인 메뉴에서 '키오스크 모드 비활성화'를 선택 후 비밀번호를 입력해 설정할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "이동 경로 연결"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "가로 또는 세로 목록의 끝에 도달하면 시작 또는 끝으로 이어서 연결합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "메뉴 활성 시 콘텐츠 일시정지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "RetroArch 메뉴가 활성화되면 실행중인 콘텐츠를 일시정지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "상태저장 후 콘텐츠 이어하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "빠른 메뉴에서 '상태저장' 또는 '상태저장 불러오기' 실행 후 자동으로 메뉴를 닫고 현재 콘텐츠를 이어합니다. 해제 시 낮은 성능의 기기에서 상태저장 성능을 개선 할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "디스크 변경 후 콘텐츠 이어하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "새 디스크 삽입이나 로딩 후 자동으로 메뉴를 닫고 현재 콘텐츠를 이어합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "콘텐츠 종료시 함께 종료"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "콘텐츠 종료 시 자동으로 RetroArch를 종료합니다. 'CLI'로 설정할 경우 컨텐츠가 명령줄로 실행된 경우에만 종료합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "메뉴 화면 보호기 대기 시간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "메뉴에서 이 시간만큼 활동이 없으면 화면 보호기를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "메뉴 화면 보호기 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "화면 보호기가 작동 중일 때 애니메이션을 표시합니다. 약간의 성능 저하가 생깁니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "눈"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "별"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX,
   "소용돌이"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "메뉴 화면 보호기 애니메이션 속도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "메뉴 화면 보호기 애니메이션 효과의 속도를 조정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "마우스 지원"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "메뉴를 마우스로 조작할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "터치 지원"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "메뉴를 터치스크린으로 조작할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "다중 스레드 작업"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "분할된 스레드에서 작업을 수행합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "활성 시가 아닐 경우 콘텐츠 일시정지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "RetroArch가 활성 창이 아닐 때 실행중인 콘텐츠를 일시중지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "데스크탑 컴포지션 사용 안함"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "윈도우 매니저는 컴포지션을 활용하여 시각적 효과를 적용하거나, 반응하지 않는 창을 찾아내는 등의 작업을 수행합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "콤퍼지션을 강제로 비활성화합니다. 비활성화는 현재 Windows Vista/7에서만 유효합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "메뉴 스크롤 가속"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "한 방향으로 스크롤할 때 커서의 최대 속도를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "메뉴 스크롤 지연 시간"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "메뉴를 누르는 방향으로 빠르게 스크롤하기 전에 기다릴 지연 시간(밀리초)입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "UI 컴패니언 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "시작시 UI 컴패니언을 실행"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "시작 시 사용자 인터페이스 컴패니언 드라이버를 실행합니다.(사용 가능 시)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "데스크탑 메뉴 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "시작시 데스크탑 메뉴 열기"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "빠른 메뉴"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "빠른 메뉴에 표시할 항목을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "설정 탭에 표시할 항목을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "'코어 불러오기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "메인 메뉴에 '코어 불러오기' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "'콘텐츠 불러오기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "메인 메뉴에 '콘텐츠 불러오기' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "'디스크 불러오기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "메인 메뉴에 '디스크 불러오기' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "'디스크 덤프' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "메인 메뉴에 '디스크 덤프' 옵션을 표시합니다."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "'디스크 꺼내기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "메인 메뉴에 '디스크 꺼내기' 옵션을 표시합니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "'온라인 업데이터' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "메인 메뉴에 '온라인 업데이터' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "'코어 다운로더' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "'온라인 업데이터' 옵션에 코어(및 코어 정보) 업데이트 기능을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "이전 버전의 '미리보기 업데이터' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "'온라인 업데이터' 옵션에 이전 버전의 미리보기 패키지 다운로드 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "'정보' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "메인 메뉴에 '정보' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "'구성 파일' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "기본 메뉴에서 '구성 파일' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "'도움말' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "메인 메뉴에 '도움말' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "'RetroArch 종료' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "메인 메뉴에 'RetroArch 종료' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "'RetroArch 다시 시작' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "메인 메뉴에 'RetroArch 다시 시작' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "'설정' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "'설정' 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "'설정' 항목에 비밀번호 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "설정 탭을 숨길 때 비밀번호를 설정해 나중에 되돌릴 수 있게 끔 합니다. 메인 메뉴에서 '설정 탭 표시'를 선택 후 비밀번호를 입력해 되돌릴 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "'즐겨찾기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "'즐겨찾기' 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "'이미지' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "'이미지' 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "'음악' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "'음악' 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "'비디오' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "'비디오' 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "'넷플레이' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "'넷플레이' 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "'최근 실행' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "최근 실행 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "'콘텐츠 가져오기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "'콘텐츠 가져오기' 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH( /* FIXME can now be replaced with MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD */
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "'콘텐츠 가져오기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "메인 메뉴 또는 실행목록 하위메뉴 상에 콘텐츠 가져오기 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "메인 메뉴"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "실행목록 메뉴"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "'실행목록' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "실행목록을 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "'탐색' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "콘텐츠 탐색 메뉴를 표시합니다. (Ozone/XMB 사용시 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "'단독 실행 코어' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "'단독 실행 코어' 메뉴에 표시할 코어의 종류를 선택합니다. '사용자 정의'로 설정할 경우, '코어 관리' 메뉴에서 개별 코어의 표시 여부를 설정할 수 있습니다. (Ozone/XMB에서 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "전체"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "단독 사용만"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "사용자 정의"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "시간/날짜 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "메뉴에 현재 시간 및 날짜를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "시간/날짜 스타일"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "메뉴에 표시될 시간/날짜 스타일을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "날짜 구분선"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "현재 날짜가 메뉴 내부에 표시될 때 년/월/일 구성 요소 사이의 구분 기호로 사용할 문자를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "배터리 수준 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "메뉴에 현재 배터리 상태를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "코어 이름 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "메뉴에 현재 사용 중인 코어 이름을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "메뉴 보조 설명 보이기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "메뉴 항목에 추가적인 정보를 제공합니다."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "시작 화면 표시"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "시작 화면을 표시합니다. 프로그램 첫 실행 후 자동으로 비활성화됩니다."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "'이어하기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "콘텐츠 이어하기 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "'다시 시작' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "'다시 시작' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "'콘텐츠 종료' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "'콘텐츠 종료' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "'상태저장' 하위메뉴 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "상태저장 옵션을 하위 메뉴로 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "'상태저장/불러오기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "상태저장/불러오기 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "'리플레이 컨트롤' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "리플레이 파일의 녹화/재생 옵션을 보입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "'상태저장/불러오기 되돌리기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "상태저장/불러오기 상태 되돌리기 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "'코어 옵션' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "'코어 옵션' 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "'옵션 디스크에 저장' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "'옵션 디스크에 저장' 항목을 '옵션 > 코어 옵션 관리'에 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "'컨트롤' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "'컨트롤' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "'스크린샷 저장' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "'스크린샷 저장' 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "'녹화 시작' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "녹화 시작 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "'스트리밍 시작' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "스트리밍 시작 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "'온스크린 오버레이' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "'온스크린 오버레이' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "'비디오 레이아웃' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "'비디오 레이아웃' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "'지연시간' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "'지연시간' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "'뒤로 감기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "'뒤로 감기' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "'코어 우선 설정 저장' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "'기존 설정 덮어쓰기' 메뉴에 '코어 우선 설정 저장' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "'콘텐츠 디렉터리 재정의 저장하기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "'기존 설정 덮어쓰기' 메뉴에 '콘텐츠 디렉터리 재정의 저장하기' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "'게임 우선 설정 저장' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "'기존 설정 덮어쓰기' 메뉴에 '게임 우선 설정 저장' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "'치트' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "'치트' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "'셰이더' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "'셰이더' 옵션을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "'즐겨찾기에 추가' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "'즐겨찾기에 추가' 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "'코어 연결 설정' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "콘텐츠 실행 중이 아닐 때 '코어 연결 설정' 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "'코어 연결 초기화' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "콘텐츠 실행 중이 아닐 때 '코어 연결 초기화' 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "'미리보기 다운로드' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "콘텐츠 실행 중이 아닐 때 '미리보기 다운로드' 항목을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "'정보' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "'정보' 옵션을 표시합니다."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "'드라이버' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "'드라이버' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "'비디오' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "'비디오' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "'오디오' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "'오디오' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "'입력' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "'입력' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "'지연시간' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "'지연시간' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "'코어' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "'코어' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "'구성' 표시하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "'구성' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "'저장' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "'저장' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "'로그' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "'로그' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "'파일 탐색기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "'파일 탐색기' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "'프레임 조절' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "'프레임 조절' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "'녹화' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "'녹화' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "'온스크린 디스플레이' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "'온스크린 디스플레이' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "'사용자 인터페이스' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "'사용자 인터페이스' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "'AI 서비스' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "'AI 서비스' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "'접근성' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "'접근성' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "'전력 관리' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "'전력 관리' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "'도전과제' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "'도전과제' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "'네트워크' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "'네트워크' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "'실행목록' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "'실행목록' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "'사용자' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "'사용자' 설정을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "'디렉토리' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "'디렉토리' 설정을 표시합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "'Steam' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "'Steam' 설정을 표시합니다."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "크기 배율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "메뉴상의 사용자 인터페이스 크기를 조절합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "배경 이미지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "메뉴 배경으로 설정할 이미지를 선택하세요. 수동 및 동적 이미지는 '색상 테마'보다 우선 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "배경 투명도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "배경화면의 투명도를 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "불투명도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "기본 메뉴 배경화면의 투명도를 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "선호된 시스템 색상 테마 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "시스템 색상 테마가 존재할 경우 이를 적용합니다. 기존 테마 설정은 무시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "첫 번째 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "표시할 미리보기 종류입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "미리보기 확대 임계값"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "지정된 값보다 작은 크기의 미리보기 이미지를 자동으로 확대합니다. 이미지 품질이 개선됩니다. 약간의 성능 저하가 있을 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "한 줄 텍스트 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "긴 메뉴 텍스트 항목을 표시하는데 사용할 가로 스크롤 방식을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "한 줄 텍스트 속도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "긴 메뉴 항목 스크롤시 사용할 애니메이션 속도입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "부드러운 한 줄 텍스트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "긴 메뉴 텍스트를 표시할 때 부드럽게 스크롤합니다. 성능에 작은 영향을 줄 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "탭 전환 시 선택된 항목 기억하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "탭 사이를 전환할 때 커서의 마지막 위치를 기억해둡니다. RGUI에는 탭은 없지만, 실행목록과 설정 메뉴가 탭인 것으로 간주됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "항상"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "실행목록만"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "메인 메뉴와 설정만"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "AI 서비스 출력"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "번역 결과를 이미지 오버레이로 표시하거나(이미지 모드), 오디오로 직접 출력하거나(음성), TTS로 읽어주거나(나레이터), 텍스트 오버레이로 표시합니다(텍스트)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "AI 서비스 URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "번역 서비스에서 사용할 http:// 주소입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "AI 서비스 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "AI 서비스 핫키가 눌러지면 AI 서비스를 실행합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "번역시 일시정지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "화면이 번역되는 동안 코어를 일시정지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "원본 언어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "서비스가 번역해야 하는 원본 언어입니다. '기본'을 선택하면 언어를 자동 탐지합니다. 언어를 지정할 경우 좀 더 정확한 번역을 얻을 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "대상 언어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "서비스가 번역하여 표시할 언어입니다. '기본'을 선택하면 영어로 번역합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_POLL_DELAY,
   "AI 서비스 자동 폴링 딜레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_POLL_DELAY,
   "자동 호출 사이의 최소 딜레이(ms)입니다. 반응성이 낮아지지만 CPU 성능을 높일 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TEXT_POSITION,
   "AI 서비스 텍스트 위치 재정의"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TEXT_POSITION,
   "AI 서비스를 텍스트 모드로 사용할 때, 오버레이의 위치를 재정의합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TEXT_PADDING,
   "AI 서비스 텍스트 패딩 (%)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TEXT_PADDING,
   "AI 서비스를 텍스트 모드로 사용할 때, 텍스트 오버레이에 적용될 수직 패딩입니다. 패딩을 늘리면 텍스트가 화면 가운데로 밀려납니다."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "접근성 기능 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "메뉴 탐색을 위한 TTS 기능을 활성화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "TTS 말하기 속도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "TTS의 말하는 속도를 조정합니다."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "도전과제 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "고전 게임에서 도전과제를 사용할 수 있습니다. 자세한 정보는 https://retroachievements.org에서 확인하세요."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "도전과제 하드코어 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "치트, 뒤로 감기, 슬로우 모션, 상태저장 불러오기를 비활성화합니다. 하드코어 모드에서 달성한 도전과제에는 특별한 표시가 붙어 에뮬레이터 기능을 사용하지 않고 달성했음을 자랑할 수 있습니다. 게임 실행 중에 이 기능을 켜거나 끄면 게임이 다시 시작됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "리더보드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "세부 내역"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "실행중인 게임 정보를 RetroAchievements 웹사이트에 주기적으로 전송합니다. '도전과제 하드코어 모드'를 활성화한 경우 작동하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "도전과제 뱃지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "도전과제 목록에 뱃지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "비공식 도전과제 테스트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "비공식 도전과제 또는 테스트 목적의 베타 기능 사용을 설정/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "잠금 해제 소리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "도전과제 달성시 소리를 재생합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "자동 스크린샷 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "도전과제 달성 시 자동으로 스크린샷을 저장합니다."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "한 번 더 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "모든 도전과제를 활성화하고 세션을 시작합니다. (이전에 달성한 과제도 다시 활성화됨)"
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "모양"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "화면에 표시되는 업적 알림의 위치와 오프셋을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "위치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "도전과제 알람이 표시될 화면상 위치를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "좌측 상단"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "상단 중앙"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "우측 상단"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "좌측 하단"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "하단 중앙"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "우측 하단"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "일관된 패딩 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "도전과제 알림에 다른 유형의 온스크린 알림과 같은 크기의 패딩을 사용할지 설정합니다. 비활성화하면 수동으로 패딩 및 위치 값을 정할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "수동 가로 패딩"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "화면의 왼쪽/오른쪽 테두리에서의 거리입니다. 화면 오버스캔이 있을 경우 보정할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "수동 세로 패딩"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "화면의 위쪽/아래쪽 테두리에서의 거리입니다. 화면 오버스캔이 있을 경우 보정할 수 있습니다."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "표시 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "어떤 메시지 및 온스크린 요소를 표시할지 변경합니다. 기능을 비활성화하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "시작 시 요약 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "불러와진 게임에 대한 정보와 사용자의 현재 진행도를 표시합니다.\n'확인된 모든 게임'으로 설정할 경우 등록된 도전과제가 없는 게임에서도 요약을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "확인된 모든 게임"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "도전과제가 있는 게임"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "달성 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "도전과제 달성 시 알림을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "완전 정복 알림"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "게임의 모든 도전과제를 달성 시 알림을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "활성 도전 지표"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "특정 도전 과제를 달성할 수 있을 때 화면에 지표를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "진행률 지표"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "특정 도전과제를 향한 진행률을 보여주는 온스크린 지표를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "리더보드 시작 메시지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "리더보드가 활성화될 때 리더보드의 설명을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "리더보드 제출 메시지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "리더보드 시도가 완료되어 결과가 제출되었을 때 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "리더보드 실패 메시지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "리더보드 시도가 실패했을 때 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "리더보드 트래커"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "활성 리더보드의 현재 값을 화면에 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "로그인 메시지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "RetroAchievements 계정 로그인과 관련된 메시지를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "자세한 메시지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "추가 진단 및 오류 메시지를 표시합니다."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "공개 넷플레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "넷플레이를 공개적으로 선언할 것인지 설정합니다. 사용하지 않으면 클라이언트는 공용 로비 대신 수동으로 연결해야 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "중계 서버 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "넷플레이 접속 시 중계 서버를 사용합니다. 방장이 방화벽을 사용하고 있거나 NAT/UPnP 문제가 생겼을 경우 유용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "중계 서버 위치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "사용할 특정 중계 서버를 선택합니다. 지리적으로 가깝게 위치한 곳이 지연율이 낮게 나옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "사용자 정의 릴레이 서버 주소"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "사용자 정의 릴레이 서버의 주소를 입력합니다. 포맷: 주소 또는 주소|포트."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "북아메리카 (동부, 미국)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "유럽 서부"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "남아메리카 (남동부, 브라질)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "동남아시아"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "사용자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "서버 주소"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "연결할 호스트 주소를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "넷플레이 TCP 포트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "호스트 IP 주소의 TCP 또는 UDP 포트입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "최대 동시 연결"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "호스트가 수락할 최대 활성 연결 개수입니다. 이 수를 초과한 연결은 모두 거부됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "핑 제한"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "호스트가 받아들일 최대 연결 지연(핑)입니다. 0으로 설정하면 제한하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "서버 비밀번호"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "호스트에 접속하는데 사용할 비밀번호입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "서버 관전 전용 비밀번호"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "관전자로 호스트에 접속할때 사용하는 비밀번호 입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "넷플레이 관전 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "넷플레이를 관전 모드로 실행합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "넷플레이를 관전 모드로 시작할지 설정합니다. 설정할 경우, 넷플레이가 관전 모드로 시작됩니다. 모드는 언제든지 변경할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "페이드 채팅"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "채팅 메시지를 서서히 사라지게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "채팅 컬러 (닉네임)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "형식: #RRGGBB 또는 RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "채팅 컬러 (메세지)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "형식: #RRGGBB 또는 RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "일시정지 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "플레이어들이 넷플레이 도중 일시정지를 할 수 있게 허용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "슬레이브 모드 클라이언트 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "슬레이브 모드 연결을 허용합니다. 슬레이브 모드 클라이언트는 양측에 처리 능력은 거의 들지 않지만 네트워크 지연 발생 시 큰 문제를 겪을 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "슬레이브 모드가 아닌 클라이언트 차단"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "슬레이브 모드가 아닌 연결을 허용하지 않습니다. 낮은 성능의 컴퓨터와 매우 빠른 네트워크가 있는 경우를 제외하고는 권장하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "넷플레이 체크 프레임"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "넷플레이 중 호스트와 클라이언트가 동기화되어있는지 확인하는 프레임의 빈도(프레임)를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "넷플레이 중 호스트와 클라이언트가 동기화되어있는지 확인하는 프레임의 빈도입니다. 대부분의 코어에서, 이 값은 특별한 효과가 없고 무시될 것입니다. 비결정식 코어의 경우, 이 값은 넷플레이 피어들이 얼마나 자주 동기화될 것인지 결정합니다. 버그가 많은 코어에서 이 값을 0 이외의 값으로 설정하는 것은 심각한 성능 문제를 일으킬 수 있습니다. 0으로 설정하면 동기화 [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "입력 대기 프레임"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "넷플레이 시 네트워크 지연을 숨기기 위해 사용할 지연 프레임 수입니다. 입력 지연을 얻는 대신 지터링과 CPU 사용률을 줄입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "넷플레이 시 네트워크 지연을 숨기기 위해 사용할 입력 지연 프레임 수입니다.\n이 옵션은 넷플레이 중에 로컬 입력을 지연시켜, 실행되는 프레임이 네트워크에서 받아오는 프레임과 더 가까워지게 합니다. 이는 화면 떨림 등의 증상을 줄이고 넷플레이의 CPU 사용량을 감소시키지만, 대신 체감 가능한 수준의 입력 지연을 발생시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "입력 대기 프레임 범위"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "네트워크 지연을 숨기기 위해 사용하는 입력 대기 프레임 범위입니다. 예측 불가능한 입력 지연으로 지터를 줄이고 넷플레이의 CPU 사용을 줄입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "넷플레이 중 네트워크 지연을 숨기기 위해 사용하는 입력 대기 프레임 범위입니다.\n설정할 경우, 넷플레이는 CPU 사용 시간, 입력 지연, 네트워크 지연을 균등하게 유지하기 위해 입력 지연 시간을 동적으로 조절합니다. 이는 화면 떨림 등의 증상을 줄이고 넷플레이의 CPU 사용량을 감소시키지만, 대신 예측 불가능한 입력 지연을 발생시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "넷플레이 NAT 트래버설"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "호스팅할 때 UPnP 또는 유사 기술을 사용하여 로컬 네트워크 밖에서의 연결을 수신합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "디지털 입력 공유"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "기기 %u 요청"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "주어진 입력장치로 플레이하기를 요청합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "네트워크 명령"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "네트워크 명령 포트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "네트워크 레트로패드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "네트워크 레트로패드 기본 포트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "사용자 %d 네트워크 레트로패드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "stdin 명령"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "표준입력 명령어 인터페이스 사용을 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "미리보기 없을 시 다운로드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "실행목록을 볼 때 미리보기가 없을 경우 자동으로 다운로드합니다. 퍼포먼스에 큰 영향을 줄 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "업데이터 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "코어 업데이터 설정에 접속"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "빌드봇 코어 URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "libretro 빌드봇 상의 코어 업데이터 디렉토리 URL입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "빌드봇 애셋 URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "libretro 빌드봇 상의 애셋 업데이터 디렉토리 URL입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "다운받은 압축 파일 자동으로 압축 해제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "다운로드 완료 후 자동으로 압축 해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "실험적인 코어 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "'실험적인' 코어를 코어 다운로드 리스트에 포함합니다. 일반적으로 개발/테스트 목적으로만 사용해야 하며 일반 사용에는 추천하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "업데이트 시 코어 백업"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "온라인 업데이트할 때 설치된 코어의 백업을 자동으로 생성합니다. 업데이트를 되돌리고 싶을 경우 손쉬운 롤백이 가능해집니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "코어 백업 히스토리 크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "설치된 각각의 코어에 대해 몇 개의 백업을 자동 생성할지 지정합니다. 지정한 크기에 도달할 경우, 온라인 업데이트로 새로운 백업이 생성되면 가장 오래된 백업이 삭제됩니다. 수동 코어 백업은 이 설정에 영향을 받지 않습니다."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "최근 실행"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "최근에 사용한 게임, 이미지, 음악 및 비디오의 실행목록을 유지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "실행 기록 목록 크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "게임, 이미지, 음악 및 비디오에 대한 최근 실행목록의 항목 수를 제한합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "즐겨찾기 크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "즐겨찾기 목록의 항목 수를 제한합니다. 제한 수량까지 채우면 이전 목록을 삭제하기 전까지 추가 저장이 제한됩니다. 설정값을 -1로 하면 '무제한'으로 설정됩니다.\n경고: 값을 줄이면 기존 항목이 삭제될 수 있습니다!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "항목 편집 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "실행목록의 항목 이름을 변경할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "항목 삭제 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "실행목록의 항목을 삭제할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "실행목록 알파벳 순서로 정렬"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "'최근 실행', '이미지', '음악', '동영상'을 제외한 콘텐츠 실행목록을 알파벳 순서로 정렬합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "이전 형식으로 실행목록 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "실행목록을 단축시킨 일반 텍스트 형식으로 저장합니다. 사용 해제시 실행목록은 JSON 포맷으로 저장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "실행목록 압축"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "디스크에 기록할때 실행목록 데이터를 보관합니다. CPU 사용량이 약간 증가하는 대신 파일 크기와 로드 시간이 줄어듭니다. 이전 혹은 새 형식의 실행목록에 모두 사용할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "실행목록에 연결된 코어 보이기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "연결된 코어가 있을 경우 항목 이름에 표시되게 합니다.\n이 설정은 실행목록 하위 라벨이 활성화되어 있으면 무시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "실행목록 하위라벨 보이기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "각각의 실행목록 항목에 연결된 코어나 실행시간 등 부가적인 정보를 표시합니다. 성능에 영향을 줄 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "최근 실행 및 즐겨찾기에 개별 컨텐츠 전용 아이콘 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "최근 실행 및 즐겨찾기 실행목록의 개별 항목에 대해 전용 아이콘을 표시합니다. 어느 정도 성능 저하가 생깁니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "코어:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "실행시간:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "최근 실행:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "초"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "초"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "분"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "분"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "시간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "시간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "일"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "일"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "주"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "주"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "개월"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "개월"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "년"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "년"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "실행목록 항목 번호 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "실행목록에 항목 번호를 표시합니다. 표시되는 방식은 선택된 메뉴 드라이버에 따라 다릅니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "실행목록 하위 실행시간 라벨 형식"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "실행목록 하위의 실행시간 라벨을 어떻게 표시할지 선택합니다.\n'저장' 설정에서 실행시간 로그 저장이 활성화되어 있어야합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "'최근 실행' 날짜 및 시간 표시형식"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "'최근 실행' 타임스탬프 정보에 표시되는 날짜 및 시간의 형식을 설정합니다. '(AM/PM)' 옵션은 일부 플랫폼에서 성능에 작은 영향을 줄 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "압축 파일 단순 확인"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "압축 파일과 연결된 실행목록 검색시 [파일명]+[콘텐츠] 대신에 파일명 기준으로만 검색합니다. 압축 파일을 불러올 때 실행 기록에 중복된 항목이 생기지 않게 해줍니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "코어 검색 없이 스캔"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "지원되는 코어가 설치되어 있지 않더라도 콘텐츠가 스캔되어 실행목록에 추가될 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "실행목록 관리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "실행목록에 관리 작업을 수행합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "휴대용 실행목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "활성 및 '파일 탐색기' 디렉토리가 선택되면 '파일 탐색기' 매개 변수의 현재 값이 실행목록에 저장됩니다. 동일 옵션이 활성화 된 다른 시스템에서 실행목록을 불러오면 '파일 탐색기' 매개 변수 값이 실행목록 값과 비교되고 다를 경우 자동으로 교정됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "미리보기 찾을 때 파일 이름 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "활성화할 경우, 항목의 미리보기를 라벨이 아닌 파일명을 기준으로 검색합니다."
   )
   MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "관리"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "기본 코어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "실행목록을 통해 실행시 기존에 연결된 코어가 없는 항목에 사용할 코어를 지정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "코어 연결 초기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "실행목록의 모든 항목에서 코어 연결을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "라벨 표시 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "컨텐츠 라벨이 실행목록에서 표시되는 방법을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "정렬 방식"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "이 실행목록에서 항목을 정렬할 방법을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "실행목록 정리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "코어 연결을 확인하고 잘못되었거나 중복된 항목들을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "실행목록 새로 고침"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "실행목록에 마지막으로 수행한 '수동 검색'을 반복하여 새로운 콘텐츠를 추가하고 올바르지 않은 항목을 제거합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "실행목록 삭제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "실행목록을 파일 시스템에서 삭제합니다."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "개인정보 보호"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "개인정보 보호 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "계정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "현재 구성된 계정들을 관리합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "사용자 이름"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "여기에 사용자 이름을 입력하십시오. 넷플레이 세션 등에 사용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "언어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "사용자 인터페이스 언어를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "여기에서 선택한 언어에 따라 메뉴와 모든 화면 메시지를 현지화합니다. 변경 사항을 적용하려면 다시 시작해야 합니다.\n각 옵션 옆에 번역 완성도가 표시됩니다. 메뉴 항목에 대해 언어가 구현되지 않은 경우 영어로 대체됩니다."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "카메라 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "코어가 카메라에 접근할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Discord 앱에 실행 중인 콘텐츠에 대한 정보를 표시합니다.\n네이티브 데스크탑 앱에서만 사용할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "위치 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "코어가 위치 정보에 접근할 수 있게 합니다."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "고전 게임에서 도전과제를 사용할 수 있습니다. 자세한 정보는 https://retroachievements.org에서 확인하세요."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "RetroAchievements 계정의 로그인 정보입니다. retroachievements.org를 방문하여 무료 계정을 만드십시오.\n가입이 완료된 후에, 사용자명과 비밀번호를 RetroArch에 입력하여 사용할 수 있습니다."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "사용자 이름"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "RetroAchievements 계정 이름을 입력합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "비밀번호"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "RetroAchievements 계정의 비밀번호를 입력하십시오. 최대 길이: 255자"
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "YouTube 스트림 키"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Twitch 스트림 키"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Facebook Gaming 스트림 키"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "시스템/바이오스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOS, 부트 롬, 기타 시스템 파일 등이 저장된 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "다운로드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "다운로드된 파일이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "애셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "RetroArch가 사용하는 메뉴 애셋이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "다이나믹 백그라운드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "메뉴에서 사용할 동적 배경화면이 저장된 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "박스 아트, 스크린샷, 타이틀 화면 미리보기가 저장되는 디렉토리입니다."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "파일 탐색기"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "파일 탐색기의 시작 디렉토리를 설정합니다."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "구성"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "메뉴 구성 브라우저의 시작 디렉터리를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "코어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Libretro 코어가 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "코어 정보"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "앱/코어 정보 파일이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "데이터베이스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "데이터베이스가 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "치트 파일"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "치트 파일이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "비디오 필터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "CPU 기반 비디오 필터가 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "오디오 필터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "오디오 DSP 필터가 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "비디오 셰이더"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "GPU 기반 비디오 셰이더가 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "녹화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "녹화 파일이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "녹화 구성하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "녹화 구성은 이 디렉터리에 저장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "오버레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "오버레이가 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "키보드 오버레이"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "키보드 오버레이는 이 디렉터리에 저장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "비디오 레이아웃"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "비디오 레이아웃이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "스크린샷"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "스크린샷이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "컨트롤러 프로필"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "컨트롤러를 자동으로 구성하는 데 사용되는 컨트롤러 프로필은 이 디렉터리에 저장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "입력 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "입력설정 파일이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "실행목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "실행목록이 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "즐겨찾기 재생목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "즐겨찾기 실행목록을 이 디렉토리에 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "최근 실행 실행목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "최근 실행 실행목록을 이 디렉토리에 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "이미지 실행목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "이미지 실행목록을 이 디렉토리에 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "음악 실행목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "음악 실행목록을 이 디렉터리에 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "비디오 실행목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "비디오 재생 목록을 이 디렉터리에 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "실행시간 로그"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "런타임 로그가 저장되는 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "세이브 파일"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "모든 세이브 파일이 저장될 디렉토리. 설정하지 않을 경우 콘텐츠 파일의 작업 디렉토리에 저장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "모든 세이브 파일(*.srm)을 이 디렉토리에 저장합니다. .rt나 .psrm 등의 관련 파일도 모두 포함됩니다. 이 설정은 명령줄 옵션으로 덮어씌워질 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "상태 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "상태저장 및 리플레이 파일이 저장되는 디렉토리입니다. 설정하지 않을 경우 콘텐츠 파일이 존재하는 디렉토리에 저장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "캐시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "압축된 콘텐츠를 임시로 압축 해제할 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "시스템 이벤트 로그"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "시스템 이벤트 로그가 저장되는 디렉토리입니다."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "고급 상태 표시 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Steam에서 RetroArch를 사용할 때 현재 상태를 공유합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "고급 상태 표시 콘텐츠 형식"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "실행중인 콘텐츠에 대해 어떤 정보를 공유할지 결정합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "콘텐츠"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "코어 이름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "시스템 이름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "콘텐츠 (시스템 이름)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "콘텐츠 (코어 이름)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "콘텐츠 (시스템 이름 - 코어 이름)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "믹서에 추가"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "이 오디오 트랙을 사용 가능한 오디오 스트림 슬롯에 추가합니다.\n재생 가능한 슬롯이 없을 경우 무시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "믹서에 추가 후 재생"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "이 오디오 트랙을 사용 가능한 오디오 스트림 슬롯에 추가한 뒤 재생합니다.\n재생 가능한 슬롯이 없을 경우 무시됩니다."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "넷플레이 호스트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "넷플레이 호스트 연결"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "서버 주소를 입력하여 클라이언트 모드로 넷플레이에 연결합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "넷플레이 호스트로부터 연결해제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "활성 중인 모든 넷플레이 연결을 해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "로비 필터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "연결 가능한 방만 표시"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "코어가 설치되어있는 방만 표시"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "비밀번호 설정된 방"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "넷플레이 호스트 목록 새로 고침"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "넷플레이 호스트를 검색합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "넷플레이 LAN 목록 새로 고침"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "LAN에서 넷플레이 호스트를 검색합니다."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "넷플레이 호스트 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "호스트(서버) 모드로 넷플레이를 시작합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "넷플레이 호스트 중지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "클라이언트 강제 퇴장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "호스트 중인 방에서 이 클라이언트를 강제로 퇴장시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "클라이언트 차단"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "호스트 중인 방에서 이 클라이언트를 차단합니다."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "디렉토리 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "디렉토리에서 데이터베이스에 일치하는 콘텐츠를 검색합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<현재 디렉토리 검색>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "선택해서 현재 디렉토리의 콘텐츠를 검색합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "파일 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "파일에서 데이터베이스에 일치하는 콘텐츠를 검색합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "수동 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "콘텐츠 파일 이름을 기반으로 구성 가능한 스캔입니다. 데이터베이스와 일치하는 내용이 필요하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "스캔"
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "믹서에 추가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "믹서에 추가 후 재생"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "콘텐츠 디렉토리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "콘텐츠를 검색할 디렉토리입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "시스템 이름"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "검색된 콘텐츠를 분류할 '시스템 이름'입니다. 생성될 실행목록의 이름과 미리보기를 찾는 데 사용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "사용자 정의 시스템 이름"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "검색된 콘텐츠의 '시스템 이름'을 직접 지정합니다. '시스템 이름'이 '<사용자 정의>'일 때만 사용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "기본 코어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "검색된 콘텐츠를 실행할 기본 코어를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "파일 확장자"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "공백으로 구분된 검색할 파일 확장자 목록입니다. 비워둘 경우 모든 파일, 코어가 지정된 경우 코어가 지원하는 모든 파일이 포함됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "연속 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "사용시 '콘텐츠 디렉토리' 의 모든 하위 디렉토리가 검색에 포함됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "압축파일 내 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "사용할 경우, 압축 파일(.zip, .7z 등) 속의 파일들도 모두 검색합니다. 검색 성능에 큰 영향을 줄 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "아케이드 DAT 파일"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Logiqx 또는 MAME 리스트 XML DAT 파일을 선택하여 검색되는 아케이드 콘텐츠(MAME, FinalBurn Neo 등)의 이름을 자동으로 정할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "아케이드 DAT 필터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "아케이드 DAT 파일 사용 시, 일치하는 DAT 파일 항목이 발견되는 콘텐츠만 실행 목록에 추가됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "실행목록 덮어쓰기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "사용할 경우, 콘텐츠를 검색하기 전 이미 존재하는 실행목록은 삭제됩니다. 그렇지 않을 경우, 이미 존재하는 실행목록은 유지되고 실행목록에 없는 새 항목만 실행목록에 추가됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "기존 항목 검증"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "사용할 경우, 새로운 콘텐츠를 검색하기 전에 실행목록에 현재 등록된 항목들을 검증합니다. 가리키는 파일이 없거나 올바르지 않은 확장자의 파일을 가리키는 항목은 제거됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "검색 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "선택한 콘텐츠 검색을 시작합니다."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "목록 준비 중..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "출시 년도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "플레이어 수"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "지역"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "태그"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "이름 검색..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "모두 보기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "추가 필터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "전체"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "추가 필터 추가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u 항목"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "개발사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "판매사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "출시 년도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "플레이어 수"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "장르"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "도전과제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "카테고리"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "언어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "지역"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "콘솔 독점"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "플랫폼 독점"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "진동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "점수"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "미디어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "조작 방식"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "아트 스타일"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "게임플레이"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "내러티브"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "진행 속도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "시점"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "비주얼"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "탑승물"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "원본"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "프랜차이즈"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "태그"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "시스템 이름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "범위 필터 설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "보기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "뷰로 저장하기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "이 뷰 삭제하기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "새로운 뷰의 이름을 입력하세요"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "같은 이름의 뷰가 이미 존재합니다"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "뷰가 저장되었습니다"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "뷰가 삭제되었습니다"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "실행"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "콘텐츠를 실행합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "이름 변경"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "항목의 제목을 수정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "제거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "실행목록에서 항목을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "즐겨찾기에 추가"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "항목을 '즐겨찾기'에 추가합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "코어 연결 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "이 콘텐츠를 실행할 코어를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "코어 연결 초기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "이 콘텐츠를 실행할 코어 설정을 초기화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "정보"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "콘텐츠에 대한 자세한 정보를 확인합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "미리보기 다운로드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "현재 콘텐츠의 스크린샷/박스아트/타이틀 화면의 미리보기 이미지를 다운로드합니다. 기존 미리보기가 있을 경우 덮어씌워집니다."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "현재 코어"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "이름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "파일 경로"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "항목: %lu/%lu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "코어"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "실행 시간"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "최근 실행"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "데이터베이스"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "이어하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "빠른 메뉴를 종료하고 현재 실행 중인 콘텐츠를 이어합니다"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "다시 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "콘텐츠를 처음부터 다시 시작합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "콘텐츠 종료"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "현재 콘텐츠를 종료합니다. 저장되지 않은 내용은 사라지게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "스크린샷 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "화면 이미지를 캡처합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "저장 슬롯"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "선택된 상태저장 슬롯을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "상태저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "현재 선택된 슬롯에 상태저장 파일을 기록합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "상태저장 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "현재 선택된 슬롯에서 상태저장 파일을 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "상태 불러오기 취소"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "상태저장 파일을 불러왔을 때, 불러오기 이전 상태로 되돌립니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "상태저장 되돌리기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "상태저장 파일을 덮어씌웠을 때, 이전 저장 상태로 되돌립니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "리플레이 슬롯"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "선택된 상태저장 슬롯을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "리플레이 재생"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "현재 선택된 슬롯에서 리플레이 파일을 재생합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "리플레이 녹화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "현재 선택된 슬롯에 리플레이 파일을 기록합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "녹화/재생 중지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "현재 리플레이 녹화/재생 중지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "즐겨찾기에 추가"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "항목을 '즐겨찾기'에 추가합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "녹화 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "비디오 녹화를 시작합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "녹화 중지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "비디오 녹화를 중지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "스트리밍 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "선택한 서비스로 스트리밍을 시작합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "스트리밍 중지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "스트리밍을 종료합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "상태 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "상태저장 옵션에 접근합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "코어 옵션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "현재 실행중인 콘텐츠의 옵션을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "컨트롤"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "현재 실행중인 콘텐츠의 조작방법 설정을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "치트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "치트 코드를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "디스크 제어"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "디스크 이미지를 관리합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "셰이더"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "화면에 시각적으로 더해질 셰이더를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "기존 설정 덮어쓰기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "전역 구성을 재정의하기 위한 옵션입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "도전과제 목록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "도전과제 및 관련 설정을 봅니다."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "코어 옵션 관리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "현재 콘텐츠에 대한 우선 설정 옵션을 저장 또는 삭제합니다."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "게임 옵션 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "현재 콘텐츠에만 적용되는 코어 옵션을 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "게임 옵션 제거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "현재 콘텐츠에만 적용되는 코어 옵션을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "콘텐츠 폴더 설정 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "현재 파일과 동일한 디렉토리에서 로드된 모든 콘텐츠에 적용되는 코어 옵션 저장입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "콘텐츠 디렉터리 제거 옵션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "현재 파일과 동일한 디렉토리에서 로드된 모든 콘텐츠에 적용되는 코어 옵션을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "활성 옵션 파일"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "현재 사용중인 옵션 파일입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "옵션 초기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "모든 코어 옵션을 기본값으로 초기화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "옵션 디스크에 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "현재 설정을 활성 옵션 파일에 강제로 작성합니다. 코어의 버그로 프론트엔드가 비정상 종료되는 등의 상황이 발생하더라도 변경한 옵션이 제대로 보존되게 합니다."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "게임 옵션 파일 생성"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "게임 옵션 파일 저장"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "입력설정 파일 관리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "현재 콘텐츠에 대한 입력설정 파일을 저장, 불러오기, 삭제합니다."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "활성 입력 설정 파일"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "현재 사용중인 입력 설정 파일입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "입력설정 파일 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "현재 입력 매핑을 로드하고 바꿉니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "다른 이름으로 리매핑 파일 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "현재 입력 매핑을 새 리매핑 파일로 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "코어 입력설정 파일 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "이 코어로 로드된 모든 콘텐츠에 적용될 리맵 파일을 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "코어 리매핑 파일 제거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "이 코어로 로드된 모든 콘텐츠에 적용될 리맵 파일을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "입력설정 파일을 콘텐츠 폴더에 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "현재 파일과 동일한 디렉토리에서 로드된 모든 콘텐츠에 적용될 리매핑 파일을 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "게임 콘텐츠 디렉토리 리매핑 파일 제거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "현재 파일과 동일한 디렉토리에서 로드된 모든 콘텐츠에 적용될 리매핑 파일을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "게임 입력설정 파일 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "현재 콘텐츠에만 적용될 리매핑 파일을 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "게임 리맵 파일 제거"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "현재 콘텐츠에만 적용될 리맵 파일을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "입력 매핑 초기화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "모든 입력설정 옵션을 기본값으로 초기화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "입력설정 파일 갱신"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "현재 입력 설정을 활성 입력설정 파일에 덮어씌워 저장합니다."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "입력설정 파일"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "치트 검색 시작 또는 재개"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT,
   "새로운 치트를 만들기위해 메모리를 스캔합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "치트 파일 불러오기 (교체)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "치트 파일을 불러옵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "치트 파일 불러오기 (추가)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "치트 파일을 불러오고 기존 치트에 병합합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "게임 전용 치트 다시 불러오기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "치트 파일 다른 이름으로 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "현재 치트를 치트 파일로 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "새 치트를 가장 위에 추가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "새 치트를 가장 아래에 추가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "전체 치트 삭제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "게임 불러올 때 자동으로 치트 적용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "게임을 불러올 때 치트를 자동으로 적용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "선택 후 적용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "선택 후 치트가 즉시 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "변경 적용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "치트의 변경 사항은 즉시 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "치트"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "치트 검색 시작 또는 재시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "좌/우로 비트 크기를 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
   "빅 엔디안"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN,
   "빅 엔디안일 때: 258 = 0x0102\n리틀 엔디안일 때: 258 = 0x0201"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "좌/우로 값을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "%u 동일값(%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "이전 보다 줄어듬"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "이전 이하"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "이전 보다 늘어남"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "이전 이상"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "이전과 동일"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "이전과 다름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "좌, 우로 값을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "이전과 동일+%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "메모리에서 값 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "좌, 우로 값을 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "이전과 동일-%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "%u 일치항목을 목록에 추가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "일치 삭제 #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "코드 일치 생성 #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "일치 주소: %08X 마스크: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "치트 파일 (교체)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "치트 파일 (추가)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "치트 세부사항"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "인덱스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "목록상의 치트 위치입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "설명"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "핸들러"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "메모리 검색 크기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "형식"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "값"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "메모리 주소"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "주소 보기: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "메모리 주소 마스크"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "메모리 검색 사이즈가 8비트 미만일 경우 사용할 주소 비트마스크입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "반복 횟수"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "치트가 적용되는 횟수입니다. 메모리의 많은 영역에 적용하려면 다른 두 반복 옵션과 함께 사용하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "반복 시 주소 증가"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "반복 횟수마다 메모리 검색 사이즈에 지정한 숫자만큼 메모리 주소를 증가시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "반복 시 증가 값"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "반복 횟수마다 지정된 크기만큼 값을 증가시킵니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "메모리 있을 시 진동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "진동 값"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "진동 포트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "진동 기본 강도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "진동 기본 지속시간(ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "진동 추가 강도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "진동 추가 지속시간(ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "코드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "다음에 새 치트 추가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "이전에 새 치트 추가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "현재 치트를 뒤에 복사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "현재 치트를 앞으로 복사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "현재 치트 삭제"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "디스크 꺼내기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "가상 디스크 트레이를 열고 현재 불러온 디스크를 제거합니다. '메뉴 활성 시 콘텐츠 일시정지'가 활성화 된 경우 일부 코어는 디스크 제어 작업 후 몇 초 동안 변경 사항을 확인하지 못할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "디스크 이미지 추가"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "'현재 디스크 인덱스'에 해당하는 디스크를 삽입하고 가상 디스크 트레이를 닫습니다. '메뉴 활성 시 콘텐츠 일시정지'가 활성화 된 경우 일부 코어는 디스크 제어 작업 후 몇 초 동안 변경 사항을 확인하지 못할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "새 디스크 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "현재 디스크를 꺼내고 파일 시스템에서 새 디스크를 선택, 삽입하고 가상 디스크 트레이를 닫습니다. \n참고: 이 기능은 레거시 기능입니다. 대신 '디스크 꺼내기/삽입' 및 '현재 디스크 인덱스' 옵션으로 디스크 선택을 할 수 있는 M3U 실행목록을 통한 멀티 디스크 타이틀 사용을 추천합니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "가상 트레이를 닫지 않고 파일 시스템에서 새 디스크를 선택, 삽입합니다. \n참고: 이 기능은 레거시 기능입니다. 대신 '현재 디스크 인덱스' 옵션으로 디스크 선택을 할 수 있는 M3U 실행목록을 통한 멀티 디스크 타이틀 사용을 추천합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "디스크 인덱스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "디스크 인덱스를 변경합니다. 디스크 변경은 트레이를 연 다음 수행해야 합니다.\nRetroArch가 메뉴 표시 중 콘텐츠를 일시정지하도록 설정되어 있는 경우, 일시정지를 해제할 때까지 트레이 열기/닫기 액션이 코어에 전달되지 않을 수도 있습니다. 그런 경우 디스크를 조작할 때마다 게임을 몇 프레임간 실행하는 것을 잊지 말아주세요."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "비디오 셰이더"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "비디오 셰이더 파이프라인을 활성화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "셰이더 파일 변경 확인"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "디스크상의 셰이더 파일 변경을 자동으로 적용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "셰이더 파일에서 변경 사항을 확인합니다. 셰이더 변경 사항을 디스크에 저장하면 자동으로 다시 컴파일되어 실행 중인 콘텐츠에 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "최근 사용한 셰이더 디렉토리 기억"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "세이더 프리셋 및 패스를 불러올때 파일 브라우저에서 최근 사용한 디렉토리를 엽니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "셰이더 프리셋을 불러옵니다. 셰이더 파이프라인이 자동으로 설정됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "셰이더 사전 설정을 직접 불러옵니다. 그에 따라 셰이더 메뉴가 업데이트됩니다.\n메뉴에 표시되는 배율은 사전 설정이 간단한 배율 조정 방법(예: 소스 배율 조정, X/Y에 대해 동일한 배율)을 사용하는 경우에만 신뢰할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "현재 셰이더 프리셋을 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "자동 셰이더 프리셋을 제거합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "변경 적용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "셰이더 구성에 대한 변경 내용은 즉시 적용됩니다. 셰이더 패스, 필터링, FBO 스케일 등의 양을 변경한 경우 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "셰이더 패스의 크기, 필터링, FBO 배율과 같은 셰이더 설정을 변경한 후 이를 사용하여 변경 사항을 적용합니다.\n이러한 셰이더 설정을 변경하는 것은 다소 성능을 소모하는 작업이므로 정확히 수행해야 합니다.\n셰이더를 적용할 때 셰이더 설정은 임시 파일(retroarch.slangp/.cgp/.glslp)에 저장하고 로드합니다. 파일은 RetroArch가 종료된 후에도 지속되며 셰이더 디렉토리에 저장됩니[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "셰이더 매개변수 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "현재 셰이더를 직접 변경합니다. 변경사항이 프리셋 파일로 저장되지는 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "셰이더 패스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "셰이더 파이프라인 패스의 양을 늘리거나 줄입니다. 개별 셰이더를 각 파이프라인 패스에 바인딩하고 크기 및 필터링을 구성할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch는 사용자 정의 하드웨어 필터 및 배율 인수 같은 임의의 셰이더 패스를 사용하여 다양한 셰이더를 혼합하고 통합 시킬 수 있습니다.\n이 옵션은 사용할 셰이더 패스 수를 지정합니다. 값을 0으로 설정하고 셰이더 변경 사항을 적용할 경우 '비어있는' 셰이더를 사용하는 것과 같습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "셰이더"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "셰이더에 대한 경로입니다. 모든 셰이더는 동일한 유형이어야 합니다. (예: Cg, GLSL 또는 Slang) 셰이더 디렉터리를 설정하여 브라우저가 셰이더를 찾을 위치를 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "필터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "현재 패스에 대한 하드웨어 필터입니다. '기본값'이 설정된 경우 필터는 비디오 설정의 '이중선형 필터링' 설정에 따라 '선형' 또는 '최근접'이 됩니다."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "크기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "현재 패스의 배율. 배율 인수는 누적됩니다. 첫 번째 패스에서 2x, 두 번째 패스에서 2x일 경우 총 배율은 4배가 됩니다.\n마지막 패스에 배율 인수가 있는 경우 결과는 비디오 설정의 이중선형 필터링 설정에 따라 기본 필터에 적용되어 화면에 확장됩니다.\n'기본값'이 설정되어 있으면 마지막 패스가 아닌지 여부에 따라 1x 배율 또는 전체 화면으로 확대가 사용됩니다."
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "간단 프리셋"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "원본 프리셋 링크와 사용자가 변경한 항목만을 포함한 셰이더 프리셋을 저장합니다."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "셰이더 프리셋 다른 이름으로 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "현재 셰이더 설정을 새로운 프리셋으로 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "전역 설정 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "현재 셰이더 설정을 기본 전역 설정으로 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "코어 프리셋 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "현재 셰이더 설정을 코어의 기본 값으로 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "콘텐츠 폴더 프리셋 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "현재의 셰이더 설정을 콘텐츠가 있는 폴더의 모든 파일에 기본 설정으로 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "게임 프리셋 저장"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "현재 셰이더 설정을 콘텐츠의 기본 값으로 저장합니다."
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "자동 셰이더 프리셋 찾지 못함"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "통합 프리셋 삭제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "모든 콘텐츠와 코어에 사용하는 통합 프리셋을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "코어 프리셋 삭제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "현재 불러온 코어에 적용되는 코어 프리셋을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "콘텐츠 디렉토리 프리셋 삭제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "현재 작업중인 디렉토리 안의 모든 컨테츠에 적용되는 디렉토리 프리셋을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "게임 프리셋 삭제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "특정 게임에만 적용되는 게임 프리셋을 삭제합니다."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "셰이더 매개변수 없음"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "우선 설정 파일 활성화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "현재 사용중인 우선 설정 파일입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "우선 설정 파일 불러오기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "현재 구성을 불러오고 교체합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "다른 이름으로 저장 재정의"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "현재 구성을 새 재정의 파일로 저장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "코어 재정의 저장하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "이 코어로 불러온 모든 콘텐츠에 적용할 재정의 구성 파일을 저장합니다. 기본 구성보다 우선합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "코어 재정의 제거하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "현재 코어에서 불러올 모든 콘텐츠에 적용할 우선 설정 파일을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "콘텐츠 디렉터리 재정의 저장하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "현재 파일과 동일한 디렉터리에서 불러온 모든 콘텐츠에 적용할 재정의 구성 파일을 저장합니다. 기본 구성보다 우선합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "콘텐츠 디렉터리 재정의 제거하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "현재 파일과 동일한 폴더에서 불러올 모든 콘텐츠에 적용할 우선 설정 파일을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "게임 재정의 저장하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "현재 콘텐츠에만 적용되는 재정의 구성 파일을 저장합니다. 기본 구성보다 우선합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "게임 재정의 제거하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "현재 콘텐츠에만 적용되는 우선 설정 파일을 삭제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "재정의 불러오기 취소"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "모든 옵션을 전역 구성 값으로 재설정합니다."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "표시할 도전과제 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "도전과제 하드코어 모드 일시정지 해제"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "현재 세션에 한해 도전과제 하드코어 모드를 활성화합니다"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "도전과제 하드코어 모드 계속하기 취소"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "현재 세션에 한해 도전과제 하드코어 모드를 비활성화합니다"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "도전과제 하드코어 모드 일시정지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "현재 세션에 대한 도전과제 하드코어 모드를 일시 정지합니다. 치트, 뒤로 감기, 슬로우 모션, 상태저장 불러오기를 활성화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "도전과제 하드코어 모드 계속하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "현재 세션에 대한 도전과제 하드코어 모드를 재개합니다. 치트, 뒤로 감기, 슬로우 모션, 상태저장 불러오기를 비활성화하고 게임을 초기화합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "로그인 되지 않음"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "네트워크 오류"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "알 수 없는 게임"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "이 코어로는 도전 과제를 사용할 수 없습니다"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "RetroAchievements 해시"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "데이터베이스 항목"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "현재 콘텐츠의 데이터베이스 정보를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "표시할 항목 없음."
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "이용 가능한 코어 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "이용 가능한 코어 옵션 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "이용 가능한 코어 정보 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "이용 가능한 코어 백업 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "즐겨찾기가 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "이용 가능한 게임 실행 내역 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "사진이 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "음악이 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "영상이 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "이용 가능한 정보 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "이용 가능한 항목이 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "설정 항목 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "이용 가능한 블루투스 기기 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "네트워크 찾지 못함."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "코어 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "검색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "미리보기 전환"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "이전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "확인"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "상위 폴더"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "부모 디렉터리로 돌아갑니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "디렉토리 찾지 못함."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "항목 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "파일 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_NORMAL,
   "보통"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_NORMAL,
   "보통"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270도"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "알 수 없는 컴파일러"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "공유"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "잡기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "투표"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "아날로그 입력 공유"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "최대"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "평균"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "선호항목 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "좌/우 바운스"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "왼쪽으로 스크롤"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "이미지 모드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "음성 모드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "나레이터 모드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TEXT_MODE,
   "텍스트 모드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TEXT_NARRATOR_MODE,
   "텍스트 + 나레이터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_NARRATOR_MODE,
   "이미지 + 나레이터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TEXT_POSITION_BOTTOM,
   "아래"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TEXT_POSITION_TOP,
   "상단"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "최근 실행 & 즐겨찾기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "모든 실행목록"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "끔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "최근 실행 & 즐겨찾기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "항상"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "표시 안 함"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "코어 별"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "종합"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "충전됨"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "충전 중"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "방전 중"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "소스 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<현재 디렉토리 사용>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "디렉토리로 설정하려면 선택하세요."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<콘텐츠 경로>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<기본>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<없음>"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
   "Retro키보드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD,
   "레트로패드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "레트로패드 아날로그"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "알 수 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "아래 + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_L1_R1_START_SELECT,
   "L1 + R1 + 시작 + 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_SELECT,
   "시작 + 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "시작 버튼 누르기 (2초)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "선택 버튼 누르기 (2초)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "아래 + 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<사용안함>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "변경"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "변경 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "증가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "감소"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= 진동 값"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= 진동 값"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< 진동 값"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> 진동 값"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "진동 값으로 증가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "진동 값으로 감소"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "전체"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<사용 안 함>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "값 지정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "다음 값으로 증가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "다음 값으로 감소"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "값 = 메모리 일시 다음 치트 실행"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "값 != 메모리 일시 다음 치트 실행"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "값 < 메모리 일시 다음 치트 실행"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "값 > 메모리 일시 다음 치트 실행"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "에뮬레이터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-bit, 최대 값 = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-bit, 최대 값 = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-bit, 최대 값 = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-bit, 최대 값 = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-bit, 최대 값 = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-bit, 최대 값 = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "시스템 기본"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "알파벳순"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "라벨 모두 표시"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "() 제외"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "[] 제외"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "() 및 [] 제외"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "지역 유지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "디스크 인덱스 유지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "지역 및 디스크 인덱스 유지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "시스템 기본"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "박스아트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "스크린샷"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "타이틀 화면"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "보통"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "빠르게"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "켬"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "끔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "예"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "아니오"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "참"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "거짓"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "사용 안 함"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "이용불가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "잠김"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "달성함"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
   "하드코어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "비공식"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "지원 안 됨"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "최근에 달성한 도전과제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "거의 다 됐어요"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "활성 중인 도전과제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "트래커만"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "알림만"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "기본"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "선형"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "최근접"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "메인"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "콘텐츠"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<콘텐츠 디렉토리>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<사용자 정의>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<지정 안 함>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "좌 아날로그"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "우 아날로그"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "좌 아날로그 (강제)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "우 아날로그 (강제)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "키 %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "마우스 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "마우스 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "마우스 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "마우스 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "마우스 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "마우스 휠업"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "마우스 휠다운"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "마우스 휠 왼쪽으로"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "마우스 휠 오른쪽으로"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "일찍"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "보통"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "늦게"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "전"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "배경화면 패턴 굵게"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "메뉴 배경의 체크 무늬 패턴을 굵게 그립니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "창 테두리 채우기 설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "창 테두리 굵기 설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "메뉴 테두리의 두께를 증가시킵니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "메뉴 테두리를 보입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "전체 너비 레이아웃 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "메뉴가 화면 영역을 최대한으로 활용하도록 크기를 조절합니다. 이전의 고정폭 레이아웃을 사용하려면 비활성하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "선형 필터"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "메뉴에 약간의 흐림 효과를 추가해서 픽셀 경계를 부드럽게 해줍니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "내부 업스케일링"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "화면을 표시하기 전에 메뉴 인터페이스를 업스케일합니다. 메뉴 선형 필터 옵션과 함께 사용 시 깔끔하면서 또렷한 이미지를 얻을 수 있습니다. 업스케일 수준을 높일 경우 심각한 성능저하를 가져올 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "화면비"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "메뉴 화면비를 선택하십시오. 와이드스크린 비율은 메뉴의 수평 해상도를 높여줍니다. (메뉴 화면비율 잠금이 해제돼있을 경우 재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "화면비 고정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "메뉴가 항상 올바른 화면비로 표시되도록 해주비다. 해제하면 빠른 메뉴가 현재 콘텐츠에 맞춰 늘려져서 표시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "색상 테마"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "컬러 테마를 선택합니다. '사용자' 설정을 선택하면 메뉴 테마 프리셋 파일을 사용할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "사용자 테마 프리셋"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "파일 탐색기에서 테마 프리셋을 선택하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "투명도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "빠른 메뉴 활성 시 실행 중인 컨텐츠를 배경 화면으로 표시합니다. 이 설정을 변경하면 테마 색상이 달라질 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "그림자 효과"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "메뉴 문자, 테두리, 미리보기에 그림자를 표시합니다. 약간의 성능저하가 생깁니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "배경 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "배경에 입자 애니메이션을 사용합니다. 심각한 성능 저하가 있을 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "배경 애니메이션 속도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "배경 입자 애니메이션 효과의 속도를 조정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "화면 보호기 배경 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "화면 보호기가 작동중일 때 배경 입자 효과를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "실행목록 미리보기 보이기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "실행목록을 볼 때 목록에 축소된 미리보기를 표시합니다. RetroPad 선택 버튼으로 켜고 끌 수 있습니다. 비활성화하더라도, 미리보기의 전체화면 표시는 RetroPad 시작 버튼을 눌러 켜고 끌 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "상단 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "오른쪽 상단에 표시될 미리보기의 종류입니다. RetroPad Y 버튼을 누르면 미리보기 종류를 전환할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "하단 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "오른쪽 아래에 표시할 미리보기의 종류입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "미리보기 전환"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "'상단 미리보기'와 '하단 미리보기'의 표시를 전환합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "미리보기 축소 방법"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "큰 미리보기 파일을 화면 해상도에 맞게 축소하는 방법입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "미리보기 지연 (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "실행목록 항목에 연결된 미리보기 불러오기 시 지연시간을 적용합니다. 256ms 이상의 값을 적용하면 느린 기기에서도 빠르고 끊김 없는 스크롤 효과를 볼 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "확장 ASCII 지원"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "비표준 ASCII 문자를 표시합니다. 일부 비영어권 서양 언어에서 필요합니다. 약간의 성능저하가 있을 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "스위치 아이콘"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "메뉴 설정 항목에서 '켬/끔' 글자 표기 대신에 아이콘을 사용합니다."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "최근접이웃 (빠름)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "이중선형보간"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "싱크/란초스3 (느림)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "자동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (가운데 맞춤)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (가운데 맞춤)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (가운데 맞춤)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (가운데 맞춤)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (가운데 맞춤)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "자동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "끄기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "화면에 맞춤"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "정수 단위 조정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "화면 채움 (늘리기)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "사용자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "클래식 빨강"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "클래식 주황"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "클래식 노랑"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "클래식 녹색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "클래식 파랑"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "클래식 보라"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "클래식 회색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "레거시 빨강"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "어두운 자주"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "암청색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "금색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "일렉트릭 파랑"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "애플 녹색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "볼캐닉 빨강"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "라군"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER,
   "브로그래머"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA,
   "드라큘라"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "솜사탕"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI,
   "플랫 UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "그루브박스 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "그루브박스 라이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "커널 해킹"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD,
   "노드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NOVA,
   "노바"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
   "원 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT,
   "페일나이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "솔라라이즈 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "솔라라이즈 라이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "탱고 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "탱고 라이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ZENBURN,
   "젠번"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ANTI_ZENBURN,
   "안티 젠번"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLUX,
   "플럭스"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "동적"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "어두운 회색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "밝은 회색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "끔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "눈 (약간)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "눈 (많이)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "비"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "소용돌이"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "별"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "두 번째 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "왼쪽에 표시할 미리보기의 종류입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "다이나믹 백그라운드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "상황에 따라 새로운 배경화면을 동적으로 불러오게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "수평 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "메뉴의 수평 애니메이션을 활성화합니다. 성능에 영향이 있을 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "수직 아이콘 강조 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "탭사이를 스크롤 할때 발생하는 애니메이션입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "상/하 이동 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "위 아래를 움직일때 발생하는 애니메이션입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "메뉴 열기/닫기 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "메뉴를 열때 발생하는 애니메이션입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "색상 테마 알파 요소"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "폰트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "메뉴에 사용할 기본 폰트를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "폰트 색 (빨강)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "폰트 색 (초록)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "폰트 색 (파랑)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "레이아웃"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "XMB 인터페이스의 레이아웃을 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "아이콘 테마"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "다른 아이콘 테마 선택. 변경 내용은 프로그램을 다시 시작 후 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "스위치 아이콘"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "메뉴 설정 항목에서 '켬/끔' 글자 표기 대신에 아이콘을 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "그림자 효과"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "아이콘, 섬네일, 글자 아래에 그림자를 그립니다. 성능에 약간의 영향을 줄 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "셰이더 파이프라인"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "애니메이션 배경 효과를 선택합니다. 선택한 효과에 따라 GPU에 부하를 줄 수 있습니다. 성능에 영향을 주는 경우 기능을 끄거나 간단한 효과로 되돌리십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "색상 테마"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "다른 배경색 테마를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "미리보기 수직 배치"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "왼쪽 미리보기를 오른쪽 미리보기의 아래에 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "미리보기 배율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "최대 폭을 제한하여 표시되는 미리보기의 크기를 줄입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "수직 페이드 양"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "제목 헤더 표시"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "제목 마진"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "제목 마진 가로 오프셋"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "설정 탭 사용 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "프로그램 설정이 담긴 설정 탭을 표시합니다."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "리본"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "리본 (단순화)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "눈 (단순화)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "눈"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
   "보케"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "눈꽃"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "사용자 지정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
   "플랫UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "흑백"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "흑백 색 반전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "체계적"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
   "미래"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
   "픽셀"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
   "레트로풍"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
   "레트로시스템"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
   "점묘법"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "자동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "자동 반전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DAITE,
   "다이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "애플 녹색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "라이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "모닝 블루"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "썬빔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "어두운 자주"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "일렉트릭 파랑"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "금색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "레거시 빨강"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "암청색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "배경 이미지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "심해"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "볼캐닉 빨강"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "라임 그린"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "피카츄 옐로우"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "큐브 퍼플"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "패밀리 레드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "플레이밍 핫"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "아이스 콜드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDGAR,
   "미드가르"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "사이드바 접기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "왼쪽 사이드바를 항상 작게 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "실행목록 이름 단축 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "실행목록에서 제조사 이름을 삭제합니다. 예를 들어 'Sony - PlayStation'은 'PlayStation'이 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "이름 축소 후 실행목록 정렬 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "이름에서 제조사 항목을 삭제한 후 실행목록을 알파벳 순으로 다시 정렬합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "색상 테마"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "다른 색깔 테마를 설정해주세요."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "기본 하얀색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "기본 검정색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_NORD,
   "노드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "그루브박스 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY,
   "보이즌베리"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "커널 해킹"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "환상특급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA,
   "드라큘라"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "솔라라이즈 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "솔라라이즈 라이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "어두운 회색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "밝은 회색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN,
   "자줏빛 비"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "두 번째 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "콘텐츠 메타데이터 패널 대신 다른 미리보기를 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "콘텐츠 메타데이터 한 줄로 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "사용할 경우, 오른쪽 사이드바에 표시되는 콘텐츠 메타데이터(코어, 실행 시간 등)를 항목당 한 줄씩으로 표시합니다. 사이드바 너비를 초과하는 길이의 문구는 스크롤됩니다. 사용하지 않을 경우, 컨텐츠 메타데이터는 필요한 만큼 줄 수를 늘려가며 정적으로 표시됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "미리보기 배율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "섬네일 바의 크기 배율을 설정합니다."
   )

/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "아이콘"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "메뉴 항목의 왼쪽에 아이콘들을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "스위치 아이콘"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "메뉴 설정 항목에서 '켬/끔' 글자 표기 대신에 아이콘을 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "실행목록 아이콘 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "실행목록에 시스템 아이콘을 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "가로 레이아웃 최적화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "화면을 가로로 사용할 경우 메뉴 레이아웃을 보기 좋게 자동으로 조정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "내비게이션 바 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "화면에 메뉴 탐색 바로 가기를 표시합니다. 메뉴 범주 간의 빠른 전환을 활성화합니다. 터치스크린 장치에 권장됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "내비게이션 바 자동 회전"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "화면을 가로로 사용할 경우 내비게이션 바를 오른쪽에 표시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "색상 테마"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "다른 배경색 테마를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "전환 애니메이션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "메뉴 사이를 전환할 때 부드러운 전환 효과를 위한 애니메이션을 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "세로 화면 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "세로 화면에서 미리보기를 표시할 방식을 지정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "가로 화면 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "가로 화면에서 미리보기를 표시할 방식을 지정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "목록에 두 번째 미리보기 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "미리보기를 '목록' 형식으로 표시할 때 두 번째 미리보기를 표시합니다. 두 번째 미리보기를 표시할 공간이 부족할 경우 표시되지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "미리보기에 배경 추가"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "미리보기 테두리에 단색의 테두리 배경을 추가합니다. 미리보기를 항상 일정한 크기로 표시하여 미리보기의 크기 또는 비율이 일정하지 않더라도 일관된 모습을 유지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "첫 번째 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "실행목록의 각 항목을 나타낼 주 미리보기의 종류입니다. 콘텐츠의 아이콘으로 사용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "두 번째 미리보기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "실행목록의 각 항목을 나타낼 보조 미리보기의 종류입니다. 실행목록의 미리보기 형식에 따라 용도가 결정됩니다."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "파란색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "청회색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "짙은 파란색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "녹색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "실드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "빨간색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "노란색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI,
   "머티리얼 UI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "머티리얼 UI 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "오존 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD,
   "노드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "그루브박스 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "솔라라이즈 다크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "큐티 블루"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "큐티 사이언"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "큐티 그린"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "큐티 오렌지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "큐티 핑크"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "큐티 퍼플"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "큐티 레드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY,
   "버추얼 보이"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "커널 해킹"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "어두운 회색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "밝은 회색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "자동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "페이드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "슬라이드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "끔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "끔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "목록 (작게)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "목록 (보통)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "듀얼 아이콘"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "끔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "목록 (작게)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "목록 (보통)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "목록 (크게)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "데스크탑"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "끔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "켬"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "미리보기 화면 제외"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "정보"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "파일(&F)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "코어 불러오기(&L)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "코어 닫기(&U)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "나가기(&X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "편집(&E)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "검색(&S)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "보기(&V)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "닫은 고정 패널"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "셰이더 매개변수"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "옵션(&O)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "고정 패널 위치 기억:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "윈도우 위치 기억:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "마지막 콘텐츠 브라우저 탭 기억:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "테마:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<시스템 기본>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "어둡게"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "사용자..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "옵션"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "도구(&T)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "도움말(&H)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "RetroArch 정보"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "문서"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "사용자 코어 불러오기..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "코어 불러오기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "코어 로드 중..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "제목"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "버전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "실행목록"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "파일 탐색기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "상단"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "위로"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "컨텐츠 브라우저"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "박스아트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "스크린"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "타이틀"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "전체 실행목록"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "코어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "코어 정보"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<확인>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "정보"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "경고"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "오류"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "네트워크 오류"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "변경사항을 적용하려면 프로그램을 다시 시작하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "로그"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 항목"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "이미지를 여기 넣으세요"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "다시 보이지 않음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "중지"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "연결된 코어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "숨겨진 실행목록"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "숨김"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "강조 색상:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "선택(&C)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "색상 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "테마 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "사용자 테마"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "파일 경로가 비어있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "파일이 비어있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "파일을 불러올 수 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "파일을 기록할 수 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "파일이 존재하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "불러온 코어 우선 추천:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "확대"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "보기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "아이콘"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "목록"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "초기화"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "진행도:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "\"전체 실행목록\" 최대 리스트 항목:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "\"전체 실행목록\" 최대 그리드 항목:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "숨김 파일 및 폴더 표시:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "새 실행목록"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "새 실행목록 이름을 입력하십시오:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "실행목록 삭제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "실행목록 이름 변경"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "정말로 \"%1\" 실행목록을 삭제하시겠습니까?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "질문"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "파일을 삭제할 수 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "파일 명을 바꿀 수 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "파일 목록 수집..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "파일을 실행목록에 추가..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "실행목록 항목"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "이름:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "경로:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "코어:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "데이터베이스:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "확장자:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(스페이스로 구분; 기본은 모두 포함)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "압축파일 내부 필터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(미리보기 검색에 사용)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "정말로 \"%1\" 항목을 삭제하시겠습니까?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "하나의 실행목록을 먼저 선택해 주세요."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "삭제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "항목 추가..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "파일 추가..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "폴더 추가..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "편집"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "파일 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "폴더 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<다중>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "실행목록 항목 업데이트 오류."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "필요한 항목을 채워주십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "RetroArch 업데이트 (nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch가 성공적으로 업데이트 되었습니다. 변경을 적용하려면 프로그램을 다시 시작하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "업데이트 실패."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "기여자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "현재 셰이더"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "아래로 이동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "위로 이동"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "불러오기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "저장"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "삭제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "패스 삭제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "적용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "패스 추가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "모든 패스 비우기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "셰이더 패스 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "패스 초기화"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "모든 패스 초기화"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "매개변수 초기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "미리보기 다운로드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "다운로드가 이미 실행중입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "실행목록에서 시작:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "미리보기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "미리보기 캐시 제한:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "드래그-앤-드롭 미리보기 크기 제한:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "전체 미리보기 다운로드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "전체 시스템"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "현재 실행목록"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "미리보기가 성공적으로 다운로드 되었습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "성공: %1 실패: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "코어 옵션"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "초기화"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "모두 초기화"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "코어 업데이터 설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "계정 도전과제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "계정 목록 종점"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "터보/데드존"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "코어 카운터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "선택된 디스크 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "프론트엔드 카운터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "수평 메뉴"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "설정 안된 코어 입력 기술자 숨김"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "입력 기술자 라벨 표시"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "온스크린 오버레이"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "최근 실행"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "최근 실행 목록에서 콘텐츠를 선택합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "콘텐츠를 불러오면 콘텐츠 및 libretro 코어 조합이 기록됩니다.\n실행 기록은 RetroArch 구성 파일과 동일한 디렉터리에 있는 파일에 저장됩니다. 시작 시 구성 파일을 불러오지 못하면 기록을 저장하거나 불러오지 못하게되며 메인 메뉴에도 표시되지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "멀티미디어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "서브시스템"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "현재 콘텐츠의 서브시스템 설정에 접근합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "현재 콘텐츠: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "넷플레이 호스트 찾지 못함."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "넷플레이 클라이언트 찾지 못함."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "성능 카운터 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "실행목록 없음."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "연결됨"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "온라인"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "포트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "%d번 포트 디바이스명: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "장치 표시 이름: %s\n장치 구성 이름: %s\n장치 VID/PID: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "치트 설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "치트 검색 시작 또는 재개"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "미디어 플레이어에서 재생"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "초"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "코어 실행"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "콘텐츠 없이 코어를 시작합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "추천 코어"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "압축 파일을 읽을 수 없습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "사용자"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "키보드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "내장 이미지 뷰어 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "최대 스왑체인 이미지"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "비디오 드라이버가 지정된 버퍼링 모드를 사용하도록 지시합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "스왑체인 이미지의 최대 크기입니다. 비디오 드라이버에 특정 비디오 버퍼링 모드를 사용하도록 지시할 수 있습니다.\n단일 버퍼링 - 1\n이중 버퍼링 - 2\n삼중 버퍼링 - 3\n올바른 버퍼링 모드를 설정하면 대기 시간에 큰 영향을 줄 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "대기 가능 스왑체인"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "CPU와 GPU를 강제 동기화합니다. 성능을 희생해 지연시간을 줄입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "최대 프레임 지연"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "비디오 드라이버가 지정된 버퍼링 모드를 사용하도록 지시합니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "현재 메뉴상에서 사용되는 셰이더를 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "셰이더 프리셋"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "셰이더 프리셋"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "셰이더 프리셋"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "URL 탐색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "URL 경로"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "시작"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "사용자: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "호환되는 콘텐츠 찾는 중..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "코어를 찾을 수 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "실행목록을 찾을 수 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "호환되는 콘텐츠 발견"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "CRC 또는 파일명이 일치하는 콘텐츠를 찾을 수 없음"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Gong 시작"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "자동 화면비"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "사용자 (랜): %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "상태"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "시스템 BGM 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
   "사용자 비율"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "녹화 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "녹화 출력을 다음으로 저장..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "녹화를 출력 폴더에 저장"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "일치 확인 #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "확인할 일치 항목을 선택하십시오."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "강제 화면비율"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "실행목록에서 선택"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME,
   "이어하기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME,
   "빠른 메뉴를 종료하고 현재 실행 중인 콘텐츠를 이어합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "%u 일치 목록 확인"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "현재 일치 항목에서 코드 생성"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "현재 일치 항목 삭제"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "푸터 투명도"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "푸터 그래픽의 투명도를 변경합니다."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "헤더 투명도"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "헤더 그래픽의 투명도를 변경합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "넷플레이 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "콘텐츠 시작"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "콘텐츠 실행 기록 경로"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "출력 디스플레이 ID"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "CRT 디스플레이와 연결된 포트를 선택해주세요."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "도움말"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "초기화"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "오디오/비디오 문제 해결"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "가상 컨트롤러 오버레이 변경"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "콘텐츠 불러오기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "콘텐츠 검색"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "코어란 무엇인가?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "디버그 정보 전송"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
   "분석을 위해 장치 및 RetroArch 구성에 대한 진단 정보를 서버로 보냅니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "데이터베이스 설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "넷플레이 지연 프레임"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "로컬 네트워크 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "로컬 네트워크 상의 넷플레이 호스트를 검색합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "넷플레이 클라이언트 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "넷플레이 관전 허용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "설명"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "최대 실행 속도 제한"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "새로운 치트 코드 검색 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "새로운 치트를 찾기 시작합니다. 비트 수가 변경될 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "계속 검색"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "계속해서 새로운 치트를 검색합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "도전과제 목록 (하드코어)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "치트 세부정보"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "치트의 세부 설정을 관리합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "치트 검색 시작 또는 재개"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "치트 코드 검색을 시작하거나 재개합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "치트 패스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "치트 갯수를 추가 또는 감소시킵니다."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "좌 아날로그 X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "좌 아날로그 Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "우 아날로그 X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "우 아날로그 Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "치트 검색 시작 또는 재개"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "데이터베이스 커서 목록"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "데이터베이스 – 필터: 개발사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "데이터베이스 – 필터: 배급사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "데이터베이스 - 필터: 국가"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "데이터베이스 - 필터: 프랜차이즈"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "데이터베이스 - 필터: ESRB 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "데이터베이스 - 필터: ELSPA 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "데이터베이스 - 필터: PEGI 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "데이터베이스 - 필터: CERO 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "데이터베이스 - 필터: BBFC 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "데이터베이스 - 필터: 최대 플레이어 수"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "데이터베이스 - 필터: 출시 월별"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "데이터베이스 - 필터: 출시 연도별"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "데이터베이스 - 필터: Edge 매거진 기사"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "데이터베이스 - 필터: Edge 매거진 등급"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "데이터베이스 정보"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "구성하기"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "다운로드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "넷플레이 설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Slang 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "OpenGL/Direct3D 텍스처 렌더링(멀티패스 셰이더) 지원"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "콘텐츠"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "일반적으로 libretro/RetroArch 앱과 애셋을 묶은 개발자가 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "묻기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "기본 메뉴 조작"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "확인"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "정보"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "나가기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "위로 스크롤"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "기본"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "키보드 전환"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "메뉴 전환"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "메뉴 선택 중"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "게임 중"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "게임 중 (일시정지됨)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "실행 중"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "일시정지"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "콘텐츠를 불러오면 넷플레이가 시작됩니다."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "넷플레이를 시작하기 전에 콘텐츠를 불러와야 합니다."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "적합한 코어 또는 콘텐츠 파일을 찾지 못했습니다. 수동으로 불러오십시오."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "그래픽 드라이버가 현재의 RetroArch 비디오 드라이버와 호환되지 않아 %s 드라이버로 되돌립니다. 변경사항 적용하려면 RetroArch를 다시 시작하십시오."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "코어 설치 성공"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "코어 설치 실패"
   )
MSG_HASH(
   MSG_AI_VIDEO_DRIVER_NOT_SUPPORTED,
   "이 비디오 드라이버에서는 AI 서비스를 사용할 수 없습니다."
   )
MSG_HASH(
   MSG_AI_AUTO_MODE_ENABLED,
   "자동 번역이 활성화되었습니다."
   )
MSG_HASH(
   MSG_AI_AUTO_MODE_DISABLED,
   "자동 번역이 비활성화되었습니다."
   )
MSG_HASH(
   MSG_AI_NOTHING_TO_TRANSLATE,
   "번역할 것이 없습니다."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "모든 치트를 삭제하려면 오른쪽을 다섯 번 입력하십시오."
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "디버그 정보 저장 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_DEBUG_INFO,
   "디버그 정보를 서버에 전송하는데 실패했습니다."
   )
MSG_HASH(
   MSG_SENDING_DEBUG_INFO,
   "디버그 정보 전송 중..."
   )
MSG_HASH(
   MSG_SENT_DEBUG_INFO,
   "디버그 정보를 성공적으로 서버에 보냈습니다. ID 번호는 %u 입니다."
   )
MSG_HASH(
   MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
   "분석 정보를 RetroArch 팀에게 보내려면 두 번 더 눌러주십시오."
   )
MSG_HASH(
   MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
   "분석 정보를 RetroArch 팀에게 보내려면 한 번 더 눌러주십시오."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "전역 오디오 믹서 볼륨"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "넷플레이 검색 완료."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "죄송합니다. 현재 콘텐츠를 요구하지 않는 코어는 넷플레이에 참여할 수 없습니다."
   )
MSG_HASH(
   MSG_NATIVE,
   "기본"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "알 수 없는 넷플레이 명령어를 받았습니다"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "파일이 이미 존재합니다. 백업 버퍼에 저장합니다"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "다음에서 연결: \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "다음에서 연결: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "넷플레이 포트 매핑 성공"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "외부 네트워크가 개인 또는 공유 주소를 사용하고 있습니다. 중계 서버를 사용하십시오."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "넷플레이 UPnP 포트 매핑 실패"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "제공되는 인수 및 메뉴, 도움말이 없습니다..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "디스크 트레이 설정"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "클라이언트를 기다리고 있습니다..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "인터넷에서 이 방에 연결할 수 없습니다."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "게임을 나왔습니다"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "%u 플레이어로 입장했습니다"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s 입력장치로 접속했습니다"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "%.*s 플레이어가 게임을 나갔습니다"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s 사용자가 %u 플레이어로 접속했습니다"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s 사용자가 %.*s 입력장치로 접속했습니다"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "사용자가 이전 버전의 RetroArch가 아니거나 구버전을 사용하여 접속에 실패했습니다."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "넷플레이 사용자가 이전 버전의 RetroArch를 사용중입니다. 접속할 수 없습니다."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "경고: 넷플레이 사용자가 다른 버전의 RetroArch를 사용중입니다. 같은 버전을 사용하십시오."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "넷플레이 사용자가 다른 코어를 사용중입니다. 접속할 수 없습니다."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "경고: 넷플레이 사용자가 다른 버전의 코어를 사용중입니다. 동일한 버전의 코어를 사용하십시오."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "현재 코어는 현재 기종의 넷플레이를 지원하지 않습니다"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "현재 코어는 다른 기종간의 넷플레이를 지원하지 않습니다"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "넷플레이 서버 비밀번호 입력:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "넷플레이 채팅 메시지 입력:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "다음 사용자의 접속을 허용하시겠습니까:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "잘못된 비밀번호"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" 연결 끊김"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "넷플레이 클라이언트가 연결해제 되었습니다"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "넷플레이 연결해제"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "플레이 권한이 없습니다"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "여유 플레이어 슬롯이 없습니다"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "입력장치 요청을 사용할 수 없습니다"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "플레이 모드로 전환하지 못했습니다"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "넷플레이 사용자 \"%s\" 일시정지됨"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "닉네임이 \"%s\"(으)로 변경됨"
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "클라이언트 강제 퇴장됨: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "클라이언트 강제 퇴장 실패함: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "클라이언트 차단됨: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "클라이언트 차단 실패함: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "실행 중"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "관전 중"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "장치"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "채팅 지원됨"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "연결 속도 저하 발생"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "오디오 볼륨"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "자동감지"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "상태저장 자동 불러오기: "
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "기능"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "넷플레이 호스트에 접속 중"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "포트에 접속 중"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "연결 슬롯"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "코어 목록을 읽어오는 중..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "코어 목록을 읽어올 수 없습니다!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "이미 최신 버전이 설치됨: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "코어 업데이트 중: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "코어 다운로드 중: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "코어 압축해제 중: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "코어 설치됨: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "코어 설치 실패: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "코어 탐색 중..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "코어 확인 중: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "모든 코어가 최신 버전입니다"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "가능한 모든 코어를 Play 스토어 버전으로 교체했습니다"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "업데이트된 코어: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "건너뛴 코어: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "코어 업데이트 사용중지 - 코어 잠김: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "코어 초기화 중: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "코어 초기화: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "실행목록 정리 중: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "실행목록 정리됨: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "새로 고침 실패 - 재생 목록에 유효한 스캔 기록이 없습니다. "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "새로 고침 실패 - 콘텐츠 디렉토리가 올바르지 않음: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "새로 고침 실패 - 올바르지 않은 시스템 이름: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "새로 고침 실패 - 올바르지 않은 코어: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "새로 고침 실패 - 올바르지 않은 아케이드 DAT 파일: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "새로 고침 실패 - 아케이드 DAT 파일이 너무 큼 (메모리 부족): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "즐겨찾기에 추가됨"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "즐겨찾기에 추가하지 못했습니다: 실행목록이 가득 참"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "코어 설정: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "실행목록 상의 코어 연결이 초기화 되었습니다."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "추가 디스크"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "디스크 추가 실패"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "프로그램 폴더"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "치트 변경 적용."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "패치 적용 중: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "셰이더 적용"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "음소거 됨."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "음소거 해제."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "컨트롤러 프로필 저장 중 오류가 발생했습니다."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "컨트롤러 프로필이 성공적으로 저장되었습니다."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "자동저장을 초기화 하지 못했습니다."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "자동 상태 저장 to"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "포트에 명령줄 인터페이스 띄우기"
   )
MSG_HASH(
   MSG_BYTES,
   "바이트"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "새 구성 경로를 유추할 수 없습니다. 현재 시간을 사용합니다."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "하드코어 모드 사용 중에는 상태저장 및 뒤로 감기 기능을 이용할 수 없습니다."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "알려진 매직 넘버와 비교 중..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "컴파일 대상 API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "구성 디렉터리가 설정되지 않았습니다. 새 구성을 저장할 수 없습니다."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "연결됨:"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "콘텐츠의 CRC32가 다릅니다. 다른 게임은 사용할 수 없습니다."
   )
MSG_HASH(
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "호스트가 다른 게임을 실행 중입니다."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "핑이 너무 높아 이 호스트에 연결할 수 없습니다."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "콘텐츠 불러오기를 건너뛰었습니다. 자체적으로 실행하게 됩니다."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "코어가 상태저장을 지원하지 않습니다."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "코어 옵션 파일이 성공적으로 생성되었습니다."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "코어 옵션 파일이 성공적으로 삭제되었습니다."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "모든 코어 옵션이 기본값으로 초기화되었습니다."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "코어 옵션 저장됨:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "코어 옵션을 저장할 수 없음:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "다음 드라이버를 찾을 수 없음"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "호환되는 시스템을 찾지 못했습니다."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "올바른 데이터 트랙을 찾지 못함"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "데이터 트랙을 열지 못함"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "콘텐츠 파일을 읽지 못함"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "동영상 헤더를 읽지 못했습니다."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "동영상에서 상태를 불러오지 못했습니다."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "리플레이 파일 헤더상의 컨텐츠 파일간 CRC32 체크섬이 일치하지 않습니다. 실행 시 싱크가 어긋날 수 있습니다."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "사용자 타이밍"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "압축해제가 이미 진행중입니다."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "압축해제 실패."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "감지된 뷰포트:"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "올바른 컨텐츠 패치를 찾지 못했습니다."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "장치 연결해제."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "가상 디스크 트레이를 닫았습니다."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "가상 디스크 트레이를 꺼냈습니다."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "다운로드 중"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "인덱스"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "다운로드 실패"
   )
MSG_HASH(
   MSG_ERROR,
   "오류"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Libretro 코어에 실행할 컨텐츠가 필요합니다."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Libretro 코어에 특별한 콘텐츠가 필요하지만 제공되지 않았습니다."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "코어가 VFS를 지원하지 않으며 로컬 사본에서 불러오지 못했습니다"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "인수 분석 오류."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "코어 옵션 파일을 저장할 수 없습니다."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "코어 옵션 파일을 삭제할 수 없습니다."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "코어 입력설정 파일 저장 오류."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "입력설정 파일 삭제 오류."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "셰이더 프리셋 저장 오류."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "외부 앱 경로"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "압축해제 중"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "파일 압축해제"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "구성 저장중 오류 위치:"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "관전자 수신 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "패치된 콘텐츠의 메모리 할당 실패..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "셰이더 적용 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "셰이더 프리셋을 적용하지 못했습니다:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "소켓 연결 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "디렉토리 생성 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "압축파일에서 콘텐츠 추출 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "클라이언트에서 별칭 얻어오기 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "불러오기 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "콘텐츠 불러오기 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "동영상 파일 불러오기 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "오버레이 불러오기 실패."
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "키보드 오버레이가 설정되지 않았습니다."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "상태 불러오기 실패:"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "libretro 코어 불러오기 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "패치 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "클라이언트에서 헤더 수신 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "이름 수신 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "호스트에서 이름 수신 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "호스트에서 이름 크기 수신 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "호스트에서 SRAM 데이터 수신 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "트레이에서 디스크 제거 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "임시파일 삭제 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "SRAM 세이브 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "SRAM을 불러올 수 없음"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "상태저장 실패:"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "이름 전송 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "이름 크기 전송 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "클라이언트에 이름 전달 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "호스트에 이름 전달 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "클라이언트에 SRAM 데이터 전송 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "오디오 드라이버 시작 실패. 오디오 없이 실행됩니다."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "동영상 녹화 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "녹화 시작 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "스크린샷 저장 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "상태저장 불러오기 취소 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "상태저장 되돌리기 실패."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "음소거 실패."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "수신 중 치명적 오류:"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "파일 없음"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "자동 상태저장 발견:"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "디스크 라벨 발견"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "파일상 첫 데이터 트랙 발견"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "최근 상태저장 슬롯 발견"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "최근 리플레이 슬롯 발견"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "현재 녹화에서 안됨"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "리플레이와 호환되지 않음"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "셰이더 발견"
   )
MSG_HASH(
   MSG_FRAMES,
   "프레임"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "게임 우선 설정 발견:"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "콘텐츠 폴더 우선 설정 발견:"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "잘못된 디스크 인덱스 수신."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "마우스 상태"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "게임 포커스 사용"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "게임 포커스 사용 안함"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "이 libretro 코어는 하드웨어 가속을 사용합니다. 셰이드 후처리된 녹화를 사용하십시오."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "체크섬이 CRC32와 일치하지 않습니다."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "치트 입력"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "치트 파일명"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "프리셋 파일명"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "입력 재정의 파일 이름"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "입력 리매핑 파일 이름"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "제목 수정"
   )
MSG_HASH(
   MSG_INTERFACE,
   "인터페이스"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "내부 저장공간"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "이동식 저장소"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "잘못된 이름 길이."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "(바이트)"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "(메가바이트)"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "(기가바이트)"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "현재 libretro 용이 아닌 다른 버전의 libretro로 컴파일 되었습니다."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "libretro 프론트엔드"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "#%d 슬롯에서 상태 불러옴."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "슬롯 #-1(자동)에서 상태 불러옴."
   )
MSG_HASH(
   MSG_LOADING,
   "불러오는 중"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "하나 이상의 펌웨어 파일이 부족"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "콘텐츠 파일 불러오는 중"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "실행 기록 파일 불러오는 중"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "즐겨찾기 파일 불러오는 중"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "상태저장 불러오는 중"
   )
MSG_HASH(
   MSG_MEMORY,
   "메모리"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "입력 리플레이 영상 파일이 올바른 리플레이 파일이 아닙니다."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "입력 리플레이 영상 포맷이 다른 버전으로 보입니다. 실패할 가능성이 큽니다."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "입력 리플레이 영상이 종료되었습니다."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "영상 기록 중지 중."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "넷플레이 초기화 실패."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "코어가 넷플레이를 지원하지 않습니다."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "콘텐츠 없음, 더미 코어 시작."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "아직 덮어쓴 상태저장 파일이 없습니다."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "아직 불러온 상태저장 파일이 없습니다."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "우선 설정 저장 오류."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "우선 설정 삭제 오류."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "우선 설정 파일 저장 성공."
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "우선 설정 파일이 성공적으로 삭제되었습니다."
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "우선 설정을 성공적으로 해제했습니다."
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "저장할 사항이 없습니다. 우선설정은 저장되지 않았습니다."
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "저장하지 않습니다. 우선 설정 활성화."
   )
MSG_HASH(
   MSG_PAUSED,
   "일시정지됨."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "첫 데이터 트랙 읽는 중..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "리사이즈 때문에 기록 중지됨."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "다음에 녹화"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "치트 파일 리다이렉션:"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "세이브 파일 리다이렉션:"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "상태저장 파일 리다이렉션:"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "입력설정 파일 저장 성공."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "입력설정 파일 삭제 성공."
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "모든 입력설정 옵션이 기본값으로 초기화되었습니다."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "트레이에서 디스크 제거됨."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "임시 콘텐츠 파일 제거 중"
   )
MSG_HASH(
   MSG_RESET,
   "초기화"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "드라이버 초기화로 레코딩 다시 시작."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "이전 상태저장 복원."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "셰이더: 기본 셰이더 프리셋 복구: "
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "세이브 파일 디렉토리 되돌리기:"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "상태저장 파일 디렉토리 되돌리기:"
   )
MSG_HASH(
   MSG_REWINDING,
   "뒤로 감기"
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "뒤로 감을 수 없습니다. 코어가 직렬 상태저장을 지원하지 않습니다."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "뒤로 감기 버퍼크기 초기화:"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "뒤로 감기 버퍼 초기화 실패. 뒤로 감기가 비활성 됩니다."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "스레드된 오디오 사용 중. 뒤로 감기 사용 불가."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "뒤로 감기 버퍼 끝에 도달했습니다."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "새 구성 저장 위치:"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "슬롯 #%d에 상태저장."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "슬롯 #-1(자동)에 상태저장."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "저장 성공:"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "RAM 저장 중 - 형식: "
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "상태저장 중"
   )
MSG_HASH(
   MSG_SCANNING,
   "검색 중"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "디렉토리 검색 완료"
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "명령 전달중"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "여러 패치가 정의되었습니다. 모두 무시합니다..."
   )
MSG_HASH(
   MSG_SHADER,
   "셰이더"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "셰이더 프리셋 저장 성공."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "슬로모션."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "빨리 감기."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "슬로모션 뒤로 감기."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "SRAM 불러오기 건너뛰는 중."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM이 저장되지 않을 것입니다."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "SRAM 덮어쓰기 차단"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "동영상 재생."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "동영상 녹화 시작:"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "상태저장 크기"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "상태 슬롯"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "리플레이 슬롯"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "스크린샷 저장 중."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "스크린샷 저장 완료."
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "도전과제 해제"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "리더보드 시도 시작됨"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "리더보드 시도 실패함"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "%s 제출함 (%s)" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "미리보기 형식 변경"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "전체화면 미리보기"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "메타데이터 전환"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "이용 가능한 미리보기가 없습니다"
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "종료하려면 한 번 더 누르세요..."
   )
MSG_HASH(
   MSG_TO,
   "-"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "상태저장 불러오기 취소."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "상태저장 되돌리는 중"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "알 수 없음"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "일시정지 해제."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "알 수 없는 커맨드 \"%s\"을(를) 받았습니다.\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "새 구성에 코어 이름을 사용하는 중입니다."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "libretro 더미코어 사용. 녹화 생략."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "이용가능한 포트로 장치 접속."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "포트에서 장치연결 해제"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "다시 시작 중..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "종료 중..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "libretro API 버전"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "뷰포트 크기 계산 실패! 원시 데이터를 계속 사용합니다. 정상 적인 동작을 보장할 수 없습니다..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "가상 디스크 트레이를 꺼내지 못했습니다."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "가상 디스크 트레이를 닫지 못했습니다."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "\"%s\"에서 상태저장을 자동으로 불러오지 못했습니다."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "\"%s\"에서 상태저장을 자동으로 불러왔습니다."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "포트에 구성됨"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s가 포트 %u에 구성됨"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "장치 연결 해제됨, 포트"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s 연결 해제됨, 포트 %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "구성 안됨"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) 구성 안됨"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "구성 안됨, 기본값 사용"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) 구성 안됨, 기본값 사용"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "블루투스 검색 완료."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "페어링이 제거되었습니다. 다시 연결하려면 RetroArch를 재시작해주세요."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Wi-Fi 검색 완료."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Bluetooth 장치를 검색 중..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "무선 네트워크 검색 중..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Wi-Fi를 켜는 중..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Wi-Fi를 끄는 중..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Wi-Fi 연결 해제 중..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "넷플레이 호스트 검색 중..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "콘텐츠 스캔 준비중..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "비밀번호 입력"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "비밀번호가 확인되었습니다."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "비밀번호가 틀렸습니다."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "비밀번호 입력"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "비밀번호가 확인되었습니다."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "비밀번호가 틀렸습니다."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "구성 재정의를 불러왔습니다."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "게임 입력설정 파일을 불러왔습니다."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "콘텐츠 디렉토리 입력설정 파일을 불러왔습니다."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "코어 입력설정 파일을 불러왔습니다."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "입력설정이 저장됨:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "입력설정 저장에 실패함:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "미리 실행이 활성화 되었습니다. 지연 프레임 제거: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "두 번째 인스턴스와 함께 미리 실행을 활성화합니다. 지연 프레임 제거: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "미리 실행 사용해제됨."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "코어가 상태 저장을 지원하지 않아 미리 실행 기능이 비활성화되었습니다."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "미리 실행을 사용할 수 없습니다. 코어가 결정식 상태저장을 지원하지 않습니다."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "상태저장에 실패했습니다. 미리 실행 기능이 비활성화됩니다."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "상태저장 불러오기에 실패했습니다. 미리 실행 기능이 비활성화됩니다."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "두 번째 인스턴스를 생성하지 못했습니다. 미리 실행에 하나의 인스턴스만 사용됩니다."
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "선제 프레임이 활성화 되었습니다. 지연 프레임 제거: %u."
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "선제 프레임 기능이 사용해제되었습니다."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "코어가 상태 저장을 지원하지 않아 선제 프레임 기능이 비활성화되었습니다."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "선제 프레임을 사용할 수 없습니다. 코어가 결정식 상태저장을 지원하지 않습니다."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "선제 프레임 기능을 위한 메모리 할당에 실패했습니다."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "상태저장에 실패했습니다. 선제 프레임 기능이 비활성화됩니다."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "상태저장 불러오기에 실패했습니다. 선제 프레임 기능이 비활성화됩니다."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "파일 스캔 완료"
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "성공적으로 치트 검색을 시작했습니다"
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "치트 검색 시작에 실패했습니다"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "검색이 초기화/시작되지 못했습니다"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "새 일치 카운트 = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "%u 일치가 추가됨"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "일치 항목 추가 실패"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "일치에서 코드 생성"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "코드 생성 실패"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "일치 삭제"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "공간이 부족합니다. 동시 사용 가능한 최대 치트 수는 100개 입니다."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "새 치트가 목록 상단에 추가되었습니다."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "새 치트가 목록 아래에 추가되었습니다."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "모든 치트가 삭제되었습니다."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "새 치트가 현재 이전에 추가되었습니다."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "새 치트가 현재 다음에 추가되었습니다."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "치트가 앞에 복사되었습니다."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "치트가 뒤에 복사되었습니다."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "치트 삭제됨."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "디스크 설정 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "최근 실행 디스크 설정 실패..."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "클라이언트에 연결할 수 없음"
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "호스트에 연결할 수 없음"
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "넷플레이 호스트가 가득 참"
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "이 호스트에서 차단되었습니다"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "호스트에서 헤더 수신 실패"
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "상태저장을 불러오려면 도전과제 하드코어 모드를 일시정지 또는 비활성화해야 합니다."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "상태저장을 불러왔습니다. 현재 세션의 하드코어 모드 도전과제는 해제됩니다."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "치트가 활성화 되었습니다. 현재 세션의 하드코어 모드 도전과제는 해제됩니다."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "%s 완전 정복"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "%s 달성"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "매우 낮음"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "낮음"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "보통"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "높음"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "매우 높음"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "경고: 애셋이 없습니다. 온라인 업데이터를 사용하십시오"
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "경고: 선택된 언어의 폰트가 없습니다. 온라인 업데이터를 사용하십시오"
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "경고: 지원되지 않는 언어입니다 - 영어로 전환합니다"
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "디스크 덤프 중..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "드라이브%d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "코어를 먼저 불러와주십시오."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "드라이브 읽기 실패. 덤프 취소됨."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "디스크 기록 실패. 덤프 취소됨."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "드라이브에 디스크가 없습니다."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "셰이더 프리셋이 성공적으로 삭제되었습니다."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "셰이더 프리셋 삭제 오류."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "아케이드 DAT 파일이 올바르지 않습니다"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "선택된 아케이드 DAT 파일이 너무 큽니다 (메모리 부족)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "아케이드 DAT 파일을 불러올 수 없습니다 (올바르지 않은 형식?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "잘못된 수동 스캔 구성"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "콘텐츠를 찾을 수 없습니다"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "콘텐츠 검색 중: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "확인중인 항목: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "검색 중: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "M3U 항목 정리 중: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "검색 완료: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "코어 검색 중: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "설치된 코어의 백업이 이미 존재: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "코어 백업: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "사용하지 않는 백업 제거 중: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "코어 백업 완료: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "선택된 코어 백업이 이미 설치되어있음: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "코어 복원 중: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "코어 복원 완료: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "선택된 코어 파일이 이미 설치되어있음: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "코어 설치 중: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "코어 설치 완료: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "잘못된 코어 파일 선택: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "코어 백업 실패: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "코어 복원 실패: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "코어 설치 실패: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "코어 복원 사용중지 - 코어 잠김: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "코어 설치 사용중지 - 코어 잠김: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "코어 잠금 실패: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "코어 잠금해제 실패: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "'단독 실행 코어' 목록에서 코어를 삭제할 수 없음: "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "'단독 실행 코어' 목록에 코어를 추가할 수 없음: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "코어 삭제 사용중지 - 코어 잠김: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "지원되지 않는 비디오 모드"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "코어 정보 디렉토리에 쓸 수 없음 - 코어 정보 캐시가 비활성화됩니다"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "시작 상태저장 발견:"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "시작 상태저장 불러오는 중:"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "GameMode 활성화 실패"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "GameMode 활성화 실패 - GameMode 데몬이 설치되어 있고 실행중인지 확인하십시오"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "정확한 프레임 동기화가 활성화되었습니다."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "정확한 프레임 동기화가 비활성화되었습니다."
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "비디오 주사율이 %s Hz로 변경되었습니다."
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Lakka 업데이트"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "프론트엔드 이름"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Lakka 버전"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "재시작"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Joy-Con 분할하기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "그래픽 위젯 크기조절 우선설정"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "위젯 표시 크기조절 값을 수동으로 변경 적용합니다. '그래픽 위젯 자동 크기 설정'이 해제 되었을 시에만 적용됩니다. 알람, 표시자, 컨트롤 등을 메뉴와 상관없이 크기를 키우거나 줄일때 사용할 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "화면 해상도"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "화면 해상도: 기본"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "화면 해상도: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "화면 해상도: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "적용 중: 기본"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "적용 중: %dx%d\nSTART를 눌러 초기화"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "적용 중: %dx%d - %s\nSTART를 눌러 초기화"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "초기화 중: 기본"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "초기화 중: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "초기화 중: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "디스플레이 모드를 선택합니다 (재시작 필요)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "종료"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "외부 파일 접근 허용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Windows의 파일 접근 권한 설정을 엽니다"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "broadFileSystemAccess 권한을 활성화하기 위해 Windows 권한 설정을 엽니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "열기..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "시스템 파일 선택기로 다른 디렉토리를 엽니다"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "깜빡임 필터"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "비디오 감마"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "소프트 필터 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_SETTINGS,
   "블루투스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "블루투스 장비를 검색하고 연결합니다."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "무선 네트워크를 검색하고 연결합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Wi-Fi 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "네트워크에 연결"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "네트워크에 연결"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "연결 해제"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "디플리커"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "VI 화면 폭 설정"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "오버스캔 보정 (위)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "지정된 개수의 스캔라인만큼 이미지 크기를 줄여서 오버스캔 잘림을 조정합니다(화면 상단 기준)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "오버스캔 보정 (아래)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "지정된 개수의 스캔라인만큼 이미지 크기를 줄여서 오버스캔 잘림을 조정합니다(화면 하단 기준)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "지속적 성능 모드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "CPU 성능 및 전력"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "정책"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "가버너 모드"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "수동"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "가버너, 클럭 속도 등 CPU의 모든 부분을 직접 설정합니다. 전문가가 아니라면 권장하지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "성능 (관리)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "추천하는 기본 모드입니다. 게임 플레이 중에는 최고 성능, 일시 정지 중이거나 메뉴 탐색 중에는 전원 절약 모드로 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "사용자 정의 관리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "메뉴와 게임 플레이 중에 어떤 가버너를 사용할 것인지 직접 선택합니다. 게임 플레이 중에는 Performance, Ondemand, Schedutil을 사용할 것을 권장합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "최고 성능"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "최상의 경험을 위해 항상 최고 성능을 유지합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "전원 절약"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "가장 낮은 속도로 작동하여 전원을 절약합니다. 배터리를 사용하는 기기에서 유용하지만 성능이 크게 감소됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "균형"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "작업량에 따라 성능을 유동적으로 조정합니다. 대부분의 기기와 에뮬레이터에서 잘 작동하고 전원을 절약할 수 있습니다. 높은 성능을 필요로 하는 게임이나 코어에서는 성능 문제가 일어날 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "최저 속도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "최고 속도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "최저 코어 속도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "최고 코어 속도"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "CPU 가버너"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "코어 가버너"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "메뉴 가버너"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "게임 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "성능 향상, 지연시간 감소, 소리 찢어짐 문제 해결 등의 효과를 볼 수 있습니다. 사용하려면 https://github.com/FeralInteractive/gamemode 가 필요합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Linux에서 GameMode를 활성화하면 자동으로 CPU와 GPU를 최고 성능 모드로 변경하여 지연 시간이 줄어들고, 오디오 깨짐이 고쳐지고, 전체적인 성능이 최대화되는 등의 효과를 볼 수 있습니다.\n사용하려면 GameMode 소프트웨어가 설치되어 있어야 합니다. GameMode를 설치하는 방법은 https://github.com/FeralInteractive/gamemode 에서 확인하실 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_REST,
   "프레임 휴식"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_REST,
   "프레임 표시 후에 가능한 만큼 최대한 슬립하여 수직 동기 시 CPU 사용량을 줄입니다. 서드 파티 스캔라인 동기화를 위한 기능입니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "PAL60 모드 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "RetroArch 다시 시작"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "종료 후 RetroArch를 다시 시작합니다. 일부 메뉴 설정의 활성화를 위해 필요합니다. (예: 메뉴 드라이버 변경 등)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "블록 프레임"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "전면 터치 선호"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "후면 대신 전면 터치를 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "터치 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "키보드 컨트롤러 맵핑"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "키보드 컨트롤러 맵핑 타입"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "작은 키보드 사용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "입력 블럭 시간 만료"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "입력 샘플을 받기 위해 대기할 시간(밀리초) 입니다. 동시 입력에 문제가 있을 경우에만 사용하십시오(안드로이드 전용)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "'재시작' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "재시작 항목을 표시/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "'전원 끄기' 표시"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "전원 끄기 항목을 표시/해제합니다."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "비밀번호 필요"
   )
MSG_HASH(
   MSG_INTERNET,
   "인터넷"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "인터넷 (중계)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "인터넷 (연결 불가능)"
   )
MSG_HASH(
   MSG_LOCAL,
   "로컬"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "내부 저장소 상태: 읽기/쓰기"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "내부 저장소 상태: 읽기 전용"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "화면 밝기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "화면의 밝기를 조절합니다."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "CPU 오버클럭"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "스위치 CPU를 오버클럭 합니다."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
   "블루투스 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "블루투스를 사용/해제합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "서비스"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "OS 수준 서비스를 관리합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "SAMBA 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "SMB 프로토콜로 네트워크 폴더를 공유합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "SSH 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "SSH로 원격 명령줄에 접속할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Wi-Fi 액세스 포인트"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Wi-Fi 액세스 포인트를 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "시간대"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "시간대를 설정하여 날짜와 시간을 현재 위치에 맞게 조정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "사용 가능한 시간대 목록을 표시합니다. 시간대를 선택하면, 시간과 날짜가 선택한 시간대에 맞춰 조정됩니다. 시스템/하드웨어 시간이 UTC에 맞춰져 있다고 가정합니다."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Nintendo Switch 옵션"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Nintendo Switch 전용 옵션을 설정합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "CPU 오버클럭"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "CPU 오버클럭 주파수 활성화"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "CEC 지원"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "TV에 도킹할 때 CEC 핸드셰이크를 사용합니다"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "블루투스 ERTM 비활성화"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "블루투스 ERTM을 비활성화하면 일부 장치와 페어링할 때 생기는 문제를 해결할 수 있습니다"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Wi-Fi 액세스 포인트를 종료합니다."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Wi-Fi '%s'에서 연결 해제됨"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Wi-Fi '%s'에 연결 중"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[SSID 없음]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Wi-Fi 액세스 포인트가 이미 켜져 있습니다"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Wi-Fi 액세스 포인트가 꺼져 있습니다"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Wi-Fi 액세스 포인트를 시작합니다. SSID=%s 암호=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Wi-Fi 액세스 포인트 구성 파일을 만들 수 없습니다."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "잘못된 구성 파일 - %s에서 APNAME 또는 PASSWORD를 찾을 수 없습니다."
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "마우스 배율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Wii 리모컨의 x/y 속도 배율을 조정합니다."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "터치 배율"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "운영체제 단의 화면 크기 배율에 맞게 터치 입력의 X/Y 좌표 배율을 조정합니다."
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "가상 터치 마우스를 포인터로 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "터치스크린 입력을 터치 이벤트로 전달할 수 있게 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "가상 터치 마우스를 마우스로 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "터치 이벤트 입력으로 가상 마우스를 구현합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "가상 터치 마우스를 터치패드로 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "마우스로 사용 옵션과 함께 활성화하면 터치스크린을 터치패드처럼 사용할 수 있게 됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "가상 터치 마우스를 트랙볼로 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "마우스로 사용 옵션과 함께 활성화하면 터치스크린을 트랙볼처럼 활용할 수 있게 되며, 포인터에 관성이 적용됩니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "가상 터치 마우스를 제스처로 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "탭, 탭 후 드래그, 손가락 스와이프 등의 터치스크린 제스처를 활성화합니다."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "RGA 크기 조절"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA 크기 설정 및 바이큐빅 필터링을 합니다. 위젯표시가 잘못될 수 있습니다."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "컨텍스트 특정 크기 조절"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "하드웨어 컨텍스트 크기 설정을 수행합니다. (사용 가능 시)"
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "New3DS 클럭 / L2 캐시 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "New3DS의 클럭 속도(804MHz)와 L2 캐시를 사용합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "3DS 하단 화면"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "하단 화면에 상태 정보를 표시합니다. 사용해제시 배터리 및 퍼포먼스가 향상될 수 있습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "3DS 화면 모드"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "3D 및 2D 화면 모드를 선택합니다. '3D' 모드에선 픽셀이 사각으로 표시되고 빠른 메뉴에 입체 효과가 적용됩니다. '2D' 모드는 최상의 퍼포먼스를 제공합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (픽셀 그리드 효과)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (고해상도)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "터치해서\nRetroArch 메뉴로"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "애셋을 찾을 수 없음"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "데이터\n없음"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "스크린샷\n없음"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "이어하기"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "상태저장\n생성하기"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "상태저장\n불러오기"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "아래 화면 애셋 디렉토리"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "아래 화면 애셋 디렉토리입니다. 디렉토리에 \"bottom_menu.png\" 파일이 반드시 존재해야 합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "폰트 사용"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "아래 화면 메뉴에 폰트를 사용합니다. 활성화하면 아래 화면에 버튼 설명이 표시됩니다. 상태저장 일자 표시는 영향을 받지 않습니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "폰트 색 빨강"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "아래 화면 폰트의 빨강색을 조절합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "폰트 색 초록"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "아래 화면 폰트의 초록색을 조절합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "폰트 색 파랑"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "아래 화면 폰트의 파랑색을 조절합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "폰트 색 불투명도"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "아래 화면 폰트의 불투명도를 조절합니다."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "폰트 크기"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "아래 화면 폰트의 크기 배율을 조절합니다."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "스캔 완료.<br><br>\n콘텐츠를 정확하게 스캔하려면:\n<ul><li>맞는 코어를 미리 다운로드 하고</li>\n<li>\"코어 정보 파일\"을 온라인 업데이터로 업데이트하며</li>\n<li>\"데이터베이스\"도 온라인 업데이터로 업데이트하고</li>\n<li>위의 사항을 적용한 후 RetroArch를 다시 시작하십시오</li></ul>\n마지막으로 콘텐츠는 <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">이 곳</a>의 데이터베이스에 일치해야 합니다. 그래도 동작하지 않는다면 <a href=\"https://www.github.com/libretro/RetroArch/issues\">버그 리포트를 해주세요</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "터치 마우스가 활성화되었습니다"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "터치 마우스가 비활성화되었습니다"
   )

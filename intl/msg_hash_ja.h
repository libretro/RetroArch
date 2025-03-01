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
   "メインメニュー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "お気に入り"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "履歴"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "画像"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "音楽"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "動画"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "ネットプレイ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "エクスプローラー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "コンテンツレスコア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "コンテンツをインポート"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "クイックメニュー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "関連するすべてのゲーム内設定にすばやくアクセスします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "コアをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "使用するコアを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Libretro コア実装を参照します。コアディレクトリに設定されているパスがブラウザの開始ディレクトリになります。空白の場合、ルートから開始します。\nコアディレクトリにディレクトリパスが設定されている場合、メニューはそのディレクトリをトップフォルダとして使用し、フルパスが設定されている場合は、そのファイルがあるフォルダから開始します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "コンテンツをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "開始するコンテンツを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "コンテンツを参照します。コンテンツをロードするには、使用する [コア] とコンテンツファイルが必要です。\nメニューの開始ディレクトリを制御するには、[ファイルブラウザディレクトリ] を設定してください。\nブラウザは [コアをロード] で最後に設定したコアがサポートする拡張子で項目をフィルタリングし、そのコアを使用してコンテンツをロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "ディスクをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "物理メディアディスクをロードします。まず、ディスクで使用するコア (コアをロード) を選択してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "ディスクをダンプ"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "物理メディアディスクを内部ストレージにダンプします。イメージファイルとして保存されます。"
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "ディスクの取り出し"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "物理 CD/DVD ドライブからディスクを取り出します。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "プレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "データベースに一致するスキャンされたコンテンツがここに表示されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "コンテンツをインポート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "コンテンツをスキャンしてプレイリストの作成や更新を行います。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "デスクトップメニューを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "従来のデスクトップメニューを開きます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "キオスクモードを無効にする (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "設定に関係するすべての設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "オンラインアップデータ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "RetroArch のアドオン、コンポーネントおよびコンテンツをダウンロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "ネットプレイ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "ネットプレイセッションに参加またはホストします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "プログラムを設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "情報"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "システム情報を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "設定ファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "設定ファイルの管理と作成を行います。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "ヘルプ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "プログラムの仕組みについてはこちらをご覧ください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "再起動"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "RetroArch アプリケーションを再起動します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "終了"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "RetroArch アプリケーションを終了します。終了時に設定を保存する設定が有効化されています。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "RetroArch アプリケーションを終了します。終了時に設定を保存する設定が無効化されています。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "RetroArch を終了します。プログラムを (SIGKILL などで) 強制終了すると、設定を保存せずに RetroArch を終了します。 Unix 系では、SIGINT/SIGTERM を有効にすることで設定の保存を含むクリーンな初期化を可能にします。"
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "コアをダウンロード..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "オンラインアップデータからコアをダウンロードしてインストールします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "コアのインストールまたは復元"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "[Downloads] ディレクトリからコアをインストールまたは復元します。"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "ビデオプロセッサを開始"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "リモートレトロパッドを開始"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "開始ディレクトリ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "ダウンロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "圧縮ファイルを閲覧"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "圧縮ファイルをロード"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "お気に入り"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "[お気に入り] に追加したコンテンツがここに表示されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "音楽"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "以前に再生した音楽がここに表示されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "画像"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "以前に表示した画像がここに表示されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "動画"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "以前に再生した動画がここに表示されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "エクスプローラー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "カテゴリー検索インターフェイスを使用してデータベースに一致するすべてのコンテンツを閲覧します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "コンテンツレスコア"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "コンテンツをロードすることなく動作するインストール済みのコアがここに表示されます。"
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "コアダウンローダ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "インストール済みのコアを更新"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "インストール済みのすべてのコアを利用可能な最新バージョンに更新します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "コアを Google Play ストアバージョンに切り替え"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "すべてのレガシーおよび手動でインストールされたコアを、Google Play ストアで利用可能な最新バージョンに置き換えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "サムネイルアップデータ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "選択したシステムの完全なサムネイルパッケージをダウンロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "プレイリストサムネイルアップデータ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "選択したプレイリストのエントリーに対応するサムネイルをダウンロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "コンテンツダウンローダ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "選択したコアのフリーコンテンツをダウンロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "コアシステムファイルダウンローダ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "コアの正しい/最適な動作に必要な補助システムファイルをダウンロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "コア情報ファイルを更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "アセットを更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "コントローラープロファイルを更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "チートを更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "データベースを更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "オーバーレイを更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "GLSL シェーダーを更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Cg シェーダーを更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Slang シェーダーを更新"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "コア情報"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "アプリケーション/コアに関する情報を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "ディスク情報"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "挿入されたディスクについての情報を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "ネットワーク情報"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "ネットワークインターフェースと関連する IP アドレスを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "システム情報"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "デバイス固有の情報を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "データベースマネージャー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "データベースを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "カーソルマネージャー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "以前の検索を表示します。"
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "コア名"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "コアラベル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "コアバージョン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "システム名"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "システム製造元"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "カテゴリー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "作者"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "許可"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "ライセンス"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "対応する拡張子"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "必須グラフィック API"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "フルパス"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "ステートセーブ対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "なし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "基本 (セーブ/ロード)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "シリアル化 (セーブ/ロード, 巻き戻し)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "確定的 (セーブ/ロード, 巻き戻し, 先行実行, ネットプレイ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "ファームウェア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "- 注意: [コンテンツディレクトリからシステムファイルを読み込む] が現在有効化されています。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "- 検索中: '%s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "不足, 必須:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "不足, 任意:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "使用可能, 必須:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "使用可能, 任意:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "インストール済みコアをロック"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "現在インストールされているコアの変更を防止します。コンテンツが特定のコアバージョン (例: アーケード ROM セット) を必要とする場合に、望ましくない更新を回避するために使用されることがあります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "[コンテンツレスコア] メニューから除外"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "このコアが [コンテンツレスコア] タブ/メニューに表示されないようにします。表示モードが [カスタム] に設定されている場合にのみ適用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "コアを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "このコアをディスクから削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "コアをバックアップ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "現在インストールされているコアの圧縮バックアップを作成します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "バックアップから復元"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "圧縮バックアップの一覧から以前のバージョンのコアをインストールします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "バックアップを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "圧縮バックアップの一覧からファイルを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[自動]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "ビルド日時"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "RetroArch バージョン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Git バージョン"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "コンパイラ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "CPU モデル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "CPU 機能"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "CPU アーキテクチャ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "CPU コア数"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT 利用可能"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "フロントエンド識別名"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "フロントエンド OS"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "RetroRating レベル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "電源"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "ビデオのコンテクストドライバ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "ディスプレイの横幅 (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "ディスプレイの縦幅 (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "ディスプレイ DPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "LibretroDB 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "オーバーレイ対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "コマンドインターフェース対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "ネットワークコマンドインターフェース対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "ネットワークコントローラー対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Cocoa 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "PNG (RPNG) 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "JPEG (RJPEG) 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "BMP (RBMP) 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "TGA (RTGA) 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "SDL1.2 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "SDL2 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D8_SUPPORT,
   "Direct3D 8 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D9_SUPPORT,
   "Direct3D 9 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D10_SUPPORT,
   "Direct3D 10 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D11_SUPPORT,
   "Direct3D 11 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D12_SUPPORT,
   "Direct3D 12 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GDI_SUPPORT,
   "GDI 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Vulkan 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Metal 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "OpenGL 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "OpenGL ES 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "スレッド対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "KMS/EGL 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "udev 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "OpenVG 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "EGL 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "X11 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Wayland 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "XVideo 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "ALSA 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "OSS 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "OpenAL 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "OpenSL 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "RSound 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "RoarAudio 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "JACK 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "PulseAudio 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PIPEWIRE_SUPPORT,
   "PipeWire 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "CoreAudio 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "CoreAudio V3 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "DirectSound 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "WASAPI 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "XAudio2 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "zlib 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "7zip 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "ダイナミックライブラリ対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "libretro ライブラリの実行時動的ロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Cg 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "GLSL 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "HLSL 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "SDL イメージ対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "FFmpeg 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "mpv 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "CoreText 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "FreeType 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "STB TrueType 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "ネットプレイ (ピアツーピア) 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Video4Linux2 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SSL_SUPPORT,
   "SSL 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "libusb 対応"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "データベース選択"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "名前"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "説明"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "ジャンル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "実績"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "カテゴリー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "言語"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "地域"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "コンソール専用"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "プラットフォーム専用"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "スコア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "メディア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "コントロール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "アートスタイル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "ゲームプレイ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "ナラティブ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "ペース"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "視点"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "ビジュアル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "乗り物"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "販売元"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "開発元"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "原点国"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "フランチャイズ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "TGDB 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "ファミ通評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Edge Magazine レビュー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Edge Magazine 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Edge Magazine 発行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "発売月"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "発売年"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "BBFC 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "ESRB 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "ELSPA 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "PEGI 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "拡張ハードウェア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "CERO 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "シリアル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "アナログ対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "振動対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "協力プレイ対応"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "設定をロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "既存の設定をロードして現在の値を置き換えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "現在の設定を保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "現在の設定ファイルを上書きします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "新しい設定を保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "現在の設定を別のファイルに保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "デフォルトに戻す"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "現在の設定をデフォルトの値に戻します。"
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "基本メニューコントロール"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "上にスクロール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "下にスクロール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "確認"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "情報"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "スタート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "メニュー切り替え"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "終了"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "キーボード切り替え"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "ドライバ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "システムのドライバを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "ビデオ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "ビデオ出力の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "オーディオ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "オーディオ入力/出力の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "入力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "コントローラー、キーボードおよびマウスの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "レイテンシ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "ビデオ、オーディオおよび入力のレイテンシに関連する設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "コア"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "コアの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "設定ファイルのデフォルト設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "保存の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "クラウド同期"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "クラウド同期の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "クラウド同期を有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "クラウドストレージプロバイダに設定、SRAM およびステートセーブ/ロードの同期を試みます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "破壊的なクラウド同期"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "同期: セーブ/ステート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "同期: 設定ファイル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "同期: サムネイル画像"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "同期: システムファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "有効にすると、セーブ/ステートがクラウドに同期されます。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "有効にすると、設定ファイルがクラウドに同期されます。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "有効にすると、サムネイル画像がクラウドに同期されます。カスタムサムネイル画像の大規模なコレクションを除いて、一般的にはサムネイルダウンローダーを使用することが推奨されます。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "有効にすると、システムファイルがクラウドに同期されます。同期にかかる時間が大幅に増加する可能性があります。注意して使用してください。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "無効にすると、ファイルは上書きまたは削除される前にバックアップフォルダに移動されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "クラウド同期のバックエンド"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "使用するクラウドストレージネットワークプロトコルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "クラウドストレージ URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "クラウドストレージサービスへの API エントリーポイントの URL です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "ユーザー名"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "クラウドストレージアカウントのユーザー名です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "パスワード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "クラウドストレージアカウントのパスワードです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "ログ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "ログの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "ファイルブラウザ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "ファイルブラウザの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "設定ファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "圧縮ファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "録画設定ファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "データベースカーソルファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "設定ファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "シェーダープリセットファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "シェーダーファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "リマップコントロールファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "チートファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "オーバーレイファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "データベースファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "TrueType フォント ファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "プレーンファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "動画です。選択してこのファイルを動画プレイヤーで開きます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "音楽です。選択してこのファイルを音楽プレイヤーで開きます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "画像ファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "画像です。選択してこのファイルを画像ビューアで開きます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Libretro コアです。これを選択すると、このコアがゲームに関連付けられます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Libretro コアです。このコアを RetroArch にロードさせるにはこのファイルを選択してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "ディレクトリです。選択してこのディレクトリを開きます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "フレーム制御"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "巻き戻し、早送りおよびスローモーションの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "録画"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "録画の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "OSD"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "OSD オーバーレイ、OSD キーボードオーバーレイおよび OSD 通知の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "ユーザーインターフェース"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "ユーザーインターフェースの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "AI サービス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "AI サービス (翻訳/テキスト読み上げ/その他) の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "ユーザー補助"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "ユーザー補助ナレーターの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "電源管理"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "電源管理の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "実績"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "実績の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "ネットワーク"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "サーバーとネットワークの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "プレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "プレイリストの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "ユーザー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "アカウント、ユーザー名および言語の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "ディレクトリ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "ファイルが置かれるデフォルトのディレクトリを変更します。"
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS,
   "ハック"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "マッピング"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "メディア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "パフォーマンス"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "サウンド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "仕様"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "ストレージ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "システム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "タイミング"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Steam に関連する設定を変更します。"
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "入力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "使用する入力のドライバです。一部のビデオドライバは異なる入力ドライバを強制します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "udev ドライバは、キーボード対応のために evdev イベントを読み込みます。キーボードのコールバック、マウス、タッチパッドにも対応します。\nほとんどのディストロで、デフォルトでは root のみです (モード 600)。これらのルールを root 以外からアクセスできるようにする udev ルールを設定できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "Linuxraw 入力ドライバにはアクティブな TTY が必要です。キーボードイベントは TTY から直接読み取られ、簡単になりますが、udev ほど柔軟ではありません。 マウスなどは全くサポートされていません。このドライバは古いジョイスティック API (/dev/input/js*) を使用しています。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "入力ドライバです。このビデオドライバは異なる入力ドライバを強制する可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "コントローラー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "使用するコントローラドライバです。(再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "DirectInput コントローラードライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "低レベルヒューマンインターフェースデバイスドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Raw Linux ドライバは、古いジョイスティック API を使用します。可能であれば代わりに udev を使用してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "特別なアダプターを介してパラレルポートに接続されたコントローラー用の Linux ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "SDL ライブラリを基にしたコントローラーのドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "udev インターフェースを持つコントローラードライバで、一般的に推奨されます。ジョイスティック対応には、最近の evdev joypad API を使用します。フォースフィードバックとホットプラグに対応します。ほとんどのディストロで、デフォルトでは /dev/input ノードは root のみです (モード 600)。これらのルールを root 以外からアクセスできるようにする udev rule を設定できます[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "XInput コントローラードライバです。主に XBox コントローラー 用です。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "ビデオ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "使用するビデオのドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "OpenGL 1.x ドライバです。最小要求バージョン: OpenGL 1.1.1。シェーダーに対応していません。可能であれば、後の OpenGL ドライバを使用してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "OpenGL 2.x ドライバです。ソフトウェアレンダリングされたコアに加えて、libretro GL コアを使用することができます。 最小要求バージョン: OpenGL 2.0 または OpenGLES 2.0。 GLSL シェーダー形式に対応します。 可能であれば、代わりに glcore ドライバを使用してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "OpenGL 3.x ドライバです。ソフトウェアレンダリングされたコアに加えて、libretro GL コアを使用することができます。 最小要求バージョン: OpenGL 3.2 または OpenGLES 3.0+。 Slang シェーダー形式をサポートします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Vulkan ドライバです。ソフトウェアレンダリングされたコアに加えて、libretro Vulkan コアを使用することができます。 最小要求バージョン: Vulkan 1.0。HDR および Slang シェーダーに対応します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "SDL 1.2 ソフトウェアレンダリングドライバです。パフォーマンスは最適ではありません。最後の手段としてのみ使用することを検討してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "SDL 2 ソフトウェアレンダリングドライバです。ソフトウェアレンダリングされた libretro コア実装のパフォーマンスは、使用しているプラットフォームの SDL の実装に依存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Apple プラットフォーム用の Metal ドライバです。Slang シェーダー形式に対応します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "シェーダーに対応しない Direct 3D 8 ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "古い Cg シェーダー形式に対応する Direct3D 9 ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "HLSL シェーダー形式に対応する Direct3D 9 ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Slang シェーダー形式に対応する Direct3D 10 ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "HDR および Slang シェーダー形式に対応する Direct3D 11 ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "HDR および Slang シェーダー形式に対応する Direct3D 12 ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "DispmanX ドライバです。Raspberry Pi 0..3 の Videocoer IV GPU で DispmanX API を使用します。オーバーレイやシェーダーに対応していません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "LibCACA ドライバです。グラフィックの代わりにキャラクター出力を生成します。実用的な使用には推奨されません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "ブリット操作に Samsung Exynos SoC の G2D ブロックを使用する低レベルの Exynos ビデオドライバです。レンダリングされたコアのパフォーマンスは最適でなければなりません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Plain DRM ビデオドライバです。GPU オーバーレイを使用したハードウェアスケーリングに libdrm を使用した低レベルビデオドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Allwinner SoCs の G2D ブロックを使用する低レベル Sunxi ビデオドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Wii U ドライバです。Slang シェーダーに対応します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Switch ドライバです。GLSL シェーダー形式に対応します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "OpenVG ドライバです。OpenVG ハードウェアアクセラレーション 2D ベクトルグラフィックス API を使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "GDI ドライバです。旧式の Windows インターフェースを使用します。使用しないことをお勧めします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "現在のビデオのドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "オーディオ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "使用するオーディオのドライバです。 "
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "ネットワークオーディオシステム用の RSound ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Legacy Open Sound System ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "デフォルトの ALSA ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "スレッドに対応する ALSA ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "依存関係なく実装された ALSA ドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "RoarAudio サウンドシステムドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "OpenAL のドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "OpenSL のドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "DirectSound のドライバーです。DirectSound は主に Windows 95 から Windows XP で使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Windows Audio Session API ドライバです。WASAPI は主に Windows 7 以降から使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "PulseAudio ドライバです。システムが PulseAudio を使用している場合は、ALSA などの代わりにこのドライバを使用するようにしてください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Jack Audio Connection Kit ドライバです。"
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "マイク"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "使用するマイクのドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "マイクリサンプラー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "使用するマイクリサンプラーのドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "マイクブロックフレーム数"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "オーディオリサンプラー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "使用するオーディオリサンプラーのドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Windows Sinc 実装です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "複雑なコサインの実装です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "ニアレストリサンプリング実装です。このリサンプラーは品質設定を無視します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "カメラ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "使用するカメラのドライバです。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "使用する Bluetooth のドライバです。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "使用する Wi-Fi のドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "位置情報"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "使用する位置情報のドライバです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "メニュー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "使用するメニュードライバです。(再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB は、第 7 世代のコンソールメニューに似た RetroArch GUI です。Ozone と同じ機能に対応します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone は、ほとんとのプラットフォームで RetroArch のデフォルトの GUI です。ゲームコントローラーでのナビゲーションに最適化されています。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI は RetroArch のシンプルなビルトイン GUI です。メニュードライバの中で最も要求パフォーマンスが低く、低解像度の画面で使用することができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "モバイルデバイスでは、RetroArch はモバイル UI、Material UI をデフォルトで使用します。このインターフェースは、マウス/トラックボールのようなポインタデバイスやタッチスクリーンを中心に設計されています。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "録画"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "使用する録画のドライバです。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "使用する MIDI のドライバです。"
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "CRT 解像度切替"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "CRT ディスプレイで利用するためのネイティブな低解像度信号を出力します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "出力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "ビデオ出力の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "フルスクリーンモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "フルスクリーンモードの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "ウィンドウモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "ウィンドウモードの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "スケーリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "ビデオのスケーリング設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "HDR の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "同期"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "ビデオの同期設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "スクリーンセーバーを一時停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "システムのスクリーンセーバーの起動を抑制します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "スクリーンセーバーを一時停止します。必ずしもビデオドライバが従う必要のないヒントです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "ビデオのスレッド化"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "遅延やビデオのカクつきを代償にパフォーマンスを改善します。最大の速度が得られない場合にのみ使用してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "スレッド化されたビデオドライバを使用します。これを使用すると、ビデオのカクつきと遅延を代償にパフォーマンスを改善することができるかもしれません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "黒フレーム挿入"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "フレーム間に黒フレームを挿入し、動きをより鮮明にします。現在のディスプレイのリフレッシュレート用に準備されたオプションのみを使用してください。144Hz、165Hz など、60Hz の倍数ではないリフレッシュレートでは使用できません。1 以上のスワップ間隔、フレーム遅延または正確なフレームレートに同期と組み合わせないでください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_120,
   "1 - 120Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_180,
   "2 - 180Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_240,
   "3 - 240Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_300,
   "4 - 300Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_360,
   "5 - 360Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_420,
   "6 - 420Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_480,
   "7 - 480Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_540,
   "8 - 540Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_600,
   "9 - 600Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_660,
   "10 - 660Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_720,
   "11 - 720Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_780,
   "12 - 780Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_840,
   "13 - 840Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_900,
   "14 - 900Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION_VALUE_960,
   "15 - 960Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "黒フレーム挿入 - 暗フレーム"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "黒フレーム挿入シーケンス全体の黒フレーム数を調整します。多くするほど動きが明瞭になり、少なくするほど輝度が高くなります。120Hz では、動作する合計黒フレーム数が 1 つしか確保できないため、適用されません。可能な限り高く設定すると、選択されたリフレッシュレートで利用可能な最大値に制限されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "黒フレーム挿入シーケンスで表示される黒フレーム数を調整します。黒フレームを増やすほど動きが鮮明になりますが、明るさが低下します。120Hz の場合、追加の 60Hz フレームを 合計 1 つしか確保できないため、黒でなければ黒フレーム挿入を有効化できません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "シェーダーサブフレーム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "コンテンツフレームレートよりも高速に動作するように設計されたシェーダー効果用に、フレーム間に追加のシェーダーフレームを挿入します。現在のディスプレイ Hz に対応するオプションのみを使用してください。144Hz、165Hz など、60Hz の倍数ではないリフレッシュレートでは使用できません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_120,
   "2 - 120Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_180,
   "3 - 180Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_240,
   "4 - 240Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_300,
   "5 - 300Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_360,
   "6 - 360Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_420,
   "7 - 420Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_480,
   "8 - 480Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_540,
   "9 - 540Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_600,
   "10 - 600Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_660,
   "11 - 660Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_720,
   "12 - 720Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_780,
   "13 - 780Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_840,
   "14 - 840Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_900,
   "15 - 900Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES_VALUE_960,
   "16 - 960Hz ディスプレイ用リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "GPU スクリーンショット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "ループ回転スキャンラインシミュレーション"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "画面を垂直に分割し、画面の上端から下端にあるサブフレームの数に応じて画面の各部分をレンダリングすることで、複数のサブフレームにわたるブラウン管をカメラで撮影したときのようなループ回転スキャンラインをシミュレートします。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "利用可能な場合、スクリーンショットは GPU シェーディングされた画像をキャプチャします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "バイリニアフィルタリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "画像にわずかなぼかしを加え、鋭いピクセルの角を和らげます。このオプションはパフォーマンスにほとんど影響を与えません。シェーダーを使用する場合は無効にしてください。"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "画像補間"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "内部 IPU 経由でコンテンツをスケーリングする際の画像補間方法を指定します。CPU ベースのビデオフィルターを使用する場合は、[バイキュービック] または [バイリニア] が推奨されます。このオプションはパフォーマンスに影響を与えません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "バイキュービック"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "バイリニア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "ニアレストネイバー"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "画像補間"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "整数倍拡大が無効になっている場合の補完方法を指定します。ニアレストネイバーはパフォーマンスへの影響が少なくなります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "ニアレストネイバー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "セミリニア"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "自動シェーダー遅延"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "シェーダーの自動ロードを遅延させます (ミリ秒単位)。画面取り込みソフトウェアを使用するときのグラフィックバグを回避できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "ビデオフィルター"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "CPU ベースのビデオフィルターを適用します。パフォーマンスに大きく影響する可能性があります。一部のビデオフィルターは、32 ビットまたは 16 ビットカラーを使用するコアでのみ動作します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "CPU ベースのビデオフィルターを適用します。パフォーマンスに大きく影響する可能性があります。一部のビデオフィルターは、32 ビットまたは 16 ビットカラーを使用するコアでのみ動作します。動的にリンクされたビデオフィルターライブラリを選択できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "CPU ベースのビデオフィルターを適用します。パフォーマンスに大きく影響する可能性があります。一部のビデオフィルターは、32 ビットまたは 16 ビットカラーを使用するコアでのみ動作します。ビルトインビデオフィルターライブラリを選択できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "ビデオフィルターを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "アクティブな CPU ベースビデオフィルターをアンロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Android と iOS デバイスでノッチ上のフルスクリーンを有効にする"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "CRT 解像度切替"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "CRT ディスプレイ専用です。コア/ゲームの正確な解像度およびリフレッシュレートを使用するよう試みます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "CRT 超解像度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "ネイティブとウルトラワイドの超解像度を切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "X 軸センタリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "画像がディスプレイ上で正しく中央寄せされていない場合はこのオプションを微調整してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "ポーチの調整"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "これらのオプションを微調整し、ポーチの設定を合わせ画像のサイズを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "高解像度メニューを使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "コンテンツが読み込まれていない場合、高解像度メニューで使用するための高解像度モデルに切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "カスタムリフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "必要に応じて設定ファイルで定義されたカスタムリフレッシュレートを使用します。"
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "モニター番号"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "使用する表示画面を選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "どのモニターを優先するかを選択します。0 (デフォルト) は特定のモニターが優先されないことを意味し、1 以上 (1 は最初のモニター) は RetroArch が特定のモニターを使用することを優先します。"
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Wii U ゲームパッドに最適化 (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "表示領域としてゲームパッドの 2x 表示倍率を使用します。テレビのネイティブ解像度で表示する場合は無効にしてください。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "ビデオの回転"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "ビデオの向きを特定の角度で強制的に回転させます。この回転はコアが設定する回転に追加されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "画面の向き"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "オペレーティングシステムから画面の向きを特定の角度で強制的に回転させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "GPU 番号"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "使用するグラフィックカードを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "画面水平オフセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "ビデオに水平方向の一定のオフセットを強制します。オフセットはグローバルに適用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "画面垂直オフセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "ビデオに垂直方向の一定のオフセットを強制します。オフセットはグローバルに適用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "垂直リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "画面の垂直リフレッシュレートです。適切なオーディオ入力レートを計算するために使用されます。\n[ビデオのスレッド化] を有効にすると、これは無視されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "推定画面リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "画面の正確な推定リフレッシュレート (Hz) です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "モニターの正確なリフレッシュレート (Hz) です。この値は、以下の式でオーディオ入力レートを計算するために使用されます:\naudio_input_rate = game input rate * display refresh rate / game refresh rate\nコアが何も値を報告しない場合、互換性のために NTSC のデフォルトと仮定されます。\nピッチが大きく変化するのを避けるため、この値は 60Hz に近づける必要があります。モニターが 60Hz[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "ディスプレイが報告するリフレッシュレートを設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "ディスプレイドライバによって報告されたリフレッシュレートです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "自動リフレッシュレート切り替え"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "現在のコンテンツに基づいて自動的に画面のリフレッシュレートを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "排他的フルスクリーンモードのみ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "ウィンドウフルスクリーンモードのみ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "すべてのフルスクリーンモード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "自動リフレッシュレート PAL しきい値"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "PAL とみなされる最大リフレッシュレートです。"
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "垂直リフレッシュレート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "ディスプレイの垂直リフレッシュレートを設定します。PAL コンテンツを実行する場合、[50Hz] に設定すると滑らかなビデオになります。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "sRGB FBO を強制的に無効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "sRGB FBO の対応を強制的に無効にします。一部の Windows 用 Intel OpenGL ドライバは sRGB FBO でビデオの問題があります。これを有効にすることで回避できます。"
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "フルスクリーンモードで開始"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "フルスクリーンで開始します。実行中に変更できます。コマンドラインスイッチで上書きできます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "ウィンドウフルスクリーンモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "フルスクリーンにする場合は、表示モードの切り替えを防ぐためにウィンドウフルスクリーンモードを使用することをお勧めします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "フルスクリーンの幅"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "排他的フルスクリーンモードのカスタム幅サイズを設定します。未設定のままにすると、デスクトップの解像度が使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "フルスクリーンの高さ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "排他的フルスクリーンモードのカスタム高さサイズを設定します。未設定のままにすると、デスクトップの解像度が使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "UWP で解像度を強制"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "解像度をフルスクリーンサイズに強制します。0 に設定すると、3840 x 2160 の固定値が使用されます。"
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "ウィンドウ表示倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "ウィンドウのサイズをコア表示領域サイズの指定された倍率に設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "ウィンドウの不透明度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "ウィンドウの透明度を設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "ウィンドウの装飾を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "ウィンドウのタイトルバーと枠を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "メニューバーを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "ウィンドウのメニューバーを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "ウィンドウの位置とサイズを記憶"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "すべてのコンテンツを [ウィンドウの幅] と [ウィンドウの高さ] で指定した寸法の固定サイズウィンドウに表示し、RetroArch を閉じたときに現在のウィンドウの位置とサイズを保存します。無効にすると、ウィンドウのサイズは [ウィンドウ表示倍率] に基づいて動的に設定されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "カスタムウインドウサイズを使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "すべてのコンテンツを [ウィンドウの幅] と [ウィンドウの高さ] で指定した寸法の固定サイズウィンドウに表示します。無効にすると、ウィンドウのサイズは [ウィンドウ表示倍率] に基づいて動的に設定されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "ウィンドウの幅"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "表示ウィンドウのカスタム幅を設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "ウィンドウの高さ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "表示ウィンドウのカスタム高さを設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "ウインドウの最大幅"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "[ウィンドウ表示倍率] に基づいて自動的にサイズを変更するとき、表示ウィンドウの最大幅を設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "ウインドウの最大高さ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "[ウィンドウ表示倍率] に基づいて自動的にサイズを変更するとき、表示ウィンドウの最大高さを設定します。"
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "整数倍拡大"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "整数倍拡大軸"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "アンダースケール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "オーバースケール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "スマート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "アスペクト比"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "表示アスペクト比を設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "構成アスペクト比"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "ビデオアスペクト比の浮動小数点値 (幅 / 高さ) です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "構成"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "コア提供"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "カスタム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "フル"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "アスペクト比を維持"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "内部 IPU 経由でコンテンツをスケーリングする際に、1:1 ピクセルアスペクト比を維持します。無効にすると、画像はディスプレイ全体に引き伸ばされます。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "カスタムアスペクト比 (X 位置)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "表示領域の X 軸位置を定義するために使用されるオフセット値です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "カスタムアスペクト比 (Y 位置)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "表示領域の Y 軸位置を定義するために使用されるオフセット値です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "表示領域 X 座標補正"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "表示領域 X 座標補正"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "表示領域 Y 座標補正"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "表示領域 Y 座標補正"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "表示領域がコンテンツの幅より広い場合、水平方向のオフセットに使用される補正値です。0.0 は左端を、1.0 は右端を表します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "表示領域がコンテンツの高さより高い場合、垂直方向のオフセットに使用される補正値です。0.0 は上端を、1.0 は下端を表します。"
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "表示領域 X 座標補正 (縦向き)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "表示領域 X 座標補正 (縦向き)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "表示領域 Y 座標補正 (縦向き)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "表示領域 Y 座標補正 (縦向き)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "表示領域がコンテンツの幅より広い場合、水平方向のオフセットに使用される補正値です。0.0 は左端を、1.0 は右端を表します (縦向き)。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "表示領域がコンテンツの高さより高い場合、垂直方向のオフセットに使用される補正値です。0.0 は上端を、1.0 は下端を表します (縦向き)。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "カスタムアスペクト比 (幅)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "アスペクト比が [カスタムアスペクト比] に設定されているときに使用されるカスタム表示領域の幅です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "カスタムアスペクト比 (高さ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "アスペクト比が [カスタムアスペクト比] に設定されているときに使用されるカスタム表示領域の高さです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "オーバースキャンをトリミング (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "画像の端の周囲数ピクセルを切り取ります。開発者が慣例的に空白のままにしている部分で、不要なピクセルが含まれる場合もあります。"
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "HDR を有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "ディスプレイが対応している場合、HDR を有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "最大輝度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "ディスプレイの最大輝度 (cd/m2) を設定します。ディスプレイの最大輝度については RTings を参照してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "ペーパーホワイト輝度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "SDR (スタンダードダイナミックレンジ) 範囲の輝度の上限、または文字として読めるようにペーパーホワイトの輝度を設定します。環境の異なる照明条件に調整するのに便利です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "コントラスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "HDR 用のガンマ/コントラストコントロールです。色を取得し、画像の最も明るい部分と最も暗い部分の間の全体的な範囲を広げます。HDR コントラストを高くするほどこの差は大きくなり、逆に低くするほど画像は退色します。ユーザーが自分の好みに合わせて画像を調整し、自分のディスプレイで最も美しく見えるようにするのに役立ちます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "色域を拡張"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "色空間がリニア空間に変換された際に、HDR10 に到達するために拡張色色域を使用するかどうかを決定します。"
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "垂直同期 (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "グラフィックカードの出力ビデオを画面のリフレッシュレートに同期します。オンにすることをお勧めします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "垂直同期のスワップ間隔"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "垂直同期にカスタムスワップ値を使用します。モニターのリフレッシュレートを指定した係数で効果的に減少させます。[自動] はコアから報告されたフレームレートに基づいて係数を決定し、例えば 60Hz のディスプレイで 30 fps のコンテンツを実行したり、120Hz のディスプレイで 60 fps のコンテンツを実行したりする場合にフレームペーシングを改善します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "自動"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "適応型垂直同期"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "実行速度がターゲットフレームレートを下回るまで垂直同期が有効になります。実行速度がリアルタイムを下回ったときのカクつきを最小限に抑え、電力効率を向上させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "フレーム遅延"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "自動フレーム遅延"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "自動"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "効果的"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "ハード GPU 同期"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "CPU と GPU をハード同期します。パフォーマンスを代償に遅延を軽減します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "ハード GPU 同期フレーム数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "[ハード GPU 同期] を使用する場合、CPU が GPU より先に実行できるフレーム数を設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "[GPU ハード同期] を使用する場合、CPU が GPU より前に実行できるフレーム数を設定します。最大値は 3 です。\n0: 即時に GPU に同期する。\n1: 前のフレームに同期する。\n2: その他…"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "正確なフレームレートに同期 (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "コアが要求したタイミングと極めて正確に同期します。可変リフレッシュレートに対応する画面 (G-Sync, FreeSync, HDMI 2.1 VRR) で使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "正確なコンテンツフレームレートに同期します。このオプションは、早送りを許可しながら x1 速度を強制するのと同等です。コアが要求するリフレッシュレートからの逸脱はなく、サウンドのダイナミックレートコントロールはありません。"
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "出力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "オーディオ出力の設定を変更します。"
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "マイク"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "オーディオ入力の設定を変更します。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "リサンプラー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "オーディオリサンプラーの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "同期"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "オーディオの同期設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "MIDI の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "ミキサー設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "オーディオミキサーの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "メニュー音"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "メニューのサウンド設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "消音"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "オーディオを消音にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "ミキサーを消音"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "ミキサーオーディオを消音にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "サイレントモードをリスペクト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "サイレントモードですべてのオーディオを消音にします。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "早送りを使用中に自動的にオーディオを消音にします。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "早送り時にオーディオを高速化します。音割れを防ぎますが、ピッチがずれます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "音量 (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "オーディオの音量 (dB) です。0dB は通常の音量で、増幅されません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "オーディオ音量は dB で表されます。0 dB は通常の音量で、ゲインは適用されません。ゲインは、入力音量増加/減少でコンテンツ実行中にコントロールできます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "ミキサー音量 (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "グローバルオーディオミキサーの音量 (dB) です。0dB は通常の音量で、増幅されません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "DSP プラグイン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "ドライバに送信される前にオーディオを処理するオーディオ DSP プラグインです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "DSP プラグインを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "アクティブなオーディオ DSP プラグインをアンロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI 排他モード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI ドライバがオーディオデバイスを排他的に制御できるようにします。無効にすると、代わりに共有モードを使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "WASAPI 浮動小数点方式"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "オーディオデバイスが対応している場合は、WASAPI ドライバに浮動小数形式を使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI 共有バッファサイズ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI ドライバを共有モードで使用する場合の中間バッファ長 (フレーム単位) です。"
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "オーディオ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "オーディオ出力を有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "デバイス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "オーディオドライバが使用するデフォルトのオーディオデバイスを上書きします。これはドライバに依存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "オーディオドライバが使用するデフォルトのオーディオデバイスを上書きします。これはドライバに依存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "ALSA ドライバのカスタム PCM デバイス値です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "OSS ドライバのカスタムパス値 (例: /dev/dsp) です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "JACK ドライバのカスタムポート名 (例: sytem:playback1、system:playback_2) です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "RSound ドライバ用の RSound サーバーのカスタム IP アドレスです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "オーディオレイテンシ (ms)"
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "マイク"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "対応しているコアでオーディオ入力を有効にします。コアがマイクを使用していない場合、オーバーヘッドはありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "デバイス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "マイクドライバが使用するデフォルトの入力デバイスを上書きします。これはドライバに依存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "マイクドライバが使用するデフォルトの入力デバイスを上書きします。これはドライバに依存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "リサンプラー品質"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "オーディオ品質よりもパフォーマンス/低遅延を優先したい場合はこの値を下げ、パフォーマンスや低遅延を犠牲にしてオーディオ品質を向上させたい場合は値を上げます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "デフォルト入力レート (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "オーディオ入力のサンプリングレートは、コアが特定の値を要求しない場合に使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "オーディオ入力レイテンシ (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "オーディオ入力レイテンシをミリ秒単位で指定します。マイクドライバが指定したレイテンシを提供できない場合、この値は使用されない可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "WASAPI 排他モード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "WASAPI マイクドライバを使用する場合、RetroArch がマイクデバイスを独占的に制御できるようにします。無効にすると、RetroArch は代わりに共有モードを使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "WASAPI 浮動小数点方式"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "オーディオデバイスが対応している場合は、WASAPI ドライバで浮動小数点入力を使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI 共有バッファサイズ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI ドライバを共有モードで使用する場合の中間バッファ長 (フレーム単位) です。"
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "リサンプラー品質"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "オーディオ品質よりもパフォーマンス/低遅延を優先したい場合はこの値を下げ、パフォーマンスや低遅延を犠牲にしてオーディオ品質を向上させたい場合は値を上げます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "出力レート (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "オーディオ出力のサンプリングレートです。"
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "同期"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "オーディオを同期します。推奨です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "最大タイミングスキュー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "オーディオ入力レートの最大変化量です。値を大きくするほど、タイミングを大きく変更できるようになりますが、代償としてオーディオピッチの精度が低下します (例: NTSC ディスプレイで PAL コアを実行する場合など)。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "最大オーディオタイミングスキューです。\n入力レートの最大変化を定義します。例えば、NTSC ディスプレイ上で PAL コアを実行する場合など、タイミングを非常に大きく変化させたい場合はオーディオピッチが不正確になることを覚悟でこの値を大きくするとよいでしょう。\n入力レートは以下のように定義されます:\n入力レート * (1.0 +/- (最大タイミングスキュー))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "ダイナミックオーディオレートコントロール"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "オーディオとビデオを同期する際に、タイミングの不完全さを平滑化します。無効にすると、適切な同期がほぼ不可能になることに注意してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "この値を 0 に設定すると、レート制御は無効になります。それ以外の値はオーディオレートコントロールデルタを制御します。\n入力レートをどれだけ動的に調整できるかを定義します。入力レートは以下のように定義されます:\n入力レート * (1.0 +/- (レート制御デルタ))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "入力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "入力デバイスを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "入力デバイス (ドライバ固有) を設定します。[オフ] に設定すると、MIDI 入力は無効になります。デバイス名も入力できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "出力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "出力デバイスを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "出力デバイス (ドライバ固有) を設定します。[オフ] に設定すると、MIDI 出力は無効になります。デバイス名も入力できます。\nMIDI 出力が有効になっていて、コアおよびゲーム/アプリが MIDI 出力に対応している場合、一部またはすべてのサウンド (ゲーム/アプリに依存) は MIDI デバイスによって生成されます。[null] MIDI ドライバの場合、これらのサウンドは聞こえないこと[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "音量"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "出力音量 (%) を設定します。"
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "ミキサーストリーム #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "再生"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "オーディオストリームの再生を開始します。再生が終わると、現在のオーディオストリームがメモリから削除されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "再生 (ループ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "オーディオストリームの再生を開始します。再生が終わると、トラックは最初から繰り返して再生されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "再生 (順番に)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "オーディオストリームの再生を開始します。再生が終わると、次のオーディオストリームに順番にジャンプし、この動作を繰り返します。アルバム再生モードとして便利です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "一時停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "オーディオストリームの再生を停止しますが、メモリからは取り除かれません。[再生] を選択することで再生を再開できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "オーディオストリームの再生を停止してメモリから完全に取り除きます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "音量"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "オーディオストリームの音量を調整します。"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "ミキサー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "メニュー内でも同時オーディオストリームを再生します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "[OK] 音を有効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "[キャンセル] 音を有効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "[通知] 音を有効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "[BGM] を有効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "[スクロール] 音を有効にする"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "最大ユーザー数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "RetroArch が対応する最大ユーザー数です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "ポーリングの動作 (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "RetroArch の入力ポーリングに影響します。設定によっては、[早い] や [遅い] に設定することで遅延が少なくなります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "RetroArch 内部で入力ポーリングがどのように行われるかに影響します。\n早い - 入力ポーリングはフレームが処理される前に実行されます。\n通常 - 入力ポーリングはポーリングが要求されたときに実行されます。\n遅い - 入力ポーリングはフレームごとの最初の入力状態要求時に実行されます。\n[早い] または [遅い] に設定すると、設定によっては遅延が少なくなります。[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "このコアでコントロールをリマップ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "入力割り当てを現在のコアに設定されているリマップ設定で上書きします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "自動設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "プロファイルが存在するコントローラーを自動的に設定します。プラグアンドプレイスタイルです。"
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Windows ホットキーを無効にする (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Win キーの組み合わせをアプリケーション内に保持します。"
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "物理キーボードを選択"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "このデバイスをゲームパッドとしてではなく物理キーボードとして使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "RetroArch がハードウェアキーボードを何らかのゲームパッドとして識別した場合、この設定を使用すると、RetroArch に誤認されたデバイスをキーボードとして認識するよう強制することができます。\nこれは、一部の Android TV デバイスでコンピュータをエミュレートしようとしている場合や、ボックスに接続できる物理キーボードを所有している場合に便利です。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "補助センサー入力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "加速度センサー、ジャイロスコープ、明るさセンサーからの入力を有効にします。現在のハードウェアでサポートされていない場合は無視されます。一部のプラットフォームでパフォーマンスに影響を与えたり、消費電力を増加させたりする可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "自動的にマウスの移動範囲をウィンドウ内に制限する"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "ウィンドウがアクティブである場合、マウスの移動範囲をウィンドウ内に制限します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "自動的に [ゲームフォーカス] モードを有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "コンテンツの起動と再開時に常に [ゲームフォーカス] モードを有効にします。[検知] に設定すると、現在のコアがフロントエンドのキーボードコールバック機能を実装している場合にオプションが有効になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "オン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "検知"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "コントローラー切断時にコンテンツを一時停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "いずれかのコントローラーが切断されたときにコンテンツを一時停止します。スタートで再開します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "入力ボタン軸のしきい値"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "[アナログデジタル変換] を使用している場合、ボタンの押下判定の発生に必要な軸の傾きです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "アナログデッドゾーン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "デッドゾーン値以下のアナログスティックの動きを無視します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "アナログ入力感度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "アナログスティックの感度を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "割り当て時のタイムアウト時間"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "次の割り当てに進むまで待機する秒数です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "割り当て時の長押し時間"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "入力を割り当てるために長押しする秒数です。"
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "キーボード、マウスまたはコントローラーを押してください"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "キーとボタンを離してください!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "タイムアウト"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "長押し"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "連射"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "ターボ間隔"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "ターボモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "ターボモードの方式を選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "クラシック"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "クラシック (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "シングルボタン (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "シングルボタン (長押し)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "連射"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "触覚フィードバック/振動"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "触覚フィードバックと振動の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "メニューコントロール"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "メニューコントロールの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "ホットキー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "ゲームプレイ中のメニュー切り替えなど、ホットキーの設定や割り当てを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "レトロパッドの割り当て"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "仮想レトロパッドが物理入力デバイスに対してどのように割り当てられるかを変更します。入力デバイスの認識と自動設定が正しく行われている場合、このメニューを使用する必要はおそらくありません。\n注意: コア固有の入力を変更するには、代わりにクイックメニューの [コントロール] サブメニューを使用してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro はレトロパッドという名称の概念を使用し、RetroArch のようなフロントエンドとコア間で相互通信を行います。このメニューは、仮想レトロパッドと物理入力デバイスおよびそれらが使用する入力ポートの割り当て設定を変更します。\n物理入力デバイスの認識と自動設定が正しく行われている場合、このメニューを使用する必要はおそらくありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "ポート %u コントロール"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "仮想レトロパッドがこの仮想ポートの物理入力デバイスに対してどのように割り当てられるかを変更します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "コア固有の入力割り当てを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Android 切断対策"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "コントローラーの切断と再接続を回避するための回避策です。同じ型番のコントローラーを使用する 2 人のプレイヤーに影響を与えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "終了時に確認"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "終了ホットキーで RetroArch を終了するとき、2 度押しを要求します。"
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "キー入力で振動"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "デバイスの振動を有効にする (対応コア用)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "振動の強さ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "触覚フィードバック効果の大きさを指定します。"
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "メニューコントロールを統一"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "キーボードでメニューを操作する際に、Enter/Backspace の代わりにレトロパッドの A/B ボタンに割り当てられたキーボードキーで決定/キャンセルを行います。PageUp/PageDown などの割り当ても、レトロパッドの割り当てに準じて変更されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "情報ボタンを無効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "有効にすると、情報ボタンの押下が無視されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "検索ボタンを無効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "有効にすると、検索ボタンの押下が無視されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "決定/キャンセルボタンの入れ替え"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "決定/キャンセルボタンを入れ替えます。無効にすると日本式 (右側ボタンを決定/下側ボタンをキャンセル) に、有効にすると西洋式 (右側ボタンをキャンセル/下側ボタンを決定) になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "メニュースクロールボタンの入れ替え"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "スクロールのボタンを入れ替えます。無効にすると、L/R で 10 項目をスクロールし、L2/R2 でアルファベット順にスクロールします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "全ユーザーにメニューの操作を許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "どのユーザーでもメニューを操作できるようにします。無効にすると、ユーザー 1 のみがメニューを操作できます。"
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "ホットキーの有効化"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "割り当てられている場合、他のホットキーが認識される前に [ホットキーの有効化] キーを押したままにする必要があります。通常の入力に影響を与えることなく、ホットキー機能をコントローラーボタンにマッピングできるようにします。変更をコントローラーのみに割り当てる場合、キーボードホットキーにはその変更は必要ありません。またその逆も同様ですが、両[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "このホットキーがキーボード、ジョイスティックボタンまたはジョイスティック軸のいずれかにバインドされている場合、このホットキーが同時に押されていない限り、他のすべてのホットキーは無効になります。\nこれは、ホットキーが邪魔になることが望ましくない、キーボードの広い範囲をクエリする RETRO_KEYBOARD 中心の実装に便利です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "ホットキーの有効化遅延 (フレーム数)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "割り当てられた [ホットキーの有効化] キーを押したあと、通常の入力がブロックされるまでにフレーム単位で遅延を追加します。[ホットキーの有効化] キーが別のアクション (例: レトロパッド [セレクト])にマッピングされたときに、そのキーからの通常の入力をキャプチャできるようにします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "ホットキーデバイスの種類の統合"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "キーボードとコントローラーのいずれかの種類のデバイスに [ホットキーの有効化] が設定されている場合、すべてのホットキーをブロックします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "メニュー切り替え (コントローラー同時押し)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "メニュー切り替えのコントローラーボタンの組み合わせです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "メニュー切り替え"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "終了 (コントローラー同時押し)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "RetroArch を終了するためのコントローラーボタンの組み合わせです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "終了"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "RetroArch を閉じ、すべてのセーブデータと設定ファイルがディスクに保存されるようにします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "コンテンツを閉じる"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "現在のコンテンツを閉じます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "コンテンツをリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "現在のコンテンツを最初から再起動します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "早送り (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "早送りと通常の速度を切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "早送り (長押し)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "押している間、早送りを有効にします。キーを放すと、コンテンツは通常の速度で実行されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "スローモーション (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "スローモーションと通常の速度を切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "スローモーション (長押し)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "押している間、スローモーションを有効にします。キーを放すと、コンテンツは通常の速度で実行されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "巻き戻し"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "キーが押されている間、現在のコンテンツを巻き戻します。[巻き戻し対応] を有効にする必要があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "一時停止"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "コマ送り"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "一時停止時に 1 フレームずつコンテンツを進めます。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "オーディオを消音"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "オーディオ出力のオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "音量アップ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "オーディオ出力の音量レベルを上げます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "音量ダウン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "オーディオ出力の音量レベルを下げます。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "ステートロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "現在選択されているスロットに保存したステートをロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "ステートセーブ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "現在選択されているスロットにステートを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "次のステートセーブスロット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "現在選択されているステートセーブスロットのインデックスを増やします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "前のステートセーブスロット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "現在選択されているステートセーブスロットのインデックスを減らします。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "ディスクの取り出し (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "仮想ディスクトレイが閉じられている場合、トレイを開いてロードされたディスクを取り出します。それ以外の場合は、現在選択されているディスクを挿入してトレイを閉じます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "次のディスク"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "現在選択されているディスク番号を増やします。仮想ディスクトレイが開いている必要があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "前のディスク"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "現在選択されているディスク番号を減らします。仮想ディスクトレイが開いている必要があります。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "シェーダー (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "現在選択中のシェーダーのオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "次のシェーダー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "「ビデオシェーダー」ディレクトリのルートにある次のシェーダープリセットをロードして適用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "前のシェーダー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "「ビデオシェーダー」ディレクトリのルートにある前のシェーダープリセットをロードして適用します。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "チート (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "現在選択されているチートのオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "次のチートインデックス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "現在選択されているのチートのインデックスを増やします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "前のチートインデックス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "現在選択されているのチートのインデックスを減らします。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "スクリーンショット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "現在のコンテンツの画像をキャプチャします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "録画 (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "現在のセッションの録画をローカルビデオファイルに開始/停止します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "配信 (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "現在のセッションの配信をオンライン動画プラットフォームに開始/停止します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "リプレイを再生"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "現在選択されているスロットからリプレイファイルを再生します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "リプレイを記録"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "現在選択されているスロットにリプレイファイルを記録します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "録画/リプレイを停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "現在のリプレイの録画/再生を停止します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "次のリプレイスロット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "現在選択されているリプレイスロットのインデックスを増やします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "前のリプレイスロット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "現在選択されているリプレイスロットのインデックスを減らします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "マウスの移動範囲をウィンドウ内に制限 (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "マウスの移動範囲をウィンドウ内に制限するかどうかを切り替えます。制限した場合、システムカーソルは非表示になり、移動範囲が RetroArch の表示ウィンドウ内に制限されます。マウス操作を行うゲームでの入力性が向上します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "ゲームフォーカス (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "[ゲームフォーカス] モードのオン/オフを切り替えます。コンテンツにフォーカスがある場合、ホットキーは無効になります (すべてのキーボード入力は実行中のコアに渡されます)。マウスは占有されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "フルスクリーン (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "フルスクリーンモードとウィンドウモードを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "デスクトップメニュー (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "コンパニオン WIMP (Windows, アイコン, メニュー, ポインタ) デスクトップユーザーインターフェースを開きます。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "正確なコンテンツフレームレートに同期 (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "正確なコンテンツフレームレートに同期のオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "先行実行 (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "先行実行のオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "先制フレーム (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "先制フレームのオン/オフを切り替えます。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "FPS を表示 (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "[1 秒ごとのフレーム数] ステータスインジケータのオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "技術統計を表示 (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "OSD 技術統計の表示のオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "キーボードオーバーレイ (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "キーボードオーバーレイのオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "次のオーバーレイ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "現在アクティブな OSD オーバーレイの次に利用可能なレイアウトに切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "AI サービス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "現在のコンテンツの画像をキャプチャし、画面上のテキストを翻訳および/または読み上げます。[AI サービス」 を有効にして設定する必要があります。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "ネットプレイ Ping (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "現在のネットプレイルームの Ping カウンターのオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "ネットプレイのホスティング (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "ネットプレイホスティングのオン/オフを切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "ネットプレイのプレイ/観戦モード (切り替え)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "現在のネットプレイセッションを [プレイ] と [観戦] モードで切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "ネットプレイプレイヤーチャット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "現在のネットプレイセッションにチャットメッセージを送信します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "ネットプレイチャットメッセージのフェードと固定を切り替えます。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "デバッグ情報の送信"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SEND_DEBUG_INFO,
   "お使いのデバイスと RetroArch の設定に関する診断情報を、分析のためにサーバーに送信します。"
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "デバイスの種類"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "エミュレートされるコントローラーの種類を指定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "アナログデジタル変換の種類"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "指定したアナログスティックを十字キー入力に使用します。[強制] モードはコアネイティブアナログ入力を上書きします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "指定したアナログ入力を十字キー入力にマップします。\nコアがネイティブアナログ入力に対応する場合、[強制] オプションを使用しない限り十字キーマッピングは無効になります。\n十字キーマッピングを強制した場合、コアは指定したスティックかのアナログ入力を受け取りません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "デバイス番号"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "RetroArch によって認識された物理コントローラーです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "このプレイヤーの予約デバイス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "このコントローラーは予約モードに応じてこのプレイヤーに割り当てられます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "予約なし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "優先"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "予約"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "デバイス予約タイプ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "優先: 指定されたデバイスが存在する場合、このプレイヤーに割り当てられます。予約: このプレイヤーには他のコントローラーが割り当てられません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "割り当てるポート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "どのコアポートがフロントエンドコントローラーポート %u からの入力を受け取るかを指定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "すべてのコントロールを設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "メニューに表示される順序で、すべての方向とボタンを次々に割り当てます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "デフォルトコントロールに戻す"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "入力割り当て設定をクリアし、デフォルトの値に戻します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "コントローラープロファイルを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "このコントローラーが再び検出された際に自動的に適用される自動設定ファイルを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "マウス番号"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "RetroArch によって認識された物理マウスです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "B ボタン (下)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Y ボタン (左)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "セレクトボタン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "スタートボタン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "十字キー 上"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "十字キー 下"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "十字キー 左"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "十字キー 右"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "A ボタン (右)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "X ボタン (上)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "L ボタン (ショルダー)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "R ボタン (ショルダー)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "L2 ボタン (トリガー)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2 ボタン (トリガー)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "L3 ボタン (親指)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "R3 ボタン (親指)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "左アナログ X+ (右)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "左アナログ X- (左)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "左アナログ Y+ (下)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "左アナログ Y- (上)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "右アナログ X+ (右)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "右アナログ X- (左)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "右アナログ Y+ (下)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "右アナログ Y- (上)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "ライトガン トリガー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "ライトガン リロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "ライトガン Aux A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "ライトガン Aux B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "ライトガン Aux C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "ライトガン スタート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "ライトガン セレクト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "ライトガン 十字キー 上"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "ライトガン 十字キー 下"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "ライトガン 十字キー 左"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "ライトガン 十字キー 右"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[先行実行を利用できません]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "現在のコアは確定的なステートセーブの対応がないため、先行実行との互換性がありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "先行実行するフレーム数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "先行実行するフレームの数です。ゲーム内部のラグフレーム数を超えると、ジッターなどのゲームプレイの問題が発生します。"
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "シングルインスタンスモード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "セカンドインスタンスモード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "先行実行の警告を隠す"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "先行実行を使用する際、コアがステートセーブに対応していない場合に表示される警告メッセージを非表示にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "先制フレーム数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "再実行するフレームの数です。ゲーム内部のラグフレームの数を越えた場合、ジッターなどのゲームプレイの問題が発生します。"
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "ハードウェア共有コンテキスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "ハードウェアレンダリングされたコアに独自のプライベートコンテキストを与えます。フレーム間でハードウェアの状態が変化することを想定しなくて済むようになります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "コアにビデオドライバの切り替えを許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "コアに現在ロードされているものとは異なるビデオドライバに切り替えることを許可します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "コアの終了時にダミーをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "一部のコアに実装されているシャットダウン機能の実行を、コアの終了時にダミーコアをロードすることで回避します。RetroArch の終了を防止できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "一部のコアにはシャットダウン機能が実装されています。このオプションを無効のままにすると、終了の手続きを選択したときに RetroArch の終了を引き起こします。\nこのオプションを有効にすることで、代わりにダミーコアがロードされ、RetroArch の終了を防止し、メニュー内に留まることができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "自動的にコアをスタート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "ロード前に不足しているファームウェアを確認"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "コンテンツをロードする前に、必要なファームウェアがすべて存在するかどうかを確認します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE,
   "一部のコアにはファームウェアや BIOS ファイルが必要なものがあります。このオプションを有効にすると、RetroArch は必須のファームウェアアイテムが不足している場合、コアの起動を許可しません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "コアオプションのカテゴリー化"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "コアオプションメニューを表示する際に、オプションをカテゴリー別に分類してサブメニュー化します。注意: 変更を適用するにはコアの再ロードが必要です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "コア情報ファイルをキャッシュ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "インストールされているコア情報の永続的なローカルキャッシュを維持します。ディスクアクセスが遅いプラットフォームでの読み込み時間を大幅に短縮します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "コア情報のステートセーブ機能の対応有無を無視"
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "コンテンツ実行時に常にコアを再ロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "要求されたコアがすでにロードされている場合でも、コンテンツを起動するときに RetroArch を再起動します。ロード時間が増加する代わりに、システムの安定性が向上する可能性があります。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "回転を許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "コアに回転設定の変更を許可します。無効にすると、回転の要求が無視されます。手動で画面を回転させるセットアップに便利です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "コアの管理"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "インストールされているコアのオフライン管理タスク (バックアップ, 復元, 削除など) およびコア情報の表示を行います。"
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "コアの管理"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Steam 経由で配布されているコアをインストールまたはアンインストールします。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "コアをインストール"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "コアをアンインストール"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "[コアの管理] を表示"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "メインメニューに [コアの管理] オプションを表示します。"
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "コアをインストール中: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "コアは RetroArch の終了時にアンインストールされます。"
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "コアは現在ダウンロード中です"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "終了時に設定を保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "終了時に設定ファイルに変更を保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "終了時に設定ファイルに変更を保存します。メニューで変更を行う際に便利です。設定ファイルは上書きされ、#include やコメントは保存されません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "終了時にリマップファイルを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "コアを閉じたり RetroArch を終了したりする際、アクティブな入力リマップファイルに変更を保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "自動的に優先コアオプションをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "起動時にカスタマイズされたコアオプションをロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "自動的に優先設定ファイルをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "起動時にカスタマイズされた設定をロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "自動的にリマップファイルをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "起動時にカスタマイズされたコントロールをロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "自動的に初期ディスク番号ファイルをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "ディスクが 2 枚以上あるコンテンツの開始時に、最後に使用したディスクをロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "自動的にシェーダープリセットをロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "グローバルコアオプションファイルを使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "すべてのコアオプションを共通の設定ファイル (retroarch-core-options.cfg) に保存します。無効にすると、各コアのオプションは RetroArch の [Configs] ディレクトリにあるコア固有のフォルダ/ファイルに分けて保存されます。"
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "コア名ごとにセーブをフォルダに分類"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "使用したコアに基づいた名前のフォルダにセーブファイルを振り分けます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "コア名ごとにステートセーブをフォルダに分類"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "使用したコアに基づいた名前のフォルダにステートセーブを振り分けます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "コンテンツディレクトリごとにセーブをフォルダに分類"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "コンテンツが存在するディレクトリに基づいた名前のフォルダにセーブファイルを振り分けます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "コンテンツディレクトリごとにステートセーブをフォルダに分類"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "コンテンツが存在するディレクトリに基づいた名前のフォルダにステートセーブを振り分けます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "ステートロード時に SaveRAM を上書きしない"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "ステートセーブをロードする際に SaveRAM の上書きをブロックします。ゲームに多数のバグが発生する可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "SaveRAM の自動保存間隔"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "不揮発性 SaveRAM を一定間隔 (秒) で自動的に保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "不揮発性 SRAM を一定間隔で自動保存します。特に設定しない限り、これはデフォルトで無効にされています。間隔は秒単位で表されます。0 の値は自動保存を無効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "リプレイチェックポイント間隔"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "リプレイの記録中にゲームのステートを自動的に一定間隔 (秒単位) でブックマークします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "リプレイの記録中にゲームステートを一定間隔で自動保存します。特に設定しない限り、これはデフォルトで無効にされています。間隔の単位は秒です。0 を指定すると、チェックポイント記録が無効になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "自動的にステートセーブインデックスを増やす"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "ステートセーブを作成する前に、ステートセーブインデックスが自動的に増加します。コンテンツをロードするとき、インデックスは既存の最も高いインデックスに設定されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "自動的にリプレイインデックスを増やす"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "リプレイを作成する前に、リプレイインデックスが自動的に増加します。コンテンツをロードするとき、インデックスは既存の最も高いインデックスに設定されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "自動ステートセーブの最大保存数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "[自動的にステートセーブインデックスを増やす] が有効になっている場合に作成されるステートセーブの数を制限します。新しくステートセーブを作成する際に制限を超えると、インデックスが最も低い既存のステートが削除されます。 [0] の値はステートが無制限に記録されることを意味します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "自動リプレイ数の上限"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "[自動的にリプレイインデックスを増やす] が有効になっている場合に作成されるリプレイ数の上限です。新しいリプレイを記録する際に上限を超えると、インデックスが最も低い既存のリプレイが削除されます。 [0] の値はリプレイが無制限に記録されることを意味します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "自動ステートセーブ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "自動ステートロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "起動時に自動ステートセーブを自動的にロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "ステートセーブサムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "メニューにステートセーブのサムネイルを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "SaveRAM 圧縮"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "不揮発性 SaveRAM ファイルを圧縮ファイル形式で書き込みます。保存/読み込み時間が増加する代わりに、ファイルサイズを大幅に削減します。\n標準 libretro SaveRAM インターフェースを介して保存を行うコアにのみ適用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "ステートセーブ圧縮"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "ステートセーブファイルを圧縮ファイル形式で書き込みます。保存/読み込み時間が増加する代わりに、ファイルサイズを大幅に削減します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "コンテンツディレクトリごとにスクリーンショットをフォルダに分類"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "コンテンツが存在するディレクトリに基づいた名前のフォルダにスクリーンショットを振り分けます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "コンテンツディレクトリにセーブファイルを書き込む"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "コンテンツディレクトリをセーブファイルのディレクトリとして使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "コンテンツディレクトリにステートセーブを書き込む"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "コンテンツディレクトリをステートセーブのディレクトリとして使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "コンテンツディレクトリからシステムファイルを読み込む"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "コンテンツディレクトリを システム/BIOS ディレクトリとして使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "コンテンツディレクトリにスクリーンショットを書き込む"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "コンテンツディレクトリをスクリーンショットのディレクトリとして使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "プレイ時間を保存 (コアごと)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "各コンテンツが実行された時間を、コアごとに分けて記録し追跡します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "プレイ時間を保存 (総計)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "各コンテンツが実行された時間を足し合わせ、すべてのコアの合計として記録し追跡します。"
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "ログの出力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "イベントをターミナルまたはファイルに記録します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "フロントエンドのログ出力レベル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "フロントエンドのログレベルを設定します。フロントエンドによって発行されたログレベルがこの値を下回る場合、それは無視されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "コアのログ出力レベル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "コアのログレベルを設定します。コアによって発行されたログレベルがこの値を下回る場合、それは無視されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "libretro コアのログレベル (GET_LOG_INTERFACE) を設定します。libretro コアによって発行されたログレベルが libretro_log レベル以下の場合、それは無視されます。詳細モードが有効になっていない限り、DEBUG ログは常に無視されます。\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR =3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (デバッグ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (情報)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (警告)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (エラー)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "ログのファイル出力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "システムイベントログメッセージをファイルにリダイレクトします。[ログの出力] を有効にする必要があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "タイムスタンプ付きのログファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "ファイルにログを出力する際、RetroArch の各セッションごとに新たなタイムスタンプ付きのファイルに出力します。無効にすると、ログは RetroArch を再起動するたび上書きされます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "パフォーマンスカウンター"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "RetroArch とコアのパフォーマンスカウンターです。カウンターデータはシステムのボトルネックを特定したり、パフォーマンスを微調整するのに役立ちます。"
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "隠しファイルとディレクトリを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "隠しファイルとディレクトリをファイルブラウザに表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "不明な拡張子を隠す"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "ファイルブラウザに表示されるファイルを対応する拡張子でフィルタリングします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "現在のコアでフィルター"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "最後に使用した開始ディレクトリを記憶"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "コンテンツをロードする際に、最後に使用したディレクトリでファイルブラウザを開きます。注意: 再起動すると場所はデフォルトにリセットされます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "内蔵メディアプレイヤーを使用"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "内蔵画像ビューアを使用"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "巻き戻し"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "巻き戻しの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "フレームタイムカウンター"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "フレームタイムカウンターに影響する設定を変更します。\nビデオのスレッド化が無効化されている場合にのみ有効になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "早送り倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "コンテンツを早送りする際の最大フレームレートです。例えば、60fps コンテンツに対して 5.0x を指定すると、最大フレームレートは 300fps 以下に制限されます。0.0x を指定すると、最大フレームレートは無制限になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "コンテンツを早送りする際の最大フレームレートです。例えば、60fps コンテンツに対して 5.0x を指定すると、最大フレームレートは 300fps 以下に制限されます。\nRetroArch は制限値を超えないように挙動を制御しますが、制限値を超過しないことを完全に保証するものではありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "早送りフレームスキップ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "早送り速度に応じてフレームをスキップします。これにより電力が節約され、サードパーティのフレーム制限を使用できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "スローモーション倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "スローモーションを使用する際のコンテンツの再生速度です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "メニューフレームレート制御"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "メニュー内でフレームレートが制限されていることを確認します。"
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "巻き戻し対応"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "ゲームプレイ中に行った操作を巻き戻します。プレイ中のパフォーマンスが大きく悪化します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "巻き戻しフレーム数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "ステップごとに巻き戻すフレーム数です。値を大きくするほど巻き戻し速度が増加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "巻き戻しバッファサイズ (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "巻き戻しバッファに確保するメモリ量 (MB 単位) です。この値を増やすと、巻き戻し履歴の量が増化します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "巻き戻しバッファサイズのステップ (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "巻き戻しバッファサイズの値が増加または減少するたびに、この量だけ変化します。"
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "早送り後にリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "早送り後にフレームタイムカウンターをリセットします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "ステートロード後にリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "ステートロード後にフレームタイムカウンターをリセットします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "ステートセーブ後にリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "ステートセーブ後にフレームタイムカウンターをリセットします。"
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "録画品質"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "カスタム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "低"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "中"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "高"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "ロスレス"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM 高速"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM 高品質"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "カスタム録画設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "録画スレッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "ポストフィルター録画を使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "フィルター (シェーダーは除く) を適用した後の画像をキャプチャします。ビデオは画面上で見るものと同じように派手なものになります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "GPU 録画を使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "可能な場合は GPU シェーディングされた素材の出力を録画します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "配信モード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "ローカル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "カスタム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "配信品質"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "カスタム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "低"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "中"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "高"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "カスタム配信設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "配信タイトル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "配信 URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "UDP 配信ポート"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "OSD オーバーレイ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "OSD キーボードオーバーレイを含む様々な OSD オーバーレイの設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "ビデオレイアウト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "ビデオレイアウトを調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "OSD 通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "OSD 通知の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "通知の表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "特定の種類の通知の表示/非表示を切り替えます。"
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "オーバーレイを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "オーバーレイは外枠や OSD コントロールに使用されます。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "メニューの背面にオーバーレイを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "オーバーレイをメニューの前面ではなく背面に表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "メニュー表示時にオーバーレイを隠す"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "メニュー内でオーバーレイを非表示にし、メニューを終了すると再び表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "コントローラー接続時にオーバーレイを隠す"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "物理コントローラがポート 1 に接続されているときにオーバーレイを非表示にし、コントローラーが切断されたときに再び表示します。"
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "物理コントローラーがポート 1 に接続されているときにオーバーレイを非表示にします。コントローラーが切断されても、オーバーレイは自動的に復元されません。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "オーバーレイに入力を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "登録された入力を OSD オーバーレイに表示します。[タッチ] ハイライトは押された/クリックされたオーバーレイ要素をハイライトします。[物理 (コントローラー)] ハイライトは、接続されたコントローラー/キーボードからコアに渡された実際の入力をハイライトします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "タッチ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "物理 (コントローラー)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "入力を監視するポート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "[オーバーレイに入力を表示] が [物理 (コントローラー)] に設定されているときに監視する入力デバイスのポートを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "オーバーレイにマウスカーソルを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "OSD オーバーレイを使用しているときにマウスカーソルを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "オーバーレイの自動回転"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "現在のオーバーレイでサポートされている場合、画面の向き/アスペクト比に合わせてレイアウトを自動的に回転させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "オーバーレイの自動スケーリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "画面のアスペクト比に合わせてオーバーレイの表示倍率と UI 要素の間隔を自動的に調整します。コントローラーオーバーレイで最も良い結果を生み出します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "十字キーの斜め感度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "対角線ゾーンのサイズを調整します。8 方向対称の場合は 100% に設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "ABXY オーバーラップ感度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "フェイスボタンのダイヤモンドのオーバーラップゾーンのサイズを調整します。8 方向対称の場合は 100% に設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "オーバーレイ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "優先オーバーレイを自動ロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "オーバーレイの不透明度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "オーバーレイの全 UI 要素の不透明度です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "オーバーレイプリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "ファイルブラウザからオーバーレイを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(横向き) オーバーレイ表示倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "横向きの表示方向を使用する際の全 UI 要素の表示倍率です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(横向き) オーバーレイアスペクト調整"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "横向きの表示方向を使用する際にオーバーレイに適用されるアスペクト比補正係数です。正の値はオーバーレイの幅を実質的に増加させ、負の値は幅を減少させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(横向き) オーバーレイ水平分離"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "現在のプリセットでサポートされている場合、横向きの表示方向を使用する際にオーバーレイの左半分と右半分にある UI 要素間の間隔を調整します。正の値は分離距離を増加させ、負の値は減少させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(横向き) オーバーレイ垂直分離"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "現在のプリセットでサポートされている場合、横向きの表示方向を使用する際にオーバーレイの上半分と下半分にある UI 要素間の間隔を調整します。正の値は分離距離を増加させ、負の値は減少させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(横向き) オーバーレイ X オフセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "横向きの表示方向を使用する際にオーバーレイに適用される水平オフセットです。正の値はオーバーレイを右方向に移動させ、負の値は左方向に移動させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(横向き) オーバーレイ Y オフセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "横向きの表示方向を使用する際にオーバーレイに適用される垂直オフセットです。正の値はオーバーレイを上方向に移動させ、負の値は下方向に移動させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(縦向き) オーバーレイ表示倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "縦向きの表示方向を使用する際の全 UI 要素の表示倍率です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(縦向き) オーバーレイアスペクト調整"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "縦向きの表示方向を使用する際にオーバーレイに適用されるアスペクト比補正係数です。正の値はオーバーレイの高さを実質的に増加させ、負の値は高さを減少させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(縦向き) オーバーレイ水平分離"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "現在のプリセットでサポートされている場合、縦向きの表示方向を使用する際にオーバーレイの左半分と右半分にある UI 要素間の間隔を調整します。正の値は分離距離を増加させ、負の値は減少させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(縦向き) オーバーレイ垂直分離"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "現在のプリセットでサポートされている場合、縦向きの表示方向を使用する際にオーバーレイの上半分と下半分にある UI 要素間の間隔を調整します。正の値は分離距離を増加させ、負の値は減少させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(縦向き) オーバーレイ X オフセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "縦向きの表示方向を使用する際にオーバーレイに適用される水平オフセットです。正の値はオーバーレイを右方向に移動させ、負の値は左方向に移動させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(縦向き) オーバーレイ Y オフセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "縦向きの表示方向を使用する際にオーバーレイに適用される垂直オフセットです。正の値はオーバーレイを上方向に移動させ、負の値は下方向に移動させます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "キーボードオーバーレイ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "キーボードオーバーレイを選択して調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "オーバーレイライトガン、マウスおよびポインタを有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "オーバーレイコントロールを押さない任意のタッチ入力を使用して、コアのポインティングデバイス入力を作成します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "オーバーレイライトガン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "オーバーレイから送信されるライトガン入力を設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "オーバーレイマウス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "オーバーレイから送信されるマウス入力を設定します。注意: 1、2、3 本指タップは左、右、中央ボタンクリックを送信します。"
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "キーボードオーバーレイプリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "ファイルブラウザからキーボードオーバーレイを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "キーボードオーバーレイの自動スケーリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "キーボードオーバーレイを元のアスペクト比に調整します。無効にすると画面にストレッチします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "キーボードオーバーレイの不透明度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "キーボードオーバーレイの全 UI 要素の不透明度です。"
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "ライトガンポート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "オーバーレイライトガンから入力を受け取るコアポートを設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "すべて"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "タッチでトリガー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "ポインタ入力でトリガー入力を送信します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "トリガー遅延 (フレーム単位)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "カーソルが移動する時間を確保するために、トリガー入力を遅延させます。この遅延はマルチフィンガータッチを正しく認識させるためのものでもあります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "2 タッチ入力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "2 つのポインタが画面上にある際に送信する入力を選択します。他の入力と区別するには、トリガー遅延を 0 以外にする必要があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "3 タッチ入力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "2 つのポインタが画面上にある際に送信する入力を選択します。他の入力と区別するには、トリガー遅延を 0 以外にする必要があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "4 タッチ入力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "4 つのポインタが画面上にある際に送信する入力を選択します。他の入力と区別するには、トリガー遅延を 0 以外にする必要があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "画面外入力を許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "画面外入力を許可します。無効にすると、画面外での入力がデバイスの端の内側に制限されます。"
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "マウス速度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "カーソルの移動速度を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "長押しでドラッグ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "画面を長押ししてボタンの長押しを開始します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "長押ししきい値 (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "長押しの検出に必要な時間を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "ダブルタップでドラッグ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "画面をダブルタップしてボタンの長押しを開始します。マウスクリックに遅延を追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "ダブルタップしきい値 (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "ダブルタップを検出する際のタップ間隔の許容時間を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "スワイプしきい値"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "長押しやタッチを検出する際に許容される移動範囲を調整します。画面の最小寸法に対するパーセンテージで表示されます。"
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "ビデオレイアウトを有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "ビデオレイアウトはベゼルなどのアートワークに使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "ビデオレイアウトのパス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "ファイルブラウザからビデオレイアウトを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "選択したビュー"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "ロードされたレイアウト内のビューを選択します。"
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "OSD 通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "グラフィックウィジェット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "装飾されたアニメーション、通知、インジケータおよびコントロールを使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "グラフィックウィジェットの自動スケーリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "現在のメニュー表示倍率に基づいて、装飾された通知、インジケータおよびコントロールのサイズを自動的に変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "グラフィックウィジェット表示倍率優先設定 (フルスクリーン)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "フルスクリーンモードで表示ウィジェットを描画する際に、手動で設定した表示倍率を適用します。[グラフィックウィジェットの自動スケーリング] が無効になっている場合にのみ適用されます。装飾された通知、インジケータおよびコントロールの表示倍率をメニューのそれから切り離して独立させ、拡大または縮小する場合に使用することができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "グラフィックウィジェット表示倍率優先設定 (ウィンドウ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "ウィンドウモードで表示ウィジェットを描画する際に、手動で設定した表示倍率を適用します。[グラフィックウィジェットの自動スケーリング] が無効になっている場合にのみ適用されます。装飾された通知、インジケータおよびコントロールの表示倍率をメニューのそれから切り離して独立させ、拡大または縮小する場合に使用することができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "フレームレートを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "1 秒ごとの現在のフレーム数を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "フレームレートの更新間隔 (フレーム数)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "指定したフレーム数ごとにフレームレートの表示が更新されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "フレームカウントを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "現在のフレームカウントを OSD に表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "統計を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "OSD 技術統計を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "メモリ使用量を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "システムの使用メモリ量と合計メモリ量を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "メモリ使用量の更新間隔 (フレーム単位)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "指定したフレーム数ごとにメモリ使用量の表示が更新されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "ネットプレイ Ping を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "現在のネットプレイルームの Ping を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "[コンテンツをロード] 起動時の通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "コンテンツのロード時に簡単な起動フィードバックアニメーションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "入力 (自動設定) 接続通知"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "チートコードの通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "チートコードの適用時に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "パッチ通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "ROM のソフトパッチ時に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "入力デバイスの接続/切断時に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "入力リマップロード時の通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "入力リマップファイルのロード時に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "優先設定ロード通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "優先設定ファイルのロード時に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "初期ディスク復元通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "ディスクが 2 枚以上あるコンテンツを M3U プレイリストを使用してロードし、最後に使用したディスクが自動的に復元される際に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "ディスクコントロール通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "ディスクを挿入または取り出した時に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "ステートセーブ通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "ステートセーブの保存とロード時に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "早送り時の通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "コンテンツの早送り時に OSD インジケータを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "スクリーンショット通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "スクリーンショットの撮影時に OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "スクリーンショット通知の持続時間"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "OSD スクリーンショットメッセージの表示時間を定義します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "通常"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "高速"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "非常に速い"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "即時"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "スクリーンショットのフラッシュ効果"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "スクリーンショットの撮影時に、指定された表示時間で白い点滅エフェクトを画面に表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "オン (通常)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "オン (高速)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "リフレッシュレート通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "リフレッシュレートを設定したときに OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "追加のネットプレイ通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "重要ではないネットプレイ OSD メッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "メニューでのみ通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "メニューが開いているときにのみ通知を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "通知のフォント"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "OSD 通知のフォントを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "通知のサイズ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "通知の表示位置 (水平)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "OSD テキストのカスタム X 軸位置を指定します。0 は左端です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "通知の表示位置 (垂直)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "OSD テキストのカスタム Y 軸位置を指定します。0 は下端です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "通知の色 (赤)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "OSD テキスト色の赤の値を設定します。有効な値は 0 から 255 の間です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "通知の色 (緑)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "OSD テキスト色の緑の値を設定します。有効な値は 0 から 255 の間です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "通知の色 (青)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "OSD テキスト色の青の値を設定します。有効な値は 0 から 255 の間です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "通知の背景"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "OSD の背景色を有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "通知の背景色 (赤)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "OSD 背景色の赤の値を設定します。有効な値は 0 から 255 の間です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "通知の背景色 (緑)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "OSD 背景色の緑の値を設定します。有効な値は 0 から 255 の間です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "通知の背景色 (青)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "OSD 背景色の青の値を設定します。有効な値は 0 から 255 の間です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "通知の背景色の不透明度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "OSD 背景色の不透明度を設定します。有効な値は 0.0 から 1.0 の間です。"
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "メニュー項目の表示/非表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "RetroArch のメニュー項目の表示/非表示を切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "外観"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "メニュー画面の外観の設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "アプリアイコン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "アプリアイコンを変更します。"
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "3DS 下画面の外観"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "下画面の外観の設定を変更します。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "詳細設定を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "パワーユーザー向けの高度な設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "キオスクモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "設定に関連するすべての設定を隠すことでセットアップを保護します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "キオスクモードを無効にするパスワードを設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "キオスクモードを有効にする際にあらかじめパスワードを設定しておくことで、後でメニューからキオスクモードを無効にすることができます。無効にするには、メインメニューを開き、[キオスクモードを無効にする] でそのパスワードを入力します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "ナビゲーションの回り込み"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "リストの終端に水平または垂直に達したときに先頭/末尾に折り返します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "メニュー表示時にコンテンツを一時停止"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "ステートセーブ後にコンテンツを再開"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "ステートセーブまたはロード後に自動的にメニューを閉じてコンテンツを再開します。これを無効にすると、非常に遅いデバイスでのステートセーブのパフォーマンスを向上させることができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "ディスク入れ替え後にコンテンツを再開"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "新しいディスクが挿入またはロードされた際に、自動的にメニューを閉じてコンテンツを再開します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "コンテンツを閉じるで終了"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "コンテンツを閉じたときに RetroArch を自動的に終了します。「CLI」が終了するのはコンテンツがコマンドライン経由で起動された場合のみです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "メニュースクリーンセーバーのタイムアウト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "メニューを表示している間、指定した非アクティブ期間の後にスクリーンセーバーが表示されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "メニュースクリーンセーバーのアニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "メニュースクリーンセーバーがアクティブである間、アニメーション効果を有効にします。パフォーマンスに影響を与えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "メニュースクリーンセーバーのアニメーション速度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "メニュースクリーンセーバーのアニメーション効果の速度を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "マウス対応"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "メニューをマウスで操作できるようにします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "タッチ対応"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "メニューをタッチスクリーンで操作できるようにします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "タスクのスレッド化"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "タスクを別のスレッドで実行します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "非アクティブ時にコンテンツを一時停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "RetroArch がアクティブウィンドウではないときにコンテンツを一時停止します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "デスクトップコンポジションを無効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "ウィンドウマネージャーはコンポジションを使用して、視覚効果を適用したり、応答しないウィンドウを検出したりします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "コンポジションを強制的に無効にします。無効化は現在 Windows Vista/7 でのみ有効です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "メニュースクロール加速度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "一定方向にスクロールし続けたときのカーソルの最大速度です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "メニュースクロール遅延"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "一定方向にスクロールし続けるときの初期遅延 (ms) です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "UI コンパニオン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "起動時に UI コンパニオンを開始"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "起動時にユーザーインターフェースコンパニオンドライバを開始します (利用可能な場合)。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "デスクトップメニュー (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "起動時にデスクトップメニューを開く"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "クイックメニュー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "クイックメニューのメニュー項目の表示/非表示を切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "設定メニューの項目の表示/非表示を切り替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "[コアをロード] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "メインメニューに [コアをロード] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "[コンテンツをロード] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "メインメニューに [コンテンツをロード] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "[ディスクをロード] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "メインメニューに [ディスクをロード] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "[ディスクをダンプ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "メインメニューに [ディスクをダンプ] オプションを表示します。"
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "[ディスクの取り出し] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "メインメニューに [ディスクの取り出し] を表示します。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "[オンラインアップデータ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "メインメニューに [オンラインアップデータ] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "[コアダウンローダ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "[オンラインアップデータ] オプションで、コア (およびコア情報ファイル) を更新する機能を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "古い [サムネイルアップデータ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "[オンラインアップデータ] オプションに、レガシーサムネイルパッケージをダウンロードするための項目を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "[情報] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "メインメニューに [情報] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "[設定ファイル] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "メインメニューに [設定ファイル] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "[ヘルプ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "メインメニューに [ヘルプ] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "[終了] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "メインメニューに [終了] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "[再起動] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "メインメニューに [再起動] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "[設定] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "[設定] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "[設定] を有効にするためのパスワードを設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "設定タブを隠す際にあらかじめパスワードを設定しておくことで、後でメニューから設定タブを復元することができます。設定タブを復元するには、メインメニュータブを開き、[設定タブを有効にする] でそのパスワードを入力します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "[お気に入り] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "[お気に入り] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "[画像] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "[画像] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "[音楽] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "[音楽] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "[動画] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "[動画] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "[ネットプレイ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "[ネットプレイ] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "[履歴] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "[履歴] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "[コンテンツをインポート] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "[コンテンツをインポート] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH( /* FIXME can now be replaced with MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD */
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "[コンテンツをインポート] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "メインメニューとプレイリストのサブメニューに [コンテンツをインポート] メニューを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "メインメニュー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "プレイリストメニュー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "[プレイリスト] を表示"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "[エクスプローラー] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "[エクスプローラー] メニューを表示します。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "[コンテンツレスコア] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "[コンテンツレスコア] メニューに表示するコアの種類を指定します。「カスタム」に設定すると、[コアの管理] メニューから個々のコアの表示/非表示を切り替えることができます。 (Ozone/XMB で再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "すべて"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "単独使用"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "カスタム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "日付と時刻を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "メニューに現在の日付と時刻を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "日付と時刻の表示形式"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "メニュー内での日付/時刻の表示形式を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "日付の区切り文字"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "現在の日付がメニュー内に表示されるときに, 年/月/日の区切りとして使用する文字を指定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "バッテリー残量を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "メニュー内に現在のバッテリー残量を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "コア名を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "メニュー内に現在のコア名を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "メニューのサブラベルを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "メニュー項目の追加情報を表示します。"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "スタート画面を表示"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "メニューにスタート画面を表示します。このオプションは、プログラムを初めて起動した後に自動的にオフに設定されます。"
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "[再開] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "コンテンツの [再開] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "[再起動] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "コンテンツの [再起動] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "[コンテンツを閉じる] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "[コンテンツを閉じる] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "[ステートセーブ] サブメニューを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "サブメニューに [ステートセーブ] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "[ステートセーブ/ロード] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "[ステートセーブ/ロード] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "[リプレイコントロール] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "リプレイファイルを録画/再生するためのオプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "[ステートセーブ/ロードを元に戻す] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "[ステートロード/セーブを元に戻す] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "[コアオプション] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "[コアオプション] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "[オプションを強制的にディスクに保存] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "[オプション > コアオプションの管理] メニューに [オプションを強制的にディスクに保存] エントリーを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "[コントロール] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "[コントロール] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "[スクリーンショットを撮影] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "[スクリーンショットを撮影] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "[録画を開始] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "[録画を開始] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "[配信を開始] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "[配信を開始] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "[OSD オーバーレイ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "[OSD オーバーレイ] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "[ビデオレイアウト] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "[ビデオレイアウト] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "[レイテンシ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "[レイテンシ] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "[巻き戻し] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "[巻き戻し] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "[コア優先設定を保存] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "[優先設定] メニューに [コア優先設定を保存] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "[コンテンツディレクトリ優先設定を保存] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "[優先設定] メニューに [コンテンツディレクトリ優先設定を保存] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "[ゲーム優先設定を保存] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "[優先設定] メニューに [ゲーム優先設定を保存] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "[チート] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "[チート] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "[シェーダー] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "[シェーダー] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "[お気に入りに追加] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "[お気に入りに追加] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "[プレイリストに追加] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "[プレイリストに追加] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "[コアの関連付けを設定] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "コンテンツが実行されていないときに [コアの関連付けを設定] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "[コアの関連付けをリセット] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "コンテンツが実行されていないときに [コアの関連付けをリセット] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "[サムネイルをダウンロード] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "コンテンツが実行されていないときに [サムネイルをダウンロード] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "[情報] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "[情報] オプションを表示します。"
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "[ドライバ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "[ドライバ] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "[ビデオ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "[ビデオ] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "[オーディオ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "[オーディオ] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "[入力] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "[入力] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "[レイテンシ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "[レイテンシ] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "[コア] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "[コア] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "[設定] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "[設定] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "[保存] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "[保存] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "[ログ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "[ログ] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "[ファイルブラウザ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "[ファイルブラウザ] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "[フレーム制御] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "[フレーム制御] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "[録画] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "[録画] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "[OSD] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "[OSD] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "[ユーザーインターフェース] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "[ユーザーインターフェース] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "[AI サービス] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "[AI サービス] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "[ユーザー補助] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "[ユーザー補助] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "[電源管理] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "[電源管理] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "[実績] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "[実績] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "[ネットワーク] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "[ネットワーク] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "[プレイリスト] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "[プレイリスト] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "[ユーザー] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "[ユーザー] 設定を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "[ディレクトリ] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "[ディレクトリ] 設定を表示します。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "[Steam] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "[Steam] 設定を表示します。"
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "表示倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "メニューのユーザーインターフェース要素のサイズの表示倍率です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "背景画像"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "メニューの背景として設定する画像を選択します。手動およびダイナミック画像は [カラーテーマ] を上書きします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "壁紙の不透明度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "背景画像の不透明度を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "不透明度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "デフォルトのメニュー背景の不透明度を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "システムのカラーテーマを優先する"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "利用可能な場合、OS のカラーテーマを使用します。テーマの設定を上書きします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "プライマリーサムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "表示するサムネイルの種類です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "サムネイル拡大しきい値"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "指定された値より小さい幅/高さの画像を自動的に拡大し、画質を改善します。パフォーマンスにやや影響します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "ティッカーテキストのアニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "長いメニューテキストを表示するために使用される水平スクロール方式を選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "ティッカーテキストの速度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "長いメニューテキストをスクロールする際のアニメーション速度です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "ティッカーテキストのスムージング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "長いメニューテキストを表示中にスムーズスクロールアニメーションを使用します。パフォーマンスに少し影響します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "タブを変更したときの選択を記憶"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "タブ内の以前のカーソル位置を記憶します。RGUI にタブはありませんが、プレイリストや設定はそのように動作します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "常に"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "プレイリストのみ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "メインメニューと設定のみ"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "AI サービス出力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "テキストオーバーレイ (画像モード)、テキスト読み上げ (音声)、または NVDA (ナレーター) のようなスクリーンリーダーのいずれかで翻訳を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "AI サービス URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "利用する翻訳サービスが指示している http:// URL です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "AI サービスを有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "AI サービスのホットキーを押したときに AI サービスの実行を有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "翻訳中に一時停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "画面の翻訳中にコアを一時停止します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "翻訳対象の言語"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "サービスが翻訳する対象の言語です。[デフォルト] に設定すると、言語の自動検出を試みます。特定の言語に設定すると、翻訳がより正確になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "翻訳後の言語"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "サービスが翻訳した後の言語です。[デフォルト] は英語です。"
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "ユーザー補助を有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "メニューナビゲーションを支援するためにテキスト読み上げを有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "テキスト読み上げ速度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "テキスト読み上げ音声の速度です。"
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "実績"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "クラシックゲームで実績を獲得しましょう。詳細については、「https://retroachievements.org」をご覧ください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "ハードコアモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "チート、巻き戻し、スローモーション、ステートセーブのロードを無効にします。ハードコアモードで獲得した実績は、エミュレータの支援機能なしで達成したことを他の人に示すことができるように独自にマークされています。実行時にこの設定を切り替えるとゲームが再起動します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "ランキング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "定期的に状況に応じたゲーム情報を RetroArchievements のウェブサイトに送信します。[ハードコアモード] が有効になっている場合は効果はありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "実績バッジ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "実績リストにバッジを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "非公式実績をテスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "非公式実績/ベータ機能をテスト目的で有効または無効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "ロック解除音"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "実績を解除したときにサウンドを再生します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "自動スクリーンショット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "実績を獲得したときに自動的にスクリーンショットを撮影します。"
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "アンコールモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "すべての実績がアクティブになった状態でセッションを開始します (以前に解除されたものも)。"
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "外観"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "OSD 実績通知の位置とオフセットを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "位置"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "実績通知を表示する画面の隅/端を設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "左上"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "中央上"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "右上"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "左下"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "中央下"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "右下"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "余白の位置合わせ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "実績通知を他のタイプの OSD 通知と整列させるかどうかを設定します。手動で余白/位置の値を設定する場合は無効にしてください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "手動水平余白"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "画面の左/右端からの距離で、ディスプレイのオーバースキャンを補正できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "手動垂直余白"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "画面の上/下端からの距離で、ディスプレイのオーバースキャンを補正できます。"
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "公開範囲"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "どのメッセージや OSD 要素を表示するかを変更します。機能を無効にしません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "スタートアップ概要"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "ロード中のゲームとユーザーの現在の進捗状況についての情報を表示します。\n[すべての認識されたゲーム] は実績が公開されていないゲームの概要を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "すべての特定されたゲーム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "実績のあるゲーム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "解除通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "実績を解除したときに通知を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "マスタリー通知"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "ゲームのすべての実績を解除したときに通知を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "アクティブチャレンジインジケータ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "特定の実績が獲得できる間、OSD インジケータを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "進捗インジケータ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "特定の実績に向けて進捗があったとき、OSD インジケータを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "リーダーボード開始メッセージ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "リーダーボードがアクティブになったときにリーダーボードの説明を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "リーダーボード送信メッセージ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "リーダーボードの試行が完了したときに送信された値のメッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "リーダーボード失敗メッセージ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "リーダーボードの試行が失敗したときにメッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "リーダーボードトラッカー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "アクティブなリーダーボードの現在の値を画面上のトラッカーに表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "ログインメッセージ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "RetroAchievements アカウントのログインに関連するメッセージを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "詳細メッセージ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "追加の診断とエラーメッセージを表示します。"
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "ネットプレイの一般公開"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "ゲームのネットプレイを一般公開するかどうかを選択します。設定しない場合、クライアントは手動接続が必要になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "中継サーバーを使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "中継サーバーを使用してネットプレイ接続を転送します。ホストがファイアウォールの内側にある場合や、NAT/UPnP の問題がある場合に役立ちます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "中継サーバーの場所"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "使用する特定の中継サーバーを選択します。地理的に近い位置であれば低遅延となる傾向があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "カスタム中継サーバーアドレス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "ここにカスタム中継サーバーのアドレスを入力してください。形式: アドレスまたはアドレス|ポート。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "北米 (東海岸, アメリカ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "西ヨーロッパ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "南アメリカ (南東, ブラジル)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "東南アジア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_5,
   "東アジア（韓国・春川市）"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "カスタム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "サーバー IP アドレス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "接続するホストのアドレスです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "ネットプレイ TCP ポート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "ホスト IP アドレスのポートです。TCP または UDP のいずれかを指定できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "最大同時接続数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "新しい接続を拒否する前にホストが受け入れるアクティブな接続の最大数です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Ping 制限"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "ホストが受け入れる最大接続遅延 (ping) です。無制限にする場合は 0 に設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "サーバーパスワード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "ホストに接続するためにクライアントが使用するパスワードです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "サーバー観戦専用パスワード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "観客としてホストに接続するためにクライアントが使用するパスワードです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "ネットプレイ観戦モード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "観戦モードでネットプレイを開始します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "観戦モードでネットプレイを開始するかどうかを選択します。はいに設定すると、ネットプレイは開始時に観戦モードになります。後でモードをいつでも変更できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "チャットをフェード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "時間の経過とともにチャットメッセージをフェードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "チャットの色 (ニックネーム)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "形式: #RRGGBB または RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "チャットの色 (メッセージ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "形式: #RRGGBB または RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "一時停止を許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "ネットプレイ中にプレイヤーが一時停止することを許可します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "スレーブモードのクライアントを許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "スレーブモードでの接続を許可します。スレーブモードのクライアントはどちらの側でもほとんど処理能力を必要としませんが、ネットワークレイテンシに大きな影響を受けます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "スレーブモードのクライアント以外を拒否"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "スレーブモードでの接続を禁止します。非常に弱いマシンがある非常に高速なネットワーク以外では推奨されません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "ネットプレイチェックフレーム数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "ネットプレイ時にホストとクライアントが同期しているがどうかを確認する頻度 (フレーム数) です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "ネットプレイがホストとクライアントの同期を確認するフレームの頻度です。ほとんどのコアで、この値は目に見える効果はなく無視できます。非確定的なコアでは、この値はネットプレイピアが同期する頻度を決定します。バグがあるコアでは、この値を 0 以外の値に設定すると深刻なパフォーマンスの問題が発生します。チェックを行わない場合は 0 に設定します。[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "入力遅延フレーム数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "ネットワーク遅延を隠すために使用する、入力遅延のフレーム数です。顕著な入力遅延を犠牲にして、ジッターとネットプレイ中の CPU 使用率を軽減します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "ネットワークレイテンシを隠すために使用するネットプレイの入力レイテンシのフレーム数です。\nネットプレイでは、このオプションはローカル入力を遅らせ、実行中のフレームをネットワークから受信するフレームに近づけます。これはジッターを減らし、ネットプレイの CPU 負荷を減らしますが、代償として顕著な入力ラグが発生します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "入力遅延フレーム数範囲"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "ネットワーク遅延を隠すために使用できる入力遅延のフレームの範囲です。予測不可能な入力遅延を犠牲にして、ジッターとネットプレイ中の CPU 使用率を軽減します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "ネットワークレイテンシを隠すためにネットプレイが使用できる入力レイテンシフレームの範囲です。\n設定した場合、ネットプレイは入力レイテンシのフレーム数を動的に調整し、CPU 時間、入力レイテンシ、ネットワークレイテンシのバランスを取ります。これはジッターを軽減し、ネットプレイの CPU 集約を減らしますが、代償として予測不可能な入力ラグが発生しま[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "ネットプレイ NAT 通過"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "ホストする場合、 LAN から逃れるために UPnP またはそれに類似した技術を使用し、パブリックインターネットからの接続のリッスンを試みます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "デジタル入力を共有"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "デバイス %u を要求する"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "指定された入力デバイスでのプレイを要求します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "ネットワークコマンド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "ネットワークコマンドのポート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "ネットワークレトロパッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "ネットワークレトロパッドのベースポート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "ユーザー %d ネットワークのレトロパッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "stdin コマンド"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "stdin コマンドインターフェースです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "オンデマンドサムネイルダウンロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "プレイリストの閲覧中、不足しているサムネイルを自動的にダウンロードします。パフォーマンスに大きな影響があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "アップデータ設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "コアアップデータ設定にアクセス"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Buildbot コア URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "Libretro buildbot のコアアップデータディレクトリを示す URL です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Buildbot アセット URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "Libretro buildbot のアセットアップデータディレクトリを示す URL です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "ダウンロードした圧縮ファイルを自動的に展開"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "ダウンロード後、ダウンロードした圧縮ファイルに含まれるファイルを自動的に展開します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "実験的なコアを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "コアダウンローダリストに「実験的な」コアを含めます。これらは通常、開発/テストのみを目的としており、一般的な使用には推奨されません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "更新時にコアをバックアップ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "オンラインアップデートを実行する際に、インストールされているコアのバックアップを自動的に作成します。更新で問題が発生した場合、動作するコアに簡単にロールバックできるようになります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "コアバックアップ履歴の上限"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "インストールされているコアごとに自動生成されるバックアップの数を指定します。この制限に達すると、オンラインアップデートを介して新しいバックアップを作成したときに最も古いバックアップが削除されます。手動でのコアバックアップはこの設定の影響を受けません。"
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "履歴"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "最近使用したゲーム、画像、音楽および動画のプレイリストを管理します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "履歴の上限"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "最近使用したゲーム、画像、音楽および動画のプレイリストの最大エントリー数です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "お気に入りの上限"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "[お気に入り] プレイリストの最大エントリー数です。上限に達すると、古いエントリーが削除されるまで新しいエントリーが追加されなくなります。値を [-1] に設定すると [無制限] を許可します。\n警告: 値を減らすと既存のエントリーが削除されます!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "エントリーの名前の変更を許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "プレイリストエントリーの名前の変更を許可します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "エントリーの削除を許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "プレイリストエントリーの削除を許可します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "アルファベット順に並び替え"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "[履歴]、[画像]、[音楽] および [動画] プレイリストを除くコンテンツプレイリストをアルファベット順に並べ替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "プレイリストを古い形式で保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "非推奨のプレーンテキスト形式でプレイリストを書き込みます。無効にすると、プレイリストは JSON を使用してフォーマットされます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "プレイリストを圧縮"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "ディスクへ書き込む際にプレイリストデータを圧縮します。CPU 使用率が (わずかに) 上昇する代わりに、ファイルサイズと読み込み時間を削減します。プレイリストの形式 (古い, 新しい) を問わず使用できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "関連付けられたコアを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "プレイリストエントリーに、現在関連付けられているコア (もしあれば) をタグ付けするタイミングを指定します。\nプレイリストのサブラベルが有効になっている場合、この設定は無視されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "プレイリストのサブラベルを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "現在のコアの関連付けやプレイ時間など、各プレイリストエントリーの追加情報を表示します (利用可能な場合)。パフォーマンスに影響します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "履歴とお気に入りにコンテンツ固有のアイコンを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "履歴やお気に入りプレイリストのエントリーごとに固有のアイコンを表示します。パフォーマンスに影響します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "コア:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "プレイ時間:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "最終プレイ日時:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "秒"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "秒"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "分"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "分"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "時間"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "時間"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "日"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "日"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "週"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "週"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "月"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "月"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "年"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "年"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "前"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "プレイリストエントリー番号を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "プレイリストを表示する際にエントリー番号を表示します。表示形式は現在選択されているメニュードライバに依存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "プレイリストのサブラベルのプレイ時間"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "サブラベルに表示するプレイ時間の種類を選択します。\n対応するプレイ時間を [保存] オプションメニューで有効にする必要があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "[最終プレイ日時] の日付と時刻の形式"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "[最終プレイ日時] 情報を表示する日付と時刻の形式を指定します。[(午前/午後)] オプションは、一部のプラットフォームでパフォーマンスに小さな影響があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "圧縮ファイルのあいまいマッチング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "圧縮ファイルに関連付けられたエントリーをプレイリストで検索する際、[ファイル名] + [コンテンツ] の代わりに圧縮ファイル名のみを一致させます。有効にすると、圧縮ファイルをロードする際にコンテンツ履歴のエントリーの重複を回避することができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "スキャン時にコアのマッチングをしない"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "対応するコアがインストールされていないコンテンツをスキャンし、プレイリストに追加することを許可します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "特に PSP/PSN タイトルで、ISO が重複するシリアルを持つ場合があります。シリアルにのみ依存すると、スキャナーがコンテンツを間違ったシステムに入れてしまうことがあります。これにより CRC チェックが追加され、スキャンが大幅に遅くなりますが、より正確になる可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "プレイリストの管理"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "プレイリストのメンテナンスタスクを実行します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "ポータブルプレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "有効にすると, 'ファイルブラウザ'ディレクトリも選択され, 現在の'ファイルブラウザ'の値がプレイリストに保存されます. 同じオプションが有効な別のシステムにプレイリストがロードされている場合, 'ファイルブラウザ'の値とプレイリストの値を比較します. それらが異なる場合, プレイリスト項目のパスは自動的に修正されます."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "サムネイルマッチングにファイル名を使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "有効にすると、ラベルではなくエントリーのファイル名でサムネイルを検索します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "サムネイル用の画像にすべての拡張子を含めることを許可する"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "有効にすると、RetroArch が対応するすべての画像 (jpeg など) をローカルサムネイルとして追加できます。パフォーマンスに影響を与える可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "管理"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "デフォルトのコア"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "コアの関連付けが存在しないプレイリストエントリーからコンテンツを起動する際に使用するコアを指定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "コアの関連付けをリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "すべてのプレイリストエントリーの既存のコアの関連付けを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "ラベルの表示モード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "プレイリスト内でのコンテンツラベルの表示方式を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "並べ替え方法"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "このプレイリストでどのようにエントリーを並べ替えるかを決定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "プレイリストをクリーニング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "コアの関連付けを検証し、無効または重複したエントリーを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "プレイリストをリフレッシュ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "プレイリストの作成または編集で最後に使用した [手動スキャン] 操作を繰り返すことで、新しいコンテンツを追加したり無効なエントリーを削除したりします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "プレイリストを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "ファイルシステムからプレイリストを削除します。"
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "プライバシー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "プライバシー設定を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "アカウント"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "現在設定されているアカウントを管理します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "ユーザー名"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "名前を入力してください。ネットプレイセッションなどで使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "言語"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "ユーザーインターフェースの言語を設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "ここで選択された言語に応じて、メニューと画面上のすべてのメッセージをローカライズします。変更を適用するには再起動が必要です。\n各オプションの横に翻訳の達成度が表示されます。メニュー項目に言語が実装されていない場合は、英語にフォールバックします。"
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "カメラを許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "コアにカメラのアクセスを許可します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Discord アプリにプレイされたコンテンツについての情報を表示することを許可します。\nネイティブデスクトップクライアントでのみ利用可能です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "位置情報を許可"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "コアに位置情報のアクセスを許可します。"
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "クラシックゲームで実績を獲得しましょう。詳細については、「https://retroachievements.org」をご覧ください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "RetroAchievements のログイン詳細です。RetroAchievements にアクセスして、無料アカウントにサインアップしてください。\n登録が完了したら、ユーザー名とパスワードを RetroArch に入力する必要があります。"
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "ユーザー名"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "RetroAchievements のアカウントユーザー名を入力してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "パスワード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "RetroAchievements アカウントのパスワードを入力してください。最大長：255 文字。"
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "YouTube ストリームキー"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Twitch 配信キー"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Facebook Gaming ストリームキー"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "システム/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOS、ブート ROM およびその他のシステム固有のファイルなどはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "ダウンロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "ダウンロードしたファイルはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "アセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "RetroArch が使用するメニューアセットはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "ダイナミック壁紙"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "メニュー内で使用される背景画像はこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "サムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "ボックスアート、スクリーンショットおよびタイトル画面のサムネイルはこのディレクトリに保存されます。"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "開始ディレクトリ"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "ファイルブラウザの開始ディレクトリを設定します。"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "設定ファイル"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "デフォルトの設定ファイルはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "コア"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Libretro コアはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "コア情報"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "アプリケーション/コア情報ファイルはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "データベース"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "データベースはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "チートファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "チートファイルはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "ビデオフィルター"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "CPU ベースのビデオフィルターはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "オーディオフィルター"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "オーディオ DSP フィルターはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "ビデオシェーダー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "GPU ベースのビデオフィルターはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "録画"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "録画はこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "録画設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "録画設定はこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "オーバーレイ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "オーバーレイはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "キーボードオーバーレイ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "キーボードオーバーレイはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "ビデオレイアウト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "ビデオレイアウトはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "スクリーンショット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "スクリーンショットはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "コントローラープロファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "コントローラーを自動設定するためのコントローラープロファイルはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "入力リマップ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "入力リマップはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "プレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "プレイリストはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "お気に入りプレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "お気に入りプレイリストはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "履歴プレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "履歴プレイリストはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "画像プレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "画像履歴プレイリストはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "音楽プレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "音楽プレイリストはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "動画プレイリスト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "動画プレイリストはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "実行時ログ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "実行時ログはこのディレクトリに保存されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "セーブファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "このディレクトリにすべてのセーブファイルを保存します。設定されていない場合は、コンテンツファイルの作業ディレクトリに保存を試みます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "このディレクトリにすべてのセーブファイル (*.srm) を保存します。これには .rt、.psrm などの関連ファイルが含まれます。これは明示的なコマンドラインオプションで上書きされます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "ステートセーブ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "ステートセーブとリプレイはこのディレクトリに保存されます。設定されていない場合、コンテンツがあるディレクトリに保存を試みます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "キャッシュ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "圧縮ファイル内のコンテンツは一時的にこのディレクトリに展開されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "システムイベントログ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "システムイベントログはこのディレクトリに保存されます。"
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Rich Presence を有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Steam の RetroArch で現在の状態を共有します。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Rich Presence コンテンツ形式"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "コンテンツ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "コア名"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "システム名"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "コンテンツ (システム名)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "コンテンツ (コア名)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "コンテンツ (システム名 - コア名)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "ミキサーに追加"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "このオーディオトラックを利用可能なオーディオストリームスロットに追加します。\n現在利用可能なスロットがない場合は無視されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "ミキサーに追加して再生"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "このオーディオトラックを利用可能なオーディオストリームスロットに追加して再生します。\n現在利用可能なスロットがない場合は無視されます。"
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "ホスト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "ネットプレイホストに接続"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "ネットプレイサーバーアドレスを入力し、クライアントモードで接続します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "ネットプレイホストから切断"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "アクティブなネットプレイ接続を切断します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "ロビーフィルター"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "接続可能な部屋のみ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "インストール済みのコアのみ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "パスワード付きルーム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "ネットプレイホスト一覧を更新"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "ネットプレイホストをスキャンします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "ネットプレイ LAN 一覧を更新"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "LAN 上のネットプレイホストをスキャンします。"
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "ネットプレイホストを開始"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "ホスト (サーバー) モードでネットプレイを開始します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "ネットプレイホストを停止"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "クライアントをキック"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "現在ホストしている部屋からクライアントをキックします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "クライアントを BAN"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "現在ホストしている部屋からクライアントを BAN します。"
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "ディレクトリをスキャン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "データベースと一致するコンテンツがあるかディレクトリをスキャンします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<このディレクトリをスキャン>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "これを選択すると、現在のディレクトリのコンテンツがスキャンされます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "ファイルをスキャン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "データベースと一致するコンテンツがあるかファイルをスキャンします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "手動スキャン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "コンテンツのファイル名に基づいて設定可能なスキャンです。コンテンツがデータベースと一致する必要はありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "スキャン"
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "ミキサーに追加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "ミキサーに追加して再生"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "コンテンツディレクトリ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "コンテンツをスキャンするディレクトリを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "システム名"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "スキャンしたコンテンツに関連付ける [システム名] を指定します。生成されたプレイリストファイルの名前およびプレイリストサムネイルの識別に使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "カスタムシステム名"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "スキャンしたコンテンツの [システム名] を手動で指定します。[システム名] が [カスタム] に設定されている場合にのみ使用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "デフォルトのコア"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "スキャンしたコンテンツを起動する際に使用するデフォルトのコアを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "ファイル拡張子"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "スキャンに含めるファイルの種類の一覧です。スペースで区切ってください。空欄にすると、すべての種類のファイルを含めます。コアが指定されている場合は、コアによってサポートされているすべてのファイルを含めます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "再帰的にスキャン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "有効にすると、指定された [コンテンツディレクトリ] のすべてのサブディレクトリがスキャンに含まれます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "圧縮ファイルの内部をスキャン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "有効にすると、圧縮ファイル (.zip, .7z など) で有効な/対応されているコンテンツを検索します。スキャンのパフォーマンスに大きな影響を与える可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Arcade DAT ファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "スキャンされたアーケードコンテンツ (MAME、FinalBurn Neo など) の自動命名を有効にするために、Logiqx または MAME リスト XML DAT ファイルを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Arcade DAT フィルター"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "アーケード DAT ファイルを使用する場合、一致する DAT ファイルエントリーが見つかった場合にのみプレイリストにコンテンツを追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "既存のプレイリストを上書き"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "有効にすると、コンテンツをスキャンする前にすべての既存のプレイリストが削除されます。無効にすると、既存のプレイリストエントリーが保護され、現在プレイリストにないコンテンツのみが追加されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "既存のエントリーを検証"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "有効にすると、新しいコンテンツをスキャンする前に既存のプレイリストのエントリーが検証されます。不足しているコンテンツおよび/または無効な拡張子を持つファイルを参照するエントリーは削除されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "スキャンを開始"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "選択されたコンテンツをスキャンします。"
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "リストを初期化中..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "発売年"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "プレイヤー数"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "地域"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "タグ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "名前で検索..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "すべて表示"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "追加フィルター"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "すべて"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "フィルターを追加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u 項目"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "開発元別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "販売元別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "発売年別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "プレイヤー数別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "ジャンル別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "実績別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "カテゴリー別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "言語別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "地域別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "コンソール専用別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "プラットフォーム専用別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "振動別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "スコア別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "メディア別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "コントロール別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "アートスタイル別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "ゲームプレイ別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "ナラティブ別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "ペーシング別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "視点別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "設定別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "視覚別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "乗り物別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "原点国別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "フランチャイズ別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "タグ別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "システム名別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "範囲フィルターを設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "表示"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "ビューとして保存"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "このビューを削除"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "新しいビューの名前を入力してください"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "ビューは既に同じ名前で存在します"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "ビューが保存されました"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "ビューが削除されました"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "実行"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "コンテンツを開始します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "名前の変更"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "このエントリーのタイトルを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "このエントリーをプレイリストから削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "お気に入りに追加"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "[お気に入り] にコンテンツを追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "プレイリストに追加"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "プレイリストにコンテンツを追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "新しいプレイリストを作成"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "新しいプレイリストを作成し、現在のエントリーを追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "コアの関連付けを設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "このコンテンツに関連付けるコアを設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "コアの関連付けをリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "このコンテンツに関連付けられたコアをリセットします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "情報"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "このコンテンツについての詳細を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "サムネイルをダウンロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "現在のコンテンツのスクリーンショット/ボックスアート/タイトルスクリーンのサムネイル画像をダウンロードします。既存のサムネイルを更新します。"
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "現在のコア"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "名前"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "ファイルパス"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "エントリー: %lu/%lu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "コア"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "プレイ時間"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "最終プレイ日時"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "データベース"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "再開"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "再起動"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "コンテンツを最初から再開します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "コンテンツを閉じる"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "スクリーンショットを撮影"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "画面をキャプチャします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "ステートスロット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "現在選択されているステートスロットを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "ステートセーブ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "現在選択されているスロットにステートを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "ステートロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "現在選択されているスロットに保存したステートをロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "ステートロードを取り消す"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "ステートがロードされていた場合、コンテンツをロード前の状態に戻します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "ステートセーブを取り消す"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "ステートが上書きされていた場合、それを以前のステートセーブにロールバックします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "リプレイスロット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "現在選択されているステートスロットを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "リプレイを再生"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "現在選択されているスロットからリプレイファイルを再生します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "リプレイを記録"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "現在選択されているスロットにリプレイファイルを記録します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "録画/リプレイを停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "現在のリプレイの録画/再生を停止します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "お気に入りに追加"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "[お気に入り] にコンテンツを追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "録画を開始"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "ビデオの録画を開始します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "録画を停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "ビデオの録画を停止します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "配信を開始"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "選択した宛先への配信を開始します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "配信を停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "配信を終了します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "ステートセーブ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "ステートセーブのオプションにアクセスします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "コアオプション"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "コントロール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "チート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "チートコードを設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "ディスクコントロール"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "ディスクイメージの管理です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "シェーダー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "画像を視覚的に補強するシェーダーを設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "優先設定"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "グローバル設定を一時的に上書きするためのオプションです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "実績"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "実績と関連する設定を表示します。"
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "コアオプションの管理"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "現在のコンテンツのオプション優先設定を保存または削除します。"
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "ゲーム優先オプションを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "現在のコンテンツのみに適用されるコアオプションを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "ゲーム優先オプションを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "現在のコンテンツのみに適用されるコアオプションを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "コンテンツディレクトリ優先オプションを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "現在のファイルと同じディレクトリからロードされるすべてのコンテンツに適用されるコアオプションを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "コンテンツディレクトリ優先オプションを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "現在のファイルと同じディレクトリからロードされるすべてのコンテンツに適用されるコアオプションを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "アクティブなオプションファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "現在使用中のオプションファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "オプションをリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "すべてのコアオプションをデフォルトの値に設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "オプションを強制的にディスクに保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "現在の設定を強制的にアクティブなオプションファイルに書き込みます。コアのバグによってフロントエンドが不適切にシャットダウンされた場合でも、オプションが確実に保持されます。"
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "ゲーム優先オプションファイルを作成"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "ゲーム優先オプションファイルを保存"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "リマップファイルの管理"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "現在のコンテンツの入力リマップファイルをロード、保存または削除します。"
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "アクティブなリマップファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "現在使用中のリマップファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "リマップファイルをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "リマップファイルをロードして現在の入力割り当てを置き換えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "リマップファイルに名前を付けて保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "現在の入力割り当てを新しいリマップファイルとして保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "コアリマップファイルを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "このコアでロードされるすべてのコンテンツに適用されるリマップファイルを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "コアリマップファイルを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "このコアでロードされるすべてのコンテンツに適用されるリマップファイルを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "コンテンツディレクトリリマップファイルを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "現在のファイルと同じディレクトリからロードされるすべてのコンテンツに適用されるリマップファイルを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "コンテンツディレクトリリマップファイルを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "現在のファイルと同じディレクトリからロードされるすべてのコンテンツに適用されるリマップファイルを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "ゲームリマップファイルを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "現在のコンテンツのみに適用されるリマップファイルを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "ゲームリマップファイルを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "現在のコンテンツのみに適用されるリマップファイルを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "入力割り当てをリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "すべての入力リマップオプションをデフォルトの値に設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "入力リマップファイルを更新"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "アクティブなリマップファイルを現在の入力リマップオプションで上書きします。"
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "リマップファイル"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "チート検索を開始または続行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT,
   "メモリをスキャンして新しいチートを作成します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "チートファイルをロード (置換)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "チートファイルをロードして既存のチートを置き換えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "チートファイルをロード (追加)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "チートファイルをロードして既存のチートに追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "ゲーム固有のチートを再ロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "チートファイルに名前を付けて保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "現在のチートをチートファイルとして保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "新しいチートを上部に追加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "新しいチートを下部に追加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "すべてのチートを削除"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "ゲームの起動時にチートを自動適用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "ゲームを起動した時にチートを自動で適用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "切り替え後に適用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "チートのオン/オフを切り替えた際に、直ちに状態を反映します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "変更を適用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "チートの変更を適用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "チート"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "チート検索を開始または再開"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "左または右を押してビットサイズを変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
   "ビッグエンディアン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN,
   "ビッグエンディアン: 258 = 0x0102\nリトルエンディアン: 258 = 0x0201"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "左または右を押して値を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "%u (%X) に等しい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "以前の値より小さい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "以前の値と等しいまたは小さい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "以前の値より大きい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "以前の値と等しいまたは大きい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "以前の値と等しい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "以前の値と等しくない"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "左または右を押して値を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "以前の値 +%u (%X ) に等しい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "値でメモリ空間を検索"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "左または右を押して値を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "以前の値 -%u (%X ) に等しい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "マッチした %u 件をリストに追加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "一致を削除 #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "コード一致を作成 #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "一致アドレス: %08X マスク: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "チートファイル (置換)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "チートファイル (追加)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "チート詳細"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "インデックス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "リスト内のチートの位置です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "有効"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "説明"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "ハンドラ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "メモリ検索サイズ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "種別"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "値"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "メモリアドレス"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "参照アドレス: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "メモリアドレスマスク"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "メモリ検索サイズ < 8ビットのときのアドレスビットマスク."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "反復回数"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "チートを適用する回数です。他の 2 つの [反復] オプションと併用することで、メモリの広い範囲に影響を与えることができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "反復ごとにアドレスを増加"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "各反復の後、[メモリアドレス] は [メモリ検索サイズ] の数だけ増加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "反復ごとに値を増加"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "各反復の後、[値] はこの量だけ増加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "メモリ内容が次のときに振動する"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "振動値"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "振動ポート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "主振動の強さ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "主振動の持続時間 (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "副振動の強さ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "副振動の持続時間 (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "コード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "新しいチートをこの後に追加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "新しいチートをこの前に追加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "このチートの後にコピー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "このチートの前にコピー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "このチートを削除"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "ディスクの取り出し"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "仮想ディスクトレイを開き、現在ロードされているディスクを取り出します。[メニュー表示時にコンテンツを一時停止] が有効の場合、一部のコアはディスクコントロールの操作後、変更が反映されるまでに数秒間かかる場合があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "ディスクを挿入"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "[現在のディスクイ番号] に対応するディスクを挿入し、仮想ディスクトレイを閉じます。[メニュー表示時にコンテンツを一時停止] が有効の場合、一部のコアでは各ディスク操作アクションの後にコンテンツが数秒間再開されない限り、変更を登録しない場合があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "新しいディスクをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "現在のディスクを取り出し、ファイルシステムから新しいディスクを選択して挿入したあと、仮想ディスクトレイを閉じます。\n注意: これは古い機能です。代わりに、M3U プレイリストを介して複数ディスクタイトルをロードすることをお勧めします。これにより、[ディスクの取り出し/挿入] と [現在のディスクインデックス] オプションを使用してディスクの選択が可能[...]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "ファイルシステムから新しいディスクを選択し、仮想ディスクトレイを閉じることなくディスクを挿入します。\n注意: これは古い機能です。代わりに、M3U プレイリストを介して複数ディスクタイトルをロードすることをお勧めします。[現在のディスクインデックス] オプションを使用してディスクを選択することができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "現在のディスク番号"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "利用可能なイメージのリストから現在のディスクを選択します。[ディスクを挿入] を選択するとディスクがロードされます。"
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "ビデオシェーダー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "ビデオシェーダーパイプラインを有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "シェーダーファイルの変更を監視"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "ディスク上のシェーダーファイルに加えられた変更を自動的に適用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "最後に使用したシェーダーディレクトリを記憶"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "シェーダープリセットとパスをロードする際に、最後に使用したディレクトリでファイルブラウザを開きます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "プリセットをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "シェーダープリセットをロードします。シェーダーパイプラインは自動的にセットアップされます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "シェーダープリセットを直接ロードします。それに応じてシェーダーメニューが更新されます。\nメニューに表示されるスケーリング倍率は、プリセットが単純なスケーリング方式 (例: ソースの拡大縮小、X/Y の拡大倍率と同じ) を使用している場合にのみ信頼できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "プリセットを先頭に追加"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "現在読み込まれているプリセットの先頭にプリセットを追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "プリセットを末尾に追加"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "現在読み込まれているプリセットの末尾にプリセットを追加します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
   "プリセットを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "現在のシェーダープリセットを保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
   "プリセットを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "自動シェーダープリセットを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "変更を適用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "シェーダーの設定の変更を直ちに反映します。シェーダーのパス数やフィルタリング、FBO スケールなどを変更したときに使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "シェーダーパスの数、フィルタリング、FBO スケールなどのシェーダー設定を変更した後、これを使用して変更を適用します。\nこれらのシェーダー設定を変更するのはやや高度な操作であるため、明示的に行う必要があります。\nシェーダーを適用すると、シェーダー設定は一時ファイル (retroarch.slangp/.cgp/.glslp) に保存され、ロードされます。このファイルが RetroArch 終了[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "シェーダーパラメータ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "現在のシェーダーを直接変更します。変更はプリセットファイルには保存されません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "シェーダーパス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "シェーダーパイプラインパスの数を増加または減少させます。各パイプラインパスに別々のシェーダーを割り当てたり、倍率とフィルタリングを個別に設定することができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch では、様々なシェーダーを任意のシェーダーパスで組み合わせ、カスタムハードウェアフィルターやスケール倍率を使用することができます。\nこのオプションは使用するシェーダーパスの数を指定します。これを 0 に設定し、シェーダーの変更を適用すると、[空] シェーダーを使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "シェーダー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "シェーダーへのパスです。すべてのシェーダーは同じタイプ (例: Cg, GLSL または Slang など) である必要があります。ブラウザがシェーダーを探し始める場所を設定するには、シェーダーディレクトリを設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "フィルター"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "このパスのハードウェアフィルターです。[デフォルト] が設定されている場合、フィルターはビデオ設定の [バイリニアフィルタリング] の設定に応じて [リニア] または [ニアレスト] のいずれかになります。"
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "倍率"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "このパスの倍率です。例えば、最初のパスで 2 倍、2 番目のパスで 2 倍であれば、合計で 4 倍の倍率になります。\n最後のパスに倍率係数がある場合、結果はビデオ設定のバイリニアフィルタリングの設定に応じてでデフォルトのフィルターで画面に引き伸ばされます。\n[デフォルト] が設定されている場合、最後のパスでないかどうかに応じて、1 倍のスケールまたはフル[...]"
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "シンプルプリセット"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "元のプリセットへのリンクが読み込まれ、パラメータのみの変更が含まれるシェーダープリセットを保存します。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "シェーダープリセットに名前を付けて保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "現在のシェーダー設定を新しいシェーダープリセットとして保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "グローバルプリセットを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "現在のシェーダー設定をデフォルトのグローバル設定として保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "コアプリセットを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "現在のシェーダー設定をこのコアのデフォルトとして保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "コンテンツディレクトリプリセットを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "現在のシェーダー設定を現在のコンテンツディレクトリ内のすべてのファイルに対するデフォルトとして保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "ゲームプリセットを保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "現在のシェーダー設定をコンテンツのデフォルト設定として保存します。"
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "自動シェーダープリセットが見つかりません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "グローバルプリセットを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "すべてのコンテンツとすべてのコアで使用されるグローバルプリセットを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "コアプリセットを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "現在ロードされているコアで実行されたすべてのコンテンツで使用されるコアプリセットを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "コンテンツディレクトリプリセットを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "現在の作業ディレクトリ内のすべてのコンテンツで使用されるコンテンツディレクトリプリセットを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "ゲームプリセットを削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "特定のゲームのみで使用されるプリセットを削除します。"
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "シェーダーのパラメータはありません。"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "アクティブな優先設定ファイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "現在使用中の優先設定ファイルです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "優先設定ファイルをロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "優先設定ファイルをロードして現在の設定を置き換えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "優先設定に名前を付けて保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "現在の設定を新しい優先設定ファイルとして保存します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "コア優先設定を保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "このコアでロードされるすべてのコンテンツに適用される優先設定ファイルを保存します。メイン設定よりも優先されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "コア優先設定を削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "このコアでロードされるすべてのコンテンツに適用される優先設定ファイルを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "コンテンツディレクトリ優先設定を保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "現在のファイルと同じディレクトリからロードされるすべてのコンテンツに適用される優先設定ファイルを保存します。メイン設定よりも優先されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "コンテンツディレクトリ優先設定を削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "現在のファイルと同じディレクトリからロードされるすべてのコンテンツに適用される優先構成ファイルを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "ゲーム優先設定を保存"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "現在のコンテンツのみに適用される優先設定ファイルを保存します。メイン設定よりも優先されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "ゲーム優先設定を削除"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "現在のコンテンツのみに適用される優先設定ファイルを削除します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "優先設定をアンロード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "すべてのオプションをグローバル設定値にリセットします。"
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "表示する実績がありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "実績ハードコアモードの一時停止をキャンセル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "現在のセッションで実績ハードコアモードを有効にします"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "実績ハードコアモードの再開をキャンセル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "現在のセッションで実績ハードコアモードを無効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "実績ハードコアモードを一時停止"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "現在のセッションで実績ハードコアモードを一時停止します。このアクションはチート、巻き戻し、スローモーションとステートセーブのロードを有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "実績ハードコアモードを再開"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "現在のセッションで実績ハードコアモードを再開します。このアクションはチート、巻き戻し、スローモーションとステートセーブのロードを無効にし、現在のゲームをリセットします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "RetroAchievements サーバーにアクセスできません"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "1 つ以上の実績の解除がサーバーに到達しませんでした。解除はアプリを開いたままにしておくと再試行されます。"
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "RetroAchievements サーバーにアクセスできません。成功するかアプリが閉じられるまで再試行します。"
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "全ての保留中のリクエストがRetroAchievementサーバーに正常に同期されました。"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "ゲームを識別中"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "ゲームデータを取得中"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "セッションを開始中"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "ログインしていません"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "ネットワークエラー"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "不明なゲーム"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "このコアでは実績をアクティブにすることはできません"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "RetroAchievements ハッシュ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "データベースエントリー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "現在のコンテンツのデータベース情報を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "表示するエントリーはありません"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "利用可能なコアがありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "利用可能なコアオプションはありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "利用可能なコア情報がありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "利用可能なコアのバックアップはありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "利用可能なお気に入りがありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "利用可能な履歴がありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "画像ファイルがありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "音楽ファイルがありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "動画ファイルがありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "利用可能な情報がありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "利用可能なプレイリストエントリーはありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "設定が見つかりません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Bluetooth デバイスが見つかりません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "ネットワークが見つかりません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "コアなし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "検索"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "サムネイル切り替え"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "戻る"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "決定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "親ディレクトリ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "親ディレクトリに戻ります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "ディレクトリが見つかりません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "項目がありません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "ファイル選択"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_NORMAL,
   "通常"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90 度"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180 度"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270 度"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_NORMAL,
   "通常"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90 度"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180 度"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270 度"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "不明なコンパイラ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "共有"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "取り組む"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "投票"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "アナログ入力を共有"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "最大"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "平均"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "なし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "優先無し"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "左右で跳ね返す"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "左にスクロール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "画像モード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "音声モード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "ナレーターモード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "履歴 & お気に入り"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "すべてのプレイリスト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "履歴 & お気に入り"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "常に"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "なし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "コアごと"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "総計"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "充電完了"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "充電中"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "放電"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "ソースなし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<このディレクトリを使用>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "これを選択してディレクトリとして設定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<コンテンツディレクトリ>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<デフォルト>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<無し>"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
   "レトロキーボード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD,
   "レトロパッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "レトロパッド (アナログ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "なし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "不明"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "下 + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_L1_R1_START_SELECT,
   "L1 + R1 + スタート + セレクト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_SELECT,
   "スタート + セレクト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "スタートを長押し (2 秒)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "セレクトを長押し (2 秒)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "下 + セレクト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<無効>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "変更される"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "変更されない"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "増加する"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "減少する"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "振動値と等しい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "振動値と等しくない"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "振動値より小さい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "振動値より大きい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "振動値ずつ増加する"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "振動値ずつ減少する"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "すべて"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<無効>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "値に設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "値ずつ増加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "値ずつ減少"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "値がメモリ内容と等しいときに次のチートを実行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "値がメモリ内容と等しくないときに次のチートを実行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "値がメモリ内容より小さいときに次のチートを実行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "値がメモリ内容より大きいときに次のチートを実行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "エミュレータ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1 ビット, 最大値 = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2 ビット, 最大値 = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4 ビット, 最大値 = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8 ビット, 最大値 = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16 ビット, 最大値 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32 ビット, 最大値 = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "システムのデフォルト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "アルファベット順"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "なし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "すべてのラベルを表示"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "丸かっこ () 内の文字を取り除く"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "角かっこ [] 内の文字を取り除く"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "丸かっこ () と角かっこ [] 内の文字を取り除く"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "地域を残す"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "ディスク番号を残す"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "地域とディスク番号を残す"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "システムのデフォルト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "ボックスアート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "スクリーンショット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "タイトルスクリーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "コンテンツロゴ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "通常"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "高速"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "オン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "はい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "いいえ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "はい"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "偽"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "有効"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "無効"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "該当なし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "未解除"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "解除済み"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
   "ハードコア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "非公式"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "非対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "最近解除された実績"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "あと少しです"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "アクティブなチャレンジ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "トラッカーのみ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "通知のみ"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "デフォルト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "リニア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "ニアレスト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "メイン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "コンテンツ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<コンテンツディレクトリ>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<カスタム>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<未指定>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "左アナログ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "右アナログ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "左アナログ (強制)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "右アナログ (強制)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "キー %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "マウス 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "マウス 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "マウス 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "マウス 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "マウス 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "マウスホイール 上"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "マウスホイール 下"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "マウスホイール 左"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "マウスホイール 右"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "早い"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "通常"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "遅い"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "年-月-日 時:分:秒"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "年-月-日 時:分"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "年-月-日"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "年-月"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "月-日-年 時:分:秒"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "月-日-年 時:分"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "月-日 時:分"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "月-日-年"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD,
   "月-日"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "日-月-年 時:分:秒"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "日-月-年 時:分"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "日-月 時:分"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "日-月-年"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM,
   "日-月"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS,
   "時:分:秒"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM,
   "時:分"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "年-月-日 時:分:秒 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "年-月-日 時:分 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "月-日-年 時:分:秒 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "月-日-年 時:分 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "月-日 時:分 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "日-月-年 時:分:秒 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "日-月-年 時:分 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "日-月 時:分 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM,
   "時:分:秒 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM,
   "時:分 (午前/午後)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "前"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "背景フィラーの厚さ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "メニュー背景チェッカーボードパターンの粗さを増やします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "ボーダーフィラー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "ボーダーフィラーの厚さ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "メニュー枠チェッカーボードの粗さを増やします。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "メニューの枠線を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "フル幅レイアウトを使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "利用可能な画面スペースを最大限に活用するために、メニューエントリーのサイズと位置を変更します。無効にすると、従来の固定幅 2 カラムレイアウトを使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "リニアフィルター"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "鋭いピクセルの角を取るため、メニューを少しだけぼかします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "内部アップスケーリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "メニューインターフェースを画面に描画する前にアップスケーリングします。[メニューリニアフィルター] を有効にして使用すると、シャープな画像を維持しながらスケーリングアーティファクト (不均一なピクセル) を除去します。アップスケーリングレベルで増加するパフォーマンスに大きな影響を与えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "アスペクト比"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "メニューのアスペクト比を選択します。ワイドスクリーン比率はメニューインターフェースの水平解像度を増加させます。[メニューアスペクト比をロック] が無効になっている場合は再起動が必要です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "アスペクト比をロック"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "メニューが常に正しいアスペクト比で表示されるようにします。無効にすると、クイックメニューは現在ロードされているコンテンツと一致するように引き伸ばされます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "カラーテーマ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "別のカラーテーマを選択します。[カスタム] を選択すると、メニューのテーマプリセットファイルを使用できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "カスタムテーマプリセット"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "ファイルブラウザからメニューテーマプリセットを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "透明度"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "影のエフェクト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "メニューテキスト、外枠およびサムネイルのドロップシャドウを有効にします。パフォーマンスに少し影響します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "背景アニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "背景パーティクルアニメーション効果を有効にします。パフォーマンスに大きく影響します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "背景アニメーション速度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "背景粒子のアニメーション効果の速度を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "スクリーンセーバーの背景アニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "メニュースクリーンセーバーがアクティブな間、背景パーティクルアニメーション効果を表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "プレイリストサムネイルを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "プレイリストの表示中にインライン縮小サムネイルの表示を有効にします。レトロパッドのセレクトボタンで切り替え可能です。無効した場合でも、レトロパッドのスタートボタンでサムネイルをフルスクリーンに切り替えることができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "上のサムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "プレイリストの右上に表示するサムネイルの種類です。このサムネイルの種類はレトロパッドの Y を押すことで切り替えることができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "下のサムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "プレイリストの右下に表示するサムネイルの種類です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "サムネイルを入れ替える"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "[上のサムネイル] と [下のサムネイル] の表示位置を入れ替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "サムネイル縮小方式"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "大きなサムネイルを画面に合わせて縮小する際に使用されるリサンプリング方式です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "サムネイル遅延 (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "プレイリストエントリーを選択したとき、関連付けられたサムネイルの読み込みを開始するまでの遅延時間です。この値を 256ms 以上に設定すると、最も低性能なデバイスでも遅延のない高速なスクロールが可能になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "拡張 ASCII サポート"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "非標準 ASCII 文字の表示を有効にします。特定の非英語言語との互換性に必要です。パフォーマンスに中程度の影響があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "切り替えアイコン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "オン/オフテキストの代わりにアイコンを使用して [切り替えスイッチ] メニュー設定エントリーを表します。"
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "ニアレストネイバー (高速)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "バイリニア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (低速)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "なし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "自動"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (中央)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (中央)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (中央)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (中央)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (中央)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "自動"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "画面に合わせる"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "整数倍拡大"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "画面に合わせる (拡大)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "カスタム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "クラシックレッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "クラシックオレンジ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "クラシックイエロー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "クラシックグリーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "クラシックブルー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "クラシックバイオレット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "クラシックグレー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "レガシーレッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "ダークパープル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "ミッドナイトブルー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "ゴールデン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "エレクトリックブルー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "アップルグリーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "ボルカニックレッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "ラグーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER,
   "ブログラマー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "ダイナミック"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "グレー ダーク"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "グレーライト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "雪 (ライト)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "雪 (ヘビー)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "雨"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "渦"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "スターフィールド"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "セカンダリーサムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "左側に表示するサムネイルの種類です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "ダイナミック壁紙"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "コンテキストに応じて新しい壁紙を動的にロードします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "水平アニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "メニューの水平アニメーションを有効にします。パフォーマンスに多少影響します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "水平アイコンハイライト時のアニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "タブ間をスクロールする際のアニメーションです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "上下移動時のアニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "上下に移動する際のアニメーションです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "メインメニュー開閉時のアニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "サブメニューを開く際のアニメーションです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "カラーテーマの不透明度"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "フォント"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "メニューで使用する別のメインフォントを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "フォントの色 (赤)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "フォントの色 (緑)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "フォントの色 (青)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "レイアウト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "XMB インターフェースの別のレイアウトを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "アイコンテーマ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "RetroArch の別のアイコンテーマを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "切り替えアイコン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "オン/オフテキストの代わりにアイコンを使用して [切り替えスイッチ] メニュー設定エントリーを表します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "影のエフェクト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "アイコン、サムネイル、文字にドロップシャドウを描画します。パフォーマンスに若干の影響を与えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "シェーダーパイプライン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "背景アニメーション効果を選択します。効果によっては GPU に大きな負荷がかかります。パフォーマンスが不足する場合、オフにするかよりシンプルな効果を選択してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "カラーテーマ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "別の背景カラーテーマを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "サムネイルの垂直配置"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "左側サムネイルを画面右側の右側サムネイルの下に表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "サムネイル表示倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "最大幅を設定することで、サムネイルの表示サイズを縮小します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "垂直フェード係数"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "見出しを表示"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "タイトル余白"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "タイトル余白の水平オフセット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "設定タブを有効にする (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "プログラムの設定を含む設定タブを表示します。"
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "リボン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "リボン (シンプル)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "シンプルスノー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "スノー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
   "ぼかし"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "スノーフレーク"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "カスタム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "アップルグリーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "ダーク"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "ライト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "モーニングブルー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "サンビーム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "ダークパープル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "エレクトリックブルー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "ゴールデン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "レガシーレッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "ミッドナイトブルー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "背景画像"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "アンダーシー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "ボルカニックレッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "ライムグリーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "ピカチュウイエロー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "キューブパープル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "ファミリーレッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "フレイミングホット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "アイスコールド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDGAR,
   "ミッドガル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "グレーダーク"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "グレーライト"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "サイドバーを折りたたむ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "左サイドバーを常に折りたたみます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "プレイリスト名を切り詰める (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "プレイリストからメーカー名を削除します。たとえば [Sony - PlayStation] は [PlayStation] になります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "プレイリスト名を切り詰め後に並べ替える (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "メーカー名が削除されたプレイリストをアルファベット順に並べ替えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "セカンダリーサムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "コンテンツのメタデータパネルを、選択したサムネイルを表示するパネルに置き換えます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "コンテンツのメタデータ表示にティッカーテキストを使用"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "有効にすると、プレイリストの右サイドバーに表示されるコンテンツメタデータの各項目 (関連付けされたコア、プレイ時間) が 1 行で表示されます。サイドバーの幅を超える文字列はスクロールティッカーテキストとして表示されます。無効にすると、コンテンツメタデータの各項目は静的に表示され、必要な行数に応じて折り返されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "サムネイル表示倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "サムネイルバーのサイズの表示倍率です。"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "カラーテーマ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "別のカラーテーマを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "ベーシックホワイト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "ベーシックブラック"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "グレーダーク"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "グレーライト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN,
   "パープルレイン"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "アイコン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "メニュー項目の左側にアイコンを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "切り替えアイコン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "オン/オフテキストの代わりにアイコンを使用して [切り替えスイッチ] メニュー設定エントリーを表します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "プレイリストアイコン (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "プレイリストにシステム固有のアイコンを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "横向きレイアウトの最適化"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "横向きの表示方向を使用する際にメニューのレイアウトを画面に合わせて自動的に調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "ナビゲーションバーを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "OSD メニューナビゲーションショートカットを常に表示します。メニューのカテゴリー間をすばやく切り替えることができます。タッチスクリーンデバイスにおすすめです。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "ナビゲーションバーの自動回転"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "横向きの表示方向を使用する際にナビゲーションバーを自動的に画面の右側に移動します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "カラーテーマ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "別の背景カラーテーマを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "遷移アニメーション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "メニューの階層を移動する際にスムーズなアニメーション効果を有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "縦向きサムネイル表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "横向きの表示方向を使用する際のプレイリストサムネイル表示モードを指定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "横向きのサムネイル表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "横向きの表示方向を使用する際のプレイリストサムネイル表示モードを指定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "リストにセカンダリーサムネイルを表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "[リスト] 形式のサムネイル表示モードを使用している場合、セカンダリーサムネイルを表示します。この設定は画面に 2 つのサムネイルを表示するのに十分な幅がある場合にのみ適用されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "サムネイルの背景を描画"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "サムネイル画像の未使用スペースを単色の背景で塗りつぶします。すべての画像の表示サイズが均一になることで、基本サイズが異なるコンテンツが混在した際のメニューの見栄えを改善します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "プライマリーサムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "各プレイリストエントリーに関連付けるメインサムネイルの種類です。通常はコンテンツアイコンとして機能します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "セカンダリーサムネイル"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "各プレイリストエントリーに関連付けるサブサムネイルの種類です。使用方法は現在のプレイリストのサムネイル表示モードによって異なります。"
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "ブルー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "ブルーグレイ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "ダークブルー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "グリーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "シールド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "レッド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "イエロー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "グレーダーク"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "グレーライト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "自動"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "フェード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "スライド"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "リスト (小)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "リスト (中)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "デュアルアイコン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "リスト (小)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "リスト (中)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "リスト (大)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "デスクトップ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "オフ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "オン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "サムネイルビューを除外"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "詳細"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "ファイル(&F)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "コアをロード(&L)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "コアをアンロード(&U)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "終了(&X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "編集(&E)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "検索(&S)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "表示(&V)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "閉じたドック"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "シェーダーパラメータ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "設定(&O)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "ドック配置を記憶:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "ウィンドウの形状を記憶:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "最後のコンテンツブラウザのタブを記憶:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "テーマ:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<システムデフォルト>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "ダーク"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "カスタム..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "ツール(&T)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "ヘルプ(&H)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "RetroArch について"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "ドキュメント"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "カスタムコアをロード..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "コアをロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "コアをロード中..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "名前"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "バージョン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "プレイリスト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "ファイルブラウザ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "先頭"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "上へ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "コンテンツブラウザ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "ボックスアート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "スクリーンショット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "タイトルスクリーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_LOGO,
   "ロゴ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "すべてのプレイリスト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "コア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "コア情報"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<確認する>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "情報"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "警告"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "エラー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "ネットワークエラー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "変更を適用するには RetroArch を再起動してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "ログ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 項目"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "ここに画像をドロップ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "今後表示しない"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "停止"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "コアを関連付ける"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "隠したプレイリスト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "隠す"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "ハイライトカラー:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "選択(&C)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "色の選択"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "テーマの選択"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "カスタムテーマ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "ファイルパスが空白です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "ファイルは空です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "ファイルの読み込みに失敗しました。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "ファイルの書き込みに失敗しました。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "ファイルは存在しません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "ロードしたコアを最初に優先する:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "ズーム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "表示"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "アイコン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "一覧"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "クリア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "進行状況:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "[すべてのプレイリスト] の最大リストエントリー:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "[すべてのプレイリスト] の最大グリッドエントリー:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "隠しファイルとフォルダを表示:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "新しいプレイリスト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "新しいプレイリストの名前を入力してください:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "プレイリストを削除"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "プレイリストの名前を変更"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "プレイリスト「%1」を削除してもよろしいですか？"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "質問"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "ファイルの削除に失敗しました。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "ファイルの名前を変更できませんでした。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "ファイルの一覧を構築しています..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "ファイルをプレイリストに追加しています..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "プレイリストエントリー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "名前:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "パス:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "コア:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "データベース:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "拡張子:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(スペースで区切る; デフォルトですべてを含む)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "圧縮ファイル内フィルター"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(サムネイルを見つけるために使用されます)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "項目「%1」を削除してもよろしいですか?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "まずプレイリストを 1 つ選んでください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "削除"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "エントリーを追加..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "ファイルを追加..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "フォルダを追加..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "編集"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "ファイル選択"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "フォルダ選択"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<複数>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "プレイリストエントリーの更新中にエラーが発生しました。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "すべての必須項目を入力してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "RetroArch を更新 (nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch の更新に成功しました。変更を適用するには RetroArch を再起動してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "更新に失敗しました。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "貢献者"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "現在のシェーダー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "下へ移動"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "上へ移動"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "ロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "保存"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "削除"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "パスを削除"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "適用"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "パスを追加"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "すべてのパスをクリア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "シェーダーパスはありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "パスをリセット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "すべてのパスをリセット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "パラメータをリセット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "サムネイルをダウンロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "他のダウンロードが進行中です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "起動時に表示するプレイリスト:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "サムネイル"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "サムネイルキャッシュの上限:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "ドラッグ&ドロップしたサムネイルサイズの上限:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "すべてのサムネイルをダウンロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "すべてのシステム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "このプレイリスト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "サムネイルのダウンロードに成功しました。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "成功: %1 失敗: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "コアオプション"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "リセット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "すべてをリセット"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "コアアップデータ設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Cheevos アカウント"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "アカウントリストエンドポイント"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "ターボ/デッドゾーン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "コアカウンター"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "ディスクが選択されていません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "フロントエンドカウンター"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "水平メニュー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "未定義のコア入力の識別子を隠す"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "入力の識別子ラベルを表示"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "OSD オーバーレイ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "履歴"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "履歴プレイリストからコンテンツを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "コンテンツをロードすると、コンテンツと libretro コアの組み合わせが履歴に保存されます。\n履歴は RetroArch の設定ファイルと同じディレクトリのファイルに保存されます。起動時に設定ファイルが読み込まれていない場合、履歴は保存も読み込みもされず、メインメニューにも現れません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "マルチメディア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "サブシステム"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "現在のコンテンツのサブシステム設定にアクセスします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "現在のコンテンツ: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "ネットプレイホストが見つかりませんでした。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "ネットプレイクライアントが見つかりません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "パフォーマンスカウンターがありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "プレイリストがありません。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "接続しました"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "オンライン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "ポート"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "ポート %d デバイス名: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "デバイス表示名: %s\nデバイス設定名: %s\nデバイス VID/PID %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "チート設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "チート検索を開始または続行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "メディアプレイヤーで再生"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "秒"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "コアを開始"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "コンテンツなしでコアを開始します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "推奨コア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "圧縮ファイルを読み込めませんでした。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "ユーザー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "キーボード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "最大スワップチェーンイメージ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "指定されたバッファリングモードを明示的に使用するようにビデオドライバに指示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "スワップチェーンイメージの最大数です。ビデオドライバに特定のビデオバッファリングモードを使用するよう指示できます。\nシングルバッファリング - 1\nダブルバッファリング - 2\nトリプルバッファリング -3\n正しいバッファリングモードを設定すると、レイテンシに大きな影響を与える可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "待機可能なスワップチェーン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "CPU と GPU をハード同期します。パフォーマンスを代償に遅延を軽減します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "最大フレーム遅延"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "指定されたバッファリングモードを明示的に使用するようにビデオドライバに指示します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "現在メニューで使用されているシェーダープリセット自体を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "シェーダープリセット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "シェーダープリセット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "シェーダープリセット"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "URL 参照"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "URL パス"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "開始"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "ニックネーム: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "互換性のあるコンテンツを探しています..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "コアが見つかりません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "プレイリストが見つかりません"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "互換性のあるコンテンツが見つかりました"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "CRC またはファイル名で一致するコンテンツを見つけることができませんでした"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "開始"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "自動アスペクト比"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "ニックネーム (LAN): %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "ステータス"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "システム BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
   "カスタム比"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "録画対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "出力録画に名前を付けて保存…"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "録画を出力ディレクトリに保存"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "一致を表示 #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "表示する一致を選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "強制アスペクト比"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "プレイリストから選択"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "%u 一致リストを表示"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "この一致からコードを作成"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "この一致を削除"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "フッターの不透明度"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "フッターグラフィックの不透明度を変更します。"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "ヘッダーの不透明度"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "ヘッダーグラフィックの不透明度を変更します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "ネットプレイ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "コンテンツを開始"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "コンテンツの履歴ディレクトリ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "出力ディスプレイ ID"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "CRT ディスプレイに接続されている出力ポートを選択します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "ヘルプ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "クリア"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "オーディオ/ビデオのトラブルシューティング"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "仮想コントローラーオーバーレイを変更しています"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "コンテンツをロードするには"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "コンテンツをスキャンする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "コアとは?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "デバッグ情報の送信"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
   "お使いのデバイスと RetroArch の設定に関する診断情報を、分析のためにサーバーに送信します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "データベース設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "ネットプレイ遅延フレーム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "ローカルネットワークをスキャン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "ローカルネットワーク上のネットプレイホストをスキャンして接続します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "ネットプレイクライアント"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "ネットプレイ観戦"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "説明"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "最大実行速度を制限"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "新しいチートコード検索を開始"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "新しいチートの検索を開始します。ビット数を変更できます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "検索を続行"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "新しいチートの検索を続行します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "実績 (ハードコア)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "チート詳細"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "チートの詳細設定を管理します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "チート検索を開始または続行"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "チートコードの検索を開始または続行します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "チートのパス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "チートの数を増加または減少させます。"
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "左アナログ X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "左アナログ Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "右アナログ X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "右アナログ Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "チート検索を開始または続行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "データベースのカーソル表"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "データベース - フィルター: 開発元"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "データベース - フィルター: 販売元"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "データベース - フィルタ: 原点"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "データベース - フィルター: フランチャイズ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "データベース - フィルター: ESRB 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "データベース - フィルター: ELSPA 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "データベース - フィルター: PEGI 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "データベース - フィルター: CERO 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "データベース - フィルター: BBFC 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "データベース - フィルター: 最大ユーザー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "データベース - フィルター: 発売日 (月別)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "データベース - フィルター: 発売日 (年別)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "データベース - フィルター: Edge Magazine 発行"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "データベース - フィルター: Edge Magazine 評価"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "データベース情報"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "設定"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "ダウンロード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "ネットプレイ設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Slang 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "OpenGL/Direct3D テクスチャーへのレンダリング (マルチパスシェーダー) 対応"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "コンテンツ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "通常、libretro/RetroArch アプリをバンドルする開発者がアセットをポイントするように設定されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "確認する"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "基本的なメニューコントルール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "確認/OK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "情報"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "終了"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "上にスクロール"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "デフォルト"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "キーボード切り替え"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "メニュー切り替え"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "メニュー内"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "ゲーム内"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "ゲーム内 (一時停止)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "プレイ中"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "一時停止"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "ネットプレイはコンテンツロード後に開始します。"
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "ネットプレイを開始する前にコンテンツをロードする必要があります。"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "該当するコアやコンテンツファイルが見つかりませんでした。手動でロードしてください。 "
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "使用中のグラフィックドライバは RetroArch で現在設定されているビデオドライバと互換性がありません。 %s ドライバにフォールバックします。変更を適用するには RetroArch を再起動してください。"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "コアのインストールに成功しました"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "コアのインストールに失敗しました"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "すべてのチートを削除するには右を 5 回押してください。"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "デバッグ情報の保存に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_DEBUG_INFO,
   "デバッグ情報をサーバーに送信できませんでした。"
   )
MSG_HASH(
   MSG_SENDING_DEBUG_INFO,
   "デバッグ情報を送信中..."
   )
MSG_HASH(
   MSG_SENT_DEBUG_INFO,
   "デバッグ情報をサーバーに正常に送信しました。ID 番号は %u です。"
   )
MSG_HASH(
   MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
   "もう 2 回押すと、診断情報を RetroArch チームに送信します."
   )
MSG_HASH(
   MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
   "もう一度押すと、診断情報を RetroArch チームに送信します。"
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "グローバルミキサーオーディオ音量"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "ネットプレイのスキャンを完了しました。"
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "申し訳ありませんが、実装されていません: コンテンツを要求しないコアはネットプレイに参加できません。"
   )
MSG_HASH(
   MSG_NATIVE,
   "ネイティブ"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "不正なネットプレイコマンドを受信しました"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "ファイルが既に存在します。バックアップバッファに保存中"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "\"%s\" からの接続を確立:"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "\"%s (%s)\" からの接続を確立:"
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "ネットプレイポートの割り当てに成功しました"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "外部ネットワークにはプライベートまたは共有アドレスがあります。リレーサーバーの使用を検討してください。"
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "ネットプレイ UPnP ポートの割り当てに失敗しました"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "引数が指定されておらず、メニューもビルトインされていないため、ヘルプを表示します…"
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "ディスクがトレイに入りました"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "クライアントを待機中..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "あなたのルームはインターネットから接続できません。"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "ゲームから退出しました"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "プレイヤー %u として参加しました"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "入力デバイス %.*s で参加しました"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "プレイヤー %.*s がゲームから退出しました"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s がプレイヤー %u として参加しました"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s が入力デバイス %.*s で参加しました"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "相手が RetroArch を実行していないか、古いバージョンの RetroArch を実行しているためネットプレイの接続に失敗しました。"
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "ネットプレイの相手が古いバージョンの RetroArch を実行しています。接続できません。"
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "警告: ネットプレイピアが異なるバージョンの RetroArch を実行しています。問題が発生した場合、同じバージョンを使用してください。"
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "ネットプレイの相手が異なるコアを実行しています。接続できません。"
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "警告: ネットプレイピアが異なるバージョンのコアを実行しています。問題が発生した場合、同じバージョンを使用してください。"
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "このコアはこれらのプラットフォーム間のネットプレイに対応していません"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "このコアは異なるプラットフォーム間のネットプレイに対応していません"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "ネットプレイサーバーのパスワードを入力してください:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "ネットプレイチャットメッセージを入力:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "ユーザーからの接続を許可しますか:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "パスワードが違います"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" は退出しました"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "ネットプレイクライアントが切断されました"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "ネットプレイが切断されました"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "プレイする権限がありません"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "空きプレイヤースロットはありません"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "要求された入力デバイスは利用できません"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "プレイモードを切り替えできませんでした"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "ネットプレイのピア \"%s\" が一時停止しました"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "ニックネームを \"%s\" に変更しました"
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "クライアントがキックされました: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "クライアントのキックに失敗しました: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "クライアントが BAN されました: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "クライアントの BAN に失敗しました: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "プレイ中"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "観戦中"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "デバイス"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "チャット対応"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "減速が発生しています"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "音量"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "自動検出"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "対応機能"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "ネットプレイホストに接続中"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "ポートに接続中"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "接続スロット"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "コアのリストを取得しています..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "コアのリストの取得に失敗しました!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "既に最新バージョンがインストールされています: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "コアを更新中: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "コアをダウンロード中: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "コアを展開中: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "コアをインストールしました: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "コアのインストールに失敗しました: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "コアをスキャン中..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "コアをチェック中: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "すべてのインストール済みコアは最新バージョンです"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "対応しているすべてのコアが Play ストアバージョンに切り替わりました"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "更新したコア: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "スキップしたコア: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "コアの更新は無効です - コアがロックされています: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "コアをリセット中: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "コアをリセット: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "プレイリストをクリーニング中: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "プレイリストをクリーニングしました: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "更新に失敗しました - プレイリストに有効なスキャン記録がありません: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "更新に失敗しました - 無効/不足しているコンテンツディレクトリ:"
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "リフレッシュに失敗 - システム名が無効/不足: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "リフレッシュに失敗 - 無効なコア: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "リフレッシュに失敗 - アーケード DAT ファイルが無効/不足: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "リフレッシュに失敗 - アーケード DAT ファイルが大きすぎます (メモリ不足): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "お気に入りに追加しました"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "お気に入りの追加に失敗しました: プレイリストがいっぱいです"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "プレイリストに追加しました"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "プレイリストに追加できませんでした: プレイリストがいっぱいです"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "コアを設定しました:"
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "プレイリストエントリーのコアの関連付けがリセットされました。"
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "ディスクを挿入しました"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "ディスクの追加に失敗しました"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "アプリディレクトリ"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "チートの変更を適用しています。"
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "パッチを適用中: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "シェーダーを適用中"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "オーディオを消音にしました。"
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "オーディオを消音解除しました。"
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "コントローラープロファイルの保存に失敗しました。"
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "コントローラープロファイルを正常に保存しました。"
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "コントローラープロファイルがコントローラープロファイルディレクトリに次の名前で保存されました\n\"%s\""
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "自動セーブを初期化できませんでした。"
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "自動ステートセーブ:"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "コマンドラインインターフェースを起動しています on port"
   )
MSG_HASH(
   MSG_BYTES,
   "バイト"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "新しい設定パスを推測できません。現在の時刻を使用してください。"
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "既知のマジックナンバーと比較しています..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "API に対してコンパイル済み"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "設定ディレクトリが設定されていません。新しい設定を保存できません。"
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "接続しました to"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "コンテンツの CRC32 が異なります。異なるゲームでは使用できません。"
   )
MSG_HASH(
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "ホストが異なるゲームを実行しています。"
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "このホストに対して Ping が高すぎます。"
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "コンテンツのロードがスキップされました。実装は独自にロードされます。"
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "コアはステートセーブに対応していません。"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "コアオプションファイルの作成に成功しました。"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "コアオプションファイルは正常に削除されました。"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "すべてのコアオプションはデフォルトにリセットされます。"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "コアオプションファイルを保存しました to:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "コアオプションの保存に失敗しました to:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "次のドライバを見つけることができませんでした"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "互換性のあるシステムを見つけることができませんでした。"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "有効なデータトラックを見つけることができませんでした"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "データトラックを開くことができませんでした"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "コンテンツファイルを読み込むことができませんでした"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "動画ヘッダーを読み込むことができませんでした。"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "動画から状態を読み込むことができませんでした。"
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "リプレイファイルヘッダーに含まれるコンテンツファイルと保存されたコンテンツチェックサムの CRC 32 チェックサムが一致しません。リプレイの再生時に高確率で同期ずれが発生します。"
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "与えられたカスタムタイミング"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "既に展開中です。"
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "展開に失敗しました。"
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "表示領域を検出しました:"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "有効なコンテンツパッチが見つかりませんでした。"
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "デバイスを有効なポートから切断してください"
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "仮想ディスクトレイを閉じました。"
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "仮想ディスクトレイを取り出しました。"
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "ダウンロード中"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "インデックス"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "ダウンロードに失敗しました"
   )
MSG_HASH(
   MSG_ERROR,
   "エラー"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Libretro コアはコンテンツを必要としますが、何も提供されませんでした。"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Libretro コアは特別なコンテンツを必要としますが、何も提供されませんでした。"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "コアが VFS をサポートしておらず、ローカルコピーからのロードに失敗しました"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "引数の解析エラーです。"
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "コアオプションファイルの保存に失敗しました。"
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "コアオプションファイルの削除に失敗しました。"
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "リマップファイルの保存に失敗しました。"
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "リマップファイルの削除に失敗しました。"
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "シェーダープリセットの保存に失敗しました。"
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "外部アプリディレクトリ"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "展開中"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "ファイルを展開中"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "設定の保存に失敗しました to"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "観客の承認に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "パッチを適用したコンテンツのメモリ割り当てに失敗しました..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "シェーダーの適用に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "シェーダープリセットの適用に失敗しました:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "ソケットの割り当てに失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "ディレクトリの作成に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "圧縮ファイルのコンテンツの展開に失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "クライアントのニックネームの取得に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "ロードに失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "コンテンツのロードに失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "動画ファイルのロードに失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "オーバーレイのロードに失敗しました。"
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "キーボードオーバーレイが設定されていません。"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "ステートロードに失敗しました from"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "libretro コアを開くことができませんでした"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "パッチに失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "クライアントからヘッダーの受信に失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "ニックネームの受信に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "ホストからニックネームの受信に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "ホストからニックネームサイズの受信に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "ホストから SRAM データを受信できませんでした。"
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "トレイからディスクを削除できませんでした。"
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "一時ファイルの除去に失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "SRAM の保存に失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "SRAM のロードに失敗しました"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "ステートセーブに失敗しました:"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "ニックネームの送信に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "ニックネームサイズの送信に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "クライアントへのニックネームの送信に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "ホストへのニックネームの送信に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "クライアントへの SRAM データの送信に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "オーディオドライバの開始に失敗しました。オーディオなしで続行します。"
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "動画の録画の開始に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "録画の開始に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "スクリーンショットの撮影に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "ステートロードの取り消しに失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "ステートセーブの取り消しに失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "オーディオの消音解除に失敗しました。"
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "致命的なエラーが発生しました:"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "ファイルが見つかりません"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "自動ステートセーブが見つかりました in"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "ディスクラベルが見つかりました"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "ファイルの最初のデータトラックが見つかりました"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "最後のステートスロットが見つかりました"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "最後のリプレイスロットが見つかりました"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "現在の記録からではありません"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "リプレイと互換性がありません"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "シェーダーが見つかりました"
   )
MSG_HASH(
   MSG_FRAMES,
   "フレーム数"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "ゲーム固有のコアプションが見つかりました at"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "フォルダ固有のコアオプションが見つかりました at"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "無効なディスク番号です。"
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "マウスの移動範囲の制限"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "ゲームフォーカスオン"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "ゲームフォーカスオフ"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Libretro コアはハードウェアレンダリングされます。ポストシェーディング録画も使用する必要があります。"
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "チェックサムが CRC32 と一致しませんでした。"
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "チートを入力"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "チートファイル名を入力"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "プリセットファイル名を入力"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "優先設定ファイル名を入力"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "リマップファイル名を入力"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "タイトルを変更"
   )
MSG_HASH(
   MSG_INTERFACE,
   "インターフェース"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "内部ストレージ"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "リムーバブルストレージ"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "無効なニックネームサイズです。"
   )
MSG_HASH(
   MSG_IN_BYTES,
   "(バイト)"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "(メガバイト)"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "(ギガバイト)"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "はこの libretro 実装とは異なるバージョンの libretro 用にコンパイルされています。"
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "libretro のフロントエンド"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "スロット #%d からステートをロードしました。"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "スロット #-1 (自動) からステートをロードしました。"
   )
MSG_HASH(
   MSG_LOADING,
   "ロード中"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "1 つ以上のファームウェアファイルが不足しています"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "コンテンツをロード中"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "履歴ファイルをロード中"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "お気に入りファイルを読み込み中"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "ステートロード中"
   )
MSG_HASH(
   MSG_MEMORY,
   "メモリ"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "入力リプレイ映像ファイルは有効なリプレイファイルではありません。"
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "入力リプレイ動画フォーマットはシリアライザのバージョンが異なるようです。ほとんどの場合失敗します。"
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "動画の再生を終了しました。"
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "動画の録画を停止しました。"
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "ネットプレイの初期化に失敗しました。"
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "コアはネットプレイに対応していません。"
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "コンテンツがありません。ダミーコアを開始します。"
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "ステートセーブはまだ上書きされていません。"
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "ステートはまだロードされていません。"
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "優先設定ファイルの保存に失敗しました。"
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "優先設定の削除に失敗しました。"
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "優先設定を正常に保存しました。"
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "優先設定を正常に削除しました。"
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "優先設定を正常にアンロードしました。"
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "保存するものがありません。優先設定は保存されません。"
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "保存されません。優先設定がアクティブです。"
   )
MSG_HASH(
   MSG_PAUSED,
   "一時停止しました。"
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "最初のデータトラックを読み込んでいます..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "サイズ変更のため録音が終了しました. "
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "録画中 to"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "チートファイルの出力先を変更しています to"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "セーブファイルの出力先を変更しています to"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "ステートセーブの出力先を変更しています to"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "リマップファイルを保存しました。"
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "リマップファイルを削除しました。"
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "すべての入力リマップオプションはデフォルトにリセットされます。"
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "ディスクをトレイから取り出しました。"
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "一時コンテンツファイルを削除中"
   )
MSG_HASH(
   MSG_RESET,
   "リセット"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "ドライバが再初期化されたため、録画を再スタートしています。"
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "以前のステートセーブを復元しました。"
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "シェーダー: デフォルトのシェーダープリセットを復元しています to"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "セーブファイルディレクトリを元に戻しています to"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "ステートセーブディレクトリを元に戻しています to"
   )
MSG_HASH(
   MSG_REWINDING,
   "巻き戻しています。"
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "このコアはシリアル化されたステートセーブの対応がないため巻き戻しを利用できません。"
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "巻き戻しバッファをサイズで初期化しています"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "巻き戻しバッファの初期化に失敗しました。巻き戻しが無効になります。"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "実装はスレッド化されたオーディオを使用しています。巻き戻しは使用できません。"
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "巻き戻しバッファの終わりに達しました。"
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "新しい設定を保存しました to"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "スロット #%d にステートを保存しました。"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "スロット #-1 (自動) にステートを保存しました。"
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "保存に成功しました:"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "RAM の種類を保存中"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "ステートセーブ中"
   )
MSG_HASH(
   MSG_SCANNING,
   "スキャン中"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "ディレクトリのスキャンが完了しました。"
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "コマンドを送信中"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "いくつかのパッチが明示的に定義されており、すべてを無視しています…"
   )
MSG_HASH(
   MSG_SHADER,
   "シェーダー"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "シェーダープリセットを正常に保存しました。"
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "スローモーションにしています。"
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "早送りしています。"
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "スローモーションで巻き戻しています。"
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "SRAM のロードをスキップしています。"
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM は保存されません。"
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "SRAM の上書きをブロックしています"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "動画の再生を開始しています。"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "動画の録画を開始しています to"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "ステートサイズ"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "ステートスロット"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "リプレイスロット"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "スクリーンショットを撮影中。"
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "スクリーンショットを保存しました"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "実績を解除しました"
   )
MSG_HASH(
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "レアな実績を解除しました"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "リーダーボードの試行を開始しました"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "リーダーボードの試行に失敗しました"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "%s を %s に送信しました" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "ランク: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "ベスト: %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "サムネイルの種類を変更"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "フルスクリーンサムネイル"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "メタデータの切り替え"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "利用可能なサムネイルがありません"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "このプレイリストエントリーに対して可能なすべてのサムネイルダウンロードはすでに試行されています。"
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "もう一度押すと終了します..."
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "ステートロードを取り消しました。"
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "ステートセーブを取り消しています"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "不明"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "一時停止を解除しました。"
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "認識できないコマンド \"%s\" を受信しました。\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "新しい設定にコア名を使用します。"
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Libretro ダミーコアを使用します。録画をスキップしています。"
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "デバイスを有効なポートから接続してください。"
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "デバイスをポートから切断中"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "再起動しています..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "シャットダウンしています..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Libretro API のバージョン"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "表示領域サイズの計算に失敗しました! RAW データを引き続き使用します。これはおそらく正常に動作しません…"
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "仮想ディスクトレイの取り出しに失敗しました。"
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "仮想ディスクトレイを閉じることができませんでした。"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "ステートセーブを自動ロード中 from"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "\"%s\" からのステートセーブの自動読み込みに失敗しました。"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "\"%s\" からのステートセーブの自動読み込みに成功しました。"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "設定されました in port"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s はポート %u で設定されました"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "ポートから切断されました"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s はポート %u から切断されました"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "設定されていません"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) が設定されていません"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "設定されていません。フォールバックを使用します"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) が設定されていません。フォールバックを使用します"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Bluetooth のスキャンが完了しました。"
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "ペアリングを削除しました。再び接続/ペアリングするには RetroArch を再起動してください。"
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Wi-Fi のスキャンが完了しました。"
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Bluetooth デバイスをスキャン中..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "ワイヤレスネットワークをスキャン中..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Wi-Fi を有効化中..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Wi-Fi を無効にしています..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Wi-Fi を切断中..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "ネットプレイホストをスキャン中…"
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "コンテンツスキャンの準備中…"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "パスワードを入力してください"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "パスワードが一致しました。"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "パスワードが違います。"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "パスワードを入力してください"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "パスワードが一致しました。"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "パスワードが一致しません。"
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "優先設定をロードしました。"
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "ゲームリマップファイルをロードしました。"
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "コンテンツディレクトリリマップファイルをロードしました。"
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "コアリマップファイルをロードしました。"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "入力リマップオプションを保存しました to:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "入力リマップオプションの保存に失敗しました to:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "先行実行を有効にしました。遅延フレームを削除しました: %u。"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "セカンダリインスタンスで先行実行を有効にしました。遅延フレームを削除しました: %u。"
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "先行実行を無効にしました。"
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "このコアはステートセーブに対応していないため先行実行が無効になりました。"
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "このコアは確定的なステートセーブの対応がないため先行実行を利用できません。"
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "ステートセーブに失敗しました。先行実行が無効になりました。"
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "ステートロードに失敗しました。先行実行が無効になりました。"
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "2 番目のインスタンスの作成に失敗しました。先行実行は 1 つのインスタンスのみを使用します。"
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "先制フレームを有効にしました。遅延フレームが削除されました: %u。"
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "先制フレームを無効にしました。"
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "このコアはステートセーブに対応していないため先制フレームが無効になりました。"
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "このコアは確定的なステートセーブの対応がないため先制フレームを利用できません。"
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "先制フレームのメモリ割り当てに失敗しました。"
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "ステートセーブに失敗しました。先制フレームが無効になりました。"
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "ステートロードに失敗しました。先制フレームが無効になりました。"
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "ファイルのスキャンが完了しました。"
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "チート検索を開始しました。"
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "チート検索を開始できませんでした。"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "検索は初期化/開始されていません。"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "新しいマッチ数 = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "一致した %u 件を追加しました。"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "一致を追加できませんでした。"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "一致からコードを作成しました。"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "コードを作成できませんでした。"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "一致を削除しました。"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "空きがありません。最大同時チート数は 100 です。"
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "新しいチートをリストの先頭に追加しました。"
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "新しいチートをリストの最後に追加しました。"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "すべてのチートを削除しました。"
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "新しいチートがこの前に追加されました。"
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "新しいチートがこの後に追加されました。"
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "チートはこの前にコピーされました。"
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "チートはこの後にコピーされました。"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "チートを削除しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "ディスクの設定に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "最後に使用したディスクを設定できませんでした。"
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "クライアントへの接続に失敗しました。"
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "ホストへの接続に失敗しました。"
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "ネットプレイホストが満員です。"
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "あなたはこのホストから BAN されています。"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "ホストからヘッダーを受信できませんでした。"
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "RetroAchievements: 「%s」としてログイン済み。"
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "ステートロードするには実績ハードコアモードを無効にするか一時停止する必要があります。"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "ステートセーブがロードされました。実績ハードコアモードが現在のセッションで無効になりました。"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "チートが有効化されました。実績ハードコアモードが現在のセッションで無効になりました。"
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "マスター済み %s"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "完了済み %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "実績ハードコアモードが有効になりました。ステートセーブと巻き戻しが無効になります。"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "このゲームには実績がありません。"
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "このセッションでの全ての %d アチーブメントが有効になりました"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "%d 非公式のアチーブメントを有効にしました"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "%d / %d のアチーブメントが解除されています"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "%d サポートされていない"
)
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "ハードコアは一時停止しています。手動のビデオフレーム遅延設定は許可されていません。"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "RetroAchievements: ゲームを特定できませんでした。"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "%sがRetroAchievementsのゲーム読み込みに失敗しました。"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "RetroAchievementsのメディア変更に失敗しました：%s"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "最低"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "低"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "通常"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "高"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "最高"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "警告: アセットがありません。利用可能な場合は、オンラインアップデータを使用してください。"
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "警告: 選択した言語のフォントがありません。 利用可能な場合は、オンラインアップデータを使用してください。"
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "警告: サポートされていない言語です - 英語を使用します。"
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "ディスクをダンプ中..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "ドライブ %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "最初にコアをロードしてください。"
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "ドライブからの読み込みに失敗しました。ダンプが中止されました。"
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "ディスクへの書き込みに失敗しました。ダンプが中止されました。"
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "ドライブにディスクが挿入されていません。"
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "シェーダープリセットの削除に成功しました。"
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "シェーダープリセットの削除に失敗しました。"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "無効なアーケード DATA ファイルが選択されました。"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "選択されたアーケード DAT ファイルが大きすぎます (空きメモリ不足)。"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "アーケード DAT ファイルのロードに失敗しました (無効なフォーマット?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "無効な手動スキャン設定です。"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "有効なコンテンツが見つかりませんでした。"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "コンテンツをスキャン中: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "現在のエントリーを確認中: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "スキャン中: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "M3U エントリーのクリーンアップ: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "スキャン完了: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "コアをスキャン中: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "インストール済みコアのバックアップは既に存在します: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "コアをバックアップ中: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "古いバックアップを削除中: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "コアのバックアップを完了しました: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "選択したコアのバックアップは既にインストールされています: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "コアの復元中: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "コアの復元が完了: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "選択されたコアファイルは既にインストールされています: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "コアをインストール中: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "コアのインストールを完了しました: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "無効なコアファイルが選択されました: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "コアのバックアップに失敗しました: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "コアの復元に失敗しました: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "コアのインストールに失敗しました: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "コアの復元は無効です - コアはロックされています: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "コアのインストールは無効です - コアがロックされています: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "コアをロックできませんでした: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "コアのロック解除に失敗しました: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "[コンテンツレスコア] リストからコアを削除できませんでした:"
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "[コンテンツレスコア] 一覧にコアを追加できませんでした: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "コアの削除は無効です - コアはロックされています: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "対応していないビデオモード"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "コア情報ディレクトリに書き込めません - コア情報のキャッシュは無効になります"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "エントリーステートを発見しました in"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "エントリーステートをロード中 from"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "ゲームモードを有効にできませんでした"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "ゲームモードを有効にできませんでした - ゲームモードデーモンがインストール/実行されていることを確認してください"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "正確なコンテンツフレームレートに同期が有効になりました。"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "正確なコンテンツフレームレートに同期が無効になりました。"
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "ビデオのリフレッシュレートを %s Hz に変更しました。"
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Lakka を更新"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "フロントエンド名"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Lakka バージョン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "再起動"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Joy-Con を分割"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "グラフィックウィジェット表示倍率のオーバーライド"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "表示ウィジェットを描画する際に、手動で設定した表示倍率を適用します。[グラフィックウィジェットの自動スケーリング] が無効になっている場合にのみ適用されます。装飾された通知、インジケータおよびコントロールの表示倍率をメニューのそれから切り離して独立させ、拡大または縮小する場合に使用することができます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "画面解像度"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "画面解像度: デフォルト"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "画面解像度: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "画面解像度: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "適用中: デフォルト"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "適用中: %dx%d\nスタートでリセット"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "適用中: %dx%d - %s\nスタートでリセット"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "リセット中: デフォルト"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "リセット中: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "リセット: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "ディスプレイモードを選択 (再起動が必要)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "シャットダウン"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "外部ファイルアクセスを有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Windows ファイルアクセス権限の設定を開く"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Windows 権限設定を開き、broadFileSystemAccess 機能を有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "開く..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "システムファイルピッカーを使用して別のディレクトリを開く"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "フリッカーフィルタ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "ビデオガンマ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "ソフトフィルター"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Bluetooth デバイスをスキャンして接続します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "無線ネットワークをスキャンして接続を確立します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Wi-Fi を有効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "ネットワークに接続"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "ネットワークに接続"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "切断"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "ちらつき抑制"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "VI 画面の幅を設定"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "オーバースキャン補正 (上部)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "指定したスキャンライン数 (画面上部から取得) だけ画像サイズを縮小することにより、ディスプレイのオーバースキャントリミングを調整します。スケーリングアーティファクトが発生する可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "オーバースキャン補正 (下部)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "指定したスキャンライン数 (画面下部から取得) だけ画像サイズを縮小することにより、ディスプレイのオーバースキャントリミングを調整します。スケーリングアーティファクトが発生する可能性があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "パフォーマンス維持モード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "CPU パフォーマンスと電力"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "ポリシー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "ガバナーモード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "手動"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "ガバナーや周波数など、CPU の詳細な挙動を手動で調整します。上級ユーザーにのみお勧めします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "パフォーマンス (管理)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "デフォルトかつ推奨モードです。プレイ中はパフォーマンスを最大化し、メニュー閲覧中または一時停止中は電力を節約します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "カスタム管理"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "メニューやゲームプレイ中に使用するガバナーを選択できます。パフォーマンス、オンデマンド、または Schedutil はゲームプレイ中に推奨されます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "最高パフォーマンス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "最高のパフォーマンスを得るため、常に最大周波数で動作します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "最低消費電力"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "消費電力を節約するため、利用可能な最小周波数で動作します。バッテリー駆動のデバイスで役立ちますが、パフォーマンスが大幅に低下します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "バランス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "現在の負荷に応じて処理性能を調整します。ほとんどのデバイスとエミュレータで動作し、消費電力を軽減するのに役立ちます。一部のデバイスでは、システム要件が高いゲームやコアで実行速度が低下する場合があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "最小周波数"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "最大周波数"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "最小コア周波数"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "最大コア周波数"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "CPU ガバナー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "コアガバナー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "メニューガバナー"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "ゲームモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "パフォーマンスを改善させ、レイテンシを軽減し、オーディオのクラックの問題を修正できます。これを動作させるには、https://github.com/FeralInteractive/gamemode が必要です。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Linux GameMode を有効にすることで、レイテンシを改善し、音割れの問題を修正し、最高のパフォーマンスを得るために CPU と GPU を自動的に設定することで全体的なパフォーマンスを最大化できます。\nGame Mode ソフトウェアをインストールする必要があります。GameMode のインストール方法については、https://github.com/FeralInteractive/gamemode を参照してください。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "PAL60モードを使用"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "RetroArch を再起動"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "終了して RetroArch を再起動します。特定のメニュー設定を有効にするために必要です (例えば、メニュードライバを変更する場合など)。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "ブロックフレーム"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "フロントタッチを優先"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "背面タッチの代わりにフロントを使用します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "タッチ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "キーボードコントローラーのマッピング"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "キーボードコントローラーのマッピングの種類"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "小さいキーボード"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "入力ブロックタイムアウト"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "完全な入力サンプルを取得するまでのミリ秒数です。ボタンの同時押しで問題が発生した場合に使用します (Android のみ)。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "[再起動] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "[再起動] オプションを表示します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "[シャットダウン] を表示"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "[シャットダウン] オプションを表示します。"
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "パスワード付き"
   )
MSG_HASH(
   MSG_INTERNET,
   "インターネット"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "インターネット (リレー)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "インターネット (接続できません)"
   )
MSG_HASH(
   MSG_LOCAL,
   "ローカル"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "内部ストレージの状態: 読み取り/書き込み"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "内部ストレージの状態: 読み取り専用"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "画面の明るさ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "画面の明るさを増減します。"
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "CPU オーバークロック"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Switch CPU をオーバークロックします。"
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Bluetooth の状態を決定します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "サービス"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "OS に関連するサービスを管理します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "SMB プロトコルを介してネットワークフォルダを共有します。"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "SSH を使用してコマンドラインにリモートでアクセスします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Wi-Fi アクセスポイント"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Wi-Fi アクセスポイントを有効または無効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "タイムゾーン"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "タイムゾーンを選択して、日付と時刻を現在地に合わせます。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "利用可能なタイムゾーンの一覧を表示します。タイムゾーンを選択したあと、日付と時刻は選択したタイムゾーンに合わせて調整されます。システム/ハードウェアの時計が UTC に設定されていることを前提としています。"
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Nintendo Switch オプション"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Nintendo Switch 固有のオプションを管理します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "CPU オーバークロック"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "CPU 周波数のオーバークロックを有効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "CEC 対応"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "ドッキング時にテレビで CEC ハンドシェイクを有効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "Bluetooth ERTM を無効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "Bluetooth ERTM を無効にしてデバイスのペアリングを修正します"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Wi-Fi アクセスポイントを切り替えています。"
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Wi-Fi '%s' から切断中"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Wi-Fi '%s' に接続中"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[SSID なし]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Wi-Fi アクセスポイントはすでに開始されています"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Wi-Fi アクセスポイントが動作していません"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Wi-Fi アクセスポイントを SSID=%s, Passkey=%s で開始しています"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Wi-Fi アクセスポイントの設定ファイルを作成できませんでした。"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "設定ファイルが不正です - APNAME または PASSWORD が %s に見つかりませんでした"
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "マウス倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Wii リモコンのライトガン速度に合わせて X/Y 倍率を調整します。"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "タッチ倍率"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "タッチスクリーン座標の X/Y 倍率を調整し、OS 水準の表示スケーリングに対応します。"
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "タッチ仮想マウス (ポインタ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "入力タッチスクリーンからタッチイベントを渡すことを有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "タッチ仮想マウス (マウス)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "入力タッチイベントを使用して仮想マウスエミュレーションを有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "タッチ仮想マウスタッチパッドモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "タッチスクリーンをタッチパッドとして使用するには、マウスと併せて有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "タッチ仮想マウストラックボールモード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "マウスと併用すると、タッチスクリーンをトラックボールとして利用でき、ポインタに慣性が加わります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "タッチ仮想マウスジェスチャー"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "タップ、タップドラッグおよび指スワイプなどを含むタッチスクリーンジェスチャーを有効にします。"
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "RGA スケーリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA スケーリングとバイキュービックフィルタリングです。ウィジェットが壊れる可能性があります。"
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "コンテキスト固有のスケーリング"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "ハードウェアコンテキストのスケーリングです (利用可能な場合)。"
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "New3DS クロック / L2 キャッシュを有効にする"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "New3DS クロック速度 (804MHz) と L2 キャッシュを有効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "3DS 下画面"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "下画面のステータス情報の表示を有効にします。バッテリー寿命とパフォーマンスを改善するには無効にします。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "3DS 表示モード"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "3D 表示モードと 2D 表示モードを選択します。[3D] モードでは、クイックメニューを表示する際にピクセルが正方形になり、奥行き効果が適用されます。[2D] モードは最高のパフォーマンスを提供します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (ピクセルグリッドエフェクト)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (高解像度)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "タッチスクリーンをタップして\nRetroArch メニューに移動します"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "アセットが見つかりません"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "なし\nデータ"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "なし\nスクリーンショット"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "ゲームを再開"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "作成\n復元ポイント"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "ロード\n復元ポイント"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "下画面アセットディレクトリ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "下画面アセットのディレクトリです。ディレクトリには「bottom_menu.png」を含める必要があります。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "フォントを有効にする"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "フォントの色 (赤)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "下画面のフォントの赤色を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "フォントの色 (緑)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "下画面のフォントの緑色を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "フォントの色 (青)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "下画面のフォントの青色を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "フォントの色の不透明度"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "下画面のフォントの不透明度を調整します。"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "フォントの大きさ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "下画面のフォントの大きさを調整します。"
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "スキャンを完了しました。<br><br>\nコンテンツを正確にスキャンするには、以下の手順を実施する必要があります:\n<ul><li>互換性のあるコアがダウンロードされている</li>\n<li>オンラインアップデータを使用して \"コア情報ファイル\" が更新されている</li>\n<li>オンラインアップデータを使用して \"データベース\" が更新されている</li>\n<li>上記の手順を実施した後、RetroArch を再起動してください。</li></ul>\nコンテンツは<a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">既存のデータベース</a>と一致する必要があります。\nそれでも上手くいかない場合は、<a href=\"https://www.github.com/libretro/RetroArch/issues\">バグレポートの送信</a>を検討してください。"
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "タッチマウスが有効になりました"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "タッチマウスが無効になりました"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "sdl2 マイクには sdl2 オーディオドライバーが必要です。"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "RetroArch ユーザー補助が有効化されています。メインメニューのコアをロードです。"
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "停止しました。"
   )
#ifdef HAVE_GAME_AI





#endif

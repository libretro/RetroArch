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
   "Menu chính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Thiết lập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Yêu thích"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Lịch sử"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Hình ảnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Âm nhạc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Video"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Khám phá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Core tự chạy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Nhập trò chơi"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Menu nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Truy xuất nhanh mọi cài đặt trong các trò chơi liên quan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Tải Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Chọn Core nào sẽ dùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "Đóng Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Giải phóng Core đã nạp khỏi bộ nhớ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Duyệt qua các Core libretro. Khi trình quản lý tập tin bắt đầu dựa vào đường dẫn thư mục Core. Nếu nó trống, nó sẽ được bắt đầu ở thư mục gốc.\nNếu thư mục lõi của bạn là một thư mục, menu sẽ sử dụng thư mục đó là thư mục trên cùng. Nếu thư mục Core của bạn là đường dẫn đầy đủ, nó sẽ bắt đầu ở thư mục nơi tệp nằm ở đó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Mở trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Chọn trò chơi nào sẽ bắt đầu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Để duyệt trò chơi, bạn cần một 'Core' để sử dụng và một tệp.\nĐể kiểm soát vị trí mà menu bắt đầu duyệt tìm tệp, hãy đặt 'Thư mục Duyệt Tệp'. Nếu không đặt, nó sẽ bắt đầu từ thư mục gốc.\nTrình duyệt sẽ lọc các tệp dựa trên phần mở rộng của core cuối cùng đã chọn trong 'Tải Core', và sẽ sử dụng core đó khi trò chơi được mở."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Mở đĩa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Mở đĩa phương tiện vật lý. Đầu tiên là chọn nhân (Nạp nhân) để dùng với đĩa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Sao lưu đĩa"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Sao chép đĩa vật lý vào bộ nhớ trong. Tệp sẽ được lưu dưới dạng file ảnh."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Đẩy đĩa ra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Đẩy đĩa ra từ ổ CD/DVD vật lý."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Danh sách chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Nội dung đã quét trùng khớp với cơ sở dữ liệu sẽ hiển thị tại đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Nhập trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Tạo và cập nhật danh sách bằng cách quét trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Hiển thị giao diện máy tính"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Mở giao diện máy tính cổ điển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Tắt chế độ Ki ốt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Hiển thị tất cả cấu hình cài đặt liên quan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Cập nhật trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Tải xuống các tiện ích bổ sung, thành phần và trò chơi cho RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Tham gia hoặc làm máy chủ cho Trò chơi trực tuyến."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Thiết lập"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Cấu hình chương trình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Thông tin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Hiển thị thông tin hệ thống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Tập tin cấu hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Quản lý và tạo tệp cấu hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Trợ giúp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Tìm hiểu thêm về cách chương trình hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Khởi động lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Khởi động lại RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Thoát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Thoát RetroArch. Lưu thiết lập khi thoát được bật."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_NOW,
   "Đồng bộ ngay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_NOW,
   "Kích hoạt đồng bộ hóa đám mây thủ công."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Giải quyết xung đột: Giữ nguyên vị trí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_LOCAL,
   "Giải quyết mọi xung đột bằng cách tải các tệp cục bộ lên máy chủ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Giải quyết xung đột: Giữ máy chủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_RESOLVE_KEEP_SERVER,
   "Giải quyết mọi xung đột bằng cách tải xuống các tệp trên máy chủ và thay thế các bản sao cục bộ."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH_NOSAVE,
   "Thoát RetroArch. Lưu thiết lập khi thoát bị tắt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_QUIT_RETROARCH,
   "Thoát RetroArch. Nếu bạn cưỡng ép tắt ứng dụng theo bất kỳ cách nào (SIGKILL,...), RetroArch sẽ không kịp lưu bất kỳ cấu hình nào trong mọi trường hợp.Trên hệ Unix (Linux, macOS,...), các tín hiệu SIGINT/SIGTERM cho phép RetroArch tắt đúng cách, giải phóng bộ nhớ đầy đủ và lưu cấu hình nếu 'Lưu tự động' được bật."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Tải về Core..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Tải về và cài đặt nhân từ internet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Cài đặt hoặc khôi phục 1 Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Cài đặt hoặc khôi phục 1 core từ thư mục 'Tải về'."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Khởi động trình xử lý hình ảnh"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Khởi động tay cầm từ xa"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Thư mục bắt đầu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Tải về"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Duyệt nội dung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Tải nội dung"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Yêu thích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Trò chơi được thêm vào mục 'Yêu thích' sẽ xuất hiện ở đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Âm nhạc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Nhạc đã được phát trước đó sẽ xuất hiện ở đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Hình ảnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Ảnh đã được xem trước đó sẽ xuất hiện ở đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Video đã được phát trước đó sẽ xuất hiện ở đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Khám phá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Duyệt tất cả trò chơi khớp với kho dữ liệu thông qua giao diện tìm kiếm theo thể loại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Core tự chạy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Các Core đã cài đặt có thể hoạt động mà không cần tải trò chơi sẽ xuất hiện ở đây."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Trình tải Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Cập nhật các Core đã được cài đặt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Cập nhất tất cả các Core lên phiên bản khả dụng mới nhất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Chuyển các Core sang phiên bản Cửa hàng Play"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Thay thế tất cả các Core cũ và được cài đặt thủ công bằng các phiên bản mới nhất từ ​​Cửa hàng Play, nếu có."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Trình cập nhật ảnh thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Tải xuống gói ảnh thu nhỏ đã hoàn thành cho hệ thống đã chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Trình cập nhật danh sách phát thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Tải ảnh thu nhỏ cho những mục trong danh sách đã chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Tải về trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Tải xuống trò chơi miễn phí cho Core đã chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Trình tải xuống Core hệ thống"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Tải xuống các tệp hệ thống phụ trợ cần thiết để Core hoạt động chính xác / tối ưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Cập nhật các tệp thông tin Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Cập nhật tài nguyên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Cập nhật cấu hình trình điều khiển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Cập nhật Cheats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Cập nhật cơ sở dữ liệu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Cập nhật lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Cập nhật bộ lọc GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Cập nhật bộ lọc Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Cập nhật bộ lọc Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Thông tin Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Xem thông tin liên quan đến ứng dụng/Core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Thông tin ổ đĩa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Xem thông tin về ổ đĩa đang cắm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Thông tin về mạng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Xem giao diện mạng và các địa chỉ IP liên quan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Thông tin hệ thống"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Xem thông tin chi tiết về thiết bị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Quản lý cho cơ sở dữ liệu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Xem cơ sở dữ liệu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Quản lý con trỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Xem những tìm kiếm trước đó."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Tên của Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Nhãn hiệu Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Phiên bản của Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Tên hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Nhà sản xuất hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Thể loại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Tác giả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Quyền truy cập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Bản quyền"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Tiện ích mở rộng được hỗ trợ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "API đồ họa được yêu cầu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_PATH,
   "Đường dẫn đầy đủ"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Hỗ trợ lưu trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Cơ bản (Lưu/Mở)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Nối tiếp (Lưu / Mở, Tua lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Xác định (Lưu / Mở, Tua lại, Tua tới, Chơi qua mạng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Phần mềm hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "Lưu ý: 'Tệp Hệ thống nằm trong Thư mục Nội dung' đã được bật."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "Đang tìm trong: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Thiếu, Yêu cầu phải có:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Thiếu, tùy chọn:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Hiện thời, Cần có:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Hiện thời, tùy chọn:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Khoá Core đã cài đặt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Ngăn chặn sửa core đang cài đặt. Có thể được dùng để né các bản cập nhật không mong muốn, nhất là khi ROM (ví dụ: Arcade ROM) cần đúng phiên bản core cụ thể, hoặc khi core thay đổi định dạng trạng thái lưu của nó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Loại core này khỏi menu 'Core tự chạy không cần trò chơi'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Ngăn không cho core này xuất hiện trong tab/menu 'Core tự chạy'. Chỉ áp dụng khi chế độ hiển thị đang đặt ở 'Tùy chỉnh'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Xóa Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Xóa Core này khỏi bộ nhớ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Sao lưu Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Tạo một bản sao lưu của core hiện được cài đặt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Khôi phục bản sao lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Cài đặt phiên bản trước của Core từ danh sách các bản sao lưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Xóa bản sao lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Xóa một tệp khỏi danh sách các bản sao lưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[Tự động]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Ngày tạo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETROARCH_VERSION,
   "Phiên bản RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Phiên bản Git"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Trình biên dịch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Loại CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Tính năng CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Kiến trúc CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Số lõi CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JIT_AVAILABLE,
   "JIT khả dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUNDLE_IDENTIFIER,
   "Mã định danh gói ứng dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Loại hệ điều hành"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Phiên bản OS"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "Cấp độ đánh giá Retro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Nguồn năng lượng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Trình điều khiển ngữ cảnh hình ảnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Độ rộng hiển thị (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Chiều cao hiển thị (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Hiển thị DPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "Hỗ trợ LibretroDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Hỗ trợ lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Hỗ trợ giao diện lệnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Hỗ trợ giao diện lệnh qua mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Hỗ trợ trình điểu khiển mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Hỗ trợ Cocoa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "Hỗ trợ PNG (RPNG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "Hỗ trợ JPEG (RJPEG)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "Hỗ trợ BMP (RBMP)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "Hỗ trợ TGA (RTGA)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "Hỗ trợ SDL 1.2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "Hỗ trợ SDL 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D8_SUPPORT,
   "Hỗ trợ Direct3D 8"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D9_SUPPORT,
   "Hỗ trợ Direct3D 9"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D10_SUPPORT,
   "Hỗ trợ Direct3D 10"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D11_SUPPORT,
   "Hỗ trợ Direct3D 11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_D3D12_SUPPORT,
   "Hỗ trợ Direct3D 12"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GDI_SUPPORT,
   "Hỗ trợ GDI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Hỗ trợ Vulkan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Hỗ trợ Metal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "Hỗ trợ OpenGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "Hỗ trợ OpenGL ES"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Hỗ trợ phân luồng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "Hỗ trợ KMS/EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Hỗ trợ udev"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "Hỗ trợ OpenVG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "Hỗ trợ EGL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "Hỗ trợ X11"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Hỗ trợ Wayland"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "Hỗ trợ XVideo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "Hỗ trợ ALSA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "Hỗ trợ OSS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "Hỗ trợ OpenAL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "Hỗ trợ OpenSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "Hỗ trợ RSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "Hỗ trợ RoarAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "Hỗ trợ JACK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "Hỗ trợ PulseAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PIPEWIRE_SUPPORT,
   "Hỗ trợ PipeWire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "Hỗ trợ CoreAudio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "Hỗ trợ CoreAudio V3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "Hỗ trợ DirectSound"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "Hỗ trợ WASAPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "Hỗ trợ XAudio2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "Hỗ trợ zlib"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "Hỗ trợ 7zip"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZSTD_SUPPORT,
   "Hỗ trợ Zstandard"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Hỗ trợ thư viện động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Tải Runtime động của Thư viện libretro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Hỗ trợ Cg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "Hỗ trợ GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "Hỗ trợ HLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "Hỗ trợ hình ảnh SDL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "Hỗ trợ FFmpeg"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "Hỗ trợ mpv"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "Hỗ trợ CoreText"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "Hỗ trợ FreeType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "Hỗ trợ STB TrueType"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Hỗ trợ Trò chơi trực tuyến (Người chơi trực tiếp)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Hỗ trợ Video4Linux2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SSL_SUPPORT,
   "Hỗ trợ SSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "hỗ trợ libusb"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Lựa chọn cơ sở dữ liệu"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Tên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Miêu tả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Thể loại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Kích hoạt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Loại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Ngôn ngữ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Khu vực"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Bảng điều khiển chọn lọc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Nền tảng độc quyền"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Điểm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Phương tiện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Điều khiển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Phong cách nghệ thuật"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Lối chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Mô tả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "Nhịp độ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Góc nhìn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Cài đặt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Đồ hoạ chính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Có phương tiện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Nhà phát hành"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Nhà phát triển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Xuất xứ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Thương hiệu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Đánh giá độ tuổi theo TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Điểm tạp chí Famitsu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Tạp chí Edge đánh giá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Xếp hạng tạp chí Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Số báo của tạp chí Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Tháng phát hành"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Năm phát hành"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Điểm BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Điểm ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Điểm ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Điểm PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Phần cứng nâng cao"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Đánh giá của CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Số sê-ri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analog được hỗ trợ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Hỗ trợ rung tay cầm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Hỗ trợ Co-op"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Tải cấu hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Tải cấu hình hiện có và thay thế các giá trị hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Lưu cấu hình hiện tại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Ghi đè tệp cấu hình hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Lưu cấu hình mới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Lưu cấu hình hiện tại thành tệp riêng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_AS_CONFIG,
   "Lưu cấu hình dưới dạng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_AS_CONFIG,
   "Lưu cấu hình hiện tại thành file tùy chỉnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_MAIN_CONFIG,
   "Lưu cấu hình chính"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_MAIN_CONFIG,
   "Lưu cấu hình hiện tại làm cấu hình chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Khôi phục mặc định"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Khôi phục cấu hình về giá trị mặc định."
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Hướng dẫn điều khiển Menu cơ bản"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Cuộn lên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Cuộn xuống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Xác nhận"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "Thông tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Bắt đầu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Bật/Tắt Menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Thoát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Bật/Tắt bàn phím"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Trình điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Thay đổi ổ đĩa được dùng bởi hệ thống."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Điều chỉnh thiết lập đầu ra cho video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Thay đổi thiết lập âm thanh vào/ra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Đều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Thay đổi cài đặt tay cầm, bàn phím và chuột."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Độ trễ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Thay đổi độ trễ của hình ảnh, âm thanh và dữ liệu điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Danh mục Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Thay đổi cài đặt Core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Cấu hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Thay đổi cài đặt mặc định cho các tệp cấu hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Thay đổi cài đặt lưu trữ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SETTINGS,
   "Đồng bộ đám mây."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SETTINGS,
   "Thay đổi thiết lập đồng bộ đám mây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_ENABLE,
   "Bật đồng bộ hóa đám mây"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_ENABLE,
   "Thử đồng bộ cấu hình, SRAM và trạng thái lên dịch vụ lưu trữ đám mây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DESTRUCTIVE,
   "Đồng bộ đám mây có khả năng ghi đè."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SAVES,
   "Đồng bộ: Tệp lưu/Trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_CONFIGS,
   "Đồng bộ: Tệp cấu hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_THUMBS,
   "Đồng bộ: Ảnh thu nhỏ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_SYSTEM,
   "Đồng bộ: Tệp hệ thống."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SAVES,
   "Khi bật, tệp lưu và trạng thái sẽ được đồng bộ lên đám mây."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_CONFIGS,
   "Khi bật, tệp cấu hình sẽ được đồng bộ lên đám mây."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_THUMBS,
   "Khi bật, ảnh thu nhỏ sẽ được đồng bộ lên đám mây. Không khuyến khích trừ khi có bộ sưu tập ảnh tùy chỉnh lớn; nếu không, tải ảnh thu nhỏ là lựa chọn tốt hơn."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_SYSTEM,
   "Khi bật, tệp hệ thống sẽ được đồng bộ lên đám mây. Có thể khiến đồng bộ mất nhiều thời gian hơn; nên cân nhắc trước khi dùng."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DESTRUCTIVE,
   "Khi tắt, các tệp sẽ được chuyển vào thư mục sao lưu trước khi bị ghi đè hoặc xoá."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE,
   "Chế độ đồng bộ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_SYNC_MODE,
   "Tự động: Đồng bộ khi khởi động RetroArch và khi các core được tắt. Thủ công: Chỉ đồng bộ khi nút Đồng bộ ngay được nhấn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_AUTOMATIC,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_SYNC_MODE_MANUAL,
   "Thủ công"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_DRIVER,
   "Nền tảng đồng bộ đám mây."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_DRIVER,
   "Chọn giao thức mạng để dùng cho lưu trữ đám mây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_URL,
   "URL lưu trữ đám mây"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_URL,
   "URL cho API kết nối tới dịch vụ lưu trữ đám mây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_USERNAME,
   "Tên người dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_USERNAME,
   "Tên đăng nhập tài khoản lưu trữ đám mây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOUD_SYNC_PASSWORD,
   "Mật khẩu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOUD_SYNC_PASSWORD,
   "Mật khẩu tài khoản lưu trữ đám mây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Ghi nhật ký"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Thay đổi cài đặt ghi nhật ký."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Quản lý tập tin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Đổi tùy chỉnh Quản lý tệp tin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CONFIG,
   "Tệp cấu hình hệ thống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_COMPRESSED_ARCHIVE,
   "Tệp lưu trữ nén."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RECORD_CONFIG,
   "Tệp cấu hình ghi hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CURSOR,
   "Tệp lưu trữ dòng đang chỉ vào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_CONFIG,
   "Tệp cấu hình đang dùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER_PRESET,
   "Tệp gói hiệu ứng đồ hoạ (Shader)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_SHADER,
   "Tệp hiệu ứng đồ hoạ (Shader)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_REMAP,
   "Tệp gán nút điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Tập tin Cheat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Tệp lớp che phủ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_RDB,
   "Tệp cơ sở dữ liệu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_FONT,
   "Tệp phông chữ TrueType."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_PLAIN_FILE,
   "Tệp thường."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MOVIE_OPEN,
   "Video. Chọn để mở bằng trình phát video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_MUSIC_OPEN,
   "Nhạc. Chọn để mở bằng trình phát nhạc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE,
   "Tệp hình ảnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER,
   "Hình ảnh. Chọn để mở bằng trình xem ảnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION,
   "Core Libretro. Chọn để gán core này cho trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Core Libretro. Chọn để RetroArch tải Core này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_DIRECTORY,
   "Thư mục. Chọn để mở thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Điều tiết tốc độ khung hình."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Thay đổi cài đặt tua lại, chuyển tiếp và chuyển động chậm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Ghi hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Thay đổi cài đặt ghi hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Hiển thị trên màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Thay đổi hiển thị che phủ, bàn phím ảo và thiết lập thông báo trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Giao diện người dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Thay đổi cài đặt giao diện người dùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Dịch vụ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Thay đổi cài đặt cho dịch vụ AI (Dịch/TTS/Khác)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Khả năng truy cập"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Thay đổi cài đặt khả năng truy cập."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Quản lý năng lượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Thay đổi cài đặt quản lý năng lượng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Kích hoạt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Thay đổi cài đặt thành tựu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Mạng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Thay đổi cài đặt máy chủ và mạng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Thay đổi cài đặt danh sách chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Người dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Thay đổi cài đặt quyền riêng tư, tài khoản và tên người dùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Thư mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Thay đổi thư mục mặc định đặt file."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS,
   "Thủ thuật"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Gán phím"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Phương tiện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Hiệu năng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Âm thanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Thông số"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Lưu trữ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Đồng bộ thời gian"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_SETTINGS,
   "Nền tảng Steam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Thay đổi cài đặt liên quan đến Steam."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Trình điều khiển đầu vào cần sử dụng. Một số trình điều khiển video yêu cầu trình điều khiển đầu vào khác. (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_UDEV,
   "Trình điều khiển udev đọc các sự kiện evdev để hỗ trợ bàn phím. Nó cũng hỗ trợ lệnh gọi lại bàn phím, chuột và bàn di chuột.\nTheo mặc định trong hầu hết các bản phân phối, các nút /dev/input chỉ dành cho root (chế độ 600). Bạn có thể thiết lập một quy tắc udev để cho phép những người dùng không phải root có thể truy cập các nút này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_LINUXRAW,
   "Trình điều khiển đầu vào Linuxraw yêu cầu TTY đang hoạt động. Các sự kiện bàn phím được đọc trực tiếp từ TTY, giúp đơn giản hơn nhưng không linh hoạt bằng udev. Chuột, v.v. hoàn toàn không được hỗ trợ. Trình điều khiển này sử dụng API cần điều khiển cũ hơn (/dev/input/js*)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_DRIVER_NO_DETAILS,
   "Trình điều khiển đầu vào. Trình điều khiển video có thể buộc phải sử dụng trình điều khiển đầu vào khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Bộ điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Trình điều khiển bộ điều khiển để sử dụng. (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_DINPUT,
   "Trình điều khiển bộ điều khiển DirectInput."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_HID,
   "Trình điều khiển thiết bị giao diện người dùng cấp thấp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_LINUXRAW,
   "Trình điều khiển Linux thô, sử dụng API cần điều khiển cũ. Nếu có thể, hãy sử dụng udev."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_PARPORT,
   "Trình điều khiển Linux cho bộ điều khiển được kết nối qua cổng song song thông qua bộ điều hợp đặc biệt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_SDL,
   "Trình điều khiển bộ điều khiển dựa trên thư viện SDL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_UDEV,
   "Trình điều khiển bộ điều khiển với giao diện udev, thường được khuyến nghị. Sử dụng API evdev joypad mới nhất để hỗ trợ cần điều khiển. Nó hỗ trợ hotpluging và phản hồi cưỡng bức.\nTheo mặc định trong hầu hết các bản phân phối, các nút /dev/input chỉ dành cho root (chế độ 600). Bạn có thể thiết lập quy tắc udev để cho phép những nút này truy cập được bởi người dùng không ph[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_JOYPAD_DRIVER_XINPUT,
   "Trình điều khiển bộ điều khiển XInput. Chủ yếu dành cho bộ điều khiển XBox."
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Trình điều khiển video để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL1,
   "Trình điều khiển OpenGL 1.x. Phiên bản tối thiểu yêu cầu: OpenGL 1.1. Không hỗ trợ shader. Nếu có thể, hãy sử dụng trình điều khiển OpenGL mới hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL,
   "Trình điều khiển OpenGL 2.x. Trình điều khiển này cho phép sử dụng Core libretro GL bên cạnh Core render phần mềm. Phiên bản tối thiểu yêu cầu: OpenGL 2.0 hoặc OpenGLES 2.0. Hỗ trợ định dạng shader GLSL. Nếu có thể, hãy sử dụng trình điều khiển glcore."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "Trình điều khiển OpenGL 3.x. Trình điều khiển này cho phép sử dụng Core libretro GL bên cạnh Core render phần mềm. Phiên bản tối thiểu yêu cầu: OpenGL 3.2 hoặc OpenGLES 3.0 trở lên. Hỗ trợ định dạng bộ lọc Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Trình điều khiển Vulkan. Trình điều khiển này cho phép sử dụng Core Vulkan libretro bên cạnh lõi render phần mềm. Phiên bản tối thiểu yêu cầu: Vulkan 1.0. Hỗ trợ trình đổ bóng HDR và ​​Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "Trình điều khiển được kết xuất bằng phần mềm SDL 1.2. Hiệu suất được đánh giá là chưa tối ưu. Chỉ nên sử dụng nó như một giải pháp cuối cùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "Trình điều khiển kết xuất phần mềm SDL 2. Hiệu suất của các triển khai Core libretro kết xuất phần mềm phụ thuộc vào triển khai SDL trên nền tảng của bạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_METAL,
   "Trình điều khiển Metal dành cho nền tảng Apple. Hỗ trợ định dạng shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D8,
   "Trình điều khiển Direct3D 8 không hỗ trợ shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_CG,
   "Trình điều khiển Direct3D 9 hỗ trợ định dạng shader Cg cũ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D9_HLSL,
   "Trình điều khiển Direct3D 9 hỗ trợ định dạng shader HLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D10,
   "Trình điều khiển Direct3D 10 hỗ trợ định dạng shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D11,
   "Trình điều khiển Direct3D 11 hỗ trợ HDR và ​​định dạng shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_D3D12,
   "Trình điều khiển Direct3D 12 hỗ trợ HDR và ​​định dạng shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DISPMANX,
   "Trình điều khiển DispmanX. Sử dụng API DispmanX cho GPU Videocore IV trong Raspberry Pi 0..3. Không hỗ trợ lớp phủ hoặc shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_CACA,
   "Trình điều khiển LibCACA. Tạo ra ký tự thay vì đồ họa. Không khuyến khích sử dụng thực tế."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_EXYNOS,
   "Trình điều khiển video Exynos cấp thấp sử dụng khối G2D trong SoC Samsung Exynos cho các hoạt động xử lý nhanh. Hiệu suất cho các Core được kết xuất bằng phần mềm sẽ đạt mức tối ưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_DRM,
   "Trình điều khiển video DRM thuần túy. Đây là trình điều khiển video cấp thấp sử dụng libdrm để mở rộng phần cứng bằng cách sử dụng lớp phủ GPU."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SUNXI,
   "Trình điều khiển video Sunxi cấp thấp sử dụng khối G2D trong Allwinner SoC."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_WIIU,
   "Trình điều khiển Wii U. Hỗ trợ trình đổ bóng Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SWITCH,
   "Trình điều khiển chuyển đổi. Hỗ trợ định dạng shader GLSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VG,
   "Trình điều khiển OpenVG. Sử dụng API đồ họa vector 2D được tăng tốc bằng phần cứng OpenVG."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GDI,
   "Trình điều khiển GDI. Sử dụng giao diện Windows cũ. Không khuyến khích sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_NO_DETAILS,
   "Trình điều khiển video hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Trình điều khiển âm thanh để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_RSOUND,
   "Trình điều khiển RSound cho hệ thống âm thanh mạng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_OSS,
   "Trình điều khiển hệ thống âm thanh mở cũ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSA,
   "Trình điều khiển ALSA mặc định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ALSATHREAD,
   "Trình điều khiển ALSA có hỗ trợ luồng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_TINYALSA,
   "Trình điều khiển ALSA được triển khai mà không cần phụ thuộc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_ROAR,
   "Trình điều khiển hệ thống âm thanh RoarAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_AL,
   "Trình điều khiển OpenAL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_SL,
   "Trình điều khiển OpenSL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_DSOUND,
   "Trình điều khiển DirectSound. DirectSound chủ yếu được sử dụng từ Windows 95 đến Windows XP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_WASAPI,
   "Trình điều khiển API phiên âm thanh Windows. WASAPI chủ yếu được sử dụng từ Windows 7 trở lên."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PULSE,
   "Trình điều khiển PulseAudio. Nếu hệ thống sử dụng PulseAudio, hãy đảm bảo sử dụng trình điều khiển này thay vì ví dụ ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_PIPEWIRE,
   "Trình điều khiển PipeWire. Nếu hệ thống sử dụng PipeWire, hãy đảm bảo sử dụng trình điều khiển này thay vì ví dụ như PulseAudio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DRIVER_JACK,
   "Trình điều khiển Jack Audio Connection Kit."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DRIVER,
   "Micrô"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DRIVER,
   "Trình điều khiển micro để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_DRIVER,
   "Bộ lấy mẫu lại micrô"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_DRIVER,
   "Trình điều khiển lấy mẫu micrô để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_BLOCK_FRAMES,
   "Khung khối micrô"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Âm thanh Resampler Driver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Trình điều khiển lấy mẫu âm thanh để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_SINC,
   "Triển khai Windowed Sinc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_CC,
   "Triển khai Cosine phức tạp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RESAMPLER_DRIVER_NEAREST,
   "Triển khai lấy mẫu lại gần nhất. Bộ lấy mẫu lại này bỏ qua cài đặt chất lượng."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Trình điều khiển camera để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_DRIVER,
   "Kích hoạt Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Trình điều khiển Bluetooth để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
   "Trình điều khiển Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Trình điều khiển Wi-Fi để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Trình điều khiển vị trí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Trình điều khiển vị trí để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Trình điều khiển menu để sử dụng. (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_XMB,
   "XMB là giao diện người dùng đồ họa RetroArch trông giống như menu điều khiển thế hệ thứ 7. Nó có thể hỗ trợ các tính năng tương tự như Ozone."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_OZONE,
   "Ozone là giao diện người dùng đồ họa (GUI) mặc định của RetroArch trên hầu hết các nền tảng. Giao diện này được tối ưu hóa để điều hướng bằng tay cầm chơi game."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_RGUI,
   "RGUI là một giao diện người dùng đồ họa tích hợp đơn giản dành cho RetroArch. Nó có yêu cầu hiệu suất thấp nhất trong số các trình điều khiển menu và có thể sử dụng trên màn hình có độ phân giải thấp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MENU_DRIVER_MATERIALUI,
   "Trên thiết bị di động, RetroArch mặc định sử dụng giao diện người dùng di động MaterialUI. Giao diện này được thiết kế dành cho màn hình cảm ứng và các thiết bị trỏ, chẳng hạn như chuột/trackball."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Trình điều khiển ghi âm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Trình điều khiển ghi âm để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Trình điều khiển MIDI để sử dụng."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "Chế độ CRT SwitchRes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Xuất tín hiệu gốc, độ phân giải thấp để sử dụng với màn hình CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Đầu ra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Điều chỉnh thiết lập cho đầu ra video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Chế độ toàn màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Thay đổi cài đặt chế độ toàn màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Chế độ cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Thay đổi cài đặt chế độ cửa sổ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Phóng to/Thu nhỏ hình ảnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Điều chỉnh màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_SETTINGS,
   "Chế độ HDR"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Thay đổi cài đặt HDR của video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Đồng bộ âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Thay đổi cài đặt đồng bộ hóa video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Tạm dừng trình bảo vệ màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Ngăn không cho trình bảo vệ màn hình của hệ thống hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SUSPEND_SCREENSAVER_ENABLE,
   "Tạm dừng trình bảo vệ màn hình. Đây là một gợi ý không nhất thiết phải được trình điều khiển video tuân thủ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Video có luồng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Cải thiện hiệu suất nhưng sẽ làm giảm độ trễ và video bị giật. Chỉ sử dụng nếu không thể đạt được tốc độ tối đa bằng cách khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_THREADED,
   "Sử dụng trình điều khiển video luồng. Việc này có thể cải thiện hiệu suất nhưng có thể gây ra độ trễ và hiện tượng giật hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Chèn khung đen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "CẢNH BÁO: Hiện tượng nhấp nháy nhanh có thể gây ra hiện tượng lưu ảnh trên một số màn hình. Tự chịu rủi ro khi sử dụng // Chèn khung đen giữa các khung hình. Có thể giảm đáng kể hiện tượng nhòe chuyển động bằng cách mô phỏng quét CRT, nhưng độ sáng sẽ giảm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BLACK_FRAME_INSERTION,
   "Chèn khung hình đen vào giữa các khung hình để tăng cường độ rõ nét của chuyển động. Chỉ sử dụng tùy chọn được chỉ định cho tần số quét màn hình hiện tại của bạn. Không sử dụng ở tần số quét không phải bội số của 60Hz, chẳng hạn như 144Hz, 165Hz, v.v. Không kết hợp với Swap Interval > 1, khung hình phụ, Frame Delay hoặc Sync to Exact Content Framerate. Bật VRR hệ thống là được, nhưng kh[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BFI_DARK_FRAMES,
   "Chèn khung đen - Khung tối"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BFI_DARK_FRAMES,
   "Điều chỉnh số khung hình đen trong tổng số chuỗi quét BFI. Nhiều hơn thì độ rõ nét chuyển động cao hơn, ít hơn thì độ sáng cao hơn. Không áp dụng ở 120Hz vì chỉ có 1 khung hình BFI để làm việc. Cài đặt cao hơn mức có thể sẽ giới hạn bạn ở mức tối đa có thể cho tần số quét bạn đã chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_BFI_DARK_FRAMES,
   "Điều chỉnh số khung hình hiển thị màu đen trong chuỗi BFI. Nhiều khung hình màu đen hơn sẽ làm tăng độ rõ nét của chuyển động nhưng lại làm giảm độ sáng. Không áp dụng ở 120Hz vì chỉ có tổng cộng một khung hình 60Hz bổ sung, do đó khung hình đó phải có màu đen, nếu không BFI sẽ không hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_SUBFRAMES,
   "Khung phụ bộ lọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "CẢNH BÁO: Hiện tượng nhấp nháy nhanh có thể gây ra hiện tượng lưu ảnh trên một số màn hình. Tự chịu rủi ro khi sử dụng // Mô phỏng một đường quét cơ bản trên nhiều khung hình phụ bằng cách chia màn hình theo chiều dọc và hiển thị từng phần của màn hình theo số lượng khung hình phụ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Chèn thêm khung đổ bóng vào giữa các khung hình để tạo hiệu ứng đổ bóng có thể chạy nhanh hơn tốc độ trò chơi. Chỉ sử dụng tùy chọn được chỉ định cho tốc độ làm mới màn hình hiện tại của bạn. Không sử dụng ở tốc độ làm mới không phải bội số của 60Hz, chẳng hạn như 144Hz, 165Hz, v.v. Không kết hợp với Swap Interval > 1, BFI, Frame Delay hoặc Sync to Exact Content Framerate. Có thể b[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Chụp màn hình bằng GPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCAN_SUBFRAMES,
   "Mô phỏng đường quét lăn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCAN_SUBFRAMES,
   "CẢNH BÁO: Hiện tượng nhấp nháy nhanh có thể gây ra hiện tượng lưu ảnh trên một số màn hình. Tự chịu rủi ro khi sử dụng // Mô phỏng một đường quét cơ bản trên nhiều khung hình phụ bằng cách chia màn hình theo chiều dọc và hiển thị từng phần của màn hình theo số lượng khung hình phụ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SCAN_SUBFRAMES,
   "Mô phỏng đường quét cơ bản trên nhiều khung hình phụ bằng cách chia màn hình theo chiều dọc và hiển thị từng phần của màn hình theo số lượng khung hình phụ tính từ trên xuống dưới của màn hình."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Ảnh chụp màn hình sẽ ghi lại hình ảnh đổ bóng GPU nếu có."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Lọc song tuyến tính"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Thêm hiệu ứng làm mờ nhẹ cho hình ảnh để làm mềm các cạnh pixel cứng. Tùy chọn này hầu như không ảnh hưởng đến hiệu suất. Nên tắt nếu sử dụng shader."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Nội suy hình ảnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Chỉ định phương pháp nội suy hình ảnh khi chia tỷ lệ trò chơi thông qua IPU nội bộ. Khuyến nghị sử dụng 'Bo tròn cạnh' hoặc 'Làm mịn' khi sử dụng bộ lọc video chạy bằng CPU. Tùy chọn này không ảnh hưởng đến hiệu suất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bo tròn cạnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Làm mịn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Pixel gốc"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Nội suy hình ảnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Chỉ định phương pháp nội suy hình ảnh khi 'Tỷ lệ số nguyên' bị tắt. 'Láng giềng gần nhất' có tác động ít nhất đến hiệu suất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Pixel gốc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Làm mịn nhẹ"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Tự động hoãn bộ lọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Hoãn việc tự động tải shader (tính bằng ms). Có thể khắc phục lỗi đồ họa khi sử dụng phần mềm 'chụp màn hình'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Bộ lọc video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Áp dụng bộ lọc video chạy bằng CPU. Có thể phải trả giá bằng hiệu năng cao. Một số bộ lọc video có thể chỉ hoạt động với Core sử dụng màu 32-bit hoặc 16-bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Áp dụng bộ lọc video chạy bằng CPU. Có thể phải trả giá bằng hiệu năng cao. Một số bộ lọc video có thể chỉ hoạt động với Core sử dụng màu 32-bit hoặc 16-bit. Có thể chọn thư viện bộ lọc video được liên kết động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Áp dụng bộ lọc video chạy bằng CPU. Có thể phải trả giá bằng hiệu năng cao. Một số bộ lọc video có thể chỉ hoạt động với Core sử dụng màu 32-bit hoặc 16-bit. Có thể chọn thư viện bộ lọc video tích hợp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Xóa bộ lọc video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Gỡ bỏ bất kỳ bộ lọc video nào đang hoạt động do CPU cung cấp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Bật chế độ toàn màn hình trên thiết bị Android và iOS có “notch”"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
   "Sử dụng Bộ đệm đối số kim loại (Yêu cầu khởi động lại)"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS,
   "Hãy thử cải thiện hiệu suất bằng cách sử dụng bộ đệm tham số Metal. Một số Core có thể yêu cầu điều này. Điều này có thể làm hỏng một số shader, đặc biệt là trên phần cứng hoặc phiên bản hệ điều hành cũ."
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "Chuyển độ phân giải CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Chỉ dành cho màn hình CRT. Cố gắng sử dụng độ phân giải Core/trò chơi và tốc độ làm mới chính xác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "Độ phân giải siêu cao CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Chuyển đổi giữa độ phân giải gốc và độ phân giải siêu rộng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Căn giữa theo chiều ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Duyệt qua các tùy chọn này nếu hình ảnh không được căn giữa đúng cách trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Kích thước ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Duyệt qua các tùy chọn này để điều chỉnh cài đặt theo chiều ngang nhằm thay đổi kích thước hình ảnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_VERTICAL_ADJUST,
   "Căn giữa theo chiều dọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_VERTICAL_ADJUST,
   "Duyệt qua các tùy chọn này nếu hình ảnh không được căn giữa đúng cách trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Sử dụng Menu độ phân giải cao"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Chuyển sang chế độ hiển thị độ phân giải cao để dùng với menu độ phân giải cao khi chưa mở trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Tốc độ làm mới tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Sử dụng tốc độ làm mới tùy chỉnh được chỉ định trong tệp cấu hình nếu cần."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Chỉ số giám sát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Chọn màn hình hiển thị để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MONITOR_INDEX,
   "Nên chọn màn hình nào. 0 (mặc định) nghĩa là không có màn hình cụ thể nào được ưu tiên, 1 trở lên (1 là màn hình đầu tiên), gợi ý RetroArch sử dụng màn hình cụ thể đó."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Tối ưu hóa cho Wii U GamePad (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Sử dụng tỷ lệ chính xác 2x của GamePad làm khung nhìn. Tắt để hiển thị ở độ phân giải TV gốc."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Xoay video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Buộc video phải xoay một góc nhất định. Góc xoay này sẽ được thêm vào các góc xoay mà Core thiết lập."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Hướng màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Buộc hệ điều hành phải điều chỉnh màn hình theo một hướng nhất định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Chỉ số GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Chọn loại card đồ họa sẽ sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Độ lệch ngang của màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Áp dụng một độ lệch nhất định theo chiều ngang cho video. Độ lệch này được áp dụng trên toàn bộ video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Độ lệch dọc màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Áp dụng một độ lệch nhất định theo chiều dọc cho Video. Độ lệch này được áp dụng trên toàn bộ Video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Tốc độ làm mới theo chiều dọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Tốc độ làm mới theo chiều dọc của màn hình. Được sử dụng để tính toán tốc độ đầu vào âm thanh phù hợp.\nThông tin này sẽ bị bỏ qua nếu 'Video luồng' được bật."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Tốc độ làm mới màn hình ước tính"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Tốc độ làm mới ước tính chính xác của màn hình tính bằng Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_REFRESH_RATE_AUTO,
   "Tần số quét chính xác của màn hình (Hz). Thông số này được sử dụng để tính toán tần số đầu vào âm thanh theo công thức:\naudio_input_rate = tần số đầu vào của trò chơi * tần số quét màn hình / tần số quét trò chơi\nNếu Core không báo cáo bất kỳ giá trị nào, giá trị mặc định của NTSC sẽ được sử dụng để đảm bảo tính tương thích.\nGiá trị này nên gần 60Hz để tránh thay đổi ca[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Đặt tốc độ làm mới được báo cáo trên màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "Tốc độ làm mới được trình điều khiển màn hình báo cáo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Chuyển đổi tốc độ làm mới tự động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Tự động chuyển đổi tốc độ làm mới màn hình dựa trên trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Chỉ ở chế độ toàn màn hình độc quyền"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Chỉ ở chế độ toàn màn hình dạng cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Tất cả các chế độ toàn màn hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Ngưỡng tốc độ làm mới tự động PAL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_PAL_THRESHOLD,
   "Tốc độ làm mới tối đa được coi là PAL."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Tốc độ làm mới theo chiều dọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Đặt tốc độ làm mới theo chiều dọc của màn hình. '50 Hz' sẽ cho phép video mượt mà khi chạy trò chơi hệ PAL."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Buộc vô hiệu hóa sRBG FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Tắt hỗ trợ sRGB FBO một cách cưỡng bức. Một số trình điều khiển Intel OpenGL trên Windows gặp sự cố video với sRGB FBO. Bật tùy chọn này có thể khắc phục được."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Hiển thị toàn màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Hiển thị toàn màn hình. Có thể thay đổi khi chạy. Có thể ghi đè bằng lệnh chuyển đổi dòng lệnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Chế độ toàn màn hình trong khung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Nếu toàn màn hình, hãy sử dụng cửa sổ toàn màn hình để tránh việc chuyển đổi chế độ hiển thị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Chiều rộng toàn màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Đặt kích thước chiều rộng tùy chỉnh cho chế độ toàn màn hình không có cửa sổ. Nếu không đặt, độ phân giải màn hình sẽ bị ảnh hưởng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Chiều cao toàn màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Đặt kích thước chiều cao tùy chỉnh cho chế độ toàn màn hình không có cửa sổ. Nếu không đặt, độ phân giải màn hình sẽ bị ảnh hưởng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Buộc giải quyết trên UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Buộc độ phân giải ở kích thước toàn màn hình, nếu đặt thành 0, giá trị cố định 3840 x 2160 sẽ được sử dụng."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Phóng to/Thu nhỏ cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Đặt kích thước cửa sổ theo bội số đã chỉ định của kích thước khung nhìn Core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Độ mờ của cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Đặt độ trong suốt của cửa sổ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Hiển thị trang trí cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Hiển thị thanh tiêu đề và đường viền cửa sổ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Hiển thị thanh menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Hiển thị thanh menu cửa sổ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Ghi nhớ vị trí và kích thước cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Hiển thị toàn bộ trò chơi trong một cửa sổ có kích thước cố định được chỉ định bởi 'Chiều rộng cửa sổ' và 'Chiều cao cửa sổ', đồng thời lưu kích thước và vị trí cửa sổ hiện tại khi đóng RetroArch. Khi tắt, kích thước cửa sổ sẽ được thiết lập động dựa trên 'Tỷ lệ cửa sổ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Sử dụng Kích thước cửa sổ tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Hiển thị toàn bộ trò chơi trong một cửa sổ có kích thước cố định được chỉ định bởi 'Chiều rộng cửa sổ' và 'Chiều cao cửa sổ'. Khi tắt, kích thước cửa sổ sẽ được thiết lập động dựa trên 'Tỷ lệ cửa sổ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Chiều rộng cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Đặt chiều rộng tùy chỉnh cho cửa sổ hiển thị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Chiều cao cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Đặt chiều cao tùy chỉnh cho cửa sổ hiển thị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Chiều rộng cửa sổ tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Đặt chiều rộng tối đa của cửa sổ hiển thị khi tự động thay đổi kích thước dựa trên 'Tỷ lệ cửa sổ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Chiều cao cửa sổ tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Đặt chiều cao tối đa của cửa sổ hiển thị khi tự động thay đổi kích thước dựa trên 'Tỷ lệ cửa sổ'."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Tỷ lệ số nguyên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Chỉ chia tỷ lệ video theo số nguyên. Kích thước cơ sở phụ thuộc vào hình học được báo cáo trên Core và tỷ lệ khung hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_AXIS,
   "Trục tỷ lệ số nguyên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_AXIS,
   "Tỷ lệ chiều cao hoặc chiều rộng, hoặc cả chiều cao và chiều rộng. Nửa bước chỉ áp dụng cho các nguồn có độ phân giải cao."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING,
   "Tỷ lệ số nguyên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_SCALING,
   "Làm tròn xuống hoặc lên đến hệ số nguyên tiếp theo. Chế độ “Thông minh” sẽ giảm xuống mức thu nhỏ khi hình ảnh bị cắt quá nhiều, và cuối cùng quay về thu phóng không nguyên nếu phần bị thu nhỏ trở nên quá lớn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_UNDERSCALE,
   "Thu nhỏ hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_OVERSCALE,
   "Quá khổ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_SCALING_SMART,
   "Thông minh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Tỷ lệ khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Đặt tỷ lệ khung hình hiển thị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Cấu hình tỷ lệ khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Giá trị dấu phẩy động cho tỷ lệ khung hình video (chiều rộng / chiều cao)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Theo cấu hình mặc định"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_SQUARE_PIXEL,
   "Pixel vuông (hiển thị %u:%u)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Core được cung cấp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Toàn màn hình"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Giữ nguyên tỷ lệ khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Duy trì tỷ lệ khung hình 1:1 khi chia tỷ lệ trò chơi thông qua IPU nội bộ. Nếu tắt, hình ảnh sẽ được kéo giãn để lấp đầy toàn bộ màn hình."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Tỷ lệ khung hình tùy chỉnh (Vị trí X)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Độ lệch khung nhìn tùy chỉnh được sử dụng để xác định vị trí trục X của khung nhìn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Tỷ lệ khung hình tùy chỉnh (Vị trí Y)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Độ lệch khung nhìn tùy chỉnh được sử dụng để xác định vị trí trục Y của khung nhìn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_X,
   "Độ lệch neo khung nhìn theo trục X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_X,
   "Độ lệch neo khung nhìn theo trục X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Độ lệch neo khung nhìn theo trục Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_Y,
   "Độ lệch neo khung nhìn theo trục Y"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_X,
   "Vị trí theo chiều ngang của trò chơi khi khung nhìn rộng hơn chiều rộng của nội dung. 0,0 là cực trái, 0,5 là ở giữa, 1,0 là cực phải."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Vị trí theo chiều dọc của trò chơi khi khung nhìn cao hơn chiều cao của nội dung. 0,0 là trên cùng, 0,5 là ở giữa, 1,0 là dưới cùng."
   )
#if defined(RARCH_MOBILE)
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Độ lệch neo khung nhìn theo trục X (Dọc)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Độ lệch neo khung nhìn theo trục X (Dọc)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Độ lệch neo khung nhìn theo trục Y (Dọc)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Độ lệch neo khung nhìn theo trục Y (Dọc)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_X,
   "Vị trí theo chiều ngang của trò chơi khi khung nhìn rộng hơn chiều rộng nội dung. 0,0 là cực trái, 0,5 là ở giữa, 1,0 là cực phải. (Hướng dọc)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Vị trí theo chiều dọc của trò chơi khi khung nhìn cao hơn chiều cao của nội dung. 0,0 là trên cùng, 0,5 là ở giữa, 1,0 là dưới cùng. (Hướng dọc)"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Tỷ lệ khung hình tùy chỉnh (Chiều rộng)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Chiều rộng khung nhìn tùy chỉnh được sử dụng nếu Tỷ lệ khung hình được đặt thành 'Tỷ lệ khung hình tùy chỉnh'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Tỷ lệ khung hình tùy chỉnh (Chiều cao)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Chiều cao khung nhìn tùy chỉnh được sử dụng nếu Tỷ lệ khung hình được đặt thành 'Tỷ lệ khung hình tùy chỉnh'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Cắt quét quá mức (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Cắt bỏ một vài điểm ảnh xung quanh các cạnh của hình ảnh mà các nhà phát triển thường để trống, đôi khi cũng chứa các điểm ảnh rác."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "Bật HDR"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Bật HDR nếu màn hình hỗ trợ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Độ sáng tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Đặt độ sáng tối đa (tính bằng cd/m2) mà màn hình của bạn có thể tái tạo. Xem RTings để biết độ sáng tối đa của màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Độ sáng trắng chuẩn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Thiết lập độ sáng trắng chuẩn cần đạt được, tức là văn bản có thể đọc được hoặc độ sáng ở mức cao nhất của dải SDR (Dải động tiêu chuẩn). Hữu ích để điều chỉnh theo các điều kiện ánh sáng khác nhau trong môi trường của bạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "Tương phản"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "Kiểm soát gamma/độ tương phản cho HDR. Lấy màu sắc và tăng phạm vi tổng thể giữa các vùng sáng nhất và tối nhất của hình ảnh. Độ tương phản HDR càng cao, sự chênh lệch này càng lớn, trong khi độ tương phản càng thấp, hình ảnh càng bị nhạt màu. Giúp người dùng tinh chỉnh hình ảnh theo ý thích và cảm thấy hình ảnh hiển thị đẹp nhất trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Mở rộng Gam màu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Sau khi không gian màu được chuyển đổi thành không gian tuyến tính, hãy quyết định xem chúng ta có nên sử dụng gam màu mở rộng để đạt được HDR10 hay không."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Đồng bộ dọc (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Đồng bộ hóa video đầu ra của card đồ họa với tốc độ làm mới của màn hình. Khuyến nghị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Thời gian hoán đổi VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Sử dụng khoảng thời gian hoán đổi tùy chỉnh cho VSync. Giảm hiệu quả tốc độ làm mới màn hình theo hệ số đã chỉ định. Chế độ 'Tự động' đặt hệ số dựa trên tốc độ khung hình do Core báo cáo, cải thiện tốc độ khung hình khi chạy, ví dụ: trò chơi 30 khung hình/giây trên màn hình 60 Hz hoặc nội dung 60 khung hình/giây trên màn hình 120 Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "VSync thích ứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "VSync được bật cho đến khi hiệu suất giảm xuống dưới tốc độ làm mới mục tiêu. Có thể giảm thiểu hiện tượng giật hình khi hiệu suất giảm xuống dưới thời gian thực và tiết kiệm năng lượng hơn. Không tương thích với 'Độ trễ khung hình'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Độ trễ khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Giảm độ trễ nhưng lại làm tăng nguy cơ video bị giật."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY,
   "Thiết lập số mili giây để ngủ trước khi chạy Core sau khi trình chiếu video. Giảm độ trễ nhưng sẽ tăng nguy cơ giật hình.\nGiá trị 20 trở lên được coi là phần trăm thời gian khung hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Tự động trì hoãn khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Điều chỉnh 'Độ trễ khung hình' hiệu quả một cách linh hoạt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FRAME_DELAY_AUTO,
   "Cố gắng duy trì mục tiêu 'Trễ khung hình' mong muốn và giảm thiểu tình trạng mất khung hình. Điểm khởi đầu là 3/4 thời gian khung hình khi 'Trễ khung hình' bằng 0 (Tự động)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "Độ trễ khung hình thực tế"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Đồng bộ GPU cứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Đồng bộ cứng CPU và GPU. Giảm độ trễ nhưng giảm hiệu suất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Khung đồng bộ GPU cứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Đặt số khung hình mà CPU có thể chạy trước GPU khi sử dụng 'Đồng bộ GPU cứng'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_HARD_SYNC_FRAMES,
   "Thiết lập số khung hình CPU có thể chạy trước GPU khi sử dụng 'GPU Hard Sync'. Tối đa là 3.\n 0: Đồng bộ với GPU ngay lập tức.\n 1: Đồng bộ với khung hình trước đó.\n 2: Vân vân ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Đồng bộ với tốc độ khung hình trò chơi chính xác (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Không sai lệch so với thời gian yêu cầu của Core. Sử dụng cho màn hình Tốc độ làm mới thay đổi (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Đồng bộ với Tốc độ Khung hình trò chơi Chính xác. Tùy chọn này tương đương với việc ép tốc độ x1 trong khi vẫn cho phép tua nhanh. Không lệch so với tốc độ làm mới yêu cầu của Core, không có Kiểm soát Tốc độ Động."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Đầu ra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Điều chỉnh đầu ra cho âm thanh."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Micrô"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Thay đổi cài đặt điều khiển âm thanh."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Bộ lấy mẫu lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Thay đổi cài đặt bộ lấy mẫu âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Đồng bộ âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Thay đổi cài đặt đồng bộ hóa âm thanh."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Thay đổi cài đặt MIDI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Bộ trộn âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Thay đổi cài đặt bộ trộn âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Menu âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Thay đổi cài đặt menu âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Tắt tiếng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Tắt tiếng âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Tắt bộ trộn âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Tắt tiếng bộ trộn âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESPECT_SILENT_MODE,
   "Tuân theo chế độ im lặng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESPECT_SILENT_MODE,
   "Tắt toàn bộ âm thanh ở Chế độ im lặng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Tắt tiếng âm thanh tua nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Tự động tắt tiếng khi sử dụng chế độ tua đi nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_SPEEDUP,
   "Tăng tốc âm thanh tua nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_SPEEDUP,
   "Tăng tốc âm thanh khi tua nhanh. Ngăn tiếng rè nhưng thay đổi cao độ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_REWIND_MUTE,
   "Tắt tiếng khi tua ngược"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_REWIND_MUTE,
   "Tự động tắt tiếng khi sử dụng chế độ tua ngược."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Tăng/giảm âm lượng (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Âm lượng âm thanh (tính bằng dB). 0 dB là âm lượng bình thường và không áp dụng bất kỳ mức khuếch đại nào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Âm lượng, được biểu thị bằng dB. 0 dB là âm lượng bình thường, không áp dụng độ khuếch đại. Độ khuếch đại có thể được điều khiển trong thời gian chạy bằng cách tăng âm lượng điều khiển/giảm âm lượng đầu vào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Tăng/giảm âm lượng bộ trộn âm thanh (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Âm lượng tổng thể của bộ trộn âm thanh (tính bằng dB). 0 dB là âm lượng bình thường và không áp dụng mức khuếch đại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Mô-đun xử lý âm thanh số"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Mô-đun DSP âm thanh xử lý âm thanh trước khi gửi đến trình điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Xóa plugin DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Gỡ bỏ bất kỳ plugin DSP âm thanh nào đang hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Chế độ độc quyền WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Cho phép trình điều khiển WASAPI kiểm soát độc quyền thiết bị âm thanh. Nếu tắt, nó sẽ sử dụng chế độ chia sẻ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Định dạng số thực WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Sử dụng định dạng số thực cho trình điều khiển WASAPI nếu thiết bị âm thanh của bạn hỗ trợ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Chiều dài bộ đệm chia sẻ WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Chiều dài bộ đệm trung gian (tính theo khung) khi sử dụng trình điều khiển WASAPI ở chế độ chia sẻ."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Bật đầu ra âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Thiết bị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Ghi đè thiết bị âm thanh mặc định mà trình điều khiển âm thanh sử dụng. Điều này phụ thuộc vào trình điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE,
   "Ghi đè thiết bị âm thanh mặc định mà trình điều khiển âm thanh sử dụng. Điều này phụ thuộc vào trình điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_ALSA,
   "Giá trị thiết bị PCM tùy chỉnh cho trình điều khiển ALSA."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_OSS,
   "Giá trị đường dẫn tùy chỉnh cho trình điều khiển OSS (ví dụ: /dev/dsp)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_JACK,
   "Giá trị tên cổng tùy chỉnh cho trình điều khiển JACK (ví dụ: system:playback1,system:playback_2)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_DEVICE_RSOUND,
   "Địa chỉ IP tùy chỉnh của máy chủ RSound cho trình điều khiển RSound."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Độ trễ âm thanh (mili giây)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Độ trễ âm thanh tối đa tính bằng mili giây. Trình điều khiển đặt mục tiêu duy trì độ trễ thực tế ở mức 50% giá trị này. Có thể không được chấp nhận nếu trình điều khiển âm thanh không thể cung cấp độ trễ nhất định."
   )

#ifdef HAVE_MICROPHONE
/* Settings > Audio > Input */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_ENABLE,
   "Micrô"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_ENABLE,
   "Cho phép điều khiển âm thanh trong các Core được hỗ trợ. Không có chi phí phát sinh nếu lõi không sử dụng micrô."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Thiết bị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Ghi đè thiết bị điều khiển mặc định mà trình điều khiển micrô sử dụng. Điều này phụ thuộc vào trình điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Ghi đè thiết bị điều khiển mặc định mà trình điều khiển micrô sử dụng. Điều này phụ thuộc vào trình điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Chất lượng tái tạo mẫu âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Giảm giá trị này để ưu tiên hiệu suất/độ trễ thấp hơn chất lượng âm thanh, tăng giá trị này để có chất lượng âm thanh tốt hơn nhưng phải đánh đổi bằng hiệu suất/độ trễ thấp hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Tốc độ điều khiển mặc định (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Tốc độ mẫu điều khiển âm thanh, được sử dụng nếu Core không yêu cầu số lượng cụ thể."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Độ trễ điều khiển âm thanh (mili giây)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Độ trễ điều khiển âm thanh mong muốn tính bằng mili giây. Có thể không được đáp ứng nếu trình điều khiển micrô không cung cấp được độ trễ mong muốn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Chế độ độc quyền WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_EXCLUSIVE_MODE,
   "Cho phép RetroArch kiểm soát độc quyền thiết bị micrô khi sử dụng trình điều khiển micrô WASAPI. Nếu tắt, RetroArch sẽ sử dụng chế độ chia sẻ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Định dạng nổi WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_FLOAT_FORMAT,
   "Sử dụng điều khiển dấu chấm động cho trình điều khiển WASAPI, nếu thiết bị âm thanh của bạn hỗ trợ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Chiều dài bộ đệm chia sẻ WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Chiều dài bộ đệm trung gian (tính theo khung) khi sử dụng trình điều khiển WASAPI ở chế độ chia sẻ."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Chất lượng tái tạo mẫu âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Giảm giá trị này để ưu tiên hiệu suất/độ trễ thấp hơn chất lượng âm thanh, tăng giá trị này để có chất lượng âm thanh tốt hơn nhưng phải đánh đổi bằng hiệu suất/độ trễ thấp hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Tốc độ đầu ra (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Tốc độ lấy mẫu đầu ra âm thanh."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Đồng bộ âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Đồng bộ âm thanh. Khuyến nghị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Độ lệch thời gian tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Mức thay đổi tối đa trong tốc độ điều khiển âm thanh. Việc tăng mức này sẽ dẫn đến những thay đổi rất lớn về thời gian, nhưng sẽ làm âm thanh không chính xác (ví dụ: chạy Core PAL trên màn hình NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Độ lệch thời gian âm thanh tối đa.\nXác định mức thay đổi tối đa của tốc độ điều khiển. Bạn có thể muốn tăng giá trị này để cho phép thay đổi thời gian rất lớn, ví dụ như chạy Core PAL trên màn hình NTSC, với chi phí là độ cao âm thanh không chính xác.\nTốc độ điều khiển được định nghĩa là:\ntốc độ đầu vào * (1,0 +/- (độ lệch thời gian tối đa))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Tự động điều chỉnh tốc độ âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Giúp làm mịn các điểm không đồng bộ về thời gian khi đồng bộ hóa âm thanh và video. Lưu ý rằng nếu tắt, việc đồng bộ hóa chính xác gần như không thể thực hiện được."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Đặt giá trị này thành 0 sẽ vô hiệu hóa điều khiển tốc độ. Bất kỳ giá trị nào khác sẽ kiểm soát delta điều khiển tốc độ âm thanh.\nXác định mức độ tốc độ điều khiển có thể được điều chỉnh động. Tốc độ điều khiển được định nghĩa là:\ntốc độ điều khiển * (1,0 +/- (delta điều khiển tốc độ))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Nhập trình điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Chọn thiết bị điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Thiết lập thiết bị điều khiển (tùy thuộc vào trình điều khiển). Khi được đặt thành \"Tắt\", đầu vào MIDI sẽ bị tắt. Bạn cũng có thể nhập tên thiết bị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Đầu ra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Chọn thiết bị đầu ra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_OUTPUT,
   "Thiết lập thiết bị đầu ra (tùy thuộc vào trình điều khiển). Khi được đặt thành \"Tắt\", đầu ra MIDI sẽ bị tắt. Bạn cũng có thể nhập tên thiết bị.\nKhi đầu ra MIDI được bật và Core và trò chơi/ứng dụng hỗ trợ đầu ra MIDI, một số hoặc tất cả âm thanh (tùy thuộc vào trò chơi/ứng dụng) sẽ được tạo ra bởi thiết bị MIDI. Trong trường hợp trình điều khiển MIDI \"null\", điều nà[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Âm lượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Đặt âm lượng đầu ra (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Luồng trộn âm thanh #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Sẽ bắt đầu phát lại luồng âm thanh. Sau khi hoàn tất, luồng âm thanh hiện tại sẽ bị xóa khỏi bộ nhớ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Phát (Lặp lại)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Sẽ bắt đầu phát lại luồng âm thanh. Sau khi hoàn tất, nó sẽ lặp lại và phát lại bản nhạc từ đầu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Phát (Tuần tự)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Sẽ bắt đầu phát lại luồng âm thanh. Sau khi hoàn tất, nó sẽ chuyển sang luồng âm thanh tiếp theo theo thứ tự và lặp lại hành vi này. Hữu ích khi sử dụng như chế độ phát lại album."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Dừng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Thao tác này sẽ dừng phát lại luồng âm thanh, nhưng không xóa nó khỏi bộ nhớ. Bạn có thể bắt đầu lại bằng cách chọn \"Phát\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Xóa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Thao tác này sẽ dừng phát lại luồng âm thanh và xóa hoàn toàn khỏi bộ nhớ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Âm lượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Điều chỉnh âm lượng của luồng âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Trạng thái: Không khả dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Trạng thái: Đã dừng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Trạng thái: Đang phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Trạng thái: Đang phát (Lặp lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Trạng thái: Đang phát (Tuần tự)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Bộ trộn âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Phát nhiều luồng âm thanh cùng lúc ngay cả trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Bật âm thanh 'OK'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Bật âm thanh 'Hủy'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Bật âm thanh 'Thông báo'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Bật âm thanh 'BGM'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_SCROLL,
   "Bật âm thanh 'Cuộn'"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Người dùng tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Số lượng người dùng tối đa được RetroArch hỗ trợ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Hành vi thăm dò (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Ảnh hưởng đến cách thực hiện thăm dò điều khiển trong RetroArch. Đặt thành \"Sớm\" hoặc \"Muộn\" có thể giảm độ trễ, tùy thuộc vào cấu hình của bạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Ảnh hưởng đến cách thực hiện thăm dò đầu vào bên trong RetroArch.\nSớm - Thăm dò điều khiển được thực hiện trước khi khung được xử lý.\nBình thường - Thăm dò điều khiển được thực hiện khi có yêu cầu thăm dò.\nTrễ - Thăm dò điều khiển được thực hiện khi có yêu cầu trạng thái điều khiển đầu tiên trên mỗi khung.\nĐặt thành 'Sớm' hoặc 'Trễ' có thể giảm độ trễ, tùy thuộc [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Gán lại phím điều khiển cho Core này"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Ghi đè cấu hình điều khiển bằng thiết lập gán phím đã lưu cho Core hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Gán lại nút cho tay cầm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_SORT_BY_CONTROLLER_ENABLE,
   "Cấu hình gán nút chỉ áp dụng cho tay cầm đang hoạt động lúc lưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Cấu hình tự động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Tự động cấu hình bộ điều khiển có cấu hình theo kiểu Cắm và Chạy."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Tắt phím tắt Windows (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Giữ tổ hợp phím Win bên trong ứng dụng."
   )
#endif
#ifdef ANDROID
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Chọn Bàn phím vật lý"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Sử dụng thiết bị này như một bàn phím vật lý chứ không phải như một tay cầm chơi game."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_SELECT_PHYSICAL_KEYBOARD,
   "Nếu RetroArch xác định bàn phím phần cứng là một loại tay cầm chơi game, bạn có thể sử dụng cài đặt này để buộc RetroArch coi thiết bị bị xác định nhầm đó là bàn phím.\nĐiều này có thể hữu ích nếu bạn đang cố gắng mô phỏng máy tính trên một số thiết bị Android TV và cũng sở hữu bàn phím vật lý có thể gắn vào hộp."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Điều khiển cảm biến phụ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Cho phép nhập dữ liệu từ cảm biến gia tốc, con quay hồi chuyển và cảm biến độ sáng, nếu được phần cứng hiện tại hỗ trợ. Có thể ảnh hưởng đến hiệu suất và/hoặc tăng mức tiêu thụ điện năng trên một số nền tảng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Tự động bắt chuột"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Bật tính năng bắt chuột khi tập trung vào ứng dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Tự động bật chế độ 'Tập trung vào trò chơi'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Luôn bật chế độ \"Tập trung vào trò chơi\" khi khởi chạy và tiếp tục nội dung. Khi được đặt thành \"Phát hiện\", tùy chọn sẽ được bật nếu Core hiện tại triển khai chức năng gọi lại bàn phím giao diện người dùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "BẬT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Phát hiện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Tạm dừng nội dung khi ngắt kết nối bộ điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Tạm dừng trò chơi khi bất kỳ bộ điều khiển nào bị ngắt kết nối. Tiếp tục bằng nút Bắt đầu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Ngưỡng trục nút điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Độ nghiêng của trục phải lớn đến mức nào để có thể nhấn nút khi sử dụng 'Chuyển đổi từ Analog sang Digital'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Vùng chết tương tự"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Bỏ qua chuyển động của cần analog dưới giá trị vùng chết."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Độ nhạy cần Analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Độ nhạy của gia tốc kế"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
   "Độ nhạy của con quay hồi chuyển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Điều chỉnh độ nhạy của cần điều khiển analog."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_ACCELEROMETER_SENSITIVITY,
   "Điều chỉnh độ nhạy của cảm biến gia tốc."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSOR_GYROSCOPE_SENSITIVITY,
  "Điều chỉnh độ nhạy của con quay hồi chuyển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Thời gian chờ gán nút bấm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Số giây phải chờ cho đến khi chuyển sang lần ràng buộc tiếp theo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Thời gian giữ nút"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Khoảng thời gian nhấn giữ nút để ràng buộc nó."
   )
MSG_HASH(
   MSG_INPUT_BIND_PRESS,
   "Nhấn bàn phím, chuột hoặc bộ điều khiển"
   )
MSG_HASH(
   MSG_INPUT_BIND_RELEASE,
   "Nhả phím và nút ra!"
   )
MSG_HASH(
   MSG_INPUT_BIND_TIMEOUT,
   "Thời gian chờ"
   )
MSG_HASH(
   MSG_INPUT_BIND_HOLD,
   "Nhấn giữ"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
   "Nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ENABLE,
   "Tắt toàn bộ nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Chu kỳ nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Khoảng thời gian tính bằng khung hình khi các nút kích hoạt nhanh được nhấn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DUTY_CYCLE,
   "Chu kỳ hoạt động nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DUTY_CYCLE,
   "Số khung hình từ Chu kỳ nhấn nhanh mà các nút được giữ. Nếu số này bằng hoặc lớn hơn Chu kỳ nhấn nhanh, các nút sẽ không bao giờ nhả ra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DUTY_CYCLE_HALF,
   "Nửa chu kỳ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Chế độ nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Chọn hành vi chung của chế độ nhấn nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Cổ điển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC_TOGGLE,
   "Cổ điển (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Nút đơn (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Nút đơn (Nhấn giữ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Chế độ cổ điển, thao tác bằng hai nút. Giữ một nút và chạm vào nút tự động nhấn nhanh để kích hoạt chuỗi nhấn nhả.\nCó thể chỉ định Gán nút nhấn nhanh trong Cài đặt/Điều khiển Cổng X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Bật/Tắt chế độ cổ điển, thao tác bằng hai nút. Giữ một nút và chạm vào nút nhấn nhanh để bật gán nút cho nút đó. Để tắt tự động nhấn nhanh: giữ nút và nhấn lại nút Turbo.\nCó thể gán nút nhấn nhanh trong Cài đặt/Điều khiển Cổng X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Chuyển đổi chế độ. Nhấn nút Tự động một lần để kích hoạt chuỗi nhấn cho nút mặc định đã chọn, nhấn lại một lần nữa để tắt.\nCó thể chỉ định Gán nút nhấn nhanh trong Cài đặt/Điều khiển Cổng X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Chế độ giữ. Chuỗi nhấn nút mặc định đã chọn sẽ hoạt động miễn là nút Tự động nhấn được giữ.\nCó thể gán chế độ tự động nhấn nhanh trong Cài đặt/Điều khiển Cổng X.\nĐể mô phỏng chức năng tự động bắn của thời đại máy tính gia đình, hãy đặt Gán và Nút thành cùng một nút bắn trên cần điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "Gán nút nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "Chế độ tự động nhấn nhanh gán cho tay cầm. Nếu để trống, sẽ dùng gán mặc định theo cổng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "Nút tự động nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "Tự động nhấn nhanh mục tiêu ở chế độ 'Nút đơn'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "Cho phép tự động nhấn nhanh trên D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "Nếu bật, các phím điều hướng (D-Pad/hatswitch) cũng hỗ trợ tự động nhấn nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Tự động nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Thay đổi cài đặt tự động nhấn nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Phản hồi xúc giác/rung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Thay đổi cài đặt phản hồi xúc giác và rung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Menu điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Thay đổi cài đặt điều khiển menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Phím tắt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Thay đổi cài đặt và chỉ định phím tắt, chẳng hạn như bật/tắt menu trong khi chơi trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Gán nút cho tay cầm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Thay đổi cách tay cầm ảo được ánh xạ đến thiết bị điều khiển vật lý. Nếu thiết bị đầu vào được nhận dạng và tự động cấu hình chính xác, người dùng có thể không cần sử dụng menu này.\nLưu ý: đối với các thay đổi điều khiển cụ thể cho Core, hãy sử dụng menu phụ 'Điều khiển' của Menu Nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro sử dụng một khái niệm trừu tượng về Tay cầm ảo được gọi là 'Tay cầm' để giao tiếp từ giao diện người dùng (như RetroArch) đến lõi và ngược lại. Menu này xác định cách Tay cầm ảo được ánh xạ đến các thiết bị điều khiển vật lý và các cổng đầu vào ảo mà các thiết bị này chiếm giữ.\nNếu thiết bị điều khiển vật lý được nhận dạng và tự động cấu hình chính [...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Tay cầm cổng %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Thay đổi cách tay cầm ảo được ánh xạ tới thiết bị điều khiển vật lý của bạn cho cổng ảo này."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Thay đổi gán nút điều khiển cụ thể của lõi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Giải pháp khắc phục lỗi ngắt kết nối Android"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Giải pháp khắc phục tình trạng bộ điều khiển bị ngắt kết nối và kết nối lại. Cản trở 2 người chơi có cùng bộ điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Xác nhận Thoát/Đóng/Đặt lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "Yêu cầu phải nhấn phím nóng Thoát/Đóng/Đặt lại hai lần."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Rung khi nhấn phím"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Bật rung (cho core hỗ trợ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Độ rung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Chỉ định mức độ hiệu ứng phản hồi xúc giác."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Điều khiển Menu hợp nhất"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Sử dụng cùng một nút điều khiển cho cả menu và trò chơi. Áp dụng cho bàn phím."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Menu hoán đổi nút OK và Hủy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Đổi nút sang OK/Hủy. Tắt là hướng nút Nhật Bản, bật là nút hướng Tây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Nút cuộn hoán đổi menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Hoán đổi các nút để cuộn. Tắt cuộn 10 mục bằng L/R và theo thứ tự bảng chữ cái bằng L2/R2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Menu điều khiển tất cả người dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Cho phép bất kỳ người dùng nào điều khiển menu. Nếu tắt, chỉ Người dùng 1 mới có thể điều khiển menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SINGLECLICK_PLAYLISTS,
   "Danh sách phát một lần nhấp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SINGLECLICK_PLAYLISTS,
   "Bỏ qua menu 'Chạy' khi khởi chạy mục danh sách phát. Nhấn phím D-Pad trong khi giữ OK để truy cập menu 'Chạy'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ALLOW_TABS_BACK,
   "Cho phép quay lại từ các tab"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ALLOW_TABS_BACK,
   "Trở lại Menu chính từ các tab/thanh bên khi nhấn Quay lại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Tăng tốc cuộn menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Tốc độ tối đa của con trỏ khi giữ một hướng để cuộn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Độ trễ cuộn menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Độ trễ ban đầu tính bằng mili giây khi giữ một hướng để cuộn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Vô hiệu nút thông tin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Ngăn chặn chức năng thông tin menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Tắt nút tìm kiếm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Ngăn chặn chức năng tìm kiếm menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Tắt Analog trái trong Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_LEFT_ANALOG_IN_MENU,
   "Ngăn chặn việc nhập cần analog trái vào menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Tắt Analog phải trong Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_RIGHT_ANALOG_IN_MENU,
   "Ngăn chặn thao tác nhập bằng cần analog phải trên menu. Cần analog phải sẽ luân phiên hiển thị hình thu nhỏ trong danh sách phát."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Bật phím tắt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Khi được gán, phím 'Hotkey Enable' phải được giữ trước khi bất kỳ phím nóng nào khác được nhận dạng. Cho phép các nút điều khiển được ánh xạ đến các chức năng phím nóng mà không ảnh hưởng đến thao tác nhập liệu thông thường. Việc gán bộ điều chỉnh chỉ cho bộ điều khiển sẽ không yêu cầu nó cho các phím nóng trên bàn phím và ngược lại, nhưng cả hai bộ điều chỉnh đều hoạt[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ENABLE_HOTKEY,
   "Nếu phím nóng này được liên kết với bàn phím, nút điều khiển hoặc trục điều khiển, tất cả các phím nóng khác sẽ bị vô hiệu hóa trừ khi phím nóng này cũng được giữ cùng lúc.\nĐiều này hữu ích cho các triển khai tập trung vào RETRO_KEYBOARD truy vấn một vùng rộng trên bàn phím, trong đó các phím nóng không được phép cản trở."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Phím tắt Bật Độ trễ (Khung hình)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Thêm độ trễ khung hình trước khi điều khiển bình thường bị chặn sau khi nhấn phím 'Bật phím tắt' được chỉ định. Cho phép ghi lại điều khiển bình thường từ phím 'Bật phím tắt' khi nó được ánh xạ sang một hành động khác (ví dụ: 'Chọn' của RetroPad)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_DEVICE_MERGE,
   "Loại thiết bị phím tắt Hợp nhất"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_DEVICE_MERGE,
   "Chặn tất cả phím tắt từ cả hai loại thiết bị bàn phím và bộ điều khiển nếu một trong hai loại có cài đặt 'Bật phím tắt'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Phím tắt theo Người chơi 1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_FOLLOWS_PLAYER1,
   "Các phím tắt được gán cho cổng lõi số 1, ngay cả khi cổng đó đã được gán lại cho một người dùng khác.Lưu ý: Các phím tắt trên bàn phím sẽ không hoạt động nếu cổng lõi số 1 được gán cho người chơi có số thứ tự lớn hơn 1 (vì bàn phím luôn được xem là thiết bị điều khiển của Người chơi 1)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Bật/Tắt Menu (Bộ điều khiển kết hợp)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Tổ hợp nút điều khiển để Bật/Tắt menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Bật/Tắt Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Chuyển đổi màn hình hiện tại giữa menu và trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Thoát (Bộ điều khiển kết hợp)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Tổ hợp nút điều khiển để thoát RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Thoát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Đóng RetroArch, đảm bảo tất cả dữ liệu đã lưu và tệp cấu hình đều được lưu vào đĩa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Đóng trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Đóng trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Đặt lại trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Khởi động lại trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Tua nhanh (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Chuyển đổi giữa chế độ tua nhanh và tốc độ bình thường."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Tua nhanh (Nhấn giữ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Cho phép tua nhanh khi giữ phím. Trò chơi sẽ chạy ở tốc độ bình thường khi nhả phím."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Chuyển động chậm (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Chuyển đổi giữa chế độ chuyển động chậm và tốc độ bình thường."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Chuyển động chậm (Nhấn giữ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Cho phép chuyển động chậm khi giữ. Trò chơi chạy ở tốc độ bình thường khi nhả phím."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Tua lùi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Tua lùi trò chơi hiện tại khi giữ phím. Phải bật 'Hỗ trợ tua lùi'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Tạm dừng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Chuyển đổi trò chơi giữa trạng thái tạm dừng và không tạm dừng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Từng khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Khi tạm dừng, trò chơi sẽ tiến thêm một khung hình."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Tắt tiếng âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Bật/tắt âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Tăng âm lượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Tăng mức âm lượng đầu ra."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Giảm âm lượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Giảm mức âm lượng đầu ra."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Tải trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Tải trạng thái đã lưu từ khe hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Lưu trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Lưu trò chơi vào khe hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Lưu trò chơi tiếp theo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Tăng chỉ mục khe trạng thái lưu hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Lưu trạng thái trước đó"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Giảm chỉ số khe trạng thái lưu hiện đang được chọn."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Đẩy đĩa (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Nếu khay đĩa ảo đang đóng, hãy mở khay và lấy đĩa đã nạp ra. Nếu không, hãy đưa đĩa hiện đang được chọn vào và đóng khay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Đĩa tiếp theo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Tăng chỉ mục đĩa hiện đang được chọn. Khay đĩa ảo phải đang mở."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Đĩa trước đó"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Giảm chỉ mục đĩa hiện đang được chọn. Khay đĩa ảo phải đang mở."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Bộ lọc đồ họa (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Bật/tắt shader hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_HOLD,
   "Bộ lọc đồ họa (Nhấn giữ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD,
   "Giữ shader hiện đang được chọn bật/tắt khi nhấn phím."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Bộ lọc đồ họa tiếp theo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Tải và áp dụng tệp cài đặt trước shader tiếp theo trong thư mục gốc 'Video Shader'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Bộ lọc đồ họa trước đó"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Tải và áp dụng tệp cài đặt sẵn shader trước đó vào thư mục gốc của thư mục 'Video Shaders'."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Cheats (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Bật/tắt cheat hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Chỉ số cheat tiếp theo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Tăng chỉ số cheat hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Chỉ số cheat trước đó"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Giảm chỉ số cheat hiện đang được chọn."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Chụp ảnh trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Ghi hình (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Bắt đầu/dừng ghi phiên hiện tại vào tệp video cục bộ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Phát trực tuyến (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Bắt đầu/dừng phát trực tuyến phiên hiện tại vào nền tảng video trực tuyến."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PLAY_REPLAY_KEY,
   "Phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PLAY_REPLAY_KEY,
   "Phát tệp phát lại từ vị trí hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORD_REPLAY_KEY,
   "Ghi lại Phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORD_REPLAY_KEY,
   "Ghi tệp phát lại vào vị trí hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_HALT_REPLAY_KEY,
   "Dừng Ghi/Phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_HALT_REPLAY_KEY,
   "Dừng ghi/phát lại phát lại hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Lưu Điểm kiểm tra Phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_REPLAY_CHECKPOINT_KEY,
   "Gắn điểm kiểm tra vào phát lại hiện đang phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Điểm kiểm tra Phát lại Trước đó"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Tua lùi phát lại về điểm kiểm tra trước đó được lưu tự động hoặc thủ công."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Điểm kiểm tra Phát lại Tiếp theo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NEXT_REPLAY_CHECKPOINT_KEY,
   "Tua nhanh phát lại đến điểm kiểm tra tiếp theo được lưu tự động hoặc thủ công."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_PLUS,
   "Khe Phát lại Tiếp theo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_PLUS,
   "Tăng chỉ số khe phát lại hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REPLAY_SLOT_MINUS,
   "Khe Phát lại Trước đó"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REPLAY_SLOT_MINUS,
   "Giảm chỉ số khe phát lại hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_TURBO_FIRE_TOGGLE,
   "Chế độ tự động nhấn nhanh (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE,
   "Chuyển chế độ tự động nhấn nhanh sang Bật/Tắt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Cầm Chuột (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Cầm hoặc thả chuột. Khi cầm, con trỏ hệ thống sẽ ẩn và giới hạn trong cửa sổ hiển thị RetroArch, cải thiện khả năng tương tác với chuột."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Tập trung Trò chơi (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Chuyển chế độ 'Tập trung vào trò chơi' bật/tắt. Khi nội dung đang tập trung, các phím nóng bị vô hiệu hóa (toàn bộ bàn phím được chuyển tới core đang chạy) và chuột bị khóa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Toàn màn hình (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Chuyển đổi giữa chế độ hiển thị toàn màn hình và cửa sổ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Menu giao diện máy tính (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Mở giao diện người dùng desktop WIMP (Windows, Icons, Menus, Pointer) đi kèm."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Đồng bộ theo tốc độ khung hình trò chơi chính xác (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Bật/tắt đồng bộ theo tốc độ khung hình trò chơi chính xác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Chạy Trước (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Bật/tắt tính năng Chạy Trước."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREEMPT_TOGGLE,
   "Khung Hình Ưu Tiên (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREEMPT_TOGGLE,
   "Bật/tắt tính năng Khung Hình Ưu Tiên."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Hiển thị FPS (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Bật/tắt hiển thị trạng thái 'khung hình trên giây'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Hiển thị Thống kê Kỹ thuật (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Bật/tắt hiển thị thống kê kỹ thuật trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Lớp phủ Bàn phím (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Bật/tắt lớp phủ bàn phím."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Lớp phủ Tiếp theo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Chuyển sang bố cục khả dụng tiếp theo của lớp phủ đang hoạt động trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Dịch vụ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Chụp ảnh trò chơi hiện tại để dịch và/hoặc đọc to bất kỳ văn bản nào trên màn hình. 'Dịch vụ AI' phải được bật và cấu hình."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Độ trễ Netplay (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Bật/tắt bộ đếm ping cho phòng Trò chơi trực tuyến hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Chế độ máy chủ Netplay (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Bật/tắt máy chủ Trò chơi trực tuyến."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Chế độ Chơi/Xem Trò chơi trực tuyến (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Chuyển đổi phiên Trò chơi trực tuyến hiện tại giữa chế độ 'play' và 'spectate'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Trò chuyện người chơi Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Gửi tin nhắn trò chuyện đến phiên netplay hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Ẩn/Hiện chat Trò chơi trực tuyến (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Chuyển đổi giữa hiển thị mờ và tĩnh cho tin nhắn chat Trò chơi trực tuyến."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Loại Thiết Bị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Chỉ định loại tay cầm được giả lập."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Loại chuyển tín hiệu từ Analog sang Digital"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Sử dụng cần analog đã chỉ định cho điều khiển D-Pad. Chế độ 'Bắt buộc' ghi đè đầu vào analog gốc của core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Gán cần analog được chỉ định cho điều khiển D-Pad.\nNếu core hỗ trợ analog gốc, ánh xạ D-Pad sẽ bị tắt trừ khi chọn tùy chọn “(Bắt buộc)”.\nNếu ánh xạ D-Pad được ép buộc, core sẽ không nhận điều khiển analog từ cần được chỉ định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Chỉ số Thiết bị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_INDEX,
   "Tay cầm vật lý được RetroArch nhận diện."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Thiết bị dành riêng cho người chơi này"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVED_DEVICE_NAME,
   "Tay cầm này sẽ được cấp cho người chơi này, theo chế độ dành riêng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_NONE,
   "Không dành riêng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_PREFERRED,
   "Ưu tiên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DEVICE_RESERVATION_RESERVED,
   "Dành riêng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_RESERVATION_TYPE,
   "Loại Dành riêng Thiết bị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_RESERVATION_TYPE,
   "Ưu tiên: nếu có thiết bị được chỉ định, thiết bị đó sẽ được phân bổ cho trình phát này. Dành riêng: không có bộ điều khiển nào khác được phân bổ cho trình phát này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Cổng được ánh xạ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Chỉ định cổng Core nào sẽ nhận điều khiển từ cổng bộ điều khiển giao diện người dùng %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Thiết lập Tất cả Điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_ALL,
   "Chỉ định tất cả các hướng và nút, lần lượt, theo thứ tự chúng xuất hiện trong menu này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Đặt lại về Điều khiển Mặc định"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_DEFAULTS,
   "Xóa cài đặt liên kết đầu vào về giá trị mặc định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Lưu cấu hình Bộ điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SAVE_AUTOCONFIG,
   "Lưu tệp cấu hình tự động sẽ được áp dụng tự động bất cứ khi nào bộ điều khiển này được phát hiện lại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Chỉ mục Chuột"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_INDEX,
   "Chuột vật lý được RetroArch nhận dạng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Nút B (Xuống)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Nút Y (Trái)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Nút SELECT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Nút START"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "D-Pad Lên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "D-Pad Xuống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "D-Pad Trái"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "Nút D-Pad Phải"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Nút A (Phải)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Nút X (Trên)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "Nút L (Vai)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "Nút R (Vai)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "Nút L2 (Kích hoạt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "Nút R2 (Kích hoạt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "Nút L3 (Ngón cái)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "Nút R3 (Ngón cái)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Analog Trái X+ (Phải)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Analog Trái X- (Trái)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Analog Trái Y+ (Xuống)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Cần Analog trái Y- (Lên)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Cần Analog phải X+ (Phải)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Cần Analog phải X- (Trái)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Cần Analog phải Y+ (Xuống)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Cần Analog phải Y- (Lên)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Nút bóp súng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Nạp đạn súng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Nút phụ A (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Nút phụ B (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Nút phụ C (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Nút Start (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Nút Select (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "D-Pad Lên (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "D-Pad Xuống (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "D-Pad Trái (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "D-Pad Phải (súng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO,
   "Tự động nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOLD,
   "Nhấn giữ"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Chạy Trước Không Khả Dụng]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "Core hiện tại không tương thích với tính năng chạy trước do không hỗ trợ lưu trạng thái xác định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE,
   "Chạy trước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Số khung hình để chạy trước"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Số khung hình để chạy trước. Gây ra các vấn đề khi chơi như giật hình nếu số khung hình trễ nội bộ trong trò chơi bị vượt quá."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE,
   "Chạy logic core bổ sung để giảm độ trễ. Chế độ Một Phiên bản chạy tới một khung hình tương lai, sau đó tải lại trạng thái hiện tại. Chế độ Phiên bản Thứ hai giữ một core chỉ video tại khung hình tương lai để tránh sự cố trạng thái âm thanh. Chế độ Khung hình Chủ động chạy qua các khung hình trước với dữ liệu điều khiển mới khi cần, để tăng hiệu quả."
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Chạy logic core bổ sung để giảm độ trễ. Chế độ Một Phiên bản chạy tới một khung hình tương lai, sau đó tải lại trạng thái hiện tại. Chế độ Khung hình Chủ động chạy qua các khung hình trước với dữ liệu điều khiển mới khi cần, để tăng hiệu quả."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SINGLE_INSTANCE,
   "Chế độ Một Phiên bản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_SECOND_INSTANCE,
   "Chế độ Phiên bản Thứ hai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNAHEAD_MODE_PREEMPTIVE_FRAMES,
   "Chế độ Khung hình Chủ động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Ẩn cảnh báo Chạy trước"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Ẩn thông báo cảnh báo xuất hiện khi sử dụng Chạy trước mà core không hỗ trợ lưu trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PREEMPT_FRAMES,
   "Số khung hình Chủ động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Số khung hình để chạy lại. Gây ra các vấn đề khi chơi như giật hình nếu số khung hình trễ nội bộ trong trò chơi bị vượt quá."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Bối cảnh chia sẻ phần cứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Cho các core sử dụng phần cứng hiển thị riêng một ngữ cảnh riêng. Tránh phải giả định thay đổi trạng thái phần cứng giữa các khung hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Cho phép Core thay đổi trình điều khiển video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Cho phép core chuyển sang trình điều khiển video khác với trình hiện tại đang được tải."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Tải core trống khi tắt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Một số core có tính năng tắt, tải một core giả sẽ ngăn RetroArch tắt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_DUMMY_ON_CORE_SHUTDOWN,
   "Một số core có thể có tính năng tắt. Nếu tùy chọn này bị tắt, chọn quy trình tắt sẽ khiến RetroArch đóng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Tự động chạy Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Kiểm tra Firmware thiếu trước khi tải"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Kiểm tra xem tất cả phần mềm hệ thống cần thiết có đầy đủ trước khi thử tải trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHECK_FOR_MISSING_FIRMWARE,
   "Một số core có thể cần firmware hoặc file BIOS. Nếu bật tùy chọn này, RetroArch sẽ không cho phép khởi động core nếu thiếu bất kỳ firmware bắt buộc nào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Danh mục Tùy chọn Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Cho phép core hiển thị các tùy chọn trong menu con theo danh mục. Core phải được tải lại để thay đổi có hiệu lực."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Lưu bộ nhớ đệm thông tin Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Duy trì bộ nhớ đệm cục bộ thông tin core đã cài. Giảm đáng kể thời gian tải trên các nền tảng có truy cập đĩa chậm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BYPASS,
   "Bỏ qua tính năng Lưu trạng thái Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_SAVESTATE_BYPASS,
   "Chỉ định có bỏ qua khả năng lưu trạng thái core hay không, cho phép thử nghiệm các tính năng liên quan (chạy trước, tua lại, v.v.)."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Luôn tải lại Core khi chạy trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Khởi động lại RetroArch khi khởi chạy trò chơi, ngay cả khi core yêu cầu đã được tải. Điều này có thể cải thiện độ ổn định hệ thống, nhưng thời gian tải sẽ tăng."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Cho phép xoay màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Cho phép core thiết lập xoay màn hình. Khi tắt, các yêu cầu xoay sẽ bị bỏ qua. Hữu ích cho các thiết lập xoay màn hình thủ công."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Quản lý Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Thực hiện các tác vụ bảo trì ngoại tuyến trên các core đã cài (sao lưu, phục hồi, xóa, v.v.) và xem thông tin core."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Quản lý Core"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Cài đặt hoặc gỡ cài đặt các core phân phối qua Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Cài đặt core"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Gỡ cài đặt core"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "Hiển thị 'Quản lý Core'"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Hiển thị tùy chọn 'Quản lý Core' trong Menu Chính."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Đang cài đặt core: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "Core sẽ được gỡ khi thoát RetroArch."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "Core hiện đang được tải xuống"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Lưu cấu hình khi thoát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Lưu thay đổi vào file cấu hình khi thoát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CONFIG_SAVE_ON_EXIT,
   "Lưu thay đổi vào file cấu hình khi thoát. Hữu ích cho các thay đổi thực hiện trong menu. Ghi đè file cấu hình, #include và các chú thích sẽ không được giữ lại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Lưu tệp gán phím khi thoát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Lưu các thay đổi trong bất kỳ file gán phím nào đang hoạt động khi đóng trò chơi hoặc thoát RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Tự động tải các tùy chọn Core dành riêng cho trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Tải các tùy chọn Core tùy chỉnh theo mặc định khi khởi động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Tải tập tin ghi đè tự động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Tải cấu hình tùy chỉnh khi khởi động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Tự động tải tệp gán phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Tải các điều khiển tùy chỉnh khi khởi động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INITIAL_DISK_CHANGE_ENABLE,
   "Tự động tải file chỉ mục đĩa ban đầu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INITIAL_DISK_CHANGE_ENABLE,
   "Chuyển sang đĩa đã sử dụng cuối cùng khi bắt đầu nội dung nhiều đĩa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Tự động tải cài đặt trước của Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Sử dụng file Tùy chọn Core Toàn cục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Lưu tất cả tùy chọn core vào một file cài đặt chung (retroarch-core-options.cfg). Khi tắt, các tùy chọn cho từng core sẽ được lưu vào thư mục/file riêng trong thư mục 'Configs' của RetroArch."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Sắp xếp file lưu theo tên Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Sắp xếp file lưu vào các thư mục đặt theo tên core được sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Sắp xếp trạng thái lưu theo tên Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Sắp xếp trạng thái lưu vào các thư mục đặt theo tên core được sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Sắp xếp file lưu theo thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Sắp xếp các tệp lưu vào các thư mục được đặt tên theo thư mục chứa trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Sắp xếp trạng thái lưu theo thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Sắp xếp trạng thái lưu trong các thư mục được đặt tên theo thư mục chứa trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Khi tải savestate đừng ghi đè SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Ngăn SaveRAM bị ghi đè khi tải trạng thái lưu. Có thể dẫn đến lỗi trong một số trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Khoảng thời gian tự động lưu SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Tự động lưu SaveRAM không bay hơi theo khoảng thời gian đều đặn (tính bằng giây)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUTOSAVE_INTERVAL,
   "Tự động lưu SRAM không bay hơi theo khoảng thời gian đều đặn. Tùy chọn này mặc định bị tắt trừ khi được đặt khác. Khoảng thời gian được tính bằng giây. Giá trị 0 sẽ tắt tính năng tự động lưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_INTERVAL,
   "Khoảng thời gian đánh dấu checkpoint cho tính năng Replay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_INTERVAL,
   "Tự động đánh dấu trạng thái trò chơi trong quá trình ghi lại phát lại theo khoảng thời gian định sẵn (tính theo giây)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_INTERVAL,
   "Tự động lưu trạng thái trò chơi trong quá trình ghi lại phát lại theo khoảng thời gian định sẵn. Mặc định tính năng này bị tắt trừ khi được thiết lập khác. Khoảng thời gian được tính theo giây. Giá trị 0 sẽ tắt ghi nhận checkpoint."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_CHECKPOINT_DESERIALIZE,
   "Có nên giải tuần tự các điểm kiểm tra được lưu trong các phát lại trong khi phát lại bình thường hay không."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_DESERIALIZE,
   "Giải tuần tự điểm kiểm tra phát lại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_DESERIALIZE,
   "Có nên giải tuần tự các điểm kiểm tra được lưu trong các phát lại trong khi phát lại bình thường hay không. Nên bật cho hầu hết các core, nhưng một số core có thể hoạt động không ổn định khi giải tuần tự trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Tự động tăng chỉ số Lưu trạng thái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Trước khi tạo một lưu trạng thái, chỉ số lưu trạng thái sẽ tự động tăng. Khi mở trò chơi, chỉ số sẽ được đặt bằng chỉ số cao nhất hiện có."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Tự động tăng chỉ số phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Trước khi tạo một phát lại, chỉ số phát lại sẽ tự động tăng. Khi mở trò chơi, chỉ số sẽ được đặt bằng chỉ số cao nhất hiện có."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Giới hạn số trạng thái lưu tự động giữ lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Giới hạn số trạng thái lưu sẽ được tạo khi “Tăng chỉ số trạng thái lưu tự động” được bật. Nếu vượt quá giới hạn khi lưu trạng thái mới, trạng thái hiện tại có chỉ số thấp nhất sẽ bị xóa. Giá trị '0' nghĩa là lưu không giới hạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_MAX_KEEP,
   "Giới hạn số lần phát lại tự động giữ lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_MAX_KEEP,
   "Giới hạn số lần phát lại sẽ được tạo khi “Tăng chỉ số phát lại tự động” được bật. Nếu vượt quá giới hạn khi ghi phát lại mới, phát lại hiện tại có chỉ số thấp nhất sẽ bị xóa. Giá trị '0' nghĩa là ghi phát lại không giới hạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Tự động lưu trạng thái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Tự động tạo trạng thái lưu khi đóng trò chơi. Trạng thái này sẽ được tải khi khởi động nếu “Tự động tải trạng thái” được bật."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Tự động tải trạng thái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Tự động tải trạng thái lưu tự động khi khởi động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Hiển thị hình thu nhỏ trạng thái lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Hiển thị hình thu nhỏ của các trạng thái lưu trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Nén SaveRAM"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Ghi các file SaveRAM không thay đổi vào định dạng lưu trữ. Giảm đáng kể kích thước file nhưng thời gian lưu/tải sẽ tăng nhẹ. Chỉ áp dụng cho các core cho phép lưu qua giao diện chuẩn libretro SaveRAM."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Nén trạng thái lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Ghi file trạng thái lưu vào định dạng lưu trữ. Giảm đáng kể kích thước file nhưng thời gian lưu/tải sẽ tăng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Sắp xếp ảnh chụp màn hình theo thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Sắp xếp ảnh chụp màn hình vào các thư mục được đặt theo tên thư mục chứa trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Ghi dữ liệu lưu vào thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Sử dụng thư mục trò chơi làm thư mục lưu tệp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Ghi trạng thái lưu vào thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Sử dụng thư mục trò chơi làm thư mục lưu trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Tập tin hệ thống nằm trong thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Sử dụng thư mục trò chơi làm thư mục Hệ thống/BIOS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Ghi ảnh chụp màn hình vào Thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Sử dụng thư mục trò chơi làm thư mục lưu ảnh chụp màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Lưu Nhật ký Thời gian Chạy (Theo Core)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Theo dõi thời gian chạy của từng trò chơi, với bản ghi được tách riêng theo core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Lưu Nhật ký Thời gian Chạy (Tổng hợp)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Theo dõi thời gian chạy của từng trò chơi, ghi lại tổng thời gian tích lũy trên tất cả các core."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Mức chi tiết ghi nhật ký"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Ghi sự kiện vào Terminal hoặc tệp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Mức ghi nhật ký Giao diện Người dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Thiết lập mức ghi log cho giao diện. Nếu mức log từ giao diện thấp hơn giá trị này, nó sẽ bị bỏ qua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Mức ghi nhật ký cho Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Thiết lập mức ghi nhật ký cho các core. Nếu mức log từ core thấp hơn giá trị này, nó sẽ bị bỏ qua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Thiết lập mức ghi nhật ký cho các core libretro (GET_LOG_INTERFACE). Nếu mức log do core libretro gửi thấp hơn mức log libretro, nó sẽ bị bỏ qua. Các log DEBUG luôn bị bỏ qua trừ khi chế độ chi tiết được bật (--verbose).\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR = 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (Gỡ lỗi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (Thông tin)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Cảnh báo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (Lỗi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Ghi nhật ký vào File"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Chuyển hướng các thông báo log sự kiện hệ thống vào file. Yêu cầu bật ‘Logging Verbosity’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Đánh Dấu Thời Gian File Log"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Khi ghi nhật ký vào file, chuyển hướng đầu ra từ mỗi phiên RetroArch sang một file mới có đánh dấu thời gian. Nếu tắt, log sẽ bị ghi đè mỗi khi RetroArch được khởi động lại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Bộ Đếm Hiệu Suất"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Bộ đếm hiệu suất cho RetroArch và các core. Dữ liệu bộ đếm có thể giúp xác định nút thắt hệ thống và tối ưu hiệu suất."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Hiển Thị File và Thư Mục Ẩn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Hiển thị các tập tin và thư mục ẩn trong Trình duyệt Tập tin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Lọc phần mở rộng không xác định"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Lọc các tập tin được hiển thị trong Trình duyệt Tập tin theo phần mở rộng được hỗ trợ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Lọc theo Core hiện tại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILTER_BY_CURRENT_CORE,
   "Lọc các tập tin được hiển thị trong Trình duyệt Tập tin theo core hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Ghi nhớ Thư mục Bắt đầu đã sử dụng lần cuối"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Mở Trình duyệt Tệp tại vị trí đã sử dụng lần cuối khi mở trò chơi từ Thư mục Bắt đầu. Lưu ý: Vị trí sẽ được đặt lại về mặc định khi khởi động lại RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SUGGEST_ALWAYS,
   "Luôn Gợi ý Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS,
   "Cung cấp các lõi có sẵn ngay cả khi một lõi đã được tải thủ công."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Sử dụng Trình phát Media tích hợp sẵn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Sử dụng Trình xem Hình ảnh tích hợp sẵn"
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Tua lùi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Thay đổi cài đặt tua lùi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Bộ đếm Thời gian Khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Thay đổi các cài đặt ảnh hưởng đến bộ đếm thời gian khung hình.\nChỉ hoạt động khi video đa luồng bị tắt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Tốc độ Tua nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Tốc độ tối đa mà trò chơi sẽ chạy khi sử dụng tua nhanh (ví dụ, 5.0x cho nội dung 60 fps = giới hạn 300 fps). Nếu đặt thành 0.0x, tỷ lệ tua nhanh là không giới hạn (không giới hạn FPS)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "Tốc độ tối đa mà trò chơi sẽ chạy khi sử dụng tua nhanh. (Ví dụ 5.0 cho nội dung 60 fps => giới hạn 300 fps).\nRetroArch sẽ tạm nghỉ để đảm bảo tốc độ tối đa không bị vượt quá. Không nên dựa vào giới hạn này để chính xác hoàn toàn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Bỏ qua Khung hình khi Tua nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Bỏ qua khung hình theo tốc độ tua nhanh. Điều này tiết kiệm năng lượng và cho phép sử dụng giới hạn khung hình từ bên thứ ba."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Tốc độ Chậm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "Tốc độ phát trò chơi khi sử dụng chế độ chậm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Giới hạn tốc độ khung hình trong Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Đảm bảo tốc độ khung hình được giới hạn khi ở trong menu."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Hỗ trợ tua lùi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Quay lại điểm trước đó trong gameplay gần đây. Điều này sẽ làm giảm hiệu suất nghiêm trọng khi chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Tua lùi Khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Số khung hình sẽ quay lại mỗi bước. Giá trị cao hơn làm tăng tốc độ tua lùi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Kích thước Bộ đệm Tua lùi (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "Lượng bộ nhớ (MB) được dành cho bộ đệm tua lùi. Tăng giá trị này sẽ tăng lượng lịch sử tua lùi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Bước Kích thước Bộ đệm Tua lùi (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Mỗi lần giá trị kích thước bộ đệm tua lùi được tăng hoặc giảm, nó sẽ thay đổi theo giá trị này."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Đặt lại sau Tua nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Đặt lại bộ đếm thời gian khung hình sau khi tua nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Đặt lại sau Tải trạng thái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Đặt lại bộ đếm thời gian khung hình sau khi tải trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Đặt lại sau khi Lưu trạng thái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Đặt lại bộ đếm thời gian khung hình sau khi lưu trạng thái."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Chất lượng ghi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Thấp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "Trung bình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Cao"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Không mất dữ liệu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM Nhanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM Chất lượng cao"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Cấu hình ghi tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Luồng ghi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Ghi hình sau khi lọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Chụp hình sau khi áp dụng bộ lọc (nhưng không áp dụng shader). Video sẽ trông đẹp giống như những gì bạn thấy trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Sử dụng Ghi hình GPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Ghi lại đầu ra của vật liệu được GPU áp shader nếu có."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Chế độ phát trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_FACEBOOK,
   "Trò chơi trên Facebook"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "Vị trí"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Chất lượng phát trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Thấp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "Trung bình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Cao"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Cấu hình Phát trực tiếp Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Tiêu đề Phát trực tiếp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "URL Phát trực tiếp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "Cổng Phát trực tiếp UDP"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Giao diện Trên Màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Điều chỉnh viền và các nút điều khiển trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Bố cục Video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Điều chỉnh Bố cục Video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Thông báo Trên Màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Điều chỉnh Thông báo Trên Màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Hiển thị Thông báo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Bật/Tắt hiển thị của các loại thông báo cụ thể."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Hiển thị Giao diện"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Giao diện được sử dụng cho viền và các nút điều khiển trên màn hình."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Hiển thị Giao diện phía sau Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Hiển thị lớp phủ phía sau thay vì ở trước menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Ẩn Lớp Phủ Trong Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Ẩn lớp phủ khi ở trong menu, và hiển thị lại khi thoát menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Ẩn Lớp Phủ Khi Kết Nối Tay Cầm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Ẩn lớp phủ khi một tay cầm vật lý được kết nối vào cổng 1, và hiển thị lại khi tay cầm bị ngắt kết nối."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Ẩn lớp phủ khi một tay cầm vật lý được kết nối vào cổng 1. Lớp phủ sẽ không được phục hồi tự động khi tay cầm bị ngắt kết nối."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Hiển Thị Phím Nhập Trên Lớp Phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Hiển thị các phím đã đăng ký trên lớp phủ màn hình. ‘Chạm’ làm nổi bật các phần tử lớp phủ đang được nhấn/nhấp. ‘Vật lý (Tay Cầm)’ làm nổi bật các tín hiệu thực tế gửi đến core, thường từ tay cầm hoặc bàn phím kết nối."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Cảm ứng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Vật lý (Tay Cầm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Hiển Thị Phím Từ Cổng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Chọn cổng của thiết bị nhập để theo dõi khi ‘Hiển thị phím nhập trên lớp phủ’ được đặt thành ‘Vật lý (Tay Cầm)’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Hiển Thị Con Trỏ Chuột Với Lớp Phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Hiển thị con trỏ chuột khi sử dụng lớp phủ trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Tự Động Xoay Lớp Phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Nếu được hỗ trợ bởi lớp phủ hiện tại, tự động xoay bố cục để phù hợp với hướng/mặt tỷ lệ màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Tự động điều chỉnh Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Tự động điều chỉnh tỷ lệ overlay và khoảng cách các phần tử giao diện để phù hợp với tỷ lệ màn hình. Cho kết quả tốt nhất với overlay tay cầm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Độ nhạy chéo D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Điều chỉnh kích thước vùng chéo. Đặt 100% để có đối xứng 8 hướng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Độ nhạy chồng lấn ABXY"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Điều chỉnh kích thước vùng chồng lấn trong hình kim cương các nút mặt. Đặt 100% để có đối xứng 8 hướng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Vùng tái định tâm Analog"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ANALOG_RECENTER_ZONE,
   "Nhập liệu cần analog sẽ được tính tương đối từ lần chạm đầu tiên nếu nhấn trong vùng này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Tự động tải lớp phủ ưu tiên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_AUTOLOAD_PREFERRED,
   "Ưu tiên tải lớp phủ dựa trên tên hệ thống trước khi quay về cài sẵn mặc định. Sẽ bị bỏ qua nếu đã thiết lập ghi đè cho lớp phủ cài sẵn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Độ mờ của lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Độ mờ của tất cả các phần tử giao diện trên lớp phủ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Cài đặt sẵn lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Chọn một lớp phủ từ Trình quản lý Tệp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(Chế độ Ngang) Tỷ lệ Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Tỷ lệ của tất cả các phần tử giao diện trên lớp phủ khi sử dụng màn hình ở chế độ ngang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(Chế độ Ngang) Điều chỉnh Tỷ lệ Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Áp dụng hệ số hiệu chỉnh tỷ lệ khung hình cho lớp phủ khi sử dụng màn hình ở chế độ ngang. Giá trị dương tăng (giá trị âm giảm) chiều rộng hiệu quả của lớp phủ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Chế độ Ngang) Khoảng cách Ngang Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Nếu được hỗ trợ bởi cài đặt sẵn hiện tại, điều chỉnh khoảng cách giữa các phần tử giao diện ở nửa trái và nửa phải của lớp phủ khi sử dụng màn hình ở chế độ ngang. Giá trị dương tăng (giá trị âm giảm) khoảng cách giữa hai nửa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Chế độ Ngang) Khoảng cách Dọc Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Nếu được hỗ trợ bởi cài đặt sẵn hiện tại, điều chỉnh khoảng cách giữa các phần tử giao diện ở nửa trên và nửa dưới của lớp phủ khi sử dụng màn hình ở chế độ ngang. Giá trị dương tăng (giá trị âm giảm) khoảng cách giữa hai nửa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Chế độ Ngang) Dịch X Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Dịch ngang lớp phủ khi sử dụng màn hình ở chế độ ngang. Giá trị dương dịch lớp phủ sang phải; giá trị âm sang trái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Chế độ Ngang) Dịch Y Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Dịch dọc lớp phủ khi sử dụng màn hình ở chế độ ngang. Giá trị dương dịch lớp phủ lên trên; giá trị âm xuống dưới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(Chế độ Dọc) Tỷ lệ Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Tỷ lệ lớn, nhỏ của tất cả các phần tử giao diện trên lớp phủ khi sử dụng màn hình ở chế độ dọc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(Ngang) Điều chỉnh tỷ lệ lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Áp dụng hệ số chỉnh tỷ lệ cho lớp phủ khi sử dụng màn hình chế độ chân dung. Giá trị dương tăng chiều cao hiệu dụng của lớp phủ, giá trị âm giảm chiều cao."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Chân dung) Điều chỉnh tỷ lệ lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Nếu được hỗ trợ bởi cài đặt hiện tại, điều chỉnh khoảng cách giữa các phần tử giao diện ở nửa trái và phải của lớp phủ khi sử dụng màn hình chế độ chân dung. Giá trị dương tăng khoảng cách giữa hai nửa, giá trị âm giảm khoảng cách."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Chân dung) Khoảng cách dọc lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Nếu được hỗ trợ bởi cài đặt hiện tại, điều chỉnh khoảng cách giữa các phần tử giao diện ở nửa trên và dưới của lớp phủ khi sử dụng màn hình chế độ chân dung. Giá trị dương tăng khoảng cách giữa hai nửa, giá trị âm giảm khoảng cách."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Chân dung) Dịch ngang lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Dịch ngang lớp phủ khi sử dụng màn hình chế độ chân dung. Giá trị dương dịch sang phải, giá trị âm dịch sang trái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Chân dung) Dịch dọc lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Dịch dọc lớp phủ khi sử dụng màn hình chế độ chân dung. Giá trị dương dịch lên trên, giá trị âm dịch xuống dưới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_SETTINGS,
   "Lớp phủ bàn phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_SETTINGS,
   "Chọn và điều chỉnh lớp phủ bàn phím."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_POINTER_ENABLE,
   "Bật lớp phủ súng ánh sáng, chuột và con trỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_POINTER_ENABLE,
   "Sử dụng bất kỳ thao tác chạm nào không nhấn vào các điều khiển lớp phủ để tạo điều khiển thiết bị trỏ cho core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_LIGHTGUN_SETTINGS,
   "Lớp phủ súng ánh sáng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_LIGHTGUN_SETTINGS,
   "Cấu hình điều khiển súng ánh sáng từ lớp phủ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_MOUSE_SETTINGS,
   "Lớp phủ chuột"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_MOUSE_SETTINGS,
   "Cấu hình điều khiển chuột từ lớp phủ. Lưu ý: Nhấn 1, 2 hoặc 3 ngón sẽ gửi nhấp chuột trái, phải và giữa."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_PRESET,
   "Cài đặt sẵn lớp phủ bàn phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_PRESET,
   "Chọn lớp phủ bàn phím từ Trình duyệt Tệp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Tự động điều chỉnh lớp phủ bàn phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OSK_OVERLAY_AUTO_SCALE,
   "Điều chỉnh lớp phủ bàn phím về tỷ lệ gốc. Tắt để phóng to toàn màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_OPACITY,
   "Độ mờ lớp phủ bàn phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_OPACITY,
   "Độ mờ của tất cả các phần tử giao diện trên lớp phủ bàn phím."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Cổng súng ánh sáng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_PORT,
   "Chọn cổng core để nhận điều khiển từ súng ánh sáng lớp phủ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_PORT_ANY,
   "Bất kỳ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Kích hoạt khi chạm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_ON_TOUCH,
   "Gửi tín hiệu kích hoạt cùng với điều khiển con trỏ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Trễ kích hoạt (số khung hình)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TRIGGER_DELAY,
   "Trì hoãn kích hoạt điều khiển để cho con trỏ có thời gian di chuyển. Trì hoãn này cũng được sử dụng để chờ số lượng chạm đa điểm đúng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Điều khiển 2 chạm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_TWO_TOUCH_INPUT,
   "Chọn điều khiển sẽ gửi khi có hai điểm chạm trên màn hình. Trì hoãn kích hoạt nên khác không để phân biệt với các điều khiển khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Điều khiển 3 chạm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_THREE_TOUCH_INPUT,
   "Chọn điều khiển sẽ gửi khi có ba điểm chạm trên màn hình. Trì hoãn kích hoạt nên khác không để phân biệt với các điều khiển khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Điều khiển vào 4 chạm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_FOUR_TOUCH_INPUT,
   "Chọn điều khiển sẽ gửi khi có bốn điểm chạm trên màn hình. Trì hoãn kích hoạt nên khác không để phân biệt với các điều khiển khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Cho phép ra ngoài màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_LIGHTGUN_ALLOW_OFFSCREEN,
   "Cho phép nhắm ngoài ranh giới. Tắt để giới hạn việc nhắm ngoài màn hình vào cạnh trong ranh giới."
   )

/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SPEED,
   "Tốc độ chuột"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SPEED,
   "Điều chỉnh tốc độ di chuyển con trỏ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Nhấn lâu để kéo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_TO_DRAG,
   "Nhấn giữ màn hình để bắt đầu giữ nút."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Ngưỡng nhấn lâu (mili giây)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_HOLD_MSEC,
   "Điều chỉnh thời gian giữ cần thiết cho nhấn lâu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Nhấn đôi để kéo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_TO_DRAG,
   "Nhấn hai lần vào màn hình để bắt đầu giữ một nút ở lần nhấn thứ hai. Thêm độ trễ cho các cú nhấp chuột."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Ngưỡng nhấn đôi (mili giây)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_DTAP_MSEC,
   "Điều chỉnh khoảng thời gian cho phép giữa các lần nhấn khi phát hiện nhấn đôi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Ngưỡng vuốt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_MOUSE_SWIPE_THRESHOLD,
   "Điều chỉnh phạm vi trôi cho phép khi phát hiện nhấn dài hoặc chạm. Được biểu thị dưới dạng phần trăm của kích thước màn hình nhỏ hơn."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Bật bố cục video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Bố cục video được sử dụng cho viền và các tác phẩm nghệ thuật khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Đường dẫn bố cục video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Chọn một bố cục video từ Trình duyệt tệp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Chế độ xem đã chọn"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Chọn chế độ xem trong bố cục đã tải."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Thông báo Trên Màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Hiển thị thông báo trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Các thành phần đồ họa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Sử dụng các hoạt ảnh, thông báo, chỉ báo và điều khiển được trang trí."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Tự động điều chỉnh tỷ lệ các thành phần đồ họa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Tự động thay đổi kích thước thông báo, chỉ báo và điều khiển được trang trí dựa trên tỷ lệ menu hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Ghi đè tỷ lệ thành phần đồ họa (Toàn màn hình)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Áp dụng hệ số tỷ lệ thủ công khi vẽ các thành phần hiển thị ở chế độ toàn màn hình. Chỉ áp dụng khi ‘Tự động điều chỉnh tỷ lệ các thành phần đồ họa’ bị tắt. Có thể dùng để tăng hoặc giảm kích thước thông báo, chỉ báo và điều khiển được trang trí độc lập với menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Ghi đè tỷ lệ thành phần đồ họa (Cửa sổ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Áp dụng hệ số tỷ lệ thủ công khi vẽ các thành phần hiển thị ở chế độ cửa sổ. Chỉ áp dụng khi ‘Tự động điều chỉnh tỷ lệ các thành phần đồ họa’ bị tắt. Có thể dùng để tăng hoặc giảm kích thước thông báo, chỉ báo và điều khiển được trang trí độc lập với menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Hiển thị tốc độ khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Hiển thị số khung hình trên giây hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Khoảng thời gian cập nhật tốc độ khung hình (tính theo khung)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "Hiển thị tốc độ khung hình sẽ được cập nhật theo khoảng thời gian thiết lập, tính bằng số khung hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Hiển thị số khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Hiển thị số khung hình hiện tại trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Hiển thị thống kê"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Hiển thị thống kê kỹ thuật trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Hiển thị mức sử dụng bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Hiển thị lượng bộ nhớ đã dùng và tổng bộ nhớ trên hệ thống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Khoảng thời gian cập nhật mức sử dụng bộ nhớ (tính theo khung hình)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "Hiển thị mức sử dụng bộ nhớ sẽ được cập nhật theo khoảng thời gian đã đặt, tính bằng khung hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Hiển thị độ trễ Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Hiển thị độ trễ cho phòng Trò chơi trực tuyến hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Thông báo khi khởi động “Mở trò chơi”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Hiển thị hiệu ứng phản hồi ngắn khi tải trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Thông báo kết nối thiết bị nhập liệu (Tự cấu hình)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Thông báo lỗi thiết bị nhập liệu (Tự cấu hình)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Thông báo mã (Cheat) gian lận"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Hiển thị thông báo trên màn hình khi áp dụng mã gian lận."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Thông báo bản vá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Hiển thị thông báo trên màn hình khi áp dụng bản vá mềm cho ROM."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Hiển thị thông báo trên màn hình khi kết nối hoặc ngắt kết nối thiết bị nhập liệu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG_FAILS,
   "Hiển thị thông báo trên màn hình khi thiết bị nhập liệu không thể cấu hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Thông báo khi nạp sơ đồ phím lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Hiển thị thông báo trên màn hình khi nạp file sơ đồ phím lại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Thông báo khi nạp ghi đè cấu hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Hiển thị thông báo trên màn hình khi nạp file ghi đè cấu hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Thông báo khôi phục đĩa ban đầu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Hiển thị thông báo trên màn hình khi tự động khôi phục đĩa cuối cùng đã sử dụng của trò chơi nhiều đĩa được tải qua danh sách phát M3U lúc khởi động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_DISK_CONTROL,
   "Thông báo điều khiển đĩa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_DISK_CONTROL,
   "Hiển thị thông báo trên màn hình khi đưa vào hoặc lấy ra đĩa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SAVE_STATE,
   "Thông báo lưu trạng thái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SAVE_STATE,
   "Hiển thị thông báo trên màn hình khi lưu hoặc nạp trạng thái lưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Thông báo tua nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Hiển thị chỉ báo trên màn hình khi tua nhanh trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Thông báo chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Hiển thị thông báo trên màn hình khi chụp ảnh màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Thời gian hiển thị thông báo chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Xác định thời lượng thông báo chụp ảnh màn hình hiển thị trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "Bình thường"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Nhanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Rất nhanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Ngay lập tức"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Hiệu ứng nháy khi chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Hiển thị hiệu ứng nháy trắng trên màn hình với thời lượng mong muốn khi chụp ảnh màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "BẬT (Bình thường)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "BẬT (Nhanh)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Thông báo tần số làm mới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Hiển thị thông báo trên màn hình khi thiết lập tần số làm mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Thông báo Trò chơi trực tuyến bổ sung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Hiển thị các thông báo Trò chơi trực tuyến không bắt buộc trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Chỉ thông báo trong menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Chỉ hiển thị thông báo khi menu đang mở."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Phông chữ thông báo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Chọn phông chữ cho thông báo trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Kích thước thông báo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Chỉ định kích thước phông chữ theo điểm. Khi sử dụng widget, kích thước này chỉ ảnh hưởng đến hiển thị thống kê trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Vị trí thông báo (Ngang)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Chỉ định vị trí trục X tùy chỉnh cho văn bản trên màn hình. 0 là mép trái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Vị trí thông báo (Dọc)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Chỉ định vị trí trục Y tùy chỉnh cho văn bản trên màn hình. 0 là mép dưới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Màu thông báo (Đỏ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_RED,
   "Thiết lập giá trị đỏ của màu văn bản OSD. Giá trị hợp lệ từ 0 đến 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Màu thông báo (Xanh lá)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_GREEN,
   "Thiết lập giá trị xanh lá của màu văn bản OSD. Giá trị hợp lệ từ 0 đến 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Màu thông báo (Xanh dương)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_COLOR_BLUE,
   "Thiết lập giá trị xanh dương của màu văn bản OSD. Giá trị hợp lệ từ 0 đến 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Nền thông báo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Bật màu nền cho OSD."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Màu nền thông báo (Đỏ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_RED,
   "Thiết lập giá trị màu đỏ của nền OSD. Giá trị hợp lệ từ 0 đến 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Màu nền thông báo (Xanh lục)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Thiết lập giá trị màu lục của nền OSD. Giá trị hợp lệ từ 0 đến 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Màu nền thông báo (Lam)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Thiết lập giá trị màu lam của nền OSD. Giá trị hợp lệ từ 0 đến 255."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Độ mờ nền thông báo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Thiết lập độ mờ của màu nền OSD. Giá trị hợp lệ từ 0.0 đến 1.0."
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Giao diện"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Thay đổi các thiết lập hiển thị của màn hình menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Hiển thị mục menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Bật/tắt hiển thị các mục menu trong RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Tạm dừng trò chơi khi menu đang hoạt động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Tạm dừng trò chơi nếu menu đang hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Tạm dừng trò chơi khi không hoạt động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Tạm dừng trò chơi khi RetroArch không phải cửa sổ đang được kích hoạt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Thoát khi đóng trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Tự động thoát RetroArch khi đóng trò chơi. Chế độ ‘CLI’ chỉ thoát khi trò chơi được khởi chạy qua dòng lệnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Tiếp tục trò chơi sau khi sử dụng trạng thái lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Tự động đóng menu và tiếp tục trò chơi sau khi lưu hoặc tải trạng thái. Tắt tùy chọn này có thể cải thiện hiệu suất trên các thiết bị rất chậm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Tiếp tục trò chơi sau khi đổi đĩa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Tự động đóng menu và tiếp tục trò chơi sau khi chèn hoặc tải một đĩa mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Cuộn danh sách vòng lặp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Quay về đầu và/hoặc cuối danh sách nếu chạm đến ranh giới theo chiều ngang hoặc dọc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Hiển thị Cài đặt Nâng cao"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Hiển thị cài đặt nâng cao cho người dùng thành thạo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Chế độ Ki ốt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Bảo vệ thiết lập bằng cách ẩn tất cả các cài đặt liên quan đến cấu hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Đặt mật khẩu để tắt Chế độ Ki ốt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Cung cấp mật khẩu khi bật chế độ ki ốt cho phép sau này có thể tắt nó từ menu, bằng cách vào Menu chính, chọn Tắt Chế độ Ki ốt và nhập mật khẩu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Hỗ trợ chuột"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Cho phép điều khiển menu bằng chuột."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Hỗ trợ cảm ứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Cho phép điều khiển menu bằng màn hình cảm ứng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Tác vụ đa luồng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Thực hiện các tác vụ trên một luồng riêng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Thời gian chờ trình bảo vệ màn hình Menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Khi menu đang hoạt động, trình bảo vệ màn hình sẽ hiển thị sau khoảng thời gian không hoạt động đã chỉ định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Hiệu ứng màn hình chờ trong menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Bật hiệu ứng hoạt hình khi màn hình chờ menu đang hoạt động. Có ảnh hưởng nhỏ đến hiệu suất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Tuyết rơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Trường sao"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX,
   "Xoáy nước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Tốc độ hiệu ứng màn hình chờ menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Điều chỉnh tốc độ của hiệu ứng màn hình chờ menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Tắt chế độ phối hợp giao diện desktop"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Trình quản lý cửa sổ sử dụng phối hợp để áp dụng hiệu ứng hình ảnh, phát hiện cửa sổ không phản hồi, và các tác vụ khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DISABLE_COMPOSITION,
   "Cưỡng bức tắt chế độ phối hợp. Hiện chỉ khả dụng trên Windows Vista/7."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Trợ thủ giao diện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Khởi động trình hỗ trợ giao diện khi bật máy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_UI_COMPANION_START_ON_BOOT,
   "Khởi động trình điều khiển hỗ trợ giao diện người dùng khi bật máy (nếu có)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Menu Desktop (cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Mở Menu Desktop khi khởi động"
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "Hiển thị màn hình dưới của 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Thay đổi cài đặt giao diện màn hình dưới."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_APPICON_SETTINGS,
   "Biểu tượng ứng dụng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_APPICON_SETTINGS,
   "Thay đổi biểu tượng ứng dụng."
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Menu nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Bật/tắt hiển thị các mục trong Menu Nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Thiết lập"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Bật/tắt hiển thị các mục trong menu Cài đặt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Hiển thị “Tải Core”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Hiển thị tùy chọn “Tải Core” trong Menu chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Hiển thị “Mở trò chơi”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Hiển thị tùy chọn “Mở trò chơi” trong Menu chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Hiển thị “Tải Đĩa”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Hiển thị tùy chọn “Tải Đĩa” trong Menu chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Hiển thị “Sao lưu Đĩa”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Hiển thị tùy chọn “Sao chép đĩa” trong Menu chính."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "Hiển thị \"Đẩy đĩa\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Hiển thị tùy chọn \"Đẩy đĩa\" trong Menu chính."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Hiển thị 'Cập nhật Trực tuyến'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Hiển thị tùy chọn 'Cập nhật Trực tuyến' trong Menu Chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Hiển thị 'Trình tải Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Hiển thị khả năng cập nhật core (và file thông tin core) trong tùy chọn 'Cập nhật Trực tuyến'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Hiển thị 'Trình cập nhật Hình thu nhỏ cũ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Hiển thị mục tải gói hình thu nhỏ cũ trong tùy chọn 'Cập nhật Trực tuyến'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Hiển thị 'Thông tin'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Hiển thị tùy chọn 'Thông tin' trong Menu Chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Hiển thị 'Tập tin Cấu hình'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Hiển thị tùy chọn 'Tập tin Cấu hình' trong Menu Chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Hiển thị 'Trợ giúp'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Hiển thị tùy chọn 'Trợ giúp' trong Menu Chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Hiển thị 'Thoát RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Hiển thị tùy chọn 'Thoát RetroArch' trong Menu Chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Hiển thị 'Khởi động lại RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Hiển thị tùy chọn \"Khởi động lại RetroArch\" trong Menu chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Hiển thị \"Cài đặt\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Hiển thị menu \"Cài đặt\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Đặt mật khẩu để bật \"Cài đặt\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Khi ẩn tab cài đặt, cung cấp mật khẩu sẽ cho phép khôi phục lại nó từ menu, bằng cách vào tab Menu chính, chọn \"Bật tab Cài đặt\" và nhập mật khẩu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Hiển thị \"Yêu thích\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Hiển thị menu \"Yêu thích\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES_FIRST,
   "Hiển thị Yêu thích trước"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES_FIRST,
   "Hiển thị \"Yêu thích\" trước \"Lịch sử\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Hiển thị \"Hình ảnh\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Hiển thị menu \"Hình ảnh\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Hiển thị \"Âm nhạc\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Hiển thị menu ‘Âm nhạc’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Hiển thị ‘Video’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Hiển thị menu ‘Video’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Hiển thị ‘Trò chơi trực tuyến’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Hiển thị menu ‘Trò chơi trực tuyến’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Hiển thị ‘Lịch sử’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Hiển thị menu lịch sử gần đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Hiển thị ‘Nhập trò chơi’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Hiển thị mục ‘Nhập trò chơi’ trong Menu Chính hoặc Danh sách Phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Menu chính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Menu Danh sách Phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Hiển thị ‘Danh sách Phát’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Hiển thị các danh sách phát trong Menu Chính. Bỏ qua trong GLUI nếu các tab danh sách phát và thanh điều hướng được bật."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLIST_TABS,
   "Hiển thị thẻ Danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLIST_TABS,
   "Hiển thị các thẻ danh sách phát. Không ảnh hưởng đến RGUI. Thanh điều hướng phải được bật trong GLUI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Hiển thị 'Khám phá'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Hiển thị tùy chọn khám phá trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Hiển thị \"Core tự chạy\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "Chỉ định loại core (nếu có) để hiển thị trong menu \"Core tự chạy\". Khi đặt thành \"Tùy chỉnh\", khả năng hiển thị từng core có thể được bật/tắt trong menu \"Quản lý Core\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Tất cả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Dùng một lần"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Hiển thị ngày và giờ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Hiển thị ngày và/hoặc giờ hiện tại trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Kiểu hiển thị ngày và giờ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Thay đổi kiểu hiển thị ngày và/hoặc giờ trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Ký tự phân tách ngày"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Chỉ định ký tự dùng để phân tách các thành phần năm/tháng/ngày khi hiển thị ngày trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Hiển thị mức pin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Hiển thị mức pin hiện tại trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Hiển thị tên của core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Hiển thị tên core hiện tại trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Hiển thị nhãn phụ trong menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Hiển thị thông tin bổ sung cho các mục trong menu."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Hiển thị màn hình khởi động"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Hiển thị màn hình khởi động trong menu. Tùy chọn này sẽ tự động chuyển thành tắt sau khi chương trình khởi chạy lần đầu."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Hiển thị 'Tiếp tục'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Hiển thị tùy chọn tiếp tục trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Hiển thị 'Chạy lại'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Hiển thị tùy chọn chạy lại trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Hiển thị 'Đóng trò chơi'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Hiển thị tùy chọn 'Đóng trò chơi'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Hiển thị menu con 'Trạng thái lưu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Hiển thị các tùy chọn trạng thái lưu trong một menu con."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Hiển thị 'Lưu/Tải trạng thái'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Hiển thị các tùy chọn để lưu/tải trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_REPLAY,
   "Hiển thị 'Điều khiển Phát lại'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_REPLAY,
   "Hiển thị các tùy chọn để ghi/phát lại các tệp replay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Hiển thị ‘Hoàn tác Lưu/Tải Trạng thái’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Hiển thị tùy chọn hoàn tác thao tác lưu/tải trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Hiển thị ‘Tùy chọn Core’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Hiển thị tùy chọn ‘Tùy chọn Core’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Hiển thị ‘Ghi Tùy chọn ra Đĩa’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Hiển thị mục ‘Ghi Tùy chọn ra Đĩa’ trong menu ‘Tùy chọn > Quản lý Tùy chọn Core’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Hiển thị ‘Điều khiển’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Hiển thị tùy chọn ‘Điều khiển’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Hiển thị ‘Chụp ảnh màn hình’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Hiển thị tùy chọn ‘Chụp ảnh màn hình’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Hiển thị ‘Bắt đầu Ghi hình’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Hiển thị tùy chọn ‘Bắt đầu Ghi hình’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Hiển thị ‘Bắt đầu Phát trực tuyến’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Hiển thị tùy chọn ‘Bắt đầu Phát trực tuyến’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Hiển thị ‘Lớp phủ trên màn hình’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Hiển thị tùy chọn 'Lớp phủ trên màn hình'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Hiển thị 'Bố cục Video'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Hiển thị tùy chọn 'Bố cục Video'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Hiển thị 'Độ trễ'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Hiển thị tùy chọn 'Độ trễ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Hiển thị 'Tua ngược'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Hiển thị tùy chọn 'Tua ngược'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Hiển thị 'Lưu Ghi đè Core'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Hiển thị tùy chọn 'Lưu Ghi đè Core' trong menu 'Ghi đè'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Hiển thị 'Lưu Ghi đè Thư mục trò chơi'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CONTENT_DIR_OVERRIDES,
   "Hiển thị tùy chọn Lưu Ghi đè Thư mục trò chơi trong menu Ghi đè."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Hiển thị Lưu Ghi đè Trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Hiển thị tùy chọn \"Ghi đè trò chơi\" trong menu \"Ghi đè\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Hiển thị \"Mã Cheat\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Hiển thị tùy chọn \"Mã Cheat\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Hiển thị ‘Bộ lọc hình ảnh’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Hiển thị tùy chọn ‘Bộ lọc hình ảnh’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Hiển thị ‘Thêm vào Yêu thích’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Hiển thị tùy chọn ‘Thêm vào Yêu thích’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Hiển thị ‘Thêm vào Danh sách phát’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_PLAYLIST,
   "Hiển thị tùy chọn ‘Thêm vào Danh sách phát’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Hiển thị ‘Gán Core’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Hiển thị tùy chọn ‘Gán Core’ khi trò chơi chưa chạy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Hiển thị ‘Xóa Gán Core’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Hiển thị tùy chọn ‘Xóa Gán Core’ khi trò chơi chưa chạy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Hiển thị ‘Tải Hình thu nhỏ’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Hiển thị tùy chọn ‘Tải Hình thu nhỏ’ khi trò chơi chưa chạy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Hiển thị 'Thông tin'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Hiển thị tùy chọn ‘Thông tin’."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Hiển thị ‘Trình điều khiển’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Hiển thị cài đặt ‘Trình điều khiển’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Hiển thị \"Video\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Hiển thị cài đặt \"Video\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Hiển thị \"Âm thanh\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Hiển thị cài đặt \"Âm thanh\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Hiển thị \"Điều khiển\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Hiển thị cài đặt \"Điều khiển\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Hiển thị \"Độ trễ\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Hiển thị cài đặt \"Độ trễ\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Hiển thị \"Core\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Hiển thị cài đặt \"Core\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Hiển thị \"Cấu hình\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Hiển thị cài đặt ‘Cấu hình’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Hiển thị ‘Lưu’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Hiển thị cài đặt ‘Lưu’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Hiển thị \"Ghi nhật ký\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Hiển thị cài đặt \"Ghi nhật ký\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Hiển thị \"Trình duyệt tệp\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Hiển thị cài đặt \"Trình duyệt tệp\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Hiển thị \"Giới hạn khung hình\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Hiển thị cài đặt \"Giới hạn khung hình\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Hiển thị \"Ghi hình\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Hiển thị cài đặt \"Ghi hình\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Hiển thị \"Hiển thị trên màn hình\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Hiển thị cài đặt \"Hiển thị trên màn hình\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Hiển thị \"Giao diện người dùng\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Hiển thị cài đặt \"Giao diện người dùng\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Hiển thị \"Dịch vụ AI\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Hiển thị cài đặt \"Dịch vụ AI\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Hiển thị \"Hỗ trợ tiếp cận\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Hiển thị cài đặt ‘Trợ năng’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Hiển thị ‘Quản lý nguồn’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Hiển thị cài đặt ‘Quản lý nguồn’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Hiển thị ‘Thành tích’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Hiển thị cài đặt ‘Thành tích’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Hiển thị ‘Mạng’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Hiển thị cài đặt ‘Mạng’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Hiển thị ‘Danh sách Phát’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Hiển thị cài đặt ‘Danh sách phát’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Hiển thị ‘Người dùng’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Hiển thị cài đặt “Người dùng”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Hiển thị “Thư mục”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Hiển thị cài đặt “Thư mục”."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "Hiển thị “Steam”"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "Hiển thị cài đặt “Steam”."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Hệ số phóng đại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Tùy chỉnh kích thước các phần tử giao diện trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Ảnh nền"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Chọn một hình ảnh để đặt làm nền menu. Ảnh thủ công và ảnh động sẽ ghi đè lên “Chủ đề Màu”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Độ mờ ảnh nền"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Thay đổi độ mờ của ảnh nền."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Độ mờ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Thay đổi độ mờ của nền mặc định trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Sử dụng chủ đề màu hệ thống ưu tiên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Sử dụng chủ đề màu của hệ điều hành (nếu có). Ghi đè thiết lập chủ đề."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Hình thu nhỏ chính"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Loại hình thu nhỏ sẽ hiển thị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Ngưỡng phóng to hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Tự động phóng to hình thu nhỏ có chiều rộng/chiều cao nhỏ hơn giá trị chỉ định. Cải thiện chất lượng hình ảnh nhưng ảnh hưởng vừa phải đến hiệu năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Nền hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_BACKGROUND_ENABLE,
   "Bật chế độ đệm khoảng trống chưa dùng trong hình thu nhỏ bằng một nền đặc. Điều này đảm bảo kích thước hiển thị đồng đều cho tất cả hình ảnh, cải thiện giao diện menu khi xem hình thu nhỏ trò chơi hỗn hợp có kích thước gốc khác nhau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Hoạt ảnh văn bản chạy ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Chọn phương thức cuộn ngang để hiển thị văn bản menu dài."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Tốc độ văn bản chạy ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Tốc độ hoạt ảnh khi cuộn văn bản menu dài."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Cuộn mượt văn bản chạy ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Sử dụng hoạt ảnh cuộn mượt khi hiển thị văn bản menu dài. Có tác động nhỏ đến hiệu năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Ghi nhớ lựa chọn khi đổi tab"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Ghi nhớ vị trí con trỏ trước đó trong các tab. RGUI không có tab, nhưng Danh sách phát và Cài đặt hoạt động tương tự."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Luôn luôn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Chỉ cho Danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Chỉ cho Menu chính và Cài đặt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_STARTUP_PAGE,
   "Trang khởi động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_STARTUP_PAGE,
   "Trang menu ban đầu khi khởi động."
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Kết quả Dịch vụ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Hiển thị bản dịch dưới dạng lớp chữ (Chế độ Hình ảnh), phát bằng giọng đọc tự động (Giọng nói), hoặc dùng trình đọc màn hình hệ thống như NVDA (Người đọc)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "Địa chỉ dịch vụ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Một địa chỉ http:// trỏ đến dịch vụ dịch thuật sẽ được sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Bật dịch vụ AI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Bật dịch vụ AI để chạy khi nhấn phím nóng của dịch vụ AI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Tạm dừng trong khi dịch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Tạm dừng core trong lúc dịch màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Ngôn ngữ nguồn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "Ngôn ngữ mà dịch vụ sẽ dịch từ đó. Nếu đặt là 'Mặc định', nó sẽ cố tự phát hiện ngôn ngữ. Đặt một ngôn ngữ cụ thể sẽ giúp bản dịch chính xác hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Ngôn ngữ đích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "Ngôn ngữ mà dịch vụ sẽ dịch sang. 'Mặc định' là tiếng Anh."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Bật hỗ trợ tiếp cận"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Bật chuyển văn bản thành giọng nói để hỗ trợ điều hướng menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Tốc độ giọng đọc văn bản thành giọng nói"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Tốc độ phát giọng đọc cho chức năng chuyển văn bản thành giọng nói."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Thành tích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Kiếm thành tích trong các trò chơi cổ điển. Để biết thêm thông tin, truy cập ‘https://retroachievements.org’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Chế độ Thử thách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Vô hiệu hóa mã gian lận, tua lại, chuyển động chậm và tải trạng thái lưu. Các thành tích kiếm được ở chế độ khó sẽ được đánh dấu đặc biệt để bạn có thể khoe với người khác những gì đã đạt được mà không cần tính năng trợ giúp của trình giả lập. Bật/tắt tùy chọn này khi đang chạy sẽ khởi động lại trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Bảng xếp hạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "Hiện diện phong phú"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Định kỳ gửi thông tin trò chơi theo ngữ cảnh đến trang Thành tích Retro. Không có tác dụng nếu chế độ “Thử thách” được bật."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Huy hiệu Thành Tích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Hiển thị huy hiệu trong danh sách thành tích."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Kiểm tra thành tích không chính thức"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Sử dụng các thành tích không chính thức và/hoặc tính năng beta cho mục đích thử nghiệm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Âm thanh Mở Khóa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Phát âm thanh khi một thành tích được mở khóa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Chụp ảnh Màn Hình Tự Động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Tự động chụp ảnh màn hình khi đạt được một thành tựu."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Chế độ Encore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Bắt đầu phiên chơi với tất cả thành tựu được kích hoạt (kể cả những thành tựu đã mở khóa trước đó)."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Giao diện"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Thay đổi vị trí và khoảng cách của thông báo thành tựu trên màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Vị trí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Đặt góc/cạnh màn hình nơi thông báo thành tích sẽ xuất hiện."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Trên Trái"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "Trên Giữa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Trên Phải"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Dưới Trái"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Dưới Giữa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "Dưới Phải"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Căn chỉnh đệm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Đặt xem thông báo thành tích có nên căn chỉnh cùng các loại thông báo trên màn hình khác hay không. Tắt để tự đặt giá trị đệm/vị trí thủ công."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "Đệm ngang thủ công"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Khoảng cách từ cạnh trái/phải màn hình, có thể bù cho hiện tượng overscan của màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "Đệm dọc thủ công"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Khoảng cách từ cạnh trên/dưới màn hình, có thể bù cho hiện tượng overscan của màn hình."
   )

/* Settings > Achievements > Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SETTINGS,
   "Hiển thị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SETTINGS,
   "Thay đổi những thông báo và phần tử trên màn hình nào được hiển thị. Không vô hiệu hóa chức năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY,
   "Tóm tắt khi khởi động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_SUMMARY,
   "Hiển thị thông tin về trò chơi đang được tải và tiến trình hiện tại của người dùng.\n“Đã nhận diện tất cả trò chơi” sẽ hiển thị tóm tắt cho các trò chơi chưa có thành tựu công bố."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_ALLGAMES,
   "Đã nhận diện tất cả trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_SUMMARY_HASCHEEVOS,
   "Trò chơi có thành tựu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_UNLOCK,
   "Thông báo mở khóa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_UNLOCK,
   "Hiển thị thông báo khi một thành tựu được mở khóa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_MASTERY,
   "Thông báo hoàn tất"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_MASTERY,
   "Hiển thị thông báo khi toàn bộ thành tựu của một trò chơi được mở khóa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Chỉ báo thử thách đang hoạt động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Hiển thị chỉ báo trên màn hình khi có thể đạt được một số thành tựu nhất định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Chỉ Báo Tiến Trình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_PROGRESS_TRACKER,
   "Hiển thị chỉ báo trên màn hình khi có tiến trình đạt được đối với một số thành tích."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_START,
   "Tin Nhắn Bắt Đầu Bảng Xếp Hạng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_START,
   "Hiển thị mô tả của bảng xếp hạng khi nó trở nên hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Tin Nhắn Gửi Bảng Xếp Hạng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_SUBMIT,
   "Hiển thị thông báo với giá trị được gửi khi một lần thử bảng xếp hạng hoàn tất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Thông báo thất bại bảng xếp hạng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_CANCEL,
   "Hiển thị thông báo khi một lần thử bảng xếp hạng thất bại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Trình theo dõi bảng xếp hạng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_LBOARD_TRACKERS,
   "Hiển thị trình theo dõi trên màn hình với giá trị hiện tại của các bảng xếp hạng đang hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VISIBILITY_ACCOUNT,
   "Thông báo đăng nhập"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VISIBILITY_ACCOUNT,
   "Hiển thị các thông báo liên quan đến đăng nhập tài khoản RetroAchievements."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Thông báo chi tiết"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Hiển thị thêm các thông báo chẩn đoán và lỗi."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Công khai thông báo Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Có công khai thông báo trò chơi Trò chơi trực tuyến hay không. Nếu không bật, người chơi phải kết nối thủ công thay vì dùng sảnh công cộng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Sử dụng máy chủ chuyển tiếp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Chuyển tiếp kết nối Trò chơi trực tuyến thông qua máy chủ trung gian. Hữu ích nếu máy chủ ở sau tường lửa hoặc gặp sự cố NAT/UPnP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Vị trí máy chủ chuyển tiếp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Chọn một máy chủ chuyển tiếp cụ thể để sử dụng. Các vị trí gần về mặt địa lý thường có độ trễ thấp hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Địa Chỉ Máy Chủ Trung Gian Tùy Chỉnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Nhập địa chỉ máy chủ trung gian tùy chỉnh tại đây. Định dạng: địa chỉ hoặc địa chỉ|cổng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "Bắc Mỹ (Bờ Đông, Mỹ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "Tây Âu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "Nam Mỹ (Đông Nam, Brazil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Đông Nam Á"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Địa chỉ máy chủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Địa chỉ của máy chủ để kết nối."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Cổng TCP Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Cổng của địa chỉ IP máy chủ. Có thể là cổng TCP hoặc UDP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Kết nối đồng thời tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Số lượng kết nối đang hoạt động tối đa mà máy chủ sẽ chấp nhận trước khi từ chối kết nối mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Giới hạn ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "Độ trễ kết nối tối đa mà máy chủ sẽ chấp nhận. Đặt 0 để không giới hạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Mật khẩu máy chủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Mật khẩu dùng cho máy khách khi kết nối tới máy chủ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Mật khẩu chỉ xem của máy chủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Mật khẩu dùng cho máy khách khi kết nối tới máy chủ ở chế độ khán giả."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Chế độ khán giả Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Bắt đầu netplay ở chế độ khán giả."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_START_AS_SPECTATOR,
   "Có bắt đầu Trò chơi trực tuyến ở chế độ khán giả hay không. Nếu bật, Trò chơi trực tuyến sẽ khởi động ở chế độ khán giả. Luôn có thể thay đổi chế độ sau này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Làm mờ trò chuyện"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Làm mờ dần tin nhắn trò chuyện theo thời gian."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Màu trò chuyện (Tên hiển thị)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Định dạng: #RRGGBB hoặc RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Màu chữ trò chuyện (Tin nhắn)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Định dạng: #RRGGBB hoặc RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Cho phép tạm dừng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Cho phép người chơi tạm dừng trong chế độ chơi mạng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Cho phép máy khách chế độ lệ thuộc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Cho phép kết nối ở chế độ lệ thuộc. Máy khách chế độ lệ thuộc yêu cầu rất ít sức mạnh xử lý ở cả hai phía, nhưng sẽ bị ảnh hưởng đáng kể bởi độ trễ mạng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Không cho phép máy khách không ở chế độ lệ thuộc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Không cho phép kết nối không ở chế độ lệ thuộc. Không khuyến nghị trừ khi sử dụng mạng rất nhanh với máy tính rất yếu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Kiểm tra khung hình Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "Tần số (tính theo khung hình) mà Trò chơi trực tuyến sẽ kiểm tra xem máy chủ và máy khách có đồng bộ hay không."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_CHECK_FRAMES,
   "Tần số tính theo khung hình mà Trò chơi trực tuyến sẽ kiểm tra xem máy chủ và máy khách có đồng bộ hay không. Với hầu hết các core, giá trị này sẽ không có ảnh hưởng rõ rệt và có thể bỏ qua. Với các core không xác định được kết quả, giá trị này quyết định tần suất người chơi trực tiếp Trò chơi trực tuyến được đồng bộ hóa. Với các core bị lỗi, thiết lập giá trị khác 0 có[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Số khung hình trễ điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Số khung hình trễ điều khiển mà Trò chơi trực tuyến sử dụng để che đi độ trễ mạng. Giảm hiện tượng giật và làm Trò chơi trực tuyến ít tốn CPU hơn, nhưng đổi lại sẽ có độ trễ điều khiển rõ rệt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Số khung hình trễ điều khiển mà trò chơi trực tuyến sử dụng để che đi độ trễ mạng.\nKhi chơi trò chơi trực tuyến, tùy chọn này sẽ trì hoãn điều khiển cục bộ, giúp khung hình đang chạy gần hơn với các khung hình nhận từ mạng. Điều này giảm hiện tượng giật và làm trò chơi trực tuyến ít tốn CPU hơn, nhưng đổi lại có độ trễ điều khiển rõ rệt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Phạm vi số khung hình trễ điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Phạm vi số khung hình độ trễ đầu vào có thể được dùng để che giấu độ trễ mạng. Giảm giật hình và làm cho chơi mạng ít tốn CPU hơn, nhưng phải đánh đổi bằng độ trễ điều khiển khó đoán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Phạm vi số khung hình độ trễ điều khiển có thể được dùng bởi mạng để ẩn độ trễ mạng.\nNếu được đặt, chơi mạng sẽ điều chỉnh số khung hình độ trễ điều khiển một cách linh hoạt để cân bằng thời gian CPU, độ trễ điều khiển và độ trễ mạng. Điều này giảm giật hình và làm cho chơi mạng ít tốn CPU hơn, nhưng phải đánh đổi bằng độ trễ điều khiển khó đoán."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Vượt NAT khi chơi mạng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Khi tạo phòng, cố gắng lắng nghe kết nối từ Internet công cộng, sử dụng UPnP hoặc các công nghệ tương tự để thoát khỏi mạng LAN."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Chia sẻ điều khiển kỹ thuật số"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Yêu cầu Thiết bị %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Yêu cầu chơi bằng thiết bị điều khiển được chỉ định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Lệnh mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Cổng lệnh mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "Tay cầm mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Cổng cơ sở Tay cầm mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Người dùng %d Tay cầm mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "lệnh stdin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Giao diện lệnh stdin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Tải hình thu nhỏ theo yêu cầu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Tự động tải các hình thu nhỏ bị thiếu trong khi duyệt danh sách phát. Ảnh hưởng nghiêm trọng đến hiệu năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Cài đặt Trình Cập Nhật"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATER_SETTINGS,
   "Truy cập cài đặt trình cập nhật core"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Đường dẫn Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL đến thư mục trình cập nhật core trên máy chủ buildbot của libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Đường dẫn tài nguyên Buildbot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL đến thư mục trình cập nhật tài nguyên trên máy chủ buildbot của libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Tự động giải nén lưu trữ tải về"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Sau khi tải xuống, tự động giải nén các tệp trong các tệp lưu trữ đã tải xuống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Hiển thị Core Thử Nghiệm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Bao gồm các core “thử nghiệm” trong danh sách Tải Core. Những core này thường chỉ dành cho mục đích phát triển/thử nghiệm và không được khuyến nghị sử dụng chung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Sao lưu Core khi Cập Nhật"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Tự động tạo bản sao lưu của bất kỳ core nào đã cài khi thực hiện cập nhật trực tuyến. Cho phép dễ dàng quay lại core hoạt động nếu bản cập nhật gây ra lỗi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Kích thước lịch sử sao lưu core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Chỉ định số lượng bản sao lưu tự động được giữ cho mỗi core đã cài. Khi đạt giới hạn này, việc tạo bản sao lưu mới thông qua cập nhật trực tuyến sẽ xóa bản sao lưu cũ nhất. Các bản sao lưu core thủ công sẽ không bị ảnh hưởng bởi thiết lập này."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Lịch sử"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Duy trì danh sách phát các trò chơi, hình ảnh, nhạc và video đã sử dụng gần đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Kích thước lịch sử"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Giới hạn số lượng mục trong danh sách phát gần đây cho trò chơi, hình ảnh, nhạc và video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Kích thước danh sách yêu thích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Giới hạn số lượng mục trong danh sách phát 'Yêu thích'. Khi đạt giới hạn, việc thêm mới sẽ bị ngăn cho đến khi các mục cũ được xóa. Đặt giá trị -1 cho phép số mục 'không giới hạn'.\nCẢNH Báo: Giảm giá trị sẽ xóa các mục hiện có!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Cho phép đổi tên mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Cho phép các mục trong danh sách phát được đổi tên."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Cho phép xóa mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Cho phép các mục trong danh sách phát được xóa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Sắp xếp danh sách phát theo thứ tự chữ cái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Sắp xếp các danh sách trò chơi theo thứ tự chữ cái, không bao gồm các danh sách “Lịch sử”, “Hình ảnh”, “Âm nhạc” và “Video”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Lưu danh sách phát theo định dạng cũ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Ghi danh sách phát sử dụng định dạng văn bản thuần đã lỗi thời. Khi tắt, danh sách phát được định dạng bằng JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Nén danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Lưu trữ dữ liệu danh sách phát khi ghi vào đĩa. Giảm dung lượng file và thời gian tải, đổi lại CPU sẽ tăng nhẹ. Có thể dùng với cả danh sách phát định dạng cũ hoặc mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Hiển thị Core liên kết trong danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Chỉ định khi nào gắn nhãn các mục trong danh sách phát với core hiện đang liên kết (nếu có).\nCài đặt này bị bỏ qua khi bật nhãn phụ của danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Hiển thị nhãn phụ của danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Hiển thị thông tin bổ sung cho mỗi mục trong danh sách phát, chẳng hạn như core đang liên kết và thời gian chạy (nếu có). Có ảnh hưởng đến hiệu suất tùy mức."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Hiển thị biểu tượng trò chơi cụ thể trong Lịch sử và Yêu thích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Hiển thị biểu tượng cụ thể cho mỗi mục trong danh sách lịch sử và yêu thích. Có ảnh hưởng biến đổi đến hiệu suất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Thời gian chạy:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Lần chơi cuối:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_PLAY_COUNT,
   "Số lần chơi:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "giây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "giây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "phút"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "phút"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "giờ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "giờ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "ngày"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "ngày"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "tuần"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "tuần"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "tháng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "tháng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "năm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "năm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "trước đây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Hiển thị chỉ số mục phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Hiển thị số mục khi xem danh sách phát. Định dạng hiển thị phụ thuộc vào trình điều khiển menu đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Thời lượng phụ của nhãn danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Chọn loại bản ghi thời lượng hiển thị trên nhãn phụ của danh sách phát.\nBản ghi thời lượng tương ứng phải được bật trong menu tùy chọn ‘Lưu’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Kiểu ngày và giờ của ‘Lần chơi cuối’"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Thiết lập kiểu ngày và giờ hiển thị cho thông tin dấu thời gian ‘Lần chơi cuối’. Tùy chọn '(AM/PM)' có thể ảnh hưởng nhẹ đến hiệu năng trên một số nền tảng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "So khớp lưu trữ gần đúng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Khi tìm kiếm trong danh sách phát các mục liên quan đến file nén, chỉ so khớp tên file lưu trữ thay vì [tên file]+[trò chơi]. Bật tùy chọn này để tránh các mục lịch sử trùng lặp khi tải file nén."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Quét mà không cần khớp Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Cho phép trò chơi được quét và thêm vào danh sách phát mà không cần cài đặt core hỗ trợ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_SERIAL_AND_CRC,
   "Quét và kiểm tra CRC trên các bản sao có thể có"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_SERIAL_AND_CRC,
   "Đôi khi các ISO có số sê-ri trùng nhau, đặc biệt với các tựa PSP/PSN. Chỉ dựa vào số serial đôi khi khiến trình quét đặt trò chơi vào hệ thống sai. Tùy chọn này thêm kiểm tra CRC, làm chậm quá trình quét đáng kể, nhưng có thể chính xác hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Quản lý danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Thực hiện các tác vụ bảo trì trên danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Danh sách phát di động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Khi bật, và thư mục 'Trình duyệt tệp' cũng được chọn, giá trị hiện tại của tham số 'Trình duyệt tệp' được lưu trong danh sách phát. Khi danh sách phát được tải trên hệ thống khác mà cùng tùy chọn này được bật, giá trị của tham số 'Trình duyệt tệp' sẽ được so sánh với giá trị trong danh sách phát; nếu khác nhau, đường dẫn các mục trong danh sách phát sẽ tự động được chỉnh sửa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_FILENAME,
   "Sử dụng tên tệp để đối chiếu hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_FILENAME,
   "Khi bật, sẽ tìm hình thu nhỏ dựa trên tên tệp của mục, thay vì nhãn của nó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ALLOW_NON_PNG,
   "Cho phép tất cả các loại hình ảnh được hỗ trợ làm hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ALLOW_NON_PNG,
   "Khi bật, hình thu nhỏ cục bộ có thể được thêm vào tất cả các loại hình ảnh được RetroArch hỗ trợ (như jpeg). Có thể ảnh hưởng nhẹ đến hiệu suất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGE,
   "Quản lý"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Core mặc định"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Chỉ định core để sử dụng khi chạy trò chơi qua mục danh sách phát mà chưa có liên kết core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Đặt lại liên kết Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Xóa các liên kết core hiện có cho tất cả các mục trong danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Chế độ hiển thị nhãn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Thay đổi cách hiển thị nhãn trò chơi trong danh sách phát này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Phương thức sắp xếp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Xác định cách sắp xếp các mục trong danh sách phát này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Dọn dẹp danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Kiểm tra liên kết core và xóa các mục không hợp lệ hoặc trùng lặp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Cập nhật danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Thêm trò chơi mới và xóa các mục không hợp lệ bằng cách lặp lại thao tác 'Quét Thủ Công' được sử dụng lần cuối để tạo hoặc chỉnh sửa danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Xóa danh mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Xóa danh sách phát khỏi hệ thống tập tin."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Quyền riêng tư"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Thay đổi cài đặt quyền riêng tư."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Tài khoản"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Quản lý các tài khoản đã cấu hình hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Tên người dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Nhập tên người dùng của bạn tại đây. Tên này sẽ được sử dụng cho các phiên chơi mạng, cùng các tính năng khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Ngôn ngữ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Thiết lập ngôn ngữ giao diện người dùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USER_LANGUAGE,
   "Dịch menu và tất cả các thông báo trên màn hình theo ngôn ngữ bạn chọn ở đây. Yêu cầu khởi động lại để thay đổi có hiệu lực.\nMức độ hoàn thiện bản dịch được hiển thị bên cạnh mỗi tùy chọn. Trong trường hợp một mục menu chưa được triển khai cho ngôn ngữ, sẽ sử dụng tiếng Anh thay thế."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Cho phép Camera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Cho phép core truy cập camera."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Trạng thái Rich Presence trên Discord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Cho phép ứng dụng Discord hiển thị dữ liệu về trò chơi đã chơi.\nChỉ có sẵn với phiên bản giao diện máy tính gốc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Cho phép vị trí"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Cho phép các core truy cập vị trí của bạn."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Thành tích Retro"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Kiếm thành tích trong các trò chơi cổ điển. Để biết thêm thông tin, truy cập ‘https://retroachievements.org’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Thông tin đăng nhập cho tài khoản RetroAchievements của bạn. Truy cập retroachievements.org và đăng ký một tài khoản miễn phí.\nSau khi đăng ký xong, bạn cần nhập tên người dùng và mật khẩu vào RetroArch."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Tên người dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Nhập tên người dùng tài khoản RetroAchievements của bạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Mật khẩu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Nhập mật khẩu của tài khoản RetroAchievements. Độ dài tối đa: 255 ký tự."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "Khóa phát trực tiếp YouTube"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Khóa phát trực tiếp Twitch"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Khóa phát trực tiếp Facebook Gaming"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Hệ thống/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOS, ROM khởi động và các tệp hệ thống đặc thù khác được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Tải về"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Các tệp đã tải về được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Tài nguyên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Các tài nguyên menu được RetroArch sử dụng được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Mục nền năng động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Hình nền sử dụng trong menu được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Hình bìa game, ảnh chụp màn hình và hình thu nhỏ màn hình tiêu đề được lưu trong thư mục này."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Thư mục bắt đầu"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Thiết lập thư mục khởi động cho Trình duyệt tệp."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Các tệp cấu hình"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Tệp cấu hình mặc định được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Các core Libretro được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Thông tin Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Các tệp thông tin ứng dụng/core được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Cơ sở dữ liệu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Các cơ sở dữ liệu được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Tệp \"Cheat\" gian lận"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Các tệp gian lận được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Bộ lọc video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Bộ lọc video dựa trên CPU được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Bộ lọc âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Bộ lọc DSP âm thanh được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Bộ lọc Shader video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Bộ lọc video dựa trên GPU được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Bản ghi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Các bản ghi được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Cấu hình bản ghi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Các cấu hình bản ghi được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Lớp phủ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Các lớp phủ được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY,
   "Lớp phủ bàn phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OSK_OVERLAY_DIRECTORY,
   "Lớp phủ bàn phím được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Bố cục Video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Bố cục Video được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Ảnh chụp màn hình được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Hồ sơ tay cầm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Hồ sơ tay cầm dùng để tự động cấu hình tay cầm được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Bản đồ gán lại phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Bản đồ gán lại phím được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Danh sách chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Danh sách phát được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Danh sách phát Yêu thích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Lưu danh sách phát Yêu thích vào thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Danh sách phát Lịch sử"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Lưu danh sách phát Lịch sử vào thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Danh sách phát Ảnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Lưu danh sách phát Lịch sử Ảnh vào thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Danh sách phát Nhạc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Lưu danh sách phát Nhạc vào thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Danh sách phát Video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Lưu danh sách phát Video vào thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Nhật ký chạy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Nhật ký chạy được lưu trong thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Thư mục lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Lưu tất cả các file lưu trữ vào thư mục này. Nếu không thiết lập, sẽ cố gắng lưu trong thư mục làm việc của tệp trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVEFILE_DIRECTORY,
   "Lưu tất cả các file lưu trữ (*.srm) vào thư mục này. Bao gồm các file liên quan như .rt, .psrm, v.v. Thiết lập này sẽ bị ghi đè bởi các tùy chọn dòng lệnh cụ thể."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Lưu trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Các trạng thái lưu và bản ghi phát lại được lưu trong thư mục này. Nếu không thiết lập, sẽ cố gắng lưu vào thư mục chứa trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Bộ nhớ đệm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Trò chơi đã lưu trữ sẽ được giải nén tạm thời vào thư mục này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Nhật ký sự kiện hệ thống"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Nhật ký sự kiện hệ thống được lưu trong thư mục này."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Bật hiển thị trạng thái nâng cao"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Chia sẻ trạng thái hiện tại của bạn trong RetroArch trên Steam."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Định dạng hiện diện của trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Quyết định thông tin liên quan đến trò chơi sẽ được chia sẻ."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Chọn trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Tên của Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Tên hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "Trò chơi (Tên hệ thống)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "Trò chơi (Tên core)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "Trò chơi (Tên hệ thống - Tên core)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Thêm vào Bộ trộn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Thêm bản nhạc này vào một khe luồng âm thanh khả dụng.\nNếu không có khe nào hiện có sẵn, thao tác sẽ bị bỏ qua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Thêm vào Bộ trộn và Phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Thêm bản nhạc này vào một khe luồng âm thanh khả dụng và phát.\nNếu không có khe nào hiện có sẵn, thao tác sẽ bị bỏ qua."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Máy chủ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Kết nối tới Máy chủ Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Nhập địa chỉ máy chủ Trò chơi trực tuyến và kết nối ở chế độ máy khách."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Ngắt kết nối khỏi Máy chủ Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Ngắt kết nối Trò chơi trực tuyến đang hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Bộ lọc phòng chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Chỉ các phòng có thể kết nối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Chỉ các core đã cài đặt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Phòng có mật khẩu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Làm mới danh sách máy chủ Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Quét các máy chủ Trò chơi trực tuyến."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Làm mới danh sách Trò chơi trực tuyến trong mạng LAN"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Quét máy chủ Trò chơi trực tuyến trong LAN."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Bắt đầu máy chủ Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Bắt đầu Trò chơi trực tuyến ở chế độ máy chủ (dịch vụ)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Dừng máy chủ Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "Đuổi máy khách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Đuổi một máy khách ra khỏi phòng bạn đang chủ trì."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "Cấm máy khách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Cấm một máy khách khỏi phòng bạn đang chủ trì."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Quét thư mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Quét một thư mục để tìm trò chơi phù hợp với cơ sở dữ liệu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Quét Thư Mục Này>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SCAN_THIS_DIRECTORY,
   "Chọn mục này để quét thư mục hiện tại tìm trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Quét tệp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Quét một tệp để tìm trò chơi phù hợp với cơ sở dữ liệu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Quét Thủ Công"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Quét có thể cấu hình dựa trên tên file trò chơi. Không yêu cầu nội dung phải khớp với cơ sở dữ liệu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_ENTRY,
   "Quét"
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Thêm vào Bộ trộn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Thêm vào Bộ trộn và Phát"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Chọn một thư mục để quét trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Tên hệ thống"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Chỉ định ‘tên hệ thống’ để liên kết với trò chơi đã quét. Dùng để đặt tên file danh sách phát tạo ra và nhận diện hình thu nhỏ của danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Tên hệ thống tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Chỉ định thủ công ‘tên hệ thống’ cho trò chơi đã quét. Chỉ dùng khi ‘Tên hệ thống’ được đặt là ‘<Tùy chỉnh>’."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Core mặc định"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Chọn core mặc định để sử dụng khi khởi chạy trò chơi đã quét."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Phần mở rộng file"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Danh sách loại file sẽ được quét, cách nhau bằng dấu cách. Nếu để trống, bao gồm tất cả loại file, hoặc nếu đã chỉ định core, bao gồm tất cả file được core hỗ trợ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Quét đệ quy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Khi bật, tất cả thư mục con của ‘Thư mục trò chơi’ được chỉ định sẽ được quét."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Quét trong file nén"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Khi bật, các file nén (.zip, .7z, v.v.) sẽ được tìm kiếm trò chơi hợp lệ/hỗ trợ. Có thể ảnh hưởng đáng kể đến hiệu năng quét."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Tệp DAT Arcade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Chọn tệp DAT Logiqx hoặc MAME List XML để bật đặt tên tự động cho trò chơi arcade được quét (MAME, FinalBurn Neo, v.v.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Bộ Lọc DAT Arcade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Khi sử dụng tệp DAT arcade, trò chơi chỉ được thêm vào danh sách phát nếu tìm thấy mục tương ứng trong tệp DAT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Ghi Đè Danh Sách Phát Hiện Có"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Khi bật, bất kỳ danh sách phát hiện có nào sẽ bị xóa trước khi quét trò chơi. Khi tắt, các mục hiện có được giữ nguyên và chỉ thêm những trò chơi đang thiếu trong danh sách chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Xác Thực Mục Hiện Có"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Khi bật, các mục trong bất kỳ danh sách chơi hiện có nào sẽ được xác minh trước khi quét trò chơi mới. Các mục tham chiếu đến trò chơi thiếu và/hoặc tệp có định dạng không hợp lệ sẽ bị xóa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Bắt Đầu Quét"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Quét trò chơi đã chọn."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Đang khởi tạo danh sách..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Năm phát hành"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Số người chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Khu vực"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Thẻ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Tìm tên..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Hiển thị tất cả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Bộ lọc bổ sung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Tất cả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Thêm bộ lọc bổ sung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Theo nhà phát triển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Theo nhà xuất bản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Theo năm phát hành"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Theo số lượng người chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Theo thể loại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Theo thành tích"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Theo danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Theo ngôn ngữ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Theo vùng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Theo độc quyền trên máy chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Theo nền tảng độc quyền"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Theo rung tay cầm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Theo điểm số"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Theo phương tiện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Theo điều khiển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Theo phong cách nghệ thuật"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Theo lối chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Theo cốt truyện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "Theo nhịp độ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Theo Góc nhìn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Theo Cài đặt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Theo Hình ảnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Theo Phương tiện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Theo nguyên bản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Theo Nhượng quyền"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Theo Thẻ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Theo Tên Hệ Thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Đặt Bộ Lọc Phạm Vi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Chế Độ Xem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Lưu thành Chế Độ Xem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Xóa Chế Độ Xem này"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Nhập tên cho chế độ xem mới"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Chế độ xem đã tồn tại với cùng tên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "Chế độ xem đã được lưu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "Chế độ xem đã bị xóa"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Chạy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Bắt đầu trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Đổi tên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Đổi tên tiêu đề của mục."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Xóa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Xóa mục này khỏi danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Thêm vào Yêu Thích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Thêm trò chơi vào 'Yêu thích'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_PLAYLIST,
   "Thêm vào Danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_PLAYLIST,
   "Thêm trò chơi vào danh sách chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CREATE_NEW_PLAYLIST,
   "Tạo Danh sách phát mới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CREATE_NEW_PLAYLIST,
   "Tạo một danh sách phát mới và thêm mục hiện tại vào đó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Thiết lập Liên kết Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Thiết lập core liên kết với trò chơi này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Đặt lại Liên kết Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Đặt lại Core được liên kết với trò chơi này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Thông tin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Xem thêm thông tin về trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Tải hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Tải xuống hình thu nhỏ ảnh chụp màn hình/bìa/hình tiêu đề cho trò chơi hiện tại. Cập nhật mọi hình thu nhỏ hiện có."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Core hiện tại"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Tên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Đường dẫn tệp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Mục: %lu/%lu"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Thời gian chơi"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Lần chơi cuối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Cơ sở dữ liệu"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Tiếp tục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Tiếp tục trò chơi và giữ lại Menu Nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Khởi động lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Khởi động lại nội dung từ đầu. Nhấn Start trên RetroPad để thực hiện reset cứng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Thoát trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Đóng trò chơi. Bất kỳ thay đổi chưa lưu nào có thể sẽ bị mất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Chụp một hình ảnh của màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Khe trạng thái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Thay đổi khe trạng thái đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Lưu trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Lưu trạng thái vào khe đang chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SAVE_STATE,
   "Lưu trạng thái vào khe đang chọn. Lưu ý: trạng thái lưu thường không di động và có thể không hoạt động với các phiên bản khác của core này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Tải trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Tải trạng thái đã lưu từ khe đang chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_STATE,
   "Tải trạng thái đã lưu từ khe đang chọn. Lưu ý: có thể không hoạt động nếu trạng thái được lưu bằng phiên bản khác của core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Hoàn tác tải trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Nếu trạng thái đã được tải, trò chơi sẽ trở về trạng thái trước khi tải."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Hoàn tác trạng thái lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Nếu trạng thái bị ghi đè, sẽ quay lại trạng thái lưu trước đó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_SLOT,
   "Khe phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_SLOT,
   "Thay đổi khe trạng thái đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAY_REPLAY,
   "Phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAY_REPLAY,
   "Phát tệp phát lại từ vị trí hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_REPLAY,
   "Ghi lại Phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_REPLAY,
   "Ghi tệp phát lại vào vị trí hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HALT_REPLAY,
   "Dừng Ghi/Phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HALT_REPLAY,
   "Dừng ghi/phát lại bản phát lại hiện tại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Thêm vào Yêu Thích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Thêm trò chơi vào 'Yêu thích'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Bắt đầu ghi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Bắt đầu ghi video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Dừng ghi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Dừng ghi hình video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Bắt đầu phát trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Bắt đầu phát trực tuyến đến điểm đến đã chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Dừng phát trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Kết thúc phát trực tuyến."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Lưu trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Truy cập các tùy chọn lưu trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Tùy chọn Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Thay đổi các tùy chọn cho trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Thay đổi các tùy chọn cho trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Mã gian lận (Cheat)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Thiết lập mã gian lận."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Điều khiển đĩa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Quản lý hình ảnh đĩa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Bộ lọc đồ họa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Thiết lập bộ lọc đồ họa để cải thiện hình ảnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Ghi đè"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Tùy chọn để ghi đè cấu hình toàn cục."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Danh sách thành tích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Xem thành tích và các thiết lập liên quan."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Quản lý Tùy chọn Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Lưu hoặc xóa ghi đè tùy chọn cho trò chơi hiện tại."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Lưu Tùy chọn Trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Lưu tùy chọn core áp dụng chỉ cho trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Xóa Tùy chọn Trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Xóa các tùy chọn core sẽ chỉ áp dụng cho trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Lưu tùy chọn thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Lưu các tùy chọn core sẽ áp dụng cho tất cả trò chơi được tải từ cùng thư mục với tệp hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Xóa tùy chọn thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Xóa các tùy chọn core sẽ áp dụng cho tất cả trò chơi được tải từ cùng thư mục với tệp hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Tệp Tùy chọn Đang hoạt động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_INFO,
   "Tệp tùy chọn hiện đang được sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Đặt lại Tùy chọn Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Đặt tất cả tùy chọn của Core hiện tại về giá trị mặc định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Ghi tùy chọn xuống đĩa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Buộc ghi các thiết lập hiện tại vào file tùy chọn đang dùng. Đảm bảo tùy chọn được giữ lại trong trường hợp lỗi nhân gây tắt không đúng của frontend."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Tạo file tùy chọn trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Lưu file tùy chọn trò chơi"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Quản lý file gán lại phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Tải, lưu hoặc xóa tệp gán lại phím điều khiển cho trò chơi hiện tại."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "File remap đang dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_INFO,
   "File gán lại phím hiện tại đang được sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Tải file gán lại phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_LOAD,
   "Tải và thay thế gán nút điều khiển hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_AS,
   "Lưu file gán lại phím thành"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_AS,
   "Lưu gán phím điều khiển hiện tại thành một file gán lại phím mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Lưu file gán lại phím cho Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CORE,
   "Lưu một tệp gán lại phím áp dụng cho tất cả trò chơi chạy với Core này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Xóa file gán lại phím cho Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CORE,
   "Xóa tệp gán lại phím áp dụng cho tất cả trò chơi chạy với Core này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Lưu tệp gán lại phím cho thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_CONTENT_DIR,
   "Lưu tệp gán phím sẽ áp dụng cho tất cả trò chơi được tải từ cùng thư mục với tệp hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Xóa tệp gán lại phím thư mục nội dung Trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Xóa tệp gán lại phím sẽ áp dụng cho tất cả trò chơi được tải từ cùng thư mục với tệp hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Lưu tệp gán lại phím Trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_SAVE_GAME,
   "Lưu tệp gán lại phím sẽ áp dụng chỉ cho trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Xóa tệp gán lại phím Trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_REMOVE_GAME,
   "Xóa tệp gán lại phím sẽ áp dụng chỉ cho trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Đặt lại Bản đồ Phím/Điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Đặt tất cả tùy chọn gán nút bấm về giá trị mặc định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Cập nhật tệp gán lại phím"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Ghi đè tệp gán lại phím đang hoạt động bằng các tùy chọn gán lại phím hiện tại."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Tệp gán lại phím"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Bắt đầu hoặc Tiếp tục Tìm Cheat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CHEAT_START_OR_CONT,
   "Quét bộ nhớ để tạo cheat mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Tải tệp Cheat (Thay thế)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Tải tệp gian lận và thay thế các gian lận hiện có."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Tải tệp Cheat (Thêm vào)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Tải một file cheat và thêm vào danh sách cheat hiện có."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Tải lại cheat riêng cho trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Lưu tập tin Cheat thành..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Lưu cheat hiện tại thành một file cheat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Thêm cheat mới lên trên cùng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Thêm cheat mới xuống cuối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Xóa tất cả cheat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Tự động áp dụng cheat khi tải trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Tự động áp dụng cheat khi trò chơi tải."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Áp dụng sau khi bật/tắt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Áp dụng cheat ngay sau khi bật/tắt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Áp dụng thay đổi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Thay đổi cheat sẽ có hiệu lực ngay lập tức."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Cheat - Gian lận"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Bắt đầu hoặc khởi động lại tìm kiếm cheat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Nhấn Trái hoặc Phải để đổi kích thước bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Tìm giá trị trong bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Nhấn Trái hoặc Phải để thay đổi giá trị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Bằng %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Tìm giá trị trong bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Nhỏ hơn trước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Tìm giá trị trong bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Nhỏ hơn hoặc bằng trước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Tìm giá trị trong bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Lớn hơn trước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Tìm giá trị trong bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Lớn hơn hoặc bằng trước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Tìm giá trị trong bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Bằng trước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Tìm giá trị trong bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Không bằng trước đó"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Tìm trong bộ nhớ các giá trị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Nhấn Trái hoặc Phải để thay đổi giá trị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Bằng trước đó +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Tìm trong bộ nhớ các giá trị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Nhấn Trái hoặc Phải để thay đổi giá trị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Bằng trước đó -%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Thêm %u kết quả khớp vào danh sách của bạn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Xóa kết quả khớp #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Tạo mã khớp #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Địa chỉ khớp: %08X Mặt nạ: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Tệp cheat (Ghi đè)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Tệp cheat (Thêm vào)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Chi tiết cheat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Chỉ mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Vị trí cheat trong danh sách."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Bật"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Miêu tả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Bộ xử lý"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Kích thước tìm kiếm bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Loại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Giá trị"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Địa chỉ bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Duyệt địa chỉ: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Mặt nạ địa chỉ bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Mặt nạ bit địa chỉ khi Kích thước tìm kiếm bộ nhớ < 8-bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Số lần lặp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Số lần cheat sẽ được áp dụng. Sử dụng cùng với hai tùy chọn 'Lặp' khác để ảnh hưởng đến khu vực bộ nhớ lớn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Tăng địa chỉ mỗi lần lặp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Sau mỗi lần lặp, 'Địa chỉ bộ nhớ' sẽ tăng thêm số này nhân với 'Kích thước tìm kiếm bộ nhớ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Tăng giá trị mỗi lần lặp"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Sau mỗi lần lặp, 'Giá trị' sẽ được tăng thêm bằng số này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Rung khi nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Giá trị rung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Cổng rung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Độ mạnh rung chính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Thời gian rung chính (mili giây)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Độ mạnh rung phụ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Thời gian rung phụ (mili giây)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Mã"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Thêm cheat mới phía sau cái này"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Thêm cheat mới phía trước cái này"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Sao chép cheat này phía sau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Sao chép cheat này phía trước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Xóa cheat này"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Đẩy đĩa ra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Mở khay đĩa ảo và gỡ đĩa hiện đang nạp. Nếu 'Tạm dừng trò chơi khi Menu đang bật' được bật, một số Core có thể không nhận thay đổi trừ khi trò chơi được tiếp tục trong vài giây sau mỗi thao tác điều khiển đĩa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Thêm đĩa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Chèn đĩa tương ứng với 'Chỉ số đĩa hiện tại' và đóng khay đĩa ảo. Nếu 'Tạm dừng trò chơi khi Menu đang hoạt động' được bật, một số Core có thể không nhận thay đổi trừ khi trò chơi được tiếp tục trong vài giây sau mỗi thao tác điều khiển đĩa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Tải đĩa mới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Đẩy đĩa hiện tại ra, chọn một đĩa mới từ hệ thống tệp rồi chèn vào và đóng khay đĩa ảo.\nLƯU Ý: Đây là tính năng cũ. Thay vào đó, nên tải các trò chơi nhiều đĩa bằng danh sách phát M3U, cho phép chọn đĩa bằng các tùy chọn 'Đẩy/Chèn đĩa' và 'Chỉ số đĩa hiện tại'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Chọn một đĩa mới từ hệ thống tệp và chèn vào mà không đóng khay đĩa ảo.\nLƯU Ý: Đây là tính năng cũ. Thay vào đó, nên tải các trò chơi nhiều đĩa bằng danh sách phát M3U, cho phép chọn đĩa bằng tùy chọn 'Chỉ số đĩa hiện tại'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Chỉ số đĩa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Chọn đĩa hiện tại từ danh sách các ảnh đĩa có sẵn. Đĩa sẽ được tải khi chọn 'Chèn đĩa'."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Bộ lọc đồ họa Shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Bật hệ thống xử lý shader video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Theo dõi thay đổi trong tập tin shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Tự động áp dụng các thay đổi được thực hiện trên tập tin shader trong ổ đĩa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_WATCH_FOR_CHANGES,
   "Theo dõi các tập tin bộ lọc để phát hiện thay đổi mới. Sau khi lưu thay đổi vào tập tin bộ lọc, bộ lọc sẽ tự động được biên dịch lại và áp dụng cho trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Ghi nhớ thư mục shader được dùng lần cuối"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Mở Trình duyệt tập tin tại thư mục được sử dụng gần nhất khi tải preset và pass của shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Tải cài sẵn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Tải một bộ lọc cài sẵn. Hệ thống Bộ lọc theo chuỗi sẽ được thiết lập tự động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PRESET,
   "Tải trực tiếp một bộ lọc cài đặt sẵn. Menu bộ lọc sẽ được cập nhật tương ứng.\nHệ số phóng to hiển thị trong menu chỉ chính xác nếu cài đặt sẵn sử dụng các phương pháp phóng to đơn giản (ví dụ: phóng to nguồn, hệ số phóng X/Y giống nhau)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Chèn trước cấu hình sẵn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Thêm preset vào trước preset hiện đang tải."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Thêm bộ cài đặt sẵn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Thêm bộ cài đặt trước vào bộ cài đặt đang sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_MANAGER,
   "Quản lý Cài đặt sẵn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_MANAGER,
   "Lưu hoặc xóa cài đặt sẵn của bộ lọc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_FILE_INFO,
   "Tệp cài đặt sẵn đang dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_FILE_INFO,
   "Bộ lọc cài đặt sẵn hiện tại đang sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Áp dụng thay đổi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Các thay đổi trong cấu hình bộ lọc sẽ có hiệu lực ngay lập tức. Dùng tùy chọn này nếu bạn thay đổi số lượng pass của bộ lọc, lọc, tỉ lệ FBO, v.v."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_SHADER_APPLY_CHANGES,
   "Sau khi thay đổi cài đặt bộ lọc như số lượng pass, lọc, tỉ lệ FBO, hãy dùng tùy chọn này để áp dụng thay đổi.\nViệc thay đổi các cài đặt shader này là một thao tác khá tốn tài nguyên nên phải thực hiện một cách rõ ràng.\nKhi bạn áp dụng bộ lọc, các cài đặt bộ lọc sẽ được lưu vào một tệp tạm thời (retroarch.slangp/.cgp/.glslp) và được tải. Tệp này vẫn tồn tại sau khi thoát Retr[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Tham số bộ lọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Chỉnh sửa trực tiếp bộ lọc hiện tại. Các thay đổi sẽ không được lưu vào tệp cài đặt sẵn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Các bước xử lý bộ lọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Tăng hoặc giảm số lần xử lý của chuỗi shader. Có thể gán shader riêng cho từng lần xử lý và cấu hình tỉ lệ cũng như bộ lọc của nó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_NUM_PASSES,
   "RetroArch cho phép kết hợp nhiều bộ lọc với các bước bộ lọc tùy ý, cùng bộ lọc phần cứng và hệ số phóng đại tùy chỉnh.\nTùy chọn này xác định số bước bộ lọc sẽ sử dụng. Nếu bạn đặt là 0 và chọn Áp dụng Thay đổi bộ lọc, bạn sẽ dùng bộ lọc “trống”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "Bộ lọc Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_PASS,
   "Đường dẫn tới bộ lọc. Tất cả các bộ lọc phải cùng loại (ví dụ: Cg, GLSL hoặc Slang). Thiết lập Thư mục Bộ lọc để chọn nơi trình duyệt bắt đầu tìm các bộ lọc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Bộ lọc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_FILTER_PASS,
   "Bộ lọc phần cứng cho lần xử lý này. Nếu chọn 'Mặc định', bộ lọc sẽ là 'Tuyến tính' hoặc 'Gần nhất' tùy thuộc vào cài đặt 'Lọc Bilinear' trong cài đặt Video."
  )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Tỷ lệ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SCALE_PASS,
   "Tỷ lệ cho lần xử lý này. Hệ số tỷ lệ được cộng dồn, ví dụ 2x cho lần xử lý đầu tiên và 2x cho lần xử lý thứ hai sẽ cho tỷ lệ tổng 4x.\nNếu có hệ số tỷ lệ cho lần xử lý cuối, kết quả sẽ được kéo dãn ra màn hình với bộ lọc mặc định, tùy thuộc vào cài đặt Lọc Bilinear trong cài đặt Video.\nNếu chọn 'Mặc định', sẽ sử dụng tỷ lệ 1x hoặc kéo dãn toàn màn hình tùy th[...]"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Cài đặt sẵn đơn giản"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Lưu một bộ lọc đã thiết lập sẵn, liên kết với bộ lọc gốc đang tải và chỉ bao gồm những thay đổi về tham số mà bạn đã thực hiện."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Lưu cài đặt sẵn hiện tại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CURRENT,
   "Lưu thiết lập bộ lọc hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Lưu cài đặt sẵn thành"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Lưu thiết lập bộ lọc hiện tại thành một bộ lọc sẵn có mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Lưu cài đặt toàn hệ thống"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Lưu các cài đặt bộ lọc hiện tại làm cài đặt mặc định toàn hệ thống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Lưu Cài đặt Mặc định của Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Lưu các cài đặt bộ lọc hiện tại làm mặc định cho core này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Lưu thiết lập thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Lưu thiết lập bộ lọc hiện tại làm mặc định cho tất cả tệp trong thư mục trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Lưu thiết lập trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Lưu thiết lập bộ lọc hiện tại làm mặc định cho trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Không tìm thấy thiết lập bộ lọc tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Xóa thiết lập toàn hệ thống"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Xóa thiết lập toàn hệ thống, được dùng cho tất cả trò chơi và tất cả Core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Xóa thiết lập Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Xóa thiết lập Core, được dùng cho tất cả trò chơi chạy bằng Core hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Xóa thiết lập thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Xóa thiết lập thư mục trò chơi, được dùng cho tất cả trò chơi trong thư mục làm việc hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Xóa thiết lập trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Xóa thiết lập trò chơi, chỉ được dùng cho trò chơi cụ thể này."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Không có tham số bộ lọc"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_INFO,
   "Tệp ghi đè đang hoạt động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_INFO,
   "Tệp ghi đè hiện đang được sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_LOAD,
   "Tải tệp ghi đè"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_LOAD,
   "Tải và thay thế cấu hình hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_FILE_SAVE_AS,
   "Lưu ghi đè thành"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_FILE_SAVE_AS,
   "Lưu cấu hình hiện tại thành một tệp ghi đè mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Lưu ghi đè Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Lưu tệp cấu hình ghi đè áp dụng cho tất cả trò chơi chạy bằng Core này. Sẽ được ưu tiên hơn cấu hình chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Xóa ghi đè Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Xóa tệp cấu hình ghi đè áp dụng cho tất cả trò chơi chạy bằng Core này."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Lưu ghi đè thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Lưu tệp cấu hình ghi đè áp dụng cho tất cả trò chơi được tải từ cùng thư mục với tệp hiện tại. Sẽ được ưu tiên hơn cấu hình chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Xóa ghi đè thư mục trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Xóa tệp cấu hình ghi đè áp dụng cho tất cả trò chơi được tải từ cùng thư mục với tệp hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Lưu ghi đè trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Lưu tệp cấu hình ghi đè áp dụng chỉ cho trò chơi hiện tại. Sẽ được ưu tiên hơn cấu hình chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Xóa Ghi đè Trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMOVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Xóa file cấu hình ghi đè chỉ áp dụng cho trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERRIDE_UNLOAD,
   "Gỡ bỏ Ghi đè"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERRIDE_UNLOAD,
   "Đặt lại tất cả tùy chọn về giá trị cấu hình toàn cục."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Không có Thành tựu để Hiển thị"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Hủy Tạm dừng Chế độ Thành tựu Thử thách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Giữ chế độ thành tựu Thử thách được bật cho phiên hiện tại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Hủy Tiếp tục Chế độ Thành tựu Thử thách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Giữ chế độ thành tựu Thử thách được tắt cho phiên hiện tại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "Tiếp tục Chế độ Thành tựu Thử thách đã Tắt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_REQUIRES_RELOAD,
   "Bạn phải tải lại core để tiếp tục Chế độ Thành tựu Thử thách"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Tạm dừng Chế độ Thành tựu Thử thách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Tạm dừng chế độ thành tựu Thử thách cho phiên hiện tại. Hành động này sẽ bật cheat, tua lại, chuyển động chậm và tải trạng thái lưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Tiếp tục Chế độ Thành tựu Thử thách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Tiếp tục chế độ thành tựu hardcore cho phiên hiện tại. Hành động này sẽ tắt cheat, tua lại, chuyển động chậm và tải trạng thái lưu, đồng thời đặt lại trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Máy chủ RetroAchievements không thể truy cập"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_SERVER_UNREACHABLE,
   "Một hoặc nhiều thành tựu mở khóa không được gửi đến máy chủ. Việc gửi lại sẽ được thử lại miễn là bạn để ứng dụng mở."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_DISCONNECTED,
   "Máy chủ RetroAchievements không thể truy cập. Sẽ thử lại cho đến khi thành công hoặc ứng dụng bị đóng."
)
MSG_HASH(
   MENU_ENUM_LABEL_CHEEVOS_SERVER_RECONNECTED,
   "Tất cả các yêu cầu đang chờ đã được đồng bộ thành công với máy chủ RetroAchievements."
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_IDENTIFYING_GAME,
   "Đang nhận diện trò chơi"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_FETCHING_GAME_DATA,
   "Đang lấy dữ liệu trò chơi"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_STARTING_SESSION,
   "Bắt đầu phiên"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Chưa đăng nhập"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Lỗi Mạng"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Trò chơi không xác định"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Không thể kích hoạt thành tựu với core này"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "Hash RetroAchievements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Mục cơ sở dữ liệu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Hiển thị thông tin cơ sở dữ liệu cho trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Không có mục nào để hiển thị"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Không có Core nào khả dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Không có tùy chọn Core nào khả dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Không có thông tin Core nào khả dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Không có bản sao lưu Core nào khả dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Không có mục ưa thích nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Không có lịch sử nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Không có hình ảnh nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Không có âm nhạc nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Không có video nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Không có thông tin nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Không có mục danh sách phát nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Không tìm thấy cài đặt nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Không tìm thấy thiết bị Bluetooth nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Không tìm thấy mạng nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Không có Core nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Tìm kiếm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "Chuyển hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RANDOM_SELECT,
   "Chọn ngẫu nhiên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Quay lại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Thư mục mẹ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_PARENT_DIRECTORY,
   "Quay lại thư mục mẹ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Không tìm thấy thư mục."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Không có mục nào"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Chọn tệp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_NORMAL,
   "Bình thường"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90 độ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180 độ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270 độ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_NORMAL,
   "Bình thường"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90 độ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180 độ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270 độ"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Không rõ trình biên dịch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Chia sẻ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Bám móc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Bình chọn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Chia sẻ điều khiển Analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Tối đa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Trung bình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Không ưu tiên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Nhảy Trái/Phải"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Cuộn sang trái"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Chế độ hình ảnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Chế độ giọng nói"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Chế độ người kể chuyện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Lịch sử & Yêu thích"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Tất cả danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Lịch sử & Yêu thích"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Luôn luôn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Không bao giờ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Theo Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Tổng hợp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Đã sạc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Đang sạc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Không sạc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "Không có nguồn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Dùng thư mục này>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_USE_THIS_DIRECTORY,
   "Chọn mục này để đặt làm thư mục."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Mục trò chơi>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Mặc định>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Không có gì>"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
   "Bàn phím"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD,
   "Tay cầm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "Tay cầm có Analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Không xác định"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Xuống + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Nhấn giữ Start (2 giây)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Giữ Select (2 giây)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Xuống + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Tắt>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Thay đổi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Không thay đổi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Tăng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Giảm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= Giá trị rung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= Giá trị rung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< Giá trị rung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> Giá trị rung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Tăng theo giá trị rung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Giảm theo giá trị rung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Tất cả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Tắt>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Đặt bằng giá trị"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Tăng theo giá trị"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Giảm theo giá trị"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Chạy mã gian tiếp theo nếu giá trị = bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Chạy mã Cheat tiếp theo nếu giá trị != bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Chạy mã Cheat tiếp theo nếu giá trị < bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Chạy mã Cheat tiếp theo nếu giá trị > bộ nhớ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Trình giả lập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Bit, Giá trị tối đa = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-Bit, Giá trị tối đa = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-Bit, Giá trị tối đa = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-Bit, Giá trị tối đa = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-Bit, Giá trị tối đa = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-Bit, Giá trị tối đa = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Mặc định hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Theo thứ tự chữ cái"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Hiển thị nhãn đầy đủ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Xóa trò chơi trong ()"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Xóa trò chơi trong []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Xóa trò chơi trong () và []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Giữ nguyên vùng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Giữ chỉ số đĩa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Giữ vùng và chỉ số đĩa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Mặc định hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Ảnh bìa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Màn hình tiêu đề"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_LOGOS,
   "Logo trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "Bình thường"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Nhanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "BẬT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Có"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Đúng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Sai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Đã bật"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Tắt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "Không áp dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Đã khóa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Đã mở"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
   "Chế độ Thử thách"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Không chính thức"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Không được hỗ trợ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Mới mở khóa gần đây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Sắp hoàn thành"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Thử thách đang hoạt động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Chỉ Trình theo dõi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Chỉ Thông báo"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Mặc định"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Tuyến tính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Gần nhất"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Mục chính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Chọn trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Thư mục trò chơi>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Tùy chỉnh>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Không xác định>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Analog Trái"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Analog Phải"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Analog Trái (Bắt buộc)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Analog Phải (Bắt buộc)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "Phím %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Chuột 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Chuột 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Chuột 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Chuột 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Chuột 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Cuộn chuột lên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Cuộn chuột xuống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Cuộn chuột sang trái"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Cuộn chuột sang phải"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Sớm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "Bình thường"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Muộn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "Năm-Tháng-Ngày Giờ:Tháng:Giây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "Năm-Tháng-Ngày Giờ:Tháng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "Năm-Tháng-Ngày"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "Năm-Tháng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "Tháng-Ngày-Năm Giờ:Phút:Giây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "Tháng-Ngày-Năm Giờ:Phút"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "Tháng-Ngày Giờ:Phút"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "Tháng-Ngày-Năm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD,
   "Tháng-Ngày"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "Ngày-Tháng-Năm Giờ:Phút:Giây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "Ngày-Tháng-Năm Giờ:Phút"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "Ngày-Tháng Giờ:Phút"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "Ngày-Tháng-Năm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM,
   "Ngày-Tháng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS,
   "Giờ:Phút:Giây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM,
   "Giờ:Phút"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "Năm-Tháng-Ngày Giờ:Phút:Giây (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "Năm-Tháng-Ngày Giờ:Phút (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "Tháng-Ngày-Năm Giờ:Phút:Giây (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "Tháng-Ngày-Năm Giờ:Tháng (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "Tháng-Ngày Giờ:Tháng (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "Ngày-Tháng-Năm Giờ:Tháng:Giây (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "Ngày-Tháng-Năm Giờ:Tháng (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "Ngày-Tháng Giờ:Tháng (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM,
   "Giờ:Tháng:Giây (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM,
   "Giờ:Tháng (SA/CH)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "Trước"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Độ dày nền lưới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Tăng độ thô của hoa văn nền dạng bàn cờ trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Viền lưới"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Độ dày viền lưới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Tăng độ thô của hoa văn viền dạng bàn cờ trong menu."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Hiển thị viền menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Sử dụng bố cục toàn chiều rộng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Thay đổi kích thước và vị trí các mục menu để tận dụng tốt nhất không gian màn hình có sẵn. Tắt tùy chọn này để dùng bố cục cổ điển hai cột với độ rộng cố định."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Bộ lọc tuyến tính"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Thêm hiệu ứng mờ nhẹ cho menu để làm mềm các cạnh điểm ảnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Tăng cường nội bộ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Phóng to giao diện menu trước khi vẽ lên màn hình. Khi sử dụng cùng với 'Bộ lọc tuyến tính của menu' được bật, sẽ loại bỏ hiện tượng răng cưa khi phóng to (điểm ảnh không đều) đồng thời vẫn giữ hình ảnh sắc nét. Ảnh hưởng đáng kể đến hiệu năng, tăng theo mức độ phóng to."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Tỷ lệ khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Chọn tỷ lệ khung hình cho menu. Tỷ lệ màn hình rộng giúp tăng độ phân giải ngang của giao diện menu. (Có thể cần khởi động lại nếu 'Khóa tỷ lệ khung hình menu' bị tắt)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Khóa tỷ lệ khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Đảm bảo menu luôn hiển thị với tỷ lệ khung hình chính xác. Nếu tắt, menu nhanh sẽ bị kéo giãn để khớp với trò chơi đang chạy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Chủ đề màu sắc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Chọn chủ đề màu khác. Chọn 'Tùy chỉnh' để cho phép sử dụng các tệp preset chủ đề menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Cài đặt sẵn chủ đề tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Chọn cài đặt sẵn chủ đề menu từ Trình duyệt tệp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Độ trong suốt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Hiển thị nền trò chơi trong khi Menu Nhanh đang mở. Tắt độ trong suốt có thể làm thay đổi màu của chủ đề."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Hiệu ứng bóng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Bật đổ bóng cho văn bản, viền và hình thu nhỏ trong menu. Ảnh hưởng nhẹ đến hiệu năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Hoạt ảnh nền"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Bật hiệu ứng hoạt ảnh hạt nền. Có ảnh hưởng đáng kể đến hiệu năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Tốc độ Hoạt ảnh Nền"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Điều chỉnh tốc độ hiệu ứng hoạt ảnh hạt nền."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Hoạt ảnh Nền Bảo vệ Màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Hiển thị hiệu ứng hoạt ảnh hạt nền khi màn hình bảo vệ menu đang hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Hiển thị Ảnh Thu Nhỏ Danh Sách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Bật hiển thị ảnh thu nhỏ giảm kích thước trực tiếp khi xem danh sách. Có thể bật/tắt bằng RetroPad Select. Khi tắt, ảnh thu nhỏ vẫn có thể xem toàn màn hình bằng RetroPad Start."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Ảnh Thu Nhỏ Trên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Loại hình thu nhỏ hiển thị ở góc trên bên phải của danh sách phát. Có thể thay đổi bằng cần analog phải lên/xuống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Ảnh Thu Nhỏ Dưới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Loại hình thu nhỏ hiển thị ở dưới cùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Đổi Chỗ Ảnh Thu Nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Đổi vị trí hiển thị của 'Ảnh Thu Nhỏ Trên' và 'Ảnh Thu Nhỏ Dưới'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Phương pháp thu nhỏ hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Phương pháp nội suy được sử dụng khi thu nhỏ các hình thu nhỏ lớn để vừa với màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Độ trễ hình thu nhỏ (mlili giây)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Thêm độ trễ thời gian giữa lúc chọn một mục trong danh sách và lúc tải hình thu nhỏ tương ứng. Đặt giá trị ít nhất 256 ms sẽ giúp cuộn nhanh và mượt ngay cả trên thiết bị chậm."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Hỗ trợ ASCII mở rộng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Bật hiển thị các ký tự ASCII không tiêu chuẩn. Cần thiết để tương thích với một số ngôn ngữ Tây phương không dùng tiếng Anh. Có thể ảnh hưởng nhẹ đến hiệu năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Chuyển đổi biểu tượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Dùng biểu tượng thay cho chữ BẬT/TẮT để hiển thị các tùy chọn kiểu chuyển đổi trong menu."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Lấy mẫu gần nhất (Nhanh)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "Song tuyến tính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (Chậm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (Căn giữa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (Căn giữa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_21_9_CENTRE,
   "21:9 (Căn giữa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (Căn giữa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (Căn giữa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Vừa khít màn hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Tỷ lệ số nguyên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Lấp đầy màn hình (kéo giãn)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Đỏ cổ điển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Cam cổ điển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Vàng cổ điển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Xanh lá cổ điển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Xanh dương cổ điển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Tím cổ điển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Xám cổ điển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Đỏ cổ điển kiểu cũ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Tím đậm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Xanh đậm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Vàng ánh kim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Xanh điện tử"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Xanh táo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Đỏ núi lửa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango Tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango Sáng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ANTI_ZENBURN,
   "Chống Zenburn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Xám Tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Xám Sáng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Tuyết (Nhẹ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Tuyết (Dày)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Mưa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "Xoáy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Trường Sao"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Ảnh Thu Nhỏ Phụ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Loại hình thu nhỏ hiển thị ở bên trái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ICON_THUMBNAILS,
   "Biểu tượng thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ICON_THUMBNAILS,
   "Loại biểu tượng thu nhỏ của danh sách phát sẽ hiển thị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Nền năng động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Tải hình nền mới động theo ngữ cảnh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Hoạt ảnh ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Bật hoạt ảnh ngang cho menu. Tùy chọn này có thể làm giảm hiệu suất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Hoạt ảnh tô sáng biểu tượng ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Hiệu ứng hoạt ảnh khi cuộn giữa các thẻ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Hoạt ảnh di chuyển Lên/Xuống"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Hiệu ứng hoạt ảnh khi di chuyển lên hoặc xuống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Hoạt ảnh Mở/Đóng menu chính"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Hiệu ứng hoạt ảnh khi mở hoặc đóng menu con."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Hệ số trong suốt của chủ đề màu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON,
   "Biểu tượng Menu hiện tại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_CURRENT_MENU_ICON,
   "Biểu tượng menu hiện tại có thể được ẩn đi, nằm dưới menu ngang hoặc trong tiêu đề đầu trang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_NORMAL,
   "Bình thường"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_CURRENT_MENU_ICON_TITLE,
   "Tiêu đề"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Phông chữ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Chọn phông chữ chính khác để dùng cho menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Màu chữ (Đỏ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Màu chữ (Lục)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Màu chữ (Lam)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Bố cục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Chọn bố cục khác cho giao diện XMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Chủ đề biểu tượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Chọn chủ đề biểu tượng khác cho RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Chuyển đổi biểu tượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "Dùng biểu tượng thay cho chữ BẬT/TẮT để hiển thị các tùy chọn kiểu chuyển đổi trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Hiệu ứng bóng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Vẽ bóng đổ cho biểu tượng, hình thu nhỏ và chữ. Việc này có thể làm giảm hiệu năng một chút."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Chuỗi xử lý Shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Chọn hiệu ứng nền động. Có thể tiêu tốn GPU tùy theo hiệu ứng. Nếu hiệu năng không đạt, hãy tắt tùy chọn này hoặc chọn hiệu ứng đơn giản hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Chủ đề màu sắc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Chọn chủ đề màu nền khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Bố trí dọc của hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Hiển thị hình thu nhỏ bên trái dưới hình bên phải, ở phía bên phải màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Hệ số tỉ lệ hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Giảm kích thước hiển thị hình thu nhỏ bằng cách thu nhỏ chiều rộng tối đa cho phép."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Hệ số làm mờ theo chiều dọc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Hiển thị tiêu đề đầu trang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Lề Tiêu đề"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Khoảng cách ngang của tiêu đề"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Bật tab Cài đặt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Hiển thị tab Cài đặt chứa các thiết lập của chương trình."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Dải băng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Dải băng (Đơn giản)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Tuyết đơn giản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Tuyết"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Hoa tuyết"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Đơn sắc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Đơn sắc đảo ngược"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
   "Điểm ảnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
   "Hệ thống hoài cổ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
   "Nghệ thuật chấm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Tự động đảo ngược"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Xanh táo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Sáng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Xanh sáng ban mai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Tia nắng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Tím đậm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Xanh điện tử"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Vàng ánh kim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Đỏ cổ điển kiểu cũ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Xanh đậm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Ảnh nền"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Dưới biển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Đỏ núi lửa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Xanh chanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Vàng Pikachu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Khối lập phương tím"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Đỏ Gia đình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Nóng rực"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Lạnh buốt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_DARK,
   "Xám tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GRAY_LIGHT,
   "Xám sáng"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT,
   "Phông chữ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT,
   "Chọn phông chữ chính khác để dùng cho menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE,
   "Tỷ lệ phông chữ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE,
   "Xác định xem kích thước phông chữ trong menu có nên có tỷ lệ riêng hay không, và liệu nó được thu phóng toàn cục hay bằng các giá trị riêng cho từng phần của menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_GLOBAL,
   "Toàn cục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_SEPARATE,
   "Giá trị riêng biệt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Hệ số tỷ lệ phông chữ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_GLOBAL,
   "Thay đổi kích thước phông chữ tuyến tính trên toàn menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Hệ số tỷ lệ phông chữ tiêu đề"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TITLE,
   "Tỷ lệ kích thước phông chữ cho văn bản tiêu đề trong phần đầu menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Hệ số tỷ lệ phông chữ thanh bên trái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SIDEBAR,
   "Tỷ lệ kích thước phông chữ cho văn bản trong thanh bên trái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Hệ số tỷ lệ phông chữ nhãn"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_LABEL,
   "Tỷ lệ kích thước phông chữ cho nhãn của các tùy chọn menu và mục danh sách phát. Cũng ảnh hưởng đến kích thước chữ trong hộp trợ giúp."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Hệ số tỷ lệ phông chữ phụ đề"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_SUBLABEL,
   "Tỷ lệ kích thước phông chữ cho phần phụ đề của các tùy chọn menu và mục danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_TIME,
   "Hệ số tỷ lệ phông chữ giờ/ngày"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_TIME,
   "Tỷ lệ kích thước phông chữ của hiển thị thời gian và ngày ở góc trên bên phải menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Hệ số tỷ lệ phông chữ chân trang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_FONT_SCALE_FACTOR_FOOTER,
   "Tỷ lệ kích thước phông chữ của văn bản trong phần chân menu. Cũng ảnh hưởng đến kích thước chữ trong thanh bên hình thu nhỏ bên phải."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Thu gọn thanh bên"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Luôn thu gọn thanh bên trái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Rút ngắn tên danh sách phát (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Xóa tên nhà sản xuất khỏi danh sách phát. Ví dụ, 'Sony - PlayStation' sẽ thành 'PlayStation'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Sắp xếp lại danh sách phát sau khi rút gọn tên (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Danh sách phát sẽ được sắp xếp lại theo thứ tự bảng chữ cái sau khi xóa phần tên nhà sản xuất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Ảnh thu nhỏ phụ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Thay thế bảng thông tin trò chơi bằng một ảnh thu nhỏ khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Sử dụng văn bản chạy cho siêu dữ liệu trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Khi bật, mỗi mục siêu dữ liệu trò chơi hiển thị ở thanh bên phải của danh sách phát (core liên kết, thời gian chơi) sẽ chiếm một dòng; các chuỗi vượt quá chiều rộng của thanh bên sẽ được hiển thị dưới dạng văn bản chạy. Khi tắt, mỗi mục siêu dữ liệu trò chơi sẽ hiển thị tĩnh, xuống dòng để chiếm nhiều dòng nếu cần."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Hệ số tỉ lệ hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Thay đổi kích thước thanh hình thu nhỏ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_PADDING_FACTOR,
   "Hệ số đệm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_PADDING_FACTOR,
   "Thay đổi kích thước đệm ngang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON,
   "Biểu tượng phần đầu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_ICON,
   "Logo phần đầu có thể được ẩn, thay đổi linh hoạt tùy theo cách điều hướng, hoặc cố định theo kiểu “classic invader”."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR,
   "Ngăn cách tiêu đề"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_HEADER_SEPARATOR,
   "Chiều rộng thay thế cho ngăn cách tiêu đề và chân trang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_DYNAMIC,
   "Động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_ICON_FIXED,
   "Cố định"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NORMAL,
   "Bình thường"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_MAXIMUM,
   "Tối đa"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Chủ đề màu sắc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Chọn chủ đề màu khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Trắng cơ bản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Đen cơ bản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox Tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY,
   "Dâu tằm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Xâm nhập nhân hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "Vùng hoàng hôn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA,
   "Ma cà rồng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Tối năng lượng mặt trời"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Sáng năng lượng mặt trời"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Xám tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Xám sáng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN,
   "Mưa tím"
   )


/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Biểu tượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Hiển thị biểu tượng ở bên trái các mục trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Chuyển đổi biểu tượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "Dùng biểu tượng thay cho chữ BẬT/TẮT để hiển thị các tùy chọn kiểu chuyển đổi trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Biểu tượng danh sách phát (Cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Hiển thị biểu tượng riêng theo hệ thống trong danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Tối ưu bố cục ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Tự động điều chỉnh bố cục menu để phù hợp hơn với màn hình khi sử dụng chế độ hiển thị ngang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Hiển thị thanh điều hướng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Hiển thị phím tắt điều hướng menu trên màn hình vĩnh viễn. Cho phép chuyển nhanh giữa các danh mục menu. Khuyến nghị cho thiết bị cảm ứng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Tự động xoay thanh điều hướng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Tự động di chuyển thanh điều hướng sang bên phải màn hình khi sử dụng chế độ hiển thị ngang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Chủ đề màu sắc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Chọn chủ đề màu nền khác."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Hiệu ứng chuyển cảnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Bật hiệu ứng chuyển động mượt khi điều hướng giữa các cấp của menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Chế độ xem hình thu nhỏ dọc"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Chỉ định chế độ xem hình thu nhỏ danh sách khi sử dụng hiển thị dọc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Chế độ xem hình thu nhỏ ngang"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Chỉ định chế độ xem hình thu nhỏ danh sách khi sử dụng hiển thị ngang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Hiển thị hình thu nhỏ phụ trong chế độ xem danh sách"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Hiển thị hình thu nhỏ thứ hai khi sử dụng chế độ xem danh sách kiểu 'List'. Thiết lập này chỉ áp dụng khi màn hình có đủ độ rộng vật lý để hiển thị hai hình thu nhỏ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Nền hình thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Bật đệm phần không sử dụng trong hình thu nhỏ bằng nền đặc. Việc này giúp các hình ảnh có kích thước hiển thị đồng đều, cải thiện giao diện menu khi xem hình thu nhỏ nội dung hỗn hợp có kích thước gốc khác nhau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Hình thu nhỏ chính"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Loại hình thu nhỏ chính liên kết với mỗi mục trong danh sách. Thường được dùng làm biểu tượng trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Hình thu nhỏ phụ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Loại hình thu nhỏ bổ sung để liên kết với mỗi mục trong danh sách phát. Cách sử dụng phụ thuộc vào chế độ hiển thị hình thu nhỏ hiện tại của danh sách phát."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Xanh dương"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Xám Xanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Xanh Đậm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Xanh Lá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "Khiên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Đỏ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Vàng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI,
   "Giao diện Material"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Giao diện Material Tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozone Tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Xanh Dễ Thương"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Xanh ngọc dễ thương"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Xanh lá dễ thương"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Cam dễ thương"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Hồng dễ thương"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Tím dễ thương"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Đỏ dễ thương"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY,
   "Máy Virtual Boy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Xâm nhập nhân hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Xám tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Xám sáng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Mờ dần"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Trượt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Danh sách (nhỏ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Danh sách (vừa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Biểu tượng kép"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Danh sách (nhỏ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Danh sách (vừa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Danh sách (Lớn)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Giao diện máy tính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "TẮT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "BẬT"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Loại trừ chế độ xem thu nhỏ"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Thông tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Tập tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Tải Core..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Gỡ Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "&Tắt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Sửa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Tìm kiếm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Xem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Các Dock đã đóng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Tham số Shader"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Cài đặt..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Ghi nhớ vị trí dock:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Ghi nhớ hình dạng cửa sổ:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Ghi nhớ thẻ trò chơi cuối cùng đã mở:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Giao diện:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Hệ thống mặc định>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Tối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Tùy chỉnh..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Thiết lập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Công cụ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Trợ giúp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "Giới thiệu về RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Tài liệu hướng dẫn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Tải Core tùy chỉnh..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Tải Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Đang tải Core..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Tên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Phiên bản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Quản lý tập tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Trên cùng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Lên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Trình duyệt trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Ảnh bìa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Màn hình tiêu đề"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Tất cả danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Thông tin Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Hỏi tôi>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Thông tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Cảnh báo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Lỗi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Lỗi Mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Vui lòng khởi động lại chương trình để các thay đổi có hiệu lực."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Nhật ký"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Kéo thả hình vào đây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Không hiển thị lại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Dừng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Liên kết Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Danh sách phát ẩn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Ẩn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Màu nổi bật:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Chọn..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Chọn màu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Chọn giao diện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Giao diện tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Đường dẫn tệp trống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Tệp rỗng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Không thể mở tệp để đọc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Không thể mở tệp để ghi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Tệp không tồn tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Gợi ý: hãy tải Core trước:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Phóng to/thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Xem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Biểu tượng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Danh sách"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Xóa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Tiến trình:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "Số mục tối đa của \"Tất cả danh sách phát\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "Số mục tối đa lưới của \"Tất cả danh sách phát\":"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Hiển thị file và thư mục ẩn:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Danh sách phát mới"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Vui lòng nhập tên danh sách phát mới:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Xóa danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Đổi tên danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Bạn có chắc muốn xóa danh sách phát \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Câu hỏi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Không thể xóa file."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Không thể đổi tên file."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Đang thu thập danh sách file..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Đang thêm file vào danh sách phát..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Mục danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Tên:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Đường dẫn:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Cơ sở dữ liệu:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Phần mở rộng:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(cách nhau bằng dấu cách; mặc định bao gồm tất cả)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Lọc trong file nén"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(được dùng để tìm hình thu nhỏ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Bạn có chắc muốn xóa mục \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Vui lòng chọn một danh sách phát duy nhất trước."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Xóa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Thêm mục..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Thêm tệp..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Thêm Thư mục..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Chỉnh sửa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Chọn Thư mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Chọn Thư mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<nhiều>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Lỗi khi cập nhật mục trong danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Vui lòng điền tất cả các trường bắt buộc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "Cập nhật RetroArch (phiên bản nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch đã được cập nhật thành công. Vui lòng khởi động lại ứng dụng để các thay đổi có hiệu lực."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Cập nhật thất bại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Người đóng góp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Shader hiện tại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Di chuyển Xuống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Di chuyển Lên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Tải"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Lưu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Xóa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Xóa Passes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Áp dụng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Thêm Pass"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Xóa tất cả Passes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Không có shader passes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Đặt lại mật khẩu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Đặt lại tất cả mật khẩu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Đặt lại tham số"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Tải ảnh thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Đang có tiến trình tải xuống."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Bắt đầu từ danh sách phát:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Ảnh thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Giới hạn bộ nhớ đệm ảnh thu nhỏ:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Giới hạn kích thước ảnh thu nhỏ kéo-thả:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Tải tất cả ảnh thu nhỏ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Toàn bộ hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Danh sách phát này"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Ảnh thu nhỏ đã tải xuống thành công."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Thành công: %1 Thất bại: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Tùy chọn Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Đặt lại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Đặt lại tất cả"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Cài đặt Cập nhật Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Tài khoản Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Điểm cuối của danh sách tài khoản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Tự động nhấn / Vùng chết"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
   "Thành tựu Retro"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Bộ đếm Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Chưa chọn đĩa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Bộ đếm Giao diện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Menu ngang"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Ẩn Mô Tả Input Core Không Ràng Buộc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Hiển Thị Nhãn Mô Tả Input"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Giao diện Trên Màn hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Lịch sử"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Chọn trò chơi từ danh sách chơi gần đây nhất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_HISTORY,
   "Khi trò chơi được mở, kết hợp giữa trò chơi và core libretro sẽ được lưu vào lịch sử.\nLịch sử được lưu vào một file trong cùng thư mục với file cấu hình RetroArch. Nếu không có file cấu hình nào được tải khi khởi động, lịch sử sẽ không được lưu hoặc tải, và sẽ không tồn tại trong menu chính."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "Đa phương tiện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Hệ thống con"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Truy cập cài đặt hệ thống con cho trò chơi hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   "Trò chơi hiện tại: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Không tìm thấy máy chủ Trò chơi trực tuyến."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Không tìm thấy client Trò chơi trực tuyến."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Không có bộ đếm hiệu năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Không có danh sách phát."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Đã kết nối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "Trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "Cổng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Cổng %d Tên Thiết Bị: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Tên hiển thị thiết bị: %s\nTên cấu hình thiết bị: %s\nVID/PID của thiết bị: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Cài đặt Cheat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Bắt đầu hoặc Tiếp tục Tìm Cheat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Phát trong Trình phát phương tiện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "giây"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Khởi động Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Khởi động Core mà không có trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Các core được đề xuất"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Không thể đọc file nén."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Người dùng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "Bàn phím"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Số ảnh Swapchain tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Báo cho driver video sử dụng một chế độ đệm cụ thể."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Số lượng ảnh swapchain tối đa. Điều này có thể báo cho driver video sử dụng một chế độ đệm video cụ thể.\nĐệm đơn - 1\nĐệm đôi - 2\nĐệm ba - 3\nChọn đúng chế độ đệm có thể ảnh hưởng lớn đến độ trễ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "Swapchain có thể chờ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Đồng bộ cứng CPU và GPU. Giảm độ trễ nhưng giảm hiệu suất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Độ trễ khung tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Báo cho driver video sử dụng một chế độ đệm cụ thể."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Chỉnh sửa shader preset hiện đang được sử dụng trong menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Bộ lọc cài đặt sẵn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Bộ lọc cài đặt sẵn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "Bộ lọc cài đặt sẵn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Duyệt URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "Đường dẫn URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Bắt đầu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Biệt danh: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Đang tìm trò chơi tương thích..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "Không tìm thấy core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "Không tìm thấy danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Tìm thấy trò chơi tương thích"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "Không thể tìm trò chơi phù hợp theo CRC hoặc tên tệp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Bắt đầu Gong"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "Biệt danh (LAN): %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "Trạng thái"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Hệ thống BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Hỗ trợ ghi hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "Lưu bản ghi đầu ra dưới tên..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "Lưu các bản ghi trong thư mục đầu ra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "Xem trận đấu #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "Chọn mục để xem."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "Xem danh sách %u mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "Tạo mã từ mục này"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "Xóa mục này"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "Độ mờ chân trang"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "Thay đổi độ mờ của hình chân trang."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "Độ mờ đầu trang"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "Thay đổi độ mờ của hình đầu trang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "Bắt đầu trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Đường dẫn lịch sử trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "ID hiển thị đầu ra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Chọn cổng xuất kết nối với màn hình CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Trợ giúp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLEAR_SETTING,
   "Xóa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Khắc phục sự cố âm thanh/video"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "Thay đổi lớp phủ Tay cầm ảo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "Đang tải trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "Quét trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "Core là gì?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Thiết lập cơ sở dữ liệu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Độ trễ khung hình Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Quét mạng cục bộ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Tìm kiếm và kết nối với các máy chủ Trò chơi trực tuyến trên mạng cục bộ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Trò chơi trực tuyến - Máy khách"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Chế độ quan sát Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Miêu tả"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "Giới hạn tốc độ chạy tối đa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "Bắt đầu tìm mã gian lận mới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "Bắt đầu tìm một mã gian lận mới. Số bit có thể thay đổi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Tiếp tục tìm kiếm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Tiếp tục tìm một mã gian lận mới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Thành tích (Chế độ Thử thách)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Chi tiết Cheat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Quản lý thiết lập chi tiết Cheat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Bắt đầu hoặc Tiếp tục Tìm Cheat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Bắt đầu hoặc tiếp tục tìm kiếm mã Cheat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Vòng Cheat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Tăng hoặc giảm số lượng mã Cheat."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Analog Trái X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Analog Trái Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Analog Phải X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Analog Phải Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Bắt đầu hoặc Tiếp tục Tìm Cheat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Danh sách con trỏ cơ sở dữ liệu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Cơ sở dữ liệu - Lọc: Nhà phát triển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Cơ sở dữ liệu - Lọc: Nhà phát hành"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Cơ sở dữ liệu - Lọc: Xuất xứ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Cơ sở dữ liệu - Lọc: Nhượng quyền"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Cơ sở dữ liệu - Lọc: Đánh giá ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Cơ sở dữ liệu - Lọc: Đánh giá ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Cơ sở dữ liệu - Lọc: Đánh giá PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Cơ sở dữ liệu - Lọc: Đánh giá CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Cơ sở dữ liệu - Lọc: Đánh giá BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Cơ sở dữ liệu - Bộ lọc: Số lượng người dùng tối đa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Cơ sở dữ liệu - Bộ lọc: Ngày phát hành theo tháng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Cơ sở dữ liệu - Bộ lọc: Ngày phát hành theo năm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Cơ sở dữ liệu - Bộ lọc: Số phát hành tạp chí Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Cơ sở dữ liệu - Bộ lọc: Đánh giá tạp chí Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_GENRE,
   "Cơ sở dữ liệu - Bộ lọc: Thể loại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_REGION,
   "Cơ sở dữ liệu - Bộ lọc: Vùng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Thông tin cơ sở dữ liệu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Cấu hình"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Tải về"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Thiết lập Trò chơi trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Hỗ trợ tiếng lóng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "Hỗ trợ kết xuất OpenGL/Direct3D vào texture (shader nhiều lần)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Chọn trò chơi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "Thường được các nhà phát triển cài sẵn khi đóng gói ứng dụng libretro/RetroArch để trỏ tới tài nguyên."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Hỏi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Menu điều khiển cơ bản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Xác nhận/OK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "Thông tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "Thoát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Cuộn lên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Mặc định"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Bật/tắt bàn phím"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Bật/tắt Menu"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "Trong Menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "Trong Trò chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "Trong Trò chơi (Tạm dừng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Đang chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Tạm dừng"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Trò chơi trực tuyến sẽ bắt đầu khi trò chơi được mở."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "Phải mở trò chơi trước khi bắt đầu Trò chơi trực tuyến."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Không tìm thấy core hoặc tệp trò chơi phù hợp, hãy tải thủ công."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Driver đồ họa của bạn không tương thích với driver video hiện tại trong RetroArch, đang chuyển sang driver %s. Vui lòng khởi động lại RetroArch để thay đổi có hiệu lực."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Cài đặt core thành công"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Cài đặt core thất bại"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Nhấn sang phải năm lần để xóa tất cả cheat."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Âm lượng tổng của bộ trộn âm thanh"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Quét Trò chơi trực tuyến hoàn tất."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Xin lỗi, chưa hỗ trợ: các core không cần trò chơi không thể tham gia Trò chơi trực tuyến."
   )
MSG_HASH(
   MSG_NATIVE,
   "Nguyên bản"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Đã nhận được lệnh Trò chơi trực tuyến không xác định"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Tệp đã tồn tại. Đang lưu vào bộ đệm sao lưu"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "Đã nhận kết nối từ: \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Đã nhận kết nối từ: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Bản đồ cổng Trò chơi trực tuyến thành công"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "Mạng ngoài có địa chỉ riêng hoặc chia sẻ. Xem xét sử dụng máy chủ trung gian."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Bản đồ cổng Trò chơi trực tuyến UPnP thất bại"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "Không có tham số nào được cung cấp và không có menu tích hợp, hiển thị hướng dẫn..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Đang đặt đĩa vào khay"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Đang chờ kết nối từ client..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "Phòng của bạn không thể kết nối từ Internet."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Bạn đã rời trò chơi"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "Bạn đã tham gia với tư cách người chơi %u"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "Bạn đã tham gia với các thiết bị nhập %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "Người chơi %.*s đã rời trò chơi"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s đã tham gia với tư cách người chơi %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s đã tham gia với các thiết bị nhập %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYERS_INFO,
   "%d người chơi"
   )
MSG_HASH(
   MSG_NETPLAY_SPECTATORS_INFO,
   "%d người chơi (%d đang xem)"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Một lần thử kết nối Trò chơi trực tuyến thất bại vì đối phương không chạy RetroArch, hoặc đang chạy phiên bản RetroArch cũ."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Một đối phương Trò chơi trực tuyến đang chạy phiên bản RetroArch cũ. Không thể kết nối."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "CẢNH BÁO: Một đối phương Trò chơi trực tuyến đang chạy phiên bản RetroArch khác. Nếu xảy ra sự cố, hãy dùng cùng phiên bản."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Một đối phương Trò chơi trực tuyến đang chạy core khác. Không thể kết nối."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "CẢNH BÁO: Một đối phương Trò chơi trực tuyến đang chạy phiên bản core khác. Nếu xảy ra sự cố, hãy dùng cùng phiên bản."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Core này không hỗ trợ Trò chơi trực tuyến giữa các nền tảng này"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Core này không hỗ trợ Trò chơi trực tuyến giữa các nền tảng khác nhau"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Nhập mật khẩu máy chủ Trò chơi trực tuyến:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Nhập tin nhắn chat Trò chơi trực tuyến:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Bạn có muốn cho phép kết nối từ người dùng:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Mật khẩu không đúng"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" đã ngắt kết nối"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Một máy khách Trò chơi trực tuyến đã ngắt kết nối"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Ngắt kết nối Trò chơi trực tuyến"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "Bạn không có quyền chơi"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "Không còn khe chơi nào trống"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "Các thiết bị nhập liệu yêu cầu không khả dụng"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Không thể chuyển sang chế độ chơi"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Đối thủ Trò chơi trực tuyến \"%s\" đã tạm dừng"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Biệt danh của bạn đã đổi thành \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "Máy khách bị đá: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "Không thể đá máy khách: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "Máy khách bị cấm: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "Không thể cấm máy khách: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Đang chơi"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "Đang theo dõi"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "Thiết bị"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "Hỗ trợ chat"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "Nguyên nhân chậm"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Âm lượng âm thanh"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Tự động phát hiện"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Khả năng"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Đang kết nối vào máy chủ Trò chơi trực tuyến"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Đang kết nối vào cổng"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Khe kết nối"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "Đang lấy danh sách core..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Không thể lấy danh sách core!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "Phiên bản mới nhất đã được cài đặt "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Đang cập nhật core: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Đang tải core: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Đang giải nén core: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Core đã được cài đặt: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Không thể cài đặt core: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Đang quét các core..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Đang kiểm tra core: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Tất cả core đã cài ở phiên bản mới nhất"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Tất cả core được hỗ trợ đã chuyển sang phiên bản Play Store"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "Đã cập nhật core "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "Bỏ qua core "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Cập nhật core bị tắt - core đã bị khóa: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Đặt lại core: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Core đã được đặt lại: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Đang làm sạch danh sách phát: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Danh sách phát đã được làm sạch: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Làm mới thất bại - danh sách phát không có bản ghi quét hợp lệ: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Làm mới thất bại - thư mục trò chơi không hợp lệ/bị thiếu: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Làm mới thất bại - tên hệ thống không hợp lệ/bị thiếu: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Làm mới thất bại - core không hợp lệ: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Làm mới thất bại - file DAT arcade không hợp lệ/bị thiếu: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Làm mới thất bại - file DAT arcade quá lớn (bộ nhớ không đủ): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Đã thêm vào yêu thích"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Thêm vào yêu thích thất bại: danh sách phát đầy"
   )
MSG_HASH(
   MSG_ADDED_TO_PLAYLIST,
   "Đã thêm vào danh sách phát"
   )
MSG_HASH(
   MSG_ADD_TO_PLAYLIST_FAILED,
   "Thêm vào danh sách phát thất bại: danh sách phát đầy"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Core đã được đặt: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "Liên kết core trong mục playlist đã được đặt lại."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Đã thêm đĩa"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Không thể thêm đĩa"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Application Danh mục"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Đang áp dụng thay đổi Cheat."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Áp dụng bản vá: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Đang áp dụng shader"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Đã tắt âm thanh."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Đã bật âm thanh"
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Lỗi khi lưu cấu hình tay cầm."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Lưu cấu hình tay cầm thành công."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY_NAMED,
   "Cấu hình tay cầm đã được lưu trong thư mục Controller Profiles dưới tên\n\"%s\""
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Không thể khởi tạo lưu tự động."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Tự động lưu trạng thái vào"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Hiển thị giao diện lệnh trên cổng"
   )
MSG_HASH(
   MSG_BYTES,
   "byte"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Không thể suy ra đường dẫn cấu hình mới. Sử dụng thời gian hiện tại."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "So sánh với các số magic đã biết..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Biên dịch dựa trên API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Thư mục cấu hình chưa được thiết lập. Không thể lưu cấu hình mới."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Đã kết nối với"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "Trò chơi CRC32 khác nhau. Không thể dùng các trò chơi khác."
   )
MSG_HASH(
   MSG_CONTENT_NETPACKET_CRC32S_DIFFER,
   "Máy chủ đang chạy trò chơi khác."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "Ping của bạn quá cao so với máy chủ này."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Bỏ qua việc mở trò chơi. Bộ thực thi sẽ tự mở."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Core không hỗ trợ trạng thái lưu."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
   "Core không hỗ trợ Điều khiển Đĩa."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Tệp tùy chọn core được tạo thành công."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "Tệp tùy chọn core đã xóa thành công."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Tất cả tùy chọn core đã được đặt lại mặc định."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "Tùy chọn core đã lưu vào:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "Không lưu được tùy chọn core vào:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Không tìm thấy trình điều khiển tiếp theo nào"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Không tìm thấy hệ thống tương thích."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Không tìm thấy đường dữ liệu hợp lệ"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "Không thể mở đường dữ liệu"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "Không thể đọc tệp trò chơi"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Không thể đọc tiêu đề movie."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Không thể đọc trạng thái từ movie."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "Không khớp mã kiểm tra CRC32 giữa tệp trò chơi và mã đã lưu trong phần đầu tệp phát lại. Việc phát lại rất có thể sẽ bị lệch đồng bộ."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Thời gian tùy chỉnh đã được cung cấp"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Đang giải nén dữ liệu."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Giải nén thất bại."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Phát hiện viewport của"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Không tìm thấy bản vá trò chơi hợp lệ."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Ngắt kết nối thiết bị khỏi cổng hợp lệ."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Đã đóng khay đĩa ảo."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Đã nhả khay đĩa ảo."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Đang tải xuống"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "chỉ mục"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Tải xuống thất bại"
   )
MSG_HASH(
   MSG_ERROR,
   "Lỗi"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Core Libretro yêu cầu trò chơi, nhưng không có gì được cung cấp."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Core Libretro yêu cầu trò chơi đặc biệt, nhưng không có trò chơi nào được cung cấp."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Core không hỗ trợ VFS, và việc tải từ bản sao cục bộ thất bại"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Lỗi khi phân tích đối số."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Lỗi khi lưu file tùy chọn core."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "Lỗi khi xóa file tùy chọn core."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Lỗi khi lưu file remap."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Lỗi khi xóa file remap."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Lỗi khi lưu preset shader."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Thư mục Ứng dụng Bên ngoài"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Đang giải nén"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Đang giải nén file"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Lưu cấu hình thất bại tới"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Không chấp nhận được spectator đến."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Không cấp phát được bộ nhớ cho trò chơi đã vá..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Không áp dụng được shader."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Áp dụng preset shader thất bại:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Liên kết socket thất bại."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Tạo thư mục thất bại."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Giải nén trò chơi từ tệp nén thất bại"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Lấy biệt danh từ client thất bại."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Tải thất bại."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Mở trò chơi thất bại."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_FROM_PLAYLIST,
   "Tải từ playlist thất bại."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Tải file movie thất bại."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Tải overlay thất bại."
   )
MSG_HASH(
   MSG_OSK_OVERLAY_NOT_SET,
   "Overlay bàn phím chưa được thiết lập."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Tải trạng thái từ thất bại"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Mở libretro core thất bại"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Không thể vá"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Không thể nhận header từ khách."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Không thể nhận biệt danh."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Không thể nhận biệt danh từ máy chủ."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Không thể nhận kích thước biệt danh từ máy chủ."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "Không thể nhận dữ liệu SRAM từ máy chủ."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Không thể lấy đĩa ra khỏi khay."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Không thể xóa file tạm"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "Không thể lưu SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "Không thể tải SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Không thể lưu trạng thái vào"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Không thể gửi biệt danh."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Không thể gửi kích thước biệt danh."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Không gửi được nickname tới client."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Không gửi được biệt danh tới máy chủ."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "Không gửi được dữ liệu SRAM tới client."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Không khởi động được trình điều khiển âm thanh. Sẽ tiếp tục mà không có âm thanh."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Không thể bắt đầu ghi video."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Không thể bắt đầu ghi âm."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Không chụp được ảnh màn hình."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Không thể hoàn tác trạng thái đã tải."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Không thể hoàn tác trạng thái đã lưu."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Không thể bật âm thanh."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Đã nhận lỗi nghiêm trọng trong"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Không tìm thấy tệp"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Tìm thấy savestate tự đông trong"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Tìm thấy nhãn đĩa"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Tìm thấy track dữ liệu đầu tiên trong file"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Đã tìm thấy slot lưu trò chơi gần đây nhất"
   )
MSG_HASH(
   MSG_FOUND_LAST_REPLAY_SLOT,
   "Tìm thấy slot replay cuối cùng"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_INCOMPAT,
   "Không phải từ bản ghi hiện tại"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_HALT_INCOMPAT,
   "Không tương thích với replay"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_FUTURE_STATE,
   "Không thể tải trạng thái tương lai trong khi phát lại"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_FAILED_WRONG_TIMELINE,
   "Lỗi dòng thời gian trong khi phát lại"
   )
MSG_HASH(
   MSG_REPLAY_LOAD_STATE_OVERWRITING_REPLAY,
   "Dòng thời gian sai; ghi đè bản ghi"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT,
   "Quay lại"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_PREV_CHECKPOINT_FAILED,
   "Tua lùi thất bại"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT,
   "Tua tới"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_NEXT_CHECKPOINT_FAILED,
   "Tua tới thất bại"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME,
   "Hoàn tất tua"
   )
MSG_HASH(
   MSG_REPLAY_SEEK_TO_FRAME_FAILED,
   "Tua thất bại"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Đã tìm thấy shader"
   )
MSG_HASH(
   MSG_FRAMES,
   "Khung hình"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Tùy chọn core riêng cho trò chơi được tìm thấy tại"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Tùy chọn core riêng cho thư mục được tìm thấy tại"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Lấy chỉ số đĩa không hợp lệ."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Lấy trạng thái chuột"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Trò chơi đang nhận focus"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Trò chơi mất focus"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Core Libretro được kết xuất phần cứng. Cần sử dụng ghi hình sau shader."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "Checksum mở rộng không khớp với CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Nhập Cheat"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Nhập Tên Tệp Cheat"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Tên Tệp Cài Sẵn"
   )
MSG_HASH(
   MSG_INPUT_OVERRIDE_FILENAME,
   "Tên Tệp Ghi Đè"
   )
MSG_HASH(
   MSG_INPUT_REMAP_FILENAME,
   "Tên Tệp Định Nghĩa Lại Phím"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Đổi Tên Tiêu Đề"
   )
MSG_HASH(
   MSG_INTERFACE,
   "Giao Diện"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Bộ Nhớ Trong"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Bộ Nhớ Tháo Rời"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Kích thước biệt danh không hợp lệ."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "tính bằng byte"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "tính bằng megabyte"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "tính bằng gigabyte"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "được biên dịch với phiên bản libretro khác với phiên bản libretro này."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Giao diện người dùng cho libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Đã tải trạng thái từ khe: %d."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Đã tải trạng thái từ khe: Tự động."
   )
MSG_HASH(
   MSG_LOADING,
   "Đang tải"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Thiếu một hoặc nhiều file firmware"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Đang mở tệp trò chơi"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Đang nạp tập tin lịch sử"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Đang tải file yêu thích"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Đang tải trò chơi"
   )
MSG_HASH(
   MSG_MEMORY,
   "Bộ nhớ"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_REPLAY_FILE,
   "File Input replay movie không phải là REPLAY file hợp lệ."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Định dạng phim phát lại điều khiển dường như có phiên bản serializer khác. Khả năng cao là sẽ không thành công."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Đầu vào phát lại phim đã kết thúc."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Dừng ghi movie."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Khởi tạo Trò chơi trực tuyến thất bại."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "Core này không hỗ trợ Trò chơi trực tuyến."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Không có trò chơi, khởi chạy core giả lập."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Chưa có Lưu trạng thái nào bị ghi đè."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Chưa có Lưu trạng thái nào được tải."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Lỗi khi lưu ghi đè."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_REMOVING,
   "Lỗi khi xóa ghi đè."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Ghi đè đã được lưu thành công."
   )
MSG_HASH(
   MSG_OVERRIDES_REMOVED_SUCCESSFULLY,
   "Ghi đè đã được xóa thành công."
   )
MSG_HASH(
   MSG_OVERRIDES_UNLOADED_SUCCESSFULLY,
   "Ghi đè đã được gỡ thành công."
   )
MSG_HASH(
   MSG_OVERRIDES_NOT_SAVED,
   "Không có gì để lưu. Ghi đè không được lưu."
   )
MSG_HASH(
   MSG_OVERRIDES_ACTIVE_NOT_SAVING,
   "Không lưu. Ghi đè đang hoạt động."
   )
MSG_HASH(
   MSG_PAUSED,
   "Tạm dừng."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Đang đọc bản dữ liệu đầu tiên..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Đã dừng ghi do thay đổi kích thước."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Ghi âm vào"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Chuyển hướng tệp Cheat đến"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Chuyển hướng tệp lưu đến"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Chuyển hướng trạng thái lưu đến"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Tệp định nghĩa phím đã được lưu thành công."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Tệp định nghĩa phím đã được xóa thành công."
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "Tất cả tùy chọn định nghĩa phím đã được đặt lại mặc định."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Đĩa đã được lấy ra khỏi khay."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Đang xóa tệp trò chơi tạm thời"
   )
MSG_HASH(
   MSG_RESET,
   "Đặt lại"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Khởi động lại ghi hình do driver được khởi tạo lại."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Đã phục hồi trạng thái lưu cũ."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Shaders: khôi phục preset shader mặc định về"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Khôi phục thư mục file lưu về"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Khôi phục thư mục trạng thái lưu về"
   )
MSG_HASH(
   MSG_REWINDING,
   "Tua lùi."
   )
MSG_HASH(
   MSG_REWIND_BUFFER_CAPACITY_INSUFFICIENT,
   "Dung lượng bộ đệm không đủ."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "Không thể tua lùi vì core này không hỗ trợ trạng thái lưu tuần tự."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Khởi tạo bộ đệm tua lùi với kích thước"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Không thể khởi tạo bộ đệm tua lùi. Tua lùi sẽ bị tắt."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "Core sử dụng audio đa luồng. Không thể dùng tua lùi."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Đã đạt cuối bộ đệm tua lùi."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Đã lưu cấu hình vào"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Đã lưu trạng thái vào khe: %d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Đã lưu trạng thái vào khe: Tự động."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Đã lưu thành công vào"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Đang lưu loại RAM"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Đang lưu trạng thái"
   )
MSG_HASH(
   MSG_SCANNING,
   "Đang quét"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Quét thư mục hoàn tất."
   )
MSG_HASH(
   MSG_SCANNING_NO_DATABASE,
   "Quét không thành công, không tìm thấy cơ sở dữ liệu."
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Đang gửi lệnh"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Một số bản vá được định nghĩa rõ ràng, bỏ qua tất cả..."
   )
MSG_HASH(
   MSG_SHADER,
   "Bộ lọc Shader"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Đã lưu preset shader thành công."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Chuyển động chậm."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Nhanh về phía trước."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Tua lùi chậm."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Bỏ qua việc nạp SRAM."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM sẽ không được lưu."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Chặn ghi đè SRAM"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Bắt đầu phát lại movie."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Bắt đầu ghi movie vào"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Kích thước trạng thái"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Ô trạng thái"
   )
MSG_HASH(
   MSG_REPLAY_SLOT,
   "Ô phát lại"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Chụp ảnh màn hình."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Ảnh màn hình đã được lưu"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Thành tựu đạt được"
   )
MSG_HASH(
   MSG_RARE_ACHIEVEMENT_UNLOCKED,
   "Thành tựu hiếm đạt được"
   )
MSG_HASH(
   MSG_LEADERBOARD_STARTED,
   "Bắt đầu thử thách trên bảng xếp hạng"
   )
MSG_HASH(
   MSG_LEADERBOARD_FAILED,
   "Thử ghi nhận bảng xếp hạng thất bại"
   )
MSG_HASH(
   MSG_LEADERBOARD_SUBMISSION,
   "Đã gửi %s cho %s" /* Submitted [value] for [leaderboard name] */
   )
MSG_HASH(
   MSG_LEADERBOARD_RANK,
   "Hạng: %d" /* Rank: [leaderboard rank] */
   )
MSG_HASH(
   MSG_LEADERBOARD_BEST,
   "Tốt nhất: %s" /* Best: [value] */
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Đổi loại hình thu nhỏ"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Hình thu nhỏ toàn màn hình"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Bật/tắt siêu dữ liệu"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Không có hình thu nhỏ"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_DOWNLOAD_POSSIBLE,
   "Tất cả hình thu nhỏ có thể tải cho mục này đã được thử."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Nhấn lại để thoát..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_CLOSE_CONTENT,
   "Nhấn lại để đóng trò chơi..."
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_RESET,
   "Nhấn lại để đặt lại..."
   )
MSG_HASH(
   MSG_TO,
   "đến"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Hoàn tác trạng thái tải."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Đang hoàn tác trạng thái lưu"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Không xác định"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Đã bỏ tạm dừng."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Lệnh \"%s\" không nhận dạng được.\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Sử dụng tên core cho cấu hình mới."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Sử dụng core libretro giả lập. Bỏ qua ghi dữ liệu."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Kết nối thiết bị từ một cổng hợp lệ."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Đang ngắt kết nối thiết bị từ cổng"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Đang khởi động lại..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Đang tắt máy..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Phiên bản của libretro API"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Tính toán kích thước khung nhìn không thành công! Sẽ tiếp tục sử dụng dữ liệu thô. Có thể cách này sẽ không hoạt động chính xác..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Không thể nhả khay đĩa ảo."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Không thể đóng khay đĩa ảo."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Đang tự đông tải savestate từ"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "Tự động tải trạng thái lưu từ \"%s\" thất bại."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "Tự động tải trạng thái lưu từ \"%s\" thành công."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "được cấu hình ở cổng"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s được cấu hình ở cổng %u"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "ngắt kết nối khỏi cổng"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s ngắt kết nối khỏi cổng %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "chưa cấu hình"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) chưa cấu hình"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "chưa cấu hình, sử dụng dự phòng"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) chưa cấu hình, sử dụng dự phòng"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Quét Bluetooth hoàn tất."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "Ghép đôi đã bị xóa. Khởi động lại RetroArch để kết nối/ghép đôi lại."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Quét Wi-Fi hoàn tất."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Quét các thiết bị Bluetooth..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Quét các mạng không dây..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Bật Wi-Fi..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Tắt Wi-Fi..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Ngắt kết nối Wi-Fi..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Quét máy chủ Trò chơi trực tuyến..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Chuẩn bị quét trò chơi..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Nhập mật khẩu"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Mật khẩu đúng."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Mật khẩu sai."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Nhập mật khẩu"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Mật khẩu đúng."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Mật khẩu sai."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "Đã tải ghi đè cấu hình."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Đã tải file định nghĩa phím trò chơi."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Đã tải tệp gán lại phím thư mục trò chơi."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Đã tải tệp gán lại phím core."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "Tùy chọn gán lại phím đã được lưu tại:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "Không thể lưu tùy chọn gán lại phím tại:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Run-Ahead đã bật. Số khung hình trễ đã loại bỏ: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Run-Ahead đã bật với Phiên bản Phụ. Số khung hình trễ đã loại bỏ: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Run-Ahead đã tắt."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Run-Ahead đã bị tắt vì core này không hỗ trợ Lưu trạng thái."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "Run-Ahead không khả dụng vì core này không hỗ trợ Lưu trạng thái xác định."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Không thể lưu trạng thái. Run-Ahead đã bị tắt."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Không thể tải trạng thái. Run-Ahead đã bị tắt."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Không thể tạo phiên bản thứ hai. Run-Ahead bây giờ sẽ chỉ dùng một phiên bản."
   )
MSG_HASH(
   MSG_PREEMPT_ENABLED,
   "Khung hình ưu tiên đã được bật. Số khung hình trễ đã bị loại bỏ: %u."
   )
MSG_HASH(
   MSG_PREEMPT_DISABLED,
   "Khung hình ưu tiên bị tắt."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Khung hình ưu tiên đã bị tắt vì core này không hỗ trợ lưu trạng thái."
   )
MSG_HASH(
   MSG_PREEMPT_CORE_DOES_NOT_SUPPORT_PREEMPT,
   "Khung dự đoán không khả dụng vì core này không hỗ trợ trạng thái lưu có xác định."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_ALLOCATE,
   "Không thể cấp phát bộ nhớ cho Khung dự đoán."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_SAVE_STATE,
   "Lưu trạng thái thất bại. Khung dự đoán đã bị vô hiệu."
   )
MSG_HASH(
   MSG_PREEMPT_FAILED_TO_LOAD_STATE,
   "Tải trạng thái thất bại. Khung dự đoán đã bị vô hiệu."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Quét file đã hoàn tất."
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Bắt đầu tìm cheat thành công."
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Bắt đầu tìm cheat thất bại."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Tìm kiếm chưa được khởi tạo/bắt đầu."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Số lượng kết quả mới = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "Đã thêm %u kết quả."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Không thể thêm kết quả."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Đã tạo mã từ kết quả."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Không thể tạo mã."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Đã xóa kết quả."
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Không đủ chỗ. Số cheat tối đa đồng thời là 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Cheat mới được thêm vào đầu danh sách."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Cheat mới được thêm vào cuối danh sách."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Tất cả Cheat đã bị xóa."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Cheat mới được thêm trước Cheat này."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Cheat mới được thêm sau Cheat này."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Cheat đã được sao chép trước Cheat này."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Cheat đã được sao chép sau Cheat này."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Cheat đã bị xóa."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Không thể đặt đĩa."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Không thể đặt đĩa đã dùng lần trước."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "Không thể kết nối với khách."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Không thể kết nối với máy chủ."
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "Máy chủ Trò chơi trực tuyến đã đầy."
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "Bạn bị cấm truy cập máy chủ này."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Không thể nhận header từ máy chủ."
   )
MSG_HASH(
   MSG_CHEEVOS_LOGGED_IN_AS_USER,
   "RetroAchievements: Đã đăng nhập dưới tên \"%s\"."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Bạn phải tạm dừng hoặc tắt Chế độ Hardcore Achievements để tải trạng thái lưu."
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_SAVEFILE_PREVENTED_BY_HARDCORE_MODE,
   "Bạn phải tạm dừng hoặc tắt Chế độ Thử thách Thành tích để tải lưu srm."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Một trạng thái lưu đã được tải. Chế độ Thử thách Thành tích bị tắt cho phiên hiện tại."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Một cheat đã được kích hoạt. Chế độ Thử thách Thành tích bị tắt cho phiên hiện tại."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_CHANGED_BY_HOST,
   "Chế độ Thử thách Thành tích đã bị thay đổi bởi máy chủ."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_REQUIRES_NEWER_HOST,
   "Máy chủ Trò chơi trực tuyến cần được cập nhật. Chế độ Thử thách Thành tích bị tắt cho phiên hiện tại."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "Đã làm chủ %s"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "Hoàn thành %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Chế độ Thành Tích Khó đã bật, Lưu trạng thái và Quay lại bị tắt."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_HAS_NO_ACHIEVEMENTS,
   "Trò chơi này không có achievements."
   )
MSG_HASH(
   MSG_CHEEVOS_ALL_ACHIEVEMENTS_ACTIVATED,
   "Tất cả %d thành tích đã được kích hoạt cho phiên này"
)
MSG_HASH(
   MSG_CHEEVOS_UNOFFICIAL_ACHIEVEMENTS_ACTIVATED,
   "Đã kích hoạt %d thành tích không chính thức"
)
MSG_HASH(
   MSG_CHEEVOS_NUMBER_ACHIEVEMENTS_UNLOCKED,
   "Bạn đã mở %d trên tổng %d achievements"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_COUNT,
   "%d không được hỗ trợ"
)
MSG_HASH(
   MSG_CHEEVOS_UNSUPPORTED_WARNING,
   "Phát hiện thành tích không hỗ trợ. Vui lòng thử core khác hoặc cập nhật RetroArch."
)
MSG_HASH(
   MSG_CHEEVOS_RICH_PRESENCE_SPECTATING,
   "Đang xem %s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_MANUAL_FRAME_DELAY,
   "Thử thách tạm dừng. Không cho phép thiết lập độ trễ khung hình video thủ công."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_VSYNC_SWAP_INTERVAL,
   "Thử thách tạm dừng. Không cho phép vsync swap interval lớn hơn 1."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_BLACK_FRAME_INSERTION,
   "Thử thách tạm dừng. Không cho phép chèn khung hình đen."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SETTING_NOT_ALLOWED,
   "Thử thách tạm dừng. Thiết lập không hợp lệ: %s=%s"
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_PAUSED_SYSTEM_NOT_FOR_CORE,
   "Chế độ Thử thách đã tạm dừng. Bạn không thể nhận thành tựu Chế độ Thử thách cho %s khi dùng %s"
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_NOT_IDENTIFIED,
   "Thành tích: Không thể xác định trò chơi."
   )
MSG_HASH(
   MSG_CHEEVOS_GAME_LOAD_FAILED,
   "Tải trò chơi RetroAchievements thất bại: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_CHANGE_MEDIA_FAILED,
   "Thành tích thay đổi phương tiện thất bại: %s"
   )
MSG_HASH(
   MSG_CHEEVOS_LOGIN_TOKEN_EXPIRED,
   "Phiên đăng nhập Thành tích Retro đã hết hạn. Vui lòng nhập lại mật khẩu và tải lại trò chơi."
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "Thấp nhất"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Thấp hơn"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "Bình thường"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Cao hơn"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Cao nhất"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Cảnh báo: Thiếu tài nguyên, hãy sử dụng Online Updater nếu có."
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Cảnh báo: Thiếu phông chữ cho ngôn ngữ đã chọn, sử dụng Trình Cập Nhật Trực Tuyến nếu có."
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Cảnh báo: Ngôn ngữ không được hỗ trợ - sử dụng tiếng Anh."
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Đang sao chép đĩa..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Ổ đĩa %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Vui lòng tải một core trước."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Không đọc được từ ổ đĩa. Quá trình sao chép bị hủy."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Không ghi được vào đĩa. Quá trình sao chép bị hủy."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Không có đĩa nào được đưa vào ổ."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Đã xóa preset shader thành công."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Lỗi khi xóa preset shader."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "File DAT arcade chọn không hợp lệ."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "File DAT arcade chọn quá lớn (không đủ bộ nhớ trống)."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Không tải được file DAT arcade (định dạng không hợp lệ?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Cấu hình quét thủ công không hợp lệ."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Không phát hiện trò chơi hợp lệ."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Quét trò chơi "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Kiểm tra các mục hiện tại: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Đang quét: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Dọn các mục M3U: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Quét hoàn tất: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Quét core: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Sao lưu core đã cài đặt trước đó tồn tại: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Đang sao lưu core: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Xóa các bản sao lưu lỗi thời: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Sao lưu core hoàn tất: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "Bản sao lưu core đã chọn đã được cài đặt: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Khôi phục core: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Khôi phục core hoàn tất: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "File core đã được cài đặt "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Đang cài đặt core: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Cài đặt core hoàn tất: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "File core được chọn không hợp lệ: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Sao lưu core thất bại: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Khôi phục core thất bại: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Cài đặt core thất bại: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Cài đặt core bị vô hiệu hóa - core bị khóa: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "Cài đặt core bị vô hiệu hóa - core bị khóa: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Khóa core thất bại: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Mở khóa core thất bại: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "Xóa core khỏi danh sách 'Contentless Cores' thất bại: "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "Thêm core vào danh sách 'Contentless Cores' thất bại: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "Xóa core bị vô hiệu hóa - core bị khóa: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Chế độ video không được hỗ trợ"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "Không thể ghi vào thư mục thông tin core - bộ nhớ cache thông tin core sẽ bị vô hiệu hóa"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "Tìm thấy trạng thái entry trong"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Đang tải trạng thái mục từ"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "Không thể vào Chế độ Game"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "Không thể vào Chế độ Game – hãy đảm bảo daemon GameMode đã được cài đặt và đang chạy"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "Đồng bộ với tốc độ khung hình trò chơi chính xác đã bật."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "Đồng bộ với tốc độ khung hình trò chơi chính xác đã tắt."
   )
MSG_HASH(
   MSG_VIDEO_REFRESH_RATE_CHANGED,
   "Tốc độ làm mới video đã thay đổi thành %s Hz."
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Cập nhật Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Tên giao diện"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Phiên bản Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Khởi động lại"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Tách Joy-Con"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Ghi đè tỉ lệ Widget đồ họa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Áp dụng ghi đè tỉ lệ thủ công khi vẽ các widget hiển thị. Chỉ áp dụng khi 'Tự động tỉ lệ Widget đồ họa' bị tắt. Có thể dùng để tăng hoặc giảm kích thước thông báo, chỉ báo và điều khiển trang trí mà không ảnh hưởng đến menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Độ phân giải màn hình"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Độ phân giải màn hình: Mặc định"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Độ phân giải màn hình: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Độ phân giải màn hình: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Áp dụng: Mặc định"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Áp dụng: %dx%d\nNHẤN Start để đặt lại"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Áp dụng: %dx%d - %s\nNHẤN Start để đặt lại"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Đặt lại về: Mặc định"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Đặt lại về: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Đặt lại về: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Chọn chế độ hiển thị (cần khởi động lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Tắt Máy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Bật truy cập file bên ngoài"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Mở cài đặt quyền truy cập file trên Windows"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Mở cài đặt quyền trên Windows để bật tính năng broadFileSystemAccess."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Mở..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Mở thư mục khác bằng trình chọn file của hệ thống"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Bộ lọc nhấp nháy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Bộ lọc mềm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Quét các thiết bị Bluetooth và kết nối."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Quét các mạng không dây và kết nối."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Bật Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Kết nối mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Kết nối mạng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Ngắt kết nối"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Khử nhấp nháy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "Đặt chiều rộng màn hình VI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Chỉnh sửa Overscan (Trên)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Điều chỉnh cắt overscan hiển thị bằng cách giảm kích thước hình ảnh theo số dòng quét nhất định (từ trên cùng màn hình). Có thể gây ra các hiện tượng méo hình khi phóng to."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Chỉnh sửa Overscan (Dưới)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Điều chỉnh cắt overscan hiển thị bằng cách giảm kích thước hình ảnh theo số dòng quét nhất định (từ dưới cùng màn hình). Có thể gây ra các hiện tượng méo hình khi phóng to."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Chế độ Hiệu năng Ổn định"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "Hiệu năng CPU và Năng lượng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "Chính sách"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Chế độ quản lý"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Thủ công"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Cho phép tinh chỉnh thủ công mọi chi tiết của CPU: governor, tần số, v.v. Chỉ khuyến nghị cho người dùng nâng cao."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Hiệu năng (Quản lý)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Chế độ mặc định và khuyến nghị. Hiệu năng tối đa khi chơi, đồng thời tiết kiệm điện khi tạm dừng hoặc duyệt menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Tùy chỉnh Quản lý"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Cho phép chọn governor sử dụng trong menu và khi chơi. Performance, Ondemand hoặc Schedutil được khuyến nghị khi chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Hiệu năng tối đa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Luôn tối đa hiệu năng: tần số cao nhất cho trải nghiệm tốt nhất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Tiết kiệm năng lượng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Sử dụng tần số thấp nhất có sẵn để tiết kiệm pin. Hữu ích cho thiết bị dùng pin nhưng hiệu năng sẽ giảm đáng kể."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Cân bằng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Tự điều chỉnh theo khối lượng công việc hiện tại. Hoạt động tốt với hầu hết thiết bị và trình giả lập, giúp tiết kiệm điện. Trò chơi và core nặng có thể bị giảm hiệu năng trên một số thiết bị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Tần số tối thiểu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Tần số tối đa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Tần số Core tối thiểu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Tần số Core tối đa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "Chính sách quản lý CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Chính sách quản lý Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Chính sách quản lý Menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "Chế độ chơi game"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Có thể cải thiện hiệu năng, giảm độ trễ và khắc phục tình trạng tiếng nứt vỡ. Bạn cần truy cập vào https://github.com/FeralInteractive/gamemode để tính năng này hoạt động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_GAMEMODE_ENABLE,
   "Bật Linux GameMode có thể cải thiện độ trễ, khắc phục tiếng nứt vỡ và tối đa hóa hiệu năng tổng thể bằng cách tự động cấu hình CPU và GPU cho hiệu suất tốt nhất.\nPhần mềm GameMode cần được cài đặt để tính năng này hoạt động. Xem https://github.com/FeralInteractive/gamemode để biết cách cài đặt GameMode."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Sử dụng Chế độ PAL60"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Khởi động lại RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Thoát rồi khởi động lại RetroArch. Cần thiết để kích hoạt một số cài đặt menu (ví dụ khi thay đổi menu driver)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Khung hình bị chặn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Ưu tiên cảm ứng phía trước"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_PREFER_FRONT_TOUCH,
   "Sử dụng cảm ứng phía trước thay vì phía sau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Màn hình cảm ứng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Bản đồ phím cho tay cầm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Loại bản đồ phím cho tay cầm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Bàn phím nhỏ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Thời gian chờ khối nhập"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Số mili-giây chờ để lấy mẫu điều khiển hoàn chỉnh. Dùng khi gặp vấn đề với nhấn nút đồng thời (chỉ Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Hiển thị 'Khởi động lại'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Hiển thị tùy chọn 'Khởi động lại'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Hiển thị 'Tắt máy'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Hiển thị tùy chọn 'Tắt máy'."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Có mật khẩu"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "Internet (Trung gian)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "Internet (Không thể kết nối)"
   )
MSG_HASH(
   MSG_LOCAL,
   "Cục bộ"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Trạng thái bộ nhớ trong: Đọc/Ghi"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Trạng thái bộ nhớ trong: Chỉ đọc"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Độ sáng màn hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Tăng hoặc giảm độ sáng màn hình."
   )
#ifdef HAVE_LIBNX
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "Ép xung CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Ép xung CPU của Switch."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Xác định trạng thái Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Dịch vụ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Quản lý dịch vụ của hệ điều hành."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "Giao thức SAMBA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Chia sẻ thư mục mạng thông qua giao thức SMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "Truy cập từ xa SSH"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Sử dụng SSH để truy cập dòng lệnh từ xa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Điểm truy cập Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Bật hoặc tắt Điểm truy cập Wi-Fi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Múi giờ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Chọn múi giờ để điều chỉnh ngày và giờ theo vị trí của bạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TIMEZONE,
   "Hiển thị danh sách các múi giờ có sẵn. Sau khi chọn múi giờ, ngày và giờ sẽ được điều chỉnh theo múi giờ đã chọn. Giả sử đồng hồ hệ thống/phần cứng được đặt theo UTC."
   )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SWITCH_OPTIONS,
   "Tùy chọn Nintendo Switch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LAKKA_SWITCH_OPTIONS,
   "Quản lý các tùy chọn riêng cho Nintendo Switch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_OC_ENABLE,
   "Ép xung CPU"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_OC_ENABLE,
   "Bật các tần số Ép xung CPU"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CEC_ENABLE,
   "Hỗ trợ CEC"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CEC_ENABLE,
   "Bật trao đổi tín hiệu CEC với TV khi đặt vào dock"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_ERTM_DISABLE,
   "Tắt ERTM Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ERTM_DISABLE,
   "Tắt ERTM Bluetooth để khắc phục việc ghép đôi với một số thiết bị"
   )
#endif
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Tắt Điểm Truy cập Wi-Fi."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Ngắt kết nối Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Kết nối Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Không có SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Điểm Truy cập Wi-Fi đã được bật"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Điểm Truy cập Wi-Fi chưa chạy"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Bắt đầu Điểm Truy cập Wi-Fi với SSID=%s và Mật khẩu=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Không thể tạo file cấu hình Điểm Truy cập Wi-Fi."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "File cấu hình sai - không tìm thấy APNAME hoặc PASSWORD trong %s"
   )
#endif
#ifdef HAVE_LAKKA_SWITCH
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Tỷ lệ Chuột"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Điều chỉnh tỷ lệ x/y để tốc độ súng ánh sáng Wiimote."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Tỷ lệ Cảm ứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "Điều chỉnh tỷ lệ x/y của tọa độ màn hình cảm ứng để phù hợp với tỉ lệ hiển thị của hệ điều hành."
   )
#ifdef UDEV_TOUCH_SUPPORT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_POINTER,
   "Dùng VMouse cảm ứng như con trỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_POINTER,
   "Bật để truyền sự kiện cảm ứng từ màn hình vào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_MOUSE,
   "Dùng VMouse cảm ứng như chuột"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_MOUSE,
   "Bật mô phỏng chuột ảo bằng sự kiện cảm ứng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Chế độ Touchpad cho VMouse cảm ứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TOUCHPAD,
   "Bật cùng chế độ Chuột để sử dụng màn hình cảm ứng như touchpad."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Chế độ Trackball cho VMouse cảm ứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_TRACKBALL,
   "Bật cùng chế độ Chuột để sử dụng màn hình cảm ứng như trackball, thêm quán tính cho con trỏ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_VMOUSE_GESTURE,
   "Cử chỉ VMouse cảm ứng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_VMOUSE_GESTURE,
   "Bật cử chỉ cảm ứng, bao gồm nhấn, kéo-thả, và vuốt ngón."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "Tỷ lệ RGA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA scaling và lọc bicubic. Có thể làm hỏng các widget."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Tỷ lệ theo ngữ cảnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Tỷ lệ phần cứng (nếu có)."
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "Bật Đồng hồ / Bộ nhớ đệm L2 New3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "Bật tốc độ đồng hồ New3DS (804MHz) và bộ nhớ đệm L2."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "Màn hình dưới cùng của 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Bật hiển thị thông tin trạng thái trên màn hình dưới. Tắt để tăng thời lượng pin và cải thiện hiệu năng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "Chế độ hiển thị 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Chọn giữa chế độ hiển thị 3D và 2D. Trong chế độ '3D', các điểm ảnh là hình vuông và có hiệu ứng chiều sâu khi xem Menu Nhanh. Chế độ '2D' cho hiệu năng tốt nhất."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (Hiệu ứng lưới điểm ảnh)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (Độ phân giải cao)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Chạm vào màn hình cảm ứng để đi\ntới menu RetroArch"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "Không tìm thấy tài nguyên"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Không có\nDữ liệu"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "Không có\nẢnh chụp màn hình"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Tiếp tục trò chơi"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Tạo\nĐiểm phục hồi"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Tải\nĐiểm phục hồi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "Thư mục tài nguyên màn hình dưới"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "Thư mục tài nguyên màn hình dưới. Thư mục phải bao gồm \"bottom_menu.png\"."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "Bật phông chữ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "Hiển thị phông chữ menu dưới. Bật để hiển thị mô tả nút trên màn hình dưới. Không áp dụng cho ngày lưu trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Màu phông chữ đỏ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Điều chỉnh màu đỏ của phông chữ màn hình dưới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Màu phông chữ xanh lá"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Điều chỉnh màu xanh lá của phông chữ màn hình dưới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Màu phông chữ xanh dương"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Điều chỉnh màu xanh dương của phông chữ màn hình dưới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Độ mờ phông chữ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "Điều chỉnh độ mờ của phông chữ màn hình dưới."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "Tỷ lệ phông chữ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "Điều chỉnh tỷ lệ phông chữ màn hình dưới."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Quét hoàn tất.<br><br>\nĐể trò chơi được quét chính xác, bạn phải:\n<ul><li>đã tải về Core tương thích</li>\n<li>cập nhật \"Thông tin tệp Core\" qua Trình cập nhật trực tuyến</li>\n<li>cập nhật \"Cơ sở dữ liệu\" qua Trình cập nhật trực tuyến</li>\n<li>khởi động lại RetroArch nếu vừa thực hiện bất kỳ bước nào ở trên</li></ul>\nCuối cùng, trò chơi phải khớp với cơ sở dữ liệu hiện có từ <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">đây</a>. Nếu vẫn không hoạt động, hãy cân nhắc <a href=\"https://www.github.com/libretro/RetroArch/issues\">gửi báo cáo lỗi</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "Chuột cảm ứng đã bật"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "Chuột cảm ứng đã tắt"
   )
MSG_HASH(
   MSG_SDL2_MIC_NEEDS_SDL2_AUDIO,
   "sdl2 microphone yêu cầu driver âm thanh Sdl2"
   )
MSG_HASH(
   MSG_ACCESSIBILITY_STARTUP,
   "Chế độ truy cập RetroArch bật. Menu Chính Tải Core."
   )
MSG_HASH(
   MSG_AI_SERVICE_STOPPED,
   "dừng."
   )
#ifdef HAVE_GAME_AI
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_MENU_OPTION,
   "Ghi đè AI player"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_MENU_OPTION,
   "Ghi đè AI player sublabel"
   )


MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P1,
   "Ghi đè p1"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P1,
   "Ghi đè người chơi 01"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_OVERRIDE_P2,
   "Ghi đè p2"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_OVERRIDE_P2,
   "Ghi đè người chơi 02"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_AI_SHOW_DEBUG,
   "Hiển thị Debug"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_AI_SHOW_DEBUG,
   "Hiển thị Debug"
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_GAME_AI,
   "Hiển thị 'Game AI'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_GAME_AI,
   "Hiển thị tùy chọn 'Game AI'."
   )
#endif

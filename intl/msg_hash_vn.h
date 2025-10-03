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
   "Trình đơn chính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Thiết lập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Ưa thích"
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
   "Kích hoạt Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Mở rộng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Lõi không nội dung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Tạo nội dung"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Trình đơn lẹ"
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
   "Chọn nhân nào sẽ dùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST_UNLOAD,
   "Gỡ Lõi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST_UNLOAD,
   "Giải phóng lõi đã nạp khỏi bộ nhớ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_CORE_LIST,
   "Duyệt qua các lõi libretro. Khi trình quản lý tập tin bắt đầu dựa vào đường dẫn thư mục lõi. Nếu nó trống, nó sẽ được bắt đầu ở thư mục gốc.\nNếu thư mục lõi của bạn là một thư mục, menu sẽ sử dụng thư mục đó là thư mục trên cùng. Nếu thư mục lõi của bạn là đường dẫn đầy đủ, nó sẽ bắt đầu ở thư mục nơi tệp nằm ở đó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Tải Content"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Chọn nội dung nào sẽ bắt đầu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LOAD_CONTENT_LIST,
   "Để duyệt nội dung, bạn cần một 'Core' để sử dụng và một tệp.\nĐể kiểm soát vị trí mà menu bắt đầu duyệt tìm tệp, hãy đặt 'Thư mục Duyệt Tệp'. Nếu không đặt, nó sẽ bắt đầu từ thư mục gốc.\nTrình duyệt sẽ lọc các tệp dựa trên phần mở rộng của core cuối cùng đã chọn trong 'Tải Core', và sẽ sử dụng core đó khi nội dung được tải."
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
   "Dump đĩa"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Dump đĩa vật lý ra bộ nhớ trong. Điều này sẽ tiến hành lưu là dạng tệp ảnh."
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
   "Playlists Danh mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Đã quét nội dung khớp với Csdl và hiển thị tại đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Tạo nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Tạo và cập nhật danh sách bằng cách quét nội dung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Hiện thị Menu Desktop"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Mở Desktop Menu cổ điển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Tắt chế độ Ki ốt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Hiện tất cả cấu hình cài đặt liên quan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Cập nhật trực tuyến"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Tải/cập nhật tiện ích và thành phần của RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Kích hoạt Netplay"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Tham gia hoặc làm máy chủ cho netplay."
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
   "Tệp cấu hình"
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
   "Khởi động RetroPad từ xa"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Yêu thích"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Mục Downloads"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Duyệt file"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Tải Archive With Core"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Ưa thích"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Nội dung được thêm vào mục 'Ưa thích' sẽ xuất hiện ở đây."
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
   "Phim đã được phát trước đó sẽ xuất hiện ở đây."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Mở rộng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Duyệt tất cả nội dung khớp với kho dữ liệu thông qua giao diện tìm kiếm theo thể loại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Lõi không nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Các lõi đã cài đặt có thể hoạt động mà không cần tải nội dung sẽ xuất hiện ở đây."
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
   "Chuyển các lõi sang phiên bản Cửa hàng Play"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Thay thế tất cả các lõi cũ và được cài đặt thủ công bằng các phiên bản mới nhất từ ​​Cửa hàng Play, nếu có."
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
   "Tải về nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Tải xuống nội dung miễn phí cho lõi đã chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Trình tải xuống lõi hệ thống"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Tải xuống các tệp hệ thống phụ trợ cần thiết để hoạt động cốt lõi chính xác / tối ưu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Cập nhật Core Info Files"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Cập nhật Assets"
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
   "Cập nhật Databases"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Cập nhật Overlays"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Cập nhật GLSL Shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Cập nhật Cg Shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Cập nhật Slang Shaders"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Core Thông tin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Xem thông tin liên quan đến ứng dụng/core."
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
   "Xem (các) giao diện mạng và các địa chỉ IP liên quan."
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
   "Xem dữ liệu."
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
   "Phiên bản của lõi"
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
   "Có hỗ trợ lưu màn chơi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Cơ bản (Lưu/Tải lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Nối tiếp (Lưu / Tải, Tua lại)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Xác định (Lưu / Tải, Tua lại, Tua tới, Chơi qua mạng)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Phần vững"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_IN_CONTENT_DIRECTORY,
   "Ghi chú: Tuỳ chọn 'Tệp Hệ Thống nằm trong Thư Mục Nội Dung' hiện đang được bật."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE_PATH,
   "- Đang tìm trong: %s"
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
   "Khoá Lõi Đã Cài Đặt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Ngăn chặn sửa lõi đang cài đặt. Có thể được dùng để né các bản cập nhật không mong muốn, nhất là khi ROM (ví dụ: Arcade ROM) cần đúng phiên bản lõi cụ thể, hoặc khi lõi thay đổi định dạng trạng thái lưu của nó."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "Loại lõi này khỏi menu 'Lõi không nội dung'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Ngăn không cho lõi này xuất hiện trong tab/menu 'Lõi không nội dung'. Chỉ áp dụng khi chế độ hiển thị đang đặt ở 'Tùy chỉnh'."
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
   "Tạo một bản sao lưu của lõi hiện được cài đặt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Khôi phục bản sao lưu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Cài đặt phiên bản trước của lõi từ danh sách các bản sao lưu."
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
   ""
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
   "DPI hiển thị"
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
   "Hỗ trợ Netplay (Peer-to-Peer)"
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
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Điều khiển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Phong cách nghệ thuật"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Lối Chơi"
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
   ""
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
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Bật/tắt trình đơn"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Thoát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Bật/tắt bàn phím"
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
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Video Driver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Điều chỉnh thiết lập cho video ra."
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
   "Input Driver"
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
   "Thay đổi độ trễ của hình ảnh, âm thanh và dữ liệu đầu vào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Core Danh mục"
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
   "Lưu trữ"
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
   "Tên truy nhập"
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
   "Đăng nhập"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Thay đổi cài đặt đăng nhập."
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
   "Tệp remap nút điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CHEAT,
   "Tệp mã gian lận."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_OVERLAY,
   "Tệp che phủ (Overlay)."
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
   "Lõi Libretro. Chọn để gán lõi này cho trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FILE_BROWSER_CORE,
   "Lõi Libretro. Chọn để RetroArch tải lõi này."
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
   "Ghi âm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Thay đổi cài đặt ghi âm."
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
   "Playlists Danh mục"
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
   "Input Driver"
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
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Video Driver"
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
   "Trình điều khiển OpenGL 2.x. Trình điều khiển này cho phép sử dụng lõi libretro GL bên cạnh lõi render phần mềm. Phiên bản tối thiểu yêu cầu: OpenGL 2.0 hoặc OpenGLES 2.0. Hỗ trợ định dạng shader GLSL. Nếu có thể, hãy sử dụng trình điều khiển glcore."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_GL_CORE,
   "Trình điều khiển OpenGL 3.x. Trình điều khiển này cho phép sử dụng lõi libretro GL bên cạnh lõi render phần mềm. Phiên bản tối thiểu yêu cầu: OpenGL 3.2 hoặc OpenGLES 3.0 trở lên. Hỗ trợ định dạng shader Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_VULKAN,
   "Trình điều khiển Vulkan. Trình điều khiển này cho phép sử dụng lõi Vulkan libretro bên cạnh lõi render phần mềm. Phiên bản tối thiểu yêu cầu: Vulkan 1.0. Hỗ trợ trình đổ bóng HDR và ​​Slang."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL1,
   "Trình điều khiển được kết xuất bằng phần mềm SDL 1.2. Hiệu suất được đánh giá là chưa tối ưu. Chỉ nên sử dụng nó như một giải pháp cuối cùng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_DRIVER_SDL2,
   "Trình điều khiển kết xuất phần mềm SDL 2. Hiệu suất của các triển khai lõi libretro kết xuất phần mềm phụ thuộc vào triển khai SDL trên nền tảng của bạn."
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
   "Trình điều khiển video Exynos cấp thấp sử dụng khối G2D trong SoC Samsung Exynos cho các hoạt động xử lý nhanh. Hiệu suất cho các lõi được kết xuất bằng phần mềm sẽ đạt mức tối ưu."
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
   "Trình điều khiển âm thanh"
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
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Trình Camera"
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
   "Wi-Fi Driver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Trình điều khiển Wi-Fi để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Location Driver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Trình điều khiển vị trí để sử dụng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Menu Driver"
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
   "Record Driver"
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
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Xuất tín hiệu gốc, độ phân giải thấp để sử dụng với màn hình CRT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Đầu ra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Điều chỉnh thiết lập cho video ra."
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
   "Tỷ lệ hiển thị"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Thay đổi cài đặt tỷ lệ video."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Thay đổi cài đặt HDR của video."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Âm thanh Sync"
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
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_SUBFRAMES,
   "CẢNH BÁO: Hiện tượng nhấp nháy nhanh có thể gây ra hiện tượng lưu ảnh trên một số màn hình. Tự chịu rủi ro khi sử dụng // Mô phỏng một đường quét cơ bản trên nhiều khung hình phụ bằng cách chia màn hình theo chiều dọc và hiển thị từng phần của màn hình theo số lượng khung hình phụ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_SHADER_SUBFRAMES,
   "Chèn thêm khung đổ bóng vào giữa các khung hình để tạo hiệu ứng đổ bóng có thể chạy nhanh hơn tốc độ nội dung. Chỉ sử dụng tùy chọn được chỉ định cho tốc độ làm mới màn hình hiện tại của bạn. Không sử dụng ở tốc độ làm mới không phải bội số của 60Hz, chẳng hạn như 144Hz, 165Hz, v.v. Không kết hợp với Swap Interval > 1, BFI, Frame Delay hoặc Sync to Exact Content Framerate. Có thể b[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Kích hoạt GPU Screenshot"
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
   "Chỉ định phương pháp nội suy hình ảnh khi chia tỷ lệ nội dung thông qua IPU nội bộ. Khuyến nghị sử dụng 'Bo tròn cạnh' hoặc 'Làm mịn' khi sử dụng bộ lọc video chạy bằng CPU. Tùy chọn này không ảnh hưởng đến hiệu suất."
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
   "Tự động hoãn shader"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Hoãn việc tự động tải shader (tính bằng ms). Có thể khắc phục lỗi đồ họa khi sử dụng phần mềm 'chụp màn hình'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Video Filter Danh mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Áp dụng bộ lọc video chạy bằng CPU. Có thể phải trả giá bằng hiệu năng cao. Một số bộ lọc video có thể chỉ hoạt động với lõi sử dụng màu 32-bit hoặc 16-bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER,
   "Áp dụng bộ lọc video chạy bằng CPU. Có thể phải trả giá bằng hiệu năng cao. Một số bộ lọc video có thể chỉ hoạt động với lõi sử dụng màu 32-bit hoặc 16-bit. Có thể chọn thư viện bộ lọc video được liên kết động."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VIDEO_FILTER_BUILTIN,
   "Áp dụng bộ lọc video chạy bằng CPU. Có thể phải trả giá bằng hiệu năng cao. Một số bộ lọc video có thể chỉ hoạt động với lõi sử dụng màu 32-bit hoặc 16-bit. Có thể chọn thư viện bộ lọc video tích hợp."
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
   MENU_ENUM_LABEL_VALUE_VIDEO_USE_METAL_ARG_BUFFERS,
   "Sử dụng Bộ đệm đối số kim loại (Yêu cầu khởi động lại)"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_USE_METAL_ARG_BUFFERS,
   "Hãy thử cải thiện hiệu suất bằng cách sử dụng bộ đệm tham số Metal. Một số lõi có thể yêu cầu điều này. Điều này có thể làm hỏng một số shader, đặc biệt là trên phần cứng hoặc phiên bản hệ điều hành cũ."
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "Chuyển độ phân giải CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Chỉ dành cho màn hình CRT. Cố gắng sử dụng độ phân giải lõi/trò chơi và tốc độ làm mới chính xác."
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
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Sử dụng Menu Độ phân giải cao"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Chuyển sang modeline có độ phân giải cao để sử dụng với menu có độ phân giải cao khi không tải nội dung nào."
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
   "Rotation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Forces a certain rotation of the screen. The rotation is added to rotations which the core sets."
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
   "Tần số quét chính xác của màn hình (Hz). Thông số này được sử dụng để tính toán tần số đầu vào âm thanh theo công thức:\naudio_input_rate = tần số đầu vào của trò chơi * tần số quét màn hình / tần số quét trò chơi\nNếu lõi không báo cáo bất kỳ giá trị nào, giá trị mặc định của NTSC sẽ được sử dụng để đảm bảo tính tương thích.\nGiá trị này nên gần 60Hz để tránh thay đổi ca[...]"
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
   "Tự động chuyển đổi tốc độ làm mới màn hình dựa trên nội dung hiện tại."
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
   "Đặt tốc độ làm mới theo chiều dọc của màn hình. '50 Hz' sẽ cho phép video mượt mà khi chạy nội dung PAL."
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
   "Tỷ lệ cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Đặt kích thước cửa sổ theo bội số đã chỉ định của kích thước khung nhìn lõi."
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
   "Hiển thị toàn bộ nội dung trong một cửa sổ có kích thước cố định được chỉ định bởi 'Chiều rộng cửa sổ' và 'Chiều cao cửa sổ', đồng thời lưu kích thước và vị trí cửa sổ hiện tại khi đóng RetroArch. Khi tắt, kích thước cửa sổ sẽ được thiết lập động dựa trên 'Tỷ lệ cửa sổ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Sử dụng Kích thước cửa sổ tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Hiển thị toàn bộ nội dung trong một cửa sổ có kích thước cố định được chỉ định bởi 'Chiều rộng cửa sổ' và 'Chiều cao cửa sổ'. Khi tắt, kích thước cửa sổ sẽ được thiết lập động dựa trên 'Tỷ lệ cửa sổ'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Chiều rộng cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Set the custom width size for the display window. Leaving it at 0 will attempt to scale the window as large as possible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Chiều cao cửa sổ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Set the custom height size for the display window. Leaving it at 0 will attempt to scale the window as large as possible."
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
   "Chỉ chia tỷ lệ video theo số nguyên. Kích thước cơ sở phụ thuộc vào hình học được báo cáo trên lõi và tỷ lệ khung hình."
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
   "Làm tròn xuống hoặc lên đến số nguyên tiếp theo. 'Thông minh' sẽ giảm xuống mức dưới tỷ lệ khi hình ảnh bị cắt quá nhiều."
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
   "Cấu hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_SQUARE_PIXEL,
   "Pixel vuông (hiển thị %u:%u)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Lõi được cung cấp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Đầy đủ"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Giữ nguyên tỷ lệ khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Duy trì tỷ lệ khung hình 1:1 khi chia tỷ lệ nội dung thông qua IPU nội bộ. Nếu tắt, hình ảnh sẽ được kéo giãn để lấp đầy toàn bộ màn hình."
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
   "Vị trí theo chiều ngang của nội dung khi khung nhìn rộng hơn chiều rộng của nội dung. 0,0 là cực trái, 0,5 là ở giữa, 1,0 là cực phải."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_Y,
   "Vị trí theo chiều dọc của nội dung khi khung nhìn cao hơn chiều cao của nội dung. 0,0 là trên cùng, 0,5 là ở giữa, 1,0 là dưới cùng."
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
   "Vị trí theo chiều ngang của nội dung khi khung nhìn rộng hơn chiều rộng nội dung. 0,0 là cực trái, 0,5 là ở giữa, 1,0 là cực phải. (Hướng dọc)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_BIAS_PORTRAIT_Y,
   "Vị trí theo chiều dọc của nội dung khi khung nhìn cao hơn chiều cao của nội dung. 0,0 là trên cùng, 0,5 là ở giữa, 1,0 là dưới cùng. (Hướng dọc)"
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
   "Sử dụng khoảng thời gian hoán đổi tùy chỉnh cho VSync. Giảm hiệu quả tốc độ làm mới màn hình theo hệ số đã chỉ định. Chế độ 'Tự động' đặt hệ số dựa trên tốc độ khung hình do lõi báo cáo, cải thiện tốc độ khung hình khi chạy, ví dụ: nội dung 30 khung hình/giây trên màn hình 60 Hz hoặc nội dung 60 khung hình/giây trên màn hình 120 Hz."
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
   "Thiết lập số mili giây để ngủ trước khi chạy lõi sau khi trình chiếu video. Giảm độ trễ nhưng sẽ tăng nguy cơ giật hình.\nGiá trị 20 trở lên được coi là phần trăm thời gian khung hình."
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
   "Đồng bộ với tốc độ khung hình nội dung chính xác (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Không sai lệch so với thời gian yêu cầu của lõi. Sử dụng cho màn hình Tốc độ làm mới thay đổi (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_VRR_RUNLOOP_ENABLE,
   "Đồng bộ với Tốc độ Khung hình Nội dung Chính xác. Tùy chọn này tương đương với việc ép tốc độ x1 trong khi vẫn cho phép tua nhanh. Không lệch so với tốc độ làm mới yêu cầu của lõi, không có Kiểm soát Tốc độ Động."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Đầu ra"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Điều chỉnh thiết lập cho âm thanh ra."
   )
#ifdef HAVE_MICROPHONE
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_SETTINGS,
   "Micrô"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_SETTINGS,
   "Thay đổi cài đặt đầu vào âm thanh."
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
   "Âm thanh Sync"
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
   "Enable menu audio"
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
   "Âm thanh Mute"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Tắt tiếng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Audio Mixer Mute"
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
   "Mức âm lượng (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Âm lượng âm thanh (tính bằng dB). 0 dB là âm lượng bình thường và không áp dụng bất kỳ mức khuếch đại nào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_VOLUME,
   "Âm lượng, được biểu thị bằng dB. 0 dB là âm lượng bình thường, không áp dụng độ khuếch đại. Độ khuếch đại có thể được điều khiển trong thời gian chạy bằng cách tăng âm lượng đầu vào/giảm âm lượng đầu vào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Audio Mixer Volume Level (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Âm lượng tổng thể của bộ trộn âm thanh (tính bằng dB). 0 dB là âm lượng bình thường và không áp dụng mức khuếch đại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Âm thanh DSP Plugin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Plugin DSP âm thanh xử lý âm thanh trước khi gửi đến trình điều khiển."
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
   "The audio buffer length when using the WASAPI driver in shared mode."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Kích hoạt âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Bật đầu ra âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Thiết bị âm thanh"
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
   "Âm thanh Latency (ms)"
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
   "Cho phép đầu vào âm thanh trong các lõi được hỗ trợ. Không có chi phí phát sinh nếu lõi không sử dụng micrô."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_DEVICE,
   "Thiết bị âm thanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_DEVICE,
   "Ghi đè thiết bị đầu vào mặc định mà trình điều khiển micrô sử dụng. Điều này phụ thuộc vào trình điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MICROPHONE_DEVICE,
   "Ghi đè thiết bị đầu vào mặc định mà trình điều khiển micrô sử dụng. Điều này phụ thuộc vào trình điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_RESAMPLER_QUALITY,
   "Audio Resampler Quality"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_RESAMPLER_QUALITY,
   "Giảm giá trị này để ưu tiên hiệu suất/độ trễ thấp hơn chất lượng âm thanh, tăng giá trị này để có chất lượng âm thanh tốt hơn nhưng phải đánh đổi bằng hiệu suất/độ trễ thấp hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_INPUT_RATE,
   "Tốc độ đầu vào mặc định (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_INPUT_RATE,
   "Tốc độ mẫu đầu vào âm thanh, được sử dụng nếu lõi không yêu cầu số lượng cụ thể."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_LATENCY,
   "Độ trễ đầu vào âm thanh (mili giây)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_LATENCY,
   "Độ trễ đầu vào âm thanh mong muốn tính bằng mili giây. Có thể không được đáp ứng nếu trình điều khiển micrô không cung cấp được độ trễ mong muốn."
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
   "Sử dụng đầu vào dấu chấm động cho trình điều khiển WASAPI, nếu thiết bị âm thanh của bạn hỗ trợ."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "Chiều dài bộ đệm chia sẻ WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MICROPHONE_WASAPI_SH_BUFFER_LENGTH,
   "The audio buffer length when using the WASAPI driver in shared mode."
   )
#endif

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Audio Resampler Quality"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Giảm giá trị này để ưu tiên hiệu suất/độ trễ thấp hơn chất lượng âm thanh, tăng giá trị này để có chất lượng âm thanh tốt hơn nhưng phải đánh đổi bằng hiệu suất/độ trễ thấp hơn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Âm thanh Output Rate (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Tốc độ lấy mẫu đầu ra âm thanh."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Âm thanh Sync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Đồng bộ âm thanh. Khuyến nghị."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Âm thanh Maximum Timing Skew"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Mức thay đổi tối đa trong tốc độ đầu vào âm thanh. Việc tăng mức này sẽ dẫn đến những thay đổi rất lớn về thời gian, nhưng sẽ làm âm thanh không chính xác (ví dụ: chạy lõi PAL trên màn hình NTSC)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_MAX_TIMING_SKEW,
   "Độ lệch thời gian âm thanh tối đa.\nXác định mức thay đổi tối đa của tốc độ đầu vào. Bạn có thể muốn tăng giá trị này để cho phép thay đổi thời gian rất lớn, ví dụ như chạy lõi PAL trên màn hình NTSC, với chi phí là độ cao âm thanh không chính xác.\nTốc độ đầu vào được định nghĩa là:\ntốc độ đầu vào * (1,0 +/- (độ lệch thời gian tối đa))"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Âm thanh Rate Control Delta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Giúp làm mịn các điểm không đồng bộ về thời gian khi đồng bộ hóa âm thanh và video. Lưu ý rằng nếu tắt, việc đồng bộ hóa chính xác gần như không thể thực hiện được."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_AUDIO_RATE_CONTROL_DELTA,
   "Đặt giá trị này thành 0 sẽ vô hiệu hóa điều khiển tốc độ. Bất kỳ giá trị nào khác sẽ kiểm soát delta điều khiển tốc độ âm thanh.\nXác định mức độ tốc độ đầu vào có thể được điều chỉnh động. Tốc độ đầu vào được định nghĩa là:\ntốc độ đầu vào * (1,0 +/- (delta điều khiển tốc độ))"
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Input Driver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Chọn thiết bị đầu vào."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_MIDI_INPUT,
   "Thiết lập thiết bị đầu vào (tùy thuộc vào trình điều khiển). Khi được đặt thành \"Tắt\", đầu vào MIDI sẽ bị tắt. Bạn cũng có thể nhập tên thiết bị."
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
   "Thiết lập thiết bị đầu ra (tùy thuộc vào trình điều khiển). Khi được đặt thành \"Tắt\", đầu ra MIDI sẽ bị tắt. Bạn cũng có thể nhập tên thiết bị.\nKhi đầu ra MIDI được bật và lõi và trò chơi/ứng dụng hỗ trợ đầu ra MIDI, một số hoặc tất cả âm thanh (tùy thuộc vào trò chơi/ứng dụng) sẽ được tạo ra bởi thiết bị MIDI. Trong trường hợp trình điều khiển MIDI \"null\", điều nà[...]"
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
   "Luồng trộn #%d: %s"
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
   "Chơi (Tuần tự)"
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
   "Xoá"
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
   "Enable menu audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Enable or disable menu sound."
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
   "Ảnh hưởng đến cách thực hiện thăm dò đầu vào trong RetroArch. Đặt thành \"Sớm\" hoặc \"Muộn\" có thể giảm độ trễ, tùy thuộc vào cấu hình của bạn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_POLL_TYPE_BEHAVIOR,
   "Ảnh hưởng đến cách thực hiện thăm dò đầu vào bên trong RetroArch.\nSớm - Thăm dò đầu vào được thực hiện trước khi khung được xử lý.\nBình thường - Thăm dò đầu vào được thực hiện khi có yêu cầu thăm dò.\nTrễ - Thăm dò đầu vào được thực hiện khi có yêu cầu trạng thái đầu vào đầu tiên trên mỗi khung.\nĐặt thành 'Sớm' hoặc 'Trễ' có thể giảm độ trễ, tùy thuộc vào cấu hình[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Ánh xạ lại các điều khiển cho lõi này"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Ghi đè các ràng buộc đầu vào bằng các ràng buộc được ánh xạ lại được thiết lập cho lõi hiện tại."
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
   "Kích hoạt Autoconfig"
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
   "Chọn Bàn phím vật lýChọn Bàn phím vật lý"
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
   "Đầu vào cảm biến phụ"
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
   "Luôn bật chế độ \"Tập trung vào trò chơi\" khi khởi chạy và tiếp tục nội dung. Khi được đặt thành \"Phát hiện\", tùy chọn sẽ được bật nếu lõi hiện tại triển khai chức năng gọi lại bàn phím giao diện người dùng."
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
   "Tạm dừng nội dung khi bộ điều khiển ngắt kết nối"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Tạm dừng nội dung khi bất kỳ bộ điều khiển nào bị ngắt kết nối. Tiếp tục bằng nút Bắt đầu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Ngưỡng trục nút đầu vào"
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
   "Độ nhạy Analog"
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
   "Cổ điển (Chuyển đổi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Nút đơn (Chuyển đổi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Nút đơn (Nhấn giữ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC,
   "Chế độ cổ điển, thao tác bằng hai nút. Giữ một nút và chạm vào nút Turbo để kích hoạt chuỗi nhấn nhả.\nCó thể chỉ định Gán nút nhấn nhanh trong Cài đặt/Đầu vào/Điều khiển Cổng X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_CLASSIC_TOGGLE,
   "Chế độ chuyển đổi cổ điển, thao tác bằng hai nút. Giữ một nút và chạm vào nút nhấn nhanh để bật gán nút cho nút đó. Để tắt Turbo: giữ nút và nhấn lại nút Turbo.\nCó thể gán nút nhấn nhanh trong Cài đặt/Đầu vào/Điều khiển Cổng X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON,
   "Chuyển đổi chế độ. Nhấn nút Turbo một lần để kích hoạt chuỗi nhấn cho nút mặc định đã chọn, nhấn lại một lần nữa để tắt.\nCó thể chỉ định Gán nút nhấn nhanh trong Cài đặt/Đầu vào/Điều khiển Cổng X."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Chế độ giữ. Chuỗi nhấn nút mặc định đã chọn sẽ hoạt động miễn là nút Turbo được giữ.\nCó thể gán chế độ Turbo Bind trong Cài đặt/Đầu vào/Điều khiển Cổng X.\nĐể mô phỏng chức năng tự động bắn của thời đại máy tính gia đình, hãy đặt Bind và Button thành cùng một nút bắn trên cần điều khiển."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BIND,
   "Gán nút nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BIND,
   "Nút nhấn nhanh gán cho RetroPad. Nếu để trống, sẽ dùng gán mặc định theo cổng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_BUTTON,
   "Nút nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_BUTTON,
   "Nhấn nhanh mục tiêu ở chế độ 'Nút đơn'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ALLOW_DPAD,
   "Cho phép nhấn nhanh trên D-Pad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_ALLOW_DPAD,
   "Nếu bật, các phím điều hướng (D-Pad/hatswitch) cũng hỗ trợ nhấn nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Thay đổi cài đặt nhấn nhanh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Phản hồi xúc giác/Rung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Thay đổi cài đặt phản hồi xúc giác và rung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Điều khiển menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Thay đổi cài đặt điều khiển menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Kích hoạt hotkeys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Thay đổi cài đặt và chỉ định phím tắt, chẳng hạn như bật/tắt menu trong khi chơi trò chơi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RETROPAD_BINDS,
   "Gán nút RetroPad"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RETROPAD_BINDS,
   "Thay đổi cách RetroPad ảo được ánh xạ đến thiết bị đầu vào vật lý. Nếu thiết bị đầu vào được nhận dạng và tự động cấu hình chính xác, người dùng có thể không cần sử dụng menu này.\nLưu ý: đối với các thay đổi đầu vào cụ thể cho lõi, hãy sử dụng menu phụ 'Điều khiển' của Menu Nhanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_RETROPAD_BINDS,
   "Libretro sử dụng một khái niệm trừu tượng về gamepad ảo được gọi là 'RetroPad' để giao tiếp từ giao diện người dùng (như RetroArch) đến lõi và ngược lại. Menu này xác định cách RetroPad ảo được ánh xạ đến các thiết bị đầu vào vật lý và các cổng đầu vào ảo mà các thiết bị này chiếm giữ.\nNếu thiết bị đầu vào vật lý được nhận dạng và tự động cấu hình chính xác, ngư[...]"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Cổng %u Điều khiển"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Thay đổi cách RetroPad ảo được ánh xạ tới thiết bị đầu vào vật lý của bạn cho cổng ảo này."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_REMAPS,
   "Thay đổi ánh xạ đầu vào cụ thể của lõi."
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
   "Tắt nút Tìm kiếm"
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
   "Thêm độ trễ khung hình trước khi đầu vào bình thường bị chặn sau khi nhấn phím 'Bật phím tắt' được chỉ định. Cho phép ghi lại đầu vào bình thường từ phím 'Bật phím tắt' khi nó được ánh xạ sang một hành động khác (ví dụ: 'Chọn' của RetroPad)."
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
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Menu chuyển đổi (Bộ điều khiển kết hợp)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Tổ hợp nút điều khiển để Bật/Tắt menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Menu (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Chuyển đổi màn hình hiện tại giữa menu và nội dung."
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
   "Đóng nội dung hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Đặt lại nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Khởi động lại nội dung hiện tại."
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
   "Cho phép tua nhanh khi giữ phím. Nội dung sẽ chạy ở tốc độ bình thường khi nhả phím."
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
   "Cho phép chuyển động chậm khi giữ. Nội dung chạy ở tốc độ bình thường khi nhả phím."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Tua lùi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Tua lại nội dung hiện tại khi giữ phím. Phải bật 'Hỗ trợ tua lùi'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Tạm dừng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Chuyển đổi nội dung giữa trạng thái tạm dừng và không tạm dừng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Từng khung hình"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Khi tạm dừng, nội dung sẽ tiến thêm một khung hình."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Tắt tiếng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Bật/tắt âm thanh."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Tăng âm"
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
   "Savestate Danh mục"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Lưu trạng thái vào khe hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Lưu trạng thái tiếp theo"
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
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Giảm chỉ mục đĩa hiện đang được chọn. Khay đĩa ảo phải đang mở."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Shaders (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Bật/tắt shader hiện đang được chọn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_HOLD,
   "Shaders (Nhấn giữ)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_HOLD,
   "Giữ shader hiện đang được chọn bật/tắt khi nhấn phím."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Shader tiếp theo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Tải và áp dụng tệp cài đặt trước shader tiếp theo trong thư mục gốc 'Video Shader'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Shader trước đó"
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
   "Chụp ảnh nội dung hiện tại."
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
   MENU_ENUM_LABEL_VALUE_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Điểm kiểm tra Phát lại Trước đó"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PREV_REPLAY_CHECKPOINT_KEY,
   "Tua lại phát lại về điểm kiểm tra trước đó được lưu tự động hoặc thủ công."
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
   "Chế độ Bấm Nhanh (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_TURBO_FIRE_TOGGLE,
   "Chuyển chế độ bấm nhanh sang Bật/Tắt."
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
   "Menu Desktop (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Mở giao diện người dùng desktop WIMP (Windows, Icons, Menus, Pointer) đi kèm."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Đồng bộ theo tốc độ khung hình nội dung chính xác (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Bật/tắt đồng bộ theo tốc độ khung hình nội dung chính xác."
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
   "Chụp ảnh nội dung hiện tại để dịch và/hoặc đọc to bất kỳ văn bản nào trên màn hình. 'Dịch vụ AI' phải được bật và cấu hình."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Netplay Ping (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Bật/tắt bộ đếm ping cho phòng netplay hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Netplay Hosting (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Bật/tắt lưu trữ netplay."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Netplay Play/Spectate Mode (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Chuyển đổi phiên netplay hiện tại giữa chế độ 'play' và 'spectate'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Netplay Player Trò chuyện"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Gửi tin nhắn trò chuyện đến phiên netplay hiện tại."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Netplay Fade Chat (Bật/Tắt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Chuyển đổi giữa hiển thị mờ và tĩnh cho tin nhắn chat netplay."
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
   "Loại Analog sang Digital"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Sử dụng cần analog đã chỉ định cho đầu vào D-Pad. Chế độ 'Bắt buộc' ghi đè đầu vào analog gốc của core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_INPUT_ADC_TYPE,
   "Ánh xạ cần analog đã chỉ định cho đầu vào D-Pad.\nNếu core có hỗ trợ analog gốc, ánh xạ D-Pad sẽ bị vô hiệu trừ khi chọn tùy chọn '(Bắt buộc)'.\nNếu ánh xạ D-Pad bị bắt buộc, core sẽ không nhận đầu vào analog từ cần đã chỉ định."
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
   "Chỉ định cổng lõi nào sẽ nhận đầu vào từ cổng bộ điều khiển giao diện người dùng %u."
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
   "Nhấn nhanh"
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
   "Chạy logic core bổ sung để giảm độ trễ. Chế độ Một Phiên bản chạy tới một khung hình tương lai, sau đó tải lại trạng thái hiện tại. Chế độ Phiên bản Thứ hai giữ một core chỉ video tại khung hình tương lai để tránh sự cố trạng thái âm thanh. Chế độ Khung hình Chủ động chạy qua các khung hình trước với dữ liệu đầu vào mới khi cần, để tăng hiệu quả."
   )
#if !(defined(HAVE_DYNAMIC) || defined(HAVE_DYLIB))
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNAHEAD_MODE_NO_SECOND_INSTANCE,
   "Chạy logic core bổ sung để giảm độ trễ. Chế độ Một Phiên bản chạy tới một khung hình tương lai, sau đó tải lại trạng thái hiện tại. Chế độ Khung hình Chủ động chạy qua các khung hình trước với dữ liệu đầu vào mới khi cần, để tăng hiệu quả."
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
   MENU_ENUM_SUBLABEL_PREEMPT_FRAMES,
   "Số khung hình để chạy lại. Gây ra các vấn đề khi chơi như giật hình nếu số khung hình trễ nội bộ trong trò chơi bị vượt quá."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Kích hoạt Hardware Shared Context"
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
   "Tải Core giả khi tắt máy"
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
   "Kiểm tra xem tất cả firmware cần thiết có đầy đủ trước khi thử tải nội dung."
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
   "Luôn tải lại Core khi chạy nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Khởi động lại RetroArch khi khởi chạy nội dung, ngay cả khi core yêu cầu đã được tải. Điều này có thể cải thiện độ ổn định hệ thống, nhưng thời gian tải sẽ tăng."
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
   "Lưu file ánh xạ phím khi thoát"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "Lưu các thay đổi trong bất kỳ file ánh xạ phím nào đang hoạt động khi đóng nội dung hoặc thoát RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Tải Content-Specific Core Options Automatically"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Enable customized core options by default at startup."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Tự động tải tập tin ghi đè"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Enable customized configuration by default at startup."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Tự động tải tập tin Remap"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Enable customized controls by default at startup."
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
   "Tự động tải Shader Presets"
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
   "Sắp xếp file lưu theo thư mục nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Sắp xếp các tệp lưu vào các thư mục được đặt tên theo thư mục chứa nội dung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Sắp xếp trạng thái lưu theo thư mục nội dung"
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
   "Có deserialize các checkpoint được lưu trong phát lại trong quá trình phát thường xuyên."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_CHECKPOINT_DESERIALIZE,
   "Giải tuần tự điểm kiểm tra phát lại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_REPLAY_CHECKPOINT_DESERIALIZE,
   "Có nên giải tuần tự các điểm kiểm tra được lưu trong các phát lại trong khi phát lại bình thường hay không. Nên bật cho hầu hết các core, nhưng một số core có thể hoạt động không ổn định khi giải tuần tự nội dung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Tự động tăng chỉ số Lưu trạng thái"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Trước khi tạo một lưu trạng thái, chỉ số lưu trạng thái sẽ tự động tăng. Khi tải nội dung, chỉ số sẽ được đặt bằng chỉ số cao nhất hiện có."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REPLAY_AUTO_INDEX,
   "Tự động tăng chỉ số phát lại"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REPLAY_AUTO_INDEX,
   "Trước khi tạo một phát lại, chỉ số phát lại sẽ tự động tăng. Khi tải nội dung, chỉ số sẽ được đặt bằng chỉ số cao nhất hiện có."
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
   "Tự động tạo trạng thái lưu khi đóng nội dung. Trạng thái này sẽ được tải khi khởi động nếu “Tự động tải trạng thái” được bật."
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
   "Sắp xếp ảnh chụp màn hình theo thư mục nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Sắp xếp ảnh chụp màn hình vào các thư mục được đặt theo tên thư mục chứa nội dung."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Ghi dữ liệu lưu vào thư mục nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Sử dụng thư mục nội dung làm thư mục lưu file."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Ghi trạng thái lưu vào thư mục nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Sử dụng thư mục nội dung làm thư mục lưu trạng thái."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "File hệ thống nằm trong thư mục nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Sử dụng thư mục nội dung làm thư mục Hệ thống/BIOS."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Ghi ảnh chụp màn hình vào Thư mục Nội dung"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Sử dụng thư mục nội dung làm thư mục lưu ảnh chụp màn hình."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Lưu Nhật ký Thời gian Chạy (Theo Core)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Theo dõi thời gian chạy của từng nội dung, với bản ghi được tách riêng theo core."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Lưu Nhật ký Thời gian Chạy (Tổng hợp)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Theo dõi thời gian chạy của từng nội dung, ghi lại tổng thời gian tích lũy trên tất cả các core."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Mức chi tiết ghi nhật ký"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Kích hoạt or disable logging to the terminal."
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
   "Mức ghi log cho Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Thiết lập mức ghi log cho các core. Nếu mức log từ core thấp hơn giá trị này, nó sẽ bị bỏ qua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_LIBRETRO_LOG_LEVEL,
   "Thiết lập mức ghi log cho các core libretro (GET_LOG_INTERFACE). Nếu mức log do core libretro gửi thấp hơn mức log libretro, nó sẽ bị bỏ qua. Các log DEBUG luôn bị bỏ qua trừ khi chế độ chi tiết được bật (--verbose).\nDEBUG = 0\nINFO = 1\nWARN = 2\nERROR = 3"
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
   "Ghi Log vào File"
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
   "Khi ghi log vào file, chuyển hướng đầu ra từ mỗi phiên RetroArch sang một file mới có đánh dấu thời gian. Nếu tắt, log sẽ bị ghi đè mỗi khi RetroArch được khởi động lại."
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
   "Mở Trình duyệt Tệp tại vị trí đã sử dụng lần cuối khi tải nội dung từ Thư mục Bắt đầu. Lưu ý: Vị trí sẽ được đặt lại về mặc định khi khởi động lại RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SUGGEST_ALWAYS,
   "Luôn Gợi ý Core"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SUGGEST_ALWAYS,
   "Gợi ý các core có sẵn ngay cả khi đã có một core được tải."
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
   "Tốc độ tối đa mà nội dung sẽ chạy khi sử dụng tua nhanh (ví dụ, 5.0x cho nội dung 60 fps = giới hạn 300 fps). Nếu đặt thành 0.0x, tỷ lệ tua nhanh là không giới hạn (không giới hạn FPS)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_HELP_FASTFORWARD_RATIO,
   "Tốc độ tối đa mà nội dung sẽ chạy khi sử dụng tua nhanh. (Ví dụ 5.0 cho nội dung 60 fps => giới hạn 300 fps).\nRetroArch sẽ tạm nghỉ để đảm bảo tốc độ tối đa không bị vượt quá. Không nên dựa vào giới hạn này để chính xác hoàn toàn."
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
   "Tốc độ phát nội dung khi sử dụng chế độ chậm."
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
   "Kích hoạt Rewind"
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
   "Kích hoạt Post Filter Recording"
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
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Tự động tải Preferred Overlay"
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

/* Settings > On-Screen Display > On-Screen Overlay > Keyboard Overlay */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Lightgun */


/* Settings > On-Screen Display > On-Screen Overlay > Overlay Mouse */


/* Settings > On-Screen Display > Video Layout */


/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Thông báo Trên Màn hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Phông chữ thông báo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Kích thước thông báo"
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Hiển thị Cài đặt Nâng cao"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Enable Kiosk Mode"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Kích hoạt UI Companion"
   )
#ifdef _3DS
#endif

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Trình đơn lẹ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Thiết lập"
   )
#ifdef HAVE_LAKKA
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Trình đơn chính"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Hiển thị tên của core"
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */


/* Settings > User Interface > Views > Settings */



/* Settings > User Interface > Appearance */


/* Settings > AI Service */


/* Settings > Accessibility */


/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Kích hoạt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Chế độ Hardcore"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Thử nghiệm không chính thức"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Enable or disable unofficial achievements and/or beta features for testing purposes."
   )

/* Settings > Achievements > Appearance */


/* Settings > Achievements > Visibility */


/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Tùy chỉnh"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Enable stdin command interface."
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "URL của Buildbot Cores"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "URL của Buildbot Assets"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Tự động giải nén lưu trữ tải về"
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Lịch sử"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Enable or disable recent playlist for games, images, music, and videos."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Core Danh mục:"
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Xoá danh mục"
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Những tài khoản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Tên truy nhập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Ngôn ngữ"
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Cho phép Camera"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Enable Discord"
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Thành tích Retro"
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Tên truy nhập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Mật khẩu"
   )

/* Settings > User > Accounts > YouTube */


/* Settings > User > Accounts > Twitch */


/* Settings > User > Accounts > Facebook Gaming */


/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "System/BIOS Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Mục Downloads"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Danh mục assets"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Mục nền năng động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Thumbnails Danh mục"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Yêu thích"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Core Info Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Playlists Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Danh mục cache"
   )

#ifdef HAVE_MIST
/* Settings > Steam */



MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "Mục nội dung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Tên của Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Tên hệ thống"
   )
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


/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Tên hệ thống"
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Khu vực"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Rename the title of the entry."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Xoá"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Thông tin"
   )

/* Playlist Item > Set Core Association */


/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Tên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Core Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Mục cơ sở dữ liệu nội dung"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Khởi động lại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Đóng nội dung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Chụp ảnh màn hình"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Savestate Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Tải State"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Undo Tải State"
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
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Điều khiển"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Điều khiển đĩa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Danh sách thành tích"
   )

/* Quick Menu > Options */


/* Quick Menu > Options > Manage Core Options */


/* - Legacy (unused) */

/* Quick Menu > Controls */


/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Tải Remap File"
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */


/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Tải tập tin Cheat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Load a cheat file."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Lưu tập tin Cheat như"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Áp dụng thay đổi của Cheat"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */


/* Quick Menu > Cheats > Load Cheat File (Replace) */


/* Quick Menu > Cheats > Load Cheat File (Append) */


/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Miêu tả"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Đẩy đĩa ra"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Thêm đĩa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Chỉ số đĩa"
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Áp dụng Changes"
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

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Trở lại"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Không tìm thấy thư mục."
   )

/* Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Mục nội dung>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Mặc định>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Không có gì>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Screenshot Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Vô hiệu hoá"
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
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Mục nội dung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Mục nội dung>"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Tỷ lệ khung hình"
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Không"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_AUTO,
   "Tự động"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Tỷ lệ số nguyên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Tùy chỉnh"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Nền năng động"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Select a different theme for the icon. Changes will take effect after you restart the program."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Tùy chỉnh"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_HEADER_SEPARATOR_NONE,
   "Không"
   )



/* MaterialUI: Settings > User Interface > Appearance */


/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "Tự động"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Thông tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Tải Core..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Preview Shader Parameters"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Thiết lập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Trợ giúp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Tải Core"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Tên"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Playlists Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Quản lý tập tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Screenshot Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Core Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Core Info Danh mục"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Thông tin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Dừng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Xóa danh sách phát"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Tên:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Core Danh mục:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Mục cơ sở dữ liệu nội dung:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Xoá"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Tài khoản Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Điểm cuối của danh sách tài khoản"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
   "Những thành tựu Retro"
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
   MENU_ENUM_LABEL_VALUE_PORT,
   "Cổng"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Người dùng"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "Đồng bộ cứng CPU và GPU. Giảm độ trễ nhưng giảm hiệu suất."
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Kích hoạt System BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Enable Recording"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Kích hoạt Netplay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Trợ giúp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Âm thanh/Video Troubleshooting"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "Đang tải Content"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Database thiết lập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Kích hoạt Netplay Client"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Kích hoạt Netplay Spectator"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Miêu tả"
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Config Danh mục"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Mục Downloads"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Netplay thiết lập"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Mục nội dung"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Hỏi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Trình đơn điều khiển căn bản"
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
   "Những mặc định"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Bật/tắt bàn phím"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Bật/tắt trình đơn"
   )

/* Discord Status */


/* Notifications */

MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Netplay không biết lệnh nhận được"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Ttệp đã tồn tại. Đang lưu vào backup buffer"
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
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Đang kết nối vào máy chủ netplay"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Đang kết nối vào port"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Khe kết nối"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Application Danh mục"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Đang áp dụng cheat changes."
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Đang áp dụng shader"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Âm thanh muted."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Âm thanh unmuted."
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Đang đưa lên lệnh giao diện trên cổng"
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Ngắt kết nối thiết bị từ cổng hợp lệ."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Bị lỗi khi chạy chương trình điều khiển âm thanh. Sẽ tiếp tục chạy và bỏ âm thanh."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Bị lỗi khi chụp ảnh màn hình."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Không thể bật âm thanh."
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
   MSG_FOUND_LAST_STATE_SLOT,
   "Tiềm thấy state slot xài lần trước"
   )
MSG_HASH(
   MSG_LOADING,
   "Đang tải"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Đang nạp tập tin lịch sử"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Đang tải state"
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
   MSG_FAST_FORWARD,
   "Nhanh về phía trước."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Kết nối thiết bị từ cổng hợp lệ."
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
   "Viewport size calculation failed! Will continue using raw data. This will probably not work right ..."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Đang tự đông tải savestate từ"
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Cập nhật Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Khởi động lại"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Tắt Máy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Kích hoạt Soft Filter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLUETOOTH_SETTINGS,
   "Kích hoạt Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
   "Wi-Fi Driver"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Thủ công"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Khởi động lại RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Kích hoạt Touch"
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
   "Kích hoạt Bluetooth"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Quản lý dịch vụ của hệ điều hành."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
   "Kích hoạt SAMBA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Bật/tắt chia sẻ thư mục trên mạng."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
   "Kích hoạt SSH"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Bật/tắt giao thức SSH."
   )
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
   "Màn hình dưới cùng của 3DS"
   )
#endif
#ifdef HAVE_QT
#endif
#ifdef HAVE_GAME_AI





#endif

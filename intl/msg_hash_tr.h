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
   "Ana Menü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Ayarlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Sık Kullanılanlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Geçmiş"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Resimler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Müzikler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Videolar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Keşfet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "İçeriksiz Çekirdekler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "İçerik Aktar"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Hızlı Menü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "İlgili tüm oyun içi ayarlara hızlıca erişin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Çekirdek Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Kullanılacak çekirdeği seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "İçerik Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Hangi içeriğin başlatılacağını seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Disk Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Fiziksel ortam diski yükleyin. Öncelikle diskle kullanacağınız çekirdeği seçmelisiniz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Diskten Aktar"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Fiziksel ortam diskini dahili depolamaya aktar. Bir kalıp dosyası olarak kaydedilecektir."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Diski Çıkar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Fiziksel diski CD/DVD sürücüsünden çıkar."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Oynatma Listeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Veritabanına uyan taranmış içerikler burada görünecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "İçerik Aktar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "İçerik tarayarak oynatma listeleri oluşturun ve güncelleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Masaüstü Menüsünü Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Geleneksel masaüstü menüsünü açar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Kiosk Kipini Devre Dışı Bırak (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Yapılandırma ile ilgili tüm ayarları göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Çevrimiçi Güncelleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "RetroArch için eklentiler, bileşenler ve içerikler indir."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Netplay oturumu kur veya katıl."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Ayarlar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Programı yapılandır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Bilgi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Sistem bilgisini göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Yapılandırma Dosyası"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Yapılandırma dosyalarını yönet ve oluştur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Yardım"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Programın nasıl çalıştığı hakkında daha fazla bilgi edinin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Yeniden Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Programı yeniden başlat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Çıkış"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Programdan çık."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Bir Çekirdek İndir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Çevrimiçi güncelleyici üstünden çekirdek indir ve kur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Kur yada Bir Çekirdeği Geri Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "'İndirilenler' dizininden Çekirdeği kur ya da geri yükle."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Video İşlemcisini Başlat"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Uzaktan RetroPad Başlat"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Başlangıç Dizini"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "İndirilenler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Arşive Gözat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Arşivi Yükle"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Sık Kullanılanlar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "'Sık Kullanılanlar' kısmına eklediğiniz içerik burada görünecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Müzik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Daha önce oynatılmış olan müzikler burada görünecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Resimler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Daha önce görüntülenen resimler burada görünecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Videolar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Daha önce oynatılmış olan videolar burada görünecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Keşfet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Kategorize edilmiş bir arama arayüzü aracılığıyla veritabanı ile eşleşen tüm içeriğe göz atın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "İçeriksiz Çekirdekler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "İçerik yüklemeden çalışabilen kurulu çekirdekler burada görünecektir."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Çekirdek İndir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Kurulu Çekirdekleri Güncelle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Kurulu tüm çekirdekleri mevcut en son sürüme güncelleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Çekirdekleri Play Store Sürümüne Göre Değiştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Tüm eski ve el ile kurulmuş çekirdekleri, mevcut olduğu yerlerde Play Store'daki en son sürümlerle değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Küçük Resim Güncelleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Seçilen sistem için komple küçük resim paketini indirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Oynatma Listesi Küçük Resim Güncelleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Seçilen oynatma listesindeki girdiler için küçük resimleri indirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "İçerik İndirici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_CONTENT,
   "Seçili çekirdek için ücretsiz içerik indirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Çekirdek Sistem Dosyaları İndirici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Çekirdeklerin çalışması için gerekli olan doğrulanmış sistem dosyalarını indirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Çekirdek Bilgileri Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "İçerikleri Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Kontrolcü Profillerini Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Hileleri Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Veritabanlarını Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Kaplamaları Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "GLSL Gölgelendiricilerini Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Cg Gölgelendiricilerini Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Slang Gölgelendiricilerini Güncelle"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Çekirdek Bilgisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Uygulama/çekirdek ilgili bilgileri görüntüle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Disk Bilgisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Takılan ortam diskleri hakkındaki bilgileri görüntüle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Ağ Bilgisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Ağ arayüzlerini ve ilgili IP adreslerini göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Sistem Bilgisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Cihaza özgü bilgileri gösterir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Veritabanı Yöneticisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Veritabanlarını görüntüle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "İmleç Yöneticisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Önceki aramaları görüntüle."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Çekirdek İsmi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Çekirdek Etiketi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_VERSION,
   "Çekirdek Sürüm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Sistem Adı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Sistem Üreticisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Kategoriler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Yazar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "İzinler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Lisans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Desteklenen Eklentiler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Gerekli Grafik API"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Durum Kaydı Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Basit (Kaydet/Yükle)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Orta (Kaydet/Yükle, Geri Sar)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Ayrıntılı (Kaydet/Yükle, Geri Sar, Önden Git, Netplay)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Ürün Yazılımı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Eksik, Gerekli:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Eksik, İsteğe Bağlı:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Mevcut, Gerekli:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Mevcut, İsteğe Bağlı:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Kurulu Çekirdeği Kilitle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Kurulu olan çekirdeğin değiştirilmesini önle. İçerik belirli bir çekirdek sürüm gerektirdiğinde istenmeyen güncellemeleri önlemek için kullanılabilir (örn. Arcade ROM setleri)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_STANDALONE_EXEMPT,
   "'İçeriksiz Çekirdekler' Menüsünden Hariç Tut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SET_STANDALONE_EXEMPT,
   "Bu çekirdeğin 'İçeriksiz Çekirdekler' sekmesinde/menüsünde görüntülenmesini önleyin. Yalnızca görüntüleme kipi 'Özel' olarak ayarlandığında geçerlidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Çekirdeği Sil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Bu çekirdeği diskten kaldır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Çekirdeği Yedekle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Kurulu olan çekirdeğin arşivlenmiş bir yedeğini oluşturun."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Yedeği Geri Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Arşivlenen yedeklemeler listesinden çekirdeğin önceki bir sürümünü kurun."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Yedeği Sil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Arşivlenen yedeklemeler listesinden bir dosyayı kaldırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_MODE_AUTO,
   "[Otomatik]"
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Oluşturulma Tarihi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Git Sürümü"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Derleyici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "CPU Modeli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "CPU Özellikleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "CPU Mimarisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "CPU Çekirdekleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Ön Uç Tanımlayıcı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "İşletim sistemi"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "RetroRating Seviyesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Güç Kaynağı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Video İçerik Sürücüsü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Ekran Genişliği (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Ekran Yüksekliği (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Ekran DPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "LibretroDB Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Kaplama Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Komut Arayüzü Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Ağ Komutu Arayüz Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Ağ Kontrolcü Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Cocoa Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "PNG (RPNG) Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "JPEG (RJPEG) Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "BMP (RBMP) Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "TGA (RTGA) Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "SDL 1.2 Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "SDL 2 Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Vulkan Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Metal Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "OpenGL Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "OpenGL ES Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "İş Parçacığı Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "KMS/EGL Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "udev Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "OpenVG Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "EGL Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "X11 Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Wayland Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "XVideo Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "ALSA Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "OSS Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "OpenAL Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "OpenSL Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "RSound Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "RoarAudio Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "JACK Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "PulseAudio Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "CoreAudio Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "CoreAudio V3 Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "DirectSound Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "WASAPI Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "XAudio2 Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "zlib Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "7zip Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Dinamik Kütüphane Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Libretro Kütüphanesinin Dinamik Çalışma Zamanı Yükleniyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Cg Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "GLSL Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "HLSL Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "SDL Image Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "FFmpeg Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "mpv Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "CoreText Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "FreeType Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "STB TrueType Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Netplay (Eşli Oynama) Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Video4Linux2 Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "libusb Desteği"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Veritabanı Seçimi"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "İsim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Açıklama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Tür"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Başarımlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CATEGORY,
   "Kategori"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Dil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Bölge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONSOLE_EXCLUSIVE,
   "Konsola özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PLATFORM_EXCLUSIVE,
   "Platforma özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SCORE,
   "Puan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MEDIA,
   "Medya"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Kontrolcüler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ARTSTYLE,
   "Sanat Tarzı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GAMEPLAY,
   "Oynanış"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NARRATIVE,
   "Hikaye"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PACING,
   "İlerleyiş"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PERSPECTIVE,
   "Perspektif"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SETTING,
   "Ayar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VISUAL,
   "Görsel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_VEHICULAR,
   "Taşıtlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Yayıncı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Geliştirici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Köken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Seri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "TGDB Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Famitsu Magazin Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Edge Magazin İncelemesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Edge Magazin Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Edge Magazin Sayısı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Çıkış Tarihi Ay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Çıkış Tarihi Yıl"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "BBFC Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "ESRB Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "ELSPA Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "PEGI Derecelendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Donanım Geliştirme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "CERO Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Seri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analog Destekli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Titreşim Destekli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Co-op Destekli"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Yapılandırma Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS,
   "Yapılandırmayı yükleyin ve mevcut değerleri değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Mevcut Yapılandırmayı Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG,
   "Mevcut yapılandırma dosyasının üzerine yaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Yeni Yapılandırmayı Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_NEW_CONFIG,
   "Mecvut yapılandırmayı ayrı bir dosyaya kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Varsayılanlara Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Mevcut yapılandırmayı varsayılan değerlere sıfırlayın."
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Temel Menü Kontrolleri"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Yukarı Kaydır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Aşağı Kaydır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Onayla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "Bilgi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Başlat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Menüyü Değiştir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Çık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Klavyeyi Değiştir"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Sürücüler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Sistem tarafından kullanılan sürücüleri değiştir."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Video çıkış ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Ses"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Ses çıkışı ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Giriş"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Oyun kolu, klavye ve fare ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Gecikme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Video, ses ve giriş gecikmesi ile ilgili ayarları değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Çekirdek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Çekirdek ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Yapılandırma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Yapılandırma dosyaları için varsayılan ayarları değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Durum Kayıtları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Durum kayıtları ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Günlükler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Günlük ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Dosya Tarayıcısı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Dosya tarayıcı ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Kare Sınırı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Geri sarma, ileri sarma ve ağır çekim ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Ekran Kayıtları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Oyun için ekran kayıtları ve yayıncılık ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Ekran Görünümü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Ekran kaplamasını, klavye kaplamasını ve ekrandaki bildirim ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Kullanıcı Arayüzü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Kullanıcı arayüzü ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Çeviri Servisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Çeviri servisi ayarlarını değiştir (Tercüme/TTS /Çeşitli)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Erişilebilirlik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Erişilebilirlik ekran okuyucusu ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Güç Yönetimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Güç yönetimi ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Başarımlar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Başarı ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Ağ"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Sunucu ve ağ ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Oynatma Listeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Oynatma listesi ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Kullanıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Hesap, kullanıcı adı veya dil ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Dizin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Dosyaların bulunduğu varsayılan dizinleri değiştir."
   )

/* Core option category placeholders for icons */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HACKS_SETTINGS,
   "Geliştirmeler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAPPING_SETTINGS,
   "Eşleme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEDIA_SETTINGS,
   "Medya"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFORMANCE_SETTINGS,
   "Performans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SOUND_SETTINGS,
   "Ses"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SPECS_SETTINGS,
   "Özellikler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STORAGE_SETTINGS,
   "Depolama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_SETTINGS,
   "Sistem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMING_SETTINGS,
   "Zamanlama"
   )

#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_SETTINGS,
   "Steam ile ilgili ayarları değiştirin."
   )
#endif

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Giriş"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Kullanılacak giriş sürücüsü. Bazı video sürücüleri farklı bir giriş sürücüsünü zorlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Kontrolcü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Kullanılacak kontrolcü sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Kullanılacak video sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Ses"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Kullanılacak ses sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Ses Yeniden Örnekleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Kullanılacak yeniden ses örnekleyici."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Kamera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Kullanılacak kamera sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Kullanılacak Bluetooth sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Kullanılacak Wi-Fi sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Konum"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Kullanılacak konum sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Menü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Kullanılacak menü sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Kayıt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Kullanılacak kayıt sürücüsü."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Kullanılacak MIDI sürücüsü."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "CRT Çözünürlüğünü Değiştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "CRT ekranlarında kullanmak için yerel, düşük çözünürlüklü sinyaller verir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Çıkış"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Video çıkış ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Tam Ekran Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Tam ekran ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Pencere Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Pencereli ekran ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Ölçekleme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Video ölçeklendirme ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Video HDR ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Eşitleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Video eşitleme ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Ekran Koruyucuyu Önle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Sisteminizin ekran koruyucusunun aktif hale gelmesini önler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Baskın Video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Gecikme ve daha fazla video takılma pahasına performansı artırır. Yalnızca tam hız elde edemiyorsanız kullanın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Siyah Kare Ekle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "Kareler arasına siyah bir kare ekler. Bazı yüksek yenileme hızlı ekranlarda gölgelenmeyi ortadan kaldırmak için kullanışlıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "GPU ile Ekran Görüntüsü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Varsa, GPU gölgelendirici ekran görüntüsünü kullanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "İki Çizgili Filtreleme"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Görüntü Enterpolasyonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Dahili IPU aracılığıyla içeriği ölçeklendirirken görüntü enterpolasyon yöntemini belirtir. CPU destekli video filtreleri kullanılırken \"Bikübik\" veya \"Çift Doğrusal\" önerilir. Bu seçeneğin performansa etkisi yoktur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bikübik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "İkili Doğrusal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "En Yakın İlişki"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Görüntü İnterpolasyonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "'Tamsayı Ölçeği' devre dışı bırakıldığında görüntü interpolasyon yöntemini belirtin. 'En Yakın İlişki' en az performans etkisine sahiptir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "En Yakın İlişki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Yarı Doğrusal"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Otomatik Gölgelendirici Gecikmesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Otomatik yüklenen gölgelendiricileri geciktirir (ms cinsinden). 'Ekran yakalama' yazılımını kullanırken grafiksel bozulmalar olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Video Filtresi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "CPU ile çalışan bir video filtresi uygula. Yüksek performans tüketebilir. Bazı video filtreleri yalnızca 32bit veya 16bit renk kullanan çekirdekler için kullanılabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Video Filtresini Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Tüm aktif CPU destekli video filtrelerini kaldırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Android cihazlarda çentik üzerinde tam ekranı etkinleştir"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "CRT Çözünürlüğünü Değiştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Sadece CRT ekranlar için. Tam çekirdek/oyun çözünürlüğünü ve yenileme hızını kullanmaya çalışır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "CRT Süper Çözünürlük"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Doğal ve ultrageniş süper çözünürlükler arasında geçiş yapın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "X-Yönünde Merkezleme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Görüntü ekranda doğru şekilde ortalanmamışsa bu seçenekler arasında gezinin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Sundurma Ayarı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Görüntü boyutunu değiştirmek için sundurma ayarlarını yapmak için bu seçenekler arasında dolaşın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Yüksek Çözünürlüklü Menü Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Hiçbir içerik yüklenmediğinde yüksek çözünürlüklü menülerle kullanım için yüksek çözünürlüklü modele geçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Özel Yenileme Oranı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Gerekirse, yapılandırma dosyasında belirtilen özel bir yenileme hızı kullanın."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Monitör İndeksi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Hangi ekranın kullanılacağını seçer."
   )
#if defined (WIIU)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WIIU_PREFER_DRC,
   "Wii U GamePad için Düzenleyin (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WIIU_PREFER_DRC,
   "Görünüm alanı olarak GamePad'in tam 2x ölçeğini kullanın. Yerel TV çözünürlüğünde görüntülemeyi devre dışı bırakın."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Videoyu Döndür"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Videonun belirli bir dönüşünü zorlar. Dönme, çekirdeğin ayarladığı dönüşlere eklenir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Ekran Yönü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "İşletim sisteminden ekranın belirli bir yönünü zorlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "GPU İndeksi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Hangi grafik kartının kullanılacağını seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Yatay Ekran Dengeleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Videoyu yatay olarak belirli bir düzene zorlar. Düzen genel ayar olarak uygulanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Dikey Ekran Dengeleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Videoyu dikey olarak belirli bir düzene zorlar. Düzen genel ayar olarak uygulanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Dikey Yenileme Hızı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Ekranınızın dikey yenileme hızıdır. Uygun bir ses giriş oranını hesaplamak için kullanılır.\n'Baskın Video' etkinse bu yok sayılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Tahmini Ekran Yenileme Hızı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Ekranın Hz cinsinden yenileme hızı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Ekranın Belirlenen Yenileme Hızını Ayarla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "Ekran sürücüsü tarafından bildirilen yenileme hızı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Yenileme Hızını Otomatik Olarak Değiştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_AUTOSWITCH_REFRESH_RATE,
   "Çalıştırılan çekirdeğe ve/veya içeriğe bağlı olarak, belirtilen ekran kipini kullanırken ekranın yenileme hızını otomatik olarak değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN,
   "Sadece Özel Tam Ekran Kipinde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN,
   "Sadece Pencereli Tam Ekran Kipinde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN,
   "Tüm Tam Ekran Kiplerinde"
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Dikey Yenileme Hızı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Ekranın dikey yenileme oranını ayarlayın. '50 Hz' PAL içeriği çalıştırırken düzgün video sağlar."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "sRGB FBO Zorla Devre Dışı Bırak"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "SRGB FBO desteğini zorla devre dışı bırakır. Windows'taki bazı Intel OpenGL sürücüleri, eğer etkinse, sRGB FBO desteğiyle ilgili video sorunları yaşayabilir. Bunu etkinleştirmek, bu durumu çözebilir."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Tam Ekran Modunda Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Tam ekran ile başlayın. Çalışma zamanında değiştirilebilir. Bir komut satırı anahtarı ile geçersiz kılınabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Pencereli Tam Ekran Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Tam ekran ise, ekran kipi geçişini önlemek için tam ekran pencere kullanmayı tercih edin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Tam Ekran Genişliği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Penceresiz tam ekran kipi için özel genişlik boyutunu ayarla. Bunu ayarsız bırakırsanız masaüstü çözünürlüğünü kullanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Tam Ekran Yüksekliği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Penceresiz tam ekran kipi için özel yükseklik boyutunu ayarla. Bunu ayarsız bırakırsanız masaüstü çözünürlüğünü kullanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "UWP'de çözünürlüğü zorla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Çözünürlüğü tam ekran boyutuna zorlar, 0 olarak ayarlanırsa sabit 3840 x 2160 değer kullanılır."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Ölçekli Pencere"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Pencere boyutunu, çekirdek görüntü alanı boyutunun belirtilen katına ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Pencere Şeffaflığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OPACITY,
   "Pencere şeffaflığını ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Pencere Süslemelerini Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Pencere başlık çubuğunu ve kenarlıklarını göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Menü Çubuğunu Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UI_MENUBAR_ENABLE,
   "Pencere menü çubuğunu göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Pencere Konumunu ve Boyutunu Hatırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Tüm içeriği 'Pencere Genişliği' ve 'Pencere Yüksekliği' ile belirtilen boyutların sabit boyutlu bir penceresinde gösterin ve RetroArch'ı kapattıktan sonra geçerli pencere boyutunu ve konumunu kaydedin. Devre dışı bırakıldığında, pencere boyutu 'Pencereli Ölçeğe' dayalı dinamik olarak ayarlanacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Özel Pencere Boyutu Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Tüm içeriği, 'Pencere Genişliği' ve 'Pencere Yüksekliği' ile belirtilen boyutların sabit boyutlu bir penceresinde gösterin. Devre dışı bırakıldığında, pencere boyutu 'Pencereli Ölçeğe' dayalı olarak dinamik olarak ayarlanacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Pencere Genişliği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Ekran penceresi için özel genişliği ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Pencere Yüksekliği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Ekran penceresi için özel yüksekliği ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Azami Pencere Genişliği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "'Pencereli Ölçeğe' dayalı otomatik olarak yeniden boyutlandırma yapılırken görüntüleme penceresinin azami genişliğini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Azami Pencere Yüksekliği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "'Pencereli Ölçeğe' dayalı otomatik olarak yeniden boyutlandırma yapılırken görüntüleme penceresinin azami yüksekliğini ayarlayın."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Tam Sayı Ölçeği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Videoyu yalnızca tamsayı adımlarla ölçeklendirir. Temel boyut, sistem tarafından bildirilen geometriye ve en/boy oranına bağlıdır. 'En/Boy Oranını Zorla' ayarlanmazsa, X/Y birbirlerinden bağımsız, tamsayı katlarıyla ölçeklendirilirler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_OVERSCALE,
   "Tamsayı Ölçeği Aşırı Ölçek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_OVERSCALE,
   "Aşağı yuvarlamak yerine bir sonraki daha büyük tam sayıya yuvarlamak için tamsayı ölçeklendirmeyi zorlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "En Boy Oranı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO_INDEX,
   "Ekran en boy oranını ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "En Boy Oranını Yapılandır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Video en boy oranı (genişlik/yükseklik) için kayan nokta değeri."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG,
   "Yapılandırma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_SQUARE_PIXEL,
   "1:1 EŞİT (%u:%u DAR)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED,
   "Çekirdeğe göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM,
   "Özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL,
   "Tam"
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "En Boy Oranını Koru"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Dahili IPU aracılığıyla içeriği ölçeklendirirken 1:1 piksel en boy oranlarını koruyun. Devre dışı bırakılırsa, görüntüler tüm ekranı dolduracak şekilde uzatılacaktır."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Özel En Boy Oranı (X Konumu)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Görünüm penceresinin X ekseni konumunu tanımlamak için kullanılan özel görünüm alanı ofseti.\n'Tam sayı Ölçeği' etkinse bunlar yok sayılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Özel En Boy Oranı (Y Konumu)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Görünüm penceresinin Y ekseni konumunu tanımlamak için kullanılan özel görünüm alanı ofseti.\n'Tam sayı Ölçeği' etkinse bunlar yok sayılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Özel En Boy Oranı (Genişlik)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "En Boy Oranı 'Özel En Boy Oranı' olarak ayarlanmışsa kullanılan özel görünüm alanı genişliği."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Özel En Boy Oranı (Yükseklik)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "En Boy Oranı 'Özel En Boy Oranı' olarak ayarlanmışsa kullanılan özel görünüm alanı yüksekliği."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Aşırı Tarama (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Görüntünün kenarları etrafındaki birkaç pikseli, bazen de çöp pikselleri de içeren geliştiriciler tarafından geleneksel olarak boş bırakılır."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "HDR'yi Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Ekran destekliyorsa HDR'yi etkinleştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Tepe Parlaklığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Ekranınızın yeniden üretebileceği tepe parlaklığını (cd/m2 olarak) ayarlayın. Ekranınızın en yüksek parlaklığı için RT'lere bakın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Beyaz Kağıt Parlaklığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Kağıdın beyaz olması gereken parlaklığı, yani okunabilir metin veya SDR (Standart Dinamik Aralık) aralığının en üstünde parlaklık ayarlayın. Bulunduğunuz ortamdaki farklı aydınlatma koşullarına uyum sağlamak için kullanışlıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "Kontrast"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "HDR için gama/kontrast kontrolü. Renkleri alır ve görüntünün en parlak kısımları ile en karanlık kısımları arasındaki genel aralığı artırır. HDR Kontrastı ne kadar yüksek olursa, bu fark o kadar büyük olur, kontrast ne kadar düşükse görüntü o kadar soluk olur. Kullanıcıların görüntüyü kendi beğenilerine ve ekranlarında en iyi hissettiklerini ayarlamalarına yardımcı olur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Renk Sınırını Genişlet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Renk alanı doğrusal alana dönüştürüldüğünde, HDR10'a ulaşmak için genişletilmiş bir renk gamı ​​kullanmamız gerekip gerekmediğine karar verin."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Dikey Eşitleme (Vsync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Grafik kartının çıkış videosunu ekranın yenileme hızıyla eşitler. Tavsiye edilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "VSync Takas Aralığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "VSync için özel bir takas aralığı kullanın. Monitör yenileme hızını belirtilen etken kadar etkili bir şekilde azaltır. 'Otomatik', temel raporlanan kare hızına dayalı olarak etkeni ayarlar ve örneğin çalışırken gelişmiş kare hızı sağlar. 60 Hz ekranda 30 fps içerik veya 120 Hz ekranda 60 fps içerik."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL_AUTO,
   "Otomatik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Uyarlamalı VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "V-Sync, performans hedef yenileme hızının altına düşene kadar etkinleştirilir. Performans gerçek zamanın altına düştüğünde aksaklığı en aza indirebilir ve daha enerji tasarruflu olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Kare Gecikmesi (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Daha fazla video takılma riski pahasına gecikmeyi azaltır. VSync'den sonra gecikme ekler (ms cinsinden)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Otomatik Kare Gecikmesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Gelecekteki kare düşüşlerini önlemek için etkin 'Kare Gecikmesini' geçici olarak azaltın. 'Kare Gecikmesi' 0 olduğunda başlangıç ​​noktası yarım kare süresidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTOMATIC,
   "Otomatik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_EFFECTIVE,
   "etkili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Katı GPU Dikey Eşitleme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "CPU ve GPU’yu sabit olarak eşitle. Performanstan ödün vererek gecikmeyi azaltır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Katı GPU Dikey Kare Eşitlemesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "'Sabit GPU Eşitleyici' kullanılırken CPU'nun GPU'dan kaç kare çalıştırabileceğini ayarlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Tam İçerik Kare Hızına Eşitle (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Çekirdek talep edilen zamanlamadan sapma yok. Değişken Yenileme Hızı ekranları (G-Sync, FreeSync, HDMI 2.1 VRR) için kullanın."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Çıkış"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Ses çıkışı ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Yeniden Örnekleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Ses örnekleyici ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Eşitleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Ses eşitleme ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "MIDI ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Karıştırıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Ses karıştırıcı ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Menü Sesleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Menü ses ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Sustur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Sesi kapat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Karıştırıcıyı Sustur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Karıştırıcı sesini kapat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Hızlı İleri Sararken Sesi Kapat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Hızlı ileri sarma kullanırken sesi otomatik olarak sessize alır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Ses Artışı (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Ses seviyesi (dB cinsinden). 0 dB normal hacimdir ve kazanç uygulanmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Karıştırıcı Ses Kazancı (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Global ses karıştırıcı sesi (dB cinsinden). 0 dB normal hacimdir ve arttırma uygulanmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "DSP Eklentisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Sürücüye gönderilmeden önce sesi işleyen DSP ses eklentisi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "DSP Eklentisini Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Herhangi bir aktif ses DSP eklentisini kaldırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI Ayrıcalıklı Kip"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI sürücüsünün ses cihazının kontrolünü tamamen ele geçirmesine izin verin. Devre dışı bırakılırsa, bunun yerine paylaşılan kipi kullanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "WASAPI Float Biçimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Ses cihazınız tarafından destekleniyorsa, WASAPI sürücüsü için kayan nokta biçimini kullanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI Paylaşılan Arabellek Uzunluğu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI sürücüsünü paylaşılan kipte kullanırken ara arabellek uzunluğu (kare cinsinden)."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Ses"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Ses çıkışını etkinleştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Aygıt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Ses sürücüsünün kullandığı varsayılan ses cihazını özelleştir. Bu işlem sürücüye bağlıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Ses Gecikmesi (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Mili saniye cinsinden istenen ses gecikmesi. Ses sürücüsü verilen gecikmeyi sağlayamıyorsa ses duyulmayabilir."
   )

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Örnekleyici Kalitesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Performans/düşük gecikme süresi için düşük gecikme süresi için bu değeri düşürün, düşük performans/düşük gecikme pahasına daha iyi ses kalitesi istiyorsanız, artırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Çıkış Oranı (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Ses çıkışı örnekleme hızı."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Eşitleyici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Sesi eşitle. Tavsiye edilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Azami Zamanlama Eğimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Ses giriş hızındaki azami değişiklik. Bunun artırılması, yanlış bir ses perdesi pahasına zamanlamada çok büyük değişiklikler yapılmasını sağlar (örneğin, NTSC ekranlarında PAL çekirdeği çalıştırma)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Dinamik Ses Hızı Kontrolü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Ses ve video eşitlemesi yaparken zamanlamadaki kusurların giderilmesine yardımcı olur. Devre dışı bırakılırsa, uygun eşitlemenin elde edilmesinin neredeyse imkansız olduğunu unutmayın."
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Giriş"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Giriş cihazı seç."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Çıkış"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Çıkış cihazı seç."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Ses"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Çıkış sesini ayarla (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_MIXER_STREAM,
   "Karıştırıcı akışı #%d: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Oynat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Ses akışının oynatılmasını başlatır. Tamamlandığında, mevcut ses akışını bellekten kaldıracak."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Oynat (Döngüsel)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Ses akışının oynatılmasını başlatır. Tamamlandığında, tekrar baştan başlayıp tekrar çalmaya başlayacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Oynat (Ardışık)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Ses akışının oynatılmasını başlatır. Tamamlandığında, sıralı sırayla bir sonraki ses akışına atlar ve bu davranışı tekrarlar. Albüm oynatma kipi olarak kullanışlıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Durdur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Ses akışının oynatılmasını durdurur, ancak bellekten çıkarmaz. 'Oynat' seçeneğini seçerek tekrar oynamaya başlayabilirsiniz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Ses akışının oynatılmasını durdurur ve tamamen bellekten kaldırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Ses"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Ses akışının sesini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_NONE,
   "Durum : N/A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_STOPPED,
   "Durum : Durduruldu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING,
   "Durum : Oynanıyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_LOOPED,
   "Durum : Oynanıyor (Döngü)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL,
   "Durum : Oynanıyor (Ardışık)"
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Karıştırıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Menüde bile eş zamanlı ses akışları oynat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Tamam Sesini Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "İptal Sesini Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Bildirim Sesini Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Arkaplan Sesini Etkinleştir"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Azami Kullanıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "RetroArch tarafından desteklenen azami kullanıcı sayısı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Yoklama Davranışı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "RetroArch'da girdi yoklamanın nasıl yapıldığını etkileyin. 'Erken' veya 'Geç' olarak ayarlamak, yapılandırmanıza bağlı olarak daha az gecikmeye neden olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Bu Çekirdeğe Yeniden Kontrolcü Yapılandırması"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Mevcut çekirdek için ayarlanmış yeniden eşlenen bağlarla giriş bağlarını özelleştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Otomatik Yapılandırma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Profili Tak ve Çalıştır şeklinde olan kontrolcüleri otomatik olarak yapılandırır."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Windows Kısayol Tuşu Devre Dışı (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Win tuşu kombinasyonlarını uygulamanın içinde tutun."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Yardımcı Sensör Girişi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Mevcut donanım tarafından destekleniyorsa ivmeölçer, jiroskop ve aydınlatma sensörlerinden girişi etkinleştirir. Bazı platformlarda performansı etkileyebilir ve/veya güç tüketimini artırabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Otomatik Fare Yakalaması"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Uygulama odağında fare yakalamayı etkinleştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Otomatik 'Oyun Odaklanma' Kipini Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "İçeriği başlatırken ve devam ettirirken her zaman 'Oyun Odaklaması' kipini etkinleştirin. 'Algıla' olarak ayarlandığında, mevcut çekirdek ön uç klavye geri arama işlevini uygularsa seçenek etkinleştirilecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "AÇIK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Algıla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_ON_DISCONNECT,
   "Kontrolcü Bağlantısı Kesildiğinde İçeriği Duraklat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_ON_DISCONNECT,
   "Herhangi bir kontrolcünün bağlantısı kesildiğinde içeriği duraklatın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Giriş Düğmesi Eksen Eşiği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "'Analogdan Dijitale' geçişte bir düğmeye basılması için bir eksenin ne kadar eğilmesi gerektiği."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Analog Ölü Bölgesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_DEADZONE,
   "Ölü bölge değerinin altındaki analog çubuk hareketlerini görmezden gelin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Analog Hassasiyeti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ANALOG_SENSITIVITY,
   "Analog çubukların hassasiyetini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Bağlama Zaman Aşımı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Bir sonraki basılı tutma işlemine kadar bekleyecek saniye miktarı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Bağlama Beklemesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Bir girişi bağlamak için tutulan saniye miktarı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Turbo Aralığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Turbo özellikli düğmeler arasında geçiş yapılan süre (kare cinsinden)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
   "Turbo Görev Döngüsü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "Düğmelerin basılı tutulduğu Turbo Periyodundan kare sayısı. Bu sayı Turbo Periyoduna eşit veya ondan büyükse, düğmeler asla bırakılmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Turbo Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Turbo kipi genel davranışını seç."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_CLASSIC,
   "Klasik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON,
   "Tek Düğme (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_MODE_SINGLEBUTTON_HOLD,
   "Tek Düğme (Basılı Tut)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DEFAULT_BUTTON,
   "Varsayılan Turbo Düğmesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DEFAULT_BUTTON,
   "Turbo Kipi için varsayılan etkin düğme 'Tek Düğme'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Turbo Ateş"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Turbo ateş ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Dokunsal Geribildirim/Titreşim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Dokunsal geri bildirim ve titreşim ayarlarını değiştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Menü Kontrolcüleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Menü kontrol ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Kısayollar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Kısa yol tuş ayarlarını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Port %u Denetimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Bu port için kontrolcüleri değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Android bağlantı kesilmesi geçici çözümü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_ANDROID_INPUT_DISCONNECT_WORKAROUND,
   "Bağlantıyı kesen ve yeniden bağlanan denetleyiciler için geçici çözüm. Aynı kontrolcüye sahip 2 oyuncuyu engeller."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Çıkışı Onayla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "RetroArch üstünden çıkmak için çık tuşuna iki kez basılması gerekir."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Tuşa Basınca Titret"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Cihaz Titreşimini Etkinleştir (Desteklenen Çekirdekler İçin)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Titreşim Gücü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Titreşim geri besleme etkilerinin büyüklüğünü belirtin."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Birleşik Menü Kontrolcüleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Hem menü hem de oyun için aynı kontrolcüleri kullan. Klavyeye uygulanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_INFO_BUTTON,
   "Bilgi Düğmesini Devre Dışı Bırak"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_INFO_BUTTON,
   "Etkinleştirilirse, bilgi düğmesine tıklamak dikkate alınmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DISABLE_SEARCH_BUTTON,
   "Arama Düğmesini Devre Dışı Bırak"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DISABLE_SEARCH_BUTTON,
   "Etkinleştirilirse, arama düğmesine tıklamak dikkate alınmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Menü Tamam & İptal Düğmeleri Değişimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "TAMAM/İptal için düğmeleri değiştirin. Devre dışı bırakılınca, Japonya düğme yönüne geçer, etkinleştirilirse Batı düğme yönüne geçer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_SCROLL,
   "Menü Kaydırma Düğmelerini Değiştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_SCROLL,
   "Kaydırmak için düğmeleri değiştirin. Devre dışı olduğunda, L/R ile 10 ögeyi ve L2/R2 ile alfabetik olarak kaydırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Tüm Kullanıcılar Menüyü Kontrol Eder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Her kullanıcının menüyü kontrol etmesine izin verir. Devre dışı bırakılırsa, menüyü yalnızca Kullanıcı 1 kontrol edebilir."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Kısayol Tuşu Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Atandığında, diğer kısayol tuşları tanınmadan önce 'Kısayol Etkinleştir' tuşu basılı tutulmalıdır. Kontrolcü düğmelerinin normal girişi etkilemeden kısayol tuşu işlevleriyle eşlenmesine izin verir. Değiştiricinin yalnızca kontrolcüye atanması, klavye kısayol tuşları için bunu gerektirmez, ancak her iki değiştirici de her iki cihaz için çalışır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Kısayol Tuşu Etkinleştirme Gecikmesi (Kareler)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Atanan 'Kısayol Tuşu Etkinleştir' tuşuna basıldıktan sonra normal giriş engellenmeden önce karelere gecikme ekler. 'Kısayol Tuşu Etkinleştir' tuşundan normal girişin başka bir eyleme eşlendiğinde yakalanmasına izin verir (örn. RetroPad 'Seç')."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Menüyü Değiştir (Kontrolcü Kombinasyonu)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Menüyü değiştirmek için kontrolcü düğmesi kombinasyonu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Menüyü Değiştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Mevcut ekranı menü ve çalışan içerik arasında değiştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Çık (Kontrolcü Kombinasyonu)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "RetroArch üstünden çıkmak için denetleyici düğme kombinasyonu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Çık"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Tüm kayıt verilerinin ve yapılandırma dosyalarının diske atılmasını sağlayarak RetroArch'ı kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "İçeriği Kapat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Mevcut içeriği kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "İçeriği Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Mevcut içeriği baştan başlatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Hızlı İleri Sar (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Hızlı ileri ve normal hız arasında geçiş yapar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Hızlı İleri Sar (Basılı Tutarak)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Basılı tutulduğunda hızlı ileri sarmayı sağlar. Anahtar bırakıldığında içerik normal hızda çalışır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Ağır Çekim (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Ağır çekim ve normal hız arasında geçiş yapar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Ağır Çekim (Basılı Tutarak)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Basılı tutulduğunda ağır çekim sağlar. Anahtar bırakıldığında içerik normal hızda çalışır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Geri sar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Tuş basılı tutulurken geçerli içeriği geri sarar. 'Geri Sarma Desteği' etkin olmalıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Duraklat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Çalışan içeriği duraklatılmış ve duraklatılmamış durumlar arasında değiştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Kare İlerlemesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Duraklatıldığında içeriği bir kare ilerletir."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Sesi Kapat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Ses çıkışını açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Sesi Yükselt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Çıkış ses seviyesi seviyesini artırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Sesi Azalt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Çıkış ses seviyesi seviyesini düşürür."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Durum Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Seçili yuvadan kayıtlı bir durumu yükler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Durum Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Seçili yuvaya bir durum kayıt eder."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Sonraki Durum Kaydı Yuvası"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Seçili olan durum kaydı yuva dizinini artırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Önceki Durum Kaydı Yuvası"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Seçili olan durum kaydı yuva dizinini azaltır."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Diski Çıkar (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Sanal disk tepsisi kapalıysa, tepsiyi açar ve yüklenen diski kaldırır. Aksi takdirde, seçili olan diski yerleştirir ve tepsiyi kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Sonraki Disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Seçili olan disk indeksini artırır. Sanal disk tepsisi açık olmalıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Önceki Disk"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Seçili olan disk indeksini azaltır. Sanal disk tepsisi açık olmalıdır."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_TOGGLE,
   "Gölgelendiriciler (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_TOGGLE,
   "Seçili olan gölgelendiriciyi açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Sonraki Gölgelendirici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "'Video Gölgelendirici' dizininin kökündeki sonraki gölgelendirici hazır ayar dosyasını yükler ve uygular."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Önceki Gölgelendirici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Önceki gölgelendirici ön ayar dosyasını 'Video Gölgelendiriciler' dizininin köküne yükler ve uygular."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Hileler (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Seçili hileyi açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Sonraki Hile Dizini"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Seçili olan hile dizinini artırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Önceki Hile Dizini"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Seçili olan hile dizinini azaltır."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Ekran Görüntüsü Al"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Mevcut içeriğin bir görüntüsünü yakalar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Kayıt (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Mevcut oturumun yerel bir video dosyasına kaydını başlatır/durdurur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Akış (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Mevcut oturumun çevrimiçi bir video platformuna akışını başlatır/durdurur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
   "Kayıt Girişi Tekrarı (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_BSV_RECORD_TOGGLE,
   "Oynanış girişlerinin .bsv biçiminde kaydedilmesini açar/kapatır."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Fareyi Yakala (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Fareyi yakalar veya serbest bırakır. Yakalandığında, sistem imleci gizlenir ve RetroArch görüntüleme penceresiyle sınırlandırılarak göreceli fare girişi iyileştirilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Oyun Odağı (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "'Oyun Odağı' kipini açar/kapatır. İçerik odaklandığında, kısayol tuşları devre dışı bırakılır (tam klavye girişi çalışan çekirdeğe iletilir) ve fare yakalanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Tam Ekran (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Tam ekran ve pencereli ekran kipleri arasında geçiş yapar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Masaüstü Menüsü (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Tamamlayıcı PSMİ (Pencere, Simgeler, Menüler, İşaretçi) masaüstü kullanıcı arayüzünü açar."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Tam İçerik Kare Hızına Eşitle (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VRR_RUNLOOP_TOGGLE,
   "Tam içerik kare hızını eşitlemesini açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Önden-Git (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Önden-Git aç/kapat."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "FPS Göster (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "'Saniyedeki kare' durum göstergesini açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Teknik İstatistikleri Göster (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Ekrandaki teknik istatistiklerin görüntülenmesini açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Ekran Klavyesi (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Ekran klavyesini açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Sonraki Kaplama"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "O anda etkin olan ekran üstü bindirmenin bir sonraki kullanılabilir düzenine geçer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Çeviri Servisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Ekrandaki herhangi bir metni çevirmek ve/veya yüksek sesle okumak için mevcut içeriğin bir görüntüsünü yakalar. 'Çeviri Servisi' etkinleştirilmeli ve yapılandırılmalıdır."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Netplay Ping (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Mevcut netplay odası için ping sayacını açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Netplay Sunucu (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Netplay barındırma özelliğini açar/kapatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Netplay Oyuncu/İzleyici Kipi (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Mevcut netplay oturumunu 'oyuncu' ve 'izleyici' kipleri arasında değiştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Netplay Oyuncu Sohbeti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Geçerli netplay oturumuna bir sohbet mesajı gönderir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Netplay Sohbet Geçişini Soluklaştır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Oda ve statik netplay sohbet mesajları arasında geçiş yapın."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "Hata Ayıklama Bilgisi Gönder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SEND_DEBUG_INFO,
   "Cihazınız ve RetroArch yapılandırması hakkındaki teşhis bilgilerini analiz için sunucularımıza gönderir."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Cihaz Türü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DEVICE_TYPE,
   "Taklit edilmiş kontrolcü türünü belirtir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Analog-Dijital Türü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "D-Pad girişi için belirtilen analog çubuğu kullanın. \"Zorunlu\" kipler, ana yerel analog girişi geçersiz kılar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Cihaz İndeksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Eşlenmiş Port"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Hangi çekirdekiğin hangi port üstünden girdi alacağını belirtir %u."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Tüm Kontrolcüleri Ayarla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Varsayılan Kontrolcülere Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Kontrolcü Profilini Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Fare İndeksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "B Düğmesi (Aşağı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Y Düğmesi (Sol)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Seç Düğmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Başlat Düğmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "D-Pad Yukarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "D-Pad Aşağı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "D-Pad Sol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "D-Pad Sağ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "A Düğmesi (Sağ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "X Düğmesi (Üst)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "L Düğmesi (Omuz)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "R Düğmesi (Omuz)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "L2 Düğmesi (Tetik)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2 Düğmesi (Tetik)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "L3 Düğmesi (Başparmak)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "R3 Düğmesi (Başparmak)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Sol Analog X+ (Sağ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Sol Analog X- (Sol)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Sol Analog Y+ (Aşağı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Sol Analog Y- (Yukarı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Sağ Analog X+ (Sağ)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Sağ Analog X- (Sol)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Sağ Analog Y+ (Aşağı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Sağ Analog Y- (Yukarı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Silah Tetikleyici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Silah Değiştirici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Silah Aux A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Silah Aux B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Silah Aux C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Silah Başlat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Silah Seç"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Silah D-Pad Üst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Silah D-Pad Alt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Silah D-Pad Sol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Silah D-Pad Sağ"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Önden-Git Kullanılamıyor]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "Mevcut çekirdek, deterministik tasarruf durumu desteğinin olmaması nedeniyle önden git ile uyumlu değil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
   "Gecikmeyi Azaltmak için Önden-Git"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
   "Çekirdek mantığını bir veya daha fazla karenin önünde çalıştırın, ardından algılanan giriş gecikmesini azaltmak için durumu geri yükleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Önden Gidilecek Kare Sayısı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Önde çalıştırılacak kare sayısı. Oyunun içindeki gecikme karelerinin sayısı aşılırsa titreme gibi oyun sorunlarına neden olur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
   "Önden-Git için İkinci Örneği Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
   "Önden-Git için RetroArch çekirdeğinin ikincil örneğini kullanın. Yükleme durumu nedeniyle ses sorunlarını önler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Önden-Git Uyarılarını Gizle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Önden-Git kullanırken görüntülenen uyarı mesajını gizleyin ve çekirdek durum kaydı desteklemez."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Paylaşılan Donanım İçeriği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Donanımla oluşturulan çekirdeklere kendi özel bağlamlarını verin. Kareler arasındaki donanım durumu değişikliklerini üstlenmek zorunda kalmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Çekirdeklerin Video Sürücüsünü Değiştirmesine İzin Ver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Çekirdeklerin şu anda yüklü olandan farklı bir video sürücüsüne geçmesine izin verin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Çekirdeğin Kapanmaması İçin Kukla Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Bazı çekirdeklerin bir kapatma özelliği vardır, sahte bir çekirdek yüklemek RetroArch'ın kapanmasını önler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Çekirdeği Otomatik Başlatır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Yüklemeden Önce Eksik Ürün Yazılımını Denetle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "İçerik yüklemeyi denemeden önce gerekli tüm üretici yazılımının olup olmadığını kontrol edin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Çekirdek Seçeneği Kategorileri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Çekirdeklerin kategori tabanlı alt menülerde seçenekler sunmasına izin verin. NOT: Değişikliklerin etkili olması için çekirdek yeniden yüklenmelidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Çekirdek Bilgi Dosyaları Önbelleği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Yüklü temel bilgilerin kalıcı bir yerel önbelleğini koruyun. Yavaş disk erişimine sahip platformlarda yükleme sürelerini büyük ölçüde azaltır."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Her Çalıştırmada İçerik Çekirdeğini Yeniden Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Gerekli çekirdek yüklü olduğu halde, içerik başlatılırken RetroArch'ı yeniden başlat. Bu, yükleme süresinin uzaması uğruna, sistem kararlılığını arttırabilir."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Döndürmeye İzin Ver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Çekirdeklerin dönüş ayarlamasına izin verin. Devre dışı bırakıldığında, döndürme istekleri yok sayılır. Ekranı el ile döndüren kurulumlar için kullanışlıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Çekirdekleri Yönet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Kurulu çekirdeklerde (yedekleme, geri yükleme, silme vb.) Çevrimdışı bakım görevlerini gerçekleştirin ve temel bilgileri görüntüleyin."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Çekirdekleri Yönet"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_STEAM_LIST,
   "Steam aracılığıyla dağıtılan çekirdekleri kurun veya kaldırın."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_INSTALL,
   "Çekirdek kur"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_STEAM_UNINSTALL,
   "Çekirdek kaldır"
)

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_MANAGER_STEAM,
   "'Çekirdekleri Yöneti' Göster"
)
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_MANAGER_STEAM,
   "Ana Menüde 'Çekirdekleri Yönet' seçeneğini gösterin."
)

MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Çekirdek kuruluyor: "
)

MSG_HASH(
   MSG_CORE_STEAM_UNINSTALLED,
   "RetroArch üstünden çıkarken çekirdek kaldırılacaktır."
)

MSG_HASH(
   MSG_CORE_STEAM_CURRENTLY_DOWNLOADING,
   "Çekirdek şu anda indiriliyor"
)
#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Çıkışta Yapılandırmayı Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Çıkışta yapılandırma dosyasındaki değişiklikleri kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_SAVE_ON_EXIT,
   "Çıkışta Yeniden Eşleme Dosyalarını Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_SAVE_ON_EXIT,
   "İçeriği kapatırken veya RetroArch'tan çıkarken herhangi bir etkin giriş yeniden eşleme dosyasındaki değişiklikleri kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "İçeriğe Özgü Çekirdek Seçeneklerini Otomatik Olarak Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Özelleştirilmiş Çekirdek seçeneklerini başlangıçta varsayılan olarak yükleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Özelleştirilmiş Dosyaları Otomatik Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Özelleştirilmiş yapılandırmayı başlangıçta yükleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Yeniden Eşleşme Dosyalarını Otomatik Olarak Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Özelleştirilmiş kontrolcüleri başlangıçta yükleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Gölgelendirici Hazır Ayarlarını Otomatik Olarak Yükle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Evrensel Çekirdek Seçenekleri Dosyalarını Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Tüm temel çekirdek seçeneklerini ortak bir ayarlar dosyasına (retroarch-core-options.cfg) kaydeder. Devre dışı bırakıldığında, her çekirdek için seçenekler RetroArch'ın 'Yapılandırma' dizinindeki çekirdeğe özgü ayrı bir klasöre/dosyaya kaydedilir."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Kayıtları Klasörlere Çekirdek Adına Göre Sırala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Kayıt dosyalarını kullanılan çekirdeğin adını taşıyan klasörlere göre sıralayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Durum Kayıtlarını Çekirdek Adına Göre Klasörlere Sırala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Kullanılan çekirdeğin adını taşıyan klasörlerde durum kayıtlarını sıralayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Kayıtları İçerik Dizinine Göre Klasörlere Göre Sırala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Kayıt dosyalarını, içeriğin bulunduğu dizinin adını taşıyan klasörler halinde sıralayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Durum Kayıtlarını İçerik Dizinine Göre Klasörlere Göre Sırala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Durum kayıtlarını, içeriğin bulunduğu dizinin adını taşıyan klasörler halinde sıralayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Durum Kaydı SaveRAM Üstüne Yazılmasın"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Durum kayıtları yüklenirken SaveRAM üzerine yazılmasını engelle. Potansiyel olarak hatalı oyunlara yol açabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "SaveRAM Otomatik Kaydetme Aralığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Kalıcı olmayan SaveRAM'i düzenli aralıklarla (saniye cinsinden) otomatik olarak kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Durum Kaydı Dizini'ni Otomatik Olarak Artır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Bir durum kaydı yapmadan önce, durum kaydı dizini otomatik olarak artar. İçerik yüklerken, dizin mevcut en yüksek dizine ayarlanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Saklanacak Azami Otomatik Artımlı Durum Kayıtları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "'Durum Kaydı Dizinini Otomatik Olarak Artır' etkinleştirildiğinde oluşturulacak kaydetme durumlarının sayısını sınırlar. Yeni bir durum kaydedilirken sınır aşılırsa, en düşük indekse sahip mevcut durum silinecektir. '0' değeri, sınırsız durumların kaydedileceği anlamına gelir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Otomatik Durum Kaydı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "İçerik kapatıldığında otomatik olarak durum kaydı oluşturur. 'Durumu Otomatik Olarak Yükle' etkinse RetroArch bu durum kaydını otomatik olarak yükler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Durumu Otomatik Olarak Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Otomatik durum kaydını başlangıçta otomatik olarak yükle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Durum Kaydedici Küçük Resimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Menüde durum kayıtlarının küçük resimlerini göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "SaveRAM Sıkıştırması"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Kalıcı olmayan SaveRAM dosyalarını arşivlenmiş biçimde yazın. Artan kaydetme/yükleme süreleri (önemsizce) pahasına dosya boyutunu önemli ölçüde azaltır.\nYalnızca standart libretro SaveRAM arabirimi üzerinden kaydetmeyi sağlayan çekirdekler için geçerlidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Durum Kaydı Sıkıştırması"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Durum kaydı dosyalarını arşivlenmiş biçimde yaz. Artan kaydetme/yükleme süreleri pahasına dosya boyutunu önemli ölçüde azaltır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Ekran Görüntülerini İçerik Dizinine Göre Klasörlere Sırala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Ekran görüntülerini, içeriğin bulunduğu dizinin adını taşıyan klasörler halinde sıralayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Kayıtları İçerik Dizinine Yaz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Durum Kayıtlarını İçerik Dizinine Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Sistem Dosyalarını İçerik Dizinine Yaz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Ekran Görüntülerini İçerik Dizinine Yaz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Çalışma Günlüğünü Kaydet (Çekirdek Başına)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Her içerik ögesinin ne kadar süredir çalıştığını ve kayıtları, çekirdeklere özel ayrılmış olarak izler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Çalışma Günlüğünü Kaydet (Toplam)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Tüm çekirdeklerde toplam toplam olarak kaydedilen her içerik ögesinin ne kadar sürdüğünü takip eder."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Günlük Ayrıntılandırma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Olayları bir uçbirimde aç veya dosyaya kaydet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Ön Uç Kayıt Seviyesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Ön uç için günlük seviyesini ayarlar. Ön uç tarafından verilen bir günlük seviyesi bu değerin altındaysa, dikkate alınmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Çekirdek Günlük Seviyesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Çekirdekler için günlük seviyesini ayarlar. Çekirdek tarafından verilen günlük seviyesi bu değerin altındaysa, dikkate alınmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_DEBUG,
   "0 (Hata Ayıkla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_INFO,
   "1 (Bilgi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_WARNING,
   "2 (Uyarı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY_ERROR,
   "3 (Hata)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Dosyaya Günlükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Sistem olay günlüğü iletilerini dosyaya yönlendirir. Etkinleştirilmesi için 'Günlük Ayrıntılandırma' gerektirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Zaman Damgalı Günlük Dosyaları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Dosyaya giriş yaparken, çıktıyı her RetroArch oturumundan yeni bir zaman damgalı dosyaya yönlendirir. Devre dışı bırakılırsa, RetroArch yeniden başlatıldığında her günlüğün üzerine yazılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Performans Sayaçları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "RetroArch ve çekirdekler için performans sayaçları. Sayaç verileri, sistem darboğazlarının belirlenmesine ve performansın ince ayarının yapılmasına yardımcı olabilir."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Gizli Dosya ve Dizinleri Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Dosya tarayıcıda gizli dosyaları ve dizinleri gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Bilinmeyen Eklentileri Süzgeçle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Dosya tarayıcısında gösterilen dosyaları desteklenen uzantılara göre süzgeçle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Dahili Medya Oynatıcı Kullan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Mevcut Çekirdeğe Göre Süzgeçle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Son Kullanılan Başlangıç ​​Dizinini Hatırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Başlangıç ​​Dizininden içerik yüklerken son kullanılan konumda dosya tarayıcısını açın. Not: RetroArch yeniden başlatıldığında konum varsayılana sıfırlanacaktır."
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Geri Sar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Geri sarma ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Kare Zaman Sayacı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Kare süresi sayacını etkileyen ayarları değiştirin.\nYalnızca baskın video devre dışı bırakıldığında etkindir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Hızlı İleri Sarma Oranı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Hızlı ileri sarma kullanılırken içeriğin çalıştırılacağı azami hız (ör. 60 fps içerik için 5.0x = 300 fps kapak). 0.0x olarak ayarlanmışsa, hızlı sarma oranı sınırsızdır (FPS sınırı yok)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Hızlı İleri Sarma Kare Atlama"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Hızlı ileri sarma durumuna göre kareleri atlayın. Bu, güç tasarrufu sağlar ve 3. taraf kare sınırlamasının kullanılmasına izin verir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Ağır-Çekim Oranı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "Ağır çekim kullanılırken içeriğin oynatılma oranı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Menü Kare Hızı Sınırı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Menünün içindeyken kare hızının kapatıldığından emin olun."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Geri Sarma Desteği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Son oynanışta önceki bir noktaya dön. Bu, oynarken ciddi bir performans düşüşüne neden olur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Kareleri Geri Sar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Adım başına geri sarılacak kare sayısı. Daha yüksek değerler geri sarma hızını artırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Geri Sarma Önbelleği (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "Geri sarma arabelleği için ayrılacak bellek miktarı (MB cinsinden). Bunu artırmak, geri sarma geçmişinin miktarını artıracaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Geri Sarma Ara Belleği Aşaması (MB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Geri sarma arabellek boyutu değeri her artırıldığında veya azaldığında, bu miktarda değişecektir."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "İleri Sarmadan Sonra Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Hızlı ileri sarmadan sonra kare süresi sayacını sıfırlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Durum Yüklemesinden Sonra Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Bir durum yükledikten sonra kare süresi sayacını sıfırlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Durumu Kaydettikten Sonra Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Bir durumu kaydettikten sonra kare zaman sayacını sıfırlayın."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Kayıt Kalitesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_CUSTOM,
   "Özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOW_QUALITY,
   "Düşük"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_MED_QUALITY,
   "Orta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_HIGH_QUALITY,
   "Yüksek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_LOSSLESS_QUALITY,
   "Kayıpsız"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_FAST,
   "WebM Hızlı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_RECORDING_WEBM_HIGH_QUALITY,
   "WebM Yüksek Kalite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Özel Kayıt Yapılandırması"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Kayıt İş Parçacığı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Kayıtlarda Yazı Filtresi Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Filtreler (gölgelendirici değil) uygulandıktan sonra görüntüyü çekin. Videonuz, ekranda gördüğünüz kadar süslü görünecek."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Kayıtlarda GPU Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Varsa GPU gölgeli malzemenin çıktısını kaydeder."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Yayıncı Kipi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_LOCAL,
   "Yerel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAMING_MODE_CUSTOM,
   "Özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Yayın Kalitesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_CUSTOM,
   "Özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY,
   "Düşük"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY,
   "Orta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY,
   "Yüksek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Özel Yayın Yapılandırması"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Yayın Başlığı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "Yayın URL'si"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "UDP Yayın Portu"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Ekran Kaplaması"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Çerçeveleri ve ekran kontrollerini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Video Düzeni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Video düzenini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Ekran Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Ekran bildirimlerini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Bildirim Görünürlüğü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Belirli bildirimlerin türlerinin görünürlüğünü değiştir."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Kaplamayı Görüntüle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Kaplamalar, kenarlıklar ve ekran kontrolleri için kullanılır."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Menünün Arkasındaki Kaplamayı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Kaplamayı menünün önünde değil arkasında gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Menüde Kaplamayı Gizle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Menü içindeyken kaplamayı gizleyin ve menüden çıkarken tekrar gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Oyun Kumandası Bağlandığında Kaplamayı Gizle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Bağlantı noktası 1'e fiziksel bir oyun kumandası bağlandığında kaplamayı gizle ve oyun kumandası bağlantısı kesildiğinde tekrar göster."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Bağlantı noktası 1'e fiziksel bir oyun kumandası bağlandığında kaplamayı gizle. Oyun kumandası bağlantısı kesildiğinde yer paylaşımı otomatik olarak geri yüklenmeyecektir."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Kaplamada Girdileri Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Kayıtlı girdileri ekran üstü katman üzerinde gösterin. \"Dokunulan\", basılan/tıklanan bindirme öğelerini vurgular. 'Fiziksel (Denetleyici)', tipik olarak bağlı bir denetleyiciden/klavyeden çekirdeklere aktarılan gerçek girdiyi vurgular."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Dokunulmuş"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Fiziksel (Kontrolcü)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Port Girdilerini Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "'Kaplamada Girdileri Göster', 'Fiziksel (Kontrolcü)' olarak ayarlandığında izlenecek girdi cihazının portunu seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Fare İmlecini Kaplamalı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Ekran kaplaması kullanırken fare imlecini göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Kaplamayı Otomatik Döndür"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Mevcut kaplama tarafından destekleniyorsa, ekran yönü/en boy oranıyla eşleşmesi için düzeni otomatik olarak döndürür."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Otomatik-Öçekli Kaplama"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Kaplama ölçeğini ve arayüz ögesi aralığını ekran en boy oranına uyacak şekilde otomatik olarak ayarlayın. Denetleyici katmanları ile en iyi sonuçları verir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "D-Pad Çapraz Hassasiyeti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_DPAD_DIAGONAL_SENSITIVITY,
   "Çapraz bölgelerin boyutunu ayarlayın. 8 yönlü simetri için %100'e ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "ABXY Örtüşme Hassasiyeti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ABXY_DIAGONAL_SENSITIVITY,
   "Örtüşme hassasiyetini ayarlayın. 8 yönlü simetri için %100'e ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Kaplama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Tercih Edilen Kaplamayı Otomatik Yükle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Kaplama Şeffaflığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Kaplamanın tüm arayüz elementlerin şeffaflığı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Kaplama Ön Ayarı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Dosya tarayıcısından bir kaplama seç."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(Manzara) Kaplama Ölçeği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Yatay görüntüleme yönlerini kullanırken kaplamanın tüm arayüz ögelerinin ölçeği."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(Manzara) Kaplama En Boy Ayarı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Yatay ekran yönlerini kullanırken kaplamaya bir en boy oranı düzeltme etkeni uygulayın. Pozitif değerler, etkin kaplama genişliğini artırır (negatif değerler azalır)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Manzara) Kaplama Yatay Ayırıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Mevcut ön ayar tarafından destekleniyorsa, yatay ekran yönlerini kullanırken bir kaplamanın sol ve sağ yarısındaki arayüz ögeleri arasındaki aralığı ayarlar. Pozitif değerler iki yarının ayrılmasını artırır (negatif değerler azalır)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Manzara) Kaplama Dikey Ayırıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Mevcut ön ayar tarafından destekleniyorsa, yatay ekran yönlerini kullanırken bir kaplamanın üst ve alt yarısındaki arayüz ögeleri arasındaki aralığı ayarlar. Pozitif değerler iki yarının ayrılmasını artırır (negatif değerler azalır)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Manzara) Kaplama X Dengesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Yatay ekran yönlerini kullanırken yatay kaplama dengesi. Pozitif değerler kaplamayı sağa kaydırır; sola negatif değerler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Manzara) Kaplama Y Dengesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Dikey ekran yönlerini kullanırken yatay kaplama dengesi. Pozitif değerler kaplamayı sağa kaydırır; sola negatif değerler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(Portre) Kaplama Ölçeği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Dikey görüntüleme yönlerini kullanırken kaplamanın tüm arayüz ögelerinin ölçeği."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(Portre) Kaplama En Boy Ayarı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Dikey ekran yönlerini kullanırken kaplamaya bir en boy oranı düzeltme etkeni uygulayın. Pozitif değerler, etkin kaplama genişliğini artırır (negatif değerler azalır)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Portre) Kaplama Yatay Ayırıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Mevcut ön ayar tarafından destekleniyorsa, dikey görüntü yönlerini kullanırken bir kaplamanın sol ve sağ yarısındaki arayüz ögeleri arasındaki aralığı ayarlar. Pozitif değerler iki yarının ayrılmasını artırır (negatif değerler azalır)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Portre) Yer Paylaşımlı Dikey Ayırıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Mevcut ön ayar tarafından destekleniyorsa, dikey görüntü yönlerini kullanırken bir kaplamanın üst ve alt yarısındaki arayüz ögeleri arasındaki aralığı ayarlar. Pozitif değerler iki yarının ayrılmasını artırır (negatif değerler azalır)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Portre) Kaplama X Dengesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Dikey ekran yönlerini kullanırken yatay kaplama dengesi. Pozitif değerler kaplamayı sağa kaydırır; sola negatif değerler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Portre) Kaplama Y Dengesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Dikey görüntüleme yönlerini kullanırken dikey kaplama dengesi. Pozitif değerler üst üste gelir; negatif değerler aşağıya."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Video Düzenini Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Video düzenleri, çerçeveler ve diğer resimler için kullanılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Video Düzeni Yolu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Dosya tarayıcısından bir video düzeni seç."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Seçili Görünüm"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Yüklenen düzen içinde bir görünüm seç."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Ekran Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Ekrandaki iletileri göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Grafik Gereçleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Süslü animasyonlar, bildirimler, göstergeler ve kontrolcüler kullanın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Grafik Pencere Ögelerini Otomatik Olarak Ölçeklendir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Mevcut menü ölçeğine göre düzenlenmiş bildirimleri, göstergeleri ve kontrolcüleri otomatik olarak yeniden boyutlandır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Grafik Gereçleri Ölçeğini Özelleştir (Tam ekran)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Tam ekran kipinde ekran gereçleri çizerken el ile ölçeklendirme etkeni geçersiz kılınır. Yalnızca 'Grafik Pencere Ögelerini Otomatik Olarak Ölçeklendir' devre dışı bırakıldığında geçerlidir. Düzenlenmiş bildirimlerin, göstergelerin ve kontrolcülerin boyutunu menünün kendisinden bağımsız olarak arttırmak veya azaltmak için kullanılabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Grafik Gereçleri Ölçeğini Özelleştir (Pencereli)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Pencereli ekran kipinde ekran gereçleri çizerken el ile ölçeklendirme etkeni geçersiz kılınır. Yalnızca 'Grafik Pencere Ögelerini Otomatik Olarak Ölçeklendir' devre dışı bırakıldığında geçerlidir. Düzenlenmiş bildirimlerin, göstergelerin ve kontrolcülerin boyutunu menünün kendisinden bağımsız olarak arttırmak veya azaltmak için kullanılabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Kare Hızını Görüntüle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Saniyedeki mevcut kareleri görüntüler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Kare Hızı Güncelleme Aralığı (Kare Cinsinden)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "Ekran kare hızı ayarlanan aralıkta kare olarak güncellenir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Kare Sayısını Görüntüle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Ekrandaki mevcut kare sayısını görüntüle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "İstatistikleri Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Ekrandaki teknik istatistikleri görüntüle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Bellek Kullanımını Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Sistemde kullanılan ve toplam bellek miktarını görüntüler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Bellek Kullanımı Güncelleme Aralığı (Karelerde)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "Bellek kullanım ekranı ayarlanan aralıkta kare olarak güncellenecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Netplay Pingi Görüntüle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Geçerli netplay odası için pingi görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "\"İçerik Yükle\" Başlangıç ​​Bildirimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "İçerik yüklerken kısa bir tanıtım geri bildirimi animasyonu gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Giriş (Otomatik Yapılandırma) Bağlantı Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Hile Kodu Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Hile kodları uygulandığında bir ekran mesajı görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Yama Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "ROM'lara hafif yama uygularken bir ekran mesajı görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Giriş cihazlarını bağlarken/bağlantısını keserken bir ekran mesajı görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Giriş Yeniden Eşleme Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Giriş yeniden eşleme dosyalarını yüklerken bir ekran mesajı görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Özelleştirilmiş Yapılandırma Yükleme Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Özelleştirilmiş yapılandırma dosyalarını yüklerken bir ekran mesajı görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "İlk Disk Geri Yüklendi Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "M3U çalma listeleri aracılığıyla yüklenen son kullanılan çoklu diskin açılışında otomatik olarak geri yükleme sırasında bir ekran mesajı görüntüler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Hızlı İleri Sarma Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "İçeriği hızlı yönlendirirken bir ekran göstergesi görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Ekran Görüntüsü Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Ekran görüntüsü alırken ekranda mesaj görüntülenir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Ekran Görüntüsü Bildirimi Kalıcılığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Ekrandaki ekran görüntüsü mesajının süresini tanımlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Hızlı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Çok Hızlı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Anlık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Ekran Görüntüsü Flaş Efekti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Ekran görüntüsü alırken, ekranda istenen süre ile beyaz yanıp sönen bir efekt görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "AÇIK (Normal)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "AÇIK (Hızlı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Yenileme Hızı Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Yenileme oranını ayarlarken bir ekran mesajı görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Fazladan Netplay Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Gerekli olmayan netplay mesajlarını ekranda görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Yalnızca Menü Bildirimleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Bildirimleri yalnızca menü açıkken görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Bildirim Yazı Tipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Ekran bildirimleri için yazı tipini seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Bildirim Boyutu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Yazı tipi boyutunu nokta cinsinden belirtin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Bildirim Konumu (Yatay)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Bildirim Konumu (Dikey)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Bildirim Rengi (Kırmızı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Bildirim Rengi (Yeşil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Bildirim Rengi (Mavi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Bildirim Arkaplanı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Bildirim Arkaplan Rengi (Kırmızı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Bildirim Arkaplan Rengi (Yeşil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Bildirim Arkaplan Rengi (Mavi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Bildirim Arkaplanı Şeffaflığı"
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Menü Ögesi Görünürlüğü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "RetroArch menü ögelerinin görünürlüğünü değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Görünüm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Menü ekranı görünüm ayarlarını düzenle."
   )
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_BOTTOM_SETTINGS,
   "3DS Alt Ekran Görünümü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_BOTTOM_SETTINGS,
   "Alt ekran görünüm ayarlarını değiştirin."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Gelişmiş Ayarları Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Uzman kullanıcılar için gelişmiş ayarları göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Kiosk Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Yapılandırmayla ilgili tüm ayarları gizleyerek kurulumu korur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Kiosk Kipini Devre Dışı Bırakmak için Parola Ayarla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Kiosk kipini etkinleştirirken bir parola girilir, Ana Menü'ye gidip Kiosk Kipini Devre Dışı Bırak'ı seçip parolayı girerek daha sonra menüden devre dışı bırakmayı mümkün kılar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Gezinti Sarmalı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Liste sınırına yatay veya dikey olarak ulaşılırsa, baştan sona veya sona doğru kaydırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Menü Etkinken İçeriği Duraklat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Menü etkinse, o anda çalışan içeriği duraklatın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Durum Kayıtlarını Kullandıktan Sonra İçeriğe Devam Et"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Bir durumu kaydettikten veya yükledikten sonra menüyü otomatik olarak kapatın ve içeriği sürdürün. Bunu devre dışı bırakmak, çok yavaş cihazlarda durum kaydı performansını artırabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Diskleri Değiştirdikten Sonra İçeriğe Devam Et"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Yeni bir disk aktardıktan veya taktıktan sonra menüyü otomatik olarak kapatın ve içeriği sürdürün."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "İçeriği Kapatırken Çık"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "İçeriği kapatırken RetroArch'tan otomatik olarak çıkın. 'CLI' yalnızca içerik komut satırı aracılığıyla başlatıldığında çıkar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Menü Ekran Koruyucu Zaman Aşımı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Menü etkinken, belirtilen hareketsizlik süresinden sonra bir ekran koruyucu görüntülenecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Menü Ekran Koruyucu Animasyon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Menü ekran koruyucu etkinken bir animasyon efektini etkinleştirin. Mütevazı bir performans etkisine sahiptir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Kar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Yıldız Alanı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_VORTEX,
   "Girdap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Menü Ekran Koruyucu Animasyon Hızı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Menü ekran koruyucu animasyon efektinin hızını ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Fare Desteği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Menünün bir fare ile kontrol edilmesini sağlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Dokunmatik Desteği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Menünün dokunmatik ekranla kontrol edilmesini sağlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Baskın Görevler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Görevleri ayrı bir iş parçacığında gerçekleştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Etkin Değilken İçeriği Duraklat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "RetroArch etkin pencere olmadığında içeriği duraklat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Masaüstü Bileşimi Devre Dışı Bırak"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Pencere yöneticileri, görsel efektleri uygulamak, yanıt vermeyen pencereleri tespit etmek için diğer şeylerin yanı sıra bileşimi kullanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Menü Kaydırma Hızlandırıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Kaydırmak için bir yön tutarken imlecin azami hızı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Menü Kaydırma Gecikmesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Kaydırmak için bir yön tutarken milisaniye cinsinden ilk gecikme."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Arayüz Yardımcısı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Önyüklemede Arayüz Yardımcısını Başlat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Masaüstü Menüsü (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Başlangıçta Masaüstü Menüsünü Aç"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Hızlı Menü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Hızlı menüde bulunan menü ögelerinin görünürlüğünü değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Ayarlar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Ayarlar menüsünde menü ögelerinin görünürlüğünü değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "'Çekirdek Yükle'yi Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Ana Menüde 'Çekirdek Yükle' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "'İçerik Yükle'yi Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Ana Menüde 'İçerik Yükle' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "'Disk Yükle'yi Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Ana Menüde 'Disk Yükle' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "'Diskten Aktar'ı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Ana Menüde 'Diskten Aktar' seçeneğini gösterin."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "'Disk Çıkar'ı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Ana Menüde 'Diski Çıkar' seçeneğini gösterin."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "'Çevrimiçi Güncelleyici'yi Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Ana Menüde 'Çevrimiçi Güncelleyici' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "'Çekirdek İndirmeyi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Çekirdekleri (ve temel bilgi dosyalarını) 'Çevrimiçi Güncelleyici' seçeneğinde gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Eski 'Küçük Resim Güncelleyiciyi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Eski küçük resim paketlerini indirmek için girişi 'Çevrimiçi Güncelleyici' seçeneğinde gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "'Bilgileri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Ana Menüde 'Bilgi' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "'Yapılandırma Dosyası'nı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Ana Menüde 'Yapılandırma Dosyası' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "'Yardım'ı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Ana Menüde 'Yardım' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "'RetroArch Çıkışı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Ana Menüde 'RetroArch Çıkış' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "'RetroArch Yeniden Başlat'ı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Ana Menüde 'RetroArch Yeniden Başlat' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "'Ayarlar'ı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "'Ayarlar' menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "'Ayarları' Etkinleştirmek için Parola Ayarla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Ayarlar sekmesini gizlerken bir şifre sağlar, Ana Menü sekmesine gidip, 'Ayarlar Sekmesini Etkinleştir' seçip parola girerek, menüden daha sonra geri yüklemeyi mümkün kılar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "'Sık Kullanılanları' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Sık Kullanılanlar' menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "'Resimleri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "'Resimler' menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "'Müzikleri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "'Müzikler' menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "'Videoyu' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "'Video' menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "'Netplay' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "'Netplay' menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "'Geçmişi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Geçmiş menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "'İçeriği İçe Aktarı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "'İçeriği İçe Aktar' menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH( /* FIXME can now be replaced with MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD */
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "'İçeriği İçe Aktarı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Ana menü veya oynatma listeleri alt menüsünde bir 'İçeriği İçe Aktar' girişi gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Ana Menü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Oynatma Listeleri Menüsü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "'Oynatma Listelerini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "'Oynatma Listesi' menüsünü gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "'Gezgini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "İçerik gezgini seçeneğini gösterin. (Ozon/XMB'de Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "'İçeriksiz Çekirdekleri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_CONTENTLESS_CORES,
   "'İçeriksiz Çekirdekler' menüsünde gösterilecek çekirdek türünü (varsa) belirtin. 'Özel' olarak ayarlandığında, bireysel çekirdek görünürlüğü 'Çekirdekleri Yönet' menüsü aracılığıyla değiştirilebilir. (Ozone/XMB Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Tümü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Tek Kullanımlık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Tarih ve Saati Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Menü içindeki geçerli tarihi ve/veya saati gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Tarih ve Saat Görünümü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Menü içindeki tarih ve/veya saatin görünümünü değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Tarih Ayırıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Mevcut tarih menünün içinde görüntülendiğinde yıl/ay/gün bileşenleri arasında ayırıcı olarak kullanılacak karakteri belirtir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Pil Seviyesini Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Menü içindeki mevcut pil seviyesini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Çekirdek İsmini Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Mevcut çekirdek adını menü içinde göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Menü Alt Etiketlerini Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Menü ögeleri için ek bilgileri gösterin."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Başlangıç Ekranını Görüntüle"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Menüde başlangıç ekranını göster. Bu program ilk kez başladıktan sonra otomatik olarak kapalı değerine ayarlanır."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "'Devam Eti' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Devam et seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "'Yeniden Başlatı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Yeniden başlat seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "'İçeriği Kapatı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "İçeriği kapat seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Durumu Kaydı Alt Menüsünü Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVESTATE_SUBMENU,
   "Bir alt menüde durum kaydı seçeneklerini göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "'Durum Kaydet/Yükle' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Durum kaydetme/yükleme seçeneklerini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "'Durum Kaydetme/Yükleme Geri Alı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Durum kaydetme/yükleme geri alma seçeneklerini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "'Çekirdek Seçeneklerini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "'Çekirdek Seçenekleri' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "'Diske Yazma Seçeneklerini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "'Diske Yazma Seçenekleri' menüsü girişi 'Seçenekler > Çekirdek Seçeneklerini Yönet' menüsü içinde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "'Kontrolcüleri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "'Kontroller' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "'Ekran Görüntüsü Almayı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "'Ekran Görüntüsü Al' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "'Ekran Kaydı Başlatmayı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "'Kaydı Başlat' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "'Yayın Başlatı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "'Yayın Başlat' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "'Ekran Üstü Yer Paylaşımını' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "'Ekran Üstü Yer Paylaşımı' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "'Video Düzenini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "'Video Düzeni' seçeneğini göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "'Gecikmeyi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "'Gecikme' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "'Geri Sarı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "'Geri Sar' seçeneklerini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "'Çekirdeği Geçersiz Kılmaları Kaydeti' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "'Geçersiz Kıl' menüsünde 'Çekirdeği Geçersiz Kılmaları Kaydet' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "'Oyun Geçersiz Kılmalarını Kaydeti' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "'Geçersiz Kıl' menüsünde 'Oyun Geçersiz Kılmalarını Kaydet' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "'Hileleri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "'Hileler' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "'Gölgelendiricileri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "'Gölgelendiriciler' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "'Sık Kullanılanlara Ekleyi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "'Sık Kullanılanlara Ekle' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "'Çekirdek İlişkilendirmeyi Ayarlayı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "İçerik çalışmıyorken 'Çekirdek İlişkilendirmeyi Ayarla' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "'Çekirdek İlişkilendirmesini Sıfırlayı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "İçerik çalışmıyorken 'Çekirdek İlişkilendirmeyi Sıfırla' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "'Küçük Resimleri İndiri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "İçerik çalışmıyorken 'Küçük Resimleri İndir' seçeneğini gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "'Bilgileri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "'Bilgi' seçeneğini gösterin."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "'Sürücüleri' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "'Sürücü' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "'Videoyu' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "'Video' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "'Sesi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "'Ses' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "'Girişi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "'Giriş' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "'Gecikmeyi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "'Gecikme' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "'Çekirdeği' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "'Çekirdek' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "'Yapılandırmayı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "'Yapılandırma' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "'Kaydetmeyi' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "'Kaydediliyor' ayarlarını göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "'Günlüğü' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "'Günlük Kaydı' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "'Dosya Tarayıcısını' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "'Dosya Tarayıcısı' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "'Kare Sınırını' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "'Kare Sınırlayıcı' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "\"Kayıt Özelliğini\" göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "'Kayıt' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "'Ekran Görünümü' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "'Ekran Görünümü' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "'Kullanıcı Arayüzünü' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "'Kullanıcı Arayüzü' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "'Çeviri Servisini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "'Çeviri Hizmeti' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "'Erişilebilirliği' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "'Erişebilirlik' Ayarlarını Göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "'Güç Yönetimini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "'Güç Yönetimi' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "'Başarıları' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "'Başarılar' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "'Ağı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "'Ağ' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "'Oynatma Listelerini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "'Oynatma Listeleri' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "'Kullanıcıyı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "'Kullanıcı' ayarlarını gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "'Dizini' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "'Dizin' ayarlarını gösterin."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_STEAM,
   "'Steam'i Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_STEAM,
   "'Steam' ayarlarını göster."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Menü Ölçeklendirme Etkeni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Menüdeki kullanıcı ara yüzü ögelerinin boyutunu ölçekleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Arkaplan Resmi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Menü arka planı olarak ayarlamak için bir resim seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Arkaplan Saydamlığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Arka plan resminin şeffaflığını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Kare Arabellek Saydamlığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Kare arabellek saydamlığını değiştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Tercih Edilen Sistem Renk Temasını Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "İşletim sisteminin renk temasını kullanın (varsa). Tema ayarlarını geçersiz kılar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Küçük Resimler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Görüntülenecek küçük resim türü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Küçük Resim Yükseltme Eşiği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Belirtilen değerden daha küçük bir genişlik/yüksekliğe sahip küçük resim görüntülerini otomatik olarak yükseltin. Görüntü kalitesini artırır. Orta düzeyde performansa eksi etkisi vardır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Kayan Yazı Animasyonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Uzun menü metnini görüntülemek için kullanılan yatay kaydırma yöntemini seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Kayan Metin Hızı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Uzun menü metnini kaydırırken animasyon hızı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Düzgün Kayan Metin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Uzun menü metnini görüntülerken yumuşak kaydırma animasyonunu kullanın. Küçük bir eksi performans etkisi vardır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION,
   "Sekmeleri Değiştirirken Seçimi Hatırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_REMEMBER_SELECTION,
   "Farklı bir sekmeye geçerken menüdeki imlecin konumunu hatırlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_ALWAYS,
   "Daima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_PLAYLISTS,
   "Sadece Oynatma Listeleri İçin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_REMEMBER_SELECTION_MAIN,
   "Sadece Ana Menü ve Ayarlar İçin"
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Çeviri Servisi Çıkışı"
   )
MSG_HASH( /* FIXME What does the Narrator mode do? */
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Çeviriyi metin kaplama olarak gösterin (Resim Kipi) veya Metinden Konuşmaya (Konuşma Kipi) olarak oynatın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "Çeviri Servisi URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Kullanılacak çeviri hizmetini gösteren bir http:// url'si."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Çeviri Servisini Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Çeviri Servisi kısayol tuşuna basıldığında Çeviri Servisini etkinleştir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Çeviri Sırasında Duraklat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Ekran tercüme edilirken çekirdek duraklatılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Kaynak Dil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "Servisin çevireceği dil. 'Varsayılan' olarak ayarlanırsa, dili otomatik olarak algılamaya çalışır. Belli bir dile ayarlamak çeviriyi daha doğru hale getirecek."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Hedef Dil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "Hizmetin çevireceği dil. \"Varsayılan\" İngilizcedir."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Erişilebilirliği Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Menü gezintisine yardımcı olmak için Metin-Konuşma özelliğini etkinleştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Metin Okuma Hızı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Metin-Konuşma sesi için hız."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Başarılar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Klasik oyunlarda başarılar kazanın. Daha fazla bilgi için 'https://retroachievements.org' adresini ziyaret edin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Zorlu Kip"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Hileleri, geri sarmayı, ağır çekimi ve durum kaydı yüklenmesini devre dışı bırakır. Zorlu kipte kazanılan başarılar benzersiz bir şekilde işaretlenir, böylece başkalarına yardım özellikleri olmadan neler başardığınızı gösterebilirsiniz. Oyun çalıştığı zaman bu ayarı değiştirmek, oyunu yeniden başlatır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Lider Tabloları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
   "Oyuna özel skor tabloları. 'Zorlu Kip' devre dışı bırakıldığında hiçbir etkisi yoktur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "Zengin İçerik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Bağlamsal oyun bilgilerini düzenli olarak RetroAchievements web sitesine gönderir. 'Zorlu Kip' etkinleştirildiğinde hiçbir etkisi yoktur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Başarı Rozetleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Başarı listesindeki rozetleri görüntüle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Resmi Olmayan Başarıları Test Et"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Test amaçlı resmi olmayan başarıları veya deneme özelliklerini kullan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Ses Kilidini Aç"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Bir başarının kilidi açıldığında bir ses çalın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Ayrıntılı Kip"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Bildirimlerde daha fazla bilgi göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Otomatik Ekran Görüntüsü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Bir başarı kazanıldığında otomatik olarak bir ekran görüntüsü alın."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Tekrar Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Oturumu tüm başarılar etkin olarak başlatın (daha önce kilidi açılmış olanlar dahil)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Meydan Okuma Göstergeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Başarı kazanılırken başarıların ekranda bir gösterge göstermesine izin verir."
   )

/* Settings > Achievements > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_SETTINGS,
   "Görünüm"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_SETTINGS,
   "Ekrandaki başarı bildirimlerinin konumunu ve dengelerini değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR,
   "Pozisyon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_ANCHOR,
   "Başarı bildirimlerinin görüneceği ekranın köşesini/kenarını ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPLEFT,
   "Sol Üst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPCENTER,
   "Üst Merkez"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_TOPRIGHT,
   "Sağ Üst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMLEFT,
   "Sol Alt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMCENTER,
   "Alt Merkez"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_ANCHOR_BOTTOMRIGHT,
   "Sağ Alt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Hizalanmış Dolgu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_AUTO,
   "Başarı bildirimlerinin diğer ekran bildirimleriyle uyumlu olup olmayacağını ayarlayın. El ile doldurma/konum değerlerini ayarlamak için devre dışı bırakın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_H,
   "El İle Yatay Dolgu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_H,
   "Ekranın aşırı taramasını telafi edebilen sol/sağ ekran kenarından mesafe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_APPEARANCE_PADDING_V,
   "El İle Dikey Dolgu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_APPEARANCE_PADDING_V,
   "Ekranın aşırı taramasını telafi edebilen üst/alt ekran kenarına olan mesafe."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Netplay Herkese Açık"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Netplay oturumunda oyunları kimler görebilsin. Oynadığınız oyun genel sunucu odaları üstüne görünür ve oyuna herkes katılabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Aktarma Sunucusu Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Ara sunucu üzerinden netplay bağlantılarını ilet. Ana bilgisayar güvenlik duvarının arkasında veya NAT/UPnP sorunları varsa kullanışlıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Aktarma Sunucusu Konumu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Kullanmak için belirli bir aktarma sunucusu seçin. Coğrafi olarak daha yakın yerler daha az gecikme eğilimindedir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Özel Geçiş Sunucusu Adresi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Özel geçiş sunucunuzun adresini buraya girin. Biçim: adres veya adres|port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_1,
   "Kuzey Amerika (Doğu Kıyısı, ABD)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_2,
   "Batı Avrupa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_3,
   "Güney Amerika (Güneydoğu, Brezilya)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_4,
   "Güneydoğu Asya"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER_LOCATION_CUSTOM,
   "Özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Sunucu Adresi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Bağlanılacak ana bilgisayarın adresi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Netplay TCP Portu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Ana bilgisayar IP adresinin bağlantı noktası. Bir TCP veya UDP bağlantı noktası olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Azami Eş Zamanlı Bağlantılar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Yenilerini reddetmeden önce ana bilgisayarın kabul edeceği en fazla etkin bağlantı sayısı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Ping Sınırlayıcı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "Ana bilgisayarın kabul edeceği en fazla bağlantı gecikmesi (ping). Limitsiz olarak 0'a ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Sunucu Parolası"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Ana bilgisayara bağlanan istemciler tarafından kullanılacak parola."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Sunucu İzleyici Parolası"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Ana bilgisayara izleyici olarak bağlanan istemciler tarafından kullanılacak parola."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Netplay İzleyici Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Netplay'i izleyici kipinde başlat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Soluk Sohbet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Sohbet mesajlarını zamanla soldurun."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_NAME,
   "Sohbet Rengi (Rumuz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_NAME,
   "Biçim: #RRGGBB yada RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHAT_COLOR_MSG,
   "Sohbet Rengi (Mesajlar)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHAT_COLOR_MSG,
   "Biçim: #RRGGBB yada RRGGBB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Duraklatmaya İzin Ver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Oyuncuların netplay sırasında duraklamasına izin verin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Bağımlı Kip İstemcilerine İzin Ver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Bağımlı kipte bağlantılara izin ver. Bağımlı kip istemcileri her iki tarafta çok az işlem gücü gerektirir, ancak ağ gecikmesinden önemli ölçüde etkilenir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Bağımlı Kip İstemcilerine İzin Verme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Bağımlı kipte olmayan bağlantılara izin verme. Çok zayıf makineleri olan çok hızlı ağlar dışında önerilmez."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Netplay Karelerini Denetle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "Netplay ana bilgisayarın ve istemcinin eşitliğini doğrulayacağı frekans (kare cinsinden)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Giriş Gecikme Kareleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Ağ gecikmesini gizlemek için ve netplay için kullanılacak giriş gecikmesi karelerinin sayısı. Belirgin giriş gecikmesi pahasına titreşimi azaltır ve netplay'i daha az CPU kullanımına yoğunlaştırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Giriş Gecikme Kareleri Aralığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Ağ gecikmesini gizlemek için ve netplay için kullanılacak giriş gecikmesi karelerinin oranı. Belirgin giriş gecikmesi pahasına titreşimi azaltır ve netplay'i daha az CPU kullanımına yoğunlaştırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Netplay NAT Geçiş"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Barındırırken, LAN'lardan kaçmak için UPnP veya benzeri teknolojileri kullanarak genel İnternet'ten bağlantıları dinlemeye çalışın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Dijital Giriş Paylaşımı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Cihaz İsteği %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Verilen giriş cihazıyla oynama isteği."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Ağ Komutları"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Ağ Komutları Portu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "Ağ RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Ağ RetroPad Temel Port"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Kullanıcı %d Ağ RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "stdin Komutları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "stdin komut arayüzü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "İsteğe Bağlı Küçük Resim İndirmeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Oynatma listelerine göz atarken eksik küçük resimleri otomatik olarak indir. Ciddi bir eksi performans etkisi vardır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Güncelleyici"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Buildbot Çekirdek URL'si"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "Libretro buildbot'taki temel güncelleyici dizinine URL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Buildbot İçerikleri URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "Libretro buildbot üstündeki içerik güncelleme dizini URL."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "İndirilen Arşivi Otomatik Olarak Çıkart"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "İndirdikten sonra, indirilen arşivlerde bulunan dosyaları otomatik olarak çıkart."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Deneysel Çekirdekleri Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Çekirdek İndirme listesine 'deneysel' çekirdekleri ekle. Bunlar genellikle geliştirme/deneme amaçlıdır ve genel kullanım için önerilmez."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Güncellemede Çekirdekleri Yedekle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Çevrimiçi güncelleme yaparken kurulu çekirdeklerin otomatik olarak bir yedeğini oluşturun. Bir güncelleme bir gerileme getiriyorsa, çalışan bir çekirdeğe kolayca geri dönmeyi sağlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Çekirdek Yedeği Geçmiş Boyutu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Kurulu her çekirdek için kaç tane otomatik olarak oluşturulan yedek tutulacağını belirtir. Bu sınıra ulaşıldığında, çevrimiçi güncelleme yoluyla yeni bir yedekleme oluşturmak en eski yedeği siler. El ile çekirdek yedeklemek bu ayardan etkilenmez."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Geçmiş"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Son kullanılan oyunların, resimlerin, müziklerin ve videoların oynatma listesini tutar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Geçmiş Boyutu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Oyunlar, resimler, müzik ve videolar için son oynatma listesindeki girdi sayısını sınırlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Sık Kullanılanlar Boyutu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Sık kullanılanlar oynatma listesindeki girdi sayısını sınırlayın. Sınıra ulaşıldığında, eski girdiler kaldırılıncaya kadar yeni eklemeler engellenir. -1 değerinin ayarlanması 'sınırsız' girdiye izin verir.\nUYARI: Değerin düşürülmesi mevcut girdileri siler!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Girdileri Yeniden Adlandırmaya İzin Ver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Oynatma listesi girişlerinin yeniden adlandırılmasına izin verin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Girdileri Kaldırmaya İzin Ver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Oynatma listesi girişlerinin kaldırılmasına izin verin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Oynatma Listelerini Alfabetik Olarak Sırala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "'Geçmiş', 'Görüntüler', 'Müzik' ve 'Videolar' oynatma listeleri hariç, içerik oynatma listelerini alfabetik sırayla sıralar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Oynatma Listelerini Eski Biçimde Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Düz metin biçimini kullanarak oynatma listeleri yazın. Devre dışı bırakıldığında, oynatma listeleri JSON kullanılarak biçimlendirilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Oynatma Listelerini Sıkıştır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Diske yazarken oynatma listesi verilerini arşivleyin. Artırılmış CPU kullanımı (göz ardı edilebilir) pahasına dosya boyutunu ve yükleme sürelerini azaltır. Eski veya yeni biçimli oynatma listeleriyle kullanılabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Oynatma Listelerinde İlişkili Çekirdekleri Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Oynatma listesi girdilerinde ilişkilendirilmiş çekirdek (varsa) ne zaman etiketleneceğini belirle.\nNOT: Oynatma listesi alt etiketleri etkinleştirildiğinde bu ayar dikkate alınmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Oynatma Listesi Alt Etiketlerini Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Her bir oynatma listesi girdisi için mevcut çekirdek ilişkilendirme ve çalışma zamanı (varsa) gibi ek bilgileri gösterir. Değişken performans etkisi vardır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "İçeriğe Özgü Simgeleri Geçmiş ve Sık Kullanılanlarda Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Her geçmiş ve sık kullanılan oynatma listesi girişi için belirli simgeleri gösterin. Değişken bir performans etkisine sahiptir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Çekirdek:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Çalışma Süresi:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Son Oynama:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_SINGLE,
   "saniye"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_SECONDS_PLURAL,
   "saniye"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_SINGLE,
   "dakika"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MINUTES_PLURAL,
   "dakika"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_SINGLE,
   "saat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_HOURS_PLURAL,
   "saat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_SINGLE,
   "gün"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_DAYS_PLURAL,
   "gün"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_SINGLE,
   "hafta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_WEEKS_PLURAL,
   "hafta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_SINGLE,
   "ay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_MONTHS_PLURAL,
   "ay"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_SINGLE,
   "yıl"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_YEARS_PLURAL,
   "yıl"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIME_UNIT_AGO,
   "önce"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Oynatma Listesi Giriş Dizinini Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Oynatma listelerini görüntülerken giriş numaralarını gösterin. Görüntü biçimi, seçili olan menü sürücüsüne bağlıdır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Oynatma Listesi Alt Etiket Çalışma Zamanı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Oynatma listesi alt etiketlerinde hangi günlük kaydının görüntüleneceğini seçer.\nİlgili çalışma zamanı günlüğünün 'Durum Kaydı' seçenekler menüsünden etkinleştirilmesi gerektiğini unutmayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "'Son Oynanan' Tarih ve Saat Biçimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "'Son Oynanan' zaman damgası bilgileri için görüntülenen tarih ve saatin şeklini ayarlar. '(ÖÖ/ÖS)' seçeneklerinin bazı platformlar üzerinde küçük bir eksi performans etkisi olacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Bozuk Arşivi Eşleştirme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Sıkıştırılmış dosyalarla ilişkili girdiler için oynatma listelerini ararken, [dosya adı] + [içerik] yerine yalnızca arşiv dosya adıyla eşleştirin. Sıkıştırılmış dosyaları yüklerken yinelenen içerik geçmişi girdilerini önlemek için bunu etkinleştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Çekirdek Eşleşmesi Olmadan Tara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "İçeriğin, onu destekleyen bir çekirdek yüklenmeden taranmasına ve bir oynatma listesine eklenmesine izin verin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Oynatma Listelerini Yönet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Oynatma listelerinde bakım görevleri gerçekleştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Taşınabilir Oynatma Listeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Etkinleştirildiğinde ve 'Dosya Tarayıcısı' dizini de seçildiğinde, 'Dosya Tarayıcısı' parametresinin mevcut değeri oynatma listesine kaydedilir. Oynatma listesi, aynı seçeneğin etkinleştirildiği başka bir sisteme yüklendiğinde, 'Dosya Tarayıcısı' parametresinin değeri oynatma listesi değeri ile karşılaştırılır; farklıysa, oynatma listesi girdilerinin yolları otomatik olarak sabitlenir."
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Varsayılan Çekirdek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Mevcut bir çekirdek ilişkisine sahip olmayan, oynatma listesi girdisi yoluyla içerik başlatırken kullanılacak çekirdeği belirtin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Çekirdek İlişkilendirmeleri Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Tüm oynatma listesi girdiler için mevcut çekirdek ilişkilendirmeleri kaldırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Etiket Görüntüleme Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "İçerik etiketlerinin bu oynatma listesinde nasıl görüntülendiğini değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Sıralama Yöntemi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Bu oynatma listesindeki girdilerin nasıl sıralanacağını belirler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Oynatma Listesini Temizle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Çekirdek ilişkilendirmeleri doğrular ve geçersiz ve yinelenen girdileri kaldırır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Oynatma Listesini Yenile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Oynatma listesini oluşturmak veya düzenlemek için en son kullanılan 'El İle Tarama' işlemini tekrarlayarak yeni içerik ekleyin ve geçersiz girişleri kaldırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Oynatma Listesini Sil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Oynatma listesini dosya sisteminden kaldır."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Gizlilik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Gizlilik ayarlarını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Hesaplar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Mevcut yapılandırılmış hesapları yönetin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Kullanıcı Adı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Kullanıcı adınızı buraya girin. Diğer işlemlerin yanı sıra Netplay oturumları için de kullanılacak."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Dil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Kullanıcı ara yüzü dilini ayarlayın."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Kameraya İzin Ver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Çekirdeklerin kameraya erişmesine izin verin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Discord Zengin İçerik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Discord uygulamasının oynatılan içerikle ilgili verileri göstermesine izin ver.\nYalnızca yerel masaüstü istemcisinde kullanılabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Konuma İzin Ver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Çekirdeklerin konumunuza erişmesine izin verin."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Klasik oyunlarda başarılar kazanın. Daha fazla bilgi için 'https://retroachievements.org' adresini ziyaret edin."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Kullanıcı adı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "RetroAchievements hesabı kullanıcı adını girin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Parola"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "RetroAchievements hesabınızın parolasını girin. Azami uzunluk: 255 karakter."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "YouTube Yayıncı Anahtarı"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Twitch Yayıncı Anahtarı"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Facebook Gaming Akış Anahtarı"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Sistem/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOS'lar, önyükleme ROM'ları ve diğer sisteme özgü dosyalar bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "İndirilenler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "İndirilen dosyalar bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "İçerikler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "RetroArch tarafından kullanılan menü içerikleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Dinamik Arkaplanlar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Menüde kullanılan arka plan resimleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Küçük Resimler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Kapak resmi, ekran görüntüsü ve başlık ekranı küçük resimleri bu dizinde saklanır."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Dosya Tarayıcısı"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Dosya tarayıcısı için başlangıç ​​dizinini ayarlayın."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Yapılandırma"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Menü yapılandırma tarayıcısı için başlangıç dizinini ayarlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Çekirdekler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Libretro çekirdekleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Çekirdek Bilgisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Uygulama/çekirdek bilgi dosyaları bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Veritabanları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Veritabanları bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Hile Dosyaları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Hile dosyaları bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Video Filtreleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "CPU tabanlı video gölgelendiriciler bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Ses Filtreleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Ses DSP filtreleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Video Gölgelendiricileri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "GPU tabanlı video gölgelendiriciler bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Ekran Kayıtları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Kayıtlar bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Kayıt Yapılandırmaları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Kayıt yapılandırmaları bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Kaplamalar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Kaplamalar bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Video Düzenleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Video düzenleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Ekran Görüntüleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Ekran görüntüleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Kontrolcü Profilleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Denetleyicileri otomatik olarak yapılandırmak için kullanılan denetleyici profilleri bu dizinde depolanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Giriş Yeniden Eşlemeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Girdi eşlemeleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Oynatma Listeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Oynatma listeleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Sık Kullanılanlar Oynatma Listesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Sık kullanılanlar oynatma listesini bu dizine kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Geçmiş Oynatma Listesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Geçmiş oynatma listesini bu dizine kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Resimler Oynatma Listesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Resimler oynatma listesini bu dizine kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Müzik Oynatma Listesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Müzik oynatma listesini bu dizine kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Video Oynatma Listesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Video oynatma listesini bu dizine kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Çalışma Zamanı Günlük"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Çalışma zamanı günlükleri bu dizinde saklanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Dosyaları Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Tüm kayıt dosyalarını bu dizine kaydedin. Ayarlanmamışsa, içerik dosyasının çalışma dizinine kaydetmeye çalışır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Durumları Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Durum kayıtları bu dizinde saklanır. Ayarlanmazsa, bunları içeriğin bulunduğu dizine kaydetmeye çalışır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Önbellek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Arşivlenen içerik geçici olarak bu dizine çıkarılacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Sistem Olay Günlükleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Sistem olay günlükleri bu dizinde saklanır."
   )

#ifdef HAVE_MIST
/* Settings > Steam */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_ENABLE,
   "Gelişmiş Durumu Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_ENABLE,
   "Mevcut durumunuzu Steam RetroArch üstünde paylaşın."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT,
   "Gelişmiş Durum İçeriği Biçimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STEAM_RICH_PRESENCE_FORMAT,
   "Çalışan içerikle ilgili hangi bilgilerin paylaşılacağına karar verin."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT,
   "İçerik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CORE,
   "Çekirdek adı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_SYSTEM,
   "Sistem adı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM,
   "İçerik (Sistem adı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_CORE,
   "İçerik (Çekirdek adı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STEAM_RICH_PRESENCE_FORMAT_CONTENT_SYSTEM_CORE,
   "İçerik (Sistem adı - Çekirdek adı)"
   )
#endif

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Karıştırıcıya Ekle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Ses parçasını kullanılabilir bir ses akışı yuvasına ekleyin.\nŞu anda mevcut yuva bulunmuyorsa, dikkate alınmaz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Karıştırıcıya Ekle ve Oynat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Ses parçasını kullanılabilir bir ses akışı yuvasına ekleyin ve oynatın.\nŞu anda mevcut yuva bulunmuyorsa, dikkate alınmaz."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Ana Bilgisayar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Netplay Sunucusuna Bağlan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Netplay sunucu adresini gir ve istemci kipinde bağlan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Netplay Sunucusuyla Bağlantıyı Kes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Etkin bir Netplay bağlantısını kesin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOBBY_FILTERS,
   "Oda Filtreleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Sadece Bağlanılabilir Odalar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_INSTALLED_CORES,
   "Sadece Kurulu Çekirdekler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_PASSWORDED,
   "Parolalı Odalar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Netplay Sunucu Listesini Yenile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Netplay sunucularını tarayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Netplay LAN Listesini Yenile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "LAN bağlantısında netplay ana bilgisayarlarını tarayın."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Netplay Sunucusu Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Netplay'i ana bilgisayar (sunucu) kipinde başlatın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Netplay Sunucusu Durdur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_KICK,
   "İstemciyi At"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_KICK,
   "Şu anda barındırılan odanızdan bir oyuncuyu atın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_BAN,
   "İstemciyi Yasakla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_BAN,
   "Şu anda barındırılan odanızdan bir oyuncuyu yasaklayın."
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Dizin Tara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Uyumlu içerik için bir dizin tarar. Bulunduğunda, içerik oynatma listesine eklenir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Bu Dizini Tara>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Dosya Tara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Uyumlu içerik için bir dosyayı tarar. Bulunduğunda, içerik oynatma listesine eklenir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "El İle Tara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "İçerik dosyası adlarına dayalı yapılandırılabilir tarama türü. Veritabanına uygun içerik gerektirmez."
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Karıştırıcıya Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Karıştırıcıya Ekle ve Oynat"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "İçerik Dizini"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "İçeriği taramak için bir dizin seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Sistem Adı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Taranan içeriğin ilişkilendirileceği bir 'sistem adı' belirtin. Oluşturulan oynatma listesi dosyasına ad vermek ve oynatma listesi küçük resimlerini tanımlamak için kullanılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Özel Sistem Adı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Taranan içerik için kendiniz bir 'sistem adı' belirtin. Yalnızca 'Sistem Adı' '<Özel>' olarak ayarlandığında kullanılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Varsayılan Çekirdek"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Taranan içeriği başlatırken kullanılacak varsayılan bir çekirdek seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Dosya Uzantıları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Boşluğa ayrılmış olarak taramaya dahil edilecek dosya türlerinin listesi. Boşsa, tüm dosya türlerini içerir veya bir çekirdek belirtilirse, çekirdek tarafından desteklenen tüm dosyalar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Yinelemeli Tara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Etkinleştirildiğinde, belirtilen 'İçerik Dizini' tüm alt dizinleri taramaya dahil edilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "İç Arşivleri Tara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Etkinleştirildiğinde, arşiv dosyalarında (.zip, .7z, vb.) mevcut/desteklenen içerik aranacaktır. Tarama performansı üzerinde önemli bir etkisi olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Arcade DAT Dosyası"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Taranan içeriğin (MAME, FinalBurn Neo, vb.) otomatik adlandırılması için Logiqx veya MAME Listesi XML DAT dosyası seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Arcade DAT Filtresi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Bir arcade DAT dosyası kullanırken, içerik sadece eşleşen bir DAT dosyası girişi bulunursa oynatma listesine eklenecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Oynatma Listesinin Üzerine Yaz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Etkinleştirildiğinde, içerik taramadan önce mevcut tüm oynatma listeleri silinir. Devre dışı bırakıldığında, mevcut oynatma listesi girdileri korunur ve yalnızca oynatma listesinden eksik olan içerik eklenir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Mevcut Girdileri Doğrula"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Etkinleştirildiğinde, yeni içerik taranmadan önce mevcut herhangi bir oynatma listesindeki girdiler doğrulanacaktır. Eksik içeriğe ve/veya geçersiz uzantılara sahip dosyalara atıfta bulunan girdiler kaldırılacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Taramayı Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Seçilen içeriği tarayın."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Liste başlatılıyor..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Çıkış Yılı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Oyuncu Sayısı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Bölge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Etiket"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "İsim Ara..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Tümünü Göster"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Ek Filtre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Tümü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Ek Filtre Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u Öge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Geliştirici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Yayıncı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Çıkış Yılı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Oyuncu Sayısı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Tür"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ACHIEVEMENTS,
   "Başarılara Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CATEGORY,
   "Kategoriye Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_LANGUAGE,
   "Dile Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Bölge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONSOLE_EXCLUSIVE,
   "Konsola Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLATFORM_EXCLUSIVE,
   "Platforma Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RUMBLE,
   "Titreşime Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SCORE,
   "Puana Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_MEDIA,
   "Medyaya Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_CONTROLS,
   "Kontrolcülere Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ARTSTYLE,
   "Sanat Tarzına Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GAMEPLAY,
   "Oynanışa Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_NARRATIVE,
   "Hikayeye Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PACING,
   "İlerlemeye Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PERSPECTIVE,
   "Perspektife Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SETTING,
   "Ayara Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VISUAL,
   "Görsele Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_VEHICULAR,
   "Taşıta Göre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Menşei"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "İmtiyaz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Etiket"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Sistem Adı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_RANGE_FILTER,
   "Aralık Filtresini Ayarla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW,
   "Görünüm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_SAVE_VIEW,
   "Görünümü Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_DELETE_VIEW,
   "Bu Görünümü Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_NEW_VIEW,
   "Yeni görünümün adını yazın"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_EXISTS,
   "Aynı isimde görünüm zaten bulunuyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_SAVED,
   "Görünüm kaydedildi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_EXPLORE_VIEW_DELETED,
   "Görünüm silindi"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Çalıştır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "İçeriği çalıştır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Adlandır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Başlığı yeniden adlandır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Bu içeriği oynatma listesinden kaldır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Sık Kullanılanlara Ekle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "İçeriği 'Sık Kullanılanlara' ekleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Çekirdek Eşleşmesi Ayarla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SET_CORE_ASSOCIATION,
   "Bu içerikle ilişkili çekirdeği ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Çekirdek Eşleşmesini Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_CORE_ASSOCIATION,
   "Bu içerikle ilişkili çekirdeği sıfırlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Bilgi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "İçerikle ilgili daha fazla bilgi görüntüle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Küçük Resimleri İndir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Mevcut içeriğe ekran görüntüsü/kapak resmi/ekran başlığı küçük resimleri indirin. Mevcut küçük resimleri günceller."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Mevcut Çekirdek"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "İsim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Dosya Yolu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Giriş: %lu/%lu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Çekirdek"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Oynama Süresi"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Son Oynama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Veritabanı"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Devam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Çalışan içeriği devam ettirip Hızlı Menüden çıkın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Yeniden Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "İçeriği yeniden başlatın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "İçeriği Kapat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Mevcut içeriği kapatın. Kaydedilmemiş tüm değişiklikler kaybolabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Ekran Görüntüsü Al"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Ekranın bir görüntüsünü yakalayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Durum Yuvası"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Seçili durum yuvasını değiştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Durum Kaydı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Seçili yuvaya durumu kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Durum Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Seçili yuvadan kaydedilmiş durum yükleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Yüklü Durumu Geri Al"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Bir durum yüklendiyse, içerik yüklenmeden önceki duruma geri döner."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Durum Kaydını Geri Al"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Bir durumun üzerine yazılmışsa, önceki durum kaydına geri döner."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Sık Kullanılanlara Ekle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "İçeriği 'Sık Kullanılanlara' ekleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Ekran Kaydı Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Video kaydını başlat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Kaydı Durdur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Video kaydını durdur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Yayın Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Seçilen hedef için yayına başlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Yayını Durdur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Yayını bitir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_LIST,
   "Durumları Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_LIST,
   "Durum kaydı seçeneklerine erişin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Çekirdek Seçenekleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Çalışan içeriğin seçeneklerini değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Kontrolcüler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Çalışan içerik için kontrolcüleri değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Hileler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Hile kodlarını ayarla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Disk Kontrolü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Disk kalıbı yönetimi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Gölgelendirici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Görüntüyü görsel olarak geliştirmek için gölgelendirici ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Özelleştirmeler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Genel yapılandırmayı özelleştirme seçenekleri."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Başarılar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Başarıları ve ilgili ayarları görüntüleyin."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Çekirdek Seçeneklerini Yönet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Mevcut içerik için seçenek geçersiz kılmaları kaydedin veya kaldırın."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Seçenekleri Oyun İçin Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Oyun Seçeneklerini Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "İçerik Dizini Seçeneklerini Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "İçerik Dizini Seçeneklerini Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Aktif Seçenekler Dosyası:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Seçenekleri Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Tüm temel seçenekleri varsayılan değerlere ayarlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Diske Yazma Seçenekleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Mevcut ayarları aktif seçenekler dosyasına yazılmaya zorlayın. Bir çekirdek hatanın ön ucun hatalı kapanmasına neden olması durumunda seçeneklerin korunmasını sağlar."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Oyun Seçenekleri Dosyası Oluştur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Oyun Seçenekleri Dosyasını Kaydet"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_MANAGER_LIST,
   "Yeniden Eşleme Dosyaları Yönet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_MANAGER_LIST,
   "Mevcut içerik için girdi yeniden eşleme dosyalarını yükleyin, kaydedin veya kaldırın."
   )

/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_INFO,
   "Etkin Yeniden Eşleme Dosyası:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Yeniden Yapılandırma Dosyasını Yükle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Çekirdek Yapılandırma Dosyasını Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Çekirdek Yeniden Yapılandırma Dosyasını Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "İçerik Dizini Yeniden Yapılandırma Dosyasını Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Oyun İçerik Dizini Yeniden Yapılandırma Dosyasını Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Oyun Yeniden Yapılandırma Dosyasını Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Oyun Yeniden Yapılandırma Dosyasını Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_RESET,
   "Giriş Eşlemesini Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_RESET,
   "Tüm giriş yeniden eşleme seçeneklerini varsayılan değerlere ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_FLUSH,
   "Girdi Yeniden Eşleme Dosyasını Güncelle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REMAP_FILE_FLUSH,
   "Geçerli girdi yeniden eşleme seçenekleriyle etkin yeniden eşleme dosyasının üzerine yazın."
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Yeniden Yapılandırma Dosyası"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Hile Aramaya Başla veya Devam Et"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Hile Dosyası Yükle (Değiştir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Hile dosyası yükleyin ve mevcut hileleri değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Hile Dosyası Yükle (Ekle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Hile dosyası yükleyin ve mevcut hilelere ekleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Oyuna Özel Hileleri Yeniden Yükle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Hile Dosyasını Farklı Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Mevcut hileleri bir hile dosyası olarak kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "En Üste Yeni Hile Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "En Alta Yeni Hile Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Tüm Hileleri Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Oyun Yüklenirken Hileleri Otomatik Uygula"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Oyun yüklendiğinde hileleri otomatik olarak uygular."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Sonra Uygula Değiştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Değişiklik sonrası hemen hileyi uygula."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Değişiklikleri Uygula"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Hile değişiklikleri derhal işleme girecektir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Hile"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Hile Aramasını Başlat veya Yeniden Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Bit boyutunu değiştirmek için Sol ve Sağ'a basın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
   "Düşük Son Haneli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Değeri değiştirmek için Sola veya Sağa basın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Eşittir %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Öncekinden Daha Az"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Öncekinden Küçüktür veya Eşittir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Öncekinden Büyüktür"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Öncekinden Büyüktür veya Eşittir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Öncekine Eşittir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Öncekine Eşit Değildir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Değeri değiştirmek için Sola veya Sağa basın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Öncekine Eşittir +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Bellekte Değerleri Ara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Değeri değiştirmek için Sola veya Sağa basın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Öncekine Eşittir -%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "%u Eşleşmeyi Listene Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Eşleşmeyi Sil #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Kod Eşleşmesi Oluştur #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Eşleşme Adresi: %08X Maske: %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Hile Dosyası (Değiştir)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Hile Dosyası (Ekle)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Hile Ayrıntıları"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Fihrist"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Listede hile pozisyonu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Etkin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Açıklama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "İşleyici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Bellek Arama Boyutu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Tür"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Değer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Bellek Adresi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Adrese Göz At: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Bellek Adresi Maskesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Adres bit maskesi zamanı Bellek Arama Boyutu < 8-bit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Yineleme Sayısı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Hile sayısı. Geniş bellek alanlarını etkilemek için diğer iki 'Yineleme' seçeneğiyle birlikte kullanın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Her Tekrarlamada Adresi Arttır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Her yinelemeden sonra 'Bellek Adresi' bu sayı ile 'Bellek Arama Boyutunun' katı kadar artırılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Her Tekrarlamada Değeri Arttır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Her yinelemeden sonra 'Değer' bu miktarda artacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Titreşim Belleği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Titreşim Değeri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Titreşim Portu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Birincil Titreşim Gücü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Birincil Titreşim Süresi (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "İkincil Titreşim Gücü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "İkincil Titreşim Süresi (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Kod"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Bundan Sonra Yeni Hile Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Bundan Önce Yeni Hile Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Bu Hileyi Sonrasına Kopyala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Bu Hile Öncesinde Kopyala"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Bu Hileyi Kaldır"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Diski Çıkar"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Sanal disk tepsisini açar ve şu anda yüklü olan diski kaldırır. 'Menü Etkinken İçeriği Duraklat' etkinleştirilirse, her disk kontrolü işleminden sonra içerik birkaç saniye devam etmediği sürece bazı çekirdekler değişiklikleri kaydetmeyebilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Diski Tak"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "'Mevcut Disk Dizini' kısmına karşılık gelen disk yerleştirir ve sanal disk tepsisini kapatır. 'Menü Etkinken İçeriği Duraklat' etkinleştirilirse, her disk kontrolü işleminden sonra içerik birkaç saniye devam etmediği sürece bazı çekirdekler değişiklikleri kaydetmeyebilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Yeni Disk Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Mevcut diski çıkarın, dosya sisteminden yeni bir disk seçin, sonra takın ve sanal disk tepsisini kapatın.\nNOT: Bu eski bir özellik. Bunun yerine, 'Diski Çıkar/Tak' ve 'Mevcut Disk İndeksi' seçeneklerini kullanarak disk seçimine izin veren M3U çalma listeleri aracılığıyla çok diskli başlıkların yüklenmesi önerilir."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Dosya sisteminden yeni bir disk seçin ve sanal disk tepsisini kapatmadan takın.\nNOT: Bu eski bir özelliktir. Bunun yerine, 'Mevcut Disk Dizini' seçeneğini kullanarak disk seçimine izin veren M3U çalma listeleri aracılığıyla çok diskli başlıkların yüklenmesi önerilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Mevcut Disk Dizini"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Kullanılabilir resimler listesinden geçerli diski seçin. 'Disk Ekle' seçildiğinde disk yüklenecektir."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Video Gölgelendirici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADERS_ENABLE,
   "Video gölgelendirici iş çizgisini etkinleştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Değişiklikler için Gölgelendirici Dosyalarını İzle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Diskteki gölgelendirici dosyalarında yapılan değişiklikleri otomatik olarak uygulayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Son Kullanılan Gölgelendirici Dizinini Hatırla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Gölgelendirici ön ayarlarını ve geçişlerini yüklerken son kullanılan dizindeki dosya tarayıcısını açın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Yükle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Gölgelendirici önayarı yükleyin. Gölgelendirici düzeni otomatik olarak kurulacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_PREPEND,
   "Başa Ekle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PREPEND,
   "Şu anda yüklü olan Ön Ayarı Başa Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_APPEND,
   "Sona Ekle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_APPEND,
   "Şu anda yüklü olan Ön Ayarı Sona Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
   "Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "Mevcut gölgelendirici ön ayarını kaydet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
   "Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "Belirli bir türdeki gölgelendirici hazır ayarlarını kaldır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Değişiklikleri Uygula"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Gölgelendirici yapılandırmasındaki değişiklikler hemen geçerli olur. Gölgelendirici geçişi, filtreleme, FBO ölçeği vb. Miktarını değiştirdiyseniz bunu kullanın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Gölgelendirici Parametreleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Mevcut gölgelendiriciyi doğrudan değiştirir. Değişiklikler önceden ayarlanmış dosyaya kaydedilmeyecek."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Gölgelendirici Geçişleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Gölgelendirici geçiş miktarını artırın veya azaltın. Ayrı gölgelendiriciler her iş geçişine bağlanabilir ve ölçeğini ve filtrelemesini yapılandırabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "Gölge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Filtre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Ölçek"
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Basit Hazır Ayarlar"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Yüklenen orijinal ön ayara bir bağlantı içeren ve yalnızca yaptığınız parametre değişikliklerini içeren bir gölgelendirici ön ayarını kaydeder."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Gölgelendirici Hazır Ayarını Farklı Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Mevcut gölgelendirici ayarlarını yeni bir gölgelendirici hazır ayarı olarak kaydet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Genel Ön Ayarı Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Mevcut gölgelendirici ayarlarını varsayılan genel ayar olarak kaydet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Çekirdek Hazır Ayarını Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Mevcut gölgelendirici ayarlarını bu çekirdek için varsayılan olarak kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "İçerik Dizini Ön Ayarını Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Mevcut gölgelendirici ayarlarını mevcut içerik dizinindeki tüm dosyalar için varsayılan olarak kaydedin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Oyun Ön Ayarını Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Mevcut gölgelendirici ayarlarını içerik için varsayılan ayarlar olarak kaydedin."
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Otomatik Gölgelendirici Hazır Ayarı Bulunamadı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Genel Hazır Ayarları Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Tüm içerik ve tüm çekirdekler tarafından kullanılan Genel Hazır Ayarı kaldırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Çekirdek Hazır Ayarını Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Şu anda yüklü olan çekirdekle çalışan tüm içerikler tarafından kullanılan Çekirdek Hazır Ayarını kaldırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "İçerik Dizini Hazır Ayarını Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Mevcut çalışma dizini içindeki tüm içerik tarafından kullanılan İçerik Dizini Hazır Ayarını kaldırın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Oyun Hazır Ayarını Kaldır"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Yalnızca mevcut oyun için kullanılan Oyun Ön Ayarını kaldırın."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Gölgelendirici Parametresi Yok"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Çekirdek Özelleştirmelerini Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Çekirdekte yüklü olan tüm içerik için geçerli olacak özelleştirme dosyasını kaydeder. Ana yapılandırmadan öncelikli olacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "İçerik Dizini Özelleştirmelerini Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Mevcut dosyayla aynı dizinden yüklenen tüm içerik için geçerli olacak özelleştirilmiş yapılandırma dosyasını kaydeder. Ana yapılandırmadan öncelikli olacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Oyun Özelleştirmelerini Kaydet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Yalnızca mevcut içerik için özelleştirilmiş yapılandırma dosyasını kaydeder. Ana yapılandırmadan öncelikli olacaktır."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Gösterilecek Başarı Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Zorlu Kipte Başarıları Duraklatmayı İptal Et"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Mevcut oturum için zorlu kip başarımını etkinleştirir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Zorlu Kipte Başarıları Devam Ettirmeyi İptal Et"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Mevcut oturum için zorlu kip başarımını devre dışı bırakır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Zorlu Kipte Başarıları Duraklat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Mevcut oturum için başarıları zorlu kipte duraklatın. Bu eylem, hileleri, geri sarmayı, ağır çekimi ve durum kaydı yüklemesini etkinleştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Zorlu Kipte Başarılara Devam Et"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Mevcut oturum için başarıları zorlu kipte devam ettirin. Bu eylem, hileleri, geri sarmayı, ağır çekimi ve durum kaydı yüklemesini devre dışı bırakır ve mevcut oyunu sıfırlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Giriş yapmadınız"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Ağ Hatası"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Bilinmeyen Oyun"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Başarılar bu çekirdekle etkinleştirilemez"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "RetroAchievements Doğrulaması"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Veritabanı Girdisi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Mevcut içerik için veritabanı bilgilerini göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Görüntülenecek Girdi Yok"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Kullanılabilir Çekirdek Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Çekirdek Seçenekleri Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Çekirdek Bilgisi Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Çekirdek Yedeklemesi Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Sık Kullanılan Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Geçmiş Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Kullanılabilir Resim Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Kullanılabilir Müzik Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Kullanılabilir Video Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Bilgi Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Oynatma Listesi Girdisi Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Ayar Bulunamadı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Bluetooth Cihazı Bulunamadı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Ağ Bulunamadı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Çekirdek Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Ara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CYCLE_THUMBNAILS,
   "Küçük resim döngüsü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Geri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "Tamam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Ana Dizin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Dizin Bulunamadı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Öge Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Dosya Seç"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_90_DEG,
   "90 derece"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_180_DEG,
   "180 derece"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION_270_DEG,
   "270 derece"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_VERTICAL,
   "90 derece"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED,
   "180 derece"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ORIENTATION_FLIPPED_ROTATED,
   "270 derece"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Bilinmeyen derleyici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Paylaş"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Tutun"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Oyla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Analog Giriş Paylaşımı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Azami"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Ortalama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Tercih yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Sola/Sağa Sıçrama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Sola Kaydır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Görüntü Kipi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Konuşma Kipi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Ekran Okuyucusu Kipi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Geçmiş & Sık Kullanılanlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Tüm Oynatma Listeleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Geçmiş & Sık Kullanılanlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Daima"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Asla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Çekirdek Başına"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Toplam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Şarj oldu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Şarj oluyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Şarjı tükeniyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "Kaynak Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Bu Dizini Kullan>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<İçerik Dizini>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Varsayılan>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Yok>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "Analog ile RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Bilinmiyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Start'a Basılı Tut (2 saniye)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Select Tuşuna Basılı Tut (2 saniye)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Aşağı + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Devre dışı>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Değişiklikler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Değiştirilemez"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Arttırır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Azalt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= Titreşim Değeri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= Titreşim Değeri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< Titreşim Değeri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> Titreşim Değeri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Titreşim Değerini Arttır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Titreşim Değerini Azalt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Tümü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Devre dışı>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Değere Ayarla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Değere Göre Arttır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Değere Göre Düşür"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Sonraki Hileyi Çalıştır Değer = Bellek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Sonraki Hileyi Çalıştır Değer != Bellek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Sonraki Hileyi Çalıştır Değer < Bellek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Sonraki Hileyi Çalıştır Değer > Bellek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Emülatör"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Bit, Azami Değer = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-Bit, Azami Değer = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-Bit, Azami Değer = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-Bit, Azami Değer = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-Bit, Azami Değer = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-Bit, Azami Değer = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Sistem Varsayılanı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Alfabetik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Etiketleri Tam Göster"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Kaldır () İçerik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Kaldır [] İçerik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Kaldır () ve []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Bölgeyi Sakla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Disk İndeksini Sakla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Bölge ve Disk İndeksini Sakla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Sistem Varsayılanı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Kapak Resmi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Ekran Görüntüsü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Ekran Başlığı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Hızlı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "AÇIK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Evet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Hayır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Doğru"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Yanlış"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Etkin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Devre dışı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "Boş"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Kilitli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Açılmış"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
   "Zorlayıcı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Resmi Olmayan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Desteklenmiyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Yakınlarda Kilidi Açılan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Neredeyse Tamamlanan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Etkin Meydan Okumalar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Sadece İzleyiciler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Sadece Bildirim"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Varsayılan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Doğrusal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "En yakın"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Ana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "İçerik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<İçerik Dizini>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Özel>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Belirtilmemiş>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Sol Analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Sağ Analog"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Sol Analog (Zorunlu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Sağ Analog (Zorunlu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "(Tuş: %s)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Fare 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Fare 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Fare 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Fare 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Fare 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Fare Tekerleği Yukarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Fare Tekerleği Aşağı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Fare Tekerleği Sol"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Fare Tekerleği Sağ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Erken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Geç"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "YYYY-AA-GG SS:DD:SN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "YYYY-AA-GG SS:DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "YYYY-AA-GG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "YYYY-AA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "AA-GG-YYYY SS:DD:SN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "AA-GG-YYYY SS:DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "AA-GG SS:DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "AA-GG-YYYY"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD,
   "AA-GG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "GG-AA-YYYY SS:DD:SN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "GG-AA-YYYY SS:DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "GG-AA SS:DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "GG-AA-YYYY"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM,
   "GG-AA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS,
   "SS:DD:SN"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM,
   "SS:DD"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "YYYY-AA-GG SS:DD:SN (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "YYYY-AA-GG SS:DD (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "AA-GG-YYYY SS:DD:SN (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "AA-GG-YYYY SS:DD (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "AA-GG SS:DD (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "GG-AA-YYYY SS:DD:SN (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "GG-AA-YYYY SS:DD (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "GG-AA SS:DD (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM,
   "SS:DD:SN (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM,
   "SS:DD (ÖÖ/ÖS)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_AGO,
   "Önce"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Arkaplan Dolgu Kalınlığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Menü arka plan dama tahtası deseninin kalınlığını artır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Kenar Dolgusu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Kenar Dolgu Kalınlığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Menü kenarlığı dama tahtası kalınlığını arttır."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Menüde kenarlığı görüntüle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Tam Genişlik Düzeni Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Kullanılabilir ekran alanını en iyi şekilde kullanmak için menü girdilerini yeniden boyutlandır ve yerleştir. Klasik sabit genişlikte iki sütun düzenini kullanmak için bunu devre dışı bırakın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Menü Doğrusal Süzgeç"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Sert piksel kenarını yumuşatmak için menüye hafif bir bulanıklık ekler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Dahili Ölçek Yükseltici"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Ekrana vermeden önce menü arayüzü yeniden ölçeklenir. 'Menü Doğrusal Filtre' etkinken kullanıldığında, keskin bir görüntüyü korurken ölçek arka plan efektlerini (düzensiz pikseller) kaldırır. Yükseltme seviyesiyle birlikte önemli bir eksi performans etkisi vardır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Menü En Boy Oranı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Menü en boy oranını seç. Geniş ekran oranları, menü arayüzünün yatay çözünürlüğünü arttırır. ('Menü En Boy Oranını Kilitle' devre dışı bırakılmışsa yeniden başlatmayı gerektirebilir)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Menü En Boy Oranını Kilitle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Menünün her zaman doğru en boy oranıyla görüntülenmesini sağlar. Devre dışı bırakılırsa, hızlı menü geçerli yüklü içerikle eşleşecek şekilde genişletilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Menü Renk Teması"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Farklı bir renk teması seçin. 'Özel' seçimi, menü teması önceden ayarlanmış dosyaların kullanımını mümkün kılar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Özel Menü Teması Ön Ayarı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Dosya tarayıcısından hazır bir menü teması seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Menü Şeffaflığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Hızlı Menü etkinken çalışan içeriğin arka planda görüntülenmesini etkinleştirin. Şeffaflığın devre dışı bırakılması tema renklerini değiştirebilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Gölge Efektleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Menü metni, kenarlık ve küçük resimler için alt gölgeleri etkinleştirin. Performansa düşük oranda eksi etkisi var."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Arkaplan Animasyonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Arka plan parçacık animasyon efektini etkinleştir. Performans üzerinde olumsuz etkisi olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Arkaplan Animasyon Hızı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Arka plan parçacık animasyon efektlerinin hızını ayarla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Ekran Koruyucu Arka Plan Animasyonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Menü ekran koruyucu etkinken arka plan parçacık animasyon efektini görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Oynatma Listesi Küçük Resimlerini Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Oynatma listeleri görüntülerken satır içi ufak küçük resimlerin görüntülenmesini etkinleştir. Devre dışı olduğunda, 'Üst Küçük Resim' RetroPad Y düğmesine basarak tam ekran olarak değiştirilebilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Üst Küçük Resim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Oynatma listelerinin sağ üst köşesinde görüntülenecek küçük resim türü. Bu küçük resim, RetroPad Y düğmesine basılarak tam ekrana getirilebilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Alt Küçük Resim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Oynatma listelerinin sağ alt kısmında gösterilecek küçük resim türü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Resimlerin Yerini Değiştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "'Üst Küçük Resim' ve 'Alt Küçük Resim' ekran konumlarını değiştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Küçük Resim Ölçeklendirme Yöntemi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Büyük küçük resimleri ekrana sığdırmak için küçültürken yeniden örnekleme yöntemi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Küçük Resim Gecikmesi (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Bir oynatma listesi girişi seçme ile ilişkili küçük resimlerini yükleme arasında bir gecikme süresi uygular. Bunu en az 256 ms değerine ayarlamak, en yavaş cihazlarda bile hızlı gecikmesiz kaydırmayı mümkün kılar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Genişletilmiş ASCII Desteği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Standart olmayan ASCII karakterlerinin görüntülenmesini etkinleştir. İngilizce dışındaki bazı Batı dillerine uyumluluk için gereklidir. Orta düzeyde performans etkisi vardır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Anahtar Simgeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "'Değiştir' menü ayarlarını belirtmek için AÇIK/KAPALI metni yerine simgeleri kullanın."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "En Yakın İlişki (Hızlı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "İkili Doğrusal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (Yavaş)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Yok"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "Otomatik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (Ortalanmış)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (Ortalanmış)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (Ortalanmış)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (Ortalanmış)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Ekrana Sığdır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Tam Sayı Ölçeği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Ekranı Doldur (Uzatılmış)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Klasik Kırmızı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Klasik Turuncu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Klasik Sarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Klasik Yeşil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Klasik Mavi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Klasik Menekşe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Klasik Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Eski Kırmızı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Koyu Mor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Gece Mavisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Altın"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektrik Mavisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Elma Yeşili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Volkanik Kırmızı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Gölcük"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DRACULA,
   "Drakula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "Pamuk Şeker"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI,
   "Düz Arayüz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Koyu Gruvbox"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Açık Gruvbox"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Kırık Çekirdek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD,
   "Kuzey"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
   "Bir Koyuluk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT,
   "Soluk Gece"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarize Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarize Açık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango Açık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dinamik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_DARK,
   "Koyu Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Açık Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Kar (Hafif)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Kar (Yoğun)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Yağmur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_VORTEX,
   "Girdap"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Yıldız Alanı"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Sol Küçük Resim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Solda görüntülenecek küçük resim türü."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Dinamik Arkaplan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Bağlama göre dinamik olarak yeni bir duvar kağıdı yükle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Yatay Animasyon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Menü için yatay animasyonu etkinleştirin. Performans kaybı etkisi olacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Animasyon Yatay Simge Vurgula"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Sekmeler arasında dolaşırken tetiklenen animasyon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Yukarı/Aşağı Hareket Etme Animasyonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Yukarı veya aşağı hareket ederken tetiklenen animasyon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Ana Menü Animasyonları Aç/Kapa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Bir alt menü açarken tetiklenen animasyon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Menü Alfa Etkeni"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Menü Yazı Tipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Menü tarafından kullanılacak farklı bir ana yazı tipi seç."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Menü Yazı Tipi Rengi (Kırmızı)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Menü Yazı Tipi Rengi (Yeşil)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Menü Yazı Tipi Rengi (Mavi)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Menü Düzeni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "XMB arayüzü için farklı bir düzen seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Menü Simgesi Teması"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "RetroArch için farklı bir simge teması seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SWITCH_ICONS,
   "Anahtar Simgeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SWITCH_ICONS,
   "'Değiştir' menü ayarlarını belirtmek için AÇIK/KAPALI metni yerine simgeleri kullanın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Gölgeleri Çiz"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Simgeler, küçük resimler ve harfler için gölgeler çizin. Bunun performansa küçük bir etkisi olacak."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Menü Arkaplan Animasyonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Animasyonlu bir arka plan efekti seçin. Efektine bağlı olarak GPU-yoğunluğu olabilir. Performans yetersizse, bunu kapatın veya daha basit bir efekte geri dönün."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Menü Tema Rengi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Farklı bir arka plan rengi dolgu teması seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Küçük Resimler Dikey Eğilimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Soldaki küçük resmi, ekranın sağ tarafında, sağ alt köşesinde görüntüleyin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Küçük Resim Ölçek Etkeni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "İzin verilen azami genişliği ölçekle küçük resim görüntüleme boyutunu küçült."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Dikey Solma Etkeni"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_SHOW_TITLE_HEADER,
   "Başlığı Göster"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN,
   "Başlık Payı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_TITLE_MARGIN_HORIZONTAL_OFFSET,
   "Başlık Payı Yatay Ofset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Ayarlar Sekmesini Etkinleştir (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Program ayarlarını içeren Ayarlar sekmesini gösterin."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Şerit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Şerit (Basitleştirilmiş)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Basit Kar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Kar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
   "Bulanıklık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Kar tanesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Özel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
   "Düz Arayüz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Tek Renk"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Tek Renk Ters"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Sistematik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
   "NeoAktif"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
   "Piksel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
   "RetroAktif"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
   "Retrosistem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
   "Nokta Sanatı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Otomatik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Otomatik Ters"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DAITE,
   "Renkli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Elma Yeşili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Açık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Sabah Mavisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Gün Işığı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Koyu Mor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Elektrik Mavisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Altın"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Eski Kırmızı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Gece Mavisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Sade"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Denizaltı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Volkanik Kırmızı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Limon Yeşili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Pikachu Sarısı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Tatlı Mor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Kırmızı Ailesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Ateşli Sıcak"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Buz Soğuğu"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Kenar Çubuğunu Daralt"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Sol kenar çubuğunu daima daralt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Oynatma Listesi Adlarını Kes (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Üretici adlarını oynatma listelerinden kaldırın. Örneğin, 'Sony - PlayStation', 'PlayStation' olur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Ad Kısaltmadan Sonra Oynatma Listelerini Sırala (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Oynatma listeleri, adlarının üretici bileşeni kaldırıldıktan sonra alfabetik olarak yeniden sıralanacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Menü Tema Rengi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Farklı bir renk teması seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Temel Beyaz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Temel Siyah"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_NORD,
   "Kuzey"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY,
   "Böğürtlen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Kırık Çekirdek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "Alacakaranlık Bölgesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_DRACULA,
   "Drakula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solar Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarize Açık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_DARK,
   "Koyu Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRAY_LIGHT,
   "Açık Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_PURPLE_RAIN,
   "Mor Yağmur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "İkinci Küçük Resim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "İçerik üst veri panelini başka bir küçük resimle değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "İçerik Üst Verileri İçin Kayan Metin Kullan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Etkinleştirildiğinde, çalma listelerinin sağ kenar çubuğunda gösterilen içerik meta verilerinin her bir maddesi (ilişkili çekirdek, çalma süresi) tek bir satır kaplar; Kenar çubuğunun genişliğini aşan dizeler kayan yazı metni olarak görüntülenir. Devre dışı bırakıldığında, içerik meta verilerinin her bir öğesi, gerektiği kadar satır tutacak şekilde kaydırılarak statik olarak görüntülenir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Küçük Resim Ölçek Etkeni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Küçük resim çubuğunun boyutunu ölçeklendirin."
   )

/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Menü Simgeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Menü girdilerinin solundaki simgeleri göster."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SWITCH_ICONS,
   "Anahtar Simgeleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SWITCH_ICONS,
   "'Değiştir' menü ayarlarını belirtmek için AÇIK/KAPALI metni yerine simgeleri kullanın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Oynatma Listesi Simgeleri (Yeniden Başlatılmalı)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Oynatma listelerinde sisteme özel simgeleri gösterin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Yatay Yerleşimi Düzenle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Yatay ekran yönlerini kullanırken ekrana daha iyi uyacak şekilde menü düzenini otomatik olarak ayarla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Gezinti Çubuğunu Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Kalıcı ekran menü gezinme kısayollarını görüntüle. Menü kategorileri arasında hızlı geçiş yapılmasını sağlar. Dokunmatik ekranlı cihazlar için önerilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Gezinti Çubuğunu Otomatik Döndür"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Yatay görüntü yönlerini kullanırken gezinme çubuğunu ekranın sağ tarafına otomatik olarak taşı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Menü Tema Rengi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Farklı bir arka plan rengi dolgu teması seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Menü Geçiş Animasyonu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Menünün farklı seviyeleri arasında gezinirken yumuşak animasyon efektlerini etkinleştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Dikey Küçük Resim Görünümü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Dikey ekran yönlerini kullanırken oynatma listesi küçük resim görünümü kipini belirtin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Yatay Küçük Resim Görünümü"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Yatay ekran yönlerini kullanırken oynatma listesi küçük resim görünümü kipini belirtin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Liste Görünümlerinde İkincil Küçük Resmi Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "'Liste' türü oynatma listesi küçük resim görüntüleme kiplerini kullanırken ikincil bir küçük resim görüntüler. Bu ayar yalnızca ekranda iki küçük resim göstermek için yeterli fiziksel genişliğe sahip olduğunda geçerlidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Küçük Resim Arkaplan Çizimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Sağlam bir arka plana sahip küçük resim görüntülerinde kullanılmayan alanın dolmasını sağlar. Bu, tüm görüntüler için tekdüze bir görüntü boyutu sağlayarak, farklı taban boyutlarına sahip karışık içerikli küçük resimleri görüntülerken menü görünümünü iyileştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Birincil Küçük Resim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Her oynatma listesi girişiyle ilişkilendirilecek ana küçük resim türü. Genellikle içerik simgesi olarak kullanılır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "İkincil Küçük Resim"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Her oynatma listesi girişi ile ilişkilendirilecek yardımcı küçük resim türü. Kullanım, geçerli oynatma listesi küçük resim görüntüleme kipine bağlıdır."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Mavi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Mavi Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Koyu Mavi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Yeşil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "Kalkan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Kırmızı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Sarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI,
   "Malzeme Arayüzü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Malzeme Arayüzü Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozon Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD,
   "Kuzey"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarize Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Tatlı Mavi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Tatlı Camgöbeği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Tatlı Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Tatlı Turuncu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Tatlı Pembe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Tatlı Mor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Tatlı Kırmızı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Kırık Çekirdek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_DARK,
   "Koyu Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRAY_LIGHT,
   "Açık Gri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "Otomatik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Karart"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Kaydır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Liste (Küçük)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Liste (Orta)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Çift Simge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Liste (Küçük)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Liste (Orta)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Liste (Büyük)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Masaüstü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "KAPALI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "AÇIK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Küçük Resim Görünümlerini Hariç Tut"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Bilgi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Dosya"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Çekirdek Yükle..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Çekirdeği Çıkart"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "&Çıkış"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Düzenle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Ara"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Görünüm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Kapalı Alanlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Gölgelendirici Parametreleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Ayarlar..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Yuva konumlarını hatırla:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Pencere geometrisini hatırla:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Son içerik tarayıcı sekmesini hatırla:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Tema:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Sistem Varsayılanı>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Koyu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Özel..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Ayarlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Araçlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Yardım"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "RetroArch Hakkında"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Dokümantasyon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Özel Çekirdek Yükle..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Çekirdek Yükle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Çekirdek Yükleniyor..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "İsim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Sürüm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Oynatma Listeleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Dosya Tarayıcısı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Üst"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Yukarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "İçerik Tarayıcı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Kapak Resmi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Ekran Görüntüsü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Ekran Başlığı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Tüm Oynatma Listeleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Çekirdek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Çekirdek Bilgisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Sor>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Bilgi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Uyarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Hata"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Ağ Hatası"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Değişikliklerin etkili olması için lütfen programı yeniden başlatın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Günlük"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 öge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Resmi buraya sürükle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Bunu bir daha gösterme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Durdur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Bağdaştırılmış Çekirdek"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Gizli Oynatma Listeleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Gizle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Vurgu rengi:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Seç..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Renk Seç"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Tema Seç"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Özel Tema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Dosya yolu boş."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Dosya boş."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Dosya okumak için açılamadı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Dosya yazmak için açılamadı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Dosya bulunmuyor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Önce yüklü çekirdeği önerin:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Yakınlaştır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Görünüm"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Simgeler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Liste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Temizle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "İlerleyiş:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "\"Tüm Oynatma Listeleri\" liste sınırı:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "\"Tüm Oynatma Listeleri\" ızgara sınırı:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Gizli dosya ve klasörleri göster:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Yeni Oynatma Listesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Lütfen yeni oynatma listesi adını girin:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Oynatma Listesini Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Oynatma Listesini Adlandır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Oynatma listesini silmek istiyor musunuz \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Soru"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Dosya silinemedi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Dosya yeniden adlandırılamadı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Dosya listesi toplanıyor..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Oynatma listesine dosyalar ekleniyor..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Oynatma Listesi Girdisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "İsim:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Yol:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Çekirdek:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Veritabanı:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Eklentiler:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(boşlukla ayrılmış; varsayılan olarak tümünü içerir)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Arşivlerin içine filtre uygula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(küçük resimleri bulmak için kullanılır)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Ögeyi silmek istiyor musunuz \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Lütfen önce tek bir oynatma listesi seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Sil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Girdi Ekle..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Dosya Ekle..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Klasör Ekle..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Düzenle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Dosyaları Seç"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Klasör Seç"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<birden fazla>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Oynatma listesi girdisi güncellenirken hata."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Lütfen tüm gerekli alanları doldurun."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "RetroArch'ı Güncelle (gecelik sürüm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch başarıyla güncellendi. Değişikliklerin etkili olması için lütfen yeniden başlatın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Güncelleme başarısız."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Katılımcılar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Mevcut gölgelendirici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Aşağı Taşı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Yukarı Taşı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Yükle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Kaldır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Geçişleri Kaldır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Uygula"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Geçiş Ekle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Tüm Geçişleri Temizle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Gölgelendirici geçişi yok."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Geçişi Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Tüm Geçişleri Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Parametreleri Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Küçük resim indir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Bir indirme işlemi zaten devam ediyor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Oynatma listesinde başla:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Küçük resim"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Küçük resim önbellek sınırı:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Sürükle ve bırak Küçük resim boyutu sınırı:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Tüm Küçük Resimleri İndir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Tüm Sistem"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Bu Oynatma Listesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Küçük resimler başarıyla indirildi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Başarılı: %1 Başarısız: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Çekirdek Seçenekleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Sıfırla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Tümünü Sıfırla"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Güncelleyici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Cheevos Hesapları"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Hesap Listesi Sonu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Turbo/Ölü Bölge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Çekirdek Sayaçları"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Disk seçilmedi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Ön Uç Sayaçları"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Yatay Menü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Bağlı Olmayan Çekirdek Giriş Tanımlayıcılarını Gizle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Giriş Tanımlayıcı Etiketlerini Göster"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Ekran Kaplama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Geçmiş"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Geçmiş oynatma listesinden son içeriği seç."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "Çoklu Ortam"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Alt Sistemler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUBSYSTEM_SETTINGS,
   "Mevcut içerik için alt sistem ayarlarına erişin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_CONTENT_INFO,
   " Mevcut İçerik: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Netplay sunucuları yok."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_CLIENTS_FOUND,
   "Netplay istemcisi bulunamadı."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Performans sayacı yok."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Oynatma listesi yok."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Bağlandı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "Çevrimiçi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Port %d Cihaz Adı: %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_INFO,
   "Cihaz Görünen Adı: %s\nCihaz Yapılandırma Adı: %s\nCihaz VID/PID: %d/%d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Hile Ayarları"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Hile Aramaya Başla veya Devam Et"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Çalıştır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "saniye"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Çekirdeği Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "İçerik olmadan çekirdeği başlat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Önerilen çekirdekler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Sıkıştırılmış dosya okunamıyor."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Kullanıcı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Dahili Resim Görüntüleyici Kullan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Azami Takas Zinciri Görüntüleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Video sürücüsüne belirli bir arabelleğe alım kipini açıkça kullanmasını söyler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WAITABLE_SWAPCHAINS,
   "Beklenebilir Takas Zinciri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WAITABLE_SWAPCHAINS,
   "CPU ve GPU zorla sabit olarak eşitle. Performanstan ödün vererek gecikmeyi azaltır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_FRAME_LATENCY,
   "Azami Kare Gecikmesi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_FRAME_LATENCY,
   "Video sürücüsüne belirli bir arabelleğe alım kipini açıkça kullanmasını söyler."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Şu anda menüde kullanılan gölgelendirici hazır ayarını değiştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Gölgelendirici Ön Ayarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PREPEND_TWO,
   "Gölgelendirici Ön Ayarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_APPEND_TWO,
   "Gölgelendirici Ön Ayarı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "URL'ye Gözat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "URL Yolu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Başlat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Kullanıcı Adı: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_LOOK,
   "Uyumlu içerik aranıyor..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_CORE,
   "Çekirdek bulunamadı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NO_PLAYLISTS,
   "Oynatma listesi bulunamadı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Uyumlu içerik bulundu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_NOT_FOUND,
   "CRC veya dosya adına göre eşleşen içerik bulunamadı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Gong Başlat"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "Otomatik En Boy Oranı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "Rumuz (LAN): %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "Durum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Sistem BGM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
   "Özel Oran"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Kayıt Desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "Çıktı Kaydını Farklı Kaydet..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "Kayıtları Çıktı Dizinine Kaydet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "Eşleşmeyi Göster #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "Görüntülenecek eşleşmeyi seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "En Boy Oranını Zorla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "Bir oynatma listesinden seç"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME,
   "Devam"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME,
   "Çalışan içeriği devam ettirip Hızlı Menüden çıkın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "%u Eşleşmelerinin Listesini Görüntüle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "Bu Eşleşmeden Kod Oluştur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "Bu Eşleşmeyi Sil"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "Alt Bilgi Şeffaflığı"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "Alt bilgi grafiğinin şeffaflığını değiştirin."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "Başlık Şeffaflığı"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "Başlık grafiğinin şeffaflığını değiştirin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "İçeriği Başlat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "İçerik Geçmişi Yolu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Çıkış Ekranı Kimliği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "CRT ekranına bağlı çıkış portu seç."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Yardım"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Ses/Video Sorun Gider"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "Sanal Kontrolcü Bindirmesini Değiştir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "İçerik Yükleniyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "İçerik Taranıyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "Çekirdek Nedir?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "Hata Ayıklama Bilgisi Gönder"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
   "Cihazınız ve RetroArch yapılandırması hakkındaki teşhis bilgilerini analiz için sunucularımıza gönderir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Veritabanı Ayarları"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Netplay Kare Gecikmeleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Yerel ağı tara"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Yerel ağdaki netplay ana bilgisayarlarını ara ve bağlan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Netplay İstemcisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Netplay İzleyici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Açıklama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "Azami Çalışma Hızını Sınırla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "Yeni Hile Kodu Aramaya Başla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "Yeni bir hile aramaya başlayın. Bit sayısı değiştirilebilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Aramaya Devam Et"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Yeni bir hile için aramaya devam edin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Başarılar (Zorlu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Hile Ayrıntıları"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Hile ayrıntıları ayarlarını yönetir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Hile Aramaya Başla veya Devam Et"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Hile kodu aramayı başla veya devam et."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Hile Geçişleri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Hileleri artırın veya azaltın."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Sol Analog X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Sol Analog Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Sağ Analog X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Sağ Analog Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Hile Aramaya Başla veya Devam Et"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Veritabanı İmleç Listesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Veritabanı - Filtre: Geliştirici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Veritabanı - Filtre: Dağıtıcı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Veritabanı - Filtre: Köken"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Veritabanı - Filtre: İmtiyaz"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Veritabanı - Filtre: ESRB Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Veritabanı - Filtre: ELSPA Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Veritabanı - Filtre: PEGI Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Veritabanı - Filtre: CERO Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Veritabanı - Filtre: BBFC Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Veritabanı - Filtre: Azami Kullanıcı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Veritabanı - Filtre: Aylık Yayın Tarihi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Veritabanı - Filtre: Yıla Göre Yayın Tarihi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Veritabanı - Filtre: Edge Dergisi Sayısı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Veritabanı - Filtre: Edge Dergisi Değerlendirmesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Veritabanı Bilgisi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Yapılandırma"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "İndirilenler"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Netplay ayarları"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Slang desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "OpenGL/Direct3D doku oluştur (çoklu geçişli gölgelendirici) desteği"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "İçerik"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "Genellikle libretro/RetroArch uygulamalarının içeriklerini işaret eden paketleyen geliştiriciler tarafından ayarlanır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Sor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Temel menü kontrolcüleri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Onayla/TAMAM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "Bilgi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "Çık"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Yukarı Kaydır"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Varsayılanlar"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Klavyeyi Değiştir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Menüyü Değiştir"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "Menü-İçinde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "Oyun-İçinde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "Oyun-İçinde (Durduruldu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Oynatılıyor"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Durduruldu"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Netplay içerik yüklendiğinde başlayacaktır."
   )
MSG_HASH(
   MSG_NETPLAY_NEED_CONTENT_LOADED,
   "Netplay başlamadan önce içerik yüklenmelidir."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Uygun bir çekirdek veya içerik dosyası bulunamadı, el ile yükleyin."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Grafik sürücünüz RetroArch'taki mevcut video sürücüsü ile uyumlu değil ve %s sürücüsüne geri dönülüyor. Lütfen değişikliklerin geçerli olması için RetroArch'ı yeniden başlatın."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Çekirdek kurulumu başarılı"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Çekirdek kurulumu başarısız"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Tüm hileleri silmek için beş kez sağa basın."
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "Hata ayıklama bilgisi kaydedilemedi."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_DEBUG_INFO,
   "Hata ayıklama bilgisi sunucuya gönderilemedi."
   )
MSG_HASH(
   MSG_SENDING_DEBUG_INFO,
   "Hata ayıklama bilgisi gönderiliyor..."
   )
MSG_HASH(
   MSG_SENT_DEBUG_INFO,
   "Hata ayıklama bilgisi sunucuya başarıyla gönderildi. Kimlik numaranız %u."
   )
MSG_HASH(
   MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
   "RetroArch ekibine tanılama bilgileri göndermek için iki kez daha basın."
   )
MSG_HASH(
   MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
   "RetroArch ekibine tanılama bilgileri göndermek için bir kez daha basın."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Global ses karıştırıcı sesi"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Netplay taraması tamamlandı."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Üzgünüz, uygulanamadı: içerik istemeyen çekirdekler netplay'e katılamaz."
   )
MSG_HASH(
   MSG_NATIVE,
   "Özgün"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Bilinmeyen netplay komutu alındı"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Dosya zaten mevcut. Yedek ara belleğe kaydediliyor"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "Bağlantı kaynağı: \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Bağlantı kaynağı: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Netplay Port Eşlemesi Başarılı"
   )
MSG_HASH(
   MSG_PRIVATE_OR_SHARED_ADDRESS,
   "Harici ağın özel veya paylaşılan bir adresi var. Bir geçiş sunucusu kullanmayı düşünün."
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Netplay UPnP Port Eşlemesi Başarısız"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "Hiçbir argüman sağlanmadı ve menü görüntülenmedi, yardım görüntüleniyor..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Disk sistem tepsisi ayarı"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "İstemci bekleniyor..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "Kurduğunuz oda internete bağlanamıyor."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Oyundan ayrıldın"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "%u oyuncu olarak katıldınız"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s Giriş cihazlarıyla katıldınız"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "Oyuncu %.*s oyunu terk etti"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s oyuncu olarak katıldı %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s giriş cihazlarıyla birlikte katıldı %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Diğer eş RetroArch çalıştırmadığı veya RetroArch'ın eski bir sürümünü çalıştırdığı için bir netplay bağlantı girişimi başarısız oldu."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Netplay eşi eski bir RetroArch sürümünü kullanıyor. Bağlantı gerçekleştiremez."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "UYARI: Netplay eşi farklı bir RetroArch sürümünü çalıştırıyor. Sorun oluşursa, aynı sürümü kullanın."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Netplay eşi farklı bir çekirdek kullanıyor. Bağlantı gerçekleştiremez."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "UYARI: Netplay eşi, çekirdeğin farklı bir sürümünü çalıştırıyor. Sorun oluşursa, aynı sürümü kullanın."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Bu çekirdek, platformlar arası netplay desteklemiyor"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Bu çekirdek, farklı platformlar arası netplay desteklemiyor"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Netplay sunucu parolasını gir:"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Netplay sohbet mesajını girin:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Bu kullanıcıdan bağlantıya izin vermek istiyor musunuz:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Yanlış parola"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" bağlantısı kesildi"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "NetPlay istemci bağlantısı kesildi"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Netplay bağlantısı kesildi"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "Oynama izniniz yok"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "Boş oyuncu yuvası yok"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "İstenen giriş cihazları mevcut değil"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Oynatma kipine geçilemiyor"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Netplay eşi \"%s\" duraklatıldı"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Takma adınız \"%s\" olarak değiştirildi"
   )
MSG_HASH(
   MSG_NETPLAY_KICKED_CLIENT_S,
   "İstemci atıldı: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_KICK_CLIENT_S,
   "İstemci atılamadı: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_BANNED_CLIENT_S,
   "İstemci yasaklandı: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_FAILED_TO_BAN_CLIENT_S,
   "İstemci yasaklanamadı: \"%s\""
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_PLAYING,
   "Oynanıyor"
   )
MSG_HASH(
   MSG_NETPLAY_STATUS_SPECTATING,
   "İzleyici"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_DEVICES,
   "Cihazlar"
   )
MSG_HASH(
   MSG_NETPLAY_CHAT_SUPPORTED,
   "Sohbet Desteği"
   )
MSG_HASH(
   MSG_NETPLAY_SLOWDOWNS_CAUSED,
   "Yavaşlama Nedenleri"
   )

MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Ses seviyesi"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Otomatik algıla"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Durum kaydından otomatik yükle"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Yetenekler"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Netplay sunucusuna bağlanılıyor"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Porta bağlanılıyor"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Yuvaya bağlanılıyor"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "Çekirdek listesi getiriliyor..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Çekirdek listesi alınamadı!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "En son sürüm zaten kurulu: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Çekirdek güncelleniyor: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Çekirdek indiriliyor: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Çekirdek çıkarılıyor: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Çekirdek kuruldu: "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Çekirdek kurulamadı: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Çekirdekler taranıyor..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Çekirdek kontrol ediliyor: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Tüm kurulu çekirdekler en son sürümde"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Desteklenen tüm çekirdekler Play Store sürümlerine geçti"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "çekirdekler güncellendi: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "atlanan çekirdekler: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Çekirdek güncelleme devre dışı - çekirdek kilitli: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Çekirdekler sıfırlanıyor: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Çekirdekleri sıfırla: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Oynatma listesi temizleniyor: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Oynatma listesi temizlendi: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Yenileme başarısız - oynatma listesi geçerli bir tarama kaydı içermiyor: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Yenileme başarısız - geçersiz/eksik içerik dizini: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Yenileme başarısız - geçersiz/eksik sistem adı: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Yenileme başarısız - geçersiz çekirdek: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Yenileme başarısız - geçersiz/eksik arcade DAT dosyası: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Yenileme başarısız - arcade DAT dosyası çok büyük (yetersiz bellek): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Sık kullanılanlara eklendi"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Sık kullanılan eklenemedi: oynatma listesi dolu"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Çekirdek ayarla: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "Oynatma listesi girişi çekirdek ilişkilendirmesi sıfırlandı."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Eklenen disk"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Disk eklenemedi"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Uygulama Dizini"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Hile değişikliklerini uygulanıyor."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Yama uygulanıyor: %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Gölgelendirici uygulanıyor"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Ses kapatıldı."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Ses açıldı."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Kontrolcü profili kaydedilirken hata oluştu."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Kontrolcü profili başarıyla kaydedildi."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Otomatik kaydetme başlatılamadı."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Durumu otomatik kaydet"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Komut arabirimini bağlantı noktasına getir"
   )
MSG_HASH(
   MSG_BYTES,
   "bayt"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Yeni yapılandırma yolu çıkarılamıyor. Mevcut saati kullanın."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Başarımlar için Zorlu Kip Etkin, durum kaydı ve geri sarma devre dışı bırakıldı."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "Bilinen sihirli sayılarla karşılaştırılıyor..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "API'ye karşı derlendi"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Yapılandırma dizini ayarlanmadı. Yeni yapılandırma kaydedilemiyor."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Bağlı"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "İçerik CRC32'leri farklıdır. Farklı oyunlar kullanılamaz."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "Ping bu ana bilgisayar için çok yüksek."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "İçerik yüklemesi atlandı. Uygulama tek başına yükleyecektir."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Çekirdek durum kayıtlarını desteklemiyor."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Çekirdek seçenekleri dosyası başarıyla oluşturuldu."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "Çekirdek seçenekleri dosyası başarıyla kaldırıldı."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Tüm temel seçenekler varsayılana sıfırlandı."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "Çekirdek seçenekleri şuraya kaydedildi:"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "Çekirdek seçenekleri şuraya kaydedilemedi:"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Bir sonraki sürücü bulunamadı"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Uyumlu sistem bulunamadı."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Mevcut veri parçası bulunamadı"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "Veri yolu açılamadı"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "İçerik dosyası okunamadı"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Film başlığı okunamadı."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Film durumu okunamadı."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "CRC32 sağlama toplamı içerik dosyası ile kayıt dosyası başlığında kaydedilen içerik sağlama toplamı arasındaki uyumsuzluk var, yeniden oynatılınca çözülebilir."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Özel zamanlama verildi"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Sıkıştırmayı açma zaten devam ediyor."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Sıkıştırmayı açma başarısız oldu."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Tespit edilen görünüm alanı"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Doğrulanmış bir içerik düzeltme eki bulamadınız."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Cihazı doğrulanmış bağlantı noktasından çıkarın."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Sanal disk tepsisi kapalı."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Sanal disk tepsisi çıkarıldı."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "İndiriliyor"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "indeks"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "İndirme başarısız"
   )
MSG_HASH(
   MSG_ERROR,
   "Hata"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Libretro çekirdeği içerik gerektirir, hiçbir şey sağlanmadı."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Libretro çekirdeği özel içerik gerektirir, hiçbir şey sağlanmadı."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Çekirdek VFS desteklemiyor ve yerel bir kopyadan yükleme başarısız oldu"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Argümanlar ayrıştırılırken hata oluştu."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Çekirdek seçenek dosyası kaydedilirken hata oluştu."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "Çekirdek seçenek dosyası silinirken hata oluştu."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Yeniden yapılandırma dosyası kayıt hatası."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Yeniden yapılandırma dosyası kaldırma hatası."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Gölgelendirici hazır ayarı kaydedilirken hata oluştu."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Harici Uygulama Dizini"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Ayıklanıyor"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Dosya ayıklanıyor"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Yapılandırma işlemi kaydedilemedi"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Gelen izleyici kabul edilemedi."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Yamalı içerik için bellek ayrılamadı..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Gölgelendirici uygulanamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Gölgelendirici ön ayarı uygulanamadı:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Soket bağlanamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Dizin oluşturulamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Sıkıştırılmış dosyadan içerik alınamadı"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "İstemciden takma ad alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Yükleme başarısız"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "İçerik yükleme başarısız"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Film dosyası yüklenemedi"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Kaplama yüklenemedi."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Durum kaydı yüklenemedi"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Libretro çekirdeği açılamadı"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Yama yapılamadı"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "İstemciden başlık alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Takma ad alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Ana bilgisayardan takma ad alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Ana bilgisayardan takma ad boyutu alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "Ana bilgisayardan SRAM verileri alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Disk tepsiden çıkartılamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Geçici dosya kaldırılamadı"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "SRAM kaydedilemedi"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "SRAM yüklenemedi"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Durum kaydı kaydedilemedi"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Takma ad gönderilemedi."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Takma ad boyutu gönderilemedi."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Takma ad diğer cihaza gönderilemedi."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Sunucuya takma ad gönderilemedi."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "SRAM verileri istemciye gönderilemedi."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Ses sürücüsü başlatılamadı. Ses olmadan devam edilecek."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Ekran kaydı başlatılamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Kayıt başlatılamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Ekran görüntüsü alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Durum yüklemesi geri alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Durum kaydı geri alınamadı."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Ses açılamadı."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Kritik hata alındı"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Dosya bulunamadı"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Otomatik durum kaydı bulundu"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Disk etiketi bulundu"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Dosyada ilk veri parçasını bulundu"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Son durum yuvası bulundu"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Gölgelendirici bulundu"
   )
MSG_HASH(
   MSG_FRAMES,
   "Kareler"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Oyuna özel çekirdek seçenekleri şurada bulunur"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Klasöre özel çekirdek seçenekleri şurada bulunur"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Geçersiz disk indeksi var."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Fare durumunu yakala"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Oyun Odaklaması aç"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Oyun Odaklaması kapat"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Libretro çekirdek donanımdır. Sonrası gölgeli kayıt kullanmanız gerekir."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "Sağlama toplamı CRC32 ile eşleşmedi."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Hile Girişi"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Hile Dosya İsmi Girişi"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Önceden Ayarlanmış Dosya Adı Gir"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Başlığı Yeniden Adlandır"
   )
MSG_HASH(
   MSG_INTERFACE,
   "Arayüz"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Dahili Depolama"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Çıkartılabilir Depolama"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Geçersiz kullanıcı adı boyutu."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "bayt cinsinden"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "megabayt cinsinden"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "gigabayt cinsinden"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "bu libretro uygulamasından farklı bir libretro sürümüne karşı derlenmiştir."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Libretro için ön uç"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Yuva #%d üstünden durum yüklendi."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Yuva #-1 üstünden durum yüklendi (otomatik)."
   )
MSG_HASH(
   MSG_LOADING,
   "Yükleniyor"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Bir veya daha fazla ürün yazılımı dosyası eksik"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "İçerik dosyası yükleniyor"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Geçmiş dosyası yükleniyor"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Sık kullanılanlar dosyası yükleniyor"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Durum yükleniyor"
   )
MSG_HASH(
   MSG_MEMORY,
   "Bellek"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
   "Giriş tekrar film dosyası geçerli bir BSV1 dosyası değil."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Giriş tekrar film biçiminin farklı bir serileştirici sürümü var gibi görünüyor. Büyük olasılıkla başarısız olacaktır."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Giriş tekrar filmi oynatımı sona erdi."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Video kaydı durduruluyor."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Netplay başlatılamadı."
   )
MSG_HASH(
   MSG_NETPLAY_UNSUPPORTED,
   "Çekirdek üstünde netplay desteği yok."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "İçerik yok, kukla çekirdek başlatılıyor."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Henüz hiçbir kayıt durum kaydı üzerine yazılmadı."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Henüz bir durum yüklenmedi."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Özelleştirmeler kaydedilemedi."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Özelleştirmeler başarıyla kaydedildi."
   )
MSG_HASH(
   MSG_PAUSED,
   "Duraklatıldı."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "İlk veri parçasını okunuyor..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Kayıt yeniden boyutlandırma nedeniyle sonlandırıldı."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Kayıt ediliyor"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Hile dosyası yeniden yönlendiriliyor"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Kayıt dosyası yeniden yönlendiriliyor"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Durum kaydı yeniden yönlendiriliyor"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Yeniden yapılandırma dosyası başarıyla kaydedildi."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Yeniden yapılandırma dosyası başarıyla kaldırıldı."
   )
MSG_HASH(
   MSG_REMAP_FILE_RESET,
   "Tüm giriş yeniden eşleme seçenekleri varsayılana sıfırlanır."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Disk tepsiden kaldırıldı."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Geçici içerik dosyasını kaldır"
   )
MSG_HASH(
   MSG_RESET,
   "Sıfırlandı"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Sürücü sıfırlaması nedeniyle kaydı yeniden başlatılıyor."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Eski durum kaydı geri yüklendi."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Gölgelendirici: varsayılan gölgelendirici ön ayarı geri yükleniyor"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Kayıt dosyası dizinini geri alınıyor"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Durum kaydı dizinini geri alınıyor"
   )
MSG_HASH(
   MSG_REWINDING,
   "Geri sarılıyor."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "Bu çekirdekte serileştirilmiş durum kaydı desteği bulunmadığından geri sarma kullanılamıyor."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Geri sarma arabellek boyutuyla başlatılıyor"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Geri sarma başlatılamadı. Geri sarma devre dışı bırakılacak."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "Uygulamada baskın ses kullanıyor. Geri sarma kullanılamaz."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Geri sarma ara belleğinin sonuna erişildi."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Yeni yapılandırma kaydedildi"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Yuvaya kaydedilmiş durum #%d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Yuvaya kaydedilmiş durum #-1 (ses)."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Başarıyla kaydedildi"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "RAM türü kaydediliyor"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Durum kaydediliyor"
   )
MSG_HASH(
   MSG_SCANNING,
   "Taranıyor"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Dizin taraması tamamlandı"
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Komutlar gönderiliyor"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Tüm yamaları görmezden gelerek birkaç yama açıkça tanımlanmıştır..."
   )
MSG_HASH(
   MSG_SHADER,
   "Gölge"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Gölgelendirici hazır ayarı başarıyla kaydedildi."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Ağır Çekim."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Hızlı İleri Sar."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Ağır çekim geri sar."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "SRAM yüklemesi atlanıyor."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM kaydedilmeyecek."
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "SRAM Üzerine Yazma Engelleniyor"
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Film oynatılmaya başlanıyor."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Video kaydına başlanıyor"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Durum boyutu"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Durum yuvası"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Ekran görüntüsü alınıyor."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Ekran görüntüsü kaydedildi"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Başarım Kilidi Açıldı"
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Küçük resim türünü değiştir"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Tam ekran küçük resimler"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Üst verileri değiştir"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Küçük resim yok"
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Çıkmak için tekrar basın..."
   )
MSG_HASH(
   MSG_TO,
   "için"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Durum yüklemesi geri alındı."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Durum kaydı geri alınıyor."
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Bilinmiyor"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Devam ediyor."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Tanınmayan komut \"%s\" alınan.\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Yeni yapılandırma için çekirdek ismi kullanılıyor."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Libretro kukla çekirdeği kullanılıyor. Kayıt atlanıyor."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Cihazı geçerli bir port üstünden bağlayın."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Cihaz bağlantı noktasından ayrılıyor"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Yeniden başlatılıyor..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Kapatılıyor..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Libretro API sürümü"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Görüntü alanı boyutu hesaplaması başarısız! Ham veriler kullanılmaya devam edilecek. Muhtemelen düzgün çalışmayacak..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Sanal disk tepsisi çıkarılamadı."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Sanal disk tepsisi kapatılamadı."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "\"%s\" konumundan durum kaydı otomatik olarak yüklenemedi."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "\"%s\" konumundan durum kaydı otomatik olarak yüklendi."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "port yapılandırıldı"
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT_NR,
   "%s yapılandırıldı %u"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "port bağlantısı kesildi"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT_NR,
   "%s bağlantı kesildi %u"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "yapılandırılmamış"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_NR,
   "%s (%u/%u) yapılandırılmadı"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "yapılandırılmamış, geri dönülüyor"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK_NR,
   "%s (%u/%u) yapılandırılmadı, geri al"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Bluetooth taraması tamamlandı."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "Eşleştirme kaldırıldı. Tekrar bağlanmak/eşleştirmek için RetroArch'ı yeniden başlatın."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Wi-Fi taraması tamamlandı."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Bluetooth cihazları taranıyor..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Kablosuz ağlar taranıyor..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Wi-Fi Etkinleştiriliyor..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Wi-Fi Devre Dışı Bırakılıyor..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Wi-Fi Bağlantısı Kesiliyor..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Netplay sunucuları taranıyor..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "İçerik taraması için hazırlanıyor..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Parola Gir"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Parola doğru."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Parola yanlış."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Parola Gir"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Parola doğru."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Parola yanlış."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "Özelleştirilmiş yapılandırma yüklendi."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Oyun yeniden yapılandırma dosyası yüklendi."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "İçerik dizini yeniden yapılandırma dosyası yüklendi."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Çekirdek yeniden yapılandırma dosyası yüklendi."
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSHED,
   "Girdi yeniden eşleme seçenekleri şuraya kaydedildi:"
   )
MSG_HASH(
   MSG_REMAP_FILE_FLUSH_FAILED,
   "Girdi yeniden eşleme seçenekleri şuraya kaydedilemedi:"
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Önden-Git etkinleştirildi. Kare gecikmeleri kaldırıldı: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Önden-Git ikincil örnek etkinleştirildi. Kare gecikmeleri kaldırıldı: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Önden-Git devre dışı."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Önden-Git devre dışı bırakıldı, bu çekirdek durum kayıtlarını desteklemiyor."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "Bu çekirdek deterministik kaydetme durumu desteğine sahip olmadığı için önden git kullanılamıyor."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Durum kaydedilemedi. Önden-Git devre dışı bırakıldı."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Durum yüklenemedi. Önden-Git devre dışı bırakıldı."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "İkinci örnek oluşturulamadı. Önden-Git artık yalnızca bir örnek kullanacak."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Dosya taraması tamamlandı"
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Hile arama başarıyla başlatıldı"
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Hile araması başlatılamadı"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Arama başlatılmadı/başlatılamadı"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Yeni eşleşme sayısı = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "%u eşleşmeleri eklendi"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Eşleşme eklenemedi"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Eşleştirmeden oluşturulan kod"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Kod oluşturulamadı"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Silinmiş eşleşme"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Yetersiz oda. Azami eş zamanlı hile sayısı 100'dür."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Listenin başına yeni hile eklendi."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Listenin altına yeni hile eklendi."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Tüm hileler silindi."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Bundan önce yeni hile eklendi."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Bundan sonra yeni hile eklendi."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Hile bundan önceye kopyalandı."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Hile bundan sonraya kopyalandı."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Hile silindi."
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Disk ayarlanamadı"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Son kullanılan disk ayarlanamadı..."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "İstemciye bağlanılamadı"
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Ana bilgisayara bağlanılamadı"
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "Netplay ana bilgisayarı dolu"
   )
MSG_HASH(
   MSG_NETPLAY_BANNED,
   "Bu sunucu üstünde yasaklandınız"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Ana bilgisayardan başlık alınamadı"
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Durumları yüklemek için Zorlu Kipte Başarılar duraklatılmalı veya devre dışı bırakmanız gerekir."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Bir durum kaydı yüklendi. Başarılar mevcut oturum için Zorlu Kip devre dışı bırakıldı."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Bir hile etkinleştirildi. Başarılar mevcut oturum için Zorlu Kip devre dışı bırakıldı."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "Ustalıkla %s"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "Tamamlandı %s"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "En düşük"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Düşük"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Yüksek"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "En yüksek"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Uyarı: Eksik içerikler varsa Çevrimiçi Güncelleyici kullanın"
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Uyarı: Seçilen dil için eksik yazı tipleri varsa Çevrimiçi Güncelleyiciyi kullanın"
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Uyarı: Desteklenmeyen dil - İngilizce kullanıyor"
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Diskten aktarılıyor..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Sürücü %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Lütfen önce bir çekirdek yükle."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Sürücüden okunamadı, Aktarım iptal edildi."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Sürücüye yazılamadı. Aktarım iptal edildi."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Sürücüde takılı disk yok."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Gölgelendirici ön ayarı başarıyla kaldırıldı."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Gölgelendirici hazır ayarını kaldırma hatası."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Geçersiz arcade DAT dosyası seçildi"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "Seçilen arcade DAT dosyası çok büyük (yetersiz boş bellek)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Arcade DAT dosyası yüklenemedi (geçersiz biçim?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Geçersiz el ile tarama yapılandırması"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Doğrulanmış içerik algılanmadı"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Taranan içerik: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Mevcut girdiler kontrol et: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Taranıyor: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "M3U girdilerini temizle: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Tarama tamamladı: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Çekirdek taranıyor: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Kurulu çekirdeğin yedeği zaten var: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Yedek çekirdek: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Eski yedekleri kaldır: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Çekirdek yedekleme tamamlandı: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "Seçilen temel yedekleme zaten kurulu: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Çekirdek geri yükleniyor: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Çekirdek geri yüklemesi tamamlandı: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "Seçilen çekirdek dosyası zaten kurulu: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Çekirdek kuruluyor: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Çekirdek kurulumu tamamlandı: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "Geçersiz çekirdek dosya seçildi: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Çekirdek yedekleme başarısız: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Çekirdek geri yükleme başarısız: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Çekirdek kurulumu başarısız: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Çekirdek geri yükleme devre dışı - çekirdek kilitli: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "Çekirdek kurulumu devre dışı - çekirdek kilitli: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Çekirdek kilitlenemedi: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Çekirdek kilidi açılamadı: "
   )
MSG_HASH(
   MSG_CORE_SET_STANDALONE_EXEMPT_FAILED,
   "Çekirdek \"İçeriksiz Çekirdekler\" listesinden kaldırılamadı: "
   )
MSG_HASH(
   MSG_CORE_UNSET_STANDALONE_EXEMPT_FAILED,
   "Çekirdek 'İçeriksiz Çekirdekler' listesine eklenemedi: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "Çekirdek silme devre dışı - çekirdek kilitli: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Desteklenmeyen video kipi"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "Çekirdek bilgi dizinine yazılamıyor - çekirdek bilgi önbelleği devre dışı bırakılacak"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "Bulunan durum girişi"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Durum girişi şuradan yükleniyor"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "OyunKipine girilemedi"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "OyunKipine girilemedi - OyunKipi arka plan programının kurulu olduğundan/çalıştığından emin olun"
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_ENABLED,
   "Tam içerik kare hızına eşitleme etkinleştirildi."
   )
MSG_HASH(
   MSG_VRR_RUNLOOP_DISABLED,
   "Tam içerik kare hızına eşitleme devre dışı bırakıldı."
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Lakka'yı Güncelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Kullanıcı arabirimi adı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Lakka Sürümü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Yeniden Başlat"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Grafik Gereçleri Ölçeğini Özelleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Ekran gereçlerini çizerken el ile ölçeklendirme etkeni geçersiz kılınır. Yalnızca 'Grafik Pencere Ögelerini Otomatik Olarak Ölçeklendir' devre dışı bırakıldığında geçerlidir. Düzenlenmiş bildirimlerin, göstergelerin ve kontrolcülerin boyutunu menünün kendisinden bağımsız olarak arttırmak veya azaltmak için kullanılabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Ekran Çözünürlüğü"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Ekran Çözünürlüğü: Varsayılan"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Ekran Çözünürlüğü: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Ekran Çözünürlüğü: %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Uygulanıyor: Varsayılan"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Uygulanıyor: %dx%d\nSTART ile sıfırla"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Uygulanıyor: %dx%d - %s\nSTART ile sıfırla"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Sıfırlanan: Varsayılan"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Sıfırlanan: %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Sıfırlanan: %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Görüntüleme kipini seçin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Kapat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Harici dosya erişimini etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Windows dosya erişim izinleri ayarlarını açın"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Aç..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Sistem dosyası seçiciyi kullanarak başka bir dizin açın"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Titreşimsiz filtre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Video Gama"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Yumuşak Filtre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Bluetooth cihazlarını tarar ve bağlantı oluşturur."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Kablosuz ağları tarar ve bağlantı kurar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Wi-Fi Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Ağa Bağlan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Ağa Bağlan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Bağlantıyı Kes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "VI Ekran Genişliğini Ayarla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Aşırı Tarama Düzeltmesi (Üst)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Görüntü boyutunu belirtilen sayıda tarama çizgisi (ekranın üst tarafından alınır) azaltarak ekran aşırı tarama kırpmasını ayarlayın. Ölçekleme yapay dokulara neden olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Aşırı Tarama Düzeltmesi (Alt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Görüntü boyutunu belirtilen sayıda tarama çizgisi (ekranın alt tarafından alınır) azaltarak ekran aşırı tarama kırpmasını ayarlayın. Ölçekleme yapay dokulara neden olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Sürdürülebilir Performans Kipi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "CPU Performansı ve Gücü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "İlke"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Yönetim Kipi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "El İle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Her CPU'daki her ayrıntıyı el ile olarak ayarlamaya izin verir: yönetim, frekanslar, vb. Yalnızca ileri düzey kullanıcılar için önerilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Performans (Yönetilen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Varsayılan ve önerilen kip. Oynatma sırasında azami performans, duraklatıldığında veya menülere göz atarken güç tasarrufu sağlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Özel Yönetimli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Menülerde ve oyun sırasında hangi valilerin kullanılacağını seçmenize izin verir. Oyun sırasında Performans, Talep Üzerine veya Çizelge önerilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Azami Performans"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Daima azami performans: en iyi deneyim için en yüksek frekanslar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Asgari Güç"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Güç tasarrufu için mevcut en düşük frekansı kullanın. Pille çalışan cihazlarda kullanışlıdır, ancak performans önemli ölçüde azalacaktır."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Dengeli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Mevcut iş yüküne uyum sağlar. Çoğu cihaz ve emülatör ile iyi çalışır ve güç tasarrufuna yardımcı olur. Zorlu oyunlar ve çekirdekler bazı cihazlarda performans düşüşüne neden olabilir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Asgari Frekans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Azami Frekans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Asgari Çekirdek Frekansı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Azami Çekirdek Frekansı"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "CPU Yönetici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Çekirdek Yönetici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Menü Yönetici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "Oyun Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Performansı iyileştirebilir, gecikmeyi azaltabilir ve ses bozulma sorunlarını çözebilir. Çalışması için https://github.com/FeralInteractive/gamemode gerekir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "PAL60 Kipi Kullan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "RetroArch Yeniden Başlat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Çıkın ve RetroArch'ı yeniden başlatın. Belirli menü ayarlarının etkinleştirilmesi için gereklidir (örneğin, menü sürücüsünü değiştirirken)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Kareleri Engelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Ön Dokunma Tercihi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Dokunmatik"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Klavye Kontrolcü Eşlemesi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Klavye Kontrolcü Eşleme Türü"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Ufak Klavye"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Giriş Engelleme Zaman Aşımı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Tam bir girdi örneği almak için beklenecek milisaniye sayısı. Eş zamanlı düğmeye basma ile ilgili sorunlarınız varsa kullanın (Android için)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "'Yeniden Başlat'ı Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "'Yeniden Başlat' seçeneğini gösterir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "'Kapatı' Göster"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "'Kapat' seçeneğini göster."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Şifreli"
   )
MSG_HASH(
   MSG_INTERNET,
   "İnternet"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "İnternet (Röle)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "İnternet (Bağlanmadı)"
   )
MSG_HASH(
   MSG_LOCAL,
   "Yerel"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Dahili Depolama Durumu: Okuma/Yazma"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Dahili Depolama Durumu: Salt Okunur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Ekran Parlaklığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Ekran parlaklığını arttır veya azalt."
   )

#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
   "GPU Hız Aşırtma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE,
   "GPU hızını arttır yada düşür."
   )
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "CPU Hız Aşırtma"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "CPU hız aşımını değiştir."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Bluetooth durumunu belirler."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Hizmetler"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "İşletim sistemi düzeyinde servisleri yönetin."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Ağ klasörlerini SMB protokolü ile paylaş."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Uzaktan komut satırına erişmek için SSH kullanın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Wi-Fi Erişim Noktası"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Wi-Fi Erişim Noktasını etkinleşti veya devre dışı bırak."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Zaman dilimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Konumunuza göre tarih ve saati ayarlamak için saat dilimini seçin."
   )
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Wi-Fi Erişim Noktası Kapatılıyor."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Wi-Fi '%s' bağlantısı kesiliyor"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Wi-Fi '%s' ağına bağlanılıyor"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[SSID Yok]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Wi-Fi Erişim Noktası zaten başlatıldı"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Wi-Fi Erişim Noktası çalışmıyor"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Wi-Fi Erişim Noktası SSID=%s ve Passkey=%s ile başlatılıyor"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Wi-Fi Erişim Noktası yapılandırma dosyası oluşturulamadı."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Yanlış yapılandırma dosyası - %s içinde APNAME veya PASSWORD bulunamadı"
   )
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Fare Ölçeği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Wiimote light gun hızı için x/y ölçeğini ayarlayın."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Dokunmatik Ölçeği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "İşletim sistemi düzeyinde ölçeklendirmeye uyum sağlamak için dokunmatik ekran koordinatlarının x/y ölçeğini ayarlayın."
   )
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "RGA Ölçeklendirme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA ölçeklendirme ve bikübik filtreleme. Gereçleri kırabilir."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Bağlama Özgü Ölçekleme"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Donanım bağlamı ölçeği (mümkün ise)."
   )
#endif
#ifdef _3DS
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEW3DS_SPEEDUP_ENABLE,
   "New3DS Saat Hızı / L2 Önbellek Etkin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NEW3DS_SPEEDUP_ENABLE,
   "New3DS saat hızı (804MHz) ve L2 önbelleği etkinleştirir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "3DS Alt Ekran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Durum bilgilerinin alt ekranda görüntülenmesini etkinleştir. Pil ömrünü uzatmak ve performansı artırmak için devre dışı bırak."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "3DS Görüntüleme Kipi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "3B ve 2B ekran kipi arasında seçim yapar. '3B' kipinde pikseller kare şeklindedir ve Hızlı Menü görüntülenirken derinlik efekti uygulanır. '2B' kipi en iyi performansı sağlar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_3D,
   "3B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D,
   "2B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2B (Piksel Izgara Efekti)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2B (Yüksek Çözünürlük)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Retroarch menüsüne\ngitmek için Dokunmatik\nEkran'a dokunun"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "İçerik bulunamadı"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Hayır\nVeri"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "Hayır Ekran\nGörüntüsü"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Oyuna Devam Et"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Oluştur\nGeri Yükleme\nNoktası"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Geri Yükleme\nNoktasından\nYükle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_ASSETS_DIRECTORY,
   "Alt Ekran İçerik Dizini"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_ASSETS_DIRECTORY,
   "Alt ekran içerik dizini. Dizin 'bottom_menu.png' içermelidir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_ENABLE,
   "Yazı Tipini Etkinleştir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_ENABLE,
   "Alt menü yazı tipini göster. Alt ekranda düğme açıklamalarını görüntülemeyi etkinleştirin. Bu durum kaydı tarihini hariç tutar."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_RED,
   "Yazı Tipi Rengi Kırmızı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_RED,
   "Alt ekran yazı tipi kırmızı rengini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_GREEN,
   "Yazı Tipi Rengi Yeşil"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_GREEN,
   "Alt ekran yazı tipi yeşil rengini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_BLUE,
   "Yazı Tipi Rengi Mavi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_BLUE,
   "Alt ekran yazı tipi mavi rengini ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_COLOR_OPACITY,
   "Yazı Tipi Rengi Şeffaflığı"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_COLOR_OPACITY,
   "Alt ekran yazı tipi şeffaflığını ayarlayın."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BOTTOM_FONT_SCALE,
   "Yazı Tipi Ölçeği"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BOTTOM_FONT_SCALE,
   "Alt ekran yazı tipi ölçeğini ayarlayın."
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Tarama Tamamlandı.<br><br>\nİçeriğin doğru bir şekilde taranabilmesi için şunları yapmanız gerekir:\n<ul><li>önceden indirilmiş uyumlu bir çekirdeğe sahip olmak</li>\n<li>\"Temel Bilgi Dosyaları\" nı Çevrimiçi Güncelleyici aracılığıyla güncelle</li>\n<li>\"Veritabanları\" nı Çevrimiçi Güncelleyici aracılığıyla güncelle</li>\n<li>Yukarıdakilerden biri yeni yapılmışsa RetroArch'ı yeniden başlat</li></ul>\nSon olarak, içeriğin mevcut veritabanlarıyla eşleşmesi gerekir <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">burdan</a>. Hala çalışmıyorsa, düşünün <a href=\"https://www.github.com/libretro/RetroArch/issues\">hata raporu gönder</a>."
   )
#endif
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_ENABLED,
   "Dokunmatik fare etkin"
   )
MSG_HASH(
   MSG_IOS_TOUCH_MOUSE_DISABLED,
   "Dokunmatik fare devre dışı"
   )

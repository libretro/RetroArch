#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
    "GPU Hız aşırtma"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE,
    "Switch GPU'sunu Hız aşırtma veya hız düşürme yap"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_BACKLIGHT_CONTROL,
    "Ekran parlaklığı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_BACKLIGHT_CONTROL,
    "Switch'in ekran parlaklığını arttırın veya azaltın"
    )
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
    "İşlemci Hızaşırtma"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
    "Switch işlemcisine hız aşırtma yap"
    )
#endif
MSG_HASH(
    MSG_COMPILER,
    "Derleyici"
    )
MSG_HASH(
    MSG_UNKNOWN_COMPILER,
    "Bilinmeyen derleyici"
    )
MSG_HASH(
    MSG_NATIVE,
    "Özgün")
MSG_HASH(
    MSG_DEVICE_DISCONNECTED_FROM_PORT,
    "Cihaz bağlantı noktasından çıkarıldı"
    )
MSG_HASH(
    MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
    "Bilinmeyen netplay komutu alındı"
    )
MSG_HASH(
    MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
    "Dosya zaten mevcut. Yedekleme arabelleğine kaydetiliyor."
    )
MSG_HASH(
    MSG_GOT_CONNECTION_FROM,
    "Gelen bağlantı: \"%s\""
    )
MSG_HASH(
    MSG_GOT_CONNECTION_FROM_NAME,
    "Gelen bağlantı: \"%s (%s)\""
    )
MSG_HASH(
    MSG_PUBLIC_ADDRESS,
    "Bağlantı Noktası Eşleme Başarılı"
    )
MSG_HASH(
    MSG_UPNP_FAILED,
    "Bağlantı Noktası Eşlemesi Başarısız Oldu"
    )
MSG_HASH(
    MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
    "Argüman yok ve menü yerleşik değil, yardım görüntüleniyor..."
    )
MSG_HASH(
    MSG_SETTING_DISK_IN_TRAY,
    "Disk tepsisindeki diski ayarlama"
    )
MSG_HASH(
    MSG_WAITING_FOR_CLIENT,
    "İstemci bekleniyor..."
    )
MSG_HASH(
    MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
    "Oyundan çıktın"
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
    "Eşiniz RetroArch'ı çalıştırmadığından veya RetroArch'ın eski bir sürümünü çalıştırdığından netplay bağlantı girişimi başarısız oldu."
    )
MSG_HASH(
    MSG_NETPLAY_OUT_OF_DATE,
    "Netplay eşi RetroArch'ın eski bir sürümünü kullanıyor. Bağlantı gerçekleştiremez."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_VERSIONS,
    "UYARI: Bir netplay eşi RetroArch'ın farklı bir sürümünü çalıştırıyor. Sorun oluşursa, aynı sürümü kullanın."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_CORES,
    "Netplay eşi farklı bir Çekirdek kullanıyor. Bağlantı gerçekleştiremez."
    )
MSG_HASH(
    MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
    "UYARI: Netplay eşi, Çekirdeğin farklı bir sürümünü çalıştırıyor. Sorun oluşursa, aynı sürümü kullanın."
    )
MSG_HASH(
    MSG_NETPLAY_ENDIAN_DEPENDENT,
    "Bu Çekirdek, sistemler arasında mimariler arası Netplay'i desteklemiyor"
    )
MSG_HASH(
    MSG_NETPLAY_PLATFORM_DEPENDENT,
    "Bu Çekirdek, mimariler arası Netplay'i desteklemiyor"
    )
MSG_HASH(
    MSG_NETPLAY_ENTER_PASSWORD,
    "NetPlay sunucu şifresini girin:"
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
    "\"%s\" bağlantısı koptu"
    )
MSG_HASH(
    MSG_NETPLAY_SERVER_HANGUP,
    "NetPlay istemcisinin bağlantısı kesildi"
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
    "Oynatma moduna geçilemiyor"
    )
MSG_HASH(
    MSG_NETPLAY_PEER_PAUSED,
    "Netplay eşi \"%s\" durdurdu"
    )
MSG_HASH(
    MSG_NETPLAY_CHANGED_NICK,
    "\"%s\"Takma adınız olarak değiştirildi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
    "Donanım tarafından oluşturulan Çekirdekler kendi özel bağlamlarını kullanır. Donanım durumunun çerçeveler arasında değişmesi gerektiğini varsaymaktan kaçınır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
    "Menü için yatay animasyonu etkinleştirin. Performans etkisi olacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SETTINGS,
    "Menü ekranı görünüm ayarlarını düzenler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
    "CPU ve GPU’yu sabit senkronize edin. Performansdan ödün vererek geçikmeyi azaltır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_THREADED,
    "Gecikme ve daha fazla video takılma pahasına performansı artırır. Aksi takdirde tam hız elde edemiyorsanız kullanın."
    )
MSG_HASH(
    MSG_AUDIO_VOLUME,
    "Ses seviyesi"
    )
MSG_HASH(
    MSG_AUTODETECT,
    "Otomatik tespit edildi"
    )
MSG_HASH(
    MSG_AUTOLOADING_SAVESTATE_FROM,
    "Belirtilen yoldan otomatik yükleme"
    )
MSG_HASH(
    MSG_CAPABILITIES,
    "Kabiliyetler"
    )
MSG_HASH(
    MSG_CONNECTING_TO_NETPLAY_HOST,
    "Netplay sunucusuna bağlanma"
    )
MSG_HASH(
    MSG_CONNECTING_TO_PORT,
    "Port'a bağlanma"
    )
MSG_HASH(
    MSG_CONNECTION_SLOT,
    "Bağlantı yuvası"
    )
MSG_HASH(
    MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
    "Üzgünüz, uygulanamadı: içerik talep etmeyen Çekirdekler Netplay'e katılamaz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
    "Şifre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
    "Cheevos Hesapları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
    "Kullanıcı adı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
    "Hesaplar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
    "Hesap Listesi Sonu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
    "RetroAchievements"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
    "Başarılar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
    "Hardcore Modunda Başarıları Duraklat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
    "Hardcore Modunda Başarıları Sürdür"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
    "Başarılar (Hardcore)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
    "İçeriği Tara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
    "Yapılandırma Dosyası"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TAB,
    "İçeriği içe aktar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
    "Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
    "Sor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
    "İçerikler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
    "Blok Kareleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
    "Cihaz"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
    "Ses"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
    "DSP Eklentisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
    "Ses"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
    "Ses Filtresi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
    "Turbo/Deadzone"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
    "Ses Geçikmesi (ms)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
    "Maksimum Zamanlama Eğimi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
    "Sustur"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
    "Çıkış hızı (Hz)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
    "Dinamik Ses Hızı Kontrolü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
    "Ses Resampler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
    "Ses"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
    "Senkronizasyon"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
    "Ses Arttırma (dB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
    "WASAPI Özel mod"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
    "WASAPI Float Biçimi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
    "WASAPI Shared Buffer Length"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
    "SaveRAM Otomatik kaydetme aralığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
    "Üzerine Yazılmış Dosyaları Otomatik Olarak Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
    "Remap Dosyalarını Otomatik Olarak Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
    "Evrensel Çekirdek Seçenekleri Dosyalarını Kullan"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
    "Tüm çekirdek seçenekleri ortak bir ayarlar dosyasına kaydedin (retroarch-core-options.cfg). Devre dışı bırakıldığında, her bir çekirdek için seçenekler RetroArch'ın 'Config' dizininde ayrı bir çekirdeğe özgü klasör / dosyaya kaydedilir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
    "Gölgelendirici Hazır Ayarlarını Otomatik Olarak Yükleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
    "Geri"
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
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
    "Çıkış"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
    "Aşağı kaydır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
    "Yukarı kaydır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
    "Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
    "Klavye aç/kapa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
    "Menu aç/kapa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
    "Temel menü kontrolleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
    "Onayla/OK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
    "Bilgi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
    "Çıkış"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
    "Yukarı kaydır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
    "Varsayılanlar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
    "Klavye aç/kapa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
    "Menu aç/kapa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
    "Savestate'i yüklerken SaveRAM'in üzerine yazma"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE,
    "Bluetooth"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
    "Buildbot İçerikleri URL’si"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
    "Önbellek"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
    "Kameraya İzin Ver"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
    "Kamera"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT,
    "Hile"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
    "Değişiklikleri uygula"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
    "Yeni Hile Kodu Aramaya Başla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
    "Aramaya Devam Et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
    "Hile Dosyası"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
    "Hile Dosyası"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
    "Hile Dosyası Yükle (Değiştir)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
    "Hile Dosyası Yükle (Ekleme)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
    "Hile Dosyasını Farklı Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
    "Hile Geçişleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
    "Açıklama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Hardcore Modu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
    "Liderler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
    "Başarı Rozetleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
    "Kilitli"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
    "RetroAchievements"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
    "Resmi Olmayan Başarıları Test Et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
    "Açılmış"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY_HARDCORE,
    "Hardcore"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
    "Verbose Modu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
    "Otomatik Ekran Görüntüsü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
    "İçeriği Kapat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIG,
    "Yapılandırma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
    "Yapılandırmayı yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
    "Yapılandırma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
    "Çıkışta Yapılandırmayı Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
    "Veritabanı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
    "İçerik"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
    "Geçmiş Listesi Boyutu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
    "Favoriler Listesi Boyutu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
    "Sık kullanılanlar oynatma listesindeki giriş sayısını sınırlayın. Sınıra ulaşıldığında, eski girişler kaldırılıncaya kadar yeni eklemeler engellenir. -1 değerinin ayarlanması 'sınırsız' (99999) girişe izin verir. UYARI: Değerin düşürülmesi mevcut girişleri siler!"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
    "Girişleri kaldırmaya izin ver"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
    "Hızlı Menü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
    "İndirilenler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
    "İndirilenler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
    "Hileler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
    "Çekirdek Sayaçları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
    "Çekirdek ismini göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
    "Çekirdek Bilgisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
    "Yaratıcı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
    "Kategoriler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
    "Çekirdek etiketi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
    "Çekirdek ismi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
    "Yazılım(lar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
    "Lisans(lar)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
    "İzinler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
    "Desteklenen uzantılar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
    "Sistem üreticisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
    "Sistem adı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
    "Kontroller"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_LIST,
    "Çekirdek Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
    "Çekirdek Yükleme veya Geri Yükleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
    "Çekirdek yüklemesi başarısız oldu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
    "Çekirdek kurulumu başarılı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
    "Seçenekler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
    "Çekirdek"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
    "Otomatik Olarak Bir Çekirdek Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
    "İndirilen arşivi otomatik olarak çıkart"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
    "Çekirdekler için Buildbot URL'si"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
    "Çekirdek Güncelleyici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
    "Güncelleyici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
    "CPU Mimarisi:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CPU_CORES,
    "CPU Çekirdekleri:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
    "İmleç"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
    "İmleç Yöneticisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
    "Özel Orantı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
    "Veritabanı Yöneticisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
    "Veri Tabanı Seçimi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
    "Kaldır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FAVORITES,
    "Başlangıç dizini"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
    "<içerik dizini>"
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
    MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
    "Dizin bulunamadı."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
    "Dizin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_INDEX,
    "Disk Dizini"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
    "Disk Kontrolü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DONT_CARE,
    "Don't care"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
    "İndirilenler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
    "Çekirdek indir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
    "İçerik İndir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
    "Sürücüler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
    "Çekirdeği Kapatmada Kukla Kullan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
    "Yüklemeden Önce Eksik Ürün Yazılımı Denetimi Yapın"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
    "Dinamik Arkaplan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
    "Dinamik Arkaplanlar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
    "Başarılar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FALSE,
    "Yanlış"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
    "Maksimum çalışma hızı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
    "Favoriler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FPS_SHOW,
    "Ekran Hızı Görüntüleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
    "Bellek Ayrıntılarını Dahil Et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
    "Maksimum Çalışma Hızını Sınırla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
    "Tam İçerik Kare Hızına Eşitle (G-Sync, FreeSync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
    "Frame Throttle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
    "Frontend Sayaçları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
    "İçeriğe Özgü Çekirdek Seçeneklerini Otomatik Olarak Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
    "Oyun seçenekleri dosyası oluştur"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
    "Oyun seçenekleri dosyasını kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP,
    "Yardım"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
    "Ses/Video Sorun Giderme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
    "Sanal Gamepad Yerleşimini Değiştirme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
    "Temel Menü Kontrolleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_LIST,
    "Yardım"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
    "İçerik Yükleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
    "İçerik Taraması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
    "Çekirdek Nedir?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
    "Geçmiş Listesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
    "Geçmiş"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
    "Horizontal Menu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
    "Görüntü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INFORMATION,
    "Bilgi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
    "Bilgi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
    "Analog'dan Digital'e Tipi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
    "Bütün kullanıcılar Menüyü kontrol eder"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
    "Sol Analog X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
    "Sol analog X- (sol)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
    "Sol analog X+ (right)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
    "Sol Analog Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
    "Sol analog Y- (yukarı)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
    "Sol analog Y+ (aşağı)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
    "Sağ Analog X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
    "Sağ analog X- (sol)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
    "Sağ analog X+ (sağ)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
    "Sağ Analog Y"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
    "Sağ analog Y- (yukarı)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
    "Sağ analog Y+ (aşağı)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
    "Gun Trigger"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
    "Gun Reload"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
    "Gun Aux A"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
    "Gun Aux B"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
    "Gun Aux C"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
    "Gun Start"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
    "Gun Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
    "Gun D-pad Up"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
    "Gun D-pad Down"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
    "Gun D-pad Left"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
    "Gun D-pad Right"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
    "Otomatik Konfigürasyon"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
    "Giriş Düğmesi Eksen Eşiği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
    "Analog Ölü bölge"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
    "Analog Hassasiyeti"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
    "Menü TAMAM & İptal tuşlarını değiştirin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
    "Tümünü Bağla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
    "Tümünü Varsayılana Bağla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
    "Basılı Tutmanın Zaman Aşımını"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
    "Bind Hold"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
    "Giriş Bloğu Zaman Aşımı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
    "Sınırlanmamış Çekirdek Giriş Tanımlayıcılarını Gizle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
    "Giriş Tanımlayıcı Etiketlerini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
    "Aygıt İndeksi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
    "Aygıt Tipi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
    "Fare İndeksi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
    "Girdi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
    "Duty Cycle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
    "Kısayol Tuşları Atamaları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
    "Keyboard Gamepad Mapping"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
    "A tuşu (right)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
    "B tuşu (down)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
    "Aşağı D-pad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
    "L2 tuşu (tetikleme)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
    "L3 tuşu (thumb)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
    "L tuşu (shoulder)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
    "Sol D-pad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
    "R2 tuşu (trigger)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
    "R3 tuşu (thumb)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
    "R tuşu (shoulder)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
    "Sağ D-pad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
    "Select tuşu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
    "Start tuşu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
    "Yukarı D-pad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
    "X tuşu (top)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
    "Y tuşu (left)"
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
    "Tekerlek Yukarı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
    "Tekerlek Aşağı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
    "Tekerlek Sola"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
    "Tekerlek Sağa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
    "Klavye Gamepad Eşleme Türü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
    "Maksimum Kullanıcı Sayısı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
    "Menü Geçiş Gamepad Kombosu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
    "Hile indeksi -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
    "Hile indeksi +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
    "Hile açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
    "Disk eject açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
    "Bir sonraki disk"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
    "Bir önceki disk"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
    "Kısayol tuşları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
    "Basılı Tutarak Hızlı İleri Sarma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
    "Aç/kapat Yaparak Hızlı İleri Sarma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
    "Frameadvance"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
    "Hata Ayıklama Bilgisi Gönder"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
    "FPS açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
    "Netplay sunucu açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
    "Tam ekran geçiş aç/kapa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
    "Grab mouse açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
    "Game focus açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
    "Masaüstü menüsü açma-kapamaaçma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
    "Konumu yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
    "Menü açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
    "Input replay movie record toggle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
    "Sessiz açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
    "Netplay oyna/izle geçiş modu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
    "Ekran klavyesi açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
    "Bir sonraki kaplama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
    "Pause açma-kapama"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
    "RetroArch'ı yeniden başlat"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
    "RetroArch'dan çık"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
    "Oyunu sıfırla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
    "Geri sar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
    "Hile Detayları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
    "Hile Aramaya Başla veya Devam Et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
    "Konumu Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
    "Ekran Görüntüsü Al"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
    "Sonraki Gölgelendirici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
    "Önceki Gölgelendirici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
    "Ağır Çekim için basılı tut"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
    "Ağır Çekim aç/kapa"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
    "Konum kaydı slotu -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
    "Konum kaydı slotu +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
    "Ses -"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
    "Ses +"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
    "Görüntü Kaplaması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
    "Menüde'de Kaplamayı Sakla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
    "Kaplamada Girdileri Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
    "Yerleşimli Fare İmlecini Göster"
    )MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
    "Kaplamayı Otomatik Döndür"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
    "Geçerli kaplama tarafından destekleniyorsa, ekran yönü/en boy oranıyla eşleşmesi için düzeni otomatik olarak döndürür."
    )    
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
    "Girişleri Bağlantıları Noktasını Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
    "Anket Tipi Davranışı"
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
    MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
    "Normal"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
    "Ön Dokunmayı Tercih Et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
    "Giriş yeniden yapılandırması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
    "Bu Çekim için Eşleşmeyi Yeniden Yapılandır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
    "Oto-yapılandırmayı Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
    "Girdi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
    "Ufak Klavye"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
    "Dokunmatik"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_ENABLE,
    "Turbo"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
    "Turbo Aralığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS, /* TODO/FIXME - Change user to port */
    "Kullanıcı %u Atamaları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
    "Gecikme Ayarları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
    "Dahili Depolama Durumu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
    "Oto-yapılandırma Girişi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
    "Joypad"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
    "Servisler"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED,
    "Çince (Basitleştirilmiş)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL,
    "Çince (Geleneksel)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DUTCH,
    "Flemenkçe"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ENGLISH,
    "İngilizce"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO,
    "Esperanto"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_FRENCH,
    "Fransızca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GERMAN,
    "Almanca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ITALIAN,
    "İtalyanca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_JAPANESE,
    "Japonca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_KOREAN,
    "Korece"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_POLISH,
    "Polanya Dili"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_BRAZIL,
    "Portekizce (Brezilya)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE_PORTUGAL,
    "Portekizce (Portekiz)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN,
    "Rusça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_SPANISH,
    "İspanyolca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_VIETNAMESE,
    "Vietnamca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_ARABIC,
    "Arapça"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_GREEK,
    "Yunanca"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_TURKISH,
    "Türkçe"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
    "Sol Analog"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
    "Çekirdek"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
    "Çekirdek Bilgisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
    "Çekirdek Günlük Seviyesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LINEAR,
    "Linear"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
    "Arşivi Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
    "En Sonuncuyu Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
    "İçerik Yükle"
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOAD_DISC,
      "Disk'i Yükle")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DUMP_DISC,
      "Disk'i Yazdır")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOAD_STATE,
    "Kayıtlı Konumu Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
    "Konum'u etkinleştir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
    "Konum"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
    "Günlükler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
    "Günlük Ayrıntılandırma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
    "Dosyaya günlüğünü gir"
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
    MENU_ENUM_LABEL_VALUE_MAIN_MENU,
    "Ana Menü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANAGEMENT,
    "Veri Tabanı Ayarları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
    "Menü Rengi Teması"
    )
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
    "Karanlık Mavi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
    "Yeşil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
    "Shield"
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
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
    "Alt Bilgi Opaklığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
    "Başlık Opaklığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
    "Menü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
    "Throttle Menü Kare Hızı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
    "Ayarlar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
    "Menü Linear Filtresi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
    "Sert piksel kenarlarından kenarı çıkarmak için menüye hafif bir bulanıklık ekler."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
    "Horizontal Animation"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
    "Görünüş"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
    "Arkaplan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
    "Arkaplan opaklığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MISSING,
    "Kayıp"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MORE,
    "..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
    "Fare Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
    "Multimedia"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
    "Müzik"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
    "Bilinmeyen uzantıları filtrele"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
    "Gezinme Kaydırma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NEAREST,
    "En yakın"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY,
    "Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
    "Allow Slave-Mode Clients"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
    "Netplay Kareleri Kontrol eder"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
    "Gecikme Kareleri Girişleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
    "Kare Gecikme Aralığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
    "Netplay Kareleri Geciktirir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
    "Netplay sunucusuyla bağlantıyı kes"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
    "Netplay"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
    "Netplay sunucusuna bağlan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
    "Netplay kurucu olarak başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
    "Netplay host'unu durdur"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
    "Sunucu Adresi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
    "Yerel ağı tara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
    "Netplay Client"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
    "Kullanıcı Adı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
    "Sunucu Şifresi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
    "Netplay'i Genel Olarak Duyurun"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
    "%u Aygıt Talebi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
    "Disallow Non-Slave-Mode Clients"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
    "Netplay ayarları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
    "Analog Giriş Paylaşımı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
    "Maksimum"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
    "Ortalama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
    "Dijital Giriş Paylaşımı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
    "Paylaş"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
    "Grapple"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
    "Vote"
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
    MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
    "Netplay Seyirci Modu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
    "Netplay Stateless Modu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
    "Sunucu Sadece-İzleyici Parolası"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
    "Netplay İzleyicisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
    "Netplay TCP Portu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
    "Netplay NAT Traversal"
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
    MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
    "Ağ Bilgisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
    "Ağ Gamepad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
    "Ağ Uzak Baz Bağlantı Noktası"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
    "İsteğe Bağlı Küçük Resim Yüklemeleri"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
    "Oynatma listelerine göz atarken eksik küçük resimleri otomatik olarak indirin. Yüksek bir performans etkisi vardır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
    "Ağ Ayarları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO,
    "Yok"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NONE,
    "None"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
    "Bilinmiyor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
    "Gösterilecek başarı yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE,
    "Çekirdek Yok"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
    "Kullanılabilir Çekirdek yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
    "Çekirdek bilgisi yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
    "Çekirdek seçenekleri yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
    "Gösterilecek giriş yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
    "Geçmiş yok..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
    "Bilgi yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_ITEMS,
    "Öğe yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
    "Netplay sunucuları yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
    "Ağ bulunamadı."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
    "Performans sayacı yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
    "Oynatma Listesi yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
    "Oynatma listesi yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
    "Ayarlar bulunamadı."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
    "Gölgelendirici parametreleri yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OFF,
    "KAPALI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ON,
    "AÇIK"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONLINE,
    "Çevrimiçi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
    "Çevrimiçi Güncelleyici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
    "Ekrandaki Görünüm"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
    "Ekran Kaplama"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
    "Çerçeveleri ve Ekran denetimlerini ayarlama"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
    "Video Düzeni"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
    "Video Düzenini Ayarla"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Ekrandaki Bildirimler"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
    "Ekran Bildirimlerini Ayarla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
    "Arşive Gözat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OPTIONAL,
    "İsteğe bağlı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY,
    "Kaplama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
    "Tercih Edilen Kaplamayı Otomatik Yükleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
    "Kaplama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
    "Kaplama Opaklığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
    "Kaplama Ön Ayarı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE,
    "Kaplama Büyüklüğü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
    "Ekran Üstü Kaplama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
    "PAL60 Modunu kullan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
    "Ana Dizin"
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
    MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
    "Menü etkinleştirildiğinde duraklat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
    "Arkaplanda çalıştırma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
    "Performans Sayaçları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
    "Oynatma Listeleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
    "Oynatma Listeleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
    "Oynatma Listeleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
    "Oynatma Listesi Yönetimi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
    "Seçili oynatma listesinde bakım görevleri gerçekleştirin (örneğin, varsayılan çekirdek ilişkilerini ayarlayın / sıfırlayın)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
    "Varsayılan Çekirdek"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
    "Varolan bir çekirdek ilişkisine sahip olmayan oynatma listesi girişi yoluyla içerik başlatırken kullanılacak çekirdeği belirtin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
    "Çekirdek İlişkilendirmeleri Sıfırla"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
    "Tüm oynatma listesi girişleri için mevcut çekirdek ilişkilendirmeleri kaldırın."
    )
MSG_HASH(
    MSG_PLAYLIST_MANAGER_RESETTING_CORES,
    "Çekirdek sıfırlanıyor: "
    )
MSG_HASH(
    MSG_PLAYLIST_MANAGER_CORES_RESET,
    "Çekirdek sıfırlama:: "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "Etiket Görüntüleme Modu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
    "İçerik etiketlerinin bu oynatma listesinde nasıl görüntülendiğini değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
    "Tüm etiketleri göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
    "() İçeriğini kaldır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
    "[] İçeriğini kaldır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
    "() ve [] Kaldırın"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
    "Bölge bilgisini tut"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
    "Disk indeksini tut"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
    "Bölge ve disk indeksini tut"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
    "Dokunmatik Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PORT,
    "Port"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PRESENT,
    "Present"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
    "Gizlilik"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_SETTINGS,
    "MIDI"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
    "RetroArch'ı Yeniden Başlat"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
    "RetroArch'dan Çık"
    )
#endif
MSG_HASH(MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
      "Veritabanı Girişi")
MSG_HASH(MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
      "Geçerli içerik için veritabanı bilgilerini göster")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
    "Analog desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
    "BBFC Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
    "CERO Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
    "Co-op destekli"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CRC32,
    "CRC32"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
    "Açıklama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
    "Geliştirici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
    "Edge Magazin Sayısı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
    "Edge Magazin Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
    "Edge Magazin İncelemesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
    "ELSPA Rating"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
    "Enhancement Hardware"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
    "ESRB Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
    "Famitsu Magazin Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
    "Seri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
    "Tür"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_MD5,
    "MD5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
    "İsim"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
    "Kökü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
    "PEGI Derecelendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
    "Yayımcı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
    "Çıkış Tarihi Ayı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
    "Çıkış Tarihi Yılı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
    "Rumble desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
    "Seri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SHA1,
    "SHA1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
    "İçeriği Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
    "TGDB Değerlendirmesi"
    )
#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REBOOT,
    "RCM'ye yeniden başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
    "İsim"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
    "Dosya Yolu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
    "Çekirdek"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
    "Veritabanı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
    "Oynama Süresi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
    "Son Oynama"
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REBOOT,
    "Yeniden Başlat"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
    "Kayıt Yapılandırması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
    "Kayıt Çıktısı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
    "Kayıt Yapma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
    "Özel Kayıt Yapılandırması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
    "Özel Yayın Yapılandırması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
    "Kayıt"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
    "MIDI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
    "Kayıt Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_PATH,
    "Kaydı farklı kaydet..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
    "Çıktı Dizinine Kayıtları Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE,
    "Yeniden Yapılandırma Dosyası"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
    "Yeniden Yapılandırma Dosyasını Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
    "Çekirdek Yeniden Yapılandırma Dosyasını Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
    "İçerik Dizini Yeniden Yapılandırma Dosyasını Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
    "Oyun Yeniden Konumlandırma Dosyasını Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
    "Çekirdek Yeniden Yapılandırma Dosyasını Sil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
    "Oyun Yeniden Konumlandırma Dosyasını Sil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
    "Delete Game Content Directory Remap File"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REQUIRED,
    "Gerekli"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
    "Yeniden başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
    "RetroArch'ı yeniden başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESUME,
    "Devam et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
    "Devam et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
    "RetroKeyboard"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROPAD,
    "RetroPad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
    "Analog ile RetroPad"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
    "Başarılar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
    "Geri Sarma desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
    "Geçiş Sonrası Uygula"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
    "Oyun Yüklenirken Hileleri Otomatik Uygula"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
    "Geri Sarma Öğe Boyu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
    "Geri Sarma Arabelleği (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
    "Geri Sarma Arabelleği Basamakları (MB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
    "Geri sar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
    "Hile Ayarları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
    "Hile Detayları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
    "Hile Aramaya Başla veya Devam Et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
    "Dosya Gezgini"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
    "Yapılandırma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
    "Başlangıç Ekranını Görüntüle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
    "Sağ Analog"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
    "Favorilere ekle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
    "Favorilere ekle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
    "Küçük Resim İndir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
    "Çekirdek Eşleşmesini Ayarla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
    "Çekirdek ilişkilendirilmesini sıfırla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN,
    "Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
    "Başlat"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE,
    "SAMBA"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
    "Kayıt Dosyası"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
    "Otomatik Konum Kaydedici İndeksi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
    "Otomatik Konum Yükleyici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
    "Otomatik Konum Kaydedici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
    "Konum Kaydedici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
    "Konum Kaydedici Küçük Resimleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
    "Mevcut Yapılandırmayı Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
    "Çekirdeğin Üzerine Yazılanları Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Save Content Directory Overrides"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
    "Oyunun Üzerine Yazılanları Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
    "Yeni Yapılandırmayı Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVE_STATE,
    "Konum Kaydetme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
    "Kaydetme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
    "Dizin Tarama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_FILE,
    "Dosya Tarama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
    "<Bu dizini tara>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
    "Ekran görüntüsü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
    "Ekran çözünürlüğü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SEARCH,
    "Arama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SECONDS,
    "saniye"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS,
    "Ayarlar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
    "Ayarlar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER,
    "Gölgelendirici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
    "Değişiklikleri Uygula"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
    "Gölgelendiriciler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
    "Ribbon"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
    "Ribbon (Basitleştirilmiş)"
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
    MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
    "Gelişmiş ayarları göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
    "Gizli Dosya ve Klasörleri Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHUTDOWN,
    "Kapat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
    "Ağır Çekim Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
    "Gecikmeyi Azaltmak için Önden Git"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
    "Önden Gidilecek Kare Sayısı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
    "RunAhead ikinci örneği kullansın"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
    "RunAhead Uyarıları Gizlesin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
    "Klasördeki Kayıtları Sıralayın"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
    "Klasörlerdeki Konum Kayıtlarını Sırala"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
    "Konum kayıtlarını içerik dizinine kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
    "Kayıtları içerik dizinine yazın"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
    "Sistem dosyaları içerik dizinine"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
    "Ekran görüntülerini içerik dizinine yaz"
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SSH_ENABLE,
    "SSH"
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_CORE,
    "Çekirdeği Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
    "Uzak RetroPad Bağlantısını Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
    "Start Video Processor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATE_SLOT,
    "Konum Yuvası"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATUS,
    "Durum"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
    "stdin Komutları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
    "Önerilen Çekirdekler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
    "Ekran Koruyucuyu önle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
    "System BGM"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
    "Sistem/BIOS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
    "Sistem bilgisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
    "7zip Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
    "ALSA Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
    "Yapılandırma Tarihi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
    "Cg Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
    "Cocoa Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
    "Command interface support"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
    "CoreText Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
    "CPU Özellikleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
    "Display metric DPI"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
    "Display metric height (mm)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
    "Display metric width (mm)"
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
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
    "Dynamic library Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
    "Dynamic run-time loading of libretro library"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
    "EGL Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
    "OpenGL/Direct3D render-to-texture (multi-pass shaders) support"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
    "FFmpeg Desteği"
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
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
    "Frontend identifier"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
    "Frontend name"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
    "Frontend OS"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
    "Git versiyonu"
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
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
    "JACK Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
    "KMS/EGL Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
    "Lakka Versiyonu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
    "LibretroDB desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
    "Libusb Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
    "Netplay (peer-to-peer) desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
    "Ağ komut arayüzü desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
    "Ağ Gamepadi desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
    "OpenAL Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
    "OpenGL ES Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
    "OpenGL Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
    "OpenSL Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
    "OpenVG Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
    "OSS Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
    "Overlay support"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
    "Güç Kaynağı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
    "Şarj Edildi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
    "Şarj Ediliyor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
    "Boşaltılıyor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
    "Kaynak Yok"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
    "PulseAudio desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
    "Python (gölgelendiricilerde komut dosyası desteği) desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
    "BMP Desteği (RBMP)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
    "RetroRating level"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
    "JPEG Desteği (RJPEG)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
    "RoarAudio Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
    "PNG Desteği (RPNG)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
    "RSound Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
    "TGA Desteği (RTGA)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
    "SDL2 Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
    "SDL image Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
    "SDL1.2 Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
    "Slang Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
    "Threading Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
    "Udev Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
    "Video4Linux2 Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
    "Video içerik sürücüsü"
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
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
    "Wayland Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
    "X11 Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
    "XAudio2 Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
    "XVideo Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
    "Zlib Desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
    "Ekran Görüntüsü Al"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
    "Zincir Görevleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS,
    "Küçük Resimler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
    "Top Küçük Resim"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
    "Left Thumbnails"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
    "Bottom Küçük Resim"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
    "İkinci Küçük Resim"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
    "Thumbnails Vertical Disposition"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
    "Oynatma Listesini Küçük Resimlerini Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
    "Enable display of inline downscaled thumbnails while viewing playlists. When disabled, 'Top Thumbnail' may still be toggled fullscreen by pressing RetroPad Y."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
    "Swap Thumbnails"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
    "Swaps the display positions of 'Top Thumbnail' and 'Bottom Thumbnail'."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
    "Thumbnail Downscaling Method"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
    "Resampling method used when shrinking large thumbnails to fit the display."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
    "Nearest Neighbour (Fast)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
    "Bilinear"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
    "Sinc/Lanczos3 (Slow)"
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
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X2,
    "x2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X3,
    "x3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X4,
    "x4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X5,
    "x5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X6,
    "x6"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X7,
    "x7"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X8,
    "x8"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_X9,
    "x9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_4_3,
    "4:3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9,
    "16:9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
    "16:9 (Ortalanmış)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10,
    "16:10"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
    "16:10 (Ortalanmış)"
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
    "Integer Scale"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
    "Küçük Resimler"
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
    "Küçük Resim Güncelleyici"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
    "Seçilen oynatma listesinin her girişi için ayrı ayrı küçük resimler indirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
    "Kutu Görüntüleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
    "Ekran Görüntüleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
    "Açılış Ekranları"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
    "Tarihi/saati göster"
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
     "Tarih/saat stili"
    )
MSG_HASH(
     MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
     "Menü içindeki tarih ve/veya saatin stilini değiştir."
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HMS,
     "YYYY-AA-GG SS:DD:SN"
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_YMD_HM,
     "YYYY-AA-GG SS:DD"
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MDYYYY,
     "AA-GG-YYYY SS:DD"
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS,
     "SS:DD:SN"
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HM,
     "SS:DD"
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_DM_HM,
     "GG/AA SS:DD"
    )
MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_MD_HM,
     "AA/GG SS:DD"
    )
 MSG_HASH(
     MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE_HMS_AM_PM,
     "SS:DD:SN (AM/PM)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
    "Kayan Yazı Animasyonu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
    "Uzun menü metin dizelerini görüntülemek için kullanılan yatay kaydırma yöntemini seçin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
    "Sol/Sağ Sıçrama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
    "Sola Kaydır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
    "Kayan Metin Hızı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
    "Uzun menü metni dizelerinde gezinirken animasyon hızı."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
    "Düzgün Kayan Metin"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
    "Uzun menü metin dizelerini görüntülerken yumuşak kaydırma animasyonu kullanın. Küçük bir performans etkisi var."
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
    "Geceyarısı Mavisi"
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
    "Lagoon"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER,
    "Brogrammer"
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
    "Düz Kullanıcı Arayüzü"
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
    "Hacking the Kernel"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD,
    "Nord"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NOVA,
    "Nova"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
    "One Dark"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT,
    "Palenight"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
    "Güneşleşmiş Karanlık"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
    "Güneşleşmiş Açıklık"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
    "Tango Karanlığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
    "Tango Açıklık"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ZENBURN,
    "Zenburn"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ANTI_ZENBURN,
    "Anti-Zenburn"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TRUE,
    "Var"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
    "UI Yardımcısı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
    "Açılışta UI Yardımcısını Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
    "Başlangıçta Masaüstü Menüsünü göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
    "Masaüstü Menüsü (yeniden başlatılır)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
    "Menü çubuğu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
    "Sıkıştırılmış dosya okunamıyor."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
    "Kayıtlı Konumu Yüklemeyi Geri Al"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
    "Konum Kaydetmeyi Geri Al"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UNKNOWN,
    "Bilinmeyen"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
    "Güncelleyici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
    "İçerikleri Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
    "Joypad Profillerini Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
    "Cg Gölgelendiricilerini Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
    "Hileleri Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
    "Çekirdek Bilgileri Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
    "Veritabanlarını Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
    "GLSL Gölgelendiricileri Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
    "Lakka'yı Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
    "Kaplamaları Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
    "Slang Gölgelendiricilerini Güncelle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER,
    "Kullanıcı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_KEYBOARD,
    "Kbd"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
    "Kullanıcı Arayüzü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
    "Dil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
    "Kullanıcı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
    "Dahili Resim Görüntüleyiciyi Kullan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
    "Dahili Medya Yürütücüyü Kullan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
    "<Bu dizini kullan>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
    "Çevirmeye İzin Ver"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
    "En Boy Oranını Yapılandır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
    "Otomatik En Boy Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
    "En Boy Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
    "Siyah Çerçeve Ekleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
    "Aşırmış Çerçeveyi Kırpma (Yeniler)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
    "Masaüstü Kompozisyonunu Devre Dışı Bırak"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
    "Video"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
    "Video Filtresi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
    "Video Filtresi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
    "Titreşimsiz filtre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
    "Ekran Bildirimleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
    "Bildirim Yazı Tipi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
    "Bildirim boyutu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
    "En boy oranını zorla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
    "sRGB FBO'yu zorlayarak devre dışı bırak"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
    "Kare Geçikmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
    "Tam Ekran Modunda Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
    "Video Gamma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
    "GPU ile Kaydetmeyi kullan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
    "GPU Ekran görüntüsü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
    "Hard GPU Sync"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
    "Hard GPU Sync"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
    "Max swapchain images"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
    "Bildirimin X Konumu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
    "Bildirimin Y Konumu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
    "Monitör İndeksi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
    "Filtre Sonrası Kaydı Kullan"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
    "Dikey Yenileme Hızı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
    "Tahmini Ekran Kare Hızı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
    "Ekranın-Belirlenen Yenileme Hızını Ayarla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
    "Görüntüyü Çevir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
    "Ekran Yönü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
    "Pencereli Ölçek"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
    "Kayıt Çekirdekleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
    "Scale Integer"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
    "Video"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
    "Video Gölgelendiricisi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
    "Gölgelendirici Geçişleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
    "Gölgelendirici Parametreleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
    "Ön Tanımlı Gölgelendirici Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
    "Gölgelendirici Hazır Ayarını Farklı Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
    "Çekirde Hazır Ayarını Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
    "İçerik Dizini Ön Ayarını Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
    "Oyun Ön Ayarını Kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
    "Paylaşılan Donanım İçeriği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
    "Bilinear Filtreleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
    "Yumuşak Filtre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
    "Dikey Senkronizasyon (Vsync) Değişme Aralığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
    "Video"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
    "Threaded Video"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
    "Deflicker"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
    "Özel En Boy Oranı Yüksekliği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
    "Özel En Boy Oranı Genişliği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
    "Özel En Boy Oranı X Poz."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
    "Özel En Boy Oranı Y Poz."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
    "VI Ekran Genişliğini Ayarla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
    "Dikey Eşitleme (Vsync)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
    "Pencerelenmiş Tam Ekran Modu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
    "Pencere genişliği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
    "Pencere yüksekliği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
    "Tam ekran genişliği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
    "Tam ekran yüksekliği"
    )
#ifdef HAVE_VIDEO_LAYOUT
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
    "Video Düzeni Dizini"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
    "Dosya tarayıcısından bir video düzeni seçin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
    "Seçilmiş Görünüm"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
    "Yüklenen düzende bir görünüm seçin."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
    "Wi-Fi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
    "Wi-Fi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
    "Menü Alfa Faktörü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
    "Menü Yazı Tipi Kırmızı Renk"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
    "Menü Yazı Tipi Yeşil Renk"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
    "Menü Yazı Tipi Mavi Renk"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_FONT,
    "Menü Fontu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
    "Özel"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
    "Düz Kullanıcı Arayüzü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
    "Monokrom"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
    "Ters Monokrom"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
    "Sistematik"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
    "NeoActive"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
    "Piksel"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
    "RetroActive"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
    "Retrosystem"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
    "Dot-Art"
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
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
    "Menü Renk Teması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
    "Elma Yeşili"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
    "Karanlık"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
    "Aydınlık"
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
    MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
    "Menü Gölgelendirici Hattı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
    "Menü Ölçeklendirme Faktörü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
    "Simge Gölgeleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
    "Geçmiş Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
    "İçerik Ekle Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
    "Oynatma Listesi Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
    "Favoriler Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
    "Resim Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
    "Müzik Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
    "Ayarlar Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
    "Video Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
    "Netplay Sekmesini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
    "Menü düzeni"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_THEME,
    "Menü Simgesi Teması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_YES,
    "Var"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
    "Gölgelendirici Ön Ayarı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
    "Klasik oyunlarda özel başarılar kazanmak için yarış.\n"
    "Daha fazla bilgi için http://retroachievements.org adresini ziyaret edin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
    "Test amaçlı gayri resmi başarıları ve/veya beta özelliklerini kullanın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Kazanılan puanları iki katına çıkarır.\n"
    "Tüm oyunlar için kaydetme konumlarını, hileleri, geri sarma, duraklatma ve yavaş hareketleri devre dışı bırakır.\n"
    "Çalışma zamanında bu ayarın değiştirilmesi, oyununuzu yeniden başlatır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
    "Oyuna özgü liderler tablosu.\n"
    "Hardcore Modu devre dışı bırakılmışsa etkisi yoktur."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
    "Başarı Listesi'ndeki rozetleri görüntüle."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
    "Bildirimlerde daha fazla bilgi göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
    "Bir başarı tetiklendiğinde otomatik olarak ekran görüntüsü al."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
    "Sistem tarafından kullanılan sürücüleri değiştir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
    "Başarı ayarlarını değiştir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_SETTINGS,
    "Çekirdek ayarlarını değiştir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
    "Kayıt ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
    "Ekran kaplamasını, klavye kaplamasını ve ekrandaki bildirim ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
    "Geri sarma, ileri sarma ve ağır çekim ayarlarını değiştirme."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
    "Kaydetme ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
    "Günlük ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
    "Kullanıcı arayüzü ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_SETTINGS,
    "Hesap, kullanıcı adı ve dil ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
    "Gizlilik ayarlarınızı değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
    "MIDI ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
    "Dosyaların bulunduğu varsayılan dizinleri değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
    "Oynatma listesi ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
    "Sunucu ve ağ ayarlarını yapılandırın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
    "İçeriği tarayın ve veritabanına ekleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
    "Ses çıkışı ayarlarını değiştirin."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
    "Bluetooth durumunu belirler."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
    "Çıkışta yapılandırma dosyasındaki değişiklikleri kaydeder."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
    "Yapılandırma dosyaları için varsayılan ayarları değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
    "Yapılandırma dosyalarını yönetin ve oluşturun."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CPU_CORES,
    "CPU'nun sahip olduğu çekirdek miktarı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FPS_SHOW,
    "Ekrandaki saniye başına geçerli kare hızını görüntüler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
    "Ekrandaki geçerli kare sayısını görüntüler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MEMORY_SHOW,
    "Geçerli bellek kullanımı/toplamı ekrandaki FPS/Kare sayısını içerir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
    "Kısayol tuşu ayarlarını yapılandırın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
    "Geçiş menüsüne Gamepad düğme kombinasyonu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
    "Joypad, klavye ve fare ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_USER_BINDS, /* TODO/FIXME - change user to port */
    "Bu kullanıcı için kontrolleri yapılandırın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
    "Video, ses ve giriş gecikmesi ile ilgili ayarları değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
    "Olayları bir terminale veya dosyaya kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY,
    "Netplay oturumun kurun veya katılın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
    "Yerel ağdaki Netplay ana bilgisayarlarını arayın ve bağlanın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
    "Sistem bilgisini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
    "RetroArch için eklentiler, bileşenler ve içerikler indirin."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
    "Ağ klasörlerini SMB protokolü ile paylaşın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
    "İşletim sistemi düzeyinde servisleri yönetin."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
    "Dosya tarayıcısının içindeki gizli dosyaları/dizinleri göster."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_SSH_ENABLE,
    "Uzaktan komut satırına erişmek için SSH kullanın."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
    "Sisteminizin ekran koruyucusunun aktif hale gelmesini önler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
    "Pencere boyutunu Çekirdek görünüm alanı boyutuna göre ayarlar. Alternatif olarak, sabit bir pencere boyutu için bir pencere genişliğini ve yüksekliğini ayarlayabilirsiniz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_USER_LANGUAGE,
    "Arayüzün dilini ayarlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
    "Çerçevelerin arasına siyah bir çerçeve ekler. Gölgelenmeyi önlemek için 60Hz içerik oynatmak isteyen 120Hz ekranlı kullanıcılar için kullanışlıdır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
    "Daha fazla video takılma riski pahasına gecikmeyi azaltır. V-Sync'ten sonra gecikme ekler (ms cinsinden)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
    "'Hard GPU Sync'i kullanırken CPU'nun GPU'dan kaç kare önce gideceğini belirler..."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
    "Video sürücüsüne açıkça belirtilen arabelleğe alma modunu kullanmasını söyler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
    "Hangi ekranın kullanılacağını seçer."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
    "Ekranın Hz cinsinden yenileme hızı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
    "Ekran sürücüsü tarafından bildirilen yenileme hızı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
    "Video çıkış ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
    "Kablosuz ağları tarar ve bağlantı kurar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HELP_LIST,
    "Programın nasıl çalıştığı hakkında daha fazla bilgi edinin."
    )
MSG_HASH(
    MSG_ADDED_TO_FAVORITES,
    "Favorilere eklendi"
    )
MSG_HASH(
    MSG_RESET_CORE_ASSOCIATION,
    "Oynatma listesi ile Çekirdek ilişkisi sıfırlandı"
    )
MSG_HASH(
    MSG_APPENDED_DISK,
    "Eklenen Disk"
    )
MSG_HASH(
    MSG_APPLICATION_DIR,
    "Uygulama Dizini"
    )
MSG_HASH(
    MSG_APPLYING_CHEAT,
    "Hile değişikliklerini uygulanıyor"
    )
MSG_HASH(
    MSG_APPLYING_SHADER,
    "Gölgelendiriciler uygulanıyor"
    )
MSG_HASH(
    MSG_AUDIO_MUTED,
    "Ses kapatıldı"
    )
MSG_HASH(
    MSG_AUDIO_UNMUTED,
    "Ses açıldı."
    )
MSG_HASH(
    MSG_AUTOCONFIG_FILE_ERROR_SAVING,
    "Otomatik yapılandırma dosyası kaydedilirken hata oluştu."
    )
MSG_HASH(
    MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
    "Otomatik yapılandırma dosyası başarıyla kaydedildi."
    )
MSG_HASH(
    MSG_AUTOSAVE_FAILED,
    "Otomatik kaydetme başlatılamadı."
    )
MSG_HASH(
    MSG_AUTO_SAVE_STATE_TO,
    "Auto save state to"
    )
MSG_HASH(
    MSG_BLOCKING_SRAM_OVERWRITE,
    "SRAM Üzerine Yazmayı Engelleniyor"
    )
MSG_HASH(
    MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
    "Bringing up command interface on port"
    )
MSG_HASH(
    MSG_BYTES,
    "bayt"
    )
MSG_HASH(
    MSG_CANNOT_INFER_NEW_CONFIG_PATH,
    "Yeni yapılandırma yolu çıkarılamıyor. Geçerli saati kullanın."
    )
MSG_HASH(
    MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
    "Başarımlar için Hardcore Modu Etkin, konum kaydedici ve geri sarma devre dışı bırakıldı."
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
    "Konfigürasyon dizini ayarlanmadı. Yeni yapılandırma kaydedilemiyor."
    )
MSG_HASH(
    MSG_CONNECTED_TO,
    "Bağlanıldı"
    )
MSG_HASH(
    MSG_CONTENT_CRC32S_DIFFER,
    "İçerik CRC32'ler farklıdır. Farklı oyunlar kullanılamaz."
    )
MSG_HASH(
    MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
    "İçerik yüklemesi atlandı. Uygulama tek başına yükleyecektir."
    )
MSG_HASH(
    MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
    "Çekirdek Konum Kayıtlarını desteklemiyor."
    )
MSG_HASH(
    MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
    "Çekirdek seçenekleri dosyası başarıyla oluşturuldu."
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
    "Geçerli veri yolu bulunamadı"
    )
MSG_HASH(
    MSG_COULD_NOT_OPEN_DATA_TRACK,
    "veri izi açılamadı"
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
    "Filmden durumu okunamadı."
    )
MSG_HASH(
    MSG_CRC32_CHECKSUM_MISMATCH,
    "CRC32 sağlama toplamı içerik dosyası ile kayıt dosyası başlığında kaydedilen içerik sağlama toplamı arasındaki uyumsuzluk var, yeniden oynatma sırasında çözülme olasılığı yüksek."
    )
MSG_HASH(
    MSG_CUSTOM_TIMING_GIVEN,
    "Özel zamanlama verildi"
    )
MSG_HASH(
    MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
    "Dekompresyon zaten devam ediyor."
    )
MSG_HASH(
    MSG_DECOMPRESSION_FAILED,
    "Dekompresyon başarısız oldu."
    )
MSG_HASH(
    MSG_DETECTED_VIEWPORT_OF,
    "Algılanan bakış açısı"
    )
MSG_HASH(
    MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
    "Geçerli bir içerik düzeltme eki bulamadınız."
    )
MSG_HASH(
    MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
    "Aygıtı geçerli bir bağlantı noktasından çıkarın."
    )
MSG_HASH(
    MSG_DISK_CLOSED,
    "Kapatıldı"
    )
MSG_HASH(
    MSG_DISK_EJECTED,
    "Çıkartıldı"
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
    "Yükleme başarısız"
    )
MSG_HASH(
    MSG_ERROR,
    "Hata"
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
    "Libretro Çekirdeği içerik gerektirir, ancak hiçbir şey sağlanmadı."
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
    "Libretro Çekirdeği özel içerik gerektirir, ancak hiçbir şey sağlanmadı"
    )
MSG_HASH(
    MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
    "Çekirdek VFS'yi desteklemiyor ve yerel bir kopyadan yükleme başarısız oldu"
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
    MSG_ERROR_SAVING_REMAP_FILE,
    "Yeniden yapılandırma dosyası kaydedilirken hata oluştu."
    )
MSG_HASH(
    MSG_ERROR_REMOVING_REMAP_FILE,
    "Yeniden yapılandırma dosyası kaldırılırken hata oluştu."
    )
MSG_HASH(
    MSG_ERROR_SAVING_SHADER_PRESET,
    "Gölgelendirici hazır ayarı kaydedilirken hata oluştu."
    )
MSG_HASH(
    MSG_EXTERNAL_APPLICATION_DIR,
    "Dış Uygulama Konumu"
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
    MSG_FAILED_TO,
    "Başaramadı"
    )
MSG_HASH(
    MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
    "Gelen seyirci kabul edilemedi."
    )
MSG_HASH(
    MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
    "Failed to allocate memory for patched content..."
    )
MSG_HASH(
    MSG_FAILED_TO_APPLY_SHADER,
    "Gölgelendirici uygulanamadı."
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
    "İçerik yüklenemedi"
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
    "Kayıtlı Konum yüklenemedi"
    )
MSG_HASH(
    MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
    "Libretro Çekirdeği açılamadı"
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
    MSG_FAILED_TO_SAVE_STATE_TO,
    "Konum kaydı kaydedilemedi"
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
    "Film kaydı başlatılamadı."
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
    "Konum durumu geri alınamadı."
    )
MSG_HASH(
    MSG_FAILED_TO_UNDO_SAVE_STATE,
    "Konum Kaydetme geri alınamadı."
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
    "İçinde otomatik konum kaydı bulundu"
    )
MSG_HASH(
    MSG_FOUND_DISK_LABEL,
    "Bulunan disk etiketi"
    )
MSG_HASH(
    MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
    "Dosyada ilk veri parçasını bulundu"
    )
MSG_HASH(
    MSG_FOUND_LAST_STATE_SLOT,
    "Son konum yuvası bulundu"
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
    "Oyun Başına Seçenekler: oyunda bulunan oyuna özgü temel seçenekler"
    )
MSG_HASH(
    MSG_GOT_INVALID_DISK_INDEX,
    "Geçersiz disk dizini var."
    )
MSG_HASH(
    MSG_GRAB_MOUSE_STATE,
    "Fare tutma durumu"
    )
MSG_HASH(
    MSG_GAME_FOCUS_ON,
    "Oyun Odaklanması açık"
    )
MSG_HASH(
    MSG_GAME_FOCUS_OFF,
    "Oyun Odaklanması kapalı"
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
    "Önceden Ayarlanmış Dosya Adı Girin"
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
    "Geçersiz Kullanıcı Adı Boyutu."
    )
MSG_HASH(
    MSG_IN_BYTES,
    "bayt cinsinden"
    )
MSG_HASH(
    MSG_IN_GIGABYTES,
    "gigabayt cinsinden"
    )
MSG_HASH(
    MSG_IN_MEGABYTES,
    "megabayt olarak"
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
    "#%d yuvasından durum yüklendi."
    )
MSG_HASH(
    MSG_LOADED_STATE_FROM_SLOT_AUTO,
    "#-1 yuvasından durum yüklendi (otomatik)."
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
    MSG_LOADING_STATE,
    "Konum yükleniyor"
    )
MSG_HASH(
    MSG_MEMORY,
    "Hafıza"
    )
MSG_HASH(
    MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
    "Giriş tekrar film dosyası geçerli bir BSV1 dosyası değil."
    )
MSG_HASH(
    MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
    "Giriş tekrar film formatı farklı bir seri hale getirici sürümüne sahip görünüyor. Büyük olasılıkla başarısız olacak."
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
    MSG_NO_CONTENT_STARTING_DUMMY_CORE,
    "İçerik yok, Kukla Çekirdek başlatıyor."
    )
MSG_HASH(
    MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
    "Henüz konum kaydetmenin üzerine yazılmadı."
    )
MSG_HASH(
    MSG_NO_STATE_HAS_BEEN_LOADED_YET,
    "Henüz bir konum yüklenmedi."
    )
MSG_HASH(
    MSG_OVERRIDES_ERROR_SAVING,
    "Üzerine yazmalar kaydedilemedi."
    )
MSG_HASH(
    MSG_OVERRIDES_SAVED_SUCCESSFULLY,
    "Üzerine yazmalar başarıyla kaydedildi."
    )
MSG_HASH(
    MSG_PAUSED,
    "Durduruldu."
    )
MSG_HASH(
    MSG_PROGRAM,
    "RetroArch"
    )
MSG_HASH(
    MSG_READING_FIRST_DATA_TRACK,
    "İlk veri parçasını okunuyor..."
    )
MSG_HASH(
    MSG_RECEIVED,
    "alındı"
    )
MSG_HASH(
    MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
    "Kayıt, yeniden boyutlandırma nedeniyle sonlandırıldı."
    )
MSG_HASH(
    MSG_RECORDING_TO,
    "Kayıt ediliyor"
    )
MSG_HASH(
    MSG_REDIRECTING_CHEATFILE_TO,
    "Hile dosyası yeniden yölnediriliyor"
    )
MSG_HASH(
    MSG_REDIRECTING_SAVEFILE_TO,
    "Kayıt dosyası yeniden yönlendiriliyor"
    )
MSG_HASH(
    MSG_REDIRECTING_SAVESTATE_TO,
    "Konum kaydı yeniden yönlendiriliyor"
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
    MSG_REMOVED_DISK_FROM_TRAY,
    "Tepsiden çıkarılmış disk."
    )
MSG_HASH(
    MSG_REMOVING_TEMPORARY_CONTENT_FILE,
    "Removing temporary content file"
    )
MSG_HASH(
    MSG_RESET,
    "Yeniden başlatılıyor"
    )
MSG_HASH(
    MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
    "Sürücü yeniden başlatması nedeniyle kayıt yeniden başlatılıyor."
    )
MSG_HASH(
    MSG_RESTORED_OLD_SAVE_STATE,
    "Eski durum kaydı geri yüklendi."
    )
MSG_HASH(
    MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
    "Gölgelendiriciler: varsayılan gölgelendirici önayarını geri yükleniyor"
    )
MSG_HASH(
    MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
    "Savefile dizinini geri alınıyor"
    )
MSG_HASH(
    MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
    "Kayıt yeri dizinini geri döndürülüyor"
    )
MSG_HASH(
    MSG_REWINDING,
    "Geri sarılıyor."
    )
MSG_HASH(
    MSG_REWIND_INIT,
    "Geri sarma arabellek boyutuyla başlatılıyor"
    )
MSG_HASH(
    MSG_REWIND_INIT_FAILED,
    "Geri alma başlatılamadı. Geri sarma devre dışı bırakılacak."
    )
MSG_HASH(
    MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
    "Uygulamada threaded ses kullanıyor. Geri sarma kullanılamaz."
    )
MSG_HASH(
    MSG_REWIND_REACHED_END,
    "Geri sarma araballeğinin sonuna erişildi."
    )
MSG_HASH(
    MSG_SAVED_NEW_CONFIG_TO,
    "Yeni yapılandırma kaydedildi"
    )
MSG_HASH(
    MSG_SAVED_STATE_TO_SLOT,
    "#%d konum yuvasına kaydedildi."
    )
MSG_HASH(
    MSG_SAVED_STATE_TO_SLOT_AUTO,
    "Durum #-1 yuvasına kaydedildi (otomatik)."
    )
MSG_HASH(
    MSG_SAVED_SUCCESSFULLY_TO,
    "Başarıyla kaydedildi"
    )
MSG_HASH(
    MSG_SAVING_RAM_TYPE,
    "RAM türü kayedediliyor"
    )
MSG_HASH(
    MSG_SAVING_STATE,
    "Konum kaydediliyor"
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
    "Several patches are explicitly defined, ignoring all..."
    )
MSG_HASH(
    MSG_SHADER,
    "Gölgelendirici"
    )
MSG_HASH(
    MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
    "Gölgelendirici hazır ayarı başarıyla kaydedildi."
    )
MSG_HASH(
    MSG_SKIPPING_SRAM_LOAD,
    "SRAM yüklemesi atlanıyor."
    )
MSG_HASH(
    MSG_SLOW_MOTION,
    "Ağır Çekim."
    )
MSG_HASH(
    MSG_FAST_FORWARD,
    "İleri sarılıyor."
    )
MSG_HASH(
    MSG_SLOW_MOTION_REWIND,
    "Ağır Çekim Geri Sarma."
    )
MSG_HASH(
    MSG_SRAM_WILL_NOT_BE_SAVED,
    "SRAM kaydedilmeyecek."
    )
MSG_HASH(
    MSG_STARTING_MOVIE_PLAYBACK,
    "Film oynatmaya başlama."
    )
MSG_HASH(
    MSG_STARTING_MOVIE_RECORD_TO,
    "Starting movie record to"
    )
MSG_HASH(
    MSG_STATE_SIZE,
    "Konum boyutu"
    )
MSG_HASH(
    MSG_STATE_SLOT,
    "Konum slotu"
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
    MSG_CHANGE_THUMBNAIL_TYPE,
    "Küçük resim türünü değiştir"
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
    "to"
    )
MSG_HASH(
    MSG_UNDID_LOAD_STATE,
    "Konum yükleme geri alındı."
    )
MSG_HASH(
    MSG_UNDOING_SAVE_STATE,
    "Konum kaydı geri alınıyor."
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
    "Tanınmayan komut"
    )
MSG_HASH(
    MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
    "Yeni Konfigürasyon için Çekirdek ismi kullanılıyor."
    )
MSG_HASH(
    MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
    "Libretro kukla Çekirdeği kullanılıyor. Kayıt atlanıyor."
    )
MSG_HASH(
    MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
    "Cihazı geçerli bir porttan bağlayın."
    )
MSG_HASH(
    MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
    "Aygıtın bağlantı noktasından çıkarılması"
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
    "Görüş alanı boyut hesaplaması başarısız oldu! Ham veriler kullanılmaya devam edilecek. Muhtemelen düzgün çalışmayacak..."
    )
MSG_HASH(
    MSG_VIRTUAL_DISK_TRAY,
    "sanal disk tepsisi."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
    "Milisaniye cinsinden istenen ses gecikmesi. Ses sürücüsü verilen gecikmeyi sağlayamıyorsa onur duyulmayabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MUTE,
    "Sesi kapat/aç."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
    "Ses ve videoyu senkronize ederken zamanlamadaki kusurların düzeltilmesine yardımcı olur. Devre dışı bırakılmışsa, uygun senkronizasyonun elde edilmesinin neredeyse imkansız olduğunu unutmayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
    "Kameranın Çekirdek tarafından erişimine izin ver veya verme."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
    "Konum servislerinin Çekirdek tarafından erişimine izin ver veya verme."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
    "RetroArch tarafından desteklenen maksimum kullanıcı sayısı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
    "Giriş yoklama işleminin RetroArch içinde yapılmasının etkisi. 'Erken' veya 'Geç' olarak ayarlamak, yapılandırmanıza bağlı olarak daha az gecikmeyle sonuçlanabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
    "Herhangi bir kullanıcının menüyü kontrol etmesine izin verir. Devre dışı bırakılırsa, menüyü yalnızca Kullanıcı 1 kontrol edebilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
    "Ses seviyesi (dB cinsinden). 0 dB normal hacimdir ve kazanç uygulanmaz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
    "WASAPI sürücüsünün ses cihazının kontrolünü tamamen ele geçirmesine izin verin. Devre dışı bırakılırsa, bunun yerine paylaşılan modu kullanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
    "Ses cihazınız tarafından destekleniyorsa, WASAPI sürücüsü için kayan nokta biçimini kullanın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
    "WASAPI sürücüsünü paylaşılan modda kullanırken ara arabellek uzunluğu (karelerde)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SYNC,
    "Sesi senkronize et. Tavsiye edilen."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
    "Bir düğmeye basılmasıyla sonuçlanacak eksenin ne kadar uzağa eğilmesi gerektiği."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
    "Bir sonraki basılı tutma işlemine kadar bekleyecek saniye miktarı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
    "Bir girişi bağlamak için tutulan saniye miktarı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
    "Turbo etkin düğmelerin değiştirildiği süreyi açıklar. Sayılar çerçevelerde açıklanmıştır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
    "Turbo etkin bir düğmenin ne kadar sürmesi gerektiğini açıklar. Sayılar çerçevelerde açıklanmıştır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
    "Grafik kartının çıkış videosunu ekranın yenileme hızıyla senkronize eder. Tavsiye edilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
    "Çekirdeklerin ekranı ayarlamasına izin ver. Devre dışı bırakıldığında, döndürme istekleri yoksayılır. Ekranı elle döndüren ayarlarda kullanışlıdır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
    "Bazı Çekirdeklerin kapatma özelliği olabilir. Etkinleştirildiğinde, Çekirdeğin RetroArch'ı kapatmasını engeller. Bunun yerine, kukla bir Çekirdek yükler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
    "İçerik yüklemeyi denemeden önce gerekli tüm üretici yazılımının olup olmadığını kontrol edin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
    "Ekranınızın dikey yenileme hızı. Uygun bir ses giriş hızı hesaplamak için kullanılır.\n"
    "NOT: 'Threaded Video' etkinleştirilmişse bu dikkate alınmaz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
    "Sesin verilip verilmediğini belirler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
    "Ses giriş hızındaki maksimum değişiklik. Bunun artırılması, yanlış bir ses perdesi pahasına zamanlamada çok büyük değişiklikler yapılmasını sağlar (örneğin, NTSC ekranlarında PAL Çekirdeği çalıştırma)."
    )
MSG_HASH(
    MSG_FAILED,
    "başarısız oldu"
    )
MSG_HASH(
    MSG_SUCCEEDED,
    "başarılı"
    )
MSG_HASH(
    MSG_DEVICE_NOT_CONFIGURED,
    "ayarlanmamış"
    )
MSG_HASH(
    MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
    "yapılandırılmamış, geri dönülüyor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
    "Veritabanı İmleç Listesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
    "Veritabanı - Filtre : Geliştirici"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
    "Veritabanı - Filtre : Dağıtıcı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISABLED,
    "Devre dışı bırakıldı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ENABLED,
    "Etkinleştirildi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
    "İçerik Geçmişi Yolu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
    "Veritabanı - Filtre : Origin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
    "Veritabanı - Filtre : Seri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
    "Veritabanı - Filtre : ESRB Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
    "Veritabanı - Filtre : ELSPA Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
    "Veritabanı - Filtre : PEGI Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
    "Veritabanı - Filtre : CERO Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
    "Veritabanı - Filtre : BBFC Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
    "Veritabanı - Filtre : Max Users"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
    "Veritabanı - Filtre : Releasedate By Month"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
    "Veritabanı - Filtre : Releasedate By Year"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
    "Veritabanı - Filtre : Edge Magazine Issue"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
    "Veritabanı - Filtre : Edge Magazine Değerlendirmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
    "Veritabanı Bilgisi"
    )
MSG_HASH(
    MSG_WIFI_SCAN_COMPLETE,
    "Wi-Fi taraması tamamlandı."
    )
MSG_HASH(
    MSG_SCANNING_WIRELESS_NETWORKS,
    "Kablosuz ağları tarama..."
    )
MSG_HASH(
    MSG_NETPLAY_LAN_SCAN_COMPLETE,
    "Netplay taraması tamamlandı."
    )
MSG_HASH(
    MSG_NETPLAY_LAN_SCANNING,
    "Netplay sunucuları aranıyor ..."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
    "RetroArch etkin pencere olmadığında oyunu duraklat."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
    "Pencere yöneticileri, görsel efektleri uygulamak, yanıt vermeyen pencereleri tespit etmek için diğer şeylerin yanı sıra bileşimi kullanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
    "Son kullanılan oyunların, resimlerin, müziklerin ve videoların oynatma listesini tutar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
    "Oyunlar, resimler, müzik ve videolar için son oynatma listesindeki giriş sayısını sınırlayın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
    "Birleşik Menü Kontrolleri"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
    "Hem menü hem de oyun için aynı kontrolleri kullanın. Klavyeye uygulanır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
    "Çıkış için iki kez basın"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
    "RetroArch'tan çıkmak için çıkış kısayol tuşuna iki kez basın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
    "Ekrandaki mesajları göster."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
    "%d Kullanıcı Bağlantısı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
    "Pil seviyesini göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
    "Menü alt etiketlerini göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
    "Seçili olan menü girişi için ek bilgi gösterir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SELECT_FILE,
    "Dosya Seç"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILTER,
    "Filtre"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SCALE,
    "Ölçek"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
    "İçerik yüklendiğinde Netplay başlayacaktır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
    "Uygun bir Çekirdek veya içerik dosyası bulunamadı, elle yükleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
    "URL'ye Gözat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_URL,
    "URL yolu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BROWSE_START,
    "Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_BOKEH,
    "Bokeh"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
    "Kar Tanesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
    "Oda Listesini Yenile"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
    "Kulanıcı Adı: %s"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
    "Kulanıcı Adı (lan): %s"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
    "Uyumlu içerik bulundu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
    "Görüntünün kenarları etrafındaki birkaç pikseli, bazen de çöp pikselleri de içeren geliştiriciler tarafından geleneksel olarak boş bırakılır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
    "Sert piksel kenarlarından kenarı çıkarmak için görüntüye hafif bir bulanıklık ekler. Bu seçeneğin performans üzerinde çok az etkisi var."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FILTER,
    "CPU ile çalışan bir video filtresi uygulayın.\n"
    "NOT: Yüksek performanslı bir maliyeti olabilir. Bazı video filtreleri yalnızca 32bit veya 16bit renk kullanan Çekirdekler için çalışabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
    "RetroAchievements hesabınızın kullanıcı adını girin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
    "RetroAchievements hesabınızın şifresini girin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
    "Kullanıcı adınızı buraya girin. Diğer şeylerin yanı sıra Netplay oturumları için de kullanılacak."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
    "Filtreler (ancak gölgelendiriciler değil) uygulandıktan sonra görüntüyü çekin. Videonuz, ekranda gördüğünüz kadar süslü görünecek."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_LIST,
    "Kullanılacak Çekirdeği seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_START_CORE,
    "İçerik olmadan Çekirdeği başlat"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
    "Çevrimiçi güncelleyiciden bir Çekirdek yükleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
    "İndirilen dizinden bir Çekirdek kurun ya da geri yükleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
    "Hangi içeriğin başlatılacağını seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
    "Ağ arayüzlerini ve ilgili IP adreslerini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
    "Aygıta özgü bilgileri gösterir."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Programı yeniden başlat."
    )
#else
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
    "Programdan çık."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
    "Programı yeniden başlatın."
    )
#endif
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
    "Ekran penceresi için özel genişliği ayarlayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
    "Ekran penceresi için özel yüksekliği ayarlayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
    "Pencere boyutunu ve konumunu hatırlayın, Pencereli Ölçekten öncelikli olmasını sağlar"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
    "Penceresiz tam ekran modu için özel genişlik boyutunu ayarlayın. Bunu ayarsız bırakmak masaüstü çözünürlüğünü kullanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
    "Penceresiz tam ekran modu için özel yükseklik boyutunu ayarlayın. Bunu ayarsız bırakmak masaüstü çözünürlüğünü kullanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
    "Ekrandaki metin için özel X ekseni konumunu belirtin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
    "Ekrandaki metin için özel Y ekseni konumunu belirtin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
    "Yazı tipi boyutunu nokta cinsinden belirtin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
    "Menü içindeyken kaplamayı gizleyin ve menüden çıkarken tekrar gösterin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
    "Ekran kaplaması üzerinde klavye/denetleyici girişlerini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
    "Yerleşim Girdilerini Göster seçeneği etkinse dinlenecek portu seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
    "Ekran kaplaması kullanırken fare imlecini gösterin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
    "Veritabanına uyan taranmış içerikler burada görünecektir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
    "Videoyu yalnızca tamsayı adımlarla ölçekler. Temel boyut, sistem tarafından bildirilen geometri ve en boy oranına bağlıdır. 'En boy oranını zorla' ayarlanmamışsa, X/Y tamsayı bağımsız olarak ölçeklendirilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
    "Varsa, GPU gölgelendirici ekran görüntüsünü kullanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
    "Videonun belirli bir dönüşünü zorlar. Dönme, çekirdeğin ayarladığı dönüşlere eklenir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
    "İşletim sisteminden ekranın belirli bir yönünü zorlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
    "SRGB FBO desteğini zorla devre dışı bırakın. Windows'taki bazı Intel OpenGL sürücüleri, eğer etkinse, sRGB FBO desteğiyle ilgili video sorunları yaşıyor. Bunu etkinleştirmek, onun etrafında çalışabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
    "Tam ekrandan başlar. Çalışma zamanında değiştirilebilir. Komut satırı tarafından geçersiz kılınabilir"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
    "Tam ekran ise, pencereli bir tam ekran modu kullanmayı tercih edin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
    "Varsa, GPU gölgelendiricileri çıkışını kaydeder."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
    "Bir kayıt yapma işlemi yaparken, kaydetme durumu indeksi, kaydedilmeden önce otomatik olarak artırılır. İçerik yüklenirken, dizin mevcut en yüksek dizine ayarlanacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
    "Kaydetme durumları yüklenirken RAM'ın üzerine yazılmasını engelleyin. Potansiyel olarak buggy oyunlara yol açabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
    "Hızlı ileri sarma kullanılırken içeriğin çalıştırılacağı maksimum hız (ör. 60 fps içerik için 5.0x = 300 fps kapak). 0.0x olarak ayarlanmışsa, hızlı sarma oranı sınırsızdır (FPS sınırı yok)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
    "Ağır çekimde, içerik belirtilen/ayarlanan faktöre göre yavaşlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
    "Çekirdek mantığını bir veya daha fazla karenin önünde çalıştırın, ardından algılanan giriş gecikmesini azaltmak için durumu geri yükleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
    "Önde çalışacak kare sayısı. Oyunun içindeki gecikmeli çerçeve sayısını aşarsanız, jitter(seğirme) gibi oyun sorunlarına neden olur."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
    "Komple bir giriş örneği almak için beklenecek milisaniye sayısı, eşzamanlı tuşlara basıldığında sorun yaşıyorsanız kullanın (yalnızca Android için)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
    "Devam etmek için RetroArch Çekirdeğin ikinci bir örneğini kullanın. Yükleme durumu nedeniyle ses sorunlarını önler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
    "RunAhead'i kullanırken ortaya çıkan uyarı mesajını gizler ve Çekirdek konum kaydediciyi desteklemez."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_ENABLE,
    "Hata mı yaptın? Geri sar ve tekrar dene.\n"
    "Oyun oynarken performansı etkileyeceğine dikkat edin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
    "Geçiş yaptıktan hemen sonra hileyi uygulayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
    "Oyun yüklendiğinde hileleri otomatik uygulayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
    "Hilenin kaç kez uygulanacağını belirler.\n"
    "Geniş bellek alanlarını etkilemek için diğer iki Yineleme seçeneğiyle birlikte kullanın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
    "Her “Yineleme Sayısı” ndan sonra Hafıza Adresi, bu sayı ile “Hafıza Arama Boyutu” ile artacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
    "Her “İterasyon Sayısı” ndan sonra, Değer bu miktar kadar artacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
    "Tanımlanmış sayıda kare geri alırken, bir defada birkaç kare geri sararak geri sarma hızını artırabilirsiniz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
    "Geri alma arabelleği için ayrılacak bellek miktarı (MB cinsinden). Bunun arttırılması, geri sarma geçmişinin miktarını artıracaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
    "Bu kullanıcı arabirimiyle geri alma arabellek boyutu değerini her artırdığınızda ya da azaltdığınızda, bu miktar kadar değişecektir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_IDX,
    "Listedeki dizin konumu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
    "ABellek Arama Boyutu <8-bit olduğunda adres bit maskesi."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
    "Görüntülenecek eşleşmeyi seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_OR_CONT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
    "Bit boyutunu değiştirmek için Sol/Sağ yapın"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
    "Değeri değiştirmek için Sol/Sağ yapın"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GT,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_LTE,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_GTE,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQ,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_NEQ,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
    "Değeri değiştirmek için Sol/Sağ"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
    "Değeri değiştirmek için Sol/Sağ"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_MATCHES,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_VIEW_MATCHES,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_CREATE_OPTION,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DELETE_OPTION,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_TOP,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_ADD_NEW_BOTTOM,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DELETE_ALL,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_RELOAD_CHEATS,
    ""
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN,
    "Düşük son hane  : 258 = 0x0102,\n"
    "Yüksek son hane : 258 = 0x0201"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
    "Çekirdekler için günlük seviyesini ayarlar. Çekirdek tarafından verilen günlük seviyesi bu değerin altındaysa, dikkate alınmaz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
    "RetroArch (ve Çekirdekler) için performans sayaçları.\n"
    "Sayaç verileri, sistemdeki darboğazların belirlenmesine ve sistemin ve uygulama performansının ince ayarının yapılmasına yardımcı olabilir"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
    "RetroArch'ın çalışma zamanının sonunda otomatik olarak bir kayıt ekranı oluşturur. 'Otomatik Yükleme Durumu' etkinse, RetroArch otomatik olarak bu ekranı yükleyecektir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
    "Otomatik kaydetme durumunu başlangıçta otomatik olarak yükle."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
    "Menü içindeki kaydetme durumlarının küçük resimlerini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
    "Geçici olmayan Save RAM'i düzenli aralıklarla otomatik olarak kaydeder. Aksi belirtilmedikçe, bu varsayılan olarak devre dışıdır. Aralık saniye cinsinden ölçülür. 0 değeri otomatik kaydetmeyi devre dışı bırakır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
    "Etkinleştirilirse, giriş, geçerli çekirdek için ayarlanan yeniden birleştirilen ciltlerle geçersiz kılınır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
    "Etkinleştirildiğinde denetleyicileri(gamepadler) otomatik yapılandırmaya geçer, Tak ve Kullan stili."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
    "TAMAM/İptal için düğmeleri değiştirin. Devre dışı bırakılınca, Japonca düğme yönüne geçer, etkinleştirilirse Batı düğme yönüne geçer."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
    "Devre dışı bırakılırsa, RetroArch menüsü değiştirildiğinde, içerik arka planda çalışmaya devam eder."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
    "Kullanılacak Video sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
    "Kullanılacak Ses sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_DRIVER,
    "Kullanılacak giriş sürücüsü. Video sürücüsüne bağlı olarak, farklı bir giriş sürücüsünü zorlayabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
    "kullanılacak Joypad sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
    "Kullanılacak Yeniden Ses örneklemesi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
    "Kullanılacak kamera sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
    "Kullanılacak konum sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_DRIVER,
    "Kullanılacak menü sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORD_DRIVER,
    "Kullanılacak kayıt sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_DRIVER,
    "Kullanılacak MIDI sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_WIFI_DRIVER,
    "Kullanılacak WiFi sürücüsü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
    "Dosya Gezgininde desteklenen uzantılarla gösterilen dosyaları filtreleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
    "Menü duvar kağıdı olarak ayarlamak için bir resim seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
    "Dynamically load a new wallpaper depending on context."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
    "Ses sürücüsünün kullandığı varsayılan ses cihazının üzerine yazın. Bu işlem sürücüye bağlıdır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
    "Sürücüye gönderilmeden önce sesi işleyen DSP Ses eklentisi."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
    "Ses çıkışı örnekleme hızı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
    "Kaplamanın Kullanıcı Arayüzündeki elementlerin opaklığı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_SCALE,
    "Kaplamanın Kullanıcı Arayüzündeki elementlerin boyutu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
    "Overlays are used for borders and on-screen controls"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
    "Dosya tarayıcısından bir kaplama seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
    "Bağlanılacak ana bilgisayarın adresi."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
    "Ana bilgisayar IP adresinin bağlantı noktası. Bir TCP veya UDP bağlantı noktası olabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
    "Netplay ana bilgisayarına bağlanmak için şifre. Yalnızca ana bilgisayar modunda kullanılır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
    "Netplay oyunlarının kamuya duyurulup duyulmayacağı. Ayarlanmamışsa, istemciler genel lobiyi kullanmak yerine el ile bağlanmalıdır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
    "Yalnızca izleyici ayrıcalıklarıyla netplay ana bilgisayarına bağlanmak için parola. Yalnızca ana bilgisayar modunda kullanılır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
    "Seyirci modunda netplay başlatılıp başlatılmayacağı."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
    "Bağımlı modda bağlantılara izin verilip verilmeyeceği. Köle modu istemciler her iki tarafta çok az işlem gücü gerektirir, ancak ağ gecikmesinden önemli ölçüde zarar görecektir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
    "Bağımlı modda olmayan bağlantılara izin verilmemesi. Çok zayıf makineleri olan çok hızlı ağlar dışında önerilmez."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
    "Netplay'i kaydetme durumları gerektirmeyen bir modda çalıştırıp çalıştırmamak. True olarak ayarlanırsa, çok hızlı bir ağ gereklidir, ancak geri sarma işlemi yapılmaz, bu yüzden net bir titreme olmaz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
    "Netplay öğesinin ana bilgisayar ve istemcinin senkronize olduğunu doğrulayacağı karelerdeki sıklık."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
    "Barındırırken, LAN'lardan kaçan UPnP veya benzeri teknolojiler kullanarak, genel İnternetten bağlantıları dinlemeye çalışır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
    "stdin komut arayuzu."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
    "Menünün fare ile kontrol edilmesini sağlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_POINTER_ENABLE,
    "Menünün ekran dokunuşlarıyla kontrol edilmesini sağlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS,
    "Görüntülenecek küçük resim türü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
    "Oynatma listelerinin sağ üst köşesinde görüntülenecek küçük resim türü. Bu küçük resim, RetroPad Y düğmesine basılarak tam ekrana getirilebilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
    "Solda görüntülenecek küçük resim türü."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
    "Oynatma listelerinin sağ alt kısmında gösterilecek küçük resim türü."
    )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "İçerik meta veri panelini başka bir küçük resim ile değiştirin."
   )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
    "Soldaki küçük resmi, ekranın sağ tarafında, sağ alt köşesinde görüntüleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
    "Menü içindeki geçerli tarihi ve/veya saati gösterir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
    "Menü içindeki geçerli pil seviyesini gösterir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
    "Liste sınırına yatay veya dikey olarak ulaşılırsa, baştan sona ve / veya sona doğru kaydırın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
    "Ana bilgisayar (host) modunda netplay'i etkinleştirir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
    "Netplay sunucu adresini girin ve istemci modunda bağlanın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
    "Aktif bir Netplay bağlantısını keser."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
    "Uyumlu içerik için bir dizin tarar. Bulunduğunda, içerik oynatma listesine eklenir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCAN_FILE,
    "Uyumlu içerik için bir dosyayı tarar. Bulunduğunda, içerik oynatma listesine eklenir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
    "Vsync için özel bir değiştirme aralığı kullanır. Bunu, monitör yenileme oranını etkin bir şekilde yarıya indirmek için ayarlayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
    "Kullanılan Çekirdeğin adını taşıyan klasörlerde kaydetme dosyalarını sıralayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
    "Kullanılan Çekirdeğin adını taşıyan klasörlerde kaydetme durumlarını sıralayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
    "Verilen giriş cihazıyla oynama isteği."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
    "Libretro buildbotundaki Çekirdek güncelleyici dizininin URL'si."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
    "Libretro buildbot'taki varlıklar güncelleyici dizinindeki URL."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
    "İndirdikten sonra, indirilen arşivlerde bulunan dosyaları otomatik olarak çıkartın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
    "Yeni odalar için tarayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DELETE_ENTRY,
    "Bu girişi Oyatma listesinden kaldır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INFORMATION,
    "İçerikle ilgili daha fazla bilgi görüntüleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
    "Girişi favorilerinize ekleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
    "Girdiyi favorilerinize ekleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUN,
    "İçeriği çalıştırın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
    "Dosya Gezgini ayarlarını düzenler."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
    "Özelleştirilmiş kontrolleri başlangıçta yükleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
    "Özelleştirilmiş yapılandırmayı başlangıçta yükleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
    "Özelleştirilmiş Çekirdek seçeneklerini başlangıçta varsayılan olarak yükleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_ENABLE,
    "Menüdeki geçerli Çekirdek adını gösterir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
    "Veritabanlarını görüntüle."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
    "Önceki aramaları görüntüle."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
    "Ekranın görüntüsünü yakalar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
    "Geçerli içeriği kapatır. Kaydedilmemiş değişiklikler kaybolabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOAD_STATE,
    "Seçili yuvadan kaydedilmiş konumu yükleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_STATE,
    "Seçili yuvaya konumu kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESUME,
    "Çalışmakta olan içeriği devam ettirin ve Hızlı Menüden çıkın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESUME_CONTENT,
    "Çalışmakta olan içeriği devam ettirin ve Hızlı Menüden çıkın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STATE_SLOT,
    "Seçili durum yuvasını değiştirir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
    "Bir durum yüklendiyse, içerik yüklenmeden önceki duruma geri döner."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
    "Bir durumun üzerine yazılmışsa, önceki kaydetme durumuna geri döner."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
    "RetroAchievements hizmeti. Daha fazla bilgi için http://retroachievements.org adresini ziyaret edin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
    "Yapılandırılmış hesapları yönetir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
    "Geri sarma ayarlarını yönetir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
    "Hile ayrıntıları ayarlarını yönetir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
    "Hile kodu aramayı başlatın veya devam edin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESTART_CONTENT,
    "İçeriği baştan yeniden başlatır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
    "Çekirdekte yüklü olan tüm içerik için geçerli olacak bir üzerine yazma dosyasını kaydeder. Ana yapılandırmadan öncelikli olacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
    "Geçerli dosyayla aynı dizinden yüklenen tüm içerik için geçerli olacak üzerine yapılandırma dosyasını kaydeder. Ana yapılandırmadan öncelikli olacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
    "Yalnızca geçerli içerik için üzerine yazma yapılandırma dosyasını kaydeder. Ana yapılandırmadan öncelikli olacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
    "Hile kodlarını ayarlayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
    "Görüntüyü görsel geliştirmek için gölgelendiricileri ayarlayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
    "Çalışmakta olan içerik için kontrolleri değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_OPTIONS,
    "Çalışmakta olan içeriğin seçeneklerini değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
    "Uzman kullanıcılar için gelişmiş ayarları göster (varsayılan olarak gizli)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
    "Ayrı bir iş parçacığında görevleri gerçekleştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
    "Kullanıcının girişleri listelerden kaldırmasına izin ver."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
    "Sistem dizinini ayarlar. Çekirdekler bu dizini BIOS'ları, sisteme özel yapılandırmaları vb. Yüklemek için sorgulayabilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
    "Dosya Gezgini için başlangıç dizinini ayarlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_DIR,
    "Genellikle, libretro/RetroArch uygulamalarını varlıklara işaret etmek üzere bir araya getiren geliştiriciler tarafından ayarlanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
    "Bağlama göre menü tarafından dinamik olarak yüklenmiş duvar kağıtlarını saklamak için dizin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
    "İlave küçük resimler (kutu resimleri / çeşitli görüntüler vb.) Burada saklanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
    "Menü yapılandırma tarayıcısı için başlangıç dizinini ayarlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
    "Ağ gecikmesini gizlemek için netplay için kullanılacak giriş gecikmesi karelerinin sayısı. Jitteri azaltır ve gözle görülür giriş gecikmesi pahasına netçeyi daha az CPU-yoğunlaştırır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
    "Ağ gecikmesini gizlemek için kullanılabilecek giriş gecikmesi çerçevelerinin aralığı. Jitteri azaltır ve tahmin edilemeyen girdi gecikmesi pahasına netçeyi daha az CPU yoğun yapar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISK_OPTIONS,
    "Disk image management."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
    "Menünün içindeyken kare hızının kapatıldığından emin olun."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
    "Çekirdekten sapmama zamanlaması istenir. Değişken Yenileme Hızına sahip ekranlar, G-Sync, FreeSync için kullanın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_LAYOUT,
    "XMB arayüzü için farklı bir düzen seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_THEME,
    "RetroArch için farklı bir simge teması seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
    "Tüm simgeler için alt gölgeler çizin.\n"
    "Bunun küçük bir performans etkisi olacak."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
    "Farklı bir arka plan rengi gradyan teması seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
    "Arka plan duvar kağıdının opaklığını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
    "Farklı bir arka plan rengi gradyan teması seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
    "Animasyonlu bir arka plan efekti seçin. Efektine bağlı olarak GPU-yoğunluğu olabilir. Performans yetersizse, bunu kapatın veya daha basit bir efekte geri dönün."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_FONT,
    "Menü tarafından kullanılacak farklı bir ana yazı tipi seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
    "Ana menünün içindeki favoriler sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
    "Ana menünün içindeki resimler sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
    "Ana menünün içindeki müzik sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
    "Ana menünün içindeki video sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
    "Ana menünün içindeki netplay sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
    "Ana menünün içindeki ayarlar sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
    "Ana menünün içindeki geçmiş sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
    "Ana menünün içindeki içerik yükle sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
    "Ana menünün içindeki oynatma listesi sekmesini göster."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
    "Menüde başlangıç ekranını göster. Bu program ilk kez başladıktan sonra otomatik olarak false değerine ayarlanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
    "Başlık grafiğinin opaklığını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
    "Altbilgi grafiğinin opaklığını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
    "İndirilen tüm dosyaları bu dizine kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
    "Tüm yeniden yapılandırılan kontrolleri bu dizine kaydeder."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
    "Programın içerik/çekirdek aradığı dizin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
    "Uygulama/çekirdek bilgi dosyaları burada saklanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
    "Bir joypad takılıysa, bu dizinde dosyaya karşılık gelen bir yapılandırma dosyası varsa, o joypad otomatik olarak yapılandırılır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
    "Tüm oynatma listelerini bu dizine kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
    "Bir dizine ayarlanırsa, geçici olarak çıkartılan içerik (örneğin arşivlerden) bu dizine çıkarılır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
    "Kayıtlı sorgular bu dizine kaydedilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
    "Veritabanları bu dizine kaydedilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
    "Menü arayüzleri yüklenebilir varlıklar vb. Aramaya çalıştığında bu konum varsayılan olarak sorgulanır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
    "Tüm kaydetme dosyalarını bu dizine kaydedin. Ayarlanmamışsa, içerik dosyasının çalışma dizinine kaydetmeye çalışır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
    "Tüm konum kaydetmeleri bu dizine kaydedin. Ayarlanmamışsa, içerik dosyasının çalışma dizinine kaydetmeye çalışır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
    "Ekran görüntüleri için kullanılacak dizin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
    "Kolay erişim için kaplamaların tutulduğu bir dizini tanımlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
    "Hile dosyaları burada tutulur."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
    "Ses DSP filtre dosyalarının tutulduğu dizin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
    "CPU tabanlı video filtre dosyalarının tutulduğu dizin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
    "GPU tabanlı video gölgelendirici dosyalarının kolay erişim için tutulduğu bir dizin tanımlar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
    "Video Kayıtlar bu dizine aktarılır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
    "Kayıt yapılandırmaları burada tutulur."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
    "Ekrandaki bildirimler için farklı bir yazı tipi seçin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
    "Gölgelendirici yapılandırmasındaki değişiklikler hemen geçerli olur. Gölgelendirici geçişi, filtreleme, FBO ölçeği vb. Miktarını değiştirdiyseniz bunu kullanın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
    "Gölgelendirici hattı geçiş miktarını artırın veya azaltın. Her hat geçişine ayrı bir gölgelendirici bağlayabilir ve ölçeğini ve filtrelemesini yapılandırabilirsiniz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
    "Gölgelendirici önayarı yükleyin. Gölgelendirici düzeni otomatik olarak kurulacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
    "Geçerli gölgelendirici ayarlarını yeni bir gölgelendirici hazır ayarı olarak kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
    "Geçerli gölgelendirici ayarlarını, uygulama/çekirdek için varsayılan ayarlar olarak kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
    "Geçerli gölgelendirici ayarlarını, geçerli içerik dizinindeki tüm dosyalar için varsayılan ayarlar olarak kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
    "Geçerli gölgelendirici ayarlarını içerik için varsayılan ayarlar olarak kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
    "Geçerli gölgelendiriciyi doğrudan değiştirir. Değişiklikler önceden ayarlanmış dosyaya kaydedilmeyecek."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
    "Şu anda menüde kullanılan gölgelendirici hazır ayarının kendisini değiştirir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
    "Hileleri artırın veya azaltın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
    "Hile değişiklikleri derhal işleme girecektir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
    "Yeni bir hile aramaya başlayın. Bit sayısı değiştirilebilir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
    "Yeni bir hile için aramaya devam edin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
    "Hile dosyası yükleyin ve mevcut hileleri değiştirin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
    "Hile dosyası yükleyin ve mevcut hilelere ekleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
    "Geçerli hileleri bir kaydetme dosyası olarak kaydedin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
    "İlgili tüm oyun içi ayarlara hızlıca erişin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_INFORMATION,
    "Uygulamaya/Çekirdek ait bilgileri görüntüleyin."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
    "En boy oranı 'Yapılandırma' olarak ayarlanmışsa, video boy oranı için kayan nokta değeri (genişlik / yükseklik)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
    "En Boy Oranı 'Özel' olarak ayarlandıysa kullanılan özel görünüm yüksekliği."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
    "En Boy Oranı 'Özel' olarak ayarlanmışsa kullanılan özel görünüm genişliği."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
    "Özel görünüm alanı ofseti, görünümün X ekseni konumunu tanımlamak için kullanılır. 'Tamsayı Ölçeği' etkinse bunlar dikkate alınmaz. Daha sonra otomatik olarak ortalanacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
    "Özel görünüm alanı ofseti, görünümün Y ekseni konumunu tanımlamak için kullanılır. 'Tamsayı Ölçeği' etkinse bunlar dikkate alınmaz. Daha sonra otomatik olarak ortalanacaktır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
    "Aktarma Sunucusunu Kullan"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
    "Ortadaki bir sunucu üzerinden netplay bağlantılarını iletin. Ana bilgisayar bir güvenlik duvarının arkasındaysa veya NAT/UPnP sorunları varsa kullanışlıdır."
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
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
    "Miksere ekle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
    "Miksere ekle ve oynat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
    "Miksere ekle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
    "Miksere ekle ve oynat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
    "Geçerli Çekirdeğe göre filtrele"
    )
MSG_HASH(
    MSG_AUDIO_MIXER_VOLUME,
    "Global ses karıştırıcı sesi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
    "Global ses karıştırıcı sesi (dB cinsinden). 0 dB normal hacimdir ve arttırma uygulanmaz."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
    "Mixer Volume Gain (dB)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
    "Mixer'i Sustur"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
    "Mikser sesini kapatın/açın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
    "Çevrimiçi Güncelleyi göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
    "'Çevrimiçi Güncelleyici' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
    "Eski Küçük Resimleri Güncelleyiciyi Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
    "Eski minik resim paketlerini indirme özelliğini gösterin/gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
    "Görünümler"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
    "Menü ekranındaki öğeleri göster veya gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
    "Çekirdek Güncelleyici'yi göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
    "Çekirdek (ve Çekirdek bilgi dosyalarını) güncelleme özelliğini gösterme/gizleme."
    )
MSG_HASH(
    MSG_PREPARING_FOR_CONTENT_SCAN,
    "İçerik taraması için hazırlanıyor..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CORE_DELETE,
    "Çekirdeği sil"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CORE_DELETE,
    "Bu Çekirdeği diskten çıkarın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
    "Kare Arabelleği Opaklığı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
    "Kare Arabelleğinin opaklığını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
    "Favoriler"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
    "'Sık Kullanılanlar'a eklediğiniz içerik burada görünecektir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
    "Müzik"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_MUSIC,
    "Daha önce çalınan müzik burada görünecektir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
    "Resim"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_IMAGES,
    "Daha önce görüntülenen resimler burada görünecek."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
    "Video"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_GOTO_VIDEO,
    "Daha önce oynatılmış olan videolar burada görünecektir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
    "Menü İkonları"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
    "Menü girişlerinin solundaki simgeleri göster."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
    "Ayarlar Sekmesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
    "Ayarları Etkinleştirmek için Şifreyi Ayarla Sekmesi"
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
    "Parolayı gir"
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
    "Parola geçerli."
    )
MSG_HASH(
    MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
    "Parola geçersiz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
    "Ayarlar sekmesini etkinleştirir. Sekmenin görünmesi için yeniden başlatma gerekir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
    "Ayarlar sekmesini gizlerken bir şifre sağlar, Ana Menü sekmesine gidip, Ayarlar Sekmesini Etkinleştir'i seçip şifreyi girerek, menüden daha sonra geri yüklemeyi mümkün kılar."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
    "Kullanıcının çalma listelerindeki girişleri yeniden adlandırmasına izin verin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
    "Girişleri yeniden adlandırmaya izin ver"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RENAME_ENTRY,
    "Girdinin başlığını yeniden adlandırın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
    "Yeniden Adlandır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
    "Çekirdek Yüklemeyi Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
    "'Çekirdek Yükle' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
    "İçerik Yükle'yi göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
    "'İçerik Yükle' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
    "Bilgi göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
    "'Bilgi' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
    "Yapılandırma Dosyasını göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
    "'Yapılandırma Dosyası' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
    "Yardım'ı göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
    "'Yardım' seçeneğini göster/gizle."
    )
#ifdef HAVE_LAKKA
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "RetroArch'ı Yeniden Başlat'ı göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "'RetroArch'ı Yeniden Başlat' seçeneğini gösterin/gizleyin."
    )
#else
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
    "RetroArch'tan Çıkışı Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
    "'RetroArch'tan Çık' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
    "RetroArch'ı Yeniden Başlat'ı göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
    "'RetroArch'ı Yeniden Başlat' seçeneğini gösterin/gizleyin."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
    "Yeniden Başlat'ı göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
    "'Yeniden başlat' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
    "Kapat'ı göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
    "'Kapat' seçeneğini göster / gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
    "Hızlı Menü"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
    "Hızlı Menü ekranındaki öğeleri göster veya gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
    "Ekran Görüntüsünü Almayı göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
    "'Ekran görüntüsü al' seçeneğini gösterin/gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
    "Kaydet/Yükle Konumunu Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
    "Show/hide the options for saving/loading state."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
    "Kaydet/Yükle Konumunu Geri Almayı Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
    "Show/hide the options for undoing save/load state."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
    "Favorilere Ekle'yi Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
    "'Favorilere Ekle' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
    "Kaydı Başlatmayı göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
    "'Kaydı Başlat' seçeneğini gösterin/gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
    "Yayını Başlat'ı göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
    "'Yayını Başlat' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
    "Çekirdek Bağlamayı Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
    "'Çekirdek Bağlamayı Ayarla' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
    "Çekirdek İlişkilendirmelerini sıfırlayını Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
    "'Çekirdek İlişkilendirmelerini sıfırla' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
    "Ayarları göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
    "'Ayarlar' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
    "Kontrolleri göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
    "'Kontroller' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
    "Hileleri göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
    "'Hileler' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
    "Gölgelendiricilerini göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
    "'Gölgelendiriciler' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
    "Çekirdek Üzerine Yazmalarını göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
    "'Çekirdek Üzerine Yazmaları' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
    "Oyun Kaydetme Üzerine Yazmalarını Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
    "'Oyun Kaydetme Üzerine Yazmaları' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
    "Bilgiyi göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
    "'Bilgi' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
    "Bildirim Arkaplanı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
    "Bildirim Arkaplan Kırmızı Renk Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
    "Bildirim Arkaplan Yeşil Renk Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
    "Bildirim Arkaplan Mavi Renk Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
    "Bildirim Arkaplan Opaklığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
    "Kiosk Modu Devre dışı bırak"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
    "Kiosk modunu devre dışı bırakın. Değişikliğin tam etkili olması için yeniden başlatma gerekir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
    "Kiosk Modu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
    "Konfigürasyonla ilgili tüm ayarları gizleyerek kurulumu korur."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
    "Kiosk Modunu Devre Dışı Bırakmak için Şifre Ayarla"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
    "Kiosk modunu etkinleştirirken bir parola girilir, Ana Menü'ye gidip Kiosk Modunu Devre Dışı Bırak'ı seçip parolayı girerek daha sonra menüden devre dışı bırakmayı mümkün kılar."
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD,
    "Parola Girin"
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
    "Parola geçerli."
    )
MSG_HASH(
    MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
    "Parola geçersiz."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
    "Bildirim Kırmızı Renk Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
    "Bildirim Yeşil Renk Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
    "Bildirim Mavi Renk Oranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
    "Ekran Kare Sayısı"
    )
MSG_HASH(
    MSG_CONFIG_OVERRIDE_LOADED,
    "Yapılandırmanın üzerine yazmaları yüklendi."
    )
MSG_HASH(
    MSG_GAME_REMAP_FILE_LOADED,
    "Oyun yeniden konumlandırma dosyası yüklendi."
    )
MSG_HASH(
    MSG_CORE_REMAP_FILE_LOADED,
    "Çekirdek yeniden konumlandırma dosyası yüklendi."
    )
MSG_HASH(
    MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
    "RunAhead devre dışı bırakıldı, çünkü bu Çekirdek konum kayıtlarını desteklemiyor."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
    "Konum kaydedilemedi. RunAhead devre dışı bırakıldı."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
    "Konum yüklenemedi. RunAhead devre dışı bırakıldı."
    )
MSG_HASH(
    MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
    "Failed to create second instance. RunAhead will now use only one instance."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
    "Oynatma listelerine otomatik olarak içerik ekleyin"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUTOMATICALLY_ADD_CONTENT_TO_PLAYLIST,
    "Oynatılan tarayıcı ile yüklü içeriği otomatik olarak tarar."
    )
MSG_HASH(
    MSG_SCANNING_OF_FILE_FINISHED,
    "Dosyanın taranması tamamlandı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
    "Pencere Opaklığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
    "Resampler Kalitesi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
    "Performans/düşük gecikme süresi için düşük gecikme süresi için bu değeri düşürün, düşük performans/düşük gecikme pahasına daha iyi ses kalitesi istiyorsanız, artırın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
    "Gölgelendirici dosyalarını değişiklikleri izleyin"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
    "Diskteki gölgelendirici dosyalarında yapılan değişiklikleri otomatik uygulayın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
    "Pencere Süslemelerini Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
    "İstatistikleri göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
    "Ekrandaki teknik istatistikleri göster."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
    "Border filler"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
    "Display menu border."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
    "Border filler thickness"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
    "Increase coarseness of menu border chequerboard pattern."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
    "Background filler thickness"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
    "Increase coarseness of menu background chequerboard pattern."
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
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
    "Internal Upscaling"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
    "Upscale menu interface before drawing to screen. When used with 'Menu Linear Filter' enabled, removes scaling artefacts (uneven pixels) while maintaining a sharp image. Has a significant performance impact that increases with upscaling level."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
    "Menü En Boy Oranı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
    "Menü en boy oranını seçin. Geniş ekran oranları, menü arayüzünün yatay çözünürlüğünü arttırır. ('Menü En Boy Oranını Kilitle' devre dışı bırakılmışsa yeniden başlatmayı gerektirebilir)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
    "Use Full-Width Layout"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
    "Kullanılabilir ekran alanını en iyi şekilde kullanmak için menü girişlerini yeniden boyutlandırın ve yerleştirin. Klasik sabit genişlikte iki sütun düzenini kullanmak için bunu devre dışı bırakın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
    "Gölge Efektleri"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
    "Menü metni, kenarlıklar ve küçük resimler için alt gölgeleri etkinleştirin. Mütevazı derecede performansa etkisi var."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
    "Sadece CRT görüntüler için. Tam Çekirdek/Oyun çözünürlüğü ve yenileme hızı kullanmaya çalışır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
    "CRT SwitchRes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
    "Yerel ve ultra süper çözünürlükler arasında geçiş yapın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
    "CRT Süper Çözünürlük"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
    "Geri Sarma Ayarlarını Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
    "Geri Sarma seçeneklerini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
    "Gecikme seçeneklerini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
    "Gecikme ayarlarını göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
    "Kaplama seçeneklerini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
    "Kaplama ayarlarını göster"
    )
#ifdef HAVE_VIDEO_LAYOUT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
    "Video Düzeni Ayarlarını Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
    "Video Düzeni seçeneklerini göster/gizle."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
    "Mikser"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
    "Menüde bile aynı anda ses akışlarını oynatın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
    "Mikser ayarları"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
    "Ses karıştırıcı(Mikser) ayarlarını görüntüleyin ve/veya değiştirin."
    )
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
    "&Çekirdek yükle..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
    "&Çekirdeği çıkart"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
    "Çıkış"
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
    "&Görüntüleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
    "Closed Docks"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
    "Shader Parameters"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
    "&Ayarlar..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
    "Dock pozisyonlarını hatırla:"
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
    "Karanlık"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
    "Özelleştirilmiş..."
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
    "Belgeler"
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
    "Versiyon"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
    "Oynatma listeleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
    "Dosya tarayıcısı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
    "En üst"
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
    "Ekran görütüsü"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
    "Başlık Ekranı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
    "Tüm oynatma listeleri"
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
    "<Bana Sor>"
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
    "Ağ hatası"
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
    "%1 items"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Resmi buraya bırakın"
   )
#ifdef HAVE_QT
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
    "Tarama bitti.<br><br>\n"
    "İçeriğin doğru şekilde taranmış olması için, dikkat edilmesi gerekenler:\n"
    "<ul><li>Uyumlu Çekirdekler önceden indirilmiş olmalı</li>\n"
    "<li>have \"Çekirdek Bilgi Dosyaları\" Çevrimiçi Güncelleyici ile güncellenmiş olmalı</li>\n"
    "<li>have \"Veritabanı\" Çevrimiçi Güncelleyici ile güncellenmiş olmalı</li>\n"
    "<li>Yukarıdakilerden herhangi biri yeni yapıldıysa RetroArch'ı yeniden başlatın</li></ul>\n"
    "Son olarak, içeriğin <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">buradaki</a> varolan veritabanlarıyla eşleşmesi gerekir. Hala çalışmıyorsa, <a href=\"https://www.github.com/libretro/RetroArch/issues\"> bir hata raporu göndermeyi düşünün</a>."
    )
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
    "Masaüstü Menüsünü Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SHOW_WIMP,
    "Masaüstü menüsünü açar."
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
    "Sakla"
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
    "Renk seç"
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
    "Gör"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
    "İkonlar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
    "Liste"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
    "Üzerine yazmalar"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
    "Genel yapılandırmanın üzerine yazma seçenekleri."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
    "Ses akışının oynatılmasını başlatır. Tamamlandığında, mevcut ses akışını bellekten kaldıracak."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
    "Ses akışının oynatılmasını başlatır. Tamamlandığında, tekrar baştan başlayıp tekrar çalmaya başlayacaktır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
    "Ses akışının oynatılmasını başlatır. Tamamlandığında, sıralı sırayla bir sonraki ses akışına atlar ve bu davranışı tekrarlar. Albüm çalma modu olarak kullanışlıdır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
    "Ses akışının çalınmasını durdurur, ancak bellekten çıkarmaz. 'Oynat' seçeneğini seçerek tekrar oynamaya başlayabilirsiniz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
    "Ses akışının çalınmasını durdurur ve tamamen bellekten kaldırır."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
    "Ses akışının sesini ayarlayın."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
    "Ses parçasını kullanılabilir bir ses akışı yuvasına ekleyin. Şu anda mevcut slot bulunmuyorsa, dikkate alınmaz."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
    "Ses parçasını kullanılabilir bir ses akışı yuvasına ekleyin ve oynatın. Şu anda mevcut slot bulunmuyorsa, dikkate alınmaz."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
    "Oynat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
    "Oynat (Döngüsel)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
    "Oynat (Ardışık)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
    "Durdur"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
    "Kaldır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
    "Ses"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
    "Geçerli Çekirdek"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
    "Temizle"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
    "Mevcut oturum için başarıları duraklatın (Bu işlem konum kaydetmeleri, hileleri, geri sarma, duraklatma ve ağır çekimi etkinleştirir)."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
    "Mevcut oturum için başarıları sürdürün (Bu işlem konum kaydetmeleri, hileleri, geri sarma, duraklatma ve ağır çekimi devre dışı bırakır ve mevcut oyunu sıfırlar)."
    )
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
    "Oyun-İçinde (Durdurulduğunda)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
    "Oynatılıyor"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
    "Durduruldu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
    "Discord Rich Presence"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
    "Discord uygulamasının, oynatılan içerik hakkında daha fazla veri göstermesine izin verir.\n"
    "NOT: Tarayıcı sürümüyle çalışmaz, yalnızca masaüstü istemcisiyle çalışır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
    "Girdi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_INPUT,
    "Giriş cihazı seçin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
    "Çıktı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
    "Çıkış cihazı seçin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
    "Ses"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MIDI_VOLUME,
    "Çıkış sesini ayarla (%)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
    "Güç yönetimi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
    "Güç yönetimi ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
    "Sürdürülebilir Performans Modu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
    "mpv desteği"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
    "Indeks"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
    "Eşleşmeyi Göster #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
    "Match Address: %08X Mask: %02X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
    "Kod Eşleşmesi Yaratın #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
    "Eşleşmeyi Sil #"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
    "Browse Address: %08X"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
    "Açıklama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
    "Etkinleştirildi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
    "Kod"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
    "Handler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
    "Memory Search Size"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
    "Tip"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
    "Değer"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
    "Memory Address"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
    "Memory Address Mask"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
    "Rumble When Memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
    "Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
    "Rumble Port"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
    "Rumble Primary Strength"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
    "Rumble Primary Duration (ms)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
    "Rumble Secondary Strength"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
    "Rumble Secondary Duration (ms)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
    "İterasyon Sayısı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
    "Her Tekrarlamada Değer Artırma"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
    "Her İterasyonda Adresin Arttırılması"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
    "Bundan Sonra Yeni Hile Ekleyin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
    "Bundan Önce Yeni Hile Ekleyin"
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
    "Bu Hileyi Sil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
    "Emulator"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_RETRO,
    "RetroArch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
    "<Devre dışı bırakıldı>"
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
    "Run next cheat if value = memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
    "Run next cheat if value != memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
    "Run next cheat if value < memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
    "Run next cheat if value > memory"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
    "<Devre dışı bırakıldı>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
    "Değişiklikler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
    "Değiştirmez"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
    "Arttırır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
    "Azalttırır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
    "= Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
    "!= Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
    "< Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
    "> Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
    "Increases by Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
    "Decreases by Rumble Value"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
    "1-bit, maksimum değer = 0x01"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
    "2-bit, maksimum değer = 0x03"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
    "4-bit, maksimum değer = 0x0F"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
    "8-bit, maksimum değer = 0xFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
    "16-bit, maksimum değer = 0xFFFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
    "32-bit, maksimum değer = 0xFFFFFFFF"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_0,
    "1"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_1,
    "2"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_2,
    "3"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_3,
    "4"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_4,
    "5"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_5,
    "6"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_6,
    "7"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_7,
    "8"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_8,
    "9"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_9,
    "10"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_10,
    "11"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_11,
    "12"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_12,
    "13"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_13,
    "14"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_14,
    "15"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_15,
    "16"
    )
MSG_HASH(
    MENU_ENUM_LABEL_RUMBLE_PORT_16,
    "Hepsi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
    "Hile Aramaya Başla veya Devam Et"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
    "Hile Aramasını Başlat veya Yeniden Başlat"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
    "Search Memory For Values"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
    "Değerler için Bellekte Ara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
    "Değerler için Bellekte Ara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
    "Değerler için Bellekte Ara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
    "Değerler için Bellekte Ara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
    "Değerler için Bellekte Ara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
    "Değerler için Bellekte Ara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
    "Değerler için Bellekte Ara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
    "Değerler için Bellekte Ara"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
    "%u Eşleşmeyi Listenize Ekle"
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
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
    "Yukarıya Yeni Kod Ekleyin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
    "Dibe Yeni Kod Ekleme"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
    "Bütün Kodları Sil"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
    "Oyuna Özel Hileleri Yeniden Yükle"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
    " %u (%X) eşittir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
    "Öncekinden daha az"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
    "Öncekinden Büyüktür"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
    "Öncekinden Küçüktür veya Eşittir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
    "Öncekinden Büyüktür veya Eşittir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
    "Öncekine Eşittir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
    "Öncekine Eşit değildir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
    "Öncekine Eşittir+%u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
    "Öncekine Eşitti-%u (%X)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
    "Hile Aramaya Başla veya Devam Et"
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
    MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
    "Düşük Son Haneli"
    )
MSG_HASH(
    MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
    " %u eşleşmeleri eklendi"
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
    "Yetersiz yer. Sahip olabileceğiniz toplam hilelerin sayısı 100'dür."
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
    MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
    "Tüm hileleri silmek için sağa beş kez basın."
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
    MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
    "İlerleme:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
    "\"Tüm Oynatma Listeleri\" maksimum liste girişleri:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
    "\"Tüm Oynatma Listeleri\" maksimum ızgara girişi:"
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
    "Oynatma Listesini Yeniden Adlandır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
    "\"%1\" oynatma listesini silmek istediğine emin misin?"
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
    "Dosyaların toplanıyor ..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
    "Oynatma listesine dosyalar ekleniyor..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
    "Oynatma Listesi Girişi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
    "Ad:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
    "Dizin:"
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
    "Uzantılar:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
    "(boşlukla ayrılmış; varsayılan olarak tümünü içerir)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
    "Arşivlerin içini filtrele"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
    "(küçük resimleri bulmak için kullanılır)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
    "Are you sure you want to delete the item \"%1\"?"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
    "Lütfen önce tek bir oynatma listesi seçin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DELETE,
    "Kaldır"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
    "Giriş Ekle..."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
    "Dosya(ları) ekle..."
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
    "Klasörleri Seç"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
    "<birden fazla>"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
    "Oyatma listesi girişi güncellenirken hata oluştu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
    "Lütfen tüm gerekli alanları doldurunuz."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
    "RetroArch'ı güncelle (nightly)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
    "RetroArch başarıyla güncellendi. Değişikliklerin etkili olması için lütfen yeniden başlatın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
    "Güncelleme başarısız oldu."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
    "Katkıda bulunanlar"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
    "Geçerli gölgelendirici"
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
    "Küçük Resim İndir"
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
   "Simge görünümü küçük resmi türü:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Küçük resim önbellek sınırı:"
   )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
    "Tüm Küçük Resimleri İndir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
    "Tüm sistemin"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
    "Bu Oynatma Listesinin"
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
    MSG_DEVICE_CONFIGURED_IN_PORT,
    "Yapılandırılan port:"
    )
MSG_HASH(
    MSG_FAILED_TO_SET_DISK,
    "Disk ayarlanamadı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
    "Çekirdek Seçenekleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
    "Adaptive Vsync"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
    "V-Sync, performans hedef yenileme hızının altına düşene kadar etkindir.\n"
    "Performans gerçek zamanın altına düştüğünde kırılmaları en aza indirebilir ve enerji açısından daha verimli olabilir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
    "CRT SwitchRes"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
    "CRT ekranlarında kullanmak için yerel, düşük çözünürlüklü sinyaller verir."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
    "Görüntü ekranda doğru şekilde ortalanmamışsa bu seçenekler arasında gezinin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
    "X-Axis Centering"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
    "Gerekirse, yapılandırma dosyasında belirtilen özel bir yenileme hızı kullanın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
    "Özel Yenileme Hızını Kullan"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
    "Select the output port connected to the CRT display."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
    "Output Display ID"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
    "Kayıt Yap"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
    "Kayıt yapmayı başlatır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
    "Kaydetmeyi durdur"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
    "Kaydı durdurur."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
    "Yayın Yap"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
    "Yayın yapmayı başlatır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
    "Yayın yapmayı durdur"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
    "Yayın yapmayı durdurur."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
    "Kayıt açma-kapama"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
    "Yayın açma-kapama"
    )
MSG_HASH(
    MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
    "Bir konum kaydı yüklendi, geçerli oturum için Hardcore Mode'un Başarımları devre dışı bırakıldı. Hardcore Modu etkinleştirmek için yeniden başlatın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
    "Kayıt Kalitesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
    "Yayın Kalitesi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_URL,
    "Akış URL’si"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
    "UDP Yayın Portu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_TWITCH,
    "Twitch"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCOUNTS_YOUTUBE,
    "YouTube"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
    "Twitch Yayıncı Anahtarı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
    "YouTube Yayıncı Anahtarı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
    "Yayıncı Modu"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
    "Yayın Başlığı"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
    "Joy-Con'u böl"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
    "Varsayılanlara dön"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
    "Geçerli yapılandırmayı varsayılan değerlere sıfırlayın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
    "Tamam"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
    "Menü Renk Teması"
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
    MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
    "Farklı bir renk teması seçin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
    "Kenar çubuğunu daralt"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
    "Sol kenar çubuğunu daima daraltın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
    "Oynatma listesi adlarını kısalt"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
    "Etkinleştirildiğinde, sistem adlarını çalma listelerinden kaldırır. Örneğin, 'Sony - PlayStation' yerine 'PlayStation' yazar. Değişikliklerin etkili olması için yeniden başlatmayı gerektirir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
    "İçerik Meta Verileri İçin Kayan Metin Kullanın"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
    "Etkinleştirildiğinde, çalma listelerinin sağ kenar çubuğunda gösterilen içerik meta verilerinin her bir maddesi (ilişkili çekirdek, çalma süresi) tek bir satır kaplar; Kenar çubuğunun genişliğini aşan dizeler kayan yazı metni olarak görüntülenir. Devre dışı bırakıldığında, içerik meta verilerinin her bir öğesi, gerektiği kadar satır tutacak şekilde kaydırılarak statik olarak görüntülenir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
    "Tercih edilen sistem renk temasını kullan"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
    "İşletim sisteminizin renk temasını kullanın (varsa) - tema ayarlarını geçersiz kılar."
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
    MSG_RESAMPLER_QUALITY_NORMAL,
    "Normal"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_HIGHER,
    "Yüksek"
    )
MSG_HASH(
    MSG_RESAMPLER_QUALITY_HIGHEST,
    "En Yüksek"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
    "Kullanılabilir bir müzik yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
    "Kullanılabilir bir video yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
    "Kullanılabilir bir resim yok."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
    "Favoriniz yok."
    )
MSG_HASH(
    MSG_MISSING_ASSETS,
    "Uyarı: Kayıp içerikler, varsa Çevrimiçi Güncelleyiciyi kullanın"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
    "Pencere Konumunu ve Boyutunu hatırla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HOLD_START,
    "Start'a Basılı Tutun (2 saniye)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
    "Eski formatı kullanarak oynatma listelerini kaydet"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
    "Oynatma listelerinde ilişkili Çekirdekleri göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
    "Oynatma listesi girişlerinin o anda ilişkilendirilmiş olan Çekirdeğin (varsa) ne zaman etiketleneceğini belirleyin. NOT: Oynatma listesi alt etiketleri etkinleştirildiğinde bu ayar dikkate alınmaz."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
    "Geçmiş & Favoriler"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
    "Her zaman"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
    "Asla"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
    "Geçmiş ve Favoriler"
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
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
    "Oynatma listelerini alfabetik olarak sırala"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
    "İçerik çalma listelerini alfabetik sıraya göre sıralar. Son kullanılan oyunların, resimlerin, müziklerin ve videoların 'geçmiş' oynatma listelerinin hariç tutulduğunu unutmayın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
    "Menü Sesleri"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
    "Tamam sesini etkinleştir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
    "İptal sesini etkinleştir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
    "Bildirim sesini etkinleştir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
    "BGM sesini etkinleştir"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
    "Down + Select"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
    "Grafik sürücünüz RetroArch'taki mevcut video sürücüsü ile uyumlu değil ve %s sürücüsüne geri dönülüyor. Lütfen değişikliklerin geçerli olması için RetroArch'ı yeniden başlatın."
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
    MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
    "Çalışma Günlüğünü Kaydet (her çekirdek için)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
    "Her bir içerik öğesinin ne kadar süre çalıştığını, kayıtlar Çekirdeğe ayrılmış olarak izler."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
    "Çalışma günlüğünü kaydet (toplam)"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
    "Tüm içeriğin toplamının toplam olarak kaydedildiği her bir içerik öğesinin ne kadar süre çalıştığını takip eder."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
    "Çalışma Zamanı Günlükleri"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
    "Oynatma zamanı günlük dosyalarını bu dizine kaydedin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
    "Oynatma listesi alt etiketlerini göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
    "Geçerli Çekirdek ilişkilendirme ve çalışma zamanı (varsa) gibi her oynatma listesi girişi için ek bilgi gösterir. Performansı etkiler(değişken)."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
    "Çekirdek:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
    "Oyun süresi:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
    "Son oynatma:"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
    "Oynatma listesi alt etiketi çalışma zamanı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
    "Oynatma listesinin alt etiketlerinde hangi çalışma zamanı günlüğü kaydının görüntüleneceğini seçer. (İlgili çalışma zamanı günlüğünün 'Kaydetme' seçenekler menüsünden etkinleştirilmesi gerektiğini unutmayın))"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
    "Çekirdek başına"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
    "Bulanık arşiv eşleştirme"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
    "Sıkıştırılmış dosyalarla ilişkili girişler için oynatma listelerini ararken, [dosya adı] + [içerik] yerine yalnızca arşiv dosya adıyla eşleştirin. Sıkıştırılmış dosyaları yüklerken yinelenen içerik geçmişi girişlerini önlemek için bunu etkinleştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
    "Toplam"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
    "Hata Ayıklama Bilgisi Gönder"
    )
MSG_HASH(
    MSG_FAILED_TO_SAVE_DEBUG_INFO,
    "Hata ayıklama bilgisi kaydedilemedi."
    )
MSG_HASH(
    MSG_FAILED_TO_SEND_DEBUG_INFO,
    "Sunucuya hata ayıklama bilgisi gönderilemedi."
    )
MSG_HASH(
    MSG_SENDING_DEBUG_INFO,
    "Hata ayıklama bilgisi gönderiliyor..."
    )
MSG_HASH(
    MSG_SENT_DEBUG_INFO,
    "Sunucuya hata ayıklama bilgisi başarıyla gönderildi. Kimlik numaranız %u."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
    "Analiz için cihazların ve RetroArch yapılandırmasına ilişkin teşhis bilgilerini gönderir."
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
    MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
    "Tuşa Basınca titret"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
    "Cihaz titreşimini etkinleştir (desteklenen Çekirdekler için)"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LOG_DIR,
    "Sistem Olayı Günlükleri"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_LOG_DIR,
    "Sistem olay günlüğü dosyalarını bu dizine kaydedin."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
      "Menü Araçları")
MSG_HASH(MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
      "Video Gölgelendiricileri")
MSG_HASH(MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
      "Çekirdek eşleşmesi olmadan tara")
MSG_HASH(MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
      "Devre dışı bırakıldığında, içerik yalnızca uzantılarını destekleyen bir Çekirdek varsa oynatma listelerine eklenir. Bunu etkinleştirerek, ne olursa olsun oynatma listesine ekleyecektir. Bu sayede, tarama işleminden sonra ihtiyacınız olan Çekirdeği yükleyebilirsiniz.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
      "Animation Horizontal Icon Highlight")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
      "Yukarı/Aşağı Hareket Etme Animasyonu")
MSG_HASH(MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
      "Açma/Kapama Ana Menü Animasyonları")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
    "GPU İndeksi"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
    "Disk Bilgisi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DISC_INFORMATION,
    "Takılan medya diskleri hakkındaki bilgileri görüntüleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
    "Frontend Günlük Seviyesi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
    "Ön uç için günlük seviyesini ayarlar. Ön uç tarafından verilen bir günlük seviyesi bu değerin altındaysa, dikkate alınmaz."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
      "Kare Hızı Güncelleme Aralığı (karelerde)")
MSG_HASH(MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
      "Kare hızı ekranı, ayarlanan aralıkta (kare olarak) güncellenir.")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
    "İçeriğini Yeniden Başlatma'yı Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
    "'İçeriği Yeniden Başlat' seçeneğini gösterin/gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "İçeriği Kapat'ı Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
    "'İçeriği Kapat' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
    "İçeriğe Devam Et'i Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
    "'İçeriğe Devam Et' seçeneğini göster/gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
    "Girişi Göster"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
    "Ayarlar"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
    "Ayarlar ekranındaki öğeleri gösterme veya gizleme."
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
    "Ayarlar ekranında 'Giriş Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
    "AI Servisi"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
    "AI Servisi için ayarları değiştirin (Tercüme / TTS / Çeşitli)."
    )
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
      "AI Servis Çıkışı")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
      "AI Servis URL'si")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
      "AI Servisi Etkinleştirildi")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_MODE, /* TODO/FIXME - update this - see US original */
      "Çeviri sırasında oyunu duraklatır (Görüntü modu) veya çalışmaya devam eder (Konuşma modu)")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
      "Kullanılacak çeviri hizmetini gösteren bir http:// url'si.")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
      "AI Servis kısayol tuşuna basıldığında çalışacak AI Servisi etkinleştirin.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
      "Hedef Dil")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
      "Hizmetin çevrileceği dil. 'Fark Etmez' olarak ayarlanırsa, varsayılan dil İngilizce olur.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
      "Kaynak Dil")
MSG_HASH(MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
      "Servisin çevireceği dil. 'Fark Etmez' olarak ayarlanırsa, dili otomatik olarak algılamaya çalışır. Belli bir dile ayarlamak çeviriyi daha doğru hale getirecek.")
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_CZECH,
    "Çekce"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_LANG_DANISH,
    "Danish"
    )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWEDISH,
   "İsveççe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CROATIAN,
   "Hırvatça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_CATALAN,
   "Katalanca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BULGARIAN,
   "Bulgarca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BENGALI,
   "Bengal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_BASQUE,
   "Bask"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AZERBAIJANI,
   "Azerice"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ALBANIAN,
   "Arnavutça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_AFRIKAANS,
   "Afrikaans"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ESTONIAN,
   "Estonya Dili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FILIPINO,
   "Filipince"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_FINNISH,
   "Fince"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GALICIAN,
   "Galiçyaca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GEORGIAN,
   "Gürcüce"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_GUJARATI,
   "Guceratça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HAITIAN_CREOLE,
   "Haiti Kreyolu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HEBREW,
   "İbranice"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HINDI,
   "Hintçe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_HUNGARIAN,
   "Macarca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ICELANDIC,
   "İzlandaca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_INDONESIAN,
   "Endonezce"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_IRISH,
   "İrlandaca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_KANNADA,
   "Kannada Dili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATIN,
   "Latince"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LATVIAN,
   "Letonca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_LITHUANIAN,
   "Litvanca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MACEDONIAN,
   "Makedonca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALAY,
   "Malayca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_MALTESE,
   "Maltaca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_NORWEGIAN,
   "Norveçce"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_PERSIAN,
   "Farsça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_ROMANIAN,
   "Romence"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SERBIAN,
   "Sırpça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVAK,
   "Slovakça"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SLOVENIAN,
   "Slovence"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_SWAHILI,
   "Svahili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TAMIL,
   "Tamilce"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_TELUGU,
   "Teluguca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_THAI,
   "Tayca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_UKRAINIAN,
   "Ukraynaca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_URDU,
   "Urduca"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_WELSH,
   "Galce"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LANG_YIDDISH,
   "Yidiş"
   )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
    "Sürücüleri Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
    "Ayarlar ekranında 'Sürücü Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
    "Videoyu Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
    "Ayarlar ekranında 'Video Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
    "Sesi Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
    "Ayarlar ekranında 'Ses Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
    "Gecikmeyi Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
    "Ayarlar ekranında 'Gecikme Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
    "Çekirdeği Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
    "Ayarlar ekranında 'Temel Ayarlar'ı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
    "Yapılandırmayı Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
    "Ayarlar ekranında 'Yapılandırma Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
    "Kaydetmeyi Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
    "Ayarlar ekranında 'Ayarları Kaydet'i göster veya gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
    "Günlüğü Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
    "Ayarlar ekranında 'Günlük Ayarlarını' göster veya gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
    "Show Frame Throttle"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
    "Show or hide 'Frame Throttle Settings' on the Settings screen."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
    "Kaydetmeyi Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
    "Ayarlar ekranında 'Kayıt Ayarlarını' gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
    "Ekrandaki Görünümü Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
    "Ayarlar ekranında 'Ekran Görünümü Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
    "Kullanıcı Arayüzünü Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
    "Ayarlar ekranında 'Kullanıcı Arabirimi Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
    "AI Servisi Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
    "Ayarlar ekranında 'AI Servis Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
    "Güç Yönetimini Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
    "Ayarlar ekranında 'Güç Yönetimi Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
    "Başarıları Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
    "Ayarlar ekranında 'Başarılar Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
    "Ağı Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
    "Ayarlar ekranında 'Ağ Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
    "Oynatma Listelerini Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
    "Ayarlar ekranında 'Oynatma Listeleri Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
    "Kullanıcıyı Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
    "Ayarlar ekranında 'Kullanıcı Ayarlarını' göster veya gizle."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
    "Dizini Göster"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
    "Ayarlar ekranında 'Dizin Ayarları'nı gösterin veya gizleyin."
    )
MSG_HASH(MENU_ENUM_SUBLABEL_LOAD_DISC,
      "Fiziksel ortam diski yükleyin. Öncelikle diskle kullanmayı düşündüğünüz çekirdeği (Çekirdek Yükle) seçmelisiniz.")
MSG_HASH(MENU_ENUM_SUBLABEL_DUMP_DISC,
      "Fiziksel ortam diskini dahili depolamaya dökün. Bir imaj dosyası olarak kaydedilecektir.")
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Resim Modu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Konuşma Modu"
   )
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
      "Kaldır")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
      "Belirli bir türdeki gölgelendirici hazır ayarlarını kaldırın.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Genel Hazır Ayarları Kaldır")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
      "Tüm içerik ve tüm çekirdekler tarafından kullanılan Genel Hazır Ayarı kaldırın.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Çekirdek Hazır Ayarını Kaldır")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
      "Şu anda yüklü olan çekirdekle çalışan tüm içerikler tarafından kullanılan Çekirdek Hazır Ayarını kaldırın.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "İçerik Dizini Hazır Ayarını Kaldır")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
      "Geçerli çalışma dizini içindeki tüm içerik tarafından kullanılan İçerik Dizini Hazır Ayarını kaldırın.")
MSG_HASH(
      MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Oyunu Hazır Ayarı Kaldır")
MSG_HASH(
      MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
      "Yalnızca söz konusu oyun için kullanılan Oyun Ön Ayarını kaldırın.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
      "Çerçeve Zaman Sayacı")
MSG_HASH(MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
      "Kare zaman sayacını etkileyen ayarları yapın (yalnızca threaded video devre dışı olduğunda etkindir).")
MSG_HASH(MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
      "Yalnızca eski metin sistemi yerine modern dekore edilmiş animasyonları, bildirimleri, göstergeleri ve kontrolleri kullanın.")
MSG_HASH(MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
      "Parça listesini sil")
#ifdef HAVE_LAKKA
MSG_HASH(MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
      "Wi-Fi Erişim Noktası")
MSG_HASH(MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
      "Wi-Fi Erişim Noktasını etkinleştirin veya devre dışı bırakın.")
MSG_HASH(MSG_LOCALAP_SWITCHING_OFF,
      "Wi-Fi Erişim Noktasını Kapatılıyor.")
MSG_HASH(MSG_WIFI_DISCONNECT_FROM,
      "Wi-Fi '%s' bağlantısnı kesiliyor")
MSG_HASH(MSG_LOCALAP_ALREADY_RUNNING,
      "Wi-Fi Erişim Noktası zaten başlatıldı")
MSG_HASH(MSG_LOCALAP_NOT_RUNNING,
      "Wi-Fi Erişim Noktası çalışmıyor")
MSG_HASH(MSG_LOCALAP_STARTING,
      "Wi-Fi Erişim Noktasını SSID=%s ve Passkey=%s ile başlatılıyor")
MSG_HASH(MSG_LOCALAP_ERROR_CONFIG_CREATE,
      "Wi-Fi Erişim Noktası yapılandırma dosyası oluşturulamadı.")
MSG_HASH(MSG_LOCALAP_ERROR_CONFIG_PARSE,
     "Yanlış yapılandırma dosyası -%s içinde APNAME veya PASSWORD bulunamadı")
#endif
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
    "Çekirdeklerin video sürücüsünü değiştirmesine izin ver"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
    "Çekirdeklerin, yüklü olanlardan farklı bir video sürücüsüne geçiş yapmasını sağlayın."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
    "AI Servisi Duraklatma Açma"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
    "Ekran çevrilirken çekirdeği duraklatır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
    "Manuel Tarama"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
    "İçerik dosyası adlarına dayalı yapılandırılabilir tarama. Veritabanına uygun içerik gerektirmez."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
    "İçerik Dizini"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
    "İçerik taramak için bir dizin seçer."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
    "Sistem Adı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
    "Taranan içeriğin ilişkilendirileceği bir 'sistem adı' belirtin. Oluşturulan çalma listesi dosyasına ad vermek ve çalma listesi küçük resimlerini tanımlamak için kullanılır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
    "Özel Sistem Adı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
    "Taranan içerik için manuel olarak bir 'sistem adı' belirtin. Yalnızca 'Sistem Adı' '<Özel>' olarak ayarlandığında kullanılır."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
    "Çekirdek"
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
    "Taramaya dahil edilecek dosya türlerinin alanla sınırlı listesi. Boşsa, tüm dosyaları içerir - veya bir çekirdek belirtilirse, çekirdek tarafından desteklenen tüm dosyalar geçerli olur."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
    "Arşivlerin İçini Tara"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
    "Etkinleştirildiğinde, arşiv dosyaları (.zip, .7z, vb.) Geçerli/desteklenen içerik aranacaktır. Tarama performansı üzerinde önemli bir etkisi olabilir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
    "Arcade DAT Dosyası"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
    "Taranan arcade içeriğinin (MAME, FinalBurn Neo, vb.) Otomatik olarak adlandırılmasını etkinleştirmek için bir Logiqx veya MAME List XML DAT dosyası seçin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
    "Mevcut Çalma Listesinin Üzerine Yaz"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
    "Etkinleştirildiğinde, içerik taramadan önce mevcut tüm çalma listeleri silinir. Devre dışı bırakıldığında, mevcut çalma listesi girişleri korunur ve yalnızca çalma listesinden eksik olan içerik eklenir."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
    "Taramayı Başlat"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
    "Seçilen içeriği tarayın."
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
    MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
    "Geçersiz Arcade DAT dosyası seçildi"
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
    "Seçilen Arcade DAT dosyası çok büyük (yetersiz boş bellek)"
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
    "Arcade DAT dosyası yüklenemedi (geçersiz format?)"
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
    "Geçersiz manuel tarama yapılandırması"
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
    "Geçerli içerik algılanmadı"
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_START,
    "Taranan içerik: "
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
    "Taranıyor: "
    )
MSG_HASH(
    MSG_MANUAL_CONTENT_SCAN_END,
    "Tarama tamamlandı: "
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
    "Erişilebilirliği Etkinleştir"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
    "Menüde gezinme için erişilebilirlik anlatıcısını açma/kapatma"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
    "Anlatıcı için konuşma hızını hızlıdan yavaşa ayarlayın"
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
    "Ekran Okuyucusu Konuşma Hızı"
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
    MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
    "Tam Ekran Modu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
    "Tam ekran modu ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
    "Pencere Modu"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
    "Pencereli mod ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
    "Çıktı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
    "Video çıkış ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
    "Senkronizasyon"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
    "Video senkronizasyon ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
    "Çıktı"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
    "Ses çıkışı ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
    "Senkronizasyon"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
    "Ses senkronizasyonu ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
    "Resampler"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
    "Ses örnekleyici ayarlarını değiştirin."
    )
MSG_HASH(
    MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
    "Menü Kontrolleri"
    )
MSG_HASH(
    MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
    "Menü kontrol ayarlarını değiştirin."
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
    MENU_ENUM_LABEL_VALUE_START_GONG,
    "Gong'u başlat"
    )

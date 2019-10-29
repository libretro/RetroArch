/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include <compat/strl.h>
#include <string/stdstring.h>

#include "../msg_hash.h"
#include "../verbosity.h"

#ifdef RARCH_INTERNAL
#include "../configuration.h"

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

int menu_hash_get_help_tr_enum(enum msg_hash_enums msg, char *s, size_t len)
{
    settings_t *settings = config_get_ptr();

    if (msg == MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM)
    {
       snprintf(s, len,
             "TODO/FIXME - Fill in message here."
             );
       return 0;
    }
    if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
        msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
    {
       unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;

       switch (idx)
       {
          case RARCH_FAST_FORWARD_KEY:
             snprintf(s, len,
                   "Normal hız ve Hızlı-sarma arasında \n"
                   "geçiş yapar."
                   );
             break;
          case RARCH_FAST_FORWARD_HOLD_KEY:
             snprintf(s, len,
                   "Hızlı sarma için basılı tutun. \n"
                   " \n"
                   "Tuşu salmak hızlı-sarmayı iptal eder."
                   );
             break;
          case RARCH_SLOWMOTION_KEY:
             snprintf(s, len,
                   "Slowmotion arasında geçiş yapar.");
             break;
          case RARCH_SLOWMOTION_HOLD_KEY:
             snprintf(s, len,
                   "Slowmotion için basılı tutun.");
             break;
          case RARCH_PAUSE_TOGGLE:
             snprintf(s, len,
                   "Duraklatılmış ve duraklatılmamış durum arasında geçiş yap.");
             break;
          case RARCH_FRAMEADVANCE:
             snprintf(s, len,
                   "İçerik duraklatıldığında kare ilerlemesi.");
             break;
          case RARCH_SHADER_NEXT:
             snprintf(s, len,
                   "Dizindeki bir sonraki gölgelendiriciyi uygular.");
             break;
          case RARCH_SHADER_PREV:
             snprintf(s, len,
                   "Dizine önceki gölgelendiriciyi uygular.");
             break;
          case RARCH_CHEAT_INDEX_PLUS:
          case RARCH_CHEAT_INDEX_MINUS:
          case RARCH_CHEAT_TOGGLE:
             snprintf(s, len,
                   "Hileler.");
             break;
          case RARCH_RESET:
             snprintf(s, len,
                   "İçeriği sıfırla.");
             break;
          case RARCH_SCREENSHOT:
             snprintf(s, len,
                   "Ekran görüntüsü almak.");
             break;
          case RARCH_MUTE:
             snprintf(s, len,
                   "Sesi kapat/aç.");
             break;
          case RARCH_OSK:
             snprintf(s, len,
                   "Ekran klavyesini aç/kapat");
             break;
          case RARCH_FPS_TOGGLE:
             snprintf(s, len,
                   "Saniye sayacındaki kareleri değiştirir.");
             break;
          case RARCH_SEND_DEBUG_INFO:
             snprintf(s, len,
                   "Analiz için cihazınızın ve RetroArch yapılandırmasına ilişkin tanılama bilgilerini sunucularımıza gönderin.");
             break;
          case RARCH_NETPLAY_HOST_TOGGLE:
             snprintf(s, len,
                   "Netplay barındırma özelliğini açar/kapatır.");
             break;
          case RARCH_NETPLAY_GAME_WATCH:
             snprintf(s, len,
                   "Netplay toggle play/spectate mode.");
             break;
          case RARCH_ENABLE_HOTKEY:
             snprintf(s, len,
                   "Diğer kısayol tuşlarını etkinleştirin. \n"
                   " \n"
                   "Bu kısayol tuşu bir klavyeye, \n"
                   "joybutton veya joyaxis'e bağlıysa, \n"
                   "diğer tüm kısayol tuşları yalnızca \n"
                   "aynı anda tutulursa etkinleştirilir. \n"
                   " \n"
                   "Alternatif olarak, klavye için tüm \n"
                   "kısayol tuşları kullanıcı tarafından devre dışı edilebilir.");
             break;
          case RARCH_VOLUME_UP:
             snprintf(s, len,
                   "Ses seviyesini arttırır.");
             break;
          case RARCH_VOLUME_DOWN:
             snprintf(s, len,
                   "Ses seviyesini azaltır.");
             break;
          case RARCH_OVERLAY_NEXT:
             snprintf(s, len,
                   "Switches to next overlay. Wraps around.");
             break;
          case RARCH_DISK_EJECT_TOGGLE:
             snprintf(s, len,
                   "Diskler için çıkarmayı değiştirir. \n"
                   " \n"
                   "Birden fazla disk içeriği için kullanılır. ");
             break;
          case RARCH_DISK_NEXT:
          case RARCH_DISK_PREV:
             snprintf(s, len,
                   "Disk görüntüleri arasında geçiş yapar. Çıkardıktan sonra kullanın. \n"
                   " \n"
                   "Çıkartmayı tekrar açarak tamamlayın.");
             break;
          case RARCH_GRAB_MOUSE_TOGGLE:
             snprintf(s, len,
                   "Fare tutmayı değiştirir. \n"
                   " \n"
                   "Fare tutulduğunda, RetroArch fareyi gizler \n"
                   " ve göreceli fare girişinin daha iyi çalışmasını \n"
                   "sağlamak için fare işaretçisini pencerenin \n"
                   "içinde tutar.");
             break;
          case RARCH_GAME_FOCUS_TOGGLE:
             snprintf(s, len,
                   "Oyun odağını değiştirir.\n"
                   " \n"
                   "Bir oyuna odaklanıldığında, RetroArch hem kısayol tuşlarını \n"
                   "devre dışı bırakacak hem de farenin imlecini pencerenin içinde tutacaktır.");
             break;
          case RARCH_MENU_TOGGLE:
             snprintf(s, len, "Menü geçişi.");
             break;
          case RARCH_LOAD_STATE_KEY:
             snprintf(s, len,
                   "Konumu yükler.");
             break;
          case RARCH_FULLSCREEN_TOGGLE_KEY:
             snprintf(s, len,
                   "Tam ekrana geçer.");
             break;
          case RARCH_QUIT_KEY:
             snprintf(s, len,
                   "RetroArch'tan temiz bir şekilde çıkmak için basın. \n"
                   " \n"
                   "Herhangi bir şekilde kapatmak (SIGKILL, vb.) \n"
                   "RAM, vb. Kaydetmeden RetroArch'ı sonlandıracaktır."
#ifdef __unix__
                   "\nUnix gibilerde, SIGINT/SIGTERM temiz bir \n"
                   "yeniden başlatmaya izin verir."
#endif
                   "");
             break;
          case RARCH_STATE_SLOT_PLUS:
          case RARCH_STATE_SLOT_MINUS:
             snprintf(s, len,
                   "Konum Kayıtları \n"
                   " \n"
                   "Yuva 0 olarak ayarlanmış durumdayken, \n"
                   "kaydetme adı *.state (ya da komut satırında ne tanımlanmışsa) olur. \n"
                   " \n"
                   "Alan 0 değilse, yol <dizin> olduğu yerde, \n"
                   "<dizin><d> olur.");
             break;
          case RARCH_SAVE_STATE_KEY:
             snprintf(s, len,
                   "Konum kaydı");
             break;
          case RARCH_REWIND:
             snprintf(s, len,
                   "Geri sarmak için düğmeyi basılı tutun. \n"
                   " \n"
                   "Geri sarma etkin olmalı.");
             break;
          case RARCH_BSV_RECORD_TOGGLE:
             snprintf(s, len,
                   "Kayıt yapmak ve yapmamak arasında geçiş yapar.");
             break;
          default:
             if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
             break;
       }

       return 0;
    }

    switch (msg)
    {
        case MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS:
            snprintf(s, len, "Retro Achievements hesabı için \n"
                    "giriş bilgileri \n"
                    " \n"
                    "Retroachievements.org adresini ziyaret edin ve \n"
                    "ücretsiz bir hesap için kaydolun. \n"
                    " \n"
                    "Kayıt işlemini tamamladıktan sonra,  \n"
                    "kullanıcı adınızı ve şifrenizi RetroArch'a \n"
                    "girmeniz gerekir.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
            snprintf(s, len, "RetroAchievements hesabınızın kullanıcı adı.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
            snprintf(s, len, "RetroAchievements hesabınızın şifresi.");
            break;
        case MENU_ENUM_LABEL_USER_LANGUAGE:
            snprintf(s, len, "LMenüyü ve ekrandaki tüm mesajları burada \n"
                    "seçtiğiniz dile göre yerelleştirir. \n"
                    " \n"
                    "Değişikliklerin etkili olması için \n"
                    "yeniden başlatmayı gerektirir.  \n"
                    " \n"
                    "Not: Tüm diller şu anda uygulanamayabilir \n"
                    "Dilin uygulanmaması durumunda \n"
                    "İngilizce'ye geri dönülür.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
            snprintf(s, len, "Ekran Görününen metin için \n"
                    "kullanılan yazı tipini değiştirin.");
            break;
        case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
            snprintf(s, len, "İçeriğe özgü Çekirdek seçeneklerini otomatik olarak yükle.");
            break;
        case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
            snprintf(s, len, "Üzerine yazma yapılandırmalarını otomatik olarak yükle.");
            break;
        case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
            snprintf(s, len, "Yeniden düzenleme dosyalarını otomatik olarak yükle.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
            snprintf(s, len, "Kullanılan libretro coreundan sonra \n"
                    "adlandırılmış klasörlerdeki kaydetme durumlarını sıralayın.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
            snprintf(s, len, "Sıralama kullanılan libretro \n"
                    "Çekirdek adını klasörlerdeki dosyalarına kaydeder. ");
            break;
        case MENU_ENUM_LABEL_RESUME_CONTENT:
            snprintf(s, len, "Menüden çıkar ve içeriğe geri döner.");
            break;
        case MENU_ENUM_LABEL_RESTART_CONTENT:
            snprintf(s, len, "İçeriği yeniden başlatır.");
            break;
        case MENU_ENUM_LABEL_CLOSE_CONTENT:
            snprintf(s, len, "İçeriği kapatır ve bellekten kaldırır.");
            break;
        case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            snprintf(s, len, "Bir konum yüklendiyse, içerik \n"
                    "yüklenmeden önceki duruma geri döner.");
            break;
        case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            snprintf(s, len, "Bir konumun üzerine yazılmışsa, \n"
                    "önceki kaydetme durumuna geri döner.");
            break;
        case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            snprintf(s, len, "Bir ekran görüntüsü oluşturun. \n"
                    " \n"
                    "Ekran görüntüsü dizininde saklanacak.");
            break;
        case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            snprintf(s, len, "Girdinizi Favorilerinize ekleyin.");
            break;
        case MENU_ENUM_LABEL_RUN:
            snprintf(s, len, "İçeriği başlat.");
            break;
        case MENU_ENUM_LABEL_INFORMATION:
            snprintf(s, len, "İçerik hakkında ek meta veri \n"
                    "bilgilerini göster.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CONFIG:
            snprintf(s, len, "Konfigürasyon dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE:
            snprintf(s, len, "Sıkıştırılmış arşiv dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG:
            snprintf(s, len, "Kayıt yapılandırma dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CURSOR:
            snprintf(s, len, "Veritabanı imleci dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_CONFIG:
            snprintf(s, len, "Konfigürasyon dosyası.");
            break;
        case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
            snprintf(s, len,
                     "Geçerli dizinde içerik taramak bunu seçin.");
            break;
        case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
            snprintf(s, len,
                     "Burayı dizin olarak ayarlamak seçin.");
            break;
        case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
            snprintf(s, len,
                     "İçerik Veritabanı Dizini. \n"
                             " \n"
                             "İçerik veritabanı dizinine \n"
                             "giden yol.");
            break;
        case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
            snprintf(s, len,
                     "Küçük Resimler Dizini. \n"
                             " \n"
                             "Küçük resim dosyalarını saklamak için.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
            snprintf(s, len,
                     "Çekirdek Bilgi Dizini. \n"
                             " \n"
                             "Libretro Çekirdek bilgisini nerede \n"
                             "arayacağınıza dair bir dizin.");
            break;
        case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            snprintf(s, len,
                     "Oynatma Listesi Dizini. \n"
                             " \n"
                             "Tüm oynatma listesi dosyalarını \n"
                             "bu dizine kaydedin.");
            break;
        case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
            snprintf(s, len,
                     "Bazı çekirdeklerin \n"
                             "kapanma özelliği olabilir. \n"
                             " \n"
                             "Bu seçenek devre dışı bırakılırsa, \n"
                             "kapatma prosedürünün seçilmesi RetroArch'in \n"
                             "kapatılmasını tetikler. \n"
                             " \n"
                             "Bu seçeneği etkinleştirmek kukla bir Çekirdek \n"
                             "yükler böylelikle menüde kalırız \n"
                             "ve RetroArch kapanmaz.");
            break;
        case MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE:
            snprintf(s, len,
                     "Bazı Core'lar için firmware \n"
                             "veya bios dosyasına ihtiyaç duyulabilir. \n"
                             " \n"
                             "Eğer bu seçenek etkisizleştirilirse, \n"
                             "firmware olmasa bile yüklemeyi \n"
                             "deneyecektir. \n");
            break;
        case MENU_ENUM_LABEL_PARENT_DIRECTORY:
            snprintf(s, len,
                     "Üst dizine geri dönün.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS:
            snprintf(s, len,
                     "BroadFileSystemAccess özelliğini etkinleştirmek \n"
                     "için Windows izin ayarlarını açın. ");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER:
           snprintf(s, len,
                     "Ek dizinlere erişmek için \n"
                     "sistem dosyası seçiciyi açın.");
           break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
            snprintf(s, len,
                     "Hazır Gölgelendirici dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
            snprintf(s, len,
                     "Gölgelendirici dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
            snprintf(s, len,
                     "Remap kontrolleri dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CHEAT:
            snprintf(s, len,
                     "Hile dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY:
            snprintf(s, len,
                     "Kaplama dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RDB:
            snprintf(s, len,
                     "Veritabanı dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_FONT:
            snprintf(s, len,
                     "TrueType font dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE:
            snprintf(s, len,
                     "Plain dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN:
            snprintf(s, len,
                     "Video. \n"
                             " \n"
                             "Video oynatıcısını açmak için  \n"
                             "kullanılır.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
            snprintf(s, len,
                     "Müzik. \n"
                             " \n"
                             "Müzik oynatıcısını açmak için  \n"
                             "kullanılır.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
            snprintf(s, len,
                     "Resim dosyası.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
            snprintf(s, len,
                     "Resim. \n"
                             " \n"
                             "Resim görüntüleyecisini açmak için  \n"
                             "kullanılır.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
            snprintf(s, len,
                     "Libretro core. \n"
                             " \n"
                             "Bunu seçmek bu Core'u oyuna \n"
                             "bağlayacaktır.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            snprintf(s, len,
                     "Libretro Core. \n"
                             " \n"
                             "RetroArch'ın bu Core'u yüklemesi için bu dosyayı seçin.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Dizin. \n"
                             " \n"
                             "Dizin açmak için seçin.");
            break;
        case MENU_ENUM_LABEL_CACHE_DIRECTORY:
            snprintf(s, len,
                     "Önbellek Dizini. \n"
                             " \n"
                             "RetroArch tarafından sıkıştırılmış içerik \n"
                             "geçici olarak bu dizine çıkarılır.");
            break;
        case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
            snprintf(s, len,
                     "Etkinleştirilirse, RetroArch'a yüklenen \n"
                             "her içerik otomatik olarak en son \n"
                             "geçmiş listesine eklenir.");
            break;
        case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Dosya Tarayıcı Dizini. \n"
                             " \n"
                             "Menü dosyası tarayıcısı için başlangıç dizinini ayarlar.");
            break;
        case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            snprintf(s, len,
                     "Giriş yoklama işleminin RetroArch \n"
                             "içinde yapılmasının etkisi. \n"
                             " \n"
                             "Erken - çerçeve işlenmeden önce \n"
                             "Girdi yoklaması yapılır. \n"
                             "Normal - Yoklama talep edildiğinde girdi \n"
                             "yoklaması gerçekleştirilir. \n"
                             "Geç   - Giriş yoklama, çerçeve başına \n"
                             "ilk giriş durumu talebinde gerçekleşir. \n"
                             " \n"
                             "'Erken' veya 'Geç' olarak ayarlamak, yapılandırmanıza \n"
                             "bağlı olarak daha az gecikmeyle sonuçlanabilir. \n"
                             "Netplay kullanırken göz \n\n"
                             "ardı edilecektir."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
            snprintf(s, len,
                     "Çekirdek tarafından ayarlanmamış giriş \n"
                             "tanımlayıcılarını gizleyin.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
            snprintf(s, len,
                     "Monitörünüzün görüntü yenileme hızı. \n"
                             "Uygun bir ses giriş hızı hesaplamak için kullanılır.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
            snprintf(s, len,
                     "SRGB FBO desteğini zorla devre dışı bırakın. Windows'taki \n"
                             "bazı Intel OpenGL sürücülerinde sRGB FBO \n"
                             "desteği etkinken video sorunları yaşanabilir.");
            break;
        case MENU_ENUM_LABEL_AUDIO_ENABLE:
            snprintf(s, len,
                     "Ses çıkışını etkinleştir.");
            break;
        case MENU_ENUM_LABEL_AUDIO_SYNC:
            snprintf(s, len,
                     "Sesi senkronize et (önerilir).");
            break;
        case MENU_ENUM_LABEL_AUDIO_LATENCY:
            snprintf(s, len,
                     "Milisaniye cinsinden istenen ses gecikmesi. \n"
                             "Ses sürücüsü verilen gecikmeyi sağlayamıyorsa, \n"
                             "değer duyulmayabilir.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
            snprintf(s, len,
                     "Coreların dönüşünü ayarlamasına izin ver. Kapalıysa, \n"
                             "dönüş istekleri yerine getirilir, ancak dikkate alınmaz. \n\n"
                             "Monitörü manuel olarak döndüren kurulumlar \n"
                             "için kullanılır.");
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
            snprintf(s, len,
                     "Varsayılan olanların yerine \n"
                             "Çekirdek tarafından ayarlanan giriş tanımlayıcılarını gösterin.");
            break;
        case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
            snprintf(s, len,
                     "İçerik geçmişi çalma listesinde \n"
                             "tutulacak girişlerin sayısı.");
            break;
        case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
            snprintf(s, len,
                     "Pencereli mod kullanmak veya tam ekrana \n"
                             "geçmemek için. ");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
            snprintf(s, len,
                     "Ekrandaki mesajlar için yazı tipi boyutu.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
            snprintf(s, len,
                     "Otomatik olarak her kaydetme işleminde yuva, \n"
                             "endeksini artırarak birden fazla kayıt yeri dosyası oluşturur. \n"
                             "İçerik yüklendiğinde, durum yuvası mevcut en \n"
                             "yüksek değere ayarlanacaktır (son kayıt noktası).");
            break;
        case MENU_ENUM_LABEL_FPS_SHOW:
            snprintf(s, len,
                     "Saniyedeki mevcut karelerin görüntülenmesini \n"
                             " sağlar.");
            break;
        case MENU_ENUM_LABEL_MEMORY_SHOW:
            snprintf(s, len,
                     "Geçerli bellek kullanımı/toplamının FPS/Kareler \n"
                             "ile görüntülenmesini içerir.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
            snprintf(s, len,
                     "Ekrandaki mesajları göster veya gizle.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
            snprintf(s, len,
                     "Ekranda mesajların nereye yerleştirileceği için \n"
                             "ofseti. Değerler [0,0, 1,0] aralığındadır.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
            snprintf(s, len,
                     "Geçerli kaplamayı etkinleştirin veya devre dışı bırakın.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
            snprintf(s, len,
                     "Geçerli kaplamayı menü içinde \n"
                             "görünmesini engelleyin.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS:
            snprintf(s, len,
                      "Ekran kaplaması üzerindeki klavye/denetleyici \n"
                            "düğmesine basıldığında gösterir.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT:
            snprintf(s, len,
                      "Ekran kaplaması üzerinde gösterilecek denetleyici girişinin \n"
                            "dinleneceği portu seçin.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_PRESET:
            snprintf(s, len,
                     "Kaplamanın girdi yolu.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_OPACITY:
            snprintf(s, len,
                     "Kaplama opaklığı.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
            snprintf(s, len,
                     "Giriş bağlama zamanlayıcı zaman aşımı süresini (saniye olarak). \n"
                             "Bir sonraki bağlama işlemine kadar bekleyecek \n"
                             " saniye miktarı.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_HOLD:
            snprintf(s, len,
               "Giriş tuşunun basılı tutma süresi (saniye cinsinden). \n"
               "Basılı tutma için geçerli olan saniye miktarı.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_SCALE:
            snprintf(s, len,
                     "Kaplama ölçeği.");
            break;
        case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
            snprintf(s, len,
                     "Ses çıkışı örneklemesi.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
            snprintf(s, len,
                     "Donanım tarafından oluşturulan Core'un \n"
                             "kendi özel bağlamlarını alması gerekiyorsa 'true' \n"
                             "olarak ayarlayın. Donanım durumlarının çerçeveler \n"
                             "arasında değişiklik yapması gerektiğini unutmayın."
            );
            break;
        case MENU_ENUM_LABEL_CORE_LIST:
            snprintf(s, len,
                     "Çekirdek Yükle. \n"
                             " \n"
                             "Libretro Çekirdek uygulaması \n"
                             "için göz atın. Tarayıcının başladığı yer \n"
                             "Çekirdek Dizin yolunuza bağlıdır \n"
                             "Boşsa, root'ta başlayacaktır. \n"
                             " \n"
                             "Çekirdek Dizini bir dizinse, menü bunu üst klasör olarak kullanır. \n"
                             "Çekirdek Dizini tam yol ise, \n"
                             "dosyanın bulunduğu klasörde \n"
                             "başlayacaktır.");
            break;
        case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
            snprintf(s, len,
                     "Menüyü kontrol etmek için \n"
                             "gamepad'inizde veya klavyenizde aşağıdaki\n"
                             "kontrolleri kullanabilirsiniz: \n"
                             " \n"
            );
            break;
        case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
            snprintf(s, len,
                     "RetroArch'a Hoşgeldiniz\n"
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC: {
            /* Work around C89 limitations */
            char u[501];
            const char *t =
                    "RetroArch, en iyi performans sonuçları için\n"
                            "ekranınızın yenileme hızına göre kalibre edilmesi gereken\n"
                            "benzersiz bir ses/video senkronizasyonu şekline dayanır.\n"
                            "Herhangi bir ses çatlaması veya video yırtılması yaşarsanız,\n"                   
                            " \n"
                            "genellikle ayarları yapmanız anlamına gelir.\n"
                            "Aşağıdaki seçenekler gibi:\n"
                            " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                     "a) '%s' -> '%s' gidin, ve 'Threaded Video'\n"
                             "etkinleştirin. Yenileme hızı bu modda\n"
                             "önemli olmayacaktır, kare hızı daha yüksek\n"
                             "olacaktır ancak video daha az düzgün olabilir.\n"
                             "b) '%s' -> '%s' gidin, ve '%s' bakın\n"
                             "2048 karede çalışmasına izin verin,\n"
                             "ardından 'Tamam'a basın.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO));
            strlcpy(s, t, len);
            strlcat(s, u, len);
        }
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC:
            snprintf(s, len,
                     "İçerik taramak için, '%s' gidin ve \n"
                             "'%s' veya %s' seçin.\n"
                             " \n"
                             "Dosyalar veritabanı girişleriyle karşılaştırılacak.\n"
                             "Bir eşleşme varsa, koleksiyona bir giriş ekler.\n"
                             " \n"
                             "Bu içeriğe daha sonra kolayca erişebilmek için\n"
                             "'%s' gidin. ->\n"
                             "'%s'\n"
                             "Her seferinde dosya tarayıcısına\n"
                             "gitmek zorunda kalmazsınız.\n"
                             " \n"
                             "NOTE: Bazı içerikler Corelar tarafından\n"
                             "taranmayabilir.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
            snprintf(s, len,
                     "RetroArch'a Hoşgeldiniz\n"
                             "\n"
                             "Varlıklar ayıklanıyor, lütfen bekleyin.\n"
                             "İşlem biraz zaman alabilir...\n"
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.input_driver : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
                     snprintf(s, len,
                           "udev sürücüsü. \n"
                           " \n"
                           "Joystick desteği için en son evdev \n"
                           "joypad API'sini kullanır. Hotplugging \n"
                           "özelliğini destekler ve geribildirimi zorlar. \n"
                           " \n"
                           "Sürücü, klavye desteği için evdev olaylarını \n"
                           "okur. Ayrıca klavye geri çağırma, fareler ve \n"
                           "dokunmatik yüzeyleri de destekler.  \n"
                           " \n"
                           "Çoğu dağıtımda varsayılan olarak, /dev/input düğümleri \n"
                           "yalnızca root'tur (mod 600). Bunları root olmayanlar \n"
                           "için erişilebilir kılan bir udev kuralı ayarlayabilirsiniz."
                           );
               else if (string_is_equal(lbl,
                        msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
                     snprintf(s, len,
                           "linuxraw sürücüsü. \n"
                           " \n"
                           "Bu sürücü aktif bir TTY gerektiriyor. \n"
                           "Klavye olayları doğrudan TTY'den okunur; \n"
                           "bu da onu basitleştirir, ancak udev kadar esnek değildir. \n" "Mice, vb, desteklenmiyor. \n"
                           " \n"
                           "Bu sürücü eski joystick API'sini kullanır \n"
                           "(/dev/input/js*).");
               else
                     snprintf(s, len,
                           "Giriş sürücüsü.\n"
                           " \n"
                           "Video sürücüsüne bağlı olarak, \n"
                           "farklı bir giriş sürücüsünü zorlayabilir. ");
            }
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            snprintf(s, len,
                     "İçeriği Yükle. \n"
                             "İçeriğe göz at. \n"
                             " \n"
                             "İçeriği yüklemek için, 'Core' ve \n"
                             "içerik dosyasına ihtiyacınız vardır.  \n"
                             " \n"
                             "Menünün, içeriğe göz atmaya başlayacağı yeri \n"
                             "kontrol etmek için 'Dosya Tarayıcı Dizini'ni  \n"
                             "ayarlayın. Ayarlanmazsa, \n"
                             "kök dizininden başlayacaktır. \n"
                             " \n"
                             "Tarayıcı, 'Çekirdek Yükle'den ayarlanan son Çekirdek \n"
                             "için uzantıları filtreleyecek ve içerik \n"
                             "yüklendiğinde bu çekirdeği kullanacaktır."
            );
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
            snprintf(s, len,
                     "Geçmişten içerik yükleniyor. \n"
                             " \n"
                             "İçerik yüklendikçe, içerik ve libretro \n"
                             "Çekirdek kombinasyonları geçmişe kaydedilir. \n"
                             " \n"
                             "Geçmiş, RetroArch yapılandırma dosyasıyla aynı \n"
                             "dizindeki bir dosyaya kaydedilir. Başlangıçta hiçbir \n"
                             "yapılandırma dosyası yüklenmemişse, geçmiş kaydedilmeyecek \n"
                             "veya yüklenmeyecek ve ana menüde bulunmayacaktır. "
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_DRIVER:
            snprintf(s, len,
                     "Geçerli Video sürücüsü.");

            if (string_is_equal(settings->arrays.video_driver, "gl"))
            {
                snprintf(s, len,
                         "OpenGL Video sürücüsü. \n"
                                 " \n"
                                 "Bu sürücü, yazılım tarafından oluşturulan \n"
                                 "Çekirdek uygulamalarına ek olarak libretro GL \n"
                                 "çekirdeklerinin kullanılmasına izin verir.\n"
                                 " \n"
                                 "Yazılım tarafından oluşturulan ve libretro GL \n"
                                 "Çekirdek uygulamaları için performans, \n"
                                 "grafik kartınızın temelindeki GL sürücüsüne bağlıdır.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
            {
                snprintf(s, len,
                         "SDL 2 Video sürücüsü.\n"
                                 " \n"
                                 "Bu bir SDL 2 yazılımı tarafından oluşturulan \n"
                                 "video sürücüsüdür.\n"
                                 " \n"
                                 "Yazılım tarafından oluşturulan libretro Çekirdek uygulamaları \n"
                                 "için performans, SDL uygulamanıza bağlıdır.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
            {
                snprintf(s, len,
                         "SDL Video sürücüsü.\n"
                                 " \n"
                                 "Bu bir SDL 1.2 yazılımı tarafından üretilmiş \n"
                                 "video sürücüsüdür.\n"
                                 " \n"
                                 "Performansın yetersiz olduğu kabul edilir. \n"
                                 "Sadece son çare olarak kullanmayı düşünün.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "d3d"))
            {
                snprintf(s, len,
                         "Direct3D Video sürücüsü. \n"
                                 " \n"
                                 "Yazılım tarafından oluşturulan Çekirdek performansı,\n"
                                 "grafik kartınızın temelindeki D3D \n"
                                 "sürücüsüne bağlıdır.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "exynos"))
            {
                snprintf(s, len,
                         "Exynos-G2D Video sürücüsü. \n"
                                 " \n"
                                 "Bu, düşük seviye bir Exynos video sürücüsüdür. \n"
                                 "Karışım işlemleri için Samsung Exynos SoC'daki \n"
                                 " G2D bloğunu kullanır. \n"
                                 " \n"
                                 "Yazılım tarafından oluşturulan Çekirdek performansı \n"
                                 "optimum olmalıdır.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "drm"))
            {
                snprintf(s, len,
                         "Plain DRM Video sürücüsü. \n"
                                 " \n"
                                 "Bu düşük bir seviye video sürücüsüdür. \n"
                                 "GPU kaplamalarını için libdrm donanım ölçeklendirmesi kullanır.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
            {
                snprintf(s, len,
                         "Sunxi-G2D Video sürücüsü. \n"
                                 " \n"
                                 "Bu düşük seviye bir Sunxi video sürücüsü. \n"
                                 "Allwinner SoC'lerde G2D bloğunu kullanır.");
            }
            break;
        case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            snprintf(s, len,
                     "Audio DSP ektesi.\n"
                             "Sesi sürücüye göndermeden \n"
                             "önce işler."
            );
            break;
        case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
                  strlcpy(s,
                        "Pencereli SINC uygulaması.", len);
               else if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
                  strlcpy(s,
                        "Kıvrımlı kosinüs uygulaması.", len);
               else if (string_is_empty(s))
                  strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            }
            break;

    case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION: snprintf(s, len, "CRT OLARAK BELIRLE");
      break;

    case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER: snprintf(s, len, "SUPER CRT OLARAK BELIRLE");
      break;

        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            snprintf(s, len,
                     "Shader Öne Ayarı yükleyin. \n"
                             " \n"
                             "Doğrudan bir gölgelendirici önayarı yükleyin. \n"
                             "Menü gölgelendirici menüsü buna göre güncellenir. \n"
                             " \n"
                             "CGP basit olmayan ölçeklendirme yöntemleri kullanıyorsa, \n"
                             "(yani kaynak ölçeklendirme, X / Y için aynı ölçeklendirme faktörü), \n"
                             "menüde görüntülenen ölçeklendirme faktörü doğru olmayabilir. \n"
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
            snprintf(s, len,
                     "Bu geçişteki ölçek. \n"
                             " \n"
                             "Ölçek faktörü birikir, yani ilk geçiş \n"
                             "için 2x ve ikinci geçiş için 2x \n"
                             "size toplam 4x ölçek verir. \n"
                             " \n"
                             "Son geçiş için bir ölçek faktörü varsa, \n"
                             "sonuç 'Varsayılan Filtre'de belirtilen \n"
                             "filtre ile ekrana uzatılır. \n"
                             " \n"
                             "'Umurumda Değil' olarak ayarlanmışsa, 1x ölçeği \n"
                             "veya tam ekrana uzat ya da son geçiş \n"
                             "yapılmadığına bağlı olarak tam ekrana geçilir. \n"
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
            snprintf(s, len,
                     "Gölgelendirici Geçişleri. \n"
                             " \n"
                             "RetroArch, çeşitli gölgelendiricileri isteğe \n"
                             "bağlı gölgelendirici geçişleri ileözel donanım filtreleri \n"
                             "özel donanım filtreleri ve ölçek faktörleriyle karıştırmanıza ve eşleştirmenize olanak sağlar. \n"
                             " \n"
                             "Bu seçenek kullanılacak gölgelendirici geçiş \n"
                             "sayısını belirtir. Bunu 0'a ayarlarsanız ve Gölgelendirici Değişiklikleri Uygula'yı \n"
                             "kullanırsanız, 'boş' bir gölgelendirici kullanırsınız. \n"
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            snprintf(s, len,
                     "Gölgelendirici Parametreleri. \n"
                             " \n"
                             "Geçerli gölgelendiriciyi doğrudan değiştirir.\n"
                             "CGP/GLSLP ön ayar dosyasına kaydedilmeyecek");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            snprintf(s, len,
                     "Gölgelendirici Ön Ayar Parametreleri. \n"
                             " \n"
                             "Şu anda menüde gölgelendirici hazır ayarını değiştirir."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            snprintf(s, len,
                     "Gölgelendiricilere giden veri yolu. \n"
                             " \n"
                             "Tüm gölgelendiriciler aynı tipte \n"
                             "olmalıdır (yani CG, GLSL veya HLSL). \n"
                             " \n"
                             "Tarayıcının gölgelendiricileri aramaya \n"
                             "başlayacağı yeri ayarlamak için \n"
                             "Gölgelendirici Dizini'ni ayarlayın. "
            );
            break;
        case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
            snprintf(s, len,
                     "Yapılandırma dosyalarının nasıl yüklendiğini \n"
                             "ve önceliklendirildiğini belirler.");
            break;
        case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
            snprintf(s, len,
                     "Çıkışta Konfigürasyon dosyasını diske kaydeder.\n"
                             "Ayarlar değiştirilebildiği için menü \n"
                             "için kullanışlıdır. Yapılandırmanın üzerine yaz\n"
                             " \n"
                             "#include's ve yorumlar \n"
                             "korunmaz.  \n"
                             " \n"
                             "Tasarım gereği, yapılandırma  \n"
                             "dosyası kullanıcı tarafından  \n"
                             "muhafaza edildiği için değişmez \n"
                             "olarak kabul edilir ve kullanıcının \n"
                             "arkasına yazılmamalıdır."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
            "\nAncak, konsollarda durum böyle değildir,\n"
            " yapılandırma dosyasına \n"
            "el ile olarak bakmak \n"
            "gerçekten bir seçenek değildir."
#endif
            );
            break;
        case MENU_ENUM_LABEL_CONFIRM_ON_EXIT:
            snprintf(s, len, "Çıkmak istediğinden emin misin?");
            break;
        case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
            snprintf(s, len, "Gizli dosya ve \n"
                    "klasörleri göster.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
            snprintf(s, len,
                     "Bu geçiş için donanım filtresi. \n"
                             " \n"
                             "'Umurumda Değil' olarak ayarlanmışsa, \n"
                             "'Varsayılan Filtre' kullanılacaktır. "
            );
            break;
        case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
            snprintf(s, len,
                     "Geçici olmayan SRAM'yi düzenli \n"
                             "aralıklarla otomatik olarak kaydeder.\n"
                             " \n"
                             "Aksi belirtilmedikçe, bu varsayılan \n"
                             "olarak devre dışıdır. Aralık saniye cinsinden \n"
                             "ölçülür. \n"
                             " \n"
                             "0 değeri otomatik kaydetmeyi devre dışı bırakır.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
            snprintf(s, len,
                     "Giriş Cihazı Tipi. \n"
                             " \n"
                             "Hangi cihaz tipini kullanacağını seçer. Bu, \n"
                             "libretro core'unun kendisi ile ilgilidir. "
            );
            break;
        case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            snprintf(s, len,
                     "Libretro Core'u için günlük seviyesini ayarlar \n"
                             "(GET_LOG_INTERFACE). \n"
                             " \n"
                             " Libretro core'u tarafından \n"
                             " verilen günlük seviyesi libretro_log \n"
                             " seviyesinin altındaysa göz ardı edilir.\n"
                             " \n"
                             " Ayrıntılı modu etkinleştirilmedikçe, \n"
                             " DEBUG günlükleri her zaman dikkate alınmaz (--verbose).\n"
                             " \n"
                             " DEBUG = 0\n"
                             " INFO  = 1\n"
                             " WARN  = 2\n"
                             " ERROR = 3"
            );
            break;
        case MENU_ENUM_LABEL_STATE_SLOT_INCREASE:
        case MENU_ENUM_LABEL_STATE_SLOT_DECREASE:
            snprintf(s, len,
                     "Konum slotları.\n"
                             " \n"
                             " Yuva 0 olarak ayarlanmış durumdayken, kaydetme adı \n"
                             " * .state (ya da komut satırında tanımlanmışsa) olur. \n"
                             "Yuva! = 0 olduğunda, yol (yol) (d) olur;  \n"
                             "burada (d) yuva numarasıdır.");
            break;
        case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            snprintf(s, len,
                     "Gölgelendirici Değişikliklerini Uygular. \n"
                             " \n"
                             "Gölgelendirici ayarlarını değiştirdikten sonra, \n"
                             "değişiklikleri uygulamak için bunu kullanın. \n"
                             " \n"
                             "Gölgelendirici ayarlarının değiştirilmesi biraz pahalı bir işlemdir, \n"
                             "bu nedenle açıkça yapılması gerekir. \n"
                             " \n"
                             "Gölgelendirici uyguladığınızda, menü gölgelendirici \n"
                             "ayarları geçici bir dosyaya kaydedilir (menu.cgp veya menu.glslp) \n"
                             "ve yüklenir. RetroArch çıktıktan sonra işlem devam eder. \n"
                             "Dosya Gölgelendirici Dizinine kaydedilir."
            );
            break;
        case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
            snprintf(s, len,
                     "Gölgelendirici dosyalarını yeni değişiklikler için izleyin. \n"
                     " \n"
                     "Değişiklikleri diskteki bir gölgelendiriciye kaydettikten sonra, \n"
                     "otomatik olarak yeniden derlenir ve \n"
                     "çalışan içeriğe uygulanır."
            );
            break;
        case MENU_ENUM_LABEL_MENU_TOGGLE:
            snprintf(s, len,
                     "Menü Aç/Kapa");
            break;
        case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
            snprintf(s, len,
                     "Fare tutmayı değiştirir.\n"
                             " \n"
                             "Fare tutulduğunda, RetroArch fareyi gizler \n"
                             "ve göreceli fare girişinin daha iyi çalışmasını \n"
                             "sağlamak için fare işaretçisini pencerenin \n"
                             "içinde tutar.");
            break;
        case MENU_ENUM_LABEL_GAME_FOCUS_TOGGLE:
            snprintf(s, len,
                     "Oyun odağını değiştirir.\n"
                             " \n"
                             "Bir oyunun odağı olduğunda, RetroArch hem kısayol \n"
                             "tuşlarını devre dışı bırakacak hem de farenin imlecini pencerenin içinde tutacaktır.");
            break;
        case MENU_ENUM_LABEL_DISK_NEXT:
            snprintf(s, len,
                     "Disk görüntüleri arasında geçiş yapar.\n"
                             "Diski çıkardıktan sonra kullanın. \n"
                             " \n"
                             "Çıkarmayı tekrar değiştirerek tamamlayın.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
            snprintf(s, len,
                  "CPU tabanlı video filtresi.");
#else
            snprintf(s, len,
                     "CPU tabanlı video filtresi\n"
                             " \n"
                             "Dinamik bir kütüphaneye giden yol.");
#endif
            break;
        case MENU_ENUM_LABEL_AUDIO_DEVICE:
            snprintf(s, len,
                     "Ses sürücüsünün kullandığı varsayılan \n"
                             "ses cihazını geçersiz kılın.\n"
                             "Bu sürücüye bağlıdır. Örn.\n"
#ifdef HAVE_ALSA
            " \n"
            "ALSA için PCM cihazı gerek."
#endif
#ifdef HAVE_OSS
            " \n"
            "OSS bir yol ister (örneğin /dev/dsp)."
#endif
#ifdef HAVE_JACK
            " \n"
            "JACK port ismi istiyor (örneğin, system:playback1 \n"
            "system:playback_2)."
#endif
#ifdef HAVE_RSOUND
            " \n"
            "RSound, bir RSound sunucusuna IP adresi \n"
            "istiyor."
#endif
            );
            break;
        case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
            snprintf(s, len,
                     "Diskler için çıkarmayı değiştirir.\n"
                             " \n"
                             "Birden fazla disk içeriği için kullanılır.");
            break;
        case MENU_ENUM_LABEL_ENABLE_HOTKEY:
            snprintf(s, len,
                     "Diğer kısayol tuşlarını etkinleştirin.\n"
                             " \n"
                             " Bu kısayol tuşu klavyeye, joybutton veya joyaxis'e bağlıysa, \n"
                             "aynı anda aynı tuşa basılmadıkça \n"
                             "diğer tüm kısayol tuşları devre dışı bırakılır.\n"
                             " \n"
                             "Klavyenin geniş bir alanını sorgular \n"
                             "kısayol tuşlarının engellenmesinin istenmediği \n"
                             "RETRO_KEYBOARDmerkezli uygulamalar için \n"
                             "kullanışlıdır.");
            break;
        case MENU_ENUM_LABEL_REWIND_ENABLE:
            snprintf(s, len,
                     "Geri sarmayı etkinleştir.\n"
                             " \n"
                             "Performans düşmesi olacaktır, \n"
                             "bu nedenle varsayılan olarak devre dışıdır.");
            break;
        case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
            snprintf(s, len,
                     "Geçiş yaptıktan hemen sonra hileyi uygulayın.");
            break;
        case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
            snprintf(s, len,
                     "Oyun yüklendiğinde hileleri otomatik uygulayın.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
            snprintf(s, len,
                     "Çekirdek Dizini. \n"
                             " \n"
                             "Libretro Çekirdek uygulamalarının \n"
                             "aranacağı dizin. ");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            snprintf(s, len,
                     "Otomatik Yenileme Hızı\n"
                             " \n"
                             "Monitörünüzün doğru yenileme hızı (Hz).\n"
                             "Bu formül, ses giriş hızını hesaplamak \n"
                             "için kullanılır: \n"
                             " \n"
                             "audio_input_rate = oyun giriş hızı * \n"
                             "ekran yenileme hızı/oyun yenileme hızı \n"
                             " \n"
                             "Uygulamada herhangi bir değer rapor edilmezse, \n"
                             "uyumluluk için NTSC varsayılanları kabul edilir. \n"
                             " \n"
                             "Geniş görüntü değişikliklerinden kaçınmak için bu değer 60Hz'ye yakın olmalıdır. \n"
                             "Monitörünüz 60Hz'de çalışmıyorsa, VSync'yi devre dışı bırakın \n");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED:
            snprintf(s, len,
                     "Yoklamalı Yenileme Hızını Ayarla\n"
                             " \n"
                            "Yenileme hızını, ekran sürücüsünden\n"
                            "sorgulanan gerçek değere ayarlar.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ROTATION:
            snprintf(s, len,
                     "Ekranın belirli bir dönüşünü \n"
                             "zorlar.\n"
                             " \n"
                             "Dönme, libretro çekirdeğinin ayarladığı\n"
                             "dönüşlere eklenir (bkz. Video Döndürmeye\n"
                             "İzin Ver).");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE:
            snprintf(s, len,
                     "Tam ekran çözünürlüğü.\n"
                             " \n"
                             "0 çözünürlüğü, \n"
                             "ortamın çözünürlüğünü kullanır.\n");
            break;
        case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            snprintf(s, len,
                     "İleri Sarma Oranı.\n"
                             " \n"
                             "Hızlı ileri sarma kullanılırken\n"
                             "içeriğin çalıştırılacağı maksimum oran.\n"
                             " \n"
                             " (Örnek 60 fps içeriği için 5.0 => 300 fps \n"
                             "sınırı).\n"
                             " \n"
                             "RetroArch, maksimum hızın aşılmamasını sağlamak \n"
                             "için uyku moduna geçecektir.\n"
                             "Mükemmel bir şekilde doğru olması için \n"
                             "bu başlığa güvenmeyin.");
            break;
        case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
            snprintf(s, len,
                     "Tam İçerik Kare Hızıyla Eşitle.\n"
                             " \n"
                             "Bu seçenek hızlı ileriye sarmaya izin verirken x1\n"
                             "hızını zorlamanın eşdeğeridir.\n"
                             "No deviation from the core requested refresh rate,\n"
                             "no sound Dynamic Rate Control).");
            break;
        case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            snprintf(s, len,
                     "Tercih edilecek monitor.\n"
                             " \n"
                             "0 (varsayılan), belirli bir monitörün \n"
                             "tercih edilmediği anlamına gelir; 1 ve \n"
                             "üstü (1, ilk monitördür), RetroArch'ın \n"
                             "belirli monitörü kullanmasını önerir.");
            break;
        case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
            snprintf(s, len,
                     "Üst tarama çerçevelerinin kırpılmasını \n"
                             "zorlar.\n"
                             " \n"
                             "Bu seçeneğin tam davranışı core\n"
                             "uygulamaya özgüdür. ");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
            snprintf(s, len,
                     "Videoyu yalnızca tamsayı adımlarla \n"
                             "ölçekler.\n"
                             " \n"
                             "Temel boyut, sistem tarafından bildirilen \n"
                             "geometri ve en boy oranına bağlıdır.\n"
                             " \n"
                             "Zorlama Boyu ayarlanmadığında, X/Y \n"
                             "tamsayı bağımsız olarak ölçeklenir.");
            break;
        case MENU_ENUM_LABEL_AUDIO_VOLUME:
            snprintf(s, len,
                     "dB olarak ifade edilen ses seviyesi.\n"
                             " \n"
                             " 0 dB normal hacimdir. Arttırma uygulanmaz.\n"
                             "Gain can be controlled in runtime with Input\n"
                             "Volume Up / Input Volume Down.");
            break;
        case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            snprintf(s, len,
                     "Ses hızı kontrolü.\n"
                             " \n"
                             "Bunu 0'a ayarlamak hız kontrolünü devre dışı bırakır.\n"
                             "Any other value controls audio rate control \n"
                             "delta.\n"
                             " \n"
                             "Defines how much input rate can be adjusted \n"
                             "dynamically.\n"
                             " \n"
                             " Giriş hızı şöyle tanımlanır: \n"
                             " Giriş hızı * (1.0 +/- (rate control delta))");
            break;
        case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            snprintf(s, len,
                     "Maksimum ses çarpıklığı zamanlaması.\n"
                             " \n"
                             "Giriş hızındaki maksimum değişikliği tanımlar.\n"
                             "Yanlış zamanlamada, örneğin NTSC ekranlarda PAL Core'larını\n"
                             "çalıştırmak gibi, zamanlamada çok büyük \n"
                             "değişiklikler yapmak için bunu artırmak isteyebilirsiniz.\n"
                             " \n"
                             " Giriş hızı şöyle tanımlanır: \n"
                             " Giriş hızı * (1.0 +/- (maksimum zamanlama çarpıklığı))");
            break;
        case MENU_ENUM_LABEL_OVERLAY_NEXT:
            snprintf(s, len,
                     "Bir sonraki kaplamaya geçer.\n"
                             " \n"
                             "Tamamını sarar.");
            break;
        case MENU_ENUM_LABEL_LOG_VERBOSITY:
            snprintf(s, len,
                     "Enable or disable verbosity level \n"
                             "of frontend.");
            break;
        case MENU_ENUM_LABEL_VOLUME_UP:
            snprintf(s, len,
                     "Ses seviyesini arttırır.");
            break;
        case MENU_ENUM_LABEL_VOLUME_DOWN:
            snprintf(s, len,
                     "Ses seviyesini azaltır.");
            break;
        case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
            snprintf(s, len,
                     "Forcibly disable composition.\n"
                             "Only valid on Windows Vista/7 for now.");
            break;
        case MENU_ENUM_LABEL_PERFCNT_ENABLE:
            snprintf(s, len,
                     "Enable or disable frontend \n"
                             "performance counters.");
            break;
        case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
            snprintf(s, len,
                     "Sistem Dizini. \n"
                             " \n"
                             "Sets the 'system' directory.\n"
                             "Cores can query for this\n"
                             "directory to load BIOSes, \n"
                             "system-specific configs, etc.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
            snprintf(s, len,
                     "Automatically saves a savestate at the \n"
                             "end of RetroArch's lifetime.\n"
                             " \n"
                             "RetroArch will automatically load any savestate\n"
                             "with this path on startup if 'Auto Load State\n"
                             "is enabled.");
            break;
        case MENU_ENUM_LABEL_VIDEO_THREADED:
            snprintf(s, len,
                     "Threaded video sürücüsü kullanın.\n"
                             " \n"
                             "Bunu kullanmak, olası gecikme maliyetinde \n"
                             "ve daha fazla video kasılmasında performansı \n"
                             "artırabilir.");
            break;
        case MENU_ENUM_LABEL_VIDEO_VSYNC:
            snprintf(s, len,
                     "Video V-Sync.\n");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
            snprintf(s, len,
                     "CPU ve GPU'yu sabit senkronize \n"
                             "etmeye çalışır. \n"
                             " \n"
                             "Performans karşılığında gecikmeyi \n"
                             "azaltır.");
            break;
        case MENU_ENUM_LABEL_REWIND_GRANULARITY:
            snprintf(s, len,
                     "Rewind granularity.\n"
                             " \n"
                             " When rewinding defined number of \n"
                             "frames, you can rewind several frames \n"
                             "at a time, increasing the rewinding \n"
                             "speed.");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE:
            snprintf(s, len,
                     "Rewind buffer size (MB).\n"
                             " \n"
                             " The amount of memory in MB to reserve \n"
                             "for rewinding.  Increasing this value \n"
                             "increases the rewind history length.\n");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP:
            snprintf(s, len,
                     "Rewind buffer size step (MB).\n"
                             " \n"
                             " Each time you increase or decrease \n"
                             "the rewind buffer size value via this \n"
                             "UI it will change by this amount.\n");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT:
            snprintf(s, len,
                     "Ekran görüntüsü al.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            snprintf(s, len,
                     "Sets how many milliseconds to delay\n"
                             "after VSync before running the core.\n"
                             "\n"
                             "Can reduce latency at the cost of\n"
                             "higher risk of stuttering.\n"
                             " \n"
                             "Maximum is 15.");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
            snprintf(s, len,
                     "'GPU Hard Sync' kullanırken CPU'nun \n"
                             "kaç tane GPU önünde çalışabileceğini \n"
                             "ayarlar.\n"
                             " \n"
                             "Maksimum 3.\n"
                             " \n"
                             " 0: Hemen GPU'ya senkronize edilir.\n"
                             " 1: Önceki kareye senkronize eder.\n"
                             " 2: Etc ...");
            break;
        case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            snprintf(s, len,
                     "Çerçevelerin arasına siyah bir \n"
                             "çerçeve ekler.\n"
                             " \n"
                             "Useful for 120 Hz monitors who want to \n"
                             "play 60 Hz material with eliminated \n"
                             "ghosting.\n"
                             " \n"
                             "Video refresh rate should still be \n"
                             "configured as if it is a 60 Hz monitor \n"
                             "(divide refresh rate by 2).");
            break;
        case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
            snprintf(s, len,
                     "Menüde başlangıç ekranını göster.\n"
                             "İlk kez görüldüğünde otomatik olarak \n"
                             "false olarak ayarlanır.\n"
                             " \n"
                             "Bu, yalnızca 'Çıkışta Konfigürasyonu Kaydet'\n"
                             "etkinse config dosyasında güncellenir.\n");
            break;
        case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
            snprintf(s, len, "Toggles fullscreen.");
            break;
        case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
            snprintf(s, len,
                     "Block SRAM from being overwritten \n"
                             "when loading save states.\n"
                             " \n"
                             "Might potentially lead to buggy games.");
            break;
        case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
            snprintf(s, len,
                     "Pause gameplay when window focus \n"
                             "is lost.");
            break;
        case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
            snprintf(s, len,
                     "Screenshots output of GPU shaded \n"
                             "material if available.");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
            snprintf(s, len,
                     "Ekran görüntüsü Dizini. \n"
                             " \n"
                             "Ekran görüntülerinin bulunacağı dizin."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
            snprintf(s, len,
                     "VSync Swap Interval.\n"
                             " \n"
                             "Uses a custom swap interval for VSync. Set this \n"
                             "to effectively halve monitor refresh rate.");
            break;
        case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
            snprintf(s, len,
                     "Kayıt dosyaları Dizini. \n"
                             " \n"
                             "Save all save files (*.srm) to this \n"
                             "directory. This includes related files like \n"
                             ".bsv, .rt, .psrm, etc...\n"
                             " \n"
                             "This will be overridden by explicit command line\n"
                             "options.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
            snprintf(s, len,
                     "Konum kayıtları Dizini. \n"
                             " \n"
                             "Save all save states (*.state) to this \n"
                             "directory.\n"
                             " \n"
                             "This will be overridden by explicit command line\n"
                             "options.");
            break;
        case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
            snprintf(s, len,
                     "İçerikler Dizini. \n"
                             " \n"
                             " This location is queried by default when \n"
                             "menu interfaces try to look for loadable \n"
                             "assets, etc.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            snprintf(s, len,
                     "Dinamik Duvar Kağıtları Dizini. \n"
                             " \n"
                             " The place to store backgrounds that will \n"
                             "be loaded dynamically by the menu depending \n"
                             "on context.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
            snprintf(s, len,
                     "Slowmotion oranı."
                             " \n"
                             "When slowmotion, content will slow\n"
                             "down by factor.");
            break;
        case MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD:
            snprintf(s, len,
                     "Defines the axis threshold.\n"
                             " \n"
                             "How far an axis must be tilted to result\n"
                             "in a button press.\n"
                             " Possible values are [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
            snprintf(s, len,
                     "Turbo period.\n"
                             " \n"
                             "Describes the period of which turbo-enabled\n"
                             "buttons toggle.\n"
                             " \n"
                             "Numbers are described in frames."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
            snprintf(s, len,
                     "Duty cycle.\n"
                             " \n"
                             "Turbo etkin bir sürenin ne kadar sürmesi\n"
                             "gerektiğini açıklar.\n"
                             " \n"
                             "Sayılar çerçevelerde açıklanmıştır."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE:
            snprintf(s, len, "Dokunma desteğini etkinleştirin.");
            break;
        case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
            snprintf(s, len, "Geri dokunma yerine önü kullanın.");
            break;
        case MENU_ENUM_LABEL_MOUSE_ENABLE:
            snprintf(s, len, "Menü içinde fare girişini etkinleştirin.");
            break;
        case MENU_ENUM_LABEL_POINTER_ENABLE:
            snprintf(s, len, "Menü içinde dokunmatik girişi etkinleştirin.");
            break;
        case MENU_ENUM_LABEL_MENU_WALLPAPER:
            snprintf(s, len, "Arka plan olarak ayarlamak için resmin yolu.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
            snprintf(s, len,
                     "Wrap-around to beginning and/or end \n"
                             "if boundary of list is reached \n"
                             "horizontally and/or vertically.");
            break;
        case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
            snprintf(s, len,
                     "If disabled, the game will keep \n"
                             "running in the background when we are in the \n"
                             "menu.");
            break;
        case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
            snprintf(s, len,
                     "Ekran koruyucuyu önler. Is a hint that \n"
                             "does not necessarily have to be \n"
                             "honored by the video driver.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MODE:
            snprintf(s, len,
                     "Netplay client mode for the current user. \n"
                             "Will be 'Server' mode if disabled.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
            snprintf(s, len,
                     "Netplay için kullanılacak gecikme karelerinin miktarı. \n"
                             " \n"
                             "Bu değerin arttırılması performansı \n"
                             "artıracak ancak daha fazla gecikme sağlayacaktır.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE:
            snprintf(s, len,
                     "Netplay oyunlarının kamuya duyurulup duyulmayacağı. \n"
                             " \n"
                             "False olarak ayarlanırsa, istemciler genel \n"
                             "lobiyi kullanmak yerine manuel olarak bağlanmalıdır.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            snprintf(s, len,
                     "İzleyici modunda netplay başlatılıp başlatılmayacağı. \n"
                             " \n"
                             "True olarak ayarlanırsa, netplay başlangıçta izleyici \n"
                             "modunda olacaktır. Daha sonra modu değiştirmek her \n"
                             "zaman mümkündür.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES:
            snprintf(s, len,
                     "Whether to allow connections in slave mode. \n"
                             " \n"
                             "Slave-mode clients require very little processing \n"
                             "power on either side, but will suffer \n"
                             "significantly from network latency.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
            snprintf(s, len,
                     "Whether to disallow connections not in slave mode. \n"
                             " \n"
                             "Not recommended except for very fast networks \n"
                             "with very weak machines. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
            snprintf(s, len,
                     "Whether to run netplay in a mode not requiring\n"
                             "save states. \n"
                             " \n"
                             "If set to true, a very fast network is required,\n"
                             "but no rewinding is performed, so there will be\n"
                             "no netplay jitter.\n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
            snprintf(s, len,
                     "The frequency in frames with which netplay \n"
                             "will verify that the host and client are in \n"
                             "sync. \n"
                             " \n"
                             "With most cores, this value will have no \n"
                             "visible effect and can be ignored. With \n"
                             "nondeterminstic cores, this value determines \n"
                             "how often the netplay peers will be brought \n"
                             "into sync. With buggy cores, setting this \n"
                             "to any non-zero value will cause severe \n"
                             "performance issues. Set to zero to perform \n"
                             "no checks. This value is only used on the \n"
                             "netplay host. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN:
            snprintf(s, len,
                     "The number of frames of input latency for \n"
                     "netplay to use to hide network latency. \n"
                     " \n"
                     "When in netplay, this option delays local \n"
                     "input, so that the frame being run is \n"
                     "closer to the frames being received from \n"
                     "the network. This reduces jitter and makes \n"
                     "netplay less CPU-intensive, but at the \n"
                     "price of noticeable input lag. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE:
            snprintf(s, len,
                     "The range of frames of input latency that \n"
                     "may be used by netplay to hide network \n"
                     "latency. \n"
                     "\n"
                     "If set, netplay will adjust the number of \n"
                     "frames of input latency dynamically to \n"
                     "balance CPU time, input latency and \n"
                     "network latency. This reduces jitter and \n"
                     "makes netplay less CPU-intensive, but at \n"
                     "the price of unpredictable input lag. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NAT_TRAVERSAL:
            snprintf(s, len,
                     "When hosting, attempt to listen for\n"
                             "connections from the public internet, using\n"
                             "UPnP or similar technologies to escape LANs. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER:
            snprintf(s, len,
                     "When hosting a netplay session, relay connection through a \n"
                             "man-in-the-middle server \n"
                             "to get around firewalls or NAT/UPnP issues. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
            snprintf(s, len,
                     "Specifies the man-in-the-middle server \n"
                             "to use for netplay. A server that is \n"
                             "located closer to you may have less latency. \n");
            break;
        case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            snprintf(s, len,
                     "Maximum amount of swapchain images. This \n"
                             "can tell the video driver to use a specific \n"
                             "video buffering mode. \n"
                             " \n"
                             "Single buffering - 1\n"
                             "Double buffering - 2\n"
                             "Triple buffering - 3\n"
                             " \n"
                             "Setting the right buffering mode can have \n"
                             "a big impact on latency.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SMOOTH:
            snprintf(s, len,
                     "Resmi bilinear filtreleme ile pürüzsüzleştirir. \n"
                             "Gölgelendiriciler kullanılıyorsa devre dışı bırakılmalıdır.");
            break;
        case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
            snprintf(s, len,
                     "Menü içindeki geçerli tarihi ve/veya saati gösterir.");
            break;
        case MENU_ENUM_LABEL_TIMEDATE_STYLE:
           snprintf(s, len,
              "İçinde geçerli tarih ve/veya saati gösterecek stil.");
           break;
        case MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE:
            snprintf(s, len,
                     "Menü içindeki geçerli pil seviyesini gösterir.");
            break;
        case MENU_ENUM_LABEL_CORE_ENABLE:
            snprintf(s, len,
                     "Menü içindeki geçerli Core'u gösterir.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            snprintf(s, len,
                     "Netplay'i ana bilgisayar (sunucu) modunda etkinleştirir.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            snprintf(s, len,
                     "Netplay'ü istemci modunda etkinleştirir.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            snprintf(s, len,
                     "Aktif bir Netplay bağlantısını keser.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
            snprintf(s, len,
                     "Yerel ağdaki Netplay ana bilgisayarlarını arayın ve bağlanın.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
            snprintf(s, len,
                     "Netplay ile ilgili ayar.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
            snprintf(s, len,
                     "Dynamically load a new background \n"
                             "depending on context.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
            snprintf(s, len,
                     "Libretro buildbotundaki çekirdek \n"
                             "güncelleyici dizininin URL'si.");
            break;
        case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
            snprintf(s, len,
                     "Libretro buildbot'taki içerikler \n"
                             "dizinindeki URL.");
            break;
        case MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE:
            snprintf(s, len,
                     "etkinse, giriş, geçerli \n"
                             "çekirdek için ayarlanan yeniden \n"
                             "birleştirilen atamalarla geçersiz kılınır.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
            snprintf(s, len,
                     "Kaplama Dizini. \n"
                             " \n"
                             "Kolay erişim için kaplamaların \n"
                             "tutulduğu bir dizini tanımlar.");
            break;
        case MENU_ENUM_LABEL_INPUT_MAX_USERS:
            snprintf(s, len,
                     "RetroArch tarafından desteklenen maksimum \n"
                             "kullanıcı sayısı.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
            snprintf(s, len,
                     "İndirdikten sonra, indirme işlemlerinin \n"
                             "içinde bulunduğu arşivleri otomatik \n"
                             "olarak çıkarır.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
            snprintf(s, len,
                     "Desteklenen uzantılar tarafından \n"
                             "gösterilen dosyaları filtrele.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
            snprintf(s, len,
                     "RetroArch çalıştıran kişinin kullanıcı adı. \n"
                             "Çevrimiçi oyunlar oynamak için kullanılacak.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
            snprintf(s, len,
                     "Ana bilgisayar IP adresinin bağlantı noktası. \n"
                             "Bir TCP veya UDP bağlantı noktası olabilir.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
            snprintf(s, len,
                     "Netplay sırasında kullanıcı için seyirci modunu \n"
                             "etkinleştirin veya devre dışı bırakın.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
            snprintf(s, len,
                     "Bağlanılacak ana bilgisayarın adresi.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PASSWORD:
            snprintf(s, len,
                     "Netplay ana bilgisayarına bağlanmak için şifre. \n"
                             "Yalnızca ana bilgisayar modunda kullanılır.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD:
            snprintf(s, len,
                     "The password for connecting to the netplay \n"
                             "host with only spectator privileges. Used \n"
                             "only in host mode.");
            break;
        case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
            snprintf(s, len,
                     "Stdin komut arayüzünü etkinleştirin.");
            break;
        case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
            snprintf(s, len,
                     "Kullanıcı Arabirimi yardımcı sürücüsünü başlat \n"
                             "boot sırasında (varsa).");
            break;
        case MENU_ENUM_LABEL_MENU_DRIVER:
            snprintf(s, len, "Kullanılacak Menü sürücüsü.");
            break;
        case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
            snprintf(s, len,
                     "Geçiş menüsüne Gamepad düğme kombinasyonu. \n"
                             " \n"
                             "0 - Boş \n"
                             "1 - L + R + Y + D-Pad Down \n"
                             "aynı anda basın. \n"
                             "2 - L3 + R3 aynı anda basın. \n"
                             "3 - Start + Select aynı anda basın.");
            break;
        case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
            snprintf(s, len, "Herhangi bir kullanıcının menüyü kontrol etmesine izin verir. \n"
                    " \n"
                    "Devre dışı bırakıldığında, sadece kullanıcı 1 menüyü kontrol edebilir.");
            break;
        case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
            snprintf(s, len,
                     "Otomatik giriş algılamayı etkinleştir.\n"
                             " \n"
                             "Joypad'leri, Tak ve Çalıştır \n"
                             "stilini otomatik olarak yapılandırmaya çalışır.");
            break;
        case MENU_ENUM_LABEL_CAMERA_ALLOW:
            snprintf(s, len,
                     "Kameranın Çekirdek tarafından erişimine izin ver \n"
                             "veya verme.");
            break;
        case MENU_ENUM_LABEL_LOCATION_ALLOW:
            snprintf(s, len,
                     "Konum servislerine izin ver veya verme \n"
                             "çekirdekler tarafından erişilir.");
            break;
        case MENU_ENUM_LABEL_TURBO:
            snprintf(s, len,
                     "Turbo enable.\n"
                             " \n"
                             "Holding the turbo while pressing another \n"
                             "button will let the button enter a turbo \n"
                             "mode where the button state is modulated \n"
                             "with a periodic signal. \n"
                             " \n"
                             "The modulation stops when the button \n"
                             "itself (not turbo button) is released.");
            break;
        case MENU_ENUM_LABEL_OSK_ENABLE:
            snprintf(s, len,
                     "Ekran klavyesini etkinleştir/devre dışı bırak.");
            break;
        case MENU_ENUM_LABEL_AUDIO_MUTE:
            snprintf(s, len,
                     "Sesi kapat/aç.");
            break;
        case MENU_ENUM_LABEL_REWIND:
            snprintf(s, len,
                     "Geri sarmak için düğmeyi basılı tutun.\n"
                             " \n"
                             "Geri sarma etkin olmalı.");
            break;
        case MENU_ENUM_LABEL_EXIT_EMULATOR:
            snprintf(s, len,
                     "RetroArch'tan temiz bir şekilde çıkmak için tuş."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
                            "\nKilling it in any hard way (SIGKILL, \n"
                            "etc) will terminate without saving\n"
                            "RAM, etc. On Unix-likes,\n"
                            "SIGINT/SIGTERM allows\n"
                            "a clean deinitialization."
#endif
            );
            break;
        case MENU_ENUM_LABEL_LOAD_STATE:
            snprintf(s, len,
                     "Konumu yükle.");
            break;
        case MENU_ENUM_LABEL_SAVE_STATE:
            snprintf(s, len,
                     "Konumu kaydet.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_GAME_WATCH:
            snprintf(s, len,
                     "Netplay toggle play/spectate mode.");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
            snprintf(s, len,
                     "Increment cheat index.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
            snprintf(s, len,
                     "Decrement cheat index.\n");
            break;
        case MENU_ENUM_LABEL_SHADER_PREV:
            snprintf(s, len,
                     "Applies previous shader in directory.");
            break;
        case MENU_ENUM_LABEL_SHADER_NEXT:
            snprintf(s, len,
                     "Dizindeki bir sonraki gölgelendiriciyi uygular.");
            break;
        case MENU_ENUM_LABEL_RESET:
            snprintf(s, len,
                     "İçeriği sıfırla.\n");
            break;
        case MENU_ENUM_LABEL_PAUSE_TOGGLE:
            snprintf(s, len,
                     "Toggle between paused and non-paused state.");
            break;
        case MENU_ENUM_LABEL_CHEAT_TOGGLE:
            snprintf(s, len,
                     "Toggle cheat index.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_IDX:
            snprintf(s, len,
                     "Index position in list.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION:
            snprintf(s, len,
                     "Address bitmask when Memory Search Size < 8-bit.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT:
            snprintf(s, len,
                     "The number of times the cheat will be applied.\nUse with the other two Iteration options to affect large areas of memory.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS:
            snprintf(s, len,
                     "Her “Yineleme Sayısı” ndan sonra Hafıza Adresi, bu sayı ile “Hafıza Arama Boyutu” ile artacaktır.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE:
            snprintf(s, len,
                     "Her 'İterasyon Sayısı' ndan sonra, Değer bu miktar kadar artacaktır.");
            break;
        case MENU_ENUM_LABEL_CHEAT_MATCH_IDX:
            snprintf(s, len,
                     "Görüntülenecek eşleşmeyi seçin.");
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            snprintf(s, len,
                     "Yeni hileler oluşturmak için belleği tarayın");
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_RESTART:
            snprintf(s, len,
                     "Sol/Sağ ile bit-boyut arasında geçiş yapın\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT:
            snprintf(s, len,
                     "Sol/Sağ ile değerleri değiştirinn");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_LT:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_GT:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EQ:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS:
            snprintf(s, len,
                     "Sol/Sağ ile değerleri değiştirin\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS:
            snprintf(s, len,
                     "Değeri değiştirmek için Sol/Sağ\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADD_MATCHES:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_VIEW_MATCHES:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_CREATE_OPTION:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_DELETE_OPTION:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADD_NEW_TOP:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_RELOAD_CHEATS:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADD_NEW_BOTTOM:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_DELETE_ALL:
            snprintf(s, len,
                     " ");
            break;
        case MENU_ENUM_LABEL_CHEAT_BIG_ENDIAN:
            snprintf(s, len,
                     "Büyük endia   : 258 = 0x0102\n"
                     "Küçük endia : 258 = 0x0201");
            break;
        case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
            snprintf(s, len,
                     "Hold for fast-forward. Releasing button \n"
                             "disables fast-forward.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_HOLD:
            snprintf(s, len,
                     "Slowmotion için basılı tutun");
            break;
        case MENU_ENUM_LABEL_FRAME_ADVANCE:
            snprintf(s, len,
                     "İçerik duraklatıldığında kare ilerlemesi.");
            break;
        case MENU_ENUM_LABEL_BSV_RECORD_TOGGLE:
            snprintf(s, len,
                     "Kayıt yapma arasında geçiş yapmak için");
            break;
        case MENU_ENUM_LABEL_L_X_PLUS:
        case MENU_ENUM_LABEL_L_X_MINUS:
        case MENU_ENUM_LABEL_L_Y_PLUS:
        case MENU_ENUM_LABEL_L_Y_MINUS:
        case MENU_ENUM_LABEL_R_X_PLUS:
        case MENU_ENUM_LABEL_R_X_MINUS:
        case MENU_ENUM_LABEL_R_Y_PLUS:
        case MENU_ENUM_LABEL_R_Y_MINUS:
            snprintf(s, len,
                     "Axis for analog stick (DualShock-esque).\n"
                             " \n"
                             "Bound as usual, however, if a real analog \n"
                             "axis is bound, it can be read as a true analog.\n"
                             " \n"
                             "Positive X axis is right. \n"
                             "Positive Y axis is down.");
            break;
        case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
            snprintf(s, len,
                     "RetroArch kendi başına hiçbir şey yapmaz. \n"
                            " \n"
                            "Bir şeyler yapması için programa \n"
                            "bir program yüklemeniz gerekir.. \n"
                            "\n"
                            "Böylelikle programa 'Libretro Çekirdek', \n"
                            "yada kısaca 'Çekirdek' diyoruz. \n"
                            " \n"
                            "Çekirdek yüklemek için 'Çekirdek Yükle' \n"
                            "kısmından bir tane seçin.\n"
                            " \n"
#ifdef HAVE_NETWORKING
                    "Çekirdekleri birkaç yolla elde edebilirsiniz: \n"
                    "* İndirmek için şöyle\n"
                    "'%s' -> '%s'.\n"
                    "* El ile yapmak için 'Core' klasörüne taşıyın\n"
                    "'%s'.",
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
                            "You can obtain cores by\n"
                            "manually moving them over to\n"
                            "'%s'.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
            snprintf(s, len,
                     "You can change the virtual gamepad overlay\n"
                             "by going to '%s' -> '%s'."
                             " \n"
                             "From there you can change the overlay,\n"
                             "change the size and opacity of the buttons, etc.\n"
                             " \n"
                             "NOTE: By default, virtual gamepad overlays are\n"
                             "hidden when in the menu.\n"
                             "If you'd like to change this behavior,\n"
                             "you can set '%s' to false.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE:
            snprintf(s, len,
                     "OSD için arka plan rengini etkinleştirir.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED:
            snprintf(s, len,
                     "OSD arka plan renginin kırmızı değerini ayarlar. Geçerli değerler 0 ile 255 arasındadır.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN:
            snprintf(s, len,
                     "OSD arka plan renginin yeşil değerini ayarlar. Geçerli değerler 0 ile 255 arasındadır.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE:
            snprintf(s, len,
                     "OSD arka plan renginin mavi değerini ayarlar. Geçerli değerler 0 ile 255 arasındadır.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY:
            snprintf(s, len,
                     "OSD arka plan renginin opaklığını ayarlar. Geçerli değerler 0,0 ile 1,0 arasındadır.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED:
            snprintf(s, len,
                     "OSD metin renginin kırmızı değerini ayarlar. Geçerli değerler 0 ile 255 arasındadır.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN:
            snprintf(s, len,
                     "OSD metin renginin yeşil değerini ayarlar. Geçerli değerler 0 ile 255 arasındadır.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE:
            snprintf(s, len,
                     "OSD metin renginin mavi değerini ayarlar. Geçerli değerler 0 ile 255 arasındadır.");
            break;
        case MENU_ENUM_LABEL_MIDI_DRIVER:
            snprintf(s, len,
                     "Kullanılacak MIDI sürücüsü.");
            break;
        case MENU_ENUM_LABEL_MIDI_INPUT:
            snprintf(s, len,
                     "Sets the input device (driver specific).\n"
                     "When set to \"Off\", MIDI input will be disabled.\n"
                     "Device name can also be typed in.");
            break;
        case MENU_ENUM_LABEL_MIDI_OUTPUT:
            snprintf(s, len,
                     "Çıkış cihazını ayarlar (sürücüye özel).\n"
                     "\"Off\"olarak ayarlandığında, MIDI çıkışı devre dışı bırakılır.\n"
                     "Cihaz adı da yazılabilir.\n"
                     " \n"
                     "When MIDI output is enabled and core and game/app support MIDI output,\n"
                     "some or all sounds (depends on game/app) will be generated by MIDI device.\n"
                     "In case of \"null\" MIDI driver this means that those sounds won't be audible.");
            break;
        case MENU_ENUM_LABEL_MIDI_VOLUME:
            snprintf(s, len,
                     "Çıkış cihazının ana ses seviyesini ayarlar.");
            break;
        default:
            if (string_is_empty(s))
                strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            return -1;
    }

    return 0;
}
#endif

#ifdef HAVE_MENU
static const char *menu_hash_to_str_tr_label_enum(enum msg_hash_enums msg)
{
   if (msg <= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_END &&
         msg >= MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN)
   {
      static char hotkey_lbl[128] = {0};
      unsigned idx = msg - MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN;
      snprintf(hotkey_lbl, sizeof(hotkey_lbl), "input_hotkey_binds_%d", idx);
      return hotkey_lbl;
   }

   switch (msg)
   {
#include "msg_hash_lbl.h"
      default:
#if 0
         RARCH_LOG("Unimplemented: [%d]\n", msg);
#endif
         break;
   }

   return "null";
}
#endif

const char *msg_hash_to_str_tr(enum msg_hash_enums msg) {
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_tr_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

    switch (msg) {
#include "msg_hash_tr.h"
        default:
#if 0
            RARCH_LOG("Unimplemented: [%d]\n", msg);
            {
               RARCH_LOG("[%d] : %s\n", msg - 1, msg_hash_to_str(((enum msg_hash_enums)(msg - 1))));
            }
#endif
            break;
    }

    return "null";
}

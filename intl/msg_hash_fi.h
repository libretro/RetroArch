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
   "Päävalikko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Asetukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Suosikit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Historia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_IMAGES_TAB,
   "Kuvat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Musiikki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Videot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Verkkopeli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Etsi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Tuo sisältöä"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Pikavalikko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Siirry nopeasti kaikkiin peliin liittyviin asetuksiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Lataa ydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Valitse käytettävä ydin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Lataa sisältöä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Valitse käynnistettävä sisältö."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Lataa levy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Lataa fyysinen medialevy. Valitse ensi ydin (Lataa ydin) mitä käytät levyn kanssa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Kopioi levy"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Kopioi fyysinen levy sisäiseen tallennustilaan. Se tallennetaan levykuvatiedostona."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Soittolistat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Skannattu tietokantaa vastaava sisältö ilmestyy tänne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Tuo sisältöä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Luo ja päivitä soittolistoja skannaamalla sisältöä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Näytä työpöytävalikko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Avaa perinteinen työpöytävalikko."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Poista kioskitila käytöstä (Uudelleenkäynnistys pakollinen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Näytä kaikki kokoonpanoon liittyvät asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Verkkopäivittäjä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Lataa lisäosia, komponentteja ja sisältöä RetroArchiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Verkkopeli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Liity tai isännöi verkkopelisessio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Asetukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Säädä ohjelman asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Tiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Näytä järjestelmän tiedot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Kokoonpanotiedosto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Hallitse ja luo kokoonpanotiedostoja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Ohje"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "Opi, kuinka ohjelma toimii."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Käynnistä RetroArch uudelleen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Käynnistä ohjelma uudelleen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Sulje RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Sulje ohjelma."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Lataa ydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Lataa ja asenna ydin verkkopäivittäjästä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Asenna tai palauta ydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Asenna tai palauta ydin latauskansiosta."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Käynnistä videoprosessori"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Käynnistä etä-RetroPad"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Aloituskansio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Lataukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Selaa arkistoa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Lataa arkisto"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Suosikit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Suosikeiksi lisätty sisältö ilmestyy tänne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Musiikki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "Viimeksi soitettu musiikki ilmestyy tänne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_IMAGES,
   "Kuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Viimeksi nähdyt kuvat ilmestyvät tänne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Videot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Viimeksi soitetut videot ilmestyvät tänne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Etsi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Selaa kaikkea tietokantaa vastaavaa sisältöä luokitellun hakuliittymän kautta."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Ydinlataaja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Päivitä asennetut ytimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Päivitä kaikki asennetut ytimet viimeisimpään saatavilla olevaan versioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Vaihda ytimet play kaupan versioihin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Korvaa kaikki vanhat ja manuaalisesti asennetut ytimet uusimmilla play kaupan versioilla, josta ne ovat saatavilla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Esikatselukuvien päivittäjä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Lataa kokonaisen esikatselukuva pakkauksen valitulle järjestelmälle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Soittolistan esikatselukuvien päivittäjä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Lataa esikatselukuvat valitun soittolistan kohteille."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Sisällön lataaja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Päivitä ytimien tietotiedostot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Päivitä resurssit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Päivitä ohjainprofiilit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Päivitä huijauskoodit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Päivitä tietokannat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Päivitä päällykset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Päivitä GLSL-varjostimet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Päivitä Cg-varjostimet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Päivitä Slang-varjostimet"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Ytimen tiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Näytä tietoa ohjelmasta/ytimestä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Levyn tiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Näytä tietoa sisälle asetetuista medialevyistä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Verkon tiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Näytä verkkoliittymä(t) ja niihin liittyvät IP-osoitteet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Järjestelmän tiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Näytä laitteen tiedot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Tietokannan hallinta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Näytä tietokannat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Kursorihallinta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Näytä edelliset haut."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Ytimen nimi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Ytimen tunniste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Järjestelmän nimi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Järjestelmän valmistaja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Luokat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Tekijä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Oikeudet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Lisenssi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Tuetut tiedostopäätteet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "Vaadittu grafiikka-API"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Laiteohjelmisto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING,
   "Puuttuvat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT,
   "Löytyvä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPTIONAL,
   "Valinnaiset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REQUIRED,
   "Vaaditaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Lukitse asennettu ydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Estää muutokset nykyiseen asennettuun ytimeen. Voi käyttää epätoivoivottujen päivitysten estämiseen, kun sisältö vaatii tietyn ydinversion (esim. Kolikkopelien ROM-setit)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Poista ydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Poista tämä ydin levyltä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Varmuuskopioi ydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Tekee arkistoidun varmuuskopion nykyisestä asennetusta ytimestä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Palauta varmuuskopio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Asentaa edellisen version ytimestä arkistoitujen varmuuskopioiden listalta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Poista varmuuskopio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Poistaa tiedoston arkistoitujen varmuuskopioiden listalta."
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Koontipäivä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Git-versio"
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Kääntäjä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Prosessorin malli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Prosessorin ominaisuudet"
   )
MSG_HASH( /* FIXME Colon should be handled in menu_display.c like the rest */
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Prosessorin arkkitehtuuri:"
   )
MSG_HASH( /* FIXME Colon should be handled in menu_display.c like the rest */
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Prosessorin ytimet:"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CPU_CORES,
   "Prosessorin ydinten määrä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Käyttöliittymän tunniste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Käyttöjärjestelmä"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "RetroRating-taso"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Virtalähde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Videokontekstin ajuri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Näytön leveys (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Näytön korkeus (mm)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Näytön DPI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "LibretroDB-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Päällystuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Komentoliittymätuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Verkkokomentoliittymätuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Verkko-ohjaintuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Cocoa-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "PNG (RPNG) -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "JPEG (RJPEG) -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "BMP (RBMP) -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "TGA (RTGA) -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "SDL 1.2 -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "SDL 2 -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Vulkan-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Metal-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "OpenGL-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "OpenGL ES -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Säikeistystuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "KMS/EGL-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "udev-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "OpenVG-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "EGL-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "X11-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Wayland-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "XVideo-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "ALSA-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "OSS-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "OpenAL-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "OpenSL-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "RSound-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "RoarAudio-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "JACK-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "PulseAudio-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "CoreAudio-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "CoreAudio V3 -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "DirectSound-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "WASAPI-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "XAudio2-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "zlib-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "7zip-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Dynaaminen kirjastotuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Dynaaminen libretro-kirjaston suorituksenaikainen lataus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Cg-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "GLSL-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "HLSL-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "SDL-kuvatuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "FFmpeg-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "mpv-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "CoreText-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "FreeType-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "STB TrueType -tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Verkkopelituki (Vertaisverkko)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PYTHON_SUPPORT,
   "Python-tuki (Skriptituki varjostimissa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Video4Linux2-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "libusb-tuki"
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Tietokannan valinta"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Nimi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DESCRIPTION,
   "Kuvaus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_GENRE,
   "Laji"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Julkaisija"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Kehittäjä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Alkuperä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Pelisarja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "TGDB-luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Famitsu Magazine -arvosana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Edge Magazine -arvostelu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Edge Magazine -arvosana"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Edge Magazine -julkaisu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Julkaisuvuosi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Julkaisukuukausi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "BBFC-luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "ESRB-luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "ELSPA-luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "PEGI-luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Lisälaitteisto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "CERO-luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Sarjanumero"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analogituki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Tärinätuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Yhteistyötuki"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Lataa kokoonpano"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Palauta oletusarvot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Palauttaa nykyisen kokoonpanon oletusarvoihin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Tallenna nykyinen kokoonpano"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Tallenna uusi kokoonpano"
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Valikon perusohjaimet"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Vieritä ylös"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Vieritä alas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Valitse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO,
   "Tiedot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Käynnistä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Avaa/sulje valikko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Lopeta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Näytä/piilota näppäimistö"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Ajurit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Muuta järjestelmän käyttämiä ajureita."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Muuta videon ulostuloasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Ääni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Muuta äänen ulostuloasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Syöte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Muuta ohjaimen, näppäimistön ja hiiren asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Viive"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Muuta videon, äänen ja syötteen viiveeseen liittyviä asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Ydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Muuta ytimen asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS,
   "Kokoonpano"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Muuta kokoonpanotiedostojen oletusasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Tallennus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Muuta tallennusasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Lokiin kirjaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Muuta lokiin kirjauksen asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Tiedostoselain"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Muuta tiedostoselaimen asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Kuvien rajoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Muuta takaisinkelauksen, pikakelauksen ja hidastuksen asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Nauhoitus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Muuta nauhoituksen asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Näyttöruutu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Muuta näytön päällystä ja näppäimistöpäällystä, ja näytöllä näkyvien ilmoitusten asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Käyttöliittymä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Muuta käyttöliittymän asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Tekoälypalvelu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Muuta tekoälypalvelun asetuksia (Käännös/Teksti puheeksi/Muut)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Esteettömyys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Muuta esteettömyyslukijan asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Virranhallinta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Muuta virranhallinnan asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Saavutukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Muuta saavutusten asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Verkko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Vaihda palvelimen ja verkon asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Soittolistat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Muuta soittolistan asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Käyttäjä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Muuta tiliä, käyttäjänimeä ja kielen asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Kansio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Muuta tiedostojen sijainnin oletuskansiot."
   )

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Syöte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Käytettävä syöteajuri. Jotkut videoajurit pakottavat eri syöteajurin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Ohjain"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Käytettävä ohjainajuri."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Käytettävä videoajuri."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Ääni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Käytettävä ääniajuri."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Ääninäytteenottaja"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Käytettävä ääninäytteenottaja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Kamera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Käytettävä kamera-ajuri."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Käytettävä Bluetooth-ajuri."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Käytettävä Wi-Fi-ajuri."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Sijainti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Käytettävä sijaintiajuri."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Valikko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Käytettävä valikkoajuri."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Nauhoitus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Käytettävä nauhoitusajuri."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Käytettävä MIDI-ajuri."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "CRT-resoluutionvaihto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Lähetä natiiveja, matalan resoluution signaaleja käytettäväksi CRT-näyttöjen kanssa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Ulostulo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Muuta videon ulostuloasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Koko näytön tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Muuta koko näytön tilan asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Ikkunoitu tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Muuta ikkunoidun tilan asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Skaalaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Muuta videon skaalausasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Synkronointi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Muuta videon synkronointiasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Estä näytönsäästäjä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Estä käyttöjärjestelmän näytönsäästäjän aktivoituminen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Säikeitetty video"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Parantaa suorituskykyä viiveen kustannuksella ja lisää videon hidastelua. Käytä vain, jos täyttä nopeutta ei saada toisin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Mustan ruudun lisäys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "Aseta musta kuva muiden kuvien väliin. Hyödyllinen joillekkin korkean virkistystaajuuden näytöille haamukuvien poistamiseksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Näytönohjain kuvakaappaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Kuvakaappaukset ottavat näytönohjaimen varjostaman kuvamateriaalin, jos mahdollista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Bilineaarinen suodatus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Lisää pieni sumennus kuvaan pehmentääksesi kovien pikselin reunoja. Tällä valinnalla on hyvin vähän vaikutusta suorituskykyyn."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Kuvan interpolointi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Määritä kuvien interpolointimenetelmä skaalatessa sisältöä sisäisen IPU:n kautta. 'Bicubic' tai 'Bilineaarinen' suositellaan käytettäessä suoritinkäyttöisiä videosuodattimia. Tällä valinnalla ei ole vaikutusta suorituskykyyn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Bilineaarinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Lähin naapuri"
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Automaattisen varjostimen viive"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Viivästyttää automaattisesti latautuvia varjostimia (ms). Pystyy kiertämään graafisia häiriöitä käytettäessä 'näyttötallennusohjelmaa'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Videosuodatin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Käytä suoritinkäyttöistä videosuodatinta. Saattaa aiheuttaa suuren suorituskyky kustannuksen. Jotkut videosuodattimet voivat toimia vain ytimillä, jotka käyttävät 32-bittisiä tai 16-bittisiä värejä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Poista videosuodatin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Poista käytöstä kaikki aktiiviset prosessori käyttöiset videosuodattimet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Ota koko näyttö käyttöön Android-laitteissa"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "CRT-resoluutionvaihto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Vain CRT-näytöille. Yrittää käyttää samaa resoluutiota ja virkistystaajuutta kuin ydin/peli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "CRT-superresoluutio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Vaihda natiivin ja ultraleveän superresoluution välillä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "X-Akselin keskitys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Vaihdä tätä asetusta, jos kuva ei ole keskitetty oikein näytöllä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Säädä porch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Selaa näitä asetuksia, säätääksesi porch asetuksia, muuttaaksesi kuvan kokoa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Käytä mukautettua virkistystaajuutta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Käytä tarvittaessa mukautettua kokoonpanotiedostossa määritettyä virkistystaajuutta."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Monitorin indeksi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Valitse mitä näyttöä käytetään."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Videon kierto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Pakottaa tietyn videon kierron. Kierto lisätään ytimen asettamaan kiertoon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Näytön suunta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Pakottaa tietyn näytön suunnan käyttöjärjestelmästä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "GPU-indeksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Pystysuuntainen virkistystaajuus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Näytön pystysuora päivitysnopeus. Käytetään sopivan äänen syöttönopeuden laskemiseen.\nTämä jätetään huomiotta, jos 'säikeitetty video' on käytössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Arvioitu näytön virkistystaajuus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Tarkka arvioitu näytön virkistystaajuus hertseinä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Aseta näytön raportoima virkistystaajuus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "Näyttöajurin raportoima virkistystaajuus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Pakota sRGB FBO:n poistaminen käytöstä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Poista sRGB FBO tuki käytöstä pakottavasti. Joillakin Intel OpenGL ajureilla windowsissa on video ongelmia sRGB FBO:ien kanssa. Tämän käyttöönotto saattaa kiertää ongelman."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Käynnistä koko näytön tilassa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Käynnistä kokoruututilassa. Voidaan muuttaa suorituksen aikana. Komentoriviargumentti voi ohittaa tämän."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Ikkunoitu koko näytön tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "Kokoruututilassa, käytä mieluummin kokoruutuikkunaa estääksesi näytön tilan vaihtaminen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Koko näytön leveys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Aseta mukautettu leveyskoko ei-ikkunoidulle koko näytön tilalle. Tyhjäksi jättäminen käyttää työpöydän resoluutiota."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Koko näytön korkeus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Aseta mukautettu korkeuskoko ei-ikkunoidulle koko näytön tilalle. Tyhjäksi jättäminen käyttää työpöydän resoluutiota."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Ikkunan skaalaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Aseta ikkunan koko suhteutettuna ytimen näkymän kokoon. Vaihtoehtoisesti ikkunan leveys ja korkeus voidaan asettaa alemmaksi kiinteälle ikkunan koolle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Ikkunan näkyvyys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Näytä ikkunan koristeet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Muista ikkunan sijainti ja koko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Muista ikkunan sijainti ja koko, tällä on etusija ikkunan skaalauksen yli."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Ikkunan leveys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Aseta mukautettu leveys ikkunalle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Ikkunan korkeus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Aseta mukautettu korkeus ikkunalle."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Skaalaa kokonaisluvuin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Skaalaa video kokonaisluku vaiheittain vain. Peruskoko riippuu järjestelmän raportoidusta geometriasta ja kuvasuhteesta. Jos 'pakota kuvasuhde' ei ole asetettu, X/Y on itsenäisesti kokonaisluvuin skaalattu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Kuvasuhde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Mukautettu kuvasuhde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Liukulukuarvo videon kuvasuhteeseen (leveys / korkeus), käytetään jos Kuvasuhde on 'Mukautettu kuvasuhde'."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Säilytä kuvasuhde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Säilytä 1:1 pikselin kuvasuhdetta skaalatessa sisältöä sisäisen IPU:n kautta. Jos tämä ei ole käytössä, kuvat venytetään koko näytön täyttämiseksi."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Mukautettu kuvasuhde (X-sijainti)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Näyttöportin X-akselin sijainnin määrittämiseen käytetty mukautettu porttipoikkeama.\nNäitä ei oteta huomioon, jos 'skaalaa kokonaisluvuin' on käytössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Mukautettu kuvasuhde (Y-sijainti)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Mukautettu näkymän siirtymä, jota käytetään näkymän X-akselin sijainnin määrittämiseen.\nNämä jätetään huomioimatta, jos 'skaalaa kokonaisluvuin' on käytössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Mukautettu kuvasuhde (Leveys)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Mukautettu ikkunan leveys mitä käytetään, jos Kuvasuhde on 'Mukautettu kuvasuhde'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Mukautettu kuvasuhde (Korkeus)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Mukautettu ikkunan korkeus mitä käytetään, jos Kuvasuhde on 'Mukautettu kuvasuhde'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Rajaa yliskannaus (uudelleenkäynnistys pakollinen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Leikkaa muutama pikseli kuvan reunojen ympäriltä, jotka tavallisesti kehittäjät jättäneet tyhjiksi, mutta joskus sisältävät myös roskipikseleitä."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Vertikaalinen synkronointi (VSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Synkronoi näytönohjaimen ulostulovideo näytön virkistystaajuuteen. Suositeltavaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "VSync vaihtoväli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Käytä mukautettua vaihtoväliä VSync:lle. Aseta tämä puolittaaksesi tehokkaasti näytön päivitysnopeuden."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Mukautuva VSync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "VSync on käytössä, kunnes suorituskyky laskee tavoitteen virkistysnopeuden alapuolelle. Voit minimoida jumiutumisen, kun suorituskyky laskee reaaliajan alapuolelle ja on energiatehokkaampi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Kuvan viive"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Vähentää viivettä korkeamman videon jumiutumisen mahdollisuudella. Lisää viiveen VSync:in jälkeen (ms)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Näytönohjaimen kova synkronointi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Kova-synkronoi suoritin ja näytönohjain. Vähentää viivettä suorituskyvyn kustannuksella."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Näytonohjaimen kova synkronoidut kuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Määritä kuinka monta kuvaa suoritin voi ajaa näytönohjaimen edellä, kun käytetään 'näytönohjaimen kova synkronointi'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Synkronoi sisällön tarkasti kuvantaajuuteen (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Ei poikkeamista ytimen ajoituksesta. Käytä muuttuvan virkistystaajuuden näytöille (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Ulostulo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Muuta äänen ulostuloasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Näytteenottaja"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Muuta äänen näytteenottajan asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Synkronointi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Muuta äänen synkronointiasetuksia."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Muuta MIDIn asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Mikseri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Muuta äänen mikserin asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Valikon äänet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Mykistä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Mykistä ääni."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Mykistä mikseri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Mykistä mikserin ääni."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Mykistä pikakelattaessa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Mykistä ääni automaattisesti, kun pikakelaus on käytössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Äänenvoimakkuuden lisäys (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Äänenvoimakkuus (desibeleinä). 0 dB on normaali voimakkuus, ja voimakkuutta ei lisätä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Mikserin äänenvoimakkuuden lisäys (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Globaalin äänimikserin voimakkuus (desibeleinä). 0 dB on normaali voimakkuus, ja voimakkuutta ei lisätä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "DSP-liitännäinen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Äänen DSP-liitännäinen, joka prosessoi äänen ennen kuin se lähetetään ajuriin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Poista DSP liitännäinen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Poista käytöstä aktiiviset audion DSP-liitännäiset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "WASAPI-eksklusiivinen tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Sallii WASAPI-ajurin ottaa eksklusiivisen hallinnan äänilaitteesta. Jos pois päältä, se sen sijaan käyttää jaettua tilaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "WASAPI liuku formaatti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Käytä WASAPI-ajurille sopivaa liuku formaattia, jos äänentoisto tukee sitä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "WASAPI jaetun puskurin pituus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Välipuskurin pituus (kuvissa), kun käytetään WASAPI-ajuria jaetussa tilassa."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Ääni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Ota äänen ulostulo käyttöön."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Laite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Ohita ääniajurin käyttämä oletusäänilaite. Tämä asetus riippuu ajurista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Äänen viive (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Haluttu ääniviive millisekunneissa. Ei saateta kunnioittaa, jos ääniajuri ei kykene tuomaan annettua viivettä."
   )

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Näytteenottajan laatu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Pienennä tätä arvoa suosiaksesi parempaa suorituskykyä / alhaisempaa viivettä, äänen laadun kustannuksella, tai nosta arvoa saadaksesi parempi äänenlaatu paremman suorituskyvyn / suuremman viiveen kustannuksella."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Ulostulotaajuus (Hz)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Äänen ulostulon näytteenottotaajuus."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Synkronointi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Synkronoi ääni. Suositellaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Suurin ajoituksen vääristymä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Suurin muutos äänen syöttötaajuudella. Tämän lisääminen mahdollistaa erittäin suuret muutokset ajoituksessa, epätarkan äänenvoimakkuuden kustannuksella (esim. PAL ytimet joita ajetaan NTSC näytöillä)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Dynaaminen äänen nopeuden säätö"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Auttaa tasoittamaan ajoituksen puutteet äänen ja videon synkronoinnissa. Huomaa, että jos pois päältä, oikea synkronointi on lähes mahdotonta saada."
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Syöte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Valitse syöttölaite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Ulostulo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Valitse ulostulolaite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_VOLUME,
   "Äänenvoimakkuus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Aseta ulostulon voimakkuus (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Toista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Aloittaa äänivirran toiston. Kun valmis, se poistaa nykyisen äänivirran muistista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Toista (Silmukoitu)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Aloittaa äänivirran toiston. Kun valmis, se silmukoi ja toistaa raidan uudelleen alusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Toista (Peräkkäinen)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Aloittaa äänivirran toiston. Kun valmis, se hyppää seuraavaan äänivirtaan peräkkäisessä järjestyksessä ja toistaa tämän toiminnon. Hyödyllinen albumintoistotilana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Pysäytä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Tämä lopettaa äänen toiston, mutta ei poista sitä muistista. Se voidaan aloittaa uudelleen valitsemalla 'toista'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Poista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Tämä pysäyttää äänivirran toiston ja poistaa sen muistista kokonaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_VOLUME,
   "Äänenvoimakkuus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Säädä äänivirran voimakkuutta."
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Mikseri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Toista samanaikaisia äänivirtoja jopa valikoissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Ota valintaääni käyttöön"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Ota peruutusääni käyttöön"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Ota ilmoitusääni käyttöön"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Ota taustamusiikki käyttöön"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Käyttäjien enimmäismäärä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Käyttäjien enimmäismäärä, mitä RetroArch tukee."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Pollauksen toiminta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Määritä kuinka syötteen pollaus toteutetaan RetroArchissa. Riippuen kokoonpanostasi valinnat \"aikaisin\" ja \"myöhään\" voivat johtaa pienempään viiveeseen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Uudelleenmääritä tämän ytimen ohjaimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Ohita syötteen sidokset nykyiselle ytimelle asetettujen uusittujen sidosten kanssa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Automaattinen kokoonpano"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Automaattisesti määrittää ohjaimet, joilla on profiili, Plug-and-Play-tyyliin."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Poista windows pikanäppäimet käytöstä (uudelleenkäynnistys vaaditaan)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Pidä windows näppäin yhdistelmät sovelluksen sisällä."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Apuanturin syöte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Salli syöte, kiihtyvyysmittarista, gryskoopista ja valaistuantureista, jos nykyinen laitteisto niitä tukee. Saattaa vaikuttaa suorituskykyyn ja/tai lisätä virrankulutusta joillakin alustoilla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Ota 'pelin tarkennus' tila käyttöön automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Ota aina käyttöön 'Pelin tarkennus' tila kun käynnistät ja jatkat sisältöä. Kun asetuksena on 'tunnista', tila on käytössä, jos nykyinen ydin toteuttaa käyttöliittymän näppäimistön takaisinkutsutoiminnon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "Pois käytöstä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "PÄÄLLÄ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Tunnista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Painikkeen akselin kynnys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "Kuinka pitkälle akselin täytyy kallistua, että se tunnistetaan painikkeen painalluksena."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Analogin katvealue"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Analogin herkkyys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Määrityksen aikakatkaisu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Odotettavien sekuntien määrä ennen seuraavaan määritykseen siirtymistä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Määrityksen pohjassapitoaika"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Syötteen pohjassapidon sekuntien määrä, jolloin se määritetään."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Turbon jakso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Ajanjakso (kuvissa) kun turbo yhteensopivia painikkeita painetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Turbo tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Valitse turbotilan yleinen käyttäytyminen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DEFAULT_BUTTON,
   "Turbon oletuspainike"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DEFAULT_BUTTON,
   "Oletusarvoinen aktivointi painike turbo tilalle käyttäessä tilaa 'yksi painike'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Haptinen palaute/värinä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Muuta haptisen palautteen ja värinän asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Valikon ohjaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Muuta valikon ohjainten asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Pikanäppäimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Muuta pikanäppäimien asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Portin %u kontrollit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Muuta tämän portin ohjauksia."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Värinä painalluksesta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Ota laitteen värinä käyttöön (Tuetuille ytimille)"
   )
#if defined(DINGUX) && defined(HAVE_LIBSHAKE)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DINGUX_RUMBLE_GAIN,
   "Tärinän voimakkuus (uudelleenkäynnistys vaaditaan)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DINGUX_RUMBLE_GAIN,
   "Määritä haptisen palautteen voimakkuus."
   )
#endif

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Yhtenäiset valikon ohjaimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Käytä samoja kontrolleja sekä valikossa että pelissä. Koskee näppäimistöä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Vaihda valikon OK- ja Peruuta-painikkeiden sijaintia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Vaihda painikkeita OK/Peruuta. Poistettu käytöstä on japanilaisten painikkeiden orientaatio, käytössä on länsimainen orientaatio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Kaikki käyttäjät voivat ohjata valikkoa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Salli kaikkien käyttäjien hallita valikkoa. Jos poistettu käytöstä, vain käyttäjä 1 voi hallita valikkoa."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Vahvista lopetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "Vaadi lopeta pikanäppäintä painettavan kahdesti lopettaaksesi RetroArchin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Vaiha valikon ohjain yhdistelmä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Ohjaimen nappiyhdistelmä valikon vaihtamiseksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Pikanäppäimen käyttöönoton viive (kuvissa)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Lisää viive jonka jälkeen normaali syöte on estetään, kun on painettu (ja pidetty) määritettyä \"pikanäppäimen käyttöönotto\" näppäintä. Sallii tavallisen syötteen 'pikanäppäin käyttöönotto' näppäimestä, kun se on yhdistetty toiseen toimintoon (esim. RetroPad \"Select\")."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Pikakelaus eteenpäin (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Vaihtaa pikakelauksen ja normaalin nopeuden välillä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Pikakelaus eteenpäin (pidä pohjassa)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Ottaa pikakelauksen käyttöön pohjallapidettäessä. Sisältö pyörii normaalilla nopeudella, kun näppäin vapautetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Hidastus (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Vaihtaa hidastuksen ja normaalin nopeuden välillä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Hidastus (pidä pohjassa)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Ottaa hidastuksen käyttöön pohjallapidettäessä. Sisältö pyörii normaalilla nopeudella, kun näppäin vapautetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Lataa pelitila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Lataa tallenetun pelitilan valitusta lohkosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Tallenna pelitila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Tallentaa pelitilan valittuna olevaan lohkoon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Koko näyttö (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Vaihtaa koko näytön tilan ja ikkunoidun tilan välillä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Sulje sisältö"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Sulkee nykyisen sisällön. Kaikki tallentamattomat muutokset saattavat kadota."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Lopeta RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Sulkee RetroArchin, varmistaen, että kaikki tallennetut tiedot ja kokoonpanotiedostot tallennetaan levylle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Pelitilan lohko +"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Lisää valittua pelitila tallenuksen indeksiä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Pelitilanteen lohko -"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Vähentää valittua pelitila tallenuksen indeksiä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Takaisinkelaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "Kelaa nykyistä sisältöä taaksepäin, kun näppäinä painetaan.\n'Takaisinkelauksen tuki' täytyy olla käytössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
   "Nauhoita syötteiden toistoa (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_BSV_RECORD_TOGGLE,
   "Vaihtaa pelin näppäinkomentojen nauhoituksen .bsv formaatilla päälle/pois."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Tauko (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Vaihtaa käynnissä olevan sisällön taukotilan päälle tai pois."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Seuraava kuva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Kun sisältö on keskeytetty, se etenee yhdellä kuvalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Nollaa peli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Käynnistää nykyisen sisällön sen alkutilasta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Seuraava varjostin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Lataa ja käytä seuraavaa varjostimen esiasetettua tiedostoa 'video varjostimet' kansion juuressa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Edellinen varjostin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Lataa ja käyttää edellistä varjostimen esiasetetta tiedostoa 'videovarjostimet' kansion juuressa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Seuraava huijauskoodin indeksi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Lisää valittua huijauskoodin indeksiä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Edellinen huijauskoodin indeksi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Vähennä valitun huijauskoodin indeksiä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Huijauskoodit (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Ottaa tai poistaa käytöstä valittuna olevan huijauksen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Ota kuvakaappaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Ottaa kuvan nykyisestä sisällöstä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Äänen mykistys (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Vaihtaa äänitulon päälle tai pois."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Näyttönäppäimistö (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Ottaa käyttöön tai poistaa käytöstä näyttönäppäimistön."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Näytä ruudunpäivitysnopeus (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Vaihtaa 'kuvaa sekunnissa' -ilmaisimen päälle tai pois."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "Lähetä vianjäljitystietoa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SEND_DEBUG_INFO,
   "Lähettää diagnoositiedot laitteestasi ja RetroArch-kokoonpanostasi palvelimillemme analysoitavaksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Verkkopelin isännöinti (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Vaihtaa verkkopelin isännöinnin tilaa päälle tai pois."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Verkkopelin peli-/seuraajatilan välillä vaihtelu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Vaihtaa nykyisen verkkopeli istunnon 'pelaa' ja 'katsele' -tilaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Pikanäppäimen käyttöönotto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Äänenvoimakkuus suuremmalle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Lisää äänenvoimakkuuden tasoa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Äänenvoimakkuus pienemmälle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Pienentää äänenvoimakkuuden tasoa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Seuraava päällys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Vaihtaa seuraavaan saatavilla olevaan aktiivisen näyttö päällyksen asetelmaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Syötä / Poista levy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Jos virtuaalinen levyasema on suljettu, avaa sen ja poistaa ladatun levyn. Muussa tapauksessa lisää valitun levyn ja sulkee aseman."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Seuraava levy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Nostaa valittua levy indeksiä.\nVirtuaalisen levyaseman on oltava auki."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Edellinen levy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Laskee valittua levy indeksiä.\nVirtuaalisen levyaseman on oltava auki."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Kaappaa hiiri (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Kaappaa tai vapauta hiiri. Kun tarrataan, järjestelmän kohdistin on piilotettu ja rajoitettu RetroArchin näyttöikkunaan, mikä parantaa suhteellista hiiren syöttöä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Pelin tarkennus (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Ottaa käyttöön tai poistaa käytöstä 'Pelin tarkennus' tilan. Kun sisältö on tarkennettu, pikanäppäimet ovat pois päältä (koko näppäimistön syöttö siirtyy käynnissä olevaan ytimeen) ja hiiri on kaapattu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Työpöytävalikko (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Avaa WIMP (Windows, Icons, Menus, Pointer) työpöydän käyttöliittymän."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Valikko (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Vaihtaa nykyisen näytön valikon ja sisällön välillä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Nauhoitus (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Käynnistä/pysäytä nykyisen istunnon nauhoitus paikalliseen videotiedostoon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Suoratoisto (päälle/pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Aloittaa/pysäyttää nykyisen istunnon suoratoiston netin videoalustalle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Edelläpyöritys (Päälle/Pois)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Vaihtaa edelläpyärityksen päälle/pois."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Tekoälypalvelu"
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Laitetyyppi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Analoginen digitaaliseen tyyppiin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Laitteen indeksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Aseta kaikki ohjaimet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Palauta oletuskontrollit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Tallenna ohjainprofiili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Hiiren indeksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "B-painike (Alas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Y-painike (Vasen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Select-painike"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Start-painike"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "Ristiohjain ylös"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "Ristiohjain alas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "Ristiohjain vasen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "Ristiohjain oikea"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "A-painike (Oikea)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "X-painike (Ylös)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "L-painike (Olka)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "R-painike (Olka)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "L2-painike (Liipaisin)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "R2-painike (Liipaisin)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "L3-painike (Peukalo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "R3-painike (Peukalo)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Vasen analogi X+ (Oikea)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Vasen analogi X- (Vasen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Vasen analogi Y+ (Alas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Vasen analogi Y- (Ylös)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Oikea analogi X+ (Oikea)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Oikea analogi X- (Vasen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Oikea analogi Y+ (Alas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Oikea analogi Y- (Ylös)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Pyssy: Liipaisin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Pyssy: Lataus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Pyssy: Aux A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Pyssy: Aux B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Pyssy: Aux C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Pyssy: Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Pyssy: Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Pyssy: Ristiohjain ylös"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Pyssy: Ristiohjain alas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Pyssy: Ristiohjain vasen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Pyssy: Ristiohjain oikea"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
   "Edelläpyöritys viiveen vähentämiseen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
   "Suorita ydin logiikkaa yksi tai useampi kuva edellä ja lataa sitten tilan takaisin vähentäen koettua viivettä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Edellä olevien kuvien lukumäärä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
   "Käytä toista instanssia edelläpyörityksessä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
   "Käytä RetroArchin ytimen toista instanssia edelläpyöritykseen. Estää pelitila latauksen aiheuttamat audio ongelmat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Piilota edelläpyöritys varoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Piilota varoitusviesti, joka ilmestyy kun edelläpyöritys ja ydin eivät tue pelitilanteiden tallentamista."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Laitteiston jaettu lonteksti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Salli ydinten vaihtaa videoajuria"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Sallii ytimien vaihtaa toiseen video ajuriin kuin tällä hetkellä on ladattu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Lataa tyhjä ydin ytimen sammutuksessa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Joissakin ytimissä on sammutustoiminto, tyhjän ytimen lataus estää RetroArchin sammutuksen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Käynnistä ydin automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Tarkista puuttuva laiteohjelmisto ennen lataamista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Tarkista, onko kaikki tarvittava laiteohjelmisto läsnä ennen kuin yrität ladata sisältöä."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Lataa ydin aina uudelleen kun sisältö suoritetaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Käynnistä RetroArch uudelleen käynnistettäessä sisältöä, vaikka haluttu ydin on jo ladattu. Tämä voi parantaa järjestelmän vakautta pidentämällä latausaikoja."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Salli kierto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Salli ytimien asettaa kierto. Kun pois päältä, kierto pyynnöt ohitetaan. Hyödyllinen asetuksille, jotka kiertävät näytön manuaalisesti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Hallitse ytimiä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Suorita ilman yhteyttä ylläpitotehtäviä asennetuissa ytimissä (varmuuskopiointi, palautus, poistaminen, jne.) ja tarkastele ydintietoja."
   )

/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Tallenna kokoonpano lopettaessa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Tallenna muutokset asetustiedostoon suljettaessa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Lataa sisältökohtaiset ydinasetukset automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Lataa mukautetut ydinasetukset oletuksena käynnistyksessä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Lataa ohitus tiedostot automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Lataa mukautettu konfiguraatio käynnistyksessä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Lataa uudelleenmääritystiedosto automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Lataa mukautetut kontrollit käynnistyksessä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Lataa varjostimen esiasetukset automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Käytä globaalia ytimen asetustiedostoa"
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Lajittele tallennukset kansioihin ytimen nimen mukaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Lajittele tallennustiedostot kansioihin, jotka on nimetty käytetyn ytimen mukaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Lajittele pelitila tallennukset kansioihin ytimen nimen mukaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Lajittele pelitila tallennukset kansioihin, jotka on nimetty käytetyn ytimen mukaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Lajittele tallennukset kansioihin sisällön hakemiston mukaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Lajittele tallennustiedostot kansioihin, jotka on nimetty kansion mukaan, jossa sisältö sijaitsee."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Lajittele pelitila tallennukset kansioihin sisällön kansion mukaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Lajittele pelitila tallennukset kansioihin, jotka on nimetty kansion mukaan, jossa sisältö sijaitsee."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Älä ylikirjoita SaveRAM ladattaessa pelitilaa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Estää SaveRAM ylikirjoituksen ladattaessa pelitilaa. Sataa mahdollisesti aiheuttaa viallisia pelejä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "SaveRAM automaattitallennuksen aikaväli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Automaattisesti tallenna haihtumaton SaveRAM säännöllisin väliajoin (sekunneissa)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Nosta pelitila tallennuksen indeksiä automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Ennen pelitila tallennuksen luomista, pelitila tallennuksen indeksiä nostetaan. Sisällön lataamisen yhteydessä indeksi asetetaan korkeimpaan olemassa olevaan indeksiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Maksimi pelitila tallennusten automaattinen määrä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Rajoita pelitila tallennuksien määrää, jotka luodaan automaattisesti, kun 'nosta pelitila tallennuksen indeksiä automaattisesti' on käytössä. Jos raja ylittyy, kun uutta tilaa tallennetaan, poistetaan olemassa oleva pelitila tallennus, jossa on alin indeksi. Arvo ’0’ tarkoittaa sitä, että pelitila tallennuksia tehdään rajattomasti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Pelitilan automaattinen tallennus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Luo pelitila tallennus automaattisesti kun suljet sisällön. RetroArch lataa automaattisesti tämän pelitila tallennuksen, jos 'lataa pelitila tallennus automaattisesti' on käytössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Lataa pelitila tallennus automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Automaattisesti lataa pelitila tallennuksen käynnistyksen yhteydessä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Pelitila tallenteiden esikatselukuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Näytä pelitila tallennusten esikatselukuvat valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "SaveRAM pakkaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Tallenna haihtumattomat SaveRAM tiedostot arkistoidussa tiedosto muodossa. Pienentää huomattavasti tiedoston kokoa, lisäten (hiukan) tallennus/lataus aikoja.\nKoskee vain ytimiä, jotka mahdollistavat tallentamisen standardi libretro SaveRAM käyttölittymän kautta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Pelitila tallenteiden pakkaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Tallenna pelitila tallennukset arkistoidussa tiedosto muodossa. Pienentää tiedoston kokoa dramaattisesti lisääntyneiden tallennus ja lataus aikojen kustannuksella."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Lajittele kuvakaappaukset kansioihin sisällön kansion mukaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Lajittele kuvakaappaukset kansioihin, jotka on nimetty kansion mukaan, jossa sisältö sijaitsee."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Tallenna tallennukset sisällön kansioon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Tallenna pelitila tallennukset sisällön kansioon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Järjestelmätiedostot ovat sisällön kansiossa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Tallenna kuvakaappaukset sisällön kansioon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Tallenna ajoaika lokit (per ydin)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Pidä kirjaa siitä, kuinka kauan kukin sisältö on ajanut, jolloin kirjaukset on ydinkohtaiset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Tallenna ajoaika lokit (Kooste)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Pidä kirjaa siitä, kuinka kauan kukin sisältö on ajanut, jolloin kirjaukset on ytimien yhteenlaskettu ajoaika."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Runsas lokitus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Lokita tapahtumat päätteeseen tai tiedostoon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Käyttöliittymän lokitustaso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Aseta lokitustaso käyttöliittymälle. Jos käyttöliittymän antama lokitustaso on tämän arvon alapuolella, se ohitetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Ytimen lokitustaso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Aseta ytimien lokitustaso. Jos ytimen antama lokitas on tämän arvon alapuolella, se ohitetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Lokita tiedostoon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Ohjaa järjestelmän tapahtumalokiviestit tiedostoon. Vaatii 'runsas lokitus' otettavaksi käyttöön."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Aikaleimaa lokitiedostot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Kun kirjataan lokia tiedostoon, ohjaa ulostulo jokaisesta RetroArch istunnosta uuteen aikaleimattuun tiedostoon. Jos poistettu käytöstä, loki ylikirjoitetaan joka kerta, kun RetroArch käynnistetään uudelleen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Suorituskyvyn laskurit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Suorituskyky laskurit RetroArchille ja ytimille. Laskuri tiedot voivat auttaa määrittämään järjestelmän pullonkaulat ja kohdistamaan suorituskykyä."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Näytä piilotetut tiedostot ja kansiot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Näytä piilotetut tiedostot ja kansiot tiedostoselaimessa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Suodata tuntemattomat tiedostopäätteet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Suodata tiedostoselaimessa näytettävät tiedostot tuetuilla laajennuksilla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Käytä sisäänrakennettua mediasoitinta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Suodata nykyisen ytimen mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Muista viimeksi käytetty aloituskansio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Avaa tiedostoselain viimeksi käytettyyn paikkaan, kun lataat sisältöä aloituskansiosta. Huomautus: Sijainti nollaantuu oletusarvoon uudelleenkäynnistyksen yhteydessä."
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Takaisinkelaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Muuta takaisin kelauksen asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Kuva-aikalaskuri"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Muuta kuva-aikalaskuriin vaikuttavia asetuksia.\nKäytössä vain kun säikeitetty video on pois käytöstä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Eteenpäin pikakelauksen tahti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Enimmäisnopeus, jolla sisältö ajetaan kun käytetään pikakelausta (esim. 5. x 60 fps sisältö = 300 fps yläraja). Jos asetettu 0,0x, nopeussuhde on rajoittamaton (ei FPS ylärajaa)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Hidastuksen tahti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "Nopeus jolla sisältö pyörii kun käytetään hidastusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Korota valikon kuvataajuutta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "Varmistaa että valikon kuvataajuudella ei ole ylärajaa."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Takaisinkelauksen tuki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Palaa edelliseen pisteeseen viimeaikaisessa pelissä. Vaikuttaa vakavasti suorituskykyyn pelatessa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Takaisin kelaa kuvia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Kelattavien kuvien määrä askelta kohden. Suuremmat arvot lisäävät kelausnopeutta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Takaisinkelauksen puskurin koko (Mt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "Muistin määrä (MB) joka varataan takaisinkelauksen puskurille. Tämän lisääminen lisää takaisinkelauksen historian määrää."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Takaisinkelauksen puskurin koko askelittain (Mt)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Joka kerta, kun palautuspuskurin kokoa nostetaan tai lasketaan, se muuttuu tällä määrällä."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Nollaa pikakelauksen jälkeen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Nollaa kuva-aikalaskurin pikakelauksen jälkeen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Nollaa pelitila latauksen jälkeen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Nollaa kuva-aikalaskurin pelitila latauksen jälkeen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Nollaa pelitila tallennuksen jälkeen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Nollaa kuva-aikalaskurin pelitila tallennuksen jälkeen."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Nauhoituslaatu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Mukautettu nauhoituksen kokoonpano"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Nauhoituksen säikeet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Käytä suodattimen jälkeistä nauhoitusta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Kaappaa kuva suodattimien (mutta ei varjostimien) jälkeen. Video näyttää yhtä hienolta kuin mitä näet näytöllä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Käytä GPU-nauhoitusta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Nauhoita näytönohjaimen varjostama materiaali, jos saatavilla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Suoratoisto tila"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Suoratoistamisen laatu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Mukautettu suoratoiston kokoonpano"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Suoratoiston otsikko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "Suoratoiston verkko-osoite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "UDP-striimauksen portti"
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Näyttöpäällys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Säädä kehyksiä ja näyttö ohjaimia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Videon asettelu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Säädä videon asettelua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Näyttöilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Säädä näyttöilmoituksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Ilmoitusten näkyvyys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Vaihtele tietyntyyppisten ilmoitusten näkyvyyttä."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Näytä päällys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Päällyksiä käytetään rajoina ja näyttö ohjaimiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Piilota päällys valikossa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Piilota päällys valikon sisällä ja näytä se uudelleen valikosta poistuessa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Piilota päällys kun ohjain on yhdistetty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Piilota päällys, kun fyysinen ohjain on yhdistetty porttiin 1, ja näytä se uudelleen, kun ohjain on irrotettu."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Piilota päällys, kun fyysinen ohjain on yhdistetty porttiin 1. Päällystä ei palauteta automaattisesti, kun ohjain irroitetaan."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
   "Näytä syötteet päällyksessä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS,
   "Näytö näppäimistön/ohjaimen syötteet näyttö peitteessä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
   "Näytä syötteiden kuuntelu portti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT,
   "Valitse portti, jota päällys kuuntelee, jos 'näytä syötteet päällyksessä' on käytössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Näytä hiiren kursori päällysteillä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Näytä hiiren kohdistin, kun käytät näytön päällystä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Automaattisesti kierrä päällystä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Jos nykyinen päällys sitä tukee, automaattisesti kiertää asetelmaa vastaamaan näytön kuvasuhdetta/asetusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Automaattisesti skaalaa päällystä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Automaattisesti säätää päällyksen skaalausta ja käyttöliittymä elementtien välitystä vastaamaan kuvasuhdetta. Tuottaa parhaat tulokset ohjain päällysten kanssa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Päällys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Lataa ensisijainen päällys automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Päällyksen läpinäkyvyys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Kaikkien käyttöliittymän päällys elementtin läpinäkyvyys."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Päällyksen esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Valitse päällys tiedostoselaimesta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(Vaakakuva) Päällyksen skaala"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Skaalaa kaikki käyttöliittymän päällys elementit kun käytetään vaakasuuntaista näyttö asetusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(Vaakakuva) Päällyksen kuvasuhteen säätö"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Käytä kuvasuhteen korjauskerrointa peitteeseen, kun käytetään vaakakuva näyttö asetusta. Positiiviset arvot kasvattavat (kun negatiiviset arvot pienentävät) käytettävän peitteen leveyttä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Vaakakuva) Päällyksen vaakasuuntainen erotus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Jos nykyinen esiasetus tätä tukee, säädä peitteen vasemmalla ja oikealla puolella olevien käyttöliittymän elementtien välistä etäisyyttä käyttäessä vaakasuuntaista näytön asetusta. Positiiviset arvot nousevat (kun taas negatiiviset arvot pienenevät) kahden puolittaisen erotuksen välilllä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Vaakakuva) Päällyksen pystysuuntainen erotus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Jos nykyinen esiasetus tätä tukee, säädä peitteen yläpuolella ja alapuolella olevien käyttöliittymän elementtien välistä etäisyyttä käyttäessä vaakasuuntaista näytön asetusta. Positiiviset arvot nousevat (kun taas negatiiviset arvot pienenevät) kahden puolittaisen erotuksen välilllä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Vaakakuva) Päällyksen X siirtymä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Päällyksen vaakasuuntainen siirtymä, kun käytetään vaakakuva näyttö asetusta. Positiiviset arvot siirtävät päällystä oikealle; negatiiviset arvot vasemalle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Vaakakuva) Päällyksen Y siirtymä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Päällyksen pystysuuntainen siirtymä, kun käytetään vaakakuva näyttö asetusta. Positiiviset arvot siirtävät päällystä ylöspäin; negatiiviset arvot alaspäin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(Pystykuva) Päällyksen mittakaava"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Käyttöliittymä elementtien skaalaus päällyksessä, kun käytetään pystykuva asetusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(Pystykuva) Päällyksen kuvasuhteen säätö"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Käytä kuvasuhteen korjauskerrointa peitteeseen, kun käytetään pystynäytön asetusta. Positiiviset arvot kasvavat (negatiiviset arvot pienenevät) käyttävän peitteen korkeutta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Pystykuva) Peittokuvan vaakatason erotus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Jos nykyinen esiasetus tätä tukee, säädä peitteen vasemmalla ja oikealla puolella olevien käyttöliittymän elementtien välistä etäisyyttä käyttäessä pystysuuntaista näytön asetusta. Positiiviset arvot nousevat (kun taas negatiiviset arvot pienenevät) kahden puolittaisen erotuksen välilllä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Pystykuva) Peittokuvan pystysuuntainen erotus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Jos nykyinen esiasetus tätä tukee, säädä peitteen yläpuolella ja alapuolella olevien käyttöliittymän elementtien välistä etäisyyttä käyttäessä pystysuuntaista näytön asetusta. Positiiviset arvot nousevat (kun taas negatiiviset arvot pienenevät) kahden puolittaisen erotuksen välilllä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Pystykuva) Päällyksen X siirtymä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Päällyksen vaakasuuntainen siirtymä, kun käytetään pystykuva asetusta. Positiiviset arvot siirtävät päällystä oikealle; negatiiviset arvot vasemalle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Pystykuva) Päällyksen Y siirtymä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Päällyksen pystysuuntainen siirtymä, kun käytetään pystykuva asetusta. Positiiviset arvot siirtävät päällystä ylöspäin; negatiiviset arvot alaspäin."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Ota videon asettelu käyttöön"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Videon asettelua käytetään kehyksiin ja muihin taideteoksiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Videon asettelujen polku"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Valitse videon asettelut tiedostoselaimesta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Valittu näkymä"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Valitse näkymä ladattuun ulkoasuun."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Näyttöilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Näytä ruudulla olevat viestit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Graafiset widgetit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Käytä koristettuja animaatioita, ilmoituksia, osoittimia ja ohjaimia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Skaalaa grafiikka widgetit automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Muuta koristettujen ilmoitusten ja osoittimien kokoa automaattisesti nykyiseen valikon asteikkoon perustuen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Grafiikka widgettien skaalauksen ohitus (kokonäyttö)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Käytä manuaalista skaalauskertoimen ohitusta, kun piirretään näyttö widgettejä koko näytön tilassa. Käytössä vain, jos 'skaalaa grafiikka widgetit automaattisesti' on pois päältä. Voidaan käyttää koristettujen ilmoitusten, indikaattorien ja hallintalaitteiden koon kasvattamiseen tai pienentämiseen riippumatta itse valikosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Grafiikka widgettien skaalauksen ohitus (ikkunoitu)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Käytä manuaalista skaalauskertoimen ohitusta, kun piirretään näyttö widgettejä ikkunatilassa. Käytössä vain, jos 'skaalaa grafiikkaa widgettejä automaattisesti' on pois päältä. Voidaan käyttää koristettujen ilmoitusten, indikaattorien ja hallintalaitteiden koon kasvattamiseen tai pienentämiseen riippumatta itse valikosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Näytä ruudunpäivitysnopeus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Näytä nykyinen kuvataajuus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Kuvataajuuden päivitysväli (kuvissa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Näytä kuvien määrä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Näytä kuvalaskuri ruudulla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Näytä tilastot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Näytä tekniset tilastot näytöllä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Näytä muistin käyttö"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Näytä käytetty ja koko muistin määrä järjestelmässä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Muistin käytön päivitysväli (kuvissa)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "\"Lataa sisältöä\" aloitusilmoitus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Näytä lyhyt palaute animaatio sisältöä ladattaessa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Syöttölaitteen (Automaattinen kokoonpano) yhdistämisen ilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Huijauskoodi-ilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Näytä näytöllä viesti kun huijauskoodeja käytetään."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Näytä näytöllä ilmoitus viesti, kun syöttölaitteita yhdistetään/irrotetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Syötteen uudelleenmääritys lataus ilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Näytä ruudulla viesti kun ladataan syötteen uudelleenmääritystiedostoja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Kokoonpanon ohitusten lataus ilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Näytä ruudulla viesti ladatessa kokoonpanon ohitustiedostoa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Palattu ensimmäiseen levyyn ilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Näytä ruudulla viesti, kun palautetaan automaattisesti käynnistyksen yhteydessä viimeksi käytetty monilevyn sisältö M3U soittolistojen kautta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Pikakelauksen ilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Näyttää ruudulla ilmaisimen kun pikakelataan sisältöä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Kuvakaappausten ilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Näytä ruudulla viesti kun otat kuvakaappauksen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Kuvakaappaus ilmoituksen pysyvyys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Määritä ruudulla olevan kuvakaappausviestin kesto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "Normaali"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Nopea"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Erittäin nopea"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Välitön"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Kuvakaappauksen välähdystehoste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Näytä halutun kestoinen valkoinen välähdys ruudulla ottaessa kuvakaappausta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "PÄÄLLÄ (Normaali)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "Päällä (Nopea)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Virkistystaajuuden ilmoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Näytä ruudulla viesti kun virkistystaajuutta asetetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Ilmoituksen fontti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Valitse näyttöilmoitusten kirjasintyyppi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Ilmoituksen koko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Määritä fontin koko pisteinä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Ilmoituksen sijainti (vaaka)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Määritä oma X-akselin sijainti ruudulla olevalle tekstille."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Ilmoituksen sijainti (pysty)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Määritä oma Y-akselin sijainti ruudulla olevalle tekstille."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Ilmoituksen väri (punainen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Ilmoituksen väri (vihreä)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Ilmoituksen väri (sininen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Ilmoituksen tausta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Ilmoituksen taustaväri (punainen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Ilmoituksen taustaväri (vihreä)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Ilmoituksen taustaväri (sininen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Ilmoituksen taustan läpinäkyvyys"
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Valikon kohteiden näkyvyys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Vaihda valikon nimikkeiden näkyvyyttä RetroArchissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Ulkoasu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Muuta valikon ulkoasun asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Näytä lisäasetukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Näytä lisäasetukset tehokäyttäjille."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Kioskitila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Suojaa asennusta piilottamalla kaikki konfiguraatioon liittyvät asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Aseta salasana kioskitilan käytöstä poistamiseksi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "Salasanan luominen kioskitilan käyttöön ottamisen yhteydessä mahdollistaa sen, että se voidaan myöhemmin poistaa käytöstä valikosta, menemällä päävalikkoon, valitsemalla Kioskitila pois päältä ja syöttämällä salasanan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Navigaation jatkuva läpikäynti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Keskeytä sisältö kun valikko on aktiivisena"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Keskeytä käynnissä oleva sisältö jos valikko on aktiivinen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Palaa sisältöön kun pelitilaa on käytetty"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Sulje valikko automaattisesti ja jatka sisältöä pelitilan tallennuksen tai lataamisen jälkeen. Tämän ottaminen pois käytöstä voi parantaa tallentamisen suorituskykyä hyvin hitailla laitteilla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Palaa sisältöön levyn vaihdon jälkeen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Sulje valikko automaattisesti ja jatka sisältöä uuden levyn lisäämisen tai latauksen jälkeen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Lopeta suljettaessa sisältöä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Lopeta RetroArch automaattisesti sulkeassa sisältöä. 'CLI' sulkeutuu vain kun sisältö käynnistetään komentorivillä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Hiirituki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Salli valikon ohjaus hiiren avulla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Kosketustuki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Salli valikon ohjaus kosketusnäytön avulla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Säikeistetyt tehtävät"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Suorita tehtäviä erillisessä säikeessä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Keskeytä sisältö kun ikkuna ei ole aktiivinen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Keskeytä sisältö, kun RetroArch ei ole aktiivinen ikkuna."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Poista käytöstä työpöydän kompositointi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Valikon vierityksen kiihdytys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Kursorin suurin nopeus, kun painetaan vierittämisen suuntaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Käyttöliittymä yhtymä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Käynnistä käyttöliittymä yhtymä käynnistyksessä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Valikkopalkki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Työpöytävalikko (vaatii uudelleenkäynnistyksen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Avaa työpöytävalikko käynnistettäessä"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Pikavalikko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Vaihda pikavalikon kohteiden näkyvyyttä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Asetukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Valitse valikon kohteiden näkyvyys asetukset-valikosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Näytä 'lataa ydin'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Näytä 'lataa ydin' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Näytä 'lataa sisältöä'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Näytä 'lataa sisältöä' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Näytä 'lataa levy'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Näytä 'lataa levy' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Näytä 'kopioi levy'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Näytä 'kopioi levy' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Näytä 'verkkopäivittäjä'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Näytä 'verkkopäivittäjä' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Näytä 'ydin lataaja'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Näytä päivitä ytimet (ja ytimien tietotiedostot) valinta 'verkkopäivittäjä' valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Näytä vanha 'esikatselukuvien päivittäjä'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Näytä valinta vanhentuneiden esikatselukuva pakettien lataamiseen 'verkkopäivittäjä' valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Näytä 'tiedot'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Näytä 'tiedot' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Näytä 'kokoonpanotiedosto'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Näytä 'kokoonpanotiedosto' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Näytä 'ohje'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Näytä 'ohje' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Näytä 'sulje RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Näytä 'sulje RetroArch' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Näytä 'käynnistä RetroArch uudelleen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Näytä 'käynnistä RetroArch uudelleen' valinta päävalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Näytä 'asetukset'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Näytä 'asetukset' valikko. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Aseta salasana saadaksi 'asetukset' käyttöön"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Salasanan syöttäminen, kun asetukset välilehti on piilotettu, mahdollistaa sen palauttamisen myöhemmin valikosta menemällä 'päävalikko' ja valitsemalla 'ota asetukset välilehti käyttöön' ja syöttämällä salasanan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Näytä 'suosikit'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Näytä 'suosikit' valikko. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Näytä 'kuvat'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Näytä 'kuvat' valikko. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Näytä 'musiikki'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Näytä 'musiikki' valikko. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Näytä 'videot'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Näytä 'videot' valikko. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Näytä 'verkkopeli'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Näytä 'verkkopeli' valikko. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Näytä 'historia'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Näytä viimeaikainen historia valikko. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "Näytä 'tuo sisältöä'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "Näytä 'tuo sisältöä' valikko. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH( /* FIXME can now be replaced with MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD */
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Näytä 'tuo sisältöä'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Näytä 'tuo sisältöä' kohde päävalikossa tai soittolistojen alavalikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Päävalikko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Soittolistojen valikko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Näytä 'soittolistat'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Näytä soittolistat. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Näytä \"etsi\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Näytä sisällön etsintä valinta. (Uudelleenkäynnistys vaaditaan Ozone/XMB ajureilla)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Näytä päivä ja aika"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Näytä nykyinen päivämäärä ja/tai aika valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Päivän ja ajan tyyli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Muuta päivämäärän ja/tai ajan tyyliä valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Päiväyksen erotin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Määritä merkki, jota käytetään erottamaan vuoden/kuukauden/päivän väliä, kun päivämäärää esitetään valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Näytä akun varaustaso"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Näytä akun nykyinen varaus valikon sisällä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Näytä ytimen nimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Näytä nykyisen ytimen nimi valikon sisällä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Näytä valikon alatunnisteet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Näytä lisätiedot valikon nimikkeille."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Näytä aloitusnäyttö"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Näytä aloitusnäyttö valikossa. Tämä asetus muutetaan automaattisesti pois käytöstä, kun ohjelma käynnistyy ensimmäistä kertaa."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Näytä \"jatka\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Näytä jatka sisältöä valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Näytä \"käynnistä uudelleen\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Näytä käynnistä sisältö uudestaan valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Näytä \"sulje sisältö\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Näytä 'sulje sisältö' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Näytä \"ota kuvakaappaus\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Näytä 'ota kuvakaappaus' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Näytä \"tallenna/lataa pelitila\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Näytä pelitilan tallentamisen/lataamisen valinnat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Näytä \"peru pelitilan lataus/tallennus\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Näytä pelitilan tallentamisen/lataamisen peruutuksen valinnat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Näytä \"lisää suosikkeihin\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Näytä 'lisää suosikkeihin' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Näytä \"aloita nauhoitus\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Näytä 'aloita nauhoitus' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Näytä \"käynnistä suoratoisto\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Näytä 'Käynnistä striimaus' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Näytä \"liitä ydin\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Näytä 'liitä ydin' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Näytä ''nollaa ytimen liitos\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Näytä 'nollaa ydin liitos' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Näytä \"asetukset\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Näytä 'asetukset' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Näytä \"ohjaimet\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Näytä 'ohjaimet' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Näytä \"huijaukset\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Näytä 'huijaukset' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Näytä \"varjostimet\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Näytä 'varjostimet' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Näytä 'takaisin kelaus'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Näytä 'takaisin kelaus' valinnat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Näytä 'viive'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Näytä 'viive' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Näytä 'näyttöpeite'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Näytä 'näyttöpeite' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Näytä 'videon asettelu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Näytä 'videon asettelu' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Näytä \"tallenna ytimen ohitukset\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Näytä 'tallenna ytimen ohitukset' valinta 'ohitukset' valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Näytä \"tallenna pelin ohitukset\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Näytä 'tallenna ytimen ohitukset' valinta 'ohitukset' valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Näytä \"tiedot\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Näytä 'tiedot' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Näytä \"lataa esikatselukuvat\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Näytä 'lataa esikatselukuvat' valinta."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Näytä 'ajurit'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Näytä 'ajuri' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Näytä 'video'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Näytä 'video' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Näytä 'ääni'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Näytä 'ääni' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Näytä 'syöte'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Näytä 'syöte' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Näytä 'viive'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Näytä 'viive' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Näytä 'ydin'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Näytä 'ydin' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Näytä 'kokoonpano'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Näytä 'kokoonpano' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Näytä 'tallennus'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Näytä 'tallennus' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Näytä 'lokiin kirjaus'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Näytä 'lokiin kirjaus' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Näytä 'tiedostoselain'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Näytä 'tiedostoselain' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Näytä 'kuvien rajoitukset'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Näytä 'kuvien rajoitukset' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Näytä 'nauhoitus'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Näytä 'nauhoitus' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Näytä 'näyttöruutu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Näytä 'näyttöruutu' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Näytä 'käyttöliittymä'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Näytä 'käyttöliittymä' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Näytä 'tekoälypalvelu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Näytä 'tekoälypalvelu' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Näytä 'esteettömyys'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Näytä 'esteettömyys' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Näytä 'virranhallinta'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Näytä 'virranhallinta' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Näytä 'saavutukset'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Näytä 'saavutukset' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Näytä 'verkko'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Näytä 'verkko' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Näytä 'soittolistat'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Näytä 'soittolistat' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Näytä 'käyttäjä'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Näytä 'käyttäjä' asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Näytä kansio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Näytä 'kansio' asetukset."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Valikon skaalauskerroin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Skaalaa käyttöliittymän valikon elementtien koko."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Taustakuva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Valitse kuva, joka asetetaan valikon taustaksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Taustan peittokyky"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Muokkaa taustakuvan läpinäkyvyyttä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Kuvapuskurin läpinäkyvyys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Muokkaa kuvapuskurin läpinäkyvyyttä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Käytä järjestelmän ensisijaista väriteemaa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Käytä käyttöjärjestelmän väriteemaa (jos on). Ohittaa teeman asetukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Esikatselukuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Näytettävän esikatselukuvan tyyppi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Esikatselukuvien suurennuksen arvo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Suurenna automaattisesti esikatselukuvat, joiden leveys / korkeus on pienempi kuin määritetty arvo. Parantaa kuvanlaatua. Vaikuttaa kohtalaisesti suorituskykyyn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Tekstinauhan animaatio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Valitse vaakasuora vieritys menetelmä, jota käytetään pitkän valikon tekstiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Tekstinauhan nopeus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Animaation nopeus, kun vieritetään pitkän valikon tekstiä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Pehmeä tekstinauha"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Käytä pehmeää vieritys animaatiota, kun näytetään pitkä valikkopalkki. Tällä on pieni vaikutus suorituskykyyn."
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Tekoälypalvelun tuloste"
   )
MSG_HASH( /* FIXME What does the Narrator mode do? */
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Näytä käännös tekstipäälllyksenä (kuvatila), tai toista puhesyntetisaattorilla. (puhetila)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "Tekoälypalvelun osoite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "http:// URL osoite, johon käytettävä käännöspalvelu osoittaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Tekoälypalvelu käytössä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Ota käyttöön tekoälypalvelu kun tekoälypalvelu pikanäppäintä painetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Keskeytä käännöksen aikana"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Pysäytä ydin kääntämisen aikana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Lähdekieli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "Kieli, josta palvelu kääntää. Jos asetus on 'oletus', se yrittää tunnistaa kielen automaattisesti. Sen asettaminen tietylle kielelle tekee käännöksestä tarkemman."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Kohdekieli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "Kieli jonhon palvelu kääntää. 'Oletus' on englanti."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Esteettömyystoiminnot käytössä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Ota puhesyntetisaattori käyttöön auttaaksesi valikon navigoinnissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Puhesyntetisaattorin nopeus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Puhesyntetisaattorin äänen nopeus."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Saavutukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Ansaitse saavutuksia klassikko peleissä. Lisätietoja 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Hardcore-tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Kaksinkertaista ansaittujen pisteiden määrä. Poistaa käytöstä, pelitila tallennukset, huijaukset, takaisin kelauksen, taukotilan ja hidastuksen kaikista peleistä. Tämän asetuksen ottaminen käyttöön suorittaessa käynnistää pelin uudelleen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Tulostaulukot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
   "Pelikohtaiset tulostaulukot. Ei ole vaikutusta, jos 'Hardcore tila' on pois päältä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "Rikastettu läsnäolo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Lähetä yksityiskohtainen toiston tila RetroAchievements sivustolle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Saavutusten merkit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Näytä merkit saavutusten listalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Testaa epävirallisia saavutuksia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Käytä epävirallisia saavutuksia ja/tai beeta-ominaisuuksia testaustarkoituksiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Avausääni"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Toista ääni, kun saavutus on avattu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Monisanainen tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Näytä lisää tietoa ilmoituksissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Automaattinen kuvakaappaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Ota kuvakaappaus automaattisesti kun saavutus on ansaittu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Käynnistä aktiivisena"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Aloita istunto kaikilla saavutuksilla (jopa aiemmin avatuilla saavutuksilla)."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Ilmoita verkkopelistä julkisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Ilmoitetaanko verkkopelit julkisesti. Jos asetus ei ole käytössä, asiakkaiden on yhdistettävä manuaalisesti, julkisen aulan sijasta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Käytä välityspalvelinta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Välitä verkkopeliyhteydet välityspalvelimen kautta. Hyödyllinen, jos isäntä on palomuurin takana tai hänellä on NAT/UPnP-ongelmia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Välityspalvelimen sijainti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Valitse tietty välityspalvelin käytettäväksi. Maantieteellisesti lähempänä olevissa paikoissa viive on yleensä pienempi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Palvelimen osoite"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Osoite johon isäntä yhdistää."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Verkkopelin TCP-portti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Isännän IP-osoitteen portti. Voi olla joko TCP tai UDP portti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Palvelimen salasana"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Salasana, jolla asiakkaat yhdistävät isäntään."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Palvelimen seuraajatilan salasana"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Salasana, jolla asiakkaat yhdistävät isäntään katsojana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Verkkopelin seuraajatila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Käynnistä verkkopeli seuraajatilassa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Salli slave-tilan asiakkaat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Salli yhteydet slave-tilassa. Slave-tilassa asiakkaat tarvitsevat hyvin vähän prosessointitehoa kummallakin puolella, mutta kärsivät merkittävästi verkon viiveestä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Estä ei-Slave-tilan asiakkaat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Estä yhteydet jotka eivät ole slave-tilassa. Ei suositella, paitsi erittäin nopeissa verkoissa, joissa erittäin heikot koneet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
   "Pelitilaton verkkopelin tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
   "Käynnistä verkkopeli tilassa, joka ei vaadi pelitila tallennuksia. Erittäin nopea verkkoyhteys tarvitaan, mutta taakse kelausta ei suoriteta, joten verkkopeli ei takkua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Verkkopelin tarkistuskuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "Taajuus (kuvissa), jossa verkkopeli tarkistaa, että isäntä ja asiakas ovat synkronoitu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Syötteen viive kuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Syötteen viiveen kuvien määrä verkon viiveen vähentämiseen. Vähentää viiveen vaihtelua ja tekee verkkopelistä vähemmän suoritin intensiivisen, syötteen viiveen huomattavalla nousulla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Syötteen viiveen kuvien vaihteluväli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Syötteen viiveen kuvien vaihteluväli verkon viiveen vähentämiseen. Vähentää viiveen vaihtelua ja tekee verkkopelistä vähemmän suoritin intensiivisen, syötteen viiveen huomattavalla nousulla."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Kun isännöi, yrittää kuunnella yhteyksiä julkisesta internetistä, käyttäen UPnP tai vastaavia tekniikoita lähiverkkojen välttämiseksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Digitaalisen syötteen jakaminen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Pyydä laitetta %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Pyydä pelaamaan annetun syöttölaitteen kanssa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Verkkokomennot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "Verkon RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Verkon RetroPad perusportti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "Käyttäjän %d verkko RetroPad"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "stdin komennot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "stdin komento käyttöliittymä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Esikatselukuvien lataus tarpeen vaatiessa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Automaattisesti lataa puuttuvat esikatselukuvat selatessasi soittolistoja. Vaikuttaa suorituskykyyn vakavasti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Päivitin"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Buildbotin ytimien URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL osoite ydinpäivittäjän kansioon libretron buildbotissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Buildbotin resussien URL"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL osoite resurssien päivittäjän kansioon libretron buildbotissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Pura ladattu arkisto automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Lataamisen jälkeen, automaattisesti pura tiedostot ladatuissa arkistoissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Näytä kokeelliset ytimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Sisällytä 'kokeelliset' ytimet ydinlataaja listaan. Nämä ovat tyypillisesti vain kehittämis/testaus tarkoituksiin, eikä niitä suositella yleiseen käyttöön."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Varmuuskopioi ytimet päivittäessä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Luo varmuuskopio automaattisesti kaikista asennetuista ytimistä, kun teet verkko päivityksen. Mahdollistaa helpon palautuksen toimivaan ytimeen, jos päivitys aiheuttaa ongelman."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Ydin varmuuskopioiden historian koko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Määritä, kuinka monta automaattisesti luotua varmuuskopiota säilytetään, kullekin asennetulle ytimelle. Kun tämä raja on saavutettu, uuden varmuuskopion luominen verkkopäivittäjän kautta poistaa vanhimman varmuuskopion. Tämä asetus ei vaikuta manuaaliseen ytimen varmuuskopiointiin."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Historia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Säilytä soittolista äskettäin käytetyistä peleistä, kuvista, musiikista ja videoista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Historian koko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Rajoita viimeaikaisten soittolistan pelien, kuvien, musiikin ja videoiden kohteiden määrää."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Suosikkien koko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Rajoita \"suosikit\" soittolistan kohteiden määrää. Kun raja on saavutettu, uudet lisäykset estetään kunnes vanhat merkinnät on poistettu. Asettamalla arvo -1 sallii \"rajoittamattomat\" merkinnät.\nVAROITUS: Arvon pienentäminen poistaa olemassa olevat merkinnät!"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Salli kohteiden uudelleennimeäminen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Salli soittolistan kohteiden uudelleen nimeäminen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Salli kohteiden poistaminen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Salli soittolistan kohteiden poistaminen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Järjestä soittolistat aakkosjärjestykseen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Pakkaa soittolistat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Arkistoi soittolistan tiedot levylle kirjoittaessa. Pienennä tiedoston kokoa ja latausaikoja lisäämällä suorittimien käyttöä (vähäisesti). Voidaan käyttää joko vanhoilla tai uusilla soittolistoilla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Näytä liitetyt ytimet soittolistoilla"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Määritä, milloin soittolistan kohteet merkitään parhaillaan liitettyyn ytimeen (jos sellaisia on).\nTämä asetus ohitetaan, kun soittolistojen alaotsikot ovat käytössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Näytä soittolistan alatunnisteet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Näytä lisätiedot jokaisesta soittolistan kohteesta, kuten nykyinen ydin liitos ja suoritusaika (jos saatavilla). Vaihteleva vaikutus suorituskykyyn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Ydin:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Ajoaika:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Viimeksi pelattu:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Soittolistan alatunnisteen ajoaika"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Valitse minkä tyyppinen ajoaika lokin tallenne näytetään soittolistan alaotsikoissa.\nVastaava ajoaika loki on otettava käyttöön 'tallennus' valikosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "'Viimeksi pelattu' Päivämäärän ja ajan esitysmuoto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Aseta päivämäärän ja ajan esitystyyli 'viimeksi pelattu' aikaleiman tietoihin. '(AM/PM)' valinoilla on pieni vaikutus suorituskykyyn joillakin alustoilla."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Kun etsitään soittolistoista nimikkeitä jotka ovat pakattuja tiedostoja, vastaa vain arkistotiedoston nimeä, [file name]+[content] sijasta. Ota tämä käyttöön, jotta vältytään sisällön kaksoiskappale kohteilta ladattaessa pakattuja tiedostoja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Skannaa ilman täsmäävää ydintä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Salli sisällön skannaaminen ja lisääminen soittolistalle ilman sitä tukevaa ydintä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Hallitse soittolistoja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Siirrettävät soittolistat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Kun käytössä, ja 'tiedostoselain' kansio on myös valittuna, nykyinen parametrin arvo 'tiedostoselain' tallennetaan soittolistalle. Kun soittolista on ladattu toiseen järjestelmään, jossa sama valinta on käytössä, parametrin 'tiedostoselain' arvoa verrataan soittolistan arvoon; jos eri, soittolistojen merkityt polut korjataan automaattisesti."
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Oletusydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Määritä ydin, jota käytetään käynnistettäessä sisältöä soittolistan kautta, jolla ei ole olemassa olevaa ydinliitosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Nollaa ydin liitokset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Poista olemassa olevat ydin liitokset kaikilta soittolistan kohteilta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Järjestystapa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Siivoa soittolista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Poista soittolista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Poista soittolista tiedostojärjestelmästä."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Yksityisyys"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Muuta yksityisyysasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Tilit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Käyttäjänimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Syötä käyttäjätunnuksesi tähän. Tätä käytetään muun muassa verkkopeli istunnoissa ja muissa asioissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Kieli"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Aseta käyttöliittymän kieli."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Salli kamera"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Salli ytimien käyttää kameraa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Discordin rikastettu läsnäolo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Salli Discord sovelluksen näyttää tietoja suoritettavasta sisällöstä.\nSaatavilla vain alkuperäisellä työpöytäsovelluksella."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Salli sijainti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Salli ytimien käyttää sijaintiasi."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Ansaitse saavutuksia klassikko peleissä. Lisätietoja 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_FACEBOOK,
   "Facebook gaming"
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Käyttäjänimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Syötä RetroAchievements tilisi käyttäjätunnus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Salasana"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Syötä salasanasi RetroAchievements tilillesi. Maksimi pituus: 255 merkkiä."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "YouTuben suoratoisto avain"
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Twitchin suoratoisto avain"
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Facebook gaming stream key"
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Järjestelmä/BIOS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "BIOS, käynnistys ROM, ja muut järjestelmäkohtaiset tiedostot on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Lataukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Ladatut tiedostot on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Resurssit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "RetroArchin käyttämät valikon resurssit on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Dynaamiset taustat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Valikossa käytetyt taustakuvat on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Esikatselukuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Kansikuvat, kuvakaappaukset ja otsikkokuvat on tallennettu tähän kansioon."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Tiedostoselain"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Aseta tiedostonselaimen aloituskansio."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Kokoonpanot"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Aseta aloituskansio valikon kokoonpanoselaimelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Ytimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Libretro ytimet on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Ytimen tiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Sovellus ydin tietotiedostot on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Tietokannat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Tietokannat on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
   "Kohdistin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
   "Kohdistimen kyselyt on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Huijaus tiedostot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Huijaus tiedostot on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Videosuodattimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Suorintin pohjaiset videosudattimet on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Äänisuodattimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Äänen DSP suodattimet on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Videovarjostimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Näytönohjain pohjaiset varjostimet on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Tallenteet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Nauhoitteet on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Nauhoituksen kokoonpanot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Nauhoitus kokoonpanot on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Päällykset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Päällykset on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Videon asettelut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Videon asettelut on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Kuvakaappaukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Kuvakaappaukset on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Syötteen automaattinen kokoonpano"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Ohjaimen profiilit, joita käytetään automaattisesti ohjainten määrittämiseen, on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Syötteen uudelleenkartoitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Syötteen uudelleenmääritykset on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Soittolistat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Soittolistat on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Suosikkien soittolista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "Tallenna suosikkien soittolista tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Historian soittolista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "Tallenna historian soittolista tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Kuvien soittolista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Tallenna kuvien soittolista tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Musiikin soittolista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Tallenna musiikin soittolista tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Videoiden soittolista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Tallenna videoiden soittolista tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Ajoaika lokit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Ajoaika lokit on tallennettu tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Tallennus tiedostot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Tallenna kaikki tallennus tiedostot tähän hakemistoon. Jos sitä ei ole asetettu, yritetään tallentaa sisällön työhakemistoon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Pelitila tallennukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Pelitila tallennukset on tallennettu tähän kansioon. Jos hakemistoa ei ole asetettu, yritetään tallentaa ne kansioon, jossa sisältö sijaitsee."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Välimuisti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Arkistoitu sisältö puretaan väliaikaisesti tähän kansioon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Järjestelmän tapahtumalokit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Järjestelmätapahtumien lokit on tallennettu tähän kansioon."
   )

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Lisää mixeriin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Lisää tämä ääniraita käytettävissä olevaan äänivirta lähtöön.\nJos lähtö ei ole tällä hetkellä saatavilla, se ohitetaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Lisää mikseriin ja soita"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Lisää tämä ääniraita käytettävissä olevaan äänivirta lähtöön ja soita se.\nJos lähtö ei ole tällä hetkellä saatavilla, se ohitetaan."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Isännöi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Yhdistä verkkopelin isäntään"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Kirjoita verkkopelin palvelimen osoite ja yhdistä asiakastilassa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Katkaise yhteys verkkopelin isäntään"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Katkaise aktiivinen verkkopeli yhteys."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Päivitä verkkopelin isännöitsijöiden lista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Skannaa ja etsi verkkopelin isäntiä."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Käynnistä verkkopelin isännöinti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Lopeta verkkopelin isännöinti"
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Etsi kansiosta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Etsi kansiosta sisältöä, joka vastaa tietokantaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Etsi tästä kansiosta>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Skannaa tiedosto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Skannaa tiedostoa sisällöksi, joka vastaa tietokantaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Manuaalinen etsintä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Muokattava etsintä sisältötiedostojen nimien perusteella. Ei vaadi tietokannan mukaista sisältöä."
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Lisää mixeriin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Lisää mikseriin ja soita"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Sisältöhakemisto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Valitse kansio, josta etsiä sisältöä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Järjestelmän nimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Määritä 'järjestelmän nimi', johon voidaan liittää etsittyä sisältöä. Nimeää luodun soittolistan ja tunnistaa soittolistan esikatselukuvat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Mukautettu järjestelmän nimi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Määritä 'järjestelmän nimi' etsittyä sisältöä varten. Käytetään vain, kun 'järjestelmän nimi' on asetettu '<Mukautettu>'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Oletusydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Valitse oletusydin, jota käytetään etsittyä sisältöä käynnistettäessä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Tiedostopäätteet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Luettelo tiedostotyypeistä, jotka on sisällytetään etsintään, välilyönneillä erotettuna. Jos tyhjä, sisältää kaikki tiedostotyypit, tai jos ydin on määritelty, kaikki ytimen tukemat tiedostot."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Etsi rekursiivisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Kun käytössä, kaikki alakansiot määritetystä 'sisältöhakemisto' sisällytetään etsintään."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Skannaa arkistotiedostojen sisältä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Kun käytössä, arkistotiedostoista (.zip, .7z jne.) haetaan pätevää tai tuettua sisältöä. Voi vaikuttaa merkittävästi etsinnän suorituskykyyn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Arcade DAT tiedosto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Valitse Logiqx tai MAME lista XML DAT tiedosto ottaaksesi käyttöön etsittyjen arcade sisältöjen automaattisen nimeämisen (MAME, FinalBurn Neo, jne.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Arcade DAT suodatin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Kun käytät arcade DAT tiedostoa, sisältöä lisätään soittolistaan vain, jos DAT tiedostoa vastaava kohde löytyy."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Korvaa olemassa oleva soittolista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Kun käytössä, mikä tahansa olemassa oleva soittolista poistetaan ennen sisällön etsintää. Kun tämä ei ole käytössä, olemassa oleva soittolistan sisältö säilytetään ja vain soittolistalta puuttuva sisältö lisätään."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Käynnistä etsintä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Etsi valittua sisältöä."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Julkaisuvuosi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Pelaajien määrä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Alue"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_TAG,
   "Tunniste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Etsi nimeä..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Näytä kaikki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Erillissuodatin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Kaikki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Lisää erillissuodatin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u kohdetta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Kehittäjän mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Julkaisijan mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Julkaisuvuoden mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Pelaajamäärän mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Tyylilajin mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Alkuperän mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Alueen mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Pelisarjan mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Tunnisteen mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Järjestelmän mukaan"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Käynnistä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Käynnistä sisältö."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Nimeä uudelleen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Nimeä kohde uudelleen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Poista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Poista tämä kohde soittolistalta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Lisää suosikkeihin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Lisää sisältö 'suosikit' soittolistaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Liitä ydin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Nollaa ytimen liitos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Tiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Katso lisätietoja sisältöön liittyen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Lataa esikatselukuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Lataa kuvakaappaus, kansikuva tai otsikkokuva nykyiseen sisältöön liittyen. Päivittää mahdollisesti olemassa olevat esikatselukuvat."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Nykyinen ydin"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Nimi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Tiedostopolku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Ydin"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Peliaika"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Viimeksi pelattu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Tietokanta"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Jatka"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Jatka käynnissä olevaa sisältöä ja poistu Pikavalikosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Käynnistä uudelleen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Käynnistä sisältö uudestaan alusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Sulje sisältö"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Sulje nykyinen sisältö. Tallentamattomat muutokset saattavat kadota."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Ota kuvakaappaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Kaappaa kuva näytöstä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Tilan lohko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Tallenna pelitila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Tallentaa pelitilan valittuun lohkoon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Lataa pelitila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Lataa pelitila tallennuksen valitusta lohkosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Kumoa pelitilan lataus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Jos pelitila oli ladattu, sisältö menee takaisin tilaan ennen latausta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Kumoa pelitilan tallennus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Jos pelitila päällekirjoitettiin, se palautuu takaisin edelliseen pelitilatallennukseen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Lisää suosikkeihin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Lisää sisältö 'suosikit' soittolistaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Aloita nauhoitus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Aloita videon nauhoitus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Lopeta nauhoitus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Lopeta videon nauhoitus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Käynnistä suoratoisto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Aloita suoratoisto valittuun kohteeseen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Lopeta suoratoisto"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Lopeta suoratoisto."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS,
   "Asetukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Muuta käynnissä olevan sisällön asetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Ohjaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Muuta käynnissä olevan sisällön ohjausasetuksia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CHEAT_OPTIONS,
   "Huijauskoodit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Aseta huijauskoodeja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Levyn hallinta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Levykuvien hallinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_OPTIONS,
   "Varjostimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Aseta kuvaa parantavia varjostimia."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Ohitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Asetuksia globaalien kokoonpanoasetusten ohittamiseen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Saavutukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Näytä saavutukset ja niihin liittyvät asetukset."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Hallitse ydinten asetuksia"
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Nollaa asetukset"
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Luo pelin asetustiedosto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Tallenna pelin asetustiedosto"
   )

/* Quick Menu > Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Lataa uudelleenmääritystiedosto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Tallenna ytimen uudelleenmääritystiedosto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Poista ytimen uudelleenmääritystiedosto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Tallenna sisällön kansion uudelleenmääritystiedosto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Poista pelin kansion uudelleenmääritystiedosto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Tallenna pelin uudelleenmääritystiedosto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Poista pelin uudelleenmääritystiedosto"
   )

/* Quick Menu > Controls > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Uudelleenmääritystiedosto"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Käynnistä tai jatka huijaushakua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Lataa Huijaustiedosto (korvaa)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Lataa huijaustiedosto ja korvaa olemassa olevat huijaukset."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Lataa Huijaustiedosto (liittää)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Lataa huijaustiedosto ja lisää se olemassa oleviin huijauksiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Lataa pelikohtaiset huijaukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Tallenna huijaustiedosto nimellä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Tallenna nykyiset huijaukset huijaustiedostona."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Lisää uusi huijaus ylös"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Lisää uusi huijaus alas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Poista kaikki huijaukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Huijaukset käyttöön automaattisesti pelin lataamisen aikana"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Ota huijaukset käyttöön automaattisesti pelin lataamisen aikana."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Käytä vaihtamisen jälkeen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Käytä huijausta heti vaihtamisen jälkeen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Toteuta muutokset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Huijausten muutokset tulevat voimaan välittömästi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT,
   "Huijaus"
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Aloita tai käynnistä uudelleen huijaushaku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Vaihda arvoa painamalla vasen tai oikea."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Yhtä suuri kuin %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Alle Ennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Vähemmän kuin tai yhtä suuri kuin ennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Suurempi kuin ennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Suurempi kuin tai yhtä suuri kuin ennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Yhtä suuri kuin ennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Ei yhtä suuri kuin ennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Vaihda arvoa painamalla vasen tai oikea."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Yhtä kuin ennen +%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Etsi arvoja muistista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Vaihda arvoa painamalla vasen tai oikea."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Yhtä kuin ennen -%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Lisää %u osumaa listaasi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Poista vastaavuus #"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Huijaustiedosto (korvaa)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Huijaustiedosto (liitä)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Huijauksen tiedot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_IDX,
   "Indeksi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Huijauksen sijainti listassa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Käytössä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DESC,
   "Kuvaus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Käsittelijä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Muistihaun koko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_TYPE,
   "Tyyppi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Arvo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Muistiosoite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Selaa osoitetta: %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Muistin osoitteen maski"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Osoitteen bitmaski, kun muistin haun koko < 8-bitti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Toiston määrä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Kuinka monta kertaa huijausta käytetään. Käytä kahta muuta 'toisto' vaihtoehtoa, jotka vaikuttavat suuriin muistialueisiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Osoitteen kasvu joka toistolla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Arvon kasvatus joka toisto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Tärise kun muisti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Tärinän arvo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Tärinä portti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Tärinän ensisijainen voimakkuus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Tärinän ensisijainen kesto (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Tärinän toissijainen voimakkuus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Tärinän toissijainen kesto (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CODE,
   "Koodi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Lisää uusi huijaus tämän jälkeen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Lisää uusi huijaus ennen tätä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Kopioi tämä huijaus jälkeen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Kopioi tämä huijaus ennen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Poista tämä huijaus"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Poista levy asemasta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Avaa virtuaalinen levyasema ja poista nyt ladattu levy. Jos 'keskeytä sisältö kun valikko on aktiivisena' on käytössä, jotkin ytimet eivät välttämättä rekisteröi muutoksia, ellei sisältöä jatketa muutamaa sekuntia jokaisen levyn hallintatoiminnon jälkeen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Syötä levy asemaan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Aseta nykyistä levyindeksiä vastaava levy ja sulje virtuaalinen levyasema Jos 'keskeytä sisältö kun valikko on aktiivisena' on käytössä, jotkin ytimet eivät välttämättä rekisteröi muutoksia, ellei sisältöä jatketa muutamaa sekuntia jokaisen levyn hallintatoiminnon jälkeen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Lataa uusi levy"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Poista nykyinen levy asemasta, valitse uusi levy tiedostojärjestelmästä, aseta se ja sulje virtuaalinen levyasema.\nHUOM: Tämä on vanhentunut ominaisuus. Sen sijaan on suositeltavaa ladata monilevyinen sisältö M3U soittolistojen kautta, jotka sallivat levyn valinnan käyttämällä 'poista/lisää levy asemaan' ja 'nykyinen levyindeksi' valintoja."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Valitse uusi levy tiedostojärjestelmästä ja aseta se sulkematta virtuaalista levyasemaa.\nHUOM: Tämä on vanhentunut ominaisuus. Sen sijaan on suositeltavaa ladata monilevyinen sisältö M3U soittolistojen kautta, jotka sallivat levyn valinnan käyttämällä 'poista/lisää levy asemaan' ja 'nykyinen levyindeksi' valintoja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Nykyinen levyindeksi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Valitse nykyinen levy käytettävissä olevien levyjen luettelosta. Levy ladataan, kun 'Syötä levy asemaan' on valittu."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Videovarjostimet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Toteuta muutokset automaattisesti varjostimen tiedostoihin levyllä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Muista viimeksi käytetty varjostin kansio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Avaa tiedostoselain viimeksi käytetyssä kansiossa, kun ladataan varjostimen esiasetuksia ja vaiheita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Lataa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Lataa varjostimen esiasetus. Varjostimen animaatio asetetaan automaattisesti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
   "Tallenna"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "Tallenna nykyinen varjostimen esiasetus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
   "Poista"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "Poista automaattinen varjostimen esiasetus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Toteuta muutokset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Varjostimen asetusten muutokset tulevat voimaan välittömästi. Käytä tätä, jos muutat varjostimen vaiheita, suodatusta, FBO asteikkoa jne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Varjostimen parametrit"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Muokkaa suoraan nykyistä varjostinta. Muutoksia ei tallenneta esiasetettuun tiedostoon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Varjostin vaiheet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Lisää tai vähennä varjostimen liukuhihnan vaiheita. Erilliset varjostimet voidaan sitoa kuhunkin liukuhihnan vaiheeseen ja määrittää sen laajuus ja suodatus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER,
   "Varjostin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Suodatin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Skaala"
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Yksinkertaiset esiasetukset"
   )
   

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Tallenna varjostin esiasetus nimellä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Tallenna nykyinen varjostimen asetukset uuden varjostimen esiasetuksena."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Tallenna globaali esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Tallenna nykyisen varjostimen asetukset globaalina oletus asetuksena."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Tallenna ytimen esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Tallenna nykyiset varjostimen asetukset oletuksena tälle ytimelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Tallenna sisältökansion esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Tallenna nykyisen varjostimen asetukset oletuksena kaikille nykyisen sisältökansion tiedostoille."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Tallenna pelin esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Tallenna nykyisen varjostimen asetukset sisällön oletusasetuksina."
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Automaattisia varjostistimen esiasetuksia ei löytynyt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Poista globaali esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Poista globaali esiasetus, jota käytetään kaikessa sisällössä ja kaikissa ytimissä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Poista ytimen esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Poista ytimen esiasetus, jota käytetään kaikissa tämän hetkisen ytimen kanssa ajetussa sisällössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Poista sisällön kansion esiasetukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Poista sisällön kansion esiasetukset, jota käytetään kaikessa nykyisessä kansiossa olevassa sisällössä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Poista pelin esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Poista pelin esiasetus, jota käytetään vain kyseisessä pelissä."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Ei varjostimen parametrejä"
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Tallenna ytimen ohitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Tallenna kokoonpanon ohitustiedosto, joka koskee kaikkea tähän ytimeen ladattua sisältöä. Asetetaan pääasetusten edelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Tallenna sisältökansion ohitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Tallenna kokoonpanon ohitustiedosto, joka koskee kaikkea sisältöä, joka on ladattu samasta kansiosta kuin nykyinen tiedosto. Asetetaan pääasetusten edelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Tallenna pelin ohitukset"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Tallenna kokoonpanon ohitustiedosto, joka koskee vain nykyistä sisältöä. Asetetaan pääasetusten edelle."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Ei saavutuksia näytettäväksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_MENU,
   "ToggleCheevosHardcore" /* not-displayed - needed to resolve submenu */
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Peru saavutusten hardcore tilan tauko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Jätä saavutusten harcore tila käyttöön nykyisessä istunnossa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Peruuta jatka saavutusten hardcore tilaa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Jätä saavutusten hardcore tila pois käytöstä nykyisessä istunnossa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Keskeytä saavutuksien hardcore tila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Pysäytä saavutusten hardcore tila nykyisessä istunnossa. Tämä toiminto ottaa käyttöön, pelitila tallennukset, huijaukset, takaisin kelauksen, taukotilan ja hidastuksen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Jatka saavutuksien hardcore tilaa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Jatka saavutusten hardcore tilaa nykyisessä istunnossa. Poistaa käytöstä, pelitila tallennukset, huijaukset, takaisin kelauksen, taukotilan ja hidastuksen kaikista peleistä. Tämän asetuksen ottaminen käyttöön suorittaessa käynnistää pelin uudelleen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Ei sisäänkirjautuneena"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Ei voida aktivoida saavutuksia käyttämällä tätä ydintä"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "RetroAchievements tarkistus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Tietokannan merkintä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Näytä tietokannan tiedot nykyiselle sisällölle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Ei merkintöjä näytettäväksi"
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Ytimiä ei ole saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Ytimen asetuksia ei ole saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Ytimen tietoja ei ole saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Ydinten varmuuskopioita ei saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Ei suosikkeja saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Ei historiaa saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Ei kuvia saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Ei musiikkia saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Ei videoita saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Ei tietoja saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Ei soittolistan kohteita saatavilla"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Asetuksia ei löytynyt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Bluetooth-laitetta ei löytynyt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Verkkoja ei löytynyt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Ei ydintä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Haku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Takaisin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Ylätason kansio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Kansiota ei löydy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Ei kohteita"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Valitse tiedosto"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Tuntematon kääntäjä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Jaa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Tartu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Äänestä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Analogisen syötteen jakaminen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Maksimi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Keskiarvo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Ei mitään"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Ei väliä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Poukkoile vasemalle/oikealle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Vieritä vasemmalle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Kuvatila"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Puhetila"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Kertojatila"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Historia & Suosikit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Kaikki soittolistat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "POIS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Historia & Suosikit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Aina"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Ei koskaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Per ydin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Kooste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Akku täynnä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "Akun lataus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Ei latauksessa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "Ei lähdettä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Käytä tätä kansiota>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Sisältöhakemisto>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Oletus>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Ei mitään>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroPad analogisten kanssa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Ei mitään"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Tuntematon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Pidä Start pohjassa (2 sekuntia)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Pidä Select pohjassa (2 sekuntia)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Alas + Valitse"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Ei käytössä>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Muuttuu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Ei muutu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Nousee"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Laskee"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= tärinän arvo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= tärinän arvo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< tärinän arvo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> tärinän arvo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Kasvaa tärinä arvon mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Laskee tärinä arvon mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Kaikki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Ei käytössä>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Aseta arvoksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Nousee arvon mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Laskeen arvon mukaan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Suorita seuraava huijaus jos arvo = muisti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Suorita seuraava huijaus jos arvo != muisti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Suorita seuraava huijaus jos arvo < muisti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Suorita seuraava huijaus jos arvo > muisti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Emulaattori"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-Bit, Enimmäisarvo = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-Bit, Enimmäisarvo = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-Bit, Enimmäisarvo = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-Bit, Enimmäisarvo = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-Bit, Enimmäisarvo = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-Bit, Enimmäisarvo = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Järjestelmän oletus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Aakkosjärjestyksessä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Ei mitään"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Näytä täydet nimet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Poista ()"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Poista []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Poista () ja []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Pidä alue"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Säilytä levyindeksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Säilytä alue- ja levyindeksi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Järjestelmän oletus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Kansikuva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Kuvakaappaus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Otsikkokuva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "Normaali"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Nopea"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "PÄÄLLÄ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "POIS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Kyllä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Ei"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Tosi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Epätosi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Käytössä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "pois käytöstä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "Ei mitään"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Lukittu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Avattu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Epävirallinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Ei tuettu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Vain jäljittimet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Vain Ilmoitukset"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Oletus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Lineaarinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Lähin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Sisältökansio>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Mukautettu>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Ei määritetty>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Vasen analoginen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Oikea analoginen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "(Näppäin: %s)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Hiiri 1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Hiiri 2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Hiiri 3"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Hiiri 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Hiiri 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Hiiren rulla ylös"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Hiiren rulla alas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Hiiren rulla vasemmalle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Hiiren rulla oikealle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Aikaisin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "Normaali"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Myöhään"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "VVVV-KK-PP TT:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "VVVV-KK-PP TT:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "VVVV-KK-PP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "VVVV-KK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "KK-PP-VVVV TT:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "KK-PP-VVVV TT:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "KK-PP TT:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "KK-PP-YYYY"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD,
   "KK-PP"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "PP-KK-VVVV TT:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "PP-KK-VVVV TT:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "PP-KK TT:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "PP-KK-VVVV"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM,
   "PP-KK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS,
   "TT:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM,
   "TT-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "VVVV-KK-PP TT:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "VVVV-KK-PP TT:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "KK-PP-VVVV HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "KK-PP-VVVV TT:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "KK-PP TT:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "PP-KK-VVVV TT:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "PP-KK-VVVV TT:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "PP-KK TT:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HMS_AMPM,
   "TT:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_HM_AMPM,
   "TT:MM (AM/PM)"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Taustan täytteen paksuus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Lisää valikon taustan ruutukuvion karkeutta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Reunan täydennys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Reunuksen täytteen paksuus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Lisää valikkoruudun reunan karkeutta."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Näytä valikon raja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Käytä täysileveää asettelua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Valikon lineaarinen suodatin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Lisää hieman sumennusta valikkoon pehmentääksesi kovien pikselien reunoja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Sisäinen suurentaminen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Skaalaa valikon käyttöliittymää, ennen näytölle piirtämistä. Kun käytössä 'valikon lineaarinen suodatin', poistaa skaalausvirheet (epätasaiset pikselit) samalla säilyttäen terävän kuvan. Vaikuttaa merkittävästi suorituskykyyn, jonka vaikutus lisääntyy skaalauksen myötä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Valikon kuvasuhde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Valitse valikon kuvasuhde. Laajakuvan kuvasuhteet lisäävät valikon käyttöliittymän vaakasuoraa resoluutiota. (Voi vaatia uudelleenkäynnistyksen, jos 'lukitse valikon kuvasuhde' on pois päältä)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Lukitse valikon kuvasuhde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Varmistaa, että valikko on aina näkyvissä oikealla kuvasuhteella. Jos tämä ei ole käytössä, pikavalikko venytetään vastaamaan tällä hetkellä ladattua sisältöä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Valikon väriteema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Valitse eri väriteema. Valitsemalla 'mukautettu' mahdollistaa valikon teeman esiasetettujen tiedostojen käytön."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Mukautetun valikon teeman esiasetus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Valitse valikon teeman esiasetus tiedostoselaimesta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Varjotehosteet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Ota käyttöön varjot valikon tekstille, rajoille ja esikatselukuville. Vaikutus suorituskykyyn on kohtalainen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Taustan animaatio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Ota taustahiukkasten animaatioefekti käyttöön. Vaikutus suorituskykyyn on merkittävä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Taustan animaation nopeus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Säädä taustaanimaation efektien nopeutta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Näytä soittolistan esikatselukuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Ota käyttöön esikatselukuvien näyttäminen pienennettyinä näytettäessä soittolistoja. Kun tämä ei ole käytössä, 'ylä esikatselukuva' voidaan silti vaihtaa kokoruututilaan painamalla RetroPad Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Ylä esikatselukuva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Pikkukuvan tyyppi joka näytetään soittolistan yläoikealla. Tämä pikkukuva voidaan näyttää kokoruudulla painamalla RetroPad Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Ala esikatselukuva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Esikatselukuvan tyyppi joka näytetään soittolistojen oikeassa alakulmassa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Vaihda esikatselukuvat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Vaihtaa 'ylä esikatselukuvan' ja 'ala esikatselukuvan' paikkoja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Esikatselukuvan kutistus menetelmä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Esikatselukuvan viive (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Vaikuttaan aika viiveeseen esikatselukuvien lataamisen välillä. Tämän määrittäminen vähintään 256 ms arvoksi mahdollistaa nopean viiveettömän vierittämisen myös hitaimmilla laitteilla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Laajennettu ASCII-tuki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Ota käyttöön ei-standardinmukaisten ASCII-merkkien näyttö. Tarvitaan tiettyjen muiden kuin länsimaisten kielten kanssa. Vaikutus suorituskykyyn on kohtalainen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Näytä Switchin kuvakkeet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Käytä kuvakkeita PÄÄLLÄ/POIS tekstin sijaan edustamaan 'vaihda switch' valikko asetusten kohteita."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Lähin naapuri (nopea)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "Bilineaarinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (Hidas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Ei mitään"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_AUTO,
   "Automaattinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (keskitetty)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (keskitetty)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (keskitetty)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "POIS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Sovita näyttöön"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Skaalaa kokonaisluvuin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Täytä näyttö (venytetty)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Mukautettu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Klassinen punainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Klassinen oranssi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Klassinen keltainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Klassinen vihreä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Klassinen sininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Klassinen violetti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Klassinen harmaa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Perinteinen punainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Tummanvioletti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Keskiyönsininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Kultainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Sähkönsininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Omenanvihreä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vulkaanisen punainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Laguuni"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Gruvbox vaalea"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hakkerointi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_NORD,
   "Pohjola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ONE_DARK,
   "One dark"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_PALENIGHT,
   "Kirkas yö"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized, tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarized, vaalea"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango, tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango, vaalea"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "POIS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Lumisade (hieman)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Lumisade (sakea)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Sade"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Tähtikenttä"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Vasen esikatselukuva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Pienoiskuvan tyyppi näytetään vasemmalla puolella."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Dynaaminen tausta"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Dynaamisesti lataa uusi taustakuva riippuen kontekstista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Vaakasuuntainen animaatio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Ota käyttöön vaakasuuntainen animaatio valikkoon. Tämä vaikuttaa suorituskykyyn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Vaakakuvakkeen korostus animaatio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Animaatio, joka näytetään vierittäessä välilehtien välillä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Ylös/alas siirtymän animaatio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Animaatio, joka näytetään liikkuessaan ylös tai alas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Sulje/avaa animaatio päävalikossa"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Animaatio, joka näytetään kun avaat alavalikon."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Valikon fontti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Valitse eri fontti valikossa käytettäväksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Valikon fontin väri (punainen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Valikon fontin väri (vihreä)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Valikon fontin väri (sininen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Valikon asettelu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Valitse eri asettelu XMB-käyttöliittymälle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Valikon kuvaketeema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Valitse eri kuvaketeema RetroArchille."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Kuvakkeiden varjot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Piirrä varjot kaikille kuvakkeille. Vaikutus suorituskykyyn pieni."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Valikon animaatio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Valitse animoitu tausta efekti. Voi olla näytönohjain intensiivinen efektin mukaan. Jos suorituskyky ei ole tyydyttävä, joko poista käytöstä tai palaa yksinkertaisempaan efektiin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Valikon väriteema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Valitse taustan eri liukuväriteema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Esikatselukuvan pystysuuntainen sijoitus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Näyttää vasemman esikatselukuvan oikean alapuolella, näytön oikeassa reunassa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Esikatselukuvan skaalauksen kerroin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Vähennä esikatselukuvan kokoa skaalaamalla suurinta sallittua leveyttä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Ota asetukset välilehti käyttöön (uudelleenkäynnistys vaaditaan)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Näytä asetukset välilehti, joka sisältää ohjelman asetukset."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Nauha"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Nauha (yksinkertaistettu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Yksinkertainen lumisade"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Lumisade"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Lumihiutale"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Mukautettu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME,
   "Yksivärinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Käänteinen yksiväri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Systemaattinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_PIXEL,
   "Pikseli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Automaattinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Automaattinen käänteinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Omenanvihreä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Vaalea"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Aamun sininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Auringonsäde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Tummanvioletti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Sähkönsininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Kultainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Perinteinen punainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Keskiyönsininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Pelkistetty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Merenalainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Vulkaanisen punainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Limen vihreä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Pikachun keltainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Lila"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Ruusun punainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Palavan kuuma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Jääkylmä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDGAR,
   "Liukuväri"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Piilota sivupalkki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Pidä vasen sivupalkki aina pienennettynä."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Soittolistat järjestetään uudelleen aakkosjärjestykseen sen jälkeen, kun valmistajan osa niiden nimistä on poistettu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Valikon väriteema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Valitse eri väriteema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Pelkistetty valkoinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Pelkistetty musta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox, tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY,
   "Karhunvatukka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Hakkerointi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "Iltarusko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Toinen esikatselukuva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Korvaa sisällön metatieto toisella esikatselukuvalla."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Käytä tekstinauhaa sisällön metatietoon"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Kun käytössä, soittolistojen oikean sivun metatietojen jokainen kohde (liitetty ydin, toistoaika) on yhdellä rivillä; merkkijonot, jotka ylittävät sivupalkin leveyden, näytetään vieritettävänä tekstinauhana. Kun pois päältä, jokaiset sisällön metatiedot näytetään staattisesti, käytetään niin monta riviä kuin tarvitaan."
   )

/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Valikon kuvakkeet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Näytä kuvakkeet valikon kohteiden vasemmalla puolella."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Soittolistan kuvakkeet (uudelleenkäynnistys vaaditaan)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Näytä järjestelmäkohtaiset kuvakkeet soittolistoissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Optimoi vaakasuuntainen asettelu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Säädä valikon asettelua automaattisesti sopivaksi näytölle kun käytetään vaakasuuntaista näytön asetelmaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Näytä navigointipalkki"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Näytä näytöllä pysyvät valikon navigointi pikakuvakkeet. Sallii nopean vaihdon valikko kategorioiden välillä. Suositellaan kosketusnäyttö laitteille."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Kierrä navigointipalkkia automaattisesti"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Siirrä navigointipalkki automaattisesti näytön oikealle puolelle, kun käytetään vaakasuuntaista näytön asetelmaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Valikon väriteema"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Valitse taustan eri liukuväriteema."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Valikon siirtymän animaatio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Ota käyttöön sulavat animaatioefektit, kun navigoit valikon eri tasojen välillä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Esikatselukuvan pystykuva näkymä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Määritä soittolistan esikatselukuvien katselutila kun käytetään pystykuva asetusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Esikatselukuvan vaakakuva näkymä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Määritä soittolistan esikatselukuvien katselutila käytettäessä vaakakuva asetusta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Näytä toissijainen esikatselukuva listanäkymässä"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Näyttää toissijaisen esikatselukuvan käytettäessä 'lista' tyyppistä soittolistan esikatselukuvan näkymätilaa. Tämä asetus pätee vain silloin, kun näytöllä on riittävä fyysinen leveys kahden esikatselukuvan näyttämiseksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Piirrä esikatselukuvan taustat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Mahdollistaa käyttämättömän tilan täyttämisen esikatselukuvissa, joissa on kiinteä tausta. Tämä takaa tasaisen koon kaikille kuville, parantaa valikon ulkonäköä katsottaessa sekamuotoisia esikatselukuvia, joilla on vaihtelevat perusmitat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Ensisijainen esikatselukuva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Esikatselukuvan päätyyppi jota näytetään jokaisen soittolistan kohteella. Tyypillisesti toimii sisällön kuvakkeena."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Toissijainen esikatselukuva"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Toissijainen esikatselukuvan tyyppi jota näytetään jokaisen soittolistan kohteella. Käyttö riippuu nykyisestä soittolistan esikatselukuvan näkymätilasta."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Sininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Siniharmaa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Tummansininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Vihreä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "Kilpi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Punainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Keltainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Material UI tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozone tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NORD,
   "Pohjola"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarized, tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Söpö sininen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Söpö syaani"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Söpö vihreä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Söpö oranssi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Söpö pinkki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Söpö violetti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Söpö punainen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_VIRTUAL_BOY,
   "Virtual boy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Hakkerointi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_AUTO,
   "Automaattinen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Häivytys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Liukuva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "POIS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "POIS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Luettelo (pieni)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Luettelo (Keskikokoinen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Kaksoiskuvake"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "POIS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Luettelo (pieni)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Luettelo (keskikokoinen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Luettelo (suuri)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Työpöytä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "POIS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "PÄÄLLÄ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Jätä esikatselukuvat pois"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Tiedot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Tiedosto"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Lataa ydin..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Vapauta ydin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "Lo&peta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Muokkaa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Haku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Näytä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Suljetut telakat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Varjostimen parametrit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Asetukset..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Muista telakoiden sijainti:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Muista ikkunan geometria:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Muista viimeisin sisältöselaimen välilehti:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Teema:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Järjestelmän oletus>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Tumma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Mukautettu..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Asetukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Työkalut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Ohje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "Tietoja RetroArchista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_DOCUMENTATION,
   "Dokumentaatio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Lataa mukautettu ydin..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Lataa ydin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Ladataan ydintä..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Nimi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_VERSION,
   "Versio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Soittolistat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Tiedostoselain"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Yläreuna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Ylös"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Sisältöselain"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Kansikuva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Kuvakaappaus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Otsikkokuva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Kaikki soittolistat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Ydin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Ytimen tiedot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Kysy>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Tietoja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Varoitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Virhe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Verkkovirhe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Käynnistä ohjelma uudelleen, jotta muutokset astuvat voimaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Loki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 kohdetta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Pudota kuva tähän"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Älä näytä tätä uudelleen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Pysäytä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Liitä ydin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Piilotetut soittolistat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Piilota"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Korostusväri:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Valitse..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Valitse väri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Valitse teema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Mukautettu teema"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "Tiedostopolku on tyhjä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Tiedosto on tyhjä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Tiedostoa ei voitu avata lukemista varten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Tiedostoa ei voitu avata kirjoitusta varten."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Tiedostoa ei ole olemassa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Ehdota ladattua ydintä ensin:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Zoomaus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Näytä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Kuvakkeet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Lista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Tyhjennä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Edistyminen:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "\"Kaikki soittolistat\" suurin listan kohteiden määrä:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "\"Kaikki soittolistat\" suurin ruudukon listan määrä:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Näytä piilotetut tiedostot ja kansiot:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Uusi soittolista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Anna nimi uudelle soittolistalle:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Poista soittolista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Nimeä soittolista uudelleen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Haluatko varmasti poistaa soittolistan \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_QUESTION,
   "Kysymys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Tiedostoa ei voitu poistaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Tiedostoa ei voitu nimetä uudelleen."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Kerätään listaa tiedostoista..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Lisätään tiedostoja soittolistaan..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Soittolistan kohde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Nimi:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Polku:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Ydin:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Tietokanta:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Laajennukset:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(välilyönnillä eroteltu; kaikki sisältyy oletusarvoisesti)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Suodata arkistojen sisällä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(käytetään esikatselukuvien löytämiseen)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Haluatko varmasti poistaa kohteen \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Valitse ensin yksi soittolista."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Poista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Lisää merkintä..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Lisää tiedosto(ja)..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Lisää kansio..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Muokkaa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Valitse tiedostot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Valitse kansio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<useita>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Virhe soittolistan kohdetta päivittäessä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Täytä kaikki vaaditut kentät."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "Päivitä RetroArch (nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch päivittyi onnistuneesti. Käynnistä sovellus uudelleen, jotta muutokset tulevat voimaan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Päivitys epäonnistui."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Avustajat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Nykyinen varjostin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Siirrä alas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Siirrä ylös"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Lataa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Tallenna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Poista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Poista vaiheet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Toteuta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Lisää vaihe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Tyhjennä kaikki vaiheet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Ei varjostin vaiheita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Nollaa vaihe"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Nollaa kaikki vaiheet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Nollaa parametri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Lataa esikatselukuva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Lataus on jo käynnissä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Aloita soittolistasta:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Esikatselukuva"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Esikatselukuvien välimuistin raja:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Vedä-pudota esikatselukuvan kokoraja:"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Lataa kaikki pikkukuvat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Koko järjestelmä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Tämä soittolista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Esikatselukuvat ladattu onnistuneesti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Onnistui: %1 Epäonnistui: %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Ytimen asetukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Palauta asetukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Palauta kaikki"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Päivitin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Cheevos käyttäjätili"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Käyttäjätililuettelon päätepiste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TURBO_DEADZONE_LIST,
   "Turbo/katvealue"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Ydin laskurit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Ei levyä valittuna"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Käyttöliittymän laskurit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Vaakasuora valikko"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Piilota sitomattomat ytimen syötteen kuvaukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Näytä syötteen kuvauksen tunnisteet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Näyttöpeite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Historia"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Valitse sisältöä viimeaikaisen historian soittolistalta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Alijärjestelmät"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Verkkopelin isäntiä ei löytynyt."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Ei suorituskyky laskureita."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Ei soittolistoja."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Yhdistetty"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "Verkossa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT,
   "Portti"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Huijaus asetukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Käynnistä tai jatka huijaushakua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Käynnistä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "sekuntia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Käynnistä ydin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Käynnistä ydin ilman sisältöä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Ehdotetut ytimet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Pakattua tiedostoa ei voitu lukea."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Käyttäjä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "Näp"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Käytä sisäänrakennettua kuvankatselinta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Suurin kuvien vaihtoketju"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Kertoo video ajurin käyttämään erikseen määriteltyä puskurointi tilaa."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Muokkaa varjostimen esiasetusta, jota käytetään tällä hetkellä valikossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Varjostimen esiasetus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Selaa URL-osoitetta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "URL Polku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Käynnistä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Nimimerkki: %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Yhteensopivaa sisältöä löytynyt"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Käynnistä gong"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "Automaattinen kuvasuhde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "Nimimerkki (LAN): %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "Tila"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Järjestelmän taustamusiikki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
   "Mukautettu suhde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Nauhoitustuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "Tallenna ulostulon nauhoitus..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "Tallenna nauhoitteet ulostulo kansioon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "Näytä vastaavuus #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "Valitse tarkasteltava vastaavuus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "Pakota kuvasuhde"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "Valitse soittolistalta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME,
   "Jatka"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME,
   "Jatka käynnissä olevaa sisältöä ja poistu pikavalikosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "Näytä luettelo %u vastaavuuksista"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "Luo koodi tästä vastaavuudesta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "Poista tämä vastaavuus"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "Alatunnisteen läpinäkyvyys"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "Muokkaa alatunnisteen grafiikan läpinäkyvyyttä."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "Otsikon läpinäkyvyys"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "Muokkaa otsikon graafista läpinäkyvyyttä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Verkkopeli"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "Käynnistä sisältö"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Sisällön historian polku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Ulostulon tunnuksen ID"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Valitse lähtöportti joka on yhdistetty CRT-näyttöön."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Ohje"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Äänen ja videon ongelmanratkaisu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "Virtuaalinohjaimen päällyskuvan vaihtaminen"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "Ladataan sisältöä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "Etsitään sisältöä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "Mikä on ydin?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "Lähetä vianjäljitystiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
   "Lähettää diagnoositiedot laitteestasi ja RetroArch-kokoonpanostasi palvelimillemme analysoitavaksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Tietokannan asetukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Verkkopelin viive kuvat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Skannaa paikallisverkko"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Etsi ja yhdistä verkkopelin isäntiin paikallisessa verkossa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Verkkopelin asiakas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Verkkopelin seuraaja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_DESCRIPTION,
   "Kuvaus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "Rajoita suurinta pyöritys nopeutta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "Aloita etsintä uudelle huijauskoodille"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "Aloita etsintä uudelle huijaukselle. Bittien määrää voidaan muuttaa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Jatka hakua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Jatka uuden huijauksen etsimistä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Saavutukset (Hardcore)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Huijauksen tiedot"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Hallitse huijauksen yksityiskohtia asetuksissa."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Käynnistä tai jatka huijaushakua"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Aloita tai jatka huijauskoodin hakua."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Huijaus vaiheet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Lisää tai vähennä huijausten määrää."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Vasen analogi X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Vasen analogi Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Oikea analogi X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Oikea analogi Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Käynnistä tai jatka huijaushakua"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Tietokannan kohdistinluettelo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Tietokanta - Suodata : Kehittäjä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Tietokanta - Suodata : Julkaisija"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Tietokanta - Suodata : Alkuperä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Tietokanta - Suodata : Pelisarja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Tietokanta - Suodata : ESRB luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Tietokanta - Suodata : ELSPA luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Tietokanta - Suodata : PEGI luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Tietokanta - Suodata: CERO luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Tietokanta - Suodata : BBFC luokitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Tietokanta - Suodata : Maksimi pelaajat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Tietokanta - Suodata : Julkaisukuukausi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Tietokanta - Suodata : Julkaisuvuosi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Tietokanta - Suodata : Edge Magazine julkaisu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Tietokanta - Suodata : Edge Magazine arvostelu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Tietokannan tiedot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Kokoonpano"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Verkkopelin asetukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Slang-tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "OpenGL/Direct3D renderöinti tekstuuriksi (multi-pass varjostimet) tuki"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Sisältö"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "Yleensä kehittäjien asettama, jotka niputtavat libretro/RetroArch sovelluksia osoittamaan resursseihin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Kysy"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Valikon perusohjaimet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Hyväksy/OK"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "Tiedot"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "Lopeta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Vieritä ylös"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Oletukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Näytä/piilota näppäimistö"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Avaa/sulje valikko"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "Valikossa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "Pelissä"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "Pelissä (keskeytetty)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "Pelaa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "Keskeytetty"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Verkkpeli alkaa, kun sisältö on ladattu."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Sopivaa ydintä tai sisältötiedostoa ei löydy, lataa manuaalisesti."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Grafiikka ohjaimesi ei ole yhteensopiva nykyisen RetroArchin video ajurin kanssa, palataan %s ajuriin. Käynnistä RetroArch uudelleen, jotta muutokset tulevat voimaan."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Ytimen asennus onnistui"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Ytimen asennus epäonnistui"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Poista kaikki huijaukset painamalla oikealle viisi kertaa."
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "Vianjäljitystietojen tallennus epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_DEBUG_INFO,
   "Vianjäljitystietojen lähettäminen palvelimeen epäonnistui."
   )
MSG_HASH(
   MSG_SENDING_DEBUG_INFO,
   "Lähetetään vianjäljitystietoa..."
   )
MSG_HASH(
   MSG_SENT_DEBUG_INFO,
   "Vianjäljitystiedot lähettiin palvelimelle onnistuneesti. ID-numerosi on %u."
   )
MSG_HASH(
   MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
   "Paina vielä kaksi kertaa lähettääksesi vianmääritystiedot RetroArchin tiimille."
   )
MSG_HASH(
   MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
   "Paina vielä kerran lähettääksesi vianmääritystiedot RetroArchin tiimille."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Yleinen mikserin äänenvoimakkuus"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Verkkopelin skannaus valmistui."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Pahoittelyt, toteuttumaton: ytimet, jotka eivät vaadi sisältöä, eivät voi osallistua verkkoonpeliin."
   )
MSG_HASH(
   MSG_NATIVE,
   "Natiivi"
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "Laitteen yhteys porttiin katkesi"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Tuntematon verkkopeli komento vastaanotettu"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Tiedosto on jo olemassa. Tallennetaan varmuuskopiointi puskuriin"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "Yhteys muodostettiin: \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Yhteys muodostettiin: \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Portin kartoitus onnistui"
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Porttien kartoitus epäonnistui"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "Ei lisättyjä parametreja ja eikä sisäänrakennettua valikkoa, näytetään ohje..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Asetetaan levy asemaan"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "Odotetaan asiakasta ..."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Poistuit pelistä"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "Liityit pelaajana %u"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "Olet liittynyt syöttölaitteisiin %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "Pelaaja %.*s poistui pelistä"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s on liittynyt pelaajaksi %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s on liittynyt syöttölaitteisiin %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Verkon yhteysyritys epäonnistui, koska toinen osapuoli ei käytä RetroArchia tai käyttää vanhaa versiota RetroArchista."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Verkkopelin toinen osapuoli käyttää vanhaa versiota RetroArchista. Ei voida yhdistää."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "VAROITUS: Verkkopelin toinen osapuoli käyttää eri RetroArch versiota. Jos ongelmia ilmenee, käytä samaa versiota."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Verkkopelaajalla on eri ydin. Ei voida muodostaa yhteyttä."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "VAROITUS: Verkkopelaajat käyttävät eri versiota ytimestä. Jos ongelmia ilmenee, käytä samaa versiota."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Tämä ydin ei tue näiden järjestelmien arkkitehtuurien välistä verkkopeliä"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Tämä ydin ei tue arkkitehtuurien välistä verkkopeliä"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Anna verkkopelin palvelimen salasana:"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Sallitko yhteyden käyttäjältä:"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Virheellinen salasana"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" katkaisi yhteyden"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Verkkopelin asiakas katkaisi yhteyden"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Verkkopelin yhteys katkaistu"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "Sinulla ei ole oikeutta pelata"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "Ei vapaita pelaajapaikkoja"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "Pyydetyt syöttölaitteet eivät ole käytettävissä"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Ei voida vaihtaa toistotilaan"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Verkkopelin vertainen \"%s\" keskeytti"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Nimimerkkisi muutettiin muotoon \"%s\""
   )
MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Äänenvoimakkuus"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Havaitse automaattisesti"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Pelitila tallennuksen lataaminen kohteesta"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Ominaisuudet"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Yhdistetään verkkopelin isäntään"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Yhdistä porttiin"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Liitäntäpaikka"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "Noudetaan ydinluetteloa..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Ydinlistan nouto epäonnistui!"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "Uusin versio on jo asennettu: "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Päivitetään ydin: "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Ladataan ydin: "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Puretaan ydintä: "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Ydin asennettu:"
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Ytimen asennus epäonnistui: "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Skannataan ytimiä..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Tarkistetaan ydintä: "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Kaikki asennetut ytimet uusimmissa versioissaan"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Kaikki tuetut ytimet vaihdettu play kaupan versioihin"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "ytimet päivitetty: "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "ytimet ohitettu: "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Ydinpäivitys poistettu käytöstä - ydin on lukittu: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Nollataan ytimet: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Nollatut ytimet: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Siivotaan soittolistaa: "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Soittolista siivottu: "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Lisätty suosikkeihin"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Suosikin lisääminen epäonnistui: soittolista täynnä"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Ydin asetettu: "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "Soittolistan kohteen ydin liitos on nollattu."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Lisätty levy"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Levyn lisääminen epäonnistui"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Sovellus kansio"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Asetetaan huijaus muutoksia."
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Asetetaan varjostinta"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Ääni mykistetty."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Äänen mykistys poistettu."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Virhe tallennettaessa automaattista kokoonpanotiedostoa."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Automaattinen kokoonpanotiedosto tallennettiin onnistuneesti."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Automaatti tallennusta ei voitu alustaa."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Pelitila tallenteen automaattinen tallennus kohteeseen"
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Estetään SRAM ylikirjoitus"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Tuo komentoliittymä porttiin"
   )
MSG_HASH(
   MSG_BYTES,
   "tavua"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Ei voida päätellä uutta kokoonpanon polkua. Käytä nykyistä aikaa."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Saavutuksien hardcore tila käytössä, pelitila tallennukset ja takaisin kelaus poistettiin käytöstä."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "Verrataan tunnettuihin taikuuden numeroihin..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Koottu vastaamaan API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Aloituskansiota ei ole asetettu. Uutta asetusta ei voida tallentaa."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Yhdistetty"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "CRC32 sisältö eroaa. Eri pelejä ei voi käyttää."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Sisällön lataus ohitettu. Toteutus lataa sen itse."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Ydin ei tue pelitilan tallennusta."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Ytimen valintatiedosto luotiin onnistuneesti."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Seuraavaa ajuria ei löytynyt"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Yhteensopivaa järjestelmää ei löytynyt."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Ei löytynyt kelpaavaa raitaa"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "raitaa ei voitu lukea"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "Sisältötiedostoa ei voitu lukea"
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Elokuvan otsikkoa ei voitu lukea."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Tilaa ei voitu lukea elokuvasta."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "CRC32 tarkistussumma ei täsmää sisältötiedoston ja tallennetun sisällön tarkistussumman välillä toistotiedoston otsikossa. Toisto erittäin todennäköisesti desynkronoitu toistaessa."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Mukautettu ajoitus annettu"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Purkaminen on jo käynnissä."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Purkaminen epäonnistui."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Havaitun ikkunan resoluutio"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Voimassa olevaa sisällön päivitystä ei löytynyt."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Irrota laite kelvollisesta portista."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Suljettiin"
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Poistettiin"
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Ladataan"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "Indeksi"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Lataaminen epäonnistui"
   )
MSG_HASH(
   MSG_ERROR,
   "Virhe"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Libretro-ydin vaatii sisältöä, mutta mitään ei annettu."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Libretro-ydin vaatii erikoissisältöä, mutta mitään ei annettu."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Ydin ei tue VFS:ää ja lataus paikallisesta kopiosta epäonnistui"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Virhe parametrien jäsentämisessä."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Virhe tallennettaessa ydinasetustiedostoa."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Virhe tallennettaessa uudelleenkartoitustiedostoa."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Virhe poistettaessa uudelleenmääritystiedostoa."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Virhe tallennettaessa varjostimen esiasetusta."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Ulkoinen sovelluskansio"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Puretaan"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Puretaan tiedostoa"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Virhe tallennettaessa asetustiedostoa"
   )
MSG_HASH(
   MSG_FAILED_TO,
   "Epäonnistui"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Saapuvan katsojan hyväksyminen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Muistin varaaminen päivitetylle sisällölle epäonnistui..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Varjostimen käyttöönotto epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Varjostimen esiasetuksia ei voitu ottaa käyttöön:"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Liitännän liittäminen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Kansion luominen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Sisällön purkaminen pakatusta tiedostosta epäonnistui"
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Nimimerkin saaminen asiakkaalta epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Epäonnistuttiin lataamaan"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Sisällön lataaminen epäonnistui"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Elokuvatiedoston lataaminen epäonnistui"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Päällyksen lataaminen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Pelitilan lataaminen epäonnistui kohteesta"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Ei voitu avata libretro-ydintä"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Päivitys epäonnistui"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Ei saatu otsikkoa asiakkaalta."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Ei saatu nimimerkkiä."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Nimimerkin saaminen isännältä epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Nimimerkin kokoa ei voitu vastaanottaa isännältä."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "SRAM dataa ei voitu vastaanottaa isännältä."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Levyn poistaminen levyasemasta epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Väliaikaisen tiedoston poistaminen epäonnistui"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "Tallennus SRAMiin epäonnistui"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Pelitilan tallennus epäonnistui kohteeseen"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Nimimerkin lähettäminen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Nimimerkin kokoa ei voitu lähettää."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Nimimerkin lähettäminen asiakkaalle epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Nimimerkin lähettäminen isännälle epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "SRAM dataa ei voitu lähettää asiakkaalle."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Ääniajurin käynnistäminen epäonnistui. Jatketaan ilman ääntä."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Elokuvan nauhoituksen käynnistäminen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Nauhoituksen käynnistäminen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Kuvakaappausta ei voitu ottaa."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Latauksen peruminen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Pelitilan tallentamisen peruminen epäonnistui."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Äänen mykistyksen poisto epäonnistui."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Kohtalokas virhe vastaanotettu"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Tiedostoa ei löytynyt"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Automaattinen pelitila tallennus löytyi"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Levyn nimi löytyi"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Ensimmäinen raita löytyi tiedostosta"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Viimeisin tallennustila löydetty"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Löydetty varjostin"
   )
MSG_HASH(
   MSG_FRAMES,
   "Kuvat"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Vastaanotettiin virheellinen levyindeksi."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "Hiiren kaappaus tila"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Pelin tarkennus käytössä"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Pelin tarkennus pois käytöstä"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Libretro ydin on laitteisto renderöivä. On käytettävä myös jälki-varjostettua nauhoitusta."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "Laskettu tarkistussumma ei vastannut CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Sisääntulon huijaus"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Sisääntulon huijauksen tiedostonimi"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Syötteen esiasetuksen tiedostonimi"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Nimeä kohde uudelleen"
   )
MSG_HASH(
   MSG_INTERFACE,
   "Käyttöliittymä"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Sisäinen tallennustila"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Irrotettava tallennustila"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Virheellinen nimen koko."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "tavuina"
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "megatavuina"
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "gigatavuina"
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "on koottu eri libretro versiota vastaan kuin tämä libretro toteutus."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Käyttöliittymä libretrolle"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Ladattiin pelitila lohkosta #%d."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Ladattiin pelitila lohkosta #-1 (automaattisesti)."
   )
MSG_HASH(
   MSG_LOADING,
   "Ladataan"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Yksi tai useampi firmware-tiedosto puuttuu"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Ladataan sisältötiedostoa"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Ladataan historiatiedostoa"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Ladataan suosikkien tiedostoa"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Ladataan pelitilaa"
   )
MSG_HASH(
   MSG_MEMORY,
   "Muisti"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
   "Syötteen toiston elokuva tiedosto ei ole kelvollinen BSV1 tiedosto."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Syötteen toiston elokuvan formaatti näytää olevan eri sarjaversio. Epäonnistuu todennäköisimmin."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "Syötteen toiston elokuvan toistaminen päättyi."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Pysäytetään elokuvan nauhoitus."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Verkkopelin alustaminen epäonnistui."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Ei sisältöä, käynnistetään tyhjä ydin."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Pelitila tallennusta ei ole vielä korvattu."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Pelitila tallennusta ei ole vielä ladattu."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Virhe tallentaessa ohituksia."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Ohitukset tallennettu onnistuneesti."
   )
MSG_HASH(
   MSG_PAUSED,
   "Keskeytetty."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Luetaan ensimmäistä raitaa..."
   )
MSG_HASH(
   MSG_RECEIVED,
   "vastaanotettu"
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Nauhoitus keskeytettiin koon muuttamisen vuoksi."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Nauhoitetaan kohteeseen"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Uudelleenohjataan huijaustiedostoa kohteeseen"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Uudelleenohjataan tallennus tiedostoa kohteeseen"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Uudelleenohjataan pelitila tallennusta kohteeseen"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Uudelleenmääritys tiedosto tallennettu onnistuneesti."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Uudelleenmääritys tiedosto poistettu onnistuneesti."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Levy poistettiin asemasta."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Poistetaan väliaikainen sisältö tiedosto"
   )
MSG_HASH(
   MSG_RESET,
   "Nollaa"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Uudelleen käynnistetään nauhoitus ajurin käynnistyksen takia."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Palautettiin vanha pelitila."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Varjostimet: palautetaan varjostimen oletusasetukset"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Palautetaan tallenus tiedoston kansio"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Palautetaan pelitila tallennus kansioon"
   )
MSG_HASH(
   MSG_REWINDING,
   "Kelataan taakse."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Alustetaan takaisinkelaus puskuria kokoon"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Takaisinkelaus puskurin alustus epäonnistui. Takaisinkelaus poistetaan käytöstä."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "Toteutus käyttää säikeitettyä ääntä. Ei voi käyttää uudelleenkelausta."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Saavutettiin takaisinkelauksen puskurin loppu."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Uusi asetus tallennettu kansioon"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Pelitila tallennettu lohkoon #%d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Pelitila tallennettu lohkoon #-1 (automaattisesti)."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Tallennettu onnistuneesti kansioon"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Tallenus muistin tyyppi"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Tallennetaan pelitilaa"
   )
MSG_HASH(
   MSG_SCANNING,
   "Skannataan"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Kansion skannaus valmistui"
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Lähetetään komento"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Monet päivitykset on erikseen määritelty, sivuutetaan kaikki..."
   )
MSG_HASH(
   MSG_SHADER,
   "Varjostin"
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Varjostimen esiasetus tallennettu onnistuneesti."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Ohitetaan SRAM lataus."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Hidastus."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Pikakelaus."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Hidastettu takaisinkelaus."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "SRAM:ia ei tallenneta."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Käynnistetään elokuvan toisto."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Aloitetaan elokuvan nauhoitus kohteeseen"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Tilan koko"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Pelitilanteen lohko"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Otetaan kuvakaappaus."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Kuvakaappaus tallennettu"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Saavutus avattu"
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Vaihda esikatselukuvan tyyppi"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Koko näytön esikatselukuvat"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Vaihda metatieto"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Esikatselukuvaa ei ole saatavilla"
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Paina uudelleen lopettaaksesi..."
   )
MSG_HASH(
   MSG_TO,
   "kohteeseen"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Peruttiin pelitilan lataus."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Perutaan pelitilan tallentaminen"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Tuntematon"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Jatkuu."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Tunnistamaton komento"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Käytetään ytimen nimeä uudelle kokoonpanolle."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Käytetään tyhjää libretro ydintä. Ohitetaan nauhoitus."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Yhdistä laite kelvollisesta portista."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Irroitetaan laite portista"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Käynnistetään uudelleen..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Sammutetaan..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "libretro-API:n versio"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Ikkunan koon laskenta epäonnistui! jatketaan raakadatan käyttöä. Tämä ei varmaan toimi oikein..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY,
   " virtuaalinen levy."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "poistamaan"
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "sulje"
   )
MSG_HASH(
   MSG_FAILED,
   "epäonnistui"
   )
MSG_HASH(
   MSG_SUCCEEDED,
   "onnistui"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "ei määritelty"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "ei määritelty, palataan"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Bluetooth-skannaus valmistui."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Wi-Fi-skannaus valmistui."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Skannataan bluetooth-laitteita..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Skannataan langattomia verkkoja..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Otetaan Wi-Fi käyttöön..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Poistetaan Wi-Fi käytöstä..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Katkaistaan Wi-Fi..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Skannataan verkkopelin isäntiä..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Valmistellaan sisällön skannausta..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Anna salasana"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Salasana oikein."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Salasana väärin."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Anna salasana"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Salasana oikein."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Salasana väärin."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "Kokoonpanon ohitus ladattu."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Pelin uudelleenmääritys tiedosto ladattu."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Sisällön kansion uudelleenmääritys tiedosto ladattu."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Ytimen uudelleenmääritys tiedosto ladattu."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Edelläpyöritys käytössä. Viive kuvat poistettu: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Edelläpyöritys käytössä toisella instanssilla. Viive kuvat poistettu: %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Edelläpyöritys pois käytöstä."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Edelläpyöritys on poistettu käytöstä, koska tämä ydin ei tue pelitila tallennuksia."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Pelitilan tallennus epäonnistui: Edelläpyöritys on poistettu käytöstä."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Pelitilan lataaminen epäonnistui: Edelläpyöritys on poistettu käytöstä."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Toisen instanssin luominen epäonnistui. Edelläpyöritys käyttää nyt vain yhtä instanssia."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Tiedoston skannaus valmistui"
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Huijaushaku käynnistetty onnistuneesti"
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Huijaushaun käynnistäminen epäonnistui"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "Etsintään ei ole aloitettu"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Uusien osumien määrä = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "Lisätty %u osumaa"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Vastaavuuksien lisääminen epäonnistui"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Luotiin koodi vastaavuudesta"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Koodin luominen epäonnistui"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Poista vastaavuus"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Ei tarpeeksi tilaa. Enimmäismäärä samanaikaisia huijauksia on 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Uusi huijaus lisätty listan alkuun."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Uusi huijaus lisätty listan loppuun."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Kaikki huijaukset poistettu."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Uusi huijaus lisätty ennen tätä."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Uusi huijaus lisätty tämän jälkeen."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Huijaus kopioitu ennen tätä."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Huijata kopioitu tämän jälkeen."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Huijaus poistettu."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "Asetettu porttiin:"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Levyn asettaminen epäonnistui"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Viimeksi käytetyn levyn asettaminen epäonnistui..."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Pelitilatalennus ladattu. Saavutusten hardcore tila pois käytöstä nykyisessä istunnossa."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Huijaus aktivoitu. Saavutusten hardcore-tila pois käytöstä nykyisessä istunnossa."
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "Alin"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Matalampi"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "Normaali"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Korkeampi"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "Korkein"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "Varoitus: Puuttuvat resurssit, käytä verkkopäivitintä, jos se on saatavilla"
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Varoitus: Puutteelliset fontit valitulle kielelle, käytä verkkopäivitintä, jos se on saatavilla"
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Varoitus: Kieltä ei tueta - käytetään englantia"
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Kopioidaan levyä..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Asema %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Lataa ensin ydin."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Levyasemalta luku epäonnistui. Kopiointi keskeytetty."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Ei voitu kirjoittaa levylle. Kopiointi keskeytetty."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Asemassa ei ole levyä."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Varjostimen esiasetus poistettu onnistuneesti."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Virhe poistettaessa varjostimen esiasetusta."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Virheellinen arcade DAT tiedosto valittu"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "Valittu arcade DAT tiedosto on liian suuri (riittämätön vapaa muisti)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Arcade DAT tiedoston lataaminen epäonnistui (virheellinen muoto?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Manuaalisen skannauksen määritys ei kelpaa"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Kelvollista sisältöä ei havaittu"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Skannataan sisältöä: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Skannataan: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Siivotaan M3U kohteita: "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Skannaus valmistui: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Skannataan ydin: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "Asennetusta ytimestä on jo tehty varmuuskopio: "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Varmuuskopioidaan ydin: "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Poistetaan vanhentuneet varmuuskopiot: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Ytimen varmuuskopiointi valmistui: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "Valitun ytimen varmuuskopio on jo asennettu: "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Palautetaan ydin: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Ytimen palautus valmistui: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "Valittu ydintiedosto on jo asennettu: "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Asennetaan ydin: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Ytimen asennus valmistui: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "Virheellinen ydintiedosto valittu: "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Ytimen varmuuskopiointi epäonnistui: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Ytimen palautus epäonnistui: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Ytimen asennus epäonnistui: "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Ytimen palauttaminen pois käytöstä - ydin on lukittu: "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "Ytimen asennus poistettu käytöstä - ydin on lukittu: "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Ytimen lukitseminen epäonnistui: "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Ytimen lukituksen avaaminen epäonnistui: "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "Ytimen poisto pois käytöstä - ydin on lukittu: "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Videotila ei ole tuettu"
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Päivitä Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Edustan nimi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Lakka-versio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Käynnistä uudelleen"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Jaa Joy-Con"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INTERNAL_STORAGE_STATUS,
   "Sisäisen tallennustilan tilanne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Grafiikka widgettien Skaalauksen ohitus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Käytä manuaalista skaalauskertoimen ohitusta, kun piirretään näyttö widgettejä. Käytössä vain, jos 'skaalaa grafiikkaa widgettejä automaattisesti' on pois päältä. Voidaan käyttää koristettujen ilmoitusten, indikaattorien ja hallintalaitteiden koon kasvattamiseen tai pienentämiseen riippumatta itse valikosta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Näytön resoluutio"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Sammuta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Salli ulkoisten tiedostojen käyttöoikeus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Avaa Windowsin tiedostojen käyttöoikeusasetukset"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Avaa..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Avaa toinen kansio käyttäen järjestelmän tiedostovalitsinta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Vilkkumis suodatin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Videon gamma"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Pehmeä suodatin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Etsi bluetooth laitteita ja yhdistä ne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS,
   "Wifi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Etsi langattomia verkkoja ja luo yhteys."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Ota Wi-Fi käyttöön"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Yhdistä verkkoon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Yhdistä verkkoon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Katkaise yhteys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Vilkkumisen poistaja"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "Aseta Vi näytön leveys"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Yliskannauksen korjaus (ylä)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Säädä näytön yliskannauksen rajausta pienentämällä kuvan kokoa määritellyllä lukumäärällä skannausrivejä (ylhäältä alaspäin). Saattaa aiheuttaa skannaus häiriöitä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Yliskannauksen korjaus (ala)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Säädä näytön yliskannauksen rajausta pienentämällä kuvan kokoa määritellyllä lukumäärällä skannausrivejä (alhaalta ylöspäin). Saattaa aiheuttaa skannaus häiriöitä."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Pitkäkestoinen suorituskyky tila"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Käytä PAL60-tilaa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Käynnistä RetroArch uudelleen"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Poistu ja käynnistä sitten RetroArch uudelleen. Tarvitaan tiettyjen valikkoasetusten aktivoimiseksi (esim. valikkoohjaimen vaihtaminen)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Estä kuvissa"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Suosi kosketusta"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Kosketus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Näppäimistö ohjaimen kartoitus"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Näppäimistö ohjaimen kartoitus tyyppi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Pieni näppäimistö"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Syötteen lohkon aikakatkaisin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Millisekuntien määrä saadaksesi kokonaisen syötteen painalluksen. Käytä tätä, jos sinulla on ongelmia samanaikaisten painallusten kanssa (vain Android)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Näytä 'käynnistä uudelleen'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Näytä 'käynnistä uudelleen' valinta."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Näytä 'sammuta'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Näytä 'sammuta' valinta."
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "Verkko (välityspalvelin)"
   )
MSG_HASH(
   MSG_LOCAL,
   "Paikallinen"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Luku/kirjoitus"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Vain luku"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Näytön kirkkaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Lisää tai vähennä näytön kirkkautta."
   )

#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
   "Näytönohjaimen ylikellotus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE,
   "Ylikellota tai alikellota Switchin näytönohjain."
   )
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "Suorittimen ylikellotus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Ylikellota Switchin prosessori."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Määritä bluetoothin tila."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES,
   "Palvelut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Hallinnoi käyttöjärjestelmän tason palveluita."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Jaa verkkokansiot SMB-protokollan kautta."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Käytä SSH:ta komentorivin etäkäyttöön."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Wifi-yhteyspiste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Ota käyttöön tai poista käytöstä wifi-yhteyspiste."
   )
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Kytketään Wi-Fi yhteyspiste pois päältä."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Katkaistaan yhteys wifi-verkkoon '%s'"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Yhdistetään Wi-Fiin '%s'"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Ei SSID:tä]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Wifi-yhteyspiste on jo käynnistetty"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Wifi-yhteyspiste ei ole käynnissä"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Käynnistetään wifi-yhteyspiste, jonka SSID=%s ja salasana=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Wifi-yhteyspisteen kokoonpanotiedostoa ei voitu luoda."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Väärä kokoonpanotiedosto- APNAME tai PASSWORD ei löydy kohteesta %s"
   )
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Hiiren mittakaava"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Säädä x/y asteikkoa Wiimoten nopeudelle."
   )
#endif
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "RGA-skaalaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "RGA skaalaus ja bicubic suodatus. Voi rikkoa widgettejä."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Kontekstin kohtainen skaalaus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Laitteiston kontekstin skaalaus (jos saatavilla)."
   )
#endif
#if defined(_3DS)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "3DS:n alanäyttö"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Ota käyttöön tilatietojen näyttäminen alareunassa. Poista käytöstä akun käyttöiän pidentämiseksi ja suorituskyvyn parantamiseksi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "3DS:n näyttötila"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Valitsee 3D ja 2D esitystilan väliltä. '3D' tilassa pikselit ovat neliön muotoisia ja syvyyttä tehostetaan pikavalikon katselun yhteydessä. '2D' tila tarjoaa parhaan suorituskyvyn."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (Pixelin ristikko efekti)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (korkea resoluutio)"
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Skannaus Valmis.<br><br>\nJotta sisältö voidaan skannata oikein, sinulla täytyy:\n<ul><li>yhteensopiva ydin olla ladattuna</li>\n<li>\"Ytimen tietotiedostot\" päivitettynä verkkopäivittäjästä</li>\n<li>on \"Tietokanta\" päivitetty verkkopäivittäjästä</li>\n<li>uudelleenkäynnistä RetroArch jos jokin edellä mainituista on juuri tehty</li></ul>\nsisällön on vastattava olemassa olevia tietokantoja <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">täältä</a>. Jos se ei vieläkään toimi, harkitse <a href=\"https://www.github.com/libretro/RetroArch/issues\">vikailmoituksen lähettämistä</a>."
   )
#endif

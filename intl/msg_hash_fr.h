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
   "Menu principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_TAB,
   "Réglages"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES_TAB,
   "Favoris"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_TAB,
   "Historique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MUSIC_TAB,
   "Musique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_TAB,
   "Vidéos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TAB,
   "Jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_TAB,
   "Parcourir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENTLESS_CORES_TAB,
   "Cœurs autonomes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TAB,
   "Importer du contenu"
   )

/* Main Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS,
   "Menu rapide"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SETTINGS,
   "Accéder rapidement à tous les réglages relatifs au jeu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LIST,
   "Charger un cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LIST,
   "Sélectionner le cœur à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST,
   "Charger du contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_LIST,
   "Sélectionner le contenu à démarrer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_DISC,
   "Charger le disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_DISC,
   "Charger un disque média physique. Sélectionnez d'abord le cœur (Charger un cœur) à utiliser avec le disque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMP_DISC,
   "Importer le disque"
   )
MSG_HASH( /* FIXME Is a specific image format used? Is it determined automatically? User choice? */
   MENU_ENUM_SUBLABEL_DUMP_DISC,
   "Importer le disque média physique vers le stockage interne. Il sera sauvegardé vers un fichier d'image disque."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EJECT_DISC,
   "Éjecter le disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_EJECT_DISC,
   "Éjecte le disque du lecteur de CD/DVD physique."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB,
   "Listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLISTS_TAB,
   "Le contenu analysé correspondant à la base de données apparaîtra ici."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST,
   "Importer du contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_CONTENT_LIST,
   "Créer et mettre à jour les listes de lecture en analysant le contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_WIMP,
   "Afficher l'interface de bureau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_WIMP,
   "Ouvrir l'interface de bureau traditionnelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DISABLE_KIOSK_MODE,
   "Désactiver le mode kiosque. (Redémarrage requis)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DISABLE_KIOSK_MODE,
   "Afficher tous les réglages liés à la configuration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER,
   "Mise à jour en ligne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONLINE_UPDATER,
   "Télécharger des modules complémentaires, des composants et du contenu pour RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY,
   "Jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY,
   "Rejoindre ou héberger une session de jeu en réseau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS,
   "Réglages"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS,
   "Configure l'application."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION_LIST,
   "Informations"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION_LIST_LIST,
   "Affiche les informations du système."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS_LIST,
   "Fichiers de configuration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATIONS_LIST,
   "Gérer et créer les fichiers de configuration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LIST,
   "Aide"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_LIST,
   "En savoir plus sur le fonctionnement du programme."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH,
   "Redémarrer RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_RETROARCH,
   "Redémarrer le programme."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH,
   "Quitter RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_RETROARCH,
   "Quitter le programme."
   )

/* Main Menu > Load Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE,
   "Télécharger un cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE,
   "Télécharger et installer un cœur depuis la mise à jour en ligne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_LIST,
   "Installer ou restaurer un cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SIDELOAD_CORE_LIST,
   "Installer ou restaurer un cœur depuis le dossier de téléchargements."
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR,
   "Démarrer le processeur vidéo"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD,
   "Démarrer la RetroManette à distance"
   )

/* Main Menu > Load Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FAVORITES,
   "Dossier de démarrage"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOADED_FILE_DETECT_CORE_LIST,
   "Téléchargements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OPEN_ARCHIVE,
   "Parcourir l'archive"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_ARCHIVE,
   "Charger l'archive"
   )

/* Main Menu > Load Content > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_FAVORITES,
   "Favoris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_FAVORITES,
   "Le contenu ajouté aux 'Favoris' apparaîtra ici."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_MUSIC,
   "Musique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_MUSIC,
   "La musique précédemment jouée apparaîtra ici."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_IMAGES,
   "Les images visionnées précédemment apparaîtront ici."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_VIDEO,
   "Vidéos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_VIDEO,
   "Les vidéos précédemment lues apparaîtront ici."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_EXPLORE,
   "Parcourir"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_EXPLORE,
   "Parcourir tout le contenu correspondant à la base de données via une interface de recherche catégorisée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GOTO_CONTENTLESS_CORES,
   "Cœurs autonomes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GOTO_CONTENTLESS_CORES,
   "Les cœurs installés qui peuvent fonctionner sans charger de contenu apparaîtront ici."
   )

/* Main Menu > Online Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST,
   "Téléchargement de cœurs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_INSTALLED_CORES,
   "Mettre à jour les cœurs installés"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UPDATE_INSTALLED_CORES,
   "Met à jour les cœurs installés vers la dernière version disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_INSTALLED_CORES_PFD,
   "Remplacer les cœurs par les versions du Play Store"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_INSTALLED_CORES_PFD,
   "Remplace tous les cœurs existants et installés manuellement par les dernières versions depuis le Play Store, si disponibles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_UPDATER_LIST,
   "Mise à jour des miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_UPDATER_LIST,
   "Télécharge le pack de miniatures complet pour le système sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PL_THUMBNAILS_UPDATER_LIST,
   "Mise à jour des miniatures pour la liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PL_THUMBNAILS_UPDATER_LIST,
   "Télécharge des miniatures pour les entrées dans la liste de lecture sélectionnée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_CONTENT,
   "Téléchargement de contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_CORE_SYSTEM_FILES,
   "Téléchargement des fichiers système de cœurs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_CORE_SYSTEM_FILES,
   "Télécharger les fichiers système auxiliaires nécessaires pour un fonctionnement correct et optimal des cœurs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CORE_INFO_FILES,
   "Mettre à jour les fichiers d'information de cœurs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_ASSETS,
   "Mettre à jour les assets"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_AUTOCONFIG_PROFILES,
   "Mettre à jour les profils de manettes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CHEATS,
   "Mettre à jour les cheats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_DATABASES,
   "Mettre à jour les bases de données"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_OVERLAYS,
   "Mettre à jour les surimpressions"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_GLSL_SHADERS,
   "Mettre à jour les shaders GLSL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_CG_SHADERS,
   "Mettre à jour les shaders CG"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_SLANG_SHADERS,
   "Mettre à jour les shaders Slang"
   )

/* Main Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFORMATION,
   "Informations sur le cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFORMATION,
   "Affiche les informations relatives à l'application/au cœur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISC_INFORMATION,
   "Informations sur le disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISC_INFORMATION,
   "Voir des informations sur les disques média insérés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_INFORMATION,
   "Informations réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_INFORMATION,
   "Voir la ou les interfaces réseau et les adresses IP associées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFORMATION,
   "Informations système"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_INFORMATION,
   "Voir les informations spécifiques à l'appareil."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_MANAGER,
   "Gestionnaire de base de données"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DATABASE_MANAGER,
   "Affiche les bases de données."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_MANAGER,
   "Gestionnaire de pointeurs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_MANAGER,
   "Affiche les recherches précédentes."
   )

/* Main Menu > Information > Core Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_NAME,
   "Nom du cœur "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CORE_LABEL,
   "Appellation du cœur "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_NAME,
   "Nom du système "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SYSTEM_MANUFACTURER,
   "Fabricant du système "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CATEGORIES,
   "Catégorie "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_AUTHORS,
   "Auteurs "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_PERMISSIONS,
   "Permissions "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_LICENSES,
   "Licence "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SUPPORTED_EXTENSIONS,
   "Extensions prises en charge "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_REQUIRED_HW_API,
   "API de graphismes requises "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SUPPORT_LEVEL,
   "Prise en charge des sauvegardes instantanées"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DISABLED,
   "Ne pas partager"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_BASIC,
   "Basique (sauvegarde/chargement)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_SERIALIZED,
   "Sérialisé (sauvegarde/chargement, rembobinage)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_SAVESTATE_DETERMINISTIC,
   "Déterministe (sauvegarde/chargement, rembobinage, exécution en avance, jeu en réseau)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_FIRMWARE,
   "Firmware(s) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_REQUIRED,
   "Manquant, requis :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MISSING_OPTIONAL,
   "Manquant, optionnel :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_REQUIRED,
   "Présent, requis :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRESENT_OPTIONAL,
   "Présent, optionnel :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_LOCK,
   "Verrouiller le cœur installé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_LOCK,
   "Empêche la modification du cœur actuellement installé. Peut être utilisé pour éviter les mises à jour indésirables lorsque le contenu nécessite une version spécifique (les sets de ROMs d'arcade par exemple)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE,
   "Supprimer le cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE,
   "Retirer ce cœur du disque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_CREATE_BACKUP,
   "Sauvegarder le cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CREATE_BACKUP,
   "Crée une sauvegarde archivée du cœur installé actuellement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_RESTORE_BACKUP_LIST,
   "Restaurer une sauvegarde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_RESTORE_BACKUP_LIST,
   "Installe une version précédente du cœur à partir d'une liste de sauvegardes archivées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_DELETE_BACKUP_LIST,
   "Supprimer une sauvegarde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_DELETE_BACKUP_LIST,
   "Supprime un fichier de la liste des sauvegardes archivées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_BACKUP_CRC,
   "CRC32 : "
   )

/* Main Menu > Information > System Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_BUILD_DATE,
   "Date de compilation "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GIT_VERSION,
   "Version Git "
   )
MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_COMPILER,
   "Compilateur "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_MODEL,
   "Modèle du processeur "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CPU_FEATURES,
   "Fonctionnalités du processeur "
   )
MSG_HASH( /* FIXME Colon should be handled in menu_display.c like the rest */
   MENU_ENUM_LABEL_VALUE_CPU_ARCHITECTURE,
   "Architecture du processeur :"
   )
MSG_HASH( /* FIXME Colon should be handled in menu_display.c like the rest */
   MENU_ENUM_LABEL_VALUE_CPU_CORES,
   "Cœurs du processeur :"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CPU_CORES,
   "Nombre de cœurs dont dispose le processeur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_IDENTIFIER,
   "Identifiant de l'interface "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_OS,
   "Système d'exploitation de l'interface"
   )
MSG_HASH( /* FIXME Maybe add a description? */
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RETRORATING_LEVEL,
   "Niveau de RetroClassification"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE,
   "Alimentation "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VIDEO_CONTEXT_DRIVER,
   "Pilote de contexte vidéo "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_WIDTH,
   "Largeur de l'écran (mm) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_MM_HEIGHT,
   "Hauteur de l'écran (mm) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DISPLAY_METRIC_DPI,
   "Points/pouce de l'écran "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBRETRODB_SUPPORT,
   "Prise en charge de LibretroDB "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OVERLAY_SUPPORT,
   "Prise en charge des surimpressions "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COMMAND_IFACE_SUPPORT,
   "Prise en charge de l'interface de commandes "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_COMMAND_IFACE_SUPPORT,
   "Prise en charge de l'interface de commandes réseau "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETWORK_REMOTE_SUPPORT,
   "Prise en charge de manette en réseau "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COCOA_SUPPORT,
   "Prise en charge de Cocoa "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RPNG_SUPPORT,
   "Prise en charge du format PNG (RPNG) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RJPEG_SUPPORT,
   "Prise en charge du format JPEG (RJPEG) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RBMP_SUPPORT,
   "Prise en charge du format BMP (RBMP) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RTGA_SUPPORT,
   "Prise en charge du format TGA (RTGA) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_SUPPORT,
   "Prise en charge de SDL 1.2 "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL2_SUPPORT,
   "Prise en charge de SDL 2 "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_VULKAN_SUPPORT,
   "Prise en charge de Vulkan "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_METAL_SUPPORT,
   "Prise en charge de Metal "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGL_SUPPORT,
   "Prise en charge d'OpenGL "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENGLES_SUPPORT,
   "Prise en charge d'OpenGL ES "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_THREADING_SUPPORT,
   "Prise en charge de plusieurs fils d'exécution "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_KMS_SUPPORT,
   "Prise en charge de KMS/EGL "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_UDEV_SUPPORT,
   "Prise en charge de udev "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENVG_SUPPORT,
   "Prise en charge d'OpenVG "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_EGL_SUPPORT,
   "Prise en charge d'EGL "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_X11_SUPPORT,
   "Prise en charge de X11 "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WAYLAND_SUPPORT,
   "Prise en charge de Wayland "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XVIDEO_SUPPORT,
   "Prise en charge de XVideo "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ALSA_SUPPORT,
   "Prise en charge d'ALSA "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OSS_SUPPORT,
   "Prise en charge d'OSS "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENAL_SUPPORT,
   "Prise en charge d'OpenAL "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_OPENSL_SUPPORT,
   "Prise en charge d'OpenSL "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_RSOUND_SUPPORT,
   "Prise en charge de RSound "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ROARAUDIO_SUPPORT,
   "Prise en charge de RoarAudio "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_JACK_SUPPORT,
   "Prise en charge de JACK "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_PULSEAUDIO_SUPPORT,
   "Prise en charge de PulseAudio "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO_SUPPORT,
   "Prise en charge de CoreAudio "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_COREAUDIO3_SUPPORT,
   "Prise en charge de CoreAudio V3 "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DSOUND_SUPPORT,
   "Prise en charge de DirectSound "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_WASAPI_SUPPORT,
   "Prise en charge de WASAPI "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_XAUDIO2_SUPPORT,
   "Prise en charge de XAudio2 "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_ZLIB_SUPPORT,
   "Prise en charge de zlib "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_7ZIP_SUPPORT,
   "Prise en charge de 7zip "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYLIB_SUPPORT,
   "Prise en charge des bibliothèques dynamiques "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_DYNAMIC_SUPPORT,
   "Prise en charge du chargement dynamique des bibliothèques "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CG_SUPPORT,
   "Prise en charge de Cg "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_GLSL_SUPPORT,
   "Prise en charge de GLSL "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_HLSL_SUPPORT,
   "Prise en charge de HLSL "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SDL_IMAGE_SUPPORT,
   "Prise en charge de SDL image "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FFMPEG_SUPPORT,
   "Prise en charge de FFmpeg "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_MPV_SUPPORT,
   "Prise en charge de mpv "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_CORETEXT_SUPPORT,
   "Prise en charge de CoreText "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FREETYPE_SUPPORT,
   "Prise en charge de FreeType "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_STB_TRUETYPE_SUPPORT,
   "Prise en charge du rendu des polices STB TrueType "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_NETPLAY_SUPPORT,
   "Prise en charge du jeu en réseau (peer-to-peer) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_V4L2_SUPPORT,
   "Prise en charge de Video4Linux2 "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LIBUSB_SUPPORT,
   "Prise en charge de Libusb "
   )

/* Main Menu > Information > Database Manager */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_SELECTION,
   "Sélection de base de données"
   )

/* Main Menu > Information > Database Manager > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_NAME,
   "Nom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ACHIEVEMENTS,
   "Succès"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_LANGUAGE,
   "Langue "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_REGION,
   "Région"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CONTROLS,
   "Touches"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PUBLISHER,
   "Éditeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DEVELOPER,
   "Développeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ORIGIN,
   "Origine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FRANCHISE,
   "Saga"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_TGDB_RATING,
   "Classification TGDB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_FAMITSU_MAGAZINE_RATING,
   "Classification du magazine Famitsu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_REVIEW,
   "Critique du magazine Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_RATING,
   "Classification du magazine Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Numéro du magazine Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_MONTH,
   "Mois de sortie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RELEASE_YEAR,
   "Année de sortie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_BBFC_RATING,
   "Classification BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ESRB_RATING,
   "Classification ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ELSPA_RATING,
   "Classification ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_PEGI_RATING,
   "Classification PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ENHANCEMENT_HW,
   "Améliorations matérielles"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_CERO_RATING,
   "Classification CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_SERIAL,
   "Numéro de série"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_ANALOG,
   "Analogique pris en charge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_RUMBLE,
   "Vibration prise en charge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_COOP,
   "Coopératif pris en charge"
   )

/* Main Menu > Configuration File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIGURATIONS,
   "Charger une configuration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_TO_DEFAULT_CONFIG,
   "Réinitialiser aux valeurs par défaut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESET_TO_DEFAULT_CONFIG,
   "Réinitialiser la configuration actuelle aux valeurs par défaut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG,
   "Sauvegarder la configuration actuelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG,
   "Sauvegarder une nouvelle configuration"
   )

/* Main Menu > Help */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CONTROLS,
   "Contrôles de base du menu"
   )

/* Main Menu > Help > Basic Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP,
   "Faire défiler vers le haut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN,
   "Faire défiler vers le bas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM,
   "Confirmer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START,
   "Démarrer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU,
   "Afficher/masquer le menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT,
   "Quitter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD,
   "Afficher/masquer le clavier"
   )

/* Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS,
   "Pilotes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SETTINGS,
   "Modifier les pilotes utilisés par ce système."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS,
   "Vidéo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SETTINGS,
   "Modifier les réglages de sortie vidéo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS,
   "Audio "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SETTINGS,
   "Modifier les réglages de sortie audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS,
   "Entrées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SETTINGS,
   "Modifier les réglages de manettes, clavier et souris."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LATENCY_SETTINGS,
   "Latence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LATENCY_SETTINGS,
   "Modifier les réglages liés à la latence vidéo, audio et d'entrées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SETTINGS,
   "Cœurs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_SETTINGS,
   "Modifier les réglages du cœur."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIGURATION_SETTINGS,
   "Modifier les réglages par défaut pour les fichiers de configuration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS,
   "Sauvegarde"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVING_SETTINGS,
   "Modifier les réglages de sauvegarde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS,
   "Journalisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOGGING_SETTINGS,
   "Modifier les réglages de journalisation."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS,
   "Navigateur de fichiers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FILE_BROWSER_SETTINGS,
   "Modifier les réglages du navigateur de fichiers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS,
   "Limiteur d'images/s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_THROTTLE_SETTINGS,
   "Modifier les réglages pour le rembobinage, l'avance rapide et le ralenti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS,
   "Enregistrement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_SETTINGS,
   "Modifier les réglages d'enregistrement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS,
   "Affichage à l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_DISPLAY_SETTINGS,
   "Modifier les réglages de surimpression à l'écran, de surimpression de clavier, et de notifications à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS,
   "Interface utilisateur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_INTERFACE_SETTINGS,
   "Modifier les réglages de l'interface utilisateur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SETTINGS,
   "Service IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SETTINGS,
   "Modifier les réglages du service IA (Traduction/TTS/autres)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_SETTINGS,
   "Accessibilité"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_SETTINGS,
   "Modifier les réglages du narrateur d'accessibilité."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POWER_MANAGEMENT_SETTINGS,
   "Gestion de l'alimentation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POWER_MANAGEMENT_SETTINGS,
   "Modifier les réglages de gestion de l'alimentation."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS,
   "Succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RETRO_ACHIEVEMENTS_SETTINGS,
   "Modifier les réglages des succès."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS,
   "Réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_SETTINGS,
   "Modifier les réglages de serveur et de réseau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS,
   "Listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SETTINGS,
   "Modifier les réglages de listes de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_SETTINGS,
   "Utilisateur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_SETTINGS,
   "Modifier les réglages de compte, de pseudo et de langue."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS,
   "Dossiers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DIRECTORY_SETTINGS,
   "Modifier les dossiers où les fichiers sont stockés par défaut."
   )

/* Settings > Drivers */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DRIVER,
   "Entrées "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DRIVER,
   "Pilote d'entrées à utiliser. Certains pilotes vidéo forcent l'utilisation d'un pilote d'entrées différent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER,
   "Manettes "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_DRIVER,
   "Pilote de manettes à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER,
   "Vidéo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DRIVER,
   "Pilote vidéo à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER,
   "Audio "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DRIVER,
   "Pilote audio à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER,
   "Ré-échantillonneur audio "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_DRIVER,
   "Pilote de rééchantillonnage audio à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER,
   "Appareil photo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_DRIVER,
   "Pilote d'appareil photo à utiliser."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_DRIVER,
   "Pilote Bluetooth à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DRIVER,
   "Wi-Fi "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_DRIVER,
   "Pilote Wi-Fi à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER,
   "Géolocalisation "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_DRIVER,
   "Pilote de localisation à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_DRIVER,
   "Menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_DRIVER,
   "Pilote de menu à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_DRIVER,
   "Enregistrement "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORD_DRIVER,
   "Pilote d'enregistrement à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_DRIVER,
   "MIDI "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_DRIVER,
   "Pilote MIDI à utiliser."
   )

/* Settings > Video */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCHRES_SETTINGS,
   "Résolution adaptée aux écrans CRT "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCHRES_SETTINGS,
   "Produit des signaux natifs de faible résolution pour une utilisation avec les écrans à tube cathodique (CRT)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OUTPUT_SETTINGS,
   "Sortie vidéo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OUTPUT_SETTINGS,
   "Modifier les réglages de sortie vidéo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Mode plein écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_MODE_SETTINGS,
   "Modifier les réglages du mode plein écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_MODE_SETTINGS,
   "Mode fenêtré"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_MODE_SETTINGS,
   "Modifier les réglages du mode fenêtré."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALING_SETTINGS,
   "Mise à l'échelle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALING_SETTINGS,
   "Modifier les réglages de mise à l'échelle vidéo."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_SETTINGS,
   "Modifier les réglages de sortie vidéo HDR."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Synchronisation vidéo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SYNCHRONIZATION_SETTINGS,
   "Modifier les réglages de synchronisation vidéo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE,
   "Suspendre l'économiseur d'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE,
   "Empêcher l'économiseur d'écran de votre système de s'activer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_THREADED,
   "Vidéo sur plusieurs fils d'exécution"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_THREADED,
   "Améliore la performance au détriment d'une latence et de saccades visuelles accrues. À n'utiliser que si vous avez des ralentissements en jeu sans cette option."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION,
   "Insertion d'images noires"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_BLACK_FRAME_INSERTION,
   "Insérer une image noire entre les images. Utile pour certains écrans à fréquence de rafraîchissement élevée pour éliminer la rémanence."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT,
   "Utiliser le processeur graphique pour les captures d'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_SCREENSHOT,
   "Les captures d'écran capturent les shaders produits par le processeur graphique si disponibles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH,
   "Filtre bilinéaire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SMOOTH,
   "Ajouter un léger flou à l'image pour atténuer le contour des pixels bruts. Cette option a très peu d'impact sur les performances."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Interpolation de l'image"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_FILTER_TYPE,
   "Spécifier la méthode d'interpolation d'image lors de la mise à l'échelle du contenu via le processeur d'images (IPU) interne. 'Bicubique' ou 'Bilinéaire' est recommandé lors de l'utilisation de filtres vidéo alimentés par le processeur. Cette option n'a aucun impact sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BICUBIC,
   "Bicubique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_BILINEAR,
   "Bilinéaire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_FILTER_NEAREST,
   "Au plus proche"
   )
#if defined(RS90) || defined(MIYOO)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Interpolation de l'image"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_RS90_SOFTFILTER_TYPE,
   "Spécifier la méthode d'interpolation d'image lorsque l'option 'Échelle à l'entier' est désactivée. 'Au plus proche' a le moins d'impact sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_POINT,
   "Au plus proche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_RS90_SOFTFILTER_BRESENHAM_HORZ,
   "Semi-linéaire"
   )
#endif
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DELAY,
   "Retarder le chargement des shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DELAY,
   "Retarder le chargement automatique des shaders (en ms). Peut résoudre des problèmes graphiques lors de l'utilisation de logiciels de 'capture d'écran'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER,
   "Filtre vidéo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER,
   "Applique un filtre vidéo produit par le processeur. Peut avoir un coût élevé en performances. Certains filtres vidéo ne peuvent fonctionner qu'avec les cœurs utilisant les modes de couleurs 32-bits ou 16-bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_REMOVE,
   "Retirer le filtre vidéo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_REMOVE,
   "Décharge tout filtre vidéo produit par le processeur actif."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_NOTCH_WRITE_OVER,
   "Activer le dépassement de l'encoche (notch) en plein écran sur les appareils Android"
)

/* Settings > Video > CRT SwitchRes */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION,
   "Résolution adaptée aux écrans CRT "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION,
   "Pour les écrans à tube cathodique (CRT) uniquement. Tente d'utiliser la résolution exacte du cœur/du jeu et sa fréquence de rafraîchissement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_SUPER,
   "Super résolution CRT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_SUPER,
   "Basculer entre les super résolutions natives et ultra-larges (UltraWide)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_X_AXIS_CENTERING,
   "Centrage sur l'axe X "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_X_AXIS_CENTERING,
   "Faire défiler cette valeur si l'image n'est pas centrée correctement à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_PORCH_ADJUST,
   "Ajustement de l'égalisation du balayage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_PORCH_ADJUST,
   "Parcourez ces options pour ajuster les paramètres d'égalisation du balayage afin de modifier la taille de l'image."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_HIRES_MENU,
   "Utiliser le menu haute résolution"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_HIRES_MENU,
   "Passer à une \"modeline\" haute résolution pour utiliser des menus en haute résolution quand aucun contenu n'est chargé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Fréquence de rafraîchissement personnalisée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_USE_CUSTOM_REFRESH_RATE,
   "Utiliser une fréquence de rafraîchissement personnalisée spécifiée dans le fichier de configuration si nécessaire."
   )

/* Settings > Video > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX,
   "Numéro du moniteur "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MONITOR_INDEX,
   "Sélectionner l'écran à utiliser."
   )
#if defined (WIIU)
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION,
   "Rotation vidéo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ROTATION,
   "Force une certaine rotation de la vidéo. La rotation s'ajoute aux rotations définies par le cœur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_ORIENTATION,
   "Orientation de l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_ORIENTATION,
   "Force une certaine orientation de l'écran à partir du système d'exploitation."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_INDEX,
   "Numéro du processeur graphique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_INDEX,
   "Sélectionner la carte graphique à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_X,
   "Décalage horizontal de l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_X,
   "Force un certain décalage horizontalement à la vidéo. Le décalage est appliqué globalement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OFFSET_Y,
   "Décalage vertical de l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_OFFSET_Y,
   "Force un certain décalage verticalement à la vidéo. Le décalage est appliqué globalement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE,
   "Fréquence de rafraîchissement vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE,
   "Renseigne la fréquence de rafraîchissement vertical actuelle de votre écran. Elle sera utilisée pour calculer un débit audio approprié.\nCette option sera ignorée si 'Vidéo sur plusieurs fils d'exécution' est activé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO,
   "Fréquence estimée de l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_AUTO,
   "Fréquence de rafraîchissement précise estimée pour l'écran en Hz."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_POLLED,
   "Définir la fréquence de rafraîchissement annoncée par l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_REFRESH_RATE_POLLED,
   "Fréquence de rafraîchissement détectée par le pilote d'affichage."
   )
#if defined(DINGUX) && defined(DINGUX_BETA)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_REFRESH_RATE,
   "Fréquence de rafraîchissement vertical"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_REFRESH_RATE,
   "Définir la fréquence de rafraîchissement vertical de l'écran. '50 Hz' sera fluide pour le contenu PAL."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE,
   "Forcer la désactivation du mode sRGB FBO"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_SRGB_DISABLE,
   "Forcer la désactivation de la prise en charge du mode sRGB FBO. Certains pilotes OpenGL d'Intel sous Windows rencontrent des problèmes vidéo avec le sRGB FBO. Activer cette option permet de contourner ce problème."
   )

/* Settings > Video > Fullscreen Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN,
   "Démarrer en mode plein écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN,
   "Démarrer en mode plein écran. Peut être changé lors de l'exécution, et peut être remplacé par une option en ligne de commande."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN,
   "Mode plein écran fenêtré"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOWED_FULLSCREEN,
   "En mode plein écran, utiliser plutôt une fenêtre en plein écran pour empêcher les changements de mode d'affichage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_X,
   "Largeur en plein écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_X,
   "Définir la largeur personnalisée pour le plein écran non fenêtré. La laisser non définie utilisera la résolution du bureau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN_Y,
   "Hauteur en plein écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FULLSCREEN_Y,
   "Définir la hauteur personnalisée pour le plein écran non fenêtré. La laisser non définie utilisera la résolution du bureau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_RESOLUTION,
   "Forcer la résolution sous UWP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FORCE_RESOLUTION,
   "Force la résolution aux dimensions en plein écran, si réglée à 0, une valeur fixe de 3840 x 2160 sera utilisée."
   )

/* Settings > Video > Windowed Mode */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE,
   "Échelle en mode fenêtré"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SCALE,
   "Définit la taille de la fenêtre à un multiple spécifié de la taille de la fenêtre principale."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_OPACITY,
   "Opacité de la fenêtre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SHOW_DECORATIONS,
   "Afficher les décorations de fenêtre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_SAVE_POSITION,
   "Restaurer l'aspect de la fenêtre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_SAVE_POSITION,
   "Affiche tout le contenu dans une fenêtre à taille fixe aux dimensions spécifiées par 'Largeur de fenêtre' et 'Hauteur de fenêtre', et enregistrer la taille et la position de la fenêtre courante lors de la fermeture de RetroArch. Lorsque cette option est désactivée, la taille de la fenêtre est définie dynamiquement en fonction de 'Échelle en mode fenêtré'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Utiliser une taille de fenêtre personnalisée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_CUSTOM_SIZE_ENABLE,
   "Affiche tout le contenu dans une fenêtre à taille fixe aux dimensions spécifiées par 'Largeur de fenêtre' et 'Hauteur de fenêtre'. Lorsque cette option est désactivée, la taille de la fenêtre est définie dynamiquement en fonction de 'Échelle en mode fenêtré'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_WIDTH,
   "Largeur de fenêtre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_WIDTH,
   "Définir une largeur personnalisée pour la fenêtre d'affichage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_HEIGHT,
   "Hauteur de fenêtre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_HEIGHT,
   "Définir une hauteur personnalisée pour la fenêtre d'affichage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Largeur de fenêtre maximale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_WIDTH_MAX,
   "Définit la largeur maximale de la fenêtre d'affichage lors du redimensionnement automatique basée sur 'Échelle en mode fenêtré'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Hauteur de fenêtre maximale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_WINDOW_AUTO_HEIGHT_MAX,
   "Définit la hauteur maximale de la fenêtre d'affichage lors du redimensionnement automatique basée sur 'Échelle en mode fenêtré'."
   )

/* Settings > Video > Scaling */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER,
   "Échelle à l'entier"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER,
   "Mettre la vidéo à l'échelle sur un nombre entier uniquement. La taille de base dépend de la géométrie et du rapport d'aspect détectés par le système. Si 'Forcer le rapport d'aspect' est désactivé, X/Y seront mis à l'échelle à l'entier indépendamment."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER_OVERSCALE,
   "Échelle à l'entier surdimensionnée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SCALE_INTEGER_OVERSCALE,
   "Force la mise à l'échelle sur l'entier à arrondir au prochain entier supérieur plutôt qu'inférieur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX,
   "Rapport d'aspect"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO,
   "Rapport d'aspect personnalisé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ASPECT_RATIO,
   "Valeur en virgule flottante du rapport d'aspect (largeur/hauteur), si 'Rapport d'aspect' est réglé sur 'Configurer'."
   )
#if defined(DINGUX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Conserver le rapport d'aspect"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DINGUX_IPU_KEEP_ASPECT,
   "Maintient les proportions des pixels à 1:1 lors de la mise à l'échelle de contenu via le processeur d'images (IPU) interne. Si désactivé, les images seront étirées pour remplir l'intégralité de l'écran."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X,
   "Position X de l'affichage (Rapport d'aspect personnalisé)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_X,
   "Décalage de la fenêtre d'affichage sur l'axe X.\nCette option sera ignorée si l'option 'Échelle à l'entier' est activée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y,
   "Position Y de l'affichage (Rapport d'aspect personnalisé)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_Y,
   "Décalage de la fenêtre d'affichage sur l'axe Y.\nCette option sera ignorée si l'option 'Échelle à l'entier' est activée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Largeur de l'affichage (Rapport d'aspect personnalisé)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH,
   "Largeur de la fenêtre d'affichage si le rapport d'aspect est réglé sur 'Rapport d'aspect personnalisé'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Hauteur de l'affichage (Rapport d'aspect personnalisé)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT,
   "Hauteur de la fenêtre d'affichage si le rapport d'aspect est réglé sur 'Rapport d'aspect personnalisé'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN,
   "Rogner le surbalayage (Redémarrage requis)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CROP_OVERSCAN,
   "Tronquer quelques pixels sur les bords de l'image habituellement laissés vides par les développeurs, qui contiennent parfois aussi des pixels parasites."
   )

/* Settings > Video > HDR */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_ENABLE,
   "Activer le HDR"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_ENABLE,
   "Activer le HDR si l'affichage le prend en charge."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_MAX_NITS,
   "Pic de luminance"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_MAX_NITS,
   "Définit le pic de luminance (en cd/m2) que votre écran peut reproduire. Voir \"RTings\" pour le pic de luminosité de votre écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_PAPER_WHITE_NITS,
   "Luminance du \"papier blanc\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_PAPER_WHITE_NITS,
   "Définit la luminance à laquelle le \"papier blanc\" doit être, comme du texte lisible ou la luminance en haut de la gamme SDR (Standard Dynamic Range). Utile pour s'adapter à différentes conditions d'éclairage dans votre environnement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_CONTRAST,
   "Contraste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_CONTRAST,
   "Contrôle du gamma/contraste pour le HDR. Prend les couleurs et augmente la portée globale entre les parties les plus claires et les parties les plus sombres de l'image. Plus le contraste HDR est élevé, plus cette différence devient grande, tandis que plus le contraste est bas, plus l'image perd de la netteté. Aide les utilisateurs à affiner l'image à leur goût et ce qu'ils pensent être le mieux sur leur écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HDR_EXPAND_GAMUT,
   "Étendre le Gamut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HDR_EXPAND_GAMUT,
   "Une fois que l'espace de couleur est converti en espace linéaire, décide si une gamme de couleurs étendue doit être utilisée pour atteindre le HDR10."
   )

/* Settings > Video > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC,
   "Synchronisation verticale (V-Sync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_VSYNC,
   "Synchroniser la sortie vidéo de la carte graphique avec la fréquence de rafraîchissement de l'écran. Recommandé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL,
   "Intervalle d'échange V-Sync"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SWAP_INTERVAL,
   "Utiliser un intervalle d'échange personnalisé pour la synchronisation verticale (V-Sync). Utilisez cette option pour réduire de moitié la fréquence de rafraîchissement du moniteur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ADAPTIVE_VSYNC,
   "Synchronisation verticale (V-Sync) adaptative"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ADAPTIVE_VSYNC,
   "La synchronisation verticale (V-Sync) est activée jusqu'à ce que les performances descendent en dessous de la fréquence de rafraîchissement cible. Peut minimiser les saccades lorsque les performances sont inférieures au temps réel, et être plus économe en énergie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY,
   "Retarder les images "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY,
   "Réduit la latence au détriment d'un risque accru de saccades visuelles. Ajoute un délai après la synchronisation verticale V-Sync (en ms)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY_AUTO,
   "Retard d'images automatique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FRAME_DELAY_AUTO,
   "Diminuer temporairement le 'Retard d'images' pour éviter les futures sautes d'images. Le point de départ est la demi-cadence d'images quand le 'Retard d'images' est 0."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC,
   "Synchronisation matérielle du processeur graphique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC,
   "Synchronisation matérielle du processeur et du processeur graphique. Réduit la latence mais affecte les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES,
   "Images de synchronisation matérielle du processeur graphique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_HARD_SYNC_FRAMES,
   "Définir le nombre d'images que le processeur peut exécuter avant le processeur graphique lors de l'utilisation de la 'Synchronisation matérielle du processeur graphique'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VRR_RUNLOOP_ENABLE,
   "Synchroniser à la fréquence exacte du contenu (G-Sync, FreeSync)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VRR_RUNLOOP_ENABLE,
   "Élimine les déviations par rapport au timing requis par le cœur. Utilisez cette option pour les écrans à fréquence de rafraîchissement variable (G-Sync, FreeSync, HDMI 2.1 VRR)."
   )

/* Settings > Audio */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_SETTINGS,
   "Sortie audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_SETTINGS,
   "Modifier les réglages de la sortie audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_SETTINGS,
   "Rééchantillonneur audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_SETTINGS,
   "Modifier les réglages du rééchantillonneur audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Synchronisation audio"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNCHRONIZATION_SETTINGS,
   "Modifier les réglages de synchronisation audio."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_SETTINGS,
   "Modifier les réglages MIDI."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_SETTINGS,
   "Réglages du mixeur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_SETTINGS,
   "Modifier les réglages du mixeur audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUNDS,
   "Sons du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SOUNDS,
   "Modifier les réglages de son du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MUTE,
   "Muet"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MUTE,
   "Couper le son."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_MUTE,
   "Couper le son du mixeur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_MUTE,
   "Couper le son du mixeur audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FASTFORWARD_MUTE,
   "Couper le son lors de l'avance rapide"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FASTFORWARD_MUTE,
   "Couper automatiquement le son lors de l'avance rapide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME,
   "Gain de volume (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_VOLUME,
   "Volume sonore (en dB). 0 dB correspond au volume normal, et aucun gain n'est appliqué."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MIXER_VOLUME,
   "Gain de volume du mixeur (dB)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MIXER_VOLUME,
   "Volume du mixeur audio global (en dB). 0 dB est le volume normal, et aucun gain n'est appliqué."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN,
   "Module DSP "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN,
   "Filtre audio DSP utilisé pour traiter l'audio avant de l'envoyer au pilote."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN_REMOVE,
   "Retirer le module DSP"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DSP_PLUGIN_REMOVE,
   "Décharge tout module audio DSP actif."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Mode exclusif WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_EXCLUSIVE_MODE,
   "Autoriser le pilote WASAPI à prendre le contrôle exclusif du périphérique audio. Si désactivé, le mode partagé sera utilisé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_FLOAT_FORMAT,
   "Format de virgule flottante WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_FLOAT_FORMAT,
   "Utiliser le format float pour le pilote WASAPI, si pris en charge par votre périphérique audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Taille de la mémoire tampon partagée WASAPI"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_WASAPI_SH_BUFFER_LENGTH,
   "Taille de la mémoire tampon intermédiaire (en images) lors de l'utilisation du pilote WASAPI en mode partagé."
   )

/* Settings > Audio > Output */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE,
   "Son"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE,
   "Activer la sortie audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE,
   "Périphérique "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_DEVICE,
   "Remplacer le périphérique audio utilisé par défaut par le pilote audio. Cette option dépend du pilote."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY,
   "Latence audio (ms) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_LATENCY,
   "Latence audio désirée en millisecondes. Peut être ignorée si le pilote audio ne peut fournir une telle valeur."
   )

/* Settings > Audio > Resampler */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_QUALITY,
   "Qualité du rééchantillonnage "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RESAMPLER_QUALITY,
   "Réduire cette valeur pour favoriser les performances/réduire la latence plutôt que la qualité audio, l'augmenter permet une meilleure qualité audio aux dépens de la performance/d'une latence inférieure."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE,
   "Fréquence de sortie (Hz) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_OUTPUT_RATE,
   "Fréquence d'échantillonnage de la sortie audio."
   )

/* Settings > Audio > Synchronization */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_SYNC,
   "Synchronisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_SYNC,
   "Synchroniser l'audio. Recommandé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW,
   "Limite de synchronisation maximale "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_MAX_TIMING_SKEW,
   "Variation maximale du débit audio. Augmenter cette valeur permet des changements très importants dans le timing en échange d'un pitch audio inexact (lors de l'exécution de cœurs PAL sur des écrans NTSC par exemple)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA,
   "Contrôle dynamique du débit audio "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_RATE_CONTROL_DELTA,
   "Aide à atténuer les imperfections de timing lors de la synchronisation audio et vidéo. Sachez que si désactivé, une synchronisation correcte est presque impossible à obtenir."
   )

/* Settings > Audio > MIDI */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_INPUT,
   "Entrée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_INPUT,
   "Sélectionner le périphérique d'entrée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIDI_OUTPUT,
   "Sortie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_OUTPUT,
   "Sélectionner le périphérique de sortie."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIDI_VOLUME,
   "Régler le volume de sortie (%)."
   )

/* Settings > Audio > Mixer Settings > Mixer Stream */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY,
   "Lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY,
   "Cela lancera la lecture du flux audio. Une fois terminée, le flux audio actuel sera supprimé de la mémoire."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_LOOPED,
   "Lecture (En boucle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_LOOPED,
   "Cela lancera la lecture du flux audio. Une fois terminée, la lecture se relancera depuis le début."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Lecture (Séquentielle)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_PLAY_SEQUENTIAL,
   "Cela lancera la lecture du flux audio. Une fois terminée, la lecture passera au prochain flux audio dans un ordre séquentiel et répétera ce comportement. Utile pour la lecture d'albums."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_STOP,
   "Arrêter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_STOP,
   "Cela arrêtera la lecture du flux audio, mais ne le supprimera pas de la mémoire. Il pourra être lu à nouveau en sélectionnant 'Lecture'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MIXER_ACTION_REMOVE,
   "Supprimer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_REMOVE,
   "Cela arrêtera la lecture du flux audio et le supprimera entièrement de la mémoire."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MIXER_ACTION_VOLUME,
   "Ajuster le volume de la diffusion audio."
   )

/* Settings > Audio > Menu Sounds */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE_MENU,
   "Mixeur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_ENABLE_MENU,
   "Lire des flux audio simultanés même dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_OK,
   "Son de confirmation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_CANCEL,
   "Son d'annulation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_NOTICE,
   "Son des notifications"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SOUND_BGM,
   "Musique de fond"
   )

/* Settings > Input */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS,
   "Nombre maximum d'utilisateurs "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MAX_USERS,
   "Nombre maximum d'utilisateurs pris en charge par RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR,
   "Détection des touches pressées "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_POLL_TYPE_BEHAVIOR,
   "Influence la façon dont les touches pressées sont détectées dans RetroArch. Utiliser 'Précoce' ou 'Tardive' peut diminuer la latence, en fonction de votre configuration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE,
   "Remapper les assignations d'entrées du cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_BINDS_ENABLE,
   "Remplacer les assignations des touches par les assignations remappées définies pour le cœur actuel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE,
   "Configuration automatique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTODETECT_ENABLE,
   "Tente de configurer automatiquement les manettes, style Plug-and-Play."
   )
#if defined(HAVE_DINPUT) || defined(HAVE_WINRAWINPUT)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_NOWINKEY_ENABLE,
   "Désactiver les raccourcis clavier Windows (Redémarrage requis)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_NOWINKEY_ENABLE,
   "Conserver les combinaisons de touches de Windows à l'intérieur de l'application."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SENSORS_ENABLE,
   "Entrées depuis les capteurs auxiliaires"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_SENSORS_ENABLE,
   "Permettre les entrées depuis l'accéléromètre, le gyroscope et les capteurs photométriques, si pris en charge par le matériel actuel. Peut avoir un impact sur les performances et/ou augmenter la consommation d'énergie sur certaines plates-formes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_MOUSE_GRAB,
   "Capture de la souris automatique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_MOUSE_GRAB,
   "Active la capture de la souris lorsque l'application est en premier plan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS,
   "Active automatiquement le mode de jeu au premier plan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_AUTO_GAME_FOCUS,
   "Toujours activer le mode 'Jeu au premier plan' lors du lancement et de la reprise du contenu. Lorsque réglé sur 'Détecter', l'option sera activée si le cœur actuel implémente le rappel du clavier de l'interface."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_OFF,
   "Désactivé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_ON,
   "Activé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_AUTO_GAME_FOCUS_DETECT,
   "Détecter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BUTTON_AXIS_THRESHOLD,
   "Seuil de l'axe des touches "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BUTTON_AXIS_THRESHOLD,
   "À quelle distance un axe doit être incliné pour entraîner une pression de touche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_DEADZONE,
   "Deadzone analogique "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_SENSITIVITY,
   "Sensibilité analogique "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT,
   "Délai pour l'assignation "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_TIMEOUT,
   "Nombre de secondes à attendre avant de passer à l'assignation de touche suivante."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_HOLD,
   "Temps de maintien pour l'assignation "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BIND_HOLD,
   "Nombre de secondes à maintenir une touche avant qu'elle ne soit assignée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD,
   "Délai d'activation du turbo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_PERIOD,
   "Durée (en images) après laquelle une touche en mode turbo est pressée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE,
   "Rapport cyclique du turbo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_DUTY_CYCLE,
   "Le nombre d'images en mode turbo Turbo durant laquelle les touches sont maintenues enfoncées. Si ce nombre est égal ou supérieur au délai d'activation du turbo, les touches ne seront jamais relâchées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_MODE,
   "Mode turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_MODE,
   "Sélectionner le comportement général du mode turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_DEFAULT_BUTTON,
   "Touche turbo par défaut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_DEFAULT_BUTTON,
   "Touche active par défaut pour le mode turbo 'Touche unique'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TURBO_FIRE_SETTINGS,
   "Tir turbo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TURBO_FIRE_SETTINGS,
   "Modifier les réglages de tir turbo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Retour haptique/vibration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HAPTIC_FEEDBACK_SETTINGS,
   "Modifier les réglages du retour haptique et de la vibration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_SETTINGS,
   "Touches pour le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_SETTINGS,
   "Modifier les réglages des touches du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS,
   "Raccourcis"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BINDS,
   "Modifier les réglages de touches de raccourcis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_USER_BINDS,
   "Touches du port %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_USER_BINDS,
   "Modifier les touches pour ce port."
   )

/* Settings > Input > Haptic Feedback/Vibration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIBRATE_ON_KEYPRESS,
   "Vibrer à chaque touche pressée"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLE_DEVICE_VIBRATION,
   "Activer la vibration du périphérique (pour les cœurs pris en charge)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_RUMBLE_GAIN,
   "Intensité de la vibration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_RUMBLE_GAIN,
   "Spécifier l'ampleur des effets de retour haptique."
   )

/* Settings > Input > Menu Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_UNIFIED_MENU_CONTROLS,
   "Contrôles du menu unifiés"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_UNIFIED_MENU_CONTROLS,
   "Utilisez les mêmes touches pour le menu et le jeu. S'applique au clavier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INPUT_SWAP_OK_CANCEL,
   "Inverser les touches OK/Annuler dans le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INPUT_SWAP_OK_CANCEL,
   "Échanger les touches pour Confirmer/Annuler. Désactivé correspond à l'orientation japonaise des touches, activé correspond à l'orientation occidentale."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU,
   "Tous les utilisateurs contrôlent le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ALL_USERS_CONTROL_MENU,
   "Permettre à tous les utilisateurs de contrôler le menu. Si désactivé, seul l'utilisateur 1 peut contrôler le menu."
   )

/* Settings > Input > Hotkeys */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_PRESS_TWICE,
   "Confirmer avant de quitter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_PRESS_TWICE,
   "Nécessite que la touche de raccourci Quitter soit pressée deux fois pour quitter RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Combinaison manette pour afficher/masquer le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO,
   "Combinaison de touches de la manette pour afficher/masquer le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_QUIT_GAMEPAD_COMBO,
   "Combinaison manette pour quitter"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_QUIT_GAMEPAD_COMBO,
   "Combinaison de touches de la manette pour quitter RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BLOCK_DELAY,
   "Délai d'activation de la touche de raccourcis (en images)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_HOTKEY_BLOCK_DELAY,
   "Ajouter un délai en images avant que l'entrée normale ne soit bloquée après avoir appuyé (et maintenu) la touche 'Raccourcis activés' assignée. Permet l'entrée normale de la touche 'Raccourcis activés' lorsqu'elle est mappée à une autre action (RetroManette 'Select' par exemple)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_KEY,
   "Avance rapide (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_KEY,
   "Bascule entre l'avance rapide et la vitesse normale."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "Avance rapide (maintenir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FAST_FORWARD_HOLD_KEY,
   "En maintenant la touche, cela active l'avance rapide. Le contenu rebascule en vitesse normale lorsque la touche n'est plus maintenue."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_KEY,
   "Ralenti (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_KEY,
   "Bascule entre le ralenti et la vitesse normale."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SLOWMOTION_HOLD_KEY,
   "Ralenti (maintenir)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SLOWMOTION_HOLD_KEY,
   "En maintenant la touche, cela active le ralenti. Le contenu rebascule en vitesse normale lorsque la touche n'est plus maintenue."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_LOAD_STATE_KEY,
   "Charger une sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_LOAD_STATE_KEY,
   "Charge une sauvegarde instantanée depuis l'emplacement actuellement sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SAVE_STATE_KEY,
   "Sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SAVE_STATE_KEY,
   "Effectue une sauvegarde instantanée dans l'emplacement actuellement sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Plein écran (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FULLSCREEN_TOGGLE_KEY,
   "Bascule entre le mode d'affichage plein écran et fenêtré."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CLOSE_CONTENT_KEY,
   "Fermer le contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CLOSE_CONTENT_KEY,
   "Ferme le contenu actuel. Toute modification non enregistrée pourrait être perdue."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_QUIT_KEY,
   "Quitter RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_QUIT_KEY,
   "Ferme RetroArch, en s'assurant que tous les fichiers de sauvegarde et de configuration sont enregistrés sur le disque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_PLUS,
   "Emplacement de sauvegarde instantanée +"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_PLUS,
   "Incrémente le numéro d'emplacement de sauvegarde instantanée sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATE_SLOT_MINUS,
   "Emplacement de sauvegarde instantanée -"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATE_SLOT_MINUS,
   "Décrémente le numéro d'emplacement de sauvegarde instantanée sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_REWIND,
   "Rembobiner"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND_HOTKEY,
   "En maintenant la touche, cela active le rembobinage.\n'Prise en charge du rembobinage' doit être activé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_BSV_RECORD_TOGGLE,
   "Enregistrement de la relecture (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_BSV_RECORD_TOGGLE,
   "Active/désactive l'enregistrement des touches pressées au format .bsv."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_PAUSE_TOGGLE,
   "Mettre en pause/reprendre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_PAUSE_TOGGLE,
   "Met en pause/reprend le contenu en cours d'exécution."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FRAMEADVANCE,
   "Avance image par image"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FRAMEADVANCE,
   "Lorsque le contenu est mis en pause, l'avancer d'une image."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESET,
   "Redémarrer le jeu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESET,
   "Redémarre le contenu actuel depuis le début."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_NEXT,
   "Shader suivant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_NEXT,
   "Charge et applique le fichier de préréglages de shaders suivant à la racine du dossier 'Shaders'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SHADER_PREV,
   "Shader précédent"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SHADER_PREV,
   "Charge et applique le fichier de préréglages de shaders précédent à la racine du dossier 'Shaders vidéo'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_PLUS,
   "Numéro de cheat suivant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_PLUS,
   "Incrémente le numéro du cheat sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_INDEX_MINUS,
   "Numéro de cheat précédent"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_INDEX_MINUS,
   "Décrémente le numéro du cheat sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_TOGGLE,
   "Cheats (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_TOGGLE,
   "Active/désactive le cheat actuellement sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SCREENSHOT,
   "Prendre une capture d'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SCREENSHOT,
   "Capture une image du contenu actuel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MUTE,
   "Mode muet (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MUTE,
   "Active/désactive la sortie audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OSK,
   "Clavier virtuel à l'écran (afficher/masquer)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OSK,
   "Active/désactive le clavier virtuel à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_FPS_TOGGLE,
   "Afficher/masquer les images/s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_FPS_TOGGLE,
   "Active/désactive l'indicateur d'état des images par seconde."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STATISTICS_TOGGLE,
   "Afficher les statistiques techniques (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STATISTICS_TOGGLE,
   "Active/désactive l'affichage des statistiques techniques à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PING_TOGGLE,
   "Afficher le Ping du jeu en réseau (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PING_TOGGLE,
   "Active/désactive le compteur de ping pour la salle de jeu en réseau actuelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_SEND_DEBUG_INFO,
   "Envoyer les informations de diagnostic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_SEND_DEBUG_INFO,
   "Envoie les informations de diagnostic pour votre appareil et la configuration de RetroArch à nos serveurs pour analyse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Hébergement du jeu en réseau (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_HOST_TOGGLE,
   "Active/désactive l'hébergement du jeu en réseau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_GAME_WATCH,
   "Mode joueur/spectateur de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_GAME_WATCH,
   "Bascule la session de jeu en réseau actuelle entre les modes 'jeu' et 'spectateur'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Chat joueur pour le jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_PLAYER_CHAT,
   "Envoie un message sur le chat de la session jeu en réseau en cours."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Activer/désactiver le fondu du chat de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_NETPLAY_FADE_CHAT_TOGGLE,
   "Basculer entre les messages en fondu et statiques pour le jeu en réseau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_ENABLE_HOTKEY,
   "Raccourcis activés"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_ENABLE_HOTKEY,
   "Si assignée, la touche 'Raccourcis activés' doit être enfoncée (et maintenue) avant que les autres touches de raccourcis ne soient reconnues. Permet aux touches d'être assignées aux fonctions de raccourci sans affecter les entrées normales."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_UP,
   "Augmenter le volume"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_UP,
   "Augmente le volume de la sortie audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_VOLUME_DOWN,
   "Baisser le volume"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_VOLUME_DOWN,
   "Diminue le volume de la sortie audio."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_OVERLAY_NEXT,
   "Surimpression suivante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_OVERLAY_NEXT,
   "Passe à la disposition suivante disponible pour la surimpression active à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_EJECT_TOGGLE,
   "Éjecter/insérer un disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_EJECT_TOGGLE,
   "Si le plateau du lecteur de disque virtuel est fermé, cela l'ouvre et retire le disque chargé. Sinon, insère le disque sélectionné et ferme le tiroir."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_NEXT,
   "Disque suivant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_NEXT,
   "Incrémente le numéro du disque sélectionné actuellement.\nLe plateau du lecteur de disque virtuel doit être ouvert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_DISK_PREV,
   "Disque précédent"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_DISK_PREV,
   "Décrémente le numéro du disque sélectionné actuellement.\nLe plateau du lecteur de disque virtuel doit être ouvert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Capture de la souris (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GRAB_MOUSE_TOGGLE,
   "Capture ou relâche la souris. Lorsqu'elle est capturée, le curseur système est masqué et confiné à la fenêtre d'affichage de RetroArch, améliorant le mouvement relatif de la souris."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_GAME_FOCUS_TOGGLE,
   "Jeu au premier plan/en arrière-plan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_GAME_FOCUS_TOGGLE,
   "Active/désactive le mode de jeu au premier plan. Lorsque le contenu est au premier plan, les touches de raccourci sont désactivées (la saisie au clavier est passée au cœur actif) et la souris est capturée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_UI_COMPANION_TOGGLE,
   "Interface de bureau (afficher/masquer)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_UI_COMPANION_TOGGLE,
   "Ouvre l'interface de bureau WIMP (fenêtres, icônes, menus et dispositif de pointage) associée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_MENU_TOGGLE,
   "Menu (afficher/masquer)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_MENU_TOGGLE,
   "Bascule l'affichage actuel entre le menu et l'exécution de contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RECORDING_TOGGLE,
   "Enregistrement (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RECORDING_TOGGLE,
   "Lance/arrête l'enregistrement de la session en cours vers un fichier vidéo local."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_STREAMING_TOGGLE,
   "Streaming (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_STREAMING_TOGGLE,
   "Lance/arrête le streaming de la session en cours vers une plateforme vidéo en ligne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RUNAHEAD_TOGGLE,
   "Exécution en avance (activer/désactiver)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RUNAHEAD_TOGGLE,
   "Active/désactive l'exécution en avance."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_AI_SERVICE,
   "Service IA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_AI_SERVICE,
   "Capture une image du contenu actuel puis traduit et/ou lit à haute voix tout texte à l'écran.\n'Service IA' doit être activé et configuré."
   )

/* Settings > Input > Port # Controls */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_TYPE,
   "Type de périphérique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ADC_TYPE,
   "Analogique vers numérique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_ADC_TYPE,
   "Utilise le stick analogique spécifié pour les entrées de la croix directionnelle. Si le cœur prend en charge l'analogique nativement, le mappage de la croix directionnelle sera désactivé sauf si une option '(Forcé)' est sélectionnée. Si le mappage de la croix directionnelle est forcé, le cœur ne recevra aucune entrée analogique à partir du stick spécifié."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DEVICE_INDEX,
   "Numéro du périphérique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAP_PORT,
   "Port affecté"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAP_PORT,
   "Spécifie quel port du 'cœur' (généralement le numéro du joueur) recevra les entrées du port global %u de l'interface."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_ALL,
   "Assigner toutes les touches"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BIND_DEFAULT_ALL,
   "Tout assigner par défaut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SAVE_AUTOCONFIG,
   "Sauvegarder la configuration automatique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_INDEX,
   "Numéro de la souris"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_B,
   "Bouton B (bas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_Y,
   "Bouton Y (gauche)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_SELECT,
   "Bouton Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_START,
   "Bouton Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_UP,
   "Croix Haut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_DOWN,
   "Croix Bas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_LEFT,
   "Croix Gauche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_RIGHT,
   "Croix Droite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_A,
   "Bouton A (droite)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_X,
   "Bouton X (haut)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L,
   "Bouton L (épaule)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R,
   "Bouton R (épaule)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L2,
   "Bouton L2 (gâchette)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R2,
   "Bouton R2 (gâchette)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_L3,
   "Bouton L3 (pouce)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_JOYPAD_R3,
   "Bouton R3 (pouce)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_PLUS,
   "Analogique gauche X+ (droite)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X_MINUS,
   "Analogique gauche X- (gauche)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_PLUS,
   "Analogique gauche Y+ (bas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y_MINUS,
   "Analogique gauche Y- (haut)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_PLUS,
   "Analogique droit X+ (droite)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X_MINUS,
   "Analogique droit X- (gauche)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_PLUS,
   "Analogique droit Y+ (bas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y_MINUS,
   "Analogique droit Y- (haut)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_TRIGGER,
   "Gâchette de pistolet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_RELOAD,
   "Rechargement de pistolet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_A,
   "Pistolet aux A"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_B,
   "Pistolet aux B"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_AUX_C,
   "Pistolet aux C"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_START,
   "Pistolet Start"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_SELECT,
   "Pistolet Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_UP,
   "Croix pistolet Haut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_DOWN,
   "Croix pistolet Bas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_LEFT,
   "Croix pistolet Gauche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_LIGHTGUN_DPAD_RIGHT,
   "Croix pistolet Droite"
   )

/* Settings > Latency */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_UNSUPPORTED,
   "[Exécution en avance indisponible]"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_UNSUPPORTED,
   "Le cœur actuel est incompatible avec l'exécution en avance en raison de la non prise en charge de la sauvegarde instantanée déterministe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_ENABLED,
   "Exécuter en avance pour réduire la latence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_ENABLED,
   "Exécute la logique du cœur une ou plusieurs images à l'avance, puis recharge l'état précédent pour réduire la latence perçue à chaque touche pressée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_FRAMES,
   "Nombre d'images à éxecuter en avance"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_FRAMES,
   "Nombre d'images à exécuter en avance. Provoque des problèmes de jeu tels que des variations de la latence si le nombre d'images de latence interne du jeu est dépassé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_SECONDARY_INSTANCE,
   "Utiliser une instance secondaire pour l'exécution en avance"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_SECONDARY_INSTANCE,
   "Utilise une seconde instance du cœur RetroArch pour l'exécution en avance. Empêche les problèmes audio dus au chargement de l'état précédent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_AHEAD_HIDE_WARNINGS,
   "Masquer les avertissements pour l'exécution en avance"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN_AHEAD_HIDE_WARNINGS,
   "Masque le message d'avertissement qui apparaît lors de l'utilisation de l'exécution en avance si le cœur ne prend pas en charge les sauvegardes instantanées."
   )

/* Settings > Core */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT,
   "Contexte matériel partagé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHARED_CONTEXT,
   "Donne aux cœurs bénéficiant de l'accélération graphique leur propre contexte privé. Évite d'avoir à supposer des changements d'état matériel entre deux images."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DRIVER_SWITCH_ENABLE,
   "Autoriser les cœurs à changer le pilote vidéo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DRIVER_SWITCH_ENABLE,
   "Autorise les cœurs à basculer vers un pilote vidéo différent de celui qui est chargé actuellement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN,
   "Charger un cœur factice à la fermeture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DUMMY_ON_CORE_SHUTDOWN,
   "Certains cœurs ont une fonctionnalité d'extinction, charger un cœur factice empêchera RetroArch de se fermer."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE,
   "Démarrer un cœur automatiquement"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHECK_FOR_MISSING_FIRMWARE,
   "Vérifier la présence du firmware avant le chargement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHECK_FOR_MISSING_FIRMWARE,
   "Vérifie que tous les firmwares requis sont présents avant de tenter le chargement du contenu sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_CATEGORY_ENABLE,
   "Catégories pour les options des cœurs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_CATEGORY_ENABLE,
   "Permettre aux cœurs de présenter leurs options dans des sous-menus par catégories. REMARQUE : Le cœur doit être rechargé pour que les changements prennent effet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INFO_CACHE_ENABLE,
   "Mettre en cache les fichiers d'information de cœurs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INFO_CACHE_ENABLE,
   "Conserve un cache local persistant des informations de cœurs installés. Réduit considérablement les temps de chargement sur les plates-formes avec un accès lent au disque."
   )
#ifndef HAVE_DYNAMIC
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Toujours recharger le cœur lors du lancement de contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ALWAYS_RELOAD_CORE_ON_RUN_CONTENT,
   "Redémarrer RetroArch lors du lancement de contenu, même lorsque le cœur demandé est déjà chargé. Cela peut améliorer la stabilité du système, au détriment d'un temps de chargement accru."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE,
   "Autoriser la rotation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_ALLOW_ROTATE,
   "Autorise les cœurs à définir la rotation. Si désactivé, les demandes de rotation seront ignorées. Utile pour les configurations où l'écran pivote manuellement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_LIST,
   "Gérer les cœurs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_MANAGER_LIST,
   "Effectue des tâches de maintenance hors-ligne sur les cœurs installés (sauvegarde, restauration, suppression, etc.) et affiche des informations sur les cœurs."
   )
#ifdef HAVE_MIST
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_MANAGER_STEAM_LIST,
   "Gérer les cœurs"
   )





MSG_HASH(
   MSG_CORE_STEAM_INSTALLING,
   "Installation du cœur : "
)


#endif
/* Settings > Configuration */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT,
   "Sauvegarder la configuration en quittant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONFIG_SAVE_ON_EXIT,
   "Enregistrer les modifications dans le fichier de configuration à la fermeture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS,
   "Charger les options du cœur par contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAME_SPECIFIC_OPTIONS,
   "Charger des options de cœur personnalisées au démarrage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE,
   "Charger les remplacements de configuration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_OVERRIDES_ENABLE,
   "Charger une configuration personnalisée au démarrage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE,
   "Charger les fichiers de remappage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTO_REMAPS_ENABLE,
   "Charger des contrôles personnalisés au démarrage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE,
   "Charger les préréglages de shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GLOBAL_CORE_OPTIONS,
   "Utiliser un fichier de configuration des cœurs global"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GLOBAL_CORE_OPTIONS,
   "Sauvegarde tous les réglages de cœurs dans un fichier de configuration commun (retroarch-core-options.cfg). Si cette option est désactivée, les réglages de chaque cœur seront sauvegardés vers un dossier/fichier de configuration spécifique au cœur séparé dans le dossier 'Config' de RetroArch."
   )

/* Settings > Saving */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE,
   "Trier les sauvegardes dans des dossiers nommés selon le nom du cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_ENABLE,
   "Classer les fichiers de sauvegarde dans des dossiers nommés d'après le cœur utilisé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE,
   "Trier les sauvegardes instantanées dans des dossiers nommés selon le nom du cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_ENABLE,
   "Classer les sauvegardes instantanées dans des dossiers nommés d'après le cœur utilisé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Trier les sauvegardes dans des dossiers nommés selon le dossier du contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVEFILES_BY_CONTENT_ENABLE,
   "Trie les fichiers de sauvegarde dans des dossiers nommés en fonction du dossier où le contenu est situé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Trier les sauvegardes instantanées dans des dossiers nommés selon le dossier du contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SAVESTATES_BY_CONTENT_ENABLE,
   "Trie les fichiers de sauvegarde instantanée dans des dossiers nommés en fonction du dossier où le contenu est situé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE,
   "Ne pas écraser la SRAM en chargeant la sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLOCK_SRAM_OVERWRITE,
   "Empêche la SRAM d'être écrasée lors du chargement d'une sauvegarde instantanée. Pourrait potentiellement conduire à des bugs de jeu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL,
   "Intervalle de sauvegarde auto de la SRAM "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUTOSAVE_INTERVAL,
   "Sauvegarder automatiquement la mémoire SRAM non volatile à un intervalle régulier (en secondes)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX,
   "Sauvegardes instantanées incrémentales"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_INDEX,
   "Avant la création d'une sauvegarde instantanée, le numéro de la sauvegarde instantanée est automatiquement incrémenté. Lors du chargement de contenu, le numéro sera réglé sur le plus haut existant."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_MAX_KEEP,
   "Nombre maximum de sauvegardes instantanées auto-incrémentales à conserver"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_MAX_KEEP,
   "Limiter le nombre de sauvegardes instantanées qui seront créées lorsque l'option 'Sauvegardes instantanées incrémentales' est activée. Si la limite est dépassée lors de l'enregistrement d'une nouvelle sauvegarde instantanée, la sauvegarde instantanée existante avec l'index le plus bas sera supprimée. Une valeur de '0' signifie que des sauvegardes instantanées illimitées seront enregistrés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE,
   "Sauvegardes instantanées automatiques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_SAVE,
   "Créer automatiquement une sauvegarde instantanée à la fermeture de contenu. RetroArch chargera cette sauvegarde instantanée automatiquement si 'Chargement auto des sauvegardes instantanées' est activé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD,
   "Chargement auto des sauvegardes instantanées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_AUTO_LOAD,
   "Charger la sauvegarde instantanée automatique au démarrage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_THUMBNAIL_ENABLE,
   "Miniatures pour les sauvegardes instantanées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_THUMBNAIL_ENABLE,
   "Affiche des miniatures pour les sauvegardes instantanées dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_FILE_COMPRESSION,
   "Compression de la SaveRAM (SRAM)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_FILE_COMPRESSION,
   "Enregistrer la mémoire SRAM non volatile sous un format archivé. Réduit considérablement la taille du fichier au détriment de l'augmentation (négligeable) des temps de sauvegarde/chargement.\nS'applique uniquement aux cœurs qui peuvent sauvegarder via l'interface Save RAM standard de libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_FILE_COMPRESSION,
   "Compression des sauvegardes instantanées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_FILE_COMPRESSION,
   "Enregistrer les fichiers de sauvegarde instantanée sous un format archivé. Réduit considérablement la taille du fichier au détriment de l'augmentation des temps de sauvegarde/chargement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Trier les captures d'écran dans des dossiers nommés selon le nom du dossier de contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SORT_SCREENSHOTS_BY_CONTENT_ENABLE,
   "Trie les captures d'écran dans des dossiers nommés en fonction du dossier où le contenu est situé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILES_IN_CONTENT_DIR_ENABLE,
   "Enregistrer les sauvegardes avec le contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATES_IN_CONTENT_DIR_ENABLE,
   "Enregistrer les sauvegardes instantanées avec le contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEMFILES_IN_CONTENT_DIR_ENABLE,
   "Fichiers système avec le contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOTS_IN_CONTENT_DIR_ENABLE,
   "Enregistrer les captures d'écran avec le contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG,
   "Enregistrer le journal du temps de jeu (par cœur)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG,
   "Garder la trace de la durée d'exécution de chaque élément de contenu, séparément par cœur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Enregistrer le journal du temps de jeu (cumulé)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_RUNTIME_LOG_AGGREGATE,
   "Garder la trace de la durée d'exécution de chaque élément de contenu, en tant que total cumulé sur tous les cœurs."
   )

/* Settings > Logging */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY,
   "Verbosité de la journalisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_VERBOSITY,
   "Journaliser les événements sur un terminal ou dans un fichier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_LOG_LEVEL,
   "Niveau de journalisation de l'interface"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRONTEND_LOG_LEVEL,
   "Ajuster le niveau de journalisation pour l'interface. Si un niveau de journalisation émis par le frontend est inférieur à cette valeur, il est alors ignoré."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL,
   "Niveau de journalisation des cœurs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_LOG_LEVEL,
   "Définir le niveau de journalisation pour les cœurs. Si un niveau de journalisation émis par un cœur est inférieur à cette valeur, il sera ignoré."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE,
   "Journaliser vers un fichier"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE,
   "Rediriger les messages de la journalisation des évènements système vers un fichier. Requiert l'activation du réglage de 'Verbosité de la journalisation'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_TO_FILE_TIMESTAMP,
   "Fichiers de journalisation horodatés"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_TO_FILE_TIMESTAMP,
   "Lors de la journalisation vers un fichier, rediriger la sortie de chaque session RetroArch vers un nouveau fichier horodaté. Si désactivé, le journal est écrasé chaque fois que RetroArch est redémarré."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE,
   "Compteurs de performance"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PERFCNT_ENABLE,
   "Compteurs de performance pour RetroArch et les cœurs. Les données de compteur peuvent aider à déterminer les goulots d'étranglement du système et à ajuster les performances."
   )

/* Settings > File Browser */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES,
   "Afficher les fichiers et dossiers cachés"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_HIDDEN_FILES,
   "Affiche les fichiers/dossiers cachés dans le navigateur de fichiers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtrer les extension inconnues"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE,
   "Filtrer les fichiers affichés dans le navigateur de fichiers selon les extensions prises en charge."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER,
   "Utiliser le lecteur média intégré"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER_BY_CURRENT_CORE,
   "Filtrer par cœur actif"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_LAST_START_DIRECTORY,
   "Recharger le dernier dossier de démarrage utilisé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USE_LAST_START_DIRECTORY,
   "Ouvrir le navigateur de fichiers au dernier emplacement utilisé lors du chargement du contenu depuis le dossier de démarrage. Remarque : L'emplacement sera réinitialisé par défaut au redémarrage de RetroArch."
   )

/* Settings > Frame Throttle */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS,
   "Rembobinage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_REWIND,
   "Modifier les réglages de rembobinage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_SETTINGS,
   "Compteur de temps par images"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_SETTINGS,
   "Modifier les paramètres qui influent sur le compteur de temps par image.\nActif uniquement lorsque la vidéo sur plusieurs fils d'exécution est désactivée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO,
   "Vitesse d'exécution maximale "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_RATIO,
   "Vitesse d'exécution maximale du contenu lors de l'avance rapide (par exemple, 5,0x pour un contenu à 60 images/s = une limitation à 300 images/s) Si définie à 0,0x, la vitesse en avance rapide est illimitée (pas de limite d'images/s)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FASTFORWARD_FRAMESKIP,
   "Saut d'images pour l'avance rapide"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FASTFORWARD_FRAMESKIP,
   "Sauter des images en fonction de la vitesse d'avance rapide. Cela permet d'économiser de l'énergie et d'utiliser des limitateurs d'images de tierce partie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO,
   "Taux de ralentissement maximal "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SLOWMOTION_RATIO,
   "Taux de ralentissement auquel le contenu sera joué en mode ralenti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE,
   "Limiter les images/s dans le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENUM_THROTTLE_FRAMERATE,
   "S'assure que le nombre d'images par seconde est plafonné dans le menu."
   )

/* Settings > Frame Throttle > Rewind */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_ENABLE,
   "Prise en charge du rembobinage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_ENABLE,
   "Revenir à un point précédent dans le jeu (limité dans le temps). Cette option entraîne une forte baisse des performances lors du jeu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY,
   "Vitesse du rembobinage "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_GRANULARITY,
   "Nombre d'images à rembobiner par étape. Les valeurs plus hautes augmentent la vitesse du rembobinage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE,
   "Mémoire tampon de rembobinage (Mo) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE,
   "Quantité de mémoire (en Mo) à réserver pour la mémoire tampon de rembobinage. Augmenter cette valeur augmentera la quantité d'historique du rembobinage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REWIND_BUFFER_SIZE_STEP,
   "Précision d'ajustement du tampon de rembobinage (Mo) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_REWIND_BUFFER_SIZE_STEP,
   "Chaque fois que la valeur de la taille du tampon de rembobinage est augmentée ou diminuée, il changera de ce montant."
   )

/* Settings > Frame Throttle > Frame Time Counter */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Réinitialiser après l'avance rapide"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_FASTFORWARDING,
   "Réinitialise le compteur de temps par images après l'avance rapide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Réinitialiser après le chargement d'une sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_LOAD_STATE,
   "Réinitialise le compteur de temps par images après le chargement d'une sauvegarde instantanée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Réinitialiser après une sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAME_TIME_COUNTER_RESET_AFTER_SAVE_STATE,
   "Réinitialise le compteur de temps par images après une sauvegarde instantanée."
   )

/* Settings > Recording */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_QUALITY,
   "Qualité de l'enregistrement "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_CONFIG,
   "Configuration d'enregistrement personnalisée "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RECORD_THREADS,
   "Fils d'exécution de l'enregistrement "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD,
   "Utiliser les filtres vidéo lors de l'enregistrement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_POST_FILTER_RECORD,
   "Capture l'image après l'application des filtres (mais pas des shaders). La vidéo sera aussi élégante que ce que vous voyez sur votre écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD,
   "Utiliser le processeur graphique pour l'enregistrement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_GPU_RECORD,
   "Enregistrer avec les shaders produits par le processeur graphique si disponibles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_MODE,
   "Mode streaming"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_STREAM_QUALITY,
   "Qualité de la diffusion "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAM_CONFIG,
   "Configuration de diffusion personnalisée "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_TITLE,
   "Titre de la diffusion "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STREAMING_URL,
   "URL de la diffusion "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UDP_STREAM_PORT,
   "Port de stream UDP "
   )

/* Settings > On-Screen Display */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS,
   "Surimpressions à l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_OVERLAY_SETTINGS,
   "Ajuste les cadres d'images et les touches à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Dispositions d'affichage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_VIDEO_LAYOUT_SETTINGS,
   "Ajuste la disposition de l'affichage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Notifications à l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_SETTINGS,
   "Ajuste les notifications à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Affichage des notifications"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ONSCREEN_NOTIFICATIONS_VIEWS_SETTINGS,
   "Afficher/masquer certains types de notifications."
   )

/* Settings > On-Screen Display > On-Screen Overlay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE,
   "Surimpression à l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_ENABLE,
   "Les surimpressions sont utilisées pour les bordures et les contrôles à l'écran."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_BEHIND_MENU,
   "Afficher la surimpression derrière le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_BEHIND_MENU,
   "Afficher la surimpression en arrière-plan plutôt qu'en premier plan du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU,
   "Masquer la surimpression dans le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_IN_MENU,
   "Masquer la surimpression à l'intérieur du menu, et l'afficher à nouveau en le quittant."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Masquer la surimpression lorsqu'une manette est connectée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED,
   "Masque la surimpression lorsqu'une manette physique est connectée dans le port 1, et l'affiche à nouveau lorsqu'elle est déconnectée."
   )
#if defined(ANDROID)
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_HIDE_WHEN_GAMEPAD_CONNECTED_ANDROID,
   "Masque la surimpression lorsqu'une manette physique est connectée dans le port 1. La surimpression ne sera pas restaurée automatiquement si la manette est déconnectée."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS,
   "Afficher les touches pressées sur la surimpression"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS,
   "Affiche les entrées détectées sur la surimpression à l'écran. 'Touchées' met en surbrillance les éléments qui sont pressés/cliqués. 'Physiques (Manette)' met en surbrillance l'entrée réelle transmise aux cœurs, généralement à partir d'une manette ou d'un clavier connectés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_TOUCHED,
   "Touchées"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PHYSICAL,
   "Physiques (Manette)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Afficher les touches pressées depuis le port "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_INPUTS_PORT,
   "Sélectionne le port du périphérique d'entrée à surveiller lorsque 'Afficher les touches pressées sur la surimpression' est réglé sur 'Physiques (Manette)'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Afficher le curseur de la souris avec la surimpression"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_SHOW_MOUSE_CURSOR,
   "Affiche le curseur de la souris lors de l'utilisation d'une surimpression à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_ROTATE,
   "Rotation automatique de la surimpression"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_ROTATE,
   "Si supporté par la surimpression active, effectue une rotation automatique de la surimpression pour correspondre à l'orientation/rapport d'aspect de l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_AUTO_SCALE,
   "Mise à l'échelle automatique de la surimpression"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_OVERLAY_AUTO_SCALE,
   "Mettre à l'échelle automatiquement la surimpression et l'espacement des éléments d'interface utilisateur pour s'adapter aux proportions de l'écran. Produit les meilleurs résultats avec les surimpressions de manettes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY,
   "Surimpression à l'écran"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED,
   "Charger la surimpression préférée"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY,
   "Opacité de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_OPACITY,
   "Opacité de tous les éléments d'interface utilisateur de la surimpression."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET,
   "Préréglages de surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_PRESET,
   "Sélectionner une surimpression à partir du navigateur de fichiers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_LANDSCAPE,
   "(Paysage) Échelle de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_LANDSCAPE,
   "Échelle de tous les éléments d'interface de la surimpression lors de l'utilisation d'une orientation d'écran en mode paysage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "(Paysage) Ajustement d'aspect de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_LANDSCAPE,
   "Appliquer un facteur de correction du rapport d'aspect à la surimpression lors de l'utilisation d'une orientation d'écran en mode paysage. Les valeurs positives augmentent (et les valeurs négatives diminuent) la largeur effective de la surimpression."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_LANDSCAPE,
   "(Paysage) Séparation horizontale de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_LANDSCAPE,
   "Si pris en charge par le préréglage actuel, ajuster l'espacement entre les éléments d'interface dans la partie gauche et droite d'une surimpression lors de l'utilisation d'une orientation d'écran en mode paysage. Les valeurs positives augmentent (et les valeurs négatives diminuent) la séparation entre les deux moitiés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "(Paysage) Séparation verticale de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_LANDSCAPE,
   "Si pris en charge par le préréglage actuel, ajuster l'espacement entre les éléments d'interface dans la partie haute et basse d'une surimpression lors de l'utilisation d'une orientation d'écran en mode paysage. Les valeurs positives augmentent (et les valeurs négatives diminuent) la séparation entre les deux moitiés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_LANDSCAPE,
   "(Paysage) Décalage X de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_LANDSCAPE,
   "Décalage horizontal de la surimpression lors de l'utilisation d'une orientation d'écran en mode paysage. Les valeurs positives déplacent la surimpression vers la droite; les valeurs négatives vers la gauche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_LANDSCAPE,
   "(Paysage) Décalage Y de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_LANDSCAPE,
   "Décalage vertical de la surimpression lors de l'utilisation d'une orientation d'écran en mode paysage. Les valeurs positives déplacent la surimpression vers le haut; les valeurs négatives vers le bas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE_PORTRAIT,
   "(Portrait) Échelle de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_SCALE_PORTRAIT,
   "Échelle de tous les éléments d'interface de la surimpression lors de l'utilisation d'une orientation d'écran en mode portrait."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "(Portrait) Ajustement d'aspect de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_ASPECT_ADJUST_PORTRAIT,
   "Appliquer un facteur de correction du rapport d'aspect à la surimpression lors de l'utilisation d'une orientation d'écran en mode portrait. Les valeurs positives augmentent (et les valeurs négatives diminuent) la hauteur effective de la surimpression."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_SEPARATION_PORTRAIT,
   "(Portrait) Séparation horizontale de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_SEPARATION_PORTRAIT,
   "Si pris en charge par le préréglage actuel, ajuster l'espacement entre les éléments d'interface dans la partie gauche et droite d'une surimpression lors de l'utilisation d'une orientation d'écran en mode portrait. Les valeurs positives augmentent (et les valeurs négatives diminuent) la séparation entre les deux moitiés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_SEPARATION_PORTRAIT,
   "(Portrait) Séparation verticale de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_SEPARATION_PORTRAIT,
   "Si pris en charge par le préréglage actuel, ajuster l'espacement entre les éléments d'interface dans la partie haute et basse d'une surimpression lors de l'utilisation d'une orientation d'écran en mode portrait. Les valeurs positives augmentent (et les valeurs négatives diminuent) la séparation entre les deux moitiés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_X_OFFSET_PORTRAIT,
   "(Portrait) Décalage X de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_X_OFFSET_PORTRAIT,
   "Décalage horizontal de la surimpression lors de l'utilisation d'une orientation d'écran en mode portrait. Les valeurs positives déplacent la surimpression vers la droite; les valeurs négatives vers la gauche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_Y_OFFSET_PORTRAIT,
   "(Portrait) Décalage Y de la surimpression "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_Y_OFFSET_PORTRAIT,
   "Décalage vertical de la surimpression lors de l'utilisation d'une orientation d'écran en mode portrait. Les valeurs positives déplacent la surimpression vers le haut; les valeurs négatives vers le bas."
   )

/* Settings > On-Screen Display > Video Layout */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_ENABLE,
   "Active la disposition d'affichage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_ENABLE,
   "Les dispositions d'affichage sont utilisées pour les bezels et autres éléments graphiques."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_PATH,
   "Dossier des dispositions d'affichage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_PATH,
   "Sélectionner une disposition d'affichage depuis le navigateur de fichiers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_SELECTED_VIEW,
   "Vue sélectionnée"
   )
MSG_HASH( /* FIXME Unused */
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_SELECTED_VIEW,
   "Sélectionne une vue pour la disposition d'affichage."
   )

/* Settings > On-Screen Display > On-Screen Notifications */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE,
   "Notifications à l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_ENABLE,
   "Affiche les messages à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGETS_ENABLE,
   "Widgets graphiques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGETS_ENABLE,
   "Utilisez des animations, des notifications, des indicateurs et des commandes modernes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_AUTO,
   "Mise à l'échelle automatique des widgets graphiques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_AUTO,
   "Redimensionne automatiquement les notifications améliorées, indicateurs et commandes en fonction de l'échelle actuelle du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Remplacement de l'échelle des widgets graphiques (Plein écran) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_FULLSCREEN,
   "Appliquer une valeur manuelle de remplacement du facteur d'échelle pour l'affichage des widgets à l'écran en mode plein écran. Cette valeur est appliquée uniquement si l'option 'Mise à l'échelle automatique des widgets graphiques' est désactivée. Peut être utile pour augmenter ou réduire la taille des notifications améliorées, indicateurs et commandes indépendamment du menu lui-même."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Remplacement de l'échelle des widgets graphiques (Fenêtré) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR_WINDOWED,
   "Appliquer une valeur manuelle de remplacement du facteur d'échelle pour l'affichage des widgets à l'écran en mode fenêtré. Cette valeur est appliquée uniquement si l'option 'Mise à l'échelle automatique des widgets graphiques' est désactivée. Peut être utile pour augmenter ou réduire la taille des notifications améliorées, indicateurs et commandes indépendamment du menu lui-même."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_SHOW,
   "Afficher le nombre d'images/s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_SHOW,
   "Afficher le nombre d'images/s actuel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FPS_UPDATE_INTERVAL,
   "Intervalle de mise à jour des images/s (en images)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FPS_UPDATE_INTERVAL,
   "L'affichage des images/s sera mis à jour à l'intervalle défini en images."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAMECOUNT_SHOW,
   "Afficher le compteur d'images"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FRAMECOUNT_SHOW,
   "Afficher le compteur d'images à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATISTICS_SHOW,
   "Afficher les statistiques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATISTICS_SHOW,
   "Afficher des statistiques techniques à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_SHOW,
   "Afficher l'utilisation de la mémoire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_SHOW,
   "Afficher la quantité de mémoire utilisée et totale sur le système."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MEMORY_UPDATE_INTERVAL,
   "Fréquence d'actualisation de l'utilisation mémoire (en images) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MEMORY_UPDATE_INTERVAL,
   "L'affichage de l'utilisation de la mémoire sera actualisé à l'intervalle défini en images."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PING_SHOW,
   "Afficher le ping du jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PING_SHOW,
   "Afficher le ping de la salle de jeu en réseau actuelle."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Notification au démarrage de contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT_ANIMATION,
   "Afficher une brève notification animée lors du chargement de contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_AUTOCONFIG,
   "Notifications à la connexion de périphériques d'entrée (configuration automatique)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Notifications de cheats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CHEATS_APPLIED,
   "Afficher un message à l'écran lorsque des codes de cheat sont appliqués."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Notifications de patchs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_PATCH_APPLIED,
   "Affiche un message à l'écran lors de l'application de patches logiciels sur des ROM."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_AUTOCONFIG,
   "Afficher un message à l'écran lors de la connexion/déconnexion de périphériques d'entrées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REMAP_LOAD,
   "Notifications au chargement de remappages des touches"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REMAP_LOAD,
   "Afficher un message à l'écran lors du chargement des fichiers de remappage des touches."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Notifications au chargement des remplacements de configuration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_CONFIG_OVERRIDE_LOAD,
   "Afficher un message à l'écran lors du chargement des fichiers de remplacement de configuration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Notifications à la restauration du disque initial"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SET_INITIAL_DISK,
   "Afficher un message à l'écran lors de la restauration automatique du dernier disque utilisé au lancement de contenu multi-disque chargé via des listes de lecture M3U."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_FAST_FORWARD,
   "Notifications d'avance rapide"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_FAST_FORWARD,
   "Afficher un indicateur à l'écran lors de l'avance rapide du contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT,
   "Notifications de capture d'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT,
   "Afficher un message à l'écran lors d'une capture d'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Persistence de la notification de capture d'écran "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_DURATION,
   "Définir la durée d'affichage du message de capture d'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_NORMAL,
   "Normale"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_FAST,
   "Courte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_VERY_FAST,
   "Très courte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_DURATION_INSTANT,
   "Instantanée"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Effet de flash lors de la capture d'écran "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_SCREENSHOT_FLASH,
   "Afficher un effet de flash blanc à l'écran lors de la capture de l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_NORMAL,
   "Activé (normal)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_SCREENSHOT_FLASH_FAST,
   "Activé (court)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_REFRESH_RATE,
   "Notifications pour la fréquence de rafraîchissement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_REFRESH_RATE,
   "Afficher un message à l'écran lors du réglage de la fréquence de rafraîchissement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Notifications supplémentaires du jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_NETPLAY_EXTRA,
   "Afficher les messages non essentiels du jeu en réseau à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Notifications dans le menu uniquement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NOTIFICATION_SHOW_WHEN_MENU_IS_ALIVE,
   "Afficher les notifications uniquement lorsque le menu est ouvert."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH,
   "Police des notifications "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_PATH,
   "Sélectionner la police pour les notifications à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE,
   "Taille des notifications "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FONT_SIZE,
   "Spécifier la taille de la police en points."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X,
   "Position horizontale des notifications "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_X,
   "Choisir la position personalisée sur l'axe X pour le texte à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y,
   "Position verticale des notifications "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MESSAGE_POS_Y,
   "Choisir la position personalisée sur l'axe Y pour le texte à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED,
   "Couleur des notifications (Rouge)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN,
   "Couleur des notifications (Vert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE,
   "Couleur des notifications (Bleu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE,
   "Arrière-plan des notifications"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED,
   "Couleur d'arrière-plan des notifications (Rouge)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN,
   "Couleur d'arrière-plan des notifications (Vert)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE,
   "Couleur d'arrière-plan des notifications (Bleu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY,
   "Opacité (Arrière-plan des notifications) "
   )

/* Settings > User Interface */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_VIEWS_SETTINGS,
   "Visibilité des éléments du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_VIEWS_SETTINGS,
   "Afficher/masquer des éléments du menu dans RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SETTINGS,
   "Apparence"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SETTINGS,
   "Modifier les réglages d'apparence de l'écran du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS,
   "Afficher les réglages avancés"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHOW_ADVANCED_SETTINGS,
   "Affiche les réglages avancés pour les utilisateurs expérimentés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ENABLE_KIOSK_MODE,
   "Mode kiosque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_ENABLE_KIOSK_MODE,
   "Protège la configuration en masquant tous les réglages liés à la configuration."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_KIOSK_MODE_PASSWORD,
   "Définir le mot de passe pour désactiver le mode kiosque "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_KIOSK_MODE_PASSWORD,
   "La saisie d'un mot de passe lors de l'activation du mode kiosque permet de le désactiver ultérieurement à partir du menu, en allant dans le menu principal, en sélectionnant 'Désactiver le mode kiosque' et en entrant le mot de passe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND,
   "Navigation en boucle dans les menus"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NAVIGATION_WRAPAROUND,
   "Retour au début et/ou à la fin si la limite de la liste est atteinte horizontalement ou verticalement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO,
   "Mettre en pause le contenu lorsque le menu est actif"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_LIBRETRO,
   "Mettre en pause le contenu en cours d'exécution si le menu est actif."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SAVESTATE_RESUME,
   "Reprendre le contenu après l'utilisation de sauvegardes instantanées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SAVESTATE_RESUME,
   "Ferme automatiquement le menu et reprends le contenu après la sauvegarde ou le chargement instantané. Désactiver cette option peut améliorer les performances de sauvegarde instantanée sur des appareils très lents."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_INSERT_DISK_RESUME,
   "Reprendre le contenu après le changement du disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_INSERT_DISK_RESUME,
   "Ferme automatiquement le menu et reprends le contenu actuel après l'insertion ou le chargement d'un nouveau disque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUIT_ON_CLOSE_CONTENT,
   "Quitter lors de la fermeture de contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUIT_ON_CLOSE_CONTENT,
   "Quitter automatiquement RetroArch lors de la fermeture du contenu. 'CLI' quitte uniquement lorsque le contenu est lancé via la ligne de commande."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_TIMEOUT,
   "Délai d'activation de l'économiseur d'écran du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_TIMEOUT,
   "Lorsque le menu est actif, un économiseur d'écran s'affichera après la période d'inactivité spécifiée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION,
   "Animation de l'économiseur d'écran du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION,
   "Activer un effet d'animation lorsque l'économiseur d'écran du menu est actif. A un impact modeste sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SNOW,
   "Neige"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_STARFIELD,
   "Ciel étoilé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Vitesse d'animation de l'économiseur d'écran de menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCREENSAVER_ANIMATION_SPEED,
   "Ajuster la vitesse de l'effet d'animation de l'économiseur d'écran du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE,
   "Prise en charge de la souris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MOUSE_ENABLE,
   "Permettre le contrôle du menu avec une souris."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_POINTER_ENABLE,
   "Prise en charge du tactile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_POINTER_ENABLE,
   "Permettre le contrôle du menu avec un écran tactile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE,
   "Tâches sur plusieurs fils d'exécution"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THREADED_DATA_RUNLOOP_ENABLE,
   "Effectue des tâches sur un fil d'exécution distinct."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE,
   "Mettre en pause le contenu lors de la mise en arrière-plan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PAUSE_NONACTIVE,
   "Mettre le contenu en pause lorsque RetroArch n'est pas au premier plan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION,
   "Désactiver la composition de bureau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_DISABLE_COMPOSITION,
   "Le gestionnaire de fenêtres utilise la composition pour appliquer des effets visuels et détecter les fenêtres qui ne répondent pas, entre autres."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_FAST,
   "Accélération au défilement du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_FAST,
   "Vitesse de déplacement maximale du curseur lors d'un défilement prolongé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCROLL_DELAY,
   "Délai de défilement du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCROLL_DELAY,
   "Délai initial en millisecondes lors du maintien d'une direction de défilement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE,
   "Interface de bureau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT,
   "Lancer l'interface de bureau au démarrage"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE,
   "Barre de menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DESKTOP_MENU_ENABLE,
   "Interface de bureau (Redémarrage requis)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UI_COMPANION_TOGGLE,
   "Ouvrir l'interface de bureau au démarrage"
   )

/* Settings > User Interface > Menu Item Visibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_VIEWS_SETTINGS,
   "Menu rapide"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_VIEWS_SETTINGS,
   "Afficher ou masquer des éléments dans le menu rapide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_VIEWS_SETTINGS,
   "Réglages"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_VIEWS_SETTINGS,
   "Afficher ou masquer des éléments dans le menu des réglages."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CORE,
   "Afficher 'Charger un cœur'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CORE,
   "Afficher l'option 'Charger un cœur' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_CONTENT,
   "Afficher 'Charger du contenu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_CONTENT,
   "Afficher l'option 'Charger du contenu' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LOAD_DISC,
   "Afficher 'Charger un disque'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LOAD_DISC,
   "Afficher l'option 'Charger un disque' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_DUMP_DISC,
   "Afficher 'Importer un disque'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_DUMP_DISC,
   "Afficher l'option 'Importer un disque' dans le menu principal."
   )
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_EJECT_DISC,
   "Afficher 'Éjecter le disque'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_EJECT_DISC,
   "Afficher l'option 'Éjecter le disque' dans le menu principal."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_ONLINE_UPDATER,
   "Afficher 'Mise à jour en ligne'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_ONLINE_UPDATER,
   "Afficher l'option 'Mise à jour en ligne' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CORE_UPDATER,
   "Afficher 'Téléchargement de cœurs'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CORE_UPDATER,
   "Afficher la possibilité de mettre à jour les cœurs (et les fichiers d'information des cœurs) dans l'option 'Mise à jour en ligne'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Afficher l'ancienne méthode de mise à jour des miniatures."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_LEGACY_THUMBNAIL_UPDATER,
   "Afficher l'entrée obsolète pour télécharger les packs de miniatures dans l'option 'Mise à jour en ligne'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_INFORMATION,
   "Afficher 'Informations'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_INFORMATION,
   "Afficher l'option 'Informations' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_CONFIGURATIONS,
   "Afficher 'Fichiers de configuration'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_CONFIGURATIONS,
   "Afficher l'option 'Fichiers de configuration' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_HELP,
   "Afficher 'Aide'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_HELP,
   "Afficher l'option 'Aide' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_QUIT_RETROARCH,
   "Afficher 'Quitter RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_QUIT_RETROARCH,
   "Afficher l'option 'Quitter RetroArch' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_RESTART_RETROARCH,
   "Afficher 'Redémarrer RetroArch'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_RESTART_RETROARCH,
   "Afficher l'option 'Redémarrer RetroArch' dans le menu principal."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS,
   "Afficher 'Réglages'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS,
   "Afficher le menu 'Réglages'. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_SETTINGS_PASSWORD,
   "Définir le mot de passe pour l'activation de 'Réglages' "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_SETTINGS_PASSWORD,
   "La saisie d'un mot de passe en masquant l'onglet des paramètres permet de le restaurer ultérieurement, en accédant à l'onglet Menu principal, en sélectionnant 'Activer l'onglet 'Réglages'' et en entrant le mot de passe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_FAVORITES,
   "Afficher 'Favoris'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_FAVORITES,
   "Afficher le menu 'Favoris'. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_IMAGES,
   "Afficher 'Images'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_IMAGES,
   "Afficher le menu 'Images'. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_MUSIC,
   "Afficher 'Musique'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_MUSIC,
   "Afficher le menu 'Musique'. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO,
   "Afficher 'Vidéo'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO,
   "Afficher le menu 'Vidéo'. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_NETPLAY,
   "Afficher 'Jeu en réseau'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_NETPLAY,
   "Afficher le menu 'Jeu en réseau'. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_HISTORY,
   "Afficher 'Historique'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_HISTORY,
   "Afficher le menu de l'historique récent. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD,
   "Afficher 'Importer du contenu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD,
   "Afficher le menu 'Importer du contenu'. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH( /* FIXME can now be replaced with MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD */
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_ADD_ENTRY,
   "Afficher 'Importer du contenu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_ADD_ENTRY,
   "Afficher une entrée 'Importer du contenu' dans le menu principal ou le sous-menu des listes de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_MAIN_TAB,
   "Menu principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_ADD_CONTENT_ENTRY_DISPLAY_PLAYLISTS_TAB,
   "Menu des listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_PLAYLISTS,
   "Afficher 'Listes de lecture'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_PLAYLISTS,
   "Afficher les listes de lecture. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_EXPLORE,
   "Afficher 'Parcourir'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_EXPLORE,
   "Afficher l'option pour parcourir le contenu. (Redémarrage requis sur Ozone/XMB)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_CONTENTLESS_CORES,
   "Afficher 'Cœurs autonomes'"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_ALL,
   "Tous"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_SINGLE_PURPOSE,
   "Usage unique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHOW_CONTENTLESS_CORES_CUSTOM,
   "Personnalisé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE,
   "Afficher la date/l'heure"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_ENABLE,
   "Afficher la date et/ou l'heure locale dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_STYLE,
   "Style d'affichage de la date/l'heure "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_STYLE,
   "Modifier le style dans lequel la date et/ou l'heure sont affichées dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DATE_SEPARATOR,
   "Séparateur de date "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEDATE_DATE_SEPARATOR,
   "Spécifier le caractère à utiliser comme séparateur entre les composants année/mois/jour lorsque la date actuelle est affichée dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BATTERY_LEVEL_ENABLE,
   "Afficher le niveau de la batterie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BATTERY_LEVEL_ENABLE,
   "Afficher le niveau actuel de la batterie dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ENABLE,
   "Afficher le nom du cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ENABLE,
   "Afficher le nom du cœur actuel dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SUBLABELS,
   "Afficher la description des éléments dans le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SUBLABELS,
   "Afficher des informations supplémentaires pour les éléments du menu."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN,
   "Afficher l'écran de configuration initiale"
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_SHOW_START_SCREEN,
   "Affiche à nouveau l'écran de configuration initiale dans le menu au prochain lancement. Cette option est automatiquement désactivée après le premier démarrage du programme."
   )

/* Settings > User Interface > Menu Item Visibility > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Afficher 'Reprendre'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESUME_CONTENT,
   "Afficher l'option pour reprendre le contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Afficher 'Redémarrer'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESTART_CONTENT,
   "Afficher l'option pour redémarrer le contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Afficher 'Fermer le contenu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CLOSE_CONTENT,
   "Afficher l'option 'Fermer le contenu'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Afficher 'Capturer l'écran'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_TAKE_SCREENSHOT,
   "Afficher l'option 'Capturer l'écran'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Afficher le chargement/l'enregistrement de sauvegardes instantanées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_LOAD_STATE,
   "Afficher les options pour charger/enregistrer une sauvegarde instantanée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Afficher l'annulation du chargement/de l'enregistrement des sauvegardes instantanées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_UNDO_SAVE_LOAD_STATE,
   "Afficher les options pour annuler le chargement/l'enregistrement d'une sauvegarde instantanée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Afficher 'Ajouter aux favoris'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_ADD_TO_FAVORITES,
   "Afficher l'option 'Ajouter aux favoris'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_RECORDING,
   "Afficher 'Lancer l'enregistrement'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_RECORDING,
   "Afficher l'option 'Lancer l'enregistrement'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_START_STREAMING,
   "Afficher 'Lancer le streaming'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_START_STREAMING,
   "Afficher l'option 'Lancer le streaming'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Afficher 'Associer au cœur'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SET_CORE_ASSOCIATION,
   "Afficher l'option 'Associer au cœur'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Afficher 'Réinitialiser l'association au cœur'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_RESET_CORE_ASSOCIATION,
   "Afficher l'option 'Réinitialiser l'association au cœur'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_OPTIONS,
   "Afficher 'Options'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_OPTIONS,
   "Afficher l'option 'Options'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Afficher 'Enregistrer les options sur le disque'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CORE_OPTIONS_FLUSH,
   "Afficher l'entrée 'Enregistrer les options sur le disque' dans le menu 'Options > Gérer les options du cœur'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CONTROLS,
   "Afficher 'Contrôles'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CONTROLS,
   "Afficher l'option 'Contrôles'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_CHEATS,
   "Afficher 'Cheats'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_CHEATS,
   "Afficher l'option 'Cheats'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SHADERS,
   "Afficher 'Shaders'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SHADERS,
   "Afficher l'option 'Shaders'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_REWIND,
   "Afficher 'Rembobinage'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_REWIND,
   "Afficher les options de 'Rembobinage'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_LATENCY,
   "Afficher 'Latence'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_LATENCY,
   "Afficher l'option 'Latence'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_OVERLAYS,
   "Afficher 'Affichage à l'écran'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_OVERLAYS,
   "Afficher l'option 'Affichage à l'écran'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_SHOW_VIDEO_LAYOUT,
   "Afficher 'Dispositions d'affichage'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_SHOW_VIDEO_LAYOUT,
   "Afficher l'option 'Dispositions d'affichage'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Afficher la sauvegarde du remplacement de configuration pour le cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_CORE_OVERRIDES,
   "Afficher l'option 'Sauvegarder le remplacement de configuration pour le cœur' dans le menu 'Remplacements de configuration'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Afficher 'Sauvegarder le remplacement de configuration pour le jeu'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_SAVE_GAME_OVERRIDES,
   "Afficher l'option 'Sauvegarder le remplacement de configuration pour le jeu' dans le menu 'Remplacements de configuration'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_INFORMATION,
   "Afficher 'Informations'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_INFORMATION,
   "Afficher l'option 'Informations'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Afficher 'Télécharger les miniatures'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_SHOW_DOWNLOAD_THUMBNAILS,
   "Afficher l'option 'Télécharger les miniatures'."
   )

/* Settings > User Interface > Views > Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DRIVERS,
   "Afficher 'Pilotes'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DRIVERS,
   "Afficher les réglages pour 'Pilotes'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_VIDEO,
   "Afficher 'Vidéo'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_VIDEO,
   "Afficher les réglages pour 'Vidéo'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AUDIO,
   "Afficher 'Audio'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AUDIO,
   "Afficher les réglages pour 'Audio'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_INPUT,
   "Afficher 'Entrées'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_INPUT,
   "Afficher les réglages pour 'Entrées'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LATENCY,
   "Afficher 'Latence'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LATENCY,
   "Afficher les réglages pour 'Latence'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CORE,
   "Afficher 'Cœurs'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CORE,
   "Afficher les réglages pour 'Cœurs'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_CONFIGURATION,
   "Afficher 'Configuration'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_CONFIGURATION,
   "Afficher les réglages pour 'Configuration'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_SAVING,
   "Afficher 'Sauvegarde'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_SAVING,
   "Afficher les réglages pour 'Sauvegarde'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_LOGGING,
   "Afficher 'Journalisation'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_LOGGING,
   "Afficher les réglages pour 'Journalisation'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FILE_BROWSER,
   "Afficher 'Navigateur de fichiers'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FILE_BROWSER,
   "Afficher les réglages pour 'Navigateur de fichiers'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_FRAME_THROTTLE,
   "Afficher 'Limiteur d'images/s'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_FRAME_THROTTLE,
   "Afficher les réglages pour 'Limiteur d'images/s'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_RECORDING,
   "Afficher 'Enregistrement'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_RECORDING,
   "Afficher les réglages pour 'Enregistrement'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Afficher 'Affichage à l'écran'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ONSCREEN_DISPLAY,
   "Afficher les réglages pour 'Affichage à l'écran'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER_INTERFACE,
   "Afficher 'Interface utilisateur'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER_INTERFACE,
   "Afficher les réglages pour 'Interface utilisateur'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_AI_SERVICE,
   "Afficher 'Service AI'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_AI_SERVICE,
   "Afficher les réglages pour 'Service AI'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACCESSIBILITY,
   "Afficher 'Accessibilité'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACCESSIBILITY,
   "Afficher les réglages pour 'Accessibilité'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Afficher 'Gestion de l'alimentation'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_POWER_MANAGEMENT,
   "Afficher les réglages pour 'Gestion de l'alimentation'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_ACHIEVEMENTS,
   "Afficher 'Succès'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_ACHIEVEMENTS,
   "Afficher les réglages pour 'Succès'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_NETWORK,
   "Afficher 'Réseau'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_NETWORK,
   "Afficher les réglages pour 'Réseau'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_PLAYLISTS,
   "Afficher 'Listes de lecture'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_PLAYLISTS,
   "Afficher les réglages pour 'Listes de lecture'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_USER,
   "Afficher 'Utilisateur'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_USER,
   "Afficher les réglages pour 'Utilisateur'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SETTINGS_SHOW_DIRECTORY,
   "Afficher 'Dossiers'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SETTINGS_SHOW_DIRECTORY,
   "Afficher les réglages pour 'Dossiers'."
   )

/* Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SCALE_FACTOR,
   "Facteur de mise à l'échelle du menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SCALE_FACTOR,
   "Mettre à l'échelle les éléments de l'interface utilisateur dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER,
   "Image d'arrière-plan "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER,
   "Sélectionner une image à définir comme arrière-plan du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY,
   "Opacité de l'arrière-plan "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WALLPAPER_OPACITY,
   "Modifier l'opacité de l'image d'arrière-plan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FRAMEBUFFER_OPACITY,
   "Opacité de l'image en mémoire (Frame Buffer) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_FRAMEBUFFER_OPACITY,
   "Modifier l'opacité de l'image en mémoire (Frame Buffer)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Utiliser le thème de couleur préféré du système"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_USE_PREFERRED_SYSTEM_COLOR_THEME,
   "Utiliser le thème de couleur du système d'exploitation (le cas échéant). Remplace les réglages du thème."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS,
   "Miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS,
   "Type de miniatures à afficher."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Seuil de l'agrandissement des miniatures "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_THUMBNAIL_UPSCALE_THRESHOLD,
   "Agrandit automatiquement les miniatures à une largeur/hauteur inférieure à la valeur spécifiée. Améliore la qualité de l'image. A un impact modéré sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE,
   "Animation du défilement de texte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_TYPE,
   "Sélectionne la méthode de défilement horizontal utilisée pour l'affichage de texte trop long dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SPEED,
   "Vitesse de défilement du texte "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SPEED,
   "Vitesse de l'animation pour l'affichage de texte trop long dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_SMOOTH,
   "Animation du défilement de texte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_TICKER_SMOOTH,
   "Utiliser une animation lisse pour l'affichage de texte trop long dans le menu. A un faible impact sur les performances."
   )

/* Settings > AI Service */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_MODE,
   "Sortie du service IA "
   )
MSG_HASH( /* FIXME What does the Narrator mode do? */
   MENU_ENUM_SUBLABEL_AI_SERVICE_MODE,
   "Afficher la traduction en tant que surimpression de texte (mode image), ou lue en tant que Text-To-Speech (mode parole)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_URL,
   "URL du service IA "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_URL,
   "Une URL http:// pointant vers le service de traduction à utiliser."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_ENABLE,
   "Service IA activé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_ENABLE,
   "Active le lancement du service IA lorsque la touche de raccourci Service AI est pressée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_PAUSE,
   "Mettre en pause lors de la traduction "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_PAUSE,
   "Mettre le cœur en pause lorsque l'écran est traduit."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SOURCE_LANG,
   "Langue source "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_SOURCE_LANG,
   "Langue à partir de laquelle le service traduira. Si réglé sur 'Par défaut', il tentera de détecter automatiquement la langue. Définir une langue spécifique rendra la traduction plus précise."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_TARGET_LANG,
   "Langue cible "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AI_SERVICE_TARGET_LANG,
   "La langue vers laquelle le service va traduire. 'Par défaut' est anglais."
   )

/* Settings > Accessibility */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_ENABLED,
   "Accessibilité activée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_ENABLED,
   "Activer la synthèse vocale pour aider à la navigation dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Débit de la synthèse vocale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCESSIBILITY_NARRATOR_SPEECH_SPEED,
   "Débit vocal pour la voix de la synthèse vocale."
   )

/* Settings > Power Management */

/* Settings > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ENABLE,
   "Succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_ENABLE,
   "Gagnez des succès dans les jeux rétro. Pour plus d'informations, visitez 'https://retroachievements.org'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Mode Hardcore"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Double le nombre de points gagnés. Désactive les sauvegardes instantanées, les cheats, le rembobinage, la mise en pause et le ralenti pour tous les jeux. Cette option redémarrera le jeu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LEADERBOARDS_ENABLE,
   "Classements "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_LEADERBOARDS_ENABLE,
   "Classements spécifiques au jeu. N'a aucun effet si le mode Hardcore est désactivé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RICHPRESENCE_ENABLE,
   "Présence enrichie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_RICHPRESENCE_ENABLE,
   "Envoyer les détails sur l'état de jeu au site web des RetroSuccès (RetroAchievements)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_BADGES_ENABLE,
   "Badges de succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_BADGES_ENABLE,
   "Affiche les badges dans la liste des succès."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL,
   "Tester les succès non-officiels"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_TEST_UNOFFICIAL,
   "Utiliser des succès non officiels et/ou fonctionnalités bêta à des fins de test."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Son de succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_UNLOCK_SOUND_ENABLE,
   "Jouer un son lorsqu'un succès est débloqué."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_VERBOSE_ENABLE,
   "Mode verbeux"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_VERBOSE_ENABLE,
   "Affiche plus d'informations dans les notifications."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_AUTO_SCREENSHOT,
   "Capture d'écran automatique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_AUTO_SCREENSHOT,
   "Prendre automatiquement une capture d'écran lorsqu'un succès est débloqué."
   )
MSG_HASH( /* suggestion for translators: translate as 'Play Again Mode' */
   MENU_ENUM_LABEL_VALUE_CHEEVOS_START_ACTIVE,
   "Mode \"encore\""
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_START_ACTIVE,
   "Démarrer la session avec tous les succès actifs (les succès précédemment débloqués inclus)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_CHALLENGE_INDICATORS,
   "Indicateurs de défi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_CHALLENGE_INDICATORS,
   "Permet aux succès d'afficher un indicateur à l'écran lorsqu'ils peuvent être gagnés."
   )

/* Settings > Network */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PUBLIC_ANNOUNCE,
   "Annoncer le jeu en réseau publiquement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PUBLIC_ANNOUNCE,
   "Détermine s'il faut annoncer les sessions de jeu en réseau publiquement. Si cette option est désactivée, les clients doivent se connecter manuellement plutôt que d'utiliser le salon public."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_USE_MITM_SERVER,
   "Utiliser un serveur relais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_USE_MITM_SERVER,
   "Transférer les connexions de jeu en réseau via un serveur intermédiaire. Utile si l'hôte est derrière un pare-feu ou a des problèmes de NAT/UPnP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MITM_SERVER,
   "Emplacement du serveur relais"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MITM_SERVER,
   "Choisissez un serveur de relais spécifique à utiliser. Les zones géographiques plus proches ont tendance à avoir une latence plus faible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CUSTOM_MITM_SERVER,
   "Adresse du serveur de relais personnalisé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CUSTOM_MITM_SERVER,
   "Entrer l'adresse de votre serveur de relais personnalisé. Format : adresse ou adresse|port."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS,
   "Adresse du serveur "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_IP_ADDRESS,
   "Adresse de l'hôte auquel se connecter."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT,
   "Port TCP du jeu en réseau "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_TCP_UDP_PORT,
   "Port de l'adresse IP de l'hôte. Peut être un port TCP ou UDP."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_CONNECTIONS,
   "Connexions simultanées maximales"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_CONNECTIONS,
   "Le nombre maximum de connexions actives que l'hôte acceptera avant de refuser les nouvelles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MAX_PING,
   "Limitation du ping"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_MAX_PING,
   "La latence maximale de connexion (ping) que l'hôte acceptera. Définissez-la à 0 pour aucune limite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_PASSWORD,
   "Mot de passe du serveur "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_PASSWORD,
   "Mot de passe utilisé par les clients pour se connecter à l'hôte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATE_PASSWORD,
   "Mot de passe du serveur pour les spectateurs "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_SPECTATE_PASSWORD,
   "Mot de passe utilisé par les clients pour se connecter à l'hôte en tant que spectateur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_AS_SPECTATOR,
   "Mode spectateur de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_START_AS_SPECTATOR,
   "Démarre le jeu en réseau en mode spectateur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_FADE_CHAT,
   "Chat en fondu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_FADE_CHAT,
   "Estomper les messages du chat au fil du temps."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_PAUSING,
   "Autoriser la mise en pause"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_PAUSING,
   "Permettre aux joueurs de mettre en pause pendant le jeu en réseau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ALLOW_SLAVES,
   "Autoriser les clients en mode passif"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ALLOW_SLAVES,
   "Autorise les connexions en mode passif. Les clients en mode passif nécessitent très peu de puissance de traitement de part et d'autre, mais souffrent considérablement de la latence du réseau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUIRE_SLAVES,
   "Interdire les clients non passifs"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUIRE_SLAVES,
   "Interdit les connexions qui ne sont pas en mode passif. Non recommandé sauf pour les réseaux très rapides avec des machines très faibles."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_STATELESS_MODE,
   "Mode sans état de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_STATELESS_MODE,
   "Lance le jeu en réseau dans un mode ne nécessitant pas de sauvegardes instantanées. Un réseau très rapide est requis, mais aucun rembobinage ne sera effectué, il n'y aura donc pas de variations de la latence lors du jeu en réseau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES,
   "Vérifier la latence par image du jeu en réseau "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_CHECK_FRAMES,
   "Fréquence (en images) à laquelle le jeu en réseau vérifiera que l'hôte et le client sont synchronisés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Latence d'entrées minimale "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN,
   "Nombre d'images de latence des entrées que le jeu en réseau doit utiliser pour masquer la latence du réseau. Réduit les variations de la latence et rend le jeu en réseau moins gourmand en ressources processeur, aux dépens d'une latence des entrées notable."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Intervalle de latence d'entrées "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE,
   "Plage d'images de latence des entrées pouvant être utilisée pour masquer la latence du réseau. Réduit les variations de la latence et rend le jeu en réseau moins gourmand en ressources processeur, aux dépens d'une latence des entrées imprévisible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NAT_TRAVERSAL,
   "Traversée du NAT pour le jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NAT_TRAVERSAL,
   "Lors de l'hébergement, tenter d'intercepter des connexions depuis l'internet public, en utilisant UPnP ou des technologies similaires pour sortir du réseau local."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL,
   "Partage des entrées numériques "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REQUEST_DEVICE_I,
   "Demander le périphérique %u"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REQUEST_DEVICE_I,
   "Demander à jouer avec le périphérique d'entrée donné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE,
   "Commandes réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT,
   "Port des commandes réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE,
   "RetroManette en réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT,
   "Port de base de la RetroManette en réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_USER_REMOTE_ENABLE,
   "RetroManette en réseau de l'utilisateur %d"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE,
   "Commandes stdin"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STDIN_CMD_ENABLE,
   "Interface de commandes stdin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ON_DEMAND_THUMBNAILS,
   "Télécharger les miniatures à la demande"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETWORK_ON_DEMAND_THUMBNAILS,
   "Télécharge automatiquement les miniatures manquantes lors de la navigation dans les listes de lecture. Affecte grandement les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS,
   "Mise à jour"
   )

/* Settings > Network > Updater */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL,
   "Adresse URL des cœurs sur le Buildbot "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_BUILDBOT_URL,
   "URL du dossier de mise à jour des cœurs sur le buildbot de libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL,
   "Adresse URL des assets sur le Buildbot "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BUILDBOT_ASSETS_URL,
   "URL du dossier de mise à jour des assets sur le buildbot de libretro."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Extraire automatiquement les archives téléchargées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE,
   "Après le téléchargement, extraire automatiquement les fichiers contenus dans les archives téléchargées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Afficher les cœurs expérimentaux"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_SHOW_EXPERIMENTAL_CORES,
   "Inclure les cœurs 'expérimentaux' dans la liste de 'Téléchargement de cœurs'. Ils sont généralement destinés à des fins de développement/test uniquement, et ne sont pas recommandés pour une utilisation générale."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP,
   "Sauvegarder les cœurs lors de la mise à jour"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP,
   "Crée automatiquement une sauvegarde de tous les cœurs installés lors d'une mise à jour en ligne. Permet un retour facile vers un cœur fonctionnel si une mise à jour introduit une régression."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Taille de l'historique de sauvegarde des cœurs "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_UPDATER_AUTO_BACKUP_HISTORY_SIZE,
   "Spécifier combien de sauvegardes générées automatiquement pour chaque cœur installé seront conservées. Lorsque cette limite est atteinte, la création d'une nouvelle sauvegarde via une mise à jour en ligne supprimera la sauvegarde la plus ancienne. Les cœurs sauvegardés manuellement ne sont pas affectées par ce paramètre."
   )

/* Settings > Playlists */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE,
   "Historique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HISTORY_LIST_ENABLE,
   "Conserver une liste de lecture des jeux, images, musiques et vidéos récemment utilisés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE,
   "Taille de l'historique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_SIZE,
   "Limiter le nombre d'entrées dans la liste de lecture des jeux, images, musiques et vidéos récemment utilisés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_SIZE,
   "Taille de la liste des favoris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_SIZE,
   "Limite le nombre d'entrées dans la liste 'Favoris'. Une fois la limite atteinte, les nouveaux ajouts seront empêchés à moins que d'anciens éléments ne soient supprimés. Définir une valeur de -1 permet un nombre d'entrées 'illimitées'.\nATTENTION : Diminuer la valeur effacera des entrées existantes !"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_RENAME,
   "Autoriser à renommer des entrées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_RENAME,
   "Autoriser le renommage d'entrées dans les listes de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE,
   "Autoriser la suppression d'entrées"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_ENTRY_REMOVE,
   "Autoriser la suppression d'entrées dans les listes de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SORT_ALPHABETICAL,
   "Trier les listes de lecture par ordre alphabétique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SORT_ALPHABETICAL,
   "Trier les listes de lecture de contenu par ordre alphabétique, excluant les listes 'Historique', 'Images', 'Musique' et 'Vidéo'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_USE_OLD_FORMAT,
   "Sauvegarder les listes de lecture dans l'ancien format"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_USE_OLD_FORMAT,
   "Sauvegarder les listes de lecture au format texte brut déprécié. Si désactivé, les listes de lecture seront au format JSON."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_COMPRESSION,
   "Compression des listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_COMPRESSION,
   "Enregistrer les données des listes de lecture sous un format archivé. Réduit la taille du fichier et les temps de sauvegarde/chargement au détriment de l'augmentation (négligeable) de l'utilisation du processeur. Peut être utilisé avec l'ancien et le nouveau format des listes de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Afficher les cœurs associés dans les listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_INLINE_CORE_NAME,
   "Indique quand marquer les entrées de la liste de lecture avec leur cœur actuellement associé (le cas échéant).\nCe réglage sera ignoré si les sous-étiquettes de la liste de lecture sont activées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_SUBLABELS,
   "Afficher les sous-étiquettes dans les listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_SUBLABELS,
   "Afficher des informations additionnelles pour chaque entrée dans les listes de lecture, telles que l'association au cœur actuelle et le temps de jeu (si disponible). A un impact variable sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_HISTORY_ICONS,
   "Afficher les icônes spécifiques au contenu dans l'historique et les favoris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_HISTORY_ICONS,
   "Afficher des icônes spécifiques pour chaque élément des listes de lecture d'historique et des favoris. A un impact variable sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_CORE,
   "Cœur :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME,
   "Temps de jeu :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED,
   "Joué pour la dernière fois :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SHOW_ENTRY_IDX,
   "Afficher l'index des entrées de la liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SHOW_ENTRY_IDX,
   "Afficher les numéros d'entrées lors de l'affichage des listes de lecture. Le format d'affichage dépend du pilote de menu actuellement sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Type du temps de jeu affiché dans les sous-étiquettes de listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_RUNTIME_TYPE,
   "Sélectionner le type d'enregistrement du temps de jeu à afficher dans les sous-étiquettes des listes de lecture.\nLe journal du temps de jeu correspondant doit être activé via le menu d'options 'Sauvegarde'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Format du temps de jeu pour 'Joué pour la dernière fois'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_SUBLABEL_LAST_PLAYED_STYLE,
   "Régler le style de la date et l'heure affichées pour les informations d'horodatage dans 'Joué pour la dernière fois'. Les options '(AM/PM)' auront un faible impact sur les performances pour certaines plateformes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Correspondance approximative pour les archives"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_FUZZY_ARCHIVE_MATCH,
   "Lors de la recherche de fichiers compressés dans les listes de lecture, faire correspondre le nom de l'archive uniquement et non [nom]+[contenu]. Activez cette option pour éviter les doublons dans l'historique lors du chargement d'archives."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_WITHOUT_CORE_MATCH,
   "Analyser du contenu sans cœur correspondant"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_WITHOUT_CORE_MATCH,
   "Autoriser du contenu à être analysé et ajouté à une liste de lecture sans cœur installé qui le prenne en charge."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LIST,
   "Gérer les listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LIST,
   "Effectue des tâches de maintenance sur les listes de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_PORTABLE_PATHS,
   "Listes de lecture portables"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_PORTABLE_PATHS,
   "Lorsque cette option est activée et que le dossier par défaut du 'Navigateur de fichiers' est également défini, cet emplacement est enregistré dans la liste de lecture. Lorsque la liste de lecture est chargée sur un autre système où cette même option est activée, la valeur du dossier défini par défaut est comparée à la valeur dans la liste de lecture; si celle-ci diffère, l'emplacement des entrées de la liste de lecture est automatiquement corrigé."
   )

/* Settings > Playlists > Playlist Management */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Cœur par défaut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_DEFAULT_CORE,
   "Spécifie le cœur à utiliser lors du lancement de contenu via une entrée de liste de lecture qui n'est pas déjà associée à un cœur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_RESET_CORES,
   "Réinitialiser les associations au cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_RESET_CORES,
   "Supprimer les associations au cœur existantes pour toutes les entrées de la liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Mode d'affichage des titres"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE,
   "Modifier la façon dont les descriptions de contenu sont affichées dans cette liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE,
   "Trier par"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_SORT_MODE,
   "Déterminer l'ordre de tri dans cette liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Nettoyer la liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_CLEAN_PLAYLIST,
   "Valider les associations aux cœurs et supprimer les entrées invalides/en double."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Rafraîchir la liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_MANAGER_REFRESH_PLAYLIST,
   "Ajouter le nouveau contenu et retirer les entrées non valides en répétant l'opération 'Analyse manuelle' utilisée pour créer ou éditer la liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_PLAYLIST,
   "Supprimer la liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_PLAYLIST,
   "Supprime la liste de lecture du système de fichiers."
   )

/* Settings > User */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS,
   "Confidentialité"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PRIVACY_SETTINGS,
   "Modifier les réglages de confidentialité."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST,
   "Comptes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_LIST,
   "Gérer les comptes actuellement configurés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME,
   "Pseudo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_NICKNAME,
   "Entrez votre pseudo ici. Il sera utilisé pour les sessions de jeu en réseau, entre autres."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER_LANGUAGE,
   "Langue "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_USER_LANGUAGE,
   "Définir la langue de l'interface utilisateur."
   )

/* Settings > User > Privacy */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW,
   "Autoriser l'appareil photo"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CAMERA_ALLOW,
   "Autoriser les cœurs à accéder à l'appareil photo."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_ALLOW,
   "Présence enrichie sur Discord"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISCORD_ALLOW,
   "Permettre à l'application Discord d'afficher des données sur le contenu joué.\nUniquement disponible avec le client de bureau natif."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW,
   "Autoriser la géolocalisation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCATION_ALLOW,
   "Autoriser les cœurs à accéder à votre position."
   )

/* Settings > User > Accounts */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "RetroSuccès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACCOUNTS_RETRO_ACHIEVEMENTS,
   "Gagnez des succès dans les jeux rétro. Pour plus d'informations, visitez 'https://retroachievements.org'."
   )

/* Settings > User > Accounts > RetroAchievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME,
   "Identifiant "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_USERNAME,
   "Entrez votre nom d'utilisateur de compte RetroSuccès (RetroAchievements)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD,
   "Mot de passe "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEEVOS_PASSWORD,
   "Entrez le mot de passe de votre compte RetroSuccès (RetroAchievements). Longueur maximale : 255 caractères."
   )

/* Settings > User > Accounts > YouTube */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YOUTUBE_STREAM_KEY,
   "Clé de streaming YouTube "
   )

/* Settings > User > Accounts > Twitch */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TWITCH_STREAM_KEY,
   "Clé de streaming Twitch "
   )

/* Settings > User > Accounts > Facebook Gaming */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FACEBOOK_STREAM_KEY,
   "Clé de stream Facebook Gaming "
   )

/* Settings > Directory */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY,
   "Système/BIOS "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SYSTEM_DIRECTORY,
   "Les BIOS, les ROM de démarrage et les autres fichiers spécifiques au système sont stockés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY,
   "Téléchargements "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_ASSETS_DIRECTORY,
   "Les fichiers téléchargés sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY,
   "Assets "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ASSETS_DIRECTORY,
   "Les assets de menu utilisés par RetroArch sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Arrière-plans dynamiques "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPERS_DIRECTORY,
   "Les images de fond utilisées dans le menu sont conservées dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY,
   "Miniatures "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_DIRECTORY,
   "Les images de pochettes, de capture d'écran et de miniatures d'écran de titre sont conservées dans ce dossier."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY,
   "Navigateur de fichiers "
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_BROWSER_DIRECTORY,
   "Définir le dossier de départ du navigateur de fichiers."
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY,
   "Fichiers de configuration "
   )
MSG_HASH( /* FIXME Not RGUI specific */
   MENU_ENUM_SUBLABEL_RGUI_CONFIG_DIRECTORY,
   "Définir le dossier de départ du navigateur de configurations du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH,
   "Cœurs "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_DIR_PATH,
   "Les cœurs Libretro sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH,
   "Informations des cœurs "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LIBRETRO_INFO_PATH,
   "Les fichiers d'informations de l'application/des cœurs sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY,
   "Bases de données "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DATABASE_DIRECTORY,
   "Les bases de données sont conservées dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY,
   "Pointeurs "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CURSOR_DIRECTORY,
   "Les requêtes de curseur sont conservées dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH,
   "Fichier de cheats "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_DATABASE_PATH,
   "Les fichiers de cheats sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR,
   "Filtres vidéo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_FILTER_DIR,
   "Les filtres vidéo produits par le processeur sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR,
   "Filtres audio "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_AUDIO_FILTER_DIR,
   "Les filtres audio DSP sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR,
   "Shaders vidéo "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_DIR,
   "Les shaders vidéo produits par le processeur graphique sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY,
   "Enregistrements "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_OUTPUT_DIRECTORY,
   "Les enregistrements sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY,
   "Configurations d'enregistrement "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RECORDING_CONFIG_DIRECTORY,
   "Les configurations d'enregistrement sont conservées ici."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY,
   "Surimpressions à l'écran "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OVERLAY_DIRECTORY,
   "Les surimpressions sont conservées dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_LAYOUT_DIRECTORY,
   "Dispositions d'affichage "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_LAYOUT_DIRECTORY,
   "Les dispositions d'affichage sont conservées dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY,
   "Captures d'écran "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREENSHOT_DIRECTORY,
   "Les captures d'écran sont conservées dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR,
   "Profils de manettes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_JOYPAD_AUTOCONFIG_DIR,
   "Les profils de manettes utilisés pour la configuration automatique sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY,
   "Remappages des touches "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_REMAPPING_DIRECTORY,
   "Les fichiers de remappage des touches sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY,
   "Listes de lecture "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_PLAYLIST_DIRECTORY,
   "Les listes de lecture sont conservées dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_FAVORITES_DIRECTORY,
   "Liste des favoris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_FAVORITES_DIRECTORY,
   "La liste des favoris sera conservée dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_DIRECTORY,
   "Liste de l'historique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_HISTORY_DIRECTORY,
   "La liste de l'historique sera conservée dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "Liste des images"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_IMAGE_HISTORY_DIRECTORY,
   "La liste des images sera conservée dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "Liste des musiques"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_MUSIC_HISTORY_DIRECTORY,
   "La liste des musiques sera conservée dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "Liste des vidéos"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_VIDEO_HISTORY_DIRECTORY,
   "La liste des vidéos sera conservée dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUNTIME_LOG_DIRECTORY,
   "Journaux du temps de jeu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUNTIME_LOG_DIRECTORY,
   "Les journaux d'exécution sont conservés dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY,
   "Fichiers de sauvegarde "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVEFILE_DIRECTORY,
   "Les fichiers de sauvegarde seront conservés dans ce dossier. Si aucun dossier n'est défini, ils seront sauvegardés dans le même dossier que le contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY,
   "Sauvegardes instantanées "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVESTATE_DIRECTORY,
   "Les sauvegardes instantanées sont conservées dans ce dossier. Si non défini, RetroArch essayera de les enregistrer dans le dossier où se trouve le contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY,
   "Cache "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CACHE_DIRECTORY,
   "Le contenu archivé sera temporairement extrait dans ce dossier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOG_DIR,
   "Journaux des événements système "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOG_DIR,
   "Les journaux d'événements système sont conservés dans ce dossier."
   )

/* Music */

/* Music > Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER,
   "Ajouter au mixeur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER,
   "Ajouter cette piste audio dans un emplacement de diffusion audio disponible.\nSi aucun emplacement n'est disponible, elle sera ignorée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_PLAY,
   "Ajouter au mixeur et lire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_MIXER_AND_PLAY,
   "Ajouter cette piste audio dans un emplacement de diffusion audio disponible et la lire.\nSi aucun emplacement n'est disponible, elle sera ignorée."
   )

/* Netplay */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_HOSTING_SETTINGS,
   "Hôte"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_CLIENT,
   "Se connecter à l'hôte de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_CLIENT,
   "Entrer l'adresse du serveur de jeu en réseau et se connecter en mode client."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISCONNECT,
   "Se déconnecter de l'hôte de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_DISCONNECT,
   "Déconnecter une connexion de jeu en réseau active."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHOW_ONLY_CONNECTABLE,
   "Salons connectables uniquement"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_ROOMS,
   "Rafraîchir la liste des salons"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_ROOMS,
   "Recherche d'hôtes de jeu en réseau."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_REFRESH_LAN,
   "Rafraîchir la liste des LAN de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_REFRESH_LAN,
   "Rechercher des hôtes de jeu en réseau sur le LAN."
   )

/* Netplay > Host */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE_HOST,
   "Commencer à héberger le jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_ENABLE_HOST,
   "Démarrer le jeu en réseau en mode hôte (serveur)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DISABLE_HOST,
   "Arrêter l'hébergement de jeu en réseau"
   )

/* Import Content */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY,
   "Analyser un dossier"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_DIRECTORY,
   "Analyser un dossier pour trouver du contenu correspondant à la base de données."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_THIS_DIRECTORY,
   "<Analyser ce dossier>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCAN_FILE,
   "Analyser un fichier"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCAN_FILE,
   "Analyser un fichier pour trouver du contenu correspondant à la base de données."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_LIST,
   "Analyse manuelle"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_LIST,
   "Analyse de contenu configurable basée sur le nom des fichiers. Le contenu n'est pas forcé d'être présent dans la base de données."
   )

/* Import Content > Scan File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION,
   "Ajouter au mixeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_MIXER_AND_COLLECTION_AND_PLAY,
   "Ajouter au mixeur et lire"
   )

/* Import Content > Manual Scan */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DIR,
   "Dossier du contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DIR,
   "Sélectionner le dossier dans lequel rechercher du contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Nom du système"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME,
   "Specifie un 'Nom du système' avec lequel associer le contenu analysé. Utilisé pour le nom de la liste de lecture générée et pour identifier les miniatures de la liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Nom de système personnalisé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM,
   "Specifie manuellement un 'Nom du système' pour le contenu analysé. Utilisé uniquement lorsque le 'Nom du système' est réglé sur '<Personnalisé>'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Cœur par défaut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_CORE_NAME,
   "Sélectionne le cœur à utiliser par défaut au lancement du contenu analysé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Extensions de fichiers"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_FILE_EXTS,
   "Liste des types de fichiers à inclure lors de l'analyse, délimitée par espaces. Si laissée vide, inclut tous les fichiers, ou si un cœur est spécifié, tous les fichiers supportés par ce cœur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Analyser récursivement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_RECURSIVELY,
   "Lorsque cette option est activée, tous les sous-dossiers du 'Dossier du contenu' spécifié seront inclus dans l'analyse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Analyser le contenu des archives"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_SEARCH_ARCHIVES,
   "Si cette option est activée, la recherche dans les fichiers d'archive (.zip, .7z, etc.) sera active lors de la recherche de contenu valide/pris en charge. Peut avoir un fort impact sur les performances lors de l'analyse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Fichier DAT d'arcade"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE,
   "Sélectionne un fichier Logiqx ou MAME List XML DAT pour activer le renommage automatique du contenu arcade analysé (MAME, FinalBurn Neo, etc.)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Filtrer le contenu arcade par DAT"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_DAT_FILE_FILTER,
   "Lors de l'utilisation d'un fichier DAT d'arcade, ajouter du contenu à la liste de lecture uniquement si une entrée correspondante est trouvée dans le fichier DAT."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Remplacer la liste de lecture existante"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_OVERWRITE,
   "Si cette option est activée, les listes de lectures existantes seront supprimées avant d'analyser le contenu. Si elle est désactivée, les entrées dans les listes de lecture existantes seront préservées et seul le contenu actuellement manquant sera ajouté."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Valider les entrées existantes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_VALIDATE_ENTRIES,
   "Lorsque cette option est activée, les entrées de toute liste de lecture existante seront vérifiées avant d'analyser le nouveau contenu. Les entrées se rapportant à du contenu manquant et/ou à des fichiers avec des extensions non valides seront retirées."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_START,
   "Lancer l'analyse"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MANUAL_CONTENT_SCAN_START,
   "Analyse le contenu sélectionné."
   )

/* Explore tab */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_INITIALISING_LIST,
   "Liste en cours d'initialisation..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_RELEASE_YEAR,
   "Année de sortie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_PLAYER_COUNT,
   "Nombre de joueurs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_CATEGORY_REGION,
   "Région"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SEARCH_NAME,
   "Rechercher le nom ..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_SHOW_ALL,
   "Tout afficher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADDITIONAL_FILTER,
   "Filtre additionnel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ALL,
   "Tout"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ADD_ADDITIONAL_FILTER,
   "Ajouter un filtre additionnel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_ITEMS_COUNT,
   "%u éléments"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_DEVELOPER,
   "Par développeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PUBLISHER,
   "Par éditeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_RELEASE_YEAR,
   "Par année de sortie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_PLAYER_COUNT,
   "Par nombre de joueurs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_GENRE,
   "Par genre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_REGION,
   "Par région"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_ORIGIN,
   "Par origine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_FRANCHISE,
   "Par saga"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_TAG,
   "Par tag"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_EXPLORE_BY_SYSTEM_NAME,
   "Par nom du système"
   )

/* Playlist > Playlist Item */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN,
   "Lancer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RUN,
   "Démarre le contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RENAME_ENTRY,
   "Renommer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RENAME_ENTRY,
   "Renommer l'entrée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DELETE_ENTRY,
   "Supprimer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DELETE_ENTRY,
   "Supprimer cette entrée de la liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES_PLAYLIST,
   "Ajouter aux favoris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES_PLAYLIST,
   "Ajouter le contenu aux 'Favoris'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SET_CORE_ASSOCIATION,
   "Associer au cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESET_CORE_ASSOCIATION,
   "Réinitialiser l'association au cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INFORMATION,
   "Informations"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INFORMATION,
   "Affiche plus d'informations sur le contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Télécharger les miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DOWNLOAD_PL_ENTRY_THUMBNAILS,
   "Télécharge les miniatures des captures d'écran/pochettes/écrans titre pour le contenu actuel. Mets à jour toutes les miniatures existantes."
   )

/* Playlist Item > Set Core Association */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DETECT_CORE_LIST_OK_CURRENT_CORE,
   "Cœur actuel"
   )

/* Playlist Item > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LABEL,
   "Nom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_PATH,
   "Emplacement du fichier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_ENTRY_IDX,
   "Entrée : %lu/%lu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CORE_NAME,
   "Cœur"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_RUNTIME,
   "Temps de jeu"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_LAST_PLAYED,
   "Dernière partie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_DATABASE,
   "Base de données"
   )

/* Quick Menu */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME_CONTENT,
   "Reprendre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME_CONTENT,
   "Reprendre le contenu en cours et quitter le menu rapide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESTART_CONTENT,
   "Redémarrer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESTART_CONTENT,
   "Redémarrer le contenu depuis le début."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CLOSE_CONTENT,
   "Fermer le contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CLOSE_CONTENT,
   "Fermer le contenu actuel. Toute modification non enregistrée pourrait être perdue."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TAKE_SCREENSHOT,
   "Capturer l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TAKE_SCREENSHOT,
   "Capturer une image de l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATE_SLOT,
   "Emplacement de la sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_STATE_SLOT,
   "Modifier l'emplacement de sauvegarde instantanée actuellement sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_STATE,
   "Sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_STATE,
   "Effectue une sauvegarde instantanée dans l'emplacement actuellement sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_STATE,
   "Charger une sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_STATE,
   "Charge une sauvegarde instantanée depuis l'emplacement actuellement sélectionné."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_LOAD_STATE,
   "Annuler le chargement de sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_LOAD_STATE,
   "Si une sauvegarde instantanée a été chargée, le contenu reviendra à l'état avant le chargement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNDO_SAVE_STATE,
   "Annuler la sauvegarde instantanée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_UNDO_SAVE_STATE,
   "Si une sauvegarde instantanée a été écrasée, elle sera restaurée à l'état de sauvegarde précédent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ADD_TO_FAVORITES,
   "Ajouter aux favoris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ADD_TO_FAVORITES,
   "Ajouter le contenu à 'Favoris'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_RECORDING,
   "Lancer l'enregistrement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_RECORDING,
   "Lancer l'enregistrement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_RECORDING,
   "Arrêter l'enregistrement"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_RECORDING,
   "Arrêter l'enregistrement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_START_STREAMING,
   "Lancer le streaming"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_START_STREAMING,
   "Commencer à diffuser vers la destination choisie."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_STOP_STREAMING,
   "Arrêter le streaming"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_STOP_STREAMING,
   "Arrêter la diffusion."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS,
   "Modifier les options pour le contenu en cours d'exécution."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS,
   "Touches"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_INPUT_REMAPPING_OPTIONS,
   "Modifier les contrôles pour le contenu en cours d'exécution."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_CHEAT_OPTIONS,
   "Configurer des cheat codes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_OPTIONS,
   "Contrôle du disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_OPTIONS,
   "Gestionnaire d'images disque."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_OPTIONS,
   "Configurer des shaders pour améliorer visuellement l'image."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QUICK_MENU_OVERRIDE_OPTIONS,
   "Remplacements de configuration"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_QUICK_MENU_OVERRIDE_OPTIONS,
   "Options pour remplacer la configuration globale."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST,
   "Succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_LIST,
   "Voir les succès et les réglages associés."
   )

/* Quick Menu > Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_LIST,
   "Gérer les options du cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTION_OVERRIDE_LIST,
   "Enregistrer ou supprimer les remplacements de configuration pour le contenu actuel."
   )

/* Quick Menu > Options > Manage Core Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_CREATE,
   "Sauvegarder les options pour le jeu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Supprimer les options pour le jeu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_CREATE,
   "Sauvegarder les options pour le dossier du contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FOLDER_SPECIFIC_CORE_OPTIONS_REMOVE,
   "Supprimer les options pour le dossier du contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTION_OVERRIDE_INFO,
   "Fichier des options actives : "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_RESET,
   "Réinitialiser les options"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_RESET,
   "Définir toutes les options du cœur aux valeurs par défaut."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_OPTIONS_FLUSH,
   "Enregistrer les options sur le disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CORE_OPTIONS_FLUSH,
   "Force l'écriture des réglages actuels dans le fichier d'options actif. Permet de s'assurer que les options sont conservées dans le cas où un bug de cœur provoque une fermeture incorrecte de l'interface."
   )

/* - Legacy (unused) */
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_CREATE,
   "Créer un fichier d'options pour le jeu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS_IN_USE,
   "Sauvegarder le fichier d'options pour le jeu"
   )

/* Quick Menu > Controls */


/* Quick Menu > Controls > Manage Remap Files */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_LOAD,
   "Charger un fichier de remappage"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CORE,
   "Sauvegarder le remappage pour le cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CORE,
   "Supprimer le remappage pour le cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_CONTENT_DIR,
   "Sauvegarder le remappage pour le dossier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_CONTENT_DIR,
   "Supprimer le remappage pour le dossier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_SAVE_GAME,
   "Sauvegarder le remappage pour le jeu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE_REMOVE_GAME,
   "Supprimer le remappage pour le jeu"
   )

/* Quick Menu > Controls > Manage Remap Files > Load Remap File */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REMAP_FILE,
   "Fichier de remappage"
   )

/* Quick Menu > Cheats */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_CONT,
   "Lancer/continuer la recherche de cheats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD,
   "Charger des cheats (Remplacer)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD,
   "Charger un fichier de cheats et remplacer les cheats existants."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_LOAD_APPEND,
   "Charger des cheats (Ajouter)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_LOAD_APPEND,
   "Charger un fichier de cheats et l'ajouter aux cheats existants."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RELOAD_CHEATS,
   "Recharger les cheats du jeu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_SAVE_AS,
   "Enregistrer les cheats sous"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_FILE_SAVE_AS,
   "Enregistrer les cheats actuels en tant que fichier de cheats."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_TOP,
   "Ajouter un nouveau cheat (en haut)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BOTTOM,
   "Ajouter un nouveau cheat (en bas)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_ALL,
   "Supprimer tous les cheats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_LOAD,
   "Appliquer les cheats au chargement du jeu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_LOAD,
   "Appliquer automatiquement les cheats au chargement du jeu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_AFTER_TOGGLE,
   "Appliquer après l'activation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_AFTER_TOGGLE,
   "Appliquer les cheats immédiatement après l'activation."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_APPLY_CHANGES,
   "Appliquer les changements"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_APPLY_CHANGES,
   "Les changements du cheat prendront effet immédiatement."
   )

/* Quick Menu > Cheats > Start or Continue Cheat Search */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_OR_RESTART,
   "Lancer/redémarrer la recherche de cheats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_OR_RESTART,
   "Appuyez sur gauche ou droite pour modifier la taille de bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BIG_ENDIAN,
   "Gros-boutien"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_BIG_ENDIAN,
   "Gros-boutienne : 258 = 0x0102\nPetit-boutienne : 258 = 0x0201"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EXACT,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EXACT,
   "Appuyez sur gauche ou droite pour modifier la valeur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT_VAL,
   "Égale à %u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LT,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LT_VAL,
   "Inférieure à la précédente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_LTE,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_LTE_VAL,
   "Inférieure ou égale à la précédente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GT,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GT_VAL,
   "Supérieure à la précédente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_GTE,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_GTE_VAL,
   "Supérieure ou égale à la précédente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQ,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQ_VAL,
   "Égale à la précédente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_NEQ,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_NEQ_VAL,
   "Différente de la précédente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQPLUS,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQPLUS,
   "Appuyez sur gauche ou droite pour modifier la valeur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQPLUS_VAL,
   "Égale à la précédente+%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_EQMINUS,
   "Recherche d'une valeur mémoire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_SEARCH_EQMINUS,
   "Appuyez sur gauche ou droite pour modifier la valeur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS_VAL,
   "Égale à la précédente-%u (%X)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_MATCHES,
   "Ajouter les %u correspondances à votre liste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_MATCH,
   "Supprimer la correspondance #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_MATCH,
   "Créer une correspondance de code #"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH,
   "Adresse de la correspondance : %08X Masque : %02X"
   )

/* Quick Menu > Cheats > Load Cheat File (Replace) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE,
   "Fichier de cheats (Remplacer)"
   )

/* Quick Menu > Cheats > Load Cheat File (Append) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_FILE_APPEND,
   "Fichier de cheats (Ajouter)"
   )

/* Quick Menu > Cheats > Cheat Details */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DETAILS_SETTINGS,
   "Détails des cheats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_IDX,
   "Position du cheat dans la liste."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_STATE,
   "Activé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_HANDLER,
   "Gestionnaire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MEMORY_SEARCH_SIZE,
   "Taille de la recherche dans la mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VALUE,
   "Valeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS,
   "Adresse mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_BROWSE_MEMORY,
   "Parcourir l'adresse : %08X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADDRESS_BIT_POSITION,
   "Masque de l'adresse mémoire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_ADDRESS_BIT_POSITION,
   "Masque binaire d'adresse lorsque la taille de la recherche dans la mémoire est < 8 bits."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_COUNT,
   "Nombre d'itérations"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_COUNT,
   "Nombre de fois que le cheat sera appliqué. Utiliser avec les deux autres options d'itération pour affecter de grandes zones de mémoire."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Augmenter l'adresse à chaque itération"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_ADDRESS,
   "Après chaque itération l'adresse mémoire sera incrémentée de ce montant multiplié par la 'Taille de recherche dans la mémoire'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_REPEAT_ADD_TO_VALUE,
   "Augmenter la valeur à chaque itération"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_REPEAT_ADD_TO_VALUE,
   "Après chaque itération la 'Valeur' sera augmentée de ce montant."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_TYPE,
   "Vibrer pour la mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_VALUE,
   "Valeur de la vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PORT,
   "Port de la vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_STRENGTH,
   "Force principale de la vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_PRIMARY_DURATION,
   "Durée principale de la vibration (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_STRENGTH,
   "Force secondaire de la vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_RUMBLE_SECONDARY_DURATION,
   "Durée secondaire de la vibration (ms)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_AFTER,
   "Ajouter un nouveau cheat après celui-ci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_ADD_NEW_BEFORE,
   "Ajouter un nouveau cheat avant celui-ci"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_AFTER,
   "Copier ce cheat après"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_COPY_BEFORE,
   "Copier ce cheat avant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE,
   "Supprimer ce cheat"
   )

/* Quick Menu > Disc Control */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_EJECT,
   "Éjecter le disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_EJECT,
   "Ouvrir le plateau du lecteur de disque virtuel et retirer le disque actuellement chargé. Si 'Mettre en pause le contenu lorsque le menu est actif' est activé, certains cœurs peuvent ne pas enregistrer les changements à moins que le contenu ne soit repris pendant quelques secondes après chaque action liée au contrôle du disque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_TRAY_INSERT,
   "Insérer le disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_TRAY_INSERT,
   "Insérer le disque correspondant au 'Numéro du disque actuel' et fermer le plateau du lecteur de disque virtuel. Si 'Mettre en pause le contenu lorsque le menu est actif' est activé, certains cœurs peuvent ne pas enregistrer les changements à moins que le contenu ne soit repris pendant quelques secondes après chaque action liée au contrôle du disque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_IMAGE_APPEND,
   "Charger un nouveau disque"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND,
   "Éjecte le disque actuel, sélectionne un nouveau disque dans le système de fichiers, puis l'insère et ferme le plateau du lecteur de disque virtuel.\nREMARQUE : C'est une fonctionnalité obsolète. Il est plutôt recommandé de charger des titres multi-disques via des listes de lecture M3U, qui permettent la sélection du disque en utilisant les options 'Éjecter/Insérer le disque' et 'Numéro du disque actuel'."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_IMAGE_APPEND_TRAY_OPEN,
   "Sélectionne un nouveau disque dans le système de fichiers puis l'insère sans fermer le plateau du lecteur de disque virtuel.\nREMARQUE : C'est une fonctionnalité obsolète. Il est plutôt recommandé de charger des titres multi-disques via des listes de lecture M3U, qui permettent la sélection du disque en utilisant l'option 'Numéro du disque actuel'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISK_INDEX,
   "Numéro du disque actuel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DISK_INDEX,
   "Sélectionne le disque actuel dans la liste des images disponibles. Le disque sera chargé si 'Insérer le disque' est sélectionné."
   )

/* Quick Menu > Shaders */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADERS_ENABLE,
   "Shaders vidéo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_WATCH_FOR_CHANGES,
   "Verifier les changements dans les fichiers de shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_WATCH_FOR_CHANGES,
   "Appliquer automatiquement les changements apportés aux fichiers de shader sur le disque."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Recharger le dernier dossier de shaders utilisé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_REMEMBER_LAST_DIR,
   "Ouvre le navigateur de fichiers au dernier dossier utilisé lors du chargement de préréglages de shaders et passages."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET,
   "Charger"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET,
   "Charger un préréglage de shaders. Le pipeline des shaders sera automatiquement configuré."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE,
   "Enregistrer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE,
   "Enregistrer les réglages de shaders actuels."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE,
   "Supprimer"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE,
   "Supprimer les préréglages de shaders d'un type specifique."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_APPLY_CHANGES,
   "Appliquer les changements"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SHADER_APPLY_CHANGES,
   "Les modifications apportées à la configuration du shader prendront effet immédiatement. Utilisez cette option si vous avez changé la quantité de passages de shader, le filtrage, l'échelle, etc."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PARAMETERS,
   "Paramètres des shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PARAMETERS,
   "Modifier le shader actuel directement. Les modifications ne seront pas enregistrées dans le fichier de préréglages."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES,
   "Passages de shaders"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_NUM_PASSES,
   "Augmenter ou diminuer le nombre de passages du pipeline des shaders. Des shader distincts peuvent être assigné à chaque passage du pipeline ainsi que la configuration de leur échelle et leur mode de filtrage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILTER,
   "Filtre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCALE,
   "Échelle"
   )

/* Quick Menu > Shaders > Save */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Préréglages simples"
   )

MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_REFERENCE,
   "Enregistrer un préréglage de shaders lié au préréglage original chargé et n'inclure que les changements de paramètres que vous avez effectués."
   )

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_AS,
   "Enregistrer les préréglages de shaders sous"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_AS,
   "Enregistrer les réglages de shaders actuels en tant que nouveaux préréglages de shaders."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Enregistrer les préréglages de shaders globaux"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GLOBAL,
   "Enregistrer les réglages de shaders actuels en tant que réglages par défaut globaux."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Enregistrer les préréglages pour le cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_CORE,
   "Enregistrer les réglages de shaders actuels en tant que réglages par défaut pour ce cœur."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Enregistrer les préréglages pour le contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_PARENT,
   "Enregistrer les réglages de shaders actuels en tant que réglages par défaut pour tous les fichiers du dossier de contenu actuel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Enregistrer les préréglages pour le jeu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_SAVE_GAME,
   "Enregistrer les réglages de shaders actuels en tant que réglages par défaut pour ce contenu."
   )

/* Quick Menu > Shaders > Remove */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PRESETS_FOUND,
   "Aucun paramètre de shaders automatiques trouvé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Supprimer les préréglages globaux"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GLOBAL,
   "Supprimer les préréglages globaux, utilisés par tout le contenu et tous les cœurs."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Supprimer les préréglages du cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_CORE,
   "Supprimer les préréglages du cœur, utilisés par tout le contenu déjà lancé par le cœur actuellement chargé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Supprimer les préréglages du dossier de contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_PARENT,
   "Supprimer les préréglages du dossier de contenu, utilisés par tout le contenu situé dans le dossier actuel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Supprimer les préréglages du jeu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_REMOVE_GAME,
   "Supprimer le préréglage du jeu, utilisé uniquement pour le jeu en question."
   )

/* Quick Menu > Shaders > Shader Parameters */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SHADER_PARAMETERS,
   "Aucun paramètre de shaders."
   )

/* Quick Menu > Overrides */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Sauvegarder le remplacement de configuration pour le cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE,
   "Enregistrer un fichier de configuration de remplacement qui s'appliquera à tout le contenu chargé avec ce cœur. Aura la priorité sur la configuration principale."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Sauvegarder le remplacement de configuration pour le dossier"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CONTENT_DIR,
   "Enregistrer un fichier de configuration de remplacement qui s'appliquera à tout le contenu chargé depuis le même dossier que le fichier actuel. Aura la priorité sur la configuration principale."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Sauvegarder le remplacement de configuration pour le jeu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME,
   "Enregistrer un fichier de configuration de remplacement qui s'appliquera uniquement au contenu actuel. Aura la priorité sur la configuration principale."
   )

/* Quick Menu > Achievements */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ACHIEVEMENTS_TO_DISPLAY,
   "Aucun succès à afficher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE_CANCEL,
   "Annuler la mise en pause du mode Hardcore des succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE_CANCEL,
   "Laisser le mode Hardcore des succès activé pour la session en cours"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME_CANCEL,
   "Annuler la reprise du mode Hardcore des succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME_CANCEL,
   "Laisser le mode Hardcore des succès désactivé pour la session en cours"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_PAUSE,
   "Mettre en pause le mode Hardcore des succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_PAUSE,
   "Mettre le mode Hardcore des succès en pause pour la session en cours. Cette action activera les sauvegardes instantanées, les cheats, le rembobinage, la mise en pause et le ralenti."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_RESUME,
   "Reprendre le mode Hardcore des succès"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_ACHIEVEMENT_RESUME,
   "Réactiver le mode Hardcore des succès pour la session en cours. Cette action désactivera les sauvegardes instantanées, les cheats, le rembobinage, la mise en pause, le ralenti et réinitialisera le jeu en cours."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_LOGGED_IN,
   "Non connecté"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETWORK_ERROR,
   "Erreur réseau"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN_GAME,
   "Jeu inconnu"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CANNOT_ACTIVATE_ACHIEVEMENTS_WITH_THIS_CORE,
   "Impossible d'activer les succès avec ce cœur"
)

/* Quick Menu > Information */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_INFO_CHEEVOS_HASH,
   "Empreinte RetroSuccès"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_DETAIL,
   "Entrée de base de données"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RDB_ENTRY_DETAIL,
   "Affiche les informations dans la base de données pour le contenu actuel."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ENTRIES_TO_DISPLAY,
   "Aucune entrée à afficher."
   )

/* Miscellaneous UI Items */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORES_AVAILABLE,
   "Aucun cœur disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE,
   "Aucune option de cœur disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE,
   "Aucune information de cœur disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE_BACKUPS_AVAILABLE,
   "Aucune sauvegarde de cœur disponible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_FAVORITES_AVAILABLE,
   "Aucun favori disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_HISTORY_AVAILABLE,
   "Aucun historique disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_IMAGES_AVAILABLE,
   "Aucune image disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_MUSIC_AVAILABLE,
   "Aucune musique disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_VIDEOS_AVAILABLE,
   "Aucune vidéo disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE,
   "Aucune information disponible."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE,
   "Liste de lecture vide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_SETTINGS_FOUND,
   "Aucun fichier de réglages trouvé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_BT_DEVICES_FOUND,
   "Aucun périphérique Bluetooth trouvé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETWORKS_FOUND,
   "Aucun réseau trouvé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_CORE,
   "Aucun cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SEARCH,
   "Recherche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK,
   "Retour"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_OK,
   "Confirmer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PARENT_DIRECTORY,
   "Dossier parent"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NOT_FOUND,
   "Dossier non trouvé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_ITEMS,
   "Aucun élément."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FILE,
   "Sélectionner un fichier"
   )

/* Settings Options */

MSG_HASH( /* FIXME Should be MENU_LABEL_VALUE */
   MSG_UNKNOWN_COMPILER,
   "Compilateur inconnu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_OR,
   "Partager"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_XOR,
   "Saisir"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_DIGITAL_VOTE,
   "Voter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG,
   "Partage des entrées analogiques "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_MAX,
   "Maximum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_ANALOG_AVERAGE,
   "Moyenne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NONE,
   "Ne pas partager"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SHARE_NO_PREFERENCE,
   "Pas de préférence"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_BOUNCE,
   "Faire rebondir de gauche à droite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_TICKER_TYPE_LOOP,
   "Faire défiler vers la gauche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_IMAGE_MODE,
   "Mode image"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_SPEECH_MODE,
   "Mode parole"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AI_SERVICE_NARRATOR_MODE,
   "Mode narrateur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_HIST_FAV,
   "Historique et favoris"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_ALL,
   "Toutes les listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_ENTRY_REMOVE_ENABLE_NONE,
   "Désactivé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_HIST_FAV,
   "Historique et favoris"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_ALWAYS,
   "Toujours"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_INLINE_CORE_DISPLAY_NEVER,
   "Jamais"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_PER_CORE,
   "Par cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_RUNTIME_AGGREGATE,
   "Cumulé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGED,
   "Chargé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_CHARGING,
   "En charge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_DISCHARGING,
   "Non chargé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_POWER_SOURCE_NO_SOURCE,
   "Non alimenté"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_THIS_DIRECTORY,
   "<Utiliser ce dossier>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT,
   "<Dossier du contenu>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT,
   "<Par défaut>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE,
   "<Aucun>"
   )
MSG_HASH( /* FIXME Unused? */
   MENU_ENUM_LABEL_VALUE_RETROKEYBOARD,
   "RetroClavier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD,
   "RetroManette"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RETROPAD_WITH_ANALOG,
   "RetroManette analogique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NONE,
   "Aucun(e)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNKNOWN,
   "Inconnu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_Y_L_R,
   "Bas + Y + L1 + R1"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_START,
   "Maintenir Start (2 secondes)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HOLD_SELECT,
   "Maintenir Select (2 secondes)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DOWN_SELECT,
   "Bas + Select"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DISABLED,
   "<Désactivé>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_CHANGES,
   "Change"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DOES_NOT_CHANGE,
   "Ne change pas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE,
   "Augmente"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE,
   "Diminue"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_EQ_VALUE,
   "= valeur de vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_NEQ_VALUE,
   "!= valeur de vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_LT_VALUE,
   "< valeur de vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_GT_VALUE,
   "> valeur de vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_INCREASE_BY_VALUE,
   "Augmente par la valeur de vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_TYPE_DECREASE_BY_VALUE,
   "Diminue par la valeur de vibration"
   )
MSG_HASH(
   MENU_ENUM_LABEL_RUMBLE_PORT_16,
   "Tous"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DISABLED,
   "<Désactivé>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_SET_TO_VALUE,
   "Régler à la valeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_INCREASE_VALUE,
   "Augmenter par la valeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_DECREASE_VALUE,
   "Diminuer par la valeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_EQ,
   "Exécuter le prochain cheat si la valeur = mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_NEQ,
   "Exécuter le prochain cheat si la valeur != mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_LT,
   "Exécuter le prochain cheat si la valeur < mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_TYPE_RUN_NEXT_IF_GT,
   "Exécuter le prochain cheat si la valeur > mémoire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_HANDLER_TYPE_EMU,
   "Emulateur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_1,
   "1-bit, valeur max = 0x01"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_2,
   "2-bit, valeur max = 0x03"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_4,
   "4-bit, valeur max = 0x0F"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_8,
   "8-bit, valeur max = 0xFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_16,
   "16-bit, valeur max = 0xFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_MEMORY_SIZE_32,
   "32-bit, valeur max = 0xFFFFFFFF"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_DEFAULT,
   "Ordre par défaut du système"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_ALPHABETICAL,
   "Ordre alphabétique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_SORT_MODE_OFF,
   "Aucun tri"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_DEFAULT,
   "Afficher le titre complet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS,
   "Supprimer le contenu entre ()"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_BRACKETS,
   "Supprimer le contenu entre []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_REMOVE_PARENS_AND_BRACKETS,
   "Supprimer le contenu entre () et []"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION,
   "Garder la région"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_DISC_INDEX,
   "Garder le numéro du disque"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_LABEL_DISPLAY_MODE_KEEP_REGION_AND_DISC_INDEX,
   "Garder la région et le numéro du disque"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PLAYLIST_MANAGER_THUMBNAIL_MODE_DEFAULT,
   "Par défaut du système"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_BOXARTS,
   "Jaquette"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_SCREENSHOTS,
   "Capture d'écran"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAIL_MODE_TITLE_SCREENS,
   "Écran titre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_NORMAL,
   "Normale"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCROLL_FAST,
   "Rapide"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ON,
   "Activé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OFF,
   "Désactivé(e)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_YES,
   "Oui"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO,
   "Non"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TRUE,
   "Vrai"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FALSE,
   "Faux"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ENABLED,
   "Activé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISABLED,
   "Désactivé(e)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE,
   "Indisponible"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_LOCKED_ENTRY,
   "Verrouillé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNLOCKED_ENTRY,
   "Débloqué"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNOFFICIAL_ENTRY,
   "Non officiel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_UNSUPPORTED_ENTRY,
   "Non pris en charge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_RECENTLY_UNLOCKED_ENTRY,
   "Récemment déverrouillés"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ALMOST_THERE_ENTRY,
   "Vous y êtes presque"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_ACTIVE_CHALLENGES_ENTRY,
   "Défis actifs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_TRACKERS_ONLY,
   "Traqueurs uniquement"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_NOTIFICATIONS_ONLY,
   "Notifications uniquement"
)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DONT_CARE,
   "Par défaut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LINEAR,
   "Linéaire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NEAREST,
   "Au plus proche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MAIN,
   "Principal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT,
   "Contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CONTENT_DIR,
   "<Dossier du contenu>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_SYSTEM_NAME_USE_CUSTOM,
   "<Personnalisé>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANUAL_CONTENT_SCAN_CORE_NAME_DETECT,
   "<Non spécifié>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG,
   "Analogique gauche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG,
   "Analogique droite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_ANALOG_FORCED,
   "Analogique gauche (Forcé)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG_FORCED,
   "Analogique droite (Forcé)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEY,
   "(Touche : %s)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_LEFT,
   "Souris 1 (clic gauche)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_RIGHT,
   "Souris 2 (clic droit)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_MIDDLE,
   "Souris 3 (clic molette)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON4,
   "Souris 4"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_BUTTON5,
   "Souris 5"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_UP,
   "Molette Haut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_WHEEL_DOWN,
   "Molette Bas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_UP,
   "Molette Gauche"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_HORIZ_WHEEL_DOWN,
   "Molette Droite"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_EARLY,
   "Précoce"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_NORMAL,
   "Normale"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR_LATE,
   "Tardive"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS,
   "AAAA-MM-JJ HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM,
   "AAAA-MM-JJ HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD,
   "AAAA-MM-JJ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YM,
   "AAAA-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS,
   "MM-JJ-AAAA HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM,
   "MM-JJ-AAAA HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM,
   "MM-JJ HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY,
   "MM-JJ-AAAA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD,
   "MM-JJ"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS,
   "JJ-MM-AAAA HH:MM:SS"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM,
   "JJ-MM-AAAA HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM,
   "JJ-MM HH:MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY,
   "JJ-MM-AAAA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM,
   "JJ-MM"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HMS_AMPM,
   "AAAA-MM-JJ HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_YMD_HM_AMPM,
   "AAAA-MM-JJ HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HMS_AMPM,
   "MM-JJ-AAAA HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MDYYYY_HM_AMPM,
   "MM-JJ-AAAA HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_MD_HM_AMPM,
   "MM-JJ HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HMS_AMPM,
   "JJ-MM-AAAA HH:MM:SS (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMMYYYY_HM_AMPM,
   "JJ-MM-AAAA HH:MM (AM/PM)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEDATE_DDMM_HM_AMPM,
   "JJ-MM HH:MM (AM/PM)"
   )

/* RGUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Épaisseur du remplissage de l'arrière-plan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BACKGROUND_FILLER_THICKNESS_ENABLE,
   "Augmente la taille du motif de damier en arrière-plan du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Remplissage de la bordure"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Épaisseur du remplissage de la bordure"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_THICKNESS_ENABLE,
   "Augmente la taille du motif de damier en arrière-plan de la bordure."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_BORDER_FILLER_ENABLE,
   "Affiche la bordure du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Utiliser la mise en page en pleine largeur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_FULL_WIDTH_LAYOUT,
   "Redimensionner et positionner les entrées du menu pour utiliser au mieux l'espace disponible à l'écran. Désactiver cette option pour utiliser une disposition classique à deux colonnes de largeur fixe."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER,
   "Filtre linéaire dans le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_LINEAR_FILTER,
   "Ajoute un léger flou au menu pour atténuer le contour des pixels bruts."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Upscaling interne"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INTERNAL_UPSCALE_LEVEL,
   "Upscale l'interface du menu avant de l'afficher à l'écran. Lors de l'utilisation du 'Filtre linéaire dans le menu', cette option supprime les artéfacts de mise à l'échelle (pixels irréguliers) tout en conservant une image nette. A un impact significatif sur les performances qui augmente avec le niveau d'upscaling."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO,
   "Rapport d'aspect du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO,
   "Sélectionner le rapport d'aspect du menu. Les rapports d'écran large augmentent la résolution horizontale de l'interface de menu. (Peut nécessiter un redémarrage si 'Verrouiller le rapport d'aspect du menu' est désactivé)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_ASPECT_RATIO_LOCK,
   "Verrouiller le rapport d'aspect du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_ASPECT_RATIO_LOCK,
   "S'assure que le menu est toujours affiché avec le bon rapport d'aspect. Si cette option est désactivée, le menu rapide sera étiré pour correspondre au contenu actuellement chargé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME,
   "Thème de couleur du menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_COLOR_THEME,
   "Selectionner un thème de couleur différent. Choisir 'Personnalisé' permet l'utilisation de fichiers de préréglages de thème du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_THEME_PRESET,
   "Préréglages de thème du menu personnalisés"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RGUI_MENU_THEME_PRESET,
   "Selectionner un fichier de préréglages de thème du menu depuis le navigateur de fichiers."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_TRANSPARENCY,
   "Transparence du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_TRANSPARENCY,
   "Activer l'affichage en arrière-plan du contenu en cours d'exécution lorsque le Menu rapide est actif. Désactiver la transparence peut modifier les couleurs du thème."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SHADOWS,
   "Effet d'ombres"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SHADOWS,
   "Activer les ombres portées pour le texte du menu, les bordures et les miniatures. A un impact modeste sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT,
   "Animation de l'arrière-plan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT,
   "Activer l'effet d'animation de particules en arrière-plan. A un impact significatif sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Vitesse de l'animation en arrière-plan"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SPEED,
   "Ajuste la vitesse des effets d'animation de particules en arrière-plan."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Animation de fond pour l'économiseur d'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_PARTICLE_EFFECT_SCREENSAVER,
   "Afficher un effet d'animation de particules en arrière-plan lorsque l'économiseur d'écran du menu est actif."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_INLINE_THUMBNAILS,
   "Afficher les miniatures dans les listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_INLINE_THUMBNAILS,
   "Réduit les miniatures intégrées lors de l'affichage des listes de lecture. Lorsque cette option est désactivée, la 'Miniature du haut' peut toujours être affichée en plein écran en appuyant sur RetroManette Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_RGUI,
   "Miniature du haut"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_RGUI,
   "Type de miniatures à afficher en haut à droite des listes de lecture. Cette vignette peut être basculée en mode plein écran en appuyant sur RetroManette Y."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_RGUI,
   "Miniature du bas"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_RGUI,
   "Type de miniatures à afficher en bas à droite des listes de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWAP_THUMBNAILS,
   "Échanger les miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWAP_THUMBNAILS,
   "Échange les positions à l'écran de 'Miniature du haut' et 'Miniature du bas'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Méthode de réduction des miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DOWNSCALER,
   "Méthode de rééchantillonnage utilisée lors de la réduction de grandes miniatures pour les adapter à l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_THUMBNAIL_DELAY,
   "Retarder l'affichage de la miniature (ms)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_THUMBNAIL_DELAY,
   "Applique un délai de temps entre la sélection d'une entrée de liste de lecture et le chargement de sa miniature associée. Régler sur une valeur d'au moins 256 ms permet la navigation rapide sans à-coups même sur les appareils les plus lents."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_EXTENDED_ASCII,
   "Prise en charge de l'ASCII étendu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_EXTENDED_ASCII,
   "Activer l'affichage des caractères ASCII non standard. Requis pour la compatibilité avec certaines langues occidentales non anglaises. A un impact modéré sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_RGUI_SWITCH_ICONS,
   "Afficher les boutons ON/OFF"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_RGUI_SWITCH_ICONS,
   "Utiliser des boutons ON/OFF au lieu du texte Activé/Désactivé pour représenter l'activation des réglages dans le menu."
   )

/* RGUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_POINT,
   "Au plus proche (Rapide)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_BILINEAR,
   "Bilinéaire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_THUMB_SCALE_SINC,
   "Sinc/Lanczos3 (Lent)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_UPSCALE_NONE,
   "Aucune"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_9_CENTRE,
   "16:9 (centré)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_16_10_CENTRE,
   "16:10 (centré)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_3_2_CENTRE,
   "3:2 (centré)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_5_3_CENTRE,
   "5:3 (centré)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_NONE,
   "Désactivé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FIT_SCREEN,
   "Adapter à l'écran"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_INTEGER,
   "Échelle à l'entier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_ASPECT_RATIO_LOCK_FILL_SCREEN,
   "Remplir l'écran (Étiré)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CUSTOM,
   "Personnalisé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_RED,
   "Rouge classique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_ORANGE,
   "Orange classique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_YELLOW,
   "Jaune classique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREEN,
   "Vert classique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_BLUE,
   "Bleu classique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_VIOLET,
   "Violet classique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_CLASSIC_GREY,
   "Gris classique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LEGACY_RED,
   "Rouge hérité"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DARK_PURPLE,
   "Violet sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Bleu nuit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GOLDEN,
   "Doré"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Bleu électrique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_APPLE_GREEN,
   "Vert pomme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_VOLCANIC_RED,
   "Rouge volcanique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_LAGOON,
   "Lagon"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_BROGRAMMER,
   "Brogrammeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FAIRYFLOSS,
   "Soie de fée"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_FLATUI,
   "Minimaliste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_GRUVBOX_LIGHT,
   "Gruvbox claire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Pirater le kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarisé sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarisé clair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_DARK,
   "Tango sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_TANGO_LIGHT,
   "Tango clair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_MENU_COLOR_THEME_DYNAMIC,
   "Dynamique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_NONE,
   "Désactivée"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW,
   "Neige (Légère)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_SNOW_ALT,
   "Neige (Intense)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_RAIN,
   "Pluie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RGUI_PARTICLE_EFFECT_STARFIELD,
   "Ciel étoilé"
   )

/* XMB: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS,
   "Miniature de gauche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS,
   "Type de miniatures à afficher à gauche."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER,
   "Arrière-plan dynamique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_DYNAMIC_WALLPAPER,
   "Charger dynamiquement un nouveau fond d'écran en fonction du contexte."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_HORIZONTAL_ANIMATION,
   "Animation horizontale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_HORIZONTAL_ANIMATION,
   "Active l'animation horizontale pour le menu. Cela aura un impact sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "Surlignage horizontal de l'icone animé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_HORIZONTAL_HIGHLIGHT,
   "L'animation qui se lance lors du passage entre les onglets."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "Déplacement vers le haut/bas animé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_MOVE_UP_DOWN,
   "L'animation qui se lance lors du déplacement vers le haut ou le bas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "Ouverture/fermeture du menu principal animé"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_ANIMATION_OPENING_MAIN_MENU,
   "L'animation qui se lance lors de l'ouverture d'un sous-menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR,
   "Opacité du menu "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_FONT,
   "Police du menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_FONT,
   "Sélectionner une police principale différente à utiliser pour le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_RED,
   "Valeur de rouge (Police du menu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_GREEN,
   "Valeur de vert (Police du menu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_FONT_COLOR_BLUE,
   "Valeur de bleu (Police du menu)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_LAYOUT,
   "Mise en page du menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_LAYOUT,
   "Sélectionner une mise en page différente pour l'interface XMB."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_THEME,
   "Thème d'icônes du menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_THEME,
   "Sélectionner un thème d'icônes différent pour RetroArch."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE,
   "Ombres des icônes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_SHADOWS_ENABLE,
   "Ajouter des ombres portées pour toutes les icônes. Cette option aura un impact mineur sur les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE,
   "Pipeline de shader du menu (fond animé) "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_RIBBON_ENABLE,
   "Sélectionner un effet d'arrière-plan animé. Peut être gourmand en processeur graphique selon l'effet. Si les performances ne sont pas satisfaisantes, veuillez le désactiver ou revenir à un effet plus simple."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME,
   "Thème de couleur du menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MENU_COLOR_THEME,
   "Sélectionner un autre thème de couleur pour le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_VERTICAL_THUMBNAILS,
   "Disposition des miniatures à la verticale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_VERTICAL_THUMBNAILS,
   "Affiche la miniature de gauche sous celle de droite, à droite de l'écran."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Facteur de mise à l'échelle des miniatures "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_XMB_THUMBNAIL_SCALE_FACTOR,
   "Réduit la taille d'affichage des miniatures en ajustant la largeur maximum autorisée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_XMB_VERTICAL_FADE_FACTOR,
   "Facteur de fondu vertical"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Activer l'onglet 'Réglages' (Redémarrage requis)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_XMB_MAIN_MENU_ENABLE_SETTINGS,
   "Afficher l'onglet Réglages contenant les paramètres du programme."
   )

/* XMB: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON,
   "Ruban"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_RIBBON_SIMPLIFIED,
   "Ruban (simplifié)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SIMPLE_SNOW,
   "Neige simple"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOW,
   "Neige"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHADER_PIPELINE_SNOWFLAKE,
   "Flocon de neige"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_CUSTOM,
   "Personnalisé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_FLATUI,
   "Minimaliste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_MONOCHROME_INVERTED,
   "Monochrome inversé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_SYSTEMATIC,
   "Systématique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_NEOACTIVE,
   "NéoActif"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROACTIVE,
   "RétroActif"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_RETROSYSTEM,
   "Rétrosystème"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_DOTART,
   "Pixel art"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC,
   "Automatique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_ICON_THEME_AUTOMATIC_INVERTED,
   "Automatique inversé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_APPLE_GREEN,
   "Vert pomme"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK,
   "Sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIGHT,
   "Clair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MORNING_BLUE,
   "Bleu du matin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_SUNBEAM,
   "Rayon de soleil"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_DARK_PURPLE,
   "Violet sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ELECTRIC_BLUE,
   "Bleu électrique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GOLDEN,
   "Doré"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LEGACY_RED,
   "Rouge hérité"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_MIDNIGHT_BLUE,
   "Bleu nuit"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PLAIN,
   "Ordinaire"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_UNDERSEA,
   "Sous-marin"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_VOLCANIC_RED,
   "Rouge volcanique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_LIME,
   "Vert citron"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_PIKACHU_YELLOW,
   "Jaune Pikachu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_GAMECUBE_PURPLE,
   "Violet Cube"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FAMICOM_RED,
   "Rouge Family"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_FLAMING_HOT,
   "Enflammé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME_ICE_COLD,
   "Glacial"
   )

/* Ozone: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLLAPSE_SIDEBAR,
   "Réduire la barre latérale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_COLLAPSE_SIDEBAR,
   "Barre latérale gauche toujours réduite."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Tronquer le nom des listes de lecture (Redémarrage requis)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_TRUNCATE_PLAYLIST_NAME,
   "Retirer le nom du fabricant des listes de lecture. Par exemple, 'Sony - PlayStation' deviendra 'PlayStation'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Trier les listes de lecture après la troncature du nom (Redémarrage requis)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SORT_AFTER_TRUNCATE_PLAYLIST_NAME,
   "Les listes de lecture seront re-triées par ordre alphabétique après avoir supprimé le composant fabricant de leurs noms."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_MENU_COLOR_THEME,
   "Thème de couleur du menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_MENU_COLOR_THEME,
   "Sélectionner un thème de couleur différent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_WHITE,
   "Blanc basique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BASIC_BLACK,
   "Noir basique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_BOYSENBERRY,
   "Mûre de Logan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_HACKING_THE_KERNEL,
   "Pirater le kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_TWILIGHT_ZONE,
   "Quatrième dimension (Twilight Zone)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_DARK,
   "Solarisé sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_COLOR_THEME_SOLARIZED_LIGHT,
   "Solarisé clair"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_OZONE,
   "Miniature secondaire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_OZONE,
   "Remplace le panneau des métadonnées du contenu par une autre miniature."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_SCROLL_CONTENT_METADATA,
   "Utiliser le défilement de texte pour les métadonnées du contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_OZONE_SCROLL_CONTENT_METADATA,
   "Si activée, cette option affichera chaque élément de métadonnées du contenu sur la barre latérale droite des listes de lectures (cœur associé, temps de jeu) sur une seule ligne; défilant si le texte est trop long pour la barre latérale. Si désactivée, chaque élément de métadonnées sera affiché statiquement, allant à la ligne si besoin."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OZONE_THUMBNAIL_SCALE_FACTOR,
   "Facteur de mise à l'échelle des miniatures "
   )

/* MaterialUI: Settings > User Interface > Appearance */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_ICONS_ENABLE,
   "Icônes du menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_ICONS_ENABLE,
   "Afficher les icônes à gauche des entrées du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Icônes de listes de lecture (Redémarrage requis)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_PLAYLIST_ICONS_ENABLE,
   "Afficher des icônes spécifiques au système dans les listes de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Optimiser pour l'affichage en mode paysage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION,
   "Ajuste automatiquement la disposition du menu pour être mieux adapté à l'utilisation d'une orientation d'écran en mode paysage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_SHOW_NAV_BAR,
   "Afficher la barre de navigation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_SHOW_NAV_BAR,
   "Affiche des raccourcis de navigation permanents dans le menu à l'écran. Permet de basculer rapidement entre les catégories du menu. Recommandé pour les appareils à écran tactile."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Rotation automatique de la barre de navigation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_AUTO_ROTATE_NAV_BAR,
   "Déplace automatiquement la barre de navigation vers le côté droit de l'écran lors de l'utilisation d'une orientation d'écran en mode paysage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME,
   "Thème de couleur du menu "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_COLOR_THEME,
   "Sélectionner un autre thème de couleur pour le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Animation de transition vers le menu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_TRANSITION_ANIMATION,
   "Active des effets d'animation de transition lors de la navigation entre différents niveaux du menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Miniatures en mode portrait"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_PORTRAIT,
   "Spécifie le mode d'affichage des miniatures dans les listes de lecture lors de l'utilisation d'un écran orienté en mode portrait."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Miniatures en mode paysage"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_THUMBNAIL_VIEW_LANDSCAPE,
   "Spécifie le mode d'affichage des miniatures dans les listes de lecture lors de l'utilisation d'un écran orienté en mode paysage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Affiche la miniature secondaire lors de l'affichage par liste"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_DUAL_THUMBNAIL_LIST_VIEW_ENABLE,
   "Affiche une miniature secondaire lors de l'utilisation d'un mode d'affichage de type 'Liste' dans les listes de lecture. Ce réglage n'est appliqué que si l'écran dispose d'une largeur suffisante pour afficher deux miniatures."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Afficher un fond pour les miniatures"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MATERIALUI_THUMBNAIL_BACKGROUND_ENABLE,
   "Active le remplissage de l'espace inutilisé autour des miniatures avec une couleur de fond. Cela assure une taille d'affichage uniforme pour toutes les images, améliorant l'apparence du menu lors de l'affichage des miniatures de contenu mixte avec différentes dimensions de base."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_THUMBNAILS_MATERIALUI,
   "Miniature principale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_THUMBNAILS_MATERIALUI,
   "Type de miniatures principal à associer à chaque entrée de liste de lecture. Habituellement utilisée comme icône pour le contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LEFT_THUMBNAILS_MATERIALUI,
   "Miniature secondaire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LEFT_THUMBNAILS_MATERIALUI,
   "Type de miniatures auxiliaire à associer à chaque entrée de liste de lecture. L'utilisation dépend du mode d'affichage actuel des miniatures dans les listes de lecture."
   )

/* MaterialUI: Settings Options */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE,
   "Bleu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_BLUE_GREY,
   "Bleu gris"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_DARK_BLUE,
   "Bleu sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GREEN,
   "Vert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_NVIDIA_SHIELD,
   "NVIDIA Shield"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_RED,
   "Rouge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_YELLOW,
   "Jaune"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_MATERIALUI_DARK,
   "Material UI sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_OZONE_DARK,
   "Ozone sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_GRUVBOX_DARK,
   "Gruvbox sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_SOLARIZED_DARK,
   "Solarisé sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_BLUE,
   "Cutie bleu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_CYAN,
   "Cutie cyan"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_GREEN,
   "Cutie vert"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_ORANGE,
   "Cutie orange"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PINK,
   "Cutie rose"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_PURPLE,
   "Cutie violet"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_CUTIE_RED,
   "Cutie rouge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME_HACKING_THE_KERNEL,
   "Pirater le kernel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_FADE,
   "Fondu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_SLIDE,
   "Glissement"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_TRANSITION_ANIM_NONE,
   "Désactivée"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DISABLED,
   "Désactivées"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_SMALL,
   "Liste (Petite)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_LIST_MEDIUM,
   "Liste (Moyenne)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_PORTRAIT_DUAL_ICON,
   "Double icône"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DISABLED,
   "Désactivées"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_SMALL,
   "Liste (Petite)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_MEDIUM,
   "Liste (Moyenne)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_LIST_LARGE,
   "Liste (Grande)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_THUMBNAIL_VIEW_LANDSCAPE_DESKTOP,
   "Bureau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_DISABLED,
   "Désactivé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_ALWAYS,
   "Activé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MATERIALUI_LANDSCAPE_LAYOUT_OPTIMIZATION_EXCLUDE_THUMBNAIL_VIEWS,
   "Exclure l'affichage des miniatures"
   )

/* Qt (Desktop Menu) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFO,
   "Informations"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE,
   "&Fichier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_LOAD_CORE,
   "&Charger un cœur..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_UNLOAD_CORE,
   "&Décharger le cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_FILE_EXIT,
   "&Quitter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT,
   "&Édition"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_EDIT_SEARCH,
   "&Rechercher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW,
   "&Présentation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_CLOSED_DOCKS,
   "Panneaux fermés"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_SHADER_PARAMS,
   "Paramètres de shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS,
   "&Réglages..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_DOCK_POSITIONS,
   "Restaurer la position des panneaux :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_GEOMETRY,
   "Restaurer la taille de la fenêtre :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SAVE_LAST_TAB,
   "Rouvrir le dernier onglet du navigateur :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME,
   "Thème :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_SYSTEM_DEFAULT,
   "<Système par défaut>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_DARK,
   "Sombre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THEME_CUSTOM,
   "Personnalisé..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_TITLE,
   "Réglages"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_TOOLS,
   "&Outils"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP,
   "&Aide"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT,
   "À propos de RetroArch"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CUSTOM_CORE,
   "Charger un cœur personnalisé..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD_CORE,
   "Charger un cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOADING_CORE,
   "Chargement du cœur..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NAME,
   "Nom"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_PLAYLISTS,
   "Listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER,
   "Navigateur de fichiers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_TOP,
   "Haut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_TAB_FILE_BROWSER_UP,
   "Niveau supérieur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_DOCK_CONTENT_BROWSER,
   "Navigateur de contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_BOXART,
   "Jaquette"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_SCREENSHOT,
   "Capture d'écran"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_TITLE_SCREEN,
   "Écran titre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ALL_PLAYLISTS,
   "Toutes les listes de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE,
   "Cœurs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_INFO,
   "Informations du cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_SELECTION_ASK,
   "<Me demander>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_INFORMATION,
   "Informations"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_WARNING,
   "Attention"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ERROR,
   "Erreur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NETWORK_ERROR,
   "Erreur réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESTART_TO_TAKE_EFFECT,
   "Veuillez redémarrer le programme pour que les modifications prennent effet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOG,
   "Journal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ITEMS_COUNT,
   "%1 éléments"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DROP_IMAGE_HERE,
   "Déposer une image ici"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DONT_SHOW_AGAIN,
   "Ne plus afficher"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_STOP,
   "Arrêter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ASSOCIATE_CORE,
   "Associer le cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDDEN_PLAYLISTS,
   "Listes de lecture masquées"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_HIDE,
   "Masquer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_HIGHLIGHT_COLOR,
   "Couleur de surbrillance :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CHOOSE,
   "&Choisir..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_COLOR,
   "Sélectionner une couleur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_THEME,
   "Sélectionner un thème"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CUSTOM_THEME,
   "Thème personnalisé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_PATH_IS_BLANK,
   "L'emplacement du fichier est vide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_IS_EMPTY,
   "Le fichier est vide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_READ_OPEN_FAILED,
   "Impossible d'ouvrir le fichier en lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_WRITE_OPEN_FAILED,
   "Impossible d'ouvrir le fichier en écriture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FILE_DOES_NOT_EXIST,
   "Le fichier n'existe pas."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SUGGEST_LOADED_CORE_FIRST,
   "Suggérer le cœur chargé en premier :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ZOOM,
   "Échelle"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW,
   "Présentation"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_ICONS,
   "Par icônes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_VIEW_TYPE_LIST,
   "Par liste"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_SEARCH_CLEAR,
   "Effacer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PROGRESS,
   "Progression :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_LIST_MAX_COUNT,
   "Nombre maximum d'entrées dans la liste \"Toutes les liste de lecture\" :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_ALL_PLAYLISTS_GRID_MAX_COUNT,
   "Nombre maximum d'entrées dans la grille \"Toutes les liste de lecture\" :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_SHOW_HIDDEN_FILES,
   "Afficher les fichiers et dossiers cachés :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_NEW_PLAYLIST,
   "Nouvelle liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ENTER_NEW_PLAYLIST_NAME,
   "Veuillez entrer le nouveau nom de la liste de lecture :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE_PLAYLIST,
   "Supprimer la liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RENAME_PLAYLIST,
   "Renommer la liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST,
   "Êtes-vous sûr de vouloir supprimer la liste de lecture \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_DELETE_FILE,
   "Impossible de supprimer le fichier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_RENAME_FILE,
   "Impossible de renommer le fichier."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_GATHERING_LIST_OF_FILES,
   "Collecte de la liste des fichiers..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADDING_FILES_TO_PLAYLIST,
   "Ajout de fichiers à la liste de lecture..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY,
   "Entrée dans la liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_NAME,
   "Nom :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_PATH,
   "Emplacement :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_CORE,
   "Cœur :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_DATABASE,
   "Base de données :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS,
   "Extensions :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_EXTENSIONS_PLACEHOLDER,
   "(séparées par des espaces; toutes sont inclues par défaut)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLAYLIST_ENTRY_FILTER_INSIDE_ARCHIVES,
   "Filtrer à l'intérieur des archives"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FOR_THUMBNAILS,
   "(utilisé pour trouver les miniatures)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CONFIRM_DELETE_PLAYLIST_ITEM,
   "Êtes-vous sûr de vouloir supprimer l'élément \"%1\"?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CANNOT_ADD_TO_ALL_PLAYLISTS,
   "Veuillez d'abord choisir une seule liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DELETE,
   "Supprimer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_ENTRY,
   "Ajouter une entrée..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FILES,
   "Ajouter un ou plusieurs fichiers..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_ADD_FOLDER,
   "Ajouter un dossier..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_EDIT,
   "Modifier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FILES,
   "Sélectionner des fichiers"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SELECT_FOLDER,
   "Sélectionner un dossier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_FIELD_MULTIPLE,
   "<multiples>"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_COULD_NOT_UPDATE_PLAYLIST_ENTRY,
   "Erreur lors de la mise à jour de l'entrée dans la liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_PLEASE_FILL_OUT_REQUIRED_FIELDS,
   "Veuillez remplir tous les champs requis."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_NIGHTLY,
   "Mettre à jour RetroArch (nightly)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FINISHED,
   "RetroArch a été mis à jour avec succès. Veuillez redémarrer l'application pour que les modifications prennent effet."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_UPDATE_RETROARCH_FAILED,
   "Mise à jour échouée."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_HELP_ABOUT_CONTRIBUTORS,
   "Contributeurs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CURRENT_SHADER,
   "Shader actuel"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_DOWN,
   "Déplacer vers le bas"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MOVE_UP,
   "Déplacer vers le haut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_LOAD,
   "Charger"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SAVE,
   "Sauvegarder"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE,
   "Supprimer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_REMOVE_PASSES,
   "Supprimer les passages"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_APPLY,
   "Appliquer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_ADD_PASS,
   "Ajouter un passage"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_CLEAR_ALL_PASSES,
   "Supprimer tous les passages"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SHADER_NO_PASSES,
   "Aucun passage de shader."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PASS,
   "Réinitialiser le passage"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL_PASSES,
   "Réinitialiser tous les passages"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_PARAMETER,
   "Réinitialiser le paramètre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_THUMBNAIL,
   "Télécharger la miniature"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALREADY_IN_PROGRESS,
   "Un téléchargement est déjà en cours."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_STARTUP_PLAYLIST,
   "Démarrer dans la liste de lecture :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_TYPE,
   "Miniatures"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_CACHE_LIMIT,
   "Limite du cache des miniatures :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_MENU_VIEW_OPTIONS_THUMBNAIL_DROP_SIZE_LIMIT,
   "Limite de taille des miniatures glissées-déposées :"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS,
   "Télécharger toutes les miniatures"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_ENTIRE_SYSTEM,
   "Système entier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_ALL_THUMBNAILS_THIS_PLAYLIST,
   "Cette liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_THUMBNAIL_PACK_DOWNLOADED_SUCCESSFULLY,
   "Miniatures téléchargées avec succès."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_DOWNLOAD_PLAYLIST_THUMBNAIL_PROGRESS,
   "Réussites : %1 Échecs : %2"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_CORE_OPTIONS,
   "Options de cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET,
   "Réinitialiser"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_RESET_ALL,
   "Tout réinitialiser"
   )

/* Unsorted */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS,
   "Mise à jour"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS,
   "Comptes Cheevos"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END,
   "Point de terminaison de la liste des comptes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS,
   "RetroSuccès"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CORE_COUNTERS,
   "Compteurs de cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_DISK,
   "Aucun disque sélectionné"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRONTEND_COUNTERS,
   "Compteurs de l'interface"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HORIZONTAL_MENU,
   "Menu horizontal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND,
   "Masquer les descripteurs d'appellation des touches spécifiques au cœur non assignés"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW,
   "Afficher les descripteurs d'appellation des touches spécifiques au cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS,
   "Surimpression à l'écran"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY,
   "Historique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOAD_CONTENT_HISTORY,
   "Sélectionner du contenu depuis la liste de lecture d'historique récent."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS,
   "Multimédia"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUBSYSTEM_SETTINGS,
   "Sous-systèmes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_NETPLAY_HOSTS_FOUND,
   "Aucun hôte de jeu en réseau trouvé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PERFORMANCE_COUNTERS,
   "Aucun compteur de performance."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NO_PLAYLISTS,
   "Aucune liste de lecture."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BT_CONNECTED,
   "Connecté"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ONLINE,
   "En ligne"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PORT_DEVICE_NAME,
   "Nom de l'appareil port %d : %s (#%d)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SETTINGS,
   "Réglages des cheats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_SEARCH_SETTINGS,
   "Démarrer ou reprendre la recherche de cheat"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RUN_MUSIC,
   "Lancer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SECONDS,
   "secondes"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_CORE,
   "Démarrer le cœur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_START_CORE,
   "Démarrer le cœur sans contenu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUPPORTED_CORES,
   "Cœurs suggérés"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE,
   "Impossible de lire l'archive."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USER,
   "Utilisateur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_KEYBOARD,
   "Clavier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER,
   "Utiliser le lecteur d'images intégré"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Nombre d'images max en mémoire tampon "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_MAX_SWAPCHAIN_IMAGES,
   "Indique au pilote vidéo d'utiliser explicitement le mode de mise en mémoire tampon spécifié."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_SHADER_PRESET_PARAMETERS,
   "Modifier les préréglages de shaders actuellement utilisés dans le menu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_TWO,
   "Préréglages de shaders"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL_LIST,
   "Parcourir l'URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_URL,
   "Emplacement de l'URL"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BROWSE_START,
   "Démarrer"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME,
   "Pseudo : %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_COMPAT_CONTENT_FOUND,
   "Contenu compatible trouvé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_START_GONG,
   "Démarrer Gong"
   )

/* Unused (Only Exist in Translation Files) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_AUTO,
   "Rapport d'aspect automatique"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ROOM_NICKNAME_LAN,
   "Pseudo (LAN) : %s"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_STATUS,
   "Statut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE,
   "Musique de fond système"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CUSTOM_RATIO,
   "Rapport d'aspect personnalisé"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_ENABLE,
   "Prise en charge de l'enregistrement"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_PATH,
   "Sauvegarder l'enregistrement sous..."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY,
   "Sauvegarder les enregistrements dans le dossier de sortie"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_MATCH_IDX,
   "Afficher la correspondance #"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_MATCH_IDX,
   "Sélectionner la correspondance à afficher."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_ASPECT,
   "Forcer le rapport d'aspect"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SELECT_FROM_PLAYLIST,
   "Sélectionner depuis une liste de lecture"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RESUME,
   "Reprendre"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_RESUME,
   "Reprendre le contenu en cours et quitter le menu rapide."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_VIEW_MATCHES,
   "Voir la liste des %u correspondances"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CREATE_OPTION,
   "Créer un code à partir de cette correspondance"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_DELETE_OPTION,
   "Supprimer cette correspondance"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY,
   "Opacité du pied de page"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_FOOTER_OPACITY,
   "Modifier l'opacité du graphique de pied de page."
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY,
   "Opacité de l'en-tête"
   )
MSG_HASH( /* FIXME Still exists in a comment about being removed */
   MENU_ENUM_SUBLABEL_MATERIALUI_MENU_HEADER_OPACITY,
   "Modifier l'opacité du graphique d'en-tête."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_ENABLE,
   "Jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_RDB_ENTRY_START_CONTENT,
   "Démarrer le contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_PATH,
   "Emplacement de l'historique de contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "ID d'affichage de la sortie"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CRT_SWITCH_RESOLUTION_OUTPUT_DISPLAY_ID,
   "Sélectionner le port de sortie connecté à l'écran à tube cathodique (CRT)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP,
   "Aide"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING,
   "Dépannage audio/vidéo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD,
   "Changement de la manette virtuelle en surimpression"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_LOADING_CONTENT,
   "Chargement de contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT,
   "Analyse de contenu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_WHAT_IS_A_CORE,
   "Qu'est-ce qu'un cœur ?"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO,
   "Envoyer des informations de diagnostic"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_HELP_SEND_DEBUG_INFO,
   "Envoyer les informations de diagnostic pour votre appareil et la configuration de RetroArch à nos serveurs pour analyse."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MANAGEMENT,
   "Réglages de la base de données"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES,
   "Retarder les images du jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_LAN_SCAN_SETTINGS,
   "Analyser le réseau local"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_NETPLAY_LAN_SCAN_SETTINGS,
   "Rechercher et se connecter à des hôtes de jeu en réseau sur le réseau local."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_MODE,
   "Client de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE,
   "Spectateur de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_ENABLE,
   "Limiter la vitesse d'exécution maximale"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_START_SEARCH,
   "Lancer la recherche d'un nouveau cheat code"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_START_SEARCH,
   "Lancer la recherche d'un nouveau cheat. Le nombre de bits peut être changé."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_CONTINUE_SEARCH,
   "Continuer la recherche"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_CONTINUE_SEARCH,
   "Continuer la recherche d'un nouveau cheat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ACHIEVEMENT_LIST_HARDCORE,
   "Succès (Hardcore)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_DETAILS,
   "Détails du cheat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_DETAILS,
   "Gérer les réglages de cheat."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_CHEAT_SEARCH,
   "Lancer ou continuer la recherche de cheat"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_CHEAT_SEARCH,
   "Lancer ou continuer la recherche de cheat codes."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CHEAT_NUM_PASSES,
   "Nombre de passages de cheats"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CHEAT_NUM_PASSES,
   "Augmenter ou diminuer le nombre de cheats."
   )

/* Unused (Needs Confirmation) */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_X,
   "Analogique gauche X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_LEFT_Y,
   "Analogique gauche Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_X,
   "Analogique droit X"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ANALOG_RIGHT_Y,
   "Analogique droit Y"
   )
MSG_HASH(
   MENU_ENUM_LABEL_CHEAT_SEARCH_SETTINGS,
   "Lancer/continuer la recherche de cheats"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST,
   "Liste de pointeurs dans la base de données"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DEVELOPER,
   "Base de données - Filtre : Développeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PUBLISHER,
   "Base de données - Filtre : Éditeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ORIGIN,
   "Base de données - Filtre : Origine"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_FRANCHISE,
   "Base de données - Filtre : Saga"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ESRB_RATING,
   "Base de données - Filtre : Classification ESRB"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_ELSPA_RATING,
   "Base de données - Filtre : Classification ELSPA"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_PEGI_RATING,
   "Base de données - Filtre : Classification PEGI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_CERO_RATING,
   "Base de données - Filtre : Classification CERO"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_BBFC_RATING,
   "Base de données - Filtre : Classification BBFC"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_MAX_USERS,
   "Base de données - Filtre : Nombre d'utilisateurs maximum"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_MONTH,
   "Base de données - Filtre : Date de sortie par mois"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_RELEASEDATE_BY_YEAR,
   "Base de données - Filtre : Date de sortie par année"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_ISSUE,
   "Base de données - Filtre : Numéro de magazine Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_EDGE_MAGAZINE_RATING,
   "Base de données - Filtre : Note du magazine Edge"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DATABASE_CURSOR_LIST_ENTRY_DATABASE_INFO,
   "Informations de la base de données"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONFIG,
   "Configuration"
   )
MSG_HASH( /* FIXME Seems related to MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY, possible duplicate */
   MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIR,
   "Téléchargements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_NETPLAY_SETTINGS,
   "Réglages de jeu en réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_SLANG_SUPPORT,
   "Prise en charge de Slang "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FBO_SUPPORT,
   "Prise en charge du rendu vers texture OpenGL/Direct3D (shaders multi-passages) "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CONTENT_DIR,
   "Contenu"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_CONTENT_DIR,
   "Généralement défini par les développeurs qui compilent les applications libretro/RetroArch pour pointer vers des assets."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_ASK_ARCHIVE,
   "Demander"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS,
   "Contrôles de base du menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_CONFIRM,
   "Confirmer/Accepter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_INFO,
   "Informations"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_QUIT,
   "Quitter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_SCROLL_UP,
   "Faire défiler vers le haut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_START,
   "Par défaut"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_KEYBOARD,
   "Afficher/masquer le clavier"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BASIC_MENU_ENUM_CONTROLS_TOGGLE_MENU,
   "Afficher/masquer le menu"
   )

/* Discord Status */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_MENU,
   "Dans le menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME,
   "Dans le jeu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_IN_GAME_PAUSED,
   "Dans le jeu (En pause)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PLAYING,
   "En jeu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_DISCORD_STATUS_PAUSED,
   "En pause"
   )

/* Notifications */

MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_START_WHEN_LOADED,
   "Le jeu en réseau débutera quand un contenu sera chargé."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_NETPLAY_LOAD_CONTENT_MANUALLY,
   "Impossible de trouver un cœur ou un jeu adapté, veuillez charger le contenu manuellement."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK,
   "Votre pilote graphique n'est pas compatible avec le pilote vidéo actuel de RetroArch, retour au pilote %s. Veuillez redémarrer RetroArch pour que les modifications prennent effet."
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_SUCCESS,
   "Installation du cœur réussie"
   )
MSG_HASH( /* FIXME Should be MSG_ */
   MENU_ENUM_LABEL_VALUE_SIDELOAD_CORE_ERROR,
   "Installation du cœur échouée"
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_INSTRUCTIONS,
   "Appuyez cinq fois sur Droite pour supprimer tous les cheats."
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_DEBUG_INFO,
   "Échec d'enregistrement des informations de diagnostic."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_DEBUG_INFO,
   "Échec d'envoi des informations de diagnostic au serveur."
   )
MSG_HASH(
   MSG_SENDING_DEBUG_INFO,
   "Envoi des informations de diagnostic..."
   )
MSG_HASH(
   MSG_SENT_DEBUG_INFO,
   "Les informations de diagnostic ont été envoyées au serveur avec succès. Votre numéro d'identification est %u."
   )
MSG_HASH(
   MSG_PRESS_TWO_MORE_TIMES_TO_SEND_DEBUG_INFO,
   "Appuyez deux fois de plus pour soumettre les informations de diagnostic à l'équipe de RetroArch."
   )
MSG_HASH(
   MSG_PRESS_ONE_MORE_TIME_TO_SEND_DEBUG_INFO,
   "Appuyez une fois de plus pour soumettre les informations de diagnostic à l'équipe de RetroArch."
   )
MSG_HASH(
   MSG_AUDIO_MIXER_VOLUME,
   "Volume du mixeur audio global"
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCAN_COMPLETE,
   "Recherche de jeu en réseau terminé."
   )
MSG_HASH(
   MSG_SORRY_UNIMPLEMENTED_CORES_DONT_DEMAND_CONTENT_NETPLAY,
   "Désolé, non implémenté : les cœurs qui ne demandent pas de contenu ne peuvent pas participer au jeu en réseau."
   )
MSG_HASH(
   MSG_DEVICE_DISCONNECTED_FROM_PORT,
   "Périphérique déconnecté du port"
   )
MSG_HASH(
   MSG_UNKNOWN_NETPLAY_COMMAND_RECEIVED,
   "Commande de jeu en réseau inconnue reçue"
   )
MSG_HASH(
   MSG_FILE_ALREADY_EXISTS_SAVING_TO_BACKUP_BUFFER,
   "Fichier déjà existant. Enregistrement dans la mémoire tampon de sauvegarde"
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM,
   "Connexion reçue depuis : \"%s\""
   )
MSG_HASH(
   MSG_GOT_CONNECTION_FROM_NAME,
   "Connexion reçue depuis : \"%s (%s)\""
   )
MSG_HASH(
   MSG_PUBLIC_ADDRESS,
   "Mappage du port Netplay réussi"
   )
MSG_HASH(
   MSG_UPNP_FAILED,
   "Mappage du port de jeu en réseau UPnP échoué"
   )
MSG_HASH(
   MSG_NO_ARGUMENTS_SUPPLIED_AND_NO_MENU_BUILTIN,
   "Aucun paramètre fourni et pas de menu intégré, affichage de l'aide..."
   )
MSG_HASH(
   MSG_SETTING_DISK_IN_TRAY,
   "Insertion de disque dans le lecteur"
   )
MSG_HASH(
   MSG_WAITING_FOR_CLIENT,
   "En attente d'un client ..."
   )
MSG_HASH(
   MSG_ROOM_NOT_CONNECTABLE,
   "Votre salon n'est pas connectable depuis internet."
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_LEFT_THE_GAME,
   "Vous avez quitté le jeu"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_AS_PLAYER_N,
   "Vous avez rejoint le jeu en tant que joueur %u"
   )
MSG_HASH(
   MSG_NETPLAY_YOU_HAVE_JOINED_WITH_INPUT_DEVICES_S,
   "Vous avez rejoint le jeu avec des dispositifs d'entrée %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_PLAYER_S_LEFT,
   "Joueur %.*s à quitté le jeu"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_AS_PLAYER_N,
   "%.*s à rejoint le jeu en tant que joueur %u"
   )
MSG_HASH(
   MSG_NETPLAY_S_HAS_JOINED_WITH_INPUT_DEVICES_S,
   "%.*s à rejoint le jeu avec des dispositifs d'entrée %.*s"
   )
MSG_HASH(
   MSG_NETPLAY_NOT_RETROARCH,
   "Une tentative de connexion de jeu en réseau à échouée car RetroArch n'est pas en cours d'exécution chez le partenaire, ou est sur une version ancienne de RetroArch."
   )
MSG_HASH(
   MSG_NETPLAY_OUT_OF_DATE,
   "Le partenaire de jeu en réseau est sur une version ancienne de RetroArch. Connexion impossible."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_VERSIONS,
   "ATTENTION : Un partenaire de jeu en réseau est sur une version différente de RetroArch. Si des problèmes surviennent, utilisez la même version."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORES,
   "Un partenaire de jeu en réseau est sur un cœur different. Connexion impossible."
   )
MSG_HASH(
   MSG_NETPLAY_DIFFERENT_CORE_VERSIONS,
   "ATTENTION : Un partenaire de jeu en réseau est sur une version différente du cœur. Si des problèmes surviennent, utilisez la même version."
   )
MSG_HASH(
   MSG_NETPLAY_ENDIAN_DEPENDENT,
   "Ce cœur ne prend pas en charge le jeu en réseau inter-architectures entre ces systèmes"
   )
MSG_HASH(
   MSG_NETPLAY_PLATFORM_DEPENDENT,
   "Ce cœur ne prend pas en charge le jeu en réseau inter-architectures"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_PASSWORD,
   "Entrez le mot de passe du serveur de jeu en réseau :"
   )
MSG_HASH(
   MSG_NETPLAY_ENTER_CHAT,
   "Entrez un message sur le chat de jeu en réseau :"
   )
MSG_HASH(
   MSG_DISCORD_CONNECTION_REQUEST,
   "Voulez-vous autoriser la connexion de l'utilisateur :"
   )
MSG_HASH(
   MSG_NETPLAY_INCORRECT_PASSWORD,
   "Mot de passe incorrect"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_NAMED_HANGUP,
   "\"%s\" s'est déconnecté"
   )
MSG_HASH(
   MSG_NETPLAY_SERVER_HANGUP,
   "Un client de jeu en réseau s'est déconnecté"
   )
MSG_HASH(
   MSG_NETPLAY_CLIENT_HANGUP,
   "Jeu en réseau déconnecté"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_UNPRIVILEGED,
   "Vous n'avez pas la permission de jouer"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NO_SLOTS,
   "Aucune place de libre pour jouer"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY_NOT_AVAILABLE,
   "Les dispositifs d'entrée demandés ne sont pas disponibles"
   )
MSG_HASH(
   MSG_NETPLAY_CANNOT_PLAY,
   "Impossible de basculer en mode jeu"
   )
MSG_HASH(
   MSG_NETPLAY_PEER_PAUSED,
   "Le partenaire de jeu en réseau \"%s\" à mis en pause"
   )
MSG_HASH(
   MSG_NETPLAY_CHANGED_NICK,
   "Votre pseudo est maintenant \"%s\""
   )
MSG_HASH(
   MSG_AUDIO_VOLUME,
   "Volume sonore"
   )
MSG_HASH(
   MSG_AUTODETECT,
   "Détection automatique"
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FROM,
   "Chargement auto d'une sauvegarde instantanée depuis"
   )
MSG_HASH(
   MSG_CAPABILITIES,
   "Capacités"
   )
MSG_HASH(
   MSG_CONNECTING_TO_NETPLAY_HOST,
   "Connexion à l'hôte de jeu en réseau"
   )
MSG_HASH(
   MSG_CONNECTING_TO_PORT,
   "Connexion au port"
   )
MSG_HASH(
   MSG_CONNECTION_SLOT,
   "Emplacement de connexion"
   )
MSG_HASH(
   MSG_FETCHING_CORE_LIST,
   "Récupération de la liste des cœurs..."
   )
MSG_HASH(
   MSG_CORE_LIST_FAILED,
   "Échec de récupération de la liste des cœurs !"
   )
MSG_HASH(
   MSG_LATEST_CORE_INSTALLED,
   "Dernière version déjà installée : "
   )
MSG_HASH(
   MSG_UPDATING_CORE,
   "Mise à jour du cœur : "
   )
MSG_HASH(
   MSG_DOWNLOADING_CORE,
   "Téléchargement du cœur : "
   )
MSG_HASH(
   MSG_EXTRACTING_CORE,
   "Extraction du cœur : "
   )
MSG_HASH(
   MSG_CORE_INSTALLED,
   "Cœur installé : "
   )
MSG_HASH(
   MSG_CORE_INSTALL_FAILED,
   "Échec d'installation du cœur : "
   )
MSG_HASH(
   MSG_SCANNING_CORES,
   "Analyse des cœurs..."
   )
MSG_HASH(
   MSG_CHECKING_CORE,
   "Vérification du cœur : "
   )
MSG_HASH(
   MSG_ALL_CORES_UPDATED,
   "Tous les cœurs installés sont à jour"
   )
MSG_HASH(
   MSG_ALL_CORES_SWITCHED_PFD,
   "Tous les cœurs supportés ont été remplacés par les versions du Play Store"
   )
MSG_HASH(
   MSG_NUM_CORES_UPDATED,
   "cœurs mis à jour : "
   )
MSG_HASH(
   MSG_NUM_CORES_LOCKED,
   "cœurs ignorés : "
   )
MSG_HASH(
   MSG_CORE_UPDATE_DISABLED,
   "Mise à jour du cœur désactivée - cœur verrouillé : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_RESETTING_CORES,
   "Réinitialisation des cœurs : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CORES_RESET,
   "Cœurs réinitialisés : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_CLEANING_PLAYLIST,
   "Nettoyage de la liste de lecture : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_PLAYLIST_CLEANED,
   "Liste de lecture nettoyée : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_MISSING_CONFIG,
   "Rafraîchissement échoué - la liste de lecture ne contient aucun enregistrement d'analyse valide : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CONTENT_DIR,
   "Rafraîchissement échoué - dossier de contenu invalide/manquant : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_SYSTEM_NAME,
   "Rafraîchissement échoué - nom du système invalide/manquant : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_CORE,
   "Rafraîchissement échoué - cœur non valide : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_INVALID_DAT_FILE,
   "Rafraîchissement échoué - fichier DAT d'arcade invalide/manquant : "
   )
MSG_HASH(
   MSG_PLAYLIST_MANAGER_REFRESH_DAT_FILE_TOO_LARGE,
   "Rafraîchissement échoué - fichier DAT d'arcade trop volumineux (mémoire insuffisante): "
   )
MSG_HASH(
   MSG_ADDED_TO_FAVORITES,
   "Ajouté aux favoris"
   )
MSG_HASH(
   MSG_ADD_TO_FAVORITES_FAILED,
   "Échec de l'ajout aux favoris : liste de lecture pleine"
   )
MSG_HASH(
   MSG_SET_CORE_ASSOCIATION,
   "Cœur associé : "
   )
MSG_HASH(
   MSG_RESET_CORE_ASSOCIATION,
   "L'association au cœur a été réinitialisée pour l'entrée dans la liste de lecture."
   )
MSG_HASH(
   MSG_APPENDED_DISK,
   "Disque ajouté"
   )
MSG_HASH(
   MSG_FAILED_TO_APPEND_DISK,
   "Échec de l'ajout du disque"
   )
MSG_HASH(
   MSG_APPLICATION_DIR,
   "Dossier de l'application"
   )
MSG_HASH(
   MSG_APPLYING_CHEAT,
   "Application des changements de cheats."
   )
MSG_HASH(
   MSG_APPLYING_PATCH,
   "Application du patch : %s"
   )
MSG_HASH(
   MSG_APPLYING_SHADER,
   "Appliquer le shader"
   )
MSG_HASH(
   MSG_AUDIO_MUTED,
   "Son coupé."
   )
MSG_HASH(
   MSG_AUDIO_UNMUTED,
   "Son réactivé."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_ERROR_SAVING,
   "Erreur lors de la sauvegarde d'une configuration automatique."
   )
MSG_HASH(
   MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY,
   "Configuration automatique enregistrée avec succès."
   )
MSG_HASH(
   MSG_AUTOSAVE_FAILED,
   "Impossible d'initialiser l'enregistrement automatique."
   )
MSG_HASH(
   MSG_AUTO_SAVE_STATE_TO,
   "Sauvegarde instantanée automatique vers"
   )
MSG_HASH(
   MSG_BLOCKING_SRAM_OVERWRITE,
   "Empêcher l'écrasement de la mémoire SRAM"
   )
MSG_HASH(
   MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT,
   "Appeler l'interface de commande sur le port"
   )
MSG_HASH(
   MSG_BYTES,
   "octets"
   )
MSG_HASH(
   MSG_CANNOT_INFER_NEW_CONFIG_PATH,
   "Impossible de déduire le nouvel emplacement du fichier de configuration. Utilisation de l'heure actuelle."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_ENABLE,
   "Mode Hardcore activé pour les succès, la sauvegarde instantanée et le rembobinage ont été désactivés."
   )
MSG_HASH(
   MSG_COMPARING_WITH_KNOWN_MAGIC_NUMBERS,
   "Comparaison avec les nombres magiques connus..."
   )
MSG_HASH(
   MSG_COMPILED_AGAINST_API,
   "Compilé avec l'API"
   )
MSG_HASH(
   MSG_CONFIG_DIRECTORY_NOT_SET,
   "Dossier de configuration non défini. Impossible de sauvegarder la nouvelle configuration."
   )
MSG_HASH(
   MSG_CONNECTED_TO,
   "Connecté à"
   )
MSG_HASH(
   MSG_CONTENT_CRC32S_DIFFER,
   "Le CRC32 du contenu est différent. Impossible d'utiliser des jeux non-identiques."
   )
MSG_HASH(
   MSG_PING_TOO_HIGH,
   "Votre ping est trop élevé pour cet hôte."
   )
MSG_HASH(
   MSG_CONTENT_LOADING_SKIPPED_IMPLEMENTATION_WILL_DO_IT,
   "Chargement du contenu ignoré. L'implémentation va le charger elle-même."
   )
MSG_HASH(
   MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "Le cœur ne prend pas en charge les sauvegardes instantanées."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_CREATED_SUCCESSFULLY,
   "Fichier d'options du cœur créé avec succès."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FILE_REMOVED_SUCCESSFULLY,
   "Fichier d'options du cœur supprimé avec succès."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_RESET,
   "Toutes les options du cœur ont été réinitialisées aux valeurs par défaut."
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSHED,
   "Options du cœur enregistrées vers :"
   )
MSG_HASH(
   MSG_CORE_OPTIONS_FLUSH_FAILED,
   "Impossible d'enregistrer les options du cœur vers :"
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER,
   "Impossible de trouver un pilote suivant."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_COMPATIBLE_SYSTEM,
   "Impossible de trouver un système compatible."
   )
MSG_HASH(
   MSG_COULD_NOT_FIND_VALID_DATA_TRACK,
   "Impossible de trouver une piste de données valide"
   )
MSG_HASH(
   MSG_COULD_NOT_OPEN_DATA_TRACK,
   "Impossible d'ouvrir la piste de données."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_CONTENT_FILE,
   "Impossible de lire le fichier de contenu."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_MOVIE_HEADER,
   "Impossible de lire l'en-tête du film."
   )
MSG_HASH(
   MSG_COULD_NOT_READ_STATE_FROM_MOVIE,
   "Impossible de lire l'état du film."
   )
MSG_HASH(
   MSG_CRC32_CHECKSUM_MISMATCH,
   "Incohérence de la somme de contrôle CRC32 entre le fichier du contenu et sa somme de contrôle enregistrée dans l'en-tête du fichier de relecture. Replay très susceptible de se désynchroniser lors de la lecture."
   )
MSG_HASH(
   MSG_CUSTOM_TIMING_GIVEN,
   "Temps personnalisé attribué"
   )
MSG_HASH(
   MSG_DECOMPRESSION_ALREADY_IN_PROGRESS,
   "Décompression déjà en cours."
   )
MSG_HASH(
   MSG_DECOMPRESSION_FAILED,
   "Échec à la décompression."
   )
MSG_HASH(
   MSG_DETECTED_VIEWPORT_OF,
   "Taille de la fenêtre d'affichage détectée de"
   )
MSG_HASH(
   MSG_DID_NOT_FIND_A_VALID_CONTENT_PATCH,
   "Impossible de trouver un patch valide pour ce contenu."
   )
MSG_HASH(
   MSG_DISCONNECT_DEVICE_FROM_A_VALID_PORT,
   "Déconnecter le périphérique d'un port valide."
   )
MSG_HASH(
   MSG_DISK_CLOSED,
   "Plateau du lecteur de disque virtuel fermé."
   )
MSG_HASH(
   MSG_DISK_EJECTED,
   "Plateau du lecteur de disque virtuel éjecté."
   )
MSG_HASH(
   MSG_DOWNLOADING,
   "Téléchargement"
   )
MSG_HASH(
   MSG_INDEX_FILE,
   "de l'index"
   )
MSG_HASH(
   MSG_DOWNLOAD_FAILED,
   "Échec du téléchargement"
   )
MSG_HASH(
   MSG_ERROR,
   "Erreur"
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_CONTENT,
   "Le cœur Libretro nécessite du contenu, mais aucun n'a été fourni."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_SPECIAL_CONTENT,
   "Le cœur Libretro nécessite un contenu spécial, mais aucun n'a été fourni."
   )
MSG_HASH(
   MSG_ERROR_LIBRETRO_CORE_REQUIRES_VFS,
   "Le cœur ne prend pas en charge le VFS, et le chargement à partir d'une copie locale a échoué"
   )
MSG_HASH(
   MSG_ERROR_PARSING_ARGUMENTS,
   "Erreur lors de l'analyse des arguments."
   )
MSG_HASH(
   MSG_ERROR_SAVING_CORE_OPTIONS_FILE,
   "Erreur lors de l'enregistrement du fichier d'options du cœur."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_CORE_OPTIONS_FILE,
   "Erreur lors de la suppression du fichier d'options du cœur."
   )
MSG_HASH(
   MSG_ERROR_SAVING_REMAP_FILE,
   "Erreur lors de l'enregistrement du fichier de remappage."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_REMAP_FILE,
   "Erreur lors de la suppression du fichier de remappage."
   )
MSG_HASH(
   MSG_ERROR_SAVING_SHADER_PRESET,
   "Erreur lors de l'enregistrement des préréglages de shaders."
   )
MSG_HASH(
   MSG_EXTERNAL_APPLICATION_DIR,
   "Dossier d'applications externes"
   )
MSG_HASH(
   MSG_EXTRACTING,
   "Extraction"
   )
MSG_HASH(
   MSG_EXTRACTING_FILE,
   "Extraction du fichier"
   )
MSG_HASH(
   MSG_FAILED_SAVING_CONFIG_TO,
   "Erreur lors de l'enregistrement de la configuration vers"
   )
MSG_HASH(
   MSG_FAILED_TO_ACCEPT_INCOMING_SPECTATOR,
   "Échec à l'accueil du spectateur entrant."
   )
MSG_HASH(
   MSG_FAILED_TO_ALLOCATE_MEMORY_FOR_PATCHED_CONTENT,
   "Échec d'allocation de mémoire pour le contenu patché..."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER,
   "Échec à l'application du shader."
   )
MSG_HASH(
   MSG_FAILED_TO_APPLY_SHADER_PRESET,
   "Échec à l'application du préréglage de shaders :"
   )
MSG_HASH(
   MSG_FAILED_TO_BIND_SOCKET,
   "Échec à l'attribution du socket."
   )
MSG_HASH(
   MSG_FAILED_TO_CREATE_THE_DIRECTORY,
   "Échec à la création du dossier."
   )
MSG_HASH(
   MSG_FAILED_TO_EXTRACT_CONTENT_FROM_COMPRESSED_FILE,
   "Échec à l'extraction de contenu depuis le fichier compressé."
   )
MSG_HASH(
   MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT,
   "Échec à l'obtention du pseudo du client."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD,
   "Échec de chargement."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_CONTENT,
   "Échec de chargement du contenu."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_MOVIE_FILE,
   "Échec de chargement du fichier vidéo."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_OVERLAY,
   "Échec de chargement de la surimpression."
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_STATE,
   "Échec de chargement de la sauvegarde instantanée depuis"
   )
MSG_HASH(
   MSG_FAILED_TO_OPEN_LIBRETRO_CORE,
   "Échec de l'ouverture du cœur Libretro"
   )
MSG_HASH(
   MSG_FAILED_TO_PATCH,
   "Échec du patch"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT,
   "Échec de l'obtention de l'entête depuis le client."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME,
   "Échec de l'obtention du pseudo."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST,
   "Échec de l'obtention du pseudo depuis l'hôte."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST,
   "Échec de l'obtention de la taille du pseudo depuis l'hôte."
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST,
   "Échec de l'obtention des données SRAM depuis l'hôte."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY,
   "Échec de l'éjection du disque depuis le lecteur."
   )
MSG_HASH(
   MSG_FAILED_TO_REMOVE_TEMPORARY_FILE,
   "Échec de la suppression du fichier temporaire"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_SRAM,
   "Échec de la sauvegarde de la SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_LOAD_SRAM,
   "Échec du chargement de la SRAM"
   )
MSG_HASH(
   MSG_FAILED_TO_SAVE_STATE_TO,
   "Échec de la sauvegarde instantanée vers"
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME,
   "Échec de l'envoi du pseudo."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_SIZE,
   "Échec de l'envoi de la taille du pseudo."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT,
   "Échec de l'envoi du pseudo vers le client."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_NICKNAME_TO_HOST,
   "Échec de l'envoi du pseudo vers l'hôte."
   )
MSG_HASH(
   MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT,
   "Échec de l'envoi des données SRAM vers le client."
   )
MSG_HASH(
   MSG_FAILED_TO_START_AUDIO_DRIVER,
   "Échec de démarrage du pilote audio. Continuera sans le son."
   )
MSG_HASH(
   MSG_FAILED_TO_START_MOVIE_RECORD,
   "Échec du démarrage de l'enregistrement vidéo."
   )
MSG_HASH(
   MSG_FAILED_TO_START_RECORDING,
   "Échec du démarrage de l'enregistrement."
   )
MSG_HASH(
   MSG_FAILED_TO_TAKE_SCREENSHOT,
   "Échec de la capture d'écran."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_LOAD_STATE,
   "Échec de l'annulation du chargement d'une sauvegarde instantanée."
   )
MSG_HASH(
   MSG_FAILED_TO_UNDO_SAVE_STATE,
   "Échec de l'annulation d'une sauvegarde instantanée."
   )
MSG_HASH(
   MSG_FAILED_TO_UNMUTE_AUDIO,
   "Échec de la réactivation du son."
   )
MSG_HASH(
   MSG_FATAL_ERROR_RECEIVED_IN,
   "Erreur fatale reçue dans"
   )
MSG_HASH(
   MSG_FILE_NOT_FOUND,
   "Fichier non trouvé"
   )
MSG_HASH(
   MSG_FOUND_AUTO_SAVESTATE_IN,
   "Sauvegarde instantanée automatique trouvée dans"
   )
MSG_HASH(
   MSG_FOUND_DISK_LABEL,
   "Label de disque trouvé"
   )
MSG_HASH(
   MSG_FOUND_FIRST_DATA_TRACK_ON_FILE,
   "Première piste de données trouvée dans le fichier"
   )
MSG_HASH(
   MSG_FOUND_LAST_STATE_SLOT,
   "Dernier emplacement de sauvegarde instantanée trouvé"
   )
MSG_HASH(
   MSG_FOUND_SHADER,
   "Shader trouvé"
   )
MSG_HASH(
   MSG_FRAMES,
   "Images"
   )
MSG_HASH(
   MSG_GAME_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Options de cœur spécifiques au jeu trouvées dans"
   )
MSG_HASH(
   MSG_FOLDER_SPECIFIC_CORE_OPTIONS_FOUND_AT,
   "Options de cœur spécifiques au dossier trouvées dans"
   )
MSG_HASH(
   MSG_GOT_INVALID_DISK_INDEX,
   "Numéro de disque non valide."
   )
MSG_HASH(
   MSG_GRAB_MOUSE_STATE,
   "État de la capture de la souris"
   )
MSG_HASH(
   MSG_GAME_FOCUS_ON,
   "Jeu au premier plan"
   )
MSG_HASH(
   MSG_GAME_FOCUS_OFF,
   "Jeu en arrière-plan"
   )
MSG_HASH(
   MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING,
   "Le cœur Libretro utilise le rendu matériel. Doit également utiliser les filtres vidéo lors de l'enregistrement."
   )
MSG_HASH(
   MSG_INFLATED_CHECKSUM_DID_NOT_MATCH_CRC32,
   "La somme de contrôle du fichier décompressé ne correspond pas au CRC32."
   )
MSG_HASH(
   MSG_INPUT_CHEAT,
   "Saisir le cheat"
   )
MSG_HASH(
   MSG_INPUT_CHEAT_FILENAME,
   "Saisir le nom du fichier de cheats"
   )
MSG_HASH(
   MSG_INPUT_PRESET_FILENAME,
   "Saisir le nom du fichier de préréglages"
   )
MSG_HASH(
   MSG_INPUT_RENAME_ENTRY,
   "Entrez le nouveau nom"
   )
MSG_HASH(
   MSG_INTERNAL_STORAGE,
   "Stockage interne"
   )
MSG_HASH(
   MSG_REMOVABLE_STORAGE,
   "Stockage amovible"
   )
MSG_HASH(
   MSG_INVALID_NICKNAME_SIZE,
   "Taille du pseudo non valide."
   )
MSG_HASH(
   MSG_IN_BYTES,
   "en octets "
   )
MSG_HASH(
   MSG_IN_MEGABYTES,
   "en mégaoctets "
   )
MSG_HASH(
   MSG_IN_GIGABYTES,
   "en gigaoctets "
   )
MSG_HASH(
   MSG_LIBRETRO_ABI_BREAK,
   "est compilé avec une version différente de l'implémentation de libretro actuelle."
   )
MSG_HASH(
   MSG_LIBRETRO_FRONTEND,
   "Interface pour libretro"
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT,
   "Sauvegarde instantanée chargée depuis l'emplacement #%d."
   )
MSG_HASH(
   MSG_LOADED_STATE_FROM_SLOT_AUTO,
   "Sauvegarde instantanée chargée depuis l'emplacement #-1 (auto)."
   )
MSG_HASH(
   MSG_LOADING,
   "Chargement"
   )
MSG_HASH(
   MSG_FIRMWARE,
   "Un ou plusieurs fichiers de firmware sont manquants"
   )
MSG_HASH(
   MSG_LOADING_CONTENT_FILE,
   "Chargement du fichier de contenu"
   )
MSG_HASH(
   MSG_LOADING_HISTORY_FILE,
   "Chargement du fichier d'historique"
   )
MSG_HASH(
   MSG_LOADING_FAVORITES_FILE,
   "Chargement du fichier des favoris"
   )
MSG_HASH(
   MSG_LOADING_STATE,
   "Chargement de la sauvegarde instantanée"
   )
MSG_HASH(
   MSG_MEMORY,
   "Mémoire"
   )
MSG_HASH(
   MSG_MOVIE_FILE_IS_NOT_A_VALID_BSV1_FILE,
   "Le fichier vidéo de relecture n'est pas un fichier BSV1 valide."
   )
MSG_HASH(
   MSG_MOVIE_FORMAT_DIFFERENT_SERIALIZER_VERSION,
   "Le format de la vidéo de relecture semble avoir une version différente du sérialiseur. Échec très probable."
   )
MSG_HASH(
   MSG_MOVIE_PLAYBACK_ENDED,
   "La relecture des touches pressées est terminée."
   )
MSG_HASH(
   MSG_MOVIE_RECORD_STOPPED,
   "Arrêt de l'enregistrement vidéo."
   )
MSG_HASH(
   MSG_NETPLAY_FAILED,
   "Échec de l'initialisation du jeu en réseau."
   )
MSG_HASH(
   MSG_NO_CONTENT_STARTING_DUMMY_CORE,
   "Aucun contenu, chargement d'un cœur factice."
   )
MSG_HASH(
   MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET,
   "Aucune sauvegarde instantanée n'a encore été écrasé."
   )
MSG_HASH(
   MSG_NO_STATE_HAS_BEEN_LOADED_YET,
   "Aucune sauvegarde instantanée n'a encore été chargée."
   )
MSG_HASH(
   MSG_OVERRIDES_ERROR_SAVING,
   "Erreur lors de l'enregistrement du fichier de remplacement de configuration."
   )
MSG_HASH(
   MSG_OVERRIDES_SAVED_SUCCESSFULLY,
   "Fichier de remplacement de configuration enregistré avec succès."
   )
MSG_HASH(
   MSG_PAUSED,
   "En pause."
   )
MSG_HASH(
   MSG_READING_FIRST_DATA_TRACK,
   "Lecture de la première piste de données..."
   )
MSG_HASH(
   MSG_RECORDING_TERMINATED_DUE_TO_RESIZE,
   "Enregistrement interrompu à cause du redimensionnement."
   )
MSG_HASH(
   MSG_RECORDING_TO,
   "Enregistrement vers"
   )
MSG_HASH(
   MSG_REDIRECTING_CHEATFILE_TO,
   "Redirection du fichier de cheats vers"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVEFILE_TO,
   "Redirection du fichier de sauvegarde vers"
   )
MSG_HASH(
   MSG_REDIRECTING_SAVESTATE_TO,
   "Redirection du fichier de sauvegarde instantanée vers"
   )
MSG_HASH(
   MSG_REMAP_FILE_SAVED_SUCCESSFULLY,
   "Fichier de remappage enregistré avec succès."
   )
MSG_HASH(
   MSG_REMAP_FILE_REMOVED_SUCCESSFULLY,
   "Fichier de remappage supprimé avec succès."
   )
MSG_HASH(
   MSG_REMOVED_DISK_FROM_TRAY,
   "Disque retiré du lecteur."
   )
MSG_HASH(
   MSG_REMOVING_TEMPORARY_CONTENT_FILE,
   "Suppression du fichier de contenu temporaire"
   )
MSG_HASH(
   MSG_RESET,
   "Réinitialisation"
   )
MSG_HASH(
   MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT,
   "Redémarrage de l'enregistrement à cause de la réinitialisation du pilote."
   )
MSG_HASH(
   MSG_RESTORED_OLD_SAVE_STATE,
   "Ancienne sauvegarde instantanée restaurée."
   )
MSG_HASH(
   MSG_RESTORING_DEFAULT_SHADER_PRESET_TO,
   "Shaders : restauration des préréglages de shaders par défaut vers"
   )
MSG_HASH(
   MSG_REVERTING_SAVEFILE_DIRECTORY_TO,
   "Rétablissement du dossier de sauvegarde vers"
   )
MSG_HASH(
   MSG_REVERTING_SAVESTATE_DIRECTORY_TO,
   "Rétablissement du dossier de sauvegarde instantanée vers"
   )
MSG_HASH(
   MSG_REWINDING,
   "Rembobinage."
   )
MSG_HASH(
   MSG_REWIND_UNSUPPORTED,
   "Rembobinage indisponible car ce cœur ne prend pas en charge la sauvegarde instantanée sérialisée."
   )
MSG_HASH(
   MSG_REWIND_INIT,
   "Initialisation de la mémoire tampon de rembobinage avec la taille"
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED,
   "Échec de l'initialisation de la mémoire tampon de rembobinage. Le rembobinage sera désactivé."
   )
MSG_HASH(
   MSG_REWIND_INIT_FAILED_THREADED_AUDIO,
   "L'implementation utilise plusieurs fils d'exécution pour l'audio. Incompatible avec le rembobinage."
   )
MSG_HASH(
   MSG_REWIND_REACHED_END,
   "Fin de la mémoire tampon de rembobinage atteinte."
   )
MSG_HASH(
   MSG_SAVED_NEW_CONFIG_TO,
   "Nouvelle configuration enregistrée vers"
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT,
   "Sauvegarde instantanée enregistrée vers l'emplacement #%d."
   )
MSG_HASH(
   MSG_SAVED_STATE_TO_SLOT_AUTO,
   "Sauvegarde instantanée enregistrée vers l'emplacement #-1 (auto)."
   )
MSG_HASH(
   MSG_SAVED_SUCCESSFULLY_TO,
   "Enregistré avec succès vers"
   )
MSG_HASH(
   MSG_SAVING_RAM_TYPE,
   "Enregistrement du type de RAM"
   )
MSG_HASH(
   MSG_SAVING_STATE,
   "Sauvegarde instantanée en cours"
   )
MSG_HASH(
   MSG_SCANNING,
   "Analyse en cours"
   )
MSG_HASH(
   MSG_SCANNING_OF_DIRECTORY_FINISHED,
   "Analyse du dossier terminée"
   )
MSG_HASH(
   MSG_SENDING_COMMAND,
   "Envoi de la commande"
   )
MSG_HASH(
   MSG_SEVERAL_PATCHES_ARE_EXPLICITLY_DEFINED,
   "Plusieurs patchs sont explicitement définis, tous sont ignorés..."
   )
MSG_HASH(
   MSG_SHADER_PRESET_SAVED_SUCCESSFULLY,
   "Préréglages de shaders enregistrés avec succès."
   )
MSG_HASH(
   MSG_SKIPPING_SRAM_LOAD,
   "Chargement de la SRAM ignoré."
   )
MSG_HASH(
   MSG_SLOW_MOTION,
   "Ralenti."
   )
MSG_HASH(
   MSG_FAST_FORWARD,
   "Avance rapide."
   )
MSG_HASH(
   MSG_SLOW_MOTION_REWIND,
   "Rembobinage au ralenti."
   )
MSG_HASH(
   MSG_SRAM_WILL_NOT_BE_SAVED,
   "La SRAM ne sera pas sauvegardée."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_PLAYBACK,
   "Démarrage de la lecture vidéo."
   )
MSG_HASH(
   MSG_STARTING_MOVIE_RECORD_TO,
   "Démarrage de l'enregistrement vidéo vers"
   )
MSG_HASH(
   MSG_STATE_SIZE,
   "Taille de la sauvegarde instantanée"
   )
MSG_HASH(
   MSG_STATE_SLOT,
   "Emplacement de la sauvegarde instantanée"
   )
MSG_HASH(
   MSG_TAKING_SCREENSHOT,
   "Capture d'écran."
   )
MSG_HASH(
   MSG_SCREENSHOT_SAVED,
   "Capture d'écran enregistrée"
   )
MSG_HASH(
   MSG_ACHIEVEMENT_UNLOCKED,
   "Succès débloqué"
   )
MSG_HASH(
   MSG_CHANGE_THUMBNAIL_TYPE,
   "Changer le type de miniatures"
   )
MSG_HASH(
   MSG_TOGGLE_FULLSCREEN_THUMBNAILS,
   "Miniatures en plein écran"
   )
MSG_HASH(
   MSG_TOGGLE_CONTENT_METADATA,
   "Afficher/masquer les métadonnées"
   )
MSG_HASH(
   MSG_NO_THUMBNAIL_AVAILABLE,
   "Aucune miniature disponible"
   )
MSG_HASH(
   MSG_PRESS_AGAIN_TO_QUIT,
   "Appuyez à nouveau pour quitter..."
   )
MSG_HASH(
   MSG_TO,
   "vers"
   )
MSG_HASH(
   MSG_UNDID_LOAD_STATE,
   "Chargement de la sauvegarde instantanée annulé."
   )
MSG_HASH(
   MSG_UNDOING_SAVE_STATE,
   "Annulation de la sauvegarde instantanée"
   )
MSG_HASH(
   MSG_UNKNOWN,
   "Inconnu"
   )
MSG_HASH(
   MSG_UNPAUSED,
   "Réactivé."
   )
MSG_HASH(
   MSG_UNRECOGNIZED_COMMAND,
   "Commande \"%s\" reçue non reconnue.\n"
   )
MSG_HASH(
   MSG_USING_CORE_NAME_FOR_NEW_CONFIG,
   "Utilisation du nom du cœur pour la nouvelle configuration."
   )
MSG_HASH(
   MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED,
   "Utilisation du cœur libretro factice. Enregistrement ignoré."
   )
MSG_HASH(
   MSG_VALUE_CONNECT_DEVICE_FROM_A_VALID_PORT,
   "Connecter le périphérique depuis un port valide."
   )
MSG_HASH(
   MSG_VALUE_DISCONNECTING_DEVICE_FROM_PORT,
   "Déconnexion du périphérique depuis le port"
   )
MSG_HASH(
   MSG_VALUE_REBOOTING,
   "Redémarrage..."
   )
MSG_HASH(
   MSG_VALUE_SHUTTING_DOWN,
   "Arrêt en cours..."
   )
MSG_HASH(
   MSG_VERSION_OF_LIBRETRO_API,
   "Version de l'API libretro"
   )
MSG_HASH(
   MSG_VIEWPORT_SIZE_CALCULATION_FAILED,
   "Le calcul de la taille de la fenêtre d'affichage a échoué ! Continuera à utiliser les données brutes. Cela ne fonctionnera probablement pas correctement..."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_EJECT,
   "Impossible d'éjecter le plateau du lecteur de disque virtuel."
   )
MSG_HASH(
   MSG_VIRTUAL_DISK_TRAY_CLOSE,
   "Impossible de fermer le plateau du lecteur de disque virtuel."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_FAILED,
   "Le chargement d'une sauvegarde automatique depuis \"%s\" a échoué."
   )
MSG_HASH(
   MSG_AUTOLOADING_SAVESTATE_SUCCEEDED,
   "Chargement d'une sauvegarde automatique depuis \"%s\" avec succès."
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED,
   "non configuré"
   )
MSG_HASH(
   MSG_DEVICE_NOT_CONFIGURED_FALLBACK,
   "non configuré, utilisation de l'état de secours"
   )
MSG_HASH(
   MSG_BLUETOOTH_SCAN_COMPLETE,
   "Recherche Bluetooth terminée."
   )
MSG_HASH(
   MSG_BLUETOOTH_PAIRING_REMOVED,
   "Appareil oublié. Redémarrer RetroArch pour connecter/jumeler à nouveau."
   )
MSG_HASH(
   MSG_WIFI_SCAN_COMPLETE,
   "Recherche Wi-Fi terminé."
   )
MSG_HASH(
   MSG_SCANNING_BLUETOOTH_DEVICES,
   "Recherche de périphériques Bluetooth..."
   )
MSG_HASH(
   MSG_SCANNING_WIRELESS_NETWORKS,
   "Recherche de réseaux sans fil..."
   )
MSG_HASH(
   MSG_ENABLING_WIRELESS,
   "Activation du Wi-Fi..."
   )
MSG_HASH(
   MSG_DISABLING_WIRELESS,
   "Désactivation du Wi-Fi..."
   )
MSG_HASH(
   MSG_DISCONNECTING_WIRELESS,
   "Déconnexion du Wi-Fi..."
   )
MSG_HASH(
   MSG_NETPLAY_LAN_SCANNING,
   "Recherche d'hôtes de jeu en réseau..."
   )
MSG_HASH(
   MSG_PREPARING_FOR_CONTENT_SCAN,
   "Préparation à l'analyse de contenu..."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD,
   "Entrer le mot de passe"
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_OK,
   "Mot de passe correct."
   )
MSG_HASH(
   MSG_INPUT_ENABLE_SETTINGS_PASSWORD_NOK,
   "Mot de passe incorrect."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD,
   "Entrer le mot de passe"
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_OK,
   "Mot de passe correct."
   )
MSG_HASH(
   MSG_INPUT_KIOSK_MODE_PASSWORD_NOK,
   "Mot de passe incorrect."
   )
MSG_HASH(
   MSG_CONFIG_OVERRIDE_LOADED,
   "Remplacement de configuration chargé."
   )
MSG_HASH(
   MSG_GAME_REMAP_FILE_LOADED,
   "Fichier de remappage pour le jeu chargé."
   )
MSG_HASH(
   MSG_DIRECTORY_REMAP_FILE_LOADED,
   "Fichier de remappage pour le dossier du contenu chargé."
   )
MSG_HASH(
   MSG_CORE_REMAP_FILE_LOADED,
   "Fichier de remappage pour le cœur chargé."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED,
   "Exécution en avance activée. Images de latence supprimées : %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_ENABLED_WITH_SECOND_INSTANCE,
   "Exécution en avance activée avec une instance secondaire. Images de latence supprimées : %u."
   )
MSG_HASH(
   MSG_RUNAHEAD_DISABLED,
   "Exécution en avance désactivée."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_SAVESTATES,
   "L'exécution en avance a été désactivée car ce cœur ne prend pas en charge les sauvegardes instantanées."
   )
MSG_HASH(
   MSG_RUNAHEAD_CORE_DOES_NOT_SUPPORT_RUNAHEAD,
   "Exécution en avance indisponible car ce cœur ne prend pas en charge la sauvegarde instantanée déterministe."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_SAVE_STATE,
   "Échec de la sauvegarde instantanée. L'exécution en avance a été désactivée."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_LOAD_STATE,
   "Échec du chargement de la sauvegarde instantanée. L'exécution en avance a été désactivée."
   )
MSG_HASH(
   MSG_RUNAHEAD_FAILED_TO_CREATE_SECONDARY_INSTANCE,
   "Impossible de créer une deuxième instance. L'exécution en avance utilisera désormais une seule instance."
   )
MSG_HASH(
   MSG_SCANNING_OF_FILE_FINISHED,
   "Analyse du fichier terminée"
   )
MSG_HASH(
   MSG_CHEAT_INIT_SUCCESS,
   "Lancement de la recherche de cheats réussi"
   )
MSG_HASH(
   MSG_CHEAT_INIT_FAIL,
   "Impossible de lancer la recherche de cheats"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_NOT_INITIALIZED,
   "La recherche n'a pas été initialisée/démarrée"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_FOUND_MATCHES,
   "Nouveau nombre de correspondances = %u"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_SUCCESS,
   "Ajouté %u correspondances"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_FAIL,
   "Impossible d'ajouter les correspondances"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_SUCCESS,
   "Code créé à partir de la correspondance"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADD_MATCH_FAIL,
   "Création du code échouée"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_DELETE_MATCH_SUCCESS,
   "Correspondance supprimée"
   )
MSG_HASH(
   MSG_CHEAT_SEARCH_ADDED_MATCHES_TOO_MANY,
   "Pas assez de place. Le nombre maximum de cheats simultanés est de 100."
   )
MSG_HASH(
   MSG_CHEAT_ADD_TOP_SUCCESS,
   "Nouveau cheat ajouté en haut de la liste."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BOTTOM_SUCCESS,
   "Nouveau cheat ajouté en bas de la liste."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_ALL_SUCCESS,
   "Tous les cheats ont été supprimés."
   )
MSG_HASH(
   MSG_CHEAT_ADD_BEFORE_SUCCESS,
   "Nouveau cheat ajouté avant celui-ci."
   )
MSG_HASH(
   MSG_CHEAT_ADD_AFTER_SUCCESS,
   "Nouveau cheat ajouté après celui-ci."
   )
MSG_HASH(
   MSG_CHEAT_COPY_BEFORE_SUCCESS,
   "Cheat copié avant celui-ci."
   )
MSG_HASH(
   MSG_CHEAT_COPY_AFTER_SUCCESS,
   "Cheat copié après celui-ci."
   )
MSG_HASH(
   MSG_CHEAT_DELETE_SUCCESS,
   "Cheat supprimé."
   )
MSG_HASH(
   MSG_DEVICE_CONFIGURED_IN_PORT,
   "Configuré dans le port :"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_DISK,
   "Impossible de sélectionner le disque"
   )
MSG_HASH(
   MSG_FAILED_TO_SET_INITIAL_DISK,
   "Impossible de sélectionner le dernier disque utilisé..."
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_CLIENT,
   "Échec de la connexion au client"
   )
MSG_HASH(
   MSG_FAILED_TO_CONNECT_TO_HOST,
   "Échec de la connexion à l'hôte"
   )
MSG_HASH(
   MSG_NETPLAY_HOST_FULL,
   "Hôte de jeu en réseau plein"
   )
MSG_HASH(
   MSG_FAILED_TO_RECEIVE_HEADER_FROM_HOST,
   "Échec de l'obtention de l'entête depuis l'hôte"
   )
MSG_HASH(
   MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE,
   "Vous devez mettre en pause ou désactiver les succès en mode Hardcore pour charger des sauvegardes instantanées."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED,
   "Une sauvegarde instantanée a été chargée. Succès en mode Hardcore désactivés pour la session en cours."
   )
MSG_HASH(
   MSG_CHEEVOS_HARDCORE_MODE_DISABLED_CHEAT,
   "Un cheat a été activé. Succès en mode Hardcore désactivés pour la session en cours."
   )
MSG_HASH(
   MSG_CHEEVOS_MASTERED_GAME,
   "Maîtrisé %s"
   )
MSG_HASH(
   MSG_CHEEVOS_COMPLETED_GAME,
   "Terminé %s"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWEST,
   "La plus basse"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_LOWER,
   "Inférieure"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_NORMAL,
   "Normale"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHER,
   "Supérieure"
   )
MSG_HASH(
   MSG_RESAMPLER_QUALITY_HIGHEST,
   "La plus élevée"
   )
MSG_HASH(
   MSG_MISSING_ASSETS,
   "AVERTISSEMENT : Assets manquants, utilisez la mise à jour en ligne si disponible"
   )
MSG_HASH(
   MSG_RGUI_MISSING_FONTS,
   "Avertissement: Polices manquantes pour la langue sélectionnée, utilisez la Mise à jour en ligne si disponible"
   )
MSG_HASH(
   MSG_RGUI_INVALID_LANGUAGE,
   "Avertissement : Langue non prise en charge - utilisation de l'anglais"
   )
MSG_HASH(
   MSG_DUMPING_DISC,
   "Importation du disque..."
   )
MSG_HASH(
   MSG_DRIVE_NUMBER,
   "Lecteur %d"
   )
MSG_HASH(
   MSG_LOAD_CORE_FIRST,
   "Veuillez d'abord charger un cœur."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_READ_FROM_DRIVE,
   "Lecture depuis le lecteur échouée. Importation annulée."
   )
MSG_HASH(
   MSG_DISC_DUMP_FAILED_TO_WRITE_TO_DISK,
   "Écriture vers le disque échouée. Importation annulée."
   )
MSG_HASH(
   MSG_NO_DISC_INSERTED,
   "Aucun disque n'est inséré dans le lecteur."
   )
MSG_HASH(
   MSG_SHADER_PRESET_REMOVED_SUCCESSFULLY,
   "Préréglages de shaders supprimés avec succès."
   )
MSG_HASH(
   MSG_ERROR_REMOVING_SHADER_PRESET,
   "Erreur lors de la suppression des préréglages de shaders."
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_INVALID,
   "Fichier arcade DAT sélectionné invalide"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_TOO_LARGE,
   "Le fichier arcade DAT sélectionné est trop lourd (mémoire libre insuffisante)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_DAT_FILE_LOAD_ERROR,
   "Chargement du fichier arcade DAT échoué (format invalide ?)"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONFIG,
   "Configuration d'analyse manuelle invalide"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_INVALID_CONTENT,
   "Aucun contenu valide détecté"
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_START,
   "Analyse de contenu : "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_PLAYLIST_CLEANUP,
   "Vérification des entrées actuelles : "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_IN_PROGRESS,
   "Analyse : "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_M3U_CLEANUP,
   "Nettoyage des entrées M3U : "
   )
MSG_HASH(
   MSG_MANUAL_CONTENT_SCAN_END,
   "Analyse terminée : "
   )
MSG_HASH(
   MSG_CORE_BACKUP_SCANNING_CORE,
   "Analyse du cœur : "
   )
MSG_HASH(
   MSG_CORE_BACKUP_ALREADY_EXISTS,
   "La sauvegarde du cœur installé existe déjà : "
   )
MSG_HASH(
   MSG_BACKING_UP_CORE,
   "Sauvegarde du cœur : "
   )
MSG_HASH(
   MSG_PRUNING_CORE_BACKUP_HISTORY,
   "Suppression des sauvegardes obsolètes : "
   )
MSG_HASH(
   MSG_CORE_BACKUP_COMPLETE,
   "Sauvegarde du cœur terminée : "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_ALREADY_INSTALLED,
   "La sauvegarde du cœur sélectionné est déjà installée : "
   )
MSG_HASH(
   MSG_RESTORING_CORE,
   "Restauration du cœur : "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_COMPLETE,
   "Restauration du cœur terminée : "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_ALREADY_INSTALLED,
   "Le fichier de cœur sélectionné est déjà installé : "
   )
MSG_HASH(
   MSG_INSTALLING_CORE,
   "Installation du cœur : "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_COMPLETE,
   "Installation du cœur terminée : "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_INVALID_CONTENT,
   "Fichier de cœur sélectionné invalide : "
   )
MSG_HASH(
   MSG_CORE_BACKUP_FAILED,
   "Échec de la sauvegarde du cœur : "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_FAILED,
   "Échec de la restauration du cœur : "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_FAILED,
   "Échec de l'installation du cœur : "
   )
MSG_HASH(
   MSG_CORE_RESTORATION_DISABLED,
   "Restauration du cœur désactivée - cœur verrouillé : "
   )
MSG_HASH(
   MSG_CORE_INSTALLATION_DISABLED,
   "Installation du cœur désactivée - cœur verrouillé : "
   )
MSG_HASH(
   MSG_CORE_LOCK_FAILED,
   "Impossible de verrouiller les cœurs : "
   )
MSG_HASH(
   MSG_CORE_UNLOCK_FAILED,
   "Impossible de déverrouiller les cœurs : "
   )
MSG_HASH(
   MSG_CORE_DELETE_DISABLED,
   "Suppression du cœur désactivée - cœur verrouillé : "
   )
MSG_HASH(
   MSG_UNSUPPORTED_VIDEO_MODE,
   "Mode vidéo non pris en charge"
   )
MSG_HASH(
   MSG_CORE_INFO_CACHE_UNSUPPORTED,
   "Impossible d'écrire dans le dossier des informations de cœurs - la mise en cache des informations de cœurs sera désactivée"
   )
MSG_HASH(
   MSG_FOUND_ENTRY_STATE_IN,
   "Démarrage instantané trouvé dans"
   )
MSG_HASH(
   MSG_LOADING_ENTRY_STATE_FROM,
   "Chargement du démarrage instantané à partir de"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE,
   "Impossible de passer en mode jeu (GameMode)"
   )
MSG_HASH(
   MSG_FAILED_TO_ENTER_GAMEMODE_LINUX,
   "Impossible de passer en mode jeu (GameMode) - assurez-vous que le daemon GameMode est installé/en cours d'exécution"
   )

/* Lakka */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_UPDATE_LAKKA,
   "Mettre à jour Lakka"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_FRONTEND_NAME,
   "Nom de l'interface "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SYSTEM_INFO_LAKKA_VERSION,
   "Version de Lakka "
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_REBOOT,
   "Redémarrer"
   )

/* Environment Specific Settings */

MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SPLIT_JOYCON,
   "Joy-Con détachés"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_WIDGET_SCALE_FACTOR,
   "Remplacement de l'échelle des widgets graphiques "
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_WIDGET_SCALE_FACTOR,
   "Appliquer une valeur manuelle de remplacement du facteur d'échelle pour l'affichage des widgets à l'écran. Cette valeur est appliquée uniquement si l'option 'Mise à l'échelle automatique des widgets graphiques' est désactivée. Peut être utile pour augmenter ou réduire la taille des notifications améliorées, indicateurs et commandes indépendamment du menu lui-même."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
   "Résolution de l'écran"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DEFAULT,
   "Résolution de l'écran : Par défaut"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_NO_DESC,
   "Résolution de l'écran : %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_DESC,
   "Résolution de l'écran : %dx%d - %s"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DEFAULT,
   "Application de : Par défaut"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_NO_DESC,
   "Application de : %dx%d\nSTART pour réinitialiser"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_APPLYING_DESC,
   "Application de : %dx%d - %s\nSTART pour réinitialiser"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DEFAULT,
   "Réinitialisation à : Par défaut"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_NO_DESC,
   "Réinitialisation à : %dx%d"
   )
MSG_HASH(
   MSG_SCREEN_RESOLUTION_RESETTING_DESC,
   "Réinitialisation à : %dx%d - %s"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SCREEN_RESOLUTION,
   "Sélectionner le mode d'affichage."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SHUTDOWN,
   "Éteindre"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Autoriser l'accès aux fichiers externes"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS,
   "Ouvrir les réglages d'autorisations d'accès aux fichiers de Windows"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_FILE_BROWSER_OPEN_PICKER,
   "Ouvrir..."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_FILE_BROWSER_OPEN_PICKER,
   "Ouvrir un autre dossier à l'aide du sélecteur de fichiers système"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
   "Filtre anti-scintillement"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
   "Gamma vidéo"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
   "Filtre logiciel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_SETTINGS,
   "Rechercher des périphériques Bluetooth et les connecter."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_WIFI_SETTINGS,
   "Analyser les réseaux sans fil et établir la connexion."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_ENABLED,
   "Activer le Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORK_SCAN,
   "Se connecter au réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_NETWORKS,
   "Se connecter au réseau"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_WIFI_DISCONNECT,
   "Déconnecter"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER,
   "Élimination des scintillements"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH,
   "Définir la largeur d'écran de VI"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Correction du surbalayage (Haut)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_TOP,
   "Ajuste le surbalayage à l'écran en réduisant la taille de l'image par un nombre spécifique de lignes de balayage (enlevées du haut de l'écran). Peut introduire des artefacts d'agrandissement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Correction du surbalayage (Bas)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_OVERSCAN_CORRECTION_BOTTOM,
   "Ajuste le surbalayage à l'écran en réduisant la taille de l'image par un nombre spécifique de lignes de balayage (enlevées du bas de l'écran). Peut introduire des artefacts d'agrandissement."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SUSTAINED_PERFORMANCE_MODE,
   "Mode de performances soutenues"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERFPOWER,
   "Performance et puissance du processeur (CPU)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_ENTRY,
   "Politique de gestion du processeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE,
   "Mode de gouvernance"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Manuel"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANUAL,
   "Permet de modifier manuellement chaque détail dans chaque CPU : gouverneur, fréquences, etc. Recommandé uniquement pour les utilisateurs avancés."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Performance (géré)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PERF,
   "Mode par défaut et recommandé. Performances maximales lors du jeu en économisant l'énergie lors de la pause ou de la navigation dans les menus."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Gestion personnalisée"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MANAGED_PER_CONTEXT,
   "Permet de choisir quels gouverneurs utiliser dans les menus et pendant le jeu. Les modes Performances, Ondemand (à la demande) ou Schedutil (intelligent) sont recommandés pendant le jeu."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Performance maximale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MAX_PERF,
   "Toujours utiliser les performances maximales : les plus hautes fréquences pour une meilleure expérience."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Puissance minimale"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_MIN_POWER,
   "Utiliser la fréquence la plus basse disponible pour économiser de l'énergie. Utile sur les appareils alimentés par batterie, mais les performances seront considérablement réduites."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "Equilibré"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VALUE_CPU_PERF_MODE_BALANCED,
   "S'adapte à la charge de travail actuelle. Fonctionne bien avec la plupart des appareils et émulateurs et aide à économiser l'énergie. Des jeux et des cœurs exigeants pourraient subir une baisse des performances sur certains appareils."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MIN_FREQ,
   "Fréquence minimale"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MAX_FREQ,
   "Fréquence maximale"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MIN_FREQ,
   "Fréquence minimale pour le coeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_MANAGED_MAX_FREQ,
   "Fréquence maximale pour le coeur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_GOVERNOR,
   "Gouverneur performance"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_CORE_GOVERNOR,
   "Gouverneur du cœur"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CPU_POLICY_MENU_GOVERNOR,
   "Gouverneur du menu"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_GAMEMODE_ENABLE,
   "Mode jeu (GameMode)"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_GAMEMODE_ENABLE_LINUX,
   "Peut améliorer les performances, réduire la latence et corriger les problèmes de crépitements audio. Vous avez besoin de https://github.com/FeralInteractive/gamemode pour que cela fonctionne."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
   "Utiliser le mode PAL60"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_META_RESTART_KEY,
   "Redémarrer RetroArch"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_META_RESTART_KEY,
   "Quitter puis redémarrer RetroArch. Requis pour l'activation de certains paramètres du menu (lors d'un changement du pilote de menu, par exemple)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES,
   "Taille des blocs"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_PREFER_FRONT_TOUCH,
   "Préférer le tactile avant"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_ENABLE,
   "Tactile"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE,
   "Mappage clavier manette"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE,
   "Type de mappage clavier manette"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE,
   "Clavier minimal"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_BLOCK_TIMEOUT,
   "Délai pour bloquer l'assignation"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_BLOCK_TIMEOUT,
   "Temps d'attente en millisecondes pour obtenir un échantillon complet des touches pressées. Utilisez cette option si vous avez des problèmes avec les touches pressées simultanément (Android uniquement)."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_REBOOT,
   "Afficher 'Redémarrer'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_REBOOT,
   "Afficher l'option 'Redémarrer'."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_MENU_SHOW_SHUTDOWN,
   "Afficher 'Éteindre'"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_MENU_SHOW_SHUTDOWN,
   "Afficher l'option 'Éteindre'."
   )
MSG_HASH(
   MSG_ROOM_PASSWORDED,
   "Protégé par mot de passe"
   )
MSG_HASH(
   MSG_INTERNET_RELAY,
   "Internet (relais)"
   )
MSG_HASH(
   MSG_INTERNET_NOT_CONNECTABLE,
   "Internet (non connectable)"
   )
MSG_HASH(
   MSG_READ_WRITE,
   "Statut du stockage interne : lecture/écriture"
   )
MSG_HASH(
   MSG_READ_ONLY,
   "Statut du stockage interne : lecture seule"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_BRIGHTNESS_CONTROL,
   "Luminosité de l'écran"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_BRIGHTNESS_CONTROL,
   "Augmenter ou diminuer la luminosité de l'écran."
   )

#ifdef HAVE_LAKKA_SWITCH
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_GPU_PROFILE,
   "Overclocker le processeur graphique"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_GPU_PROFILE,
   "Overclocker ou underclocker le processeur graphique de la Switch."
   )
#endif
#if defined(HAVE_LAKKA_SWITCH) || defined(HAVE_LIBNX)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_SWITCH_CPU_PROFILE,
   "Overclocker le processeur"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SWITCH_CPU_PROFILE,
   "Overclocker le processeur de la Switch."
   )
#endif
#ifdef HAVE_LAKKA
MSG_HASH(
   MENU_ENUM_SUBLABEL_BLUETOOTH_ENABLE,
   "Déterminer l'état de Bluetooth."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SERVICES_SETTINGS,
   "Gérer les services au niveau du système d'exploitation."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SAMBA_ENABLE,
   "Partage des dossiers réseau via le protocole SMB."
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_SSH_ENABLE,
   "Utiliser SSH pour accéder à la ligne de commande à distance."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_LOCALAP_ENABLE,
   "Point d'accès Wi-Fi"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_LOCALAP_ENABLE,
   "Activer ou désactiver le point d'accès Wi-Fi."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_TIMEZONE,
   "Fuseau horaire"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_TIMEZONE,
   "Sélectionnez votre fuseau horaire pour ajuster la date et l'heure à votre emplacement."
   )
MSG_HASH(
   MSG_LOCALAP_SWITCHING_OFF,
   "Désactivation du point d'accès Wi-Fi."
   )
MSG_HASH(
   MSG_WIFI_DISCONNECT_FROM,
   "Déconnexion du Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_CONNECTING_TO,
   "Connexion au réseau Wi-Fi '%s'"
   )
MSG_HASH(
   MSG_WIFI_EMPTY_SSID,
   "[Pas de SSID]"
   )
MSG_HASH(
   MSG_LOCALAP_ALREADY_RUNNING,
   "Le point d'accès Wi-Fi est déja actif"
   )
MSG_HASH(
   MSG_LOCALAP_NOT_RUNNING,
   "Le point d'accès Wi-Fi n'est pas actif"
   )
MSG_HASH(
   MSG_LOCALAP_STARTING,
   "Démarrage du point d'accès Wi-Fi avec le SSID=%s et la Clé=%s"
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_CREATE,
   "Impossible de créer un fichier de configuration pour le point d'accès Wi-Fi."
   )
MSG_HASH(
   MSG_LOCALAP_ERROR_CONFIG_PARSE,
   "Mauvais fichier de configuration - impossible de trouver l'APNAME ou le PASSWORD dans %s"
   )
#endif
#ifdef GEKKO
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_MOUSE_SCALE,
   "Échelle de la souris"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_MOUSE_SCALE,
   "Ajuste l'échelle x/y pour la vitesse du pointeur Wiimote."
   )
#endif
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_INPUT_TOUCH_SCALE,
   "Échelle du tactile"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_INPUT_TOUCH_SCALE,
   "Ajuster l'échelle x/y des coordonnées de l'écran tactile pour s'adapter à la mise à l'échelle de l'écran au niveau du système d'exploitation."
   )
#ifdef HAVE_ODROIDGO2
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_RGA_SCALING,
   "Mise à l'échelle RGA"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_RGA_SCALING,
   "Mise à l'échelle RGA et filtrage bicubique. Peut affecter les widgets."
   )
#else
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_CTX_SCALING,
   "Mise à l'échelle spécifique au contexte"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_CTX_SCALING,
   "Mise à l'échelle selon le contexte matériel (si disponible)."
   )
#endif
#if defined(_3DS)
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_LCD_BOTTOM,
   "Écran inférieur 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_LCD_BOTTOM,
   "Permet l'affichage d'informations d'état sur l'écran inférieur. Désactiver pour augmenter la durée de vie de la batterie et améliorer les performances."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_VIDEO_3DS_DISPLAY_MODE,
   "Mode d'affichage 3DS"
   )
MSG_HASH(
   MENU_ENUM_SUBLABEL_VIDEO_3DS_DISPLAY_MODE,
   "Sélectionne le mode d'affichage entre les modes 2D et 3D. En mode '3D', les pixels sont carrés et un effet de profondeur est appliqué lors de l'affichage du menu rapide. Le mode '2D' offre la meilleure performance."
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_400X240,
   "2D (Effet grille de pixels)"
   )
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_CTR_VIDEO_MODE_2D_800X240,
   "2D (Haute résolution)"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_DEFAULT,
   "Touchez l'écran tactile\npour ouvrir le menu de\nRetroarch."
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_ASSET_NOT_FOUND,
   "bottom_menu.png introuvable\ndans le dossier assets/ctr"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_DATA,
   "Aucunes\ndonnées"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL,
   "Pas de\ncaptures d'écran"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_RESUME,
   "Continuer\nla partie"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_SAVE_STATE,
   "Établir le\npoint de\nrécupération"
   )
MSG_HASH(
   MSG_3DS_BOTTOM_MENU_LOAD_STATE,
   "Reprendre au\npoint de\nrécupération"
   )
#endif
#ifdef HAVE_QT
MSG_HASH(
   MENU_ENUM_LABEL_VALUE_QT_SCAN_FINISHED,
   "Scan terminé.<br><br>\nPour que le contenu soit correctement analysé, vous devez :\n<ul><li>avoir un cœur compatible déjà téléchargé</li>\n<li>avoir les \"Fichiers d'information de cœurs\" à jour via la mise à jour en ligne</li>\n<li>avoir les \"Bases de données\" à jour via la mise à jour en ligne</li>\n<li>redémarrer RetroArch si l'une des opérations ci-dessus vient d'être effectuée</li></ul>\nEnfin, le contenu doit correspondre aux bases de données existantes <a href=\"https://docs.libretro.com/guides/roms-playlists-thumbnails/#sources\">ici</a>. Si cela ne fonctionne toujours pas, veuillez envisager de <a href=\"https://www.github.com/libretro/RetroArch/issues\">soumettre un rapport d'erreur</a>."
   )
#endif

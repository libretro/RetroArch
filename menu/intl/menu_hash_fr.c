/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *
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

#include <stdint.h>
#include <string.h>

#include "../menu_hash.h"

 /* IMPORTANT:
  * For non-english characters to work without proper unicode support,
  * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
  * If you save this file as UTF-8, you'll break non-english characters
  * (e.g. German "Umlauts" and Portugese diacritics).
 */
extern const char encoding_test[sizeof("àéÉèêô")==6+1 ? 1 : -1];

const char *menu_hash_to_str_fr(uint32_t hash)
{
   switch (hash)
   {
      case MENU_LABEL_VALUE_INFORMATION_LIST:
         return "Informations";
      case MENU_LABEL_VALUE_USE_BUILTIN_PLAYER:
         return "Utiliser le lecteur embarqué";
      case MENU_LABEL_VALUE_CONTENT_SETTINGS:
         return "Menu rapide";
      case MENU_LABEL_VALUE_RDB_ENTRY_CRC32:
         return "CRC32";
      case MENU_LABEL_VALUE_RDB_ENTRY_MD5:
         return "MD5";
      case MENU_LABEL_VALUE_LOAD_CONTENT_LIST:
         return "Charger un contenu";
      case MENU_VALUE_LOAD_ARCHIVE:
         return "Charger l'archive";
      case MENU_VALUE_OPEN_ARCHIVE:
         return "Ouvrir l'archive";
      case MENU_VALUE_ASK_ARCHIVE:
         return "Demander";
      case MENU_LABEL_VALUE_PRIVACY_SETTINGS:
         return "Réglages de la vie privée";
      case MENU_VALUE_HORIZONTAL_MENU:
         return "Horizontal Menu";
      case MENU_LABEL_VALUE_NO_SETTINGS_FOUND:
         return "Pas de réglages trouvés.";
      case MENU_LABEL_VALUE_NO_PERFORMANCE_COUNTERS:
         return "Pas de compteurs de performance.";
      case MENU_LABEL_VALUE_DRIVER_SETTINGS:
         return "Réglages des pilotes";
      case MENU_LABEL_VALUE_CONFIGURATION_SETTINGS:
         return "Réglages des configurations";
      case MENU_LABEL_VALUE_CORE_SETTINGS:
         return "Réglages des cores";
      case MENU_LABEL_VALUE_VIDEO_SETTINGS:
         return "Réglages vidéo";
      case MENU_LABEL_VALUE_LOGGING_SETTINGS:
         return "Réglages des logs";
      case MENU_LABEL_VALUE_SAVING_SETTINGS:
         return "Réglages des sauvegardes";
      case MENU_LABEL_VALUE_REWIND_SETTINGS:
         return "Réglages du rembobinage";
      case MENU_LABEL_VALUE_CUSTOM_VIEWPORT_1:
         return "Set Upper-Left Corner";
      case MENU_LABEL_CUSTOM_VIEWPORT_2:
         return "custom_viewport_2";
      case MENU_LABEL_VALUE_CUSTOM_VIEWPORT_2:
         return "Set Bottom-Right Corner";
      case MENU_VALUE_SHADER:
         return "Shader";
      case MENU_VALUE_CHEAT:
         return "Triche";
      case MENU_VALUE_USER:
         return "User";
      case MENU_LABEL_VALUE_SYSTEM_BGM_ENABLE:
         return "System BGM Enable";
      case MENU_VALUE_RETROPAD:
         return "RetroPad";
      case MENU_VALUE_RETROKEYBOARD:
         return "RetroKeyboard";
      case MENU_LABEL_AUDIO_BLOCK_FRAMES:
         return "audio_block_frames";
      case MENU_LABEL_VALUE_AUDIO_BLOCK_FRAMES:
         return "Block Frames";
      case MENU_LABEL_VALUE_INPUT_BIND_MODE:
         return "Bind Mode";
      case MENU_LABEL_AUTOCONFIG_DESCRIPTOR_LABEL_SHOW:
         return "autoconfig_descriptor_label_show";
      case MENU_LABEL_VALUE_AUTOCONFIG_DESCRIPTOR_LABEL_SHOW:
         return "Display Autoconfig Descriptor Labels";
      case MENU_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "input_descriptor_label_show";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW:
         return "Display Core Input Descriptor Labels";
      case MENU_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "input_descriptor_hide_unbound";
      case MENU_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND:
         return "Hide Unbound Core Input Descriptors";
      case MENU_LABEL_VALUE_VIDEO_FONT_ENABLE:
         return "Afficher les messages d'info";
      case MENU_LABEL_VALUE_VIDEO_FONT_PATH:
         return "Police des messages d'info";
      case MENU_LABEL_VALUE_VIDEO_FONT_SIZE:
         return "Taille du texte des messages";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_X:
         return "Position sur X";
      case MENU_LABEL_VALUE_VIDEO_MESSAGE_POS_Y:
         return "Position sur Y";
      case MENU_LABEL_VALUE_VIDEO_SOFT_FILTER:
         return "Soft Filter Enable";
      case MENU_LABEL_VIDEO_FILTER_FLICKER:
         return "video_filter_flicker";
      case MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER:
         return "Flicker filter";
      case MENU_VALUE_DIRECTORY_CONTENT:
         return "<Dossier contenus>";
      case MENU_VALUE_UNKNOWN:
         return "Inconnu";
      case MENU_VALUE_DONT_CARE:
         return "Don't care";
      case MENU_VALUE_LINEAR:
         return "Linear";
      case MENU_VALUE_NEAREST:
         return "Nearest";
      case MENU_VALUE_DIRECTORY_DEFAULT:
         return "<Par defaut>";
      case MENU_VALUE_DIRECTORY_NONE:
         return "<Aucun>";
      case MENU_VALUE_NOT_AVAILABLE:
         return "Indisponible";
      case MENU_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY:
         return "Dossier de remaps d'entrées";
      case MENU_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR:
         return "Dossier des autoconfigs d'entrées";
      case MENU_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY:
         return "Dossier des réglages de capture vidéo";
      case MENU_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY:
         return "Dossier d'enregistrement des vidéos";
      case MENU_LABEL_VALUE_SCREENSHOT_DIRECTORY:
         return "Dossier des captures d'écran";
      case MENU_LABEL_VALUE_PLAYLIST_DIRECTORY:
         return "Dossier des playlists";
      case MENU_LABEL_VALUE_SAVEFILE_DIRECTORY:
         return "Dossier des sauvegardes";
      case MENU_LABEL_VALUE_SAVESTATE_DIRECTORY:
         return "Dossier des sauvegardes rapides";
      case MENU_LABEL_VALUE_STDIN_CMD_ENABLE:
         return "Commandes stdin";
      case MENU_LABEL_VALUE_VIDEO_DRIVER:
         return "Pilote vidéo";
      case MENU_LABEL_VALUE_RECORD_ENABLE:
         return "Autoriser les captures vidéo";
      case MENU_LABEL_VALUE_VIDEO_GPU_RECORD:
         return "Captures vidéo via le GPU";
      case MENU_LABEL_VALUE_RECORD_PATH:
         return "Chemin de l'enregistrement";
      case MENU_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY:
         return "Utiliser le dossier d'enregistrement";
      case MENU_LABEL_VALUE_RECORD_CONFIG:
         return "Configuration de capture";
      case MENU_LABEL_VALUE_VIDEO_POST_FILTER_RECORD:
         return "Post filter record Enable";
      case MENU_LABEL_VALUE_CORE_ASSETS_DIRECTORY:
         return "Dossier des assets de core";
      case MENU_LABEL_VALUE_ASSETS_DIRECTORY:
         return "Dossier des assets";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY:
         return "Dossier des fonds d'écran dynamiques";
      case MENU_LABEL_VALUE_BOXARTS_DIRECTORY:
         return "Dossier des vignettes";
      case MENU_LABEL_VALUE_RGUI_BROWSER_DIRECTORY:
         return "Dossier racine de navigation";
      case MENU_LABEL_VALUE_RGUI_CONFIG_DIRECTORY:
         return "Dossier des fichiers de configuration";
      case MENU_LABEL_VALUE_LIBRETRO_INFO_PATH:
         return "Dossier des core info";
      case MENU_LABEL_VALUE_LIBRETRO_DIR_PATH:
         return "Dossier des cores";
      case MENU_LABEL_VALUE_CURSOR_DIRECTORY:
         return "Dossier des curseurs";
      case MENU_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY:
         return "Dossier des bases de données de contenus";
      case MENU_LABEL_VALUE_SYSTEM_DIRECTORY:
         return "Dossier système";
      case MENU_LABEL_VALUE_CHEAT_DATABASE_PATH:
         return "Dossier des fichiers de triche";
      case MENU_LABEL_VALUE_EXTRACTION_DIRECTORY:
         return "Dossier d'extraction";
      case MENU_LABEL_VALUE_AUDIO_FILTER_DIR:
         return "Dossier des filtres audio";
      case MENU_LABEL_VALUE_VIDEO_SHADER_DIR:
         return "Dossier des shaders vidéo";
      case MENU_LABEL_VALUE_VIDEO_FILTER_DIR:
         return "Dossier des filtres vidéo";
      case MENU_LABEL_VALUE_OVERLAY_DIRECTORY:
         return "Dossier des overlays";
      case MENU_LABEL_VALUE_OSK_OVERLAY_DIRECTORY:
         return "Dossier des overlays claviers";
      case MENU_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT:
         return "Swap Netplay Input";
      case MENU_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE:
         return "Netplay Spectator Enable";
      case MENU_LABEL_VALUE_NETPLAY_IP_ADDRESS:
         return "Adresse IP";
      case MENU_LABEL_VALUE_NETPLAY_TCP_UDP_PORT:
         return "Netplay TCP/UDP Port";
      case MENU_LABEL_VALUE_NETPLAY_ENABLE:
         return "Autoriser le jeu en réseau";
      case MENU_LABEL_VALUE_NETPLAY_DELAY_FRAMES:
         return "Netplay Delay Frames";
      case MENU_LABEL_VALUE_NETPLAY_MODE:
         return "Netplay Client Enable";
      case MENU_LABEL_VALUE_RGUI_SHOW_START_SCREEN:
         return "Afficher l'écran de d'aide";
      case MENU_LABEL_VALUE_TITLE_COLOR:
         return "Couleur du titre du menu";
      case MENU_LABEL_VALUE_ENTRY_HOVER_COLOR:
         return "Couleur de l'entrée active";
      case MENU_LABEL_VALUE_TIMEDATE_ENABLE:
         return "Afficher la date et l'heure";
      case MENU_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE:
         return "Threaded data runloop";
      case MENU_LABEL_VALUE_ENTRY_NORMAL_COLOR:
         return "Couleur des entrées du menu";
      case MENU_LABEL_VALUE_SHOW_ADVANCED_SETTINGS:
         return "Afficher les réglages avancés";
      case MENU_LABEL_VALUE_COLLAPSE_SUBGROUPS_ENABLE:
         return "Fusionner les sous-groupes";
      case MENU_LABEL_VALUE_MOUSE_ENABLE:
         return "Support de la souris";
      case MENU_LABEL_VALUE_POINTER_ENABLE:
         return "Support du tactile";
      case MENU_LABEL_VALUE_CORE_ENABLE:
         return "Afficher le core actuel";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_ENABLE:
         return "Surcharger le DPI";
      case MENU_LABEL_VALUE_DPI_OVERRIDE_VALUE:
         return "DPI personnalisé";
      case MENU_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE:
         return "Désactiver l'éconimiseur d'écran";
      case MENU_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION:
         return "Désactiver le compositeur du bureau";
      case MENU_LABEL_VALUE_PAUSE_NONACTIVE:
         return "Don't run in background";
      case MENU_LABEL_VALUE_UI_COMPANION_START_ON_BOOT:
         return "UI Companion Start On Boot";
      case MENU_LABEL_VALUE_UI_MENUBAR_ENABLE:
         return "Menubar (Hint)";
      case MENU_LABEL_VALUE_ARCHIVE_MODE:
         return "Mode d'ouverture des archives";
      case MENU_LABEL_VALUE_NETWORK_CMD_ENABLE:
         return "Commandes réseau";
      case MENU_LABEL_VALUE_NETWORK_CMD_PORT:
         return "Port des commandes réseau";
      case MENU_LABEL_VALUE_HISTORY_LIST_ENABLE:
         return "Afficher l'historique";
      case MENU_LABEL_VALUE_CONTENT_HISTORY_SIZE:
         return "Taille de l'historique";
      case MENU_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO:
         return "Fréquence estimée de l'écran";
      case MENU_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN:
         return "Dummy On Core Shutdown";
      case MENU_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE:
         return "Ne pas démarrer un core automatiquement";
      case MENU_LABEL_VALUE_FRAME_THROTTLE_ENABLE:
         return "Limiter la vitesse d'exécution";
      case MENU_LABEL_VALUE_FASTFORWARD_RATIO:
         return "Vitesse d'avance rapide";
      case MENU_LABEL_VALUE_AUTO_REMAPS_ENABLE:
         return "Charger les fichiers remaps automatiquement";
      case MENU_LABEL_VALUE_SLOWMOTION_RATIO:
         return "Slow-Motion Ratio";
      case MENU_LABEL_VALUE_CORE_SPECIFIC_CONFIG:
         return "Configuration par-core";
      case MENU_LABEL_VALUE_AUTO_OVERRIDES_ENABLE:
         return "Load Override Files Automatically";
      case MENU_LABEL_VALUE_CONFIG_SAVE_ON_EXIT:
         return "Sauver la config en quittant";
      case MENU_LABEL_VALUE_VIDEO_SMOOTH:
         return "Filtre bilineaire (HW)";
      case MENU_LABEL_VALUE_VIDEO_GAMMA:
         return "Gamma";
      case MENU_LABEL_VALUE_VIDEO_ALLOW_ROTATE:
         return "Autoriser la rotation";
      case MENU_LABEL_VALUE_VIDEO_HARD_SYNC:
         return "Synchroniser le GPU au CPU";
      case MENU_LABEL_VALUE_VIDEO_SWAP_INTERVAL:
         return "Intervale de synchro verticale";
      case MENU_LABEL_VALUE_VIDEO_VSYNC:
         return "Synchronisation verticale";
      case MENU_LABEL_VALUE_VIDEO_THREADED:
         return "Threader l'affichage";
      case MENU_LABEL_VALUE_VIDEO_ROTATION:
         return "Rotation";
      case MENU_LABEL_VALUE_VIDEO_CROP_OVERSCAN:
         return "Tronquer l'overscan (Reload)";
      case MENU_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION:
         return "Inserer des images noires";
      case MENU_LABEL_VALUE_SORT_SAVEFILES_ENABLE:
         return "Classer les sauvegardes par dossier";
      case MENU_LABEL_VALUE_SORT_SAVESTATES_ENABLE:
         return "Classer les sauvegardes d'état par dossier";
      case MENU_LABEL_VALUE_VIDEO_FULLSCREEN:
         return "Plein écran";
      case MENU_LABEL_VALUE_VIDEO_SCALE:
         return "Zoom (en fenêtre)";
      case MENU_LABEL_VALUE_VIDEO_SCALE_INTEGER:
         return "Aligner aux pixels de l'écran";
      case MENU_LABEL_VALUE_PERFCNT_ENABLE:
         return "Compteurs de performance";
      case MENU_LABEL_VALUE_LIBRETRO_LOG_LEVEL:
         return "Niveau de log des cores";
      case MENU_LABEL_VALUE_LOG_VERBOSITY:
         return "Logs verbeux";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_LOAD:
         return "Charger automatiquement l'état";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_INDEX:
         return "Indice automatique de sauvegarde d'etat";
      case MENU_LABEL_VALUE_SAVESTATE_AUTO_SAVE:
         return "Sauvegarde automatique";
      case MENU_LABEL_VALUE_AUTOSAVE_INTERVAL:
         return "Intervale de sauvegarde SaveRAM";
      case MENU_LABEL_VALUE_BLOCK_SRAM_OVERWRITE:
         return "Ne pas écraser la SaveRAM en chargeant l'état";
      case MENU_LABEL_VALUE_VIDEO_SHARED_CONTEXT:
         return "Partager le contexte matériel";
      case MENU_LABEL_VALUE_RESTART_RETROARCH:
         return "Redémarrer RetroArch";
      case MENU_LABEL_VALUE_NETPLAY_NICKNAME:
         return "Nom d'utilisateur";
      case MENU_LABEL_VALUE_USER_LANGUAGE:
         return "Langue";
      case MENU_LABEL_VALUE_CAMERA_ALLOW:
         return "Autoriser la caméra";
      case MENU_LABEL_VALUE_LOCATION_ALLOW:
         return "Autoriser la localisation";
      case MENU_LABEL_VALUE_PAUSE_LIBRETRO:
         return "Pause quand le menu est activé";
      case MENU_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE:
         return "Afficher l'overlay clavier";
      case MENU_LABEL_VALUE_INPUT_OVERLAY_ENABLE:
         return "Activer les overlays";
      case MENU_LABEL_VALUE_VIDEO_MONITOR_INDEX:
         return "Écran";
      case MENU_LABEL_VALUE_VIDEO_FRAME_DELAY:
         return "Delai d'image";
      case MENU_LABEL_VALUE_INPUT_DUTY_CYCLE:
         return "Rapport de cycle";
      case MENU_LABEL_VALUE_INPUT_TURBO_PERIOD:
         return "Delai du turbo";
      case MENU_LABEL_VALUE_INPUT_AXIS_THRESHOLD:
         return "Seuil des axes";
      case MENU_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE:
         return "Autoriser le remapping des entrées";
      case MENU_LABEL_VALUE_INPUT_MAX_USERS:
         return "Nombre d'utilisateurs";
      case MENU_LABEL_VALUE_INPUT_AUTODETECT_ENABLE:
         return "Activer l'autoconfiguration";
      case MENU_LABEL_VALUE_AUDIO_OUTPUT_RATE:
         return "Fréquence de sortie (KHz)";
      case MENU_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW:
         return "Limite max de l'ajustement";
      case MENU_LABEL_VALUE_CHEAT_NUM_PASSES:
         return "Nombre de passages";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_CORE:
         return "Charger un fichier remap de core";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_GAME:
         return "Charger un fichier remap de jeu";
      case MENU_LABEL_VALUE_CHEAT_APPLY_CHANGES:
         return "Appliquer les changements";
      case MENU_LABEL_VALUE_SHADER_APPLY_CHANGES:
         return "Appliquer les changements";
      case MENU_LABEL_VALUE_REWIND_ENABLE:
         return "Activer le rembobinage";
      case MENU_LABEL_VALUE_CONTENT_COLLECTION_LIST:
         return "Via les collections";
      case MENU_LABEL_VALUE_DETECT_CORE_LIST:
         return "Via les fichiers + détecter le core";
      case MENU_LABEL_VALUE_LOAD_CONTENT_HISTORY:
         return "Via l'historique";
      case MENU_LABEL_VALUE_AUDIO_ENABLE:
         return "Activer le son";
      case MENU_LABEL_VALUE_FPS_SHOW:
         return "Afficher le FPS";
      case MENU_LABEL_VALUE_AUDIO_MUTE:
         return "Muet";
      case MENU_LABEL_VALUE_AUDIO_VOLUME:
         return "Volume sonnore (dB)";
      case MENU_LABEL_VALUE_AUDIO_SYNC:
         return "Synchroniser le son";
      case MENU_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA:
         return "Delta du taux de contrôle";
      case MENU_LABEL_VALUE_VIDEO_SHADER_NUM_PASSES:
         return "Nombre de passages";
      case MENU_LABEL_VALUE_CONFIGURATIONS:
         return "Charger un fichier de config";
      case MENU_LABEL_VALUE_REWIND_GRANULARITY:
         return "Précision du rembobinage";
      case MENU_LABEL_VALUE_REMAP_FILE_LOAD:
         return "Charger un fichier de remap";
      case MENU_LABEL_VALUE_REMAP_FILE_SAVE_AS:
         return "Enregistrer un fichier de remap sous";
      case MENU_LABEL_VALUE_CUSTOM_RATIO:
         return "Forcer une résolution";
      case MENU_LABEL_VALUE_USE_THIS_DIRECTORY:
         return "<Choisir ce dossier>";
      case MENU_LABEL_VALUE_RDB_ENTRY_START_CONTENT:
         return "Lancer le contenu";
      case MENU_LABEL_VALUE_DISK_OPTIONS:
         return "Options de disques";
      case MENU_LABEL_VALUE_CORE_OPTIONS:
         return "Options du core";
      case MENU_LABEL_VALUE_CORE_CHEAT_OPTIONS:
         return "Options de triche";
      case MENU_LABEL_VALUE_TAKE_SCREENSHOT:
         return "Capturer l écran";
      case MENU_LABEL_VALUE_RESUME:
         return "Reprendre";
      case MENU_LABEL_VALUE_DISK_INDEX:
         return "Numéro du disque";
      case MENU_LABEL_VALUE_FRONTEND_COUNTERS:
         return "Compteurs du Frontend";
      case MENU_LABEL_VALUE_DISK_IMAGE_APPEND:
         return "Ajouter une image de disque";
      case MENU_LABEL_VALUE_DISK_CYCLE_TRAY_STATUS:
         return "État du lecteur de disque";
      case MENU_LABEL_VALUE_NO_PLAYLIST_ENTRIES_AVAILABLE:
         return "Playlist vide.";
      case MENU_LABEL_VALUE_NO_CORE_INFORMATION_AVAILABLE:
         return "Pad d'informations disponibles.";
      case MENU_LABEL_VALUE_NO_CORE_OPTIONS_AVAILABLE:
         return "Pas d'options disponibles.";
      case MENU_LABEL_VALUE_NO_CORES_AVAILABLE:
         return "Aucun core disponible.";
      case MENU_VALUE_NO_CORE:
         return "Aucun core";
      case MENU_LABEL_VALUE_DATABASE_MANAGER:
         return "Base de données";
      case MENU_LABEL_VALUE_CURSOR_MANAGER:
         return "Curseurs";
      case MENU_LABEL_VALUE_RECORDING_SETTINGS:
         return "Réglages de capture video";
      case MENU_VALUE_MAIN_MENU:
         return "Main Menu";
      case MENU_LABEL_VALUE_SETTINGS:
         return "Réglages du frontend"; /* FIXME */
      case MENU_LABEL_VALUE_QUIT_RETROARCH:
         return "Quitter RetroArch";
      case MENU_LABEL_VALUE_HELP:
         return "Aide";
      case MENU_LABEL_VALUE_SAVE_NEW_CONFIG:
         return "Sauvegarder la configuration";
      case MENU_LABEL_VALUE_RESTART_CONTENT:
         return "Redémarrer le contenu";
      case MENU_LABEL_VALUE_CORE_UPDATER_LIST:
         return "Mise à jour des cores";
      case MENU_LABEL_VALUE_SYSTEM_INFORMATION:
         return "Informations du système";
      case MENU_LABEL_VALUE_ONLINE_UPDATER:
         return "Mises à jour";
      case MENU_LABEL_VALUE_CORE_INFORMATION:
         return "Informations sur le core";
      case MENU_LABEL_VALUE_DIRECTORY_NOT_FOUND:
         return "Dossier non trouvé.";
      case MENU_LABEL_VALUE_NO_ITEMS:
         return "Vide.";
      case MENU_LABEL_VALUE_CORE_LIST:
         return "Charger un core";
      case MENU_LABEL_VALUE_LOAD_CONTENT:
         return "Via les fichiers";
      case MENU_LABEL_VALUE_CLOSE_CONTENT:
         return "Quitter le core";
      case MENU_LABEL_VALUE_MANAGEMENT:
         return "Gestion avancée";
      case MENU_LABEL_VALUE_SAVE_STATE:
         return "Sauvegarder un état";
      case MENU_LABEL_VALUE_LOAD_STATE:
         return "Charger un etat";
      case MENU_LABEL_VALUE_RESUME_CONTENT:
         return "Reprendre";
      case MENU_LABEL_VALUE_INPUT_DRIVER:
         return "Pilote des entrées";
      case MENU_LABEL_VALUE_AUDIO_DRIVER:
         return "Pilote audio";
      case MENU_LABEL_VALUE_JOYPAD_DRIVER:
         return "Pilote des manettes";
      case MENU_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER:
         return "Pilote de ré-échantillonage audio";
      case MENU_LABEL_VALUE_RECORD_DRIVER:
         return "Pilote de capture vidéo";
      case MENU_LABEL_VALUE_MENU_DRIVER:
         return "Pilote de menu";
      case MENU_LABEL_VALUE_CAMERA_DRIVER:
         return "Pilote de caméra";
      case MENU_LABEL_VALUE_LOCATION_DRIVER:
         return "Pilote de localisation";
      case MENU_LABEL_VALUE_UNABLE_TO_READ_COMPRESSED_FILE:
         return "Impossible de lire l'archive.";
      case MENU_LABEL_VALUE_OVERLAY_SCALE:
         return "Zoom de l'Overlay";
      case MENU_LABEL_VALUE_OVERLAY_PRESET:
         return "Préréglages d'Overlay";
      case MENU_LABEL_VALUE_AUDIO_LATENCY:
         return "Latence audio (ms)";
      case MENU_LABEL_VALUE_AUDIO_DEVICE:
         return "Carte son";
      case MENU_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET:
         return "Prereglages d'overlay clavier";
      case MENU_LABEL_VALUE_OVERLAY_OPACITY:
         return "Transparence de l'overlay";
      case MENU_LABEL_VALUE_MENU_WALLPAPER:
         return "Fond d'écran";
      case MENU_LABEL_VALUE_DYNAMIC_WALLPAPER:
         return "Fond d'écran dynamique";
      case MENU_LABEL_VALUE_BOXART:
         return "Afficher les vignettes";
      case MENU_LABEL_VALUE_CORE_INPUT_REMAPPING_OPTIONS:
         return "Options de remap d'entrées du core";
      case MENU_LABEL_VALUE_SHADER_OPTIONS:
         return "Options de shaders";
      case MENU_LABEL_VALUE_NO_SHADER_PARAMETERS:
         return "Aucun paramètres.";
      case MENU_LABEL_VALUE_VIDEO_FILTER:
         return "Filtre vidéo";
      case MENU_LABEL_VALUE_AUDIO_DSP_PLUGIN:
         return "Module DSP";
      case MENU_LABEL_VALUE_STARTING_DOWNLOAD:
         return "Téléchargement de : ";
      case MENU_VALUE_OFF:
         return "OFF";
      case MENU_VALUE_ON:
         return "ON";
      default:
         break;
   }

   return "null";
}

int menu_hash_get_help_fr(uint32_t hash, char *s, size_t len)
{
   switch (hash)
   {
      default:
         return -1;
   }

   return 0;
}

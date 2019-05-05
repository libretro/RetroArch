/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
 *  Copyright (C) 2016-2019 - Brad Parker
 *  Copyright (C) 2019 - Weedy Weed Smoker
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

int menu_hash_get_help_fr_enum(enum msg_hash_enums msg, char *s, size_t len)
{
    settings_t *settings = config_get_ptr();

    if (msg == MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM)
    {
       snprintf(s, len,
             "TODO/FIXME - Entrez le message ici."
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
                   "Bascule entre l'avance rapide et \n"
                   "la vitesse normale."
                   );
             break;
          case RARCH_FAST_FORWARD_HOLD_KEY:
             snprintf(s, len,
                   "Maintenir pour l'avance rapide. \n"
                   " \n"
                   "Relâcher la touche désactive l'avance rapide."
                   );
             break;
          case RARCH_SLOWMOTION_KEY:
             snprintf(s, len,
                   "Active/désactive le ralenti.");
             break;
          case RARCH_SLOWMOTION_HOLD_KEY:
             snprintf(s, len,
                   "Maintenir pour le ralenti.");
             break;
          case RARCH_PAUSE_TOGGLE:
             snprintf(s, len,
                   "Activer/désactiver la mise en pause.");
             break;
          case RARCH_FRAMEADVANCE:
             snprintf(s, len,
                   "Avance image par image lorsque le contenu est en pause.");
             break;
          case RARCH_SHADER_NEXT:
             snprintf(s, len,
                   "Applique le prochain shader dans le dossier.");
             break;
          case RARCH_SHADER_PREV:
             snprintf(s, len,
                   "Applique le shader précédent dans le dossier.");
             break;
          case RARCH_CHEAT_INDEX_PLUS:
          case RARCH_CHEAT_INDEX_MINUS:
          case RARCH_CHEAT_TOGGLE:
             snprintf(s, len,
                   "Cheats.");
             break;
          case RARCH_RESET:
             snprintf(s, len,
                   "Réinitialiser le contenu.");
             break;
          case RARCH_SCREENSHOT:
             snprintf(s, len,
                   "Prendre une capture d'écran.");
             break;
          case RARCH_MUTE:
             snprintf(s, len,
                   "Désactiver/réactiver le son.");
             break;
          case RARCH_OSK:
             snprintf(s, len,
                   "Afficher/masquer le clavier à l'écran.");
             break;
          case RARCH_FPS_TOGGLE:
             snprintf(s, len,
                   "Afficher/masquer le compteur d'images/s.");
             break;
          case RARCH_SEND_DEBUG_INFO:
             snprintf(s, len,
                   "Envoie des informations de diagnostic sur votre appareil et la configuration de RetroArch à nos serveurs pour analyse.");
             break;
          case RARCH_NETPLAY_HOST_TOGGLE:
             snprintf(s, len,
                   "Activer/désactiver l'hébergement du jeu en réseau.");
             break;
          case RARCH_NETPLAY_GAME_WATCH:
             snprintf(s, len,
                   "Bascule entre le mode jeu/spectateur du jeu en réseau.");
             break;
          case RARCH_ENABLE_HOTKEY:
             snprintf(s, len,
                   "Activer d'autres touches de raccourci. \n"
                   " \n"
                   "Si ce raccourci est assigné à soit au clavier, \n"
                   "à une touche ou à un axe de manette, toutes\n"
                   "les autres touches de raccourci ne seront \n"
                   "activées que si celle-ci est maintenue en même temps. \n"
                   " \n"
                   "Alternativement, toutes les touches de raccourci \n"
                   "du clavier pourrait être désactivé par l'utilisateur.");
             break;
          case RARCH_VOLUME_UP:
             snprintf(s, len,
                   "Augmente le volume audio.");
             break;
          case RARCH_VOLUME_DOWN:
             snprintf(s, len,
                   "Diminue le volume audio.");
             break;
          case RARCH_OVERLAY_NEXT:
             snprintf(s, len,
                   "Passe à la surimpression suivante. Retourne au début.");
             break;
          case RARCH_DISK_EJECT_TOGGLE:
             snprintf(s, len,
                   "Bascule l'éjection pour les disques. \n"
                   " \n"
                   "Utilisé pour le contenu multi-disque. ");
             break;
          case RARCH_DISK_NEXT:
          case RARCH_DISK_PREV:
             snprintf(s, len,
                   "Parcourt les images de disque. Utiliser après l'éjection. \n"
                   " \n"
                   "Terminez en basculant l'éjection.");
             break;
          case RARCH_GRAB_MOUSE_TOGGLE:
             snprintf(s, len,
                   "Capture/relâche la souris. \n"
                   " \n"
                   "When mouse is grabbed, RetroArch hides the \n"
                   "mouse, and keeps the mouse pointer inside \n"
                   "the window to allow relative mouse input to \n"
                   "work better.");
             break;
          case RARCH_GAME_FOCUS_TOGGLE:
             snprintf(s, len,
                   "Jeu au premier-plan/en arrière-plan.\n"
                   " \n"
                   "When a game has focus, RetroArch will both disable \n"
                   "hotkeys and keep/warp the mouse pointer inside the window.");
             break;
          case RARCH_MENU_TOGGLE:
             snprintf(s, len, "Affiche/masque le menu.");
             break;
          case RARCH_LOAD_STATE_KEY:
             snprintf(s, len,
                   "Charge une sauvegarde instantanée.");
             break;
          case RARCH_FULLSCREEN_TOGGLE_KEY:
             snprintf(s, len,
                   "Active/désactive le mode plein écran.");
             break;
          case RARCH_QUIT_KEY:
             snprintf(s, len,
                   "Touche pour quitter RetroArch proprement. \n"
                   " \n"
                   "Killing it in any hard way (SIGKILL, etc.) will \n"
                   "terminate RetroArch without saving RAM, etc."
#ifdef __unix__
                   "\nOn Unix-likes, SIGINT/SIGTERM allows a clean \n"
                   "deinitialization."
#endif
                   "");
             break;
          case RARCH_STATE_SLOT_PLUS:
          case RARCH_STATE_SLOT_MINUS:
             snprintf(s, len,
                   "Emplacements de sauvegardes instantanées. \n"
                   " \n"
                   "With slot set to 0, save state name is \n"
                   "*.state (or whatever defined on commandline). \n"
                   " \n"
                   "When slot is not 0, path will be <path><d>, \n"
                   "where <d> is slot number.");
             break;
          case RARCH_SAVE_STATE_KEY:
             snprintf(s, len,
                   "Effectue une sauvegarde instantanée.");
             break;
          case RARCH_REWIND:
             snprintf(s, len,
                   "Maintenez la touche pour rembobiner. \n"
                   " \n"
                   "Le rembobinage doit être activé.");
             break;
          case RARCH_BSV_RECORD_TOGGLE:
             snprintf(s, len,
                   "Active/désactive l'enregistrement.");
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
            snprintf(s, len, "Informations de connexion pour \n"
                    "votre compte RetroSuccès (RetroAchievements). \n"
                    " \n"
                    "Visit retroachievements.org and sign up \n"
                    "for a free account. \n"
                    " \n"
                    "After you are done registering, you need \n"
                    "to input the username and password into \n"
                    "RetroArch.");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_USERNAME:
            snprintf(s, len, "Nom d'utilisateur de votre compte RetroSuccès (RetroAchievements).");
            break;
        case MENU_ENUM_LABEL_CHEEVOS_PASSWORD:
            snprintf(s, len, "Mot de passe de votre compte RetroSuccès (RetroAchievements).");
            break;
        case MENU_ENUM_LABEL_USER_LANGUAGE:
            snprintf(s, len, "Localise le menu et tous les messages \n"
                    "à l'écran en fonction de la langue sélectionnée \n"
                    "ici. \n"
                    " \n"
                    "Requires a restart for the changes \n"
                    "to take effect. \n"
                    " \n"
                    "REMARQUE : not all languages might be currently \n"
                    "implemented. \n"
                    " \n"
                    "In case a language is not implemented, \n"
                    "we fallback to English.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_PATH:
            snprintf(s, len, "Change la police utilisée \n"
                    "pour le texte à l'écran.");
            break;
        case MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS:
            snprintf(s, len, "Charger automatiquement les options de cœur spécifiques au contenu.");
            break;
        case MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE:
            snprintf(s, len, "Charger automatiquement les configurations de remplacement.");
            break;
        case MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE:
            snprintf(s, len, "Charger automatiquement les fichiers de remappage d'entrées.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE:
            snprintf(s, len, "Classe les sauvegardes instantanées dans des dossiers \n"
                    "nommés d'après le cœur libretro utilisé.");
            break;
        case MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE:
            snprintf(s, len, "Classe les sauvegardes dans des dossiers \n"
                    "nommés d'après le cœur libretro utilisé.");
            break;
        case MENU_ENUM_LABEL_RESUME_CONTENT:
            snprintf(s, len, "Quitte le menu et retourne \n"
                    "au contenu.");
            break;
        case MENU_ENUM_LABEL_RESTART_CONTENT:
            snprintf(s, len, "Redémarre le contenu depuis le début.");
            break;
        case MENU_ENUM_LABEL_CLOSE_CONTENT:
            snprintf(s, len, "Ferme le contenu et le décharge de \n"
                    "la mémoire.");
            break;
        case MENU_ENUM_LABEL_UNDO_LOAD_STATE:
            snprintf(s, len, "Si une sauvegarde instantanée a été chargée, \n"
                    "le contenu reviendra à l'état avant le chargement.");
            break;
        case MENU_ENUM_LABEL_UNDO_SAVE_STATE:
            snprintf(s, len, "Si une sauvegarde instantanée a été écrasée, \n"
                    "elle sera rétablie à l'état de sauvegarde précédent.");
            break;
        case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
            snprintf(s, len, "Crée une capture d'écran. \n"
                    " \n"
                    "The screenshot will be stored inside the \n"
                    "Screenshot Directory.");
            break;
        case MENU_ENUM_LABEL_ADD_TO_FAVORITES:
            snprintf(s, len, "Ajoute l'entrée à vos favoris.");
            break;
        case MENU_ENUM_LABEL_RUN:
            snprintf(s, len, "Démarre le contenu.");
            break;
        case MENU_ENUM_LABEL_INFORMATION:
            snprintf(s, len, "Affiche des informations de métadonnées supplémentaires \n"
                    "pour le contenu.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CONFIG:
            snprintf(s, len, "Fichier de configuration.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_COMPRESSED_ARCHIVE:
            snprintf(s, len, "Archive compressée.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RECORD_CONFIG:
            snprintf(s, len, "Fichier de configuration d'enregistrement.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CURSOR:
            snprintf(s, len, "Fichier de pointeur de base de données.");
            break;
        case MENU_ENUM_LABEL_FILE_CONFIG:
            snprintf(s, len, "Fichier de configuration.");
            break;
        case MENU_ENUM_LABEL_SCAN_THIS_DIRECTORY:
            snprintf(s, len,
                     "Sélectionner cette option pour analyser le dossier actuel \n"
                             "pour y rechercher du contenu.");
            break;
        case MENU_ENUM_LABEL_USE_THIS_DIRECTORY:
            snprintf(s, len,
                     "Sélectionner ce dossier pour l'utiliser'.");
            break;
        case MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY:
            snprintf(s, len,
                     "Dossier de base de données de contenu. \n"
                             " \n"
                             "Path to content database \n"
                             "directory.");
            break;
        case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
            snprintf(s, len,
                     "Dossier des miniatures. \n"
                             " \n"
                             "To store thumbnail files.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
            snprintf(s, len,
                     "Dossier des informations de cœurs. \n"
                             " \n"
                             "A directory for where to search \n"
                             "for libretro core information.");
            break;
        case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            snprintf(s, len,
                     "Dossier des listes de lecture. \n"
                             " \n"
                             "Save all playlist files to this \n"
                             "directory.");
            break;
        case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
            snprintf(s, len,
                     "Certains cœurs peuvent avoir \n"
                             "une fonctionnalité d'extinction. \n"
                             " \n"
                             "If this option is left disabled, \n"
                             "selecting the shutdown procedure \n"
                             "would trigger RetroArch being shut \n"
                             "down. \n"
                             " \n"
                             "Enabling this option will load a \n"
                             "dummy core instead so that we remain \n"
                             "inside the menu and RetroArch won't \n"
                             "shutdown.");
            break;
        case MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE:
            snprintf(s, len,
                     "Certains cœurs peuvent nécessiter des fichiers \n"
                             "de firmware ou de BIOS. \n"
                             " \n"
                             "If this option is disabled, \n"
                             "it will try to load even if such \n"
                             "firmware is missing. \n");
            break;
        case MENU_ENUM_LABEL_PARENT_DIRECTORY:
            snprintf(s, len,
                     "Revenir au dossier parent.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_UWP_PERMISSIONS:
            snprintf(s, len,
                     "Ouvre les paramètres de permissions de Windows \n"
                     "pour activer la fonctionnalité broadFileSystemAccess.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OPEN_PICKER:
           snprintf(s, len,
                     "Ouvre le sélecteur de fichiers du système \n"
                     "pour accéder à des dossiers supplémentaires.");
           break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER_PRESET:
            snprintf(s, len,
                     "Fichier de préréglages de shader.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_SHADER:
            snprintf(s, len,
                     "Fichier de shader.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_REMAP:
            snprintf(s, len,
                     "Fichier de remappage des touches.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CHEAT:
            snprintf(s, len,
                     "Fichier de cheats.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_OVERLAY:
            snprintf(s, len,
                     "Fichier de surimpression.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_RDB:
            snprintf(s, len,
                     "Fichier de base de donnée.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_FONT:
            snprintf(s, len,
                     "Fichier de police TrueType.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_PLAIN_FILE:
            snprintf(s, len,
                     "Fichier simple.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MOVIE_OPEN:
            snprintf(s, len,
                     "Vidéo. \n"
                             " \n"
                             "Select it to open this file with the \n"
                             "video player.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
            snprintf(s, len,
                     "Musique. \n"
                             " \n"
                             "Select it to open this file with the \n"
                             "music player.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
            snprintf(s, len,
                     "Fichier d'image.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
            snprintf(s, len,
                     "Image. \n"
                             " \n"
                             "Select it to open this file with the \n"
                             "image viewer.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
            snprintf(s, len,
                     "Cœur libretro. \n"
                             " \n"
                             "Selecting this will associate this core \n"
                             "to the game.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            snprintf(s, len,
                     "Cœur libretro. \n"
                             " \n"
                             "Select this file to have RetroArch load this core.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Dossier. \n"
                             " \n"
                             "Select it to open this directory.");
            break;
        case MENU_ENUM_LABEL_CACHE_DIRECTORY:
            snprintf(s, len,
                     "Dossier de cache. \n"
                             " \n"
                             "Content decompressed by RetroArch will be \n"
                             "temporarily extracted to this directory.");
            break;
        case MENU_ENUM_LABEL_HISTORY_LIST_ENABLE:
            snprintf(s, len,
                     "Si activé, le contenu chargé dans RetroArch \n"
                             "sera automatiquement ajouté \n"
                             "à la liste de l'historique récent.");
            break;
        case MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Dossier du navigateur de fichiers. \n"
                             " \n"
                             "Sets start directory for menu file browser.");
            break;
        case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            snprintf(s, len,
                     "Influence la façon dont la détection \n"
                             "des touches pressées est effectuée dans RetroArch. \n"
                             " \n"
                             "Early  - Input polling is performed before \n"
                             "the frame is processed. \n"
                             "Normal - Input polling is performed when \n"
                             "polling is requested. \n"
                             "Late   - Input polling is performed on \n"
                             "first input state request per frame.\n"
                             " \n"
                             "Setting it to 'Early' or 'Late' can result \n"
                             "in less latency, \n"
                             "depending on your configuration.\n\n"
                             "Will be ignored when using netplay."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
            snprintf(s, len,
                     "Masquer les descripteurs d'appellation des touches \n"
                             "non définis par le cœur.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
            snprintf(s, len,
                     "fréquence de rafraîchissement de votre écran. \n"
                             "Utilisée pour calculer un débit audio approprié.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE:
            snprintf(s, len,
                     "Force la désactivation de la prise en charge du mode sRGB FBO. \n"
                             "Certains pilotes OpenGL d'Intel sous Windows rencontrent \n"
                             "des problèmes vidéo avec le mode sRGB FBO lorsqu'il est activé.");
            break;
        case MENU_ENUM_LABEL_AUDIO_ENABLE:
            snprintf(s, len,
                     "Active la sortie audio.");
            break;
        case MENU_ENUM_LABEL_AUDIO_SYNC:
            snprintf(s, len,
                     "Synchronise l'audio (recommandé).");
            break;
        case MENU_ENUM_LABEL_AUDIO_LATENCY:
            snprintf(s, len,
                     "Latence audio souhaitée en millisecondes. \n"
                             "Might not be honored if the audio driver \n"
                             "can't provide given latency.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
            snprintf(s, len,
                     "Autorise les cœurs à définir la rotation. If false, \n"
                             "rotation requests are honored, but ignored.\n\n"
                             "Used for setups where one manually rotates \n"
                             "the monitor.");
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW:
            snprintf(s, len,
                     "Affiche les descripteurs d'appellation des touches \n"
                             "définis par le cœur plutôt que ceux par défaut.");
            break;
        case MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE:
            snprintf(s, len,
                     "Nombre d'entrées qui seront conservées dans \n"
                             "la liste de lecture d'historique du contenu.");
            break;
        case MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN:
            snprintf(s, len,
                     "Utiliser le mode fenêtré ou non en mode \n"
                             "plein écran.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_SIZE:
            snprintf(s, len,
                     "Taille de police pour les messages à l'écran.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX:
            snprintf(s, len,
                     "Incrémente automatiquement l'emplacement de chaque sauvegarde, \n"
                             "générant plusieurs fichiers de sauvegarde instantanée. \n"
                             "When the content is loaded, state slot will be \n"
                             "set to the highest existing value (last savestate).");
            break;
        case MENU_ENUM_LABEL_FPS_SHOW:
            snprintf(s, len,
                     "Permet d'afficher les images par seconde \n"
                             "actuelles.");
            break;
        case MENU_ENUM_LABEL_MEMORY_SHOW:
            snprintf(s, len,
                     "Inclut l'affichage de l'utilisation actuelle/du total \n"
                             "de la mémoire utilisée avec les images par seconde/images.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FONT_ENABLE:
            snprintf(s, len,
                     "Afficher et/ou masquer les messages à l'écran.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X:
        case MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y:
            snprintf(s, len,
                     "Offset for where messages will be placed \n"
                             "onscreen. Values are in range [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
            snprintf(s, len,
                     "Enable or disable the current overlay.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
            snprintf(s, len,
                     "Hide the current overlay from appearing \n"
                             "inside the menu.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS:
            snprintf(s, len,
                      "Show keyboard/controller button presses on \n"
                            "the onscreen overlay.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT:
            snprintf(s, len,
                      "Select the port to listen for controller input \n"
                            "to display on the onscreen overlay.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_PRESET:
            snprintf(s, len,
                     "Emplacement de la surimpression.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_OPACITY:
            snprintf(s, len,
                     "Opacité de la surimpression.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
            snprintf(s, len,
                     "Input bind timer timeout (in seconds). \n"
                             "Amount of seconds to wait until proceeding \n"
                             "to the next bind.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_HOLD:
            snprintf(s, len,
               "Input bind hold time (in seconds). \n"
               "Amount of seconds to hold an input to bind it.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_SCALE:
            snprintf(s, len,
                     "Échelle de la surimpression.");
            break;
        case MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE:
            snprintf(s, len,
                     "Fréquence de sortie audio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT:
            snprintf(s, len,
                     "Set to true if hardware-rendered cores \n"
                             "should get their private context. \n"
                             "Avoids having to assume hardware state changes \n"
                             "inbetween frames."
            );
            break;
        case MENU_ENUM_LABEL_CORE_LIST:
            snprintf(s, len,
                     "Charger un cœur. \n"
                             " \n"
                             "Browse for a libretro core \n"
                             "implementation. Where the browser \n"
                             "starts depends on your Core Directory \n"
                             "path. If blank, it will start in root. \n"
                             " \n"
                             "If Core Directory is a directory, the menu \n"
                             "will use that as top folder. If Core \n"
                             "Directory is a full path, it will start \n"
                             "in the folder where the file is.");
            break;
        case MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG:
            snprintf(s, len,
                     "Vous pouvez utiliser les contrôles suivants ci-dessous \n"
                             "sur votre manette de jeu ou votre clavier\n"
                             "pour contrôler le menu: \n"
                             " \n"
            );
            break;
        case MENU_ENUM_LABEL_WELCOME_TO_RETROARCH:
            snprintf(s, len,
                     "Bienvenue dans RetroArch\n"
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC: {
            /* Work around C89 limitations */
            char u[501];
            const char *t =
                    "RetroArch repose sur une forme unique\n"
                            "de synchronisation audio/vidéo dans laquelle il doit\n"
                            "être calibré par rapport à la fréquence de rafraîchissement\n"
                            "de votre affichage pour des performances optimales.\n"
                            " \n"
                            "Si vous rencontrez des craquements dans le son ou\n"
                            "une déchirure vidéo, cela signifie généralement que\n"
                            "vous devez calibrer les paramètres. Quelques choix ci-dessous :\n"
                            " \n";
            snprintf(u, sizeof(u), /* can't inline this due to the printf arguments */
                     "a) Allez dans '%s' -> '%s', et activez\n"
                             "'Vidéo sur plusieurs fils d'exécution'. La fréquence\n"
                             "de rafraîchissement importera peu dans ce mode,\n"
                             "les images/s seront plus élevées, mais la vidéo\n"
                             "sera peut-être moins fluide.\n"
                             "b) Allez dans '%s' -> '%s', et regardez la\n"
                             "'%s'. Laissez-la tourner pendant\n"
                             "2048 images, puis pressez 'Confirmer'.",
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
                     "Pour analyser du contenu, allez dans '%s' et\n"
                             "sélectionnez soit '%s', soit '%s'.\n"
                             "\n"
                             "Les fichiers seront comparés aux entrées de la\n"
                             "base de données. S'il y a une correspondance, cela\n"
                             "ajoutera une entrée à une liste de lecture.\n"
                             "\n"
                             "Vous pouvez alors accéder facilement à ce contenu\n"
                             "en allant dans '%s' -> '%s'\n"
                             "au lieu de devoir passer par le navigateur de fichiers\n"
                             "à chaque fois.\n"
                             "\n"
                             "REMERQUE : le contenu de certains cœurs peut toujours\n"
                             "ne pas être analysable.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_DIRECTORY),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCAN_FILE),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT:
            snprintf(s, len,
                     "Bienvenue dans RetroArch\n"
                             "\n"
                             "Extraction des assets, veuillez patienter.\n"
                             "Cela peut prendre un certain temps...\n"
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.input_driver : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_UDEV)))
                     snprintf(s, len,
                           "Pilote d'entrées udev. \n"
                           " \n"
                           "It uses the recent evdev joypad API \n"
                           "for joystick support. It supports \n"
                           "hotplugging and force feedback. \n"
                           " \n"
                           "The driver reads evdev events for keyboard \n"
                           "support. It also supports keyboard callback, \n"
                           "mice and touchpads. \n"
                           " \n"
                           "By default in most distros, /dev/input nodes \n"
                           "are root-only (mode 600). You can set up a udev \n"
                           "rule which makes these accessible to non-root."
                           );
               else if (string_is_equal(lbl,
                        msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
                     snprintf(s, len,
                           "Pilote d'entrées linuxraw. \n"
                           " \n"
                           "This driver requires an active TTY. Keyboard \n"
                           "events are read directly from the TTY which \n"
                           "makes it simpler, but not as flexible as udev. \n" "Mice, etc, are not supported at all. \n"
                           " \n"
                           "This driver uses the older joystick API \n"
                           "(/dev/input/js*).");
               else
                     snprintf(s, len,
                           "Pilote d'entrées.\n"
                           " \n"
                           "Depending on video driver, it might \n"
                           "force a different input driver.");
            }
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_LIST:
            snprintf(s, len,
                     "Charger du contenu. \n"
                             "Parcourir le contenu. \n"
                             " \n"
                             "Pour charger du contenu, vous avez besoin \n"
                             "d'un 'Cœur' et d'un fichier de contenu. \n"
                             " \n"
                             "Pour choisir où le menu démarre \n"
                             "le navigateur de contenu, définissez  \n"
                             "le dossier 'Navigateur de fichiers'. \n"
                             "Si non défini, il démarrera à la racine. \n"
                             " \n"
                             "Le navigateur filtrera les extensions \n"
                             "pour le dernier cœur chargé avec l'option \n"
                             "'Charger un cœur', et utilisera ce cœur \n"
                             "lorsque du contenu sera chargé."
            );
            break;
        case MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY:
            snprintf(s, len,
                     "Charger du contenu depuis l'historique. \n"
                             " \n"
                             "As content is loaded, content and libretro \n"
                             "core combinations are saved to history. \n"
                             " \n"
                             "The history is saved to a file in the same \n"
                             "directory as the RetroArch config file. If \n"
                             "no config file was loaded in startup, history \n"
                             "will not be saved or loaded, and will not exist \n"
                             "in the main menu."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_DRIVER:
            snprintf(s, len,
                     "Pilote vidéo actuel.");

            if (string_is_equal(settings->arrays.video_driver, "gl"))
            {
                snprintf(s, len,
                         "Pilote vidéo OpenGL. \n"
                                 " \n"
                                 "This driver allows libretro GL cores to  \n"
                                 "be used in addition to software-rendered \n"
                                 "core implementations.\n"
                                 " \n"
                                 "Performance for software-rendered and \n"
                                 "libretro GL core implementations is \n"
                                 "dependent on your graphics card's \n"
                                 "underlying GL driver).");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
            {
                snprintf(s, len,
                         "Pilote vidéo SDL 2.\n"
                                 " \n"
                                 "This is an SDL 2 software-rendered video \n"
                                 "driver.\n"
                                 " \n"
                                 "Performance for software-rendered libretro \n"
                                 "core implementations is dependent \n"
                                 "on your platform SDL implementation.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
            {
                snprintf(s, len,
                         "Pilote vidéo SDL.\n"
                                 " \n"
                                 "This is an SDL 1.2 software-rendered video \n"
                                 "driver.\n"
                                 " \n"
                                 "Performance is considered to be suboptimal. \n"
                                 "Consider using it only as a last resort.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "d3d"))
            {
                snprintf(s, len,
                         "Pilote vidéo Direct3D. \n"
                                 " \n"
                                 "Performance for software-rendered cores \n"
                                 "is dependent on your graphic card's \n"
                                 "underlying D3D driver).");
            }
            else if (string_is_equal(settings->arrays.video_driver, "exynos"))
            {
                snprintf(s, len,
                         "Pilote vidéo Exynos-G2D. \n"
                                 " \n"
                                 "This is a low-level Exynos video driver. \n"
                                 "Uses the G2D block in Samsung Exynos SoC \n"
                                 "for blit operations. \n"
                                 " \n"
                                 "Performance for software rendered cores \n"
                                 "should be optimal.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "drm"))
            {
                snprintf(s, len,
                         "Pilote vidéo DRM simple. \n"
                                 " \n"
                                 "This is a low-level video driver using. \n"
                                 "libdrm for hardware scaling using \n"
                                 "GPU overlays.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
            {
                snprintf(s, len,
                         "Pilote vidéo Sunxi-G2D. \n"
                                 " \n"
                                 "This is a low-level Sunxi video driver. \n"
                                 "Uses the G2D block in Allwinner SoCs.");
            }
            break;
        case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            snprintf(s, len,
                     "Module audio DSP.\n"
                             "Processes audio before it's sent to \n"
                             "the driver."
            );
            break;
        case MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER:
            {
               const char *lbl = settings ? settings->arrays.audio_resampler : NULL;

               if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_SINC)))
                  strlcpy(s,
                        "Implémentation SINC fenêtrée.", len);
               else if (string_is_equal(lbl, msg_hash_to_str(
                           MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER_CC)))
                  strlcpy(s,
                        "Implémentation de cosinus compliqués.", len);
               else if (string_is_empty(s))
                  strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NO_INFORMATION_AVAILABLE), len);
            }
            break;

		case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION: snprintf(s, len, "CRT défini");
			break;

		case MENU_ENUM_LABEL_CRT_SWITCH_RESOLUTION_SUPER: snprintf(s, len, "CRT SUPER défini");
			break;

        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET:
            snprintf(s, len,
                     "Charger les préréglages de shaders. \n"
                             " \n"
                             "Load a shader preset directly. \n"
                             "The menu shader menu is updated accordingly. \n"
                             " \n"
                             "If the CGP uses scaling methods which are not \n"
                             "simple, (i.e. source scaling, same scaling \n"
                             "factor for X/Y), the scaling factor displayed \n"
                             "in the menu might not be correct."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
            snprintf(s, len,
                     "Echelle pour ce passage. \n"
                             " \n"
                             "The scale factor accumulates, i.e. 2x \n"
                             "for first pass and 2x for second pass \n"
                             "will give you a 4x total scale. \n"
                             " \n"
                             "If there is a scale factor for last \n"
                             "pass, the result is stretched to \n"
                             "screen with the filter specified in \n"
                             "'Default Filter'. \n"
                             " \n"
                             "If 'Don't Care' is set, either 1x \n"
                             "scale or stretch to fullscreen will \n"
                             "be used depending if it's not the last \n"
                             "pass or not."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
            snprintf(s, len,
                     "Passages de shaders. \n"
                             " \n"
                             "RetroArch allows you to mix and match various \n"
                             "shaders with arbitrary shader passes, with \n"
                             "custom hardware filters and scale factors. \n"
                             " \n"
                             "This option specifies the number of shader \n"
                             "passes to use. If you set this to 0, and use \n"
                             "Apply Shader Changes, you use a 'blank' shader. \n"
                             " \n"
                             "The Default Filter option will affect the \n"
                             "stretching filter.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            snprintf(s, len,
                     "Paramètres de shaders. \n"
                             " \n"
                             "Modifies current shader directly. Will not be \n"
                             "saved to CGP/GLSLP preset file.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            snprintf(s, len,
                     "Paramètres de préréglages de shaders. \n"
                             " \n"
                             "Modifies shader preset currently in menu."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            snprintf(s, len,
                     "Emplacement du shader. \n"
                             " \n"
                             "All shaders must be of the same \n"
                             "type (i.e. CG, GLSL or HLSL). \n"
                             " \n"
                             "Set Shader Directory to set where \n"
                             "the browser starts to look for \n"
                             "shaders."
            );
            break;
        case MENU_ENUM_LABEL_CONFIGURATION_SETTINGS:
            snprintf(s, len,
                     "Détermine comment les fichiers de configuration \n"
                             "sont chargés et priorisés.");
            break;
        case MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT:
            snprintf(s, len,
                     "Enregistre la configuration sur le disque à la sortie.\n"
                             "Useful for menu as settings can be\n"
                             "modified. Overwrites the config.\n"
                             " \n"
                             "#include's and comments are not \n"
                             "preserved. \n"
                             " \n"
                             "By design, the config file is \n"
                             "considered immutable as it is \n"
                             "likely maintained by the user, \n"
                             "and should not be overwritten \n"
                             "behind the user's back."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
            "\nThis is not not the case on \n"
            "consoles however, where \n"
            "looking at the config file \n"
            "manually isn't really an option."
#endif
            );
            break;
        case MENU_ENUM_LABEL_CONFIRM_ON_EXIT:
            snprintf(s, len, "Êtes-vous sûr de vouloir quitter ?");
            break;
        case MENU_ENUM_LABEL_SHOW_HIDDEN_FILES:
            snprintf(s, len, "Afficher les fichiers\n"
                    "et dossiers cachés.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
            snprintf(s, len,
                     "Filtre matériel pour ce passage. \n"
                             " \n"
                             "If 'Don't Care' is set, 'Default \n"
                             "Filter' will be used."
            );
            break;
        case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
            snprintf(s, len,
                     "Sauvegarde automatiquement la mémoire SRAM \n"
                             "non volatile à intervalles réguliers.\n"
                             " \n"
                             "This is disabled by default unless set \n"
                             "otherwise. The interval is measured in \n"
                             "seconds. \n"
                             " \n"
                             "A value of 0 disables autosave.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
            snprintf(s, len,
                     "Type de périphérique d'entrée. \n"
                             " \n"
                             "Picks which device type to use. This is \n"
                             "relevant for the libretro core itself."
            );
            break;
        case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            snprintf(s, len,
                     "Définit le niveau de journalisation pour les cœurs libretro \n"
                             "(GET_LOG_INTERFACE). \n"
                             " \n"
                             " If a log level issued by a libretro \n"
                             " core is below libretro_log level, it \n"
                             " is ignored.\n"
                             " \n"
                             " DEBUG logs are always ignored unless \n"
                             " verbose mode is activated (--verbose).\n"
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
                     "Emplacements de sauvegardes instantanées.\n"
                             " \n"
                             " With slot set to 0, save state name is *.state \n"
                             " (or whatever defined on commandline).\n"
                             "When slot is != 0, path will be (path)(d), \n"
                             "where (d) is slot number.");
            break;
        case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            snprintf(s, len,
                     "Applique les changements de shaders. \n"
                             " \n"
                             "After changing shader settings, use this to \n"
                             "apply changes. \n"
                             " \n"
                             "Changing shader settings is a somewhat \n"
                             "expensive operation so it has to be \n"
                             "done explicitly. \n"
                             " \n"
                             "When you apply shaders, the menu shader \n"
                             "settings are saved to a temporary file (either \n"
                             "menu.cgp or menu.glslp) and loaded. The file \n"
                             "persists after RetroArch exits. The file is \n"
                             "saved to Shader Directory."
            );
            break;
        case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
            snprintf(s, len,
                     "Verifie les changements dans les fichiers de shaders. \n"
                     " \n"
                     "After saving changes to a shader on disk, \n"
                     "it will automatically be recompiled \n"
                     "and applied to the running content."
            );
            break;
        case MENU_ENUM_LABEL_MENU_TOGGLE:
            snprintf(s, len,
                     "Affiche/masque le menu.");
            break;
        case MENU_ENUM_LABEL_GRAB_MOUSE_TOGGLE:
            snprintf(s, len,
                     "Capture/relâche la souris.\n"
                             " \n"
                             "When mouse is grabbed, RetroArch hides the \n"
                             "mouse, and keeps the mouse pointer inside \n"
                             "the window to allow relative mouse input to \n"
                             "work better.");
            break;
        case MENU_ENUM_LABEL_GAME_FOCUS_TOGGLE:
            snprintf(s, len,
                     "Jeu au premier plan/en arrière-plan.\n"
                             " \n"
                             "When a game has focus, RetroArch will both disable \n"
                             "hotkeys and keep/warp the mouse pointer inside the window.");
            break;
        case MENU_ENUM_LABEL_DISK_NEXT:
            snprintf(s, len,
                     "Fait défiler les images disques. Utiliser après \n"
                             "l'éjection. \n"
                             " \n"
                             "Terminez par 'Éjecter/insérer un disque'.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FILTER:
#ifdef HAVE_FILTERS_BUILTIN
            snprintf(s, len,
                  "Filtre vidéo basé sur le processeur.");
#else
            snprintf(s, len,
                     "Filtre vidéo basé sur le processeur.\n"
                             " \n"
                             "Emplacement d'une bibliothèque dynamique.");
#endif
            break;
        case MENU_ENUM_LABEL_AUDIO_DEVICE:
            snprintf(s, len,
                     "Remplace le périphérique audio utilisé par défaut \n"
                             "par le pilote audio.\n"
                             "Cette option dépend du pilote. Par exemple\n"
#ifdef HAVE_ALSA
            " \n"
            "ALSA requiert un périphérique PCM."
#endif
#ifdef HAVE_OSS
            " \n"
            "OSS requiert un emplacement (/dev/dsp par exemple)."
#endif
#ifdef HAVE_JACK
            " \n"
            "JACK requiert des noms de ports (system:playback1\n"
            ",system:playback_2 par exemple)."
#endif
#ifdef HAVE_RSOUND
            " \n"
            "RSound requiert une adresse IP vers un serveur \n"
            "RSound."
#endif
            );
            break;
        case MENU_ENUM_LABEL_DISK_EJECT_TOGGLE:
            snprintf(s, len,
                     "Éjecte/insére un disque.\n"
                             " \n"
                             "Utilisé pour le contenu multi-disque.");
            break;
        case MENU_ENUM_LABEL_ENABLE_HOTKEY:
            snprintf(s, len,
                     "Active d'autres touches de raccourcis.\n"
                             " \n"
                             " If this hotkey is bound to either keyboard, \n"
                             "joybutton or joyaxis, all other hotkeys will \n"
                             "be disabled unless this hotkey is also held \n"
                             "at the same time. \n"
                             " \n"
                             "This is useful for RETRO_KEYBOARD centric \n"
                             "implementations which query a large area of \n"
                             "the keyboard, where it is not desirable that \n"
                             "hotkeys get in the way.");
            break;
        case MENU_ENUM_LABEL_REWIND_ENABLE:
            snprintf(s, len,
                     "Active le rembobinage.\n"
                             " \n"
                             "This will take a performance hit, \n"
                             "so it is disabled by default.");
            break;
        case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_TOGGLE:
            snprintf(s, len,
                     "Applique les cheats immédiatement après l'activation.");
            break;
        case MENU_ENUM_LABEL_CHEAT_APPLY_AFTER_LOAD:
            snprintf(s, len,
                     "Applique les cheats automatiquement au chargement d'un jeu.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_DIR_PATH:
            snprintf(s, len,
                     "Dossier des cœurs. \n"
                             " \n"
                             "A directory for where to search for \n"
                             "libretro core implementations.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            snprintf(s, len,
                     "Fréquence de rafraîchissement automatique.\n"
                             " \n"
                             "The accurate refresh rate of our monitor (Hz).\n"
                             "This is used to calculate audio input rate with \n"
                             "the formula: \n"
                             " \n"
                             "audio_input_rate = game input rate * display \n"
                             "refresh rate / game refresh rate\n"
                             " \n"
                             "If the implementation does not report any \n"
                             "values, NTSC defaults will be assumed for \n"
                             "compatibility.\n"
                             " \n"
                             "This value should stay close to 60Hz to avoid \n"
                             "large pitch changes. If your monitor does \n"
                             "not run at 60Hz, or something close to it, \n"
                             "disable VSync, and leave this at its default.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED:
            snprintf(s, len,
                     "Définir la fréquence de rafraîchissement détectée\n"
                             " \n"
                            "Sets the refresh rate to the actual value\n"
                            "polled from the display driver.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ROTATION:
            snprintf(s, len,
                     "Force une certaine rotation \n"
                             "de l'écran.\n"
                             " \n"
                             "The rotation is added to rotations which\n"
                             "the libretro core sets (see Video Allow\n"
                             "Rotate).");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE:
            snprintf(s, len,
                     "Résolution en plein écran.\n"
                             " \n"
                             "Resolution of 0 uses the \n"
                             "resolution of the environment.\n");
            break;
        case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            snprintf(s, len,
                     "Vitesse de l'avance rapide.\n"
                             " \n"
                             "The maximum rate at which content will\n"
                             "be run when using fast forward.\n"
                             " \n"
                             " (E.g. 5.0 for 60 fps content => 300 fps \n"
                             "cap).\n"
                             " \n"
                             "RetroArch will go to sleep to ensure that \n"
                             "the maximum rate will not be exceeded.\n"
                             "Do not rely on this cap to be perfectly \n"
                             "accurate.");
            break;
        case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
            snprintf(s, len,
                     "Synchronise avec la fréquence exacte du contenu.\n"
                             " \n"
                             "This option is the equivalent of forcing x1 speed\n"
                             "while still allowing fast forward.\n"
                             "No deviation from the core requested refresh rate,\n"
                             "no sound Dynamic Rate Control).");
            break;
        case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            snprintf(s, len,
                     "Quel moniteur préférer.\n"
                             " \n"
                             "0 (default) means no particular monitor \n"
                             "is preferred, 1 and up (1 being first \n"
                             "monitor), suggests RetroArch to use that \n"
                             "particular monitor.");
            break;
        case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
            snprintf(s, len,
                     "Force le recadrage du surbalayage \n"
                             "des images.\n"
                             " \n"
                             "Exact behavior of this option is \n"
                             "core-implementation specific.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
            snprintf(s, len,
                     "Mets la vidéo à l'échelle uniquement \n"
                             "à l'entier le plus proche.\n"
                             " \n"
                             "The base size depends on system-reported \n"
                             "geometry and aspect ratio.\n"
                             " \n"
                             "If Force Aspect is not set, X/Y will be \n"
                             "integer scaled independently.");
            break;
        case MENU_ENUM_LABEL_AUDIO_VOLUME:
            snprintf(s, len,
                     "Volume audio, exprimé en dB.\n"
                             " \n"
                             " 0 dB is normal volume. No gain will be applied.\n"
                             "Gain can be controlled in runtime with Input\n"
                             "Volume Up / Input Volume Down.");
            break;
        case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            snprintf(s, len,
                     "Contrôle du débit audio.\n"
                             " \n"
                             "Setting this to 0 disables rate control.\n"
                             "Any other value controls audio rate control \n"
                             "delta.\n"
                             " \n"
                             "Defines how much input rate can be adjusted \n"
                             "dynamically.\n"
                             " \n"
                             " Input rate is defined as: \n"
                             " input rate * (1.0 +/- (rate control delta))");
            break;
        case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            snprintf(s, len,
                     "Limite de synchronisation audio maximale.\n"
                             " \n"
                             "Defines the maximum change in input rate.\n"
                             "You may want to increase this to enable\n"
                             "very large changes in timing, for example\n"
                             "running PAL cores on NTSC displays, at the\n"
                             "cost of inaccurate audio pitch.\n"
                             " \n"
                             " Input rate is defined as: \n"
                             " input rate * (1.0 +/- (max timing skew))");
            break;
        case MENU_ENUM_LABEL_OVERLAY_NEXT:
            snprintf(s, len,
                     "Applique la prochaine surimpression.\n"
                             " \n"
                             "Wraps around.");
            break;
        case MENU_ENUM_LABEL_LOG_VERBOSITY:
            snprintf(s, len,
                     "Active ou désactive le niveau de journalisation \n"
                             "du programme.");
            break;
        case MENU_ENUM_LABEL_VOLUME_UP:
            snprintf(s, len,
                     "Augmente le volume audio.");
            break;
        case MENU_ENUM_LABEL_VOLUME_DOWN:
            snprintf(s, len,
                     "Diminue le volume audio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION:
            snprintf(s, len,
                     "Désactiver de force la composition.\n"
                             "Valide uniquement pour Windows Vista/7 pour l'instant.");
            break;
        case MENU_ENUM_LABEL_PERFCNT_ENABLE:
            snprintf(s, len,
                     "Activer ou désactiver les compteurs \n"
                             "de performance du programme.");
            break;
        case MENU_ENUM_LABEL_SYSTEM_DIRECTORY:
            snprintf(s, len,
                     "Dossier système. \n"
                             " \n"
                             "Sets the 'system' directory.\n"
                             "Cores can query for this\n"
                             "directory to load BIOSes, \n"
                             "system-specific configs, etc.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
            snprintf(s, len,
                     "Enregistre automatiquement une sauvegarde instantanée \n"
                             "à la fin de l'éxécution' de RetroArch.\n"
                             " \n"
                             "RetroArch will automatically load any savestate\n"
                             "with this path on startup if 'Auto Load State\n"
                             "is enabled.");
            break;
        case MENU_ENUM_LABEL_VIDEO_THREADED:
            snprintf(s, len,
                     "Utilise le pilote vidéo sur plusieurs fils d'exécution.\n"
                             " \n"
                             "Using this might improve performance at the \n"
                             "possible cost of latency and more video \n"
                             "stuttering.");
            break;
        case MENU_ENUM_LABEL_VIDEO_VSYNC:
            snprintf(s, len,
                     "Synchronisation vertivale vidéo (V-Sync).\n");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
            snprintf(s, len,
                     "Tente de synchroniser matériellement \n"
                             "le processeur et le processeur graphique.\n"
                             " \n"
                             "Can reduce latency at the cost of \n"
                             "performance.");
            break;
        case MENU_ENUM_LABEL_REWIND_GRANULARITY:
            snprintf(s, len,
                     "Précision du rembobinage.\n"
                             " \n"
                             " When rewinding defined number of \n"
                             "frames, you can rewind several frames \n"
                             "at a time, increasing the rewinding \n"
                             "speed.");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE:
            snprintf(s, len,
                     "Mémoire tampon de rembobinage (Mo).\n"
                             " \n"
                             " The amount of memory in Mo to reserve \n"
                             "for rewinding.  Increasing this value \n"
                             "increases the rewind history length.\n");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP:
            snprintf(s, len,
                     "Précision d'ajustement du tampon de rembobinage (Mo).\n"
                             " \n"
                             " Each time you increase or decrease \n"
                             "the rewind buffer size value via this \n"
                             "UI it will change by this amount.\n");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT:
            snprintf(s, len,
                     "Prendre une capture d'écran.");
            break;
        case MENU_ENUM_LABEL_VIDEO_FRAME_DELAY:
            snprintf(s, len,
                     "Définit le délai en millisecondes après V-Sync\n"
                             "avant l'exécution du cœur.\n"
                             "\n"
                             "Can reduce latency at the cost of\n"
                             "higher risk of stuttering.\n"
                             " \n"
                             "Maximum is 15.");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
            snprintf(s, len,
                     "Nombre d'images que le processeur peut éxécuter en avance \n"
                             "du processeur graphique avec l'option 'Synchronisation \n"
                             "matérielle du processeur graphique'.\n"
                             " \n"
                             "Maximum is 3.\n"
                             " \n"
                             " 0: Syncs to GPU immediately.\n"
                             " 1: Syncs to previous frame.\n"
                             " 2: Etc ...");
            break;
        case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            snprintf(s, len,
                     "Insère une image noire \n"
                             "entre chaque image.\n"
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
                     "Afficher l'écran de configuration initiale.\n"
                             "Is automatically set to false when seen\n"
                             "for the first time.\n"
                             " \n"
                             "This is only updated in config if\n"
                             "'Save Configuration on Exit' is enabled.\n");
            break;
        case MENU_ENUM_LABEL_VIDEO_FULLSCREEN:
            snprintf(s, len, "Active/désactive le mode plein écran.");
            break;
        case MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE:
            snprintf(s, len,
                     "Empêche l'écrasement de la mémoire SRAM \n"
                             "lors du chargement d'une sauvegarde instantanée.\n"
                             " \n"
                             "Pourrait potentiellement conduire à des bugs de jeu.");
            break;
        case MENU_ENUM_LABEL_PAUSE_NONACTIVE:
            snprintf(s, len,
                     "Mettre en pause quand la fenêtre \n"
                             "est en arrière-plan.");
            break;
        case MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT:
            snprintf(s, len,
                     "Capture l'écran en utilisant les shaders produits \n"
                             "par le processeur graphique si disponibles.");
            break;
        case MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY:
            snprintf(s, len,
                     "Dossier des captures d'écran. \n"
                             " \n"
                             "Directory to dump screenshots to."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
            snprintf(s, len,
                     "Intervalle d'échange V-Sync.\n"
                             " \n"
                             "Uses a custom swap interval for VSync. Set this \n"
                             "to effectively halve monitor refresh rate.");
            break;
        case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
            snprintf(s, len,
                     "Dossier des sauvegardes. \n"
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
                     "Dossier des sauvegardes instantanées. \n"
                             " \n"
                             "Save all save states (*.state) to this \n"
                             "directory.\n"
                             " \n"
                             "This will be overridden by explicit command line\n"
                             "options.");
            break;
        case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
            snprintf(s, len,
                     "Dossier des assets. \n"
                             " \n"
                             " This location is queried by default when \n"
                             "menu interfaces try to look for loadable \n"
                             "assets, etc.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            snprintf(s, len,
                     "Dossier des arrière-plans dynamiques. \n"
                             " \n"
                             " The place to store backgrounds that will \n"
                             "be loaded dynamically by the menu depending \n"
                             "on context.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
            snprintf(s, len,
                     "Taux de ralentissement maximal."
                             " \n"
                             "When slowmotion, content will slow\n"
                             "down by factor.");
            break;
        case MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD:
            snprintf(s, len,
                     "Definit le seuil de l'axe des touches.\n"
                             " \n"
                             "How far an axis must be tilted to result\n"
                             "in a button press.\n"
                             " Possible values are [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
            snprintf(s, len,
                     "Délai d'activation du turbo.\n"
                             " \n"
                             "Describes the period of which turbo-enabled\n"
                             "buttons toggle.\n"
                             " \n"
                             "Numbers are described in frames."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
            snprintf(s, len,
                     "Cycle de répétition des touches.\n"
                             " \n"
                             "Describes how long the period of a turbo-enabled\n"
                             "should be.\n"
                             " \n"
                             "Numbers are described in frames."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_TOUCH_ENABLE:
            snprintf(s, len, "Active la prise en charge du tactile.");
            break;
        case MENU_ENUM_LABEL_INPUT_PREFER_FRONT_TOUCH:
            snprintf(s, len, "Utilise le tactile avant plutôt que le tactile arrière.");
            break;
        case MENU_ENUM_LABEL_MOUSE_ENABLE:
            snprintf(s, len, "Active la souris dans le menu.");
            break;
        case MENU_ENUM_LABEL_POINTER_ENABLE:
            snprintf(s, len, "Active le tactile dans le menu.");
            break;
        case MENU_ENUM_LABEL_MENU_WALLPAPER:
            snprintf(s, len, "Emplacement d'une image à définir comme arrière-plan.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND:
            snprintf(s, len,
                     "Retour au début et/ou à la fin \n"
                             "si les limites de la liste sont atteintes \n"
                             "horizontalement et/ou verticalement.");
            break;
        case MENU_ENUM_LABEL_PAUSE_LIBRETRO:
            snprintf(s, len,
                     "Si désactivé, le jeu continuera à fonctionner \n"
                             "en arrière-plan lorsque vous serez \n"
                             "dans le menu.");
            break;
        case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
            snprintf(s, len,
                     "Suspend l'économiseur d'écran. Is a hint that \n"
                             "does not necessarily have to be \n"
                             "honored by the video driver.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MODE:
            snprintf(s, len,
                     "Mode client de jeu en réseau pour l'utilisateur actuel. \n"
                             "Will be 'Server' mode if disabled.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
            snprintf(s, len,
                     "Nombre d'images de délai à utiliser pour le jeu en réseau. \n"
                             " \n"
                             "Increasing this value will increase \n"
                             "performance, but introduce more latency.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE:
            snprintf(s, len,
                     "Annoncer ou non le jeu en réseau publiquement. \n"
                             " \n"
                             "If set to false, clients must manually connect \n"
                             "rather than using the public lobby.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            snprintf(s, len,
                     "Démarrer le jeu en réseau en mode spectateur ou non. \n"
                             " \n"
                             "If set to true, netplay will be in spectator mode \n"
                             "on start. It's always possible to change mode \n"
                             "later.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES:
            snprintf(s, len,
                     "Autoriser les connexions en mode passif ou non. \n"
                             " \n"
                             "Slave-mode clients require very little processing \n"
                             "power on either side, but will suffer \n"
                             "significantly from network latency.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
            snprintf(s, len,
                     "Autoriser les connexions en mode actif ou non. \n"
                             " \n"
                             "Not recommended except for very fast networks \n"
                             "with very weak machines. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
            snprintf(s, len,
                     "Faire tourner le jeu en réseau dans un mode\n"
                             "ne nécessitant pas de sauvegardes instantanées. \n"
                             " \n"
                             "If set to true, a very fast network is required,\n"
                             "but no rewinding is performed, so there will be\n"
                             "no netplay jitter.\n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
            snprintf(s, len,
                     "Fréquence en images avec laquelle le jeu \n"
                             "en réseau vérifiera que l'hôte et le client \n"
                             "sont synchronisés. \n"
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
                     "Nombre d'images de latence des entrées que le jeu \n"
                     "en réseau doit utiliser pour masquer la latence du réseau. \n"
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
                     "Plage d'images de latence des entrées pouvant \n"
                     "être utilisée pour masquer la latence \n"
                     "du réseau. \n"
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
                     "Lors de l'hébergement, tenter d'intercepter des connexions\n"
                             "depuis l'internet public, en utilisant UPnP ou\n"
                             "des technologies similaires pour sortir du réseau local. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_USE_MITM_SERVER:
            snprintf(s, len,
                     "Lors de l'hébergement d'une session de jeu en réseau, transférer  \n"
                             "les connexions via un serveur intermédiaire pour contourner \n"
                             "le pare-feu ou les problèmes de NAT/UPnP. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MITM_SERVER:
            snprintf(s, len,
                     "Choisissez un serveur de relais spécifique à utiliser \n"
                             "pour le jeu en réseau. A server that is \n"
                             "located closer to you may have less latency. \n");
            break;
        case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            snprintf(s, len,
                     "Nombre d'images max en mémoire tampon. This \n"
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
                     "Lisse l'image avec le filtrage bilinéaire. \n"
                             "Should be disabled if using shaders.");
            break;
        case MENU_ENUM_LABEL_TIMEDATE_ENABLE:
            snprintf(s, len,
                     "Affiche la date et/ou l'heure locale dans le menu.");
            break;
        case MENU_ENUM_LABEL_TIMEDATE_STYLE:
           snprintf(s, len,
              "Style d'affichage dans lequel afficher la date/l'heure.");
           break;
        case MENU_ENUM_LABEL_BATTERY_LEVEL_ENABLE:
            snprintf(s, len,
                     "Affiche le niveau de la batterie actuel dans le menu.");
            break;
        case MENU_ENUM_LABEL_CORE_ENABLE:
            snprintf(s, len,
                     "Affiche le cœur actuel dans le menu.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_HOST:
            snprintf(s, len,
                     "Active le jeu en réseau en mode hébergement (serveur).");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ENABLE_CLIENT:
            snprintf(s, len,
                     "Active le jeu en réseau en mode client.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DISCONNECT:
            snprintf(s, len,
                     "Déconnect une connexion de jeu en réseau active.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_LAN_SCAN_SETTINGS:
            snprintf(s, len,
                     "Rechercher et se connecter à des hôtes de jeu en réseau sur le réseau local.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SETTINGS:
            snprintf(s, len,
                     "Réglages liés au jeu en réseau.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPER:
            snprintf(s, len,
                     "Charge dynamiquement un nouvel arrière-plan \n"
                             "en fonction du contexte.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL:
            snprintf(s, len,
                     "URL du dossier principal de mise à jour sur le \n"
                             "buildbot Libretro.");
            break;
        case MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL:
            snprintf(s, len,
                     "URL du dossier de mise à jour des assets sur le \n"
                             "buildbot Libretro.");
            break;
        case MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE:
            snprintf(s, len,
                     "Si activé, remplace les assignations de touches \n"
                             "avec les touches remappées pour le \n"
                             "cœur actuel.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_DIRECTORY:
            snprintf(s, len,
                     "Dossier des surimpressions. \n"
                             " \n"
                             "Defines a directory where overlays are \n"
                             "kept for easy access.");
            break;
        case MENU_ENUM_LABEL_INPUT_MAX_USERS:
            snprintf(s, len,
                     "Nombre maximum d'utilisateurs pris en charge par \n"
                             "RetroArch.");
            break;
        case MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE:
            snprintf(s, len,
                     "Après le téléchargement, extrait automatiquement \n"
                             "le contenu des archives qui ont été \n"
                             "téléchargées.");
            break;
        case MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE:
            snprintf(s, len,
                     "Filtrer les fichiers affichés selon \n"
                             "les extensions prises en charge.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_NICKNAME:
            snprintf(s, len,
                     "Nom d'utilisateur de la personne utilisant RetroArch. \n"
                             "This will be used for playing online games.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
            snprintf(s, len,
                     "Port de l'adresse IP de l'hôte. \n"
                             "Can be either a TCP or UDP port.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
            snprintf(s, len,
                     "Active ou désactive le modespectateur pour \n"
                             "l'utilisateur durant le jeu en réseau.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
            snprintf(s, len,
                     "Adresse de l'hôte auquel se connecter.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PASSWORD:
            snprintf(s, len,
                     "Mot de passe pour se connecter à l'hôte \n"
                             "de jeu en réseau. Utilisé uniquement en mode hébergement.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATE_PASSWORD:
            snprintf(s, len,
                     "Mot de passe pour se connecter à l'hôte \n"
                             "de jeu en réseau avec des privilèges spectateur \n"
                             "uniquement. Utilisé uniquement en mode hôte.");
            break;
        case MENU_ENUM_LABEL_STDIN_CMD_ENABLE:
            snprintf(s, len,
                     "Active l'interface de commandes stdin.");
            break;
        case MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT:
            snprintf(s, len,
                     "Lancer l'interface de bureau \n"
                             "au démarrage (si disponible).");
            break;
        case MENU_ENUM_LABEL_MENU_DRIVER:
            snprintf(s, len, "Pilote de menu à utiliser.");
            break;
        case MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO:
            snprintf(s, len,
                     "Combinaison de touches de la manette pour afficher/masquer le menu. \n"
                             " \n"
                             "0 - None \n"
                             "1 - Press L + R + Y + D-Pad Down \n"
                             "simultaneously. \n"
                             "2 - Press L3 + R3 simultaneously. \n"
                             "3 - Press Start + Select simultaneously.");
            break;
        case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
            snprintf(s, len, "Autorise n'importe quel utilisateur à contrôler le menu. \n"
                    " \n"
                    "When disabled, only user 1 can control the menu.");
            break;
        case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
            snprintf(s, len,
                     "Active la détection automatique des touches.\n"
                             " \n"
                             "Will attempt to auto-configure \n"
                             "joypads, Plug-and-Play style.");
            break;
        case MENU_ENUM_LABEL_CAMERA_ALLOW:
            snprintf(s, len,
                     "Autorise ou interdit l'accès à la caméra \n"
                             "par les cœurs.");
            break;
        case MENU_ENUM_LABEL_LOCATION_ALLOW:
            snprintf(s, len,
                     "Autorise ou interdit l'accès aux données de localisation \n"
                             "par les cœurs.");
            break;
        case MENU_ENUM_LABEL_TURBO:
            snprintf(s, len,
                     "Active le mode turbo.\n"
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
                     "Affiche/masque le clavier à l'écran.");
            break;
        case MENU_ENUM_LABEL_AUDIO_MUTE:
            snprintf(s, len,
                     "Active/désactive le son.");
            break;
        case MENU_ENUM_LABEL_REWIND:
            snprintf(s, len,
                     "Maintenir la touche pour rembobiner.\n"
                             " \n"
                             "Rewind must be enabled.");
            break;
        case MENU_ENUM_LABEL_EXIT_EMULATOR:
            snprintf(s, len,
                     "Touche pour quitter RetroArch proprement."
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
                     "Charge une sauvegarde instantanée.");
            break;
        case MENU_ENUM_LABEL_SAVE_STATE:
            snprintf(s, len,
                     "Sauvegarde instantanée.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_GAME_WATCH:
            snprintf(s, len,
                     "Basculer le jeu en réseau entre le mode jouer et le mode spectateur.");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_PLUS:
            snprintf(s, len,
                     "Incrémente l'index du cheat.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_INDEX_MINUS:
            snprintf(s, len,
                     "Décrémente l'index du cheat.\n");
            break;
        case MENU_ENUM_LABEL_SHADER_PREV:
            snprintf(s, len,
                     "Applique le shader précédent dans le dossier.");
            break;
        case MENU_ENUM_LABEL_SHADER_NEXT:
            snprintf(s, len,
                     "Applique le prochain shader dans le dossier.");
            break;
        case MENU_ENUM_LABEL_RESET:
            snprintf(s, len,
                     "Réinitialise le contenu.\n");
            break;
        case MENU_ENUM_LABEL_PAUSE_TOGGLE:
            snprintf(s, len,
                     "Bascule entre la mise en pause et la reprise.");
            break;
        case MENU_ENUM_LABEL_CHEAT_TOGGLE:
            snprintf(s, len,
                     "Active/désactive l'index du cheat.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_IDX:
            snprintf(s, len,
                     "Position d'index dans la liste.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_ADDRESS_BIT_POSITION:
            snprintf(s, len,
                     "Masque de bit d'adresse lorsque taille de la recherche en mémoire < 8-bit.\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_COUNT:
            snprintf(s, len,
                     "Nombre de fois que le cheat sera appliqué.\nUse with the other two Iteration options to affect large areas of memory.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_ADDRESS:
            snprintf(s, len,
                     "Après chaque 'Nombre d'itérations' l'adresse mémoire sera incrémentée de ce montant multiplié par la 'Taille de la recherche dans la mémoire'.");
            break;
        case MENU_ENUM_LABEL_CHEAT_REPEAT_ADD_TO_VALUE:
            snprintf(s, len,
                     "Après chaque 'Nombre d'itérations' la valeur sera incrémentée de ce montant.");
            break;
        case MENU_ENUM_LABEL_CHEAT_MATCH_IDX:
            snprintf(s, len,
                     "Selectionner la correspondance à afficher.");
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_CONT:
            snprintf(s, len,
                     "Rechercher dans la mémoire pour créer un nouveau cheat");
            break;
        case MENU_ENUM_LABEL_CHEAT_START_OR_RESTART:
            snprintf(s, len,
                     "Gauche/droite pour changer la taille de bits\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EXACT:
            snprintf(s, len,
                     "Gauche/droite pour changer la valeur\n");
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
                     "Gauche/droite pour changer la valeur\n");
            break;
        case MENU_ENUM_LABEL_CHEAT_SEARCH_EQMINUS:
            snprintf(s, len,
                     "Gauche/droite pour changer la valeur\n");
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
                     "Gros-boutienne  : 258 = 0x0102\n"
                     "Petit-boutienne : 258 = 0x0201");
            break;
        case MENU_ENUM_LABEL_HOLD_FAST_FORWARD:
            snprintf(s, len,
                     "Maintenir pour l'avance rapide. Relâcher la touche \n"
                             "désactive l'avance rapide.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_HOLD:
            snprintf(s, len,
                     "Maintenir pour le ralenti.");
            break;
        case MENU_ENUM_LABEL_FRAME_ADVANCE:
            snprintf(s, len,
                     "Avance image par image lorsque le contenu est en pause.");
            break;
        case MENU_ENUM_LABEL_BSV_RECORD_TOGGLE:
            snprintf(s, len,
                     "Activer/désactiver l'enregistrement.");
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
                     "Axe pour le stick analogique (DualShock-esque).\n"
                             " \n"
                             "Bound as usual, however, if a real analog \n"
                             "axis is bound, it can be read as a true analog.\n"
                             " \n"
                             "Positive X axis is right. \n"
                             "Positive Y axis is down.");
            break;
        case MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC:
            snprintf(s, len,
                     "RetroArch par lui-même ne fait rien. \n"
                            " \n"
                            "Pour le faire faire des choses, vous devez y charger \n"
                            "un programme. \n"
                            "\n"
                            "Nous appelons un tel programme 'cœur Libretro', \n"
                            "ou 'cœur' en bref. \n"
                            " \n"
                            "Pour charger un cœur, sélectionnez-en un dans\n"
                            "'Charger un cœur'.\n"
                            " \n"
#ifdef HAVE_NETWORKING
                    "Vous pouvez obtenir des cœurs de plusieurs manières : \n"
                    "* Téléchargez-les en allant dans\n"
                    "'%s' -> '%s'.\n"
                    "* Déplacez-les manuellement vers\n"
                    "'%s'.",
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_LIST),
                    msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#else
                            "Vous pouvez obtenir des cœurs\n"
                            "en les déplaçant manuellement vers\n"
                            "'%s'.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH)
#endif
            );
            break;
        case MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC:
            snprintf(s, len,
                     "Vous pouvez changer la surimpression de manette de jeu virtuelle\n"
                             "en allant dans '%s' -> '%s'."
                             " \n"
                             "De là, vous pouvez changer la surimpression,\n"
                             "changer la taille et l'opacité des touches, etc.\n"
                             " \n"
                             "REMARQUE : Par défaut, les surimpressions de manettes\n"
                             "de jeu virtuelles sont masquées dans le menu.\n"
                             "Si vous souhaitez modifier ce comportement, vous pouvez\n"
                             "définir '%s' sur désactivé.",
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU)
            );
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_ENABLE:
            snprintf(s, len,
                     "Active l'utilisation d'une couleur de fond pour le texte d'affichage à l'écran (OSD).");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_RED:
            snprintf(s, len,
                     "Définit la valeur de rouge de la couleur d'arrière-plan du texte d'affichage à l'écran (OSD). Les valeurs valides sont comprises entre 0 et 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_GREEN:
            snprintf(s, len,
                     "Définit la valeur de vert de la couleur d'arrière-plan du texte d'affichage à l'écran (OSD). Les valeurs valides sont comprises entre 0 et 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_BLUE:
            snprintf(s, len,
                     "Définit la valeur de bleu de la couleur d'arrière-plan du texte d'affichage à l'écran (OSD). Les valeurs valides sont comprises entre 0 et 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_BGCOLOR_OPACITY:
            snprintf(s, len,
                     "Définit l'opacité de la couleur d'arrière-plan du texte d'affichage à l'écran (OSD). Les valeurs valides sont comprises entre 0.0 et 1.0.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_RED:
            snprintf(s, len,
                     "Définit la valeur de rouge de la couleur du texte d'affichage à l'écran (OSD). Les valeurs valides sont comprises entre 0 et 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_GREEN:
            snprintf(s, len,
                     "Définit la valeur de vert de la couleur du texte d'affichage à l'écran (OSD). Les valeurs valides sont comprises entre 0 et 255.");
            break;
        case MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_COLOR_BLUE:
            snprintf(s, len,
                     "Définit la valeur de bleu de la couleur du texte d'affichage à l'écran (OSD). Les valeurs valides sont comprises entre 0 et 255.");
            break;
        case MENU_ENUM_LABEL_MIDI_DRIVER:
            snprintf(s, len,
                     "Pilote MIDI à utiliser.");
            break;
        case MENU_ENUM_LABEL_MIDI_INPUT:
            snprintf(s, len,
                     "Définit le périphérique d'entrée (spécifique au pilote).\n"
                     "When set to \"Off\", MIDI input will be disabled.\n"
                     "Device name can also be typed in.");
            break;
        case MENU_ENUM_LABEL_MIDI_OUTPUT:
            snprintf(s, len,
                     "Définit le périphérique de sortie (spécifique au pilote).\n"
                     "When set to \"Off\", MIDI output will be disabled.\n"
                     "Device name can also be typed in.\n"
                     " \n"
                     "When MIDI output is enabled and core and game/app support MIDI output,\n"
                     "some or all sounds (depends on game/app) will be generated by MIDI device.\n"
                     "In case of \"null\" MIDI driver this means that those sounds won't be audible.");
            break;
        case MENU_ENUM_LABEL_MIDI_VOLUME:
            snprintf(s, len,
                     "Définit le volume principal du périphérique de sortie.");
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
static const char *menu_hash_to_str_fr_label_enum(enum msg_hash_enums msg)
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
         RARCH_LOG("Non implémenté : [%d]\n", msg);
#endif
         break;
   }

   return "null";
}
#endif

const char *msg_hash_to_str_fr(enum msg_hash_enums msg) {
#ifdef HAVE_MENU
    const char *ret = menu_hash_to_str_fr_label_enum(msg);

    if (ret && !string_is_equal(ret, "null"))
       return ret;
#endif

    switch (msg) {
#include "msg_hash_fr.h"
        default:
#if 0
            RARCH_LOG("Non implémenté : [%d]\n", msg);
            {
               RARCH_LOG("[%d] : %s\n", msg - 1, msg_hash_to_str(((enum msg_hash_enums)(msg - 1))));
            }
#endif
            break;
    }

    return "null";
}

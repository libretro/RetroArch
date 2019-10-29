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

#if defined(_MSC_VER) && !defined(_XBOX) && (_MSC_VER >= 1500 && _MSC_VER < 1900)
#if (_MSC_VER >= 1700)
/* https://support.microsoft.com/en-us/kb/980263 */
#pragma execution_character_set("utf-8")
#endif
#pragma warning(disable:4566)
#endif

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
                   "Lorsque la souris est capturée, RetroArch la masque \n"
                   "et conserve le pointeur de la souris à l'intérieur \n"
                   "de la fenêtre pour permettre au contrôle \n"
                   "relatif à la souris de mieux fonctionner.");
             break;
          case RARCH_GAME_FOCUS_TOGGLE:
             snprintf(s, len,
                   "Jeu au premier-plan/en arrière-plan.\n"
                   " \n"
                   "Quand un jeu est au premier plan, RetroArch désactive les raccourcis \n"
                   "et garde/ramène le pointeur de la souris dans la fenêtre.");
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
                   "Le fermer de manière dure (SIGKILL, etc.) quittera \n"
                   "RetroArch sans sauvegarder la RAM, etc."
#ifdef __unix__
                   "\nSur les Unix-likes, SIGINT/SIGTERM permet une \n"
                   "désinitialisation propre."
#endif
                   "");
             break;
          case RARCH_STATE_SLOT_PLUS:
          case RARCH_STATE_SLOT_MINUS:
             snprintf(s, len,
                   "Emplacements de sauvegardes instantanées. \n"
                   " \n"
                   "Lorsque l'emplacement est défini sur 0, le nom des sauvegardes instantanées \n"
                   "est *.state (ou ce qui est défini en ligne de commande). \n"
                   " \n"
                   "Lorsque l'emplacement n'est pas défini sur 0, l'emplacement sera \n"
                   "<emplacement><d>, où <d> est le numéro d'emplacement.");
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
                    "Visitez retroachievements.org et créez \n"
                    "un compte gratuit. \n"
                    " \n"
                    "Une fois que vous avez terminé votre inscription, \n"
                    "vous devrez entrer votre nom d'utilisateur \n"
                    "et votre mot de passe dans RetroArch.");
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
                    "Nécessite un redémarrage pour que les modifications \n"
                    "prennent effet. \n"
                    " \n"
                    "REMARQUE : toutes les langues ne sont peut-être pas  \n"
                    "actuellement implémentées. \n"
                    " \n"
                    "Dans le cas où une langue n'est pas implémentée, \n"
                    "l'anglais sera utilisé.");
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
                    "La capture d'écran sera sauvegardée \n"
                    "dans le dossier assigné aux captures d'écran.");
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
                             "Emplacement du dossier de la base de données \n"
                             "de contenu.");
            break;
        case MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY:
            snprintf(s, len,
                     "Dossier des miniatures. \n"
                             " \n"
                             "Pour stocker des fichiers de miniatures.");
            break;
        case MENU_ENUM_LABEL_LIBRETRO_INFO_PATH:
            snprintf(s, len,
                     "Dossier des informations de cœurs. \n"
                             " \n"
                             "Un dossier où seront recherchées \n"
                             "les informations de cœurs de libretro.");
            break;
        case MENU_ENUM_LABEL_PLAYLIST_DIRECTORY:
            snprintf(s, len,
                     "Dossier des listes de lecture. \n"
                             " \n"
                             "Enregistre tous les fichiers de listes de lecture \n"
                             "dans ce dossier.");
            break;
        case MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN:
            snprintf(s, len,
                     "Certains cœurs peuvent avoir \n"
                             "une fonctionnalité d'extinction. \n"
                             " \n"
                             "Si cette option est laissée \n"
                             "désactivée, la sélection de \n"
                             "l'arrêt du cœur déclenchera \n"
                             "l'arrêt de RetroArch. \n"
                             " \n"
                             "Activer cette option chargera \n"
                             "un cœur factice afin de retourner \n"
                             "dans le menu et que RetroArch \n"
                             "s'arrête pas.");
            break;
        case MENU_ENUM_LABEL_CHECK_FOR_MISSING_FIRMWARE:
            snprintf(s, len,
                     "Certains cœurs peuvent nécessiter des fichiers \n"
                             "de firmware ou de BIOS. \n"
                             " \n"
                             "Si cette option est désactivée, \n"
                             "il essaiera de se charger même \n"
                             "si ce firmware est manquant. \n");
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
                             "Sélectionnez-le pour ouvrir ce fichier avec \n"
                             "le lecteur vidéo.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_MUSIC_OPEN:
            snprintf(s, len,
                     "Musique. \n"
                             " \n"
                             "Sélectionnez-le pour ouvrir ce fichier avec \n"
                             "le lecteur de musique.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE:
            snprintf(s, len,
                     "Fichier d'image.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_IMAGE_OPEN_WITH_VIEWER:
            snprintf(s, len,
                     "Image. \n"
                             " \n"
                             "Sélectionnez-le pour ouvrir ce fichier avec \n"
                             "la visionneuse d'images.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE_SELECT_FROM_COLLECTION:
            snprintf(s, len,
                     "Cœur libretro. \n"
                             " \n"
                             "Sélectionner ceci associera ce cœur \n"
                             "au jeu.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_CORE:
            snprintf(s, len,
                     "Cœur libretro. \n"
                             " \n"
                             "Sélectionnez ce fichier pour que RetroArch charge ce cœur.");
            break;
        case MENU_ENUM_LABEL_FILE_BROWSER_DIRECTORY:
            snprintf(s, len,
                     "Dossier. \n"
                             " \n"
                             "Sélectionnez-le pour ouvrir ce dossier.");
            break;
        case MENU_ENUM_LABEL_CACHE_DIRECTORY:
            snprintf(s, len,
                     "Dossier de cache. \n"
                             " \n"
                             "Le contenu décompressé par RetroArch \n"
                             "sera extrait temporairement dans ce dossier.");
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
                             "Définit le dossier de départ du navigateur de fichiers du menu.");
            break;
        case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
            snprintf(s, len,
                     "Influence la façon dont la détection\n"
                             "des touches pressées est effectuée dans RetroArch.\n"
                             "\n"
                             "Précoce - La détection est effectuée\n"
                             "avant le traitement de l'image.\n"
                             "Normale - La détection est effectuée\n"
                             "lorsque son traîtement est demandé.\n"
                             "Tardive - La détection est effectuée\n"
                             "à la première requête d'entrées par image.\n"
                             "\n"
                             "La régler sur 'Précoce' ou 'Tardive' peut\n"
                             "entraîner une réduction de la latence,\n"
                             "en fonction de votre configuration.\n\n"
                             "Sera ignorée lors du jeu en réseau."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND:
            snprintf(s, len,
                     "Masquer les descripteurs d'appellation des touches \n"
                             "non définis par le cœur.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE:
            snprintf(s, len,
                     "Fréquence de rafraîchissement de votre écran. \n"
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
                             "Peut ne pas être honoré si le pilote audio \n"
                             "ne peut pas fournir une latence donnée.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE:
            snprintf(s, len,
                     "Autorise les cœurs à définir la rotation. Si cette option est désactivée, \n"
                             "les requêtes de rotation sont honorées mais ignorées.\n\n"
                             "Utilisé pour les configurations dans lesquelles on fait pivoter \n"
                             "manuellement l'écran.");
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
                             "Lorsque le contenu est chargé, l'emplacement de sauvegarde instantanée \n"
                             "sera positionné sur la valeur existante la plus élevée (dernière sauvegarde instantanée).");
            break;
        case MENU_ENUM_LABEL_FPS_SHOW:
            snprintf(s, len,
                     "Permet d'afficher le nombre d'images par seconde \n"
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
                     "Décalage pour l'emplacement où les messages seront placés \n"
                             "à l'écran. Les valeurs sont dans la plage [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE:
            snprintf(s, len,
                     "Active ou désactive la surimpression actuelle.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU:
            snprintf(s, len,
                     "Empêche la surimpression actuelle d'apparaître \n"
                             "dans le menu.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS:
            snprintf(s, len,
                      "Affiche les touches clavier/manette pressées \n"
                            "sur la surimpression à l'écran.");
            break;
        case MENU_ENUM_LABEL_INPUT_OVERLAY_SHOW_PHYSICAL_INPUTS_PORT:
            snprintf(s, len,
                      "Sélectionne le port d'écoute des touches pressées \n"
                            "affichées sur la surimpression à l'écran.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_PRESET:
            snprintf(s, len,
                     "Emplacement de la surimpression.");
            break;
        case MENU_ENUM_LABEL_OVERLAY_OPACITY:
            snprintf(s, len,
                     "Opacité de la surimpression.");
#ifdef HAVE_VIDEO_LAYOUT
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_ENABLE:
            snprintf(s, len,
                      "Active ou désactive la disposition d'affichage actuelle.");
            break;
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_PATH:
            snprintf(s, len,
                      "Emplacement de la disposition d'affichage.");
            break;
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_SELECTED_VIEW:
            snprintf(s, len,
                      "Les dispositions d'affichage peuvent contenir plusieurs vues. \n"
                      "Sélectionne une vue.");
            break;
#endif
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT:
            snprintf(s, len,
                     "Délai pour l'assignation (en secondes). \n"
                             "Nombre de secondes à attendre avant de passer \n"
                             "à l'assignation de touche suivante.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_HOLD:
            snprintf(s, len,
               "Temps de maintien pour l'assignation (en secondes). \n"
               "Nombre de secondes à maintenir une touche pour l'assigner.");
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
                     "Activez cette option si les cœurs bénéficiant de l'accélération graphique \n"
                             "devraient bénéficier de leur propre contexte privé. \n"
                             "Évite d'avoir à supposer des changements \n"
                             "d'état matériel entre deux images."
            );
            break;
        case MENU_ENUM_LABEL_CORE_LIST:
            snprintf(s, len,
                     "Charger un cœur. \n"
                             " \n"
                             "Recherchez une implémentation de cœur libretro. \n"
                             "Le dossier de démarrage du navigateur dépend \n"
                             "de votre dossier d'emplacement des cœurs. S'il \n"
                             "n'est pas défini, il commencera à la racine. \n"
                             " \n"
                             "S'il est défini, le menu l'utilisera comme \n"
                             "dossier principal. Si le dossier principal \n"
                             "est un chemin complet, il démarrera dans \n"
                             "le dossier où se trouve le fichier.");
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
                             "REMARQUE : le contenu de certains cœurs peut toujours\n"
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
                           "Pilote d'entrées udev.\n"
                           "\n"
                           "Utilise la récente API de manettes evdev pour leur\n"
                           "prise en charge. Il prend en charge le branchement\n"
                           "à chaud et le retour de force.\n"
                           "\n"
                           "Il lit les événements evdev pour la prise en charge\n"
                           "du clavier. Il prend également en charge le rappel au clavier,\n"
                           "les souris et les pavés tactiles.\n"
                           "\n"
                           "Par défaut, dans la plupart des distributions, les nœuds /dev/input\n"
                           "sont uniquement root (mode 600). Vous pouvez configurer une règle udev\n"
                           "qui les rend accessibles aux utilisateurs non root."
                           );
               else if (string_is_equal(lbl,
                        msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER_LINUXRAW)))
                     snprintf(s, len,
                           "Pilote d'entrées linuxraw. \n"
                           " \n"
                           "Ce pilote nécessite un téléscripteur actif. Les événements \n"
                           "de clavier sont lus directement à partir du téléscripteur, \n"
                           "ce qui le rend plus simple, mais pas aussi flexible que udev. \n"
                           "Les souris, etc, ne sont pas supportées du tout. \n"
                           " \n"
                           "Ce pilote utilise l'ancienne API de manettes \n"
                           "(/dev/input/js*).");
               else
                     snprintf(s, len,
                           "Pilote d'entrées.\n"
                           " \n"
                           "Selon le pilote vidéo sélectionné, l'utilisation \n"
                           "d'un pilote d'entrées différent peut être forcée.");
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
                             "Lorsque le contenu est chargé, les combinaisons de contenu \n"
                             "et de cœur libretro sont enregistrées dans l'historique. \n"
                             " \n"
                             "L'historique est enregistré dans un fichier dans le même \n"
                             "dossier que le fichier de configuration de RetroArch. Si \n"
                             "aucun fichier de configuration n'a été chargé au démarrage, \n"
                             "l'historique ne sera ni sauvegardé ni chargé et n'existera \n"
                             "pas dans le menu principal."
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
                                 "Ce pilote permet d’utiliser les cœurs  \n"
                                 "libretro GL en plus des implémentations \n"
                                 "en mode logiciel.\n"
                                 " \n"
                                 "Les performances pour les implémentations \n"
                                 "logicielles et les cœurs libretro GL \n"
                                 "dépendent du pilote GL sous-jacent de votre \n"
                                 "carte graphique.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl2"))
            {
                snprintf(s, len,
                         "Pilote vidéo SDL 2.\n"
                                 " \n"
                                 "Ce pilote vidéo SDL 2 utilise le rendu en mode \n"
                                 "logiciel.\n"
                                 " \n"
                                 "Les performances pour les implémentations \n"
                                 "de cœurs libretro en mode logiciel dépendent \n"
                                 "de l’implémentation SDL pour votre plateforme.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sdl1"))
            {
                snprintf(s, len,
                         "Pilote vidéo SDL.\n"
                                 " \n"
                                 "Ce pilote vidéo SDL 1.2 utilise le rendu en mode \n"
                                 "logiciel.\n"
                                 " \n"
                                 "Ses performances sont considérées comme sous-optimales. \n"
                                 "Pensez à ne l'utiliser qu'en dernier recours.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "d3d"))
            {
                snprintf(s, len,
                         "Pilote vidéo Direct3D. \n"
                                 " \n"
                                 "Les performances des cœurs en mode logiciel \n"
                                 "dépendent du pilote D3D sous-jacent de votre \n"
                                 "carte graphique.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "exynos"))
            {
                snprintf(s, len,
                         "Pilote vidéo Exynos-G2D. \n"
                                 " \n"
                                 "Pilote vidéo Exynos de bas niveau. Utilise \n"
                                 "le bloc G2D dans le SoC Samsung Exynos \n"
                                 "pour les opérations blit. \n"
                                 " \n"
                                 "Les performances pour les cœurs en mode \n"
                                 "logiciel devraient être optimales.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "drm"))
            {
                snprintf(s, len,
                         "Pilote vidéo DRM simple. \n"
                                 " \n"
                                 "Pilote vidéo de bas niveau utilisant libdrm pour \n"
                                 "la mise à l'échelle matérielle en utilisant des \n"
                                 "surimpressions accélérées par le processeur graphique.");
            }
            else if (string_is_equal(settings->arrays.video_driver, "sunxi"))
            {
                snprintf(s, len,
                         "Pilote vidéo Sunxi-G2D. \n"
                                 " \n"
                                 "Pilote vidéo Sunxi de bas niveau. \n"
                                 "Utilise le bloc G2D dans les SoC Allwinner.");
            }
            break;
        case MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN:
            snprintf(s, len,
                     "Module audio DSP.\n"
                             "Traite l'audio avant de l'envoyer \n"
                             "au pilote."
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
                             "Charge un préréglage de shaders directement. \n"
                             "Le menu des shaders est mis à jour en conséquence. \n"
                             " \n"
                             "Si le CGP utilise des méthodes de mise à l'échelle qui ne sont \n"
                             "pas simples (c'est-à-dire la mise à l'échelle source, le même \n"
                             "facteur de mise à l'échelle pour X/Y), le facteur de mise à \n"
                             "l'échelle affiché dans le menu peut ne pas être correct."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
            snprintf(s, len,
                     "Échelle pour ce passage. \n"
                             " \n"
                             "Le facteur d’échelle s’accumule, c’est-à-dire \n"
                             "2x pour le premier passage et 2x pour le second \n"
                             "passage vous donneront une échelle totale de 4x. \n"
                             " \n"
                             "S'il existe un facteur d'échelle pour \n"
                             "la dernière passe, le résultat sera étiré \n"
                             "à l'écran avec le filtre spécifié dans \n"
                             "'Filtre par défaut'. \n"
                             " \n"
                             "Si l'option 'Peu importe' est définie, soit \n"
                             "une échelle de 1x ou une extension en plein écran \n"
                             "sera utilisée selon que c'est pas le dernier \n"
                             "passage ou non."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
            snprintf(s, len,
                     "Passages de shaders. \n"
                             " \n"
                             "RetroArch vous permet de mélanger et de faire correspondre \n"
                             "différents shaders avec des passages de shader arbitraires, \n"
                             "avec des filtres matériels personnalisés et des facteurs d'échelle. \n"
                             " \n"
                             "Cette option spécifie le nombre de passages de shader à utiliser. \n"
                             "Si vous définissez cette valeur sur 0 et utilisez 'Appliquer \n"
                             "changements', vous utilisez un shader 'vide'. \n"
                             " \n"
                             "L'option 'Filtre par défaut' affectera \n"
                             "le filtre d'étirement.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PARAMETERS:
            snprintf(s, len,
                     "Paramètres de shaders. \n"
                             " \n"
                             "Modifie le shader actuel directement. Ne sera pas \n"
                             "sauvegardé dans un fichier de préréglages CGP/GLSLP.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PRESET_PARAMETERS:
            snprintf(s, len,
                     "Paramètres de préréglages de shaders. \n"
                             " \n"
                             "Modifie le préréglage de shaders actuellement dans le menu."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SHADER_PASS:
            snprintf(s, len,
                     "Emplacement du shader. \n"
                             " \n"
                             "Tous les shaders doivent être du même \n"
                             "type (par exemple CG, GLSL or HLSL). \n"
                             " \n"
                             "Définir le dossier de shaders pour définir \n"
                             "où le navigateur commence à chercher \n"
                             "les shaders."
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
                             "Utile pour le menu car les réglages peuvent\n"
                             "être modifiés. Écrase la configuration.\n"
                             " \n"
                             "Les #include et les commentaires ne sont \n"
                             "pas conservés. \n"
                             " \n"
                             "De par sa conception, le fichier de \n"
                             "configuration est considéré comme immuable \n"
                             "car il est probablement conservé par \n"
                             "l'utilisateur et ne doit pas être écrasé \n"
                             "derrière son dos."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
            "\nCependant, ce n'est pas le cas sur \n"
            "les consoles, où examiner le fichier \n"
            "de configuration manuellement n'est \n"
            "pas vraiment une option."
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
                             "Si 'Peu importe' est sélectionné, \n"
                             "'Filtrage par défaut' sera utilisé."
            );
            break;
        case MENU_ENUM_LABEL_AUTOSAVE_INTERVAL:
            snprintf(s, len,
                     "Sauvegarde automatiquement la mémoire SRAM \n"
                             "non volatile à intervalles réguliers.\n"
                             " \n"
                             "Ceci est désactivé par défaut. \n"
                             "L'intervalle est mesuré en secondes \n"
                             " \n"
                             "Une valeur de 0 désactive \n"
                             "la sauvegarde automatique.");
            break;
        case MENU_ENUM_LABEL_INPUT_BIND_DEVICE_TYPE:
            snprintf(s, len,
                     "Type de périphérique d'entrée. \n"
                             " \n"
                             "Choisit le type d'appareil à utiliser. Cette option \n"
                             "est utilisée pour le cœur libretro lui-même."
            );
            break;
        case MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL:
            snprintf(s, len,
                     "Définit le niveau de journalisation pour les cœurs libretro \n"
                             "(GET_LOG_INTERFACE). \n"
                             " \n"
                             " Si un niveau de journal émis par un cœur libretro \n"
                             " est inférieur au niveau libretro_log, \n"
                             " il sera ignoré.\n"
                             " \n"
                             " Les journaux DEBUG seront toujours ignorés \n"
                             " sauf si le mode verbeux est activé (--verbose).\n"
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
                             "Si l'emplacement est défini sur 0, le nom de la sauvegarde \n"
                             "est *.state (ou celui défini en ligne de commande).\n"
                             "Lorsque l'emplacement est !=0, le chemin sera \n"
                             "(chemin)(d), où (d) est le numéro d'emplacement.");
            break;
        case MENU_ENUM_LABEL_SHADER_APPLY_CHANGES:
            snprintf(s, len,
                     "Applique les changements de shaders.\n"
                             "\n"
                             "Après avoir modifié les paramètres du shader, utilisez\n"
                             "cette option pour appliquer les modifications.\n"
                             "\n"
                             "La modification des paramètres de shader est une opération\n"
                             "un peu coûteuse et doit donc être effectuée explicitement.\n"
                             "\n"
                             "Lorsque vous appliquez des shaders, les paramètres\n"
                             "de shader de menu sont enregistrés temporairement\n"
                             "(menu.cgp ou menu.glslp) et chargés.\n"
                             "Le fichier persiste après la sortie de RetroArch,\n"
                             "et est enregistré dans le dossier des shaders."
            );
            break;
        case MENU_ENUM_LABEL_SHADER_WATCH_FOR_CHANGES:
            snprintf(s, len,
                     "Verifie les changements dans les fichiers de shaders. \n"
                     " \n"
                     "Après avoir enregistré les modifications dans un shader \n"
                     "sur le disque, il sera automatiquement recompilé \n"
                     "et appliqué au contenu en cours."
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
                             "Lorsque la souris est capturée, RetroArch la masque \n"
                             "et conserve le pointeur de la souris à l'intérieur \n"
                             "de la fenêtre pour permettre le contrôle relatif \n"
                             "à la souris de mieux fonctionner.");
            break;
        case MENU_ENUM_LABEL_GAME_FOCUS_TOGGLE:
            snprintf(s, len,
                     "Jeu au premier plan/en arrière-plan.\n"
                             " \n"
                             "Quand un jeu est en premier plan, RetroArch désactive les raccourcis clavier \n"
                             "et garde/remet le pointeur de la souris dans la fenêtre.");
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
                             "Cette option dépend du pilote. \n"
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
                             "Si cette touche de raccourci est liée à un clavier, \n"
                             "à une touche ou à un axe de manette, toutes les autres \n"
                             "touches de raccourci seront désactivées, à moins que \n"
                             "ce raccourci ne soit également maintenu enfoncé. \n"
                             " \n"
                             "Cette option est utile pour les implémentations \n"
                             "centrées RETRO_KEYBOARD qui utilisent une grande \n"
                             "zone du clavier, où il n’est pas souhaitable que \n"
                             "les raccourcis clavier puissent gêner.");
            break;
        case MENU_ENUM_LABEL_REWIND_ENABLE:
            snprintf(s, len,
                     "Active le rembobinage.\n"
                             " \n"
                             "Cette option aura un coût en performances, \n"
                             "elle est donc désactivée par défaut.");
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
                             "Dossier de recherche des implémentations \n"
                             "de cœurs libretro.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            snprintf(s, len,
                     "Fréquence de rafraîchissement auto.\n"
                             " \n"
                             "Fréquence de rafraîchissement précise du moniteur (Hz).\n"
                             "Utilisée pour calculer le débit audio avec la formule :\n"
                             " \n"
                             "Débit audio = fréquence du jeu * fréquence de\n"
                             "l'écran / fréquence du jeu\n"
                             " \n"
                             "Si aucune valeur n'est rapportée, les valeurs NTSC\n"
                             "seront utilisées pour la compatibilité.\n"
                             " \n"
                             "Cette valeur devrait rester proche de 60Hz pour éviter\n"
                             "des changements de pitch importants. Sinon, désactivez\n"
                             "V-Sync et laissez ce paramètre par défaut.");
            break;
        case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_POLLED:
            snprintf(s, len,
                     "Définir la fréquence de rafraîchissement détectée\n"
                             " \n"
                            "Définit la fréquence de rafraîchissement à\n"
                            "celle détectée par le pilote d'affichage.");
            break;
        case MENU_ENUM_LABEL_VIDEO_ROTATION:
            snprintf(s, len,
                     "Force une certaine rotation \n"
                             "de l'écran.\n"
                             " \n"
                             "La rotation est ajoutée aux rotations\n"
                             "définies par le cœur libretro (voir\n"
                             "Autoriser la rotation).");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE:
            snprintf(s, len,
                     "Résolution en plein écran.\n"
                             " \n"
                             "Une résolution de 0 utilise \n"
                             "la résolution du bureau.\n");
            break;
        case MENU_ENUM_LABEL_FASTFORWARD_RATIO:
            snprintf(s, len,
                     "Vitesse de l'avance rapide.\n"
                             " \n"
                             "Fréquence maximum à laquelle le contenu sera exécuté\n"
                             "lors de l'utilisation de l'avance rapide.\n"
                             " \n"
                             "(Par exemple, 5,0 pour un contenu de 60 images/s => \n"
                             "un plafond à 300 images/s).\n"
                             " \n"
                             "RetroArch se ralentira pour veiller à \n"
                             "ce que le taux maximal ne soit pas\n"
                             "dépassé. Ne comptez pas sur ce plafonnage \n"
                             "pour être parfaitement précis.");
            break;
        case MENU_ENUM_LABEL_VRR_RUNLOOP_ENABLE:
            snprintf(s, len,
                     "Synchronise avec la fréquence exacte du contenu.\n"
                             " \n"
                             "Cette option équivaut à forcer la vitesse x1\n"
                             "tout en permettant l'avance rapide.\n"
                             "Aucun écart par rapport à la fréquence de rafraîchissement\n"
                             "demandée par le cœur, aucun contrôle dynamique du débit audio.");
            break;
        case MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX:
            snprintf(s, len,
                     "Quel moniteur préférer.\n"
                             " \n"
                             "0 (par défaut) signifie qu'aucun moniteur particulier \n"
                             "n'est préféré, et plus (1 étant le premier moniteur), \n"
                             "suggère à RetroArch d'utiliser ce moniteur \n"
                             "en particulier.");
            break;
        case MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN:
            snprintf(s, len,
                     "Force le recadrage du surbalayage \n"
                             "des images.\n"
                             " \n"
                             "Le comportement exact de cette option \n"
                             "est spécifique à l'implémentation du cœur.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
            snprintf(s, len,
                     "Mets la vidéo à l'échelle uniquement \n"
                             "à l'entier le plus proche.\n"
                             " \n"
                             "La taille de base dépend de la géométrie \n"
                             "et du rapport d'aspect détectés par le système.\n"
                             " \n"
                             "Si 'Forcer le rapport d'aspect' est désactivé, X/Y \n"
                             "seront mis à l'échelle à l'entier indépendamment.");
            break;
        case MENU_ENUM_LABEL_AUDIO_VOLUME:
            snprintf(s, len,
                     "Volume audio, exprimé en dB.\n"
                             " \n"
                             "0 dB est le volume normal. Aucun gain ne sera appliqué.\n"
                             "Le gain peut être contrôlé en cours d’exécution avec\n"
                             "les touches Volume + / Volume -.");
            break;
        case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            snprintf(s, len,
                     "Contrôle dynamique du débit audio.\n"
                             " \n"
                             "Mettre cette option à 0 désactive le contrôle du débit.\n"
                             "Toute autre valeur contrôle dynamiquement le débit \n"
                             "audio.\n"
                             " \n"
                             "Définit de combien le débit d'entrée peut \n"
                             "être ajusté dynamiquement.\n"
                             " \n"
                             " Le débit audio est défini ainsi : \n"
                             " débit audio * (1.0 +/- (Contrôle dynamique du débit audio))");
            break;
        case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            snprintf(s, len,
                     "Variation maximale du débit audio.\n"
                             " \n"
                             "Définit le changement maximal du débit audio.\n"
                             "Augmenter cette valeur permet des changements\n"
                             "très importants dans le timing en échange\n"
                             "d'un pitch audio inexact (par exemple, lors\n"
                             "de l'exécution de cœurs PAL sur des écrans NTSC).\n"
                             " \n"
                             " Le débit audio est défini ainsi : \n"
                             " débit audio * (1.0 +/- (Variation maximale du débit audio))");
            break;
        case MENU_ENUM_LABEL_OVERLAY_NEXT:
            snprintf(s, len,
                     "Applique la prochaine surimpression.\n"
                             " \n"
                             "Revient à la première si la fin est atteinte.");
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
                     "Dossier 'Système'. \n"
                             " \n"
                             "Définit le dossier 'Système'.\n"
                             "Les cœurs peuvent rechercher dans\n"
                             "ce dossier les BIOS, configurations \n"
                             "spécifiques au système, etc.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE:
        case MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD:
            snprintf(s, len,
                     "Enregistre automatiquement une sauvegarde instantanée \n"
                             "à la fin de l'éxécution' de RetroArch.\n"
                             " \n"
                             "RetroArch chargera automatiquement les sauvegardes instantanées\n"
                             "au démarrage à partir de cet emplacement si 'Chargement auto\n"
                             "des sauvegardes instantanées' est activé.");
            break;
        case MENU_ENUM_LABEL_VIDEO_THREADED:
            snprintf(s, len,
                     "Utilise le pilote vidéo sur plusieurs fils d'exécution.\n"
                             " \n"
                             "Cette option peut améliorer la performance \n"
                             "au détriment possible d'une latence \n"
                             "et de saccades visuelles accrues.");
            break;
        case MENU_ENUM_LABEL_VIDEO_VSYNC:
            snprintf(s, len,
                     "Synchronisation verticale vidéo (V-Sync).\n");
            break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC:
            snprintf(s, len,
                     "Tente de synchroniser matériellement \n"
                             "le processeur et le processeur graphique.\n"
                             " \n"
                             "Peut réduire la latence au détriment \n"
                             "de la performance.");
            break;
        case MENU_ENUM_LABEL_REWIND_GRANULARITY:
            snprintf(s, len,
                     "Précision du rembobinage.\n"
                             " \n"
                             "Lors du rembobinage d’un nombre défini  \n"
                             "d’images, vous pouvez rembobiner plusieurs \n"
                             "images à la fois, ce qui augmente sa \n"
                             "vitesse.");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE:
            snprintf(s, len,
                     "Mémoire tampon de rembobinage (Mo).\n"
                             " \n"
                             "Quantité de mémoire en Mo à réserver pour \n"
                             "le rembobinage. L'augmentation de cette valeur \n"
                             "augmente la longueur de l'historique de rembobinage.\n");
            break;
        case MENU_ENUM_LABEL_REWIND_BUFFER_SIZE_STEP:
            snprintf(s, len,
                     "Précision d'ajustement du tampon de rembobinage (Mo).\n"
                             " \n"
                             "Chaque fois que vous augmentez ou diminuez la valeur \n"
                             "de la taille de la mémoire tampon de rembobinage \n"
                             "via cette interface, cette valeur changera de ce montant.\n");
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
                             "Peut réduire la latence au prix\n"
                             "d'un plus grand risque de saccades.\n"
                             " \n"
                             "La valeur maximum est 15.");
            break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_DELAY:
                snprintf(s, len,
                         "Définit le délai en millisecondes après lequel\n"
                                 "les shaders sont chargés.\n"
                                 "\n"
                                 "Peut résoudre des problèmes graphiques\n"
                                 "lors de l'utilisation de logiciels de 'capture d'écran'.");
                break;
        case MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES:
            snprintf(s, len,
                     "Nombre d'images que le processeur peut éxécuter en avance \n"
                             "du processeur graphique avec l'option 'Synchronisation \n"
                             "matérielle du processeur graphique'.\n"
                             " \n"
                             "La valeur maximum est 3.\n"
                             " \n"
                             " 0: Se synchronise immédiatement sur le processeur graphique.\n"
                             " 1: Se synchronise avec l'image précédente.\n"
                             " 2: Etc ...");
            break;
        case MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION:
            snprintf(s, len,
                     "Insère une image noire \n"
                             "entre chaque image.\n"
                             " \n"
                             "Utile pour les utilisateurs de moniteurs \n"
                             "à 120 Hz qui souhaitent jouer à des jeux \n"
                             "en 60 Hz sans rémanence.\n"
                             " \n"
                             "La fréquence de rafraîchissement vidéo doit toujours \n"
                             "être configurée comme s'il s'agissait d'un moniteur \n"
                             "60 Hz (divisez le taux de rafraîchissement par 2).");
            break;
        case MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN:
            snprintf(s, len,
                     "Afficher l'écran de configuration initiale.\n"
                             "Cette option est automatiquement désactivée\n"
                             "lorsqu'il est vu pour la première fois.\n"
                             " \n"
                             "Ceci n’est mis à jour dans la configuration que si\n"
                             "'Sauvegarder la configuration en quittant' est activé.\n");
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
                             "Dossier de sauvegarde des captures d'écran."
            );
            break;
        case MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL:
            snprintf(s, len,
                     "Intervalle d'échange V-Sync.\n"
                             " \n"
                             "Utilise un intervalle d'échange personnalisé pour V-Sync. Utilisez cette option \n"
                             "pour réduire de moitié la fréquence de rafraîchissement du moniteur.");
            break;
        case MENU_ENUM_LABEL_SAVEFILE_DIRECTORY:
            snprintf(s, len,
                     "Dossier des sauvegardes. \n"
                             " \n"
                             "Enregistre toutes les sauvegardes (*.srm) dans \n"
                             "ce dossier. Cela inclut les fichiers liés comme \n"
                             ".bsv, .rt, .psrm, etc...\n"
                             " \n"
                             "Cette option sera remplacée par des options\n"
                             "en ligne de commande explicites.");
            break;
        case MENU_ENUM_LABEL_SAVESTATE_DIRECTORY:
            snprintf(s, len,
                     "Dossier des sauvegardes instantanées. \n"
                             " \n"
                             "Enregistre toutes les sauvegardes instantanées \n"
                             " (*.state) dans ce dossier.\n"
                             " \n"
                             "Cette option sera remplacée par des options\n"
                             "en ligne de commande explicites.");
            break;
        case MENU_ENUM_LABEL_ASSETS_DIRECTORY:
            snprintf(s, len,
                     "Dossier des assets. \n"
                             " \n"
                             "Cet emplacement est requis par défaut lorsque \n"
                             "les interfaces de menu tentent de rechercher \n"
                             "des assets chargeables, etc.");
            break;
        case MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY:
            snprintf(s, len,
                     "Dossier des arrière-plans dynamiques. \n"
                             " \n"
                             "Dossier de stockage des fonds d'écran \n"
                             "chargés dynamiquement par le menu \n"
                             "en fonction du contexte.");
            break;
        case MENU_ENUM_LABEL_SLOWMOTION_RATIO:
            snprintf(s, len,
                     "Taux de ralentissement maximal."
                             " \n"
                             "En mode ralenti, le contenu sera ralenti\n"
                             "par ce facteur.");
            break;
        case MENU_ENUM_LABEL_INPUT_BUTTON_AXIS_THRESHOLD:
            snprintf(s, len,
                     "Definit le seuil de l'axe des touches.\n"
                             " \n"
                             "À quelle distance un axe doit être incliné\n"
                             "pour entraîner une pression de touche.\n"
                             "Les valeurs possibles sont [0.0, 1.0].");
            break;
        case MENU_ENUM_LABEL_INPUT_TURBO_PERIOD:
            snprintf(s, len,
                     "Délai d'activation du turbo.\n"
                             " \n"
                             "Décrit la durée après laquelle une touche\n"
                             "est en mode turbo.\n"
                             " \n"
                             "Les nombres sont décrits en images."
            );
            break;
        case MENU_ENUM_LABEL_INPUT_DUTY_CYCLE:
            snprintf(s, len,
                     "Cycle de répétition des touches.\n"
                             " \n"
                             "Décrit la durée après laquelle une touche\n"
                             "en mode turbo se répète.\n"
                             " \n"
                             "Les nombres sont décrits en images."
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
                     "Suspend l'économiseur d'écran. C'est un indice \n"
                             "qui ne sera pas nécessairement \n"
                             "honoré par le pilote vidéo.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_MODE:
            snprintf(s, len,
                     "Mode client de jeu en réseau pour l'utilisateur actuel. \n"
                             "Sera en mode 'Serveur' si cette option est désactivée.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
            snprintf(s, len,
                     "Nombre d'images de délai à utiliser pour le jeu en réseau. \n"
                             " \n"
                             "Augmenter cette valeur augmentera les performances, \n"
                             "mais introduira plus de latence.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_PUBLIC_ANNOUNCE:
            snprintf(s, len,
                     "Annoncer ou non le jeu en réseau publiquement. \n"
                             " \n"
                             "Si cette option est désactivée, les clients devront \n"
                             "se connecter manuellement plutôt que d'utiliser le salon public.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_START_AS_SPECTATOR:
            snprintf(s, len,
                     "Démarrer le jeu en réseau en mode spectateur ou non. \n"
                             " \n"
                             "Si cette option est activée, le jeu en réseau \n"
                             "sera en mode spectateur au démarrage. Il est \n"
                             "toujours possible de changer de mode plus tard.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_ALLOW_SLAVES:
            snprintf(s, len,
                     "Autoriser les connexions en mode passif ou non. \n"
                             " \n"
                             "Les clients en mode passif nécessite très peu de puissance \n"
                             "de traitement de chaque côté, mais souffrira considérablement \n"
                             "de la latence du réseau.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_REQUIRE_SLAVES:
            snprintf(s, len,
                     "Autoriser les connexions en mode actif ou non. \n"
                             " \n"
                             "Non recommandé sauf pour les réseaux très rapides \n"
                             "avec des machines très faibles. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_STATELESS_MODE:
            snprintf(s, len,
                     "Faire tourner le jeu en réseau dans un mode\n"
                             "ne nécessitant pas de sauvegardes instantanées. \n"
                             " \n"
                             "Si cette option est activée, un réseau très rapide est requis,\n"
                             "mais aucun rembobinage n'est effectué. Il n'y aura donc\n"
                             "pas de variations de la latence sur le jeu en réseau.\n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
            snprintf(s, len,
                     "Fréquence en images avec laquelle le jeu\n"
                             "en réseau vérifiera que l'hôte et le client\n"
                             "sont synchronisés.\n"
                             "\n"
                             "Avec la plupart des cœurs, cela n'aura\n"
                             "aucun effet visible et peut être ignoré. Avec\n"
                             "les cœurs non déterministes, cette valeur détermine\n"
                             "la fréquence de synchronisation des pairs.\n"
                             "Avec les cœurs buggés, ne pas définir\n"
                             "cette valeur sur zéro affectera grandement\n"
                             "les performances. Réglez à zéro pour ne pas\n"
                             "effectuer de contrôles. Cette valeur n'est\n"
                             "utilisée que par l'hôte.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_MIN:
            snprintf(s, len,
                     "Nombre d'images de latence des entrées que le jeu \n"
                     "en réseau doit utiliser pour masquer la latence du réseau. \n"
                     " \n"
                     "Lors du jeu en réseau, cette option retarde les entrées \n"
                     "locales, de sorte que l'image en cours d'exécution \n"
                     "soit plus proche des images reçues par le réseau. \n"
                     "Cela réduit les variations de la latence et rend \n"
                     "le jeu en réseau moins gourmand en ressources processeur, \n"
                     "mais au prix d'un retard des entrées notable. \n");
            break;
        case MENU_ENUM_LABEL_NETPLAY_INPUT_LATENCY_FRAMES_RANGE:
            snprintf(s, len,
                     "Plage d'images de latence des entrées pouvant \n"
                     "être utilisée pour masquer la latence \n"
                     "du réseau. \n"
                     "\n"
                     "Si cette option est activée, le jeu en réseau ajustera \n"
                     "le nombre d'images de latence d'entrée de manière dynamique \n"
                     "pour équilibrer le temps processeur, la latence d'entrée \n"
                     "et la latence du réseau. Cela réduit les variations de la latence \n"
                     "et rend le jeu en réseau moins gourmand en ressources processeur, \n"
                     "mais au prix d'un retard des entrées imprévisible. \n");
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
                             "pour le jeu en réseau. Un serveur qui est situé \n"
                             "plus près de vous peut avoir moins de latence. \n");
            break;
        case MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES:
            snprintf(s, len,
                     "Nombre d'images max en mémoire tampon. Cette option \n"
                             "peut indiquer au pilote vidéo d'utiliser un mode de \n"
                             "mise en mémoire tampon vidéo spécifique. \n"
                             " \n"
                             "Mise en mémoire tampon unique - 1\n"
                             "Double mise en mémoire tampon - 2\n"
                             "Triple mise en mémoire tampon - 3\n"
                             " \n"
                             "La définition du bon mode de mise en mémoire tampon \n"
                             "peut avoir un impact important sur la latence.");
            break;
        case MENU_ENUM_LABEL_VIDEO_SMOOTH:
            snprintf(s, len,
                     "Lisse l'image avec le filtrage bilinéaire. \n"
                             "Devrait être désactivé si vous utilisez des shaders.");
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
                     "Déconnecte une connexion de jeu en réseau active.");
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
                     "Dossier des Surimpressions. \n"
                             " \n"
                             "Définit un dossier dans lequel les surimpressions \n"
                             "seront conservées pour un accès facile.");
            break;
#ifdef HAVE_VIDEO_LAYOUT
        case MENU_ENUM_LABEL_VIDEO_LAYOUT_DIRECTORY:
            snprintf(s, len,
                     "Dossier des Dispositions d'affichage. \n"
                             " \n"
                             "Définit un dossier dans lequel les dispositions d'affichage \n"
                             "seront conservées pour un accès facile.");
        break;
#endif
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
                             "Il sera utilisé pour jouer à des jeux en ligne.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT:
            snprintf(s, len,
                     "Port de l'adresse IP de l'hôte. \n"
                             "Peut être soit un port TCP soit UDP.");
            break;
        case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
            snprintf(s, len,
                     "Active ou désactive le mode spectateur pour \n"
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
                             "0 - Aucune \n"
                             "1 - Appuyer sur L + R + Y + Croix bas \n"
                             "en même temps. \n"
                             "2 - Appuyer sur L3 + R3 simultanément. \n"
                             "3 - Appuyer sur Start + Select simultanément.");
            break;
        case MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU:
            snprintf(s, len, "Autorise n'importe quel utilisateur à contrôler le menu. \n"
                    " \n"
                    "Si désactivé, seul l'utilisateur 1 peut contrôler le menu.");
            break;
        case MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE:
            snprintf(s, len,
                     "Active la détection automatique des touches.\n"
                             " \n"
                             "Cette option va tenter de configurer automatiquement \n"
                             "les manettes, style Plug-and-Play.");
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
                             "Maintenir le turbo tout en appuyant sur une \n"
                             "autre touche permet à la touche d'entrer dans \n"
                             "un mode turbo où l'état du bouton est modulé \n"
                             "avec un signal périodique. \n"
                             " \n"
                             "La modulation s'arrête lorsque la touche \n"
                             "elle-même (pas la touche turbo) est relâchée.");
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
                             "Le rembobinage doit être activé.");
            break;
        case MENU_ENUM_LABEL_EXIT_EMULATOR:
            snprintf(s, len,
                     "Touche pour quitter RetroArch proprement."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
                            "\nLe fermer de manière dure \n"
                            "(SIGKILL, etc.) quittera RetroArch\n"
                            "sanssauvegarder la RAM, etc. Sur\n"
                            "les Unix-likes, SIGINT/SIGTERM permet\n"
                            "une désinitialisation propre."
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
                     "Nombre de fois que le cheat sera appliqué.\nUtiliser avec les deux autres options d'itération pour affecter de grandes zones de mémoire.");
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
                             "Assigné normalement, cependant, si un axe analogique réel \n"
                             "est lié il peut être lu comme un véritable axe analogique.\n"
                             " \n"
                             "L'axe X positif est vers la droite. \n"
                             "L'axe Y positif est vers le bas.");
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
                     "Lorsque cette option est \"Off\", la sortie MIDI sera désactivée.\n"
                     "Le nom du périphérique peut aussi être entré manuellement.");
            break;
        case MENU_ENUM_LABEL_MIDI_OUTPUT:
            snprintf(s, len,
                     "Définit le périphérique de sortie (spécifique au pilote).\n"
                     "Lorsque cette option est \"Off\", la sortie MIDI sera désactivée.\n"
                     "Le nom du périphérique peut aussi être entré manuellement.\n"
                     " \n"
                     "Lorsque la sortie MIDI est activée et que le cœur et le jeu / l'application prennent en charge la sortie MIDI,\n"
                     "tout ou partie des sons (dépend du jeu/de l'application) sera généré par le périphérique MIDI.\n"
                     "Dans le cas du pilote MIDI \"null\" cela signifie que ces sons ne seront pas audibles.");
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

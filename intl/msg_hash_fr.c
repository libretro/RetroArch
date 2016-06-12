/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../msg_hash.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Winvalid-source-encoding"
#endif

 /* IMPORTANT:
  * For non-english characters to work without proper unicode support,
  * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
  * If you save this file as UTF-8, you'll break non-english characters
  * (e.g. German "Umlauts" and Portugese diacritics).
 */
/* DO NOT REMOVE THIS. If it causes build failure, it's because you saved the file as UTF-8. Read the above comment. */
extern const char force_iso_8859_1[sizeof("äÄöÖßüÜ")==7+1 ? 1 : -1];

const char *msg_hash_to_str_fr(uint32_t hash)
{
   switch (hash)
   {
      case MSG_PROGRAM:
         return "RetroArch";
      case MSG_MOVIE_RECORD_STOPPED:
         return "Arrêt de l'enregistrement vidéo.";
      case MSG_MOVIE_PLAYBACK_ENDED:
         return "Fin de la lecture vidéo";
      case MSG_AUTOSAVE_FAILED:
         return "Impossible d'activer l'enregistrement automatique.";
      case MSG_NETPLAY_FAILED_MOVIE_PLAYBACK_HAS_STARTED:
         return "Lecture en cours. Impossible d'activer le jeu en réseau.";
      case MSG_NETPLAY_FAILED:
         return "Échec de l'initialisation du jeu en réseau";
      case MSG_LIBRETRO_ABI_BREAK:
         return "est compilé avec une version différente de la bibliothèque libretro actuelle.";
      case MSG_REWIND_INIT_FAILED_NO_SAVESTATES:
         return "L'implémentation ne supporte pas la sauvegarde d'état. Impossible d'activer le retour rapide.";
      case MSG_REWIND_INIT_FAILED_THREADED_AUDIO:
         return "L'implémentation utilise audio thread. Impossible d'activer le retour rapide.";
      case MSG_REWIND_INIT_FAILED:
         return "Échec de l'initialisation du tampon pour le retour rapide. Cette fonctionnalité sera désactivée.";
      case MSG_REWIND_INIT:
         return "Initialisation du tampon pour le retour rapide avec une taille";
      case MSG_CUSTOM_TIMING_GIVEN:
         return "Temps personnalisé attribué";
      case MSG_VIEWPORT_SIZE_CALCULATION_FAILED:
         return "Échec du calcul de la taille du visuel ! Utilisation des données brutes. Cela ne fonctionnera probablement pas bien ...";
      case MSG_HW_RENDERED_MUST_USE_POSTSHADED_RECORDING:
         return "Le core Libretro utilise le rendu matériel. Obligation d'utiliser également l'enregistrement post-shaded.";
      case MSG_RECORDING_TO:
         return "Enregistrement vers";
      case MSG_DETECTED_VIEWPORT_OF:
         return "Détection du visuel";
      case MSG_TAKING_SCREENSHOT:
         return "Réalisation d'une copie d'écran.";
      case MSG_FAILED_TO_TAKE_SCREENSHOT:
         return "Échec de la copie d'écran.";
      case MSG_FAILED_TO_START_RECORDING:
         return "Échec de l'activation de l'enregistrement.";
      case MSG_RECORDING_TERMINATED_DUE_TO_RESIZE:
         return "Enregistrement interrompu à cause du redimensionnement.";
      case MSG_USING_LIBRETRO_DUMMY_CORE_RECORDING_SKIPPED:
         return "Utilisation d'un core libretro simple. Ignore l'enregistrement.";
      case MSG_UNKNOWN:
         return "Inconnu";
      case MSG_LOADING_CONTENT_FILE:
         return "Chargement du contenu";
      case MSG_RECEIVED:
         return "Reçu";
      case MSG_UNRECOGNIZED_COMMAND:
         return "Commande non reconnue";
      case MSG_SENDING_COMMAND:
         return "Envoi commande";
      case MSG_GOT_INVALID_DISK_INDEX:
         return "Index du disque invalide.";
      case MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY:
         return "Impossible d'éjecter le disque du lecteur.";
      case MSG_REMOVED_DISK_FROM_TRAY:
         return "Éjection du disque du lecteur.";
      case MSG_VIRTUAL_DISK_TRAY:
         return "Lecteur de disque virtuel.";
      case MSG_FAILED_TO:
         return "Échec de";
      case MSG_TO:
         return "de";
      case MSG_SAVING_RAM_TYPE:
         return "Sauvegarde du type de RAM";
      case MSG_SAVING_STATE:
         return "Sauvegarde savestate";
      case MSG_LOADING_STATE:
         return "Chargement savestate";
      case MSG_FAILED_TO_LOAD_MOVIE_FILE:
         return "Échec du chargement du fichier vidéo";
      case MSG_FAILED_TO_LOAD_CONTENT:
         return "Échec du chargement du fichier";
      case MSG_COULD_NOT_READ_CONTENT_FILE:
         return "Impossible de lire le contenu du fichier";
      case MSG_GRAB_MOUSE_STATE:
         return "Capture la souris";
      case MSG_PAUSED:
         return "Pause.";
      case MSG_UNPAUSED:
         return "Relancé.";
      case MSG_FAILED_TO_LOAD_OVERLAY:
         return "Impossible de charger l'overlay.";
      case MSG_FAILED_TO_UNMUTE_AUDIO:
         return "Impossible de remettre le son.";
      case MSG_AUDIO_MUTED:
         return "Son coupé.";
      case MSG_AUDIO_UNMUTED:
         return "Remise du son.";
      case MSG_RESET:
         return "Reset";
      case MSG_FAILED_TO_LOAD_STATE:
         return "Impossible de charger la savestate à partir de";
      case MSG_FAILED_TO_SAVE_STATE_TO:
         return "Impossible de sauvegarder la savestate vers";
      case MSG_FAILED_TO_LOAD_UNDO:
         return "Aucun savestate de retour arrière trouvé";
      case MSG_FAILED_TO_SAVE_UNDO:
         return "Impossible de sauvegarder les informations de savestate de retour arrière";
      case MSG_FAILED_TO_SAVE_SRAM:
         return "Impossible de sauvegarder la SRAM";
      case MSG_STATE_SIZE:
         return "Taille savestate";
      case MSG_FOUND_SHADER:
         return "Shader trouvé";
      case MSG_SRAM_WILL_NOT_BE_SAVED:
         return "SRAM ne sera pas sauvegardée.";
      case MSG_BLOCKING_SRAM_OVERWRITE:
         return "Bloque l'écrasement de la SRAM";
      case MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES:
         return "Le core ne supporte pas les savestates.";
      case MSG_SAVED_STATE_TO_SLOT:
         return "Savestate vers slot";
      case MSG_SAVED_SUCCESSFULLY_TO:
         return "Sauvegarde réussie vers";
      case MSG_BYTES:
         return "octets";
      case MSG_CONFIG_DIRECTORY_NOT_SET:
         return "Répertoire de configuration non défini. Impossible de sauvegarder le nouveau fichier.";
      case MSG_SKIPPING_SRAM_LOAD:
         return "Ignore le chargement de la SRAM.";
      case MSG_APPENDED_DISK:
         return "Disque fusionné";
      case MSG_STARTING_MOVIE_PLAYBACK:
         return "Démarrage de la lecture vidéo.";
      case MSG_FAILED_TO_REMOVE_TEMPORARY_FILE:
         return "Impossible de supprimer le fichier temporaire";
      case MSG_REMOVING_TEMPORARY_CONTENT_FILE:
         return "Suppression du fichier temporaire";
      case MSG_LOADED_STATE_FROM_SLOT:
         return "Chargement du savestate à partir du slot";
      case MSG_COULD_NOT_PROCESS_ZIP_FILE:
         return "Impossible de traiter le fichier ZIP.";
      case MSG_SCANNING_OF_DIRECTORY_FINISHED:
         return "Analyse des dossiers terminée";
      case MSG_SCANNING:
         return "Analyse";
      case MSG_REDIRECTING_CHEATFILE_TO:
         return "Redirection du fichier triche vers";
      case MSG_REDIRECTING_SAVEFILE_TO:
         return "Redirection de la sauvegarde vers";
      case MSG_REDIRECTING_SAVESTATE_TO:
         return "Redirection de la savestate vers";
      case MSG_SHADER:
         return "Shader";
      case MSG_APPLYING_SHADER:
         return "Application du shader";
      case MSG_FAILED_TO_APPLY_SHADER:
         return "Impossible d'appliquer le shader.";
      case MSG_STARTING_MOVIE_RECORD_TO:
         return "Démarrage de l'enregistrement vidéo vers";
      case MSG_FAILED_TO_START_MOVIE_RECORD:
         return "Impossible de démarrer l'enregistrement vidéo.";
      case MSG_STATE_SLOT:
         return "State slot";
      case MSG_RESTARTING_RECORDING_DUE_TO_DRIVER_REINIT:
         return "Redémarrage de l'enregistrement à cause de la réinitialisation du pilote.";
      case MSG_SLOW_MOTION:
         return "Ralenti.";
      case MSG_SLOW_MOTION_REWIND:
         return "Rembobinage ralenti.";
      case MSG_REWINDING:
         return "Rembobinage.";
      case MSG_REWIND_REACHED_END:
         return "Atteinte de la fin du tampon de rembobinage.";
      case MSG_CHEEVOS_HARDCORE_MODE_ENABLE:
         return "Mode matériel activé : savestate et rembobinage sont désactivés.";
      case MSG_TASK_FAILED:
         return "Échec";
      case MSG_DOWNLOADING:
         return "Téléchargement";
      case MSG_EXTRACTING:
         return "Extraction";
      default:
         break;
   }

   return "null";
}

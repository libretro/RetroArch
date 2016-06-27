/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifndef __INTL_FRENCH_H
#define __INTL_FRENCH_H

/* IMPORTANT:
 * For non-english characters to work without proper unicode support,
 * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
 * If you save this file as UTF-8, you'll break non-english characters
 * (e.g. German "Umlauts" and Portugese diacritics).
 */
/* DO NOT REMOVE THIS. If it causes build failure, it's because you saved the file as UTF-8. Read the above comment. */
extern const char force_iso_8859_1[sizeof("àèéìòù")==6+1 ? 1 : -1];

#define RETRO_LBL_JOYPAD_B "Bouton RetroPad B"
#define RETRO_LBL_JOYPAD_Y "Bouton RetroPad Y"
#define RETRO_LBL_JOYPAD_SELECT "Bouton RetroPad Select"
#define RETRO_LBL_JOYPAD_START "Bouton RetroPad Start"
#define RETRO_LBL_JOYPAD_UP "RetroPad D-Pad Up"
#define RETRO_LBL_JOYPAD_DOWN "Croix directionnelle RetroPad Haut"
#define RETRO_LBL_JOYPAD_LEFT "Croix directionnelle RetroPad Gauche"
#define RETRO_LBL_JOYPAD_RIGHT "Croix directionnelle RetroPad Droite"
#define RETRO_LBL_JOYPAD_A "Bouton RetroPad A"
#define RETRO_LBL_JOYPAD_X "Bouton RetroPad X"
#define RETRO_LBL_JOYPAD_L "Bouton RetroPad L"
#define RETRO_LBL_JOYPAD_R "Bouton RetroPad R"
#define RETRO_LBL_JOYPAD_L2 "Bouton RetroPad L2"
#define RETRO_LBL_JOYPAD_R2 "Bouton RetroPad R2"
#define RETRO_LBL_JOYPAD_L3 "Bouton RetroPad L3"
#define RETRO_LBL_JOYPAD_R3 "Bouton RetroPad R3"
#define RETRO_LBL_TURBO_ENABLE "Turbo Activé"
#define RETRO_LBL_ANALOG_LEFT_X "Analogue Gauche X"
#define RETRO_LBL_ANALOG_LEFT_Y "Analogue Gauche Y"
#define RETRO_LBL_ANALOG_RIGHT_X "Analogue Droite X"
#define RETRO_LBL_ANALOG_RIGHT_Y "Analogue Droite Y"
#define RETRO_LBL_ANALOG_LEFT_X_PLUS "Analogue Gauche X +"
#define RETRO_LBL_ANALOG_LEFT_X_MINUS "Analogue Gauche X -"
#define RETRO_LBL_ANALOG_LEFT_Y_PLUS "Analogue Gauche Y +"
#define RETRO_LBL_ANALOG_LEFT_Y_MINUS "Analogue Gauche Y -"
#define RETRO_LBL_ANALOG_RIGHT_X_PLUS "Analogue Droite X +"
#define RETRO_LBL_ANALOG_RIGHT_X_MINUS "Analogue Droite X -"
#define RETRO_LBL_ANALOG_RIGHT_Y_PLUS "Analogue Droite Y +"
#define RETRO_LBL_ANALOG_RIGHT_Y_MINUS "Analogue Droite Y -"
#define RETRO_LBL_FAST_FORWARD_KEY "Avance Rapide"
#define RETRO_LBL_FAST_FORWARD_HOLD_KEY "Avance Rapide Appui Maintenu"
#define RETRO_LBL_LOAD_STATE_KEY "Charger une savestate"
#define RETRO_LBL_SAVE_STATE_KEY "Sauvegarder une savestate"
#define RETRO_LBL_FULLSCREEN_TOGGLE_KEY "Mode plein écran"
#define RETRO_LBL_QUIT_KEY "Quitter"
#define RETRO_LBL_STATE_SLOT_PLUS "État Slot Suivant"
#define RETRO_LBL_STATE_SLOT_MINUS "État Slot Antérieur"
#define RETRO_LBL_REWIND "Rembobinage"
#define RETRO_LBL_MOVIE_RECORD_TOGGLE "Commutateur enregistrement vidéo"
#define RETRO_LBL_PAUSE_TOGGLE "Pause"
#define RETRO_LBL_FRAMEADVANCE "Défiler image"
#define RETRO_LBL_RESET "Reset"
#define RETRO_LBL_SHADER_NEXT "Prochain Shader"
#define RETRO_LBL_SHADER_PREV "Précédent Shader"
#define RETRO_LBL_CHEAT_INDEX_PLUS "Index Cheat Suivant"
#define RETRO_LBL_CHEAT_INDEX_MINUS "Index Cheat Antérieur"
#define RETRO_LBL_CHEAT_TOGGLE "Commutateur Mode Triche"
#define RETRO_LBL_SCREENSHOT "Capture d'écran"
#define RETRO_LBL_MUTE "Couper le son"
#define RETRO_LBL_OSK "Active le clavier visuel"
#define RETRO_LBL_NETPLAY_FLIP "Inversement des joueurs Netplay"
#define RETRO_LBL_SLOWMOTION "Ralenti"
#define RETRO_LBL_ENABLE_HOTKEY "Active raccourci clavier"
#define RETRO_LBL_VOLUME_UP "Augmenter le volume"
#define RETRO_LBL_VOLUME_DOWN "Diminuer le volume"
#define RETRO_LBL_OVERLAY_NEXT "Prochain Overlay"
#define RETRO_LBL_DISK_EJECT_TOGGLE "Commutateur éjecter le disque"
#define RETRO_LBL_DISK_NEXT "Prochain Changement Disque"
#define RETRO_LBL_DISK_PREV "Précédent Changement Disque"
#define RETRO_LBL_GRAB_MOUSE_TOGGLE "Commutateur capturer la souris"
#define RETRO_LBL_MENU_TOGGLE "Commutateur Menu"

#endif

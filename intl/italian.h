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

#ifndef __INTL_ITALIAN_H
#define __INTL_ITALIAN_H

/* IMPORTANT:
 * For non-english characters to work without proper unicode support,
 * we need this file to be encoded in ISO 8859-1 (Latin1), not UTF-8.
 * If you save this file as UTF-8, you'll break non-english characters
 * (e.g. German "Umlauts" and Portugese diacritics).
 */
/* DO NOT REMOVE THIS. If it causes build failure, it's because you saved the file as UTF-8. Read the above comment. */
extern const char force_iso_8859_1[sizeof("àèéìòù")==6+1 ? 1 : -1];

#define RETRO_LBL_JOYPAD_B "Tasto B RetroPad"
#define RETRO_LBL_JOYPAD_Y "Tasto Y RetroPad"
#define RETRO_LBL_JOYPAD_SELECT "Tasto Select RetroPad"
#define RETRO_LBL_JOYPAD_START "Tasto Start RetroPad"
#define RETRO_LBL_JOYPAD_UP "Croce direzionale Sù RetroPad"
#define RETRO_LBL_JOYPAD_DOWN "Croce direzionale Giù RetroPad"
#define RETRO_LBL_JOYPAD_LEFT "Croce direzionale Sinistra RetroPad"
#define RETRO_LBL_JOYPAD_RIGHT "Croce direzionale Destra RetroPad"
#define RETRO_LBL_JOYPAD_A "Tasto A RetroPad"
#define RETRO_LBL_JOYPAD_X "Tasto X RetroPad"
#define RETRO_LBL_JOYPAD_L "Tasto L RetroPad"
#define RETRO_LBL_JOYPAD_R "Tasto R RetroPad"
#define RETRO_LBL_JOYPAD_L2 "Tasto L2 RetroPad"
#define RETRO_LBL_JOYPAD_R2 "Tasto R2 RetroPad"
#define RETRO_LBL_JOYPAD_L3 "Tasto L3 RetroPad"
#define RETRO_LBL_JOYPAD_R3 "Tasto R3 RetroPad"
#define RETRO_LBL_TURBO_ENABLE "Abilita Turbo"
#define RETRO_LBL_ANALOG_LEFT_X "Analogico Sinistro X"
#define RETRO_LBL_ANALOG_LEFT_Y "Analogico Sinistro Y"
#define RETRO_LBL_ANALOG_RIGHT_X "Analogico Destro X"
#define RETRO_LBL_ANALOG_RIGHT_Y "Analogico Destro Y"
#define RETRO_LBL_ANALOG_LEFT_X_PLUS "Analogico Sinistro X +"
#define RETRO_LBL_ANALOG_LEFT_X_MINUS "Analogico Sinistro X -"
#define RETRO_LBL_ANALOG_LEFT_Y_PLUS "Analogico Sinistro Y +"
#define RETRO_LBL_ANALOG_LEFT_Y_MINUS "Analogico Sinistro Y -"
#define RETRO_LBL_ANALOG_RIGHT_X_PLUS "Analogico Destro X +"
#define RETRO_LBL_ANALOG_RIGHT_X_MINUS "Analogico Destro X -"
#define RETRO_LBL_ANALOG_RIGHT_Y_PLUS "Analogico Destro Y +"
#define RETRO_LBL_ANALOG_RIGHT_Y_MINUS "Analogico Destro Y -"
#define RETRO_LBL_FAST_FORWARD_KEY "Avanti Veloce"
#define RETRO_LBL_FAST_FORWARD_HOLD_KEY "Tieni Premuto Avanti Veloce"
#define RETRO_LBL_LOAD_STATE_KEY "Carica Stato"
#define RETRO_LBL_SAVE_STATE_KEY "Salva Stato"
#define RETRO_LBL_FULLSCREEN_TOGGLE_KEY "Interrutore Schermo Intero"
#define RETRO_LBL_QUIT_KEY "Tasto Esci"
#define RETRO_LBL_STATE_SLOT_PLUS "Stato Slot Successivo"
#define RETRO_LBL_STATE_SLOT_MINUS "Stato Slot Precedente"
#define RETRO_LBL_REWIND "Riavvolgi"
#define RETRO_LBL_MOVIE_RECORD_TOGGLE "Interruttore Registrazione Video"
#define RETRO_LBL_PAUSE_TOGGLE "Pausa"
#define RETRO_LBL_FRAMEADVANCE "Avanza Fotogramma"
#define RETRO_LBL_RESET "Azzera"
#define RETRO_LBL_SHADER_NEXT "Prossimo Shader"
#define RETRO_LBL_SHADER_PREV "Shader Precedente"
#define RETRO_LBL_CHEAT_INDEX_PLUS "Indice dei Trucchi Successivo"
#define RETRO_LBL_CHEAT_INDEX_MINUS "Indice dei Trucchi Anteriore"
#define RETRO_LBL_CHEAT_TOGGLE "Interruttore Trucchi"
#define RETRO_LBL_SCREENSHOT "Cattura Schermata"
#define RETRO_LBL_MUTE "Silenzia Audio"
#define RETRO_LBL_OSK "Abilita Tastiera a Schermo"
#define RETRO_LBL_NETPLAY_FLIP "Cambia Utenti Netplay"
#define RETRO_LBL_SLOWMOTION "Rallentatore"
#define RETRO_LBL_ENABLE_HOTKEY "Abilita Tasti Rapidi"
#define RETRO_LBL_VOLUME_UP "Aumenta Volume"
#define RETRO_LBL_VOLUME_DOWN "Abbassa Volume"
#define RETRO_LBL_OVERLAY_NEXT "Overlay Successivo"
#define RETRO_LBL_DISK_EJECT_TOGGLE "Interruttore Espelli Disco"
#define RETRO_LBL_DISK_NEXT "Cambia Disco Successivo"
#define RETRO_LBL_DISK_PREV "Cambia Disco Precedente"
#define RETRO_LBL_GRAB_MOUSE_TOGGLE "Attiva presa mouse"
#define RETRO_LBL_MENU_TOGGLE "Menù a comparsa"

#endif

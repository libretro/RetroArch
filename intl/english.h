/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#ifndef __INTL_ENGLISH_H
#define __INTL_ENGLISH_H

#define RETRO_FRONTEND "RetroArch"

#define RETRO_LBL_JOYPAD_B "RetroPad B Button"
#define RETRO_LBL_JOYPAD_Y "RetroPad Y Button"
#define RETRO_LBL_JOYPAD_SELECT "RetroPad Select Button"
#define RETRO_LBL_JOYPAD_START "RetroPad Start Button"
#define RETRO_LBL_JOYPAD_UP "RetroPad D-Pad Up"
#define RETRO_LBL_JOYPAD_DOWN "RetroPad D-Pad Down"
#define RETRO_LBL_JOYPAD_LEFT "RetroPad D-Pad Left"
#define RETRO_LBL_JOYPAD_RIGHT "RetroPad D-Pad Right"
#define RETRO_LBL_JOYPAD_A "RetroPad A Button"
#define RETRO_LBL_JOYPAD_X "RetroPad X Button"
#define RETRO_LBL_JOYPAD_L "RetroPad L Button"
#define RETRO_LBL_JOYPAD_R "RetroPad R Button"
#define RETRO_LBL_JOYPAD_L2 "RetroPad L2 Button"
#define RETRO_LBL_JOYPAD_R2 "RetroPad R2 Button"
#define RETRO_LBL_JOYPAD_L3 "RetroPad L3 Button"
#define RETRO_LBL_JOYPAD_R3 "RetroPad R3 Button"
#define RETRO_LBL_TURBO_ENABLE "Turbo Enable"
#define RETRO_LBL_ANALOG_LEFT_X_PLUS "Left Analog X +"
#define RETRO_LBL_ANALOG_LEFT_X_MINUS "Left Analog X -"
#define RETRO_LBL_ANALOG_LEFT_Y_PLUS "Left Analog Y +"
#define RETRO_LBL_ANALOG_LEFT_Y_MINUS "Left Analog Y -"
#define RETRO_LBL_ANALOG_RIGHT_X_PLUS "Right Analog X +"
#define RETRO_LBL_ANALOG_RIGHT_X_MINUS "Right Analog X -"
#define RETRO_LBL_ANALOG_RIGHT_Y_PLUS "Right Analog Y +"
#define RETRO_LBL_ANALOG_RIGHT_Y_MINUS "Right Analog Y -"
#define RETRO_LBL_FAST_FORWARD_KEY "Fast Forward"
#define RETRO_LBL_FAST_FORWARD_HOLD_KEY "Fast Forward Hold"
#define RETRO_LBL_LOAD_STATE_KEY "Load State"
#define RETRO_LBL_SAVE_STATE_KEY "Save State"
#define RETRO_LBL_FULLSCREEN_TOGGLE_KEY "Fullscreen Toggle"
#define RETRO_LBL_QUIT_KEY "Quit Key"
#define RETRO_LBL_STATE_SLOT_PLUS "State Slot Plus"
#define RETRO_LBL_STATE_SLOT_MINUS "State Slot Minus"
#define RETRO_LBL_REWIND "Rewind"
#define RETRO_LBL_MOVIE_RECORD_TOGGLE "Movie Record Toggle"
#define RETRO_LBL_PAUSE_TOGGLE "Pause Toggle"
#define RETRO_LBL_FRAMEADVANCE "Frame Advance"
#define RETRO_LBL_RESET "Reset"
#define RETRO_LBL_SHADER_NEXT "Next Shader"
#define RETRO_LBL_SHADER_PREV "Previous Shader"
#define RETRO_LBL_CHEAT_INDEX_PLUS "Cheat Index Plus"
#define RETRO_LBL_CHEAT_INDEX_MINUS "Cheat Index Minus"
#define RETRO_LBL_CHEAT_TOGGLE "Cheat Toggle"
#define RETRO_LBL_SCREENSHOT "Screenshot"
#define RETRO_LBL_MUTE "Mute Audio"
#define RETRO_LBL_NETPLAY_FLIP "Netplay Flip Players"
#define RETRO_LBL_SLOWMOTION "Slowmotion"
#define RETRO_LBL_ENABLE_HOTKEY "Enable Hotkey"
#define RETRO_LBL_VOLUME_UP "Volume Up"
#define RETRO_LBL_VOLUME_DOWN "Volume Down"
#define RETRO_LBL_OVERLAY_NEXT "Next Overlay"
#define RETRO_LBL_DISK_EJECT_TOGGLE "Disk Eject Toggle"
#define RETRO_LBL_DISK_NEXT "Disk Swap Next"
#define RETRO_LBL_GRAB_MOUSE_TOGGLE "Grab mouse toggle"
#define RETRO_LBL_MENU_TOGGLE "Menu toggle"

#define TERM_STR "\n"

#define RETRO_MSG_INIT_RECORDING_SKIPPED "Using libretro dummy core. Skipping recording."
#define RETRO_MSG_INIT_RECORDING_FAILED "Failed to start recording."
#define RETRO_MSG_TAKE_SCREENSHOT "Taking screenshot."
#define RETRO_MSG_TAKE_SCREENSHOT_FAILED "Failed to take screenshot."
#define RETRO_MSG_TAKE_SCREENSHOT_ERROR "Cannot take screenshot. GPU rendering is used and read_viewport is not supported."
#define RETRO_MSG_AUDIO_WRITE_FAILED "Audio backend failed to write. Will continue without sound."
#define RETRO_MSG_MOVIE_STARTED_INIT_NETPLAY_FAILED "Movie playback has started. Cannot start netplay."
#define RETRO_MSG_INIT_NETPLAY_FAILED "Failed to initialize netplay."
#define RETRO_MSG_INIT_AUTOSAVE_FAILED "Could not initialize autosave."
#define RETRO_MSG_REWINDING "Rewinding."
#define RETRO_MSG_REWIND_REACHED_END "Reached end of rewind buffer."
#define RETRO_MSG_MOVIE_RECORD_STOPPING "Stopping movie record."
#define RETRO_MSG_MOVIE_PLAYBACK_ENDED "Movie playback ended."
#define RETRO_MSG_LIBRETRO_ABI_BREAK RETRO_FRONTEND " is compiled against a different version of libretro than this libretro implementation."
#define RETRO_MSG_RESETTING_CONTENT "Resetting content."
#define RETRO_MSG_REWIND_INIT "Initializing rewind buffer with size: "
#define RETRO_MSG_REWIND_INIT_FAILED "Failed to initialize rewind buffer. Rewinding will be disabled"
#define RETRO_MSG_REWIND_INIT_FAILED_NO_SAVESTATES "Implementation does not support save states. Cannot use rewind."
#define RETRO_MSG_REWIND_INIT_FAILED_THREADED_AUDIO "Implementation uses threaded audio. Cannot use rewind."

#define RETRO_LOG_INIT_RECORDING_SKIPPED RETRO_MSG_INIT_RECORDING_SKIPPED TERM_STR
#define RETRO_LOG_INIT_RECORDING_FAILED RETRO_MSG_INIT_RECORDING_FAILED TERM_STR
#define RETRO_LOG_TAKE_SCREENSHOT RETRO_MSG_TAKE_SCREENSHOT TERM_STR
#define RETRO_LOG_TAKE_SCREENSHOT_FAILED RETRO_MSG_TAKE_SCREENSHOT_FAILED TERM_STR
#define RETRO_LOG_TAKE_SCREENSHOT_ERROR RETRO_MSG_TAKE_SCREENSHOT_ERROR TERM_STR
#define RETRO_LOG_AUDIO_WRITE_FAILED RETRO_MSG_AUDIO_WRITE_FAILED TERM_STR
#define RETRO_LOG_MOVIE_STARTED_INIT_NETPLAY_FAILED RETRO_MSG_MOVIE_STARTED_INIT_NETPLAY_FAILED TERM_STR
#define RETRO_LOG_INIT_NETPLAY_FAILED RETRO_MSG_INIT_NETPLAY_FAILED TERM_STR
#define RETRO_LOG_INIT_AUTOSAVE_FAILED RETRO_MSG_INIT_AUTOSAVE_FAILED TERM_STR
#define RETRO_LOG_REWINDING RETRO_MSG_REWINDING TERM_STR
#define RETRO_LOG_REWIND_REACHED_END RETRO_MSG_REWIND_REACHED_END TERM_STR
#define RETRO_LOG_MOVIE_RECORD_STOPPING RETRO_MSG_MOVIE_RECORD_STOPPING TERM_STR
#define RETRO_LOG_MOVIE_PLAYBACK_ENDED RETRO_MSG_MOVIE_PLAYBACK_ENDED TERM_STR
#define RETRO_LOG_LIBRETRO_ABI_BREAK RETRO_MSG_LIBRETRO_ABI_BREAK RETRO_FRONTEND TERM_STR 
#define RETRO_LOG_RESETTING_CONTENT RETRO_MSG_RESETTING_CONTENT TERM_STR
#define RETRO_LOG_REWIND_INIT RETRO_MSG_REWIND_INIT TERM_STR
#define RETRO_LOG_REWIND_INIT_FAILED RETRO_MSG_REWIND_INIT_FAILED TERM_STR
#define RETRO_LOG_REWIND_INIT_FAILED_NO_SAVESTATES RETRO_MSG_REWIND_INIT_FAILED_NO_SAVESTATES TERM_STR
#define RETRO_LOG_REWIND_INIT_FAILED_THREADED_AUDIO RETRO_MSG_REWIND_INIT_FAILED_THREADED_AUDIO TERM_STR

#endif

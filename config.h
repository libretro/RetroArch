/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/// Config header for SSNES
//
//

#ifndef __CONFIG_H
#define __CONFIG_H

#include <stdbool.h>
#include "libsnes.hpp"
#include "driver.h"
#include <samplerate.h>



///////////////// Drivers
#define VIDEO_GL 0
////////////////////////
#define AUDIO_RSOUND 1
#define AUDIO_OSS 2
#define AUDIO_ALSA 3
////////////////////////

// Chooses which video and audio subsystem to use. Remember to update config.mk if you change these.
#define VIDEO_DRIVER VIDEO_GL
#define AUDIO_DRIVER AUDIO_RSOUND


////////////////
// Video
////////////////

// Windowed
static const float xscale = 4.0; // Real x res = 296 * xscale
static const float yscale = 4.0; // Real y res = 224 * yscale

// Fullscreen
static bool fullscreen = false; // To start in Fullscreen or not
static const unsigned fullscreen_x = 1280;
static const unsigned fullscreen_y = 720;

// Video VSYNC (recommended)
static const bool vsync = true;

// Smooths picture
static const bool video_smooth = true;

// On resize and fullscreen, rendering area will stay 4:3
static const bool force_aspect = true; 

/////////// Video filters
#define FILTER_NONE 0
#define FILTER_HQ2X 1
#define FILTER_HQ4X 2
////////////////////////

// If you change this to something other than the HQ filters, make sure that you build the filter module in config.mk.
#define VIDEO_FILTER FILTER_NONE


////////////////
// Audio
////////////////

// Output samplerate
static const unsigned out_rate = 48000; 

// Input samplerate from libSNES. 
// Lower this (slightly) if you are experiencing frequent audio dropouts and vsync is enabled.
static const unsigned in_rate = 31950; 

// Audio device (e.g. hw:0,0 or /dev/audio). If NULL, will use defaults.
static const char* audio_device = NULL;

// Desired audio latency in milliseconds. Might not be honored if driver can't provide given latency.
static const int out_latency = 64;

// Defines the quality (and cpu reqirements) of samplerate conversion.
#define SAMPLERATE_QUALITY SRC_SINC_FASTEST




////////////////////
// Keybinds, Joypad
////////////////////

// To figure out which joypad buttons to use, check jstest or similar.

static const struct snes_keybind snes_keybinds[] = {
   // SNES button                 |   keyboard key   |   joypad button   |
   { SNES_DEVICE_ID_JOYPAD_A,             'X',                 1 },
   { SNES_DEVICE_ID_JOYPAD_B,             'Z',                 0 },
   { SNES_DEVICE_ID_JOYPAD_X,             'S',                 3 },
   { SNES_DEVICE_ID_JOYPAD_Y,             'A',                 2 },
   { SNES_DEVICE_ID_JOYPAD_L,             'Q',                 4 },
   { SNES_DEVICE_ID_JOYPAD_R,             'W',                 5 },
   { SNES_DEVICE_ID_JOYPAD_LEFT,    GLFW_KEY_LEFT,             12 },
   { SNES_DEVICE_ID_JOYPAD_RIGHT,   GLFW_KEY_RIGHT,            13 },
   { SNES_DEVICE_ID_JOYPAD_UP,      GLFW_KEY_UP,               10 },
   { SNES_DEVICE_ID_JOYPAD_DOWN,    GLFW_KEY_DOWN,             11 },
   { SNES_DEVICE_ID_JOYPAD_START,   GLFW_KEY_ENTER,            6 },
   { SNES_DEVICE_ID_JOYPAD_SELECT,  GLFW_KEY_RSHIFT,           14 },
   { -1 }
};

///// Save state
#define SAVE_STATE_KEY GLFW_KEY_F2
///// Load state
#define LOAD_STATE_KEY GLFW_KEY_F4

//// Toggles between fullscreen and windowed mode.
#define TOGGLE_FULLSCREEN 'F'



#endif


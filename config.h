/// Config header for SSNES
//
//

#ifndef __CONFIG_H
#define __CONFIG_H

#include <GL/glfw.h>
#include "libsnes.hpp"
#include <stdbool.h>

struct snes_keybind
{
   int id;
   int key;
   int joykey;
};

static const bool force_aspect = true; // On resize and fullscreen, rendering area will stay 8:7

// Windowed
static const float xscale = 5.0; // Real x res = 256 * xscale
static const float yscale = 5.0; // Real y res = 224 * yscale

// Fullscreen
static const bool fullscreen = false;
static const unsigned fullscreen_x = 1920;
static const unsigned fullscreen_y = 1200;

// Video VSYNC (recommended)
static const bool vsync = true;

// Keybinds
static const struct snes_keybind snes_keybinds[] = {
   { SNES_DEVICE_ID_JOYPAD_A, 'X', 1 },
   { SNES_DEVICE_ID_JOYPAD_B, 'Z', 0 },
   { SNES_DEVICE_ID_JOYPAD_X, 'S', 3 },
   { SNES_DEVICE_ID_JOYPAD_Y, 'A', 2 },
   { SNES_DEVICE_ID_JOYPAD_L, 'Q', 4 },
   { SNES_DEVICE_ID_JOYPAD_R, 'W', 5 },
   { SNES_DEVICE_ID_JOYPAD_LEFT, GLFW_KEY_LEFT, 12 },
   { SNES_DEVICE_ID_JOYPAD_RIGHT, GLFW_KEY_RIGHT, 13 },
   { SNES_DEVICE_ID_JOYPAD_UP, GLFW_KEY_UP, 10 },
   { SNES_DEVICE_ID_JOYPAD_DOWN, GLFW_KEY_DOWN, 11 },
   { SNES_DEVICE_ID_JOYPAD_START, GLFW_KEY_ENTER, 6 },
   { SNES_DEVICE_ID_JOYPAD_SELECT, GLFW_KEY_RSHIFT, 14 },
   { -1 }
};

#endif


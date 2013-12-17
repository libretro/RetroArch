/* Copyright (C) 2010-2013 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this libretro API header (libretro.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef LIBRETRO_H__
#define LIBRETRO_H__

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

// Hack applied for MSVC when compiling in C89 mode as it isn't C99 compliant.
#ifdef __cplusplus
extern "C" {
#else
#if defined(_MSC_VER) && !defined(SN_TARGET_PS3) && !defined(__cplusplus)
#define bool unsigned char
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif
#endif

// Used for checking API/ABI mismatches that can break libretro implementations.
// It is not incremented for compatible changes to the API.
#define RETRO_API_VERSION         1

// Libretro's fundamental device abstractions.
#define RETRO_DEVICE_MASK         0xff
#define RETRO_DEVICE_NONE         0

// The JOYPAD is called RetroPad. It is essentially a Super Nintendo controller,
// but with additional L2/R2/L3/R3 buttons, similar to a PS1 DualShock.
#define RETRO_DEVICE_JOYPAD       1

// The mouse is a simple mouse, similar to Super Nintendo's mouse.
// X and Y coordinates are reported relatively to last poll (poll callback).
// It is up to the libretro implementation to keep track of where the mouse pointer is supposed to be on the screen.
// The frontend must make sure not to interfere with its own hardware mouse pointer.
#define RETRO_DEVICE_MOUSE        2

// KEYBOARD device lets one poll for raw key pressed.
// It is poll based, so input callback will return with the current pressed state.
#define RETRO_DEVICE_KEYBOARD     3

// Lightgun X/Y coordinates are reported relatively to last poll, similar to mouse.
#define RETRO_DEVICE_LIGHTGUN     4

// The ANALOG device is an extension to JOYPAD (RetroPad).
// Similar to DualShock it adds two analog sticks.
// This is treated as a separate device type as it returns values in the full analog range
// of [-0x8000, 0x7fff]. Positive X axis is right. Positive Y axis is down.
// Only use ANALOG type when polling for analog values of the axes.
#define RETRO_DEVICE_ANALOG       5

// Abstracts the concept of a pointing mechanism, e.g. touch.
// This allows libretro to query in absolute coordinates where on the screen a mouse (or something similar) is being placed.
// For a touch centric device, coordinates reported are the coordinates of the press.
//
// Coordinates in X and Y are reported as:
// [-0x7fff, 0x7fff]: -0x7fff corresponds to the far left/top of the screen,
// and 0x7fff corresponds to the far right/bottom of the screen.
// The "screen" is here defined as area that is passed to the frontend and later displayed on the monitor.
// The frontend is free to scale/resize this screen as it sees fit, however,
// (X, Y) = (-0x7fff, -0x7fff) will correspond to the top-left pixel of the game image, etc.
//
// To check if the pointer coordinates are valid (e.g. a touch display actually being touched),
// PRESSED returns 1 or 0.
// If using a mouse, PRESSED will usually correspond to the left mouse button.
// PRESSED will only return 1 if the pointer is inside the game screen.
//
// For multi-touch, the index variable can be used to successively query more presses.
// If index = 0 returns true for _PRESSED, coordinates can be extracted
// with _X, _Y for index = 0. One can then query _PRESSED, _X, _Y with index = 1, and so on.
// Eventually _PRESSED will return false for an index. No further presses are registered at this point.
#define RETRO_DEVICE_POINTER      6

// FIXME: Document this.
#define RETRO_DEVICE_SENSOR_ACCELEROMETER 7

// These device types are specializations of the base types above.
// They should only be used in retro_set_controller_type() to inform libretro implementations
// about use of a very specific device type.
//
// In input state callback, however, only the base type should be used in the 'device' field.
#define RETRO_DEVICE_JOYPAD_MULTITAP        ((1 << 8) | RETRO_DEVICE_JOYPAD)
#define RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE   ((1 << 8) | RETRO_DEVICE_LIGHTGUN)
#define RETRO_DEVICE_LIGHTGUN_JUSTIFIER     ((2 << 8) | RETRO_DEVICE_LIGHTGUN)
#define RETRO_DEVICE_LIGHTGUN_JUSTIFIERS    ((3 << 8) | RETRO_DEVICE_LIGHTGUN)

// Buttons for the RetroPad (JOYPAD).
// The placement of these is equivalent to placements on the Super Nintendo controller.
// L2/R2/L3/R3 buttons correspond to the PS1 DualShock.
#define RETRO_DEVICE_ID_JOYPAD_B        0
#define RETRO_DEVICE_ID_JOYPAD_Y        1
#define RETRO_DEVICE_ID_JOYPAD_SELECT   2
#define RETRO_DEVICE_ID_JOYPAD_START    3
#define RETRO_DEVICE_ID_JOYPAD_UP       4
#define RETRO_DEVICE_ID_JOYPAD_DOWN     5
#define RETRO_DEVICE_ID_JOYPAD_LEFT     6
#define RETRO_DEVICE_ID_JOYPAD_RIGHT    7
#define RETRO_DEVICE_ID_JOYPAD_A        8
#define RETRO_DEVICE_ID_JOYPAD_X        9
#define RETRO_DEVICE_ID_JOYPAD_L       10
#define RETRO_DEVICE_ID_JOYPAD_R       11
#define RETRO_DEVICE_ID_JOYPAD_L2      12
#define RETRO_DEVICE_ID_JOYPAD_R2      13
#define RETRO_DEVICE_ID_JOYPAD_L3      14
#define RETRO_DEVICE_ID_JOYPAD_R3      15

// Index / Id values for ANALOG device.
#define RETRO_DEVICE_INDEX_ANALOG_LEFT   0
#define RETRO_DEVICE_INDEX_ANALOG_RIGHT  1
#define RETRO_DEVICE_ID_ANALOG_X         0
#define RETRO_DEVICE_ID_ANALOG_Y         1

// Id values for MOUSE.
#define RETRO_DEVICE_ID_MOUSE_X      0
#define RETRO_DEVICE_ID_MOUSE_Y      1
#define RETRO_DEVICE_ID_MOUSE_LEFT   2
#define RETRO_DEVICE_ID_MOUSE_RIGHT  3

// Id values for LIGHTGUN types.
#define RETRO_DEVICE_ID_LIGHTGUN_X        0
#define RETRO_DEVICE_ID_LIGHTGUN_Y        1
#define RETRO_DEVICE_ID_LIGHTGUN_TRIGGER  2
#define RETRO_DEVICE_ID_LIGHTGUN_CURSOR   3
#define RETRO_DEVICE_ID_LIGHTGUN_TURBO    4
#define RETRO_DEVICE_ID_LIGHTGUN_PAUSE    5
#define RETRO_DEVICE_ID_LIGHTGUN_START    6

// Id values for POINTER.
#define RETRO_DEVICE_ID_POINTER_X         0
#define RETRO_DEVICE_ID_POINTER_Y         1
#define RETRO_DEVICE_ID_POINTER_PRESSED   2

// Id values for SENSOR types.
#define RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_X      0
#define RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Y      1
#define RETRO_DEVICE_ID_SENSOR_ACCELEROMETER_Z      2

// Returned from retro_get_region().
#define RETRO_REGION_NTSC  0
#define RETRO_REGION_PAL   1

// Passed to retro_get_memory_data/size().
// If the memory type doesn't apply to the implementation NULL/0 can be returned.
#define RETRO_MEMORY_MASK        0xff

// Regular save ram. This ram is usually found on a game cartridge, backed up by a battery.
// If save game data is too complex for a single memory buffer,
// the SYSTEM_DIRECTORY environment callback can be used.
#define RETRO_MEMORY_SAVE_RAM    0

// Some games have a built-in clock to keep track of time.
// This memory is usually just a couple of bytes to keep track of time.
#define RETRO_MEMORY_RTC         1

// System ram lets a frontend peek into a game systems main RAM.
#define RETRO_MEMORY_SYSTEM_RAM  2

// Video ram lets a frontend peek into a game systems video RAM (VRAM).
#define RETRO_MEMORY_VIDEO_RAM   3

// Special memory types.
#define RETRO_MEMORY_SNES_BSX_RAM             ((1 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_BSX_PRAM            ((2 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM  ((3 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM  ((4 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_GAME_BOY_RAM        ((5 << 8) | RETRO_MEMORY_SAVE_RAM)
#define RETRO_MEMORY_SNES_GAME_BOY_RTC        ((6 << 8) | RETRO_MEMORY_RTC)

// Special game types passed into retro_load_game_special().
// Only used when multiple ROMs are required.
#define RETRO_GAME_TYPE_BSX             0x101
#define RETRO_GAME_TYPE_BSX_SLOTTED     0x102
#define RETRO_GAME_TYPE_SUFAMI_TURBO    0x103
#define RETRO_GAME_TYPE_SUPER_GAME_BOY  0x104

// Keysyms used for ID in input state callback when polling RETRO_KEYBOARD.
enum retro_key
{
   RETROK_UNKNOWN        = 0,
   RETROK_FIRST          = 0,
   RETROK_BACKSPACE      = 8,
   RETROK_TAB            = 9,
   RETROK_CLEAR          = 12,
   RETROK_RETURN         = 13,
   RETROK_PAUSE          = 19,
   RETROK_ESCAPE         = 27,
   RETROK_SPACE          = 32,
   RETROK_EXCLAIM        = 33,
   RETROK_QUOTEDBL       = 34,
   RETROK_HASH           = 35,
   RETROK_DOLLAR         = 36,
   RETROK_AMPERSAND      = 38,
   RETROK_QUOTE          = 39,
   RETROK_LEFTPAREN      = 40,
   RETROK_RIGHTPAREN     = 41,
   RETROK_ASTERISK       = 42,
   RETROK_PLUS           = 43,
   RETROK_COMMA          = 44,
   RETROK_MINUS          = 45,
   RETROK_PERIOD         = 46,
   RETROK_SLASH          = 47,
   RETROK_0              = 48,
   RETROK_1              = 49,
   RETROK_2              = 50,
   RETROK_3              = 51,
   RETROK_4              = 52,
   RETROK_5              = 53,
   RETROK_6              = 54,
   RETROK_7              = 55,
   RETROK_8              = 56,
   RETROK_9              = 57,
   RETROK_COLON          = 58,
   RETROK_SEMICOLON      = 59,
   RETROK_LESS           = 60,
   RETROK_EQUALS         = 61,
   RETROK_GREATER        = 62,
   RETROK_QUESTION       = 63,
   RETROK_AT             = 64,
   RETROK_LEFTBRACKET    = 91,
   RETROK_BACKSLASH      = 92,
   RETROK_RIGHTBRACKET   = 93,
   RETROK_CARET          = 94,
   RETROK_UNDERSCORE     = 95,
   RETROK_BACKQUOTE      = 96,
   RETROK_a              = 97,
   RETROK_b              = 98,
   RETROK_c              = 99,
   RETROK_d              = 100,
   RETROK_e              = 101,
   RETROK_f              = 102,
   RETROK_g              = 103,
   RETROK_h              = 104,
   RETROK_i              = 105,
   RETROK_j              = 106,
   RETROK_k              = 107,
   RETROK_l              = 108,
   RETROK_m              = 109,
   RETROK_n              = 110,
   RETROK_o              = 111,
   RETROK_p              = 112,
   RETROK_q              = 113,
   RETROK_r              = 114,
   RETROK_s              = 115,
   RETROK_t              = 116,
   RETROK_u              = 117,
   RETROK_v              = 118,
   RETROK_w              = 119,
   RETROK_x              = 120,
   RETROK_y              = 121,
   RETROK_z              = 122,
   RETROK_DELETE         = 127,

   RETROK_KP0            = 256,
   RETROK_KP1            = 257,
   RETROK_KP2            = 258,
   RETROK_KP3            = 259,
   RETROK_KP4            = 260,
   RETROK_KP5            = 261,
   RETROK_KP6            = 262,
   RETROK_KP7            = 263,
   RETROK_KP8            = 264,
   RETROK_KP9            = 265,
   RETROK_KP_PERIOD      = 266,
   RETROK_KP_DIVIDE      = 267,
   RETROK_KP_MULTIPLY    = 268,
   RETROK_KP_MINUS       = 269,
   RETROK_KP_PLUS        = 270,
   RETROK_KP_ENTER       = 271,
   RETROK_KP_EQUALS      = 272,

   RETROK_UP             = 273,
   RETROK_DOWN           = 274,
   RETROK_RIGHT          = 275,
   RETROK_LEFT           = 276,
   RETROK_INSERT         = 277,
   RETROK_HOME           = 278,
   RETROK_END            = 279,
   RETROK_PAGEUP         = 280,
   RETROK_PAGEDOWN       = 281,

   RETROK_F1             = 282,
   RETROK_F2             = 283,
   RETROK_F3             = 284,
   RETROK_F4             = 285,
   RETROK_F5             = 286,
   RETROK_F6             = 287,
   RETROK_F7             = 288,
   RETROK_F8             = 289,
   RETROK_F9             = 290,
   RETROK_F10            = 291,
   RETROK_F11            = 292,
   RETROK_F12            = 293,
   RETROK_F13            = 294,
   RETROK_F14            = 295,
   RETROK_F15            = 296,

   RETROK_NUMLOCK        = 300,
   RETROK_CAPSLOCK       = 301,
   RETROK_SCROLLOCK      = 302,
   RETROK_RSHIFT         = 303,
   RETROK_LSHIFT         = 304,
   RETROK_RCTRL          = 305,
   RETROK_LCTRL          = 306,
   RETROK_RALT           = 307,
   RETROK_LALT           = 308,
   RETROK_RMETA          = 309,
   RETROK_LMETA          = 310,
   RETROK_LSUPER         = 311,
   RETROK_RSUPER         = 312,
   RETROK_MODE           = 313,
   RETROK_COMPOSE        = 314,

   RETROK_HELP           = 315,
   RETROK_PRINT          = 316,
   RETROK_SYSREQ         = 317,
   RETROK_BREAK          = 318,
   RETROK_MENU           = 319,
   RETROK_POWER          = 320,
   RETROK_EURO           = 321,
   RETROK_UNDO           = 322,

   RETROK_LAST,

   RETROK_DUMMY          = INT_MAX // Ensure sizeof(enum) == sizeof(int)
};

enum retro_mod
{
   RETROKMOD_NONE       = 0x0000,

   RETROKMOD_SHIFT      = 0x01,
   RETROKMOD_CTRL       = 0x02,
   RETROKMOD_ALT        = 0x04,
   RETROKMOD_META       = 0x08,

   RETROKMOD_NUMLOCK    = 0x10,
   RETROKMOD_CAPSLOCK   = 0x20,
   RETROKMOD_SCROLLOCK  = 0x40,

   RETROKMOD_DUMMY = INT_MAX // Ensure sizeof(enum) == sizeof(int)
};

// If set, this call is not part of the public libretro API yet. It can change or be removed at any time.
#define RETRO_ENVIRONMENT_EXPERIMENTAL 0x10000
// Environment callback to be used internally in frontend.
#define RETRO_ENVIRONMENT_PRIVATE 0x20000

// Environment commands.
#define RETRO_ENVIRONMENT_SET_ROTATION  1  // const unsigned * --
                                           // Sets screen rotation of graphics.
                                           // Is only implemented if rotation can be accelerated by hardware.
                                           // Valid values are 0, 1, 2, 3, which rotates screen by 0, 90, 180, 270 degrees
                                           // counter-clockwise respectively.
                                           //
#define RETRO_ENVIRONMENT_GET_OVERSCAN  2  // bool * --
                                           // Boolean value whether or not the implementation should use overscan, or crop away overscan.
                                           //
#define RETRO_ENVIRONMENT_GET_CAN_DUPE  3  // bool * --
                                           // Boolean value whether or not frontend supports frame duping,
                                           // passing NULL to video frame callback.
                                           //
// Environ 4, 5 are no longer supported (GET_VARIABLE / SET_VARIABLES), and reserved to avoid possible ABI clash.
#define RETRO_ENVIRONMENT_SET_MESSAGE   6  // const struct retro_message * --
                                           // Sets a message to be displayed in implementation-specific manner for a certain amount of 'frames'.
                                           // Should not be used for trivial messages, which should simply be logged to stderr.
#define RETRO_ENVIRONMENT_SHUTDOWN      7  // N/A (NULL) --
                                           // Requests the frontend to shutdown.
                                           // Should only be used if game has a specific
                                           // way to shutdown the game from a menu item or similar.
                                           //
#define RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL 8
                                           // const unsigned * --
                                           // Gives a hint to the frontend how demanding this implementation
                                           // is on a system. E.g. reporting a level of 2 means
                                           // this implementation should run decently on all frontends
                                           // of level 2 and up.
                                           //
                                           // It can be used by the frontend to potentially warn
                                           // about too demanding implementations.
                                           //
                                           // The levels are "floating", but roughly defined as:
                                           // 0: Low-powered embedded devices such as Raspberry Pi
                                           // 1: 6th generation consoles, such as Wii/Xbox 1, and phones, tablets, etc.
                                           // 2: 7th generation consoles, such as PS3/360, with sub-par CPUs.
                                           // 3: Modern desktop/laptops with reasonably powerful CPUs.
                                           // 4: High-end desktops with very powerful CPUs.
                                           //
                                           // This function can be called on a per-game basis,
                                           // as certain games an implementation can play might be
                                           // particularily demanding.
                                           // If called, it should be called in retro_load_game().
                                           //
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9
                                           // const char ** --
                                           // Returns the "system" directory of the frontend.
                                           // This directory can be used to store system specific ROMs such as BIOSes, configuration data, etc.
                                           // The returned value can be NULL.
                                           // If so, no such directory is defined,
                                           // and it's up to the implementation to find a suitable directory.
                                           //
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 10
                                           // const enum retro_pixel_format * --
                                           // Sets the internal pixel format used by the implementation.
                                           // The default pixel format is RETRO_PIXEL_FORMAT_0RGB1555.
                                           // This pixel format however, is deprecated (see enum retro_pixel_format).
                                           // If the call returns false, the frontend does not support this pixel format.
                                           // This function should be called inside retro_load_game() or retro_get_system_av_info().
                                           //
#define RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS 11
                                           // const struct retro_input_descriptor * --
                                           // Sets an array of retro_input_descriptors.
                                           // It is up to the frontend to present this in a usable way.
                                           // The array is terminated by retro_input_descriptor::description being set to NULL.
                                           // This function can be called at any time, but it is recommended to call it as early as possible.
#define RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK 12
                                           // const struct retro_keyboard_callback * --
                                           // Sets a callback function used to notify core about keyboard events.
                                           //
#define RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE 13
                                           // const struct retro_disk_control_callback * --
                                           // Sets an interface which frontend can use to eject and insert disk images.
                                           // This is used for games which consist of multiple images and must be manually
                                           // swapped out by the user (e.g. PSX).
#define RETRO_ENVIRONMENT_SET_HW_RENDER 14
                                           // struct retro_hw_render_callback * --
                                           // Sets an interface to let a libretro core render with hardware acceleration.
                                           // Should be called in retro_load_game().
                                           // If successful, libretro cores will be able to render to a frontend-provided framebuffer.
                                           // The size of this framebuffer will be at least as large as max_width/max_height provided in get_av_info().
                                           // If HW rendering is used, pass only RETRO_HW_FRAME_BUFFER_VALID or NULL to retro_video_refresh_t.
#define RETRO_ENVIRONMENT_GET_VARIABLE 15
                                           // struct retro_variable * --
                                           // Interface to acquire user-defined information from environment
                                           // that cannot feasibly be supported in a multi-system way.
                                           // 'key' should be set to a key which has already been set by SET_VARIABLES.
                                           // 'data' will be set to a value or NULL.
                                           //
#define RETRO_ENVIRONMENT_SET_VARIABLES 16
                                           // const struct retro_variable * --
                                           // Allows an implementation to signal the environment
                                           // which variables it might want to check for later using GET_VARIABLE.
                                           // This allows the frontend to present these variables to a user dynamically.
                                           // This should be called as early as possible (ideally in retro_set_environment).
                                           //
                                           // 'data' points to an array of retro_variable structs terminated by a { NULL, NULL } element.
                                           // retro_variable::key should be namespaced to not collide with other implementations' keys. E.g. A core called 'foo' should use keys named as 'foo_option'.
                                           // retro_variable::value should contain a human readable description of the key as well as a '|' delimited list of expected values.
                                           // The number of possible options should be very limited, i.e. it should be feasible to cycle through options without a keyboard.
                                           // First entry should be treated as a default.
                                           //
                                           // Example entry:
                                           // { "foo_option", "Speed hack coprocessor X; false|true" }
                                           //
                                           // Text before first ';' is description. This ';' must be followed by a space, and followed by a list of possible values split up with '|'.
                                           // Only strings are operated on. The possible values will generally be displayed and stored as-is by the frontend.
                                           //
#define RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE 17
                                           // bool * --
                                           // Result is set to true if some variables are updated by
                                           // frontend since last call to RETRO_ENVIRONMENT_GET_VARIABLE.
                                           // Variables should be queried with GET_VARIABLE.
                                           //
#define RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME 18
                                           // const bool * --
                                           // If true, the libretro implementation supports calls to retro_load_game() with NULL as argument.
                                           // Used by cores which can run without particular game data.
                                           // This should be called within retro_set_environment() only.
                                           //
#define RETRO_ENVIRONMENT_GET_LIBRETRO_PATH 19
                                           // const char ** --
                                           // Retrieves the absolute path from where this libretro implementation was loaded.
                                           // NULL is returned if the libretro was loaded statically (i.e. linked statically to frontend), or if the path cannot be determined.
                                           // Mostly useful in cooperation with SET_SUPPORT_NO_GAME as assets can be loaded without ugly hacks.
                                           //
                                           //
// Environment 20 was an obsolete version of SET_AUDIO_CALLBACK. It was not used by any known core at the time,
// and was removed from the API.
#define RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK 22
                                           // const struct retro_audio_callback * --
                                           // Sets an interface which is used to notify a libretro core about audio being available for writing.
                                           // The callback can be called from any thread, so a core using this must have a thread safe audio implementation.
                                           // It is intended for games where audio and video are completely asynchronous and audio can be generated on the fly.
                                           // This interface is not recommended for use with emulators which have highly synchronous audio.
                                           //
                                           // The callback only notifies about writability; the libretro core still has to call the normal audio callbacks
                                           // to write audio. The audio callbacks must be called from within the notification callback.
                                           // The amount of audio data to write is up to the implementation.
                                           // Generally, the audio callback will be called continously in a loop.
                                           //
                                           // Due to thread safety guarantees and lack of sync between audio and video, a frontend
                                           // can selectively disallow this interface based on internal configuration. A core using
                                           // this interface must also implement the "normal" audio interface.
                                           //
                                           // A libretro core using SET_AUDIO_CALLBACK should also make use of SET_FRAME_TIME_CALLBACK.
#define RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK 21
                                           // const struct retro_frame_time_callback * --
                                           // Lets the core know how much time has passed since last invocation of retro_run().
                                           // The frontend can tamper with the timing to fake fast-forward, slow-motion, frame stepping, etc.
                                           // In this case the delta time will use the reference value in frame_time_callback..
                                           //
#define RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE 23
                                           // struct retro_rumble_interface * --
                                           // Gets an interface which is used by a libretro core to set state of rumble motors in controllers.
                                           // A strong and weak motor is supported, and they can be controlled indepedently.
                                           //
#define RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES 24
                                           // uint64_t * --
                                           // Gets a bitmask telling which device type are expected to be handled properly in a call to retro_input_state_t.
                                           // Devices which are not handled or recognized always return 0 in retro_input_state_t.
                                           // Example bitmask: caps = (1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG).
                                           // Should only be called in retro_run().
                                           //
#define RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE (25 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           // struct retro_sensor_interface * --
                                           // Gets access to the sensor interface.
                                           // The purpose of this interface is to allow
                                           // setting state related to sensors such as polling rate, enabling/disable it entirely, etc.
                                           // Reading sensor state is done via the normal input_state_callback API.
                                           //
#define RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE (26 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           // struct retro_camera_callback * --
                                           // Gets an interface to a video camera driver.
                                           // A libretro core can use this interface to get access to a video camera.
                                           // New video frames are delivered in a callback in same thread as retro_run().
                                           //
                                           // GET_CAMERA_INTERFACE should be called in retro_load_game().
                                           //
                                           // Depending on the camera implementation used, camera frames will be delivered as a raw framebuffer,
                                           // or as an OpenGL texture directly.
                                           //
                                           // The core has to tell the frontend here which types of buffers can be handled properly.
                                           // An OpenGL texture can only be handled when using a libretro GL core (SET_HW_RENDER).
                                           // It is recommended to use a libretro GL core when using camera interface.
                                           //
                                           // The camera is not started automatically. The retrieved start/stop functions must be used to explicitly
                                           // start and stop the camera driver.
                                           //
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27
                                           // struct retro_log_callback * --
                                           // Gets an interface for logging. This is useful for logging in a cross-platform way
                                           // as certain platforms cannot use use stderr for logging. It also allows the frontend to
                                           // show logging information in a more suitable way.
                                           // If this interface is not used, libretro cores should log to stderr as desired.
#define RETRO_ENVIRONMENT_GET_PERF_INTERFACE 28
                                           // struct retro_perf_callback * --
                                           // Gets an interface for performance counters. This is useful for performance logging in a 
                                           // cross-platform way and for detecting architecture-specific features, such as SIMD support.

enum retro_log_level
{
   RETRO_LOG_DEBUG = 0,
   RETRO_LOG_INFO,
   RETRO_LOG_WARN,
   RETRO_LOG_ERROR,

   RETRO_LOG_DUMMY = INT_MAX
};

// Logging function. Takes log level argument as well.
typedef void (*retro_log_printf_t)(enum retro_log_level level, const char *fmt, ...);

struct retro_log_callback
{
   retro_log_printf_t log;
};

// Performance functions
//
// Id values for SIMD CPU features
#define RETRO_SIMD_SSE      (1 << 0)
#define RETRO_SIMD_SSE2     (1 << 1)
#define RETRO_SIMD_VMX      (1 << 2)
#define RETRO_SIMD_VMX128   (1 << 3)
#define RETRO_SIMD_AVX      (1 << 4)
#define RETRO_SIMD_NEON     (1 << 5)
#define RETRO_SIMD_SSE3     (1 << 6)
#define RETRO_SIMD_SSSE3    (1 << 7)

typedef unsigned long long retro_perf_tick_t;
typedef int64_t retro_time_t;

typedef struct retro_perf_counter
{
   const char *ident;
   retro_perf_tick_t start;
   retro_perf_tick_t total;
   retro_perf_tick_t call_cnt;

   bool registered;
} retro_perf_counter_t;


typedef retro_time_t (*retro_perf_get_time_usec_t)(void);
typedef retro_perf_tick_t (*retro_perf_get_counter_t)(void);
typedef void (*retro_get_cpu_features_t)(unsigned*);

struct retro_perf_callback
{
   retro_perf_get_time_usec_t    get_time_usec;
   retro_perf_get_counter_t      get_perf_counter; 
   retro_get_cpu_features_t      get_cpu_features;
};

#if defined(PERF_TEST)

#define RARCH_PERFORMANCE_INIT(X) \
   static retro_perf_counter_t X = {#X}; \
   do { \
      if (!(X).registered) \
         rarch_perf_register(&(X)); \
   } while(0)

#define RARCH_PERFORMANCE_START(X) do { \
   (X).call_cnt++; \
   (X).start  = rarch_get_perf_counter(); \
} while(0)

#define RARCH_PERFORMANCE_STOP(X) do { \
   (X).total += rarch_get_perf_counter() - (X).start; \
} while(0)

#elif !defined(RARCH_INTERNAL)

#define RARCH_PERFORMANCE_INIT(X, perf_register_cb)  static retro_perf_counter_t X = {#X}; \
   do { \
      if (!(X).registered) \
         perf_register_cb(&(X)); \
   } while(0)

#define RARCH_PERFORMANCE_START(X, get_perf_counter_cb) do { \
   (X).call_cnt++; \
   (X).start  = get_perf_counter_cb(); \
} while(0)

#define RARCH_PERFORMANCE_STOP(X, get_perf_counter_cb) do { \
   (X).total += get_perf_counter_cb() - (X).start; \
} while(0)
#else
#define RARCH_PERFORMANCE_INIT(X)
#define RARCH_PERFORMANCE_START(X)
#define RARCH_PERFORMANCE_STOP(X)
#endif

// FIXME: Document the sensor API and work out behavior.
// It will be marked as experimental until then.
enum retro_sensor_action
{
   RETRO_SENSOR_ACCELEROMETER_ENABLE = 0,
   RETRO_SENSOR_ACCELEROMETER_DISABLE,

   RETRO_SENSOR_DUMMY = INT_MAX
};

typedef bool (*retro_set_sensor_state_t)(unsigned port, enum retro_sensor_action action, unsigned rate);
struct retro_sensor_interface
{
   retro_set_sensor_state_t set_sensor_state;
};
////

enum retro_camera_buffer
{
   RETRO_CAMERA_BUFFER_OPENGL_TEXTURE = 0,
   RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER,

   RETRO_CAMERA_BUFFER_DUMMY = INT_MAX
};

// Starts the camera driver. Can only be called in retro_run().
typedef bool (*retro_camera_start_t)(void);
// Stops the camera driver. Can only be called in retro_run().
typedef void (*retro_camera_stop_t)(void);
// Callback which signals when the camera driver is initialized and/or deinitialized.
// retro_camera_start_t can be called in initialized callback.
typedef void (*retro_camera_lifetime_status_t)(void);
// A callback for raw framebuffer data. buffer points to an XRGB8888 buffer.
// Width, height and pitch are similar to retro_video_refresh_t.
// First pixel is top-left origin.
typedef void (*retro_camera_frame_raw_framebuffer_t)(const uint32_t *buffer, unsigned width, unsigned height, size_t pitch);
// A callback for when OpenGL textures are used.
//
// texture_id is a texture owned by camera driver.
// Its state or content should be considered immutable, except for things like texture filtering and clamping.
//
// texture_target is the texture target for the GL texture.
// These can include e.g. GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE, and possibly more depending on extensions.
// 
// affine points to a packed 3x3 column-major matrix used to apply an affine transform to texture coordinates. (affine_matrix * vec3(coord_x, coord_y, 1.0))
// After transform, normalized texture coord (0, 0) should be bottom-left and (1, 1) should be top-right (or (width, height) for RECTANGLE).
//
// GL-specific typedefs are avoided here to avoid relying on gl.h in the API definition.
typedef void (*retro_camera_frame_opengl_texture_t)(unsigned texture_id, unsigned texture_target, const float *affine);
struct retro_camera_callback
{
   uint64_t caps; // Set by libretro core. Example bitmask: caps = (1 << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE) | (1 << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER).

   unsigned width; // Desired resolution for camera. Is only used as a hint.
   unsigned height;
   retro_camera_start_t start; // Set by frontend.
   retro_camera_stop_t stop; // Set by frontend.

   retro_camera_frame_raw_framebuffer_t frame_raw_framebuffer; // Set by libretro core if raw framebuffer callbacks will be used.
   retro_camera_frame_opengl_texture_t frame_opengl_texture; // Set by libretro core if OpenGL texture callbacks will be used.

   // Set by libretro core. Called after camera driver is initialized and ready to be started.
   // Can be NULL, in which this callback is not called.
   retro_camera_lifetime_status_t initialized;

   // Set by libretro core. Called right before camera driver is deinitialized.
   // Can be NULL, in which this callback is not called.
   retro_camera_lifetime_status_t deinitialized;
};

enum retro_rumble_effect
{
   RETRO_RUMBLE_STRONG = 0,
   RETRO_RUMBLE_WEAK = 1,

   RETRO_RUMBLE_DUMMY = INT_MAX
};

// Sets rumble state for joypad plugged in port 'port'. Rumble effects are controlled independently,
// and setting e.g. strong rumble does not override weak rumble.
// Strength has a range of [0, 0xffff].
//
// Returns true if rumble state request was honored. Calling this before first retro_run() is likely to return false.
typedef bool (*retro_set_rumble_state_t)(unsigned port, enum retro_rumble_effect effect, uint16_t strength);
struct retro_rumble_interface
{
   retro_set_rumble_state_t set_rumble_state;
};

// Notifies libretro that audio data should be written.
typedef void (*retro_audio_callback_t)(void);

// True: Audio driver in frontend is active, and callback is expected to be called regularily.
// False: Audio driver in frontend is paused or inactive. Audio callback will not be called until set_state has been called with true.
// Initial state is false (inactive).
typedef void (*retro_audio_set_state_callback_t)(bool enabled);
struct retro_audio_callback
{
   retro_audio_callback_t callback;
   retro_audio_set_state_callback_t set_state;
};

// Notifies a libretro core of time spent since last invocation of retro_run() in microseconds.
// It will be called right before retro_run() every frame.
// The frontend can tamper with timing to support cases like fast-forward, slow-motion and framestepping.
// In those scenarios the reference frame time value will be used.
typedef int64_t retro_usec_t;
typedef void (*retro_frame_time_callback_t)(retro_usec_t usec);
struct retro_frame_time_callback
{
   retro_frame_time_callback_t callback;
   retro_usec_t reference; // Represents the time of one frame. It is computed as 1000000 / fps, but the implementation will resolve the rounding to ensure that framestepping, etc is exact.
};

// Pass this to retro_video_refresh_t if rendering to hardware.
// Passing NULL to retro_video_refresh_t is still a frame dupe as normal.
#define RETRO_HW_FRAME_BUFFER_VALID ((void*)-1)

// Invalidates the current HW context.
// Any GL state is lost, and must not be deinitialized explicitly. If explicit deinitialization is desired by the libretro core,
// it should implement context_destroy callback.
// If called, all GPU resources must be reinitialized.
// Usually called when frontend reinits video driver.
// Also called first time video driver is initialized, allowing libretro core to init resources.
typedef void (*retro_hw_context_reset_t)(void);
// Gets current framebuffer which is to be rendered to. Could change every frame potentially.
typedef uintptr_t (*retro_hw_get_current_framebuffer_t)(void);

// Get a symbol from HW context.
typedef void (*retro_proc_address_t)(void);
typedef retro_proc_address_t (*retro_hw_get_proc_address_t)(const char *sym);

enum retro_hw_context_type
{
   RETRO_HW_CONTEXT_NONE = 0,
   RETRO_HW_CONTEXT_OPENGL, // OpenGL 2.x. Latest version available before 3.x+. Driver can choose to use latest compatibility context.
   RETRO_HW_CONTEXT_OPENGLES2, // GLES 2.0
   RETRO_HW_CONTEXT_OPENGL_CORE, // Modern desktop core GL context. Use major/minor fields to set GL version.
   RETRO_HW_CONTEXT_OPENGLES3, // GLES 3.0

   RETRO_HW_CONTEXT_DUMMY = INT_MAX
};

struct retro_hw_render_callback
{
   enum retro_hw_context_type context_type; // Which API to use. Set by libretro core.
   retro_hw_context_reset_t context_reset; // Called when a context has been created or when it has been reset.
   retro_hw_get_current_framebuffer_t get_current_framebuffer; // Set by frontend.
   retro_hw_get_proc_address_t get_proc_address; // Set by frontend.
   bool depth; // Set if render buffers should have depth component attached.
   bool stencil; // Set if stencil buffers should be attached.
   // If depth and stencil are true, a packed 24/8 buffer will be added. Only attaching stencil is invalid and will be ignored.
   bool bottom_left_origin; // Use conventional bottom-left origin convention. Is false, standard libretro top-left origin semantics are used.
   unsigned version_major; // Major version number for core GL context.
   unsigned version_minor; // Minor version number for core GL context.

   bool cache_context; // If this is true, the frontend will go very far to avoid resetting context in scenarios like toggling fullscreen, etc.
   // The reset callback might still be called in extreme situations such as if the context is lost beyond recovery. 
   // For optimal stability, set this to false, and allow context to be reset at any time.
   retro_hw_context_reset_t context_destroy; // A callback to be called before the context is destroyed. Resources can be deinitialized at this step. This can be set to NULL, in which resources will just be destroyed without any notification.
   bool debug_context; // Creates a debug context.
};

// Callback type passed in RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK. Called by the frontend in response to keyboard events.
// down is set if the key is being pressed, or false if it is being released.
// keycode is the RETROK value of the char.
// character is the text character of the pressed key. (UTF-32).
// key_modifiers is a set of RETROKMOD values or'ed together.
//
// The pressed/keycode state can be indepedent of the character.
// It is also possible that multiple characters are generated from a single keypress.
// Keycode events should be treated separately from character events.
// However, when possible, the frontend should try to synchronize these.
// If only a character is posted, keycode should be RETROK_UNKNOWN.
// Similarily if only a keycode event is generated with no corresponding character, character should be 0.
typedef void (*retro_keyboard_event_t)(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers);

struct retro_keyboard_callback
{
   retro_keyboard_event_t callback;
};

// Callbacks for RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE.
// Should be set for implementations which can swap out multiple disk images in runtime.
// If the implementation can do this automatically, it should strive to do so.
// However, there are cases where the user must manually do so.
//
// Overview: To swap a disk image, eject the disk image with set_eject_state(true).
// Set the disk index with set_image_index(index). Insert the disk again with set_eject_state(false).

// If ejected is true, "ejects" the virtual disk tray.
// When ejected, the disk image index can be set.
typedef bool (*retro_set_eject_state_t)(bool ejected);
// Gets current eject state. The initial state is 'not ejected'.
typedef bool (*retro_get_eject_state_t)(void);
// Gets current disk index. First disk is index 0.
// If return value is >= get_num_images(), no disk is currently inserted.
typedef unsigned (*retro_get_image_index_t)(void);
// Sets image index. Can only be called when disk is ejected.
// The implementation supports setting "no disk" by using an index >= get_num_images().
typedef bool (*retro_set_image_index_t)(unsigned index);
// Gets total number of images which are available to use.
typedef unsigned (*retro_get_num_images_t)(void);
//
// Replaces the disk image associated with index.
// Arguments to pass in info have same requirements as retro_load_game().
// Virtual disk tray must be ejected when calling this.
// Replacing a disk image with info = NULL will remove the disk image from the internal list.
// As a result, calls to get_image_index() can change.
//
// E.g. replace_image_index(1, NULL), and previous get_image_index() returned 4 before.
// Index 1 will be removed, and the new index is 3.
struct retro_game_info;
typedef bool (*retro_replace_image_index_t)(unsigned index, const struct retro_game_info *info);
// Adds a new valid index (get_num_images()) to the internal disk list.
// This will increment subsequent return values from get_num_images() by 1.
// This image index cannot be used until a disk image has been set with replace_image_index.
typedef bool (*retro_add_image_index_t)(void);

struct retro_disk_control_callback
{
   retro_set_eject_state_t set_eject_state;
   retro_get_eject_state_t get_eject_state;

   retro_get_image_index_t get_image_index;
   retro_set_image_index_t set_image_index;
   retro_get_num_images_t  get_num_images;

   retro_replace_image_index_t replace_image_index;
   retro_add_image_index_t add_image_index;
};

enum retro_pixel_format
{
   // 0RGB1555, native endian. 0 bit must be set to 0.
   // This pixel format is default for compatibility concerns only.
   // If a 15/16-bit pixel format is desired, consider using RGB565.
   RETRO_PIXEL_FORMAT_0RGB1555 = 0,

   // XRGB8888, native endian. X bits are ignored.
   RETRO_PIXEL_FORMAT_XRGB8888 = 1,

   // RGB565, native endian. This pixel format is the recommended format to use if a 15/16-bit format is desired
   // as it is the pixel format that is typically available on a wide range of low-power devices.
   // It is also natively supported in APIs like OpenGL ES.
   RETRO_PIXEL_FORMAT_RGB565   = 2,

   // Ensure sizeof() == sizeof(int).
   RETRO_PIXEL_FORMAT_UNKNOWN  = INT_MAX
};

struct retro_message
{
   const char *msg;        // Message to be displayed.
   unsigned    frames;     // Duration in frames of message.
};

// Describes how the libretro implementation maps a libretro input bind
// to its internal input system through a human readable string.
// This string can be used to better let a user configure input.
struct retro_input_descriptor
{
   // Associates given parameters with a description.
   unsigned port;
   unsigned device;
   unsigned index;
   unsigned id;

   const char *description; // Human readable description for parameters.
                            // The pointer must remain valid until retro_unload_game() is called.
};

struct retro_system_info
{
   // All pointers are owned by libretro implementation, and pointers must remain valid until retro_deinit() is called.

   const char *library_name;      // Descriptive name of library. Should not contain any version numbers, etc.
   const char *library_version;   // Descriptive version of core.

   const char *valid_extensions;  // A string listing probably rom extensions the core will be able to load, separated with pipe.
                                  // I.e. "bin|rom|iso".
                                  // Typically used for a GUI to filter out extensions.

   bool        need_fullpath;     // If true, retro_load_game() is guaranteed to provide a valid pathname in retro_game_info::path.
                                  // ::data and ::size are both invalid.
                                  // If false, ::data and ::size are guaranteed to be valid, but ::path might not be valid.
                                  // This is typically set to true for libretro implementations that must load from file.
                                  // Implementations should strive for setting this to false, as it allows the frontend to perform patching, etc.

   bool        block_extract;     // If true, the frontend is not allowed to extract any archives before loading the real ROM.
                                  // Necessary for certain libretro implementations that load games from zipped archives.
};

struct retro_game_geometry
{
   unsigned base_width;    // Nominal video width of game.
   unsigned base_height;   // Nominal video height of game.
   unsigned max_width;     // Maximum possible width of game.
   unsigned max_height;    // Maximum possible height of game.

   float    aspect_ratio;  // Nominal aspect ratio of game. If aspect_ratio is <= 0.0,
                           // an aspect ratio of base_width / base_height is assumed.
                           // A frontend could override this setting if desired.
};

struct retro_system_timing
{
   double fps;             // FPS of video content.
   double sample_rate;     // Sampling rate of audio.
};

struct retro_system_av_info
{
   struct retro_game_geometry geometry;
   struct retro_system_timing timing;
};

struct retro_variable
{
   const char *key;        // Variable to query in RETRO_ENVIRONMENT_GET_VARIABLE.
                           // If NULL, obtains the complete environment string if more complex parsing is necessary.
                           // The environment string is formatted as key-value pairs delimited by semicolons as so:
                           // "key1=value1;key2=value2;..."
   const char *value;      // Value to be obtained. If key does not exist, it is set to NULL.
};

struct retro_game_info
{
   const char *path;       // Path to game, UTF-8 encoded. Usually used as a reference.
                           // May be NULL if rom was loaded from stdin or similar.
                           // retro_system_info::need_fullpath guaranteed that this path is valid.
   const void *data;       // Memory buffer of loaded game. Will be NULL if need_fullpath was set.
   size_t      size;       // Size of memory buffer.
   const char *meta;       // String of implementation specific meta-data.
};

// Callbacks
//
// Environment callback. Gives implementations a way of performing uncommon tasks. Extensible.
typedef bool (*retro_environment_t)(unsigned cmd, void *data);

// Render a frame. Pixel format is 15-bit 0RGB1555 native endian unless changed (see RETRO_ENVIRONMENT_SET_PIXEL_FORMAT).
// Width and height specify dimensions of buffer.
// Pitch specifices length in bytes between two lines in buffer.
// For performance reasons, it is highly recommended to have a frame that is packed in memory, i.e. pitch == width * byte_per_pixel.
// Certain graphic APIs, such as OpenGL ES, do not like textures that are not packed in memory.
typedef void (*retro_video_refresh_t)(const void *data, unsigned width, unsigned height, size_t pitch);

// Renders a single audio frame. Should only be used if implementation generates a single sample at a time.
// Format is signed 16-bit native endian.
typedef void (*retro_audio_sample_t)(int16_t left, int16_t right);
// Renders multiple audio frames in one go. One frame is defined as a sample of left and right channels, interleaved.
// I.e. int16_t buf[4] = { l, r, l, r }; would be 2 frames.
// Only one of the audio callbacks must ever be used.
typedef size_t (*retro_audio_sample_batch_t)(const int16_t *data, size_t frames);

// Polls input.
typedef void (*retro_input_poll_t)(void);
// Queries for input for player 'port'. device will be masked with RETRO_DEVICE_MASK.
// Specialization of devices such as RETRO_DEVICE_JOYPAD_MULTITAP that have been set with retro_set_controller_port_device()
// will still use the higher level RETRO_DEVICE_JOYPAD to request input.
typedef int16_t (*retro_input_state_t)(unsigned port, unsigned device, unsigned index, unsigned id);

// Sets callbacks. retro_set_environment() is guaranteed to be called before retro_init().
// The rest of the set_* functions are guaranteed to have been called before the first call to retro_run() is made.
void retro_set_environment(retro_environment_t);
void retro_set_video_refresh(retro_video_refresh_t);
void retro_set_audio_sample(retro_audio_sample_t);
void retro_set_audio_sample_batch(retro_audio_sample_batch_t);
void retro_set_input_poll(retro_input_poll_t);
void retro_set_input_state(retro_input_state_t);

// Library global initialization/deinitialization.
void retro_init(void);
void retro_deinit(void);

// Must return RETRO_API_VERSION. Used to validate ABI compatibility when the API is revised.
unsigned retro_api_version(void);

// Gets statically known system info. Pointers provided in *info must be statically allocated.
// Can be called at any time, even before retro_init().
void retro_get_system_info(struct retro_system_info *info);

// Gets information about system audio/video timings and geometry.
// Can be called only after retro_load_game() has successfully completed.
// NOTE: The implementation of this function might not initialize every variable if needed.
// E.g. geom.aspect_ratio might not be initialized if core doesn't desire a particular aspect ratio.
void retro_get_system_av_info(struct retro_system_av_info *info);

// Sets device to be used for player 'port'.
void retro_set_controller_port_device(unsigned port, unsigned device);

// Resets the current game.
void retro_reset(void);

// Runs the game for one video frame.
// During retro_run(), input_poll callback must be called at least once.
//
// If a frame is not rendered for reasons where a game "dropped" a frame,
// this still counts as a frame, and retro_run() should explicitly dupe a frame if GET_CAN_DUPE returns true.
// In this case, the video callback can take a NULL argument for data.
void retro_run(void);

// Returns the amount of data the implementation requires to serialize internal state (save states).
// Beetween calls to retro_load_game() and retro_unload_game(), the returned size is never allowed to be larger than a previous returned value, to
// ensure that the frontend can allocate a save state buffer once.
size_t retro_serialize_size(void);

// Serializes internal state. If failed, or size is lower than retro_serialize_size(), it should return false, true otherwise.
bool retro_serialize(void *data, size_t size);
bool retro_unserialize(const void *data, size_t size);

void retro_cheat_reset(void);
void retro_cheat_set(unsigned index, bool enabled, const char *code);

// Loads a game.
bool retro_load_game(const struct retro_game_info *game);

// Loads a "special" kind of game. Should not be used except in extreme cases.
bool retro_load_game_special(
  unsigned game_type,
  const struct retro_game_info *info, size_t num_info
);

// Unloads a currently loaded game.
void retro_unload_game(void);

// Gets region of game.
unsigned retro_get_region(void);

// Gets region of memory.
void *retro_get_memory_data(unsigned id);
size_t retro_get_memory_size(unsigned id);

#ifdef __cplusplus
}
#endif

#endif

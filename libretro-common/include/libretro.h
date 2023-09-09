/* Copyright (C) 2010-2020 The RetroArch team
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
#if defined(_MSC_VER) && _MSC_VER < 1800 && !defined(SN_TARGET_PS3)
/* Hack applied for MSVC when compiling in C89 mode
 * as it isn't C99-compliant. */
#define bool unsigned char
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif
#endif

#ifndef RETRO_CALLCONV
#  if defined(__GNUC__) && defined(__i386__) && !defined(__x86_64__)
#    define RETRO_CALLCONV __attribute__((cdecl))
#  elif defined(_MSC_VER) && defined(_M_X86) && !defined(_M_X64)
#    define RETRO_CALLCONV __cdecl
#  else
#    define RETRO_CALLCONV /* all other platforms only have one calling convention each */
#  endif
#endif

#ifndef RETRO_API
#  if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
#    ifdef RETRO_IMPORT_SYMBOLS
#      ifdef __GNUC__
#        define RETRO_API RETRO_CALLCONV __attribute__((__dllimport__))
#      else
#        define RETRO_API RETRO_CALLCONV __declspec(dllimport)
#      endif
#    else
#      ifdef __GNUC__
#        define RETRO_API RETRO_CALLCONV __attribute__((__dllexport__))
#      else
#        define RETRO_API RETRO_CALLCONV __declspec(dllexport)
#      endif
#    endif
#  else
#      if defined(__GNUC__) && __GNUC__ >= 4
#        define RETRO_API RETRO_CALLCONV __attribute__((__visibility__("default")))
#      else
#        define RETRO_API RETRO_CALLCONV
#      endif
#  endif
#endif

/**
 * The major version of the libretro API and ABI.
 * Cores may support multiple versions,
 * or they may reject cores with unsupported versions.
 * It is only incremented for incompatible API/ABI changes;
 * this generally implies a function was removed or changed,
 * or that a \c struct had fields removed or changed.
 * @note A design goal of libretro is to avoid having to increase this value at all costs.
 * This is why there are APIs that are "extended" or "V2".
 */
#define RETRO_API_VERSION         1

/**
 * @defgroup RETRO_DEVICE Input Devices
 * @{
 */

/*
 * Libretro's fundamental device abstractions.
 *
 * Libretro's input system consists of some standardized device types,
 * such as a joypad (with/without analog), mouse, keyboard, lightgun
 * and a pointer.
 *
 * The functionality of these devices are fixed, and individual cores
 * map their own concept of a controller to libretro's abstractions.
 * This makes it possible for frontends to map the abstract types to a
 * real input device, and not having to worry about binding input
 * correctly to arbitrary controller layouts.
 */

#define RETRO_DEVICE_TYPE_SHIFT         8
#define RETRO_DEVICE_MASK               ((1 << RETRO_DEVICE_TYPE_SHIFT) - 1)
#define RETRO_DEVICE_SUBCLASS(base, id) (((id + 1) << RETRO_DEVICE_TYPE_SHIFT) | base)

/**
 * @defgroup RETRO_DEVICE Input Device Types
 * @{
 */

/* Input disabled. */
#define RETRO_DEVICE_NONE         0

/* The JOYPAD is called RetroPad. It is essentially a Super Nintendo
 * controller, but with additional L2/R2/L3/R3 buttons, similar to a
 * PS1 DualShock. */
#define RETRO_DEVICE_JOYPAD       1

/* The mouse is a simple mouse, similar to Super Nintendo's mouse.
 * X and Y coordinates are reported relatively to last poll (poll callback).
 * It is up to the libretro implementation to keep track of where the mouse
 * pointer is supposed to be on the screen.
 * The frontend must make sure not to interfere with its own hardware
 * mouse pointer.
 */
#define RETRO_DEVICE_MOUSE        2

/* KEYBOARD device lets one poll for raw key pressed.
 * It is poll based, so input callback will return with the current
 * pressed state.
 * For event/text based keyboard input, see
 * RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK.
 */
#define RETRO_DEVICE_KEYBOARD     3

/* LIGHTGUN device is similar to Guncon-2 for PlayStation 2.
 * It reports X/Y coordinates in screen space (similar to the pointer)
 * in the range [-0x8000, 0x7fff] in both axes, with zero being center and
 * -0x8000 being out of bounds.
 * As well as reporting on/off screen state. It features a trigger,
 * start/select buttons, auxiliary action buttons and a
 * directional pad. A forced off-screen shot can be requested for
 * auto-reloading function in some games.
 */
#define RETRO_DEVICE_LIGHTGUN     4

/* The ANALOG device is an extension to JOYPAD (RetroPad).
 * Similar to DualShock2 it adds two analog sticks and all buttons can
 * be analog. This is treated as a separate device type as it returns
 * axis values in the full analog range of [-0x7fff, 0x7fff],
 * although some devices may return -0x8000.
 * Positive X axis is right. Positive Y axis is down.
 * Buttons are returned in the range [0, 0x7fff].
 * Only use ANALOG type when polling for analog values.
 */
#define RETRO_DEVICE_ANALOG       5

/* Abstracts the concept of a pointing mechanism, e.g. touch.
 * This allows libretro to query in absolute coordinates where on the
 * screen a mouse (or something similar) is being placed.
 * For a touch centric device, coordinates reported are the coordinates
 * of the press.
 *
 * Coordinates in X and Y are reported as:
 * [-0x7fff, 0x7fff]: -0x7fff corresponds to the far left/top of the screen,
 * and 0x7fff corresponds to the far right/bottom of the screen.
 * The "screen" is here defined as area that is passed to the frontend and
 * later displayed on the monitor.
 *
 * The frontend is free to scale/resize this screen as it sees fit, however,
 * (X, Y) = (-0x7fff, -0x7fff) will correspond to the top-left pixel of the
 * game image, etc.
 *
 * To check if the pointer coordinates are valid (e.g. a touch display
 * actually being touched), PRESSED returns 1 or 0.
 *
 * If using a mouse on a desktop, PRESSED will usually correspond to the
 * left mouse button, but this is a frontend decision.
 * PRESSED will only return 1 if the pointer is inside the game screen.
 *
 * For multi-touch, the index variable can be used to successively query
 * more presses.
 * If index = 0 returns true for _PRESSED, coordinates can be extracted
 * with _X, _Y for index = 0. One can then query _PRESSED, _X, _Y with
 * index = 1, and so on.
 * Eventually _PRESSED will return false for an index. No further presses
 * are registered at this point. */
#define RETRO_DEVICE_POINTER      6

/** @} */

/* Buttons for the RetroPad (JOYPAD).
 * The placement of these is equivalent to placements on the
 * Super Nintendo controller.
 * L2/R2/L3/R3 buttons correspond to the PS1 DualShock.
 * Also used as id values for RETRO_DEVICE_INDEX_ANALOG_BUTTON */
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

#define RETRO_DEVICE_ID_JOYPAD_MASK    256

/* Index / Id values for ANALOG device. */
#define RETRO_DEVICE_INDEX_ANALOG_LEFT       0
#define RETRO_DEVICE_INDEX_ANALOG_RIGHT      1
#define RETRO_DEVICE_INDEX_ANALOG_BUTTON     2
#define RETRO_DEVICE_ID_ANALOG_X             0
#define RETRO_DEVICE_ID_ANALOG_Y             1

/* Id values for MOUSE. */
#define RETRO_DEVICE_ID_MOUSE_X                0
#define RETRO_DEVICE_ID_MOUSE_Y                1
#define RETRO_DEVICE_ID_MOUSE_LEFT             2
#define RETRO_DEVICE_ID_MOUSE_RIGHT            3
#define RETRO_DEVICE_ID_MOUSE_WHEELUP          4
#define RETRO_DEVICE_ID_MOUSE_WHEELDOWN        5
#define RETRO_DEVICE_ID_MOUSE_MIDDLE           6
#define RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP    7
#define RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN  8
#define RETRO_DEVICE_ID_MOUSE_BUTTON_4         9
#define RETRO_DEVICE_ID_MOUSE_BUTTON_5         10

/* Id values for LIGHTGUN. */
#define RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X        13 /*Absolute Position*/
#define RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y        14 /*Absolute*/
#define RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN    15 /*Status Check*/
#define RETRO_DEVICE_ID_LIGHTGUN_TRIGGER          2
#define RETRO_DEVICE_ID_LIGHTGUN_RELOAD          16 /*Forced off-screen shot*/
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_A            3
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_B            4
#define RETRO_DEVICE_ID_LIGHTGUN_START            6
#define RETRO_DEVICE_ID_LIGHTGUN_SELECT           7
#define RETRO_DEVICE_ID_LIGHTGUN_AUX_C            8
#define RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP          9
#define RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN       10
#define RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT       11
#define RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT      12
/* deprecated */
#define RETRO_DEVICE_ID_LIGHTGUN_X                0 /*Relative Position*/
#define RETRO_DEVICE_ID_LIGHTGUN_Y                1 /*Relative*/
#define RETRO_DEVICE_ID_LIGHTGUN_CURSOR           3 /*Use Aux:A*/
#define RETRO_DEVICE_ID_LIGHTGUN_TURBO            4 /*Use Aux:B*/
#define RETRO_DEVICE_ID_LIGHTGUN_PAUSE            5 /*Use Start*/

/* Id values for POINTER. */
#define RETRO_DEVICE_ID_POINTER_X         0
#define RETRO_DEVICE_ID_POINTER_Y         1
#define RETRO_DEVICE_ID_POINTER_PRESSED   2
#define RETRO_DEVICE_ID_POINTER_COUNT     3

/** @} */

/* Returned from retro_get_region(). */
#define RETRO_REGION_NTSC  0
#define RETRO_REGION_PAL   1

/**
 * Identifiers for supported languages.
 * @see RETRO_ENVIRONMENT_GET_LANGUAGE
 */
enum retro_language
{
   RETRO_LANGUAGE_ENGLISH             = 0,
   RETRO_LANGUAGE_JAPANESE            = 1,
   RETRO_LANGUAGE_FRENCH              = 2,
   RETRO_LANGUAGE_SPANISH             = 3,
   RETRO_LANGUAGE_GERMAN              = 4,
   RETRO_LANGUAGE_ITALIAN             = 5,
   RETRO_LANGUAGE_DUTCH               = 6,
   RETRO_LANGUAGE_PORTUGUESE_BRAZIL   = 7,
   RETRO_LANGUAGE_PORTUGUESE_PORTUGAL = 8,
   RETRO_LANGUAGE_RUSSIAN             = 9,
   RETRO_LANGUAGE_KOREAN              = 10,
   RETRO_LANGUAGE_CHINESE_TRADITIONAL = 11,
   RETRO_LANGUAGE_CHINESE_SIMPLIFIED  = 12,
   RETRO_LANGUAGE_ESPERANTO           = 13,
   RETRO_LANGUAGE_POLISH              = 14,
   RETRO_LANGUAGE_VIETNAMESE          = 15,
   RETRO_LANGUAGE_ARABIC              = 16,
   RETRO_LANGUAGE_GREEK               = 17,
   RETRO_LANGUAGE_TURKISH             = 18,
   RETRO_LANGUAGE_SLOVAK              = 19,
   RETRO_LANGUAGE_PERSIAN             = 20,
   RETRO_LANGUAGE_HEBREW              = 21,
   RETRO_LANGUAGE_ASTURIAN            = 22,
   RETRO_LANGUAGE_FINNISH             = 23,
   RETRO_LANGUAGE_INDONESIAN          = 24,
   RETRO_LANGUAGE_SWEDISH             = 25,
   RETRO_LANGUAGE_UKRAINIAN           = 26,
   RETRO_LANGUAGE_CZECH               = 27,
   RETRO_LANGUAGE_CATALAN_VALENCIA    = 28,
   RETRO_LANGUAGE_CATALAN             = 29,
   RETRO_LANGUAGE_BRITISH_ENGLISH     = 30,
   RETRO_LANGUAGE_HUNGARIAN           = 31,
   RETRO_LANGUAGE_LAST,

   /** Defined to ensure that <tt>sizeof(retro_language) == sizeof(int)</tt>. Do not use. */
   RETRO_LANGUAGE_DUMMY          = INT_MAX
};

/* Passed to retro_get_memory_data/size().
 * If the memory type doesn't apply to the
 * implementation NULL/0 can be returned.
 */
#define RETRO_MEMORY_MASK        0xff

/* Regular save RAM. This RAM is usually found on a game cartridge,
 * backed up by a battery.
 * If save game data is too complex for a single memory buffer,
 * the SAVE_DIRECTORY (preferably) or SYSTEM_DIRECTORY environment
 * callback can be used. */
#define RETRO_MEMORY_SAVE_RAM    0

/* Some games have a built-in clock to keep track of time.
 * This memory is usually just a couple of bytes to keep track of time.
 */
#define RETRO_MEMORY_RTC         1

/* System ram lets a frontend peek into a game systems main RAM. */
#define RETRO_MEMORY_SYSTEM_RAM  2

/* Video ram lets a frontend peek into a game systems video RAM (VRAM). */
#define RETRO_MEMORY_VIDEO_RAM   3

/* Keysyms used for ID in input state callback when polling RETRO_KEYBOARD. */
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
   RETROK_LEFTBRACE      = 123,
   RETROK_BAR            = 124,
   RETROK_RIGHTBRACE     = 125,
   RETROK_TILDE          = 126,
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
   RETROK_OEM_102        = 323,

   RETROK_LAST,

   RETROK_DUMMY          = INT_MAX /* Ensure sizeof(enum) == sizeof(int) */
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

   RETROKMOD_DUMMY = INT_MAX /* Ensure sizeof(enum) == sizeof(int) */
};

/**
 * @defgroup RETRO_ENVIRONMENT Environment Callbacks
 * @{
 */

/**
 * This bit indicates that the associated environment call is experimental,
 * and may be changed or removed in the future.
 * Frontends should mask out this bit before handling the environment call.
 */
#define RETRO_ENVIRONMENT_EXPERIMENTAL 0x10000
/* Environment callback to be used internally in frontend. */
#define RETRO_ENVIRONMENT_PRIVATE 0x20000

/* Environment commands. */
/**
 * Requests the frontend to set the screen rotation.
 *
 * @param data[in] <tt>const unsigned*</tt>.
 * Valid values are 0, 1, 2, and 3.
 * These numbers respectively set the screen rotation to 0, 90, 180, and 270 degrees counter-clockwise.
 * @returns \c true if the screen rotation was set successfully.
 */
#define RETRO_ENVIRONMENT_SET_ROTATION  1

/**
 * Queries whether the core should use overscan or not.
 *
 * @param data[out] <tt>bool*</tt>.
 * Set to \c true if the core should use overscan,
 * \c false if it should be cropped away.
 * @returns \c true if the environment call is available.
 * Does \em not indicate whether overscan should be used.
 * @deprecated As of 2019 this callback is considered deprecated in favor of
 * using core options to manage overscan in a more nuanced, core-specific way.
 */
#define RETRO_ENVIRONMENT_GET_OVERSCAN  2

/**
 * Queries whether the frontend supports frame duping,
 * in the form of passing \c NULL to the video frame callback.
 *
 * @param data[out] <tt>bool*</tt>.
 * Set to \c true if the frontend supports frame duping.
 * @returns \c true if the environment call is available.
 * @see retro_video_refresh_t
 */
#define RETRO_ENVIRONMENT_GET_CAN_DUPE  3

/*
 * Environ 4, 5 are no longer supported (GET_VARIABLE / SET_VARIABLES),
 * and reserved to avoid possible ABI clash.
 */

/**
 * @brief Displays a user-facing message for a short time.
 *
 * Use this callback to convey important status messages,
 * such as errors or the result of long-running operations.
 * For trivial messages or logging, use \c RETRO_ENVIRONMENT_GET_LOG_INTERFACE or \c stderr.
 *
 * @example
 * \code{.c}
 * void set_message_example(void)
 * {
 *    struct retro_message msg;
 *    msg.frames = 60 * 5; // 5 seconds
 *    msg.msg = "Hello world!";
 *
 *    environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &msg);
 * }
 * \endcode
 *
 * @deprecated Prefer using \c RETRO_ENVIRONMENT_SET_MESSAGE_EXT for new code,
 * as it offers more features.
 * Only use this environment call for compatibility with older cores or frontends.
 *
 * @param data[in] <tt>const struct retro_message*</tt>.
 * Details about the message to show to the user.
 * Behavior is undefined if <tt>NULL</tt>.
 * @returns \c true if the environment call is available.
 * @see retro_message
 * @see RETRO_ENVIRONMENT_GET_LOG_INTERFACE
 * @see RETRO_ENVIRONMENT_SET_MESSAGE_EXT
 * @see RETRO_ENVIRONMENT_SET_MESSAGE
 * @note The frontend must make its own copy of the message and the underlying string.
 */
#define RETRO_ENVIRONMENT_SET_MESSAGE   6

/**
 * Requests the frontend to shutdown the core.
 * Should only be used if the core can exit on its own,
 * such as from a menu item in a game
 * or an emulated power-off in an emulator.
 *
 * @param data Ignored.
 * @returns \c true if the environment call is available.
 */
#define RETRO_ENVIRONMENT_SHUTDOWN      7

/**
 * Gives a hint to the frontend of how demanding this core is on the system.
 * For example, reporting a level of 2 means that
 * this implementation should run decently on frontends
 * of level 2 and above.
 *
 * It can be used by the frontend to potentially warn
 * about too demanding implementations.
 *
 * The levels are "floating".
 *
 * This function can be called on a per-game basis,
 * as a core may have different demands for different games or settings.
 * If called, it should be called in <tt>retro_load_game()</tt>.
 * @param data[in] <tt>const unsigned*</tt>.
*/
#define RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL 8

/**
 * Returns the path to the frontend's system directory,
 * which can be used to store system-specific configuration
 * such as BIOS files or cached data.
 *
 * @param data[out] <tt>const char**</tt>.
 * Pointer to the \c char* in which the system directory will be saved.
 * The string is managed by the frontend and must not be modified or freed by the core.
 * May be \c NULL if no system directory is defined,
 * in which case the core should find an alternative directory.
 * @return \c true if the environment call is available,
 * even if the value returned in \c data is <tt>NULL</tt>.
 * @note Historically, some cores would use this folder for save data such as memory cards or SRAM.
 * This is now discouraged in favor of \c RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY.
 * @see RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY
 */
#define RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY 9

/**
 * Sets the internal pixel format used by the frontend for rendering.
 * The default pixel format is \c RETRO_PIXEL_FORMAT_0RGB1555 for compatibility reasons,
 * although it's considered deprecated and shouldn't be used by new code.
 *
 * @param data[in] <tt>const enum retro_pixel_format *</tt>.
 * Pointer to the pixel format to use.
 * @returns \c true if the pixel format was set successfully,
 * \c false if it's not supported or this callback is unavailable.
 * @note This function should be called inside \c retro_load_game()
 * or <tt>retro_get_system_av_info()</tt>.
 * @see retro_pixel_format
 */
#define RETRO_ENVIRONMENT_SET_PIXEL_FORMAT 10

/**
 * Sets an array of input descriptors for the frontend
 * to present to the user for configuring the core's controls.
 *
 * This function can be called at any time,
 * preferably early in the core's life cycle.
 * Ideally, no later than <tt>retro_load_game()<tt>.
 *
 * @param data[in] <tt>const struct retro_input_descriptor *</tt>.
 * An array of input descriptors terminated by one whose
 * \c retro_input_descriptor::description field is set to <tt>NULL</tt>.
 * Behavior is undefined if <tt>NULL</tt>.
 * @return \c true if the environment call is recognized.
 * @see retro_input_descriptor
 */
#define RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS 11

/**
 * Sets a callback function used to notify the core about keyboard events.
 * This should only be used for cores that specifically need keyboard input,
 * such as for home computer emulators or games with text entry.
 *
 * @param data[in] <tt>const struct retro_keyboard_callback *</tt>.
 * Pointer to the callback function.
 * Behavior is undefined if <tt>NULL</tt>.
 * @return \c true if the environment call is recognized.
 * @see retro_keyboard_callback
 * @see retro_key
 */
#define RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK 12

/**
 * Sets an interface that the frontend can use to insert and remove emulated disk images.
 *
 * @note This is intended for games that span multiple disks that are
 * manually swapped out by the user (e.g. PSX).
 * @param data[in] <tt>const struct retro_disk_control_callback *</tt>.
 * Pointer to the callback functions to use.
 * @see retro_disk_control_callback
 * @see RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE
 */
#define RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE 13

/**
 * Requests that a frontend enable a particular hardware rendering API.
 *
 * If successful, the frontend will create a context (and other related resources)
 * that the core can use for rendering.
 * The framebuffer will be at least as large as
 * the maximum dimensions provided in <tt>retro_get_system_av_info</tt>.
 *
 * @param data[in, out] <tt>struct retro_hw_render_callback *</tt>.
 * Pointer to the hardware render callback struct.
 * Used to define callbacks for the hardware-rendering life cycle,
 * as well as to request a particular rendering API.
 * @return \c true if the environment call is recognized
 * and the requested rendering API is supported.
 * \c false if \c data is \c NULL
 * or the frontend can't provide the requested rendering API.
 * @see retro_hw_render_callback
 * @see retro_video_refresh_t
 * @see RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER
 * @note Should be called in <tt>retro_load_game()</tt>.
 * @note If HW rendering is used, pass only \c RETRO_HW_FRAME_BUFFER_VALID or
 * \c NULL to <tt>retro_video_refresh_t</tt>.
 */
#define RETRO_ENVIRONMENT_SET_HW_RENDER 14

/**
 * Retrieves a core option's value from the frontend.
 * \c retro_variable::key should be set to an option key
 * that was previously set in \c RETRO_ENVIRONMENT_SET_VARIABLES
 * (or a similar environment call).
 *
 * @param data[in,out] <tt>struct retro_variable *</tt>.
 * Pointer to a single \c retro_variable struct.
 * See the documentation for \c retro_variable for details
 * on which fields are set by the frontend or core.
 * May be \c NULL.
 * @returns \c true if the environment call is available,
 * even if \c data is \c NULL or the key it specifies is not found.
 * @note Passing \c NULL in to \c data can be useful to
 * test for support of this environment call without looking up any variables.
 * @see retro_variable
 * @see RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2
 * @see RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE
 */
#define RETRO_ENVIRONMENT_GET_VARIABLE 15

/**
 * Notifies the frontend of the core's available options.
 *
 * The core may check these options later using \c RETRO_ENVIRONMENT_GET_VARIABLE.
 * The frontend may also present these options to the user
 * in its own configuration UI.
 *
 * This should be called the first time as early as possible,
 * ideally in \c retro_set_environment.
 * The core may later call this function again
 * to communicate updated options to the frontend,
 * but the number of core options must not change.
 *
 * retro_variable::value should be formatted as follows:
 *
 * <ul>
 * <li>The text before the first \c ';' is the option's human-readable title.</li>
 * <li>A single space follows the \c ';'.</li>
 * <li>The rest of the string is a <tt>'|'</tt>-delimited list of possible values,
 * with the first one being the default.</li>
 * </ul>
 *
 * Here's an example that sets two options.
 *
 * @code
 * void set_variables_example(void)
 * {
 *    struct retro_variable options[] = {
 *        { "foo_speedhack", "Speed hack; false|true" }, // false by default
 *        { "foo_displayscale", "Display scale factor; 1|2|3|4" }, // 1 by default
 *        { NULL, NULL },
 *    };
 *
 *    environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, &options);
 * }
 * @endcode
 *
 * The possible values will generally be displayed and stored as-is by the frontend.
 *
 * @deprecated Prefer using \c RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 for new code,
 * as it offers more features such as categories and translation.
 * Only use this environment call to maintain compatibility
 * with older frontends or cores.
 * @note Keep the available options (and their possible values) as low as possible;
 * it should be feasible to cycle through them without a keyboard.
 * @param data[in] <tt>const struct retro_variable *</tt>.
 * Pointer to an array of \c retro_variable structs that define available core options,
 * terminated by a <tt>{ NULL, NULL }</tt> element.
 * The frontend must maintain its own copy of this array.
 *
 * @returns \c true if the environment call is available,
 * even if \c data is <tt>NULL</tt>.
 * @see retro_variable
 * @see RETRO_ENVIRONMENT_GET_VARIABLE
 * @see RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE
 * @see RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2
 */
#define RETRO_ENVIRONMENT_SET_VARIABLES 16

/**
 * Queries whether at least one core option was updated by the frontend
 * since the last call to <tt>RETRO_ENVIRONMENT_GET_VARIABLE</tt>.
 * This typically means that the user opened the core options menu and made some changes.
 *
 * Cores usually call this each frame before the core's main emulation logic.
 * Specific options can then be queried with <tt>RETRO_ENVIRONMENT_GET_VARIABLE</tt>.
 *
 * @param data[out] <tt>bool *</tt>.
 * Set to \c true if at least one core option was updated
 * since the last call to <tt>RETRO_ENVIRONMENT_GET_VARIABLE</tt>.
 * Behavior is undefined if this pointer is <tt>NULL</tt>.
 * @returns \c true if the environment call is available.
 * @see RETRO_ENVIRONMENT_GET_VARIABLE
 * @see RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2
 */
#define RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE 17

/**
 * Notifies the frontend that this core can run without loading any content,
 * such as when emulating a console that has built-in software.
 * When a core is loaded without content,
 * \c retro_load_game receives an argument of <tt>NULL</tt>.
 * This should be called within \c retro_set_environment() only.
 *
 * @param data[in] <tt>const bool *</tt>.
 * Pointer to a single \c bool that indicates whether this frontend can run without content.
 * Can point to a value of \c false but this isn't necessary,
 * as contentless support is opt-in.
 * The behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available.
 * @see retro_load_game
 */
#define RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME 18

/**
 * Retrieves the absolute path from which this core was loaded.
 * Useful when loading assets from paths relative to the core,
 * as is sometimes the case when using <tt>RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME</tt>.
 *
 * @param data[out] <tt>const char **</tt>.
 * Pointer to a string in which the core's path will be saved.
 * The string is managed by the frontend and must not be modified or freed by the core.
 * May be \c NULL if the core is statically linked to the frontend
 * or if the core's path otherwise cannot be determined.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available.
 */
#define RETRO_ENVIRONMENT_GET_LIBRETRO_PATH 19

/* Environment call 20 was an obsolete version of SET_AUDIO_CALLBACK.
 * It was not used by any known core at the time, and was removed from the API.
 * The number 20 is reserved to prevent ABI clashes.
 */

/**
 * Sets a callback that notifies the core of how much time has passed
 * since the last iteration of <tt>retro_run</tt>.
 * If the frontend is not running the core in real time
 * (e.g. it's frame-stepping or running in slow motion),
 * then the reference value will be provided to the callback instead.
 *
 * @param data[in] <tt>const struct retro_frame_time_callback *</tt>.
 * Pointer to a single \c retro_frame_time_callback struct.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available.
 * @note Frontends may disable this environment call in certain situations.
 * It will return \c false in those cases.
 * @see retro_frame_time_callback
 */
#define RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK 21

/**
 * Registers a set of functions that the frontend can use
 * to tell the core it's ready for audio output.
 *
 * It is intended for games that feature asynchronous audio.
 * It should not be used for emulators unless their audio is asynchronous.
 *
 *
 * The callback only notifies about writability; the libretro core still
 * has to call the normal audio callbacks
 * to write audio. The audio callbacks must be called from within the
 * notification callback.
 * The amount of audio data to write is up to the core.
 * Generally, the audio callback will be called continously in a loop.
 *
 * A frontend may disable this callback in certain situations.
 * The core must be able to render audio with the "normal" interface.
 *
 * @param data[in] <tt>const struct retro_audio_callback *</tt>.
 * Pointer to a set of functions that the frontend will call to notify the core
 * when it's ready to receive audio data.
 * May be \c NULL, in which case the frontend will return
 * whether this environment callback is available.
 * @return \c true if this environment call is available,
 * even if \c data is \c NULL.
 * @warning The provided callbacks can be invoked from any thread,
 * so their implementations \em must be thread-safe.
 * @note If a core uses this callback,
 * it should also use <tt>RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK</tt>.
 * @see retro_audio_callback
 * @see retro_audio_sample_t
 * @see retro_audio_sample_batch_t
 * @see RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK
 */
#define RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK 22

/**
 * Gets an interface that a core can use to access a controller's rumble motors.
 *
 * The interface supports two independently-controlled motors,
 * one strong and one weak.
 *
 * Should be called from either \c retro_init() or \c retro_load_game(),
 * but not from \c retro_set_environment().
 *
 * @param data[out] <tt>struct retro_rumble_interface *</tt>.
 * Pointer to the interface struct.
 * Behavior is undefined if \c NULL.
 * @returns \c true if the environment call is available,
 * even if the current device doesn't support vibration.
 * @see retro_rumble_interface
 * @defgroup GET_RUMBLE_INTERFACE Rumble Interface
 */
#define RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE 23

/**
 * Returns the frontend's supported input device types.
 *
 * The supported device types are returned as a bitmask,
 * with each value of \ref RETRO_DEVICE corresponding to a bit.
 *
 * Should only be called in \c retro_run().
 *
 * @code
 * void get_input_device_capabilities_example(void)
 * {
 *    uint64_t capabilities;
 *    environ_cb(RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES, &capabilities);
 *    if (capabilities & ((1 << RETRO_DEVICE_JOYPAD) | (1 << RETRO_DEVICE_ANALOG)))
 *      printf("Joypad and analog device types are supported");
 * }
 * @endcode
 *
 * @param data[out] <tt>uint64_t *</tt>.
 * Pointer to a bitmask of supported input device types.
 * If the frontend supports a particular \c RETRO_DEVICE_* type,
 * then the bit <tt>(1 << RETRO_DEVICE_*)</tt> will be set.
 *
 * Each bit represents a \c RETRO_DEVICE constant,
 * e.g. bit 1 represents \c RETRO_DEVICE_JOYPAD,
 * bit 2 represents \c RETRO_DEVICE_MOUSE, and so on.
 *
 * Bits that do not correspond to known device types will be set to zero
 * and are reserved for future use.
 *
 * Behavior is undefined if \c NULL.
 * @returns \c true if the environment call is available.
 * @note If the frontend supports multiple input drivers,
 * availability of this environment call (and the reported capabilities)
 * may depend on the active driver.
 * @see RETRO_DEVICE
 */
#define RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES 24

/**
 * Returns an interface that the core can use to access and configure available sensors,
 * such as an accelerometer or gyroscope.
 *
 * @param data[out] <tt>struct retro_sensor_interface *</tt>.
 * Pointer to the sensor interface that the frontend will populate.
 * Behavior is undefined if is \c NULL.
 * @returns \c true if the environment call is available,
 * even if the device doesn't have any supported sensors.
 * @see retro_sensor_interface
 * @see retro_sensor_action
 * @see RETRO_SENSOR
 * @addtogroup RETRO_SENSOR
 */
#define RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE (25 | RETRO_ENVIRONMENT_EXPERIMENTAL)

/**
 * Gets an interface to the device's video camera.
 *
 * The frontend delivers new video frames via a user-defined callback
 * that runs in the same thread as <tt>retro_run()</tt>.
 *
 * Should be called in retro_load_game().
 *
 * Camera frames may be provided as a raw frame buffer or as an OpenGL texture,
 * depending on the camera driver's support and the core's requested capabilities.
 *
 * Video frames may only be provided as OpenGL textures
 * when the core is using an OpenGL context via <tt>RETRO_ENVIRONMENT_SET_HW_RENDER</tt>.
 *
 * The camera is not started automatically;
 * the retrieved interface functions must be used to explicitly do so.
 *
 * @param data[in,out] <tt>struct retro_camera_callback *</tt>.
 * Pointer to the camera driver interface.
 * Some fields in the struct must be filled in by the core,
 * others are provided by the frontend.
 * @returns \c true if the environment call is available,
 * even if camera support isn't.
 * @note This API only supports one video camera at a time.
 * If the device provides multiple cameras (e.g. inner/outer cameras on a phone),
 * the frontend will choose one to use.
 * @see retro_camera_callback
 * @see RETRO_ENVIRONMENT_SET_HW_RENDER
 */
#define RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE (26 | RETRO_ENVIRONMENT_EXPERIMENTAL)

/**
 * Gets an interface that the core can use for cross-platform logging.
 * Certain platforms don't have a console or <tt>stderr</tt>,
 * or they have their own preferred logging methods.
 * The frontend itself may also display log output.
 *
 * @param data[out] <tt>struct retro_log_callback *</tt>.
 * Pointer to the callback where the function pointer will be saved.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available.
 * @see retro_log_callback
 * @note Cores can fall back to \c stderr if this interface is not available.
 */
#define RETRO_ENVIRONMENT_GET_LOG_INTERFACE 27

#define RETRO_ENVIRONMENT_GET_PERF_INTERFACE 28
                                           /* struct retro_perf_callback * --
                                            * Gets an interface for performance counters. This is useful
                                            * for performance logging in a cross-platform way and for detecting
                                            * architecture-specific features, such as SIMD support.
                                            */
#define RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE 29
                                           /* struct retro_location_callback * --
                                            * Gets access to the location interface.
                                            * The purpose of this interface is to be able to retrieve
                                            * location-based information from the host device,
                                            * such as current latitude / longitude.
                                            */

/**
 * @deprecated An obsolete alias to \c RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY kept for compatibility.
 * @see RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY
 **/
#define RETRO_ENVIRONMENT_GET_CONTENT_DIRECTORY 30

/**
 * Returns the frontend's "core assets" directory,
 * which can be used to store assets that the core needs
 * such as art assets or level data.
 *
 * @param data[out] <tt>const char **</tt>.
 * Pointer to a string in which the core assets directory will be saved.
 * This string is managed by the frontend and must not be modified or freed by the core.
 * May be \c NULL if no core assets directory is defined,
 * in which case the core should find an alternative directory.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available,
 * even if the value returned in \c data is <tt>NULL</tt>.
 */
#define RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY 30

/**
 * Returns the frontend's save data directory, if available.
 * This directory should be used to store game-specific save data,
 * including memory card images.
 *
 * Although libretro provides an interface for cores to expose SRAM to the frontend,
 * not all cores can support it correctly.
 * In this case, cores should use this environment callback
 * to save their game data to disk manually.
 *
 * Cores that use this environment callback
 * should flush their save data to disk periodically and when unloading.
 *
 * @param data[out] <tt>const char **</tt>.
 * Pointer to the string in which the save data directory will be saved.
 * This string is managed by the frontend and must not be modified or freed by the core.
 * May return \c NULL if no save data directory is defined.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available,
 * even if the value returned in \c data is <tt>NULL</tt>.
 * @note Early libretro cores used \c RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY for save data.
 * This is still supported for backwards compatibility,
 * but new cores should use this environment call instead.
 * \c RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY should be used for game-agnostic data
 * such as BIOS files or core-specific configuration.
 * @note The returned directory may or may not be the same
 * as the one used for \c retro_get_memory_data.
 *
 * @see retro_get_memory_data
 * @see RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY
 */
#define RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY 31

/**
 * Sets new video and audio parameters for the core.
 * This can only be called from within <tt>retro_run</tt>.
 *
 * This environment call may entail a full reinitialization of the frontend's audio/video drivers,
 * hence it should \em only be used if the core needs to make drastic changes
 * to audio/video parameters.
 *
 * This environment call should \em not be used when:
 * <ul>
 * <li>Changing the emulated system's internal resolution,
 * within the limits defined by the existing values of \c max_width and \c max_height.
 * Use \c RETRO_ENVIRONMENT_SET_GEOMETRY instead,
 * and adjust \c retro_get_system_av_info to account fo
 * supported scale factors and screen layouts
 * when computing \c max_width and \c max_height.
 * Only use this environment call if \c max_width or \c max_height needs to increase.
 * <li>Adjusting the screen's aspect ratio,
 * e.g. when changing the layout of the screen(s).
 * Use \c RETRO_ENVIRONMENT_SET_GEOMETRY or \c RETRO_ENVIRONMENT_SET_ROTATION instead.
 * </ul>
 *
 * The frontend will reinitialize its audio and video drivers within this callback;
 * after that happens, audio and video callbacks will target the newly-initialized driver,
 * even within the same \c retro_run call.
 *
 * This callback makes it possible to support configurable resolutions
 * while avoiding the need to compute the "worst case" values of \c max_width and \c max_height.
 *
 * @param data[in] <tt>const struct retro_system_av_info *</tt>.
 * Pointer to the new video and audio parameters that the frontend should adopt.
 * @returns \c true if the environment call is available
 * and the new av_info struct was accepted.
 * \c false if the environment call is unavailable or \c data is <tt>NULL</tt>.
 * @see retro_system_av_info
 * @see RETRO_ENVIRONMENT_SET_GEOMETRY
 */
#define RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO 32

/**
 * Provides an interface that a frontend can use
 * to get function pointers from the core.
 *
 * This allows cores to define their own extensions to the libretro API,
 * or to expose implementations of a frontend's libretro extensions.
 *
 * @param data[in] <tt>const struct retro_get_proc_address_interface *</tt>.
 * Pointer to the interface that the frontend can use to get function pointers from the core.
 * The frontend must maintain its own copy of this interface.
 * @returns \c true if the environment call is available
 * and the returned interface was accepted.
 * @note The provided interface may be called at any time,
 * even before this environment call returns.
 * @note Extensions should be prefixed with the name of the frontend or core that defines them.
 * For example, a frontend named "foo" that defines a debugging extension
 * should expect the core to define functions prefixed with "foo_debug_".
 * @warning If a core wants to use this environment call,
 * it \em must do so from within \c retro_set_environment().
 * @see retro_get_proc_address_interface
 */
#define RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK 33

#define RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO 34
                                           /* const struct retro_subsystem_info * --
                                            * This environment call introduces the concept of libretro "subsystems".
                                            * A subsystem is a variant of a libretro core which supports
                                            * different kinds of games.
                                            * The purpose of this is to support e.g. emulators which might
                                            * have special needs, e.g. Super Nintendo's Super GameBoy, Sufami Turbo.
                                            * It can also be used to pick among subsystems in an explicit way
                                            * if the libretro implementation is a multi-system emulator itself.
                                            *
                                            * Loading a game via a subsystem is done with retro_load_game_special(),
                                            * and this environment call allows a libretro core to expose which
                                            * subsystems are supported for use with retro_load_game_special().
                                            * A core passes an array of retro_game_special_info which is terminated
                                            * with a zeroed out retro_game_special_info struct.
                                            *
                                            * If a core wants to use this functionality, SET_SUBSYSTEM_INFO
                                            * **MUST** be called from within retro_set_environment().
                                            */
#define RETRO_ENVIRONMENT_SET_CONTROLLER_INFO 35
                                           /* const struct retro_controller_info * --
                                            * This environment call lets a libretro core tell the frontend
                                            * which controller subclasses are recognized in calls to
                                            * retro_set_controller_port_device().
                                            *
                                            * Some emulators such as Super Nintendo support multiple lightgun
                                            * types which must be specifically selected from. It is therefore
                                            * sometimes necessary for a frontend to be able to tell the core
                                            * about a special kind of input device which is not specifcally
                                            * provided by the Libretro API.
                                            *
                                            * In order for a frontend to understand the workings of those devices,
                                            * they must be defined as a specialized subclass of the generic device
                                            * types already defined in the libretro API.
                                            *
                                            * The core must pass an array of const struct retro_controller_info which
                                            * is terminated with a blanked out struct. Each element of the
                                            * retro_controller_info struct corresponds to the ascending port index
                                            * that is passed to retro_set_controller_port_device() when that function
                                            * is called to indicate to the core that the frontend has changed the
                                            * active device subclass. SEE ALSO: retro_set_controller_port_device()
                                            *
                                            * The ascending input port indexes provided by the core in the struct
                                            * are generally presented by frontends as ascending User # or Player #,
                                            * such as Player 1, Player 2, Player 3, etc. Which device subclasses are
                                            * supported can vary per input port.
                                            *
                                            * The first inner element of each entry in the retro_controller_info array
                                            * is a retro_controller_description struct that specifies the names and
                                            * codes of all device subclasses that are available for the corresponding
                                            * User or Player, beginning with the generic Libretro device that the
                                            * subclasses are derived from. The second inner element of each entry is the
                                            * total number of subclasses that are listed in the retro_controller_description.
                                            *
                                            * NOTE: Even if special device types are set in the libretro core,
                                            * libretro should only poll input based on the base input device types.
                                            */
#define RETRO_ENVIRONMENT_SET_MEMORY_MAPS (36 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* const struct retro_memory_map * --
                                            * This environment call lets a libretro core tell the frontend
                                            * about the memory maps this core emulates.
                                            * This can be used to implement, for example, cheats in a core-agnostic way.
                                            *
                                            * Should only be used by emulators; it doesn't make much sense for
                                            * anything else.
                                            * It is recommended to expose all relevant pointers through
                                            * retro_get_memory_* as well.
                                            */
#define RETRO_ENVIRONMENT_SET_GEOMETRY 37
                                           /* const struct retro_game_geometry * --
                                            * This environment call is similar to SET_SYSTEM_AV_INFO for changing
                                            * video parameters, but provides a guarantee that drivers will not be
                                            * reinitialized.
                                            * This can only be called from within retro_run().
                                            *
                                            * The purpose of this call is to allow a core to alter nominal
                                            * width/heights as well as aspect ratios on-the-fly, which can be
                                            * useful for some emulators to change in run-time.
                                            *
                                            * max_width/max_height arguments are ignored and cannot be changed
                                            * with this call as this could potentially require a reinitialization or a
                                            * non-constant time operation.
                                            * If max_width/max_height are to be changed, SET_SYSTEM_AV_INFO is required.
                                            *
                                            * A frontend must guarantee that this environment call completes in
                                            * constant time.
                                            */

/**
 * Returns the name of the user, if possible.
 * This callback is suitable for cores that offer personalization,
 * such as online facilities or user profiles on the emulated system.
 * @param data[out] <tt>const char **</tt>.
 * Pointer to the user name string.
 * May be <tt>NULL</tt>, in which case the core should use a default name.
 * The returned pointer is owned by the frontend and must not be modified or freed by the core.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available,
 * even if the frontend couldn't provide a name.
 */
#define RETRO_ENVIRONMENT_GET_USERNAME 38

/**
 * Returns the frontend's configured language.
 * It can be used to localize the core's UI,
 * or to customize the emulated firmware if applicable.
 *
 * @param data[out] <tt>retro_language *</tt>.
 * Pointer to the language identifier.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available.
 * @note The returned language may not be the same as the operating system's language.
 * Cores should fall back to the operating system's language (or to English)
 * if the environment call is unavailable or the returned language is unsupported.
 * @see retro_language
 * @see RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL
 */
#define RETRO_ENVIRONMENT_GET_LANGUAGE 39

#define RETRO_ENVIRONMENT_GET_CURRENT_SOFTWARE_FRAMEBUFFER (40 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_framebuffer * --
                                            * Returns a preallocated framebuffer which the core can use for rendering
                                            * the frame into when not using SET_HW_RENDER.
                                            * The framebuffer returned from this call must not be used
                                            * after the current call to retro_run() returns.
                                            *
                                            * The goal of this call is to allow zero-copy behavior where a core
                                            * can render directly into video memory, avoiding extra bandwidth cost by copying
                                            * memory from core to video memory.
                                            *
                                            * If this call succeeds and the core renders into it,
                                            * the framebuffer pointer and pitch can be passed to retro_video_refresh_t.
                                            * If the buffer from GET_CURRENT_SOFTWARE_FRAMEBUFFER is to be used,
                                            * the core must pass the exact
                                            * same pointer as returned by GET_CURRENT_SOFTWARE_FRAMEBUFFER;
                                            * i.e. passing a pointer which is offset from the
                                            * buffer is undefined. The width, height and pitch parameters
                                            * must also match exactly to the values obtained from GET_CURRENT_SOFTWARE_FRAMEBUFFER.
                                            *
                                            * It is possible for a frontend to return a different pixel format
                                            * than the one used in SET_PIXEL_FORMAT. This can happen if the frontend
                                            * needs to perform conversion.
                                            *
                                            * It is still valid for a core to render to a different buffer
                                            * even if GET_CURRENT_SOFTWARE_FRAMEBUFFER succeeds.
                                            *
                                            * A frontend must make sure that the pointer obtained from this function is
                                            * writeable (and readable).
                                            */
#define RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE (41 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* const struct retro_hw_render_interface ** --
                                            * Returns an API specific rendering interface for accessing API specific data.
                                            * Not all HW rendering APIs support or need this.
                                            * The contents of the returned pointer is specific to the rendering API
                                            * being used. See the various headers like libretro_vulkan.h, etc.
                                            *
                                            * GET_HW_RENDER_INTERFACE cannot be called before context_reset has been called.
                                            * Similarly, after context_destroyed callback returns,
                                            * the contents of the HW_RENDER_INTERFACE are invalidated.
                                            */

/**
 * Notifies the frontend that this core supports achievements.
 * The core must expose its emulated address space via
 * \c retro_get_memory_data or \c RETRO_ENVIRONMENT_GET_MEMORY_MAPS.
 *
 * Must be called before the first call to <tt>retro_run</tt>.
 *
 * @param data[in] <tt>const bool *</tt>.
 * Pointer to a single \c bool that indicates whether this core supports achievements.
 * Can be \c false but this isn't necessary,
 * as cores must opt in to achievement support.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if the environment call is available.
 * @see RETRO_ENVIRONMENT_SET_MEMORY_MAPS
 * @see retro_get_memory_data
 */
#define RETRO_ENVIRONMENT_SET_SUPPORT_ACHIEVEMENTS (42 | RETRO_ENVIRONMENT_EXPERIMENTAL)

#define RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE (43 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* const struct retro_hw_render_context_negotiation_interface * --
                                            * Sets an interface which lets the libretro core negotiate with frontend how a context is created.
                                            * The semantics of this interface depends on which API is used in SET_HW_RENDER earlier.
                                            * This interface will be used when the frontend is trying to create a HW rendering context,
                                            * so it will be used after SET_HW_RENDER, but before the context_reset callback.
                                            */
#define RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS 44
                                           /* uint64_t * --
                                            * Sets quirk flags associated with serialization. The frontend will zero any flags it doesn't
                                            * recognize or support. Should be set in either retro_init or retro_load_game, but not both.
                                            */

/**
 * The frontend will try to use a "shared" context when setting up a hardware context.
 * Mostly applicable to OpenGL.
 *
 * In order for this to have any effect,
 * the core must call \c RETRO_ENVIRONMENT_SET_HW_RENDER at some point
 * if it hasn't already.
 *
 * @param data Ignored.
 * @returns \c true if the environment call is available
 * and the frontend supports shared hardware contexts.
 */
#define RETRO_ENVIRONMENT_SET_HW_SHARED_CONTEXT (44 | RETRO_ENVIRONMENT_EXPERIMENTAL)

#define RETRO_ENVIRONMENT_GET_VFS_INTERFACE (45 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_vfs_interface_info * --
                                            * Gets access to the VFS interface.
                                            * VFS presence needs to be queried prior to load_game or any
                                            * get_system/save/other_directory being called to let front end know
                                            * core supports VFS before it starts handing out paths.
                                            * It is recomended to do so in retro_set_environment
                                            */
#define RETRO_ENVIRONMENT_GET_LED_INTERFACE (46 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_led_interface * --
                                            * Gets an interface which is used by a libretro core to set
                                            * state of LEDs.
                                            */
#define RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE (47 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* int * --
                                            * Tells the core if the frontend wants audio or video.
                                            * If disabled, the frontend will discard the audio or video,
                                            * so the core may decide to skip generating a frame or generating audio.
                                            * This is mainly used for increasing performance.
                                            * Bit 0 (value 1): Enable Video
                                            * Bit 1 (value 2): Enable Audio
                                            * Bit 2 (value 4): Use Fast Savestates.
                                            * Bit 3 (value 8): Hard Disable Audio
                                            * Other bits are reserved for future use and will default to zero.
                                            * If video is disabled:
                                            * * The frontend wants the core to not generate any video,
                                            *   including presenting frames via hardware acceleration.
                                            * * The frontend's video frame callback will do nothing.
                                            * * After running the frame, the video output of the next frame should be
                                            *   no different than if video was enabled, and saving and loading state
                                            *   should have no issues.
                                            * If audio is disabled:
                                            * * The frontend wants the core to not generate any audio.
                                            * * The frontend's audio callbacks will do nothing.
                                            * * After running the frame, the audio output of the next frame should be
                                            *   no different than if audio was enabled, and saving and loading state
                                            *   should have no issues.
                                            * Fast Savestates:
                                            * * Guaranteed to be created by the same binary that will load them.
                                            * * Will not be written to or read from the disk.
                                            * * Suggest that the core assumes loading state will succeed.
                                            * * Suggest that the core updates its memory buffers in-place if possible.
                                            * * Suggest that the core skips clearing memory.
                                            * * Suggest that the core skips resetting the system.
                                            * * Suggest that the core may skip validation steps.
                                            * Hard Disable Audio:
                                            * * Used for a secondary core when running ahead.
                                            * * Indicates that the frontend will never need audio from the core.
                                            * * Suggests that the core may stop synthesizing audio, but this should not
                                            *   compromise emulation accuracy.
                                            * * Audio output for the next frame does not matter, and the frontend will
                                            *   never need an accurate audio state in the future.
                                            * * State will never be saved when using Hard Disable Audio.
                                            */
#define RETRO_ENVIRONMENT_GET_MIDI_INTERFACE (48 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_midi_interface ** --
                                            * Returns a MIDI interface that can be used for raw data I/O.
                                            */

/**
 * Asks the frontend if it's currently in fast-forward mode.
 * @param data[out] <tt>bool *</tt>.
 * Set to \c true if the frontend is currently fast-forwarding its main loop.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @returns \c true if this environment call is available,
 * regardless of the value returned in \c data.
 */
#define RETRO_ENVIRONMENT_GET_FASTFORWARDING (49 | RETRO_ENVIRONMENT_EXPERIMENTAL)

/**
 * Returns the refresh rate the frontend is targeting, in Hz.
 * The intended use case is for the core to use the result to select an ideal refresh rate.
 *
 * @param data[out] <tt>float *</tt>.
 * Pointer to the \c float in which the frontend will store its target refresh rate.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * @return \c true if this environment call is available,
 * regardless of the value returned in \c data.
*/
#define RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE (50 | RETRO_ENVIRONMENT_EXPERIMENTAL)

#define RETRO_ENVIRONMENT_GET_INPUT_BITMASKS (51 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                            /* bool * --
                                            * Boolean value that indicates whether or not the frontend supports
                                            * input bitmasks being returned by retro_input_state_t. The advantage
                                            * of this is that retro_input_state_t has to be only called once to
                                            * grab all button states instead of multiple times.
                                            *
                                            * If it returns true, you can pass RETRO_DEVICE_ID_JOYPAD_MASK as 'id'
                                            * to retro_input_state_t (make sure 'device' is set to RETRO_DEVICE_JOYPAD).
                                            * It will return a bitmask of all the digital buttons.
                                            */

#define RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION 52
                                           /* unsigned * --
                                            * Unsigned value is the API version number of the core options
                                            * interface supported by the frontend. If callback return false,
                                            * API version is assumed to be 0.
                                            *
                                            * In legacy code, core options are set by passing an array of
                                            * retro_variable structs to RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This may be still be done regardless of the core options
                                            * interface version.
                                            *
                                            * If version is >= 1 however, core options may instead be set by
                                            * passing an array of retro_core_option_definition structs to
                                            * RETRO_ENVIRONMENT_SET_CORE_OPTIONS, or a 2D array of
                                            * retro_core_option_definition structs to RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL.
                                            * This allows the core to additionally set option sublabel information
                                            * and/or provide localisation support.
                                            *
                                            * If version is >= 2, core options may instead be set by passing
                                            * a retro_core_options_v2 struct to RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
                                            * or an array of retro_core_options_v2 structs to
                                            * RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL. This allows the core
                                            * to additionally set optional core option category information
                                            * for frontends with core option category support.
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS 53
                                           /* const struct retro_core_option_definition ** --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should only be called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
                                            * returns an API version of >= 1.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterwards it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            *
                                            * 'data' points to an array of retro_core_option_definition structs
                                            * terminated by a { NULL, NULL, NULL, {{0}}, NULL } element.
                                            * retro_core_option_definition::key should be namespaced to not collide
                                            * with other implementations' keys. e.g. A core called
                                            * 'foo' should use keys named as 'foo_option'.
                                            * retro_core_option_definition::desc should contain a human readable
                                            * description of the key.
                                            * retro_core_option_definition::info should contain any additional human
                                            * readable information text that a typical user may need to
                                            * understand the functionality of the option.
                                            * retro_core_option_definition::values is an array of retro_core_option_value
                                            * structs terminated by a { NULL, NULL } element.
                                            * > retro_core_option_definition::values[index].value is an expected option
                                            *   value.
                                            * > retro_core_option_definition::values[index].label is a human readable
                                            *   label used when displaying the value on screen. If NULL,
                                            *   the value itself is used.
                                            * retro_core_option_definition::default_value is the default core option
                                            * setting. It must match one of the expected option values in the
                                            * retro_core_option_definition::values array. If it does not, or the
                                            * default value is NULL, the first entry in the
                                            * retro_core_option_definition::values array is treated as the default.
                                            *
                                            * The number of possible option values should be very limited,
                                            * and must be less than RETRO_NUM_CORE_OPTION_VALUES_MAX.
                                            * i.e. it should be feasible to cycle through options
                                            * without a keyboard.
                                            *
                                            * Example entry:
                                            * {
                                            *     "foo_option",
                                            *     "Speed hack coprocessor X",
                                            *     "Provides increased performance at the expense of reduced accuracy",
                                            * 	  {
                                            *         { "false",    NULL },
                                            *         { "true",     NULL },
                                            *         { "unstable", "Turbo (Unstable)" },
                                            *         { NULL, NULL },
                                            *     },
                                            *     "false"
                                            * }
                                            *
                                            * Only strings are operated on. The possible values will
                                            * generally be displayed and stored as-is by the frontend.
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL 54
                                           /* const struct retro_core_options_intl * --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should only be called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
                                            * returns an API version of >= 1.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_CORE_OPTIONS.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterwards it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            *
                                            * This is fundamentally the same as RETRO_ENVIRONMENT_SET_CORE_OPTIONS,
                                            * with the addition of localisation support. The description of the
                                            * RETRO_ENVIRONMENT_SET_CORE_OPTIONS callback should be consulted
                                            * for further details.
                                            *
                                            * 'data' points to a retro_core_options_intl struct.
                                            *
                                            * retro_core_options_intl::us is a pointer to an array of
                                            * retro_core_option_definition structs defining the US English
                                            * core options implementation. It must point to a valid array.
                                            *
                                            * retro_core_options_intl::local is a pointer to an array of
                                            * retro_core_option_definition structs defining core options for
                                            * the current frontend language. It may be NULL (in which case
                                            * retro_core_options_intl::us is used by the frontend). Any items
                                            * missing from this array will be read from retro_core_options_intl::us
                                            * instead.
                                            *
                                            * NOTE: Default core option values are always taken from the
                                            * retro_core_options_intl::us array. Any default values in
                                            * retro_core_options_intl::local array will be ignored.
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY 55
                                           /* struct retro_core_option_display * --
                                            *
                                            * Allows an implementation to signal the environment to show
                                            * or hide a variable when displaying core options. This is
                                            * considered a *suggestion*. The frontend is free to ignore
                                            * this callback, and its implementation not considered mandatory.
                                            *
                                            * 'data' points to a retro_core_option_display struct
                                            *
                                            * retro_core_option_display::key is a variable identifier
                                            * which has already been set by SET_VARIABLES/SET_CORE_OPTIONS.
                                            *
                                            * retro_core_option_display::visible is a boolean, specifying
                                            * whether variable should be displayed
                                            *
                                            * Note that all core option variables will be set visible by
                                            * default when calling SET_VARIABLES/SET_CORE_OPTIONS.
                                            */

/**
 * Returns the frontend's preferred hardware rendering API.
 * Cores should use this information to decide which API to use with \c RETRO_ENVIRONMENT_SET_HW_RENDER.
 * @param data[out] <tt>retro_hw_context_type *</tt>.
 * Pointer to the hardware context type.
 * Behavior is undefined if \c data is <tt>NULL</tt>.
 * This value will be set even if the environment call returns <tt>false</tt>,
 * unless the frontend doesn't implement it.
 * @returns \c true if the environment call is available
 * and the frontend is able to use a hardware rendering API besides the one returned.
 * If \c false is returned and the core cannot use the preferred rendering API,
 * then it should exit or fall back to software rendering.
 * @note The returned value does not indicate which API is currently in use.
 * For example, the frontend may return \c RETRO_HW_CONTEXT_OPENGL
 * while a Direct3D context from a previous session is active;
 * this would signal that the frontend's current preference is for OpenGL.
 * @see retro_hw_context_type
 * @see RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE
 * @see RETRO_ENVIRONMENT_SET_HW_RENDER
 */
#define RETRO_ENVIRONMENT_GET_PREFERRED_HW_RENDER 56

#define RETRO_ENVIRONMENT_GET_DISK_CONTROL_INTERFACE_VERSION 57
                                           /* unsigned * --
                                            * Unsigned value is the API version number of the disk control
                                            * interface supported by the frontend. If callback return false,
                                            * API version is assumed to be 0.
                                            *
                                            * In legacy code, the disk control interface is defined by passing
                                            * a struct of type retro_disk_control_callback to
                                            * RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE.
                                            * This may be still be done regardless of the disk control
                                            * interface version.
                                            *
                                            * If version is >= 1 however, the disk control interface may
                                            * instead be defined by passing a struct of type
                                            * retro_disk_control_ext_callback to
                                            * RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE.
                                            * This allows the core to provide additional information about
                                            * disk images to the frontend and/or enables extra
                                            * disk control functionality by the frontend.
                                            */

#define RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE 58
                                           /* const struct retro_disk_control_ext_callback * --
                                            * Sets an interface which frontend can use to eject and insert
                                            * disk images, and also obtain information about individual
                                            * disk image files registered by the core.
                                            * This is used for games which consist of multiple images and
                                            * must be manually swapped out by the user (e.g. PSX, floppy disk
                                            * based systems).
                                            */

#define RETRO_ENVIRONMENT_GET_MESSAGE_INTERFACE_VERSION 59
                                           /* unsigned * --
                                            * Unsigned value is the API version number of the message
                                            * interface supported by the frontend. If callback returns
                                            * false, API version is assumed to be 0.
                                            *
                                            * In legacy code, messages may be displayed in an
                                            * implementation-specific manner by passing a struct
                                            * of type retro_message to RETRO_ENVIRONMENT_SET_MESSAGE.
                                            * This may be still be done regardless of the message
                                            * interface version.
                                            *
                                            * If version is >= 1 however, messages may instead be
                                            * displayed by passing a struct of type retro_message_ext
                                            * to RETRO_ENVIRONMENT_SET_MESSAGE_EXT. This allows the
                                            * core to specify message logging level, priority and
                                            * destination (OSD, logging interface or both).
                                            */

#define RETRO_ENVIRONMENT_SET_MESSAGE_EXT 60
                                           /* const struct retro_message_ext * --
                                            * Sets a message to be displayed in an implementation-specific
                                            * manner for a certain amount of 'frames'. Additionally allows
                                            * the core to specify message logging level, priority and
                                            * destination (OSD, logging interface or both).
                                            * Should not be used for trivial messages, which should simply be
                                            * logged via RETRO_ENVIRONMENT_GET_LOG_INTERFACE (or as a
                                            * fallback, stderr).
                                            */

#define RETRO_ENVIRONMENT_GET_INPUT_MAX_USERS 61
                                           /* unsigned * --
                                            * Unsigned value is the number of active input devices
                                            * provided by the frontend. This may change between
                                            * frames, but will remain constant for the duration
                                            * of each frame.
                                            * If callback returns true, a core need not poll any
                                            * input device with an index greater than or equal to
                                            * the number of active devices.
                                            * If callback returns false, the number of active input
                                            * devices is unknown. In this case, all input devices
                                            * should be considered active.
                                            */

#define RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK 62
                                           /* const struct retro_audio_buffer_status_callback * --
                                            * Lets the core know the occupancy level of the frontend
                                            * audio buffer. Can be used by a core to attempt frame
                                            * skipping in order to avoid buffer under-runs.
                                            * A core may pass NULL to disable buffer status reporting
                                            * in the frontend.
                                            */

#define RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY 63
                                           /* const unsigned * --
                                            * Sets minimum frontend audio latency in milliseconds.
                                            * Resultant audio latency may be larger than set value,
                                            * or smaller if a hardware limit is encountered. A frontend
                                            * is expected to honour requests up to 512 ms.
                                            *
                                            * - If value is less than current frontend
                                            *   audio latency, callback has no effect
                                            * - If value is zero, default frontend audio
                                            *   latency is set
                                            *
                                            * May be used by a core to increase audio latency and
                                            * therefore decrease the probability of buffer under-runs
                                            * (crackling) when performing 'intensive' operations.
                                            * A core utilising RETRO_ENVIRONMENT_SET_AUDIO_BUFFER_STATUS_CALLBACK
                                            * to implement audio-buffer-based frame skipping may achieve
                                            * optimal results by setting the audio latency to a 'high'
                                            * (typically 6x or 8x) integer multiple of the expected
                                            * frame time.
                                            *
                                            * WARNING: This can only be called from within retro_run().
                                            * Calling this can require a full reinitialization of audio
                                            * drivers in the frontend, so it is important to call it very
                                            * sparingly, and usually only with the users explicit consent.
                                            * An eventual driver reinitialize will happen so that audio
                                            * callbacks happening after this call within the same retro_run()
                                            * call will target the newly initialized driver.
                                            */

#define RETRO_ENVIRONMENT_SET_FASTFORWARDING_OVERRIDE 64
                                           /* const struct retro_fastforwarding_override * --
                                            * Used by a libretro core to override the current
                                            * fastforwarding mode of the frontend.
                                            * If NULL is passed to this function, the frontend
                                            * will return true if fastforwarding override
                                            * functionality is supported (no change in
                                            * fastforwarding state will occur in this case).
                                            */

#define RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE 65
                                           /* const struct retro_system_content_info_override * --
                                            * Allows an implementation to override 'global' content
                                            * info parameters reported by retro_get_system_info().
                                            * Overrides also affect subsystem content info parameters
                                            * set via RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO.
                                            * This function must be called inside retro_set_environment().
                                            * If callback returns false, content info overrides
                                            * are unsupported by the frontend, and will be ignored.
                                            * If callback returns true, extended game info may be
                                            * retrieved by calling RETRO_ENVIRONMENT_GET_GAME_INFO_EXT
                                            * in retro_load_game() or retro_load_game_special().
                                            *
                                            * 'data' points to an array of retro_system_content_info_override
                                            * structs terminated by a { NULL, false, false } element.
                                            * If 'data' is NULL, no changes will be made to the frontend;
                                            * a core may therefore pass NULL in order to test whether
                                            * the RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE and
                                            * RETRO_ENVIRONMENT_GET_GAME_INFO_EXT callbacks are supported
                                            * by the frontend.
                                            *
                                            * For struct member descriptions, see the definition of
                                            * struct retro_system_content_info_override.
                                            *
                                            * Example:
                                            *
                                            * - struct retro_system_info:
                                            * {
                                            *    "My Core",                      // library_name
                                            *    "v1.0",                         // library_version
                                            *    "m3u|md|cue|iso|chd|sms|gg|sg", // valid_extensions
                                            *    true,                           // need_fullpath
                                            *    false                           // block_extract
                                            * }
                                            *
                                            * - Array of struct retro_system_content_info_override:
                                            * {
                                            *    {
                                            *       "md|sms|gg", // extensions
                                            *       false,       // need_fullpath
                                            *       true         // persistent_data
                                            *    },
                                            *    {
                                            *       "sg",        // extensions
                                            *       false,       // need_fullpath
                                            *       false        // persistent_data
                                            *    },
                                            *    { NULL, false, false }
                                            * }
                                            *
                                            * Result:
                                            * - Files of type m3u, cue, iso, chd will not be
                                            *   loaded by the frontend. Frontend will pass a
                                            *   valid path to the core, and core will handle
                                            *   loading internally
                                            * - Files of type md, sms, gg will be loaded by
                                            *   the frontend. A valid memory buffer will be
                                            *   passed to the core. This memory buffer will
                                            *   remain valid until retro_deinit() returns
                                            * - Files of type sg will be loaded by the frontend.
                                            *   A valid memory buffer will be passed to the core.
                                            *   This memory buffer will remain valid until
                                            *   retro_load_game() (or retro_load_game_special())
                                            *   returns
                                            *
                                            * NOTE: If an extension is listed multiple times in
                                            * an array of retro_system_content_info_override
                                            * structs, only the first instance will be registered
                                            */

#define RETRO_ENVIRONMENT_GET_GAME_INFO_EXT 66
                                           /* const struct retro_game_info_ext ** --
                                            * Allows an implementation to fetch extended game
                                            * information, providing additional content path
                                            * and memory buffer status details.
                                            * This function may only be called inside
                                            * retro_load_game() or retro_load_game_special().
                                            * If callback returns false, extended game information
                                            * is unsupported by the frontend. In this case, only
                                            * regular retro_game_info will be available.
                                            * RETRO_ENVIRONMENT_GET_GAME_INFO_EXT is guaranteed
                                            * to return true if RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE
                                            * returns true.
                                            *
                                            * 'data' points to an array of retro_game_info_ext structs.
                                            *
                                            * For struct member descriptions, see the definition of
                                            * struct retro_game_info_ext.
                                            *
                                            * - If function is called inside retro_load_game(),
                                            *   the retro_game_info_ext array is guaranteed to
                                            *   have a size of 1 - i.e. the returned pointer may
                                            *   be used to access directly the members of the
                                            *   first retro_game_info_ext struct, for example:
                                            *
                                            *      struct retro_game_info_ext *game_info_ext;
                                            *      if (environ_cb(RETRO_ENVIRONMENT_GET_GAME_INFO_EXT, &game_info_ext))
                                            *         printf("Content Directory: %s\n", game_info_ext->dir);
                                            *
                                            * - If the function is called inside retro_load_game_special(),
                                            *   the retro_game_info_ext array is guaranteed to have a
                                            *   size equal to the num_info argument passed to
                                            *   retro_load_game_special()
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 67
                                           /* const struct retro_core_options_v2 * --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should only be called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
                                            * returns an API version of >= 2.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_CORE_OPTIONS.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterwards it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            * If RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION returns an API
                                            * version of >= 2, this callback is guaranteed to succeed
                                            * (i.e. callback return value does not indicate success)
                                            * If callback returns true, frontend has core option category
                                            * support.
                                            * If callback returns false, frontend does not have core option
                                            * category support.
                                            *
                                            * 'data' points to a retro_core_options_v2 struct, containing
                                            * of two pointers:
                                            * - retro_core_options_v2::categories is an array of
                                            *   retro_core_option_v2_category structs terminated by a
                                            *   { NULL, NULL, NULL } element. If retro_core_options_v2::categories
                                            *   is NULL, all core options will have no category and will be shown
                                            *   at the top level of the frontend core option interface. If frontend
                                            *   does not have core option category support, categories array will
                                            *   be ignored.
                                            * - retro_core_options_v2::definitions is an array of
                                            *   retro_core_option_v2_definition structs terminated by a
                                            *   { NULL, NULL, NULL, NULL, NULL, NULL, {{0}}, NULL }
                                            *   element.
                                            *
                                            * >> retro_core_option_v2_category notes:
                                            *
                                            * - retro_core_option_v2_category::key should contain string
                                            *   that uniquely identifies the core option category. Valid
                                            *   key characters are [a-z, A-Z, 0-9, _, -]
                                            *   Namespace collisions with other implementations' category
                                            *   keys are permitted.
                                            * - retro_core_option_v2_category::desc should contain a human
                                            *   readable description of the category key.
                                            * - retro_core_option_v2_category::info should contain any
                                            *   additional human readable information text that a typical
                                            *   user may need to understand the nature of the core option
                                            *   category.
                                            *
                                            * Example entry:
                                            * {
                                            *     "advanced_settings",
                                            *     "Advanced",
                                            *     "Options affecting low-level emulation performance and accuracy."
                                            * }
                                            *
                                            * >> retro_core_option_v2_definition notes:
                                            *
                                            * - retro_core_option_v2_definition::key should be namespaced to not
                                            *   collide with other implementations' keys. e.g. A core called
                                            *   'foo' should use keys named as 'foo_option'. Valid key characters
                                            *   are [a-z, A-Z, 0-9, _, -].
                                            * - retro_core_option_v2_definition::desc should contain a human readable
                                            *   description of the key. Will be used when the frontend does not
                                            *   have core option category support. Examples: "Aspect Ratio" or
                                            *   "Video > Aspect Ratio".
                                            * - retro_core_option_v2_definition::desc_categorized should contain a
                                            *   human readable description of the key, which will be used when
                                            *   frontend has core option category support. Example: "Aspect Ratio",
                                            *   where associated retro_core_option_v2_category::desc is "Video".
                                            *   If empty or NULL, the string specified by
                                            *   retro_core_option_v2_definition::desc will be used instead.
                                            *   retro_core_option_v2_definition::desc_categorized will be ignored
                                            *   if retro_core_option_v2_definition::category_key is empty or NULL.
                                            * - retro_core_option_v2_definition::info should contain any additional
                                            *   human readable information text that a typical user may need to
                                            *   understand the functionality of the option.
                                            * - retro_core_option_v2_definition::info_categorized should contain
                                            *   any additional human readable information text that a typical user
                                            *   may need to understand the functionality of the option, and will be
                                            *   used when frontend has core option category support. This is provided
                                            *   to accommodate the case where info text references an option by
                                            *   name/desc, and the desc/desc_categorized text for that option differ.
                                            *   If empty or NULL, the string specified by
                                            *   retro_core_option_v2_definition::info will be used instead.
                                            *   retro_core_option_v2_definition::info_categorized will be ignored
                                            *   if retro_core_option_v2_definition::category_key is empty or NULL.
                                            * - retro_core_option_v2_definition::category_key should contain a
                                            *   category identifier (e.g. "video" or "audio") that will be
                                            *   assigned to the core option if frontend has core option category
                                            *   support. A categorized option will be shown in a subsection/
                                            *   submenu of the frontend core option interface. If key is empty
                                            *   or NULL, or if key does not match one of the
                                            *   retro_core_option_v2_category::key values in the associated
                                            *   retro_core_option_v2_category array, option will have no category
                                            *   and will be shown at the top level of the frontend core option
                                            *   interface.
                                            * - retro_core_option_v2_definition::values is an array of
                                            *   retro_core_option_value structs terminated by a { NULL, NULL }
                                            *   element.
                                            * --> retro_core_option_v2_definition::values[index].value is an
                                            *     expected option value.
                                            * --> retro_core_option_v2_definition::values[index].label is a
                                            *     human readable label used when displaying the value on screen.
                                            *     If NULL, the value itself is used.
                                            * - retro_core_option_v2_definition::default_value is the default
                                            *   core option setting. It must match one of the expected option
                                            *   values in the retro_core_option_v2_definition::values array. If
                                            *   it does not, or the default value is NULL, the first entry in the
                                            *   retro_core_option_v2_definition::values array is treated as the
                                            *   default.
                                            *
                                            * The number of possible option values should be very limited,
                                            * and must be less than RETRO_NUM_CORE_OPTION_VALUES_MAX.
                                            * i.e. it should be feasible to cycle through options
                                            * without a keyboard.
                                            *
                                            * Example entries:
                                            *
                                            * - Uncategorized:
                                            *
                                            * {
                                            *     "foo_option",
                                            *     "Speed hack coprocessor X",
                                            *     NULL,
                                            *     "Provides increased performance at the expense of reduced accuracy.",
                                            *     NULL,
                                            *     NULL,
                                            * 	  {
                                            *         { "false",    NULL },
                                            *         { "true",     NULL },
                                            *         { "unstable", "Turbo (Unstable)" },
                                            *         { NULL, NULL },
                                            *     },
                                            *     "false"
                                            * }
                                            *
                                            * - Categorized:
                                            *
                                            * {
                                            *     "foo_option",
                                            *     "Advanced > Speed hack coprocessor X",
                                            *     "Speed hack coprocessor X",
                                            *     "Setting 'Advanced > Speed hack coprocessor X' to 'true' or 'Turbo' provides increased performance at the expense of reduced accuracy",
                                            *     "Setting 'Speed hack coprocessor X' to 'true' or 'Turbo' provides increased performance at the expense of reduced accuracy",
                                            *     "advanced_settings",
                                            * 	  {
                                            *         { "false",    NULL },
                                            *         { "true",     NULL },
                                            *         { "unstable", "Turbo (Unstable)" },
                                            *         { NULL, NULL },
                                            *     },
                                            *     "false"
                                            * }
                                            *
                                            * Only strings are operated on. The possible values will
                                            * generally be displayed and stored as-is by the frontend.
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL 68
                                           /* const struct retro_core_options_v2_intl * --
                                            * Allows an implementation to signal the environment
                                            * which variables it might want to check for later using
                                            * GET_VARIABLE.
                                            * This allows the frontend to present these variables to
                                            * a user dynamically.
                                            * This should only be called if RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION
                                            * returns an API version of >= 2.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_VARIABLES.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_CORE_OPTIONS.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL.
                                            * This should be called instead of RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2.
                                            * This should be called the first time as early as
                                            * possible (ideally in retro_set_environment).
                                            * Afterwards it may be called again for the core to communicate
                                            * updated options to the frontend, but the number of core
                                            * options must not change from the number in the initial call.
                                            * If RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION returns an API
                                            * version of >= 2, this callback is guaranteed to succeed
                                            * (i.e. callback return value does not indicate success)
                                            * If callback returns true, frontend has core option category
                                            * support.
                                            * If callback returns false, frontend does not have core option
                                            * category support.
                                            *
                                            * This is fundamentally the same as RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2,
                                            * with the addition of localisation support. The description of the
                                            * RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2 callback should be consulted
                                            * for further details.
                                            *
                                            * 'data' points to a retro_core_options_v2_intl struct.
                                            *
                                            * - retro_core_options_v2_intl::us is a pointer to a
                                            *   retro_core_options_v2 struct defining the US English
                                            *   core options implementation. It must point to a valid struct.
                                            *
                                            * - retro_core_options_v2_intl::local is a pointer to a
                                            *   retro_core_options_v2 struct defining core options for
                                            *   the current frontend language. It may be NULL (in which case
                                            *   retro_core_options_v2_intl::us is used by the frontend). Any items
                                            *   missing from this struct will be read from
                                            *   retro_core_options_v2_intl::us instead.
                                            *
                                            * NOTE: Default core option values are always taken from the
                                            * retro_core_options_v2_intl::us struct. Any default values in
                                            * the retro_core_options_v2_intl::local struct will be ignored.
                                            */

#define RETRO_ENVIRONMENT_SET_CORE_OPTIONS_UPDATE_DISPLAY_CALLBACK 69
                                           /* const struct retro_core_options_update_display_callback * --
                                            * Allows a frontend to signal that a core must update
                                            * the visibility of any dynamically hidden core options,
                                            * and enables the frontend to detect visibility changes.
                                            * Used by the frontend to update the menu display status
                                            * of core options without requiring a call of retro_run().
                                            * Must be called in retro_set_environment().
                                            */

#define RETRO_ENVIRONMENT_SET_VARIABLE 70
                                           /* const struct retro_variable * --
                                            * Allows an implementation to notify the frontend
                                            * that a core option value has changed.
                                            *
                                            * retro_variable::key and retro_variable::value
                                            * must match strings that have been set previously
                                            * via one of the following:
                                            *
                                            * - RETRO_ENVIRONMENT_SET_VARIABLES
                                            * - RETRO_ENVIRONMENT_SET_CORE_OPTIONS
                                            * - RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL
                                            * - RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2
                                            * - RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL
                                            *
                                            * After changing a core option value via this
                                            * callback, RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE
                                            * will return true.
                                            *
                                            * If data is NULL, no changes will be registered
                                            * and the callback will return true; an
                                            * implementation may therefore pass NULL in order
                                            * to test whether the callback is supported.
                                            */

#define RETRO_ENVIRONMENT_GET_THROTTLE_STATE (71 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_throttle_state * --
                                            * Allows an implementation to get details on the actual rate
                                            * the frontend is attempting to call retro_run().
                                            */

#define RETRO_ENVIRONMENT_GET_SAVESTATE_CONTEXT (72 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* int * --
                                            * Tells the core about the context the frontend is asking for savestate.
                                            * (see enum retro_savestate_context)
                                            */

#define RETRO_ENVIRONMENT_GET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_SUPPORT (73 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                            /* struct retro_hw_render_context_negotiation_interface * --
                                             * Before calling SET_HW_RNEDER_CONTEXT_NEGOTIATION_INTERFACE, a core can query
                                             * which version of the interface is supported.
                                             *
                                             * Frontend looks at interface_type and returns the maximum supported
                                             * context negotiation interface version.
                                             * If the interface_type is not supported or recognized by the frontend, a version of 0
                                             * must be returned in interface_version and true is returned by frontend.
                                             *
                                             * If this environment call returns true with interface_version greater than 0,
                                             * a core can always use a negotiation interface version larger than what the frontend returns, but only
                                             * earlier versions of the interface will be used by the frontend.
                                             * A frontend must not reject a negotiation interface version that is larger than
                                             * what the frontend supports. Instead, the frontend will use the older entry points that it recognizes.
                                             * If this is incompatible with a particular core's requirements, it can error out early.
                                             *
                                             * Backwards compatibility note:
                                             * This environment call was introduced after Vulkan v1 context negotiation.
                                             * If this environment call is not supported by frontend - i.e. the environment call returns false -
                                             * only Vulkan v1 context negotiation is supported (if Vulkan HW rendering is supported at all).
                                             * If a core uses Vulkan negotiation interface with version > 1, negotiation may fail unexpectedly.
                                             * All future updates to the context negotiation interface implies that frontend must support
                                             * this environment call to query support.
                                             */

/**
 * Asks the frontend whether JIT compilation can be used.
 * Primarily used by iOS and tvOS.
 * @param data[out] <tt>bool *</tt>.
 * Set to \c true if the frontend has verified that JIT compilation is possible.
 * @return \c true if the environment call is available.
 */
#define RETRO_ENVIRONMENT_GET_JIT_CAPABLE 74

#define RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE (75 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_microphone_interface * --
                                            * Returns an interface that can be used to receive input from the microphone driver.
                                            *
                                            * Returns true if microphone support is available,
                                            * even if no microphones are plugged in.
                                            * Returns false if mic support is disabled or unavailable.
                                            *
                                            * This callback can be invoked at any time,
                                            * even before the microphone driver is ready.
                                            */

#define RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE 76
                                           /* const struct retro_netpacket_callback * --
                                            * When set, a core gains control over network packets sent and
                                            * received during a multiplayer session. This can be used to
                                            * emulate multiplayer games that were originally played on two
                                            * or more separate consoles or computers connected together.
                                            *
                                            * The frontend will take care of connecting players together,
                                            * and the core only needs to send the actual data as needed for
                                            * the emulation, while handshake and connection management happen
                                            * in the background.
                                            *
                                            * When two or more players are connected and this interface has
                                            * been set, time manipulation features (such as pausing, slow motion,
                                            * fast forward, rewinding, save state loading, etc.) are disabled to
                                            * avoid interrupting communication.
                                            *
                                            * Should be set in either retro_init or retro_load_game, but not both.
                                            *
                                            * When not set, a frontend may use state serialization-based
                                            * multiplayer, where a deterministic core supporting multiple
                                            * input devices does not need to take any action on its own.
                                            */

#define RETRO_ENVIRONMENT_GET_DEVICE_POWER (77 | RETRO_ENVIRONMENT_EXPERIMENTAL)
                                           /* struct retro_device_power * --
                                            * Returns the device's current power state as reported by the frontend.
                                            * This is useful for emulating the battery level in handheld consoles,
                                            * or for reducing power consumption when on battery power.
                                            *
                                            * The return value indicates whether the frontend can provide this information,
                                            * even if the parameter is NULL.
                                            *
                                            * If the frontend does not support this functionality,
                                            * then the provided argument will remain unchanged.
                                            *
                                            * Note that this environment call describes the power state for the entire device,
                                            * not for individual peripherals like controllers.
                                            */

/**@}*/

/* VFS functionality */

/* File paths:
 * File paths passed as parameters when using this API shall be well formed UNIX-style,
 * using "/" (unquoted forward slash) as directory separator regardless of the platform's native separator.
 * Paths shall also include at least one forward slash ("game.bin" is an invalid path, use "./game.bin" instead).
 * Other than the directory separator, cores shall not make assumptions about path format:
 * "C:/path/game.bin", "http://example.com/game.bin", "#game/game.bin", "./game.bin" (without quotes) are all valid paths.
 * Cores may replace the basename or remove path components from the end, and/or add new components;
 * however, cores shall not append "./", "../" or multiple consecutive forward slashes ("//") to paths they request to front end.
 * The frontend is encouraged to make such paths work as well as it can, but is allowed to give up if the core alters paths too much.
 * Frontends are encouraged, but not required, to support native file system paths (modulo replacing the directory separator, if applicable).
 * Cores are allowed to try using them, but must remain functional if the front rejects such requests.
 * Cores are encouraged to use the libretro-common filestream functions for file I/O,
 * as they seamlessly integrate with VFS, deal with directory separator replacement as appropriate
 * and provide platform-specific fallbacks in cases where front ends do not support VFS. */

/* Opaque file handle
 * Introduced in VFS API v1 */
struct retro_vfs_file_handle;

/* Opaque directory handle
 * Introduced in VFS API v3 */
struct retro_vfs_dir_handle;

/* File open flags
 * Introduced in VFS API v1 */
#define RETRO_VFS_FILE_ACCESS_READ            (1 << 0) /* Read only mode */
#define RETRO_VFS_FILE_ACCESS_WRITE           (1 << 1) /* Write only mode, discard contents and overwrites existing file unless RETRO_VFS_FILE_ACCESS_UPDATE is also specified */
#define RETRO_VFS_FILE_ACCESS_READ_WRITE      (RETRO_VFS_FILE_ACCESS_READ | RETRO_VFS_FILE_ACCESS_WRITE) /* Read-write mode, discard contents and overwrites existing file unless RETRO_VFS_FILE_ACCESS_UPDATE is also specified*/
#define RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING (1 << 2) /* Prevents discarding content of existing files opened for writing */

/* These are only hints. The frontend may choose to ignore them. Other than RAM/CPU/etc use,
   and how they react to unlikely external interference (for example someone else writing to that file,
   or the file's server going down), behavior will not change. */
#define RETRO_VFS_FILE_ACCESS_HINT_NONE              (0)
/* Indicate that the file will be accessed many times. The frontend should aggressively cache everything. */
#define RETRO_VFS_FILE_ACCESS_HINT_FREQUENT_ACCESS   (1 << 0)

/* Seek positions */
#define RETRO_VFS_SEEK_POSITION_START    0
#define RETRO_VFS_SEEK_POSITION_CURRENT  1
#define RETRO_VFS_SEEK_POSITION_END      2

/* stat() result flags
 * Introduced in VFS API v3 */
#define RETRO_VFS_STAT_IS_VALID               (1 << 0)
#define RETRO_VFS_STAT_IS_DIRECTORY           (1 << 1)
#define RETRO_VFS_STAT_IS_CHARACTER_SPECIAL   (1 << 2)

/* Get path from opaque handle. Returns the exact same path passed to file_open when getting the handle
 * Introduced in VFS API v1 */
typedef const char *(RETRO_CALLCONV *retro_vfs_get_path_t)(struct retro_vfs_file_handle *stream);

/* Open a file for reading or writing. If path points to a directory, this will
 * fail. Returns the opaque file handle, or NULL for error.
 * Introduced in VFS API v1 */
typedef struct retro_vfs_file_handle *(RETRO_CALLCONV *retro_vfs_open_t)(const char *path, unsigned mode, unsigned hints);

/* Close the file and release its resources. Must be called if open_file returns non-NULL. Returns 0 on success, -1 on failure.
 * Whether the call succeeds ot not, the handle passed as parameter becomes invalid and should no longer be used.
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_close_t)(struct retro_vfs_file_handle *stream);

/* Return the size of the file in bytes, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_size_t)(struct retro_vfs_file_handle *stream);

/* Truncate file to specified size. Returns 0 on success or -1 on error
 * Introduced in VFS API v2 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_truncate_t)(struct retro_vfs_file_handle *stream, int64_t length);

/* Get the current read / write position for the file. Returns -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_tell_t)(struct retro_vfs_file_handle *stream);

/* Set the current read/write position for the file. Returns the new position, -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_seek_t)(struct retro_vfs_file_handle *stream, int64_t offset, int seek_position);

/* Read data from a file. Returns the number of bytes read, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_read_t)(struct retro_vfs_file_handle *stream, void *s, uint64_t len);

/* Write data to a file. Returns the number of bytes written, or -1 for error.
 * Introduced in VFS API v1 */
typedef int64_t (RETRO_CALLCONV *retro_vfs_write_t)(struct retro_vfs_file_handle *stream, const void *s, uint64_t len);

/* Flush pending writes to file, if using buffered IO. Returns 0 on sucess, or -1 on failure.
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_flush_t)(struct retro_vfs_file_handle *stream);

/* Delete the specified file. Returns 0 on success, -1 on failure
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_remove_t)(const char *path);

/* Rename the specified file. Returns 0 on success, -1 on failure
 * Introduced in VFS API v1 */
typedef int (RETRO_CALLCONV *retro_vfs_rename_t)(const char *old_path, const char *new_path);

/* Stat the specified file. Retruns a bitmask of RETRO_VFS_STAT_* flags, none are set if path was not valid.
 * Additionally stores file size in given variable, unless NULL is given.
 * Introduced in VFS API v3 */
typedef int (RETRO_CALLCONV *retro_vfs_stat_t)(const char *path, int32_t *size);

/* Create the specified directory. Returns 0 on success, -1 on unknown failure, -2 if already exists.
 * Introduced in VFS API v3 */
typedef int (RETRO_CALLCONV *retro_vfs_mkdir_t)(const char *dir);

/* Open the specified directory for listing. Returns the opaque dir handle, or NULL for error.
 * Support for the include_hidden argument may vary depending on the platform.
 * Introduced in VFS API v3 */
typedef struct retro_vfs_dir_handle *(RETRO_CALLCONV *retro_vfs_opendir_t)(const char *dir, bool include_hidden);

/* Read the directory entry at the current position, and move the read pointer to the next position.
 * Returns true on success, false if already on the last entry.
 * Introduced in VFS API v3 */
typedef bool (RETRO_CALLCONV *retro_vfs_readdir_t)(struct retro_vfs_dir_handle *dirstream);

/* Get the name of the last entry read. Returns a string on success, or NULL for error.
 * The returned string pointer is valid until the next call to readdir or closedir.
 * Introduced in VFS API v3 */
typedef const char *(RETRO_CALLCONV *retro_vfs_dirent_get_name_t)(struct retro_vfs_dir_handle *dirstream);

/* Check if the last entry read was a directory. Returns true if it was, false otherwise (or on error).
 * Introduced in VFS API v3 */
typedef bool (RETRO_CALLCONV *retro_vfs_dirent_is_dir_t)(struct retro_vfs_dir_handle *dirstream);

/* Close the directory and release its resources. Must be called if opendir returns non-NULL. Returns 0 on success, -1 on failure.
 * Whether the call succeeds ot not, the handle passed as parameter becomes invalid and should no longer be used.
 * Introduced in VFS API v3 */
typedef int (RETRO_CALLCONV *retro_vfs_closedir_t)(struct retro_vfs_dir_handle *dirstream);

struct retro_vfs_interface
{
   /* VFS API v1 */
	retro_vfs_get_path_t get_path;
	retro_vfs_open_t open;
	retro_vfs_close_t close;
	retro_vfs_size_t size;
	retro_vfs_tell_t tell;
	retro_vfs_seek_t seek;
	retro_vfs_read_t read;
	retro_vfs_write_t write;
	retro_vfs_flush_t flush;
	retro_vfs_remove_t remove;
	retro_vfs_rename_t rename;
   /* VFS API v2 */
   retro_vfs_truncate_t truncate;
   /* VFS API v3 */
   retro_vfs_stat_t stat;
   retro_vfs_mkdir_t mkdir;
   retro_vfs_opendir_t opendir;
   retro_vfs_readdir_t readdir;
   retro_vfs_dirent_get_name_t dirent_get_name;
   retro_vfs_dirent_is_dir_t dirent_is_dir;
   retro_vfs_closedir_t closedir;
};

struct retro_vfs_interface_info
{
   /* Set by core: should this be higher than the version the front end supports,
    * front end will return false in the RETRO_ENVIRONMENT_GET_VFS_INTERFACE call
    * Introduced in VFS API v1 */
   uint32_t required_interface_version;

   /* Frontend writes interface pointer here. The frontend also sets the actual
    * version, must be at least required_interface_version.
    * Introduced in VFS API v1 */
   struct retro_vfs_interface *iface;
};

enum retro_hw_render_interface_type
{
   RETRO_HW_RENDER_INTERFACE_VULKAN     = 0,
   RETRO_HW_RENDER_INTERFACE_D3D9       = 1,
   RETRO_HW_RENDER_INTERFACE_D3D10      = 2,
   RETRO_HW_RENDER_INTERFACE_D3D11      = 3,
   RETRO_HW_RENDER_INTERFACE_D3D12      = 4,
   RETRO_HW_RENDER_INTERFACE_GSKIT_PS2  = 5,
   RETRO_HW_RENDER_INTERFACE_DUMMY      = INT_MAX
};

/* Base struct. All retro_hw_render_interface_* types
 * contain at least these fields. */
struct retro_hw_render_interface
{
   enum retro_hw_render_interface_type interface_type;
   unsigned interface_version;
};

typedef void (RETRO_CALLCONV *retro_set_led_state_t)(int led, int state);
struct retro_led_interface
{
    retro_set_led_state_t set_led_state;
};

/* Retrieves the current state of the MIDI input.
 * Returns true if it's enabled, false otherwise. */
typedef bool (RETRO_CALLCONV *retro_midi_input_enabled_t)(void);

/* Retrieves the current state of the MIDI output.
 * Returns true if it's enabled, false otherwise */
typedef bool (RETRO_CALLCONV *retro_midi_output_enabled_t)(void);

/* Reads next byte from the input stream.
 * Returns true if byte is read, false otherwise. */
typedef bool (RETRO_CALLCONV *retro_midi_read_t)(uint8_t *byte);

/* Writes byte to the output stream.
 * 'delta_time' is in microseconds and represent time elapsed since previous write.
 * Returns true if byte is written, false otherwise. */
typedef bool (RETRO_CALLCONV *retro_midi_write_t)(uint8_t byte, uint32_t delta_time);

/* Flushes previously written data.
 * Returns true if successful, false otherwise. */
typedef bool (RETRO_CALLCONV *retro_midi_flush_t)(void);

struct retro_midi_interface
{
   retro_midi_input_enabled_t input_enabled;
   retro_midi_output_enabled_t output_enabled;
   retro_midi_read_t read;
   retro_midi_write_t write;
   retro_midi_flush_t flush;
};

enum retro_hw_render_context_negotiation_interface_type
{
   RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN = 0,
   RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_DUMMY = INT_MAX
};

/* Base struct. All retro_hw_render_context_negotiation_interface_* types
 * contain at least these fields. */
struct retro_hw_render_context_negotiation_interface
{
   enum retro_hw_render_context_negotiation_interface_type interface_type;
   unsigned interface_version;
};

/* Serialized state is incomplete in some way. Set if serialization is
 * usable in typical end-user cases but should not be relied upon to
 * implement frame-sensitive frontend features such as netplay or
 * rerecording. */
#define RETRO_SERIALIZATION_QUIRK_INCOMPLETE (1 << 0)
/* The core must spend some time initializing before serialization is
 * supported. retro_serialize() will initially fail; retro_unserialize()
 * and retro_serialize_size() may or may not work correctly either. */
#define RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE (1 << 1)
/* Serialization size may change within a session. */
#define RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE (1 << 2)
/* Set by the frontend to acknowledge that it supports variable-sized
 * states. */
#define RETRO_SERIALIZATION_QUIRK_FRONT_VARIABLE_SIZE (1 << 3)
/* Serialized state can only be loaded during the same session. */
#define RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION (1 << 4)
/* Serialized state cannot be loaded on an architecture with a different
 * endianness from the one it was saved on. */
#define RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT (1 << 5)
/* Serialized state cannot be loaded on a different platform from the one it
 * was saved on for reasons other than endianness, such as word size
 * dependence */
#define RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT (1 << 6)

#define RETRO_MEMDESC_CONST      (1 << 0)   /* The frontend will never change this memory area once retro_load_game has returned. */
#define RETRO_MEMDESC_BIGENDIAN  (1 << 1)   /* The memory area contains big endian data. Default is little endian. */
#define RETRO_MEMDESC_SYSTEM_RAM (1 << 2)   /* The memory area is system RAM.  This is main RAM of the gaming system. */
#define RETRO_MEMDESC_SAVE_RAM   (1 << 3)   /* The memory area is save RAM. This RAM is usually found on a game cartridge, backed up by a battery. */
#define RETRO_MEMDESC_VIDEO_RAM  (1 << 4)   /* The memory area is video RAM (VRAM) */
#define RETRO_MEMDESC_ALIGN_2    (1 << 16)  /* All memory access in this area is aligned to their own size, or 2, whichever is smaller. */
#define RETRO_MEMDESC_ALIGN_4    (2 << 16)
#define RETRO_MEMDESC_ALIGN_8    (3 << 16)
#define RETRO_MEMDESC_MINSIZE_2  (1 << 24)  /* All memory in this region is accessed at least 2 bytes at the time. */
#define RETRO_MEMDESC_MINSIZE_4  (2 << 24)
#define RETRO_MEMDESC_MINSIZE_8  (3 << 24)
struct retro_memory_descriptor
{
   uint64_t flags;

   /* Pointer to the start of the relevant ROM or RAM chip.
    * It's strongly recommended to use 'offset' if possible, rather than
    * doing math on the pointer.
    *
    * If the same byte is mapped my multiple descriptors, their descriptors
    * must have the same pointer.
    * If 'start' does not point to the first byte in the pointer, put the
    * difference in 'offset' instead.
    *
    * May be NULL if there's nothing usable here (e.g. hardware registers and
    * open bus). No flags should be set if the pointer is NULL.
    * It's recommended to minimize the number of descriptors if possible,
    * but not mandatory. */
   void *ptr;
   size_t offset;

   /* This is the location in the emulated address space
    * where the mapping starts. */
   size_t start;

   /* Which bits must be same as in 'start' for this mapping to apply.
    * The first memory descriptor to claim a certain byte is the one
    * that applies.
    * A bit which is set in 'start' must also be set in this.
    * Can be zero, in which case each byte is assumed mapped exactly once.
    * In this case, 'len' must be a power of two. */
   size_t select;

   /* If this is nonzero, the set bits are assumed not connected to the
    * memory chip's address pins. */
   size_t disconnect;

   /* This one tells the size of the current memory area.
    * If, after start+disconnect are applied, the address is higher than
    * this, the highest bit of the address is cleared.
    *
    * If the address is still too high, the next highest bit is cleared.
    * Can be zero, in which case it's assumed to be infinite (as limited
    * by 'select' and 'disconnect'). */
   size_t len;

   /* To go from emulated address to physical address, the following
    * order applies:
    * Subtract 'start', pick off 'disconnect', apply 'len', add 'offset'. */

   /* The address space name must consist of only a-zA-Z0-9_-,
    * should be as short as feasible (maximum length is 8 plus the NUL),
    * and may not be any other address space plus one or more 0-9A-F
    * at the end.
    * However, multiple memory descriptors for the same address space is
    * allowed, and the address space name can be empty. NULL is treated
    * as empty.
    *
    * Address space names are case sensitive, but avoid lowercase if possible.
    * The same pointer may exist in multiple address spaces.
    *
    * Examples:
    * blank+blank - valid (multiple things may be mapped in the same namespace)
    * 'Sp'+'Sp' - valid (multiple things may be mapped in the same namespace)
    * 'A'+'B' - valid (neither is a prefix of each other)
    * 'S'+blank - valid ('S' is not in 0-9A-F)
    * 'a'+blank - valid ('a' is not in 0-9A-F)
    * 'a'+'A' - valid (neither is a prefix of each other)
    * 'AR'+blank - valid ('R' is not in 0-9A-F)
    * 'ARB'+blank - valid (the B can't be part of the address either, because
    *                      there is no namespace 'AR')
    * blank+'B' - not valid, because it's ambigous which address space B1234
    *             would refer to.
    * The length can't be used for that purpose; the frontend may want
    * to append arbitrary data to an address, without a separator. */
   const char *addrspace;

   /* TODO: When finalizing this one, add a description field, which should be
    * "WRAM" or something roughly equally long. */

   /* TODO: When finalizing this one, replace 'select' with 'limit', which tells
    * which bits can vary and still refer to the same address (limit = ~select).
    * TODO: limit? range? vary? something else? */

   /* TODO: When finalizing this one, if 'len' is above what 'select' (or
    * 'limit') allows, it's bankswitched. Bankswitched data must have both 'len'
    * and 'select' != 0, and the mappings don't tell how the system switches the
    * banks. */

   /* TODO: When finalizing this one, fix the 'len' bit removal order.
    * For len=0x1800, pointer 0x1C00 should go to 0x1400, not 0x0C00.
    * Algorithm: Take bits highest to lowest, but if it goes above len, clear
    * the most recent addition and continue on the next bit.
    * TODO: Can the above be optimized? Is "remove the lowest bit set in both
    * pointer and 'len'" equivalent? */

   /* TODO: Some emulators (MAME?) emulate big endian systems by only accessing
    * the emulated memory in 32-bit chunks, native endian. But that's nothing
    * compared to Darek Mihocka <http://www.emulators.com/docs/nx07_vm101.htm>
    * (section Emulation 103 - Nearly Free Byte Reversal) - he flips the ENTIRE
    * RAM backwards! I'll want to represent both of those, via some flags.
    *
    * I suspect MAME either didn't think of that idea, or don't want the #ifdef.
    * Not sure which, nor do I really care. */

   /* TODO: Some of those flags are unused and/or don't really make sense. Clean
    * them up. */
};

/* The frontend may use the largest value of 'start'+'select' in a
 * certain namespace to infer the size of the address space.
 *
 * If the address space is larger than that, a mapping with .ptr=NULL
 * should be at the end of the array, with .select set to all ones for
 * as long as the address space is big.
 *
 * Sample descriptors (minus .ptr, and RETRO_MEMFLAG_ on the flags):
 * SNES WRAM:
 * .start=0x7E0000, .len=0x20000
 * (Note that this must be mapped before the ROM in most cases; some of the
 * ROM mappers
 * try to claim $7E0000, or at least $7E8000.)
 * SNES SPC700 RAM:
 * .addrspace="S", .len=0x10000
 * SNES WRAM mirrors:
 * .flags=MIRROR, .start=0x000000, .select=0xC0E000, .len=0x2000
 * .flags=MIRROR, .start=0x800000, .select=0xC0E000, .len=0x2000
 * SNES WRAM mirrors, alternate equivalent descriptor:
 * .flags=MIRROR, .select=0x40E000, .disconnect=~0x1FFF
 * (Various similar constructions can be created by combining parts of
 * the above two.)
 * SNES LoROM (512KB, mirrored a couple of times):
 * .flags=CONST, .start=0x008000, .select=0x408000, .disconnect=0x8000, .len=512*1024
 * .flags=CONST, .start=0x400000, .select=0x400000, .disconnect=0x8000, .len=512*1024
 * SNES HiROM (4MB):
 * .flags=CONST,                 .start=0x400000, .select=0x400000, .len=4*1024*1024
 * .flags=CONST, .offset=0x8000, .start=0x008000, .select=0x408000, .len=4*1024*1024
 * SNES ExHiROM (8MB):
 * .flags=CONST, .offset=0,                  .start=0xC00000, .select=0xC00000, .len=4*1024*1024
 * .flags=CONST, .offset=4*1024*1024,        .start=0x400000, .select=0xC00000, .len=4*1024*1024
 * .flags=CONST, .offset=0x8000,             .start=0x808000, .select=0xC08000, .len=4*1024*1024
 * .flags=CONST, .offset=4*1024*1024+0x8000, .start=0x008000, .select=0xC08000, .len=4*1024*1024
 * Clarify the size of the address space:
 * .ptr=NULL, .select=0xFFFFFF
 * .len can be implied by .select in many of them, but was included for clarity.
 */

struct retro_memory_map
{
   const struct retro_memory_descriptor *descriptors;
   unsigned num_descriptors;
};

struct retro_controller_description
{
   /* Human-readable description of the controller. Even if using a generic
    * input device type, this can be set to the particular device type the
    * core uses. */
   const char *desc;

   /* Device type passed to retro_set_controller_port_device(). If the device
    * type is a sub-class of a generic input device type, use the
    * RETRO_DEVICE_SUBCLASS macro to create an ID.
    *
    * E.g. RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1). */
   unsigned id;
};

struct retro_controller_info
{
   const struct retro_controller_description *types;
   unsigned num_types;
};

struct retro_subsystem_memory_info
{
   /* The extension associated with a memory type, e.g. "psram". */
   const char *extension;

   /* The memory type for retro_get_memory(). This should be at
    * least 0x100 to avoid conflict with standardized
    * libretro memory types. */
   unsigned type;
};

struct retro_subsystem_rom_info
{
   /* Describes what the content is (SGB BIOS, GB ROM, etc). */
   const char *desc;

   /* Same definition as retro_get_system_info(). */
   const char *valid_extensions;

   /* Same definition as retro_get_system_info(). */
   bool need_fullpath;

   /* Same definition as retro_get_system_info(). */
   bool block_extract;

   /* This is set if the content is required to load a game.
    * If this is set to false, a zeroed-out retro_game_info can be passed. */
   bool required;

   /* Content can have multiple associated persistent
    * memory types (retro_get_memory()). */
   const struct retro_subsystem_memory_info *memory;
   unsigned num_memory;
};

struct retro_subsystem_info
{
   /* Human-readable string of the subsystem type, e.g. "Super GameBoy" */
   const char *desc;

   /* A computer friendly short string identifier for the subsystem type.
    * This name must be [a-z].
    * E.g. if desc is "Super GameBoy", this can be "sgb".
    * This identifier can be used for command-line interfaces, etc.
    */
   const char *ident;

   /* Infos for each content file. The first entry is assumed to be the
    * "most significant" content for frontend purposes.
    * E.g. with Super GameBoy, the first content should be the GameBoy ROM,
    * as it is the most "significant" content to a user.
    * If a frontend creates new file paths based on the content used
    * (e.g. savestates), it should use the path for the first ROM to do so. */
   const struct retro_subsystem_rom_info *roms;

   /* Number of content files associated with a subsystem. */
   unsigned num_roms;

   /* The type passed to retro_load_game_special(). */
   unsigned id;
};

/** @defgroup SET_PROC_ADDRESS_CALLBACK Core Function Pointers
 * @{ */

/**
 * The function pointer type that \c retro_get_proc_address_t returns.
 *
 * Despite the signature shown here, the original function may include any parameters and return type
 * that respects the calling convention and C ABI.
 *
 * The frontend is expected to cast the function pointer to the correct type.
 */
typedef void (RETRO_CALLCONV *retro_proc_address_t)(void);

/**
 * Get a symbol from a libretro core.
 *
 * Cores should only return symbols that serve as libretro extensions.
 * Frontends should not use this to obtain symbols to standard libretro entry points;
 * instead, they should link to the core statically or use \c dlsym (or local equivalent).
 *
 * The symbol name must be equal to the function name.
 * e.g. if <tt>void retro_foo(void);</tt> exists, the symbol in the compiled library must be called \c retro_foo.
 * The returned function pointer must be cast to the corresponding type.
 *
 * @param \c sym The name of the symbol to look up.
 * @return Pointer to the exposed function with the name given in \c sym,
 * or \c NULL if one couldn't be found.
 * @note The frontend is expected to know the returned pointer's type in advance
 * so that it can be cast correctly.
 * @note The core doesn't need to expose every possible function through this interface.
 * It's enough to only expose the ones that it expects the frontend to use.
 * @note The functions exposed through this interface
 * don't need to be publicly exposed in the compiled library
 * (e.g. via \c __declspec(dllexport)).
 * @see RETRO_ENVIRONMENT_SET_PROC_ADDRESS_INTERFACE
 */
typedef retro_proc_address_t (RETRO_CALLCONV *retro_get_proc_address_t)(const char *sym);

/**
 * An interface that the frontend can use to get function pointers from the core.
 *
 * @note The returned function pointer will be invalidated once the core is unloaded.
 * How and when that happens is up to the frontend.
 *
 * @see retro_get_proc_address_t
 * @see RETRO_ENVIRONMENT_GET_PROC_ADDRESS_INTERFACE
 */
struct retro_get_proc_address_interface
{
   /** Set by the core. */
   retro_get_proc_address_t get_proc_address;
};

/** @} */

/**
 * The severity of a given message.
 * The frontend may log messages differently depending on the level.
 * It may also ignore log messages of a certain level.
 * @see retro_log_callback
 */
enum retro_log_level
{
   /** The logged message is most likely not interesting to the user. */
   RETRO_LOG_DEBUG = 0,

   /** Information about the core operating normally. */
   RETRO_LOG_INFO,

   /** Indicates a potential problem, possibly one that the core can recover from. */
   RETRO_LOG_WARN,

   /** Indicates a degraded experience, if not failure. */
   RETRO_LOG_ERROR,

   /** Defined to ensure that sizeof(enum retro_log_level) == sizeof(int). Do not use. */
   RETRO_LOG_DUMMY = INT_MAX
};

/**
 * Logs a message to the frontend.
 *
 * @param level The log level of the message.
 * @param fmt The format string to log.
 * Same format as \c printf.
 * Behavior is undefined if this is \c NULL.
 * @param ... Zero or more arguments used by the format string.
 * Behavior is undefined if these don't match the ones expected by \c fmt.
 * @see retro_log_level
 * @see retro_log_callback
 * @see RETRO_ENVIRONMENT_GET_LOG_INTERFACE
 * @see printf
 */
typedef void (RETRO_CALLCONV *retro_log_printf_t)(enum retro_log_level level,
      const char *fmt, ...);

/**
 * Details about how to make log messages.
 *
 * @see retro_log_printf_t
 * @see RETRO_ENVIRONMENT_GET_LOG_INTERFACE
 */
struct retro_log_callback
{
   /**
    * Called when logging a message.
    *
    * @note Set by the frontend.
    */
   retro_log_printf_t log;
};

/* Performance related functions */

/* ID values for SIMD CPU features */
#define RETRO_SIMD_SSE      (1 << 0)
#define RETRO_SIMD_SSE2     (1 << 1)
#define RETRO_SIMD_VMX      (1 << 2)
#define RETRO_SIMD_VMX128   (1 << 3)
#define RETRO_SIMD_AVX      (1 << 4)
#define RETRO_SIMD_NEON     (1 << 5)
#define RETRO_SIMD_SSE3     (1 << 6)
#define RETRO_SIMD_SSSE3    (1 << 7)
#define RETRO_SIMD_MMX      (1 << 8)
#define RETRO_SIMD_MMXEXT   (1 << 9)
#define RETRO_SIMD_SSE4     (1 << 10)
#define RETRO_SIMD_SSE42    (1 << 11)
#define RETRO_SIMD_AVX2     (1 << 12)
#define RETRO_SIMD_VFPU     (1 << 13)
#define RETRO_SIMD_PS       (1 << 14)
#define RETRO_SIMD_AES      (1 << 15)
#define RETRO_SIMD_VFPV3    (1 << 16)
#define RETRO_SIMD_VFPV4    (1 << 17)
#define RETRO_SIMD_POPCNT   (1 << 18)
#define RETRO_SIMD_MOVBE    (1 << 19)
#define RETRO_SIMD_CMOV     (1 << 20)
#define RETRO_SIMD_ASIMD    (1 << 21)

typedef uint64_t retro_perf_tick_t;
typedef int64_t retro_time_t;

struct retro_perf_counter
{
   const char *ident;
   retro_perf_tick_t start;
   retro_perf_tick_t total;
   retro_perf_tick_t call_cnt;

   bool registered;
};

/**
 * @returns The current system time in microseconds.
 * @note Accuracy may vary by platform.
 * The frontend should use the most accurate timer possible.
 * @see RETRO_ENVIRONMENT_GET_PERF_INTERFACE
 */
typedef retro_time_t (RETRO_CALLCONV *retro_perf_get_time_usec_t)(void);

/* A simple counter. Usually nanoseconds, but can also be CPU cycles.
 * Can be used directly if desired (when creating a more sophisticated
 * performance counter system).
 * */
typedef retro_perf_tick_t (RETRO_CALLCONV *retro_perf_get_counter_t)(void);

/* Returns a bit-mask of detected CPU features (RETRO_SIMD_*). */
typedef uint64_t (RETRO_CALLCONV *retro_get_cpu_features_t)(void);

/* Asks frontend to log and/or display the state of performance counters.
 * Performance counters can always be poked into manually as well.
 */
typedef void (RETRO_CALLCONV *retro_perf_log_t)(void);

/* Register a performance counter.
 * ident field must be set with a discrete value and other values in
 * retro_perf_counter must be 0.
 * Registering can be called multiple times. To avoid calling to
 * frontend redundantly, you can check registered field first. */
typedef void (RETRO_CALLCONV *retro_perf_register_t)(struct retro_perf_counter *counter);

/* Starts a registered counter. */
typedef void (RETRO_CALLCONV *retro_perf_start_t)(struct retro_perf_counter *counter);

/* Stops a registered counter. */
typedef void (RETRO_CALLCONV *retro_perf_stop_t)(struct retro_perf_counter *counter);

/* For convenience it can be useful to wrap register, start and stop in macros.
 * E.g.:
 * #ifdef LOG_PERFORMANCE
 * #define RETRO_PERFORMANCE_INIT(perf_cb, name) static struct retro_perf_counter name = {#name}; if (!name.registered) perf_cb.perf_register(&(name))
 * #define RETRO_PERFORMANCE_START(perf_cb, name) perf_cb.perf_start(&(name))
 * #define RETRO_PERFORMANCE_STOP(perf_cb, name) perf_cb.perf_stop(&(name))
 * #else
 * ... Blank macros ...
 * #endif
 *
 * These can then be used mid-functions around code snippets.
 *
 * extern struct retro_perf_callback perf_cb;  * Somewhere in the core.
 *
 * void do_some_heavy_work(void)
 * {
 *    RETRO_PERFORMANCE_INIT(cb, work_1;
 *    RETRO_PERFORMANCE_START(cb, work_1);
 *    heavy_work_1();
 *    RETRO_PERFORMANCE_STOP(cb, work_1);
 *
 *    RETRO_PERFORMANCE_INIT(cb, work_2);
 *    RETRO_PERFORMANCE_START(cb, work_2);
 *    heavy_work_2();
 *    RETRO_PERFORMANCE_STOP(cb, work_2);
 * }
 *
 * void retro_deinit(void)
 * {
 *    perf_cb.perf_log();  * Log all perf counters here for example.
 * }
 */

struct retro_perf_callback
{
   retro_perf_get_time_usec_t    get_time_usec;
   retro_get_cpu_features_t      get_cpu_features;

   retro_perf_get_counter_t      get_perf_counter;
   retro_perf_register_t         perf_register;
   retro_perf_start_t            perf_start;
   retro_perf_stop_t             perf_stop;
   retro_perf_log_t              perf_log;
};

/**
 * @defgroup RETRO_SENSOR Sensor Interface
 * @todo Document the sensor API and work out behavior.
 * @todo It will be marked as experimental until then.
 * @{
 */

/**
 * Defines actions that can be performed on sensors.
 * @note Cores should only enable sensors while they're actively being used;
 * depending on the frontend and platform,
 * enabling these sensors may impact battery life.
 *
 * @see RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE
 * @see retro_sensor_interface
 * @see retro_set_sensor_state_t
 */
enum retro_sensor_action
{
   /** Enables accelerometer input, if one exists. */
   RETRO_SENSOR_ACCELEROMETER_ENABLE = 0,

   /** Disables accelerometer input, if one exists. */
   RETRO_SENSOR_ACCELEROMETER_DISABLE,

   /** Enables gyroscope input, if one exists. */
   RETRO_SENSOR_GYROSCOPE_ENABLE,

   /** Disables gyroscope input, if one exists. */
   RETRO_SENSOR_GYROSCOPE_DISABLE,

   /** Enables ambient light input, if a luminance sensor exists. */
   RETRO_SENSOR_ILLUMINANCE_ENABLE,

   /** Disables ambient light input, if a luminance sensor exists. */
   RETRO_SENSOR_ILLUMINANCE_DISABLE,

   /** @private Defined to ensure <tt>sizeof(enum retro_sensor_action) == sizeof(int)</tt>. Do not use. */
   RETRO_SENSOR_DUMMY = INT_MAX
};

/** @defgroup RETRO_SENSOR_ID Sensor Value IDs
 * @{
 */
/* Id values for SENSOR types. */

/**
 * Returns the device's acceleration along its local X axis minus the effect of gravity, in m/s^2.
 *
 * Positive values mean that the device is accelerating to the right.
 * assuming the user is looking at it head-on.
 */
#define RETRO_SENSOR_ACCELEROMETER_X 0

/**
 * Returns the device's acceleration along its local Y axis minus the effect of gravity, in m/s^2.
 *
 * Positive values mean that the device is accelerating upwards,
 * assuming the user is looking at it head-on.
 */
#define RETRO_SENSOR_ACCELEROMETER_Y 1

/**
 * Returns the the device's acceleration along its local Z axis minus the effect of gravity, in m/s^2.
 *
 * Positive values indicate forward acceleration towards the user,
 * assuming the user is looking at the device head-on.
 */
#define RETRO_SENSOR_ACCELEROMETER_Z 2

/**
 * Returns the angular velocity of the device around its local X axis, in radians per second.
 *
 * Positive values indicate counter-clockwise rotation.
 *
 * @note A radian is about 57 degrees, and a full 360-degree rotation is 2*pi radians.
 * @see https://developer.android.com/reference/android/hardware/SensorEvent#sensor.type_gyroscope
 * for guidance on using this value to derive a device's orientation.
 */
#define RETRO_SENSOR_GYROSCOPE_X 3

/**
 * Returns the angular velocity of the device around its local Z axis, in radians per second.
 *
 * Positive values indicate counter-clockwise rotation.
 *
 * @note A radian is about 57 degrees, and a full 360-degree rotation is 2*pi radians.
 * @see https://developer.android.com/reference/android/hardware/SensorEvent#sensor.type_gyroscope
 * for guidance on using this value to derive a device's orientation.
 */
#define RETRO_SENSOR_GYROSCOPE_Y 4

/**
 * Returns the angular velocity of the device around its local Z axis, in radians per second.
 *
 * Positive values indicate counter-clockwise rotation.
 *
 * @note A radian is about 57 degrees, and a full 360-degree rotation is 2*pi radians.
 * @see https://developer.android.com/reference/android/hardware/SensorEvent#sensor.type_gyroscope
 * for guidance on using this value to derive a device's orientation.
 */
#define RETRO_SENSOR_GYROSCOPE_Z 5

/**
 * Returns the ambient illuminance (light intensity) of the device's environment, in lux.
 *
 * @see https://en.wikipedia.org/wiki/Lux for a table of common lux values.
 */
#define RETRO_SENSOR_ILLUMINANCE 6
/** @} */

/**
 * Adjusts the state of a sensor.
 *
 * @param port The device port of the controller that owns the sensor given in \c action.
 * @param action The action to perform on the sensor.
 * Different devices support different sensors.
 * @param rate The rate at which the underlying sensor should be updated, in Hz.
 * This should be treated as a hint,
 * as some device sensors may not support the requested rate
 * (if it's configurable at all).
 * @returns \c true if the sensor state was successfully adjusted, \c false otherwise.
 * @note If one of the \c RETRO_SENSOR_*_ENABLE actions fails,
 * this likely means that the given sensor is not available
 * on the provided \c port.
 * @see retro_sensor_action
 */
typedef bool (RETRO_CALLCONV *retro_set_sensor_state_t)(unsigned port,
      enum retro_sensor_action action, unsigned rate);

/**
 * Retrieves the current value reported by sensor.
 * @param port The device port of the controller that owns the sensor given in \c id.
 * @param id The sensor value to query.
 * @returns The current sensor value.
 * Exact semantics depend on the value given in \c id,
 * but will return 0 for invalid arguments.
 *
 * @see RETRO_SENSOR_ID
 */
typedef float (RETRO_CALLCONV *retro_sensor_get_input_t)(unsigned port, unsigned id);

/**
 * An interface that cores can use to access device sensors.
 *
 * All function pointers are set by the frontend.
 */
struct retro_sensor_interface
{
   /** @copydoc retro_set_sensor_state_t */
   retro_set_sensor_state_t set_sensor_state;

   /** @copydoc retro_sensor_get_input_t */
   retro_sensor_get_input_t get_sensor_input;
};

/** @} */

/** @defgroup GET_CAMERA_INTERFACE Camera Interface
 * @{
 */

enum retro_camera_buffer
{
   RETRO_CAMERA_BUFFER_OPENGL_TEXTURE = 0,
   RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER,

   RETRO_CAMERA_BUFFER_DUMMY = INT_MAX
};

/* Starts the camera driver. Can only be called in retro_run(). */
typedef bool (RETRO_CALLCONV *retro_camera_start_t)(void);

/* Stops the camera driver. Can only be called in retro_run(). */
typedef void (RETRO_CALLCONV *retro_camera_stop_t)(void);

/* Callback which signals when the camera driver is initialized
 * and/or deinitialized.
 * retro_camera_start_t can be called in initialized callback.
 */
typedef void (RETRO_CALLCONV *retro_camera_lifetime_status_t)(void);

/* A callback for raw framebuffer data. buffer points to an XRGB8888 buffer.
 * Width, height and pitch are similar to retro_video_refresh_t.
 * First pixel is top-left origin.
 */
typedef void (RETRO_CALLCONV *retro_camera_frame_raw_framebuffer_t)(const uint32_t *buffer,
      unsigned width, unsigned height, size_t pitch);

/* A callback for when OpenGL textures are used.
 *
 * texture_id is a texture owned by camera driver.
 * Its state or content should be considered immutable, except for things like
 * texture filtering and clamping.
 *
 * texture_target is the texture target for the GL texture.
 * These can include e.g. GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE, and possibly
 * more depending on extensions.
 *
 * affine points to a packed 3x3 column-major matrix used to apply an affine
 * transform to texture coordinates. (affine_matrix * vec3(coord_x, coord_y, 1.0))
 * After transform, normalized texture coord (0, 0) should be bottom-left
 * and (1, 1) should be top-right (or (width, height) for RECTANGLE).
 *
 * GL-specific typedefs are avoided here to avoid relying on gl.h in
 * the API definition.
 */
typedef void (RETRO_CALLCONV *retro_camera_frame_opengl_texture_t)(unsigned texture_id,
      unsigned texture_target, const float *affine);

/**
 * @see RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE
 */
struct retro_camera_callback
{
   /* Set by libretro core.
    * Example bitmask: caps = (1 << RETRO_CAMERA_BUFFER_OPENGL_TEXTURE) | (1 << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER).
    */
   uint64_t caps;

   /* Desired resolution for camera. Is only used as a hint. */
   unsigned width;
   unsigned height;

   /**
    * Starts the camera driver.
    * Set by the frontend.
    * @returns \c true if the driver was successfully started.
    * @note Must be called in <tt>retro_run()</tt>.
    * @see retro_camera_callback
    */
   retro_camera_start_t start;

   /**
    * Stops the camera driver.
    * @note Must be called in <tt>retro_run()</tt>.
    * @warning The frontend may close the camera on its own when unloading the core,
    * but this behavior is not guaranteed.
    * Cores should clean up the camera before exiting.
    * @see retro_camera_callback
    */
   retro_camera_stop_t stop;

   /* Set by libretro core if raw framebuffer callbacks will be used. */
   retro_camera_frame_raw_framebuffer_t frame_raw_framebuffer;

   /* Set by libretro core if OpenGL texture callbacks will be used. */
   retro_camera_frame_opengl_texture_t frame_opengl_texture;

   /**
    * Core-defined callback invoked by the frontend right after the camera driver is initialized
    * (\em not when calling \c start).
    * May be \c NULL, in which case this function is skipped.
    */
   retro_camera_lifetime_status_t initialized;

   /**
    * Core-defined callback invoked by the frontend
    * right before the video camera driver is deinitialized
    * (\em not when calling \c stop).
    * May be \c NULL, in which case this function is skipped.
    */
   retro_camera_lifetime_status_t deinitialized;
};

/* Sets the interval of time and/or distance at which to update/poll
 * location-based data.
 *
 * To ensure compatibility with all location-based implementations,
 * values for both interval_ms and interval_distance should be provided.
 *
 * interval_ms is the interval expressed in milliseconds.
 * interval_distance is the distance interval expressed in meters.
 */
typedef void (RETRO_CALLCONV *retro_location_set_interval_t)(unsigned interval_ms,
      unsigned interval_distance);

/* Start location services. The device will start listening for changes to the
 * current location at regular intervals (which are defined with
 * retro_location_set_interval_t). */
typedef bool (RETRO_CALLCONV *retro_location_start_t)(void);

/* Stop location services. The device will stop listening for changes
 * to the current location. */
typedef void (RETRO_CALLCONV *retro_location_stop_t)(void);

/* Get the position of the current location. Will set parameters to
 * 0 if no new  location update has happened since the last time. */
typedef bool (RETRO_CALLCONV *retro_location_get_position_t)(double *lat, double *lon,
      double *horiz_accuracy, double *vert_accuracy);

/* Callback which signals when the location driver is initialized
 * and/or deinitialized.
 * retro_location_start_t can be called in initialized callback.
 */
typedef void (RETRO_CALLCONV *retro_location_lifetime_status_t)(void);

struct retro_location_callback
{
   retro_location_start_t         start;
   retro_location_stop_t          stop;
   retro_location_get_position_t  get_position;
   retro_location_set_interval_t  set_interval;

   retro_location_lifetime_status_t initialized;
   retro_location_lifetime_status_t deinitialized;
};

/** @addtogroup GET_RUMBLE_INTERFACE
 * @{ */

/**
 * The type of rumble motor in a controller.
 *
 * Both motors can be controlled independently,
 * and the strong motor does not override the weak motor.
 * @see RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE
 */
enum retro_rumble_effect
{
   RETRO_RUMBLE_STRONG = 0,
   RETRO_RUMBLE_WEAK = 1,

   /** @private Defined to ensure <tt>sizeof(enum retro_rumble_effect) == sizeof(int)</tt>. Do not use. */
   RETRO_RUMBLE_DUMMY = INT_MAX
};

/**
 * Requests a rumble state change for a controller.
 *
 * @param port The controller port to set the rumble state for.
 * @param effect The rumble motor to set the strength of.
 * @param strength The desired intensity of the rumble motor, ranging from \c 0 to \c 0xffff (inclusive).
 * @return \c true if the requested rumble state was honored.
 * If the controller doesn't support rumble, will return \c false.
 * @note Calling this before the first \c retro_run() may return \c false.
 * @see RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE
 */
typedef bool (RETRO_CALLCONV *retro_set_rumble_state_t)(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength);

/**
 * An interface that the core can use to set the rumble state of a controller.
 * @see RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE
 */
struct retro_rumble_interface
{
   /** Set by the frontend. */
   retro_set_rumble_state_t set_rumble_state;
};

/** @} */

/**
 * Called by the frontend to request audio samples.
 * The core should render audio within this function
 * using the callback provided by \c retro_set_audio_sample or \c retro_set_audio_sample_batch.
 *
 * @warning This function may be called by any thread,
 * therefore it must be thread-safe.
 * @see RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK
 * @see retro_audio_callback
 * @see retro_audio_sample_batch_t
 * @see retro_audio_sample_t
 */
typedef void (RETRO_CALLCONV *retro_audio_callback_t)(void);

/**
 * Called by the frontend to notify the core that it should pause or resume audio rendering.
 * The initial state of the audio driver after registering this callback is \c false (inactive).
 *
 * @param enabled \c true if the frontend's audio driver is active.
 * If so, the registered audio callback will be called regularly.
 * If not, the audio callback will not be invoked until the next time
 * the frontend calls this function with \c true.
 * @warning This function may be called by any thread,
 * therefore it must be thread-safe.
 * @note Even if no audio samples are rendered,
 * the core should continue to update its emulated platform's audio engine if necessary.
 * @see RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK
 * @see retro_audio_callback
 * @see retro_audio_callback_t
 */
typedef void (RETRO_CALLCONV *retro_audio_set_state_callback_t)(bool enabled);

/**
 * An interface that the frontend uses to request audio samples from the core.
 * @note To unregister a callback, pass a \c retro_audio_callback_t
 * with both fields set to <tt>NULL</tt>.
 * @see RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK
 */
struct retro_audio_callback
{
   /** @see retro_audio_callback_t */
   retro_audio_callback_t callback;

   /** @see retro_audio_set_state_callback_t */
   retro_audio_set_state_callback_t set_state;
};

typedef int64_t retro_usec_t;

/**
 * Called right before each iteration of \c retro_run
 * if registered via <tt>RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK</tt>.
 *
 * @param usec Time since the last call to <tt>retro_run</tt>, in microseconds.
 * If the frontend is manipulating the frame time
 * (e.g. via fast-forward or slow motion),
 * this value will be the reference value initially provided to the environment call.
 * @see RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK
 * @see retro_frame_time_callback
 */
typedef void (RETRO_CALLCONV *retro_frame_time_callback_t)(retro_usec_t usec);

/**
 * @see RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK
 */
struct retro_frame_time_callback
{
   /**
    * Called to notify the core of the current frame time.
    * If <tt>NULL</tt>, the frontend will clear its registered callback.
    */
   retro_frame_time_callback_t callback;

   /**
    * The ideal duration of one frame, in microseconds.
    * Compute it as <tt>1000000 / fps</tt>.
    * The frontend will resolve rounding to ensure that framestepping, etc is exact.
    */
   retro_usec_t reference;
};

/* Notifies a libretro core of the current occupancy
 * level of the frontend audio buffer.
 *
 * - active: 'true' if audio buffer is currently
 *           in use. Will be 'false' if audio is
 *           disabled in the frontend
 *
 * - occupancy: Given as a value in the range [0,100],
 *              corresponding to the occupancy percentage
 *              of the audio buffer
 *
 * - underrun_likely: 'true' if the frontend expects an
 *                    audio buffer underrun during the
 *                    next frame (indicates that a core
 *                    should attempt frame skipping)
 *
 * It will be called right before retro_run() every frame. */
typedef void (RETRO_CALLCONV *retro_audio_buffer_status_callback_t)(
      bool active, unsigned occupancy, bool underrun_likely);
struct retro_audio_buffer_status_callback
{
   retro_audio_buffer_status_callback_t callback;
};

/* Pass this to retro_video_refresh_t if rendering to hardware.
 * Passing NULL to retro_video_refresh_t is still a frame dupe as normal.
 * */
#define RETRO_HW_FRAME_BUFFER_VALID ((void*)-1)

/* Invalidates the current HW context.
 * Any GL state is lost, and must not be deinitialized explicitly.
 * If explicit deinitialization is desired by the libretro core,
 * it should implement context_destroy callback.
 * If called, all GPU resources must be reinitialized.
 * Usually called when frontend reinits video driver.
 * Also called first time video driver is initialized,
 * allowing libretro core to initialize resources.
 */
typedef void (RETRO_CALLCONV *retro_hw_context_reset_t)(void);

/* Gets current framebuffer which is to be rendered to.
 * Could change every frame potentially.
 */
typedef uintptr_t (RETRO_CALLCONV *retro_hw_get_current_framebuffer_t)(void);

/* Get a symbol from HW context. */
typedef retro_proc_address_t (RETRO_CALLCONV *retro_hw_get_proc_address_t)(const char *sym);

enum retro_hw_context_type
{
   RETRO_HW_CONTEXT_NONE             = 0,
   /* OpenGL 2.x. Driver can choose to use latest compatibility context. */
   RETRO_HW_CONTEXT_OPENGL           = 1,
   /* OpenGL ES 2.0. */
   RETRO_HW_CONTEXT_OPENGLES2        = 2,
   /* Modern desktop core GL context. Use version_major/
    * version_minor fields to set GL version. */
   RETRO_HW_CONTEXT_OPENGL_CORE      = 3,
   /* OpenGL ES 3.0 */
   RETRO_HW_CONTEXT_OPENGLES3        = 4,
   /* OpenGL ES 3.1+. Set version_major/version_minor. For GLES2 and GLES3,
    * use the corresponding enums directly. */
   RETRO_HW_CONTEXT_OPENGLES_VERSION = 5,

   /* Vulkan, see RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE. */
   RETRO_HW_CONTEXT_VULKAN           = 6,

   /* Direct3D11, see RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE */
   RETRO_HW_CONTEXT_D3D11            = 7,

   /* Direct3D10, see RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE */
   RETRO_HW_CONTEXT_D3D10            = 8,

   /* Direct3D12, see RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE */
   RETRO_HW_CONTEXT_D3D12            = 9,

   /* Direct3D9, see RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE */
   RETRO_HW_CONTEXT_D3D9             = 10,

   /** Dummy value to ensure sizeof(enum retro_hw_context_type) == sizeof(int). Do not use. */
   RETRO_HW_CONTEXT_DUMMY = INT_MAX
};

struct retro_hw_render_callback
{
   /* Which API to use. Set by libretro core. */
   enum retro_hw_context_type context_type;

   /* Called when a context has been created or when it has been reset.
    * An OpenGL context is only valid after context_reset() has been called.
    *
    * When context_reset is called, OpenGL resources in the libretro
    * implementation are guaranteed to be invalid.
    *
    * It is possible that context_reset is called multiple times during an
    * application lifecycle.
    * If context_reset is called without any notification (context_destroy),
    * the OpenGL context was lost and resources should just be recreated
    * without any attempt to "free" old resources.
    */
   retro_hw_context_reset_t context_reset;

   /* Set by frontend.
    * TODO: This is rather obsolete. The frontend should not
    * be providing preallocated framebuffers. */
   retro_hw_get_current_framebuffer_t get_current_framebuffer;

   /* Set by frontend.
    * Can return all relevant functions, including glClear on Windows. */
   retro_hw_get_proc_address_t get_proc_address;

   /* Set if render buffers should have depth component attached.
    * TODO: Obsolete. */
   bool depth;

   /* Set if stencil buffers should be attached.
    * TODO: Obsolete. */
   bool stencil;

   /* If depth and stencil are true, a packed 24/8 buffer will be added.
    * Only attaching stencil is invalid and will be ignored. */

   /* Use conventional bottom-left origin convention. If false,
    * standard libretro top-left origin semantics are used.
    * TODO: Move to GL specific interface. */
   bool bottom_left_origin;

   /* Major version number for core GL context or GLES 3.1+. */
   unsigned version_major;

   /* Minor version number for core GL context or GLES 3.1+. */
   unsigned version_minor;

   /* If this is true, the frontend will go very far to avoid
    * resetting context in scenarios like toggling fullscreen, etc.
    * TODO: Obsolete? Maybe frontend should just always assume this ...
    */
   bool cache_context;

   /* The reset callback might still be called in extreme situations
    * such as if the context is lost beyond recovery.
    *
    * For optimal stability, set this to false, and allow context to be
    * reset at any time.
    */

   /* A callback to be called before the context is destroyed in a
    * controlled way by the frontend. */
   retro_hw_context_reset_t context_destroy;

   /* OpenGL resources can be deinitialized cleanly at this step.
    * context_destroy can be set to NULL, in which resources will
    * just be destroyed without any notification.
    *
    * Even when context_destroy is non-NULL, it is possible that
    * context_reset is called without any destroy notification.
    * This happens if context is lost by external factors (such as
    * notified by GL_ARB_robustness).
    *
    * In this case, the context is assumed to be already dead,
    * and the libretro implementation must not try to free any OpenGL
    * resources in the subsequent context_reset.
    */

   /* Creates a debug context. */
   bool debug_context;
};

/* Callback type passed in RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK.
 * Called by the frontend in response to keyboard events.
 * down is set if the key is being pressed, or false if it is being released.
 * keycode is the RETROK value of the char.
 * character is the text character of the pressed key. (UTF-32).
 * key_modifiers is a set of RETROKMOD values or'ed together.
 *
 * The pressed/keycode state can be indepedent of the character.
 * It is also possible that multiple characters are generated from a
 * single keypress.
 * Keycode events should be treated separately from character events.
 * However, when possible, the frontend should try to synchronize these.
 * If only a character is posted, keycode should be RETROK_UNKNOWN.
 *
 * Similarily if only a keycode event is generated with no corresponding
 * character, character should be 0.
 */
typedef void (RETRO_CALLCONV *retro_keyboard_event_t)(bool down, unsigned keycode,
      uint32_t character, uint16_t key_modifiers);

struct retro_keyboard_callback
{
   retro_keyboard_event_t callback;
};

/* Callbacks for RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE &
 * RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE.
 * Should be set for implementations which can swap out multiple disk
 * images in runtime.
 *
 * If the implementation can do this automatically, it should strive to do so.
 * However, there are cases where the user must manually do so.
 *
 * Overview: To swap a disk image, eject the disk image with
 * set_eject_state(true).
 * Set the disk index with set_image_index(index). Insert the disk again
 * with set_eject_state(false).
 */

/* If ejected is true, "ejects" the virtual disk tray.
 * When ejected, the disk image index can be set.
 */
typedef bool (RETRO_CALLCONV *retro_set_eject_state_t)(bool ejected);

/* Gets current eject state. The initial state is 'not ejected'. */
typedef bool (RETRO_CALLCONV *retro_get_eject_state_t)(void);

/* Gets current disk index. First disk is index 0.
 * If return value is >= get_num_images(), no disk is currently inserted.
 */
typedef unsigned (RETRO_CALLCONV *retro_get_image_index_t)(void);

/* Sets image index. Can only be called when disk is ejected.
 * The implementation supports setting "no disk" by using an
 * index >= get_num_images().
 */
typedef bool (RETRO_CALLCONV *retro_set_image_index_t)(unsigned index);

/* Gets total number of images which are available to use. */
typedef unsigned (RETRO_CALLCONV *retro_get_num_images_t)(void);

struct retro_game_info;

/* Replaces the disk image associated with index.
 * Arguments to pass in info have same requirements as retro_load_game().
 * Virtual disk tray must be ejected when calling this.
 *
 * Replacing a disk image with info = NULL will remove the disk image
 * from the internal list.
 * As a result, calls to get_image_index() can change.
 *
 * E.g. replace_image_index(1, NULL), and previous get_image_index()
 * returned 4 before.
 * Index 1 will be removed, and the new index is 3.
 */
typedef bool (RETRO_CALLCONV *retro_replace_image_index_t)(unsigned index,
      const struct retro_game_info *info);

/* Adds a new valid index (get_num_images()) to the internal disk list.
 * This will increment subsequent return values from get_num_images() by 1.
 * This image index cannot be used until a disk image has been set
 * with replace_image_index. */
typedef bool (RETRO_CALLCONV *retro_add_image_index_t)(void);

/* Sets initial image to insert in drive when calling
 * core_load_game().
 * Since we cannot pass the initial index when loading
 * content (this would require a major API change), this
 * is set by the frontend *before* calling the core's
 * retro_load_game()/retro_load_game_special() implementation.
 * A core should therefore cache the index/path values and handle
 * them inside retro_load_game()/retro_load_game_special().
 * - If 'index' is invalid (index >= get_num_images()), the
 *   core should ignore the set value and instead use 0
 * - 'path' is used purely for error checking - i.e. when
 *   content is loaded, the core should verify that the
 *   disk specified by 'index' has the specified file path.
 *   This is to guard against auto selecting the wrong image
 *   if (for example) the user should modify an existing M3U
 *   playlist. We have to let the core handle this because
 *   set_initial_image() must be called before loading content,
 *   i.e. the frontend cannot access image paths in advance
 *   and thus cannot perform the error check itself.
 *   If set path and content path do not match, the core should
 *   ignore the set 'index' value and instead use 0
 * Returns 'false' if index or 'path' are invalid, or core
 * does not support this functionality
 */
typedef bool (RETRO_CALLCONV *retro_set_initial_image_t)(unsigned index, const char *path);

/* Fetches the path of the specified disk image file.
 * Returns 'false' if index is invalid (index >= get_num_images())
 * or path is otherwise unavailable.
 */
typedef bool (RETRO_CALLCONV *retro_get_image_path_t)(unsigned index, char *path, size_t len);

/* Fetches a core-provided 'label' for the specified disk
 * image file. In the simplest case this may be a file name
 * (without extension), but for cores with more complex
 * content requirements information may be provided to
 * facilitate user disk swapping - for example, a core
 * running floppy-disk-based content may uniquely label
 * save disks, data disks, level disks, etc. with names
 * corresponding to in-game disk change prompts (so the
 * frontend can provide better user guidance than a 'dumb'
 * disk index value).
 * Returns 'false' if index is invalid (index >= get_num_images())
 * or label is otherwise unavailable.
 */
typedef bool (RETRO_CALLCONV *retro_get_image_label_t)(unsigned index, char *label, size_t len);

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

struct retro_disk_control_ext_callback
{
   retro_set_eject_state_t set_eject_state;
   retro_get_eject_state_t get_eject_state;

   retro_get_image_index_t get_image_index;
   retro_set_image_index_t set_image_index;
   retro_get_num_images_t  get_num_images;

   retro_replace_image_index_t replace_image_index;
   retro_add_image_index_t add_image_index;

   /* NOTE: Frontend will only attempt to record/restore
    * last used disk index if both set_initial_image()
    * and get_image_path() are implemented */
   retro_set_initial_image_t set_initial_image; /* Optional - may be NULL */

   retro_get_image_path_t get_image_path;       /* Optional - may be NULL */
   retro_get_image_label_t get_image_label;     /* Optional - may be NULL */
};

/* Definitions for RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE.
 * A core can set it if sending and receiving custom network packets
 * during a multiplayer session is desired.
 */

/* Netpacket flags for retro_netpacket_send_t */
#define RETRO_NETPACKET_UNRELIABLE  0        /* Packet to be sent unreliable, depending on network quality it might not arrive. */
#define RETRO_NETPACKET_RELIABLE    (1 << 0) /* Reliable packets are guaranteed to arrive at the target in the order they were send. */
#define RETRO_NETPACKET_UNSEQUENCED (1 << 1) /* Packet will not be sequenced with other packets and may arrive out of order. Cannot be set on reliable packets. */

/* Used by the core to send a packet to one or more connected players.
 * A single packet sent via this interface can contain up to 64 KB of data.
 *
 * The broadcast flag can be set to true to send to multiple connected clients.
 * In a broadcast, the client_id argument indicates 1 client NOT to send the
 * packet to (pass 0xFFFF to send to everyone). Otherwise, the client_id
 * argument indicates a single client to send the packet to.
 *
 * A frontend must support sending reliable packets (RETRO_NETPACKET_RELIABLE).
 * Unreliable packets might not be supported by the frontend, but the flags can
 * still be specified. Reliable transmission will be used instead.
 *
 * If this function is called passing NULL for buf, it will instead flush all
 * previously buffered outgoing packets and instantly read any incoming packets.
 * During such a call, retro_netpacket_receive_t and retro_netpacket_stop_t can
 * be called. The core can perform this in a loop to do a blocking read, i.e.,
 * wait for incoming data, but needs to handle stop getting called and also
 * give up after a short while to avoid freezing on a connection problem.
 *
 * This function is not guaranteed to be thread-safe and must be called during
 * retro_run or any of the netpacket callbacks passed with this interface.
 */
typedef void (RETRO_CALLCONV *retro_netpacket_send_t)(int flags, const void* buf, size_t len, uint16_t client_id, bool broadcast);

/* Called by the frontend to signify that a multiplayer session has started.
 * If client_id is 0 the local player is the host of the session and at this
 * point no other player has connected yet.
 *
 * If client_id is > 0 the local player is a client connected to a host and
 * at this point is already fully connected to the host.
 *
 * The core must store the retro_netpacket_send_t function pointer provided
 * here and use it whenever it wants to send a packet. This function pointer
 * remains valid until the frontend calls retro_netpacket_stop_t.
 */
typedef void (RETRO_CALLCONV *retro_netpacket_start_t)(uint16_t client_id, retro_netpacket_send_t send_fn);

/* Called by the frontend when a new packet arrives which has been sent from
 * another player with retro_netpacket_send_t. The client_id argument indicates
 * who has sent the packet.
 */
typedef void (RETRO_CALLCONV *retro_netpacket_receive_t)(const void* buf, size_t len, uint16_t client_id);

/* Called by the frontend when the multiplayer session has ended.
 * Once this gets called the retro_netpacket_send_t function pointer passed
 * to retro_netpacket_start_t will not be valid anymore.
 */
typedef void (RETRO_CALLCONV *retro_netpacket_stop_t)(void);

/* Called by the frontend every frame (between calls to retro_run while
 * updating the state of the multiplayer session.
 * This is a good place for the core to call retro_netpacket_send_t from.
 */
typedef void (RETRO_CALLCONV *retro_netpacket_poll_t)(void);

/* Called by the frontend when a new player connects to the hosted session.
 * This is only called on the host side, not for clients connected to the host.
 * If this function returns false, the newly connected player gets dropped.
 * This can be used for example to limit the number of players.
 */
typedef bool (RETRO_CALLCONV *retro_netpacket_connected_t)(uint16_t client_id);

/* Called by the frontend when a player leaves or disconnects from the hosted session.
 * This is only called on the host side, not for clients connected to the host.
 */
typedef void (RETRO_CALLCONV *retro_netpacket_disconnected_t)(uint16_t client_id);

/**
 * A callback interface for giving a core the ability to send and receive custom
 * network packets during a multiplayer session between two or more instances
 * of a libretro frontend.
 *
 * @see RETRO_ENVIRONMENT_SET_NETPACKET_INTERFACE
 */
struct retro_netpacket_callback
{
   retro_netpacket_start_t        start;
   retro_netpacket_receive_t      receive;
   retro_netpacket_stop_t         stop;         /* Optional - may be NULL */
   retro_netpacket_poll_t         poll;         /* Optional - may be NULL */
   retro_netpacket_connected_t    connected;    /* Optional - may be NULL */
   retro_netpacket_disconnected_t disconnected; /* Optional - may be NULL */
};

/**
 * The pixel format used for rendering.
 * @see RETRO_ENVIRONMENT_SET_PIXEL_FORMAT
 */
enum retro_pixel_format
{
   /**
    * 0RGB1555, native endian.
    * Used as the default if \c RETRO_ENVIRONMENT_SET_PIXEL_FORMAT is not called.
    * The most significant bit must be set to 0.
    * @deprecated This format remains supported to maintain compatibility.
    * New code should use <tt>RETRO_PIXEL_FORMAT_RGB565</tt> instead.
    * @see RETRO_PIXEL_FORMAT_RGB565
    */
   RETRO_PIXEL_FORMAT_0RGB1555 = 0,

   /**
    * XRGB8888, native endian.
    * The most significant byte (the <tt>X</tt>) is ignored.
    */
   RETRO_PIXEL_FORMAT_XRGB8888 = 1,

   /**
    * RGB565, native endian.
    * This format is recommended if 16-bit pixels are desired,
    * as it is available on a variety of devices and APIs.
    */
   RETRO_PIXEL_FORMAT_RGB565   = 2,

   /** Defined to ensure that <tt>sizeof(retro_pixel_format) == sizeof(int)</tt>. Do not use. */
   RETRO_PIXEL_FORMAT_UNKNOWN  = INT_MAX
};

enum retro_savestate_context
{
   /* Standard savestate written to disk. */
   RETRO_SAVESTATE_CONTEXT_NORMAL                 = 0,

   /* Savestate where you are guaranteed that the same instance will load the save state.
    * You can store internal pointers to code or data.
    * It's still a full serialization and deserialization, and could be loaded or saved at any time.
    * It won't be written to disk or sent over the network.
    */
   RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_INSTANCE = 1,

   /* Savestate where you are guaranteed that the same emulator binary will load that savestate.
    * You can skip anything that would slow down saving or loading state but you can not store internal pointers.
    * It won't be written to disk or sent over the network.
    * Example: "Second Instance" runahead
    */
   RETRO_SAVESTATE_CONTEXT_RUNAHEAD_SAME_BINARY   = 2,

   /* Savestate used within a rollback netplay feature.
    * You should skip anything that would unnecessarily increase bandwidth usage.
    * It won't be written to disk but it will be sent over the network.
    */
   RETRO_SAVESTATE_CONTEXT_ROLLBACK_NETPLAY       = 3,

   /* Ensure sizeof() == sizeof(int). */
   RETRO_SAVESTATE_CONTEXT_UNKNOWN                = INT_MAX
};

/**
 * Defines a message that the frontend will display to the user,
 * as determined by <tt>RETRO_ENVIRONMENT_SET_MESSAGE</tt>.
 * @see RETRO_ENVIRONMENT_SET_MESSAGE
 * @see retro_message_ext
 */
struct retro_message
{
   /**
    * Null-terminated message to be displayed.
    * If \c NULL or empty, the message will be ignored.
    */
   const char *msg;

   /** Duration to display \c msg in frames. */
   unsigned    frames;
};

enum retro_message_target
{
   RETRO_MESSAGE_TARGET_ALL = 0,
   RETRO_MESSAGE_TARGET_OSD,
   RETRO_MESSAGE_TARGET_LOG
};

enum retro_message_type
{
   RETRO_MESSAGE_TYPE_NOTIFICATION = 0,
   RETRO_MESSAGE_TYPE_NOTIFICATION_ALT,
   RETRO_MESSAGE_TYPE_STATUS,
   RETRO_MESSAGE_TYPE_PROGRESS
};

struct retro_message_ext
{
   /* Message string to be displayed/logged */
   const char *msg;
   /* Duration (in ms) of message when targeting the OSD */
   unsigned duration;
   /* Message priority when targeting the OSD
    * > When multiple concurrent messages are sent to
    *   the frontend and the frontend does not have the
    *   capacity to display them all, messages with the
    *   *highest* priority value should be shown
    * > There is no upper limit to a message priority
    *   value (within the bounds of the unsigned data type)
    * > In the reference frontend (RetroArch), the same
    *   priority values are used for frontend-generated
    *   notifications, which are typically assigned values
    *   between 0 and 3 depending upon importance */
   unsigned priority;
   /* Message logging level (info, warn, error, etc.) */
   enum retro_log_level level;
   /* Message destination: OSD, logging interface or both */
   enum retro_message_target target;
   /* Message 'type' when targeting the OSD
    * > RETRO_MESSAGE_TYPE_NOTIFICATION: Specifies that a
    *   message should be handled in identical fashion to
    *   a standard frontend-generated notification
    * > RETRO_MESSAGE_TYPE_NOTIFICATION_ALT: Specifies that
    *   message is a notification that requires user attention
    *   or action, but that it should be displayed in a manner
    *   that differs from standard frontend-generated notifications.
    *   This would typically correspond to messages that should be
    *   displayed immediately (independently from any internal
    *   frontend message queue), and/or which should be visually
    *   distinguishable from frontend-generated notifications.
    *   For example, a core may wish to inform the user of
    *   information related to a disk-change event. It is
    *   expected that the frontend itself may provide a
    *   notification in this case; if the core sends a
    *   message of type RETRO_MESSAGE_TYPE_NOTIFICATION, an
    *   uncomfortable 'double-notification' may occur. A message
    *   of RETRO_MESSAGE_TYPE_NOTIFICATION_ALT should therefore
    *   be presented such that visual conflict with regular
    *   notifications does not occur
    * > RETRO_MESSAGE_TYPE_STATUS: Indicates that message
    *   is not a standard notification. This typically
    *   corresponds to 'status' indicators, such as a core's
    *   internal FPS, which are intended to be displayed
    *   either permanently while a core is running, or in
    *   a manner that does not suggest user attention or action
    *   is required. 'Status' type messages should therefore be
    *   displayed in a different on-screen location and in a manner
    *   easily distinguishable from both standard frontend-generated
    *   notifications and messages of type RETRO_MESSAGE_TYPE_NOTIFICATION_ALT
    * > RETRO_MESSAGE_TYPE_PROGRESS: Indicates that message reports
    *   the progress of an internal core task. For example, in cases
    *   where a core itself handles the loading of content from a file,
    *   this may correspond to the percentage of the file that has been
    *   read. Alternatively, an audio/video playback core may use a
    *   message of type RETRO_MESSAGE_TYPE_PROGRESS to display the current
    *   playback position as a percentage of the runtime. 'Progress' type
    *   messages should therefore be displayed as a literal progress bar,
    *   where:
    *   - 'retro_message_ext.msg' is the progress bar title/label
    *   - 'retro_message_ext.progress' determines the length of
    *     the progress bar
    * NOTE: Message type is a *hint*, and may be ignored
    * by the frontend. If a frontend lacks support for
    * displaying messages via alternate means than standard
    * frontend-generated notifications, it will treat *all*
    * messages as having the type RETRO_MESSAGE_TYPE_NOTIFICATION */
   enum retro_message_type type;
   /* Task progress when targeting the OSD and message is
    * of type RETRO_MESSAGE_TYPE_PROGRESS
    * > -1:    Unmetered/indeterminate
    * > 0-100: Current progress percentage
    * NOTE: Since message type is a hint, a frontend may ignore
    * progress values. Where relevant, a core should therefore
    * include progress percentage within the message string,
    * such that the message intent remains clear when displayed
    * as a standard frontend-generated notification */
   int8_t progress;
};

/* Describes how the libretro implementation maps a libretro input bind
 * to its internal input system through a human readable string.
 * This string can be used to better let a user configure input. */
struct retro_input_descriptor
{
   /* Associates given parameters with a description. */
   unsigned port;
   unsigned device;
   unsigned index;
   unsigned id;

   /* Human readable description for parameters.
    * The pointer must remain valid until
    * retro_unload_game() is called. */
   const char *description;
};

/**
 * Contains basic information about the core.
 *
 * @see retro_get_system_info
 * @warning All pointers are owned by the core
 * and must remain valid throughout its lifetime.
 */
struct retro_system_info
{

   const char *library_name;      /* Descriptive name of library. Should not
                                   * contain any version numbers, etc. */
   const char *library_version;   /* Descriptive version of core. */

   /**
    * A pipe-delimited string list of file extensions that this core can load, e.g. "bin|rom|iso".
    * Typically used by a frontend for filtering or core selection.
    */
   const char *valid_extensions;

   /* Libretro cores that need to have direct access to their content
    * files, including cores which use the path of the content files to
    * determine the paths of other files, should set need_fullpath to true.
    *
    * Cores should strive for setting need_fullpath to false,
    * as it allows the frontend to perform patching, etc.
    *
    * If need_fullpath is true and retro_load_game() is called:
    *    - retro_game_info::path is guaranteed to have a valid path
    *    - retro_game_info::data and retro_game_info::size are invalid
    *
    * If need_fullpath is false and retro_load_game() is called:
    *    - retro_game_info::path may be NULL
    *    - retro_game_info::data and retro_game_info::size are guaranteed
    *      to be valid
    *
    * See also:
    *    - RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY
    *    - RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY
    */
   bool        need_fullpath;

   /* If true, the frontend is not allowed to extract any archives before
    * loading the real content.
    * Necessary for certain libretro implementations that load games
    * from zipped archives. */
   bool        block_extract;
};

/* Defines overrides which modify frontend handling of
 * specific content file types.
 * An array of retro_system_content_info_override is
 * passed to RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE
 * NOTE: In the following descriptions, references to
 *       retro_load_game() may be replaced with
 *       retro_load_game_special() */
struct retro_system_content_info_override
{
   /* A list of file extensions for which the override
    * should apply, delimited by a 'pipe' character
    * (e.g. "md|sms|gg")
    * Permitted file extensions are limited to those
    * included in retro_system_info::valid_extensions
    * and/or retro_subsystem_rom_info::valid_extensions */
   const char *extensions;

   /* Overrides the need_fullpath value set in
    * retro_system_info and/or retro_subsystem_rom_info.
    * To reiterate:
    *
    * If need_fullpath is true and retro_load_game() is called:
    *    - retro_game_info::path is guaranteed to contain a valid
    *      path to an existent file
    *    - retro_game_info::data and retro_game_info::size are invalid
    *
    * If need_fullpath is false and retro_load_game() is called:
    *    - retro_game_info::path may be NULL
    *    - retro_game_info::data and retro_game_info::size are guaranteed
    *      to be valid
    *
    * In addition:
    *
    * If need_fullpath is true and retro_load_game() is called:
    *    - retro_game_info_ext::full_path is guaranteed to contain a valid
    *      path to an existent file
    *    - retro_game_info_ext::archive_path may be NULL
    *    - retro_game_info_ext::archive_file may be NULL
    *    - retro_game_info_ext::dir is guaranteed to contain a valid path
    *      to the directory in which the content file exists
    *    - retro_game_info_ext::name is guaranteed to contain the
    *      basename of the content file, without extension
    *    - retro_game_info_ext::ext is guaranteed to contain the
    *      extension of the content file in lower case format
    *    - retro_game_info_ext::data and retro_game_info_ext::size
    *      are invalid
    *
    * If need_fullpath is false and retro_load_game() is called:
    *    - If retro_game_info_ext::file_in_archive is false:
    *       - retro_game_info_ext::full_path is guaranteed to contain
    *         a valid path to an existent file
    *       - retro_game_info_ext::archive_path may be NULL
    *       - retro_game_info_ext::archive_file may be NULL
    *       - retro_game_info_ext::dir is guaranteed to contain a
    *         valid path to the directory in which the content file exists
    *       - retro_game_info_ext::name is guaranteed to contain the
    *         basename of the content file, without extension
    *       - retro_game_info_ext::ext is guaranteed to contain the
    *         extension of the content file in lower case format
    *    - If retro_game_info_ext::file_in_archive is true:
    *       - retro_game_info_ext::full_path may be NULL
    *       - retro_game_info_ext::archive_path is guaranteed to
    *         contain a valid path to an existent compressed file
    *         inside which the content file is located
    *       - retro_game_info_ext::archive_file is guaranteed to
    *         contain a valid path to an existent content file
    *         inside the compressed file referred to by
    *         retro_game_info_ext::archive_path
    *            e.g. for a compressed file '/path/to/foo.zip'
    *            containing 'bar.sfc'
    *             > retro_game_info_ext::archive_path will be '/path/to/foo.zip'
    *             > retro_game_info_ext::archive_file will be 'bar.sfc'
    *       - retro_game_info_ext::dir is guaranteed to contain a
    *         valid path to the directory in which the compressed file
    *         (containing the content file) exists
    *       - retro_game_info_ext::name is guaranteed to contain
    *         EITHER
    *         1) the basename of the compressed file (containing
    *            the content file), without extension
    *         OR
    *         2) the basename of the content file inside the
    *            compressed file, without extension
    *         In either case, a core should consider 'name' to
    *         be the canonical name/ID of the the content file
    *       - retro_game_info_ext::ext is guaranteed to contain the
    *         extension of the content file inside the compressed file,
    *         in lower case format
    *    - retro_game_info_ext::data and retro_game_info_ext::size are
    *      guaranteed to be valid */
   bool need_fullpath;

   /* If need_fullpath is false, specifies whether the content
    * data buffer available in retro_load_game() is 'persistent'
    *
    * If persistent_data is false and retro_load_game() is called:
    *    - retro_game_info::data and retro_game_info::size
    *      are valid only until retro_load_game() returns
    *    - retro_game_info_ext::data and retro_game_info_ext::size
    *      are valid only until retro_load_game() returns
    *
    * If persistent_data is true and retro_load_game() is called:
    *    - retro_game_info::data and retro_game_info::size
    *      are valid until retro_deinit() returns
    *    - retro_game_info_ext::data and retro_game_info_ext::size
    *      are valid until retro_deinit() returns */
   bool persistent_data;
};

/* Similar to retro_game_info, but provides extended
 * information about the source content file and
 * game memory buffer status.
 * And array of retro_game_info_ext is returned by
 * RETRO_ENVIRONMENT_GET_GAME_INFO_EXT
 * NOTE: In the following descriptions, references to
 *       retro_load_game() may be replaced with
 *       retro_load_game_special() */
struct retro_game_info_ext
{
   /* - If file_in_archive is false, contains a valid
    *   path to an existent content file (UTF-8 encoded)
    * - If file_in_archive is true, may be NULL */
   const char *full_path;

   /* - If file_in_archive is false, may be NULL
    * - If file_in_archive is true, contains a valid path
    *   to an existent compressed file inside which the
    *   content file is located (UTF-8 encoded) */
   const char *archive_path;

   /* - If file_in_archive is false, may be NULL
    * - If file_in_archive is true, contain a valid path
    *   to an existent content file inside the compressed
    *   file referred to by archive_path (UTF-8 encoded)
    *      e.g. for a compressed file '/path/to/foo.zip'
    *      containing 'bar.sfc'
    *      > archive_path will be '/path/to/foo.zip'
    *      > archive_file will be 'bar.sfc' */
   const char *archive_file;

   /* - If file_in_archive is false, contains a valid path
    *   to the directory in which the content file exists
    *   (UTF-8 encoded)
    * - If file_in_archive is true, contains a valid path
    *   to the directory in which the compressed file
    *   (containing the content file) exists (UTF-8 encoded) */
   const char *dir;

   /* Contains the canonical name/ID of the content file
    * (UTF-8 encoded). Intended for use when identifying
    * 'complementary' content named after the loaded file -
    * i.e. companion data of a different format (a CD image
    * required by a ROM), texture packs, internally handled
    * save files, etc.
    * - If file_in_archive is false, contains the basename
    *   of the content file, without extension
    * - If file_in_archive is true, then string is
    *   implementation specific. A frontend may choose to
    *   set a name value of:
    *   EITHER
    *   1) the basename of the compressed file (containing
    *      the content file), without extension
    *   OR
    *   2) the basename of the content file inside the
    *      compressed file, without extension
    *   RetroArch sets the 'name' value according to (1).
    *   A frontend that supports routine loading of
    *   content from archives containing multiple unrelated
    *   content files may set the 'name' value according
    *   to (2). */
   const char *name;

   /* - If file_in_archive is false, contains the extension
    *   of the content file in lower case format
    * - If file_in_archive is true, contains the extension
    *   of the content file inside the compressed file,
    *   in lower case format */
   const char *ext;

   /* String of implementation specific meta-data. */
   const char *meta;

   /* Memory buffer of loaded game content. Will be NULL:
    * IF
    * - retro_system_info::need_fullpath is true and
    *   retro_system_content_info_override::need_fullpath
    *   is unset
    * OR
    * - retro_system_content_info_override::need_fullpath
    *   is true */
   const void *data;

   /* Size of game content memory buffer, in bytes */
   size_t size;

   /* True if loaded content file is inside a compressed
    * archive */
   bool file_in_archive;

   /* - If data is NULL, value is unset/ignored
    * - If data is non-NULL:
    *   - If persistent_data is false, data and size are
    *     valid only until retro_load_game() returns
    *   - If persistent_data is true, data and size are
    *     are valid until retro_deinit() returns */
   bool persistent_data;
};

/**
 * Parameters describing the size and shape of the video frame.
 * @see retro_system_av_info
 * @see RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO
 * @see retro_get_system_av_info
 */
struct retro_game_geometry
{
   /**
    * Nominal video width of game, in pixels.
    * This will typically be the emulated platform's native video width
    * (or its smallest, if the original hardware supports multiple resolutions).
    */
   unsigned base_width;

   /**
    * Nominal video height of game, in pixels.
    * This will typically be the emulated platform's native video height
    * (or its smallest, if the original hardware supports multiple resolutions).
    */
   unsigned base_height;

   /**
    * Maximum possible width of the game screen, in pixels.
    * This will typically be the emulated platform's maximum video width.
    * For cores that emulate platforms with multiple screens (such as the Nintendo DS),
    * this should assume the core's widest possible screen layout (e.g. side-by-side).
    * For cores that support upscaling the resolution,
    * this should assume the highest supported scale factor is active.
    */
   unsigned max_width;

   /**
    * Maximum possible height of the game screen, in pixels.
    * This will typically be the emulated platform's maximum video height.
    * For cores that emulate platforms with multiple screens (such as the Nintendo DS),
    * this should assume the core's tallest possible screen layout (e.g. vertical).
    * For cores that support upscaling the resolution,
    * this should assume the highest supported scale factor is active.
    */
   unsigned max_height;    /* Maximum possible height of game. */

   /**
    * Nominal aspect ratio of game.
    * If zero or less,
    * an aspect ratio of <tt>base_width / base_height</tt> is assumed.
    *
    * @note A frontend may ignore this setting.
    */
   float    aspect_ratio;
};

/**
 * Parameters describing the timing of the video and audio.
 * @see retro_system_av_info
 * @see RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO
 * @see retro_get_system_av_info
 */
struct retro_system_timing
{
   /** Video output refresh rate, in frames per second. */
   double fps;

   /** The audio output sample rate, in Hz. */
   double sample_rate;
};

/**
 * Configures how the core's audio and video should be updated.
 * @see RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO
 * @see retro_get_system_av_info
 */
struct retro_system_av_info
{
   /** Parameters describing the size and shape of the video frame. */
   struct retro_game_geometry geometry;

   /** Parameters describing the timing of the video and audio. */
   struct retro_system_timing timing;
};

struct retro_variable
{
   /**
    * Variable to query in RETRO_ENVIRONMENT_GET_VARIABLE.
    * If NULL, obtains the complete environment string if more
    * complex parsing is necessary.
    * The environment string is formatted as key-value pairs
    * delimited by semicolons as so:
    * "key1=value1;key2=value2;..."
    * Should be prefixed with the core's name
    * to minimize the risk of collisions with another core's options.
    */
   const char *key;

   /* Value to be obtained. If key does not exist, it is set to NULL. */
   const char *value;
};

struct retro_core_option_display
{
   /* Variable to configure in RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY */
   const char *key;

   /* Specifies whether variable should be displayed
    * when presenting core options to the user */
   bool visible;
};

/* Maximum number of values permitted for a core option
 * > Note: We have to set a maximum value due the limitations
 *   of the C language - i.e. it is not possible to create an
 *   array of structs each containing a variable sized array,
 *   so the retro_core_option_definition values array must
 *   have a fixed size. The size limit of 128 is a balancing
 *   act - it needs to be large enough to support all 'sane'
 *   core options, but setting it too large may impact low memory
 *   platforms. In practise, if a core option has more than
 *   128 values then the implementation is likely flawed.
 *   To quote the above API reference:
 *      "The number of possible options should be very limited
 *       i.e. it should be feasible to cycle through options
 *       without a keyboard."
 */
#define RETRO_NUM_CORE_OPTION_VALUES_MAX 128

struct retro_core_option_value
{
   /* Expected option value */
   const char *value;

   /* Human-readable value label. If NULL, value itself
    * will be displayed by the frontend */
   const char *label;
};

struct retro_core_option_definition
{
   /* Variable to query in RETRO_ENVIRONMENT_GET_VARIABLE. */
   const char *key;

   /* Human-readable core option description (used as menu label) */
   const char *desc;

   /* Human-readable core option information (used as menu sublabel) */
   const char *info;

   /* Array of retro_core_option_value structs, terminated by NULL */
   struct retro_core_option_value values[RETRO_NUM_CORE_OPTION_VALUES_MAX];

   /* Default core option value. Must match one of the values
    * in the retro_core_option_value array, otherwise will be
    * ignored */
   const char *default_value;
};

#ifdef __PS3__
#undef local
#endif

struct retro_core_options_intl
{
   /* Pointer to an array of retro_core_option_definition structs
    * - US English implementation
    * - Must point to a valid array */
   struct retro_core_option_definition *us;

   /* Pointer to an array of retro_core_option_definition structs
    * - Implementation for current frontend language
    * - May be NULL */
   struct retro_core_option_definition *local;
};

struct retro_core_option_v2_category
{
   /* Variable uniquely identifying the
    * option category. Valid key characters
    * are [a-z, A-Z, 0-9, _, -] */
   const char *key;

   /* Human-readable category description
    * > Used as category menu label when
    *   frontend has core option category
    *   support */
   const char *desc;

   /* Human-readable category information
    * > Used as category menu sublabel when
    *   frontend has core option category
    *   support
    * > Optional (may be NULL or an empty
    *   string) */
   const char *info;
};

struct retro_core_option_v2_definition
{
   /* Variable to query in RETRO_ENVIRONMENT_GET_VARIABLE.
    * Valid key characters are [a-z, A-Z, 0-9, _, -] */
   const char *key;

   /* Human-readable core option description
    * > Used as menu label when frontend does
    *   not have core option category support
    *   e.g. "Video > Aspect Ratio" */
   const char *desc;

   /* Human-readable core option description
    * > Used as menu label when frontend has
    *   core option category support
    *   e.g. "Aspect Ratio", where associated
    *   retro_core_option_v2_category::desc
    *   is "Video"
    * > If empty or NULL, the string specified by
    *   desc will be used as the menu label
    * > Will be ignored (and may be set to NULL)
    *   if category_key is empty or NULL */
   const char *desc_categorized;

   /* Human-readable core option information
    * > Used as menu sublabel */
   const char *info;

   /* Human-readable core option information
    * > Used as menu sublabel when frontend
    *   has core option category support
    *   (e.g. may be required when info text
    *   references an option by name/desc,
    *   and the desc/desc_categorized text
    *   for that option differ)
    * > If empty or NULL, the string specified by
    *   info will be used as the menu sublabel
    * > Will be ignored (and may be set to NULL)
    *   if category_key is empty or NULL */
   const char *info_categorized;

   /* Variable specifying category (e.g. "video",
    * "audio") that will be assigned to the option
    * if frontend has core option category support.
    * > Categorized options will be displayed in a
    *   subsection/submenu of the frontend core
    *   option interface
    * > Specified string must match one of the
    *   retro_core_option_v2_category::key values
    *   in the associated retro_core_option_v2_category
    *   array; If no match is not found, specified
    *   string will be considered as NULL
    * > If specified string is empty or NULL, option will
    *   have no category and will be shown at the top
    *   level of the frontend core option interface */
   const char *category_key;

   /* Array of retro_core_option_value structs, terminated by NULL */
   struct retro_core_option_value values[RETRO_NUM_CORE_OPTION_VALUES_MAX];

   /* Default core option value. Must match one of the values
    * in the retro_core_option_value array, otherwise will be
    * ignored */
   const char *default_value;
};

struct retro_core_options_v2
{
   /* Array of retro_core_option_v2_category structs,
    * terminated by NULL
    * > If NULL, all entries in definitions array
    *   will have no category and will be shown at
    *   the top level of the frontend core option
    *   interface
    * > Will be ignored if frontend does not have
    *   core option category support */
   struct retro_core_option_v2_category *categories;

   /* Array of retro_core_option_v2_definition structs,
    * terminated by NULL */
   struct retro_core_option_v2_definition *definitions;
};

struct retro_core_options_v2_intl
{
   /* Pointer to a retro_core_options_v2 struct
    * > US English implementation
    * > Must point to a valid struct */
   struct retro_core_options_v2 *us;

   /* Pointer to a retro_core_options_v2 struct
    * - Implementation for current frontend language
    * - May be NULL */
   struct retro_core_options_v2 *local;
};

/* Used by the frontend to monitor changes in core option
 * visibility. May be called each time any core option
 * value is set via the frontend.
 * - On each invocation, the core must update the visibility
 *   of any dynamically hidden options using the
 *   RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY environment
 *   callback.
 * - On the first invocation, returns 'true' if the visibility
 *   of any core option has changed since the last call of
 *   retro_load_game() or retro_load_game_special().
 * - On each subsequent invocation, returns 'true' if the
 *   visibility of any core option has changed since the last
 *   time the function was called. */
typedef bool (RETRO_CALLCONV *retro_core_options_update_display_callback_t)(void);
struct retro_core_options_update_display_callback
{
   retro_core_options_update_display_callback_t callback;
};

struct retro_game_info
{
   const char *path;       /* Path to game, UTF-8 encoded.
                            * Sometimes used as a reference for building other paths.
                            * May be NULL if game was loaded from stdin or similar,
                            * but in this case some cores will be unable to load `data`.
                            * So, it is preferable to fabricate something here instead
                            * of passing NULL, which will help more cores to succeed.
                            * retro_system_info::need_fullpath requires
                            * that this path is valid. */
   const void *data;       /* Memory buffer of loaded game. Will be NULL
                            * if need_fullpath was set. */
   size_t      size;       /* Size of memory buffer. */
   const char *meta;       /* String of implementation specific meta-data. */
};

#define RETRO_MEMORY_ACCESS_WRITE (1 << 0)
   /* The core will write to the buffer provided by retro_framebuffer::data. */
#define RETRO_MEMORY_ACCESS_READ (1 << 1)
   /* The core will read from retro_framebuffer::data. */
#define RETRO_MEMORY_TYPE_CACHED (1 << 0)
   /* The memory in data is cached.
    * If not cached, random writes and/or reading from the buffer is expected to be very slow. */
struct retro_framebuffer
{
   void *data;                      /* The framebuffer which the core can render into.
                                       Set by frontend in GET_CURRENT_SOFTWARE_FRAMEBUFFER.
                                       The initial contents of data are unspecified. */
   unsigned width;                  /* The framebuffer width used by the core. Set by core. */
   unsigned height;                 /* The framebuffer height used by the core. Set by core. */
   size_t pitch;                    /* The number of bytes between the beginning of a scanline,
                                       and beginning of the next scanline.
                                       Set by frontend in GET_CURRENT_SOFTWARE_FRAMEBUFFER. */
   enum retro_pixel_format format;  /* The pixel format the core must use to render into data.
                                       This format could differ from the format used in
                                       SET_PIXEL_FORMAT.
                                       Set by frontend in GET_CURRENT_SOFTWARE_FRAMEBUFFER. */

   unsigned access_flags;           /* How the core will access the memory in the framebuffer.
                                       RETRO_MEMORY_ACCESS_* flags.
                                       Set by core. */
   unsigned memory_flags;           /* Flags telling core how the memory has been mapped.
                                       RETRO_MEMORY_TYPE_* flags.
                                       Set by frontend in GET_CURRENT_SOFTWARE_FRAMEBUFFER. */
};

/* Used by a libretro core to override the current
 * fastforwarding mode of the frontend */
struct retro_fastforwarding_override
{
   /* Specifies the runtime speed multiplier that
    * will be applied when 'fastforward' is true.
    * For example, a value of 5.0 when running 60 FPS
    * content will cap the fast-forward rate at 300 FPS.
    * Note that the target multiplier may not be achieved
    * if the host hardware has insufficient processing
    * power.
    * Setting a value of 0.0 (or greater than 0.0 but
    * less than 1.0) will result in an uncapped
    * fast-forward rate (limited only by hardware
    * capacity).
    * If the value is negative, it will be ignored
    * (i.e. the frontend will use a runtime speed
    * multiplier of its own choosing) */
   float ratio;

   /* If true, fastforwarding mode will be enabled.
    * If false, fastforwarding mode will be disabled. */
   bool fastforward;

   /* If true, and if supported by the frontend, an
    * on-screen notification will be displayed while
    * 'fastforward' is true.
    * If false, and if supported by the frontend, any
    * on-screen fast-forward notifications will be
    * suppressed */
   bool notification;

   /* If true, the core will have sole control over
    * when fastforwarding mode is enabled/disabled;
    * the frontend will not be able to change the
    * state set by 'fastforward' until either
    * 'inhibit_toggle' is set to false, or the core
    * is unloaded */
   bool inhibit_toggle;
};

/**
 * During normal operation.
 *
 * @note Rate will be equal to the core's internal FPS.
 */
#define RETRO_THROTTLE_NONE              0

/**
 * While paused or stepping single frames.
 *
 * @note Rate will be 0.
 */
#define RETRO_THROTTLE_FRAME_STEPPING    1

/**
 * During fast forwarding.
 *
 * @note Rate will be 0 if not specifically limited to a maximum speed.
 */
#define RETRO_THROTTLE_FAST_FORWARD      2

/**
 * During slow motion.
 *
 * @note Rate will be less than the core's internal FPS.
 */
#define RETRO_THROTTLE_SLOW_MOTION       3

/**
 * While rewinding recorded save states.
 *
 * @note Rate can vary depending on the rewind speed or be 0 if the frontend
 * is not aiming for a specific rate.
 */
#define RETRO_THROTTLE_REWINDING         4

/**
 * While vsync is active in the video driver, and the target refresh rate is lower than the core's internal FPS.
 *
 * @note Rate is the target refresh rate.
 */
#define RETRO_THROTTLE_VSYNC             5

/**
 * When the frontend does not throttle in any way.
 *
 * @note Rate will be 0. An example could be if no vsync or audio output is active.
 */
#define RETRO_THROTTLE_UNBLOCKED         6

/**
 * Details about the actual rate an implementation is calling \c retro_run() at.
 *
 * @see RETRO_ENVIRONMENT_GET_THROTTLE_STATE
 */
struct retro_throttle_state
{
   /**
    * The current throttling mode.
    *
    * @note Should be one of the \c RETRO_THROTTLE_* values.
    * @see RETRO_THROTTLE_NONE
    * @see RETRO_THROTTLE_FRAME_STEPPING
    * @see RETRO_THROTTLE_FAST_FORWARD
    * @see RETRO_THROTTLE_SLOW_MOTION
    * @see RETRO_THROTTLE_REWINDING
    * @see RETRO_THROTTLE_VSYNC
    * @see RETRO_THROTTLE_UNBLOCKED
    */
   unsigned mode;

   /**
    * How many times per second the frontend aims to call retro_run.
    *
    * @note Depending on the mode, it can be 0 if there is no known fixed rate.
    * This won't be accurate if the total processing time of the core and
    * the frontend is longer than what is available for one frame.
    */
   float rate;
};

/**
 * Opaque handle to a microphone that's been opened for use.
 * The underlying object is accessed or created with \c retro_microphone_interface_t.
 */
typedef struct retro_microphone retro_microphone_t;

/**
 * Parameters for configuring a microphone.
 * Some of these might not be honored,
 * depending on the available hardware and driver configuration.
 */
typedef struct retro_microphone_params
{
   /**
    * The desired sample rate of the microphone's input, in Hz.
    * The microphone's input will be resampled,
    * so cores can ask for whichever frequency they need.
    *
    * If zero, some reasonable default will be provided by the frontend
    * (usually from its config file).
    *
    * @see retro_get_mic_rate_t
    */
   unsigned rate;
} retro_microphone_params_t;

/**
 * @copydoc retro_microphone_interface::open_mic
 */
typedef retro_microphone_t *(RETRO_CALLCONV *retro_open_mic_t)(const retro_microphone_params_t *params);

/**
 * @copydoc retro_microphone_interface::close_mic
 */
typedef void (RETRO_CALLCONV *retro_close_mic_t)(retro_microphone_t *microphone);

/**
 * @copydoc retro_microphone_interface::get_params
 */
typedef bool (RETRO_CALLCONV *retro_get_mic_params_t)(const retro_microphone_t *microphone, retro_microphone_params_t *params);

/**
 * @copydoc retro_microphone_interface::set_mic_state
 */
typedef bool (RETRO_CALLCONV *retro_set_mic_state_t)(retro_microphone_t *microphone, bool state);

/**
 * @copydoc retro_microphone_interface::get_mic_state
 */
typedef bool (RETRO_CALLCONV *retro_get_mic_state_t)(const retro_microphone_t *microphone);

/**
 * @copydoc retro_microphone_interface::read_mic
 */
typedef int (RETRO_CALLCONV *retro_read_mic_t)(retro_microphone_t *microphone, int16_t* samples, size_t num_samples);

/**
 * The current version of the microphone interface.
 * Will be incremented whenever \c retro_microphone_interface or \c retro_microphone_params_t
 * receive new fields.
 *
 * Frontends using cores built against older mic interface versions
 * should not access fields introduced in newer versions.
 */
#define RETRO_MICROPHONE_INTERFACE_VERSION 1

/**
 * An interface for querying the microphone and accessing data read from it.
 *
 * @see RETRO_ENVIRONMENT_GET_MICROPHONE_INTERFACE
 */
struct retro_microphone_interface
{
   /**
    * The version of this microphone interface.
    * Set by the core to request a particular version,
    * and set by the frontend to indicate the returned version.
    * 0 indicates that the interface is invalid or uninitialized.
    */
   unsigned interface_version;

   /**
    * Initializes a new microphone.
    * Assuming that microphone support is enabled and provided by the frontend,
    * cores may call this function whenever necessary.
    * A microphone could be opened throughout a core's lifetime,
    * or it could wait until a microphone is plugged in to the emulated device.
    *
    * The returned handle will be valid until it's freed,
    * even if the audio driver is reinitialized.
    *
    * This function is not guaranteed to be thread-safe.
    *
    * @param args[in] Parameters used to create the microphone.
    * May be \c NULL, in which case the default value of each parameter will be used.
    *
    * @returns Pointer to the newly-opened microphone,
    * or \c NULL if one couldn't be opened.
    * This likely means that no microphone is plugged in and recognized,
    * or the maximum number of supported microphones has been reached.
    *
    * @note Microphones are \em inactive by default;
    * to begin capturing audio, call \c set_mic_state.
    * @see retro_microphone_params_t
    */
   retro_open_mic_t open_mic;

   /**
    * Closes a microphone that was initialized with \c open_mic.
    * Calling this function will stop all microphone activity
    * and free up the resources that it allocated.
    * Afterwards, the handle is invalid and must not be used.
    *
    * A frontend may close opened microphones when unloading content,
    * but this behavior is not guaranteed.
    * Cores should close their microphones when exiting, just to be safe.
    *
    * @param microphone Pointer to the microphone that was allocated by \c open_mic.
    * If \c NULL, this function does nothing.
    *
    * @note The handle might be reused if another microphone is opened later.
    */
   retro_close_mic_t close_mic;

   /**
    * Returns the configured parameters of this microphone.
    * These may differ from what was requested depending on
    * the driver and device configuration.
    *
    * Cores should check these values before they start fetching samples.
    *
    * Will not change after the mic was opened.
    *
    * @param microphone[in] Opaque handle to the microphone
    * whose parameters will be retrieved.
    * @param params[out] The parameters object that the
    * microphone's parameters will be copied to.
    *
    * @return \c true if the parameters were retrieved,
    * \c false if there was an error.
    */
   retro_get_mic_params_t get_params;

   /**
    * Enables or disables the given microphone.
    * Microphones are disabled by default
    * and must be explicitly enabled before they can be used.
    * Disabled microphones will not process incoming audio samples,
    * and will therefore have minimal impact on overall performance.
    * Cores may enable microphones throughout their lifetime,
    * or only for periods where they're needed.
    *
    * Cores that accept microphone input should be able to operate without it;
    * we suggest substituting silence in this case.
    *
    * @param microphone Opaque handle to the microphone
    * whose state will be adjusted.
    * This will have been provided by \c open_mic.
    * @param state \c true if the microphone should receive audio input,
    * \c false if it should be idle.
    * @returns \c true if the microphone's state was successfully set,
    * \c false if \c microphone is invalid
    * or if there was an error.
    */
   retro_set_mic_state_t set_mic_state;

   /**
    * Queries the active state of a microphone at the given index.
    * Will return whether the microphone is enabled,
    * even if the driver is paused.
    *
    * @param microphone Opaque handle to the microphone
    * whose state will be queried.
    * @return \c true if the provided \c microphone is valid and active,
    * \c false if not or if there was an error.
    */
   retro_get_mic_state_t get_mic_state;

   /**
    * Retrieves the input processed by the microphone since the last call.
    * \em Must be called every frame unless \c microphone is disabled,
    * similar to how \c retro_audio_sample_batch_t works.
    *
    * @param[in] microphone Opaque handle to the microphone
    * whose recent input will be retrieved.
    * @param[out] samples The buffer that will be used to store the microphone's data.
    * Microphone input is in mono (i.e. one number per sample).
    * Should be large enough to accommodate the expected number of samples per frame;
    * for example, a 44.1kHz sample rate at 60 FPS would require space for 735 samples.
    * @param[in] num_samples The size of the data buffer in samples (\em not bytes).
    * Microphone input is in mono, so a "frame" and a "sample" are equivalent in length here.
    *
    * @return The number of samples that were copied into \c samples.
    * If \c microphone is pending driver initialization,
    * this function will copy silence of the requested length into \c samples.
    *
    * Will return -1 if the microphone is disabled,
    * the audio driver is paused,
    * or there was an error.
    */
   retro_read_mic_t read_mic;
};

/**
 * Describes how a device is being powered.
 * @see RETRO_ENVIRONMENT_GET_DEVICE_POWER
 */
enum retro_power_state
{
   /**
    * Indicates that the frontend cannot report its power state at this time,
    * most likely due to a lack of support.
    *
    * \c RETRO_ENVIRONMENT_GET_DEVICE_POWER will not return this value;
    * instead, the environment callback will return \c false.
    */
   RETRO_POWERSTATE_UNKNOWN = 0,

   /**
    * Indicates that the device is running on its battery.
    * Usually applies to portable devices such as handhelds, laptops, and smartphones.
    */
   RETRO_POWERSTATE_DISCHARGING,

   /**
    * Indicates that the device's battery is currently charging.
    */
   RETRO_POWERSTATE_CHARGING,

   /**
    * Indicates that the device is connected to a power source
    * and that its battery has finished charging.
    */
   RETRO_POWERSTATE_CHARGED,

   /**
    * Indicates that the device is connected to a power source
    * and that it does not have a battery.
    * This usually suggests a desktop computer or a non-portable game console.
    */
   RETRO_POWERSTATE_PLUGGED_IN
};

/**
 * Indicates that an estimate is not available for the battery level or time remaining,
 * even if the actual power state is known.
 */
#define RETRO_POWERSTATE_NO_ESTIMATE (-1)

/**
 * Describes the power state of the device running the frontend.
 * @see RETRO_ENVIRONMENT_GET_DEVICE_POWER
 */
struct retro_device_power
{
   /**
    * The current state of the frontend's power usage.
    */
   enum retro_power_state state;

   /**
    * A rough estimate of the amount of time remaining (in seconds)
    * before the device powers off.
    * This value depends on a variety of factors,
    * so it is not guaranteed to be accurate.
    *
    * Will be set to \c RETRO_POWERSTATE_NO_ESTIMATE if \c state does not equal \c RETRO_POWERSTATE_DISCHARGING.
    * May still be set to \c RETRO_POWERSTATE_NO_ESTIMATE if the frontend is unable to provide an estimate.
    */
   int seconds;

   /**
    * The approximate percentage of battery charge,
    * ranging from 0 to 100 (inclusive).
    * The device may power off before this reaches 0.
    *
    * The user might have configured their device
    * to stop charging before the battery is full,
    * so do not assume that this will be 100 in the \c RETRO_POWERSTATE_CHARGED state.
    */
   int8_t percent;
};

/**
 * @defgroup Callbacks
 * @{
 */

/**
 * Environment callback to give implementations a way of performing uncommon tasks.
 *
 * @note Extensible.
 *
 * @param cmd The command to run.
 * @param data A pointer to the data associated with the command.
 *
 * @return Varies by callback,
 * but will always return \c false if the command is not recognized.
 *
 * @see RETRO_ENVIRONMENT_SET_ROTATION
 * @see retro_set_environment()
 */
typedef bool (RETRO_CALLCONV *retro_environment_t)(unsigned cmd, void *data);

/**
 * Render a frame.
 *
 * @note For performance reasons, it is highly recommended to have a frame
 * that is packed in memory, i.e. pitch == width * byte_per_pixel.
 * Certain graphic APIs, such as OpenGL ES, do not like textures
 * that are not packed in memory.
 *
 * @param data A pointer to the frame buffer data with a pixel format of 15-bit \c 0RGB1555 native endian, unless changed with \c RETRO_ENVIRONMENT_SET_PIXEL_FORMAT.
 * @param width The width of the frame buffer, in pixels.
 * @param height The height frame buffer, in pixels.
 * @param pitch The width of the frame buffer, in bytes.
 *
 * @see retro_set_video_refresh()
 * @see RETRO_ENVIRONMENT_SET_PIXEL_FORMAT
 * @see retro_pixel_format
 */
typedef void (RETRO_CALLCONV *retro_video_refresh_t)(const void *data, unsigned width,
      unsigned height, size_t pitch);

/**
 * Renders a single audio frame. Should only be used if implementation generates a single sample at a time.
 *
 * @param left The left audio sample represented as a signed 16-bit native endian.
 * @param right The right audio sample represented as a signed 16-bit native endian.
 *
 * @see retro_set_audio_sample()
 * @see retro_set_audio_sample_batch()
 */
typedef void (RETRO_CALLCONV *retro_audio_sample_t)(int16_t left, int16_t right);

/**
 * Renders multiple audio frames in one go.
 *
 * @note Only one of the audio callbacks must ever be used.
 *
 * @param data A pointer to the audio sample data pairs to render.
 * @param frames The number of frames that are represented in the data. One frame
 *     is defined as a sample of left and right channels, interleaved.
 *     For example: <tt>int16_t buf[4] = { l, r, l, r };</tt> would be 2 frames.
 *
 * @return The number of samples that were processed.
 *
 * @see retro_set_audio_sample_batch()
 * @see retro_set_audio_sample()
 */
typedef size_t (RETRO_CALLCONV *retro_audio_sample_batch_t)(const int16_t *data,
      size_t frames);

/**
 * Polls input.
 *
 * @see retro_set_input_poll()
 */
typedef void (RETRO_CALLCONV *retro_input_poll_t)(void);

/**
 * Queries for input for player 'port'.
 *
 * @param port Which player 'port' to query.
 * @param device Which device to query for. Will be masked with \c RETRO_DEVICE_MASK.
 * @param index The input index to retrieve.
 * The exact semantics depend on the device type given in \c device.
 * @param id The ID of which value to query, like \c RETRO_DEVICE_ID_JOYPAD_B.
 * @returns Depends on the provided arguments,
 * but will return 0 if their values are unsupported
 * by the frontend or the backing physical device.
 * @note Specialization of devices such as \c RETRO_DEVICE_JOYPAD_MULTITAP that
 * have been set with \c retro_set_controller_port_device() will still use the
 * higher level \c RETRO_DEVICE_JOYPAD to request input.
 *
 * @see retro_set_input_state()
 * @see RETRO_DEVICE_NONE
 * @see RETRO_DEVICE_JOYPAD
 * @see RETRO_DEVICE_MOUSE
 * @see RETRO_DEVICE_KEYBOARD
 * @see RETRO_DEVICE_LIGHTGUN
 * @see RETRO_DEVICE_ANALOG
 * @see RETRO_DEVICE_POINTER
 */
typedef int16_t (RETRO_CALLCONV *retro_input_state_t)(unsigned port, unsigned device,
      unsigned index, unsigned id);

/**
 * Sets the environment callback.
 *
 * @param cb The function which is used when making environment calls.
 *
 * @note Guaranteed to be called before \c retro_init().
 *
 * @see RETRO_ENVIRONMENT
 */
RETRO_API void retro_set_environment(retro_environment_t cb);

/**
 * Sets the video refresh callback.
 *
 * @param cb The function which is used when rendering a frame.
 *
 * @note Guaranteed to have been called before the first call to \c retro_run() is made.
 */
RETRO_API void retro_set_video_refresh(retro_video_refresh_t cb);

/**
 * Sets the audio sample callback.
 *
 * @param cb The function which is used when rendering a single audio frame.
 *
 * @note Guaranteed to have been called before the first call to \c retro_run() is made.
 */
RETRO_API void retro_set_audio_sample(retro_audio_sample_t cb);

/**
 * Sets the audio sample batch callback.
 *
 * @param cb The function which is used when rendering multiple audio frames in one go.
 *
 * @note Guaranteed to have been called before the first call to \c retro_run() is made.
 */
RETRO_API void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb);

/**
 * Sets the input poll callback.
 *
 * @param cb The function which is used to poll the active input.
 *
 * @note Guaranteed to have been called before the first call to \c retro_run() is made.
 */
RETRO_API void retro_set_input_poll(retro_input_poll_t cb);

/**
 * Sets the input state callback.
 *
 * @param cb The function which is used to query the input state.
 *
 *@note Guaranteed to have been called before the first call to \c retro_run() is made.
 */
RETRO_API void retro_set_input_state(retro_input_state_t cb);

/**
 * @}
 */

/**
 * Called by the frontend when initializing a libretro core.
 *
 * @warning There are many possible "gotchas" with global state in dynamic libraries.
 * Here are some to keep in mind:
 * <ul>
 * <li>Do not assume that the core was loaded by the operating system
 * for the first time within this call.
 * It may have been statically linked or retained from a previous session.
 * Consequently, cores must not rely on global variables being initialized
 * to their default values before this function is called;
 * this also goes for object constructors in C++.
 * <li>Although C++ requires that constructors be called for global variables,
 * it does not require that their destructors be called
 * if stored within a dynamic library's global scope.
 * <li>If the core is statically linked to the frontend,
 * global variables may be initialized when the frontend itself is initially executed.
 * </ul>
 * @see retro_deinit
 */
RETRO_API void retro_init(void);

/**
 * Called by the frontend when deinitializing a libretro core.
 * The core must release all of its allocated resources before this function returns.
 *
 * @warning There are many possible "gotchas" with global state in dynamic libraries.
 * Here are some to keep in mind:
 * <ul>
 * <li>Do not assume that the operating system will unload the core after this function returns,
 * as the core may be linked statically or retained in memory.
 * Cores should use this function to clean up all allocated resources
 * and reset all global variables to their default states.
 * <li>Do not assume that this core won't be loaded again after this function returns.
 * It may be kept in memory by the frontend for later use,
 * or it may be statically linked.
 * Therefore, all global variables should be reset to their default states within this function.
 * <li>C++ does not require that destructors be called
 * for variables within a dynamic library's global scope.
 * Therefore, global objects that own dynamically-managed resources
 * (such as \c std::string or <tt>std::vector</tt>)
 * should be kept behind pointers that are explicitly deallocated within this function.
 * </ul>
 * @see retro_init
 */
RETRO_API void retro_deinit(void);

/**
 * Retrieves which version of the libretro API is being used.
 *
 * @note This is used to validate ABI compatibility when the API is revised.
 *
 * @return Must return \c RETRO_API_VERSION.
 *
 * @see RETRO_API_VERSION
 */
RETRO_API unsigned retro_api_version(void);

/**
 * Gets statically known system info.
 *
 * @note Can be called at any time, even before retro_init().
 *
 * @param info A pointer to a \c retro_system_info where the info is to be loaded into. This must be statically allocated.
 */
RETRO_API void retro_get_system_info(struct retro_system_info *info);

/**
 * Gets information about system audio/video timings and geometry.
 *
 * @note Can be called only after \c retro_load_game() has successfully completed.
 *
 * @note The implementation of this function might not initialize every variable
 * if needed. For example, \c geom.aspect_ratio might not be initialized if
 * the core doesn't desire a particular aspect ratio.
 *
 * @param info A pointer to a \c retro_system_av_info where the audio/video information should be loaded into.
 *
 * @see retro_system_av_info
 */
RETRO_API void retro_get_system_av_info(struct retro_system_av_info *info);

/**
 * Sets device to be used for player 'port'.
 *
 * By default, \c RETRO_DEVICE_JOYPAD is assumed to be plugged into all
 * available ports.
 *
 * @note Setting a particular device type is not a guarantee that libretro cores
 * will only poll input based on that particular device type. It is only a
 * hint to the libretro core when a core cannot automatically detect the
 * appropriate input device type on its own. It is also relevant when a
 * core can change its behavior depending on device type.
 *
 * @note As part of the core's implementation of retro_set_controller_port_device,
 * the core should call \c RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS to notify the
 * frontend if the descriptions for any controls have changed as a
 * result of changing the device type.
 *
 * @param port Which port to set the device for, usually indicates the player number.
 * @param device Which device the given port is using. By default, \c RETRO_DEVICE_JOYPAD is assumed for all ports.
 *
 * @see RETRO_DEVICE_NONE
 * @see RETRO_DEVICE_JOYPAD
 * @see RETRO_DEVICE_MOUSE
 * @see RETRO_DEVICE_KEYBOARD
 * @see RETRO_DEVICE_LIGHTGUN
 * @see RETRO_DEVICE_ANALOG
 * @see RETRO_DEVICE_POINTER
 */
RETRO_API void retro_set_controller_port_device(unsigned port, unsigned device);

/**
 * Resets the currently-loaded game.
 * Cores should treat this as a soft reset (i.e. an emulated reset button) if possible,
 * but hard resets are acceptable.
 */
RETRO_API void retro_reset(void);

/**
 * Runs the game for one video frame.
 *
 * During \c retro_run(), the \c retro_input_poll_t callback must be called at least once.
 *
 * @note If a frame is not rendered for reasons where a game "dropped" a frame,
 * this still counts as a frame, and \c retro_run() should explicitly dupe
 * a frame if \c RETRO_ENVIRONMENT_GET_CAN_DUPE returns true. In this case,
 * the video callback can take a NULL argument for data.
 *
 * @see retro_input_poll_t
 */
RETRO_API void retro_run(void);

/**
 * Returns the amount of data the implementation requires to serialize internal state (save states).
 *
 * @note Between calls to \c retro_load_game() and \c retro_unload_game(), the
 * returned size is never allowed to be larger than a previous returned
 * value, to ensure that the frontend can allocate a save state buffer once.
 *
 * @return The amount of data the implementation requires to serialize the internal state.
 *
 * @see retro_serialize()
 */
RETRO_API size_t retro_serialize_size(void);

/**
 * Serializes the internal state.
 *
 * @param data A pointer to where the serialized data should be saved to.
 * @param size The size of the memory.
 *
 * @return If failed, or size is lower than \c retro_serialize_size(), it
 * should return false. On success, it will return true.
 *
 * @see retro_serialize_size()
 * @see retro_unserialize()
 */
RETRO_API bool retro_serialize(void *data, size_t size);

/**
 * Unserialize the given state data, and load it into the internal state.
 *
 * @return Returns true if loading the state was successful, false otherwise.
 *
 * @see retro_serialize()
 */
RETRO_API bool retro_unserialize(const void *data, size_t size);

/**
 * Reset all the active cheats to their default disabled state.
 *
 * @see retro_cheat_set()
 */
RETRO_API void retro_cheat_reset(void);

/**
 * Enable or disable a cheat.
 *
 * @param index The index of the cheat to act upon.
 * @param enabled Whether to enable or disable the cheat.
 * @param code A string of the code used for the cheat.
 *
 * @see retro_cheat_reset()
 */
RETRO_API void retro_cheat_set(unsigned index, bool enabled, const char *code);

/**
 * Loads a game.
 *
 * @param game A pointer to a \c retro_game_info detailing information about the game to load.
 * May be \c NULL if the core is loaded without content.
 *
 * @return Will return true when the game was loaded successfully, or false otherwise.
 *
 * @see retro_game_info
 * @see RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME
 */
RETRO_API bool retro_load_game(const struct retro_game_info *game);

/**
 * Called when the frontend has loaded one or more "special" content files,
 * typically through subsystems.
 *
 * @note Only necessary for cores that support subsystems.
 * Others may return \c false or delegate to <tt>retro_load_game</tt>.
 *
 * @param game_type The type of game to load.
 * @param info A pointer to an array of \c retro_game_info objects
 * providing information about the loaded content.
 * @param num_info The number of \c retro_game_info objects passed into the info parameter.
 *
 * @return Will return \c true when loading is successful, false otherwise.
 *
 * @see RETRO_ENVIRONMENT_GET_GAME_INFO_EXT
 * @see RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO
 * @see retro_load_game()
 */
RETRO_API bool retro_load_game_special(
  unsigned game_type,
  const struct retro_game_info *info, size_t num_info
);

/**
 * Unloads the currently loaded game.
 *
 * @note This is called before \c retro_deinit(void).
 *
 * @see retro_load_game()
 * @see retro_deinit()
 */
RETRO_API void retro_unload_game(void);

/**
 * Gets the region of the actively loaded content as either \c RETRO_REGION_NTSC or \c RETRO_REGION_PAL.
 * @note This refers to the region of the content's intended television standard,
 * not necessarily the region of the content's origin.
 * For emulated consoles that don't use either standard
 * (e.g. handhelds or post-HD platforms),
 * the core should return \c RETRO_REGION_NTSC.
 * @return The region of the actively loaded content.
 *
 * @see RETRO_REGION_NTSC
 * @see RETRO_REGION_PAL
 */
RETRO_API unsigned retro_get_region(void);

/**
 * Get a region of memory.
 *
 * @param id The ID for the memory block that's desired to retrieve. Can be \c RETRO_MEMORY_SAVE_RAM, \c RETRO_MEMORY_RTC, \c RETRO_MEMORY_SYSTEM_RAM, or \c RETRO_MEMORY_VIDEO_RAM.
 *
 * @return A pointer to the desired region of memory, or NULL when not available.
 *
 * @see RETRO_MEMORY_SAVE_RAM
 * @see RETRO_MEMORY_RTC
 * @see RETRO_MEMORY_SYSTEM_RAM
 * @see RETRO_MEMORY_VIDEO_RAM
 */
RETRO_API void *retro_get_memory_data(unsigned id);

/**
 * Gets the size of the given region of memory.
 *
 * @param id The ID for the memory block to check the size of. Can be RETRO_MEMORY_SAVE_RAM, RETRO_MEMORY_RTC, RETRO_MEMORY_SYSTEM_RAM, or RETRO_MEMORY_VIDEO_RAM.
 *
 * @return The size of the region in memory, or 0 when not available.
 *
 * @see RETRO_MEMORY_SAVE_RAM
 * @see RETRO_MEMORY_RTC
 * @see RETRO_MEMORY_SYSTEM_RAM
 * @see RETRO_MEMORY_VIDEO_RAM
 */
RETRO_API size_t retro_get_memory_size(unsigned id);

#ifdef __cplusplus
}
#endif

#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#ifndef __INPUT_DRIVER__H
#define __INPUT_DRIVER__H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>

#include "input_types.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#include "input_defines.h"

#include "../msg_hash.h"
#include "include/hid_types.h"
#include "include/hid_driver.h"
#include "include/gamepad.h"

RETRO_BEGIN_DECLS

struct retro_keybind
{
   char     *joykey_label;
   char     *joyaxis_label;

   /* Joypad axis. Negative and positive axes
    * are embedded into this variable. */
   uint32_t joyaxis;

   /* Default joy axis binding value -
    * for resetting bind to default */
   uint32_t def_joyaxis;

   /* Used by input_{push,pop}_analog_dpad(). */
   uint32_t orig_joyaxis;

   enum msg_hash_enums enum_idx;
   enum retro_key key;

   uint16_t id;

   uint16_t mbutton; /* mouse button ID */

   /* Joypad key. Joypad POV (hats)
    * are embedded into this key as well. */
   uint16_t joykey;

   /* Default key binding value -
    * for resetting bind to default */
   uint16_t def_joykey;

   bool valid;
};

extern struct retro_keybind input_config_binds[MAX_USERS][RARCH_BIND_LIST_END];
extern struct retro_keybind input_autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];

struct rarch_joypad_info
{
   const struct retro_keybind *auto_binds;
   float axis_threshold;
   uint16_t joy_idx;
};

typedef struct
{
   unsigned name_index;
   uint16_t vid;
   uint16_t pid;
   char joypad_driver[32];
   char name[256];
   char display_name[256];
   char config_path[PATH_MAX_LENGTH]; /* Path to the RetroArch config file */
   char config_name[PATH_MAX_LENGTH]; /* Base name of the RetroArch config file */
   bool autoconfigured;
} input_device_info_t;

/**
 * input_driver:
 * Organizes the functions and data structures of each driver that are accessed
 * by other parts of the input code. The input_driver structs are the "interface"
 * between RetroArch and the input driver.
 * Every driver must establish an input_driver struct with pointers to its own 
 * implementations of these functions, and each of those input_driver structs is
 * declared below.
 */
struct input_driver
{
   /** 
    * init: Initializes input driver.
    * 
    * PARAMETERS
    * @joypad_driver: Name of the joypad driver associated with the input driver
    */
   void *(*init)(const char *joypad_driver);

  /**
    * poll: Called once every frame to poll input. This function pointer can be set 
    * to NULL if not supported by the input driver, for example if a joypad driver
    * is responsible for polling on a particular driver/platform.
    *
    * PARAMETERS
    * @data:   the input state struct
    */
   void (*poll)(void *data);

   /** 
    * input_state: Queries state for a specified control on a specified input port.
    * This function pointer can be set to NULL if not supported by the input driver,
    * for example if a joypad driver is responsible for quering state for a particular
    * driver/platform.
    *
    * PARAMETERS
    * @joypad_data:      The input state struct, defined by the input driver
    * @sec_joypad_data:  The input state struct of any secondary input device
    *                    (e.g. MFi controllers), defined by a secondary input driver.
    *                    May be NULL. Queried state to be returned is the logical 
    *                    OR of 'joypad_data' and 'sec_joypad_data'
    * @joypad_info:      Info struct for the controller to be queried, containing
    *                    physical (hardware) device index and autoconfig input mapping.
    * @retro_keybinds:   Data structure for control mappings for all libretro
    *                    input device abstractions
    * @keyboard_mapping_blocked: 
    *                    If true, disregard custom keyboard mapping
    * @port:             Which RetroArch port is being polled
    * @device:           Which libretro abstraction is being polled 
                         (RETRO_DEVICE_ID_RETROPAD, RETRO_DEVICE_ID_MOUSE, etc)
    * @index:            For controls that support more than one axis or can return 
    *                    multiple simultaneous inputs, such as an analog joystick
    *                    or touchpad.
    * @id:               Which control is being polled (eg RETRO_DEVICE_ID_JOYPAD_START)
    *
    * RETURNS
    * Digital controls:  Returns 1 for a pressed control and 0 otherwise.
    * Analog controls:   Retruns values in the range of a signed 16-bit integer,
    *                    [-0x8000, 0x7fff]
    */
   int16_t (*input_state)(void *data,
         const input_device_driver_t *joypad_data,
         const input_device_driver_t *sec_joypad_data,
         rarch_joypad_info_t *joypad_info,
         const struct retro_keybind **retro_keybinds,
         bool keyboard_mapping_blocked,
         unsigned port, unsigned device, unsigned index, unsigned id);

   /**
    * free: Frees the input struct.
    * 
    * PARAMETERS
    * @data:   The input state struct
    */
   void (*free)(void *data);

   /**
    * set_sensor_state: Sets the state related for sensors, such
    * as polling rate or enabling/disable the sensor entirely, etc.
    * This function pointer may be set to NULL if setting sensor
    * values is not supported.
    * 
    * PARAMETERS
    * @data:   The input state struct
    * @port:   Input port
    * @effect: Sensor action
    * @rate:   Sensor rate update
    * 
    * RETURNS
    * Boolean true if the operation is successful. 
   **/
   bool (*set_sensor_state)(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate);

   /**
    * get_sensor_input: Retrieves the sensor state associated with
    * the provided port and ID. This function pointer may be set to 
    * NULL if retreiving sensor state is not supported.
    * 
    * PARAMETERS
    * @data:   The input state struct
    * @port:   Input port
    * @id:     Sensor ID
    * 
    * RETURNS
    * The current state associated with the port and ID as a float.
   **/
   float (*get_sensor_input)(void *data, unsigned port, unsigned id);

   /**
    * get_capabilities: The means for an input driver to indicate to RetroArch
    *                   which libretro input abstractions the driver supports
    * PARAMETERS
    * @data:  The input state struct
    * 
    * RETURNS
    * a unit64_t composed via bitwise operators
    */
   uint64_t (*get_capabilities)(void *data);

   /**
    * ident: The human-readable name of the input driver
    */
   const char *ident;

   /**
    * grab_mouse: Grab or ungrab the mouse according to the value of `state`.
    * This function pointer can be set to NULL if the driver does not support
    * grabbing the mouse.
    * 
    * PARAMETERS
    * @data:  The input state struct
    * @state: True to grab the mouse, false to ungrab
    */
   void (*grab_mouse)(void *data, bool state);

   /**
    * grab_stdin: Check to see if the input driver has claimed stdin, and
    * therefore it is not available for other input. This function pointer
    * can be set to NULL if the driver does not support claiming stdin.
    * 
    * PARAMETERS
    * @data:  The input state struct
    * 
    * RETURNS
    * True if the input driver has claimed stdin. This function pointer 
    */
   bool (*grab_stdin)(void *data);
};

struct rarch_joypad_driver
{
   void *(*init)(void *data);
   bool (*query_pad)(unsigned);
   void (*destroy)(void);
   int16_t (*button)(unsigned, uint16_t);
   int16_t (*state)(rarch_joypad_info_t *joypad_info,
         const struct retro_keybind *binds, unsigned port);
   void (*get_buttons)(unsigned, input_bits_t *);
   int16_t (*axis)(unsigned, uint32_t);
   void (*poll)(void);
   bool (*set_rumble)(unsigned, enum retro_rumble_effect, uint16_t);
   const char *(*name)(unsigned);

   const char *ident;
};

#if defined(ANDROID)
#define DEFAULT_MAX_PADS 8
#define ANDROID_KEYBOARD_PORT DEFAULT_MAX_PADS
#elif defined(_3DS)
#define DEFAULT_MAX_PADS 1
#elif defined(SWITCH) || defined(HAVE_LIBNX)
#define DEFAULT_MAX_PADS 8
#elif defined(WIIU)
#ifdef WIIU_HID
#define DEFAULT_MAX_PADS 16
#else
#define DEFAULT_MAX_PADS 5
#endif
#elif defined(DJGPP)
#define DEFAULT_MAX_PADS 1
#define DOS_KEYBOARD_PORT DEFAULT_MAX_PADS
#elif defined(XENON)
#define DEFAULT_MAX_PADS 4
#elif defined(VITA) || defined(SN_TARGET_PSP2)
#define DEFAULT_MAX_PADS 4
#elif defined(PSP)
#define DEFAULT_MAX_PADS 1
#elif defined(PS2)
#define DEFAULT_MAX_PADS 8
#elif defined(GEKKO) || defined(HW_RVL)
#define DEFAULT_MAX_PADS 4
#elif defined(HAVE_ODROIDGO2)
#define DEFAULT_MAX_PADS 1
#elif defined(__linux__) || (defined(BSD) && !defined(__MACH__))
#define DEFAULT_MAX_PADS 8
#elif defined(__QNX__)
#define DEFAULT_MAX_PADS 8
#elif defined(__PS3__)
#define DEFAULT_MAX_PADS 7
#elif defined(_XBOX)
#define DEFAULT_MAX_PADS 4
#elif defined(HAVE_XINPUT) && !defined(HAVE_DINPUT)
#define DEFAULT_MAX_PADS 4
#elif defined(DINGUX)
#define DEFAULT_MAX_PADS 2
#else
#define DEFAULT_MAX_PADS 16
#endif

/**
 * config_get_input_driver_options:
 *
 * Get an enumerated list of all input driver names, separated by '|'.
 *
 * Returns: string listing of all input driver names, separated by '|'.
 **/
const char* config_get_input_driver_options(void);

/**
 * input_driver_set_rumble_state:
 * @port               : User number.
 * @effect             : Rumble effect.
 * @strength           : Strength of rumble effect.
 *
 * Sets the rumble state.
 * Used by RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE.
 **/
bool input_driver_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength);

/**
 * input_sensor_set_state:
 * @port               : User number.
 * @effect             : Sensor action.
 * @rate               : Sensor rate update.
 *
 * Sets the sensor state.
 * Used by RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE.
 **/
bool input_sensor_set_state(unsigned port,
      enum retro_sensor_action action, unsigned rate);

float input_sensor_get_input(unsigned port, unsigned id);

void *input_driver_get_data(void);

void input_driver_set_nonblock_state(void);

void input_driver_unset_nonblock_state(void);

float *input_driver_get_float(enum input_action action);

unsigned *input_driver_get_uint(enum input_action action);

/**
 * config_get_joypad_driver_options:
 *
 * Get an enumerated list of all joypad driver names, separated by '|'.
 *
 * Returns: string listing of all joypad driver names, separated by '|'.
 **/
const char* config_get_joypad_driver_options(void);

/**
 * input_joypad_init_driver:
 * @ident                           : identifier of driver to initialize.
 *
 * Initialize a joypad driver of name @ident.
 *
 * If ident points to NULL or a zero-length string,
 * equivalent to calling input_joypad_init_first().
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
const input_device_driver_t *input_joypad_init_driver(
      const char *ident, void *data);

/**
 * input_conv_analog_id_to_bind_id:
 * @idx                     : Analog key index.
 *                            E.g.:
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @ident_minus             : Bind ID minus, will be set by function.
 * @ident_plus              : Bind ID plus,  will be set by function.
 *
 * Takes as input analog key identifiers and converts
 * them to corresponding bind IDs @ident_minus and @ident_plus.
 **/
#define input_conv_analog_id_to_bind_id(idx, ident, ident_minus, ident_plus) \
   switch ((idx << 1) | ident) \
   { \
      case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_X: \
         ident_minus = RARCH_ANALOG_LEFT_X_MINUS; \
         ident_plus  = RARCH_ANALOG_LEFT_X_PLUS; \
         break; \
      case (RETRO_DEVICE_INDEX_ANALOG_LEFT << 1) | RETRO_DEVICE_ID_ANALOG_Y: \
         ident_minus = RARCH_ANALOG_LEFT_Y_MINUS; \
         ident_plus  = RARCH_ANALOG_LEFT_Y_PLUS; \
         break; \
      case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_X: \
         ident_minus = RARCH_ANALOG_RIGHT_X_MINUS; \
         ident_plus  = RARCH_ANALOG_RIGHT_X_PLUS; \
         break; \
      case (RETRO_DEVICE_INDEX_ANALOG_RIGHT << 1) | RETRO_DEVICE_ID_ANALOG_Y: \
         ident_minus = RARCH_ANALOG_RIGHT_Y_MINUS; \
         ident_plus  = RARCH_ANALOG_RIGHT_Y_PLUS; \
         break; \
   }

/**
 * input_pad_connect:
 * @port                    : Joystick number.
 * @driver                  : handle for joypad driver handling joystick's input
 *
 * Registers a newly connected pad with RetroArch.
 **/
void input_pad_connect(unsigned port, input_device_driver_t *driver);

#ifdef HAVE_HID
#include "include/hid_driver.h"

/**
 * config_get_hid_driver_options:
 *
 * Get an enumerated list of all HID driver names, separated by '|'.
 *
 * Returns: string listing of all HID driver names, separated by '|'.
 **/
const char* config_get_hid_driver_options(void);

/**
 * input_hid_init_first:
 *
 * Finds first suitable HID driver and initializes.
 *
 * Returns: HID driver if found, otherwise NULL.
 **/
const hid_driver_t *input_hid_init_first(void);

const void *hid_driver_get_data(void);
void hid_driver_reset_data(void);
#endif

/** Line complete callback.
 * Calls back after return is pressed with the completed line.
 * Line can be NULL.
 **/
typedef void (*input_keyboard_line_complete_t)(void *userdata,
      const char *line);

typedef bool (*input_keyboard_press_t)(void *userdata, unsigned code);

struct input_keyboard_ctx_wait
{
   void *userdata;
   input_keyboard_press_t cb;
};

/**
 * input_keyboard_event:
 * @down                     : Keycode was pressed down?
 * @code                     : Keycode.
 * @character                : Character inputted.
 * @mod                      : TODO/FIXME: ???
 *
 * Keyboard event utils. Called by drivers when keyboard events are fired.
 * This interfaces with the global driver struct and libretro callbacks.
 **/
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device);

const char *input_config_bind_map_get_base(unsigned i);

unsigned input_config_bind_map_get_meta(unsigned i);

const char *input_config_bind_map_get_desc(unsigned i);

uint8_t input_config_bind_map_get_retro_key(unsigned i);

/* auto_bind can be NULL. */
void input_config_get_bind_string(char *buf,
      const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t size);

/**
 * input_config_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates string representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str);

/**
 * input_config_translate_str_to_bind_id:
 * @str                            : String to translate to bind ID.
 *
 * Translate string representation to bind ID.
 *
 * Returns: Bind ID value on success, otherwise
 * RARCH_BIND_LIST_END on not found.
 **/
unsigned input_config_translate_str_to_bind_id(const char *str);

void config_read_keybinds_conf(void *data);

/* Note: 'data' is an object of type config_file_t
 * > We assume it was done like this to avoid including
 *   config_file.h... */
void input_config_set_autoconfig_binds(unsigned port, void *data);

/* Set input_device_info */
void input_config_set_device_name(unsigned port, const char *name);
void input_config_set_device_display_name(unsigned port, const char *name);
void input_config_set_device_config_path(unsigned port, const char *path);
void input_config_set_device_config_name(unsigned port, const char *name);
void input_config_set_device_joypad_driver(unsigned port, const char *driver);
void input_config_set_device_vid(unsigned port, uint16_t vid);
void input_config_set_device_pid(unsigned port, uint16_t pid);
void input_config_set_device_autoconfigured(unsigned port, bool autoconfigured);
void input_config_set_device_name_index(unsigned port, unsigned name_index);

/* Clear input_device_info */
void input_config_clear_device_name(unsigned port);
void input_config_clear_device_display_name(unsigned port);
void input_config_clear_device_config_path(unsigned port);
void input_config_clear_device_config_name(unsigned port);
void input_config_clear_device_joypad_driver(unsigned port);

unsigned input_config_get_device_count(void);

unsigned *input_config_get_device_ptr(unsigned port);

unsigned input_config_get_device(unsigned port);

void input_config_set_device(unsigned port, unsigned id);

/* Get input_device_info */
const char *input_config_get_device_name(unsigned port);
const char *input_config_get_device_display_name(unsigned port);
const char *input_config_get_device_config_path(unsigned port);
const char *input_config_get_device_config_name(unsigned port);
const char *input_config_get_device_joypad_driver(unsigned port);
uint16_t input_config_get_device_vid(unsigned port);
uint16_t input_config_get_device_pid(unsigned port);
bool input_config_get_device_autoconfigured(unsigned port);
unsigned input_config_get_device_name_index(unsigned port);

/* TODO/FIXME: This is required by linuxraw_joypad.c
 * and parport_joypad.c. These input drivers should
 * be refactored such that this dubious low-level
 * access is not required */
char *input_config_get_device_name_ptr(unsigned port);
size_t input_config_get_device_name_size(unsigned port);

const struct retro_keybind *input_config_get_bind_auto(unsigned port, unsigned id);

void input_config_save_keybinds_user(void *data, unsigned user);

void input_config_save_keybind(void *data, const char *prefix,
      const char *base, const struct retro_keybind *bind,
      bool save_empty);

void input_config_reset_autoconfig_binds(unsigned port);
void input_config_reset(void);

void set_connection_listener(pad_connection_listener_t *listener);

extern input_device_driver_t dinput_joypad;
extern input_device_driver_t linuxraw_joypad;
extern input_device_driver_t parport_joypad;
extern input_device_driver_t udev_joypad;
extern input_device_driver_t xinput_joypad;
extern input_device_driver_t sdl_joypad;
extern input_device_driver_t sdl_dingux_joypad;
extern input_device_driver_t ps4_joypad;
extern input_device_driver_t ps3_joypad;
extern input_device_driver_t psp_joypad;
extern input_device_driver_t ps2_joypad;
extern input_device_driver_t ctr_joypad;
extern input_device_driver_t switch_joypad;
extern input_device_driver_t xdk_joypad;
extern input_device_driver_t gx_joypad;
extern input_device_driver_t wiiu_joypad;
extern input_device_driver_t hid_joypad;
extern input_device_driver_t android_joypad;
extern input_device_driver_t qnx_joypad;
extern input_device_driver_t mfi_joypad;
extern input_device_driver_t dos_joypad;
extern input_device_driver_t rwebpad_joypad;

extern input_driver_t input_android;
extern input_driver_t input_sdl;
extern input_driver_t input_sdl_dingux;
extern input_driver_t input_dinput;
extern input_driver_t input_x;
extern input_driver_t input_ps4;
extern input_driver_t input_ps3;
extern input_driver_t input_psp;
extern input_driver_t input_ps2;
extern input_driver_t input_ctr;
extern input_driver_t input_switch;
extern input_driver_t input_xenon360;
extern input_driver_t input_gx;
extern input_driver_t input_wiiu;
extern input_driver_t input_xinput;
extern input_driver_t input_uwp;
extern input_driver_t input_linuxraw;
extern input_driver_t input_udev;
extern input_driver_t input_cocoa;
extern input_driver_t input_qnx;
extern input_driver_t input_rwebinput;
extern input_driver_t input_dos;
extern input_driver_t input_winraw;
extern input_driver_t input_wayland;

#ifdef HAVE_HID
extern hid_driver_t iohidmanager_hid;
extern hid_driver_t btstack_hid;
extern hid_driver_t libusb_hid;
extern hid_driver_t wiiusb_hid;
#endif

typedef struct menu_input_ctx_line
{
   const char *label;
   const char *label_setting;
   unsigned type;
   unsigned idx;
   input_keyboard_line_complete_t cb;
} menu_input_ctx_line_t;

const char *menu_input_dialog_get_label_setting_buffer(void);

const char *menu_input_dialog_get_label_buffer(void);

const char *menu_input_dialog_get_buffer(void);

unsigned menu_input_dialog_get_kb_idx(void);

bool menu_input_dialog_start_search(void);

bool menu_input_dialog_get_display_kb(void);

bool menu_input_dialog_start(menu_input_ctx_line_t *line);

void menu_input_dialog_end(void);

RETRO_END_DECLS

#endif

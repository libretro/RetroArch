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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <libretro.h>

#include "input_defines.h"

#include "../msg_hash.h"

RETRO_BEGIN_DECLS

typedef struct rarch_joypad_driver input_device_driver_t;

typedef struct hid_driver hid_driver_t;

/* Keyboard line reader. Handles textual input in a direct fashion. */
typedef struct input_keyboard_line input_keyboard_line_t;

enum input_device_type
{
   INPUT_DEVICE_TYPE_NONE = 0,
   INPUT_DEVICE_TYPE_KEYBOARD,
   INPUT_DEVICE_TYPE_JOYPAD
};

enum input_toggle_type
{
   INPUT_TOGGLE_NONE = 0,
   INPUT_TOGGLE_DOWN_Y_L_R,
   INPUT_TOGGLE_L3_R3,
   INPUT_TOGGLE_L1_R1_START_SELECT,
   INPUT_TOGGLE_START_SELECT,
   INPUT_TOGGLE_LAST
};

enum input_action
{
   INPUT_ACTION_NONE = 0,
   INPUT_ACTION_AXIS_THRESHOLD,
   INPUT_ACTION_MAX_USERS
};

enum rarch_input_keyboard_ctl_state
{
   RARCH_INPUT_KEYBOARD_CTL_NONE = 0,
   RARCH_INPUT_KEYBOARD_CTL_SET_LINEFEED_ENABLED,
   RARCH_INPUT_KEYBOARD_CTL_UNSET_LINEFEED_ENABLED,
   RARCH_INPUT_KEYBOARD_CTL_IS_LINEFEED_ENABLED,

   RARCH_INPUT_KEYBOARD_CTL_LINE_FREE,

   /*
    * Waits for keys to be pressed (used for binding 
    * keys in the menu).
    * Callback returns false when all polling is done.
    **/
   RARCH_INPUT_KEYBOARD_CTL_START_WAIT_KEYS,

   /* Cancels keyboard wait for keys function callback. */
   RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS
};

struct retro_keybind
{
   bool valid;
   uint16_t id;
   enum msg_hash_enums enum_idx;
   enum retro_key key;

   /* Joypad key. Joypad POV (hats) 
    * are embedded into this key as well. */
   uint64_t joykey;

   /* Default key binding value - 
    * for resetting bind to default */
   uint64_t def_joykey;

   /* Joypad axis. Negative and positive axes 
    * are embedded into this variable. */
   uint32_t joyaxis;

   /* Default joy axis binding value - 
    * for resetting bind to default */
   uint32_t def_joyaxis;

   /* Used by input_{push,pop}_analog_dpad(). */
   uint32_t orig_joyaxis;

   char     joykey_label[256];
   char     joyaxis_label[256];
};

typedef struct rarch_joypad_info
{
   uint16_t joy_idx;
   const struct retro_keybind *auto_binds;
   float axis_threshold;
} rarch_joypad_info_t;

typedef struct input_driver
{
   /* Inits input driver. 
    */
   void *(*init)(const char *joypad_driver);

   /* Polls input. Called once every frame. */
   void (*poll)(void *data);

   /* Queries input state for a certain key on a certain player.
    * Players are 1 - MAX_USERS.
    * For digital inputs, pressed key is 1, not pressed key is 0.
    * Analog values have same range as a signed 16-bit integer.
    */
   int16_t (*input_state)(void *data,
         rarch_joypad_info_t joypad_info,
         const struct retro_keybind **retro_keybinds,
         unsigned port, unsigned device, unsigned index, unsigned id);

   bool (*meta_key_pressed)(void *data, int key);

   /* Frees the input struct. */
   void (*free)(void *data);

   bool (*set_sensor_state)(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate);
   float (*get_sensor_input)(void *data, unsigned port, unsigned id);
   uint64_t (*get_capabilities)(void *data);
   const char *ident;

   void (*grab_mouse)(void *data, bool state);
   bool (*grab_stdin)(void *data);
   bool (*set_rumble)(void *data, unsigned port,
         enum retro_rumble_effect effect, uint16_t state);
   const input_device_driver_t *(*get_joypad_driver)(void *data);
   const input_device_driver_t *(*get_sec_joypad_driver)(void *data);
   bool (*keyboard_mapping_is_blocked)(void *data);
   void (*keyboard_mapping_set_block)(void *data, bool value);
} input_driver_t;

struct rarch_joypad_driver
{
   bool (*init)(void *data);
   bool (*query_pad)(unsigned);
   void (*destroy)(void);
   bool (*button)(unsigned, uint16_t);
   uint64_t (*get_buttons)(unsigned);
   int16_t (*axis)(unsigned, uint32_t);
   void (*poll)(void);
   bool (*set_rumble)(unsigned, enum retro_rumble_effect, uint16_t);
   const char *(*name)(unsigned);

   const char *ident;
};

struct hid_driver
{
   void *(*init)(void);
   bool (*query_pad)(void *, unsigned);
   void (*free)(void *);
   bool (*button)(void *, unsigned, uint16_t);
   uint64_t (*get_buttons)(void *, unsigned);
   int16_t (*axis)(void *, unsigned, uint32_t);
   void (*poll)(void *);
   bool (*set_rumble)(void *, unsigned, enum retro_rumble_effect, uint16_t);
   const char *(*name)(void *, unsigned);

   const char *ident;
};

extern const input_driver_t *current_input;
extern void *current_input_data;

/**
 * input_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to input driver at index. Can be NULL
 * if nothing found.
 **/
const void *input_driver_find_handle(int index);

/**
 * input_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of input driver at index. Can be NULL
 * if nothing found.
 **/
const char *input_driver_find_ident(int index);

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

uint64_t input_driver_get_capabilities(void);

const input_device_driver_t * input_driver_get_joypad_driver(void);

const input_device_driver_t * input_driver_get_sec_joypad_driver(void);

void input_driver_keyboard_mapping_set_block(bool value);

void input_driver_set(const input_driver_t **input, void **input_data);

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

#define inherit_joyaxis(binds) (((binds)[x_plus].joyaxis == (binds)[x_minus].joyaxis) || (  (binds)[y_plus].joyaxis == (binds)[y_minus].joyaxis))

/**
 * input_pop_analog_dpad:
 * @binds                          : Binds to modify.
 *
 * Restores binds temporarily overridden by input_push_analog_dpad().
 **/
#define input_pop_analog_dpad(binds) \
{ \
   unsigned j; \
   for (j = RETRO_DEVICE_ID_JOYPAD_UP; j <= RETRO_DEVICE_ID_JOYPAD_RIGHT; j++) \
      (binds)[j].joyaxis = (binds)[j].orig_joyaxis; \
}

/**
 * input_push_analog_dpad:
 * @binds                          : Binds to modify.
 * @mode                           : Which analog stick to bind D-Pad to.
 *                                   E.g:
 *                                   ANALOG_DPAD_LSTICK
 *                                   ANALOG_DPAD_RSTICK
 *
 * Push analog to D-Pad mappings to binds.
 **/
#define input_push_analog_dpad(binds, mode) \
{ \
   unsigned k; \
   unsigned x_plus      =  RARCH_ANALOG_RIGHT_X_PLUS; \
   unsigned y_plus      =  RARCH_ANALOG_RIGHT_Y_PLUS; \
   unsigned x_minus     =  RARCH_ANALOG_RIGHT_X_MINUS; \
   unsigned y_minus     =  RARCH_ANALOG_RIGHT_Y_MINUS; \
   if ((mode) == ANALOG_DPAD_LSTICK) \
   { \
      x_plus            =  RARCH_ANALOG_LEFT_X_PLUS; \
      y_plus            =  RARCH_ANALOG_LEFT_Y_PLUS; \
      x_minus           =  RARCH_ANALOG_LEFT_X_MINUS; \
      y_minus           =  RARCH_ANALOG_LEFT_Y_MINUS; \
   } \
   for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++) \
      (binds)[k].orig_joyaxis = (binds)[k].joyaxis; \
   if (!inherit_joyaxis(binds)) \
   { \
      unsigned j = x_plus + 3; \
      /* Inherit joyaxis from analogs. */ \
      for (k = RETRO_DEVICE_ID_JOYPAD_UP; k <= RETRO_DEVICE_ID_JOYPAD_RIGHT; k++) \
         (binds)[k].joyaxis = (binds)[j--].joyaxis; \
   } \
}

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
void input_poll(void);

/**
 * input_state:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id) was pressed by the user
 * (assigned to @port).
 **/
int16_t input_state(unsigned port, unsigned device,
      unsigned idx, unsigned id);

uint64_t input_keys_pressed(void *data, uint64_t last_input);

#ifdef HAVE_MENU
uint64_t input_menu_keys_pressed(void *data, uint64_t last_input);
#endif

void *input_driver_get_data(void);

const input_driver_t *input_get_ptr(void);

const input_driver_t **input_get_double_ptr(void);

void **input_driver_get_data_ptr(void);

bool input_driver_has_capabilities(void);

void input_driver_poll(void);

bool input_driver_init(void);

void input_driver_deinit(void);

void input_driver_destroy_data(void);

void input_driver_destroy(void);

bool input_driver_grab_stdin(void);

bool input_driver_keyboard_mapping_is_blocked(void);

bool input_driver_find_driver(void);

void input_driver_set_flushing_input(void);

void input_driver_unset_hotkey_block(void);

void input_driver_set_hotkey_block(void);

void input_driver_set_libretro_input_blocked(void);

void input_driver_unset_libretro_input_blocked(void);

bool input_driver_is_libretro_input_blocked(void);

void input_driver_set_nonblock_state(void);

void input_driver_unset_nonblock_state(void);

bool input_driver_is_nonblock_state(void);

void input_driver_set_own_driver(void);

void input_driver_unset_own_driver(void);

bool input_driver_owns_driver(void);

void input_driver_deinit_command(void);

bool input_driver_init_command(void);

void input_driver_deinit_remote(void);

bool input_driver_init_remote(void);

bool input_driver_grab_mouse(void);

bool input_driver_ungrab_mouse(void);

float *input_driver_get_float(enum input_action action);

unsigned *input_driver_get_uint(enum input_action action);

bool input_driver_is_data_ptr_same(void *data);

/**
 * joypad_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to joypad driver at index. Can be NULL
 * if nothing found.
 **/
const void *joypad_driver_find_handle(int index);

/**
 * joypad_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of joypad driver at index. Can be NULL
 * if nothing found.
 **/
const char *joypad_driver_find_ident(int index);

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
const input_device_driver_t *input_joypad_init_driver(const char *ident, void *data);

/**
 * input_joypad_init_first:
 *
 * Finds first suitable joypad driver and initializes.
 *
 * Returns: joypad driver if found, otherwise NULL.
 **/
const input_device_driver_t *input_joypad_init_first(void *data);

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
void input_conv_analog_id_to_bind_id(unsigned idx, unsigned ident,
      unsigned *ident_minus, unsigned *ident_plus);

/**
 * input_joypad_pressed:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @binds                   : Binds of user.
 * @key                     : Identifier of key.
 *
 * Checks if key (@key) was being pressed by user
 * with number @port with provided keybinds (@binds).
 *
 * Returns: true (1) if key was pressed, otherwise
 * false (0).
 **/
static INLINE bool input_joypad_pressed(
      const input_device_driver_t *drv,
      rarch_joypad_info_t joypad_info,
      unsigned port,
      const struct retro_keybind *binds,
      unsigned key)
{
   /* Auto-binds are per joypad, not per user. */
   uint64_t                        joykey = (binds[key].joykey != NO_BTN)
      ? binds[key].joykey : joypad_info.auto_binds[key].joykey;
   uint32_t                       joyaxis = (binds[key].joyaxis != AXIS_NONE) 
      ? binds[key].joyaxis : joypad_info.auto_binds[key].joyaxis;

   if ((uint16_t)joykey != NO_BTN && drv->button(joypad_info.joy_idx, (uint16_t)joykey))
      return true;

   return ((float)abs(drv->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold;

}

/**
 * input_joypad_analog:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @idx                     : Analog key index.
 *                            E.g.: 
 *                            - RETRO_DEVICE_INDEX_ANALOG_LEFT
 *                            - RETRO_DEVICE_INDEX_ANALOG_RIGHT
 * @ident                   : Analog key identifier.
 *                            E.g.:
 *                            - RETRO_DEVICE_ID_ANALOG_X
 *                            - RETRO_DEVICE_ID_ANALOG_Y
 * @binds                   : Binds of user.
 *
 * Gets analog value of analog key identifiers @idx and @ident
 * from user with number @port with provided keybinds (@binds).
 *
 * Returns: analog value on success, otherwise 0.
 **/
int16_t input_joypad_analog(const input_device_driver_t *driver,
      rarch_joypad_info_t joypad_info,
      unsigned port, unsigned idx, unsigned ident,
      const struct retro_keybind *binds);

/**
 * input_joypad_set_rumble:
 * @drv                     : Input device driver handle.
 * @port                    : User number.
 * @effect                  : Rumble effect to set.
 * @strength                : Strength of rumble effect.
 *
 * Sets rumble effect @effect with strength @strength.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_joypad_set_rumble(const input_device_driver_t *driver,
      unsigned port, enum retro_rumble_effect effect, uint16_t strength);

/**
 * input_joypad_axis_raw:  
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 * @axis                    : Identifier of axis.
 *
 * Checks if axis (@axis) was being pressed by user   
 * with joystick number @port.
 *
 * Returns: true (1) if axis was pressed, otherwise
 * false (0).
 **/
int16_t input_joypad_axis_raw(const input_device_driver_t *driver,
      unsigned port, unsigned axis);

/**
 * input_joypad_button_raw:
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 * @button                  : Identifier of key.
 *
 * Checks if key (@button) was being pressed by user
 * with joystick number @port.
 *
 * Returns: true (1) if key was pressed, otherwise
 * false (0).
 **/
bool input_joypad_button_raw(const input_device_driver_t *driver,
      unsigned port, unsigned button);

bool input_joypad_hat_raw(const input_device_driver_t *driver,
      unsigned joypad, unsigned hat_dir, unsigned hat);

/**
 * input_joypad_name:  
 * @drv                     : Input device driver handle.
 * @port                    : Joystick number.
 *
 * Gets name of the joystick (@port).
 *
 * Returns: name of joystick #port.
 **/
const char *input_joypad_name(const input_device_driver_t *driver,
      unsigned port);

bool input_config_get_bind_idx(unsigned port, unsigned *joy_idx_real);

#ifdef HAVE_HID
/**
 * hid_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to HID driver at index. Can be NULL
 * if nothing found.
 **/
const void *hid_driver_find_handle(int index);

/**
 * hid_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of HID driver at index. Can be NULL
 * if nothing found.
 **/
const char *hid_driver_find_ident(int index);

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
#endif

/** Line complete callback. 
 * Calls back after return is pressed with the completed line.
 * Line can be NULL.
 **/
typedef void (*input_keyboard_line_complete_t)(void *userdata,
      const char *line);

typedef bool (*input_keyboard_press_t)(void *userdata, unsigned code);

typedef struct input_keyboard_ctx_wait
{
   void *userdata;
   input_keyboard_press_t cb;
} input_keyboard_ctx_wait_t;

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

bool input_keyboard_line_append(const char *word);

/**
 * input_keyboard_start_line:
 * @userdata                 : Userdata.
 * @cb                       : Line complete callback function.
 *
 * Sets function pointer for keyboard line handle.
 *
 * The underlying buffer can be reallocated at any time 
 * (or be NULL), but the pointer to it remains constant 
 * throughout the objects lifetime.
 *
 * Returns: underlying buffer of the keyboard line.
 **/
const char **input_keyboard_start_line(void *userdata,
      input_keyboard_line_complete_t cb);


bool input_keyboard_ctl(enum rarch_input_keyboard_ctl_state state, void *data);

extern struct retro_keybind input_config_binds[MAX_USERS][RARCH_BIND_LIST_END];
extern struct retro_keybind input_autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];
extern const struct retro_keybind *libretro_input_binds[MAX_USERS];
extern char input_device_names[MAX_USERS][64];

const char *input_config_bind_map_get_base(unsigned i);

unsigned input_config_bind_map_get_meta(unsigned i);

const char *input_config_bind_map_get_desc(unsigned i);

bool input_config_bind_map_get_valid(unsigned i);

/* auto_bind can be NULL. */
void input_config_get_bind_string(char *buf,
      const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t size);

/**
 * input_config_translate_str_to_rk:
 * @str                            : String to translate to key ID.
 *
 * Translates tring representation to key identifier.
 *
 * Returns: key identifier.
 **/
enum retro_key input_config_translate_str_to_rk(const char *str);

const char *input_config_get_prefix(unsigned user, bool meta);

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

void input_config_parse_key(void *data,
      const char *prefix, const char *btn,
      struct retro_keybind *bind);

void input_config_parse_joy_button(void *data, const char *prefix,
      const char *btn, struct retro_keybind *bind);

void input_config_parse_joy_axis(void *data, const char *prefix,
      const char *axis, struct retro_keybind *bind);

void input_config_set_device_name(unsigned port, const char *name);

void input_config_clear_device_name(unsigned port);

unsigned *input_config_get_device_ptr(unsigned port);

unsigned input_config_get_device(unsigned port);

void input_config_set_device(unsigned port, unsigned id);

const char *input_config_get_device_name(unsigned port);

const struct retro_keybind *input_config_get_bind_auto(unsigned port, unsigned id);

void input_config_set_pid(unsigned port, uint16_t pid);

uint16_t input_config_get_pid(unsigned port);

void input_config_set_vid(unsigned port, uint16_t vid);

uint16_t input_config_get_vid(unsigned port);

void input_config_reset(void);

extern input_device_driver_t dinput_joypad;
extern input_device_driver_t linuxraw_joypad;
extern input_device_driver_t parport_joypad;
extern input_device_driver_t udev_joypad;
extern input_device_driver_t xinput_joypad;
extern input_device_driver_t sdl_joypad;
extern input_device_driver_t ps3_joypad;
extern input_device_driver_t psp_joypad;
extern input_device_driver_t ctr_joypad;
extern input_device_driver_t xdk_joypad;
extern input_device_driver_t gx_joypad;
extern input_device_driver_t wiiu_joypad;
extern input_device_driver_t hid_joypad;
extern input_device_driver_t android_joypad;
extern input_device_driver_t qnx_joypad;
extern input_device_driver_t null_joypad;
extern input_device_driver_t mfi_joypad;
extern input_device_driver_t dos_joypad;

extern input_driver_t input_android;
extern input_driver_t input_sdl;
extern input_driver_t input_dinput;
extern input_driver_t input_x;
extern input_driver_t input_ps3;
extern input_driver_t input_psp;
extern input_driver_t input_ctr;
extern input_driver_t input_xenon360;
extern input_driver_t input_gx;
extern input_driver_t input_wiiu;
extern input_driver_t input_xinput;
extern input_driver_t input_linuxraw;
extern input_driver_t input_udev;
extern input_driver_t input_cocoa;
extern input_driver_t input_qnx;
extern input_driver_t input_rwebinput;
extern input_driver_t input_dos;
extern input_driver_t input_winraw;
extern input_driver_t input_wayland;
extern input_driver_t input_null;

#ifdef HAVE_HID
extern hid_driver_t iohidmanager_hid;
extern hid_driver_t btstack_hid;
extern hid_driver_t libusb_hid;
extern hid_driver_t wiiusb_hid;
extern hid_driver_t null_hid;
#endif

RETRO_END_DECLS

#endif

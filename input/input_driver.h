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

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>
#include <libretro.h>
#include <retro_miscellaneous.h>
#include <streams/interface_stream.h>
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#if defined(_WIN32) && !defined(SOCKET)
#include <winsock2.h>
#endif

#include "input_defines.h"
#include "input_types.h"
#ifdef HAVE_OVERLAY
#include "input_overlay.h"
#endif
#include "input_osk.h"

#include "../msg_hash.h"
#ifdef HAVE_HID
#include "include/hid_types.h"
#include "include/hid_driver.h"
#endif
#include "include/gamepad.h"
#include "../configuration.h"
#include "../performance_counters.h"

#ifdef HAVE_COMMAND
#include "../command.h"
#endif

#ifdef HAVE_BSV_MOVIE
#include "bsv/uint32s_index.h"
#endif

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
#endif /* WIIU_HID */
#elif defined(DJGPP)
#define DEFAULT_MAX_PADS 1
#define DOS_KEYBOARD_PORT DEFAULT_MAX_PADS
#elif defined(XENON)
#define DEFAULT_MAX_PADS 4
#elif defined(VITA) || defined(SN_TARGET_PSP2) || defined(ORBIS)
#define DEFAULT_MAX_PADS 4
#elif defined(PSP)
#define DEFAULT_MAX_PADS 1
#elif defined(PS2)
#define DEFAULT_MAX_PADS 8
#elif defined(GEKKO) || defined(HW_RVL)
#define DEFAULT_MAX_PADS 4
#elif defined(HAVE_ODROIDGO2)
#define DEFAULT_MAX_PADS 8
#elif (defined(BSD) && !defined(__MACH__))
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
#elif defined(EMSCRIPTEN)
#define DEFAULT_MAX_PADS 4
#else
#define DEFAULT_MAX_PADS 16
#endif /* defined(ANDROID) */

#define MAPPER_GET_KEY(state, key) (((state)->keys[(key) / 32] >> ((key) % 32)) & 1)
#define MAPPER_SET_KEY(state, key) (state)->keys[(key) / 32] |= 1 << ((key) % 32)
#define MAPPER_UNSET_KEY(state, key) (state)->keys[(key) / 32] &= ~(1 << ((key) % 32))

/*
  INVALID: should never arise.
  REGULAR: just key and button inputs, nothing else
  CHECKPOINT: an 8-byte size and serialized raw state follow the actions.
  CHECKPOINT2: a state follows the actions, but it is encoded and/or
               compressed in some way. The next two bytes are the compression
               type and the encoding type, followed by the 4-byte uncompressed,
               unencoded size; the 4-byte uncompressed, encoded size; the 4-byte
               compressed, encoeded, size; and the compressed, encoded data.
               If either the encoding or the compression codec are not supported,
               the checkpoint will be skipped.
 */
#define REPLAY_TOKEN_INVALID          '\0'
#define REPLAY_TOKEN_REGULAR_FRAME     'f'
#define REPLAY_TOKEN_CHECKPOINT_FRAME  'c'
#define REPLAY_TOKEN_CHECKPOINT2_FRAME 'C'

/* Which compression codec to use. */
#define REPLAY_CHECKPOINT2_COMPRESSION_NONE 0
#define REPLAY_CHECKPOINT2_COMPRESSION_ZLIB 1
#define REPLAY_CHECKPOINT2_COMPRESSION_ZSTD 2

/* Which encoding to use.
   RAW: Just raw checkpoint data, possibly compressed.
   STATESTREAM: Incremental, block-deduplicated encoding per
             https://github.com/sumitshetye2/v86_savestreams
*/
#define REPLAY_CHECKPOINT2_ENCODING_RAW 0
#define REPLAY_CHECKPOINT2_ENCODING_STATESTREAM 1

/**
 * Takes as input analog key identifiers and converts them to corresponding
 * bind IDs ident_minus and ident_plus.
 *
 * @param idx          Analog key index (eg RETRO_DEVICE_INDEX_ANALOG_LEFT)
 * @param ident        Analog key identifier (eg RETRO_DEVICE_ID_ANALOG_X)
 * @param ident_minus  Bind ID minus, will be set by function.
 * @param ident_plus   Bind ID plus,  will be set by function.
 */
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

RETRO_BEGIN_DECLS

enum rarch_movie_type
{
   RARCH_MOVIE_PLAYBACK = 0,
   RARCH_MOVIE_RECORD
};

enum input_driver_state_flags
{
   INP_FLAG_NONBLOCKING              = (1 << 0),
   INP_FLAG_KB_LINEFEED_ENABLE       = (1 << 1),
   INP_FLAG_KB_MAPPING_BLOCKED       = (1 << 2),
   INP_FLAG_BLOCK_HOTKEY             = (1 << 3),
   INP_FLAG_BLOCK_LIBRETRO_INPUT     = (1 << 4),
   INP_FLAG_BLOCK_POINTER_INPUT      = (1 << 5),
   INP_FLAG_GRAB_MOUSE_STATE         = (1 << 6),
   INP_FLAG_REMAPPING_CACHE_ACTIVE   = (1 << 7),
   INP_FLAG_DEFERRED_WAIT_KEYS       = (1 << 8),
   INP_FLAG_WAIT_INPUT_RELEASE       = (1 << 9),
   INP_FLAG_MENU_PRESS_PENDING       = (1 << 10),
   INP_FLAG_MENU_PRESS_CANCEL        = (1 << 11)
};

#ifdef HAVE_BSV_MOVIE
enum bsv_flags
{
   BSV_FLAG_MOVIE_START_RECORDING    = (1 << 0),
   BSV_FLAG_MOVIE_START_PLAYBACK     = (1 << 1),
   BSV_FLAG_MOVIE_PLAYBACK           = (1 << 2),
   BSV_FLAG_MOVIE_RECORDING          = (1 << 3),
   BSV_FLAG_MOVIE_END                = (1 << 4),
   BSV_FLAG_MOVIE_EOF_EXIT           = (1 << 5)
};

struct bsv_state
{
   uint8_t flags;
   /* Movie playback/recording support. */
   char movie_auto_path[PATH_MAX_LENGTH];
   /* Immediate playback/recording. */
   char movie_start_path[PATH_MAX_LENGTH];
};

/* These data are always little-endian. */
struct bsv_key_data
{
   uint8_t down;
   uint8_t _padding;
   uint16_t mod;
   uint32_t code;
   uint32_t character;
};
typedef struct bsv_key_data bsv_key_data_t;

struct bsv_input_data
{
   uint8_t port;
   uint8_t device;
   uint8_t idx;
   uint8_t _padding;
   /* little-endian numbers */
   uint16_t id;
   int16_t value;
};
typedef struct bsv_input_data bsv_input_data_t;

struct bsv_movie
{
   intfstream_t *file;
   int64_t identifier;
   uint32_t version;
   size_t min_file_pos;

   /* A ring buffer keeping track of positions
    * in the file for each frame. */
   size_t *frame_pos;
   size_t frame_mask;
   uint64_t frame_counter;

   /* Staging variables for events */
   uint8_t key_event_count;
   uint16_t input_event_count;
   bsv_key_data_t key_events[128];
   bsv_input_data_t input_events[512];

   /* Rewind state */
   bool playback;
   bool first_rewind;
   bool did_rewind;

#ifdef HAVE_STATESTREAM
   /* Block index and superblock index for incremental checkpoints */
   uint32s_index_t *superblocks;
   uint32s_index_t *blocks;
   uint32_t *superblock_seq;
   uint8_t commit_interval, commit_threshold;
#endif

   uint8_t checkpoint_compression, checkpoint_encoding;

   uint8_t *last_save, *cur_save;
   size_t last_save_size, cur_save_size;

   bool cur_save_valid;
};

typedef struct bsv_movie bsv_movie_t;
#endif

/**
 * line_complete callback (when carriage return is pressed)
 *
 * @param userdata User data which will be passed to subsequent callbacks.
 * @param line      the line of input, which can be NULL.
 **/
typedef void (*input_keyboard_line_complete_t)(void *userdata,
      const char *line);

struct input_keyboard_line
{
   char *buffer;
   void *userdata;
   /** Line complete callback.
    * Calls back after return is
    * pressed with the completed line.
    * Line can be NULL.
    **/
   input_keyboard_line_complete_t cb;
   size_t ptr;
   size_t size;
   bool enabled;
};

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
   char name[128];
   char display_name[128];
   char phys[NAME_MAX_LENGTH];
   char config_name[NAME_MAX_LENGTH]; /* Base name of the RetroArch config file */
   bool autoconfigured;
} input_device_info_t;

struct remote_message
{
   int port;
   int device;
   int index;
   int id;
   uint16_t state;
};

struct input_remote
{
#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
#ifdef _WIN32
   SOCKET net_fd[MAX_USERS];
#else
   int net_fd[MAX_USERS];
#endif
#endif
   bool state[RARCH_BIND_LIST_END];
};

typedef struct
{
   char display_name[NAME_MAX_LENGTH];
} input_mouse_info_t;

typedef struct input_remote input_remote_t;

typedef struct input_remote_state
{
   /* This is a bitmask of (1 << key_bind_id). */
   uint64_t buttons[MAX_USERS];
   /* Left X, Left Y, Right X, Right Y */
   int16_t analog[4][MAX_USERS];
} input_remote_state_t;

typedef struct input_list_element_t
{
   int16_t *state;
   unsigned port;
   unsigned device;
   unsigned index;
   unsigned int state_size;
} input_list_element;

/**
 * Organizes the functions and data structures of each driver that are accessed
 * by other parts of the input code. The input_driver structs are the "interface"
 * between RetroArch and the input driver.
 *
 * Every driver must establish an input_driver struct with pointers to its own
 * implementations of these functions, and each of those input_driver structs is
 * declared below.
 */
struct input_driver
{
   /**
    * Initializes input driver.
    *
    * @param joypad_driver  Name of the joypad driver associated with the
    *                       input driver
    */
   void *(*init)(const char *joypad_driver);

  /**
    * Called once every frame to poll input. This function pointer can be set
    * to NULL if not supported by the input driver, for example if a joypad
    * driver is responsible for polling on a particular driver/platform.
    *
    * @param data  the input state struct
    */
   void (*poll)(void *data);

   /**
    * Queries state for a specified control on a specified input port. This
    * function pointer can be set to NULL if not supported by the input driver,
    * for example if a joypad driver is responsible for querying state for a
    * particular driver/platform.
    *
    * @param joypad_data      Input state struct, defined by the input driver
    * @param sec_joypad_data  Input state struct for secondary input devices (eg
    *                         MFi controllers), defined by a secondary driver.
    *                         Queried state to be returned is the logical OR of
    *                         joypad_data and sec_joypad_data. May be NULL.
    * @param joypad_info      Info struct for the controller to be queried,
    *                         with hardware device ID and autoconfig mapping.
    * @param retro_keybinds   Structure for control mappings for all libretro
    *                         input device abstractions
    * @param keyboard_mapping_blocked
    *                         If true, disregard custom keyboard mapping
    * @param port             Which RetroArch port is being polled
    * @param device           Which libretro abstraction is being polled
    *                         (RETRO_DEVICE_ID_RETROPAD, RETRO_DEVICE_ID_MOUSE)
    * @param index            For controls with more than one axis or multiple
    *                         simultaneous inputs, such as an analog joystick
    *                         or touchpad.
    * @param id               Which control is being polled
    *                         (eg RETRO_DEVICE_ID_JOYPAD_START)
    *
    * @return 1 for pressed digital control, 0 for non-pressed digital control.
    *          Values in the range of a signed 16-bit integer,[-0x8000, 0x7fff]
    */
   int16_t (*input_state)(void *data,
         const input_device_driver_t *joypad_data,
         const input_device_driver_t *sec_joypad_data,
         rarch_joypad_info_t *joypad_info,
         const retro_keybind_set *retro_keybinds,
         bool keyboard_mapping_blocked,
         unsigned port, unsigned device, unsigned index, unsigned id);

   /**
    * Frees the input struct.
    *
    * @param data The input state struct.
    */
   void (*free)(void *data);

   /**
    * Sets the state related for sensors, such as polling rate or to deactivate
    * the sensor entirely, etc. This function pointer may be set to NULL if
    * setting sensor values is not supported.
    *
    * @param data
    * The input state struct
    * @param port
    * The port of the device
    * @param effect
    * Sensor action
    * @param rate
    * Sensor rate update
    *
    * @return true if the operation is successful.
   **/
   bool (*set_sensor_state)(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate);

   /**
    * Retrieves the sensor state associated with the provided port and ID. This
    * function pointer may be set to NULL if retrieving sensor state is not
    * supported.
    *
    * @param data
    * The input state struct
    * @param port
    * The port of the device
    * @param id
    * Sensor ID
    *
    * @return The current state associated with the port and ID as a float
    **/
   float (*get_sensor_input)(void *data, unsigned port, unsigned id);

   /**
    * The means for an input driver to indicate to RetroArch which libretro
    * input abstractions the driver supports.
    *
    * @param data  The input state struct.
    *
    * @return A unit64_t composed via bitwise operators.
    */
   uint64_t (*get_capabilities)(void *data);

   /**
    * The human-readable name of the input driver.
    */
   const char *ident;

   /**
    * Grab or ungrab the mouse according to the value of `state`. This function
    * pointer can be set to NULL if the driver does not support grabbing the
    * mouse.
    *
    * @param data   The input state struct
    * @param state  True to grab the mouse, false to ungrab
    */
   void (*grab_mouse)(void *data, bool state);

   /**
    * Check to see if the input driver has claimed stdin, and therefore it is
    * not available for other input. This function pointercan be set to NULL if
    * the driver does not support claiming stdin.
    *
    * @param data  The input state struct
    *
    * @return True if the input driver has claimed stdin.
    */
   bool (*grab_stdin)(void *data);

   /**
    * Haptic feedback for touchscreen key presses. This function pointer can be
    * set to NULL if haptic feedback / vibration is not supported.
    */
   void (*keypress_vibrate)(void);
};

struct rarch_joypad_driver
{
   void *(*init)(void *data);
   bool (*query_pad)(unsigned);
   void (*destroy)(void);
   int32_t (*button)(unsigned, uint16_t);
   int16_t (*state)(rarch_joypad_info_t *joypad_info,
         const struct retro_keybind *binds, unsigned port);
   void (*get_buttons)(unsigned, input_bits_t *);
   int16_t (*axis)(unsigned, uint32_t);
   void (*poll)(void);
   bool (*set_rumble)(unsigned, enum retro_rumble_effect, uint16_t);
   bool (*set_rumble_gain)(unsigned, unsigned);
   bool (*set_sensor_state)(void *data, unsigned port,
         enum retro_sensor_action action, unsigned rate);
   float (*get_sensor_input)(void *data, unsigned port, unsigned id);
   const char *(*name)(unsigned);

   const char *ident;
};

/**
 * Callback for keypress events
 *
 * @param userdata The user data that was passed through from the keyboard press callback.
 * @param code      keycode
 **/
typedef bool (*input_keyboard_press_t)(void *userdata, unsigned code);

struct input_keyboard_ctx_wait
{
   void *userdata;
   input_keyboard_press_t cb;
};

typedef struct
{
   /**
    * Array of timers, one for each entry in enum input_combo_type.
    */
   rarch_timer_t combo_timers[INPUT_COMBO_LAST];

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
   input_remote_state_t remote_st_ptr;        /* uint64_t alignment */
#endif

   /* pointers */
#ifdef HAVE_HID
   const void *hid_data;
#endif
   void *keyboard_press_data;
   input_keyboard_line_t keyboard_line;                  /* ptr alignment */
   input_keyboard_press_t keyboard_press_cb;             /* ptr alignment */
   input_driver_t                *current_driver;
   void                          *current_data;
   const input_device_driver_t   *primary_joypad;        /* ptr alignment */
   const input_device_driver_t   *secondary_joypad;      /* ptr alignment */
   const retro_keybind_set *libretro_input_binds[MAX_USERS];
#ifdef HAVE_COMMAND
   command_t *command[MAX_CMD_DRIVERS];
#endif
#ifdef HAVE_BSV_MOVIE
   bsv_movie_t     *bsv_movie_state_handle;              /* ptr alignment */
   bsv_movie_t     *bsv_movie_state_next_handle;         /* ptr alignment */
#endif
#ifdef HAVE_OVERLAY
   input_overlay_t *overlay_ptr;
   input_overlay_t *overlay_cache_ptr;
   enum overlay_visibility *overlay_visibility;
   float overlay_eightway_dpad_slopes[2];
   float overlay_eightway_abxy_slopes[2];

   /* touch pointer indexes from previous poll */
   int old_touch_index_lut[OVERLAY_MAX_TOUCH];
#endif
   uint16_t flags;
#ifdef HAVE_NETWORKGAMEPAD
   input_remote_t *remote;
#endif
   char    *osk_grid[45];                                /* ptr alignment */
#if defined(HAVE_TRANSLATE)
#if defined(HAVE_ACCESSIBILITY)
   int ai_gamepad_state[MAX_USERS];
#endif
#endif
   int osk_ptr;
   turbo_buttons_t turbo_btns; /* int32_t alignment */

   input_mapper_t mapper;          /* uint32_t alignment */
   input_remap_cache_t remapping_cache;
   input_device_info_t input_device_info[MAX_INPUT_DEVICES]; /* unsigned alignment */
   input_mouse_info_t input_mouse_info[MAX_INPUT_DEVICES];
   unsigned osk_last_codepoint;
   unsigned osk_last_codepoint_len;
   unsigned input_hotkey_block_counter;
#ifdef HAVE_ACCESSIBILITY
   unsigned gamepad_input_override;
#endif

   enum osk_type osk_idx;

#ifdef HAVE_BSV_MOVIE
   struct bsv_state bsv_movie_state;            /* char alignment */
#endif

   /* primitives */
   bool analog_requested[MAX_USERS];
   retro_bits_512_t keyboard_mapping_bits;    /* bool alignment */
   input_game_focus_state_t game_focus_state; /* bool alignment */
} input_driver_state_t;


void input_driver_init_joypads(void);

/**
 * Get an enumerated list of all input driver names
 *
 * @return string listing of all input driver names, separated by '|'.
 **/
const char* config_get_input_driver_options(void);

/**
 * Sets the rumble state.
 *
 * @param port
 * User number.
 * @param joy_idx
 * TODO/FIXME ???
 * @param effect
 * Rumble effect.
 * @param strength
 * Strength of rumble effect.
 *
 * @return true if the rumble state has been successfully set
 **/
bool input_driver_set_rumble(
         unsigned port, unsigned joy_idx,
         enum retro_rumble_effect effect, uint16_t strength);
/**
 * Sets the rumble gain.
 *
 * @param gain
 * Rumble gain, 0-100 [%]
 * @param input_max_users
 * TODO/FIXME - ???
 *
 * @return true if the rumble gain has been successfully set
 **/
bool input_driver_set_rumble_gain(
         unsigned gain,
         unsigned input_max_users);

/**
 * Sets the sensor state.
 *
 * @param port
 * User number.
 * @param sensors_enable
 * TODO/FIXME - ???
 * @param effect
 * Sensor action
 * @param rate
 * Sensor rate update
 *
 * @return true if the sensor state has been successfully set
 **/
bool input_driver_set_sensor(
         unsigned port, bool sensors_enable,
         enum retro_sensor_action action, unsigned rate);

/**
 * Retrieves the sensor state associated with the provided port and ID.
 *
 * @param port
 * Port of the device
 * @param sensors_enable
 * TODO/FIXME - ???
 * @param id
 * Sensor ID
 *
 * @return The current state associated with the port and ID as a float
 **/
float input_driver_get_sensor(
         unsigned port, bool sensors_enable, unsigned id);

uint64_t input_driver_get_capabilities(void);

bool video_driver_init_input(
      input_driver_t *tmp,
      settings_t *settings,
      bool verbosity_enabled);

bool input_driver_grab_mouse(void);

bool input_driver_ungrab_mouse(void);

/**
 * Get an enumerated list of all joypad driver names
 *
 * @return String listing of all joypad driver names, separated by '|'.
 **/
const char* config_get_joypad_driver_options(void);

/**
 * Initialize a joypad driver of name ident. If ident points to NULL or a
 * zero-length string, equivalent to calling input_joypad_init_first().
 *
 * @param ident  identifier of driver to initialize.
 * @param data   joypad state data pointer, which can be NULL and will be
 *               initialized by the new joypad driver, if one is found.
 *
 * @return The joypad driver if found, otherwise NULL.
 **/
const input_device_driver_t *input_joypad_init_driver(
      const char *ident, void *data);

/**
 * Registers a newly connected pad with RetroArch.
 *
 * @param port
 * Joystick number
 * @param driver
 * Handle for joypad driver handling joystick's input
 **/
void input_pad_connect(unsigned port, input_device_driver_t *driver);

/**
 * Called by drivers when keyboard events are fired. Interfaces with the global
 * driver struct and libretro callbacks.
 *
 * @param down
 * Was Keycode pressed down?
 * @param code
 * Keycode.
 * @param character
 * Character inputted.
 * @param mod
 * TODO/FIXME/???
 **/
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device);

input_driver_state_t *input_state_get_ptr(void);

/*************************************/
#ifdef HAVE_HID
/**
 * Get an enumerated list of all HID driver names
 *
 * @return String listing of all HID driver names, separated by '|'.
 **/
const char* config_get_hid_driver_options(void);

/**
 * Finds first suitable HID driver and initializes.
 *
 * @return HID driver if found, otherwise NULL.
 **/
const hid_driver_t *input_hid_init_first(void);

/**
 * Get a pointer to the HID driver data structure
 *
 * @return Pointer to hid_data struct
 **/
void *hid_driver_get_data(void);

/**
 * This should be called after we've invoked free() on the HID driver; the
 * memory will have already been freed so we need to reset the pointer.
 */
void hid_driver_reset_data(void);

#endif /* HAVE_HID */
/*************************************/


/**
 * Set the name of the device in the specified port
 *
 * @param port
 * The port of the device to be assigned to
 */
void input_config_set_device_name(unsigned port, const char *name);

/**
 * Set the formatted "display name" of the device in the specified port
 *
 * @param port
 * The port of the device to be assigned to
 */
void input_config_set_device_display_name(unsigned port, const char *name);
void input_config_set_mouse_display_name(unsigned port, const char *name);

/**
 * Set the configuration name for the device in the specified port
 *
 * @param port
 * The port of the device to be assigned to
 * @param name
 * The name of the config to set.
 */
void input_config_set_device_config_name(unsigned port, const char *name);

/**
 * Set the joypad driver for the device in the specified port
 *
 * @param port
 * The port of the device to be assigned to
 * @param driver
 * The driver to set the given port to.
 */
void input_config_set_device_joypad_driver(unsigned port, const char *driver);

/**
 * Set the vendor ID (vid) for the device in the specified port
 *
 * @param port
 * The port of the device to be assigned to
 * @param vid
 * The VID to set the given device port to.
 */
void input_config_set_device_vid(unsigned port, uint16_t vid);

/**
 * Set the pad ID (pid) for the device in the specified port
 *
 * @param port
 * The port of the device to be assigned to
 * @param pid
 * The PID to set the given device port to.
 */
void input_config_set_device_pid(unsigned port, uint16_t pid);

/**
 * Sets the autoconfigured flag for the device in the specified port
 *
 * @param port
 * The port of the device to be assigned to
 * @param autoconfigured
 * Whether or nor the device is configured automatically.
 */
void input_config_set_device_autoconfigured(unsigned port, bool autoconfigured);

/**
 * Sets the name index number for the device in the specified port
 *
 * @param port
 * The port of the device to be assigned to
 * @param name_index
 * The name index to set the device to use.
 */
void input_config_set_device_name_index(unsigned port, unsigned name_index);

/**
 * Sets the device type of the specified port
 *
 * @param port
 * The port of the device to be assigned to
 * @param id
 * The device type (RETRO_DEVICE_JOYPAD, RETRO_DEVICE_MOUSE, etc)
 */
void input_config_set_device(unsigned port, unsigned id);

/* Clear input_device_info */
void input_config_clear_device_name(unsigned port);
void input_config_clear_device_display_name(unsigned port);
void input_config_clear_device_config_name(unsigned port);
void input_config_clear_device_joypad_driver(unsigned port);

unsigned input_config_get_device_count(void);

unsigned *input_config_get_device_ptr(unsigned port);

unsigned input_config_get_device(unsigned port);

/* Get input_device_info */
const char *input_config_get_device_name(unsigned port);
const char *input_config_get_device_display_name(unsigned port);
const char *input_config_get_mouse_display_name(unsigned port);
const char *input_config_get_device_config_name(unsigned port);
const char *input_config_get_device_joypad_driver(unsigned port);

/**
 * Retrieves the vendor id (vid) of a connected controller
 *
 * @param port
 * The port of the device
 *
 * @return the vendor id VID of the device
 */
uint16_t input_config_get_device_vid(unsigned port);

/**
 * Retrieves the pad id (pad) of a connected controller
 *
 * @param port
 * The port of the device
 *
 * @return the port id PID of the device
 */
uint16_t input_config_get_device_pid(unsigned port);

/**
 * Returns the value of the autoconfigured flag for the specified device
 *
 * @param port
 * The port of the device
 *
 * @return the autoconfigured flag
 */
bool input_config_get_device_autoconfigured(unsigned port);

/**
 * Get the name index number for the device in this port
 *
 * @param port
 * The port of the device
 *
 * @return the name index for this device
 */
unsigned input_config_get_device_name_index(unsigned port);


/*****************************************************************************/

/**
 * Retrieve the device name char pointer.
 *
 * @deprecated input_config_get_device_name_ptr is required by linuxraw_joypad
 * and parport_joypad. These drivers should be refactored such that this
 * low-level access is not required.
 *
 * @param port
 * The port of the device
 *
 * @return a pointer to the device name on the specified port
 */
char *input_config_get_device_name_ptr(unsigned port);

/**
 * Get the size of the device name.
 *
 * @deprecated input_config_get_device_name_size is required by linuxraw_joypad
 * and parport_joypad. These drivers should be refactored such that this
 * low-level access is not required.
 *
 * @param port
 * The port of the device
 *
 * @return the size of the device name on the specified port
 */
size_t input_config_get_device_name_size(unsigned port);

unsigned input_driver_lightgun_id_convert(unsigned id);

bool input_driver_pointer_is_offscreen(int16_t x, int16_t y);

bool input_driver_button_combo(
      unsigned mode,
      retro_time_t current_time,
      input_bits_t* p_input);

bool input_driver_find_driver(
      settings_t *settings,
      const char *prefix,
      bool verbosity_enabled);

void input_keyboard_line_append(
      struct input_keyboard_line *keyboard_line,
      const char *word, size_t len);

void input_keyboard_line_clear(input_driver_state_t *input_st);
void input_keyboard_line_free(input_driver_state_t *input_st);

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
const char **input_keyboard_start_line(
      void *userdata,
      struct input_keyboard_line *keyboard_line,
      input_keyboard_line_complete_t cb);

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
input_remote_t *input_driver_init_remote(
      settings_t *settings,
      unsigned num_active_users);

void input_remote_free(input_remote_t *handle, unsigned max_users);
#endif

void input_game_focus_free(void);

/**
 * Converts a retro_keybind to a human-readable string, optionally allowing a
 * fallback auto_bind to be used as the source for the string.
 *
 * @param buf        A string which will be overwritten with the returned value
 * @param bind       A binding to convert to a string
 * @param auto_bind  A default binding which will be used after `bind`. Can be NULL.
 * @param size       The maximum length that will be written to `buf`
 */
size_t input_config_get_bind_string(void *settings_data,
      char *s, const struct retro_keybind *bind,
      const struct retro_keybind *auto_bind, size_t len);

size_t input_config_get_bind_string_joyaxis(
      bool input_descriptor_label_show,
      char *s, const char *prefix,
      const struct retro_keybind *bind, size_t len);

size_t input_config_get_bind_string_joykey(
      bool input_descriptor_label_show,
      char *s, const char *prefix,
      const struct retro_keybind *bind, size_t len);

bool input_key_pressed(int key, bool keyboard_pressed);

bool input_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength);

bool input_set_rumble_gain(unsigned gain);

float input_get_sensor_state(unsigned port, unsigned id);

bool input_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate);

void *input_driver_init_wrap(input_driver_t *input, const char *name);

const struct retro_keybind *input_config_get_bind_auto(unsigned port, unsigned id);

void input_config_reset_autoconfig_binds(unsigned port);

void input_config_reset(void);

const char *joypad_driver_name(unsigned i);

void joypad_driver_reinit(void *data, const char *joypad_driver_name);

#ifdef HAVE_COMMAND
void input_driver_init_command(
      input_driver_state_t *input_st,
      settings_t *settings);

void input_driver_deinit_command(input_driver_state_t *input_st);
#endif

#ifdef HAVE_OVERLAY
void input_overlay_unload(void);

void input_overlay_init(void);

void input_overlay_check_mouse_cursor(void);
#endif

#ifdef HAVE_BSV_MOVIE
void bsv_movie_frame_rewind(void);
void bsv_movie_next_frame(input_driver_state_t *input_st);
bool bsv_movie_read_next_events(bsv_movie_t *handle, bool skip_checkpoints);
bool bsv_movie_reset_recording(bsv_movie_t *handle);
void bsv_movie_finish_rewind(input_driver_state_t *input_st);
void bsv_movie_deinit(input_driver_state_t *input_st);
void bsv_movie_deinit_full(input_driver_state_t *input_st);
void bsv_movie_enqueue(input_driver_state_t *input_st, bsv_movie_t *state, enum bsv_flags flags);

bool movie_commit_checkpoint(input_driver_state_t *input_st);
bool movie_skip_to_prev_checkpoint(input_driver_state_t *input_st);
bool movie_skip_to_next_checkpoint(input_driver_state_t *input_st);
bool movie_start_playback(input_driver_state_t *input_st, char *path);
bool movie_start_record(input_driver_state_t *input_st, char *path);
bool movie_stop_playback(input_driver_state_t *input_st);
bool movie_stop_record(input_driver_state_t *input_st);
bool movie_stop(input_driver_state_t *input_st);

size_t replay_get_serialize_size(void);
bool replay_get_serialized_data(void* buffer);
bool replay_set_serialized_data(void* buffer);
#endif

/**
 * input_poll:
 *
 * Input polling callback function.
 **/
void input_driver_poll(void);

/**
 * input_state_wrapper:
 * @port                 : user number.
 * @device               : device identifier of user.
 * @idx                  : index value of user.
 * @id                   : identifier of key pressed by user.
 *
 * Input state callback function.
 *
 * Returns: Non-zero if the given key (identified by @id)
 * was pressed by the user (assigned to @port).
 **/
int16_t input_driver_state_wrapper(unsigned port, unsigned device,
      unsigned idx, unsigned id);

void input_driver_collect_system_input(input_driver_state_t *input_st,
      settings_t *settings, input_bits_t *current_bits);

/**
 * input_keyboard_event:
 * @down                     : Keycode was pressed down?
 * @code                     : Keycode.
 * @character                : Character inputted.
 * @mod                      : TODO/FIXME: ???
 *
 * Keyboard event utils. Called by drivers when keyboard events
 * are fired.
 * This interfaces with the global system driver struct
 * and libretro callbacks.
 **/
void input_keyboard_event(bool down, unsigned code,
      uint32_t character, uint16_t mod, unsigned device);

extern const unsigned input_config_bind_order[24];

extern input_device_driver_t *joypad_drivers[];
extern input_driver_t *input_drivers[];
#ifdef HAVE_HID
extern hid_driver_t *hid_drivers[];
#endif

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
extern input_driver_t input_test;

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
extern input_device_driver_t test_joypad;

#ifdef HAVE_HID
extern hid_driver_t iohidmanager_hid;
extern hid_driver_t btstack_hid;
extern hid_driver_t libusb_hid;
extern hid_driver_t wiiusb_hid;
extern hid_driver_t wiiu_hid;
#endif /* HAVE_HID */

extern retro_keybind_set input_config_binds[MAX_USERS];
extern retro_keybind_set input_autoconf_binds[MAX_USERS];

RETRO_END_DECLS

#endif /* __INPUT_DRIVER__H */

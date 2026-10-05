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
#include <retro_atomic.h>
#include <retro_inline.h>
#include <libretro.h>
#include <retro_miscellaneous.h>
#include <streams/interface_stream.h>
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

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
#elif defined(__EMSCRIPTEN__)
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
   INP_FLAG_MENU_PRESS_CANCEL        = (1 << 11),
   /* A system-provided keyboard panel is on screen and owns text
    * entry. Set and cleared by whichever input driver put it there,
    * once per poll; read through input_osk_native_active(). */
   INP_FLAG_NATIVE_KB_SHOWN          = (1 << 12),
   /* This device has a native keyboard panel the frontend could use
    * in place of the built-in OSK. Published the same way; read
    * through input_osk_native_available(). */
   INP_FLAG_NATIVE_KB_AVAIL          = (1 << 13),
   /* Background controller input is off and the window is not
    * focused: the joypad read paths see an idle controller. Set once
    * per poll; read through input_driver_joypad_for_read(). */
   INP_FLAG_JOYPAD_UNFOCUSED         = (1 << 14),
   /* led_driver is "overlay": the overlay shows the LEDs'
    * state (overlay_leds_lit). */
   INP_FLAG_OVERLAY_LEDS             = (1 << 15)
};

#ifdef HAVE_BSV_MOVIE
enum bsv_flags
{
   BSV_FLAG_MOVIE_START_RECORDING    = (1 << 0),
   BSV_FLAG_MOVIE_START_PLAYBACK     = (1 << 1),
   BSV_FLAG_MOVIE_PLAYBACK           = (1 << 2),
   BSV_FLAG_MOVIE_RECORDING          = (1 << 3),
   BSV_FLAG_MOVIE_END                = (1 << 4),
   BSV_FLAG_MOVIE_EOF_EXIT           = (1 << 5),
   BSV_FLAG_MOVIE_FORCE_CHECKPOINT   = (1 << 6),
   BSV_FLAG_MOVIE_PREV_CHECKPOINT    = (1 << 7),
   BSV_FLAG_MOVIE_NEXT_CHECKPOINT    = (1 << 8),
   BSV_FLAG_MOVIE_SEEK_TO_FRAME      = (1 << 9),
   BSV_FLAG_MOVIE_SEEKING            = (1 << 10)
};

struct bsv_state
{
   uint16_t flags;
   /* Movie playback/recording support. */
   char movie_auto_path[PATH_MAX_LENGTH];
   /* Immediate playback/recording. */
   char movie_start_path[PATH_MAX_LENGTH];
   /* Target frame/position to seek to next iteration. */
   int64_t seek_target_frame, seek_target_pos;
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
   bool checkpoint_ready;

#ifdef HAVE_STATESTREAM
   /* Block index and superblock index for incremental checkpoints */
   uint32s_index_t *superblocks;
   uint32s_index_t *blocks;
   uint32_t *superblock_seq;
   size_t superblock_seq_len;
   uint8_t commit_interval, commit_threshold;
#endif

   uint8_t checkpoint_compression, checkpoint_encoding;

   uint8_t *last_save, *cur_save;
   size_t last_save_size, cur_save_size;

   bool cur_save_valid;
};

typedef struct bsv_movie bsv_movie_t;

enum replay_checkpoint_behavior_ {
   REPLAY_CPBEHAVIOR_SKIP,
   REPLAY_CPBEHAVIOR_UPDATE,
   REPLAY_CPBEHAVIOR_DESERIALIZE
};

typedef enum replay_checkpoint_behavior_ replay_checkpoint_behavior;

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
   size_t capacity;  /* Allocated size of buffer (excluding NUL) */
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

typedef struct
{
   char display_name[NAME_MAX_LENGTH];
   /* For the menu's Input Information, where the driver says: which
    * device on the desk this mouse is part of ("" if not known), its
    * USB ids (both 0 if not known), and whether it is one to leave
    * out - an index with no mouse behind it. */
   char device[64];
   uint16_t vid;
   uint16_t pid;
   bool hidden;
} input_mouse_info_t;

/* A keyboard the input driver can tell from the others: its name, for
 * the menu. In the order the driver lists them. */
typedef struct
{
   char display_name[NAME_MAX_LENGTH];
   uint16_t vid; /* 0 with pid 0: not known */
   uint16_t pid;
} input_keyboard_info_t;

typedef struct
{
   int8_t source; /* RETRO_SENSOR_* ID to read from (0-5), -1 = unmapped */
   int8_t sign;   /* +1 or -1 */
} input_sensor_axis_t;

typedef struct
{
   /* indexed by RETRO_SENSOR_ACCELEROMETER_X..RETRO_SENSOR_GYROSCOPE_Z */
   input_sensor_axis_t axes[6];
} input_sensor_map_t;

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

   /**
    * Whether the driver, as it is running now, can be left running while
    * the video driver is restarted: nothing it holds belongs to the video
    * driver, its window or its thread, and it needs no restart to notice
    * anything a restart used to make it notice. The joypad driver in use
    * is its to answer for as well, since the two are kept or restarted
    * together. NULL, the last member and so what every driver that does
    * not name it has, means no: the driver is freed with the video driver
    * and started again, as it always was. See
    * input_driver_keep_for_video_restart().
    *
    * @param data  The input state struct
    *
    * @return True if the driver can stay.
    */
   bool (*survives_video)(void *data);
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
   bool (*set_sensor_state)(unsigned port,
         enum retro_sensor_action action, unsigned rate);
   /* return true if handled; false to fall back to input driver */
   bool (*get_sensor_input)(unsigned port, unsigned id, float *value);
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
   /* When the devices were last read, on cpu_features_get_time_usec()'s
    * clock; 0 while nothing asks for it. See input_driver_poll(). */
   retro_time_t poll_time_us;
   /* The input driver left running across a video driver restart, from
    * the video driver's teardown until its start-up hands it back.
    * input_driver_keep_for_video_restart(). */
   input_driver_t *kept_driver;
   void *kept_data;
   /* The joypad driver setting the joypad driver was started with. */
   char joypad_setting_at_init[32];
   /* The settings that decide which port a controller gets, as they
    * were then (input_driver_detect_settings()). */
   uint32_t detect_settings_at_init;
   /* Something has thrown away what the drivers' start gives - the
    * controllers' configuration - so the next restart is a restart of
    * the input driver too, whatever it says.
    * input_driver_restart_with_next_video_restart(). */
   bool kept_not_next;
#ifdef HAVE_COMMAND
   /* Bumped whenever the command interfaces below are torn down. A
    * command that reinitialises the input driver - LOAD_CONTENT,
    * DRIVERS_REINIT - frees the very object whose poll dispatched it;
    * the dispatcher compares this before and after and stops when it
    * has changed rather than touch that object again. */
   unsigned command_generation;
   command_t *command[MAX_CMD_DRIVERS];
#endif
#ifdef HAVE_BSV_MOVIE
   bsv_movie_t     *bsv_movie_state_handle;              /* ptr alignment */
   bsv_movie_t     *bsv_movie_state_next_handle;         /* ptr alignment */
#endif
#ifdef HAVE_OVERLAY
   input_overlay_t *overlay_ptr;
   input_overlay_t *overlay_cache_ptr;
   float overlay_eightway_dpad_slopes[2];
   float overlay_eightway_abxy_slopes[2];

   /* touch pointer indexes from previous poll */
   int old_touch_index_lut[OVERLAY_MAX_TOUCH];
   /* Bit n: LED n + 1 is lit, for the overlay LED driver. */
   uint32_t overlay_leds_lit;
#endif
   uint16_t flags;
   /* Read and written every poll; kept beside flags so the per-poll
    * bookkeeping shares a cacheline with the hot pointer block above,
    * rather than sitting past the input_device_info string tables
    * (~17 KB of cold data) as it used to. */
   unsigned input_hotkey_block_counter;
   /* the users the poll's remap work ran for last time, one bit each:
    * what it left in the mapper is cleared when it stops running */
   uint32_t remap_worked;
#ifdef HAVE_ACCESSIBILITY
   unsigned gamepad_input_override;
#endif
#ifdef HAVE_NETWORKGAMEPAD
   input_remote_t *remote;
#endif
#if defined(HAVE_TRANSLATE)
#if defined(HAVE_ACCESSIBILITY)
   int ai_gamepad_state[MAX_USERS];
#endif
#endif
   bool osk_textbox_focus;
   turbo_buttons_t turbo_btns; /* int32_t alignment */
   hold_buttons_t hold_btns;   /* int32_t alignment */

   input_mapper_t mapper;          /* uint32_t alignment */
   input_remap_cache_t remapping_cache;
   input_device_info_t input_device_info[MAX_INPUT_DEVICES]; /* unsigned alignment */
   input_mouse_info_t input_mouse_info[MAX_INPUT_DEVICES];
   input_keyboard_info_t input_keyboard_info[MAX_INPUT_DEVICES];
   /* the listed keyboards' identities, and what each port reads */
   char     keyboard_identity[MAX_INPUT_DEVICES][64];
   unsigned keyboard_identities;
   int8_t   keyboard_choice[MAX_USERS];
   uint16_t keyboard_absent;         /* a bit a port: pinned, not there */
   /* the same for the mice */
   char     mouse_identity[MAX_INPUT_DEVICES][64];
   unsigned mouse_identities;
   int16_t  mouse_choice[MAX_USERS];
   uint16_t mouse_pinned;            /* a bit a port: mouse_choice holds */
   uint16_t mouse_absent;
   input_sensor_map_t input_sensor_map[MAX_INPUT_DEVICES];

   /**
    * Array of timers, one for each entry in enum input_combo_type.
    * One indexed entry is read per frame, and only while a button
    * combo is bound; cold enough that it has no business occupying
    * the first six cachelines of this struct, which it used to.
    * (rarch_timer_t is int64-aligned; the compiler pads as needed
    * here, which costs at most 4 bytes.)
    */
   rarch_timer_t combo_timers[INPUT_COMBO_LAST];

   unsigned osk_last_codepoint;
   unsigned osk_last_codepoint_len;


#ifdef HAVE_BSV_MOVIE
   struct bsv_state bsv_movie_state;            /* char alignment */
#endif

   /* primitives */
   bool analog_requested[MAX_USERS];

   /* Per-port joypad state bitmask cache.
    * Populated lazily on the first JOYPAD query for each mapped_port
    * within a frame, then reused for subsequent individual button
    * queries to the same port.  Avoids up to 32 indirect
    * joypad->button()/axis() calls per port when cores use the
    * old per-button query pattern instead of JOYPAD_MASK.
    * Invalidated at the start of each input_driver_poll(). */
   int32_t joypad_state_cache[MAX_USERS];

   /* Per-port set of RetroPad buttons (bits 0..RARCH_FIRST_CUSTOM_BIND-1)
    * that INP_FLAG_WAIT_INPUT_RELEASE is still waiting on.  Captured
    * from whatever is held on the frame the wait is armed and pruned
    * as those buttons are released; a button pressed after the wait
    * was armed is never in the set, so it is delivered normally.
    * While the flag is clear it simply tracks what is held, so a wait
    * armed outside input_keys_pressed() starts from the previous
    * frame's held set. */
   uint16_t wait_release_mask[MAX_USERS];

   /* The frame's view of each port's RetroPad buttons, as a core is
    * given them: after port mapping, remaps, turbo, hold, overlays and
    * analog-to-d-pad.  Compiled when a core asks for the port's mask,
    * or for a second button in the frame; from then on every button
    * and mask query for the port reads this word.  Invalidated by
    * input_driver_poll(), and when the core's first analog request
    * changes analog-to-d-pad for the port. */
   int16_t frame_view_joypad[MAX_USERS];
   /* Full-range triggers, per controller, as bits by axis: axes bound
    * to L2 or R2 that were seen resting at the far end from their
    * bind (the pull is counted from there), and axes not to be looked
    * at again because both their directions are bound. */
   uint16_t trigger_rest[MAX_USERS];
   /* Pointer capture: the reasons held (enum input_capture_reason),
    * and whether the cursor was last hidden for them. */
   uint8_t  capture_reasons;
   /* The RetroPad buttons something other than the overlay held when
    * the system's input was last collected: a controller, a key, a
    * command, a network pad. What is pressed and not in here came from
    * the overlay alone. */
   uint16_t system_buttons_not_overlay;
   bool     capture_cursor_hidden;
   uint16_t trigger_two_way[MAX_USERS];
   /* SOCD cleaning, per core port: the D-Pad as it was held when the
    * port's view was last compiled, and for each axis the direction
    * (as its RetroPad button bit) that has the say while both are held. */
   uint8_t socd_held[MAX_USERS];
   uint8_t socd_winner[MAX_USERS][2];

   /* What input_driver_poll() invalidates, a bit per port in each
    * word, kept together so that invalidating all of it is one store:
    *   joypad_cache - joypad_state_cache[port] holds this frame's mask
    *   view         - frame_view_joypad[port] has been compiled
    *   asked        - the port's first button of the frame was read */
   struct
   {
      uint16_t joypad_cache;
      uint16_t view;
      uint16_t asked;
      uint16_t snapshot[2]; /* pads snapshotted: primary, secondary driver */
   } frame_valid;

   retro_bits_512_t keyboard_mapping_bits;    /* bool alignment */
   input_game_focus_state_t game_focus_state; /* bool alignment */

   /* Cached sensor values — written once per frame in input_driver_poll()
    * on the main thread, read by shader backends on the video thread.
    * Not formally synchronized; relies on single-writer/single-reader
    * float stores being practically atomic on ARM/x86. Worst case is
    * a single stale frame of sensor data. */
   float sensor_gyroscope_cache[3];
   float sensor_accelerometer_cache[3];

   /* Accelerometer rest position capture state.
    * Same thread-safety model as the caches above:
    * written on the main thread, read by shader backends
    * on the video thread. */
   float sensor_accelerometer_rest[3];
   float rest_accum[3];
   unsigned rest_sample_count;
   bool rest_capturing;
   /* Set from the shader backends' pass builders (video thread under
    * the threaded wrapper) through
    * input_driver_set_shader_uses_sensors(), read by the poll's
    * demand check on the main thread: atomic with release/acquire,
    * not a plain bool. */
   retro_atomic_int_t shader_uses_sensors;
   /* Seqlock publication of the sensor caches below - poll computes
    * the caches, then publishes gyro, accelerometer and rest (nine
    * floats as bits) here for the shader backends' per-frame reads
    * from the video thread. */
   retro_atomic_int_t sensor_snap_seq;
   retro_atomic_int_t sensor_snap_bits[9];
   bool frontend_sensors_enabled;
   /* The snapshot shaders read holds noughts and no shader reads
    * sensors: the poll has nothing to publish. The poll's own. */
   bool sensor_snap_quiet;
   /* the platform's own menu button is held
    * (input_driver_set_platform_menu_button()) */
   bool platform_menu_button;
   unsigned core_accel_rate; /* >0 means core wants accel at this rate */
   unsigned core_gyro_rate;  /* >0 means core wants gyro at this rate */
   /* First-press port assignment, see input_first_press_apply():
    * buttons seen released on a user with no core port, the users to
    * map at the next frame boundary, and whether any of it is live. */
   uint32_t first_press_released[MAX_USERS];
   uint32_t first_press_pending;
   /* users whose core port a press gave them, not the user */
   uint32_t first_press_assigned;
   /* the policy, as it stood when the remap defaults were last set */
   bool     first_press_on;
   bool     first_press_live;
} input_driver_state_t;


/**
 * input_driver_joypad_for_read:
 * @drv                      : primary or secondary joypad driver, or NULL.
 *
 * The joypad driver to read controller state from. While background
 * controller input is off and RetroArch is unfocused this is a stand-in
 * that reports every button released and every axis centred, so the
 * menu, hotkeys and the core all see an idle controller. Rumble,
 * sensors, device names and driver lifetime keep using @drv directly.
 *
 * Returns: @drv, or the idle stand-in; NULL if @drv is NULL.
 **/
const input_device_driver_t *input_driver_joypad_for_read(
      const input_device_driver_t *drv);

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
/* Release-stores the shader-demand latch; callable from the shader
 * backends on the video thread. */
void input_driver_set_shader_uses_sensors(bool uses);

/* Seqlock read of the poll-published sensor snapshot: gyroscope,
 * accelerometer and accelerometer-rest vec3s, coherent as a set.
 * For the shader backends' per-frame uniform uploads on the video
 * thread; converges immediately on the main thread. */
void input_driver_read_sensor_snapshot(float *gyro3,
      float *accel3, float *rest3);

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

/* A port's keyboard, kept by what the keyboard is
 * (input/common/input_device_pins.h).
 *
 * An input driver that lists keyboards tells what it knows each by -
 * its USB ids, or its name where it has none - whenever it has made
 * its list, in the list's order: */
void input_keyboard_pins_set_devices(const char (*base)[64], unsigned n);
/* What a port reads now: 0 for every keyboard as one, a keyboard's
 * number in the list, or -1 for none. Read on the input path. */
int input_keyboard_port_choice(unsigned port);
/* The port's Keyboard Index was changed by hand: the port is pinned
 * to the keyboard that number names, or to none. */
void input_keyboard_pin_from_index(unsigned port);
/* The port is pinned to a keyboard that is not there. */
bool input_keyboard_pin_absent(unsigned port);

/* The same for a port's mouse. An input driver that lists mice tells
 * what it knows each by, in the order of their Mouse Index: */
void input_mouse_pins_set_devices(const char (*base)[64], unsigned n);
/* The mouse a port reads now, as a Mouse Index: the port's own
 * setting, the place its pinned mouse has in the list, or
 * MAX_INPUT_DEVICES for none. Read on the input path, in place of
 * the setting. */
unsigned input_mouse_port_index(unsigned port);
/* The port's Mouse Index was changed by hand: the port is pinned to
 * the mouse that number names. @pin false removes the pin. */
void input_mouse_pin_from_index(unsigned port, bool pin);
bool input_mouse_pin_absent(unsigned port);

/* The running input driver's name ("raw", "udev", "wayland"...), or
 * an empty string when there is none. */
const char *input_driver_get_ident(void);

/* Which input driver a window gets: input_driver_choice.c. */
#if defined(_WIN32) || defined(_XBOX) || defined(__WINRT__)
void input_driver_init_windows(const char *joypad_name,
      input_driver_t **input, void **input_data);
#endif
#ifdef HAVE_X11
void input_driver_init_x11(const char *joypad_name,
      input_driver_t **input, void **input_data);
#endif
void input_driver_init_kms(const char *joypad_name,
      input_driver_t **input, void **input_data);
#ifdef HAVE_WAYLAND
void input_driver_init_wayland(const char *joypad_name, void *window_data,
      input_driver_t **input, void **input_data);
#endif
#if defined(HAVE_SDL) && !defined(HAVE_SDL2) && !defined(HAVE_SDL3)
void input_driver_init_sdl1(const char *joypad_name,
      input_driver_t **input, void **input_data);
#endif
/* The settings a driver reads, by name: a driver does not take the
 * settings (config_get_ptr()) and pick a field out. */
unsigned input_config_get_mouse_index(unsigned port);
unsigned input_config_get_joypad_index(unsigned port);
unsigned input_config_get_rumble_gain(void);
bool input_config_get_nowinkey_enable(void);
bool input_config_get_keyboard_background(void);
bool input_config_get_winraw_xinput_enable(void);
bool input_config_get_winraw_player_lights(void);
bool input_config_get_sdl3_system_keyboard(void);
bool input_config_overlay_configured(void);
unsigned input_config_get_split_joycon(unsigned port);
bool input_config_get_backtouch_enable(void);
bool input_config_get_backtouch_toggle(void);
bool input_config_get_keyboard_gamepad_enable(void);
bool input_config_get_small_keyboard_enable(void);
unsigned input_config_get_keyboard_gamepad_mapping_type(void);
bool input_config_get_suspend_screensaver_enable(void);
unsigned input_config_get_mouse_scale(void);
bool input_config_get_touch_vmouse_pointer(void);
bool input_config_get_touch_vmouse_mouse(void);
bool input_config_get_touch_vmouse_touchpad(void);
bool input_config_get_touch_vmouse_trackball(void);
bool input_config_get_touch_vmouse_gesture(void);
/* The two things a driver changes in the settings, each by name. */
void input_config_set_joypad_index(unsigned port, unsigned idx);
bool input_driver_first_start_fallback(const char *ident);
bool input_config_get_sensors_enable(void);
unsigned input_config_get_block_timeout(void);
bool input_config_get_device_vibration(void);
bool input_config_get_stylus_enable(void);
bool input_config_get_stylus_require_contact_for_click(void);
bool input_config_get_stylus_hover_moves_pointer(void);
unsigned input_config_get_stylus_pressure_sensitivity(void);
bool input_config_get_android_disconnect_workaround(void);
const char *input_config_get_android_physical_keyboard(void);
const char *input_config_get_joypad_driver(void);
const char *input_config_get_keyboard_layout(void);
const char *input_config_get_autoconfig_dir(void);
const char *input_config_get_test_input_file(bool joypad);

/* What a driver may ask the frontend, and tell it, without taking its
 * state (input_state_get_ptr()): each is one thing, by name. */
void input_driver_set_native_keyboard_available(bool available);
void input_driver_set_native_keyboard_shown(bool shown);
bool input_driver_native_keyboard_shown(void);
bool input_driver_keyboard_mapping_blocked(void);
bool input_driver_keyboard_line_enabled(void);
void input_driver_keyboard_line_set(const char *utf8, size_t len);
void input_driver_keyboard_line_end(void);
bool input_driver_pointer_input_blocked(void);
bool input_driver_game_focus_enabled(void);
bool input_driver_mouse_grabbed(void);
void *input_driver_current_data(void);

/* The platform's own menu button - one that is not a controller's and
 * has no bind - is held, or is not. Held, it counts as the Menu Toggle
 * hotkey held. For the driver that reads the button, on the frontend's
 * thread. */
void input_driver_set_platform_menu_button(bool held);

/* Reads of the input driver for a consumer that is not the core - the
 * menu. It asks here and holds neither the driver nor its data nor a
 * joypad. The two reads are valid only while there is a driver to
 * read, input_driver_has_device_state().
 *
 * input_driver_device_state(): a mouse, a pointer or a key; no binds
 * go to the driver.
 * input_driver_bind_capture_state(): the same with the frontend's
 * binds and the pad @joy_idx, as the capture of a bind reads them. */
bool input_driver_has_device_state(void);
int16_t input_driver_device_state(unsigned port,
      unsigned device, unsigned idx, unsigned id);
int16_t input_driver_bind_capture_state(unsigned joy_idx, unsigned port,
      unsigned device, unsigned idx, unsigned id);

/* A controller as the controller has it, for the capture of a bind:
 * each array that is given is filled - @buttons with whether each is
 * down, @axes with each axis's position, @hats with the directions
 * held OR-ed in. @poll polls the joypad drivers first. */
void input_driver_capture_pad(unsigned pad, bool poll,
      bool *buttons, unsigned num_buttons,
      int16_t *axes, unsigned num_axes,
      uint16_t *hats, unsigned num_hats);

/* The device drivers are polled and nothing else of a poll is done. */
void input_driver_poll_devices(void);

/* A controller's profile is looked up again, as if just connected. */
void input_driver_autoconfigure_pad(unsigned pad);

/* The RetroPad controls a user's controller and keys hold right now, as
 * bound and before remaps: a bit each for the sixteen buttons, then
 * for the sticks' eight directions (RARCH_ANALOG_LEFT_X_PLUS on).
 * Reads the devices; for the menu while it waits for a press. */
uint32_t input_driver_user_controls_bound(unsigned user);

void input_driver_init_platform(const char *joypad_name,
      input_driver_t **input, void **input_data);
#ifdef HAVE_SDL3
void input_driver_init_sdl3(const char *joypad_name,
      input_driver_t **input, void **input_data);
/* Whether the input driver in use is the SDL 3 one, which reads the
 * SDL window's event queue. For the code that pumps that queue. */
bool input_driver_is_sdl3(void);
#endif

/* Leaving the input driver running across a video driver restart: see
 * input_driver.c. */
void input_driver_keep_for_video_restart(bool restart, const void *video_data);
bool input_driver_take_kept(input_driver_t **input, void **input_data);
void input_driver_drop_kept(void);
void input_driver_restart_with_next_video_restart(void);

/* For the video driver: see input_driver.c. */
retro_time_t input_driver_get_poll_time(void);
uint32_t input_driver_get_flags(void);
input_driver_t *input_driver_get_current(void);
input_driver_t **input_driver_video_slots(void ***data_slot);
void input_driver_free_with_video(const void *video_data);
/* Why the pointer is captured. It used to be one flag that about ten
 * places toggled or rewrote, each after looking at it, so what it
 * ended up as went by the order they ran in: leaving fullscreen let
 * go of a grab game focus still wanted, and game focus going off let
 * go of one the user had asked for.
 *
 * Now each of them holds or releases its own reason, and the pointer
 * is captured while any is held. INP_FLAG_GRAB_MOUSE_STATE is what
 * that comes to, and is only written by input_pointer_capture_apply().
 * The reasons are kept above the drivers, so a driver that restarts
 * has them applied to it again. */
enum input_capture_reason
{
   /* the hotkey, or a menu's entry */
   INPUT_CAPTURE_USER              = (1 << 0),
   /* "Automatic Mouse Grab", taken when the window gains focus */
   INPUT_CAPTURE_AUTO_FOCUS        = (1 << 1),
   INPUT_CAPTURE_GAME_FOCUS        = (1 << 2),
   /* exclusive fullscreen */
   INPUT_CAPTURE_FULLSCREEN        = (1 << 3),
   /* any fullscreen: this one hides the cursor and grabs nothing */
   INPUT_CAPTURE_FULLSCREEN_CURSOR = (1 << 4)
};

void input_pointer_capture_hold(unsigned reasons);
void input_pointer_capture_release(unsigned reasons);

/* What exclusive fullscreen adds, for the video mode there is now.
 * Nothing is applied: input_pointer_capture_apply() follows. */
void input_pointer_capture_set_fullscreen(bool fullscreen, bool exclusive);

/* The user's toggle: captured, every reason is let go; not captured,
 * the user's is held. False, and nothing changed, if the input driver
 * cannot grab. */
bool input_pointer_capture_toggle(void);

/* Makes the grab and the cursor what the reasons come to. @force is
 * for a driver that has just started and has been told nothing yet. */
void input_pointer_capture_apply(bool force);

/* The reasons held, for the tests. */
unsigned input_pointer_capture_reasons(void);

/* For a combination of RetroPad buttons that opens the menu: buttons
 * held on the overlay alone are taken out of @bits when the overlay
 * shown has a menu button of its own. A thumb resting on two overlay
 * buttons otherwise opens the menu by accident, on an overlay that
 * has a button for that anyway (issue #19472). An overlay with no
 * menu button is left alone: the combination is its only way in. */
void input_driver_menu_combo_source_gate(input_bits_t *bits);

/* What kind of window a video driver put up, for the input driver that
 * goes with it. */
enum input_window_kind
{
   INPUT_WINDOW_OTHER = 0,
   /* a Windows window (or UWP, or Xbox): input_driver_init_windows() */
   INPUT_WINDOW_WINDOWS,
   /* an X11 window: input_driver_init_x11() */
   INPUT_WINDOW_X11,
   /* no window system - KMS/DRM, a Vulkan display:
    * input_driver_init_kms() */
   INPUT_WINDOW_KMS,
   /* a Wayland surface, whose seat's state the video context holds
    * and hands over (input_driver_left_to_frontend_with()):
    * input_driver_init_wayland() */
   INPUT_WINDOW_WAYLAND,
   /* an SDL 3 window: input_driver_init_sdl3() */
   INPUT_WINDOW_SDL3,
   /* an SDL 1.2 window: input_driver_init_sdl1() */
   INPUT_WINDOW_SDL1,
   /* a platform with one input driver of its own - a console,
    * Android, the web: input_driver_init_platform() */
   INPUT_WINDOW_PLATFORM
};

/* For a video driver's or a context's start-up, in place of starting
 * an input driver itself: it brings none, and the frontend is to
 * start the one that goes with a window of this kind. Clears the two
 * slots. A driver that says nothing here and fills in no slots gets
 * the input driver the setting names, as ever. */
void input_driver_left_to_frontend(enum input_window_kind window,
      input_driver_t **input, void **input_data);

/* The same, for a window system whose input state lives with the
 * window, in the video driver's own data: @window_data is that state,
 * and is what the input driver the frontend starts is given. */
void input_driver_left_to_frontend_with(enum input_window_kind window,
      void *window_data,
      input_driver_t **input, void **input_data);

/* Called once the video driver is up. If it brought an input driver of
 * its own, that is the input driver. Otherwise one is started here:
 * the one kept from before the restart; the one that goes with the
 * kind of window the video driver named
 * (input_driver_left_to_frontend()); failing those, the one the
 * setting names. */
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

/* Keyboard events reported from another thread wait for the poll in a
 * queue (see input_keyboard_event() in input_driver.c); this is how
 * many were dropped because it was full. */
unsigned input_driver_key_events_dropped(void);

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

/* The mice, for the menu (Information > Input Information). A mouse is
 * numbered by its Mouse Index, which is the input driver's own index
 * and is not changed by any of this. A driver that knows more says it
 * here: @device is what tells one device on the desk from another,
 * the same for every part of it ("" if not known) - one mouse is
 * often two or three of the system's mice, and they are listed on
 * one line; @hidden is an index nothing on the desk is behind.
 * Cleared, names included, whenever an input driver starts. Main
 * thread. */
void input_config_clear_mouse_info(void);
void input_config_set_mouse_device(unsigned idx, const char *device,
      uint16_t vid, uint16_t pid, bool hidden);
const char *input_config_get_mouse_device(unsigned idx);
/* Whether Mouse Index offers this index. Where the input driver
 * lists its mice, the ones on the desk are offered - not an index
 * with no mouse, and not one the driver says to leave out (a keyboard
 * that can send pointer events, a mouse a program made). Under a
 * driver that names no mice every index is offered, as it always
 * was. */
bool input_config_mouse_offered(unsigned idx);
uint16_t input_config_get_mouse_vid(unsigned idx);
uint16_t input_config_get_mouse_pid(unsigned idx);
bool input_config_get_mouse_hidden(unsigned idx);

/* The keyboards the input driver can tell apart, for the menu
 * (Information > Input Information). A driver that lists its
 * keyboards clears the names and sets one for each, in its own order,
 * when it starts and whenever its list changes; a name that would be
 * empty is stored as "N/A". Under a driver that cannot tell keyboards
 * apart there are none. Main thread. */
void input_config_clear_keyboard_display_names(void);
void input_config_set_keyboard_display_name(unsigned idx, const char *name);
/* NULL if there is no keyboard at that index */
const char *input_config_get_keyboard_display_name(unsigned idx);
/* a listed keyboard's USB ids, where the driver knows them; both 0
 * where it does not */
void input_config_set_keyboard_ids(unsigned idx, uint16_t vid, uint16_t pid);
uint16_t input_config_get_keyboard_vid(unsigned idx);
uint16_t input_config_get_keyboard_pid(unsigned idx);

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
 * Set the physical location of the device in the specified port
 *
 * A NULL or empty location clears the stored one, so that a port
 * whose device reports no location cannot inherit the location of
 * whatever occupied it before.
 *
 * @param port
 * The port of the device to be assigned to
 * @param phys
 * The physical location to set the given port to.
 */
void input_config_set_device_phys(unsigned port, const char *phys);

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

/**
 * input_config_sanitize_joypad_indices:
 *
 * Restores the invariant that settings->uints.input_joypad_index[]
 * is a permutation of [0, MAX_USERS): every player maps to a distinct
 * pad index, and every entry is in range.
 *
 * Entries are examined in ascending player order and the first
 * claimant of an index keeps it, so repairing a duplicate never takes
 * a pad away from a lower numbered player.
 *
 * Returns true if the mapping had to be corrected.
 **/
bool input_config_sanitize_joypad_indices(void);

unsigned *input_config_get_device_ptr(unsigned port);

unsigned input_config_get_device(unsigned port);

/* Get input_device_info */
const char *input_config_get_device_name(unsigned port);
const char *input_config_get_device_display_name(unsigned port);
const char *input_config_get_mouse_display_name(unsigned port);
const char *input_config_get_device_config_name(unsigned port);
const char *input_config_get_device_joypad_driver(unsigned port);
const char *input_config_get_device_phys(unsigned port);

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

#ifdef ANDROID
/**
 * android_keyboard_start:
 * @buffer_ptr               : Pointer to the keyboard line buffer.
 * @size_ptr                 : Pointer to the keyboard line size.
 * @ptr_ptr                  : Pointer to the keyboard line cursor.
 * @label                    : Hint shown on the keyboard, or NULL.
 * @cb                       : Line complete callback function.
 * @userdata                 : Userdata passed to the callback.
 *
 * Raises the native Android (IME) keyboard for menu text entry, made to
 * mirror the iOS ios_keyboard_* hooks. Swaps the custom on-screen keyboard
 * for the system soft keyboard, enabling paste and password managers.
 * Implemented in input/drivers/android_input.c.
 *
 * Returns: true if the keyboard was shown.
 **/
bool android_keyboard_start(char **buffer_ptr, size_t *size_ptr,
      size_t *ptr_ptr, const char *label,
      input_keyboard_line_complete_t cb, void *userdata);
bool android_keyboard_active(void);
void android_keyboard_end(void);
#endif

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

/* For Information > Input Information: whether @user has a Network
 * RetroPad listening, the port it listens on, and the address of the
 * device last heard from it (@heard false while there has been none). */
bool input_remote_info(unsigned user, unsigned *port,
      uint32_t *address, bool *heard);
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
      const struct retro_keybind *auto_bind,
      const struct input_bind_label *label,
      const struct input_bind_label *auto_label, size_t len);

size_t input_config_get_bind_string_joyaxis(
      bool input_descriptor_label_show,
      char *s, const char *prefix,
      const struct retro_keybind *bind,
      const struct input_bind_label *label, size_t len);

size_t input_config_get_bind_string_joykey(
      bool input_descriptor_label_show,
      char *s, const char *prefix,
      const struct retro_keybind *bind,
      const struct input_bind_label *label, size_t len);

bool input_key_pressed(int key, bool keyboard_pressed);

bool input_set_rumble_state(unsigned port,
      enum retro_rumble_effect effect, uint16_t strength);

/* The device registry (input_registry.h): which controllers there are
 * and which have been here before. It mirrors what the joypad drivers
 * report; nothing is assigned from it yet. Main thread only. */
const struct input_registry *input_driver_get_registry(void);

/* A joypad driver's connect or disconnect has been applied to @slot. */
void input_driver_registry_connect(unsigned slot, const char *provider,
      const char *name, const char *phys, uint16_t vid, uint16_t pid);
void input_driver_registry_disconnect(unsigned slot);

/* The joypad driver is about to start over and report its controllers
 * again. If it reports the ones it had, each goes back to the port it
 * was on. */
void input_driver_registry_restart(void);

/* Read controllers through a snapshot taken once a poll, in place of
 * calls into the joypad driver, whatever the driver. For the harness:
 * drivers are switched over one at a time as each is checked. */
void input_driver_set_snapshot_bridge(bool on);

/* The pad index to write to the config file for @port: what the user
 * configured, which is not what the setting holds while controllers
 * have been put back on their ports after a driver restart. */
unsigned input_config_get_saved_joypad_index(unsigned port);

/* Write the rumble strengths a core's calls left this frame. Main
 * thread, once the core has run. */
void input_driver_flush_rumble(void);

/* Stop every motor now and drop what was waiting to be written. */
void input_driver_stop_rumble(void);

bool input_set_rumble_gain(unsigned gain);

float input_get_sensor_state(unsigned port, unsigned id);

void input_sensor_start_rest_capture(void);

bool input_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate);

/* Core-facing sensor callbacks that track core enable/disable state */
bool input_core_set_sensor_state(unsigned port,
      enum retro_sensor_action action, unsigned rate);
float input_core_get_sensor_state(unsigned port, unsigned id);

void *input_driver_init_wrap(input_driver_t *input, const char *name);

const struct retro_keybind *input_config_get_bind_auto(unsigned port, unsigned id);

void input_config_reset_autoconfig_binds(unsigned port);

const input_sensor_map_t *input_config_get_sensor_map(unsigned port);

void input_config_reset(void);

const char *joypad_driver_name(unsigned i);

void joypad_driver_reinit(void *data, const char *joypad_driver_name);

#ifdef HAVE_COMMAND
/* See command_generation in input_driver_state_t. */
unsigned input_driver_command_generation(void);

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
void bsv_movie_frame_rewind(input_driver_state_t *input_st);

/* The replay is over: said by the code that reads it, which is handed
 * the replay and not the input state. */
void bsv_movie_set_end(void);
/* @checkpoint_interval and @checkpoint_deserialize are the two replay
 * settings, handed over by the run loop, which has them. */
void bsv_movie_next_frame(input_driver_state_t *input_st,
      unsigned checkpoint_interval, bool checkpoint_deserialize);
bool bsv_movie_read_next_events(bsv_movie_t *handle, replay_checkpoint_behavior checkpoint_behavior, bool end_movie_on_eof);
bool bsv_movie_reset_playback(bsv_movie_t *handle);
bool bsv_movie_reset_recording(bsv_movie_t *handle);
void bsv_movie_finish_rewind(input_driver_state_t *input_st);
void bsv_movie_deinit(input_driver_state_t *input_st);
void bsv_movie_deinit_full(input_driver_state_t *input_st);
void bsv_movie_enqueue(input_driver_state_t *input_st, bsv_movie_t *state, enum bsv_flags flags);
void bsv_movie_dequeue_next(input_driver_state_t *input_st);

bool movie_commit_checkpoint(input_driver_state_t *input_st);
bool movie_skip_to_prev_checkpoint(input_driver_state_t *input_st);
bool movie_skip_to_next_checkpoint(input_driver_state_t *input_st);
bool movie_seek_to_frame(input_driver_state_t *input_st, int64_t frame);
bool movie_start_playback(input_driver_state_t *input_st, char *path);

/* True while a playback-start task is pending, i.e. until its
 * callback has installed the replay handle. */
bool movie_playback_start_in_progress(void *data);
/* The identifier of the replay the last playback start installed, 0
 * if it installed none; meaningful once the start is no longer in
 * progress.  Read it there: the run loop moves the new handle out of
 * bsv_movie_state_next_handle on the following frame. */
int64_t movie_playback_start_identifier(void);
bool movie_start_record(input_driver_state_t *input_st, char *path);
bool movie_stop_playback(input_driver_state_t *input_st);
bool movie_stop_record(input_driver_state_t *input_st);
bool movie_stop(input_driver_state_t *input_st);

size_t replay_get_serialize_size(input_driver_state_t *input_st);
bool replay_get_serialized_data(input_driver_state_t *input_st, void* buffer);
bool replay_set_serialized_data(input_driver_state_t *input_st, void* buffer);
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
extern input_driver_t input_sdl1;
extern input_driver_t input_sdl2;
extern input_driver_t input_sdl3;
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
extern input_device_driver_t sdl1_joypad; /** SDL1. @see sdl1_joypad.c */
extern input_device_driver_t sdl2_joypad; /** SDL2. @see sdl2_joypad.c */
extern input_device_driver_t sdl_dingux_joypad;
extern input_device_driver_t sdl3_joypad; /** SDL3. @see sdl3_joypad.c */
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
extern input_device_driver_t winraw_joypad;
extern input_device_driver_t test_joypad;

#ifdef HAVE_HID
extern hid_driver_t iohidmanager_hid;
extern hid_driver_t btstack_hid;
extern hid_driver_t libusb_hid;
extern hid_driver_t wiiusb_hid;
extern hid_driver_t gekko_hid;
extern hid_driver_t wiiu_hid;
#endif /* HAVE_HID */

extern retro_keybind_set input_config_binds[MAX_USERS];
extern retro_keybind_set input_autoconf_binds[MAX_USERS];
extern input_bind_label_set input_config_bind_labels[MAX_USERS];
extern input_bind_label_set input_autoconf_bind_labels[MAX_USERS];

RETRO_END_DECLS

#endif /* __INPUT_DRIVER__H */

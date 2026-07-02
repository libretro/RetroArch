/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024 - RetroArch
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __INPUT_DSU__H
#define __INPUT_DSU__H

#include <stdint.h>
#include <stddef.h>
#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>

#include "input_defines.h"

RETRO_BEGIN_DECLS

/* ---- Protocol constants ---- */

#define DSU_MAGIC_CLIENT     "DSUC"
#define DSU_MAGIC_SERVER     "DSUS"  /* Server responses use this */
#define DSU_PROTOCOL_VERSION 1001
#define DSU_DEFAULT_PORT     26760
#define DSU_MAX_CONTROLLERS  MAX_USERS
#define DSU_HEADER_SIZE      20  /* 16-byte header + 4-byte message type */
#define DSU_MAX_PACKET_SIZE  2048

/* ---- Message types ---- */

#define DSU_MSG_VERSION      0x100000
#define DSU_MSG_CONTROLLER   0x100001
#define DSU_MSG_DATA         0x100002
#define DSU_MSG_MOTOR_INFO   0x110001
#define DSU_MSG_RUMBLE       0x110002
#define DSU_MSG_KEYBOARD     0x120000
#define DSU_MSG_MOUSE        0x120001
#define DSU_MSG_COMMAND      0x130000
#define DSU_MSG_STATE        0x140000
#define DSU_MSG_STREAM_START 0x150000
#define DSU_MSG_STREAM_STOP  0x150001
#define DSU_MSG_STREAM_STATUS 0x150002
#define DSU_MSG_AUX_STREAM_START 0x150100
#define DSU_MSG_AUX_STREAM_STOP  0x150101

/* ---- Playlist query message types ---- */
#define DSU_MSG_PLAYLIST_LIST_REQUEST     0x160000
#define DSU_MSG_PLAYLIST_CONTENTS_REQUEST 0x160001
#define DSU_MSG_PLAYLIST_LIST             0x160002
#define DSU_MSG_PLAYLIST_CONTENTS         0x160003

/* ---- Contentless core message types ---- */
#define DSU_MSG_CONTENTLESS_CORE_LIST_REQUEST 0x170000
#define DSU_MSG_CONTENTLESS_CORE_LIST         0x170001

/* ---- Connection states ---- */

#define DSU_CONN_NONE        0
#define DSU_CONN_RESERVED    1
#define DSU_CONN_CONNECTED   2

/* ---- Model types ---- */

#define DSU_MODEL_NA           0
#define DSU_MODEL_PARTIAL_GYRO 1
#define DSU_MODEL_FULL_GYRO    2

/* ---- Connection types ---- */

#define DSU_CONNTYPE_NA        0
#define DSU_CONNTYPE_USB       1
#define DSU_CONNTYPE_BT        2

/* ---- Battery values ---- */

#define DSU_BATTERY_NA         0x00
#define DSU_BATTERY_DYING      0x01
#define DSU_BATTERY_LOW        0x02
#define DSU_BATTERY_MEDIUM     0x03
#define DSU_BATTERY_HIGH       0x04
#define DSU_BATTERY_FULL       0x05
#define DSU_BATTERY_CHARGING   0xEE
#define DSU_BATTERY_CHARGED    0xEF

/* ---- Subscription flags ---- */

#define DSU_SUB_ALL_CONTROLLERS 0x00
#define DSU_SUB_SLOT_BASED      0x01
#define DSU_SUB_MAC_BASED       0x02

/* ---- Data type flags (byte 1) ---- */

#define DSU_DATA_GAMEPAD        (1 << 0)
#define DSU_DATA_MOTION         (1 << 1)
#define DSU_DATA_TOUCH          (1 << 2)
#define DSU_DATA_KEYBOARD       (1 << 3)
#define DSU_DATA_MOUSE          (1 << 4)
#define DSU_DATA_PAD            (1 << 5)
#define DSU_DATA_PAD_MOTION     (1 << 6)
#define DSU_DATA_PAD_BUTTONS    (1 << 7)

/* ---- DSU button bitmasks (byte 16 - Buttons1) ---- */

#define DSU_BTN1_SHARE         (1 << 0)
#define DSU_BTN1_L3            (1 << 1)
#define DSU_BTN1_R3            (1 << 2)
#define DSU_BTN1_OPTIONS       (1 << 3)
#define DSU_BTN1_DPAD_UP       (1 << 4)
#define DSU_BTN1_DPAD_RIGHT    (1 << 5)
#define DSU_BTN1_DPAD_DOWN     (1 << 6)
#define DSU_BTN1_DPAD_LEFT     (1 << 7)

/* ---- DSU button bitmasks (byte 17 - Buttons2) ---- */

#define DSU_BTN2_L2            (1 << 0)
#define DSU_BTN2_R2            (1 << 1)
#define DSU_BTN2_L1            (1 << 2)
#define DSU_BTN2_R1            (1 << 3)
#define DSU_BTN2_X             (1 << 4) /* Triangle/North */
#define DSU_BTN2_A             (1 << 5) /* Circle/East */
#define DSU_BTN2_B             (1 << 6) /* Cross/South */
#define DSU_BTN2_Y             (1 << 7) /* Square/West */

/* ---- Conversion constant: degrees to radians ---- */
#define DSU_DEG_TO_RAD         0.01745329251994329577f  /* PI / 180 */

/* ---- Timeouts ---- */
#define DSU_CLIENT_TIMEOUT_US  5000000  /* 5 seconds in microseconds */
#define DSU_RUMBLE_TIMEOUT_US  5000000  /* 5 seconds */
#define DSU_RUMBLE_RESEND_MS   200      /* resend rumble every 200ms */

/* ---- CRC32 polynomial ---- */
#define DSU_CRC32_POLY         0xEDB88320

/* ---- Extension helpers ---- */
#define DSU_KEYBOARD_WORDS   ((RETROK_LAST + 31) / 32)

/* ---- Packed structures ---- */

#ifdef _MSC_VER
#pragma pack(push, 1)
#define DSU_PACKED
#else
#define DSU_PACKED __attribute__((packed))
#endif

typedef struct DSU_PACKED
{
   char     magic[4];        /* "DSUC" or "DSUS" */
   uint16_t version;         /* Protocol version (1001) */
   uint16_t length;          /* Payload length (after header byte 16) */
   uint32_t crc32;           /* CRC32 of entire packet (this field zeroed) */
   uint32_t id;              /* Unique session ID */
   uint32_t msg_type;        /* Message type */
} dsu_header_t;

typedef struct DSU_PACKED
{
   uint8_t  slot;            /* Controller slot 0-3 */
   uint8_t  state;           /* 0=none, 1=reserved, 2=connected */
   uint8_t  model;           /* 0=N/A, 1=partial gyro, 2=full gyro */
   uint8_t  conn_type;       /* 0=N/A, 1=USB, 2=BT */
   uint8_t  mac[6];          /* MAC address */
   uint8_t  battery;         /* Battery status */
} dsu_shared_response_t;

typedef struct DSU_PACKED
{
   dsu_shared_response_t info;
   uint8_t  connected;       /* 0=disconnected, 1=connected */
   uint32_t packet_number;
   /* Digital buttons */
   uint8_t  buttons1;
   uint8_t  buttons2;
   uint8_t  home;
   uint8_t  touch_btn;
   /* Analog sticks */
   uint8_t  lx;              /* 0-255, center=128 */
   uint8_t  ly;              /* 0-255, center=128, 0=down, 255=up */
   uint8_t  rx;
   uint8_t  ry;
   /* Analog buttons (pressure-sensitive) */
   uint8_t  analog_dpad_left;
   uint8_t  analog_dpad_down;
   uint8_t  analog_dpad_right;
   uint8_t  analog_dpad_up;
   uint8_t  analog_y;        /* Triangle */
   uint8_t  analog_b;        /* Circle */
   uint8_t  analog_a;        /* Cross */
   uint8_t  analog_x;        /* Square */
   uint8_t  analog_r1;
   uint8_t  analog_l1;
   uint8_t  analog_r2;
   uint8_t  analog_l2;
   /* Touch */
   uint8_t  touch1_active;
   uint8_t  touch1_id;
   uint16_t touch1_x;
   uint16_t touch1_y;
   uint8_t  touch2_active;
   uint8_t  touch2_id;
   uint16_t touch2_x;
   uint16_t touch2_y;
   /* Motion */
   uint64_t motion_timestamp; /* microseconds */
   float    accel_x;          /* in g */
   float    accel_y;
   float    accel_z;
   float    gyro_pitch;       /* in deg/s */
   float    gyro_yaw;
   float    gyro_roll;
} dsu_controller_data_t;

typedef struct DSU_PACKED
{
   uint8_t  flags;           /* 0x00=all, 0x01=slot, 0x02=MAC */
   uint8_t  data_type_flags;
   uint8_t  slot;
   uint8_t  mac[6];
} dsu_subscribe_request_t;

typedef struct DSU_PACKED
{
   int32_t  port_count;
   uint8_t  slot_indices[DSU_MAX_CONTROLLERS];
} dsu_controller_info_request_t;

typedef struct DSU_PACKED
{
   dsu_shared_response_t info;
   uint8_t  padding;
} dsu_controller_info_response_t;

typedef struct DSU_PACKED
{
   uint16_t max_version;
} dsu_version_response_t;

typedef struct DSU_PACKED
{
   uint8_t  flags;
   uint8_t  slot;
   uint8_t  mac[6];
   uint8_t  motor_id;
   uint8_t  intensity;
} dsu_rumble_t;

typedef struct DSU_PACKED
{
   dsu_shared_response_t info;
   uint8_t  motor_count;
} dsu_motor_info_response_t;

/* Command structure for remote game launch (0x130000) */
typedef struct DSU_PACKED
{
   uint32_t cmd_type;        /* 1 = launch game */
   char     content_path[256]; /* Path to content file */
   char     core_name[256];    /* Core to use (optional, empty = auto) */
} dsu_command_t;

/* Stream request structure (0x150000) */
typedef struct DSU_PACKED
{
   uint32_t stream_type;     /* 0=OBS, 1=RTSP, 2=built-in */
   char     url[256];        /* Target URL/path */
   uint32_t bitrate;         /* Video bitrate in kbps */
   uint16_t width;           /* Resolution width */
   uint16_t height;          /* Resolution height */
   uint8_t  fps;             /* Target framerate */
} dsu_stream_request_t;

/* Stream status structure (0x150002), 272 bytes on the wire.
 * Sent by RetroArch to the DSU client in response to STREAM_START /
 * STREAM_STOP / AUX_STREAM_START / AUX_STREAM_STOP, so third-party apps
 * know what stream is active and where to tune in (URL for Local/Custom,
 * brand URL for Twitch/YouTube/Facebook). */
typedef struct DSU_PACKED
{
   uint32_t state;           /* 0=stopped, 1=active, 2=error */
   uint32_t error_code;      /* Error code if state=2 */
   uint32_t stream_type;     /* Current stream type */
   uint8_t  screen_id;       /* 0=main stream, 1-4=aux screen */
   uint8_t  reserved[3];
   char     url[256];        /* Active stream URL, null-terminated */
} dsu_stream_status_t;

/* Auxiliary screen stream request (0x150100) */
typedef struct DSU_PACKED
{
   uint8_t  screen_id;       /* 0=main, 1=aux1, 2=aux2, etc. */
   uint32_t stream_type;     /* 0=OBS, 1=RTSP, 2=built-in */
   char     url[256];        /* Target URL/path */
   uint32_t bitrate;         /* Video bitrate in kbps */
   uint16_t width;           /* Resolution width */
   uint16_t height;          /* Resolution height */
   uint8_t  fps;             /* Target framerate */
} dsu_aux_stream_request_t;

#ifdef _MSC_VER
#pragma pack(pop)
#endif
#undef DSU_PACKED

/* ---- Runtime state (per DSU controller) ---- */

typedef struct
{
   bool     connected;
   uint8_t  slot;
   uint8_t  model;
   uint8_t  conn_type;
   uint8_t  mac[6];
   uint8_t  battery;
   int8_t   attached_port;
   char     server_address[64];
   uint16_t server_port;

   /* Buttons (RetroArch format after conversion) */
   uint16_t buttons;
   int16_t  analog_lx;
   int16_t  analog_ly;
   int16_t  analog_rx;
   int16_t  analog_ry;
   uint8_t  l2_analog;
   uint8_t  r2_analog;

   /* Sensors */
   float    accel[3];        /* in g (same as RetroArch) */
   float    gyro[3];         /* converted to rad/s for RetroArch */

   /* Touch */
   bool     touch1_active;
   uint8_t  touch1_id;
   int16_t  touch1_x;
   int16_t  touch1_y;
   bool     touch2_active;
   uint8_t  touch2_id;
   int16_t  touch2_x;
   int16_t  touch2_y;

   /* Home/PS button and touch button */
   bool     home;
   bool     touch_btn;

   /* Keyboard extension */
   uint32_t keyboard_bits[DSU_KEYBOARD_WORDS];
   uint16_t keyboard_mod;
   uint32_t keyboard_character;

   /* Mouse extension */
   int16_t  mouse_delta_x;
   int16_t  mouse_delta_y;
   int16_t  mouse_wheel_x;
   int16_t  mouse_wheel_y;
   int16_t  mouse_x;
   int16_t  mouse_y;
   uint16_t mouse_buttons;

   /* Timestamp */
   uint64_t motion_timestamp;
   uint32_t packet_number;
} dsu_controller_state_t;

/* ---- DSU client state ---- */

typedef struct
{
   bool     enabled;
   bool     client_enabled;

   int      socket_fd;
   uint32_t client_id;
   uint32_t packet_counter;

   /* Subscription tracking */
   int64_t  last_request_time;
   bool     subscribed[DSU_MAX_CONTROLLERS];

   /* Receive buffer */
   uint8_t  recv_buf[DSU_MAX_PACKET_SIZE];

   dsu_controller_state_t controllers[DSU_MAX_CONTROLLERS];

   /* Mutex for protecting controller state access */
   void    *state_lock;

   /* Per-player DSU server IP/port (overrides global if set) */
   char     player_server_address[MAX_USERS][64];
   uint16_t player_server_port[MAX_USERS];

   /* Controller-to-port mapping: port_map[ra_port] = dsu_slot (-1=none) */
   int8_t   port_map[MAX_USERS];

   /* Per-player mode flags (copied from settings at init/update) */
   bool     player_addon[MAX_USERS];
   bool     player_addon_attached[MAX_USERS];
   bool     player_accel[MAX_USERS];
   bool     player_gyro[MAX_USERS];
   bool     player_touch[MAX_USERS];
   bool     player_keyboard[MAX_USERS];
   bool     player_mouse[MAX_USERS];
   bool     player_gamepad[MAX_USERS];
   bool     player_broadcast_state[MAX_USERS];
   bool     player_allow_remote_commands[MAX_USERS];
   bool     player_allow_stream_control[MAX_USERS];
   bool     player_allow_aux_streaming[MAX_USERS];

   /* Rumble state for client mode */
   int64_t  rumble_last_sent[DSU_MAX_CONTROLLERS];

   /* Track if remapping has been triggered for each slot */
   bool     slot_remapped[DSU_MAX_CONTROLLERS];

   /* One-time initial remap done after first controller connects */
   bool     initial_remap_done;
} dsu_state_t;

/* ---- Public API ---- */

uint32_t dsu_crc32(const uint8_t *data, size_t len);

bool dsu_client_init(dsu_state_t *dsu);
void dsu_client_deinit(dsu_state_t *dsu);
void dsu_client_poll(dsu_state_t *dsu);

/* Server functions removed - client mode only */

void dsu_send_rumble(dsu_state_t *dsu, unsigned slot,
      uint8_t motor_id, uint8_t intensity);
void dsu_send_state(dsu_state_t *dsu, const char *game_name,
      const char *platform_name, const char *core_name, uint8_t state_flags);

/* Build packets */
size_t dsu_build_version_request(uint8_t *buf, size_t buf_len, uint32_t id);
size_t dsu_build_controller_info_request(uint8_t *buf, size_t buf_len,
      uint32_t id, int port_count, const uint8_t *slot_indices);
size_t dsu_build_subscribe_request(uint8_t *buf, size_t buf_len,
      uint32_t id, uint8_t flags, uint8_t data_type_flags, uint8_t slot, const uint8_t *mac);

/* Parse incoming controller data into runtime state */
bool dsu_parse_controller_data(const uint8_t *payload, size_t len,
      dsu_controller_state_t *out);

/* Sensor getters (for input_driver integration) */
float dsu_get_sensor(const dsu_state_t *dsu, unsigned port, unsigned id);
bool  dsu_has_sensor(const dsu_state_t *dsu, unsigned port);

/* Dynamic slot remapping based on connected controllers */
void dsu_remap_ports_to_connected_slots(dsu_state_t *dsu);

/* Button/analog getters */
uint16_t dsu_get_buttons(const dsu_state_t *dsu, unsigned port);
bool     dsu_get_home(const dsu_state_t *dsu, unsigned port);
int16_t  dsu_get_analog(const dsu_state_t *dsu, unsigned port,
      unsigned index, unsigned id);

/* Touch getters */
bool    dsu_get_touch_active(const dsu_state_t *dsu, unsigned port, unsigned touch_index);
int16_t dsu_get_touch_x(const dsu_state_t *dsu, unsigned port, unsigned touch_index);
int16_t dsu_get_touch_y(const dsu_state_t *dsu, unsigned port, unsigned touch_index);

/* Per-player mode helpers */
/* Returns true if port has a DSU slot assigned and is in full gamepad mode */
bool    dsu_port_is_fullpad(const dsu_state_t *dsu, unsigned port);
/* Returns true if port has a DSU slot and the given sensor type is enabled */
bool    dsu_port_has_accel(const dsu_state_t *dsu, unsigned port);
bool    dsu_port_has_gyro(const dsu_state_t *dsu, unsigned port);
bool    dsu_port_has_touch(const dsu_state_t *dsu, unsigned port);
bool    dsu_port_has_keyboard(const dsu_state_t *dsu, unsigned port);
bool    dsu_port_has_mouse(const dsu_state_t *dsu, unsigned port);

/* Keyboard integration */
bool    dsu_get_keyboard_key(const dsu_state_t *dsu, unsigned port, unsigned key);

/* Pointer integration (for touch/mouse mode) */
int16_t dsu_get_pointer_x(const dsu_state_t *dsu, unsigned port, unsigned idx);
int16_t dsu_get_pointer_y(const dsu_state_t *dsu, unsigned port, unsigned idx);
int16_t dsu_get_pointer_pressed(const dsu_state_t *dsu, unsigned port, unsigned idx);
int16_t dsu_get_pointer_count(const dsu_state_t *dsu, unsigned port);

/* Mouse integration (for mouse mode) */
void   dsu_get_mouse_delta(const dsu_state_t *dsu, unsigned port, int *delta_x, int *delta_y);
bool   dsu_get_mouse_button(const dsu_state_t *dsu, unsigned port, unsigned btn);

void   input_dsu_broadcast_current_state(void);

/* Remote command handling (game launch) */
void   dsu_handle_command(const char *source_address, uint16_t source_port,
      const dsu_command_t *cmd);

/* Stream handling */
void   dsu_handle_stream_start(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      const dsu_stream_request_t *req);
void   dsu_handle_stream_stop(dsu_state_t *dsu, const char *source_address, uint16_t source_port);
void   dsu_handle_aux_stream_start(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      const dsu_aux_stream_request_t *req);
void   dsu_handle_aux_stream_stop(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      uint8_t screen_id);
void   dsu_broadcast_stream_status_to_all(dsu_state_t *dsu, uint32_t state,
      uint32_t error_code, uint32_t stream_type, uint8_t screen_id, const char *url);
void   dsu_broadcast_aux_stream_status(dsu_state_t *dsu, unsigned player,
      uint32_t state, uint32_t error_code, uint32_t stream_type, const char *url);

/* Playlist query handling */
void   dsu_send_playlist_list(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      uint32_t id, uint8_t page);
void   dsu_send_playlist_contents(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      uint32_t id, const char *playlist_path, uint8_t page);

/* Contentless core handling */
void   dsu_send_contentless_core_list(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      uint32_t id, uint8_t page);

RETRO_END_DECLS

#endif /* __INPUT_DSU__H */

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Gregor Richards
 *  Copyright (C) 2021-2022 - Roberto V. Rampim
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

#ifndef __RARCH_NETPLAY_PRIVATE_H
#define __RARCH_NETPLAY_PRIVATE_H

#include "netplay.h"
#include "netplay_protocol.h"

#include <libretro.h>

#include <streams/trans_stream.h>

#include "../../retroarch_types.h"

#ifndef VITA
#define RARCH_DEFAULT_PORT   55435
#else
#define RARCH_DEFAULT_PORT   19492
#endif
#define RARCH_DISCOVERY_PORT 55435
#define RARCH_DEFAULT_NICK   "Anonymous"

#define NETPLAY_PASS_LEN      128
#define NETPLAY_PASS_HASH_LEN 64 /* length of a SHA-256 hash */

#define MAX_SERVER_STALL_TIME_USEC (5*1000*1000)
#define MAX_CLIENT_STALL_TIME_USEC (10*1000*1000)
#define CATCH_UP_CHECK_TIME_USEC   (500*1000)
#define MAX_RETRIES                16
#define RETRY_MS                   500
#define MAX_INPUT_DEVICES          16

/* We allow only 32 clients to fit into a 32-bit bitmap */
#define MAX_CLIENTS 32

/* Because the callback keyboard reverses some assumptions, when the keyboard
 * callbacks are in use, we assign a pseudodevice for it */
#define RETRO_DEVICE_NETPLAY_KEYBOARD RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_KEYBOARD, 65535)

#define NETPLAY_MAX_STALL_FRAMES        60
#define NETPLAY_FRAME_RUN_TIME_WINDOW   120
#define NETPLAY_MAX_REQ_STALL_TIME      60
#define NETPLAY_MAX_REQ_STALL_FREQUENCY 120

#define PREV_PTR(x) ((x) == 0 ? netplay->buffer_size - 1 : (x) - 1)
#define NEXT_PTR(x) ((x + 1) % netplay->buffer_size)

/* Quirks mandated by how particular cores save states. This is distilled from
 * the larger set of quirks that the quirks environment can communicate. */
#define NETPLAY_QUIRK_INITIALIZATION     (1 << 0)
#define NETPLAY_QUIRK_ENDIAN_DEPENDENT   (1 << 1)
#define NETPLAY_QUIRK_PLATFORM_DEPENDENT (1 << 2)

/* Compression protocols supported */
#define NETPLAY_COMPRESSION_ZLIB (1<<0)
#if HAVE_ZLIB
#define NETPLAY_COMPRESSION_SUPPORTED NETPLAY_COMPRESSION_ZLIB
#else
#define NETPLAY_COMPRESSION_SUPPORTED 0
#endif

/* The keys supported by netplay */
enum netplay_keys
{
   NETPLAY_KEY_UNKNOWN = 0,
#define K(k) NETPLAY_KEY_ ## k,
#define KL(k,l) K(k)
#include "netplay_keys.h"
#undef KL
#undef K
   NETPLAY_KEY_LAST
};

enum netplay_cmd
{
   /* Basic commands */

   /* Acknowlegement response */
   NETPLAY_CMD_ACK            = 0x0000,

   /* Failed acknowlegement response */
   NETPLAY_CMD_NAK            = 0x0001,

   /* Gracefully disconnects from host */
   NETPLAY_CMD_DISCONNECT     = 0x0002,

   /* Input data */
   NETPLAY_CMD_INPUT          = 0x0003,

   /* Non-input data */
   NETPLAY_CMD_NOINPUT        = 0x0004,

   /* Initialization commands */

   /* Inform the other side of our nick (must be first command) */
   NETPLAY_CMD_NICK           = 0x0020,

   /* Give the connection password */
   NETPLAY_CMD_PASSWORD       = 0x0021,

   /* Give core/content info */
   NETPLAY_CMD_INFO           = 0x0022,

   /* Initial synchronization info (frame, sram, player info) */
   NETPLAY_CMD_SYNC           = 0x0023,

   /* Join spectator mode */
   NETPLAY_CMD_SPECTATE       = 0x0024,

   /* Join play mode */
   NETPLAY_CMD_PLAY           = 0x0025,

   /* Report player mode */
   NETPLAY_CMD_MODE           = 0x0026,

   /* Report player mode refused */
   NETPLAY_CMD_MODE_REFUSED   = 0x0027,

   /* Loading and synchronization */

   /* Send the CRC hash of a frame's state */
   NETPLAY_CMD_CRC            = 0x0040,

   /* Request a savestate */
   NETPLAY_CMD_REQUEST_SAVESTATE = 0x0041,

   /* Send a savestate for the client to load */
   NETPLAY_CMD_LOAD_SAVESTATE = 0x0042,

   /* Pauses the game, takes no arguments  */
   NETPLAY_CMD_PAUSE          = 0x0043,

   /* Resumes the game, takes no arguments */
   NETPLAY_CMD_RESUME         = 0x0044,

   /* Request that a client stall because it's running fast */
   NETPLAY_CMD_STALL          = 0x0045,

   /* Request a core reset */
   NETPLAY_CMD_RESET          = 0x0046,

   /* Sends over cheats enabled on client (unsupported) */
   NETPLAY_CMD_CHEATS         = 0x0047,

   /* Misc. commands */

   /* Sends multiple config requests over,
    * See enum netplay_cmd_cfg */
   NETPLAY_CMD_CFG            = 0x0061,

   /* CMD_CFG streamlines sending multiple
      configurations. This acknowledges
      each one individually */
   NETPLAY_CMD_CFG_ACK        = 0x0062,

   /* Chat commands */

   /* Sends a player chat message.
    * The server is responsible for formatting/truncating 
    * the message and relaying it to all playing clients,
    * including the one that sent the message. */
   NETPLAY_CMD_PLAYER_CHAT    = 0x1000,

   /* Ping commands */

   /* Sends a ping command to the server/client.
    * Intended for estimating the latency between these two peers. */
   NETPLAY_CMD_PING_REQUEST   = 0x1100,
   NETPLAY_CMD_PING_RESPONSE  = 0x1101,

   /* Setting commands */

   /* These host settings should be honored by the client,
    * but they are not enforced. */
   NETPLAY_CMD_SETTING_ALLOW_PAUSING        = 0x2000,
   NETPLAY_CMD_SETTING_INPUT_LATENCY_FRAMES = 0x2001
};

#define NETPLAY_CMD_SYNC_BIT_PAUSED  (1U<<31)
#define NETPLAY_CMD_PLAY_BIT_SLAVE   (1U<<31)
#define NETPLAY_CMD_MODE_BIT_YOU     (1U<<31)
#define NETPLAY_CMD_MODE_BIT_PLAYING (1U<<30)
#define NETPLAY_CMD_MODE_BIT_SLAVE   (1U<<29)

/* These are the reasons given for mode changes to be rejected */
enum netplay_cmd_mode_reasons
{
   /* Other/unknown reason */
   NETPLAY_CMD_MODE_REFUSED_REASON_OTHER,

   /* You don't have permission to play */
   NETPLAY_CMD_MODE_REFUSED_REASON_UNPRIVILEGED,

   /* There are no free player slots */
   NETPLAY_CMD_MODE_REFUSED_REASON_NO_SLOTS,

   /* You're changing modes too fast */
   NETPLAY_CMD_MODE_REFUSED_REASON_TOO_FAST,

   /* You requested a particular port but it's not available */
   NETPLAY_CMD_MODE_REFUSED_REASON_NOT_AVAILABLE
};

/* Real preferences for sharing devices */
enum rarch_netplay_share_preference
{
   /* Prefer not to share, shouldn't be set 
      as a sharing mode for an shared device */
   NETPLAY_SHARE_NO_SHARING     = 0x00,

   /* No preference. Only for requests.
      Set if sharing is requested but either
    * digital or analog doesn't have a preference. */
   NETPLAY_SHARE_NO_PREFERENCE  = 0x01,

   /* For digital devices */
   NETPLAY_SHARE_DIGITAL_BITS   = 0x1C,
   NETPLAY_SHARE_DIGITAL_OR     = 0x04,
   NETPLAY_SHARE_DIGITAL_XOR    = 0x08,
   NETPLAY_SHARE_DIGITAL_VOTE   = 0x0C,

   /* For analog devices */
   NETPLAY_SHARE_ANALOG_BITS    = 0xE0,
   NETPLAY_SHARE_ANALOG_MAX     = 0x20,
   NETPLAY_SHARE_ANALOG_AVERAGE = 0x40
};

enum rarch_netplay_stall_reason
{
   NETPLAY_STALL_NONE = 0,

   /* We're so far ahead that we can't read 
      more data without overflowing the buffer */
   NETPLAY_STALL_RUNNING_FAST,

   /* We're in spectator or slave mode 
      and are running ahead at all */
   NETPLAY_STALL_SPECTATOR_WAIT,

   /* Our actual execution is catching up 
      with latency-adjusted input frames */
   NETPLAY_STALL_INPUT_LATENCY,

   /* The server asked us to stall */
   NETPLAY_STALL_SERVER_REQUESTED
};

/* Input state for a particular client-device pair */
typedef struct netplay_input_state
{
   /* The next input state (forming a list) */
   struct netplay_input_state *next;

   /* Whose data is this? */
   uint32_t client_num;

   /* How many words of input data do we have? */
   uint32_t size;

   /* Is this a buffer with real data? */
   bool used;

   /* The input data itself (note: should expand 
      beyond 1 by overallocating). */
   uint32_t data[1];

   /* Warning: No members allowed past this point, 
      due to dynamic resizing. */
} *netplay_input_state_t;

struct delta_frame
{
   /* The resolved input, i.e., what's actually 
      going to the core. One input per device. */
   netplay_input_state_t resolved_input[MAX_INPUT_DEVICES]; /* ptr alignment */

   /* The real input */
   netplay_input_state_t real_input[MAX_INPUT_DEVICES]; /* ptr alignment */

   /* The simulated input. is_real here means the simulation is done, i.e.,
    * it's a real simulation, not real input. */
   netplay_input_state_t simulated_input[MAX_INPUT_DEVICES];

   /* The serialized state of the core at this frame, before input */
   void *state;

   uint32_t frame;

   /* The CRC-32 of the serialized state if we've calculated it, else 0 */
   uint32_t crc;

   /* Have we read local input? */
   bool have_local;

   /* Have we read the real (remote) input? */
   bool have_real[MAX_CLIENTS];

   /* A bit derpy, but this is how we know if the delta
    * has been used at all. */
   bool used;
};

struct socket_buffer
{
   unsigned char *data;
   size_t bufsz;
   size_t start;
   size_t end;
   size_t read;
};

/* We do it like this instead of using sockaddr_storage
   in order to have relay server IPv6 support on platforms
   that do not support IPv6. */
typedef struct netplay_address
{
   /* Can hold an IPv6 address aswell as an IPv4 address in the
      ::ffff:a.b.c.d format. */
   uint8_t addr[16];
} netplay_address_t;

/* Each connection gets a connection struct */
struct netplay_connection
{
   /* Timer used to estimate a connection's latency */
   retro_time_t ping_timer;

   /* Connection's address */
   netplay_address_t addr;

   /* Buffers for sending and receiving data */
   struct socket_buffer send_packet_buffer;
   struct socket_buffer recv_packet_buffer;

   /* What compression does this peer support? */
   uint32_t compression_supported;

   /* Salt associated with password transaction */
   uint32_t salt;

   /* Which netplay protocol is this connection running? */
   uint32_t netplay_protocol;

   /* If the mode is a DELAYED_DISCONNECT or SPECTATOR, 
    * the transmission of the mode change may have to 
    * wait for data to be forwarded.
    * This is the frame to wait for, or 0 if no delay 
    * is active. */
   uint32_t delay_frame;

   /* For the server: When was the last time we requested 
    * this client to stall?
    * For the client: How many frames of stall do we have left? */
   uint32_t stall_frame;

   /* How many times has this connection caused a stall because it's running
      too slow? */
   uint32_t stall_slow;

   /* What latency is this connection running on? 
    * Network latency has limited precision as we estimate it
    * once every pre-frame. */
   int32_t ping;

   /* fd associated with this connection */
   int fd;

   /* Mode of the connection */
   enum rarch_netplay_connection_mode mode;

   /* Is this connection stalling? */
   enum rarch_netplay_stall_reason stall;

   /* Nickname of peer */
   char nick[NETPLAY_NICK_LEN];

   /* Is this connection buffer in use? */
   bool active;

   /* Is this player paused? */
   bool paused;

   /* Is this connection allowed to play (server only)? */
   bool can_play;

   /* Did we request a ping response? */
   bool ping_requested;
};

/* Compression transcoder */
struct compression_transcoder
{
   const struct trans_stream_backend *compression_backend;
   const struct trans_stream_backend *decompression_backend;
   void *compression_stream;
   void *decompression_stream;
};

typedef struct mitm_id
{
   uint32_t magic;
   uint8_t  unique[12];
} mitm_id_t;

#define NETPLAY_MITM_MAX_PENDING 8
struct netplay_mitm_handler
{
   struct
   {
      retro_time_t timeout;
      mitm_id_t id;
      netplay_address_t addr;
      int fd;
      bool has_addr;
   } pending[NETPLAY_MITM_MAX_PENDING];

   mitm_id_t id_buf;
   netplay_address_t addr_buf;
   struct addrinfo *base_addr;
   const struct addrinfo *addr;
   size_t id_recvd;
   size_t addr_recvd;
};

struct netplay_ban_list
{
   netplay_address_t *list;
   size_t size;
   size_t allocated;
};

struct netplay_chat
{
   struct
   {
      uint32_t frames;
      char     nick[NETPLAY_NICK_LEN];
      char     msg[NETPLAY_CHAT_MAX_SIZE];
   } messages[NETPLAY_CHAT_MAX_MESSAGES];
};

struct netplay
{
   /* We stall if we're far enough ahead that we
    * couldn't transparently rewind.
    * To know if we could transparently rewind,
    * we need to know how long running a frame takes.
    * We record that every frame and get a running (window) average. */
   retro_time_t frame_run_time[NETPLAY_FRAME_RUN_TIME_WINDOW];
   retro_time_t frame_run_time_sum;
   retro_time_t frame_run_time_avg;

   /* When did we start falling behind? */
   retro_time_t catch_up_time;
   /* How long have we been stalled? */
   retro_time_t stall_time;

   struct retro_callbacks cbs;

   /* Compression transcoder */
   struct compression_transcoder compress_nil;
   struct compression_transcoder compress_zlib;

   /* MITM session id */
   mitm_id_t mitm_session_id;

   /* Banned addresses */
   struct netplay_ban_list ban_list;

   /* Chat messages */
   struct netplay_chat chat;

   /* MITM connection handler */
   struct netplay_mitm_handler *mitm_handler;

   /* All of our connections */
   struct netplay_connection *connections;

   struct delta_frame *buffer;

   /* A buffer into which to compress frames for transfer */
   uint8_t *zbuffer;

   size_t connections_size;
   size_t buffer_size;
   size_t zbuffer_size;
   /* The size of our packet buffers */
   size_t packet_buffer_size;
   /* Size of savestates */
   size_t state_size;

   /* The frame we're currently inputting */
   size_t self_ptr;
   /* The frame we're currently running, which may be
    * behind the frame we're currently inputting if
    * we're using input latency */
   size_t run_ptr;
   /* The first frame at which some data might be unreliable */
   size_t other_ptr;
   /* Pointer to the first frame for which we're missing
    * the data of at least one connected player excluding ourself.
    * Generally, other_ptr <= unread_ptr <= self_ptr,
    * but unread_ptr can get ahead of self_ptr if the peer
    * is running fast. */
   size_t unread_ptr;
   /* Pointer to the next frame to read from each client */
   size_t read_ptr[MAX_CLIENTS];
   /* Pointer to the next frame to read from the server
    * (as it might not be a player but still synchronizes)
    */
   size_t server_ptr;
   /* A pointer used temporarily for replay. */
   size_t replay_ptr;

   /* Pseudo random seed */
   unsigned long simple_rand_next;

   /* Quirks in the savestate implementation */
   uint32_t quirks;

   /* Our client number */
   uint32_t self_client_num;

   /* Bitmap of clients with input devices */
   uint32_t connected_players;

   /* Bitmap of clients playing in slave mode (should be a subset of
    * connected_players) */
   uint32_t connected_slaves;

   /* For each client, the bitmap of devices they're connected to */
   uint32_t client_devices[MAX_CLIENTS];

   /* For each device, the bitmap of clients connected */
   uint32_t device_clients[MAX_INPUT_DEVICES];

   /* Our own device bitmap */
   uint32_t self_devices;

   /* The device types for every connected device. 
    * We store them and ignore any menu changes,
    * as netplay needs fixed devices. */
   uint32_t config_devices[MAX_INPUT_DEVICES];

   uint32_t self_frame_count;
   uint32_t run_frame_count;
   uint32_t other_frame_count;
   uint32_t unread_frame_count;
   uint32_t read_frame_count[MAX_CLIENTS];
   uint32_t server_frame_count;
   uint32_t replay_frame_count;

   /* Frequency with which to check CRCs */
   uint32_t check_frames;

   /* How far behind did we fall? */
   uint32_t catch_up_behind;

   /* Number of desync operations we're currently performing. 
    * If set, we don't attempt to stay in sync. */
   uint32_t desync;

   /* Host settings */
   int32_t input_latency_frames_min;
   int32_t input_latency_frames_max;

   /* TCP connection for listening (server only) */
   int listen_fd;

   int frame_run_time_ptr;

   /* Latency frames; positive to hide network latency, 
    * negative to hide input latency */
   int input_latency_frames;

   int reannounce;

   int reping;

   /* Our mode and status */
   enum rarch_netplay_connection_mode self_mode;

   /* Are we stalled? */
   enum rarch_netplay_stall_reason stall;

   /* Keyboard mapping (network and host) */
   uint16_t mapping_hton[RETROK_LAST];
   uint16_t mapping_ntoh[NETPLAY_KEY_LAST];

   /* TCP port (only set if serving) */
   uint16_t tcp_port;
   uint16_t ext_tcp_port;

   /* The sharing mode for each device */
   uint8_t device_share_modes[MAX_INPUT_DEVICES];

   /* Our nickname */
   char nick[NETPLAY_NICK_LEN];

   /* Set to true if we have a device that most cores 
    * translate to "up/down" actions, typically a keyboard.
    * We need to keep track of this because with such a device,
    * we need to "fix" the input state to the frame BEFORE a
    * state load, then perform the state load, and the 
    * up/down states will proceed as expected. */
   bool have_updown_device;

   /* Are we the server? */
   bool is_server;

   bool nat_traversal;

   /* Have we checked whether CRCs are valid at all? */
   bool crc_validity_checked;

   /* Are they valid? */
   bool crcs_valid;

   /* Netplay pausing */
   bool local_paused;
   bool remote_paused;

   /* Are we replaying old frames? */
   bool is_replay;

   /* Opposite of stalling, should we be catching up? */
   bool catch_up;

   /* Force a rewind to other_frame_count/other_ptr. 
    * This is for synchronized events, such as restarting 
    * or savestate loading. */
   bool force_rewind;

   /* Force a reset */
   bool force_reset;

   /* Force our state to be sent to all connections */
   bool force_send_savestate;

   /* Have we requested a savestate as a sync point? */
   bool savestate_request_outstanding;

   /* Host settings */
   bool allow_pausing;
};

void video_frame_net(const void *data,
   unsigned width, unsigned height, size_t pitch);
void audio_sample_net(int16_t left, int16_t right);
size_t audio_sample_batch_net(const int16_t *data, size_t frames);
int16_t input_state_net(unsigned port, unsigned device,
   unsigned idx, unsigned id);

/***************************************************************
 * NETPLAY-BUF.C
 **************************************************************/

/**
 * netplay_send
 *
 * Queue the given data for sending.
 */
bool netplay_send(struct socket_buffer *sbuf,
      int sockfd, const void *buf,
      size_t len);

/**
 * netplay_send_flush
 *
 * Flush unsent data in the given socket buffer, 
 * blocking to do so if requested.
 *
 * Returns false only on socket failures, true otherwise.
 */
bool netplay_send_flush(struct socket_buffer *sbuf,
      int sockfd, bool block);

/**
 * netplay_recv
 *
 * Receive buffered or fresh data.
 *
 * Returns number of bytes returned, which may be short, 0, or -1 on error.
 */
ssize_t netplay_recv(struct socket_buffer *sbuf, int sockfd,
      void *buf, size_t len);

/**
 * netplay_recv_reset
 *
 * Reset our recv buffer so that future netplay_recvs 
 * will read the same data again.
 */
void netplay_recv_reset(struct socket_buffer *sbuf);

/**
 * netplay_recv_flush
 *
 * Flush our recv buffer, so a future netplay_recv_reset 
 * will reset to this point.
 */
void netplay_recv_flush(struct socket_buffer *sbuf);

/***************************************************************
 * NETPLAY-DELTA.C
 **************************************************************/

/**
 * netplay_delta_frame_ready
 *
 * Prepares, if possible, a delta frame for input, and reports 
 * whether it is ready.
 *
 * Returns: True if the delta frame is ready for input at 
 * the given frame, false otherwise.
 */
bool netplay_delta_frame_ready(netplay_t *netplay,
      struct delta_frame *delta,
      uint32_t frame);

/***************************************************************
 * NETPLAY-FRONTEND.C
 **************************************************************/

/**
 * input_poll_net
 * @netplay              : pointer to netplay object
 *
 * Poll the network if necessary.
 */
void input_poll_net(netplay_t *netplay);

/***************************************************************
 * NETPLAY-INIT.C
 **************************************************************/

/**
 * netplay_wait_and_init_serialization
 *
 * Try very hard to initialize serialization, simulating 
 * multiple frames if necessary. For quirky cores.
 *
 * Returns true if serialization is now ready, false otherwise.
 */
bool netplay_wait_and_init_serialization(netplay_t *netplay);

/***************************************************************
 * NETPLAY-IO.C
 **************************************************************/

/**
 * netplay_send_cur_input
 *
 * Send the current input frame to a given connection.
 *
 * Returns true if successful, false otherwise.
 */
bool netplay_send_cur_input(netplay_t *netplay,
   struct netplay_connection *connection);

/**
 * netplay_send_raw_cmd
 *
 * Send a raw Netplay command to the given connection.
 *
 * Returns true on success, false on failure.
 */
bool netplay_send_raw_cmd(netplay_t *netplay,
   struct netplay_connection *connection, uint32_t cmd, const void *data,
   size_t size);

/**
 * netplay_send_raw_cmd_all
 *
 * Send a raw Netplay command to all connections, 
 * optionally excluding one
 * (typically the client that the relevant command came from)
 */
void netplay_send_raw_cmd_all(netplay_t *netplay,
   struct netplay_connection *except, uint32_t cmd, const void *data,
   size_t size);

/**
 * netplay_cmd_mode
 *
 * Send a mode change request. As a server, 
 * the request is to ourself, and so honored instantly.
 */
bool netplay_cmd_mode(netplay_t *netplay,
   enum rarch_netplay_connection_mode mode);

/***************************************************************
 * NETPLAY-SYNC.C
 **************************************************************/

/**
 * netplay_load_savestate
 * @netplay              : pointer to netplay object
 * @serial_info          : the savestate being loaded, NULL means
 *                         "load it yourself"
 * @save                 : Whether to save the provided serial_info
 *                         into the frame buffer
 *
 * Inform Netplay of a savestate load and send it to the other side
 **/
void netplay_load_savestate(netplay_t *netplay,
      retro_ctx_serialize_info_t *serial_info, bool save);
#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2017 - Gregor Richards
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

#include <net/net_compat.h>
#include <net/net_natt.h>
#include <features/features_cpu.h>
#include <streams/trans_stream.h>

#include "../../msg_hash.h"
#include "../../verbosity.h"

#define WORDS_PER_INPUT 3 /* Buttons, left stick, right stick */
#define WORDS_PER_FRAME (WORDS_PER_INPUT+2) /* + frameno, playerno */

#define NETPLAY_PROTOCOL_VERSION 4

#define RARCH_DEFAULT_PORT 55435
#define RARCH_DEFAULT_NICK "Anonymous"

#define NETPLAY_NICK_LEN      32
#define NETPLAY_PASS_LEN      128
#define NETPLAY_PASS_HASH_LEN 64 /* length of a SHA-256 hash */

#define MAX_SERVER_STALL_TIME_USEC  (5*1000*1000)
#define MAX_CLIENT_STALL_TIME_USEC  (10*1000*1000)
#define CATCH_UP_CHECK_TIME_USEC    (500*1000)
#define MAX_RETRIES                 16
#define RETRY_MS                    500

#define NETPLAY_MAX_STALL_FRAMES       60
#define NETPLAY_FRAME_RUN_TIME_WINDOW  120
#define NETPLAY_MAX_REQ_STALL_TIME     60
#define NETPLAY_MAX_REQ_STALL_FREQUENCY 120

#define PREV_PTR(x) ((x) == 0 ? netplay->buffer_size - 1 : (x) - 1)
#define NEXT_PTR(x) ((x + 1) % netplay->buffer_size)

/* Quirks mandated by how particular cores save states. This is distilled from
 * the larger set of quirks that the quirks environment can communicate. */
#define NETPLAY_QUIRK_NO_SAVESTATES (1<<0)
#define NETPLAY_QUIRK_NO_TRANSMISSION (1<<1)
#define NETPLAY_QUIRK_INITIALIZATION (1<<2)
#define NETPLAY_QUIRK_ENDIAN_DEPENDENT (1<<3)
#define NETPLAY_QUIRK_PLATFORM_DEPENDENT (1<<4)

/* Mapping of serialization quirks to netplay quirks. */
#define NETPLAY_QUIRK_MAP_UNDERSTOOD \
   (RETRO_SERIALIZATION_QUIRK_INCOMPLETE \
   |RETRO_SERIALIZATION_QUIRK_CORE_VARIABLE_SIZE \
   |RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE \
   |RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION \
   |RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT \
   |RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT)
#define NETPLAY_QUIRK_MAP_NO_SAVESTATES \
   (RETRO_SERIALIZATION_QUIRK_INCOMPLETE)
#define NETPLAY_QUIRK_MAP_NO_TRANSMISSION \
   (RETRO_SERIALIZATION_QUIRK_SINGLE_SESSION)
#define NETPLAY_QUIRK_MAP_INITIALIZATION \
   (RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE)
#define NETPLAY_QUIRK_MAP_ENDIAN_DEPENDENT \
   (RETRO_SERIALIZATION_QUIRK_ENDIAN_DEPENDENT)
#define NETPLAY_QUIRK_MAP_PLATFORM_DEPENDENT \
   (RETRO_SERIALIZATION_QUIRK_PLATFORM_DEPENDENT)

/* Compression protocols supported */
#define NETPLAY_COMPRESSION_ZLIB (1<<0)
#if HAVE_ZLIB
#define NETPLAY_COMPRESSION_SUPPORTED NETPLAY_COMPRESSION_ZLIB
#else
#define NETPLAY_COMPRESSION_SUPPORTED 0
#endif

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

   /* Swap inputs between player 1 and player 2 */
   NETPLAY_CMD_FLIP_PLAYERS   = 0x0060,

   /* Sends multiple config requests over,
    * See enum netplay_cmd_cfg */
   NETPLAY_CMD_CFG            = 0x0061,

   /* CMD_CFG streamlines sending multiple
      configurations. This acknowledges
      each one individually */
   NETPLAY_CMD_CFG_ACK        = 0x0062
};

#define NETPLAY_CMD_INPUT_BIT_SERVER   (1U<<31)
#define NETPLAY_CMD_SYNC_BIT_PAUSED    (1U<<31)
#define NETPLAY_CMD_PLAY_BIT_SLAVE         (1U)
#define NETPLAY_CMD_MODE_BIT_SLAVE     (1U<<18)
#define NETPLAY_CMD_MODE_BIT_PLAYING   (1U<<17)
#define NETPLAY_CMD_MODE_BIT_YOU       (1U<<16)

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
   NETPLAY_CMD_MODE_REFUSED_REASON_TOO_FAST
};

enum rarch_netplay_connection_mode
{
   NETPLAY_CONNECTION_NONE = 0,

   NETPLAY_CONNECTION_DELAYED_DISCONNECT, /* The connection is dead, but data
                                             is still waiting to be forwarded */

   /* Initialization: */
   NETPLAY_CONNECTION_INIT, /* Waiting for header */
   NETPLAY_CONNECTION_PRE_NICK, /* Waiting for nick */
   NETPLAY_CONNECTION_PRE_PASSWORD, /* Waiting for password */
   NETPLAY_CONNECTION_PRE_INFO, /* Waiting for core/content info */
   NETPLAY_CONNECTION_PRE_SYNC, /* Waiting for sync */

   /* Ready: */
   NETPLAY_CONNECTION_CONNECTED, /* Modes above this are connected */
   NETPLAY_CONNECTION_SPECTATING, /* Spectator mode */
   NETPLAY_CONNECTION_SLAVE, /* Playing in slave mode */
   NETPLAY_CONNECTION_PLAYING /* Normal ready state */
};

enum rarch_netplay_stall_reason
{
   NETPLAY_STALL_NONE = 0,

   /* We're so far ahead that we can't read more data without overflowing the
    * buffer */
   NETPLAY_STALL_RUNNING_FAST,

   /* We're in spectator or slave mode and are running ahead at all */
   NETPLAY_STALL_SPECTATOR_WAIT,

   /* Our actual execution is catching up with latency-adjusted input frames */
   NETPLAY_STALL_INPUT_LATENCY,

   /* The server asked us to stall */
   NETPLAY_STALL_SERVER_REQUESTED,

   /* We have no connection and must have one to proceed */
   NETPLAY_STALL_NO_CONNECTION
};

typedef uint32_t netplay_input_state_t[WORDS_PER_INPUT];

struct delta_frame
{
   bool used; /* a bit derpy, but this is how we know if the delta's been used at all */
   uint32_t frame;

   /* The serialized state of the core at this frame, before input */
   void *state;

   /* The CRC-32 of the serialized state if we've calculated it, else 0 */
   uint32_t crc;

   /* The real, simulated and local input. If we're playing, self_state is
    * mirrored to the appropriate real_input_state player. */
   netplay_input_state_t real_input_state[MAX_USERS];
   netplay_input_state_t simulated_input_state[MAX_USERS];
   netplay_input_state_t self_state;

   /* Have we read local input? */
   bool have_local;

   /* Have we read the real (remote) input? */
   bool have_real[MAX_USERS];

   /* Is the current state as of self_frame_count using the real (remote) data? */
   bool used_real[MAX_USERS];
};

struct socket_buffer
{
   unsigned char *data;
   size_t bufsz;
   size_t start, end;
   size_t read;
};

/* Each connection gets a connection struct */
struct netplay_connection
{
   /* Is this connection buffer in use? */
   bool active;

   /* fd associated with this connection */
   int fd;

   /* Address of peer */
   struct sockaddr_storage addr;

   /* Nickname of peer */
   char nick[NETPLAY_NICK_LEN];

   /* Salt associated with password transaction */
   uint32_t salt;

   /* Is this connection allowed to play (server only)? */
   bool can_play;

   /* Buffers for sending and receiving data */
   struct socket_buffer send_packet_buffer, recv_packet_buffer;

   /* Mode of the connection */
   enum rarch_netplay_connection_mode mode;

   /* If the mode is a DELAYED_DISCONNECT or SPECTATOR, the transmission of the
    * mode change may have to wait for data to be forwarded. This is the frame
    * to wait for, or 0 if no delay is active. */
   uint32_t delay_frame;

   /* Player # of connected player */
   uint32_t player;

   /* What compression does this peer support? */
   uint32_t compression_supported;

   /* Is this player paused? */
   bool paused;

   /* Is this connection stalling? */
   enum rarch_netplay_stall_reason stall;
   retro_time_t stall_time;

   /* For the server: When was the last time we requested this client to stall?
    * For the client: How many frames of stall do we have left? */
   uint32_t stall_frame;
};

/* Compression transcoder */
struct compression_transcoder
{
   const struct trans_stream_backend *compression_backend;
   void *compression_stream;
   const struct trans_stream_backend *decompression_backend;
   void *decompression_stream;
};

struct netplay
{
   /* Are we the server? */
   bool is_server;

   /* Are we the connected? */
   bool is_connected;

   /* Our nickname */
   char nick[NETPLAY_NICK_LEN];

   /* TCP connection for listening (server only) */
   int listen_fd;

   /* Our player number */
   uint32_t self_player;

   /* Our mode and status */
   enum rarch_netplay_connection_mode self_mode;

   /* All of our connections */
   struct netplay_connection *connections;
   size_t connections_size;
   struct netplay_connection one_connection; /* Client only */

   /* Bitmap of players with controllers (low bit is player 1) */
   uint32_t connected_players;

   /* Bitmap of players playing in slave mode (should be a subset of
    * connected_players) */
   uint32_t connected_slaves;

   /* Number of desync operations we're currently performing. If set, we don't
    * attempt to stay in sync. */
   uint32_t desync;

   /* Maximum player number */
   uint32_t player_max;

   struct retro_callbacks cbs;

   /* TCP port (only set if serving) */
   uint16_t tcp_port;

   /* NAT traversal info (if NAT traversal is used and serving) */
   bool nat_traversal, nat_traversal_task_oustanding;
   struct natt_status nat_traversal_state;

   struct delta_frame *buffer;
   size_t buffer_size;

   /* Compression transcoder */
   struct compression_transcoder compress_nil,
                                 compress_zlib;

   /* A buffer into which to compress frames for transfer */
   uint8_t *zbuffer;
   size_t zbuffer_size;

   /* The size of our packet buffers */
   size_t packet_buffer_size;

   /* The frame we're currently inputting */
   size_t self_ptr;
   uint32_t self_frame_count;

   /* The frame we're currently running, which may be behind the frame we're
    * currently inputting if we're using input latency */
   size_t run_ptr;
   uint32_t run_frame_count;

   /* The first frame at which some data might be unreliable */
   size_t other_ptr;
   uint32_t other_frame_count;

   /* Pointer to the first frame for which we're missing the data of at least
    * one connected player excluding ourself.
    * Generally, other_ptr <= unread_ptr <= self_ptr, but unread_ptr can get ahead
    * of self_ptr if the peer is running fast. */
   size_t unread_ptr;
   uint32_t unread_frame_count;

   /* Pointer to the next frame to read from each player */
   size_t read_ptr[MAX_USERS];
   uint32_t read_frame_count[MAX_USERS];

   /* Pointer to the next frame to read from the server (as it might not be a
    * player but still synchronizes) */
   size_t server_ptr;
   uint32_t server_frame_count;

   /* A pointer used temporarily for replay. */
   size_t replay_ptr;
   uint32_t replay_frame_count;

   /* Size of savestates */
   size_t state_size;

   /* Are we replaying old frames? */
   bool is_replay;

   /* We don't want to poll several times on a frame. */
   bool can_poll;

   /* Force a rewind to other_frame_count/other_ptr. This is for synchronized
    * events, such as player flipping or savestate loading. */
   bool force_rewind;

   /* Force a reset */
   bool force_reset;

   /* Quirks in the savestate implementation */
   uint64_t quirks;

   /* Force our state to be sent to all connections */
   bool force_send_savestate;

   /* Have we requested a savestate as a sync point? */
   bool savestate_request_outstanding;

   /* A buffer for outgoing input packets. */
   uint32_t input_packet_buffer[2 + WORDS_PER_FRAME];

   /* Our local socket info */
   struct addrinfo *addr;

   /* Counter for timeouts */
   unsigned timeout_cnt;

   /* User flipping
    * Flipping state. If frame >= flip_frame, we apply the flip.
    * If not, we apply the opposite, effectively creating a trigger point. */
   bool flip;
   uint32_t flip_frame;

   /* Netplay pausing */
   bool local_paused;
   bool remote_paused;

   /* If true, never progress without peer input (stateless/rewindless mode) */
   bool stateless_mode;

   /* We stall if we're far enough ahead that we couldn't transparently rewind.
    * To know if we could transparently rewind, we need to know how long
    * running a frame takes. We record that every frame and get a running
    * (window) average */
   retro_time_t frame_run_time[NETPLAY_FRAME_RUN_TIME_WINDOW];
   int frame_run_time_ptr;
   retro_time_t frame_run_time_sum, frame_run_time_avg;

   /* Latency frames and limits */
   unsigned input_latency_frames;

   /* Are we stalled? */
   enum rarch_netplay_stall_reason stall;

   /* How long have we been stalled? */
   retro_time_t stall_time;

   /* Opposite of stalling, should we be catching up? */
   bool catch_up;

   /* When did we start falling behind? */
   retro_time_t catch_up_time;

   /* How far behind did we fall? */
   uint32_t catch_up_behind;

   /* Frequency with which to check CRCs */
   int check_frames;

   /* Have we checked whether CRCs are valid at all? */
   bool crc_validity_checked;

   /* Are they valid? */
   bool crcs_valid;
};


/***************************************************************
 * NETPLAY-BUF.C
 **************************************************************/

/**
 * netplay_init_socket_buffer
 *
 * Initialize a new socket buffer.
 */
bool netplay_init_socket_buffer(struct socket_buffer *sbuf, size_t size);

/**
 * netplay_resize_socket_buffer
 *
 * Resize the given socket_buffer's buffer to the requested size.
 */
bool netplay_resize_socket_buffer(struct socket_buffer *sbuf, size_t newsize);

/**
 * netplay_deinit_socket_buffer
 *
 * Free a socket buffer.
 */
void netplay_deinit_socket_buffer(struct socket_buffer *sbuf);

/**
 * netplay_send
 *
 * Queue the given data for sending.
 */
bool netplay_send(struct socket_buffer *sbuf, int sockfd, const void *buf,
   size_t len);

/**
 * netplay_send_flush
 *
 * Flush unsent data in the given socket buffer, blocking to do so if
 * requested.
 *
 * Returns false only on socket failures, true otherwise.
 */
bool netplay_send_flush(struct socket_buffer *sbuf, int sockfd, bool block);

/**
 * netplay_recv
 *
 * Receive buffered or fresh data.
 *
 * Returns number of bytes returned, which may be short or 0, or -1 on error.
 */
ssize_t netplay_recv(struct socket_buffer *sbuf, int sockfd, void *buf,
   size_t len, bool block);

/**
 * netplay_recv_reset
 *
 * Reset our recv buffer so that future netplay_recvs will read the same data
 * again.
 */
void netplay_recv_reset(struct socket_buffer *sbuf);

/**
 * netplay_recv_flush
 *
 * Flush our recv buffer, so a future netplay_recv_reset will reset to this
 * point.
 */
void netplay_recv_flush(struct socket_buffer *sbuf);


/***************************************************************
 * NETPLAY-DELTA.C
 **************************************************************/

/**
 * netplay_delta_frame_ready
 *
 * Prepares, if possible, a delta frame for input, and reports whether it is
 * ready.
 *
 * Returns: True if the delta frame is ready for input at the given frame,
 * false otherwise.
 */
bool netplay_delta_frame_ready(netplay_t *netplay, struct delta_frame *delta,
   uint32_t frame);

/**
 * netplay_delta_frame_crc
 *
 * Get the CRC for the serialization of this frame.
 */
uint32_t netplay_delta_frame_crc(netplay_t *netplay, struct delta_frame *delta);


/***************************************************************
 * NETPLAY-DISCOVERY.C
 **************************************************************/

/**
 * netplay_lan_ad_server
 *
 * Respond to any LAN ad queries that the netplay server has received.
 */
bool netplay_lan_ad_server(netplay_t *netplay);


/***************************************************************
 * NETPLAY-FRONTEND.C
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

/**
 * input_poll_net
 *
 * Poll the network if necessary.
 */
void input_poll_net(void);

/***************************************************************
 * NETPLAY-HANDSHAKE.C
 **************************************************************/

/**
 * netplay_handshake_init_send
 *
 * Initialize our handshake and send the first part of the handshake protocol.
 */
bool netplay_handshake_init_send(netplay_t *netplay,
   struct netplay_connection *connection);

/**
 * netplay_handshake
 *
 * Data receiver for all handshake states.
 */
bool netplay_handshake(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input);


/***************************************************************
 * NETPLAY-INIT.C
 **************************************************************/

/**
 * netplay_try_init_serialization
 *
 * Try to initialize serialization. For quirky cores.
 *
 * Returns true if serialization is now ready, false otherwise.
 */
bool netplay_try_init_serialization(netplay_t *netplay);

/**
 * netplay_wait_and_init_serialization
 *
 * Try very hard to initialize serialization, simulating multiple frames if
 * necessary. For quirky cores.
 *
 * Returns true if serialization is now ready, false otherwise.
 */
bool netplay_wait_and_init_serialization(netplay_t *netplay);

/**
 * netplay_new:
 * @direct_host          : Netplay host discovered from scanning.
 * @server               : IP address of server.
 * @port                 : Port of server.
 * @stateless_mode       : Shall we run in stateless mode?
 * @check_frames         : Frequency with which to check CRCs.
 * @cb                   : Libretro callbacks.
 * @nat_traversal        : If true, attempt NAT traversal.
 * @nick                 : Nickname of user.
 * @quirks               : Netplay quirks required for this session.
 *
 * Creates a new netplay handle. A NULL server means we're
 * hosting.
 *
 * Returns: new netplay data.
 */
netplay_t *netplay_new(void *direct_host, const char *server, uint16_t port,
   bool stateless_mode, int check_frames,
   const struct retro_callbacks *cb, bool nat_traversal, const char *nick,
   uint64_t quirks);

/**
 * netplay_free
 * @netplay              : pointer to netplay object
 *
 * Frees netplay data/
 */
void netplay_free(netplay_t *netplay);


/***************************************************************
 * NETPLAY-IO.C
 **************************************************************/

/**
 * netplay_hangup:
 *
 * Disconnects an active Netplay connection due to an error
 */
void netplay_hangup(netplay_t *netplay, struct netplay_connection *connection);

/**
 * netplay_delayed_state_change:
 *
 * Handle any pending state changes which are ready as of the beginning of the
 * current frame.
 */
void netplay_delayed_state_change(netplay_t *netplay);

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
 * Send a raw Netplay command to all connections, optionally excluding one
 * (typically the client that the relevant command came from)
 */
void netplay_send_raw_cmd_all(netplay_t *netplay,
   struct netplay_connection *except, uint32_t cmd, const void *data,
   size_t size);

/**
 * netplay_cmd_crc
 *
 * Send a CRC command to all active clients.
 */
bool netplay_cmd_crc(netplay_t *netplay, struct delta_frame *delta);

/**
 * netplay_cmd_request_savestate
 *
 * Send a savestate request command.
 */
bool netplay_cmd_request_savestate(netplay_t *netplay);

/**
 * netplay_cmd_mode
 *
 * Send a mode request command to either play or spectate.
 */
bool netplay_cmd_mode(netplay_t *netplay,
   struct netplay_connection *connection,
   enum rarch_netplay_connection_mode mode);

/**
 * netplay_cmd_stall
 *
 * Send a stall command.
 */
bool netplay_cmd_stall(netplay_t *netplay,
   struct netplay_connection *connection,
   uint32_t frames);

/**
 * netplay_poll_net_input
 *
 * Poll input from the network
 */
int netplay_poll_net_input(netplay_t *netplay, bool block);

/**
 * netplay_handle_slaves
 *
 * Handle any slave connections
 */
void netplay_handle_slaves(netplay_t *netplay);

/**
 * netplay_flip_port
 *
 * Should we flip ports 0 and 1?
 */
bool netplay_flip_port(netplay_t *netplay);

/**
 * netplay_announce_nat_traversal
 *
 * Announce successful NAT traversal.
 */
void netplay_announce_nat_traversal(netplay_t *netplay);

/**
 * netplay_init_nat_traversal
 *
 * Initialize the NAT traversal library and try to open a port
 */
void netplay_init_nat_traversal(netplay_t *netplay);


/***************************************************************
 * NETPLAY-SYNC.C
 **************************************************************/

/**
 * netplay_update_unread_ptr
 *
 * Update the global unread_ptr and unread_frame_count to correspond to the
 * earliest unread frame count of any connected player
 */
void netplay_update_unread_ptr(netplay_t *netplay);

/**
 * netplay_simulate_input
 * @netplay             : pointer to netplay object
 * @sim_ptr             : frame index for which to simulate input
 * @resim               : are we resimulating, or simulating this frame for the
 *                        first time?
 *
 * "Simulate" input by assuming it hasn't changed since the last read input.
 */
void netplay_simulate_input(netplay_t *netplay, size_t sim_ptr, bool resim);

/**
 * netplay_sync_pre_frame
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay synchronization.
 */
bool netplay_sync_pre_frame(netplay_t *netplay);

/**
 * netplay_sync_post_frame
 * @netplay              : pointer to netplay object
 * @stalled              : true if we're currently stalled
 *
 * Post-frame for Netplay synchronization.
 * We check if we have new input and replay from recorded input.
 */
void netplay_sync_post_frame(netplay_t *netplay, bool stalled);

#endif

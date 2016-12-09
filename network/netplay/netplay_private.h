/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
 *  Copyright (C)      2016 - Gregor Richards
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
#include <retro_endianness.h>

#include "../../core.h"
#include "../../msg_hash.h"
#include "../../verbosity.h"

#ifdef ANDROID
#define HAVE_IPV6
#endif

#define WORDS_PER_FRAME 4 /* Allows us to send 128 bits worth of state per frame. */
#define MAX_SPECTATORS 16
#define RARCH_DEFAULT_PORT 55435
#define RARCH_DEFAULT_NICK "Anonymous"

#define NETPLAY_PROTOCOL_VERSION 4

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

   /* Initialization commands */

   /* Inform the other side of our nick (must be first command) */
   NETPLAY_CMD_NICK           = 0x0020,

   /* Initial synchronization info (frame, sram, player info) */
   NETPLAY_CMD_SYNC           = 0x0021,

   /* Join spectator mode */
   NETPLAY_CMD_SPECTATE       = 0x0022,

   /* Join play mode */
   NETPLAY_CMD_PLAY           = 0x0023,

   /* Report player mode */
   NETPLAY_CMD_MODE           = 0x0024,

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

   /* Sends over cheats enabled on client (unsupported) */
   NETPLAY_CMD_CHEATS         = 0x0045,

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

/* These are the configurations sent by NETPLAY_CMD_CFG. */
enum netplay_cmd_cfg
{
   /* Nickname */
   NETPLAY_CFG_NICK           = 0x0001,

   /* input.netplay_client_swap_input */
   NETPLAY_CFG_SWAP_INPUT     = 0x0002,

   /* netplay.delay_frames */
   NETPLAY_CFG_DELAY_FRAMES   = 0x0004,

   /* For more than 2 players */
   NETPLAY_CFG_PLAYER_SLOT    = 0x0008
};

enum rarch_netplay_connection_mode
{
   NETPLAY_CONNECTION_NONE = 0,

   /* Initialization: */
   NETPLAY_CONNECTION_INIT, /* Waiting for header */
   NETPLAY_CONNECTION_PRE_NICK, /* Waiting for nick */
   NETPLAY_CONNECTION_PRE_SYNC, /* Waiting for sync */

   /* Ready: */
   NETPLAY_CONNECTION_CONNECTED, /* Modes above this are connected */
   NETPLAY_CONNECTION_SPECTATING, /* Spectator mode */
   NETPLAY_CONNECTION_PLAYING /* Normal ready state */
};

enum rarch_netplay_stall_reason
{
   NETPLAY_STALL_NONE = 0,
   NETPLAY_STALL_RUNNING_FAST,
   NETPLAY_STALL_NO_CONNECTION
};

struct delta_frame
{
   bool used; /* a bit derpy, but this is how we know if the delta's been used at all */
   uint32_t frame;

   /* The serialized state of the core at this frame, before input */
   void *state;

   /* The CRC-32 of the serialized state if we've calculated it, else 0 */
   uint32_t crc;

   uint32_t real_input_state[WORDS_PER_FRAME - 1];
   uint32_t simulated_input_state[WORDS_PER_FRAME - 1];
   uint32_t self_state[WORDS_PER_FRAME - 1];

   /* Have we read local input? */
   bool have_local;

   /* Have we read the real remote input? */
   bool have_remote;

   /* Is the current state as of self_frame_count using the real remote data? */
   bool used_real;
};

struct socket_buffer
{
   unsigned char *data;
   size_t bufsz;
   size_t start, end;
   size_t read;
};

struct netplay_callbacks {
   bool (*pre_frame) (netplay_t *netplay);
   void (*post_frame)(netplay_t *netplay);
   bool (*info_cb)   (netplay_t *netplay, unsigned frames);
};

/* Each connection gets a connection struct */
struct netplay_connection
{
   /* Is this connection buffer in use? */
   bool active;

   /* fd associated with this connection */
   int fd;

   /* Buffers for sending and receiving data */
   struct socket_buffer send_packet_buffer, recv_packet_buffer;

   /* Mode of the connection */
   enum rarch_netplay_connection_mode mode;

   /* Player # of connected player, or -1 if not a player */
   int player;
};

struct netplay
{
   /* Our nickname */
   char nick[32];

   /* Nickname of peer */
   char other_nick[32];

   /* Address of peer */
   struct sockaddr_storage other_addr;

   /* TCP connection for listening (server only) */
   int listen_fd;

   /* Our connection number */
   uint32_t self_connection_num;

   /* Our mode and status */
   enum rarch_netplay_connection_mode self_mode;

   /* All of our connections */
   struct netplay_connection *connections;
   size_t connections_size;
   struct netplay_connection one_connection; /* Client only */

   /* True if any of our connections are players (i.e., we actually need to do
    * netplay) */
   bool have_player_connections;

   struct retro_callbacks cbs;

   /* TCP port (only set if serving) */
   uint16_t tcp_port;

   /* NAT traversal info (if NAT traversal is used and serving) */
   bool nat_traversal;
   struct natt_status nat_traversal_state;

   /* Which port is governed by netplay (other user)? */
   unsigned port;

   struct delta_frame *buffer;
   size_t buffer_size;

   /* Compression transcoder */
   const struct trans_stream_backend *compression_backend;
   void *compression_stream;
   const struct trans_stream_backend *decompression_backend;
   void *decompression_stream;

   /* A buffer into which to compress frames for transfer */
   uint8_t *zbuffer;
   size_t zbuffer_size;

   /* The size of our packet buffers */
   size_t packet_buffer_size;

   /* Pointer where we are now. */
   size_t self_ptr;
   /* Points to the last reliable state that self ever had. */
   size_t other_ptr;
   /* Pointer to where we are reading.
    * Generally, other_ptr <= read_ptr <= self_ptr. */
   size_t read_ptr;
   /* A pointer used temporarily for replay. */
   size_t replay_ptr;

   size_t state_size;

   /* Are we replaying old frames? */
   bool is_replay;

   /* We don't want to poll several times on a frame. */
   bool can_poll;

   /* Force a rewind to other_frame_count/other_ptr. This is for synchronized
    * events, such as player flipping or savestate loading. */
   bool force_rewind;

   /* Quirks in the savestate implementation */
   uint64_t quirks;

   /* Force our state to be sent to the other side. Used when they request a
    * savestate, to send at the next pre-frame. */
   bool force_send_savestate;

   /* Have we requested a savestate as a sync point? */
   bool savestate_request_outstanding;

   /* A buffer for outgoing input packets. */
   uint32_t input_packet_buffer[2 + WORDS_PER_FRAME];

   /* All of our frame counts */
   uint32_t self_frame_count;
   uint32_t read_frame_count;
   uint32_t other_frame_count;
   uint32_t replay_frame_count;

   /* And socket info */
   struct addrinfo *addr;
   struct sockaddr_storage their_addr;
   bool has_client_addr;

   unsigned timeout_cnt;

   /* Spectating. */
   struct {
      bool enabled;
      int fds[MAX_SPECTATORS];
      uint32_t frames[MAX_SPECTATORS];
      uint16_t *input;
      size_t input_ptr;
      size_t input_sz;
   } spectate;
   bool is_server;

   /* User flipping
    * Flipping state. If frame >= flip_frame, we apply the flip.
    * If not, we apply the opposite, effectively creating a trigger point. */
   bool flip;
   uint32_t flip_frame;

   /* Netplay pausing
    */
   bool local_paused;
   bool remote_paused;

   /* And stalling */
   uint32_t delay_frames;
   enum rarch_netplay_stall_reason stall;
   retro_time_t stall_time;

   /* Frequency with which to check CRCs */
   uint32_t check_frames;

   struct netplay_callbacks* net_cbs;
};

void input_poll_net(void);

/**
 * netplay_new:
 * @direct_host          : Netplay host discovered from scanning.
 * @server               : IP address of server.
 * @port                 : Port of server.
 * @delay_frames         : Amount of delay frames.
 * @check_frames         : Frequency with which to check CRCs.
 * @cb                   : Libretro callbacks.
 * @spectate             : If true, enable spectator mode.
 * @nat_traversal        : If true, attempt NAT traversal.
 * @nick                 : Nickname of user.
 * @quirks               : Netplay quirks.
 *
 * Creates a new netplay handle. A NULL host means we're
 * hosting (user 1).
 *
 * Returns: new netplay handle.
 **/
netplay_t *netplay_new(void *direct_host, const char *server,
      uint16_t port, unsigned delay_frames, unsigned check_frames,
      const struct retro_callbacks *cb, bool spectate, bool nat_traversal,
      const char *nick, uint64_t quirks);

/**
 * netplay_free:
 * @netplay              : pointer to netplay object
 *
 * Frees netplay handle.
 **/
void netplay_free(netplay_t *handle);

/**
 * netplay_pre_frame:
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay.
 * Call this before running retro_run().
 *
 * Returns: true (1) if the frontend is clear to emulate the frame, false (0)
 * if we're stalled or paused
 **/
bool netplay_pre_frame(netplay_t *handle);

/**
 * netplay_post_frame:
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay.
 * We check if we have new input and replay from recorded input.
 * Call this after running retro_run().
 **/
void netplay_post_frame(netplay_t *handle);

/**
 * netplay_frontend_paused
 * @netplay              : pointer to netplay object
 * @paused               : true if frontend is paused
 *
 * Inform Netplay of the frontend's pause state (paused or otherwise)
 **/
void netplay_frontend_paused(netplay_t *netplay, bool paused);

/**
 * netplay_load_savestate
 * @netplay              : pointer to netplay object
 * @serial_info          : the savestate being loaded, NULL means "load it yourself"
 * @save                 : whether to save the provided serial_info into the frame buffer
 *
 * Inform Netplay of a savestate load and send it to the other side
 **/
void netplay_load_savestate(netplay_t *netplay, retro_ctx_serialize_info_t *serial_info, bool save);

/**
 * netplay_disconnect
 * @netplay              : pointer to netplay object
 *
 * Disconnect netplay.
 *
 * Returns: true (1) if successful. At present, cannot fail.
 **/
bool netplay_disconnect(netplay_t *netplay);

struct netplay_callbacks* netplay_get_cbs_net(void);

struct netplay_callbacks* netplay_get_cbs_spectate(void);

/* Normally called at init time, unless the INITIALIZATION quirk is set */
bool netplay_init_serialization(netplay_t *netplay);

/* Force serialization to be ready by fast-forwarding the core */
bool netplay_wait_and_init_serialization(netplay_t *netplay);

void netplay_simulate_input(netplay_t *netplay, uint32_t sim_ptr, bool resim);

void   netplay_log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick);

bool netplay_get_nickname(netplay_t *netplay, int fd);

bool netplay_send_nickname(netplay_t *netplay, int fd);

/* Various netplay initialization modes: */
bool netplay_handshake_init_send(netplay_t *netplay, struct netplay_connection *connection);
bool netplay_handshake_init(netplay_t *netplay, struct netplay_connection *connection, bool *had_input);
bool netplay_handshake_pre_nick(netplay_t *netplay, struct netplay_connection *connection, bool *had_input);
bool netplay_handshake_pre_sync(netplay_t *netplay, struct netplay_connection *connection, bool *had_input);

uint32_t netplay_impl_magic(void);

bool netplay_is_server(netplay_t* netplay);

bool netplay_is_spectate(netplay_t* netplay);

bool netplay_delta_frame_ready(netplay_t *netplay, struct delta_frame *delta, uint32_t frame);

uint32_t netplay_delta_frame_crc(netplay_t *netplay, struct delta_frame *delta);

bool netplay_cmd_crc(netplay_t *netplay, struct delta_frame *delta);

bool netplay_cmd_request_savestate(netplay_t *netplay);

/* DISCOVERY: */

bool netplay_lan_ad_server(netplay_t *netplay);

bool netplay_init_socket_buffer(struct socket_buffer *sbuf, size_t size);

bool netplay_resize_socket_buffer(struct socket_buffer *sbuf, size_t newsize);

void netplay_deinit_socket_buffer(struct socket_buffer *sbuf);

void netplay_clear_socket_buffer(struct socket_buffer *sbuf);

bool netplay_send(struct socket_buffer *sbuf, int sockfd, const void *buf, size_t len);

bool netplay_send_flush(struct socket_buffer *sbuf, int sockfd, bool block);

ssize_t netplay_recv(struct socket_buffer *sbuf, int sockfd, void *buf, size_t len, bool block);

void netplay_recv_reset(struct socket_buffer *sbuf);

void netplay_recv_flush(struct socket_buffer *sbuf);

#endif

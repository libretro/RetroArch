/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#define NETPLAY_PROTOCOL_VERSION 3

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

struct netplay_callbacks {
   bool (*pre_frame) (netplay_t *netplay);
   void (*post_frame)(netplay_t *netplay);
   bool (*info_cb)   (netplay_t *netplay, unsigned frames);
};

enum rarch_netplay_stall_reasons
{
    RARCH_NETPLAY_STALL_NONE = 0,
    RARCH_NETPLAY_STALL_RUNNING_FAST,
    RARCH_NETPLAY_STALL_NO_CONNECTION
};

struct netplay
{
   char nick[32];
   char other_nick[32];
   struct sockaddr_storage other_addr;

   struct retro_callbacks cbs;
   /* TCP connection for state sending, etc. Also used for commands */
   int fd;
   /* TCP port (if serving) */
   uint16_t tcp_port;
   /* NAT traversal info (if NAT traversal is used and serving) */
   bool nat_traversal;
   struct natt_status nat_traversal_state;
   /* Which port is governed by netplay (other user)? */
   unsigned port;
   bool has_connection;

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
   uint32_t packet_buffer[2 + WORDS_PER_FRAME];
   uint32_t self_frame_count;
   uint32_t read_frame_count;
   uint32_t other_frame_count;
   uint32_t replay_frame_count;
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
   uint32_t stall_frames;
   int stall;
   retro_time_t stall_time;

   /* Frequency with which to check CRCs */
   uint32_t check_frames;

   struct netplay_callbacks* net_cbs;
};

struct netplay_callbacks* netplay_get_cbs_net(void);

struct netplay_callbacks* netplay_get_cbs_spectate(void);

/* Normally called at init time, unless the INITIALIZATION quirk is set */
bool netplay_init_serialization(netplay_t *netplay);

/* Force serialization to be ready by fast-forwarding the core */
bool netplay_wait_and_init_serialization(netplay_t *netplay);

void netplay_simulate_input(netplay_t *netplay, uint32_t sim_ptr);

void   netplay_log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick);

bool netplay_get_nickname(netplay_t *netplay, int fd);

bool netplay_send_nickname(netplay_t *netplay, int fd);

bool netplay_handshake(netplay_t *netplay);

uint32_t netplay_impl_magic(void);

bool netplay_is_server(netplay_t* netplay);

bool netplay_is_spectate(netplay_t* netplay);

bool netplay_delta_frame_ready(netplay_t *netplay, struct delta_frame *delta, uint32_t frame);

uint32_t netplay_delta_frame_crc(netplay_t *netplay, struct delta_frame *delta);

bool netplay_cmd_crc(netplay_t *netplay, struct delta_frame *delta);

bool netplay_cmd_request_savestate(netplay_t *netplay);

bool netplay_ad_server(netplay_t *netplay, int ad_fd);

#endif

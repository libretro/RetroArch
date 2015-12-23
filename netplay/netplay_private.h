/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <retro_endianness.h>
#include "../command_event.h"
#include "../general.h"
#include "../autosave.h"
#include "../dynamic.h"
#include "../movie.h"
#include "../msg_hash.h"
#include "../system.h"
#include "../runloop.h"
#include "../verbosity.h"

#ifdef ANDROID
#define HAVE_IPV6
#endif

#define UDP_FRAME_PACKETS 16
#define MAX_SPECTATORS 16
#define RARCH_DEFAULT_PORT 55435

#define PREV_PTR(x) ((x) == 0 ? netplay->buffer_size - 1 : (x) - 1)
#define NEXT_PTR(x) ((x + 1) % netplay->buffer_size)

struct delta_frame
{
   void *state;

   uint16_t real_input_state;
   uint16_t simulated_input_state;
   uint16_t self_state;

   bool is_simulated;
   bool used_real;
};

struct netplay_callbacks {
   void (*pre_frame) (netplay_t *netplay);
   void (*post_frame)(netplay_t *netplay);
   bool (*info_cb)   (netplay_t *netplay, unsigned frames);
};

struct netplay
{
   char nick[32];
   char other_nick[32];
   struct sockaddr_storage other_addr;

   struct retro_callbacks cbs;
   /* TCP connection for state sending, etc. Also used for commands */
   int fd;
   /* UDP connection for game state updates. */
   int udp_fd;
   /* Which port is governed by netplay (other user)? */
   unsigned port;
   bool has_connection;

   struct delta_frame *buffer;
   size_t buffer_size;

   /* Pointer where we are now. */
   size_t self_ptr; 
   /* Points to the last reliable state that self ever had. */
   size_t other_ptr;
   /* Pointer to where we are reading. 
    * Generally, other_ptr <= read_ptr <= self_ptr. */
   size_t read_ptr;
   /* A temporary pointer used on replay. */
   size_t tmp_ptr;

   size_t state_size;

   /* Are we replaying old frames? */
   bool is_replay;
   /* We don't want to poll several times on a frame. */
   bool can_poll;

   /* To compat UDP packet loss we also send 
    * old data along with the packets. */
   uint32_t packet_buffer[UDP_FRAME_PACKETS * 2];
   uint32_t frame_count;
   uint32_t read_frame_count;
   uint32_t other_frame_count;
   uint32_t tmp_frame_count;
   struct addrinfo *addr;
   struct sockaddr_storage their_addr;
   bool has_client_addr;

   unsigned timeout_cnt;

   /* Spectating. */
   struct {
      bool enabled;
      int fds[MAX_SPECTATORS];
      uint16_t *input;
      size_t input_ptr;
      size_t input_sz;
   } spectate;
   bool is_server;
   /* User flipping
    * Flipping state. If ptr >= flip_frame, we apply the flip.
    * If not, we apply the opposite, effectively creating a trigger point.
    * To avoid collition we need to make sure our client/host is synced up 
    * well after flip_frame before allowing another flip. */
   bool flip;
   uint32_t flip_frame;

   /* Netplay pausing
    */
   bool pause;
   uint32_t pause_frame;

   struct netplay_callbacks* net_cbs;
};

void *netplay_data;

struct netplay_callbacks* netplay_get_cbs_net(void);
struct netplay_callbacks* netplay_get_cbs_spectate(void);
void np_log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick);

bool np_get_nickname(netplay_t *netplay, int fd);
bool np_send_nickname(netplay_t *netplay, int fd);
bool np_send_info(netplay_t *netplay);
uint32_t *np_bsv_header_generate(size_t *size, uint32_t magic);
bool np_bsv_parse_header(const uint32_t *header, uint32_t magic);
uint32_t np_impl_magic(void);
bool np_send_info(netplay_t *netplay);
bool np_get_info(netplay_t *netplay);
bool np_is_server(netplay_t* netplay);
bool np_is_spectate(netplay_t* netplay);
#endif
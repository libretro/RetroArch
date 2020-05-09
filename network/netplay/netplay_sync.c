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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <boolean.h>

#include "netplay_private.h"

#include "../../autosave.h"
#include "../../driver.h"
#include "../../input/input_driver.h"

#if 0
#define DEBUG_NONDETERMINISTIC_CORES
#endif

/**
 * netplay_update_unread_ptr
 *
 * Update the global unread_ptr and unread_frame_count to correspond to the
 * earliest unread frame count of any connected player
 */
void netplay_update_unread_ptr(netplay_t *netplay)
{
   if (netplay->is_server && netplay->connected_players<=1)
   {
      /* Nothing at all to read! */
      netplay->unread_ptr         = netplay->self_ptr;
      netplay->unread_frame_count = netplay->self_frame_count;

   }
   else
   {
      size_t           new_unread_ptr = 0;
      uint32_t new_unread_frame_count = (uint32_t) -1;
      uint32_t client;

      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(netplay->connected_players & (1<<client)))
            continue;
         if ((netplay->connected_slaves & (1<<client)))
            continue;

         if (netplay->read_frame_count[client] < new_unread_frame_count)
         {
            new_unread_ptr         = netplay->read_ptr[client];
            new_unread_frame_count = netplay->read_frame_count[client];
         }
      }

      if ( !netplay->is_server && 
            netplay->server_frame_count < new_unread_frame_count)
      {
         new_unread_ptr              = netplay->server_ptr;
         new_unread_frame_count      = netplay->server_frame_count;
      }

      if (new_unread_frame_count != (uint32_t) -1)
      {
         netplay->unread_ptr         = new_unread_ptr;
         netplay->unread_frame_count = new_unread_frame_count;
      }
      else
      {
         netplay->unread_ptr         = netplay->self_ptr;
         netplay->unread_frame_count = netplay->self_frame_count;
      }
   }
}

struct vote_count {
   uint16_t votes[32];
};

/**
 * netplay_device_client_state
 * @netplay             : pointer to netplay object
 * @simframe            : frame in which merging is being performed
 * @device              : device being merged
 * @client              : client to find state for
 */
netplay_input_state_t netplay_device_client_state(netplay_t *netplay,
      struct delta_frame *simframe, uint32_t device, uint32_t client)
{
   uint32_t                 dsize = 
      netplay_expected_input_size(netplay, 1 << device);
   netplay_input_state_t simstate =
      netplay_input_state_for(
         &simframe->real_input[device], client,
         dsize, false, true);

   if (!simstate)
   {
      if (netplay->read_frame_count[client] > simframe->frame)
         return NULL;
      simstate = netplay_input_state_for(&simframe->simlated_input[device],
            client, dsize, false, true);
   }
   return simstate;
}

/**
 * netplay_merge_digital
 * @netplay             : pointer to netplay object
 * @resstate            : state being resolved
 * @simframe            : frame in which merging is being performed
 * @device              : device being merged
 * @clients             : bitmap of clients being merged
 * @digital             : bitmap of digital bits
 */
static void netplay_merge_digital(netplay_t *netplay,
      netplay_input_state_t resstate, struct delta_frame *simframe,
      uint32_t device, uint32_t clients, const uint32_t *digital)
{
   netplay_input_state_t simstate;
   uint32_t word, bit, client;
   uint8_t share_mode = netplay->device_share_modes[device]
      & NETPLAY_SHARE_DIGITAL_BITS;

   /* Make sure all real clients are accounted for */
   for (simstate = simframe->real_input[device];
         simstate; simstate = simstate->next)
   {
      if (!simstate->used || simstate->size != resstate->size)
         continue;
      clients |= 1<<simstate->client_num;
   }

   if (share_mode == NETPLAY_SHARE_DIGITAL_VOTE)
   {
      unsigned i, j;
      /* This just assumes we have no more than
       * three words, will need to be adjusted for new devices */
      struct vote_count votes[3];
      /* Vote mode requires counting all the bits */
      uint32_t client_count      = 0;

      for (i = 0; i < 3; i++)
         for (j = 0; j < 32; j++)
            votes[i].votes[j] = 0;

      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(clients & (1<<client)))
            continue;

         simstate = netplay_device_client_state(
               netplay, simframe, device, client);

         if (!simstate)
            continue;
         client_count++;

         for (word = 0; word < resstate->size; word++)
         {
            if (!digital[word])
               continue;
            for (bit = 0; bit < 32; bit++)
            {
               if (!(digital[word] & (1<<bit)))
                  continue;
               if (simstate->data[word] & (1<<bit))
                  votes[word].votes[bit]++;
            }
         }
      }

      /* Now count all the bits */
      client_count /= 2;
      for (word = 0; word < resstate->size; word++)
      {
         for (bit = 0; bit < 32; bit++)
         {
            if (votes[word].votes[bit] > client_count)
               resstate->data[word] |= (1<<bit);
         }
      }
   }
   else /* !VOTE */
   {
      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(clients & (1<<client)))
            continue;
         simstate = netplay_device_client_state(
               netplay, simframe, device, client);

         if (!simstate)
            continue;
         for (word = 0; word < resstate->size; word++)
         {
            uint32_t part;
            if (!digital[word])
               continue;
            part = simstate->data[word];

            if (digital[word] == (uint32_t) -1)
            {
               /* Combine the whole word */
               switch (share_mode)
               {
                  case NETPLAY_SHARE_DIGITAL_XOR:
                     resstate->data[word] ^= part;
                     break;
                  default:
                     resstate->data[word] |= part;
               }

            }
            else /* !whole word */
            {
               for (bit = 0; bit < 32; bit++)
               {
                  if (!(digital[word] & (1<<bit)))
                     continue;
                  switch (share_mode)
                  {
                     case NETPLAY_SHARE_DIGITAL_XOR:
                        resstate->data[word] ^= part & (1<<bit);
                        break;
                     default:
                        resstate->data[word] |= part & (1<<bit);
                  }
               }
            }
         }
      }

   }
}

/**
 * merge_analog_part
 * @netplay             : pointer to netplay object
 * @resstate            : state being resolved
 * @simframe            : frame in which merging is being performed
 * @device              : device being merged
 * @clients             : bitmap of clients being merged
 * @word                : word to merge
 * @bit                 : first bit to merge
 */
static void merge_analog_part(netplay_t *netplay,
      netplay_input_state_t resstate, struct delta_frame *simframe,
      uint32_t device, uint32_t clients, uint32_t word, uint8_t bit)
{
   netplay_input_state_t simstate;
   uint32_t client, client_count = 0;
   uint8_t share_mode            = netplay->device_share_modes[device]
      & NETPLAY_SHARE_ANALOG_BITS;
   int32_t value                 = 0, new_value;

   /* Make sure all real clients are accounted for */
   for (simstate = simframe->real_input[device]; simstate; simstate = simstate->next)
   {
      if (!simstate->used || simstate->size != resstate->size)
         continue;
      clients |= 1<<simstate->client_num;
   }

   for (client = 0; client < MAX_CLIENTS; client++)
   {
      if (!(clients & (1<<client)))
         continue;
      simstate = netplay_device_client_state(
            netplay, simframe, device, client);
      if (!simstate)
         continue;
      client_count++;
      new_value = (int16_t) ((simstate->data[word]>>bit) & 0xFFFF);
      switch (share_mode)
      {
         case NETPLAY_SHARE_ANALOG_AVERAGE:
            value += (int32_t) new_value;
            break;
         default:
            if (abs(new_value) > abs(value) ||
                (abs(new_value) == abs(value) && new_value > value))
               value = new_value;
      }
   }

   if (share_mode == NETPLAY_SHARE_ANALOG_AVERAGE)
      if (client_count > 0) /* Prevent potential divide by zero */
         value /= client_count;

   resstate->data[word] |= ((uint32_t) (uint16_t) value) << bit;
}

/**
 * netplay_merge_analog
 * @netplay             : pointer to netplay object
 * @resstate            : state being resolved
 * @simframe            : frame in which merging is being performed
 * @device              : device being merged
 * @clients             : bitmap of clients being merged
 * @dtype               : device type
 */
static void netplay_merge_analog(netplay_t *netplay,
      netplay_input_state_t resstate, struct delta_frame *simframe,
      uint32_t device, uint32_t clients, unsigned dtype)
{
   /* Devices with no analog parts */
   if (dtype == RETRO_DEVICE_JOYPAD || dtype == RETRO_DEVICE_KEYBOARD)
      return;

   /* All other devices have at least one analog word */
   merge_analog_part(netplay, resstate, simframe, device, clients, 1, 0);
   merge_analog_part(netplay, resstate, simframe, device, clients, 1, 16);

   /* And the ANALOG device has two (two sticks) */
   if (dtype == RETRO_DEVICE_ANALOG)
   {
      merge_analog_part(netplay, resstate, simframe, device, clients, 2, 0);
      merge_analog_part(netplay, resstate, simframe, device, clients, 2, 16);
   }
}

/**
 * netplay_resolve_input
 * @netplay             : pointer to netplay object
 * @sim_ptr             : frame pointer for which to resolve input
 * @resim               : are we resimulating, or simulating this frame for the
 *                        first time?
 *
 * "Simulate" input by assuming it hasn't changed since the last read input.
 * Returns true if the resolved input changed from the last time it was
 * resolved.
 */
bool netplay_resolve_input(netplay_t *netplay, size_t sim_ptr, bool resim)
{
   size_t prev;
   uint32_t device;
   uint32_t clients, client, client_count;
   netplay_input_state_t simstate, client_state = NULL,
                         resstate, oldresstate, pstate;
   bool ret                     = false;
   struct delta_frame *pframe   = NULL;
   struct delta_frame *simframe = &netplay->buffer[sim_ptr];

   for (device = 0; device < MAX_INPUT_DEVICES; device++)
   {
      unsigned dtype = netplay->config_devices[device]&RETRO_DEVICE_MASK;
      uint32_t dsize = netplay_expected_input_size(netplay, 1 << device);
      clients        = netplay->device_clients[device];
      client_count   = 0;

      /* Make sure all real clients are accounted for */
      for (simstate = simframe->real_input[device]; simstate; simstate = simstate->next)
      {
         if (!simstate->used || simstate->size != dsize)
            continue;
         clients |= 1<<simstate->client_num;
      }

      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(clients & (1<<client)))
            continue;

         /* Resolve this client-device */
         simstate = netplay_input_state_for(
               &simframe->real_input[device], client, dsize, false, true);
         if (!simstate)
         {
            /* Don't already have this input, so must
             * simulate if we're supposed to have it at all */
            if (netplay->read_frame_count[client] > simframe->frame)
               continue;
            simstate = netplay_input_state_for(&simframe->simlated_input[device], client, dsize, false, false);
            if (!simstate)
               continue;

            prev = PREV_PTR(netplay->read_ptr[client]);
            pframe = &netplay->buffer[prev];
            pstate = netplay_input_state_for(&pframe->real_input[device], client, dsize, false, true);
            if (!pstate)
               continue;

            if (resim && (dtype == RETRO_DEVICE_JOYPAD || dtype == RETRO_DEVICE_ANALOG))
            {
               /* In resimulation mode, we only copy the buttons. The reason for this
                * is nonobvious:
                *
                * If we resimulated nothing, then the /duration/ with which any input
                * was pressed would be approximately correct, since the original
                * simulation came in as the input came in, but the /number of times/
                * the input was pressed would be wrong, as there would be an
                * advancing wavefront of real data overtaking the simulated data
                * (which is really just real data offset by some frames).
                *
                * That's acceptable for arrows in most situations, since the amount
                * you move is tied to the duration, but unacceptable for buttons,
                * which will seem to jerkily be pressed numerous times with those
                * wavefronts.
                */
               const uint32_t keep =
                  (1U<<RETRO_DEVICE_ID_JOYPAD_UP) |
                  (1U<<RETRO_DEVICE_ID_JOYPAD_DOWN) |
                  (1U<<RETRO_DEVICE_ID_JOYPAD_LEFT) |
                  (1U<<RETRO_DEVICE_ID_JOYPAD_RIGHT);
               simstate->data[0] &= keep;
               simstate->data[0] |= pstate->data[0] & ~keep;
            }
            else
               memcpy(simstate->data, pstate->data,
                     dsize * sizeof(uint32_t));
         }

         client_state = simstate;
         client_count++;
      }

      /* The frontend always uses the first resolved input,
       * so make sure it's right */
      while (simframe->resolved_input[device]
            && (simframe->resolved_input[device]->size != dsize
                  || simframe->resolved_input[device]->client_num != 0))
      {
         /* The default resolved input is of the wrong size! */
         netplay_input_state_t nextistate =
            simframe->resolved_input[device]->next;
         free(simframe->resolved_input[device]);
         simframe->resolved_input[device] = nextistate;
      }

      /* Now we copy the state, whether real or simulated,
       * out into the resolved state */
      resstate = netplay_input_state_for(
            &simframe->resolved_input[device], 0,
            dsize, false, false);
      if (!resstate)
         continue;

      if (client_count == 1)
      {
         /* Trivial in the common 1-client case */
         if (memcmp(resstate->data, client_state->data,
                  dsize * sizeof(uint32_t)))
            ret = true;
         memcpy(resstate->data, client_state->data,
               dsize * sizeof(uint32_t));

      }
      else if (client_count == 0)
      {
         uint32_t word;
         for (word = 0; word < dsize; word++)
         {
            if (resstate->data[word])
               ret = true;
            resstate->data[word] = 0;
         }

      }
      else
      {
         /* Merge them */
         /* Most devices have all the digital parts in the first word. */
         static const uint32_t digital_common[3]   = {~0u, 0u, 0u};
         static const uint32_t digital_keyboard[5] = {~0u, ~0u, ~0u, ~0u, ~0u};
         const uint32_t *digital                   = NULL;

         if (dtype == RETRO_DEVICE_KEYBOARD)
            digital = digital_keyboard;
         else
            digital = digital_common;

         oldresstate = netplay_input_state_for(
               &simframe->resolved_input[device], 1, dsize, false, false);

         if (!oldresstate)
            continue;
         memcpy(oldresstate->data, resstate->data, dsize * sizeof(uint32_t));
         memset(resstate->data, 0, dsize * sizeof(uint32_t));

         netplay_merge_digital(netplay, resstate, simframe,
               device, clients, digital);
         netplay_merge_analog(netplay, resstate, simframe,
               device, clients, dtype);

         if (memcmp(resstate->data, oldresstate->data,
                  dsize * sizeof(uint32_t)))
            ret = true;

      }
   }

   return ret;
}

static void netplay_handle_frame_hash(netplay_t *netplay,
      struct delta_frame *delta)
{
   if (netplay->is_server)
   {
      if (netplay->check_frames &&
          delta->frame % abs(netplay->check_frames) == 0)
      {
         delta->crc = netplay_delta_frame_crc(netplay, delta);
         netplay_cmd_crc(netplay, delta);
      }
   }
   else if (delta->crc && netplay->crcs_valid)
   {
      /* We have a remote CRC, so check it */
      uint32_t local_crc = netplay_delta_frame_crc(netplay, delta);
      if (local_crc != delta->crc)
      {
         /* If the very first check frame is wrong,
          * they probably just don't work */
         if (!netplay->crc_validity_checked)
            netplay->crcs_valid = false;
         else if (netplay->crcs_valid)
         {
            /* Fix this! */
            if (netplay->check_frames < 0)
            {
               /* Just report */
               RARCH_ERR("Netplay CRCs mismatch!\n");
            }
            else
               netplay_cmd_request_savestate(netplay);
         }
      }
      else if (!netplay->crc_validity_checked)
         netplay->crc_validity_checked = true;
   }
}

/**
 * netplay_sync_pre_frame
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay synchronization.
 */
bool netplay_sync_pre_frame(netplay_t *netplay)
{
   retro_ctx_serialize_info_t serial_info;

   if (netplay_delta_frame_ready(netplay,
            &netplay->buffer[netplay->run_ptr], netplay->run_frame_count))
   {
      serial_info.data_const = NULL;
      serial_info.data       = netplay->buffer[netplay->run_ptr].state;
      serial_info.size       = netplay->state_size;

      memset(serial_info.data, 0, serial_info.size);
      if ((netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
            || netplay->run_frame_count == 0)
      {
         /* Don't serialize until it's safe */
      }
      else if (!(netplay->quirks & NETPLAY_QUIRK_NO_SAVESTATES)
            && core_serialize(&serial_info))
      {
         if (netplay->force_send_savestate && !netplay->stall
               && !netplay->remote_paused)
         {
            /* Bring our running frame and input frames into
             * parity so we don't send old info. */
            if (netplay->run_ptr != netplay->self_ptr)
            {
               memcpy(netplay->buffer[netplay->self_ptr].state,
                  netplay->buffer[netplay->run_ptr].state,
                  netplay->state_size);
               netplay->run_ptr         = netplay->self_ptr;
               netplay->run_frame_count = netplay->self_frame_count;
            }

            /* Send this along to the other side */
            serial_info.data_const = netplay->buffer[netplay->run_ptr].state;
            netplay_load_savestate(netplay, &serial_info, false);
            netplay->force_send_savestate = false;
         }
      }
      else
      {
         /* If the core can't serialize properly, we must stall for the
          * remote input on EVERY frame, because we can't recover */
         netplay->quirks |= NETPLAY_QUIRK_NO_SAVESTATES;
         netplay->stateless_mode = true;
      }

      /* If we can't transmit savestates, we must stall
       * until the client is ready. */
      if (netplay->run_frame_count > 0 &&
          (netplay->quirks & (NETPLAY_QUIRK_NO_SAVESTATES|NETPLAY_QUIRK_NO_TRANSMISSION)) &&
          (netplay->connections_size == 0 || !netplay->connections[0].active ||
           netplay->connections[0].mode < NETPLAY_CONNECTION_CONNECTED))
         netplay->stall = NETPLAY_STALL_NO_CONNECTION;
   }

   if (netplay->is_server)
   {
      fd_set fds;
      struct timeval tmp_tv = {0};
      int new_fd;
      struct sockaddr_storage their_addr;
      socklen_t addr_size;
      struct netplay_connection *connection;
      size_t connection_num;

      /* Check for a connection */
      FD_ZERO(&fds);
      FD_SET(netplay->listen_fd, &fds);
      if (socket_select(netplay->listen_fd + 1,
               &fds, NULL, NULL, &tmp_tv) > 0 &&
          FD_ISSET(netplay->listen_fd, &fds))
      {
         addr_size = sizeof(their_addr);
         new_fd = accept(netplay->listen_fd,
               (struct sockaddr*)&their_addr, &addr_size);

         if (new_fd < 0)
         {
            RARCH_ERR("%s\n", msg_hash_to_str(MSG_NETPLAY_FAILED));
            goto process;
         }

         /* Set the socket nonblocking */
         if (!socket_nonblock(new_fd))
         {
            /* Catastrophe! */
            socket_close(new_fd);
            goto process;
         }

#if defined(IPPROTO_TCP) && defined(TCP_NODELAY)
         {
            int flag = 1;
            if (setsockopt(new_fd, IPPROTO_TCP, TCP_NODELAY,
#ifdef _WIN32
               (const char*)
#else
               (const void*)
#endif
               &flag,
               sizeof(int)) < 0)
               RARCH_WARN("Could not set netplay TCP socket to nodelay. Expect jitter.\n");
         }
#endif

#if defined(F_SETFD) && defined(FD_CLOEXEC)
         /* Don't let any inherited processes keep open our port */
         if (fcntl(new_fd, F_SETFD, FD_CLOEXEC) < 0)
            RARCH_WARN("Cannot set Netplay port to close-on-exec. It may fail to reopen if the client disconnects.\n");
#endif

         /* Allocate a connection */
         for (connection_num = 0; connection_num < netplay->connections_size; connection_num++)
            if (!netplay->connections[connection_num].active &&
                  netplay->connections[connection_num].mode != NETPLAY_CONNECTION_DELAYED_DISCONNECT)
               break;
         if (connection_num == netplay->connections_size)
         {
            if (connection_num == 0)
            {
               netplay->connections = (struct netplay_connection*)
                  malloc(sizeof(struct netplay_connection));

               if (!netplay->connections)
               {
                  socket_close(new_fd);
                  goto process;
               }
               netplay->connections_size = 1;

            }
            else
            {
               size_t new_connections_size = netplay->connections_size * 2;
               struct netplay_connection
                  *new_connections         = (struct netplay_connection*)

                  realloc(netplay->connections,
                     new_connections_size*sizeof(struct netplay_connection));

               if (!new_connections)
               {
                  socket_close(new_fd);
                  goto process;
               }

               memset(new_connections + netplay->connections_size, 0,
                  netplay->connections_size * sizeof(struct netplay_connection));
               netplay->connections = new_connections;
               netplay->connections_size = new_connections_size;

            }
         }
         connection         = &netplay->connections[connection_num];

         /* Set it up */
         memset(connection, 0, sizeof(*connection));
         connection->active = true;
         connection->fd     = new_fd;
         connection->mode   = NETPLAY_CONNECTION_INIT;

         if (!netplay_init_socket_buffer(&connection->send_packet_buffer,
               netplay->packet_buffer_size) ||
             !netplay_init_socket_buffer(&connection->recv_packet_buffer,
               netplay->packet_buffer_size))
         {
            if (connection->send_packet_buffer.data)
               netplay_deinit_socket_buffer(&connection->send_packet_buffer);
            connection->active = false;
            socket_close(new_fd);
            goto process;
         }

         netplay_handshake_init_send(netplay, connection);

      }
   }

process:
   netplay->can_poll = true;
   input_poll_net();

   return (netplay->stall != NETPLAY_STALL_NO_CONNECTION);
}

/**
 * netplay_sync_post_frame
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay synchronization.
 * We check if we have new input and replay from recorded input.
 */
void netplay_sync_post_frame(netplay_t *netplay, bool stalled)
{
   uint32_t lo_frame_count, hi_frame_count;

   /* Unless we're stalling, we've just finished running a frame */
   if (!stalled)
   {
      netplay->run_ptr = NEXT_PTR(netplay->run_ptr);
      netplay->run_frame_count++;
   }

   /* We've finished an input frame even if we're stalling */
   if ((!stalled || netplay->stall == NETPLAY_STALL_INPUT_LATENCY) &&
       netplay->self_frame_count <
       netplay->run_frame_count + netplay->input_latency_frames)
   {
      netplay->self_ptr = NEXT_PTR(netplay->self_ptr);
      netplay->self_frame_count++;
   }

   /* Only relevant if we're connected and not in a desynching operation */
   if ((netplay->is_server && (netplay->connected_players<=1)) ||
       (netplay->self_mode < NETPLAY_CONNECTION_CONNECTED)     ||
       (netplay->desync))
   {
      netplay->other_frame_count = netplay->self_frame_count;
      netplay->other_ptr         = netplay->self_ptr;

      /* FIXME: Duplication */
      if (netplay->catch_up)
      {
         netplay->catch_up = false;
         input_driver_unset_nonblock_state();
         driver_set_nonblock_state();
      }
      return;
   }

   /* Reset if it was requested */
   if (netplay->force_reset)
   {
      core_reset();
      netplay->force_reset = false;
   }

   netplay->replay_ptr = netplay->other_ptr;
   netplay->replay_frame_count = netplay->other_frame_count;

#ifndef DEBUG_NONDETERMINISTIC_CORES
   if (!netplay->force_rewind)
   {
      bool cont = true;

      /* Skip ahead if we predicted correctly.
       * Skip until our simulation failed. */
      while (netplay->other_frame_count < netplay->unread_frame_count &&
             netplay->other_frame_count < netplay->run_frame_count)
      {
         struct delta_frame *ptr = &netplay->buffer[netplay->other_ptr];

         /* If resolving the input changes it, we used bad input */
         if (netplay_resolve_input(netplay, netplay->other_ptr, true))
         {
            cont = false;
            break;
         }

         netplay_handle_frame_hash(netplay, ptr);
         netplay->other_ptr = NEXT_PTR(netplay->other_ptr);
         netplay->other_frame_count++;
      }
      netplay->replay_ptr = netplay->other_ptr;
      netplay->replay_frame_count = netplay->other_frame_count;

      if (cont)
      {
         while (netplay->replay_frame_count < netplay->run_frame_count)
         {
            if (netplay_resolve_input(netplay, netplay->replay_ptr, true))
               break;
            netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
            netplay->replay_frame_count++;
         }
      }
   }
#endif

   /* Now replay the real input if we've gotten ahead of it */
   if (netplay->force_rewind ||
       netplay->replay_frame_count < netplay->run_frame_count)
   {
      retro_ctx_serialize_info_t serial_info;

      /* Replay frames. */
      netplay->is_replay = true;

      /* If we have a keyboard device, we replay the previous frame's input
       * just to assert that the keydown/keyup events work if the core
       * translates them in that way */
      if (netplay->have_updown_device)
      {
         netplay->replay_ptr = PREV_PTR(netplay->replay_ptr);
         netplay->replay_frame_count--;
#ifdef HAVE_THREADS
         autosave_lock();
#endif
         core_run();
#ifdef HAVE_THREADS
         autosave_unlock();
#endif
         netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
         netplay->replay_frame_count++;
      }

      if (netplay->quirks & NETPLAY_QUIRK_INITIALIZATION)
         /* Make sure we're initialized before we start loading things */
         netplay_wait_and_init_serialization(netplay);

      serial_info.data       = NULL;
      serial_info.data_const = netplay->buffer[netplay->replay_ptr].state;
      serial_info.size       = netplay->state_size;

      if (!core_unserialize(&serial_info))
      {
         RARCH_ERR("Netplay savestate loading failed: Prepare for desync!\n");
      }

      while (netplay->replay_frame_count < netplay->run_frame_count)
      {
         retro_time_t start, tm;
         struct delta_frame *ptr = &netplay->buffer[netplay->replay_ptr];

         serial_info.data        = ptr->state;
         serial_info.size        = netplay->state_size;
         serial_info.data_const  = NULL;

         start                   = cpu_features_get_time_usec();

         /* Remember the current state */
         memset(serial_info.data, 0, serial_info.size);
         core_serialize(&serial_info);
         if (netplay->replay_frame_count < netplay->unread_frame_count)
            netplay_handle_frame_hash(netplay, ptr);

         /* Re-simulate this frame's input */
         netplay_resolve_input(netplay, netplay->replay_ptr, true);

#ifdef HAVE_THREADS
         autosave_lock();
#endif
         core_run();
#ifdef HAVE_THREADS
         autosave_unlock();
#endif
         netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
         netplay->replay_frame_count++;

#ifdef DEBUG_NONDETERMINISTIC_CORES
         if (ptr->have_remote && netplay_delta_frame_ready(netplay, &netplay->buffer[netplay->replay_ptr], netplay->replay_frame_count))
         {
            RARCH_LOG("PRE  %u: %X\n", netplay->replay_frame_count-1, netplay_delta_frame_crc(netplay, ptr));
            if (netplay->is_server)
               RARCH_LOG("INP  %X %X\n", ptr->real_input_state[0], ptr->self_state[0]);
            else
               RARCH_LOG("INP  %X %X\n", ptr->self_state[0], ptr->real_input_state[0]);
            ptr = &netplay->buffer[netplay->replay_ptr];
            serial_info.data = ptr->state;
            memset(serial_info.data, 0, serial_info.size);
            core_serialize(&serial_info);
            RARCH_LOG("POST %u: %X\n", netplay->replay_frame_count-1, netplay_delta_frame_crc(netplay, ptr));
         }
#endif

         /* Get our time window */
         tm = cpu_features_get_time_usec() - start;
         netplay->frame_run_time_sum -= netplay->frame_run_time[netplay->frame_run_time_ptr];
         netplay->frame_run_time[netplay->frame_run_time_ptr] = tm;
         netplay->frame_run_time_sum += tm;
         netplay->frame_run_time_ptr++;
         if (netplay->frame_run_time_ptr >= NETPLAY_FRAME_RUN_TIME_WINDOW)
            netplay->frame_run_time_ptr = 0;
      }

      /* Average our time */
      netplay->frame_run_time_avg   = netplay->frame_run_time_sum / NETPLAY_FRAME_RUN_TIME_WINDOW;

      if (netplay->unread_frame_count < netplay->run_frame_count)
      {
         netplay->other_ptr         = netplay->unread_ptr;
         netplay->other_frame_count = netplay->unread_frame_count;
      }
      else
      {
         netplay->other_ptr         = netplay->run_ptr;
         netplay->other_frame_count = netplay->run_frame_count;
      }
      netplay->is_replay            = false;
      netplay->force_rewind         = false;
   }

   if (netplay->is_server)
   {
      uint32_t client;

      lo_frame_count = hi_frame_count = netplay->unread_frame_count;

      /* Look for players that are ahead of us */
      for (client = 0; client < MAX_CLIENTS; client++)
      {
         if (!(netplay->connected_players & (1<<client)))
            continue;
         if (netplay->read_frame_count[client] > hi_frame_count)
            hi_frame_count = netplay->read_frame_count[client];
      }
   }
   else
      lo_frame_count = hi_frame_count = netplay->server_frame_count;

   /* If we're behind, try to catch up */
   if (netplay->catch_up)
   {
      /* Are we caught up? */
      if (netplay->self_frame_count + 1 >= lo_frame_count)
      {
         netplay->catch_up = false;
         input_driver_unset_nonblock_state();
         driver_set_nonblock_state();
      }

   }
   else if (!stalled)
   {
      if (netplay->self_frame_count + 3 < lo_frame_count)
      {
         retro_time_t cur_time = cpu_features_get_time_usec();
         uint32_t cur_behind = lo_frame_count - netplay->self_frame_count;

         /* We're behind, but we'll only try to catch up if we're actually
          * falling behind, i.e. if we're more behind after some time */
         if (netplay->catch_up_time == 0)
         {
            /* Record our current time to check for catch-up later */
            netplay->catch_up_time = cur_time;
            netplay->catch_up_behind = cur_behind;

         }
         else if (cur_time - netplay->catch_up_time > CATCH_UP_CHECK_TIME_USEC)
         {
            /* Time to check how far behind we are */
            if (netplay->catch_up_behind <= cur_behind)
            {
               /* We're definitely falling behind! */
               netplay->catch_up = true;
               netplay->catch_up_time = 0;
               input_driver_set_nonblock_state();
               driver_set_nonblock_state();
            }
            else
            {
               /* Check again in another period */
               netplay->catch_up_time = cur_time;
               netplay->catch_up_behind = cur_behind;
            }
         }

      }
      else if (netplay->self_frame_count + 3 < hi_frame_count)
      {
         size_t i;
         netplay->catch_up_time = 0;

         /* We're falling behind some clients but not others, so request that
          * clients ahead of us stall */
         for (i = 0; i < netplay->connections_size; i++)
         {
            uint32_t client_num;
            struct netplay_connection *connection = &netplay->connections[i];

            if (!connection->active ||
                connection->mode != NETPLAY_CONNECTION_PLAYING)
               continue;

            client_num = (uint32_t)(i + 1);

            /* Are they ahead? */
            if (netplay->self_frame_count + 3 < netplay->read_frame_count[client_num])
            {
               /* Tell them to stall */
               if (connection->stall_frame + NETPLAY_MAX_REQ_STALL_FREQUENCY <
                     netplay->self_frame_count)
               {
                  connection->stall_frame = netplay->self_frame_count;
                  netplay_cmd_stall(netplay, connection,
                     netplay->read_frame_count[client_num] -
                     netplay->self_frame_count + 1);
               }
            }
         }
      }
      else
         netplay->catch_up_time = 0;
   }
   else
      netplay->catch_up_time =  0;
}

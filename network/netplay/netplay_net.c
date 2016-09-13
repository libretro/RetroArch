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

#include <compat/strl.h>
#include <stdio.h>

#include "netplay_private.h"

#include "retro_assert.h"

#include "../../autosave.h"

/**
 * pre_frame:   
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay (normal version).
 **/
static void netplay_net_pre_frame(netplay_t *netplay)
{
   retro_ctx_serialize_info_t serial_info;

   if (netplay_delta_frame_ready(netplay, &netplay->buffer[netplay->self_ptr], netplay->self_frame_count))
   {
       serial_info.data_const = NULL;
       serial_info.data = netplay->buffer[netplay->self_ptr].state;
       serial_info.size = netplay->state_size;

       if (!core_serialize(&serial_info))
       {
           /* If the core can't serialize properly, we must stall for the
            * remote input on EVERY frame, because we can't recover */
           netplay->stall_frames = 0;
       }
   }

   netplay->can_poll = true;

   input_poll_net();
}

/**
 * post_frame:   
 * @netplay              : pointer to netplay object
 *
 * Post-frame for Netplay (normal version).
 * We check if we have new input and replay from recorded input.
 **/
static void netplay_net_post_frame(netplay_t *netplay)
{
   netplay->self_frame_count++;

   /* Skip ahead if we predicted correctly.
    * Skip until our simulation failed. */
   while (netplay->other_frame_count < netplay->read_frame_count)
   {
      const struct delta_frame *ptr = &netplay->buffer[netplay->other_ptr];

      if (memcmp(ptr->simulated_input_state, ptr->real_input_state,
               sizeof(ptr->real_input_state)) != 0
            && !ptr->used_real)
         break;
      netplay->other_ptr = NEXT_PTR(netplay->other_ptr);
      netplay->other_frame_count++;
   }

   /* Now replay the real input if we've gotten ahead of it */
   if (netplay->other_frame_count < netplay->read_frame_count)
   {
      retro_ctx_serialize_info_t serial_info;

      /* Replay frames. */
      netplay->is_replay = true;
      netplay->replay_ptr = netplay->other_ptr;
      netplay->replay_frame_count = netplay->other_frame_count;

      if (netplay->replay_frame_count < netplay->self_frame_count)
      {
         serial_info.data       = NULL;
         serial_info.data_const = netplay->buffer[netplay->replay_ptr].state;
         serial_info.size       = netplay->state_size;

         core_unserialize(&serial_info);
      }

      while (netplay->replay_frame_count < netplay->self_frame_count)
      {
         serial_info.data       = netplay->buffer[netplay->replay_ptr].state;
         serial_info.size       = netplay->state_size;
         serial_info.data_const = NULL;

         core_serialize(&serial_info);

#if defined(HAVE_THREADS)
         autosave_lock();
#endif
         core_run();
#if defined(HAVE_THREADS)
         autosave_unlock();
#endif
         netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
         netplay->replay_frame_count++;
      }

      /* For the remainder of the frames up to the read count, we can use the real data */
      while (netplay->replay_frame_count < netplay->read_frame_count)
      {
          retro_assert(netplay->buffer[netplay->replay_ptr].have_remote);
          netplay->replay_ptr = NEXT_PTR(netplay->replay_ptr);
          netplay->replay_frame_count++;
      }

      netplay->other_ptr = netplay->read_ptr;
      netplay->other_frame_count = netplay->read_frame_count;
      netplay->is_replay = false;
   }

   /* If we're supposed to stall, rewind */
   if (netplay->stall)
   {
      retro_ctx_serialize_info_t serial_info;

      netplay->self_ptr = PREV_PTR(netplay->self_ptr);
      netplay->self_frame_count--;

      serial_info.data       = NULL;
      serial_info.data_const = netplay->buffer[netplay->self_ptr].state;
      serial_info.size       = netplay->state_size;

      core_unserialize(&serial_info);
   }
}
static bool netplay_net_init_buffers(netplay_t *netplay)
{
   unsigned i;
   retro_ctx_size_info_t info;

   if (!netplay)
      return false;

   netplay->buffer = (struct delta_frame*)calloc(netplay->buffer_size,
         sizeof(*netplay->buffer));
   
   if (!netplay->buffer)
      return false;

   core_serialize_size(&info);

   netplay->state_size = info.size;

   for (i = 0; i < netplay->buffer_size; i++)
   {
      netplay->buffer[i].state = malloc(netplay->state_size);

      if (!netplay->buffer[i].state)
         return false;
   }

   return true;
}

static bool netplay_net_info_cb(netplay_t* netplay, unsigned frames)
{
   if (netplay_is_server(netplay))
   {
      if (!netplay_send_info(netplay))
         return false;
   }
   else
   {
      if (!netplay_get_info(netplay))
         return false;
   }

   /* * 2 + 1 because:
    * Self sits in the middle,
    * Other is allowed to drift as much as 'frames' frames behind
    * Read is allowed to drift as much as 'frames' frames ahead */
   netplay->buffer_size = frames * 2 + 1;

   if (!netplay_net_init_buffers(netplay))
      return false;

   netplay->has_connection = true;

   return true;
}

struct netplay_callbacks* netplay_get_cbs_net(void)
{
   static struct netplay_callbacks cbs = {
      &netplay_net_pre_frame,
      &netplay_net_post_frame,
      &netplay_net_info_cb
   };
   return &cbs;
}

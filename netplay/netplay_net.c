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

#include "netplay_private.h"
/**
 * pre_frame:   
 * @netplay              : pointer to netplay object
 *
 * Pre-frame for Netplay (normal version).
 **/
static void netplay_net_pre_frame(netplay_t *netplay)
{
   retro_ctx_serialize_info_t serial_info;

   serial_info.data = netplay->buffer[netplay->self_ptr].state;
   serial_info.size = netplay->state_size;

   core_ctl(CORE_CTL_RETRO_SERIALIZE, &serial_info);

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
   netplay->frame_count++;

   /* Nothing to do... */
   if (netplay->other_frame_count == netplay->read_frame_count)
      return;

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

   if (netplay->other_frame_count < netplay->read_frame_count)
   {
      retro_ctx_serialize_info_t serial_info;
      bool first = true;

      /* Replay frames. */
      netplay->is_replay = true;
      netplay->tmp_ptr = netplay->other_ptr;
      netplay->tmp_frame_count = netplay->other_frame_count;

      serial_info.data_const = netplay->buffer[netplay->other_ptr].state;
      serial_info.size       = netplay->state_size;

      core_ctl(CORE_CTL_RETRO_UNSERIALIZE, &serial_info);

      while (first || (netplay->tmp_ptr != netplay->self_ptr))
      {
         serial_info.data       = netplay->buffer[netplay->tmp_ptr].state;
         serial_info.size       = netplay->state_size;
         serial_info.data_const = NULL;

         core_ctl(CORE_CTL_RETRO_SERIALIZE, &serial_info);

#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
         lock_autosave();
#endif
         core_ctl(CORE_CTL_RETRO_RUN, NULL);
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
         unlock_autosave();
#endif
         netplay->tmp_ptr = NEXT_PTR(netplay->tmp_ptr);
         netplay->tmp_frame_count++;
         first = false;
      }

      netplay->other_ptr = netplay->read_ptr;
      netplay->other_frame_count = netplay->read_frame_count;
      netplay->is_replay = false;
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

   core_ctl(CORE_CTL_RETRO_SERIALIZE_SIZE, &info);

   netplay->state_size = info.size;

   for (i = 0; i < netplay->buffer_size; i++)
   {
      netplay->buffer[i].state = malloc(netplay->state_size);

      if (!netplay->buffer[i].state)
         return false;

      netplay->buffer[i].is_simulated = true;
   }

   return true;
}

static bool netplay_net_info_cb(netplay_t* netplay, unsigned frames)
{
   if (np_is_server(netplay))
   {
      if (!np_send_info(netplay))
         return false;
   }
   else
   {
      if (!np_get_info(netplay))
         return false;
   }

   netplay->buffer_size = frames + 1;

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

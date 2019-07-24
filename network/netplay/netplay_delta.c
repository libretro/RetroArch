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

#include <stdlib.h>
#include <sys/types.h>

#include <boolean.h>
#include <encodings/crc32.h>

#include "netplay_private.h"

static void clear_input(netplay_input_state_t istate)
{
   while (istate)
   {
      istate->used = false;
      istate = istate->next;
   }
}

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
   uint32_t frame)
{
   size_t i;
   if (delta->used)
   {
      if (delta->frame == frame)
         return true;
      /* We haven't even replayed this frame yet,
       * so we can't overwrite it! */
      if (netplay->other_frame_count <= delta->frame)
         return false;
   }

   delta->used  = true;
   delta->frame = frame;
   delta->crc   = 0;

   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      clear_input(delta->resolved_input[i]);
      clear_input(delta->real_input[i]);
      clear_input(delta->simlated_input[i]);
   }
   delta->have_local = false;
   for (i = 0; i < MAX_CLIENTS; i++)
      delta->have_real[i] = false;
   return true;
}

/**
 * netplay_delta_frame_crc
 *
 * Get the CRC for the serialization of this frame.
 */
uint32_t netplay_delta_frame_crc(netplay_t *netplay, struct delta_frame *delta)
{
   if (!netplay->state_size)
      return 0;
   return encoding_crc32(0L, (const unsigned char*)delta->state, netplay->state_size);
}

/*
 * Free an input state list
 */
static void free_input_state(netplay_input_state_t *list)
{
   netplay_input_state_t cur, next;
   cur = *list;
   while (cur)
   {
      next = cur->next;
      free(cur);
      cur = next;
   }
   *list = NULL;
}

/**
 * netplay_delta_frame_free
 *
 * Free a delta frame's dependencies
 */
void netplay_delta_frame_free(struct delta_frame *delta)
{
   uint32_t i;

   if (delta->state)
   {
      free(delta->state);
      delta->state = NULL;
   }

   for (i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      free_input_state(&delta->resolved_input[i]);
      free_input_state(&delta->real_input[i]);
      free_input_state(&delta->simlated_input[i]);
   }
}

/**
 * netplay_input_state_for
 *
 * Get an input state for a particular client
 */
netplay_input_state_t netplay_input_state_for(netplay_input_state_t *list,
      uint32_t client_num, size_t size, bool must_create, bool must_not_create)
{
   netplay_input_state_t ret;
   while (*list)
   {
      ret = *list;
      if (!ret->used && !must_not_create && ret->size == size)
      {
         ret->client_num = client_num;
         ret->used = true;
         memset(ret->data, 0, size*sizeof(uint32_t));
         return ret;
      }
      else if (ret->used && ret->client_num == client_num)
      {
         if (!must_create && ret->size == size)
            return ret;
         return NULL;
      }
      list = &(ret->next);
   }

   if (must_not_create)
      return NULL;

   /* Couldn't find a slot, allocate a fresh one */
   ret = (netplay_input_state_t)calloc(1, sizeof(struct netplay_input_state) + (size-1) * sizeof(uint32_t));
   if (!ret)
      return NULL;
   *list           = ret;
   ret->client_num = client_num;
   ret->used       = true;
   ret->size       = (uint32_t)size;
   return ret;
}

/**
 * netplay_expected_input_size
 *
 * Size in words for a given set of devices.
 */
uint32_t netplay_expected_input_size(netplay_t *netplay, uint32_t devices)
{
   uint32_t ret = 0, device;

   for (device = 0; device < MAX_INPUT_DEVICES; device++)
   {
      if (!(devices & (1<<device)))
         continue;

      switch (netplay->config_devices[device]&RETRO_DEVICE_MASK)
      {
         /* These are all essentially magic numbers, but each device has a
          * fixed size, documented in network/netplay/README */
         case RETRO_DEVICE_JOYPAD:
            ret += 1;
            break;
         case RETRO_DEVICE_MOUSE:
            ret += 2;
            break;
         case RETRO_DEVICE_KEYBOARD:
            ret += 5;
            break;
         case RETRO_DEVICE_LIGHTGUN:
            ret += 2;
            break;
         case RETRO_DEVICE_ANALOG:
            ret += 3;
            break;
         default:
            break; /* Unsupported */
      }
   }

   return ret;
}

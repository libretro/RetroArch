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

#include <compat/strl.h>

#include "netplay_private.h"
#include <net/net_socket.h>

#include <encodings/crc32.h>

#include "../../movie.h"
#include "../../msg_hash.h"
#include "../../content.h"
#include "../../runloop.h"
#include "../../version.h"

bool netplay_get_nickname(netplay_t *netplay, int fd)
{
   uint8_t nick_size;

   if (!socket_receive_all_blocking(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_NICKNAME_SIZE_FROM_HOST));
      return false;
   }

   if (nick_size >= sizeof(netplay->other_nick))
   {
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_INVALID_NICKNAME_SIZE));
      return false;
   }

   if (!socket_receive_all_blocking(fd, netplay->other_nick, nick_size))
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_RECEIVE_NICKNAME));
      return false;
   }

   return true;
}
bool netplay_send_nickname(netplay_t *netplay, int fd)
{
   uint8_t nick_size = strlen(netplay->nick);

   if (!socket_send_all_blocking(fd, &nick_size, sizeof(nick_size), false))
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_NICKNAME_SIZE));
      return false;
   }

   if (!socket_send_all_blocking(fd, netplay->nick, nick_size, false))
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_FAILED_TO_SEND_NICKNAME));
      return false;
   }

   return true;
}

/**
 * netplay_impl_magic:
 *
 * Not really a hash, but should be enough to differentiate 
 * implementations from each other.
 *
 * Subtle differences in the implementation will not be possible to spot.
 * The alternative would have been checking serialization sizes, but it 
 * was troublesome for cross platform compat.
 **/
uint32_t netplay_impl_magic(void)
{
   size_t i, len;
   retro_ctx_api_info_t api_info;
   unsigned api;
   uint32_t res                        = 0;
   rarch_system_info_t *info           = NULL;
   const char *lib                     = NULL;
   const char *ver                     = PACKAGE_VERSION;

   core_api_version(&api_info);

   api                                 = api_info.version;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   res |= api;

   if (info)
   {
      lib = info->info.library_name;

      len = strlen(lib);
      for (i = 0; i < len; i++)
         res ^= lib[i] << (i & 0xf);

      lib = info->info.library_version;
      len = strlen(lib);

      for (i = 0; i < len; i++)
         res ^= lib[i] << (i & 0xf);
   }

   len = strlen(ver);
   for (i = 0; i < len; i++)
      res ^= ver[i] << ((i & 0xf) + 16);

   res ^= NETPLAY_PROTOCOL_VERSION << 24;

   return res;
}

/**
 * netplay_platform_magic
 *
 * Just enough info to tell us if our platforms mismatch: Endianness and a
 * couple of type sizes.
 *
 * Format:
 *    bit 31:     Reserved
 *    bit 30:     1 for big endian
 *    bits 29-15: sizeof(size_t)
 *    bits 14-0:  sizeof(long)
 */
static uint32_t netplay_platform_magic(void)
{
   uint32_t ret =
       ((1 == htonl(1)) << 30)
      |(sizeof(size_t) << 15)
      |(sizeof(long));
   return ret;
}

/**
 * netplay_endian_mismatch
 *
 * Do the platform magics mismatch on endianness?
 */
static bool netplay_endian_mismatch(uint32_t pma, uint32_t pmb)
{
   uint32_t ebit = (1<<30);
   return (pma & ebit) != (pmb & ebit);
}

bool netplay_handshake(netplay_t *netplay)
{
   unsigned sram_size, remote_sram_size;
   retro_ctx_memory_info_t mem_info;
   char msg[512]             = {0};
   uint32_t *content_crc_ptr = NULL;
   void *sram                = NULL;
   uint32_t header[4]        = {0};
   size_t i;
   uint32_t local_pmagic, remote_pmagic;
   bool is_server = netplay->is_server;

   mem_info.id = RETRO_MEMORY_SAVE_RAM;

   core_get_memory(&mem_info);
   content_get_crc(&content_crc_ptr);

   local_pmagic = netplay_platform_magic();

   header[0] = htonl(*content_crc_ptr);
   header[1] = htonl(netplay_impl_magic());
   header[2] = htonl(mem_info.size);
   header[3] = htonl(local_pmagic);

   if (!socket_send_all_blocking(netplay->fd, header, sizeof(header), false))
      return false;

   if (!socket_receive_all_blocking(netplay->fd, header, sizeof(header)))
   {
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT));
      return false;
   }

   if (*content_crc_ptr != ntohl(header[0]))
   {
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_CONTENT_CRC32S_DIFFER));
      return false;
   }

   if (netplay_impl_magic() != ntohl(header[1]))
   {
      RARCH_ERR("Implementations differ, make sure you're using exact same "
            "libretro implementations and RetroArch version.\n");
      return false;
   }

   /* Some cores only report the correct sram size late, so we can't actually
    * error out if the sram size seems wrong. */
   sram_size = mem_info.size;
   remote_sram_size = ntohl(header[2]);
   if (sram_size != 0 && remote_sram_size != 0 && sram_size != remote_sram_size)
   {
      RARCH_WARN("Content SRAM sizes do not correspond.\n");
   }

   /* We only care about platform magic if our core is quirky */
   remote_pmagic = ntohl(header[3]);
   if ((netplay->quirks & NETPLAY_QUIRK_ENDIAN_DEPENDENT) &&
       netplay_endian_mismatch(local_pmagic, remote_pmagic))
   {
      RARCH_ERR("Endianness mismatch with an endian-sensitive core.\n");
      return false;
   }
   if ((netplay->quirks & NETPLAY_QUIRK_PLATFORM_DEPENDENT) &&
       (local_pmagic != remote_pmagic))
   {
      RARCH_ERR("Platform mismatch with a platform-sensitive core.\n");
      return false;
   }

   /* Client sends nickname first, server replies with nickname */
   if (!is_server)
   {
      if (!netplay_send_nickname(netplay, netplay->fd))
      {
         RARCH_ERR("%s\n",
               msg_hash_to_str(MSG_FAILED_TO_SEND_NICKNAME_TO_HOST));
         return false;
      }
   }

   if (!netplay_get_nickname(netplay, netplay->fd))
   {
      if (is_server)
         RARCH_ERR("%s\n",
               msg_hash_to_str(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT));
      else
         RARCH_ERR("%s\n", 
               msg_hash_to_str(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST));
      return false;
   }

   if (is_server)
   {
      if (!netplay_send_nickname(netplay, netplay->fd))
      {
         RARCH_ERR("%s\n",
               msg_hash_to_str(MSG_FAILED_TO_SEND_NICKNAME_TO_CLIENT));
         return false;
      }
   }

   /* Server sends SRAM, client receives */
   if (is_server)
   {
      /* Send SRAM data to the client */
      sram      = mem_info.data;

      if (!socket_send_all_blocking(netplay->fd, sram, sram_size, false))
      {
         RARCH_ERR("%s\n",
               msg_hash_to_str(MSG_FAILED_TO_SEND_SRAM_DATA_TO_CLIENT));
         return false;
      }

   }
   else
   {
      /* Get SRAM data from User 1. */
      if (sram_size != 0 && sram_size == remote_sram_size)
      {
         sram      = mem_info.data;

         if (!socket_receive_all_blocking(netplay->fd, sram, sram_size))
         {
            RARCH_ERR("%s\n",
                  msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
            return false;
         }

      }
      else if (remote_sram_size != 0)
      {
         /* We can't load this, but we still need to get rid of the data */
         uint32_t quickbuf;
         while (remote_sram_size > 0)
         {
            if (!socket_receive_all_blocking(netplay->fd, &quickbuf, (remote_sram_size > sizeof(uint32_t)) ? sizeof(uint32_t) : remote_sram_size))
            {
               RARCH_ERR("%s\n",
                     msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
               return false;
            }
            if (remote_sram_size > sizeof(uint32_t))
               remote_sram_size -= sizeof(uint32_t);
            else
               remote_sram_size = 0;
         }

      }

   }

   /* Reset our frame count so it's consistent with the server */
   netplay->self_frame_count = netplay->other_frame_count = 0;
   netplay->read_frame_count = 1;
   for (i = 0; i < netplay->buffer_size; i++)
   {
      netplay->buffer[i].used = false;
      if (i == netplay->self_ptr)
      {
         netplay_delta_frame_ready(netplay, &netplay->buffer[i], 0);
         netplay->buffer[i].have_remote = true;
         netplay->other_ptr = i;
         netplay->read_ptr = NEXT_PTR(i);
      }
      else
      {
         netplay->buffer[i].used = false;
      }
   }

   if (is_server)
   {
      netplay_log_connection(&netplay->other_addr, 0, netplay->other_nick);
   }
   else
   {
      snprintf(msg, sizeof(msg), "%s: \"%s\"",
            msg_hash_to_str(MSG_CONNECTED_TO),
            netplay->other_nick);
      RARCH_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
   }

   return true;
}

bool netplay_is_server(netplay_t* netplay)
{
   if (!netplay)
      return false;
   return netplay->is_server;
}

bool netplay_is_spectate(netplay_t* netplay)
{
   if (!netplay)
      return false;
   return netplay->spectate.enabled;
}

bool netplay_delta_frame_ready(netplay_t *netplay, struct delta_frame *delta, uint32_t frame)
{
   void *remember_state;
   if (delta->used)
   {
      if (delta->frame == frame) return true;
      if (netplay->other_frame_count <= delta->frame)
      {
         /* We haven't even replayed this frame yet, so we can't overwrite it! */
         return false;
      }
   }
   remember_state = delta->state;
   memset(delta, 0, sizeof(struct delta_frame));
   delta->used = true;
   delta->frame = frame;
   delta->state = remember_state;
   return true;
}

uint32_t netplay_delta_frame_crc(netplay_t *netplay, struct delta_frame *delta)
{
   if (!netplay->state_size)
      return 0;
   return encoding_crc32(0L, (const unsigned char*)delta->state, netplay->state_size);
}

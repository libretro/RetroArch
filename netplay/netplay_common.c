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
#include "../content.h"

bool np_get_nickname(netplay_t *netplay, int fd)
{
   uint8_t nick_size;

   if (!socket_receive_all_blocking(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("Failed to receive nick size from host.\n");
      return false;
   }

   if (nick_size >= sizeof(netplay->other_nick))
   {
      RARCH_ERR("Invalid nick size.\n");
      return false;
   }

   if (!socket_receive_all_blocking(fd, netplay->other_nick, nick_size))
   {
      RARCH_ERR("Failed to receive nick.\n");
      return false;
   }

   return true;
}
bool np_send_nickname(netplay_t *netplay, int fd)
{
   uint8_t nick_size = strlen(netplay->nick);

   if (!socket_send_all_blocking(fd, &nick_size, sizeof(nick_size)))
   {
      RARCH_ERR("Failed to send nick size.\n");
      return false;
   }

   if (!socket_send_all_blocking(fd, netplay->nick, nick_size))
   {
      RARCH_ERR("Failed to send nick.\n");
      return false;
   }

   return true;
}

uint32_t *np_bsv_header_generate(size_t *size, uint32_t magic)
{
   retro_ctx_serialize_info_t serial_info;
   retro_ctx_size_info_t info;
   uint32_t *content_crc_ptr;
   size_t serialize_size, header_size;
   uint32_t *header, bsv_header[4] = {0};

   core_ctl(CORE_CTL_RETRO_SERIALIZE_SIZE, &info);
   
   serialize_size = info.size;
   header_size    = sizeof(bsv_header) + serialize_size;
   *size          = header_size;
   header         = (uint32_t*)malloc(header_size);
   if (!header)
      goto error;

   content_ctl(CONTENT_CTL_GET_CRC, &content_crc_ptr);

   bsv_header[MAGIC_INDEX]      = swap_if_little32(BSV_MAGIC);
   bsv_header[SERIALIZER_INDEX] = swap_if_big32(magic);
   bsv_header[CRC_INDEX]        = swap_if_big32(*content_crc_ptr);
   bsv_header[STATE_SIZE_INDEX] = swap_if_big32(serialize_size);

   serial_info.data             = header + 4;
   serial_info.size             = serialize_size;

   if (serialize_size && !core_ctl(CORE_CTL_RETRO_SERIALIZE, &serial_info))
      goto error;

   memcpy(header, bsv_header, sizeof(bsv_header));
   return header;

error:
   if (header)
      free(header);
   return NULL;
}

bool np_bsv_parse_header(const uint32_t *header, uint32_t magic)
{
   retro_ctx_size_info_t info;
   uint32_t *content_crc_ptr;
   uint32_t in_crc, in_magic, in_state_size;
   uint32_t in_bsv = swap_if_little32(header[MAGIC_INDEX]);

   if (in_bsv != BSV_MAGIC)
   {
      RARCH_ERR("BSV magic mismatch, got 0x%x, expected 0x%x.\n",
            in_bsv, BSV_MAGIC);
      return false;
   }

   in_magic = swap_if_big32(header[SERIALIZER_INDEX]);
   if (in_magic != magic)
   {
      RARCH_ERR("Magic mismatch, got 0x%x, expected 0x%x.\n", in_magic, magic);
      return false;
   }

   in_crc = swap_if_big32(header[CRC_INDEX]);

   content_ctl(CONTENT_CTL_GET_CRC, &content_crc_ptr);

   if (in_crc != *content_crc_ptr)
   {
      RARCH_ERR("CRC32 mismatch, got 0x%x, expected 0x%x.\n", in_crc,
            *content_crc_ptr);
      return false;
   }

   core_ctl(CORE_CTL_RETRO_SERIALIZE_SIZE, &info);

   in_state_size = swap_if_big32(header[STATE_SIZE_INDEX]);
   if (in_state_size != info.size)
   {
      RARCH_ERR("Serialization size mismatch, got 0x%x, expected 0x%x.\n",
            (unsigned)in_state_size, (unsigned)info.size);
      return false;
   }

   return true;
}

/**
 * np_impl_magic:
 *
 * Not really a hash, but should be enough to differentiate 
 * implementations from each other.
 *
 * Subtle differences in the implementation will not be possible to spot.
 * The alternative would have been checking serialization sizes, but it 
 * was troublesome for cross platform compat.
 **/
uint32_t np_impl_magic(void)
{
   size_t i, len;
   uint32_t res                        = 0;
   rarch_system_info_t *info           = NULL;
   const char *lib                     = NULL;
   const char *ver                     = PACKAGE_VERSION;
   unsigned api                        = core.retro_api_version();
   
   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);
   
   if (info)
      lib = info->info.library_name;

   res |= api;

   len = strlen(lib);
   for (i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   lib = info->info.library_version;
   len = strlen(lib);

   for (i = 0; i < len; i++)
      res ^= lib[i] << (i & 0xf);

   len = strlen(ver);
   for (i = 0; i < len; i++)
      res ^= ver[i] << ((i & 0xf) + 16);

   return res;
}

bool np_send_info(netplay_t *netplay)
{
   unsigned sram_size;
   retro_ctx_memory_info_t mem_info;
   char msg[512]             = {0};
   uint32_t *content_crc_ptr = NULL;
   void *sram                = NULL;
   uint32_t header[3]        = {0};

   mem_info.id = RETRO_MEMORY_SAVE_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);
   content_ctl(CONTENT_CTL_GET_CRC, &content_crc_ptr);
   
   header[0] = htonl(*content_crc_ptr);
   header[1] = htonl(np_impl_magic());
   header[2] = htonl(mem_info.size);

   if (!socket_send_all_blocking(netplay->fd, header, sizeof(header)))
      return false;

   if (!np_send_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to send nick to host.\n");
      return false;
   }

   /* Get SRAM data from User 1. */
   sram      = mem_info.data;
   sram_size = mem_info.size;

   if (!socket_receive_all_blocking(netplay->fd, sram, sram_size))
   {
      RARCH_ERR("Failed to receive SRAM data from host.\n");
      return false;
   }

   if (!np_get_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to receive nick from host.\n");
      return false;
   }

   snprintf(msg, sizeof(msg), "Connected to: \"%s\"", netplay->other_nick);
   RARCH_LOG("%s\n", msg);
   runloop_msg_queue_push(msg, 1, 180, false);

   return true;
}

bool np_get_info(netplay_t *netplay)
{
   unsigned sram_size;
   uint32_t header[3];
   retro_ctx_memory_info_t mem_info;
   uint32_t *content_crc_ptr = NULL;
   const void *sram          = NULL;

   if (!socket_receive_all_blocking(netplay->fd, header, sizeof(header)))
   {
      RARCH_ERR("Failed to receive header from client.\n");
      return false;
   }

   content_ctl(CONTENT_CTL_GET_CRC, &content_crc_ptr);

   if (*content_crc_ptr != ntohl(header[0]))
   {
      RARCH_ERR("Content CRC32s differ. Cannot use different games.\n");
      return false;
   }

   if (np_impl_magic() != ntohl(header[1]))
   {
      RARCH_ERR("Implementations differ, make sure you're using exact same libretro implementations and RetroArch version.\n");
      return false;
   }

   mem_info.id = RETRO_MEMORY_SAVE_RAM;

   core_ctl(CORE_CTL_RETRO_GET_MEMORY, &mem_info);

   if (mem_info.size != ntohl(header[2]))
   {
      RARCH_ERR("Content SRAM sizes do not correspond.\n");
      return false;
   }

   if (!np_get_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to get nickname from client.\n");
      return false;
   }

   /* Send SRAM data to our User 2. */
   sram      = mem_info.data;
   sram_size = mem_info.size;

   if (!socket_send_all_blocking(netplay->fd, sram, sram_size))
   {
      RARCH_ERR("Failed to send SRAM data to client.\n");
      return false;
   }

   if (!np_send_nickname(netplay, netplay->fd))
   {
      RARCH_ERR("Failed to send nickname to client.\n");
      return false;
   }

#ifndef HAVE_SOCKET_LEGACY
   np_log_connection(&netplay->other_addr, 0, netplay->other_nick);
#endif

   return true;
}

bool np_is_server(netplay_t* netplay)
{
   return netplay->is_server;
}

bool np_is_spectate(netplay_t* netplay)
{
   return netplay->spectate.enabled;
}

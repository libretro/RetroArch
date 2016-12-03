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
#include <compat/strl.h>

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

bool netplay_handshake_init_send(netplay_t *netplay)
{
   uint32_t *content_crc_ptr = NULL;
   uint32_t header[4] = {0};

   content_get_crc(&content_crc_ptr);

   header[0] = htonl(netplay_impl_magic());
   header[1] = htonl(*content_crc_ptr);
   header[2] = htonl(netplay_platform_magic());
   header[3] = htonl(NETPLAY_COMPRESSION_SUPPORTED);

   if (!netplay_send(&netplay->send_packet_buffer, netplay->fd, header,
         sizeof(header)) ||
       !netplay_send_flush(&netplay->send_packet_buffer, netplay->fd, false))
      return false;

   return true;
}

struct nick_buf_s
{
   uint32_t cmd[2];
   char nick[32];
};

#define RECV(buf, sz) \
   recvd = netplay_recv(&netplay->recv_packet_buffer, netplay->fd, (buf), (sz), false); \
   if (recvd >= 0 && recvd < (sz)) \
   { \
      netplay_recv_reset(&netplay->recv_packet_buffer); \
      return true; \
   } \
   else if (recvd < 0)

bool netplay_handshake_init(netplay_t *netplay, bool *had_input)
{
   uint32_t header[4] = {0};
   ssize_t recvd;
   char msg[512];
   struct nick_buf_s nick_buf;
   uint32_t *content_crc_ptr = NULL;
   uint32_t local_pmagic, remote_pmagic;
   uint32_t compression;

   msg[0] = '\0';

   RECV(header, sizeof(header))
   {
      strlcpy(msg, msg_hash_to_str(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT), sizeof(msg));
      goto error;
   }

   if (netplay_impl_magic() != ntohl(header[0]))
   {
      strlcpy(msg, "Implementations differ. Make sure you're using exact same "
         "libretro implementations and RetroArch version.", sizeof(msg));
      goto error;
   }

   content_get_crc(&content_crc_ptr);
   if (*content_crc_ptr != ntohl(header[1]))
   {
      strlcpy(msg, msg_hash_to_str(MSG_CONTENT_CRC32S_DIFFER), sizeof(msg));
      goto error;
   }

   /* We only care about platform magic if our core is quirky */
   local_pmagic = netplay_platform_magic();
   remote_pmagic = ntohl(header[2]);
   if ((netplay->quirks & NETPLAY_QUIRK_ENDIAN_DEPENDENT) &&
       netplay_endian_mismatch(local_pmagic, remote_pmagic))
   {
      RARCH_ERR("Endianness mismatch with an endian-sensitive core.\n");
      strlcpy(msg, "This core does not support inter-architecture netplay "
         "between these systems.", sizeof(msg));
      goto error;
   }
   if ((netplay->quirks & NETPLAY_QUIRK_PLATFORM_DEPENDENT) &&
       (local_pmagic != remote_pmagic))
   {
      RARCH_ERR("Platform mismatch with a platform-sensitive core.\n");
      strlcpy(msg, "This core does not support inter-architecture netplay.",
         sizeof(msg));
      goto error;
   }

   /* Clear any existing compression */
   if (netplay->compression_stream)
      netplay->compression_backend->stream_free(netplay->compression_stream);
   if (netplay->decompression_stream)
      netplay->decompression_backend->stream_free(netplay->decompression_stream);

   /* Check what compression is supported */
   compression = ntohl(header[3]);
   compression &= NETPLAY_COMPRESSION_SUPPORTED;
   if (compression & NETPLAY_COMPRESSION_ZLIB)
   {
      netplay->compression_backend = trans_stream_get_zlib_deflate_backend();
      if (!netplay->compression_backend)
         netplay->compression_backend = trans_stream_get_pipe_backend();
   }
   else
   {
      netplay->compression_backend = trans_stream_get_pipe_backend();
   }
   netplay->decompression_backend = netplay->compression_backend->reverse;

   /* Allocate our compression stream */
   netplay->compression_stream = netplay->compression_backend->stream_new();
   netplay->decompression_stream = netplay->decompression_backend->stream_new();
   if (!netplay->compression_stream || !netplay->decompression_stream)
   {
      RARCH_ERR("Failed to allocate compression transcoder!\n");
      return false;
   }

   /* Send our nick */
   nick_buf.cmd[0] = htonl(NETPLAY_CMD_NICK);
   nick_buf.cmd[1] = htonl(sizeof(nick_buf.nick));
   memset(nick_buf.nick, 0, sizeof(nick_buf.nick));
   strlcpy(nick_buf.nick, netplay->nick, sizeof(nick_buf.nick));
   if (!netplay_send(&netplay->send_packet_buffer, netplay->fd, &nick_buf,
         sizeof(nick_buf)) ||
       !netplay_send_flush(&netplay->send_packet_buffer, netplay->fd, false))
      return false;

   /* Move on to the next mode */
   netplay->status = RARCH_NETPLAY_CONNECTION_PRE_NICK;
   *had_input = true;
   netplay_recv_flush(&netplay->recv_packet_buffer);
   return true;

error:
   if (msg[0])
   {
      RARCH_ERR("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
   }
   return false;
}

static void netplay_handshake_ready(netplay_t *netplay)
{
   size_t i;
   char msg[512];

   /* Reset our frame count so it's consistent between server and client */
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

   if (netplay->is_server)
   {
      netplay_log_connection(&netplay->other_addr, 0, netplay->other_nick);

      /* Send them the savestate */
      if (!(netplay->quirks & (NETPLAY_QUIRK_NO_SAVESTATES|NETPLAY_QUIRK_NO_TRANSMISSION)))
      {
         netplay->force_send_savestate = true;
      }
   }
   else
   {
      snprintf(msg, sizeof(msg), "%s: \"%s\"",
            msg_hash_to_str(MSG_CONNECTED_TO),
            netplay->other_nick);
      RARCH_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
   }

   /* Unstall if we were waiting for this */
   if (netplay->stall == RARCH_NETPLAY_STALL_NO_CONNECTION)
       netplay->stall = 0;

   netplay->status = RARCH_NETPLAY_CONNECTION_PLAYING;
}

bool netplay_handshake_pre_nick(netplay_t *netplay, bool *had_input)
{
   struct nick_buf_s nick_buf;
   ssize_t recvd;
   char msg[512];

   msg[0] = '\0';

   RECV(&nick_buf, sizeof(nick_buf));

   /* Expecting only a nick command */
   if (recvd < 0 ||
       ntohl(nick_buf.cmd[0]) != NETPLAY_CMD_NICK ||
       ntohl(nick_buf.cmd[1]) != sizeof(nick_buf.nick))
   {
      if (netplay->is_server)
         strlcpy(msg, msg_hash_to_str(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT),
            sizeof(msg));
      else
         strlcpy(msg,
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST),
            sizeof(msg));
      RARCH_ERR("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
      return false;
   }

   strlcpy(netplay->other_nick, nick_buf.nick,
      (sizeof(netplay->other_nick) < sizeof(nick_buf.nick)) ?
      sizeof(netplay->other_nick) : sizeof(nick_buf.nick));

   if (netplay->is_server)
   {
      /* If we're the server, now we send our SRAM */
      uint32_t cmd[2];
      retro_ctx_memory_info_t mem_info;

      mem_info.id = RETRO_MEMORY_SAVE_RAM;
      core_get_memory(&mem_info);

      cmd[0] = htonl(NETPLAY_CMD_SRAM);
      cmd[1] = htonl(mem_info.size);

      if (!netplay_send(&netplay->send_packet_buffer, netplay->fd, cmd,
            sizeof(cmd)))
         return false;
      if (!netplay_send(&netplay->send_packet_buffer, netplay->fd,
            mem_info.data, mem_info.size) ||
          !netplay_send_flush(&netplay->send_packet_buffer, netplay->fd,
            false))
         return false;

      /* Now we're ready! */
      netplay_handshake_ready(netplay);

   }
   else
   {
      /* Client needs to wait for SRAM */
      netplay->status = RARCH_NETPLAY_CONNECTION_PRE_SRAM;

   }

   *had_input = true;
   netplay_recv_flush(&netplay->recv_packet_buffer);
   return true;
}

bool netplay_handshake_pre_sram(netplay_t *netplay, bool *had_input)
{
   uint32_t cmd[2];
   uint32_t local_sram_size, remote_sram_size;
   ssize_t recvd;
   retro_ctx_memory_info_t mem_info;

   RECV(cmd, sizeof(cmd))
      return false;

   /* Only expecting an SRAM command */
   if (ntohl(cmd[0]) != NETPLAY_CMD_SRAM)
   {
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
      return false;
   }

   mem_info.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&mem_info);

   local_sram_size = mem_info.size;
   remote_sram_size = ntohl(cmd[1]);

   if (local_sram_size != 0 && local_sram_size == remote_sram_size)
   {
      RECV(mem_info.data, local_sram_size)
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
         RECV(&quickbuf, (remote_sram_size > sizeof(uint32_t)) ? sizeof(uint32_t) : remote_sram_size)
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

   /* We're ready! */
   netplay_handshake_ready(netplay);
   *had_input = true;
   netplay_recv_flush(&netplay->recv_packet_buffer);
   return true;
}

#if 0
bool netplay_handshake(netplay_t *netplay)
{
   size_t i;
   uint32_t local_pmagic, remote_pmagic;
   unsigned sram_size, remote_sram_size;
   retro_ctx_memory_info_t mem_info;
   char msg[512];
   uint32_t *content_crc_ptr = NULL;
   void *sram                = NULL;
   uint32_t header[5]        = {0};
   bool is_server            = netplay->is_server;
   int compression           = 0;

   msg[0] = '\0';

   mem_info.id = RETRO_MEMORY_SAVE_RAM;

   core_get_memory(&mem_info);
   content_get_crc(&content_crc_ptr);

   local_pmagic = netplay_platform_magic();

   header[0] = htonl(*content_crc_ptr);
   header[1] = htonl(netplay_impl_magic());
   header[2] = htonl(mem_info.size);
   header[3] = htonl(local_pmagic);
   header[4] = htonl(NETPLAY_COMPRESSION_SUPPORTED);

   if (!socket_send_all_blocking(netplay->fd, header, sizeof(header), false))
      return false;

   if (!socket_receive_all_blocking(netplay->fd, header, sizeof(header)))
   {
      strlcpy(msg, msg_hash_to_str(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT), sizeof(msg));
      goto error;
   }

   if (*content_crc_ptr != ntohl(header[0]))
   {
      strlcpy(msg, msg_hash_to_str(MSG_CONTENT_CRC32S_DIFFER), sizeof(msg));
      goto error;
   }

   if (netplay_impl_magic() != ntohl(header[1]))
   {
      strlcpy(msg, "Implementations differ. Make sure you're using exact same "
         "libretro implementations and RetroArch version.", sizeof(msg));
      goto error;
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
      strlcpy(msg, "This core does not support inter-architecture netplay "
         "between these systems.", sizeof(msg));
      goto error;
   }
   if ((netplay->quirks & NETPLAY_QUIRK_PLATFORM_DEPENDENT) &&
       (local_pmagic != remote_pmagic))
   {
      RARCH_ERR("Platform mismatch with a platform-sensitive core.\n");
      strlcpy(msg, "This core does not support inter-architecture netplay.",
         sizeof(msg));
      goto error;
   }

   /* Clear any existing compression */
   if (netplay->compression_stream)
      netplay->compression_backend->stream_free(netplay->compression_stream);
   if (netplay->decompression_stream)
      netplay->decompression_backend->stream_free(netplay->decompression_stream);

   /* Check what compression is supported */
   compression = ntohl(header[4]);
   compression &= NETPLAY_COMPRESSION_SUPPORTED;
   if (compression & NETPLAY_COMPRESSION_ZLIB)
   {
      netplay->compression_backend = trans_stream_get_zlib_deflate_backend();
      if (!netplay->compression_backend)
         netplay->compression_backend = trans_stream_get_pipe_backend();
   }
   else
   {
      netplay->compression_backend = trans_stream_get_pipe_backend();
   }
   netplay->decompression_backend = netplay->compression_backend->reverse;

   /* Allocate our compression stream */
   netplay->compression_stream = netplay->compression_backend->stream_new();
   netplay->decompression_stream = netplay->decompression_backend->stream_new();
   if (!netplay->compression_stream || !netplay->decompression_stream)
   {
      RARCH_ERR("Failed to allocate compression transcoder!\n");
      return false;
   }

   /* Client sends nickname first, server replies with nickname */
   if (!is_server)
   {
      if (!netplay_send_nickname(netplay, netplay->fd))
      {
         strlcpy(msg, msg_hash_to_str(MSG_FAILED_TO_SEND_NICKNAME_TO_HOST),
            sizeof(msg));
         goto error;
      }
   }

   if (!netplay_get_nickname(netplay, netplay->fd))
   {
      if (is_server)
         strlcpy(msg, msg_hash_to_str(MSG_FAILED_TO_GET_NICKNAME_FROM_CLIENT),
            sizeof(msg));
      else
         strlcpy(msg,
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_NICKNAME_FROM_HOST),
            sizeof(msg));
      goto error;
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

error:
   if (msg[0])
   {
      RARCH_ERR("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
   }
   return false;
}
#endif

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

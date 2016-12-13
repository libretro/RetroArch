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
#include <rhash.h>
#include <compat/strl.h>

#include <encodings/crc32.h>

#include "../../movie.h"
#include "../../msg_hash.h"
#include "../../configuration.h"
#include "../../content.h"
#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../version.h"
#include "../../menu/widgets/menu_input_dialog.h"

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

bool netplay_handshake_init_send(netplay_t *netplay, struct netplay_connection *connection)
{
   uint32_t *content_crc_ptr = NULL;
   uint32_t header[5] = {0};

   content_get_crc(&content_crc_ptr);

   header[0] = htonl(netplay_impl_magic());
   header[1] = htonl(*content_crc_ptr);
   header[2] = htonl(netplay_platform_magic());
   header[3] = htonl(NETPLAY_COMPRESSION_SUPPORTED);
   if (netplay->is_server && netplay->password[0])
   {
      /* Demand a password */
      /* FIXME: Better randomness, or at least seed it */
      connection->salt = rand();
      if (connection->salt == 0) connection->salt = 1;
      header[4] = htonl(connection->salt);
   }
   else
   {
      header[4] = htonl(0);
   }

   if (!netplay_send(&connection->send_packet_buffer, connection->fd, header,
         sizeof(header)) ||
       !netplay_send_flush(&connection->send_packet_buffer, connection->fd, false))
      return false;

   return true;
}

struct nick_buf_s
{
   uint32_t cmd[2];
   char nick[32];
};

struct password_buf_s
{
   uint32_t cmd[2];
   char password[64];
};

#define RECV(buf, sz) \
   recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd, (buf), (sz), false); \
   if (recvd >= 0 && recvd < (sz)) \
   { \
      netplay_recv_reset(&connection->recv_packet_buffer); \
      return true; \
   } \
   else if (recvd < 0)

static netplay_t *handshake_password_netplay;

static void handshake_password(void *ignore, const char *line)
{
   struct password_buf_s password_buf;
   char password[8+128]; /* 8 for salt, 128 for password */
   uint32_t cmd[2];
   netplay_t *netplay = handshake_password_netplay;
   struct netplay_connection *connection = &netplay->connections[0];

   snprintf(password, sizeof(password), "%08X", connection->salt);
   strlcpy(password + 8, line, sizeof(password)-8);

   password_buf.cmd[0] = htonl(NETPLAY_CMD_PASSWORD);
   password_buf.cmd[1] = htonl(sizeof(password_buf.password));
   sha256_hash(password_buf.password, (uint8_t *) password, strlen(password));

   netplay_send(&connection->send_packet_buffer, connection->fd, &password_buf, sizeof(password_buf)) &&
   netplay_send_flush(&connection->send_packet_buffer, connection->fd, false);

   menu_input_dialog_end();
   rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
}

bool netplay_handshake_init(netplay_t *netplay, struct netplay_connection *connection, bool *had_input)
{
   uint32_t header[5] = {0};
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

   /* If a password is demanded, ask for it */
   if (!netplay->is_server && (connection->salt = ntohl(header[4])))
   {
      menu_input_ctx_line_t line;
      rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
      memset(&line, 0, sizeof(line));
      handshake_password_netplay = netplay;
      line.label = "Enter Netplay server password:";
      line.label_setting = "no_setting";
      line.cb = handshake_password;
      menu_input_dialog_start(&line);
   }

   /* Send our nick */
   nick_buf.cmd[0] = htonl(NETPLAY_CMD_NICK);
   nick_buf.cmd[1] = htonl(sizeof(nick_buf.nick));
   memset(nick_buf.nick, 0, sizeof(nick_buf.nick));
   strlcpy(nick_buf.nick, netplay->nick, sizeof(nick_buf.nick));
   if (!netplay_send(&connection->send_packet_buffer, connection->fd, &nick_buf,
         sizeof(nick_buf)) ||
       !netplay_send_flush(&connection->send_packet_buffer, connection->fd, false))
      return false;

   /* Move on to the next mode */
   connection->mode = NETPLAY_CONNECTION_PRE_NICK;
   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;

error:
   if (msg[0])
   {
      RARCH_ERR("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
   }
   return false;
}

static void netplay_handshake_ready(netplay_t *netplay, struct netplay_connection *connection)
{
   size_t i;
   char msg[512];

   if (netplay->is_server)
   {
      netplay_log_connection(&connection->addr, connection - netplay->connections, connection->nick);

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
            connection->nick);
      RARCH_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
   }

   /* Unstall if we were waiting for this */
   if (netplay->stall == NETPLAY_STALL_NO_CONNECTION)
       netplay->stall = 0;
}

bool netplay_handshake_sync(netplay_t *netplay, struct netplay_connection *connection)
{
   /* If we're the server, now we send sync info */
   uint32_t cmd[5];
   uint32_t connected_players;
   settings_t *settings = config_get_ptr();
   size_t i;
   uint32_t device;
   retro_ctx_memory_info_t mem_info;

   mem_info.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&mem_info);

   /* Send basic sync info */
   cmd[0] = htonl(NETPLAY_CMD_SYNC);
   cmd[1] = htonl(3*sizeof(uint32_t) + MAX_USERS*sizeof(uint32_t) + mem_info.size);
   cmd[2] = htonl(netplay->self_frame_count);
   connected_players = netplay->connected_players;
   if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
      connected_players |= 1<<netplay->self_player;
   cmd[3] = htonl(connected_players);
   if (netplay->flip)
      cmd[4] = htonl(netplay->flip_frame);
   else
      cmd[4] = htonl(0);

   if (!netplay_send(&connection->send_packet_buffer, connection->fd, cmd,
            sizeof(cmd)))
      return false;

   /* Now send the device info */
   for (i = 0; i < MAX_USERS; i++)
   {
      device = htonl(settings->input.libretro_device[i]);
      if (!netplay_send(&connection->send_packet_buffer, connection->fd,
               &device, sizeof(device)))
         return false;
   }

   /* And finally, the SRAM */
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
            mem_info.data, mem_info.size) ||
         !netplay_send_flush(&connection->send_packet_buffer, connection->fd,
            false))
      return false;

   /* Now we're ready! */
   connection->mode = NETPLAY_CONNECTION_SPECTATING;
   netplay_handshake_ready(netplay, connection);

   return true;
}

bool netplay_handshake_pre_nick(netplay_t *netplay, struct netplay_connection *connection, bool *had_input)
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

   strlcpy(connection->nick, nick_buf.nick,
      (sizeof(connection->nick) < sizeof(nick_buf.nick)) ?
      sizeof(connection->nick) : sizeof(nick_buf.nick));

   if (netplay->is_server)
   {
      if (netplay->password[0])
      {
         /* There's a password, so just put them in PRE_PASSWORD mode */
         connection->mode = NETPLAY_CONNECTION_PRE_PASSWORD;
      }
      else
      {
         if (!netplay_handshake_sync(netplay, connection))
            return false;
      }

   }
   else
   {
      /* Client needs to wait for sync info */
      connection->mode = NETPLAY_CONNECTION_PRE_SYNC;

   }

   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;
}

bool netplay_handshake_pre_password(netplay_t *netplay, struct netplay_connection *connection, bool *had_input)
{
   struct password_buf_s password_buf, corr_password_buf;
   char password[8+128]; /* 8 for salt, 128 for password */
   ssize_t recvd;
   char msg[512];

   msg[0] = '\0';

   RECV(&password_buf, sizeof(password_buf));

   /* Expecting only a password command */
   if (recvd < 0 ||
       ntohl(password_buf.cmd[0]) != NETPLAY_CMD_PASSWORD ||
       ntohl(password_buf.cmd[1]) != sizeof(password_buf.password))
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

   /* Calculate the correct password */
   snprintf(password, sizeof(password), "%08X", connection->salt);
   strlcpy(password + 8, netplay->password, sizeof(password)-8);
   sha256_hash(corr_password_buf.password, (uint8_t *) password, strlen(password));

   /* Compare them */
   if (memcmp(password_buf.password, corr_password_buf.password, sizeof(password_buf.password)))
      return false;

   if (!netplay_handshake_sync(netplay, connection))
      return false;

   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;
}

bool netplay_handshake_pre_sync(netplay_t *netplay, struct netplay_connection *connection, bool *had_input)
{
   uint32_t cmd[2];
   uint32_t new_frame_count, connected_players, flip_frame;
   uint32_t device;
   uint32_t local_sram_size, remote_sram_size;
   size_t i;
   ssize_t recvd;
   settings_t *settings = config_get_ptr();
   retro_ctx_controller_info_t pad;
   retro_ctx_memory_info_t mem_info;

   RECV(cmd, sizeof(cmd))
      return false;

   /* Only expecting a sync command */
   if (ntohl(cmd[0]) != NETPLAY_CMD_SYNC ||
       ntohl(cmd[1]) < 3*sizeof(uint32_t) + MAX_USERS*sizeof(uint32_t))
   {
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
      return false;
   }

   /* Get the frame count */
   RECV(&new_frame_count, sizeof(new_frame_count))
      return false;
   new_frame_count = ntohl(new_frame_count);

   /* Get the connected players */
   RECV(&connected_players, sizeof(connected_players))
      return false;
   connected_players = ntohl(connected_players);
   netplay->connected_players = connected_players;

   /* And the flip state */
   RECV(&flip_frame, sizeof(flip_frame))
      return false;
   flip_frame = ntohl(flip_frame);
   netplay->flip = !!flip_frame;
   netplay->flip_frame = flip_frame;

   /* Set our frame counters as requested */
   netplay->self_frame_count = netplay->other_frame_count =
      netplay->unread_frame_count = netplay->server_frame_count =
      new_frame_count;
   for (i = 0; i < netplay->buffer_size; i++)
   {
      struct delta_frame *ptr = &netplay->buffer[i];
      ptr->used = false;

      if (i == netplay->self_ptr)
      {
         /* Clear out any current data but still use this frame */
         netplay_delta_frame_ready(netplay, ptr, 0);
         ptr->frame = new_frame_count;
         ptr->have_local = true;
         netplay->other_ptr = netplay->unread_ptr = netplay->server_ptr = i;

      }
   }
   for (i = 0; i < MAX_USERS; i++)
   {
      if (connected_players & (1<<i))
      {
         netplay->read_ptr[i] = netplay->self_ptr;
         netplay->read_frame_count[i] = netplay->self_frame_count;
      }
   }

   /* Get and set each pad */
   for (i = 0; i < MAX_USERS; i++)
   {
      RECV(&device, sizeof(device))
         return false;
      pad.port = i;
      pad.device = ntohl(device);
      core_set_controller_port_device(&pad);
   }

   /* Now check the SRAM */
   mem_info.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&mem_info);

   local_sram_size = mem_info.size;
   remote_sram_size = ntohl(cmd[1]) - 3*sizeof(uint32_t) - MAX_USERS*sizeof(uint32_t);

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
   netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
   connection->mode = NETPLAY_CONNECTION_PLAYING;
   netplay_handshake_ready(netplay, connection);
   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);

   /* Ask to go to player mode */
   return netplay_cmd_mode(netplay, connection, NETPLAY_CONNECTION_PLAYING);
}

bool netplay_is_server(netplay_t* netplay)
{
   if (!netplay)
      return false;
   return netplay->is_server;
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

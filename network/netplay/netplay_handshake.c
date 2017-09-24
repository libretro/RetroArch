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
#include <compat/strl.h>
#include <rhash.h>
#include <retro_timers.h>

#include "netplay_private.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../autosave.h"
#include "../../configuration.h"
#include "../../content.h"
#include "../../retroarch.h"
#include "../../version.h"
#include "../../input/input_driver.h"

#ifdef HAVE_MENU
#include "../../menu/widgets/menu_input_dialog.h"
#endif

/* TODO/FIXME - replace netplay_log_connection with calls
 * to inet_ntop_compat and move runloop message queue pushing
 * outside */
#if !defined(HAVE_SOCKET_LEGACY) && !defined(WIIU)
/* Custom inet_ntop. Win32 doesn't seem to support this ... */
void netplay_log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick, char *s, size_t len)
{
   union
   {
      const struct sockaddr_storage *storage;
      const struct sockaddr_in *v4;
      const struct sockaddr_in6 *v6;
   } u;
   const char *str               = NULL;
   char buf_v4[INET_ADDRSTRLEN]  = {0};
   char buf_v6[INET6_ADDRSTRLEN] = {0};

   u.storage                     = their_addr;

   switch (their_addr->ss_family)
   {
      case AF_INET:
         {
            struct sockaddr_in in;

            memset(&in, 0, sizeof(in));

            str           = buf_v4;
            in.sin_family = AF_INET;
            memcpy(&in.sin_addr, &u.v4->sin_addr, sizeof(struct in_addr));

            getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in),
                  buf_v4, sizeof(buf_v4),
                  NULL, 0, NI_NUMERICHOST);
         }
         break;
      case AF_INET6:
         {
            struct sockaddr_in6 in;
            memset(&in, 0, sizeof(in));

            str            = buf_v6;
            in.sin6_family = AF_INET6;
            memcpy(&in.sin6_addr, &u.v6->sin6_addr, sizeof(struct in6_addr));

            getnameinfo((struct sockaddr*)&in, sizeof(struct sockaddr_in6),
                  buf_v6, sizeof(buf_v6), NULL, 0, NI_NUMERICHOST);
         }
         break;
      default:
         break;
   }

   if (str)
   {
      snprintf(s, len, msg_hash_to_str(MSG_GOT_CONNECTION_FROM_NAME),
            nick, str);
   }
   else
   {
      snprintf(s, len, msg_hash_to_str(MSG_GOT_CONNECTION_FROM),
            nick);
   }
}

#else
void netplay_log_connection(const struct sockaddr_storage *their_addr,
      unsigned slot, const char *nick, char *s, size_t len)
{
   /* Stub code - will need to be implemented */
   snprintf(s, len, msg_hash_to_str(MSG_GOT_CONNECTION_FROM),
         nick);
}

#endif

/**
 * netplay_impl_magic:
 *
 * A pseudo-hash of the RetroArch and Netplay version, so only compatible
 * versions play together.
 */
uint32_t netplay_impl_magic(void)
{
   size_t i;
   uint32_t res                        = 0;
   const char *ver                     = PACKAGE_VERSION;
   size_t len                          = strlen(ver);

   for (i = 0; i < len; i++)
      res ^= ver[i] << (i & 0xf);

   res ^= NETPLAY_PROTOCOL_VERSION << (i & 0xf);

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

static unsigned long simple_rand_next = 1;

static int simple_rand(void)
{
   simple_rand_next = simple_rand_next * 1103515245 + 12345;
   return((unsigned)(simple_rand_next/65536) % 32768);
}

static void simple_srand(unsigned int seed)
{
   simple_rand_next = seed;
}

static uint32_t simple_rand_uint32(void)
{
   uint32_t parts[3];
   parts[0] = simple_rand();
   parts[1] = simple_rand();
   parts[2] = simple_rand();
   return ((parts[0] << 30) +
           (parts[1] << 15) +
            parts[2]);
}

/**
 * netplay_handshake_init_send
 *
 * Initialize our handshake and send the first part of the handshake protocol.
 */
bool netplay_handshake_init_send(netplay_t *netplay,
   struct netplay_connection *connection)
{
   uint32_t header[4];
   settings_t *settings = config_get_ptr();

   header[0] = htonl(netplay_impl_magic());
   header[1] = htonl(netplay_platform_magic());
   header[2] = htonl(NETPLAY_COMPRESSION_SUPPORTED);
   header[3] = 0;

   if (netplay->is_server &&
       (settings->paths.netplay_password[0] || 
        settings->paths.netplay_spectate_password[0]))
   {
      /* Demand a password */
      if (simple_rand_next == 1)
         simple_srand((unsigned int) time(NULL));
      connection->salt = simple_rand_uint32();
      if (connection->salt == 0) connection->salt = 1;
      header[3] = htonl(connection->salt);
   }
   else
   {
      header[3] = htonl(0);
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
   char nick[NETPLAY_NICK_LEN];
};

struct password_buf_s
{
   uint32_t cmd[2];
   char password[NETPLAY_PASS_HASH_LEN];
};

struct info_buf_s
{
   uint32_t cmd[2];
   char core_name[NETPLAY_NICK_LEN];
   char core_version[NETPLAY_NICK_LEN];
   uint32_t content_crc;
};

#define RECV(buf, sz) \
   recvd = netplay_recv(&connection->recv_packet_buffer, connection->fd, (buf), (sz), false); \
   if (recvd >= 0 && recvd < (ssize_t) (sz)) \
   { \
      netplay_recv_reset(&connection->recv_packet_buffer); \
      return true; \
   } \
   else if (recvd < 0)

static netplay_t *handshake_password_netplay = NULL;

#ifdef HAVE_MENU
static void handshake_password(void *ignore, const char *line)
{
   struct password_buf_s password_buf;
   char password[8+NETPLAY_PASS_LEN]; /* 8 for salt, 128 for password */
   netplay_t *netplay = handshake_password_netplay;
   struct netplay_connection *connection = &netplay->connections[0];

   snprintf(password, sizeof(password), "%08X", connection->salt);
   strlcpy(password + 8, line, sizeof(password)-8);

   password_buf.cmd[0] = htonl(NETPLAY_CMD_PASSWORD);
   password_buf.cmd[1] = htonl(sizeof(password_buf.password));
   sha256_hash(password_buf.password, (uint8_t *) password, strlen(password));

   /* We have no way to handle an error here, so we'll let the next function error out */
   if (netplay_send(&connection->send_packet_buffer, connection->fd, &password_buf, sizeof(password_buf)))
      netplay_send_flush(&connection->send_packet_buffer, connection->fd, false);

#ifdef HAVE_MENU
   menu_input_dialog_end();
   rarch_menu_running_finished();
#endif
}
#endif

/**
 * netplay_handshake_init
 *
 * Data receiver for the initial part of the handshake, i.e., waiting for the
 * netplay header.
 */
bool netplay_handshake_init(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   ssize_t recvd;
   struct nick_buf_s nick_buf;
   uint32_t header[4];
   uint32_t local_pmagic                 = 0;
   uint32_t remote_pmagic                = 0;
   uint32_t compression                  = 0;
   struct compression_transcoder *ctrans = NULL;
   const char *dmsg                      = NULL;

   header[0] = 0;
   header[1] = 0;
   header[2] = 0;
   header[3] = 0;

   RECV(header, sizeof(header))
   {
      dmsg = msg_hash_to_str(MSG_FAILED_TO_RECEIVE_HEADER_FROM_CLIENT);
      goto error;
   }

   if (netplay_impl_magic() != ntohl(header[0]))
   {
      dmsg = msg_hash_to_str(MSG_NETPLAY_IMPLEMENTATIONS_DIFFER);
      goto error;
   }

   /* We only care about platform magic if our core is quirky */
   local_pmagic  = netplay_platform_magic();
   remote_pmagic = ntohl(header[1]);

   if ((netplay->quirks & NETPLAY_QUIRK_ENDIAN_DEPENDENT) &&
       netplay_endian_mismatch(local_pmagic, remote_pmagic))
   {
      RARCH_ERR("Endianness mismatch with an endian-sensitive core.\n");
      dmsg = msg_hash_to_str(MSG_NETPLAY_ENDIAN_DEPENDENT);
      goto error;
   }
   if ((netplay->quirks & NETPLAY_QUIRK_PLATFORM_DEPENDENT) &&
       (local_pmagic != remote_pmagic))
   {
      RARCH_ERR("Platform mismatch with a platform-sensitive core.\n");
      dmsg = msg_hash_to_str(MSG_NETPLAY_PLATFORM_DEPENDENT);
      goto error;
   }

   /* Check what compression is supported */
   compression  = ntohl(header[2]);
   compression &= NETPLAY_COMPRESSION_SUPPORTED;

   if (compression & NETPLAY_COMPRESSION_ZLIB)
   {
      ctrans = &netplay->compress_zlib;
      if (!ctrans->compression_backend)
      {
         ctrans->compression_backend =
            trans_stream_get_zlib_deflate_backend();
         if (!ctrans->compression_backend)
            ctrans->compression_backend = trans_stream_get_pipe_backend();
      }
      connection->compression_supported = NETPLAY_COMPRESSION_ZLIB;
   }
   else
   {
      ctrans = &netplay->compress_nil;
      if (!ctrans->compression_backend)
      {
         ctrans->compression_backend =
            trans_stream_get_pipe_backend();
      }
      connection->compression_supported = 0;
   }

   if (!ctrans->decompression_backend)
      ctrans->decompression_backend = ctrans->compression_backend->reverse;

   /* Allocate our compression stream */
   if (!ctrans->compression_stream)
   {
      ctrans->compression_stream   = ctrans->compression_backend->stream_new();
      ctrans->decompression_stream = ctrans->decompression_backend->stream_new();
   }
   if (!ctrans->compression_stream || !ctrans->decompression_stream)
   {
      RARCH_ERR("Failed to allocate compression transcoder!\n");
      return false;
   }

   /* If a password is demanded, ask for it */
   if (!netplay->is_server && (connection->salt = ntohl(header[3])))
   {
#ifdef HAVE_MENU
      menu_input_ctx_line_t line;
      rarch_menu_running();
#endif

      handshake_password_netplay = netplay;

#ifdef HAVE_MENU
      memset(&line, 0, sizeof(line));
      line.label         = msg_hash_to_str(MSG_NETPLAY_ENTER_PASSWORD);
      line.label_setting = "no_setting";
      line.cb            = handshake_password;
      if (!menu_input_dialog_start(&line))
         return false;
#endif
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
   if (dmsg)
   {
      RARCH_ERR("%s\n", dmsg);
      runloop_msg_queue_push(dmsg, 1, 180, false);
   }
   return false;
}

static void netplay_handshake_ready(netplay_t *netplay,
      struct netplay_connection *connection)
{
   char msg[512];
   msg[0] = '\0';

   if (netplay->is_server)
   {
      unsigned slot = (unsigned)(connection - netplay->connections);
      
      netplay_log_connection(&connection->addr,
            slot, connection->nick, msg, sizeof(msg));

      RARCH_LOG("%s %u\n", msg_hash_to_str(MSG_CONNECTION_SLOT), slot);

      /* Send them the savestate */
      if (!(netplay->quirks & 
               (NETPLAY_QUIRK_NO_SAVESTATES|NETPLAY_QUIRK_NO_TRANSMISSION)))
         netplay->force_send_savestate = true;
   }
   else
   {
      netplay->is_connected = true;
      snprintf(msg, sizeof(msg), "%s: \"%s\"",
            msg_hash_to_str(MSG_CONNECTED_TO),
            connection->nick);
   }

   RARCH_LOG("%s\n", msg);
   runloop_msg_queue_push(msg, 1, 180, false);

   /* Unstall if we were waiting for this */
   if (netplay->stall == NETPLAY_STALL_NO_CONNECTION)
       netplay->stall = NETPLAY_STALL_NONE;
}

/**
 * netplay_handshake_info
 *
 * Send an INFO command.
 */
bool netplay_handshake_info(netplay_t *netplay,
      struct netplay_connection *connection)
{
   struct info_buf_s info_buf;
   uint32_t      content_crc      = 0;
   rarch_system_info_t *core_info = runloop_get_system_info();

   memset(&info_buf, 0, sizeof(info_buf));
   info_buf.cmd[0] = htonl(NETPLAY_CMD_INFO);
   info_buf.cmd[1] = htonl(sizeof(info_buf) - 2*sizeof(uint32_t));

   /* Get our core info */
   if (core_info)
   {
      strlcpy(info_buf.core_name,
            core_info->info.library_name, sizeof(info_buf.core_name));
      strlcpy(info_buf.core_version,
            core_info->info.library_version, sizeof(info_buf.core_version));
   }
   else
   {
      strlcpy(info_buf.core_name,
            "UNKNOWN", sizeof(info_buf.core_name));
      strlcpy(info_buf.core_version,
            "UNKNOWN", sizeof(info_buf.core_version));
   }

   /* Get our content CRC */
   content_crc = content_get_crc();

   if (content_crc != 0)
      info_buf.content_crc = htonl(content_crc);

   /* Send it off and wait for info back */
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
         &info_buf, sizeof(info_buf)) ||
       !netplay_send_flush(&connection->send_packet_buffer, connection->fd,
         false))
      return false;

   connection->mode = NETPLAY_CONNECTION_PRE_INFO;
   return true;
}

/**
 * netplay_handshake_sync
 *
 * Send a SYNC command.
 */
bool netplay_handshake_sync(netplay_t *netplay,
      struct netplay_connection *connection)
{
   /* If we're the server, now we send sync info */
   size_t i;
   int matchct;
   uint32_t cmd[5];
   retro_ctx_memory_info_t mem_info;
   uint32_t device            = 0;
   uint32_t connected_players = 0;
   size_t nicklen, nickmangle = 0;
   bool nick_matched          = false;

   autosave_lock();
   mem_info.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&mem_info);
   autosave_unlock();

   /* Send basic sync info */
   cmd[0] = htonl(NETPLAY_CMD_SYNC);
   cmd[1] = htonl(3*sizeof(uint32_t) + MAX_USERS*sizeof(uint32_t) +
      NETPLAY_NICK_LEN + mem_info.size);
   cmd[2] = htonl(netplay->self_frame_count);
   connected_players = netplay->connected_players;
   if (netplay->self_mode == NETPLAY_CONNECTION_PLAYING)
      connected_players |= 1<<netplay->self_player;
   if (netplay->local_paused || netplay->remote_paused)
      connected_players |= NETPLAY_CMD_SYNC_BIT_PAUSED;
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
      device = htonl(input_config_get_device((unsigned)i));
      if (!netplay_send(&connection->send_packet_buffer, connection->fd,
               &device, sizeof(device)))
         return false;
   }

   /* Now see if we need to mangle their nick */
   nicklen = strlen(connection->nick);
   if (nicklen > NETPLAY_NICK_LEN - 5)
      nickmangle = NETPLAY_NICK_LEN - 5;
   else
      nickmangle = nicklen;
   matchct = 1;
   do {
      nick_matched = false;
      for (i = 0; i < netplay->connections_size; i++)
      {
         struct netplay_connection *sc = &netplay->connections[i];
         if (sc == connection) continue;
         if (sc->active &&
             sc->mode >= NETPLAY_CONNECTION_CONNECTED &&
             !strncmp(connection->nick, sc->nick, NETPLAY_NICK_LEN))
         {
            nick_matched = true;
            break;
         }
      }
      if (!strncmp(connection->nick, netplay->nick, NETPLAY_NICK_LEN))
         nick_matched = true;

      if (nick_matched)
      {
         /* Somebody has this nick, make a new one! */
         snprintf(connection->nick + nickmangle,
               NETPLAY_NICK_LEN - nickmangle, " (%d)", ++matchct);
         connection->nick[NETPLAY_NICK_LEN - 1] = '\0';
      }
   } while (nick_matched);

   /* Send the nick */
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
         connection->nick, NETPLAY_NICK_LEN))
      return false;

   /* And finally, the SRAM */
   autosave_lock();
   if (!netplay_send(&connection->send_packet_buffer, connection->fd,
            mem_info.data, mem_info.size) ||
         !netplay_send_flush(&connection->send_packet_buffer, connection->fd,
            false))
   {
      autosave_unlock();
      return false;
   }
   autosave_unlock();

   /* Now we're ready! */
   connection->mode = NETPLAY_CONNECTION_SPECTATING;
   netplay_handshake_ready(netplay, connection);

   return true;
}

/**
 * netplay_handshake_pre_nick
 *
 * Data receiver for the second stage of handshake, receiving the other side's
 * nickname.
 */
bool netplay_handshake_pre_nick(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   struct nick_buf_s nick_buf;
   ssize_t recvd;
   char msg[512];

   msg[0] = '\0';

   RECV(&nick_buf, sizeof(nick_buf)) {}

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
      settings_t *settings = config_get_ptr();

      if (  settings->paths.netplay_password[0] || 
            settings->paths.netplay_spectate_password[0])
      {
         /* There's a password, so just put them in PRE_PASSWORD mode */
         connection->mode = NETPLAY_CONNECTION_PRE_PASSWORD;
      }
      else
      {
         connection->can_play = true;
         if (!netplay_handshake_info(netplay, connection))
            return false;
         connection->mode = NETPLAY_CONNECTION_PRE_INFO;
      }

   }
   else
   {
      /* Client needs to wait for INFO */
      connection->mode = NETPLAY_CONNECTION_PRE_INFO;

   }

   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;
}

/**
 * netplay_handshake_pre_password
 *
 * Data receiver for the third, optional stage of server handshake, receiving
 * the password and sending core/content info.
 */
bool netplay_handshake_pre_password(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   struct password_buf_s password_buf, corr_password_buf;
   char password[8+NETPLAY_PASS_LEN]; /* 8 for salt */
   ssize_t recvd;
   char msg[512];
   bool correct         = false;
   settings_t *settings = config_get_ptr();

   msg[0] = '\0';

   RECV(&password_buf, sizeof(password_buf)) {}

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

   /* Calculate the correct password hash(es) and compare */
   correct = false;
   snprintf(password, sizeof(password), "%08X", connection->salt);

   if (settings->paths.netplay_password[0])
   {
      strlcpy(password + 8,
            settings->paths.netplay_password, sizeof(password)-8);

      sha256_hash(corr_password_buf.password,
            (uint8_t *) password, strlen(password));

      if (!memcmp(password_buf.password,
               corr_password_buf.password, sizeof(password_buf.password)))
      {
         correct = true;
         connection->can_play = true;
      }
   }
   if (settings->paths.netplay_spectate_password[0])
   {
      strlcpy(password + 8,
            settings->paths.netplay_spectate_password, sizeof(password)-8);

      sha256_hash(corr_password_buf.password,
            (uint8_t *) password, strlen(password));

      if (!memcmp(password_buf.password,
               corr_password_buf.password, sizeof(password_buf.password)))
         correct = true;
   }

   /* Just disconnect if it was wrong */
   if (!correct)
      return false;

   /* Otherwise, exchange info */
   if (!netplay_handshake_info(netplay, connection))
      return false;

   *had_input = true;
   connection->mode = NETPLAY_CONNECTION_PRE_INFO;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;
}

/**
 * netplay_handshake_pre_info
 *
 * Data receiver for the third stage of server handshake, receiving
 * the password.
 */
bool netplay_handshake_pre_info(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   struct info_buf_s info_buf;
   uint32_t cmd_size;
   ssize_t recvd;
   uint32_t content_crc           = 0;
   const char *dmsg               = NULL;
   rarch_system_info_t *core_info = runloop_get_system_info();

   RECV(&info_buf, sizeof(info_buf.cmd)) {}

   if (recvd < 0 ||
       ntohl(info_buf.cmd[0]) != NETPLAY_CMD_INFO)
   {
      RARCH_ERR("Failed to receive netplay info.\n");
      return false;
   }

   cmd_size = ntohl(info_buf.cmd[1]);
   if (cmd_size != sizeof(info_buf) - 2*sizeof(uint32_t))
   {
      /* Either the host doesn't have anything loaded, or this is just screwy */
      if (cmd_size != 0)
      {
         /* Huh? */
         RARCH_ERR("Invalid NETPLAY_CMD_INFO payload size.\n");
         return false;
      }

      /* Send our info and hope they load it! */
      if (!netplay_handshake_info(netplay, connection))
         return false;

      *had_input = true;
      netplay_recv_flush(&connection->recv_packet_buffer);
      return true;
   }

   RECV(&info_buf.core_name, cmd_size)
   {
      RARCH_ERR("Failed to receive netplay info payload.\n");
      return false;
   }

   /* Check the core info */

   if (core_info)
   {
      if (strncmp(info_buf.core_name,
               core_info->info.library_name, sizeof(info_buf.core_name)) ||
          strncmp(info_buf.core_version,
             core_info->info.library_version, sizeof(info_buf.core_version)))
      {
         dmsg = msg_hash_to_str(MSG_NETPLAY_IMPLEMENTATIONS_DIFFER);
         goto error;
      }
   }

   /* Check the content CRC */
   content_crc = content_get_crc();

   if (content_crc != 0)
   {
      if (ntohl(info_buf.content_crc) != content_crc)
      {
         dmsg = msg_hash_to_str(MSG_CONTENT_CRC32S_DIFFER);
         goto error;
      }
   }

   /* Now switch to the right mode */
   if (netplay->is_server)
   {
      if (!netplay_handshake_sync(netplay, connection))
         return false;

   }
   else
   {
      if (!netplay_handshake_info(netplay, connection))
         return false;
      connection->mode = NETPLAY_CONNECTION_PRE_SYNC;
   }

   *had_input = true;
   netplay_recv_flush(&connection->recv_packet_buffer);
   return true;

error:
   if (dmsg)
   {
      RARCH_ERR("%s\n", dmsg);
      runloop_msg_queue_push(dmsg, 1, 180, false);
   }

   if (!netplay->is_server)
   {
      /* Counter-intuitively, we still want to send our info. This is simply so
       * that the server knows why we disconnected. */
      if (!netplay_handshake_info(netplay, connection))
         return false;
   }

   return false;
}
/**
 * netplay_handshake_pre_sync
 *
 * Data receiver for the client's third handshake stage, receiving the
 * synchronization information.
 */
bool netplay_handshake_pre_sync(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   uint32_t cmd[2];
   uint32_t new_frame_count, connected_players, flip_frame;
   uint32_t device;
   uint32_t local_sram_size, remote_sram_size;
   size_t i;
   ssize_t recvd;
   retro_ctx_controller_info_t pad;
   char new_nick[NETPLAY_NICK_LEN];
   retro_ctx_memory_info_t mem_info;
   settings_t *settings = config_get_ptr();

   RECV(cmd, sizeof(cmd))
   {
      const char *msg = msg_hash_to_str(MSG_NETPLAY_INCORRECT_PASSWORD);
      RARCH_ERR("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
      return false;
   }

   /* Only expecting a sync command */
   if (ntohl(cmd[0]) != NETPLAY_CMD_SYNC ||
       ntohl(cmd[1]) < 3*sizeof(uint32_t) + MAX_USERS*sizeof(uint32_t) +
         NETPLAY_NICK_LEN)
   {
      RARCH_ERR("%s\n",
            msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
      return false;
   }

   /* Get the frame count */
   RECV(&new_frame_count, sizeof(new_frame_count))
      return false;
   new_frame_count = ntohl(new_frame_count);

   /* Get the connected players and pause mode */
   RECV(&connected_players, sizeof(connected_players))
      return false;
   connected_players = ntohl(connected_players);
   if (connected_players & NETPLAY_CMD_SYNC_BIT_PAUSED)
   {
      netplay->remote_paused = true;
      connected_players ^= NETPLAY_CMD_SYNC_BIT_PAUSED;
   }
   netplay->connected_players = connected_players;

   /* And the flip state */
   RECV(&flip_frame, sizeof(flip_frame))
      return false;

   flip_frame          = ntohl(flip_frame);
   netplay->flip       = !!flip_frame;
   netplay->flip_frame = flip_frame;

   /* Set our frame counters as requested */
   netplay->self_frame_count      = netplay->run_frame_count    =
      netplay->other_frame_count  = netplay->unread_frame_count =
      netplay->server_frame_count = new_frame_count;

   for (i = 0; i < netplay->buffer_size; i++)
   {
      struct delta_frame *ptr = &netplay->buffer[i];

      ptr->used               = false;

      if (i == netplay->self_ptr)
      {
         /* Clear out any current data but still use this frame */
         if (!netplay_delta_frame_ready(netplay, ptr, 0))
            return false;

         ptr->frame       = new_frame_count;
         ptr->have_local  = true;
         netplay->run_ptr = netplay->other_ptr = netplay->unread_ptr =
            netplay->server_ptr = i;

      }
   }
   for (i = 0; i < MAX_USERS; i++)
   {
      if (connected_players & (1<<i))
      {
         netplay->read_ptr[i]         = netplay->self_ptr;
         netplay->read_frame_count[i] = netplay->self_frame_count;
      }
   }

   /* Get and set each pad */
   for (i = 0; i < MAX_USERS; i++)
   {
      RECV(&device, sizeof(device))
         return false;

      pad.port   = (unsigned)i;
      pad.device = ntohl(device);

      core_set_controller_port_device(&pad);
   }

   /* Get our nick */
   RECV(new_nick, NETPLAY_NICK_LEN)
      return false;

   if (strncmp(netplay->nick, new_nick, NETPLAY_NICK_LEN))
   {
      char msg[512];
      strlcpy(netplay->nick, new_nick, NETPLAY_NICK_LEN);
      snprintf(msg, sizeof(msg),
            msg_hash_to_str(MSG_NETPLAY_CHANGED_NICK), netplay->nick);
      RARCH_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, false);
   }

   /* Now check the SRAM */
   autosave_lock();
   mem_info.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&mem_info);

   local_sram_size  = (unsigned)mem_info.size;
   remote_sram_size = ntohl(cmd[1]) - 3*sizeof(uint32_t) -
      MAX_USERS*sizeof(uint32_t) - NETPLAY_NICK_LEN;

   if (local_sram_size != 0 && local_sram_size == remote_sram_size)
   {
      RECV(mem_info.data, local_sram_size)
      {
         RARCH_ERR("%s\n",
               msg_hash_to_str(MSG_FAILED_TO_RECEIVE_SRAM_DATA_FROM_HOST));
         autosave_unlock();
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
            autosave_unlock();
            return false;
         }
         if (remote_sram_size > sizeof(uint32_t))
            remote_sram_size -= sizeof(uint32_t);
         else
            remote_sram_size = 0;
      }

   }
   autosave_unlock();

   /* We're ready! */
   *had_input = true;
   netplay->self_mode = NETPLAY_CONNECTION_SPECTATING;
   connection->mode = NETPLAY_CONNECTION_PLAYING;
   netplay_handshake_ready(netplay, connection);
   netplay_recv_flush(&connection->recv_packet_buffer);

   /* Ask to switch to playing mode if we should */
   if (!settings->bools.netplay_start_as_spectator)
      return netplay_cmd_mode(netplay, connection, NETPLAY_CONNECTION_PLAYING);

   return true;
}

/**
 * netplay_handshake
 *
 * Data receiver for all handshake states.
 */
bool netplay_handshake(netplay_t *netplay,
   struct netplay_connection *connection, bool *had_input)
{
   bool ret = false;

   switch (connection->mode)
   {
      case NETPLAY_CONNECTION_INIT:
         ret = netplay_handshake_init(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_PRE_NICK:
         ret = netplay_handshake_pre_nick(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_PRE_PASSWORD:
         ret = netplay_handshake_pre_password(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_PRE_INFO:
         ret = netplay_handshake_pre_info(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_PRE_SYNC:
         ret = netplay_handshake_pre_sync(netplay, connection, had_input);
         break;
      case NETPLAY_CONNECTION_NONE:
      default:
         return false;
   }

   if (connection->mode >= NETPLAY_CONNECTION_CONNECTED &&
         !netplay_send_cur_input(netplay, connection))
      return false;

   return ret;
}

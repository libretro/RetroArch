/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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

#include "netplay_compat.h"
#include "network_cmd.h"
#include "driver.h"
#include "general.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

struct network_cmd
{
   int fd;
   bool state[RARCH_BIND_LIST_END];
};

static bool socket_nonblock(int fd)
{
#ifdef _WIN32
   u_long mode = 1;
   return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
   return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK) == 0;
#endif
}

network_cmd_t *network_cmd_new(uint16_t port)
{
   network_cmd_t *handle = (network_cmd_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->fd = -1;

   struct addrinfo hints, *res = NULL;
   memset(&hints, 0, sizeof(hints));
#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family   = AF_INET;
#else
   hints.ai_family   = AF_UNSPEC;
#endif
   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags    = AI_PASSIVE;

   char port_buf[16];
   int yes = 1;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo(NULL, port_buf, &hints, &res) < 0)
      goto error;

   handle->fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   if (handle->fd < 0)
      goto error;

   if (!socket_nonblock(handle->fd))
      goto error;

   setsockopt(handle->fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));
   if (bind(handle->fd, res->ai_addr, res->ai_addrlen) < 0)
   {
      RARCH_ERR("Failed to bind socket.\n");
      goto error;
   }

   freeaddrinfo(res);

   return handle;

error:
   if (res)
      freeaddrinfo(res);
   network_cmd_free(handle);
   return NULL;
}

void network_cmd_free(network_cmd_t *handle)
{
   if (handle->fd >= 0)
      close(handle->fd);

   free(handle);
}

struct cmd_map
{
   const char *str;
   unsigned id;
};

static const struct cmd_map map[] = {
   { "FAST_FORWARD",           RARCH_FAST_FORWARD_KEY },
   { "FAST_FORWARD_HOLD",      RARCH_FAST_FORWARD_HOLD_KEY },
   { "LOAD_STATE",             RARCH_LOAD_STATE_KEY },
   { "SAVE_STATE",             RARCH_SAVE_STATE_KEY },
   { "FULLSCREEN_TOGGLE",      RARCH_FULLSCREEN_TOGGLE_KEY },
   { "QUIT",                   RARCH_QUIT_KEY },
   { "STATE_SLOT_PLUS",        RARCH_STATE_SLOT_PLUS },
   { "STATE_SLOT_MINUS",       RARCH_STATE_SLOT_MINUS },
   { "AUDIO_INPUT_RATE_PLUS",  RARCH_AUDIO_INPUT_RATE_PLUS },
   { "AUDIO_INPUT_RATE_MINUS", RARCH_AUDIO_INPUT_RATE_MINUS },
   { "REWIND",                 RARCH_REWIND },
   { "MOVIE_RECORD_TOGGLE",    RARCH_MOVIE_RECORD_TOGGLE },
   { "PAUSE_TOGGLE",           RARCH_PAUSE_TOGGLE },
   { "FRAMEADVANCE",           RARCH_FRAMEADVANCE },
   { "RESET",                  RARCH_RESET },
   { "SHADER_NEXT",            RARCH_SHADER_NEXT },
   { "SHADER_PREV",            RARCH_SHADER_PREV },
   { "CHEAT_INDEX_PLUS",       RARCH_CHEAT_INDEX_PLUS },
   { "CHEAT_INDEX_MINUS",      RARCH_CHEAT_INDEX_MINUS },
   { "CHEAT_TOGGLE",           RARCH_CHEAT_TOGGLE },
   { "SCREENSHOT",             RARCH_SCREENSHOT },
   { "DSP_CONFIG",             RARCH_DSP_CONFIG },
   { "MUTE",                   RARCH_MUTE },
   { "NETPLAY_FLIP",           RARCH_NETPLAY_FLIP },
   { "SLOWMOTION",             RARCH_SLOWMOTION },
};

static void parse_sub_msg(network_cmd_t *handle, const char *tok)
{
   for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
   {
      if (strcmp(tok, map[i].str) == 0)
      {
         handle->state[map[i].id] = true;
         return;
      }
   }

   RARCH_WARN("Unrecognized command \"%s\" received.\n", tok);
}

static void parse_msg(network_cmd_t *handle, char *buf)
{
   const char *tok = strtok(buf, "\n");
   while (tok)
   {
      parse_sub_msg(handle, tok);
      tok = strtok(NULL, "\n");
   }
}

void network_cmd_set(network_cmd_t *handle, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
      handle->state[id] = true;
}

bool network_cmd_get(network_cmd_t *handle, unsigned id)
{
   return id < RARCH_BIND_LIST_END && handle->state[id];
}

void network_cmd_pre_frame(network_cmd_t *handle)
{
   memset(handle->state, 0, sizeof(handle->state));

   fd_set fds;
   FD_ZERO(&fds);
   FD_SET(handle->fd, &fds);

   struct timeval tmp_tv = {0};
   if (select(handle->fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
      return;

   if (!FD_ISSET(handle->fd, &fds))
      return;

   for (;;)
   {
      char buf[1024];
      ssize_t ret = recvfrom(handle->fd, buf, sizeof(buf) - 1, 0, NULL, NULL);
      if (ret <= 0)
         break;

      buf[ret] = '\0';
      parse_msg(handle, buf);
   }
}


/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "command.h"

#ifdef HAVE_NETWORK_CMD
#include "netplay_compat.h"
#include "netplay.h"
#endif

#include "driver.h"
#include "general.h"
#include "compat/strl.h"
#include "compat/posix_string.h"
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

#define DEFAULT_NETWORK_CMD_PORT 55355
#define STDIN_BUF_SIZE 4096

struct rarch_cmd
{
#ifdef HAVE_STDIN_CMD
   bool stdin_enable;
   char stdin_buf[STDIN_BUF_SIZE];
   size_t stdin_buf_ptr;
#endif

#ifdef HAVE_NETWORK_CMD
   int net_fd;
#endif

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

#ifdef HAVE_NETWORK_CMD
static bool cmd_init_network(rarch_cmd_t *handle, uint16_t port)
{
   if (!netplay_init_network())
      return false;

   RARCH_LOG("Bringing up command interface on port %hu.\n", (unsigned short)port);

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

   handle->net_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
   if (handle->net_fd < 0)
      goto error;

   if (!socket_nonblock(handle->net_fd))
      goto error;

   setsockopt(handle->net_fd, SOL_SOCKET, SO_REUSEADDR, CONST_CAST &yes, sizeof(int));
   if (bind(handle->net_fd, res->ai_addr, res->ai_addrlen) < 0)
   {
      RARCH_ERR("Failed to bind socket.\n");
      goto error;
   }

   freeaddrinfo(res);
   return true;

error:
   if (res)
      freeaddrinfo(res);
   return false;
}
#endif

#ifdef HAVE_STDIN_CMD
static bool cmd_init_stdin(rarch_cmd_t *handle)
{
#ifndef _WIN32
   if (!socket_nonblock(STDIN_FILENO))
      return false;
#endif

   handle->stdin_enable = true;
   return true;
}
#endif

rarch_cmd_t *rarch_cmd_new(bool stdin_enable, bool network_enable, uint16_t port)
{
   rarch_cmd_t *handle = (rarch_cmd_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

#ifdef HAVE_NETWORK_CMD
   handle->net_fd = -1;
   if (network_enable && !cmd_init_network(handle, port))
      goto error;
#else
   (void)network_enable;
   (void)port;
#endif

#ifdef HAVE_STDIN_CMD
   handle->stdin_enable = stdin_enable;
   if (stdin_enable && !cmd_init_stdin(handle))
      goto error;
#else
   (void)stdin_enable;
#endif

   return handle;

error:
   rarch_cmd_free(handle);
   return NULL;
}

void rarch_cmd_free(rarch_cmd_t *handle)
{
#ifdef HAVE_NETWORK_CMD
   if (handle->net_fd >= 0)
      close(handle->net_fd);
#endif

   free(handle);
}

struct cmd_map
{
   const char *str;
   unsigned id;
};

struct cmd_action_map
{
   const char *str;
   bool (*action)(const char *arg);
   const char *arg_desc;
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
   { "VOLUME_UP",              RARCH_VOLUME_UP },
   { "VOLUME_DOWN",            RARCH_VOLUME_DOWN },
   { "OVERLAY_NEXT",           RARCH_OVERLAY_NEXT },
   { "DISK_EJECT_TOGGLE",      RARCH_DISK_EJECT_TOGGLE },
   { "DISK_NEXT",              RARCH_DISK_NEXT },
   { "GRAB_MOUSE_TOGGLE",      RARCH_GRAB_MOUSE_TOGGLE },
   { "MENU_TOGGLE",            RARCH_MENU_TOGGLE },
};

static bool cmd_set_shader(const char *arg)
{
   if (!driver.video->set_shader)
      return false;

   enum rarch_shader_type type = RARCH_SHADER_NONE;
   const char *ext = strrchr(arg, '.');

   if (ext)
   {
      if (strcmp(ext, ".shader") == 0)
         type = RARCH_SHADER_GLSL;
      else if (strcmp(ext, ".cg") == 0 || strcmp(ext, ".cgp") == 0)
         type = RARCH_SHADER_CG;
   }

   if (type == RARCH_SHADER_NONE)
      return false;

   msg_queue_clear(g_extern.msg_queue);

   char msg[PATH_MAX];
   snprintf(msg, sizeof(msg), "Shader: \"%s\"", arg);
   msg_queue_push(g_extern.msg_queue, msg, 1, 120);
   RARCH_LOG("Applying shader \"%s\".\n", arg);

   return video_set_shader_func(type, arg);
}

static const struct cmd_action_map action_map[] = {
   { "SET_SHADER", cmd_set_shader, "<shader path>" },
};

static bool command_get_arg(const char *tok, const char **arg, unsigned *index)
{
   for (unsigned i = 0; i < ARRAY_SIZE(map); i++)
   {
      if (strcmp(tok, map[i].str) == 0)
      {
         if (arg)
            *arg = NULL;

         if (index)
            *index = i;

         return true;
      }
   }

   for (unsigned i = 0; i < ARRAY_SIZE(action_map); i++)
   {
      const char *str = strstr(tok, action_map[i].str);
      if (str == tok)
      {
         const char *argument = str + strlen(action_map[i].str);
         if (*argument != ' ')
            return false;

         if (arg)
            *arg = argument + 1;

         if (index)
            *index = i;

         return true;
      }
   }

   return false;
}

static void parse_sub_msg(rarch_cmd_t *handle, const char *tok)
{
   const char *arg = NULL;
   unsigned index  = 0;

   if (command_get_arg(tok, &arg, &index))
   {
      if (arg)
      {
         if (!action_map[index].action(arg))
            RARCH_ERR("Command \"%s\" failed.\n", arg);
      }
      else
         handle->state[map[index].id] = true;
   }
   else
      RARCH_WARN("Unrecognized command \"%s\" received.\n", tok);
}

static void parse_msg(rarch_cmd_t *handle, char *buf)
{
   char *save;
   const char *tok = strtok_r(buf, "\n", &save);
   while (tok)
   {
      parse_sub_msg(handle, tok);
      tok = strtok_r(NULL, "\n", &save);
   }
}

void rarch_cmd_set(rarch_cmd_t *handle, unsigned id)
{
   if (id < RARCH_BIND_LIST_END)
      handle->state[id] = true;
}

bool rarch_cmd_get(rarch_cmd_t *handle, unsigned id)
{
   return id < RARCH_BIND_LIST_END && handle->state[id];
}

#ifdef HAVE_NETWORK_CMD
static void network_cmd_pre_frame(rarch_cmd_t *handle)
{
   if (handle->net_fd < 0)
      return;

   fd_set fds;
   FD_ZERO(&fds);
   FD_SET(handle->net_fd, &fds);

   struct timeval tmp_tv = {0};
   if (select(handle->net_fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
      return;

   if (!FD_ISSET(handle->net_fd, &fds))
      return;

   for (;;)
   {
      char buf[1024];
      ssize_t ret = recvfrom(handle->net_fd, buf, sizeof(buf) - 1, 0, NULL, NULL);
      if (ret <= 0)
         break;

      buf[ret] = '\0';
      parse_msg(handle, buf);
   }
}
#endif

#ifdef HAVE_STDIN_CMD

#ifdef _WIN32
// Oh you, Win32 ... <_<
static size_t read_stdin(char *buf, size_t size)
{
   HANDLE hnd = GetStdHandle(STD_INPUT_HANDLE);
   if (hnd == INVALID_HANDLE_VALUE)
      return 0;

   // Check first if we're a pipe
   // (not console).
   DWORD avail = 0;
   bool echo = false;

   // If not a pipe, check if we're running in a console.
   if (!PeekNamedPipe(hnd, NULL, 0, NULL, &avail, NULL))
   {
      DWORD mode = 0;
      if (!GetConsoleMode(hnd, &mode))
         return 0;

      if ((mode & (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT))
            && !SetConsoleMode(hnd, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)))
         return 0;

      // Win32, Y U NO SANE NONBLOCK READ!?
      DWORD has_read = 0;
      INPUT_RECORD recs[256];
      if (!PeekConsoleInput(hnd, recs, sizeof(recs) / sizeof(recs[0]), &has_read))
         return 0;

      bool has_key = false;
      for (DWORD i = 0; i < has_read; i++)
      {
         // Very crude, but should get the job done ...
         if (recs[i].EventType == KEY_EVENT &&
               recs[i].Event.KeyEvent.bKeyDown &&
               (isgraph(recs[i].Event.KeyEvent.wVirtualKeyCode) || recs[i].Event.KeyEvent.wVirtualKeyCode == VK_RETURN))
         {
            has_key = true;
            echo    = true;
            avail   = size;
            break;
         }
      }

      if (!has_key)
      {
         FlushConsoleInputBuffer(hnd);
         return 0;
      }
   }

   if (!avail)
      return 0;

   if (avail > size)
      avail = size;

   DWORD has_read = 0;
   if (!ReadFile(hnd, buf, avail, &has_read, NULL))
      return 0;

   for (DWORD i = 0; i < has_read; i++)
      if (buf[i] == '\r')
         buf[i] = '\n';

   // Console won't echo for us while in non-line mode, so do it manually ...
   if (echo)
   {
      HANDLE hnd_out = GetStdHandle(STD_OUTPUT_HANDLE);
      if (hnd_out != INVALID_HANDLE_VALUE)
      {
         DWORD has_written;
         WriteConsole(hnd_out, buf, has_read, &has_written, NULL);
      }
   }

   return has_read;
}
#else
static size_t read_stdin(char *buf, size_t size)
{
   size_t has_read = 0;
   while (size)
   {
      ssize_t ret = read(STDIN_FILENO, buf, size);

      if (ret <= 0)
         break;

      buf      += ret;
      has_read += ret;
      size     -= ret;
   }

   return has_read;
}
#endif

static void stdin_cmd_pre_frame(rarch_cmd_t *handle)
{
   if (!handle->stdin_enable)
      return;

   size_t ret = read_stdin(handle->stdin_buf + handle->stdin_buf_ptr, STDIN_BUF_SIZE - handle->stdin_buf_ptr - 1);
   if (ret == 0)
      return;

   handle->stdin_buf_ptr += ret;
   handle->stdin_buf[handle->stdin_buf_ptr] = '\0';

   char *last_newline = strrchr(handle->stdin_buf, '\n');
   if (!last_newline)
   {
      // We're receiving bogus data in pipe (no terminating newline),
      // flush out the buffer.
      if (handle->stdin_buf_ptr + 1 >= STDIN_BUF_SIZE)
      {
         handle->stdin_buf_ptr = 0;
         handle->stdin_buf[0] = '\0';
      }

      return;
   }

   *last_newline++ = '\0';
   ptrdiff_t msg_len = last_newline - handle->stdin_buf;

   parse_msg(handle, handle->stdin_buf);

   memmove(handle->stdin_buf, last_newline, handle->stdin_buf_ptr - msg_len);
   handle->stdin_buf_ptr -= msg_len;
}
#endif

void rarch_cmd_pre_frame(rarch_cmd_t *handle)
{
   memset(handle->state, 0, sizeof(handle->state));

#ifdef HAVE_NETWORK_CMD
   network_cmd_pre_frame(handle);
#endif

#ifdef HAVE_STDIN_CMD
   stdin_cmd_pre_frame(handle);
#endif
}

#ifdef HAVE_NETWORK_CMD
static bool send_udp_packet(const char *host, uint16_t port, const char *msg)
{
   struct addrinfo hints, *res = NULL;
   memset(&hints, 0, sizeof(hints));
#if defined(_WIN32) || defined(HAVE_SOCKET_LEGACY)
   hints.ai_family   = AF_INET;
#else
   hints.ai_family   = AF_UNSPEC;
#endif
   hints.ai_socktype = SOCK_DGRAM;

   int fd = -1;
   bool ret = true;
   char port_buf[16];

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo(host, port_buf, &hints, &res) < 0)
      return false;

   // Send to all possible targets.
   // "localhost" might resolve to several different IPs.
   const struct addrinfo *tmp = res;
   while (tmp)
   {
      fd = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
      if (fd < 0)
      {
         ret = false;
         goto end;
      }

      ssize_t len = strlen(msg);
      ssize_t ret = sendto(fd, msg, len, 0, tmp->ai_addr, tmp->ai_addrlen);
      if (ret < len)
      {
         ret = false;
         goto end;
      }

      close(fd);
      fd = -1;
      tmp = tmp->ai_next;
   }

end:
   freeaddrinfo(res);
   if (fd >= 0)
      close(fd);
   return ret;
}

static bool verify_command(const char *cmd)
{
   if (command_get_arg(cmd, NULL, NULL))
      return true;

   RARCH_ERR("Command \"%s\" is not recognized by RetroArch.\n", cmd);
   RARCH_ERR("\tValid commands:\n");
   for (unsigned i = 0; i < sizeof(map) / sizeof(map[0]); i++)
      RARCH_ERR("\t\t%s\n", map[i].str);

   for (unsigned i = 0; i < sizeof(action_map) / sizeof(action_map[0]); i++)
      RARCH_ERR("\t\t%s %s\n", action_map[i].str, action_map[i].arg_desc);

   return false;
}

bool network_cmd_send(const char *cmd_)
{
   if (!netplay_init_network())
      return NULL;

   char *command = strdup(cmd_);
   if (!command)
      return false;

   bool old_verbose = g_extern.verbose;
   g_extern.verbose = true;

   const char *cmd = NULL;
   const char *host = NULL;
   const char *port_ = NULL;
   uint16_t port = DEFAULT_NETWORK_CMD_PORT;

   char *save;
   cmd = strtok_r(command, ";", &save);
   if (cmd)
      host = strtok_r(NULL, ";", &save);
   if (host)
      port_ = strtok_r(NULL, ";", &save);

   if (!host)
   {
#ifdef _WIN32
      host = "127.0.0.1";
#else
      host = "localhost";
#endif
   }

   if (port_)
      port = strtoul(port_, NULL, 0);

   RARCH_LOG("Sending command: \"%s\" to %s:%hu\n", cmd, host, (unsigned short)port);

   bool ret = verify_command(cmd) && send_udp_packet(host, port, cmd);
   free(command);

   g_extern.verbose = old_verbose;
   return ret;
}
#endif



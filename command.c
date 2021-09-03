/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2021      - David G.F.
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
#include <stddef.h>
#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif
#include <streams/stdin_stream.h>
#include <string/stdstring.h>

#include "verbosity.h"
#include "command.h"

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#endif

#define CMD_BUF_SIZE           4096

#if defined(HAVE_COMMAND)

/* Generic command parse utilities */

static bool command_get_arg(const char *tok,
      const char **arg, unsigned *index)
{
   unsigned i;

   for (i = 0; i < ARRAY_SIZE(map); i++)
   {
      if (string_is_equal(tok, map[i].str))
      {
         if (arg)
            *arg = NULL;

         if (index)
            *index = i;

         return true;
      }
   }

   for (i = 0; i < ARRAY_SIZE(action_map); i++)
   {
      const char *str = strstr(tok, action_map[i].str);
      if (str == tok)
      {
         const char *argument = str + strlen(action_map[i].str);
         if (*argument != ' ' && *argument != '\0')
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

static void command_parse_sub_msg(command_t *handle, const char *tok)
{
   const char *arg = NULL;
   unsigned index  = 0;

   if (command_get_arg(tok, &arg, &index))
   {
      if (arg)
      {
         if (!action_map[index].action(handle, arg))
            RARCH_ERR("Command \"%s\" failed.\n", arg);
      }
      else
         handle->state[map[index].id] = true;
   }
   else
      RARCH_WARN("%s \"%s\" %s.\n",
            msg_hash_to_str(MSG_UNRECOGNIZED_COMMAND),
            tok,
            msg_hash_to_str(MSG_RECEIVED));
}

static void command_parse_msg(
      command_t *handle, char *buf)
{
   char     *save  = NULL;
   const char *tok = strtok_r(buf, "\n", &save);

   while (tok)
   {
      command_parse_sub_msg(handle, tok);
      tok = strtok_r(NULL, "\n", &save);
   }
}

#if defined(HAVE_NETWORK_CMD)
typedef struct
{
   /* Network socket FD */
   int net_fd;
   /* Source address for the command received */
   struct sockaddr_storage cmd_source;
   /* Size of the previous structure in use */
   socklen_t cmd_source_len;
} command_network_t;

static void network_command_reply(
      command_t *cmd,
      const char * data, size_t len)
{
   command_network_t *netcmd = (command_network_t*)cmd->userptr;
   /* Respond (fire and forget since it's UDP) */
   sendto(netcmd->net_fd, data, len, 0,
      (struct sockaddr*)&netcmd->cmd_source, netcmd->cmd_source_len);
}

static void network_command_free(command_t *handle)
{
   command_network_t *netcmd = (command_network_t*)handle->userptr;

   if (netcmd->net_fd >= 0)
      socket_close(netcmd->net_fd);

   free(netcmd);
   free(handle);
}

static void command_network_poll(command_t *handle)
{
   fd_set fds;
   struct timeval       tmp_tv = {0};
   command_network_t   *netcmd = (command_network_t*)handle->userptr;

   if (netcmd->net_fd < 0)
      return;

   FD_ZERO(&fds);
   FD_SET(netcmd->net_fd, &fds);

   if (socket_select(netcmd->net_fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
      return;

   if (!FD_ISSET(netcmd->net_fd, &fds))
      return;

   for (;;)
   {
      ssize_t ret;
      char buf[1024];

      buf[0] = '\0';
      netcmd->cmd_source_len = sizeof(struct sockaddr_storage);
      ret  = recvfrom(netcmd->net_fd, buf, sizeof(buf) - 1, 0,
            (struct sockaddr*)&netcmd->cmd_source,
            &netcmd->cmd_source_len);

      if (ret <= 0)
         break;

      buf[ret] = '\0';

      command_parse_msg(handle, buf);
   }
}

command_t* command_network_new(uint16_t port)
{
   struct addrinfo     *res  = NULL;
   command_t            *cmd = (command_t*)calloc(1, sizeof(command_t));
   command_network_t *netcmd = (command_network_t*)calloc(
                                   1, sizeof(command_network_t));
   int fd                    = socket_init(
         (void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM);

   RARCH_LOG("%s %hu.\n",
         msg_hash_to_str(MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT),
         (unsigned short)port);

   if (fd < 0)
      goto error;

   netcmd->net_fd = fd;
   cmd->userptr   = netcmd;
   cmd->poll      = command_network_poll;
   cmd->replier   = network_command_reply;
   cmd->destroy   = network_command_free;

   if (!socket_nonblock(netcmd->net_fd))
      goto error;

   if (!socket_bind(netcmd->net_fd, (void*)res))
   {
      RARCH_ERR("%s.\n",
            msg_hash_to_str(MSG_FAILED_TO_BIND_SOCKET));
      goto error;
   }

   freeaddrinfo_retro(res);
   return cmd;

error:
   if (res)
      freeaddrinfo_retro(res);
   free(netcmd);
   free(cmd);
   return NULL;
}
#endif


#if defined(HAVE_STDIN_CMD)
typedef struct
{
   /* Buffer and pointer for stdin reads */
   size_t stdin_buf_ptr;
   char stdin_buf[CMD_BUF_SIZE];
} command_stdin_t;

static void stdin_command_reply(
      command_t *cmd,
      const char * data, size_t len)
{
   /* Just write to stdout! */
   fwrite(data, 1, len, stdout);
}

static void stdin_command_free(command_t *handle)
{
   free(handle->userptr);
   free(handle);
}

static void command_stdin_poll(command_t *handle)
{
   ptrdiff_t msg_len;
   char        *last_newline = NULL;
   command_stdin_t *stdincmd = (command_stdin_t*)handle->userptr;
   ssize_t               ret = read_stdin(
         stdincmd->stdin_buf + stdincmd->stdin_buf_ptr,
         CMD_BUF_SIZE - stdincmd->stdin_buf_ptr - 1);

   if (ret == 0)
      return;

   stdincmd->stdin_buf_ptr                      += ret;
   stdincmd->stdin_buf[stdincmd->stdin_buf_ptr]  = '\0';

   last_newline = strrchr(stdincmd->stdin_buf, '\n');

   if (!last_newline)
   {
      /* We're receiving bogus data in pipe
       * (no terminating newline), flush out the buffer. */
      if (stdincmd->stdin_buf_ptr + 1 >= CMD_BUF_SIZE)
      {
         stdincmd->stdin_buf_ptr = 0;
         stdincmd->stdin_buf[0]  = '\0';
      }

      return;
   }

   *last_newline++ = '\0';
   msg_len         = last_newline - stdincmd->stdin_buf;

   command_parse_msg(handle, stdincmd->stdin_buf);

   memmove(stdincmd->stdin_buf, last_newline,
         stdincmd->stdin_buf_ptr - msg_len);
   stdincmd->stdin_buf_ptr -= msg_len;
}

command_t* command_stdin_new(void)
{
   command_t *cmd;
   command_stdin_t *stdincmd;

#ifndef _WIN32
#ifdef HAVE_NETWORKING
   if (!socket_nonblock(STDIN_FILENO))
      return NULL;
#endif
#endif

   cmd          = (command_t*)calloc(1, sizeof(command_t));
   stdincmd     = (command_stdin_t*)calloc(1, sizeof(command_stdin_t));
   cmd->userptr = stdincmd;
   cmd->poll    = command_stdin_poll;
   cmd->replier = stdin_command_reply;
   cmd->destroy = stdin_command_free;

   return cmd;
}
#endif

#if defined(HAVE_LAKKA)
#include <sys/un.h>
#define MAX_USER_CONNECTIONS  4
typedef struct
{
   /* File descriptor for the domain socket */
   int sfd;
   /* Client sockets */
   int userfd[MAX_USER_CONNECTIONS];
   /* Last received user socket */
   int last_fd;
} command_uds_t;

static void uds_command_reply(
      command_t *cmd,
      const char * data, size_t len)
{
   command_uds_t *subcmd = (command_uds_t*)cmd->userptr;
   write(subcmd->last_fd, data, len);
}

static void uds_command_free(command_t *handle)
{
   int i;
   command_uds_t *udscmd = (command_uds_t*)handle->userptr;

   for (i = 0; i < MAX_USER_CONNECTIONS; i++)
      if (udscmd->userfd[i] >= 0)
         socket_close(udscmd->userfd[i]);
   socket_close(udscmd->sfd);

   free(handle->userptr);
   free(handle);
}

static void command_uds_poll(command_t *handle)
{
   int i;
   fd_set fds;
   command_uds_t *udscmd       = (command_uds_t*)handle->userptr;
   int maxfd                   = udscmd->sfd;
   struct timeval       tmp_tv = {0};

   if (udscmd->sfd < 0)
      return;

   FD_ZERO(&fds);
   FD_SET(udscmd->sfd, &fds);

   for (i = 0; i < MAX_USER_CONNECTIONS; i++)
   {
      if (udscmd->userfd[i] >= 0)
      {
         maxfd = MAX(udscmd->userfd[i], maxfd);
         FD_SET(udscmd->userfd[i], &fds);
      }
   }

   if (socket_select(maxfd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
      return;

   /* Read data from clients and process commands */
   for (i = 0; i < MAX_USER_CONNECTIONS; i++)
   {
      if (udscmd->userfd[i] >= 0 && FD_ISSET(udscmd->userfd[i], &fds))
      {
         while (1)
         {
            char buf[2048];
            ssize_t ret = recv(udscmd->userfd[i], buf, sizeof(buf) - 1, 0);

            if (ret < 0)
               break;   /* no more data */
            if (!ret)
            {
               socket_close(udscmd->userfd[i]);
               udscmd->userfd[i] = -1;
               break;
            }

            buf[ret] = 0;
            udscmd->last_fd = udscmd->userfd[i];
            command_parse_msg(handle, buf);
         }
      }
   }

   if (FD_ISSET(udscmd->sfd, &fds))
   {
      /* Accepts new connections from clients */
      int cfd = accept(udscmd->sfd, NULL, NULL);
      if (cfd >= 0) {
         if (!socket_nonblock(cfd))
            socket_close(cfd);
         else {
            for (i = 0; i < MAX_USER_CONNECTIONS; i++)
               if (udscmd->userfd[i] < 0)
               {
                  udscmd->userfd[i] = cfd;
                  break;
               }
         }
      }
   }
}

command_t* command_uds_new(void)
{
   int i;
   command_t *cmd;
   command_uds_t *subcmd;
   struct sockaddr_un addr;
   const char   *sp = "retroarch/cmd";
   socklen_t addrsz = offsetof(struct sockaddr_un, sun_path) + strlen(sp) + 1;
   int           fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0)
      return NULL;

   /* use an abstract socket for simplicity */
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   strcpy(&addr.sun_path[1], sp);

   if (bind(fd, (struct sockaddr*)&addr, addrsz) < 0 ||
       listen(fd, MAX_USER_CONNECTIONS) < 0)
   {
      socket_close(fd);
      return NULL;
   }

   if (!socket_nonblock(fd))
   {
      socket_close(fd);
      return NULL;
   }

   cmd             = (command_t*)calloc(1, sizeof(command_t));
   subcmd          = (command_uds_t*)calloc(1, sizeof(command_uds_t));
   subcmd->sfd     = fd;
   subcmd->last_fd = -1;
   for (i = 0; i < MAX_USER_CONNECTIONS; i++)
      subcmd->userfd[i] = -1;

   cmd->userptr = subcmd;
   cmd->poll    = command_uds_poll;
   cmd->replier = uds_command_reply;
   cmd->destroy = uds_command_free;

   return cmd;
}
#endif


/* Routines used to invoke retroarch command ... */

#ifdef HAVE_NETWORK_CMD
static bool command_verify(const char *cmd)
{
   unsigned i;

   if (command_get_arg(cmd, NULL, NULL))
      return true;

   RARCH_ERR("Command \"%s\" is not recognized by the program.\n", cmd);
   RARCH_ERR("\tValid commands:\n");
   for (i = 0; i < ARRAY_SIZE(map); i++)
      RARCH_ERR("\t\t%s\n", map[i].str);

   for (i = 0; i < ARRAY_SIZE(action_map); i++)
      RARCH_ERR("\t\t%s %s\n", action_map[i].str, action_map[i].arg_desc);

   return false;
}

bool command_network_send(const char *cmd_)
{
   char *command        = NULL;
   char *save           = NULL;
   const char *cmd      = NULL;

   if (!network_init())
      return false;

   if (!(command = strdup(cmd_)))
      return false;

   cmd                  = strtok_r(command, ";", &save);
   if (cmd)
   {
      uint16_t port     = DEFAULT_NETWORK_CMD_PORT;
      const char *port_ = NULL;
      const char *host  = strtok_r(NULL, ";", &save);
      if (host)
         port_          = strtok_r(NULL, ";", &save);
      else
      {
#ifdef _WIN32
         host = "127.0.0.1";
#else
         host = "localhost";
#endif
      }

      if (port_)
         port = strtoul(port_, NULL, 0);

      RARCH_LOG("%s: \"%s\" to %s:%hu\n",
            msg_hash_to_str(MSG_SENDING_COMMAND),
            cmd, host, (unsigned short)port);

      if (command_verify(cmd) && udp_send_packet(host, port, cmd))
      {
         free(command);
         return true;
      }
   }

   free(command);
   return false;
}
#endif

bool command_show_osd_msg(command_t *cmd, const char* arg)
{
    runloop_msg_queue_push(arg, 1, 180, false, NULL,
          MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
    return true;
}

#if defined(HAVE_CHEEVOS)
bool command_read_ram(command_t *cmd, const char *arg)
{
   unsigned i;
   char *reply                  = NULL;
   const uint8_t  *data         = NULL;
   char *reply_at               = NULL;
   unsigned int nbytes          = 0;
   unsigned int alloc_size      = 0;
   unsigned int addr            = -1;
   unsigned int len             = 0;

   if (sscanf(arg, "%x %u", &addr, &nbytes) != 2)
      return true;
   /* We allocate more than needed, saving 20 bytes is not really relevant */
   alloc_size              = 40 + nbytes * 3;
   reply                   = (char*)malloc(alloc_size);
   reply[0]                = '\0';
   reply_at                = reply + snprintf(
         reply, alloc_size - 1, "READ_CORE_RAM" " %x", addr);

   if ((data = rcheevos_patch_address(addr)))
   {
      for (i = 0; i < nbytes; i++)
         snprintf(reply_at + 3 * i, 4, " %.2X", data[i]);
      reply_at[3 * nbytes] = '\n';
      len                  = reply_at + 3 * nbytes + 1 - reply;
   }
   else
   {
      strlcpy(reply_at, " -1\n", sizeof(reply) - strlen(reply));
      len                  = reply_at + STRLEN_CONST(" -1\n") - reply;
   }
   cmd->replier(cmd, reply, len);
   free(reply);
   return true;
}

bool command_write_ram(command_t *cmd, const char *arg)
{
   unsigned int addr    = (unsigned int)strtoul(arg, (char**)&arg, 16);
   uint8_t *data        = (uint8_t *)rcheevos_patch_address(addr);

   if (!data)
      return false;

   if (rcheevos_hardcore_active())
   {
      RARCH_LOG("Achievements hardcore mode disabled by WRITE_CORE_RAM\n");
      rcheevos_pause_hardcore();
   }

   while (*arg)
   {
      *data = strtoul(arg, (char**)&arg, 16);
      data++;
   }
   return true;
}
#endif

#endif

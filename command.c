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

#include "input/input_driver.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <locale.h>
#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#include <net/net_socket.h>
#endif
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <streams/stdin_stream.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>
#include <features/features_cpu.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "gfx/gfx_widgets.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#endif

#ifdef HAVE_NETWORKING
#include "network/netplay/netplay.h"
#endif

#include "audio/audio_driver.h"
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
#include "gfx/video_shader_parse.h"
#endif
#include "autosave.h"
#include "command.h"
#include "core.h"
#include "core_info.h"
#include "playlist.h"
#include <queues/task_queue.h>
#include <retro_atomic.h>
#include "cheat_manager.h"
#include "content.h"
#include "dynamic.h"
#include "file_path_special.h"
#include "list_special.h"
#include "paths.h"
#include "record/record_driver.h"
#include "retroarch.h"
#include "runloop.h"
#include "verbosity.h"
#include "version.h"
#include "version_git.h"
#include "tasks/task_content.h"
#ifdef HAVE_TRANSLATE
#include "accessibility.h"
#endif
#ifdef HAVE_SCREENSHOTS
#include "tasks/tasks_internal.h"
#endif
#include <compat/strl.h>
#ifdef __MACH__
#include <TargetConditionals.h>
#endif

#define CMD_BUF_SIZE 4096

/* Replies with a failure and why, and marks the reply as one: a line
 * client reads the text, a structured one is told outright. The text
 * opens with @name and ERROR, as a deferred reply's does. */
void command_reply_error(command_t *cmd, const char *name,
      const char *fmt, ...)
{
   char    msg[512];
   int     n;
   size_t  _len;
   va_list ap;

   if (!cmd || !cmd->replier)
      return;

   /* Both of these report the length they wanted rather than the one
    * they wrote, so each is brought back inside the buffer before the
    * next writes past its end. */
   if ((n = snprintf(msg, sizeof(msg), "%s ERROR ", name)) < 0)
      return;
   _len = (size_t)n;
   if (_len >= sizeof(msg))
      _len = sizeof(msg) - 1;

   va_start(ap, fmt);
   n = vsnprintf(msg + _len, sizeof(msg) - _len, fmt, ap);
   va_end(ap);
   if (n > 0)
   {
      _len += (size_t)n;
      if (_len >= sizeof(msg))
         _len = sizeof(msg) - 1;
   }

   cmd->error = true;
   cmd->replier(cmd, msg, _len);
}

#ifdef HAVE_COMMAND
/* a queued STEPS was dropped since the reply now sent began */
static bool command_queue_ended;
#endif

static void command_post_state_loaded(void)
{
#ifdef HAVE_COMMAND
   /* steps queued from the state it left would play on from this one */
   command_run_frames_forget();
#endif
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
   {
      const char *_msg = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_DISABLED);
      rcheevos_pause_hardcore();
      runloop_msg_queue_push(_msg, strlen(_msg), 0, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
#endif
#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, NULL);
#endif
   {
     video_driver_state_t *video_st                 =
       video_state_get_ptr();
     bool frame_time_counter_auto_reset             =
       config_get_ptr()->bools.frame_time_counter_auto_reset;
     if (frame_time_counter_auto_reset)
        video_st->frame_time_count = 0;
   }
#if defined(HAVE_GFX_WIDGETS) && defined(HAVE_SCREENSHOTS)
   {
      gfx_widget_state_slot_show(dispwidget_get_ptr(), NULL, NULL);
   }
#endif
}

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
         if (!argument)
            return false;
         if (*argument != ' ' && *argument != '\0')
            return false;

         /* past the separating space; a bare command gets "", not the
          * byte after its terminator */
         if (arg)
            *arg = *argument ? argument + 1 : argument;

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

   handle->error   = false;

   if (command_get_arg(tok, &arg, &index))
   {
      if (arg)
      {
         if (!action_map[index].action(handle, arg))
            RARCH_ERR("[Command] Command \"%s\" failed.\n", arg);
      }
      else
      {
         /* For MENU_TOGGLE, bypass the press-and-release mechanism
          * by directly invoking the command event.  This avoids
          * timing issues with runahead (single-instance) where the
          * 1-frame pulse from network commands can be lost. */
         if (map[index].id == RARCH_MENU_TOGGLE)
            command_event(CMD_EVENT_MENU_TOGGLE, NULL);
         else
            handle->state[map[index].id] = true;
      }
   }
   else
      RARCH_WARN(msg_hash_to_str(MSG_UNRECOGNIZED_COMMAND), tok);
}

static void command_parse_msg(command_t *handle, char *buf)
{
   char     *save  = NULL;
   char     *tok   = strtok_r(buf, "\n", &save);
   unsigned gen    = input_driver_command_generation();

   while (tok)
   {
      /* a client ending its lines in CRLF leaves no \r on the last word;
       * a \r anywhere else is the command's own */
      size_t len = strlen(tok);
      if (len && tok[len - 1] == '\r')
         tok[len - 1] = '\0';
      command_parse_sub_msg(handle, tok);
      /* The command may have reinitialised the input driver, which
       * owns and has now freed 'handle'. Nothing further may use it. */
      if (input_driver_command_generation() != gen)
         return;
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

static void network_command_reply(command_t *cmd,
   const char *s, size_t len)
{
   command_network_t *netcmd = (command_network_t*)cmd->userptr;
   /* Respond (fire and forget since it's UDP) */
   sendto(netcmd->net_fd, s, len, 0,
      (struct sockaddr*)&netcmd->cmd_source, netcmd->cmd_source_len);
}

typedef struct
{
   struct sockaddr_storage addr;
   socklen_t addr_len;
} command_network_dest_t;

static void *network_command_reply_dest(command_t *cmd)
{
   command_network_t      *netcmd = (command_network_t*)cmd->userptr;
   command_network_dest_t *dest   = (command_network_dest_t*)
      malloc(sizeof(*dest));
   if (dest)
   {
      dest->addr     = netcmd->cmd_source;
      dest->addr_len = netcmd->cmd_source_len;
   }
   return dest;
}

static void network_command_reply_to(command_t *cmd, void *data,
   const char *s, size_t len)
{
   command_network_t      *netcmd = (command_network_t*)cmd->userptr;
   command_network_dest_t *dest   = (command_network_dest_t*)data;
   sendto(netcmd->net_fd, s, len, 0,
      (struct sockaddr*)&dest->addr, dest->addr_len);
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
   ssize_t ret;
   unsigned gen = input_driver_command_generation();
   char buf[2048];
   command_network_t *netcmd = (command_network_t*)handle->userptr;

   if (netcmd->net_fd < 0)
      return;

   for (;;)
   {
      netcmd->cmd_source_len = sizeof(netcmd->cmd_source);

      if ((ret = recvfrom(netcmd->net_fd, buf, sizeof(buf) - 1, 0,
                  (struct sockaddr*)&netcmd->cmd_source,
                  &netcmd->cmd_source_len)) <= 0)
         return;

      buf[ret] = '\0';

      command_parse_msg(handle, buf);
      /* See command_generation: the command may have freed this
       * object. The remaining datagrams wait for the next poll. */
      if (     input_driver_command_generation() != gen
            || command_interfaces_held())
         return;
   }
}

command_t* command_network_new(uint16_t port, const char *bind_address)
{
   struct addrinfo     *res  = NULL;
   command_t            *cmd = NULL;
   command_network_t *netcmd = NULL;
   int                    fd = -1;

   /* Allocate-then-check in sequence, matching command_uds_new.
    * The previous code allocated cmd + netcmd back-to-back and then
    * assigned netcmd->net_fd / cmd->userptr before NULL-checking
    * either pointer - an OOM on either calloc would NULL-deref on
    * line 252/253.  It also leaked netcmd on the '!cmd' failure
    * path via the 'error' label (which now cleanly frees both
    * because they are initialised to NULL up-front; free(NULL) is
    * a no-op). */
   if (!(cmd = (command_t*)calloc(1, sizeof(*cmd))))
      goto error;

   if (!(netcmd = (command_network_t*)calloc(1, sizeof(command_network_t))))
      goto error;

   /* An empty bind address means every interface (NULL server ->
    * AI_PASSIVE -> 0.0.0.0), which is what this interface has always
    * done. Anyone on that network can then send LOAD_CORE or
    * WRITE_CORE_RAM, so users who only drive RetroArch from the same
    * machine can set network_cmd_bind_address = "127.0.0.1". */
   if (bind_address && !*bind_address)
      bind_address = NULL;

   fd = socket_init((void**)&res, port, bind_address,
         SOCKET_TYPE_DATAGRAM, AF_INET);

   RARCH_LOG("[NetCMD] %s %hu (%s).\n",
         msg_hash_to_str(MSG_BRINGING_UP_COMMAND_INTERFACE_ON_PORT),
         (unsigned short)port,
         bind_address ? bind_address : "all interfaces");

   if (fd < 0)
      goto error;

   netcmd->net_fd = fd;
   cmd->userptr   = netcmd;
   cmd->poll      = command_network_poll;
   cmd->replier    = network_command_reply;
   cmd->reply_dest = network_command_reply_dest;
   cmd->reply_to   = network_command_reply_to;
   cmd->destroy    = network_command_free;

   if (!socket_nonblock(netcmd->net_fd))
      goto error;

   if (!socket_bind(netcmd->net_fd, (void*)res))
   {
      RARCH_ERR("[NetCMD] %s\n",
            msg_hash_to_str(MSG_FAILED_TO_BIND_SOCKET));
      goto error;
   }

   freeaddrinfo_retro(res);
   return cmd;

error:
   if (res)
      freeaddrinfo_retro(res);
   /* The only path that reaches 'error' with fd >= 0 is the
    * socket_nonblock / socket_bind failure after fd was already
    * stored in netcmd->net_fd.  Close it so we don't leak the
    * socket file descriptor along with the allocations. */
   if (fd >= 0)
      socket_close(fd);
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

static void stdin_command_reply(command_t *cmd,
   const char *s, size_t len)
{
   /* Just write to stdout! */
   fwrite(s, 1, len, stdout);
   fflush(stdout);
}

static void stdin_command_free(command_t *handle)
{
   free(handle->userptr);
   free(handle);
}

static void command_stdin_poll(command_t *handle)
{
   command_stdin_t *stdincmd = (command_stdin_t*)handle->userptr;
   ssize_t               ret = read_stdin(
         stdincmd->stdin_buf + stdincmd->stdin_buf_ptr,
         CMD_BUF_SIZE - stdincmd->stdin_buf_ptr - 1);

   if (ret != 0)
   {
      char *last_newline = NULL;
      stdincmd->stdin_buf_ptr                      += ret;
      
      /* Ensure we don't write past buffer bounds */
      if (stdincmd->stdin_buf_ptr >= CMD_BUF_SIZE)
         stdincmd->stdin_buf_ptr = CMD_BUF_SIZE - 1;
         
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
      }
      else
      {
         ptrdiff_t msg_len;
         size_t    rest;
         char     *msg;
         *last_newline++ = '\0';
         msg_len         = last_newline - stdincmd->stdin_buf;
         rest            = stdincmd->stdin_buf_ptr - msg_len;

         /* A command can free this object from under us (see
          * command_generation): take the message off the buffer, tidy the
          * buffer, and only then run the message from a private copy,
          * touching nothing of this object afterwards. */
         msg = strdup(stdincmd->stdin_buf);
         memmove(stdincmd->stdin_buf, last_newline, rest);
         stdincmd->stdin_buf_ptr = rest;
         if (msg)
         {
            command_parse_msg(handle, msg);
            free(msg);
         }
         return;
      }
   }
}

command_t* command_stdin_new(void)
{
   command_t *cmd;
   command_stdin_t *stdincmd;

#if !(defined(_WIN32) || defined(__EMSCRIPTEN__))
#ifdef HAVE_NETWORKING
   if (!socket_nonblock(STDIN_FILENO))
      return NULL;
#endif
#endif

   /* Allocate in order with per-step failure handling.  The earlier
    * form allocated both cmd and stdincmd back-to-back then checked
    * them, which leaked stdincmd on the '!cmd' failure path (the
    * '!cmd' return simply dropped the stdincmd pointer).  Also
    * matches the command_uds_new pattern further down. */
   if (!(cmd = (command_t*)calloc(1, sizeof(command_t))))
      return NULL;

   if (!(stdincmd = (command_stdin_t*)calloc(1, sizeof(command_stdin_t))))
   {
      free(cmd);
      return NULL;
   }

   cmd->userptr = stdincmd;
   cmd->poll    = command_stdin_poll;
   cmd->replier = stdin_command_reply;
   cmd->destroy = stdin_command_free;

   return cmd;
}
#endif

#if defined(__EMSCRIPTEN__)
#include "frontend/drivers/platform_emscripten.h"
typedef struct
{
   char command_buf[CMD_BUF_SIZE];
} command_emscripten_t;

static void emscripten_command_reply(command_t *_cmd,
   const char *s, size_t len)
{
   platform_emscripten_command_reply(s, len);
}

static void emscripten_command_free(command_t *handle)
{
   free(handle->userptr);
   free(handle);
}

static void command_emscripten_poll(command_t *handle)
{
   command_emscripten_t *emscriptencmd = (command_emscripten_t*)handle->userptr;
   ptrdiff_t msg_len = platform_emscripten_command_read((char **)(&emscriptencmd->command_buf), CMD_BUF_SIZE);
   if (msg_len != 0)
      command_parse_msg(handle, emscriptencmd->command_buf);
}

command_t* command_emscripten_new(void)
{
   command_t *cmd;
   command_emscripten_t *emscriptencmd;

   /* Same sequential-allocation-with-per-step-cleanup pattern as
    * command_stdin_new / command_uds_new.  Previously allocated both
    * structs before any NULL check, leaking emscriptencmd on the
    * '!cmd' failure path. */
   if (!(cmd = (command_t*)calloc(1, sizeof(command_t))))
      return NULL;

   if (!(emscriptencmd = (command_emscripten_t*)calloc(1, sizeof(command_emscripten_t))))
   {
      free(cmd);
      return NULL;
   }

   cmd->userptr = emscriptencmd;
   cmd->poll    = command_emscripten_poll;
   cmd->replier = emscripten_command_reply;
   cmd->destroy = emscripten_command_free;

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
   /* Which connection holds each slot: a closed descriptor's number
    * is handed out again, so the number alone does not say. */
   unsigned serial[MAX_USER_CONNECTIONS];
   unsigned next_serial;
   /* Last received user socket */
   int last_fd;
   unsigned last_serial;
} command_uds_t;

typedef struct
{
   int fd;
   unsigned serial;
} command_uds_dest_t;

static void uds_command_reply(command_t *cmd,
      const char *s, size_t len)
{
   command_uds_t *subcmd = (command_uds_t*)cmd->userptr;
   write(subcmd->last_fd, s, len);
}

static void *uds_command_reply_dest(command_t *cmd)
{
   command_uds_t      *subcmd = (command_uds_t*)cmd->userptr;
   command_uds_dest_t *dest   = (command_uds_dest_t*)malloc(sizeof(*dest));
   if (dest)
   {
      dest->fd     = subcmd->last_fd;
      dest->serial = subcmd->last_serial;
   }
   return dest;
}

/* Only to the connection that asked: gone, the reply is dropped. */
static void uds_command_reply_to(command_t *cmd, void *data,
      const char *s, size_t len)
{
   int i;
   command_uds_t      *subcmd = (command_uds_t*)cmd->userptr;
   command_uds_dest_t *dest   = (command_uds_dest_t*)data;
   for (i = 0; i < MAX_USER_CONNECTIONS; i++)
      if (   subcmd->userfd[i] == dest->fd
          && subcmd->serial[i] == dest->serial)
      {
         write(dest->fd, s, len);
         return;
      }
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
   unsigned gen = input_driver_command_generation();
   int i;
   int fd;
   ssize_t ret;
   char buf[2048];
   command_uds_t *udscmd = (command_uds_t*)handle->userptr;

   if (udscmd->sfd < 0)
      return;

   /* Read data from clients and process commands */
   for (i = 0; i < MAX_USER_CONNECTIONS; i++)
   {
      bool err = false;

      fd = udscmd->userfd[i];
      if (fd < 0)
         continue;

      ret = socket_receive_all_nonblocking(fd, &err, buf, sizeof(buf) - 1);
      if (!ret)
         continue;

      if (!err)
      {
         buf[ret]            = '\0';
         udscmd->last_fd     = fd;
         udscmd->last_serial = udscmd->serial[i];

         command_parse_msg(handle, buf);
         /* See command_generation: this object may be gone now. */
         if (     input_driver_command_generation() != gen
               || command_interfaces_held())
            return;
      }
      else
      {
         socket_close(fd);
         udscmd->userfd[i] = -1;
      }
   }

   /* Accepts new connections from clients */
   fd = accept(udscmd->sfd, NULL, NULL);
   if (fd >= 0)
   {
      if (socket_nonblock(fd))
      {
         for (i = 0; i < MAX_USER_CONNECTIONS; i++)
         {
            if (udscmd->userfd[i] < 0)
            {
               udscmd->userfd[i] = fd;
               udscmd->serial[i] = ++udscmd->next_serial;
               return;
            }
         }
      }

      socket_close(fd);
   }
}

command_t* command_uds_new(void)
{
   int i;
   command_t *cmd;
   command_uds_t *subcmd;
   struct sockaddr_un addr;
   socklen_t addrsz = offsetof(struct sockaddr_un, sun_path) + STRLEN_CONST("retroarch/cmd") + 1;
   int           fd = socket(AF_UNIX, SOCK_STREAM, 0);
   if (fd < 0)
      return NULL;
   /* use an abstract socket for simplicity */
   memset(&addr, 0, sizeof(addr));
   addr.sun_family = AF_UNIX;
   strlcpy_lit(&addr.sun_path[1], "retroarch/cmd", sizeof(addr.sun_path) - 1);
   if (   bind(fd, (struct sockaddr*)&addr, addrsz) < 0
       || listen(fd, MAX_USER_CONNECTIONS) < 0
       || !socket_nonblock(fd))
   {
      socket_close(fd);
      return NULL;
   }
   cmd = (command_t*)calloc(1, sizeof(command_t));
   if (!cmd)
   {
      socket_close(fd);
      return NULL;
   }
   subcmd = (command_uds_t*)calloc(1, sizeof(command_uds_t));
   if (!subcmd)
   {
      free(cmd);
      socket_close(fd);
      return NULL;
   }
   subcmd->sfd          = fd;
   subcmd->last_fd      = -1;
   for (i = 0; i < MAX_USER_CONNECTIONS; i++)
      subcmd->userfd[i] = -1;
   cmd->userptr         = subcmd;
   cmd->poll            = command_uds_poll;
   cmd->replier         = uds_command_reply;
   cmd->reply_dest      = uds_command_reply_dest;
   cmd->reply_to        = uds_command_reply_to;
   cmd->destroy         = uds_command_free;
   return cmd;
}
#endif


/* Routines used to invoke retroarch command ... */

/* The family tool a command runs for, so its answers carry the tool's
 * name: STATE save 1 rather than SAVE_STATE_SLOT 1. */
struct command_family
{
   const char        *family;
   const char        *word;
   const char        *name;
   command_replier_t  replier;
   /* the command answered, or will */
   bool               answered;
};
static struct command_family command_family_now;

/* @msg under @f's name, allocated, its length in @out_len; NULL where
 * it could not be, or where it is to go as it is. */
static char *command_family_rename(const struct command_family *f,
      const char *msg, size_t len, size_t *out_len)
{
   size_t n    = strlen(f->name);
   size_t size = len + strlen(f->family) + strlen(f->word) + 3;
   char  *s    = (char*)malloc(size);
   int    _len;
   if (!s)
      return NULL;
   if (     len > n && !strncmp(msg, f->name, n)
         && (msg[n] == ' ' || msg[n] == '\n' || msg[n] == ','))
   {
      msg += n;
      len -= n;
      if (!strncmp(msg, " ERROR", 6))
         _len = snprintf(s, size, "%s%.*s", f->family, (int)len, msg);
      else
         _len = snprintf(s, size, "%s %s%.*s", f->family, f->word,
               (int)len, msg);
   }
   else if (len == n && !strncmp(msg, f->name, n))
      _len = snprintf(s, size, "%s %s", f->family, f->word);
   /* rows, which name nothing, are the tool's answer as they are */
   else
   {
      free(s);
      return NULL;
   }
   if (_len <= 0 || (size_t)_len >= size)
   {
      free(s);
      return NULL;
   }
   *out_len = (size_t)_len;
   return s;
}

static void command_family_replier(command_t *cmd, const char *data,
      size_t len)
{
   size_t _len = 0;
   char  *msg  = command_family_rename(&command_family_now, data, len,
         &_len);
   command_family_now.answered = true;
   if (msg)
      command_family_now.replier(cmd, msg, _len);
   else
      command_family_now.replier(cmd, data, len);
   free(msg);
}

/* A reply sent after its command has returned: where it goes is taken
 * while the command runs, as afterwards the interface may be answering
 * someone else. */
typedef struct
{
   command_t            *cmd;
   void                 *dest;
   struct command_family family;
   unsigned              gen;
} command_deferred_t;

static void command_deferred_take(command_deferred_t *d, command_t *cmd)
{
   d->cmd  = cmd;
   d->dest = cmd->reply_dest ? cmd->reply_dest(cmd) : NULL;
   d->gen    = input_driver_command_generation();
   d->family = command_family_now;
   if (command_family_now.family)
      command_family_now.answered = true;
}

/* Sends @msg (none when NULL) and releases @d.  Dropped, unsent, if the
 * interface was torn down meanwhile (the command generation moved).
 * @is_error says the reply reports a failure, so the interface sending
 * it is told rather than left to read the text. */
static void command_deferred_send(command_deferred_t *d,
      const char *msg, size_t len, bool is_error)
{
   char *renamed = NULL;
   if (msg && input_driver_command_generation() == d->gen)
   {
      size_t _len = 0;
      if (d->family.family && (renamed = command_family_rename(&d->family,
               msg, len, &_len)))
      {
         msg = renamed;
         len = _len;
      }
      d->cmd->error = is_error;
      if (d->cmd->reply_to && d->dest)
         d->cmd->reply_to(d->cmd, d->dest, msg, len);
      else if (!d->cmd->reply_dest)
         d->cmd->replier(d->cmd, msg, len);
   }
   free(renamed);
   free(d->dest);
   d->dest = NULL;
}

#ifdef HAVE_SCREENSHOTS
/* SCREENSHOT's answer, and GET_FRAME's - the frame written nowhere, so
 * with no path: the picture where the interface takes one, with the
 * file's path or the frame's size as its text. */
static void command_screenshot_done(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   command_deferred_t             *d = (command_deferred_t*)user_data;
   const struct screenshot_result *r = (const struct screenshot_result*)task_data;
   bool                        frame = r && !*r->path;
   char msg[PATH_MAX_LENGTH];
   if (error || !r || (frame && !r->png_base64))
   {
      snprintf(msg, sizeof(msg), "%s ERROR %s",
            frame ? "GET_FRAME" : "SCREENSHOT",
            error ? error : "no result");
      command_deferred_send(d, msg, strlen(msg), true);
   }
   else
   {
      if (frame)
         snprintf(msg, sizeof(msg), "GET_FRAME %ux%u", r->width, r->height);
      else
         strlcpy(msg, r->path, sizeof(msg));
      if (     r->png_base64 && d->cmd->reply_image_to && d->dest
            && input_driver_command_generation() == d->gen)
      {
         d->cmd->reply_image_to(d->cmd, d->dest, msg, strlen(msg),
               "image/png", r->png_base64, r->png_base64_len);
         command_deferred_send(d, NULL, 0, false);
      }
      else
         command_deferred_send(d, msg, strlen(msg), false);
   }
   free(d);
}

/* SCREENSHOT run by command_run(): answered with the file's path once
 * the screenshot task has written it. */
static bool command_screenshot(command_t *cmd)
{
   settings_t      *settings   = config_get_ptr();
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   command_deferred_t *d       = (command_deferred_t*)calloc(1, sizeof(*d));
   if (!d)
      return false;
   /* Threaded (use_thread true), so the callback, which reads @d, runs
    * from the task queue on a later frame, after it is taken below.
    * Unthreaded, the callback would run inside this call and free @d
    * before it was taken. It is taken only once the screenshot is
    * queued, as taking it marks the asking MCP request as waiting. */
   if (!take_screenshot_notify(settings->paths.directory_screenshot,
            runloop_st->runtime_content_path_basename, false,
            video_driver_cached_frame_is_hw_render(), false, true,
            command_screenshot_done, d))
   {
      free(d);
      return false;
   }
   /* the task's callback, which uses this, runs on a later frame */
   command_deferred_take(d, cmd);
   return true;
}

#ifdef HAVE_RPNG
/* GET_FRAME [size]: the frame on screen as image content, scaled to fit
 * a square of @size and never up. Nothing is written: a software core's
 * frame is copied, a hardware core's read back from the GPU. */
bool command_get_frame(command_t *cmd, const char *arg)
{
   command_deferred_t *d;
   unsigned long       size = COMMAND_FRAME_SIZE;
   char               *end  = NULL;
   while (arg && *arg == ' ')
      arg++;
   if (arg && *arg)
   {
      size = strtoul(arg, &end, 10);
      if (     *arg < '0' || *arg > '9' || *end
            || size < COMMAND_FRAME_SIZE_MIN || size > COMMAND_FRAME_SIZE_MAX)
      {
         command_reply_error(cmd, "GET_FRAME", "a size from %u to %u",
               COMMAND_FRAME_SIZE_MIN, COMMAND_FRAME_SIZE_MAX);
         return false;
      }
   }
   /* the line interfaces carry text alone */
   if (!cmd->reply_image_to)
   {
      command_reply_error(cmd, "GET_FRAME", "this interface carries no "
            "image; SCREENSHOT writes one to a file");
      return false;
   }
   if (!(runloop_get_flags() & RUNLOOP_FLAG_CORE_RUNNING))
   {
      command_reply_error(cmd, "GET_FRAME", "no content is running");
      return false;
   }
   if (!(d = (command_deferred_t*)calloc(1, sizeof(*d))))
      return false;
   if (!take_frame_notify((unsigned)size, (unsigned)size,
            command_screenshot_done, d))
   {
      free(d);
      command_reply_error(cmd, "GET_FRAME", "the frame could not be "
            "taken: none has been drawn, or a screenshot is being taken");
      return false;
   }
   command_deferred_take(d, cmd);
   return true;
}
#endif
#endif

/* A command answered once the work it starts is through: by @ok, else
 * by @name and the task's result, or by @name, ERROR and the reason. */
struct command_reply
{
   command_deferred_t   deferred;
   enum event_command   event;
   /* Run instead of the event, when set; false has the failure
    * reported for it. */
   bool               (*start)(struct command_reply *r);
   const char          *name;
   char                 ok[64];
   char                 path[PATH_MAX_LENGTH];
   /* a load that ends on the dummy core failed */
   bool                 want_core;
   /* LOAD_CONTENT ... paused: pause once it has loaded */
   bool                 pause;
   /* what succeeded loaded a state */
   bool                 state_loaded;
   bool                 reset;
};

static struct command_reply *command_reply_new(const char *name)
{
   struct command_reply *r = (struct command_reply*)calloc(1, sizeof(*r));
   if (r)
      r->name = name;
   return r;
}

static void command_reply_done(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   struct command_reply *r = (struct command_reply*)user_data;
   char msg[PATH_MAX_LENGTH + 128];
   /* a core that fails to start is replaced by the dummy core, and the
    * load goes on to succeed */
   if (     !error && r->want_core
         && runloop_state_get_ptr()->current_core_type == CORE_TYPE_DUMMY)
      error = msg_hash_to_str(MSG_FAILED_TO_LOAD_CONTENT);
   if (error)
      snprintf(msg, sizeof(msg), "%s ERROR %s", r->name, error);
   else
   {
      if (*r->ok)
         strlcpy(msg, r->ok, sizeof(msg));
      else if (task_data)
         snprintf(msg, sizeof(msg), "%s %s", r->name, (const char*)task_data);
      else
         strlcpy(msg, r->name, sizeof(msg));
      if (r->state_loaded)
         command_post_state_loaded();
#ifdef HAVE_COMMAND
      if (     command_queue_ended
            && (r->state_loaded || r->reset))
         strlcat(msg, ", queue ended", sizeof(msg));
#endif
      /* a pause is dropped with no core running yet, as after a cold
       * start: then it is the first frame's to do */
      if (r->pause)
      {
         if (runloop_get_flags() & RUNLOOP_FLAG_CORE_RUNNING)
            command_event(CMD_EVENT_PAUSE, NULL);
         else
            runloop_frame_work_set(RUNLOOP_WORK_PAUSE_LOADED, true);
      }
   }
   command_deferred_send(&r->deferred, msg, strlen(msg), error != NULL);
   free(r);
}

/* Runs the work from a main-thread task, outside the core's run: a
 * command can arrive from the core's own input poll. */
static void command_reply_handler(retro_task_t *task)
{
   struct command_reply *r = (struct command_reply*)task->state;
   task_notify_t notify;
   task->state      = NULL;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
   notify.cb        = command_reply_done;
   notify.user_data = r;
#ifdef HAVE_COMMAND
   command_queue_ended = false;
#endif
   if (!(r->start ? r->start(r) : command_event(r->event, &notify)))
      command_reply_done(NULL, NULL, r, "The command failed.");
}

/* Takes @r, NULL included. */
static bool command_reply_push(command_t *cmd, struct command_reply *r)
{
   retro_task_t *task;
   if (!r)
      return false;
   if (!(task = task_init()))
   {
      free(r);
      return false;
   }
   task->handler = command_reply_handler;
   task->state   = r;
   task->flags  |= RETRO_TASK_FLG_MAIN_THREAD | RETRO_TASK_FLG_MUTE;
   if (!task_queue_push(task))
   {
      free(task);
      free(r);
      return false;
   }
   command_deferred_take(&r->deferred, cmd);
   return true;
}

#ifdef HAVE_BSV_MOVIE
/* Pushes @event as the work of command @name. */
static bool command_reply_event(command_t *cmd, enum event_command event,
      const char *name)
{
   struct command_reply *r = command_reply_new(name);
   if (r)
      r->event = event;
   return command_reply_push(cmd, r);
}
#endif

#ifdef HAVE_TRANSLATE
/* AI_SERVICE run by command_run(): one translation of the screen,
 * answered with the service's text. */
static bool command_ai_service_start(struct command_reply *r)
{
   settings_t *settings = config_get_ptr();
   if (!settings->bools.ai_service_enable)
      return false;
   return run_translation_service_notify(settings,
         (runloop_get_flags() & RUNLOOP_FLAG_PAUSED) != 0,
         command_reply_done, r);
}
#endif

/* Answers @cmd once the content load in progress, or with @next the
 * one the command leads to, is through. */
static bool command_content_reply(command_t *cmd, const char *name,
      bool next, bool want_core, bool pause)
{
   struct command_reply *r;
   /* a line client is answered too where it asked for a pause, which
    * happens here */
   if (     (!cmd->structured && !pause)
         || (!next && !task_content_load_pending())
         || !(r = command_reply_new(name)))
      return true;
   r->want_core = want_core;
   r->pause     = pause;
   task_notify_set(&content_state_get_ptr()->load_notify,
         command_reply_done, r);
   command_deferred_take(&r->deferred, cmd);
   return true;
}

/* Runs @name with @arg (NULL or "" for none) as if it had arrived as a
 * command line on @handle: replies go to @handle's replier, a hotkey
 * presses on @handle for one frame. For interfaces that receive
 * commands as structured requests rather than lines.
 *
 * Returns: false for an unknown command or one whose handler failed. */
bool command_run(command_t *handle, const char *name, const char *arg)
{
   unsigned i;
   handle->error = false;
   for (i = 0; i < ARRAY_SIZE(action_map); i++)
      if (string_is_equal(name, action_map[i].str))
         return action_map[i].action(handle, arg ? arg : "");
   for (i = 0; i < ARRAY_SIZE(map); i++)
      if (string_is_equal(name, map[i].str))
      {
         if (map[i].id == RARCH_MENU_TOGGLE)
            command_event(CMD_EVENT_MENU_TOGGLE, NULL);
#ifdef HAVE_SCREENSHOTS
         else if (map[i].id == RARCH_SCREENSHOT)
            return command_screenshot(handle);
#endif
#ifdef HAVE_BSV_MOVIE
         else if (map[i].id == RARCH_RECORD_REPLAY_KEY)
            return command_reply_event(handle,
                  CMD_EVENT_RECORD_REPLAY, map[i].str);
         else if (map[i].id == RARCH_SAVE_REPLAY_CHECKPOINT_KEY)
            return command_reply_event(handle,
                  CMD_EVENT_SAVE_REPLAY_CHECKPOINT, map[i].str);
         else if (map[i].id == RARCH_PREV_REPLAY_CHECKPOINT_KEY)
            return command_reply_event(handle,
                  CMD_EVENT_PREV_REPLAY_CHECKPOINT, map[i].str);
         else if (map[i].id == RARCH_NEXT_REPLAY_CHECKPOINT_KEY)
            return command_reply_event(handle,
                  CMD_EVENT_NEXT_REPLAY_CHECKPOINT, map[i].str);
#endif
#ifdef HAVE_TRANSLATE
         else if (map[i].id == RARCH_AI_SERVICE)
         {
            struct command_reply *r = command_reply_new(map[i].str);
            if (r)
               r->start = command_ai_service_start;
            return command_reply_push(handle, r);
         }
#endif
         else
            handle->state[map[i].id] = true;
         /* The press goes out on the next poll and the hotkey acts a
          * frame later, so there is nothing yet to report but that it
          * was sent. A structured client is told that much rather than
          * a bare "done": what the hotkey changed is read back with
          * GET_OPTION. MENU_TOGGLE is the one that has already acted,
          * above, and reports the menu instead. */
         if (handle->structured)
         {
            char   reply[NAME_MAX_LENGTH + 16];
            size_t _len;
#ifdef HAVE_MENU
            if (map[i].id == RARCH_MENU_TOGGLE)
            {
               struct menu_state *menu_st = menu_state_get_ptr();
               _len = (size_t)snprintf(reply, sizeof(reply), "%s %s\n",
                     map[i].str,
                     (menu_st && (menu_st->flags & MENU_ST_FLAG_ALIVE))
                        ? "open" : "closed");
            }
            else
#endif
               _len = (size_t)snprintf(reply, sizeof(reply), "%s pressed\n",
                     map[i].str);
            handle->replier(handle, reply, _len);
         }
         return true;
      }
   return false;
}

/* MENU, DISK, REINIT, STATE, REPLAY and CORE: a family of commands as one,
 * so a structured client is offered one tool with a choice rather than
 * a tool apiece. Each word runs as the command it stands for, which
 * line clients can still send by name; @rest says it takes what
 * follows the word, a slot or a frame. */
struct cmd_choice
{
   const char *word;
   const char *name;
   bool        rest;
};

static bool command_run_choice(command_t *cmd, const char *family,
      const struct cmd_choice *choices, size_t count, const char *values,
      const char *arg)
{
   size_t      i;
   size_t      n;
   const char *rest;
   while (arg && *arg == ' ')
      arg++;
   if (!arg)
      arg = "";
   for (rest = arg; *rest && *rest != ' '; rest++)
      ;
   n = (size_t)(rest - arg);
   while (*rest == ' ')
      rest++;
   for (i = 0; i < count; i++)
   {
      if (strncmp(arg, choices[i].word, n) || choices[i].word[n])
         continue;
      if (!choices[i].rest && *rest)
      {
         command_reply_error(cmd, family, "nothing after %s",
               choices[i].word);
         return false;
      }
      {
         bool ret;
         command_family_now.family  = family;
         command_family_now.word    = choices[i].word;
         command_family_now.name    = choices[i].name;
         command_family_now.replier = cmd->replier;
         cmd->replier               = command_family_replier;
         ret                        = command_run(cmd, choices[i].name, rest);
         cmd->replier               = command_family_now.replier;
         /* what answers nothing of its own still answers as the tool */
         if (ret && !command_family_now.answered && cmd->structured)
         {
            char   msg[64];
            int    _len = snprintf(msg, sizeof(msg), "%s %s\n", family,
                  choices[i].word);
            if (_len > 0 && (size_t)_len < sizeof(msg))
               cmd->replier(cmd, msg, (size_t)_len);
         }
         memset(&command_family_now, 0, sizeof(command_family_now));
         return ret;
      }
   }
   command_reply_error(cmd, family, "one of %s", values);
   return false;
}

#ifdef HAVE_MENU
/* MENU: the request whose action the next menu frame runs, as the
 * menu's own input would, answered a frame after that with what the
 * menu shows then: a list or a message an action opens is filled in by
 * the frame that follows it. */
static struct
{
   command_deferred_t deferred;
   size_t             index;
   unsigned           from;
   unsigned           action;
   /* menu frames still to run before the answer: the action's, then one */
   unsigned           frames;
   bool               pending;
   bool               has_index;
   bool               has_from;
   bool               lost;
   /* the list the index was given in: how deep, and its label */
   size_t             depth;
   char               list[64];
} command_menu_st;

/* How deep the list shown is, and its label in @s. */
static size_t command_menu_where(char *s, size_t len)
{
   menu_list_t *list  = menu_state_get_ptr()->entries.list;
   file_list_t *stack = list ? MENU_LIST_GET(list, 0) : NULL;
   s[0] = '\0';
   if (!stack || !stack->size)
      return 0;
   if (stack->list[stack->size - 1].label)
      strlcpy(s, stack->list[stack->size - 1].label, len);
   return stack->size;
}

static void command_menu_clean(char *s)
{
   for (; *s; s++)
      if (*s == '\t' || *s == '\n' || *s == '\r')
         *s = ' ';
}

/* The title of the list @i deep in @stack. A playlist's tab is titled
 * "Horizontal Menu" there; its playlist's name says which it is. */
static void command_menu_level_title(const file_list_t *stack, size_t i,
      char *s, size_t len)
{
   struct menu_state       *menu_st = menu_state_get_ptr();
   const menu_ctx_driver_t *drv     = menu_st->driver_ctx;
   menu_file_list_cbs_t    *cbs     =
      (menu_file_list_cbs_t*)stack->list[i].actiondata;
   s[0] = '\0';
   if (     stack->list[i].type == MENU_SETTING_HORIZONTAL_MENU
         && drv && drv->list_get_entry && drv->list_get_selection
         && drv->list_get_size)
   {
      size_t sel  = drv->list_get_selection(menu_st->userdata);
      size_t tabs = drv->list_get_size(menu_st->userdata, MENU_LIST_TABS);
      struct item_file *item = sel > tabs
         ? (struct item_file*)drv->list_get_entry(menu_st->userdata,
               MENU_LIST_HORIZONTAL, (unsigned)(sel - (tabs + 1)))
         : NULL;
      if (item && item->path && *item->path)
      {
         strlcpy(s, path_basename(item->path), len);
         path_remove_extension(s);
         return;
      }
   }
   if (i + 1 == stack->size)
      menu_entries_get_title(s, len);
   else if (cbs && cbs->action_get_title)
      cbs->action_get_title(stack->list[i].path, stack->list[i].label,
            stack->list[i].type, s, len);
}

/* What the menu shows: its title, a page of entries - number, label
 * and value - holding the selection, or from @from, and which entry is
 * selected of how many. NULL where out of memory; the length in
 * @out_len. */
static char *command_menu_answer(bool has_from, unsigned from,
      size_t *out_len)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t       *list    = menu_st->entries.list;
   file_list_t       *buf     = list
      ? MENU_LIST_GET_SELECTION(list, (unsigned)0) : NULL;
   size_t             count   = buf ? buf->size : 0;
   size_t             sel     = menu_st->selection_ptr;
   size_t             size    = 8192 + COMMAND_MENU_PAGE * 560;
   size_t             _len;
   size_t             i;
   size_t             first;
   char              *out;
   menu_entry_t      *entry;

   if (!(out = (char*)malloc(size)))
      return NULL;
   if (!(menu_st->flags & MENU_ST_FLAG_ALIVE) || !buf)
   {
      *out_len = strlcpy(out, "CLOSED\n", size);
      return out;
   }
   if (!(entry = (menu_entry_t*)malloc(sizeof(*entry))))
   {
      free(out);
      return NULL;
   }
   {
      file_list_t *stack = MENU_LIST_GET(list, 0);
      char         title[256];
      title[0] = '\0';
      if (stack && stack->size)
         command_menu_level_title(stack, stack->size - 1, title,
               sizeof(title));
      command_menu_clean(title);
      _len = (size_t)snprintf(out, size, "MENU %s\n", title);
      /* how it got there: the open menu remembers where it was left */
      if (stack && stack->size > 1)
      {
         size_t d;
         _len += strlcpy(out + _len, "PATH ", size - _len);
         for (d = 0; d < stack->size && d < 16; d++)
         {
            command_menu_level_title(stack, d, title, sizeof(title));
            command_menu_clean(title);
            _len += (size_t)snprintf(out + _len, size - _len, "%s%s",
                  d ? " > " : "", title);
         }
         _len += strlcpy(out + _len, "\n", size - _len);
      }
   }
   /* what waits on the user over the list: typing, or a message that a
    * second a confirms - "Press again to quit" */
   if (menu_input_dialog_get_display_kb())
   {
      char label[sizeof(menu_st->input_dialog_kb_label)];
      strlcpy(label, menu_st->input_dialog_kb_label, sizeof(label));
      command_menu_clean(label);
      _len += (size_t)snprintf(out + _len, size - _len, "KEYBOARD %s\n",
            label);
   }
   else if (menu_st->driver_data && *menu_st->driver_data->menu_state_msg)
   {
      char msg[sizeof(menu_st->driver_data->menu_state_msg)];
      strlcpy(msg, menu_st->driver_data->menu_state_msg, sizeof(msg));
      command_menu_clean(msg);
      _len += (size_t)snprintf(out + _len, size - _len, "MESSAGE %s\n",
            msg);
   }
   first = has_from ? from : sel - sel % COMMAND_MENU_PAGE;
   for (i = first; i < count && i < first + COMMAND_MENU_PAGE; i++)
   {
      char       *label;
      char       *value;
      const char *shown;
      MENU_ENTRY_INITIALIZE((*entry));
      entry->flags |= MENU_ENTRY_FLAG_PATH_ENABLED
                    | MENU_ENTRY_FLAG_RICH_LABEL_ENABLED
                    | MENU_ENTRY_FLAG_VALUE_ENABLED;
      menu_entry_get(entry, 0, i, NULL, true);
      label = *entry->rich_label ? entry->rich_label : entry->path;
      value = entry->enum_idx == MENU_ENUM_LABEL_CHEEVOS_PASSWORD
         ? entry->password_value : entry->value;
      command_menu_clean(label);
      command_menu_clean(value);
      /* a choice in a list of them has no value, only whether it is the
       * one chosen */
      shown = !*value && (entry->flags & MENU_ENTRY_FLAG_CHECKED)
         ? "checked" : value;
      _len += (size_t)snprintf(out + _len, size - _len, "%u\t%.255s\t%.255s\n",
            (unsigned)i, label, shown);
   }
   _len += (size_t)snprintf(out + _len, size - _len, "SELECTED %u\nCOUNT %u\n",
         (unsigned)sel, (unsigned)count);
   if (i < count)
      _len += (size_t)snprintf(out + _len, size - _len, "MORE %u\n",
            (unsigned)i);
   free(entry);
   *out_len = _len;
   return out;
}

/* The menu's own action for this frame, or the one a MENU request
 * asks for in its place: on the entry it names, where it names one. */
unsigned command_menu_action(unsigned action)
{
   struct menu_state *menu_st = menu_state_get_ptr();
   char               where[sizeof(command_menu_st.list)];
   if (!command_menu_st.pending || command_menu_st.frames < 2)
      return action;
   /* a request is input, as a press is: no screensaver comes over it,
    * and one up takes the press that ends it */
   menu_st->input_last_time_us = menu_st->current_time_us;
   if (menu_st->flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE)
   {
      menu_st->flags &= ~MENU_ST_FLAG_SCREENSAVER_ACTIVE;
      if (menu_st->driver_ctx && menu_st->driver_ctx->environ_cb)
         menu_st->driver_ctx->environ_cb(MENU_ENVIRON_DISABLE_SCREENSAVER,
               NULL, menu_st->userdata);
   }
   if (command_menu_st.has_index)
   {
      command_menu_st.has_index = false;
      /* the list may have changed since the request checked it - another
       * list, or this one shorter: then nothing is done to whichever
       * entry took that place */
      if (     !menu_st->entries.list
            || command_menu_where(where, sizeof(where))
               != command_menu_st.depth
            || !string_is_equal(where, command_menu_st.list)
            || command_menu_st.index
               >= MENU_LIST_GET_SELECTION(menu_st->entries.list, 0)->size)
      {
         command_menu_st.lost   = true;
         command_menu_st.action = MENU_ACTION_NOOP;
         return action;
      }
      menu_st->selection_ptr = command_menu_st.index;
      if (menu_st->driver_ctx && menu_st->driver_ctx->navigation_set)
         menu_st->driver_ctx->navigation_set(menu_st->userdata, true);
   }
   return command_menu_st.action;
}

/* Before commands are read: a MENU request whose menu closed before
 * running it - a hotkey, another client - is answered that it is
 * closed, as no menu frame will answer it. */
void command_menu_closed(void)
{
   if (     command_menu_st.pending
         && !(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      command_menu_iterated();
}

/* After a menu frame: answers the MENU request it ran. */
void command_menu_iterated(void)
{
   char  *out;
   size_t _len = 0;
   if (!command_menu_st.pending)
      return;
   if (     --command_menu_st.frames
         && (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      return;
   command_menu_st.pending = false;
   if (command_menu_st.lost)
   {
      command_menu_st.lost = false;
      command_deferred_send(&command_menu_st.deferred,
            "MENU ERROR the list changed before the entry could be "
            "reached\n", STRLEN_CONST("MENU ERROR the list changed before "
               "the entry could be reached\n"), true);
      return;
   }
   if ((out = command_menu_answer(command_menu_st.has_from,
               command_menu_st.from, &_len)))
      command_deferred_send(&command_menu_st.deferred, out, _len, false);
   else
      command_deferred_send(&command_menu_st.deferred,
            "MENU ERROR out of memory\n",
            STRLEN_CONST("MENU ERROR out of memory\n"), true);
   free(out);
}
#endif

/* MENU <action> [index] [from <entry>] */
bool command_menu(command_t *cmd, const char *arg)
{
#ifdef HAVE_MENU
   static const struct
   {
      const char *word;
      unsigned    action;
      bool        index;
   } actions[] = {
      { "show",   MENU_ACTION_NOOP,   false },
      { "open",   MENU_ACTION_NOOP,   false },
      { "close",  MENU_ACTION_NOOP,   false },
      { "select", MENU_ACTION_NOOP,   true  },
      { "up",     MENU_ACTION_UP,     false },
      { "down",   MENU_ACTION_DOWN,   false },
      { "left",   MENU_ACTION_LEFT,   true  },
      { "right",  MENU_ACTION_RIGHT,  true  },
      { "a",      MENU_ACTION_OK,     true  },
      { "b",      MENU_ACTION_CANCEL, false },
      { "type",   MENU_ACTION_NOOP,   false }
   };
   struct menu_state *menu_st = menu_state_get_ptr();
   menu_list_t       *list    = menu_st->entries.list;
   file_list_t       *buf     = list
      ? MENU_LIST_GET_SELECTION(list, (unsigned)0) : NULL;
   bool               alive   = (menu_st->flags & MENU_ST_FLAG_ALIVE) && buf;
   const char        *p       = arg ? arg : "";
   char              *end;
   unsigned long      index   = 0;
   unsigned long      from    = 0;
   bool               has_index = false;
   bool               has_from  = false;
   size_t             n;
   size_t             i;

   while (*p == ' ')
      p++;
   for (n = 0; p[n] && p[n] != ' '; n++)
      ;
   for (i = 0; i < ARRAY_SIZE(actions); i++)
      if (!strncmp(p, actions[i].word, n) && !actions[i].word[n])
         break;
   if (!n || i == ARRAY_SIZE(actions))
   {
      command_reply_error(cmd, "MENU", "one of %s", COMMAND_MENU_ACTIONS);
      return false;
   }
   for (p += n; *p == ' '; p++)
      ;
   /* the rest of the line is the text, spaces and all */
   if (string_is_equal(actions[i].word, "type"))
   {
      if (!alive || !menu_input_dialog_get_display_kb())
      {
         command_reply_error(cmd, "MENU", "no keyboard is open: a on an "
               "entry that asks for text opens one");
         return false;
      }
      for (n = 0; p[n]; n++)
         if ((unsigned char)p[n] < 0x20 || p[n] == 0x7f)
         {
            command_reply_error(cmd, "MENU", "type takes one line of text, "
                  "with no tab or control character");
            return false;
         }
      input_driver_keyboard_line_set(p, n);
      input_keyboard_event(true,  '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
      input_keyboard_event(false, '\n', 0,    0, RETRO_DEVICE_KEYBOARD);
      p += n;
   }
   else if (*p >= '0' && *p <= '9')
   {
      index     = strtoul(p, &end, 10);
      has_index = true;
      for (p = end; *p == ' '; p++)
         ;
   }
   if (!strncmp(p, "from ", 5))
   {
      for (p += 5; *p == ' '; p++)
         ;
      if (*p < '0' || *p > '9')
      {
         command_reply_error(cmd, "MENU", "an entry number after from");
         return false;
      }
      from     = strtoul(p, &end, 10);
      has_from = true;
      for (p = end; *p == ' '; p++)
         ;
   }
   if (*p)
   {
      command_reply_error(cmd, "MENU", "%s [index] [from <entry>], not %s",
            actions[i].word, p);
      return false;
   }
   if (has_index && !actions[i].index)
   {
      command_reply_error(cmd, "MENU", "%s takes no index", actions[i].word);
      return false;
   }
   if (!has_index && string_is_equal(actions[i].word, "select"))
   {
      command_reply_error(cmd, "MENU", "select needs an index");
      return false;
   }
   if (string_is_equal(actions[i].word, "close"))
   {
      char  *out;
      size_t _len = 0;
      if (menu_st->flags & MENU_ST_FLAG_ALIVE)
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      if (menu_st->flags & MENU_ST_FLAG_ALIVE)
      {
         command_reply_error(cmd, "MENU", "the menu stays open with no "
               "content running");
         return false;
      }
      if (!(out = command_menu_answer(false, 0, &_len)))
         return false;
      cmd->replier(cmd, out, _len);
      free(out);
      return true;
   }
   if (string_is_equal(actions[i].word, "open"))
   {
      if (!(menu_st->flags & MENU_ST_FLAG_ALIVE))
         command_event(CMD_EVENT_MENU_TOGGLE, NULL);
      if (!(menu_st->flags & MENU_ST_FLAG_ALIVE))
      {
         command_reply_error(cmd, "MENU", "the menu cannot open now");
         return false;
      }
   }
   else if (!alive && !string_is_equal(actions[i].word, "show"))
   {
      command_reply_error(cmd, "MENU", "the menu is closed: MENU open "
            "opens it");
      return false;
   }
   else if (has_index && index >= buf->size)
   {
      command_reply_error(cmd, "MENU", "no entry %lu: the list has %u",
            index, (unsigned)buf->size);
      return false;
   }
   /* what nothing changes is answered now */
   if (string_is_equal(actions[i].word, "show"))
   {
      char  *out;
      size_t _len = 0;
      if (!(out = command_menu_answer(has_from, (unsigned)from, &_len)))
         return false;
      cmd->replier(cmd, out, _len);
      free(out);
      return true;
   }
   if (command_menu_st.pending)
      command_deferred_send(&command_menu_st.deferred,
            "MENU ERROR replaced by another MENU request\n",
            STRLEN_CONST("MENU ERROR replaced by another MENU request\n"),
            true);
   command_menu_st.action    = actions[i].action;
   command_menu_st.index     = (size_t)index;
   command_menu_st.has_index = has_index;
   command_menu_st.from      = (unsigned)from;
   command_menu_st.has_from  = has_from;
   command_menu_st.depth     = command_menu_where(command_menu_st.list,
         sizeof(command_menu_st.list));
   command_menu_st.lost      = false;
   command_menu_st.frames    = 2;
   command_menu_st.pending   = true;
   command_deferred_take(&command_menu_st.deferred, cmd);
   return true;
#else
   command_reply_error(cmd, "MENU", "this build has no menu");
   return false;
#endif
}

bool command_disk(command_t *cmd, const char *arg)
{
   static const struct cmd_choice choices[] = {
      { "eject", "DISK_EJECT_TOGGLE", false },
      { "next",  "DISK_NEXT",         false },
      { "prev",  "DISK_PREV",         false }
   };
   /* the hotkeys are pressed regardless; the tool says when they can
    * do nothing */
   if (!disk_control_enabled(&runloop_state_get_ptr()->system.disk_control))
   {
      command_reply_error(cmd, "DISK", "this core has no disk control");
      return false;
   }
   return command_run_choice(cmd, "DISK", choices, ARRAY_SIZE(choices),
         COMMAND_DISK_ACTIONS, arg);
}

bool command_reinit(command_t *cmd, const char *arg)
{
   static const struct cmd_choice choices[] = {
      { "video", "VIDEO_REINIT",   false },
      { "audio", "AUDIO_REINIT",   false },
      { "all",   "DRIVERS_REINIT", false }
   };
   return command_run_choice(cmd, "REINIT", choices, ARRAY_SIZE(choices),
         COMMAND_REINIT_DRIVERS, arg);
}

bool command_state(command_t *cmd, const char *arg)
{
   static const struct cmd_choice choices[] = {
      { "save",       "SAVE_STATE_SLOT",  true  },
      { "load",       "LOAD_STATE_SLOT",  true  },
      { "checkpoint", "CHECKPOINT_STATE", false },
      { "rollback",   "ROLLBACK_STATE",   false }
   };
   return command_run_choice(cmd, "STATE", choices, ARRAY_SIZE(choices),
         COMMAND_STATE_ACTIONS, arg);
}

bool command_core(command_t *cmd, const char *arg)
{
   static const struct cmd_choice choices[] = {
      { "load",   "LOAD_CORE",   true  },
      { "start",  "START_CORE",  false },
      { "unload", "UNLOAD_CORE", false }
   };
   return command_run_choice(cmd, "CORE", choices, ARRAY_SIZE(choices),
         COMMAND_CORE_ACTIONS, arg);
}

bool command_replay(command_t *cmd, const char *arg)
{
   static const struct cmd_choice choices[] = {
      { "play",       "PLAY_REPLAY_SLOT",       true  },
      { "seek",       "SEEK_REPLAY",            true  },
      { "record",     "RECORD_REPLAY",          false },
      { "halt",       "HALT_REPLAY",            false },
      { "checkpoint", "SAVE_REPLAY_CHECKPOINT", false }
   };
   return command_run_choice(cmd, "REPLAY", choices, ARRAY_SIZE(choices),
         COMMAND_REPLAY_ACTIONS, arg);
}

bool command_input(command_t *cmd, const char *arg)
{
   static const struct cmd_choice choices[] = {
      { "set",     "SET_INPUT",     true },
      { "release", "RELEASE_INPUT", true },
      { "show",    "GET_INPUT",     true }
   };
   return command_run_choice(cmd, "INPUT", choices, ARRAY_SIZE(choices),
         COMMAND_INPUT_ACTIONS, arg);
}

bool command_option(command_t *cmd, const char *arg)
{
   static const struct cmd_choice choices[] = {
      { "show", "GET_OPTION", true },
      { "set",  "SET_OPTION", true }
   };
   return command_run_choice(cmd, "OPTION", choices, ARRAY_SIZE(choices),
         COMMAND_OPTION_ACTIONS, arg);
}

bool command_core_option(command_t *cmd, const char *arg)
{
   static const struct cmd_choice choices[] = {
      { "show", "GET_CORE_OPTION", true },
      { "set",  "SET_CORE_OPTION", true }
   };
   return command_run_choice(cmd, "CORE_OPTION", choices,
         ARRAY_SIZE(choices), COMMAND_OPTION_ACTIONS, arg);
}

bool command_interfaces_held(void)
{
   return runloop_is_content_switching();
}

const struct cmd_action_map *command_action_list(size_t *count)
{
   *count = ARRAY_SIZE(action_map);
   return action_map;
}

const struct cmd_map *command_hotkey_list(size_t *count)
{
   *count = ARRAY_SIZE(map);
   return map;
}

#ifdef HAVE_NETWORK_CMD
static bool command_verify(const char *cmd)
{
   size_t i;
   if (command_get_arg(cmd, NULL, NULL))
      return true;

   RARCH_ERR("[NetCMD] Command \"%s\" is not recognized by the program.\n", cmd);
   RARCH_ERR("[NetCMD] \tValid commands:\n");
   for (i = 0; i < ARRAY_SIZE(map); i++)
      RARCH_ERR("[NetCMD] \t\t%s\n", map[i].str);

   for (i = 0; i < ARRAY_SIZE(action_map); i++)
      RARCH_ERR("[NetCMD] \t\t%s %s\n", action_map[i].str, action_map[i].arg_desc);

   return false;
}

static bool udp_send_packet(const char *host, uint16_t port, const char *msg)
{
   char port_buf[6];
   const struct addrinfo *tmp_info;
   struct addrinfo *addr = NULL;
   struct addrinfo hints = {0};
   size_t          len   = strlen(msg);
   bool            ret   = false;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);

   hints.ai_socktype = SOCK_DGRAM;
   hints.ai_flags    = AI_NUMERICSERV;

   if (getaddrinfo_retro(host, port_buf, &hints, &addr))
      return false;
   if (!addr)
      return false;

   /* Send to all possible targets. */
   tmp_info = addr;

   do
   {
      int fd = socket(tmp_info->ai_family,
         tmp_info->ai_socktype, tmp_info->ai_protocol);

      if (fd < 0)
         continue;

      if (sendto(fd, msg, len, 0, tmp_info->ai_addr, tmp_info->ai_addrlen) ==
            (ssize_t)len)
         ret = true;

      socket_close(fd);
   } while ((tmp_info = tmp_info->ai_next));

   freeaddrinfo_retro(addr);

   return ret;
}

/* Whether @s, after a ";", can be a host or a port rather than a STEPS
 * step: no space, no "|" (a list's "|on"), and not "checkpoint". Empty
 * is the default, as in "CMD;;PORT". */
static bool command_network_host_like(const char *s)
{
   return !strchr(s, ' ') && !strchr(s, '|')
      && !string_is_equal(s, "checkpoint");
}

bool command_network_send(const char *cmd_)
{
   char buf[4096];
   char *ptr;
   const char *cmd      = NULL;
   const char *host     = NULL;
   const char *port_str = NULL;
   uint16_t port        = DEFAULT_NETWORK_CMD_PORT;
   size_t len;

   if (!cmd_ || !*cmd_)
      return false;

   len = strlen(cmd_);
   if (len >= sizeof(buf))
      return false;

   if (!network_init())
      return false;

   memcpy(buf, cmd_, len + 1);

   /* ";host;port" ends it, the two read from the end and only where they
    * can be no STEPS step: a list separates its steps with ";" too */
   cmd = buf;
   if ((ptr = strrchr(buf, ';')) && command_network_host_like(ptr + 1))
   {
      char *prev;
      *ptr = '\0';
      host = ptr + 1;
      if ((prev = strrchr(buf, ';')) && command_network_host_like(prev + 1))
      {
         *prev    = '\0';
         port_str = host;
         host     = prev + 1;
      }
   }

   if (!cmd || !*cmd)
      return false;

   if (!host || !*host)
   {
#ifdef _WIN32
      host = "127.0.0.1";
#else
      host = "localhost";
#endif
   }

   if (port_str && *port_str)
   {
      unsigned long val = strtoul(port_str, NULL, 0);
      if (val > 0 && val <= 0xFFFF)
         port = (uint16_t)val;
   }

   RARCH_LOG("[NetCMD] %s: \"%s\" to %s:%hu.\n",
         msg_hash_to_str(MSG_SENDING_COMMAND),
         cmd, host, (unsigned short)port);

   if (!command_verify(cmd))
      return false;

   return udp_send_packet(host, port, cmd);
}
#endif

bool command_show_osd_msg(command_t *cmd, const char* arg)
{
    runloop_msg_queue_push(arg, strlen(arg), 1, 180, false, NULL,
          MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
    return true;
}


/* A whole number and nothing after it but spaces, into @out. "3xyz" is
 * not one: it used to come out as 3, and a save then overwrote that
 * slot on an argument its sender had got wrong. */
static bool command_arg_uint(const char *arg, unsigned *out)
{
   char         *end = NULL;
   unsigned long v;

   while (arg && *arg == ' ')
      arg++;
   if (!arg || *arg < '0' || *arg > '9')
      return false;
   errno = 0;
   v     = strtoul(arg, &end, 10);
   while (*end == ' ')
      end++;
   if (*end)
      return false;
   *out = (unsigned)v;
   /* range-checked before narrowing, so 4294967297 is not slot 1, and
    * where long is 32 bits strtoul clamps it and says so */
   return errno != ERANGE && (unsigned long)*out == v;
}

/* A state or replay slot: past INT_MAX one turns negative, which is the
 * auto slot's file. */
static bool command_arg_slot(const char *arg, unsigned *out)
{
   return command_arg_uint(arg, out) && *out <= INT_MAX;
}

static bool command_load_state_start(struct command_reply *r)
{
   return content_load_state_notify(r->path, false, false,
         command_reply_done, r);
}

/* Serializing re-enters the core, so this must not run from a command's
 * own poll. */
static bool command_save_state_start(struct command_reply *r)
{
   if (!content_save_state_notify(r->path, true, command_reply_done, r))
   {
      /* on the heap: this runs from a task, whose thread may have a
       * small stack */
      const char *what = msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO);
      size_t      len  = strlen(what) + strlen(r->path) + 8;
      char       *msg  = (char*)malloc(len);
      if (msg)
         snprintf(msg, len, "%s \"%s\".", what, r->path);
      command_reply_done(NULL, NULL, r, msg ? msg : what);
      free(msg);
   }
   return true;
}

/* LOAD_STATE_SLOT and SAVE_STATE_SLOT, answered once their task is
 * through. */
static bool command_state_slot(command_t *cmd, const char *arg, bool load)
{
   const char *name             = load ? "LOAD_STATE_SLOT" : "SAVE_STATE_SLOT";
   unsigned int slot            = 0;
   bool savestates_enabled      = core_info_current_supports_savestate();
   struct command_reply *r;

   if (!command_arg_slot(arg, &slot))
   {
      command_reply_error(cmd, name, "a slot number, not %s",
            (arg && *arg) ? arg : "nothing");
      return false;
   }

   if ((r = command_reply_new(name)))
   {
      runloop_get_savestate_path(r->path, sizeof(r->path), slot);
      /* For LOADING, an existing state file outranks metadata and
       * save-capability probes: core_serialize_size() measures whether
       * the core can SAVE right now (0 at e.g. a game's own main menu),
       * which says nothing about whether it can restore. Let the load
       * task and retro_unserialize() arbitrate. */
      if (load && !savestates_enabled)
         savestates_enabled = path_is_valid(r->path);
      if (savestates_enabled)
      {
         snprintf(r->ok, sizeof(r->ok), "%s %u", name, slot);
         r->state_loaded = load;
         r->start        = load ? command_load_state_start
                                : command_save_state_start;
         return command_reply_push(cmd, r);
      }
      free(r);
      command_reply_error(cmd, name,
            load ? "no state in slot %u, and this core cannot restore one"
                 : "this core cannot save a state to slot %u right now",
            slot);
      return false;
   }

   command_reply_error(cmd, name, "out of memory");
   return false;
}

/* The state STATE checkpoint keeps, in memory rather than a slot's
 * file, for STATE rollback; dropped when the content closes. */
static struct
{
   void  *data;
   size_t size;
} command_checkpoint;

void command_checkpoint_clear(void)
{
   free(command_checkpoint.data);
   command_checkpoint.data = NULL;
   command_checkpoint.size = 0;
}

/* Keeps the core's state as the checkpoint. Serializing re-enters the
 * core, so only with the core off the stack. */
static bool command_checkpoint_take(void)
{
   retro_ctx_serialize_info_t info;
   size_t len = core_serialize_size();
   void  *buf = len ? malloc(len) : NULL;
   info.data_const = NULL;
   info.data       = buf;
   info.size       = len;
   if (!buf || !core_serialize(&info))
   {
      free(buf);
      return false;
   }
   free(command_checkpoint.data);
   command_checkpoint.data = buf;
   command_checkpoint.size = len;
   return true;
}

/* Serializing re-enters the core, so this runs from the reply's main
 * thread task, as a save to a slot does. */
static bool command_checkpoint_start(struct command_reply *r)
{
   const char *error = NULL;
   if (r->state_loaded)
   {
      bool ramped;
      if (!command_checkpoint.data)
         error = "no checkpoint to roll back to; STATE checkpoint keeps one";
      /* as a load from a slot is refused: the recording would go on
       * from a state it never saw */
      else if (input_driver_replay_recording())
         error = "a replay is recording, which a rollback would break";
      else
      {
         /* through the loader of states without RetroArch's wrapper,
          * which resets the achievements and stops a replay playing,
          * as a load does */
         ramped = audio_driver_jump_fade_begin();
         if (!content_deserialize_state(command_checkpoint.data,
                  command_checkpoint.size))
            error = "the core could not restore the checkpoint";
         audio_driver_jump_fade_end(ramped);
      }
   }
   else if (!command_checkpoint_take())
      error = "this core cannot save a state right now";
   command_reply_done(NULL, NULL, r, error);
   return true;
}

static bool command_checkpoint_state(command_t *cmd, const char *arg,
      bool rollback)
{
   const char *name = rollback ? "ROLLBACK_STATE" : "CHECKPOINT_STATE";
   struct command_reply *r;
   while (arg && *arg == ' ')
      arg++;
   if (arg && *arg)
   {
      command_reply_error(cmd, name, "nothing after it");
      return false;
   }
   if (!core_info_current_supports_savestate())
   {
      command_reply_error(cmd, name, "this core cannot save a state");
      return false;
   }
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
   {
      command_reply_error(cmd, name,
            "achievements hardcore mode forbids states");
      return false;
   }
#endif
   if (!(r = command_reply_new(name)))
   {
      command_reply_error(cmd, name, "out of memory");
      return false;
   }
   r->state_loaded = rollback;
   r->start        = command_checkpoint_start;
   return command_reply_push(cmd, r);
}

bool command_checkpoint_save(command_t *cmd, const char *arg)
{
   return command_checkpoint_state(cmd, arg, false);
}

bool command_checkpoint_rollback(command_t *cmd, const char *arg)
{
   return command_checkpoint_state(cmd, arg, true);
}

/* RESET [on|off]: from the reply's main thread task, since a reset
 * re-enters the core; with on, paused after it, as LOAD_CONTENT ...
 * paused is, so nothing runs before the first step. */
static bool command_reset_start(struct command_reply *r)
{
   command_event(CMD_EVENT_RESET, NULL);
   command_reply_done(NULL, NULL, r, NULL);
   return true;
}

bool command_reset(command_t *cmd, const char *arg)
{
   struct command_reply *r;
   bool                  pause = false;
   while (arg && *arg == ' ')
      arg++;
   if (arg && *arg)
   {
      if (string_is_equal(arg, "on"))
         pause = true;
      else if (!string_is_equal(arg, "off"))
      {
         command_reply_error(cmd, "RESET", "on or off, not %s", arg);
         return false;
      }
   }
   if (!(runloop_get_flags() & RUNLOOP_FLAG_CORE_RUNNING))
   {
      command_reply_error(cmd, "RESET", "no content is running");
      return false;
   }
   if (!(r = command_reply_new("RESET")))
   {
      command_reply_error(cmd, "RESET", "out of memory");
      return false;
   }
   r->pause = pause;
   r->reset = true;
   r->start = command_reset_start;
   return command_reply_push(cmd, r);
}

bool command_load_state_slot(command_t *cmd, const char *arg)
{
   return command_state_slot(cmd, arg, true);
}

bool command_save_state_slot(command_t *cmd, const char *arg)
{
   return command_state_slot(cmd, arg, false);
}

bool command_play_replay_slot(command_t *cmd, const char *arg)
{
#ifdef HAVE_BSV_MOVIE
   unsigned int slot            = 0;
   struct command_reply *r      = NULL;
   if (!command_arg_slot(arg, &slot))
   {
      command_reply_error(cmd, "PLAY_REPLAY_SLOT", "a slot number, not %s",
            (arg && *arg) ? arg : "nothing");
      return false;
   }
   if (     core_info_current_supports_savestate()
         && (r = command_reply_new("PLAY_REPLAY_SLOT")))
   {
      runloop_get_replay_path(r->path, sizeof(r->path), slot);
      r->state_loaded = true;
      /* answered from the movie task's callback, on a later frame */
      if (movie_start_playback_notify(input_state_get_ptr(), r->path,
               command_reply_done, r))
      {
         command_deferred_take(&r->deferred, cmd);
         return true;
      }
      free(r);
      command_reply_error(cmd, "PLAY_REPLAY_SLOT",
            "no replay in slot %u", slot);
      return false;
   }
   command_reply_error(cmd, "PLAY_REPLAY_SLOT",
         "this core cannot play a replay");
   return false;
#else
   command_reply_error(cmd, "PLAY_REPLAY_SLOT",
         "this build has no replay support");
   return false;
#endif
}

bool command_seek_replay(command_t *cmd, const char *arg)
{
#ifdef HAVE_BSV_MOVIE
   char reply[32];
   char *endptr  = NULL;
   size_t _len;
   int64_t frame = arg ? (int64_t)strtoll(arg, &endptr, 10) : 0;
   input_driver_state_t *input_st = input_state_get_ptr();
   struct command_reply *r;
   /* strtoll always writes a valid pointer, so an empty or non-numeric
    * argument shows up as no characters consumed; anything but spaces
    * after the number means it was not one. */
   if (arg && endptr != arg)
      while (*endptr == ' ')
         endptr++;
   if (!arg || endptr == arg || *endptr)
   {
      command_reply_error(cmd, "SEEK_REPLAY", "a frame number, not %s",
            (arg && *arg) ? arg : "nothing");
      return false;
   }
   if (!(input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_PLAYBACK | BSV_FLAG_MOVIE_RECORDING)))
   {
      command_reply_error(cmd, "SEEK_REPLAY",
            "no replay is playing or recording");
      return false;
   }
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
   {
      command_reply_error(cmd, "SEEK_REPLAY",
            "achievements hardcore mode forbids seeking");
      return false;
   }
#endif
   if (!movie_seek_to_frame(input_st, frame))
   {
      command_reply_error(cmd, "SEEK_REPLAY",
            "the replay could not be seeked to frame %" PRId64, frame);
      return false;
   }
   /* A structured request is answered once the seek has run, on a
    * later frame; a line-based one at once, as it always was. */
   if (cmd->structured && (r = command_reply_new("SEEK_REPLAY")))
   {
      snprintf(r->ok, sizeof(r->ok), "OK %" PRId64,
            input_st->bsv_movie_state.seek_target_frame);
      task_notify_set(&input_st->bsv_movie_op, command_reply_done, r);
      command_deferred_take(&r->deferred, cmd);
      return true;
   }
   _len = (size_t)snprintf(reply, sizeof(reply), "OK %" PRId64 "\n",
         input_st->bsv_movie_state.seek_target_frame);
   cmd->replier(cmd, reply, _len);
   return true;
#else
   command_reply_error(cmd, "SEEK_REPLAY",
         "this build has no replay support");
   return false;
#endif
}

bool command_save_savefiles(command_t *cmd, const char* arg)
{
   /* In the future, this should probably send each saved file path
      to the replier. */
   if (!command_event(CMD_EVENT_SAVE_FILES, NULL))
   {
      command_reply_error(cmd, "SAVE_FILES",
            "nothing to save: no content is loaded, or the core has no "
            "save files");
      return false;
   }
   cmd->replier(cmd, "OK\n", 3);
   return true;
}

bool command_load_savefiles(command_t *cmd, const char* arg)
{
   if (!command_event(CMD_EVENT_LOAD_FILES, NULL))
   {
      command_reply_error(cmd, "LOAD_FILES",
            "nothing to load: no content is loaded, or the core has no "
            "save files");
      return false;
   }
   cmd->replier(cmd, "OK\n", 3);
   return true;
}

/* Largest byte count READ_CORE_RAM / READ_CORE_MEMORY will serve. The
 * reply carries 3 characters per byte and goes out as one UDP datagram,
 * so anything much bigger could not be delivered anyway; the cap also
 * keeps `nbytes * 3` far from wrapping in unsigned int, which a request
 * of ~1431655766 bytes did, allocating a few bytes and writing 4 GiB. */
#define COMMAND_READ_NBYTES_MAX 16384u

/* A system or achievement address in hexadecimal, range-checked before
 * it is narrowed, so 100001001 is refused rather than read as 1001. */
static bool command_parse_address(const char *s, unsigned *address,
      char **end)
{
   unsigned long num;
   while (s && *s == ' ')
      s++;
   if (!s || (unsigned char)*s <= ' ' || *s == '-' || *s == '+')
      return false;
   errno    = 0;
   num      = strtoul(s, end, 16);
   *address = (unsigned)num;
   return *end != s && errno != ERANGE && (unsigned long)*address == num;
}

/* The next of a run of hexadecimal bytes separated by spaces: 1 with
 * it in @out, 0 at the end, -1 at anything that is not a byte. A
 * write checks every byte before it makes the first. */
static int command_next_byte(const char **s, uint8_t *out)
{
   char         *end = NULL;
   unsigned long num;
   while (**s == ' ')
      (*s)++;
   if (!**s)
      return 0;
   if (**s == '-' || **s == '+')
      return -1;
   num = strtoul(*s, &end, 16);
   if (end == *s || num > 0xff || (*end && *end != ' '))
      return -1;
   *s   = end;
   *out = (uint8_t)num;
   return 1;
}

static bool command_bytes_valid(const char *s)
{
   uint8_t b;
   int     r;
   while ((r = command_next_byte(&s, &b)) > 0)
      ;
   return r == 0;
}

/* @n bytes of @data as " XX" apiece; gives back the end of what it
 * wrote. */
static char *command_hex_bytes(char *at, const uint8_t *data, size_t n)
{
   static const char hex[] = "0123456789ABCDEF";
   size_t i;
   for (i = 0; i < n; i++)
   {
      *at++ = ' ';
      *at++ = hex[data[i] >> 4];
      *at++ = hex[data[i] & 0x0f];
   }
   return at;
}

#if defined(HAVE_CHEEVOS)
bool command_read_ram(command_t *cmd, const char *arg)
{
   unsigned long num          = 0;
   unsigned int nbytes        = 0;
   unsigned int addr          = 0;
   char *end                  = NULL;

   if (command_parse_address(arg, &addr, &end) && *end == ' ')
      num             = strtoul(end + 1, NULL, 10);
   nbytes             = num <= COMMAND_READ_NBYTES_MAX ? (unsigned)num : 0;

   if (!nbytes)
   {
      command_reply_error(cmd, "READ_CORE_RAM",
            "an achievement address in hexadecimal, a space, and a byte "
            "count from 1 to %u", COMMAND_READ_NBYTES_MAX);
      return false;
   }
   {
      size_t _len             = 0;
      char *reply_at          = NULL;
      unsigned int avail      = 0;
      const uint8_t *data     = rcheevos_patch_address_avail(addr, &avail);
      unsigned int alloc_size;
      char *reply;

      /* Never read past the end of the region the address lives in. */
      if (data && nbytes > avail)
         nbytes = avail;

      /* We allocate more than needed, saving 20 bytes is not really relevant */
      alloc_size = 40 + nbytes * 3;
      reply      = (char*)malloc(alloc_size);
      
      if (!reply)
      {
         command_reply_error(cmd, "READ_CORE_RAM", "out of memory");
         return false;
      }
      
      reply[0]                = '\0';
      reply_at                = reply + snprintf(
            reply, alloc_size - 1, "READ_CORE_RAM" " %x", addr);

      if (data && nbytes > 0)
      {
         char *end = command_hex_bytes(reply_at, data, nbytes);
         *end++    = '\n';
         _len      = (size_t)(end - reply);
      }
      else
      {
         /* no achievement memory at this address; " -1" is what line
          * clients have always read */
         strlcpy_lit(reply_at, " -1\n", alloc_size - (size_t)(reply_at - reply));
         _len = reply_at + STRLEN_CONST(" -1\n") - reply;
         cmd->error = true;
      }
      cmd->replier(cmd, reply, _len);
      free(reply);
   }
   return !cmd->error;
}

bool command_write_ram(command_t *cmd, const char *arg)
{
   unsigned int addr    = 0;
   unsigned int avail   = 0;
   char *end            = NULL;
   uint8_t *data        = NULL;
   uint8_t more;
   const uint8_t *start;

   if (!command_parse_address(arg, &addr, &end) || !command_bytes_valid(end))
   {
      command_reply_error(cmd, "WRITE_CORE_RAM", "an achievement address "
            "in hexadecimal, then bytes from 00 to ff separated by "
            "spaces");
      return false;
   }
   arg   = end;
   data  = (uint8_t *)rcheevos_patch_address_avail(addr, &avail);
   start = data;

   if (!data)
   {
      command_reply_error(cmd, "WRITE_CORE_RAM",
            "no achievement memory at %x; a game with achievements must "
            "be running", addr);
      return false;
   }

   if (rcheevos_hardcore_active())
   {
      RARCH_LOG("[Command] Achievements hardcore mode disabled by WRITE_CORE_RAM.\n");
      rcheevos_pause_hardcore();
   }

   /* Stop at the end of the region: the payload length is whatever the
    * sender put in the datagram. */
   while (avail && command_next_byte(&arg, data) > 0)
   {
      data++;
      avail--;
   }
   /* read into a byte of its own: @data is past the region here */
   if (command_next_byte(&arg, &more) > 0)
      RARCH_WARN("[Command] WRITE_CORE_RAM at %x reached the end of the "
            "memory region; remainder of the payload ignored.\n", addr);
   {
      char   reply[64];
      size_t _len = (size_t)snprintf(reply, sizeof(reply),
            "WRITE_CORE_RAM %x %u\n", addr, (unsigned)(data - start));
      cmd->replier(cmd, reply, _len);
   }
   return true;
}
#endif

/* ---- Queries for clients that browse and launch content --------- */

/* A reply larger than one UDP datagram cannot go back over the network
 * command port; lists stop short of it and say where to go on. */
#define COMMAND_LIST_REPLY_MAX 60000
#define COMMAND_PLAYLIST_PAGE  200

/* ------------------------------------------------------------------ */
/* Options: what GET_OPTION reports and SET_OPTION writes              */

/* Which option a row of the table below is. Every one has a case in
 * command_option_read(), and no default, so that one added here without
 * a case is a warning rather than an option that reports nothing. */
enum cmd_option_id
{
   /* on or off */
   CMD_OPT_PAUSE = 0,
   CMD_OPT_FAST_FORWARD,
   CMD_OPT_SLOW_MOTION,
   CMD_OPT_MUTE,
   CMD_OPT_FPS,
   CMD_OPT_STATISTICS,
   CMD_OPT_FULLSCREEN,
   CMD_OPT_SHADER,
   CMD_OPT_VIDEO_FILTER,
   CMD_OPT_REWIND,
   CMD_OPT_RUNAHEAD,
   CMD_OPT_PREEMPT,
   CMD_OPT_VRR_RUNLOOP,
   CMD_OPT_TURBO_FIRE,
   CMD_OPT_GRAB_MOUSE,
   CMD_OPT_GAME_FOCUS,
   CMD_OPT_RECORDING,
   CMD_OPT_STREAMING,
   CMD_OPT_CHEEVOS_HARDCORE,
   CMD_OPT_CHEEVOS_ENABLE,
   CMD_OPT_LOADING,
   /* a value they report */
   CMD_OPT_FRAME_COUNT,
   CMD_OPT_STATE_SLOT,
   CMD_OPT_REPLAY_SLOT,
   CMD_OPT_ACTIVE_REPLAY,
   CMD_OPT_SYSTEM,
   CMD_OPT_CONTENT,
   CMD_OPT_DIR_SAVEFILE,
   CMD_OPT_DIR_SAVESTATE,
   CMD_OPT_DIR_RUNTIME_LOG,
   CMD_OPT_DIR_LOG,
   CMD_OPT_DIR_CACHE,
   CMD_OPT_DIR_SYSTEM,
   CMD_OPT_NICKNAME
};

/* An option GET_OPTION reports and SET_OPTION writes.
 *
 * @alias is the name GET_CONFIG_PARAM knew the option by, kept so that
 * interface answers what it always did; NULL where there was none.
 *
 * SET_OPTION is asked for a state, not a change, and writes only when
 * the option is not already in it, so sending the same value twice is
 * one change and then nothing. That is why @event, the single event the
 * hotkey toggles the option with, is enough for most of them; the few
 * the frontend does differently are @settable with no event, and
 * command_option_set() has them by @id. */
struct cmd_option_map
{
   const char        *str;
   const char        *alias;
   enum cmd_option_id id;
   bool               settable;
   enum event_command event;
   const char        *desc;
};

/* Whether @o is on, and its value into @s - @yes or @no for one that is
 * on or off, else what it reports - unless @s is NULL.
 *
 * Every option is read where its state lives rather than from the
 * setting behind it: a setting says what was configured, and this says
 * what is true now. An option a build does not have never reaches here,
 * having no row in the table, but its case is still compiled: what the
 * state would be read from may not exist. */
static bool command_option_read(const struct cmd_option_map *o,
      char *s, size_t len, const char *yes, const char *no)
{
   settings_t *settings = config_get_ptr();
   const char *text     = NULL;
   bool        on       = false;
   char        num[64];

   switch (o->id)
   {
      case CMD_OPT_PAUSE:
         on   = (runloop_get_flags() & RUNLOOP_FLAG_PAUSED) != 0;
         break;
      case CMD_OPT_FAST_FORWARD:
         on   = (runloop_get_flags() & RUNLOOP_FLAG_FASTMOTION) != 0;
         break;
      case CMD_OPT_SLOW_MOTION:
         on   = (runloop_get_flags() & RUNLOOP_FLAG_SLOWMOTION) != 0;
         break;
      case CMD_OPT_MUTE:
         on   = audio_state_get_ptr()->mute_enable;
         break;
      case CMD_OPT_FPS:
         on   = settings->bools.video_fps_show;
         break;
      case CMD_OPT_STATISTICS:
         on   = settings->bools.video_statistics_show;
         break;
      case CMD_OPT_FULLSCREEN:
         on   = settings->bools.video_fullscreen;
         break;
      case CMD_OPT_SHADER:
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
         on   = settings->bools.video_shader_enable;
#endif
         break;
      case CMD_OPT_VIDEO_FILTER:
#ifdef HAVE_VIDEO_FILTER
         on   = settings->bools.video_filter_enable;
#endif
         break;
      case CMD_OPT_REWIND:
#ifdef HAVE_REWIND
         on   = settings->bools.rewind_enable;
#endif
         break;
      case CMD_OPT_RUNAHEAD:
#ifdef HAVE_RUNAHEAD
         on   = settings->bools.run_ahead_enabled;
#endif
         break;
      case CMD_OPT_PREEMPT:
#ifdef HAVE_RUNAHEAD
         on   = settings->bools.preemptive_frames_enable;
#endif
         break;
      case CMD_OPT_VRR_RUNLOOP:
         on   = settings->bools.vrr_runloop_enable;
         break;
      case CMD_OPT_TURBO_FIRE:
         on   = settings->bools.input_turbo_enable;
         break;
      case CMD_OPT_GRAB_MOUSE:
         on   = input_driver_mouse_grabbed();
         break;
      case CMD_OPT_GAME_FOCUS:
         on   = input_driver_game_focus_enabled();
         break;
      /* Both are the recorder running; the stream is the one that has
       * somewhere to send it. */
      case CMD_OPT_RECORDING:
      case CMD_OPT_STREAMING:
         {
            const recording_state_t *rec_st = recording_state_get_ptr();
            on = rec_st && rec_st->enable
               && (rec_st->streaming_enable == (o->id == CMD_OPT_STREAMING));
         }
         break;
      case CMD_OPT_CHEEVOS_HARDCORE:
#ifdef HAVE_CHEEVOS
         on   = rcheevos_hardcore_active();
#endif
         break;
      case CMD_OPT_CHEEVOS_ENABLE:
#ifdef HAVE_CHEEVOS
         on   = settings->bools.cheevos_enable;
#endif
         break;
      case CMD_OPT_LOADING:
         on   = command_interfaces_held();
         break;

      case CMD_OPT_FRAME_COUNT:
         snprintf(num, sizeof(num), "%" PRIu64,
               video_driver_get_frame_count());
         text = num;
         break;
      case CMD_OPT_STATE_SLOT:
         snprintf(num, sizeof(num), "%d", settings->ints.state_slot);
         text = num;
         break;
      case CMD_OPT_REPLAY_SLOT:
         snprintf(num, sizeof(num), "%d", settings->ints.replay_slot);
         text = num;
         break;
      /* the replay's identifier, its state flags and the frame it is
       * on, as GET_CONFIG_PARAM has always given them */
      case CMD_OPT_ACTIVE_REPLAY:
         text = "0 0 0";
#ifdef HAVE_BSV_MOVIE
         {
            input_driver_state_t *input_st = input_state_get_ptr();
            if (input_st->bsv_movie_state_handle)
            {
               bsv_movie_t *movie = input_st->bsv_movie_state_handle;
               snprintf(num, sizeof(num), "%lld %u %lld",
                     (long long)movie->identifier,
                     input_st->bsv_movie_state.flags,
                     (long long)movie->frame_counter);
               text = num;
            }
         }
#endif
         break;
      case CMD_OPT_SYSTEM:
         {
            core_info_t     *core_info  = NULL;
            runloop_state_t *runloop_st = runloop_state_get_ptr();
            core_info_get_current_core(&core_info);
            if (core_info && core_info->system_id)
               text = core_info->system_id;
            else
               text = runloop_st->system.info.library_name;
         }
         break;
      case CMD_OPT_CONTENT:
         text = path_get(RARCH_PATH_BASENAME);
         break;
      case CMD_OPT_DIR_SAVEFILE:
         text = dir_get_ptr(RARCH_DIR_SAVEFILE);
         break;
      case CMD_OPT_DIR_SAVESTATE:
         text = dir_get_ptr(RARCH_DIR_SAVESTATE);
         break;
      case CMD_OPT_DIR_RUNTIME_LOG:
         text = settings->paths.directory_runtime_log;
         break;
      case CMD_OPT_DIR_LOG:
         text = settings->paths.log_dir;
         break;
      case CMD_OPT_DIR_CACHE:
         text = settings->paths.directory_cache;
         break;
      case CMD_OPT_DIR_SYSTEM:
         text = settings->paths.directory_system;
         break;
      case CMD_OPT_NICKNAME:
         text = settings->paths.username;
         break;
   }

   if (s)
      strlcpy(s, text ? text : (on ? yes : no), len);
   return on;
}

/* Puts @o in state @on, which it is not already in. */
static bool command_option_set(const struct cmd_option_map *o,
      command_t *cmd, bool on)
{
   switch (o->id)
   {
      case CMD_OPT_PAUSE:
         command_event(on ? CMD_EVENT_PAUSE : CMD_EVENT_UNPAUSE, NULL);
         break;
      case CMD_OPT_RECORDING:
      case CMD_OPT_STREAMING:
         if (on && recording_state_get_ptr()->enable)
         {
            command_reply_error(cmd, "SET_OPTION",
                  "stop the active recording or stream before starting %s",
                  o->str);
            return false;
         }
         if (o->id == CMD_OPT_STREAMING)
            command_event(CMD_EVENT_STREAMING_TOGGLE, NULL);
         else
            command_event(on ? CMD_EVENT_RECORD_INIT
                             : CMD_EVENT_RECORD_DEINIT, NULL);
         break;
      case CMD_OPT_GAME_FOCUS:
         {
            enum input_game_focus_cmd_type type = on
               ? GAME_FOCUS_CMD_ON : GAME_FOCUS_CMD_OFF;
            command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &type);
         }
         break;
      case CMD_OPT_FAST_FORWARD:
         return runloop_set_fastmotion(on);
      case CMD_OPT_SLOW_MOTION:
         return runloop_set_slowmotion(on);
      default:
         command_event(o->event, NULL);
         break;
   }
   /* An event that cannot act now - pausing with no core running, the
    * video filter likewise - does nothing and reports success all the
    * same, so what it reached is read back. */
   return command_option_read(o, NULL, 0, NULL, NULL) == on;
}

static const struct cmd_option_map option_map[] = {
   { "pause", NULL, CMD_OPT_PAUSE, true, CMD_EVENT_NONE,
      "The content is paused." },
   { "fast_forward", NULL, CMD_OPT_FAST_FORWARD, true, CMD_EVENT_NONE,
      "Fast-forward is running." },
   { "slow_motion", NULL, CMD_OPT_SLOW_MOTION, true, CMD_EVENT_NONE,
      "Slow motion is running." },
   { "mute", NULL, CMD_OPT_MUTE, true, CMD_EVENT_AUDIO_MUTE_TOGGLE,
      "Audio is muted." },
   { "fps", NULL, CMD_OPT_FPS, true, CMD_EVENT_FPS_TOGGLE,
      "The framerate is shown on screen." },
   { "statistics", NULL, CMD_OPT_STATISTICS, true,
      CMD_EVENT_STATISTICS_TOGGLE,
      "The technical statistics are shown on screen." },
   { "fullscreen", "video_fullscreen", CMD_OPT_FULLSCREEN, true,
      CMD_EVENT_FULLSCREEN_TOGGLE, "The window is fullscreen." },
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   { "shader", NULL, CMD_OPT_SHADER, true, CMD_EVENT_SHADER_TOGGLE,
      "The shader preset is applied." },
#endif
#ifdef HAVE_VIDEO_FILTER
   { "video_filter", NULL, CMD_OPT_VIDEO_FILTER, true,
      CMD_VIDEO_FILTER_TOGGLE, "The video filter is applied." },
#endif
#ifdef HAVE_REWIND
   { "rewind", NULL, CMD_OPT_REWIND, true, CMD_EVENT_REWIND_TOGGLE,
      "Rewind is available." },
#endif
#ifdef HAVE_RUNAHEAD
   { "runahead", NULL, CMD_OPT_RUNAHEAD, true, CMD_EVENT_RUNAHEAD_TOGGLE,
      "Run-ahead is on." },
   { "preempt", NULL, CMD_OPT_PREEMPT, true, CMD_EVENT_PREEMPT_TOGGLE,
      "Preemptive frames are on." },
#endif
   { "vrr_runloop", NULL, CMD_OPT_VRR_RUNLOOP, true,
      CMD_EVENT_VRR_RUNLOOP_TOGGLE,
      "The runloop syncs to the exact content framerate." },
   { "turbo_fire", NULL, CMD_OPT_TURBO_FIRE, true,
      CMD_EVENT_TURBO_FIRE_TOGGLE, "Turbo fire is on." },
   { "grab_mouse", NULL, CMD_OPT_GRAB_MOUSE, true,
      CMD_EVENT_GRAB_MOUSE_TOGGLE, "The mouse is grabbed." },
   { "game_focus", NULL, CMD_OPT_GAME_FOCUS, true, CMD_EVENT_NONE,
      "The game has all keyboard input, rather than the hotkeys." },
   { "recording", NULL, CMD_OPT_RECORDING, true, CMD_EVENT_NONE,
      "Video is being recorded to a file." },
   { "streaming", NULL, CMD_OPT_STREAMING, true, CMD_EVENT_NONE,
      "The session is being streamed." },
#ifdef HAVE_CHEEVOS
   { "cheevos_hardcore", NULL, CMD_OPT_CHEEVOS_HARDCORE, true,
      CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE,
      "Achievements hardcore mode is on: it forbids states, rewind and "
      "frame advance." },
   { "cheevos_enable", "cheevos_enable", CMD_OPT_CHEEVOS_ENABLE, false,
      CMD_EVENT_NONE, "Achievements are enabled." },
#endif
   /* Reported, never set. */
   { "loading", NULL, CMD_OPT_LOADING, false, CMD_EVENT_NONE,
      "Content is loading: a command that needs the core waits for it." },
   { "frame_count", NULL, CMD_OPT_FRAME_COUNT, false, CMD_EVENT_NONE,
      "Frames shown since the video driver started." },
   { "state_slot", NULL, CMD_OPT_STATE_SLOT, false, CMD_EVENT_NONE,
      "The save state slot LOAD_STATE and SAVE_STATE use." },
   { "replay_slot", NULL, CMD_OPT_REPLAY_SLOT, false, CMD_EVENT_NONE,
      "The replay slot PLAY_REPLAY and RECORD_REPLAY use." },
   { "active_replay", "active_replay", CMD_OPT_ACTIVE_REPLAY, false,
      CMD_EVENT_NONE,
      "The open replay: its identifier, state flags and frame, or zeroes." },
   { "system", NULL, CMD_OPT_SYSTEM, false, CMD_EVENT_NONE,
      "The running core's system, or empty when none is loaded." },
   { "content", NULL, CMD_OPT_CONTENT, false, CMD_EVENT_NONE,
      "The running content's path, or empty when none is loaded." },
   { "savefile_directory", "savefile_directory", CMD_OPT_DIR_SAVEFILE,
      false, CMD_EVENT_NONE, "Where save files are written." },
   { "savestate_directory", "savestate_directory", CMD_OPT_DIR_SAVESTATE,
      false, CMD_EVENT_NONE, "Where save states are written." },
   { "runtime_log_directory", "runtime_log_directory",
      CMD_OPT_DIR_RUNTIME_LOG, false, CMD_EVENT_NONE,
      "Where runtime logs are written." },
   { "log_dir", "log_dir", CMD_OPT_DIR_LOG, false, CMD_EVENT_NONE,
      "Where the log is written." },
   { "cache_directory", "cache_directory", CMD_OPT_DIR_CACHE, false,
      CMD_EVENT_NONE, "Where extracted content is cached." },
   { "system_directory", "system_directory", CMD_OPT_DIR_SYSTEM, false,
      CMD_EVENT_NONE, "Where cores look for system files." },
   { "netplay_nickname", "netplay_nickname", CMD_OPT_NICKNAME, false,
      CMD_EVENT_NONE, "The name shown to netplay peers." }
};

/* The option @name, by its own name or the one GET_CONFIG_PARAM knows
 * it by when @alias, or NULL. */
static const struct cmd_option_map *command_option_find(const char *name,
      bool alias)
{
   size_t i;
   for (i = 0; i < ARRAY_SIZE(option_map); i++)
   {
      if (string_is_equal(name, option_map[i].str))
         return &option_map[i];
      if (     alias && option_map[i].alias
            && string_is_equal(name, option_map[i].alias))
         return &option_map[i];
   }
   return NULL;
}

/* GET_OPTION [option]
 *
 * Every option and its value, name and value tab separated, one per
 * line; with a name, that option's line alone. */
bool command_get_option(command_t *cmd, const char *arg)
{
   char   value[PATH_MAX_LENGTH];
   size_t i;
   size_t _len  = 0;
   bool   found = false;
   bool   one;
   size_t cap;
   char  *reply;

   while (arg && *arg == ' ')
      arg++;
   one = arg && *arg;
   /* one line needs room for one; the whole table stops short of what a
    * reply can carry, as the other lists do */
   cap = one ? NAME_MAX_LENGTH + PATH_MAX_LENGTH + 2
             : COMMAND_LIST_REPLY_MAX;

   if (!(reply = (char*)malloc(cap + 1)))
   {
      command_reply_error(cmd, "GET_OPTION", "out of memory");
      return false;
   }
   reply[0] = '\0';

   for (i = 0; i < ARRAY_SIZE(option_map); i++)
   {
      int n;
      if (one && !string_is_equal(arg, option_map[i].str))
         continue;
      found = true;
      command_option_read(&option_map[i], value, sizeof(value), "on", "off");
      n = snprintf(reply + _len, cap - _len, "%s\t%s\n",
            option_map[i].str, value);
      /* Short of a reply holding every option, half the list would be
       * read as the whole of it, so say so instead. */
      if (n < 0 || (size_t)n >= cap - _len)
      {
         free(reply);
         command_reply_error(cmd, "GET_OPTION",
               "the options did not fit one reply, stopping at %s",
               option_map[i].str);
         return false;
      }
      _len += (size_t)n;
   }

   if (!found)
   {
      free(reply);
      command_reply_error(cmd, "GET_OPTION",
            "no such option %s; OPTION show lists them", arg);
      return false;
   }

   cmd->replier(cmd, reply, _len);
   free(reply);
   return true;
}

/* SET_OPTION <option> on|off
 *
 * Asking for the state the option is already in changes nothing and
 * succeeds, so the same request twice is one change. */
bool command_set_option(command_t *cmd, const char *arg)
{
   char   reply[NAME_MAX_LENGTH + 32];
   size_t _len;
   const struct cmd_option_map *o;
   const char *sp;
   char   name[NAME_MAX_LENGTH];
   bool   on;

   while (arg && *arg == ' ')
      arg++;
   if (!arg || !*arg || !(sp = strchr(arg, ' ')))
   {
      command_reply_error(cmd, "SET_OPTION",
            "an option and on or off, as in OPTION set pause on");
      return false;
   }
   if ((size_t)(sp - arg) >= sizeof(name))
   {
      command_reply_error(cmd, "SET_OPTION", "no such option");
      return false;
   }
   memcpy(name, arg, (size_t)(sp - arg));
   name[sp - arg] = '\0';
   while (*sp == ' ')
      sp++;

   if (string_is_equal(sp, "on"))
      on = true;
   else if (string_is_equal(sp, "off"))
      on = false;
   else
   {
      command_reply_error(cmd, "SET_OPTION",
            "%s takes on or off, not %s", name, sp);
      return false;
   }

   if (!(o = command_option_find(name, false)))
   {
      command_reply_error(cmd, "SET_OPTION",
            "no such option %s; OPTION show lists them",
            name);
      return false;
   }
   if (!o->settable)
   {
      command_reply_error(cmd, "SET_OPTION", "%s only reports", o->str);
      return false;
   }
   if (     command_option_read(o, NULL, 0, NULL, NULL) != on
         && !command_option_set(o, cmd, on))
   {
      if (!cmd->error)
         command_reply_error(cmd, "SET_OPTION", "%s could not be turned %s",
               o->str, on ? "on" : "off");
      return false;
   }

   /* the shape GET_OPTION gives, so one reading serves both */
   _len = (size_t)snprintf(reply, sizeof(reply), "%s\t%s\n", o->str,
         on ? "on" : "off");
   cmd->replier(cmd, reply, _len);
   return true;
}

#ifdef HAVE_MCP
/* GET_LOG [since]
 *
 * The lines logged since line @since (0 when left out), as many as one
 * reply holds, then NEXT and the number to ask from next time. Lines
 * logged before the first GET_LOG, or the MCP server started, are not
 * kept; SKIPPED and a count says lines were overwritten before they
 * were read. */
bool command_get_log(command_t *cmd, const char *arg)
{
   size_t first, next, _len;
   unsigned since = 0;
   char  *reply;

   while (arg && *arg == ' ')
      arg++;
   if (arg && *arg && !command_arg_uint(arg, &since))
   {
      command_reply_error(cmd, "GET_LOG", "a line number, not %s", arg);
      return false;
   }
   verbosity_ring_arm();
   if (!(reply = (char*)malloc(COMMAND_LIST_REPLY_MAX + 1)))
   {
      command_reply_error(cmd, "GET_LOG", "out of memory");
      return false;
   }
   /* room kept for the SKIPPED and NEXT lines */
   _len = verbosity_ring_read(since, reply + 32, COMMAND_LIST_REPLY_MAX - 64,
         &first, &next);
   if (first > since)
   {
      int n = snprintf(reply, 32, "SKIPPED %u\n", (unsigned)(first - since));
      memmove(reply + n, reply + 32, _len);
      _len += (size_t)n;
   }
   else
      memmove(reply, reply + 32, _len);
   _len += (size_t)snprintf(reply + _len, 32, "NEXT %u\n", (unsigned)next);
   cmd->replier(cmd, reply, _len);
   free(reply);
   return true;
}
#endif

/* Where a system address lands, defined with the memory commands. */
static uint8_t *command_memory_get_pointer(
      const rarch_system_info_t* sys_info,
      unsigned address, unsigned int* max_bytes,
      int for_write, char *s, size_t len);

/* What RUN_UNTIL, a PRESS that waits after it and a STEPS list wait on:
 * the byte at @lo, or with @wide the value @hi * 256 + @lo, from two
 * bytes that need not be adjacent. */
struct command_cond
{
   unsigned hi;
   unsigned lo;
   unsigned mask;   /* "& <mask>": the bits compared; 0 for all */
   unsigned want;
   uint8_t  op;     /* enum cmd_until_op */
   bool     wide;
};

/* "<address>[:<address>]" into @c, @end after it: 0, or 1 where the
 * first address is not one and 2 where the second is not. */
static int command_cond_address(const char *s, struct command_cond *c,
      char **end)
{
   unsigned address = 0;
   if (!command_parse_address(s, &address, end))
      return 1;
   c->lo = address;
   if (**end != ':')
      return 0;
   if (     !command_parse_address(*end + 1, &address, end)
         || (**end && **end != ' '))
      return 2;
   c->hi   = c->lo;
   c->lo   = address;
   c->wide = true;
   return 0;
}

static bool command_cond_read(const struct command_cond *c,
      unsigned *out, char *why, size_t why_len)
{
   unsigned       avail = 0;
   const rarch_system_info_t *sys = &runloop_state_get_ptr()->system;
   const uint8_t *lo    = command_memory_get_pointer(sys, c->lo, &avail, 0,
         why, why_len);
   const uint8_t *hi    = NULL;
   if (!lo || (c->wide && !(hi = command_memory_get_pointer(sys, c->hi,
                  &avail, 0, why, why_len))))
      return false;
   *out = hi ? ((unsigned)*hi << 8) | *lo : *lo;
   if (c->mask)
      *out &= c->mask;
   return true;
}

/* The reason command_memory_get_pointer() wrote, as line clients read
 * it - " -1 ", why, a newline - trimmed to the why. */
static const char *command_memory_why(char *reason)
{
   char *why = !strncmp(reason, " -1 ", STRLEN_CONST(" -1 "))
      ? reason + STRLEN_CONST(" -1 ") : reason;
   char *nl  = strchr(why, '\n');
   if (nl)
      *nl = '\0';
   return *why ? why : "no memory at that address";
}

/* The bytes a client asked to be told about, by the labels it gave
 * them. A step is a request and a look - press, then read where the
 * player is, then read the enemies, then read the game mode - so the
 * look rides along with the step instead of being three more requests.
 *
 * Each is looked up afresh on every reply rather than kept as a
 * pointer: a core may move its memory without closing the content. */
#define COMMAND_WATCH_MAX 16
/* the most one watch's line takes: label, address and value */
#define COMMAND_WATCH_LINE 48

struct command_watch_entry
{
   struct command_cond where; /* its address or two, read as a condition is */
   char                label[16];
};
static struct command_watch_entry command_watches[COMMAND_WATCH_MAX];
static unsigned command_watch_count;

void command_watch_clear(void)
{
   command_watch_count = 0;
}

/* The watches as lines of label, address and value, which is what a
 * line client reads and what the rows of a structured reply are. */
static size_t command_watch_lines(char *s, size_t len)
{
   char      value[8];
   char      where[24];
   char      reason[128];
   size_t    _len = 0;
   unsigned  i;
   for (i = 0; i < command_watch_count && _len + COMMAND_WATCH_LINE < len;
         i++)
   {
      const struct command_watch_entry *e = &command_watches[i];
      unsigned now = 0;
      if (command_cond_read(&e->where, &now, reason, sizeof(reason)))
         snprintf(value, sizeof(value), "%u", now);
      else
         strlcpy(value, "none", sizeof(value));
      if (e->where.wide)
         snprintf(where, sizeof(where), "%x:%x", e->where.hi, e->where.lo);
      else
         snprintf(where, sizeof(where), "%x", e->where.lo);
      _len += (size_t)snprintf(s + _len, len - _len, "%s\t%s\t%s\n",
            e->label, where, value);
   }
   return _len;
}

/* How a condition compares, in the order the words below name them. */
enum cmd_until_op
{
   CMD_UNTIL_EQ = 0,
   CMD_UNTIL_NE,
   CMD_UNTIL_LT,
   CMD_UNTIL_GT,
   CMD_UNTIL_LE,
   CMD_UNTIL_GE
};
#define COMMAND_UNTIL_OPS "==, !=, <, >, <= or >="
static const char *const command_until_ops[] = {
   "==", "!=", "<", ">", "<=", ">="
};

/* A number in a condition: "0x" and hexadecimal digits, or decimal.
 * False where @s starts with neither; @end after it. */
static bool command_cond_number(const char *s, char **end,
      unsigned long *out)
{
   errno = 0;
   if (     s[0] == '0' && (s[1] == 'x' || s[1] == 'X')
         && ((s[2] >= '0' && s[2] <= '9') || (s[2] >= 'a' && s[2] <= 'f')
            || (s[2] >= 'A' && s[2] <= 'F')))
      *out = strtoul(s + 2, end, 16);
   else if (*s >= '0' && *s <= '9' && s[1] != 'x' && s[1] != 'X')
      *out = strtoul(s, end, 10);
   else
      return false;
   return errno != ERANGE;
}

/* "<address>[:<address>] <condition> <value>", @end after the value;
 * false with what was wrong in @why. */
static bool command_cond_parse(const char *s, struct command_cond *c,
      char **end, char *why, size_t why_len)
{
   unsigned      i;
   unsigned long num;
   const char   *sp;
   int           bad;

   memset(c, 0, sizeof(*c));
   if ((bad = command_cond_address(s, c, end)))
   {
      strlcpy(why, bad == 1
            ? "a hexadecimal address, or two as high:low, then a "
              "condition and a value"
            : "a second hexadecimal address after the \":\"", why_len);
      return false;
   }
   while (**end == ' ')
      (*end)++;
   /* "& 0x40" or "& 64": only those bits, as a game keeps its flags */
   if (**end == '&')
   {
      const char *m = *end + 1;
      while (*m == ' ')
         m++;
      if (     !command_cond_number(m, end, &num) || !num
            || num > (c->wide ? 65535UL : 255UL))
      {
         strlcpy(why, "a mask after \"&\", such as 0x40 or 64", why_len);
         return false;
      }
      c->mask = (unsigned)num;
      while (**end == ' ')
         (*end)++;
   }
   if (!**end)
   {
      strlcpy(why, "a condition and a value after the address", why_len);
      return false;
   }
   if (!(sp = strchr(*end, ' ')))
   {
      strlcpy(why, "a condition and a value after the address", why_len);
      return false;
   }
   for (i = 0; i < ARRAY_SIZE(command_until_ops); i++)
      if (     !strncmp(*end, command_until_ops[i], (size_t)(sp - *end))
            && !command_until_ops[i][sp - *end])
         break;
   if (i == ARRAY_SIZE(command_until_ops))
   {
      snprintf(why, why_len, "a condition of %s, not %.*s",
            COMMAND_UNTIL_OPS, (int)(sp - *end), *end);
      return false;
   }
   while (*sp == ' ')
      sp++;
   /* "== 112" or "== 0x70", as a mask is given */
   if (     !command_cond_number(sp, end, &num)
         || num > (c->wide ? 65535UL : 255UL))
   {
      strlcpy(why, c->wide
            ? "a value from 0 to 65535 (0xFFFF), which is what two bytes hold"
            : "a value from 0 to 255 (0xFF), which is what a byte holds",
            why_len);
      return false;
   }
   c->want = (unsigned)num;
   c->op   = (uint8_t)i;
   return true;
}

/* What @c's memory holds now, looked up afresh as WATCH's bytes are,
 * since a core may move its memory while the content runs; false where
 * nothing maps it, with the reason as line clients read it in @why. */
static bool command_cond_met(const struct command_cond *c, unsigned now)
{
   switch (c->op)
   {
      case CMD_UNTIL_EQ: return now == c->want;
      case CMD_UNTIL_NE: return now != c->want;
      case CMD_UNTIL_LT: return now <  c->want;
      case CMD_UNTIL_GT: return now >  c->want;
      case CMD_UNTIL_LE: return now <= c->want;
      case CMD_UNTIL_GE: return now >= c->want;
      default:           break;
   }
   return true;
}

/* Whether @c's memory is mapped now: answered to @cmd as @name's error
 * where it is not, so a wait that could not end is refused before any
 * frame runs. */
static bool command_cond_mapped(command_t *cmd, const char *name,
      const struct command_cond *c)
{
   char     reason[128];
   unsigned now = 0;
   if (command_cond_read(c, &now, reason, sizeof(reason)))
      return true;
   command_reply_error(cmd, name, "%s", command_memory_why(reason));
   return false;
}

static unsigned command_frames_default(void);

/* A frame job is a list of steps run in order, one frame at a time:
 * run some frames, press buttons for some, or run until a condition
 * holds. RUN_FRAMES and PRESS are one step, RUN_UNTIL is one, a PRESS
 * that waits after it is two, and STEPS is whatever the client lists -
 * approach, jump, land - so a stretch of play that took three requests
 * takes one. STOP and CHECKPOINT take no frame: a list's stops go
 * ahead of its steps, so a list queued behind another brings its own. */
enum cmd_step_kind
{
   CMD_STEP_RUN = 0,
   CMD_STEP_PRESS,
   CMD_STEP_UNTIL,
   CMD_STEP_STOP,
   CMD_STEP_CHECKPOINT
};

struct command_step
{
   struct command_cond cond;   /* UNTIL and STOP */
   unsigned            frames; /* RUN and PRESS */
   unsigned            port;   /* PRESS: the player, from 0 */
   uint16_t            mask;   /* PRESS: the buttons, by bit */
   uint8_t             kind;   /* enum cmd_step_kind */
   uint8_t             idx;    /* its number in its list, from 1; a
                                  STOP's among the stops */
};

#define COMMAND_STEPS_MAX 32
#define COMMAND_STOPS_MAX 4
/* steps queued and running, their stops among them; each queued list
 * also has a marker, which does not count */
#define COMMAND_QUEUE_MAX 64

/* How the job answers: the command's own line (RUN_FRAMES, PRESS), how
 * the wait ended and the value (RUN_UNTIL, a PRESS that waits), or how
 * the list ended and on which step (STEPS). */
enum cmd_job_answer
{
   CMD_ANSWER_LINE = 0,
   CMD_ANSWER_UNTIL,
   CMD_ANSWER_STEPS
};

/* The job in progress. One at a time: a second replaces the first,
 * which is answered with an error. A queued one has no request waiting
 * on it: how it stopped or failed is kept for the next queued STEPS. */
static struct
{
   command_deferred_t  deferred;
   struct command_step steps[2 * COMMAND_QUEUE_MAX];
   /* STEPS: end the list where any of them holds - a death, the level's
    * end */
   struct command_cond stops[COMMAND_STOPS_MAX];
   const char         *name;    /* whichever command owes the reply */
   unsigned            count;   /* steps; 0 when no job runs */
   unsigned            cur;     /* the step running */
   unsigned            left;    /* frames left in a RUN or PRESS step */
   unsigned            budget;  /* frames left for the whole job */
   unsigned            ran;     /* frames run so far */
   unsigned            value;   /* what the last condition read held */
   unsigned            port;    /* the player whose buttons are held */
   uint16_t            held;    /* the buttons the job let go of, by bit */
   uint8_t             answer;  /* enum cmd_job_answer */
   unsigned            nstops;
   unsigned            stopped; /* which stop held, from 1 */
   /* a frame with the buttons let go between two presses of the same
    * one, so the game sees two taps rather than one long press */
   bool                gap;
   /* what a press let go of on the frame boundary being crossed, and
    * whose: a press starting on the same boundary waits a frame, in
    * this job or the next request's, until a frame has run */
   uint16_t            let_go;
   unsigned            let_go_port;
   char                reply[256]; /* CMD_ANSWER_LINE: the line */
   bool                queued;
   /* how a queued list stopped, gave up or failed, as one PREVIOUS
    * line, until the next call that runs frames carries it */
   char                prev[192];
   /* the PREVIOUS line this job's answer carries */
   char                carry[192];
   /* queued: when it ran dry, and the content paused waiting for more */
   retro_time_t        dry_since;
} command_run_frames_st;

/* A PRESS step lets go of what it held, on its last frame or on
 * whatever ended it early; what SET_INPUT held stays held. */
static void command_run_frames_release(void)
{
   unsigned i;
   for (i = 0; i < 16; i++)
      if (command_run_frames_st.held & (1u << i))
         input_driver_command_hold(command_run_frames_st.port, i, 0);
   command_run_frames_st.held = 0;
}

/* Ends the job, answering no one. */
static void command_run_frames_finish(void)
{
   runloop_frame_work_set(RUNLOOP_WORK_RUN_FRAMES, false);
   command_run_frames_st.count = 0;
   command_run_frames_st.left  = 0;
   command_run_frames_release();
   /* a frame run without a job ends what the next one would carry */
   runloop_frame_work_set(RUNLOOP_WORK_FREE_FRAME,
         command_run_frames_st.let_go != 0);
}

/* Ends the job a request waits on: @first, the PREVIOUS line it
 * carries, the watches and @last, or where it failed @first, the
 * reason, and that line. */
static void command_run_frames_answer(const char *first, const char *last,
      bool is_error)
{
   /* the step's own line, then a line for each byte watched: room for
    * the longest reply and every watch */
   char   out[256 + sizeof(command_run_frames_st.carry)
      + COMMAND_WATCH_MAX * COMMAND_WATCH_LINE + 32];
   size_t _len = strlcpy(out, first, sizeof(out));

   if (_len >= sizeof(out))
      _len = sizeof(out) - 1;
   command_run_frames_finish();
   /* how the queued list before it ended, after its own first line */
   if (_len < sizeof(out))
      _len += strlcpy(out + _len, command_run_frames_st.carry,
            sizeof(out) - _len);
   command_run_frames_st.carry[0] = '\0';
   if (!is_error && _len < sizeof(out))
   {
      _len += command_watch_lines(out + _len, sizeof(out) - _len);
      if (last && _len < sizeof(out))
         _len += strlcpy(out + _len, last, sizeof(out) - _len);
   }
   if (_len >= sizeof(out))
      _len = sizeof(out) - 1;
   command_deferred_send(&command_run_frames_st.deferred, out, _len,
         is_error);
}

/* The step the job is on, from 1, as its list numbers it: the last
 * where all have run. */
static unsigned command_run_frames_step(void)
{
   return command_run_frames_st.steps[
      command_run_frames_st.cur < command_run_frames_st.count
      ? command_run_frames_st.cur : command_run_frames_st.count - 1].idx;
}

/* @how ("MET", "WAITING", "DONE", "STOPPED") and the frames run, as the
 * job's kind of answer gives it. */
static void command_run_frames_end(const char *how)
{
   char   first[48];
   char   last[48];
   size_t _len;
   /* a queue run dry pauses, with nothing to tell */
   if (command_run_frames_st.queued && string_is_equal(how, "DONE"))
   {
      command_run_frames_finish();
      command_run_frames_st.dry_since = cpu_features_get_time_usec();
      runloop_frame_work_set(RUNLOOP_WORK_FREE_FRAME, true);
      return;
   }
   /* nothing waits on a queued list: how it ended goes to the next call
    * that runs frames */
   if (command_run_frames_st.queued)
   {
      size_t n = (size_t)snprintf(command_run_frames_st.prev,
            sizeof(command_run_frames_st.prev), "PREVIOUS %s %u", how,
            command_run_frames_st.ran);
      if (string_is_equal(how, "STOPPED") && n < sizeof(command_run_frames_st.prev))
         n += (size_t)snprintf(command_run_frames_st.prev + n,
               sizeof(command_run_frames_st.prev) - n, " STOP %u",
               command_run_frames_st.stopped);
      if (n < sizeof(command_run_frames_st.prev))
         snprintf(command_run_frames_st.prev + n,
               sizeof(command_run_frames_st.prev) - n, " STEP %u\n",
               command_run_frames_step());
      command_run_frames_finish();
      return;
   }
   switch (command_run_frames_st.answer)
   {
      case CMD_ANSWER_UNTIL:
         snprintf(first, sizeof(first), "%s %u\n", how,
               command_run_frames_st.ran);
         snprintf(last, sizeof(last), "VALUE %u\n",
               command_run_frames_st.value);
         command_run_frames_answer(first, last, false);
         break;
      case CMD_ANSWER_STEPS:
         snprintf(first, sizeof(first), "%s %u\n", how,
               command_run_frames_st.ran);
         /* which stop held, where one did, then the step it ended on,
          * from 1: the last when all ran */
         _len = 0;
         if (string_is_equal(how, "STOPPED"))
            _len = (size_t)snprintf(last, sizeof(last), "STOP %u\n",
                  command_run_frames_st.stopped);
         snprintf(last + _len, sizeof(last) - _len, "STEP %u\n",
               command_run_frames_step());
         command_run_frames_answer(first, last, false);
         break;
      default:
         command_run_frames_answer(command_run_frames_st.reply, NULL,
               false);
         break;
   }
}

/* Ends the job on a failure, answered as the job answers; a queued
 * one's goes to the next call as a PREVIOUS ERROR line. */
void command_run_frames_cancel(const char *why)
{
   char msg[128];
   if (command_run_frames_st.queued)
   {
      snprintf(command_run_frames_st.prev,
            sizeof(command_run_frames_st.prev), "PREVIOUS ERROR %s\n", why);
      command_run_frames_finish();
      return;
   }
   snprintf(msg, sizeof(msg), "%s ERROR %s",
         command_run_frames_st.name ? command_run_frames_st.name
                                    : "RUN_FRAMES", why);
   command_run_frames_answer(msg, NULL, true);
}

/* A frame has run with no job: a press now is a fresh tap, and the
 * content is no longer sitting idle where a queue ran dry. */
void command_run_frames_free_frame(void)
{
   command_run_frames_st.let_go    = 0;
   command_run_frames_st.dry_since = 0;
   runloop_frame_work_set(RUNLOOP_WORK_FREE_FRAME, false);
}

const char *command_tools_refused(void)
{
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_in_game())
      return "achievements hardcore mode is on; the player can turn it off";
#endif
   return NULL;
}

#ifdef HAVE_THREADS
/* hardcore mode took effect on a task thread, as a game's achievements
 * finish loading: what it stops waits for the main thread's next frame */
static retro_atomic_int_t command_hardcore_due;
#endif

void command_hardcore_entered(void)
{
#ifdef HAVE_THREADS
   if (!task_is_on_main_thread())
   {
      retro_atomic_store_release_int(&command_hardcore_due, 1);
      return;
   }
#endif
   /* a queued job keeps why it ended, told once hardcore mode is off */
   if (runloop_state_get_ptr()->frame_work & RUNLOOP_WORK_RUN_FRAMES)
      command_run_frames_cancel("achievements hardcore mode is on");
   else
      command_run_frames_forget();
   input_driver_command_release(MAX_USERS);
}

void command_frame_start(void)
{
#ifdef HAVE_THREADS
   if (retro_atomic_exchange_int(&command_hardcore_due, 0))
      command_hardcore_entered();
#endif
#ifdef HAVE_MENU
   command_menu_closed();
#endif
}

void command_run_frames_forget(void)
{
   if (command_run_frames_st.prev[0])
      command_queue_ended = true;
   if (     command_run_frames_st.queued
         && (runloop_state_get_ptr()->frame_work & RUNLOOP_WORK_RUN_FRAMES))
   {
      command_run_frames_finish();
      command_queue_ended = true;
   }
   command_run_frames_st.prev[0] = '\0';
   command_run_frames_free_frame();
}

/* A PRESS step holds its buttons; what SET_INPUT already holds stays
 * held after it. */
static void command_run_frames_hold(const struct command_step *s)
{
   unsigned i;
   int16_t  analog[4];
   command_run_frames_st.port = s->port;
   command_run_frames_st.held = s->mask
      & (uint16_t)~input_driver_command_held(s->port, analog);
   for (i = 0; i < 16; i++)
      if (s->mask & (1u << i))
         input_driver_command_hold(s->port, i, 1);
}

/* Starts the step at cur and any after it that need no frame: a PRESS
 * holds its buttons, a condition that already holds is passed, stops
 * take over, and a checkpoint is kept where @safe says the core is off
 * the stack; otherwise it waits for command_run_frames_between().
 * Answers where that ends the job. */
static void command_run_frames_enter(bool safe)
{
   char     why[128];
   while (command_run_frames_st.cur < command_run_frames_st.count)
   {
      struct command_step *s =
         &command_run_frames_st.steps[command_run_frames_st.cur];
      if (s->kind == CMD_STEP_STOP)
      {
         /* stop 0 starts a queued list: none of the last one's stops,
          * and its own count of frames */
         if (!s->idx)
         {
            command_run_frames_st.nstops = 0;
            command_run_frames_st.ran    = 0;
         }
         else
         {
            command_run_frames_st.stops[s->idx - 1] = s->cond;
            command_run_frames_st.nstops            = s->idx;
         }
         command_run_frames_st.cur++;
         continue;
      }
      if (s->kind == CMD_STEP_CHECKPOINT)
      {
         if (!safe)
            return;
         if (!command_checkpoint_take())
         {
            command_run_frames_cancel("the core could not keep a "
                  "checkpoint");
            return;
         }
         command_run_frames_st.cur++;
         continue;
      }
      if (s->kind == CMD_STEP_UNTIL)
      {
         if (!command_cond_read(&s->cond, &command_run_frames_st.value,
                  why, sizeof(why)))
         {
            command_run_frames_cancel("nothing maps the address waited "
                  "on now");
            return;
         }
         if (!command_cond_met(&s->cond, command_run_frames_st.value))
         {
            /* a queued list has no request's limit, so each wait has
             * one of its own */
            command_run_frames_st.left = command_frames_default();
            return;
         }
         command_run_frames_st.cur++;
         continue;
      }
      command_run_frames_st.left = s->frames;
      if (s->kind == CMD_STEP_PRESS)
      {
         /* the same buttons let go on this boundary: a frame let go
          * first, whatever passed in between without one */
         if (     (s->mask & command_run_frames_st.let_go)
               && s->port == command_run_frames_st.let_go_port)
         {
            command_run_frames_st.gap = true;
            /* that frame is not the press's own */
            if (command_run_frames_st.budget)
               command_run_frames_st.budget++;
            return;
         }
         command_run_frames_hold(s);
      }
      return;
   }
   command_run_frames_end(command_run_frames_st.answer == CMD_ANSWER_STEPS
         ? "DONE" : "MET");
}

/* Replaces any job running with @steps, which @cmd asked for as @name,
 * and starts it: a condition that already holds is answered at once,
 * without a frame. @line is the answer of a CMD_ANSWER_LINE job. With
 * no @cmd it is queued: nothing waits on it. */
static void command_run_frames_start(command_t *cmd, const char *name,
      const struct command_step *steps, unsigned count, unsigned budget,
      uint8_t answer, const char *line)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   command_run_frames_st.carry[0] = '\0';
   if (     (runloop_st->frame_work & RUNLOOP_WORK_RUN_FRAMES)
         && command_run_frames_st.queued)
   {
      /* a queue still playing, which this request ends */
      snprintf(command_run_frames_st.carry,
            sizeof(command_run_frames_st.carry),
            "PREVIOUS REPLACED %u STEP %u\n", command_run_frames_st.ran,
            command_run_frames_step());
      command_run_frames_finish();
   }
   else if (runloop_st->frame_work & RUNLOOP_WORK_RUN_FRAMES)
   {
      char why[48];
      snprintf(why, sizeof(why), "replaced by %s%s",
            string_is_equal(name, "RUN_FRAMES") ? "another " : "", name);
      command_run_frames_cancel(why);
   }
   else if (cmd && command_run_frames_st.prev[0])
      /* how the last queued list ended, not yet told */
      strlcpy(command_run_frames_st.carry, command_run_frames_st.prev,
            sizeof(command_run_frames_st.carry));
   command_run_frames_st.prev[0] = '\0';
   memcpy(command_run_frames_st.steps, steps, count * sizeof(*steps));
   command_run_frames_st.nstops     = 0;
   command_run_frames_st.queued     = !cmd;
   command_run_frames_st.dry_since  = 0;
   strlcpy(command_run_frames_st.reply, line ? line : "",
         sizeof(command_run_frames_st.reply));
   command_run_frames_st.name   = name;
   command_run_frames_st.count  = count;
   command_run_frames_st.cur    = 0;
   command_run_frames_st.ran    = 0;
   command_run_frames_st.budget = budget;
   command_run_frames_st.answer = answer;
   command_run_frames_st.held   = 0;
   command_run_frames_st.gap    = false;
   if (cmd)
      command_deferred_take(&command_run_frames_st.deferred, cmd);
   runloop_frame_work_set(RUNLOOP_WORK_RUN_FRAMES, true);
   command_run_frames_enter(false);
}

/* Puts @steps behind those the queued job has left, or starts one with
 * them, which nothing waits on: the content plays on through them and
 * pauses once they have run. Room is the caller's to check. */
static void command_run_frames_queue(const struct command_step *steps,
      unsigned count)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   if (runloop_st->frame_work & RUNLOOP_WORK_RUN_FRAMES)
   {
      /* the steps already run make room */
      unsigned cur = command_run_frames_st.cur;
      memmove(command_run_frames_st.steps, command_run_frames_st.steps + cur,
            (command_run_frames_st.count - cur) * sizeof(*steps));
      command_run_frames_st.count -= cur;
      command_run_frames_st.cur    = 0;
      memcpy(command_run_frames_st.steps + command_run_frames_st.count,
            steps, count * sizeof(*steps));
      command_run_frames_st.count += count;
      return;
   }
   command_run_frames_start(NULL, "STEPS", steps, count, 0,
         CMD_ANSWER_STEPS, NULL);
}

void command_run_frames_between(void)
{
   if (     command_run_frames_st.cur < command_run_frames_st.count
         && command_run_frames_st.steps[command_run_frames_st.cur].kind
            == CMD_STEP_CHECKPOINT)
      command_run_frames_enter(true);
}

/* A frame has run: the step it belonged to goes on or ends, and the
 * reply goes once the last has, the list is stopped, or the frames
 * allowed have all run. */
void command_run_frames_tick(void)
{
   char                 why[128];
   unsigned             now = 0;
   unsigned             i;
   struct command_step *s;

   if (command_run_frames_st.cur >= command_run_frames_st.count)
      return;
   /* a frame has crossed the boundary a press let go on */
   command_run_frames_st.let_go = 0;
   command_run_frames_st.ran++;
   if (command_run_frames_st.budget)
      command_run_frames_st.budget--;
   for (i = 0; i < command_run_frames_st.nstops; i++)
   {
      if (!command_cond_read(&command_run_frames_st.stops[i], &now, why,
               sizeof(why)))
      {
         command_run_frames_cancel("nothing maps the stop address now");
         return;
      }
      if (command_cond_met(&command_run_frames_st.stops[i], now))
      {
         command_run_frames_st.stopped = i + 1;
         command_run_frames_end("STOPPED");
         return;
      }
   }
   s = &command_run_frames_st.steps[command_run_frames_st.cur];
   if (command_run_frames_st.gap)
   {
      /* that was the frame let go: the press starts now */
      command_run_frames_st.gap = false;
      command_run_frames_hold(s);
   }
   else if (s->kind == CMD_STEP_CHECKPOINT)
      /* a job started while that frame ran */
      command_run_frames_enter(true);
   else if (s->kind == CMD_STEP_UNTIL)
   {
      if (!command_cond_read(&s->cond, &command_run_frames_st.value, why,
               sizeof(why)))
      {
         command_run_frames_cancel("nothing maps the address waited on "
               "now");
         return;
      }
      if (command_cond_met(&s->cond, command_run_frames_st.value))
      {
         command_run_frames_st.cur++;
         command_run_frames_enter(true);
      }
      else if (command_run_frames_st.queued && !--command_run_frames_st.left)
      {
         command_run_frames_end("WAITING");
         return;
      }
   }
   else if (!--command_run_frames_st.left)
   {
      /* only what it let go: a button SET_INPUT holds stays down, and
       * a press of it needs no frame between */
      if (s->kind == CMD_STEP_PRESS)
      {
         command_run_frames_st.let_go      = command_run_frames_st.held;
         command_run_frames_st.let_go_port = s->port;
      }
      command_run_frames_release();
      command_run_frames_st.cur++;
      command_run_frames_enter(true);
   }
   /* the frames allowed have run with a step still going: how far it
    * got, so the next request carries on from there */
   if (     command_run_frames_st.count && !command_run_frames_st.budget
         && !command_run_frames_st.queued)
      command_run_frames_end("WAITING");
}

/* What a command needs before it can run a frame of content: content
 * to run, no menu over it, and achievements not in hardcore mode.
 * False with the reason answered to @cmd. */
static bool command_frames_ready(command_t *cmd, const char *name)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
#ifdef HAVE_MENU
   struct menu_state *menu_st  = menu_state_get_ptr();
#endif

   if (!(runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING))
   {
      command_reply_error(cmd, name, "no content is running");
      return false;
   }
#ifdef HAVE_MENU
   if (menu_st && (menu_st->flags & MENU_ST_FLAG_ALIVE))
   {
      command_reply_error(cmd, name, "the menu is open");
      return false;
   }
#endif
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
   {
      command_reply_error(cmd, name,
            "achievements hardcore mode forbids frame advance");
      return false;
   }
#endif
   return true;
}

/* About nine seconds of the content's own frames, at most
 * COMMAND_FRAMES_REPLY_MAX: a structured reply comes inside its
 * ten-second deadline at any frame rate, and a wait runs this long when
 * not told. */
static unsigned command_frames_default(void)
{
   double fps = video_state_get_ptr()->av_info.timing.fps;
   if (fps > 0.0 && fps * 9.0 < (double)COMMAND_FRAMES_REPLY_MAX)
      return fps * 9.0 >= 1.0 ? (unsigned)(fps * 9.0) : 1;
   return COMMAND_FRAMES_REPLY_MAX;
}

/* The most frames one request runs for @cmd */
static unsigned command_frames_reply_max(const command_t *cmd)
{
   return cmd->structured ? command_frames_default()
                          : COMMAND_FRAMES_REPLY_MAX;
}

/* RUN_FRAMES <count>
 *
 * Pauses, if the content is running, and runs it @count frames,
 * answered once the last has run; it stays paused. Another RUN_FRAMES
 * replaces one still running, which is answered with an error. */
bool command_run_frames(command_t *cmd, const char *arg)
{
   char                line[48];
   struct command_step step;
   unsigned            n   = 0;
   /* over MCP no more than the reply is waited for, refused now rather
    * than failed with the frames already running */
   unsigned            max = cmd->structured
      ? command_frames_reply_max(cmd) : COMMAND_RUN_FRAMES_MAX;

   if (!command_arg_uint(arg, &n) || n < 1 || n > max)
   {
      command_reply_error(cmd, "RUN_FRAMES", "a frame count from 1 to %u%s",
            max, cmd->structured ? "; ask again to run more" : "");
      return false;
   }
   if (!command_frames_ready(cmd, "RUN_FRAMES"))
      return false;

   memset(&step, 0, sizeof(step));
   step.kind   = CMD_STEP_RUN;
   step.frames = n;
   snprintf(line, sizeof(line), "RUN_FRAMES %u\n", n);
   command_run_frames_start(cmd, "RUN_FRAMES", &step, 1, n,
         CMD_ANSWER_LINE, line);
   return true;
}

/* RetroPad controls by the names SET_INPUT takes them by: the sixteen
 * buttons in RETRO_DEVICE_ID_JOYPAD order, then the four stick axes. */
static const char *const command_input_names[] = {
   "b", "y", "select", "start", "up", "down", "left", "right",
   "a", "x", "l", "r", "l2", "r2", "l3", "r3",
   "left_x", "left_y", "right_x", "right_y"
};

/* @port's held controls, the shape SET_INPUT and RELEASE_INPUT answer
 * with: the player, then the buttons and the axes held, tab separated,
 * "none" for either when there are none. */
static size_t command_input_line(unsigned port, char *s, size_t len)
{
   unsigned i;
   int16_t  analog[4];
   uint16_t buttons = input_driver_command_held(port, analog);
   size_t   _len  = (size_t)snprintf(s, len, "%u\t", port + 1);
   bool     any   = false;

   for (i = 0; i < 16 && _len < len; i++)
      if (buttons & (1u << i))
      {
         _len += (size_t)snprintf(s + _len, len - _len, "%s%s",
               any ? " " : "", command_input_names[i]);
         any   = true;
      }
   if (!any && _len < len)
      _len += strlcpy(s + _len, "none", len - _len);
   if (_len < len)
      s[_len++] = '\t';
   any = false;
   for (i = 0; i < 4 && _len < len; i++)
      if (analog[i])
      {
         _len += (size_t)snprintf(s + _len, len - _len, "%s%s=%d",
               any ? " " : "", command_input_names[16 + i],
               (int)analog[i]);
         any   = true;
      }
   if (!any && _len < len)
      _len += strlcpy(s + _len, "none", len - _len);
   if (_len + 1 >= len)
      _len = len - 2;
   s[_len++] = '\n';
   s[_len]   = '\0';
   return _len;
}

/* SET_INPUT <player> <control> <value>
 *
 * Holds a RetroPad button (1, or 0 to let it go) or a stick axis
 * (-32768 to 32767, 0 to let it go) for the core, over the player's own
 * input, until it is set again or RELEASE_INPUT lets it go.
 *
 * Held as the game is to see it: SOCD cleaning and D-Pad rotation are
 * for a person's hands on a pad and are not applied. Under netplay the
 * player is this machine's local device, sampled with its input and
 * sent to the others, so every peer runs the same frames. */
/* "<player> <control>[+<control>...] <number>", the request SET_INPUT
 * and PRESS both take: the player in @player, the controls in @ids and
 * how many of them in @count, and the number in @number. False with the
 * reason answered to @cmd, naming @what - what that command's own
 * number is - and @name.
 *
 * Several controls at once, joined by "+", so a combination a game
 * wants together - right+a to run and jump - is one request and one
 * frame rather than two of each. Every one is resolved before the
 * caller acts on any, so a combination with something wrong in it does
 * nothing at all. */
static bool command_input_args(command_t *cmd, const char *name,
      const char *what, const char *arg, unsigned *player,
      unsigned *ids, unsigned *count, long *number)
{
   char   control[160];
   unsigned i;
   char  *end                     = NULL;
   char  *tok;
   const char *sp;
   unsigned long v                = 0;

   *count = 0;
   while (arg && *arg == ' ')
      arg++;
   /* range-checked before narrowing: 4294967297 is not player 1 */
   if (arg && *arg >= '0' && *arg <= '9')
      v = strtoul(arg, &end, 10);
   *player = v <= MAX_USERS ? (unsigned)v : 0;
   if (!end || end == arg || *end != ' ' || *player < 1)
   {
      command_reply_error(cmd, name,
            "a player from 1 to %u, a control and %s",
            (unsigned)MAX_USERS, what);
      return false;
   }
   while (*end == ' ')
      end++;
   if (!(sp = strchr(end, ' ')) || (size_t)(sp - end) >= sizeof(control))
   {
      command_reply_error(cmd, name, "a control and %s", what);
      return false;
   }
   memcpy(control, end, (size_t)(sp - end));
   control[sp - end] = '\0';
   *number = strtol(sp + 1, &end, 10);
   if (end == sp + 1 || *end)
   {
      command_reply_error(cmd, name, "%s after the control", what);
      return false;
   }
   for (tok = control; *tok; )
   {
      char *plus = strchr(tok, '+');
      if (plus)
         *plus = '\0';
      if (!*tok)
      {
         /* "a++b", and a "+" at either end: nothing between them */
         tok = plus + 1;
         continue;
      }
      for (i = 0; i < ARRAY_SIZE(command_input_names); i++)
         if (string_is_equal(tok, command_input_names[i]))
            break;
      if (i == ARRAY_SIZE(command_input_names))
      {
         command_reply_error(cmd, name, "no control named %s", tok);
         return false;
      }
      /* a control named over and over would run past the array */
      if (*count >= ARRAY_SIZE(command_input_names))
      {
         command_reply_error(cmd, name, "at most %u controls at once",
               (unsigned)ARRAY_SIZE(command_input_names));
         return false;
      }
      ids[(*count)++] = i;
      if (!plus)
         break;
      tok = plus + 1;
   }
   if (!*count)
   {
      command_reply_error(cmd, name, "a control, or several "
            "joined by \"+\"");
      return false;
   }
   return true;
}

bool command_set_input(command_t *cmd, const char *arg)
{
   char   reply[192];
   unsigned ids[ARRAY_SIZE(command_input_names)];
   unsigned count                 = 0;
   unsigned player;
   unsigned i;
   long   value;

   if (!command_input_args(cmd, "SET_INPUT", "a value", arg, &player,
            ids, &count, &value))
      return false;
   for (i = 0; i < count; i++)
   {
      unsigned id = ids[i];
      if (     (id <  16 && value != 0 && value != 1)
            || (id >= 16 && (value < -32768 || value > 32767)))
      {
         command_reply_error(cmd, "SET_INPUT", id < 16
               ? "%s takes 1 to hold it or 0 to let it go"
               : "%s takes -32768 to 32767", command_input_names[id]);
         return false;
      }
   }
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
   {
      command_reply_error(cmd, "SET_INPUT",
            "achievements hardcore mode forbids input from a command");
      return false;
   }
#endif

   for (i = 0; i < count; i++)
      input_driver_command_hold(player - 1, ids[i], (int16_t)value);

   cmd->replier(cmd, reply,
         command_input_line(player - 1, reply, sizeof(reply)));
   return true;
}


/* WATCH [<label> <address|off>]...
 *
 * With nothing, what is watched now. Each label and address watches the
 * byte there under that label, carried by RUN_FRAMES, PRESS and
 * RUN_UNTIL; a label and "off" stops watching it. Every pair is taken
 * or none is, so a refusal leaves the list as it was.
 *
 * Answers with the list either way, so a client sees what it has. */
bool command_watch(command_t *cmd, const char *arg)
{
   struct command_watch_entry next[COMMAND_WATCH_MAX];
   char      reply[64 + COMMAND_WATCH_MAX * COMMAND_WATCH_LINE];
   char      label[sizeof(next[0].label)];
   char      reason[128];
   size_t    _len  = 0;
   size_t    n;
   unsigned  count = command_watch_count;
   unsigned  i;
   const char *sp;

   memcpy(next, command_watches, sizeof(next));
   while (arg && *arg)
   {
      while (*arg == ' ')
         arg++;
      if (!*arg)
         break;
      /* a tab or a newline would split the label's row */
      for (sp = arg; (unsigned char)*sp > ' '; sp++)
         ;
      if ((*sp && *sp != ' ') || (size_t)(sp - arg) >= sizeof(label))
      {
         command_reply_error(cmd, "WATCH", "a label of up to %u "
               "characters, then an address or \"off\"",
               (unsigned)(sizeof(label) - 1));
         return false;
      }
      memcpy(label, arg, (size_t)(sp - arg));
      label[sp - arg] = '\0';
      while (*sp == ' ')
         sp++;
      if (!*sp)
      {
         command_reply_error(cmd, "WATCH", "%s needs an address or "
               "\"off\" after it", label);
         return false;
      }
      for (arg = sp; (unsigned char)*arg > ' '; arg++)
         ;
      n = (size_t)(arg - sp);
      /* a label already watched keeps its place in the list, so the
       * rows a client reads stay where they were */
      for (i = 0; i < count; i++)
         if (string_is_equal(next[i].label, label))
            break;
      if (n == STRLEN_CONST("off") && !strncmp(sp, "off", n))
      {
         if (i < count)
         {
            memmove(&next[i], &next[i + 1],
                  (count - i - 1) * sizeof(*next));
            count--;
         }
      }
      else
      {
         struct command_cond where;
         unsigned  now = 0;
         char     *end = NULL;
         memset(&where, 0, sizeof(where));
         /* high:low, two bytes as one value, as conditions take them */
         if (     command_cond_address(sp, &where, &end) || end != arg
               || (*arg && *arg != ' '))
         {
            command_reply_error(cmd, "WATCH", "a hexadecimal address, or "
                  "\"off\" to stop watching %s", label);
            return false;
         }
         /* only a label that is not watched yet needs room */
         if (i == count && count >= COMMAND_WATCH_MAX)
         {
            command_reply_error(cmd, "WATCH", "at most %u bytes watched "
                  "at once", (unsigned)COMMAND_WATCH_MAX);
            return false;
         }
         /* found now, as RUN_UNTIL finds the byte it waits on, so an
          * address nothing maps is refused rather than read later */
         if (!command_cond_read(&where, &now, reason, sizeof(reason)))
         {
            command_reply_error(cmd, "WATCH", "%s: %s", label,
                  command_memory_why(reason));
            return false;
         }
         if (i == count)
            count++;
         next[i].where = where;
         strlcpy(next[i].label, label, sizeof(next[i].label));
      }
   }
   memcpy(command_watches, next, sizeof(next));
   command_watch_count = count;

   _len = command_watch_lines(reply, sizeof(reply));
   if (!_len)
      _len = strlcpy_lit(reply, "WATCH none\n", sizeof(reply));
   cmd->replier(cmd, reply, _len);
   return true;
}

/* RUN_UNTIL <address> <condition> <value> [max frames]
 *
 * Runs the content until the byte at a system address meets what was
 * asked, then answers with how many frames that took and the value it
 * found. Where a client would otherwise run a few frames, read the
 * byte, and go round again - which is where a field run spent most of
 * its calls - this is one request.
 *
 * It waits no longer than a reply is waited for. At the cap it answers
 * WAITING with the frames it ran, so the next request carries on from
 * there rather than the client being told it failed. */
bool command_run_until(command_t *cmd, const char *arg)
{
   char                why[160];
   struct command_step step;
   unsigned            frames = 0;
   char               *end    = NULL;
   unsigned long       num;

   memset(&step, 0, sizeof(step));
   if (!command_cond_parse(arg, &step.cond, &end, why, sizeof(why)))
   {
      command_reply_error(cmd, "RUN_UNTIL", "%s", why);
      return false;
   }
   if (*end == ' ')
   {
      while (*end == ' ')
         end++;
      if (*end)
      {
         num = strtoul(end, &end, 10);
         if (num < 1 || num > command_frames_reply_max(cmd))
         {
            command_reply_error(cmd, "RUN_UNTIL", "at most %u frames "
                  "to wait, which is as long as a reply is waited for",
                  command_frames_reply_max(cmd));
            return false;
         }
         frames = (unsigned)num;
      }
   }
   if (*end)
   {
      command_reply_error(cmd, "RUN_UNTIL", "nothing after the frames "
            "to wait");
      return false;
   }
   if (!command_frames_ready(cmd, "RUN_UNTIL"))
      return false;
   /* the memory is found before any frame runs, so an address nothing
    * maps is answered now rather than after a wait that could not have
    * ended */
   if (!command_cond_mapped(cmd, "RUN_UNTIL", &step.cond))
      return false;
   step.kind = CMD_STEP_UNTIL;
   command_run_frames_start(cmd, "RUN_UNTIL", &step, 1,
         frames ? frames : command_frames_default(), CMD_ANSWER_UNTIL,
         NULL);
   return true;
}

/* PRESS <player> <control>[+<control>...] <frames>
 *
 * Holds the buttons, runs that many frames and lets them go, answered
 * once the last of them has run. One request where a tap took four:
 * SET_INPUT, RUN_FRAMES, SET_INPUT to release and RUN_FRAMES again.
 * Axes are SET_INPUT's: they are held at a value, not pressed. */
bool command_press(command_t *cmd, const char *arg)
{
   char     line[256];
   char     reply[256];
   char     why[160];
   unsigned ids[ARRAY_SIZE(command_input_names)];
   unsigned count              = 0;
   unsigned player;
   unsigned i;
   unsigned budget;
   size_t   _len;
   long     frames;
   char    *until;
   struct command_step steps[2];

   if (strlcpy(line, arg ? arg : "", sizeof(line)) >= sizeof(line))
   {
      command_reply_error(cmd, "PRESS", "at most %u characters",
            (unsigned)sizeof(line) - 1);
      return false;
   }
   memset(steps, 0, sizeof(steps));
   /* "<frames> until <condition>": run on after the press until the
    * condition holds - jump, then land - in the same request */
   if ((until = strstr(line, " until ")))
   {
      char *end = NULL;
      *until    = '\0';
      until    += STRLEN_CONST(" until ");
      if (!command_cond_parse(until, &steps[1].cond, &end, why,
               sizeof(why)))
      {
         command_reply_error(cmd, "PRESS", "%s after until", why);
         return false;
      }
      while (*end == ' ')
         end++;
      if (*end)
      {
         command_reply_error(cmd, "PRESS", "nothing after the condition");
         return false;
      }
   }
   if (!command_input_args(cmd, "PRESS", "a frame count", line, &player,
            ids, &count, &frames))
      return false;
   if (frames < 1 || frames > (long)command_frames_reply_max(cmd))
   {
      command_reply_error(cmd, "PRESS", "a frame count from 1 to %u; a "
            "longer hold is INPUT set with RUN_FRAMES",
            command_frames_reply_max(cmd));
      return false;
   }
   for (i = 0; i < count; i++)
      if (ids[i] >= 16)
      {
         command_reply_error(cmd, "PRESS",
               "%s is an axis, which INPUT set holds at a value",
               command_input_names[ids[i]]);
         return false;
      }
   if (!command_frames_ready(cmd, "PRESS"))
      return false;
   if (until && !command_cond_mapped(cmd, "PRESS", &steps[1].cond))
      return false;

   steps[0].kind   = CMD_STEP_PRESS;
   steps[0].frames = (unsigned)frames;
   steps[0].port   = player - 1;
   for (i = 0; i < count; i++)
      steps[0].mask |= (uint16_t)(1u << ids[i]);
   steps[1].kind   = CMD_STEP_UNTIL;
   /* the controls as they were resolved, so "a++b" comes back "a+b" */
   _len = (size_t)snprintf(reply, sizeof(reply), "PRESS %u ", player);
   for (i = 0; i < count && _len < sizeof(reply); i++)
      _len += (size_t)snprintf(reply + _len, sizeof(reply) - _len, "%s%s",
            i ? "+" : "", command_input_names[ids[i]]);
   if (_len < sizeof(reply))
      snprintf(reply + _len, sizeof(reply) - _len, " %u\n",
            (unsigned)frames);
   /* the wait gets what a wait gets, and the press at least its own */
   budget = command_frames_default();
   if (budget < (unsigned)frames)
      budget = (unsigned)frames;
   command_run_frames_start(cmd, "PRESS", steps, until ? 2 : 1,
         until ? budget : (unsigned)frames,
         until ? CMD_ANSWER_UNTIL : CMD_ANSWER_LINE, reply);
   return true;
}

/* How long, in milliseconds, the content has sat paused since a queue
 * ran dry; -1 while it runs, or when it stopped for another reason. */
static long command_steps_idle(void)
{
   if (     !command_run_frames_st.dry_since
         || (runloop_state_get_ptr()->frame_work & RUNLOOP_WORK_RUN_FRAMES))
      return -1;
   return (long)((cpu_features_get_time_usec()
            - command_run_frames_st.dry_since) / 1000);
}

/* The answer of a queued STEPS: how many steps are left to run, or,
 * where the last queued list stopped, gave up or failed, how many of
 * this call's @discarded steps were dropped rather than played on from
 * that, and its PREVIOUS line; what WATCH watches; and how long the
 * content sat idle, run dry, before @idle says these came. */
static void command_steps_report(command_t *cmd, long idle,
      unsigned discarded)
{
   char     out[64 + sizeof(command_run_frames_st.prev)
      + COMMAND_WATCH_MAX * COMMAND_WATCH_LINE];
   size_t   _len;
   unsigned i;
   unsigned left = 0;
   if (command_run_frames_st.prev[0])
   {
      _len = discarded
         ? (size_t)snprintf(out, sizeof(out), "DISCARDED %u\n", discarded)
         : strlcpy(out, "QUEUED 0\n", sizeof(out));
      _len += strlcpy(out + _len, command_run_frames_st.prev,
            sizeof(out) - _len);
      command_run_frames_st.prev[0] = '\0';
      idle = -1;
   }
   else
   {
      for (i = command_run_frames_st.cur; i < command_run_frames_st.count;
            i++)
         if (command_run_frames_st.steps[i].kind != CMD_STEP_STOP)
            left++;
      _len = (size_t)snprintf(out, sizeof(out), "QUEUED %u\n", left);
   }
   if (_len < sizeof(out))
      _len += command_watch_lines(out + _len, sizeof(out) - _len);
   if (idle >= 0 && _len < sizeof(out))
      _len += (size_t)snprintf(out + _len, sizeof(out) - _len,
            "IDLE %ld\n", idle);
   if (_len >= sizeof(out))
      _len = sizeof(out) - 1;
   cmd->replier(cmd, out, _len);
}

/* STEPS <step>[; <step>...][|on]
 *
 * Runs a stretch of play as one request: "run <frames>", "press
 * <player> <controls> <frames>" and "until <condition>", in order, then
 * answers with how it ended and what WATCH watches. A "stop
 * <condition>" entry ends the list after any frame where it holds, so a
 * death does not play out the rest, and "checkpoint" keeps the state
 * STATE rollback returns to. The whole list runs no longer than a wait
 * does; past that it answers WAITING and the step it was on, and the
 * next request carries on from there.
 *
 * With on, it answers at once and the steps go behind those still
 * queued, so the content plays on while the next stretch is planned,
 * and pauses once they have all run. A stop or a failure since the last
 * queued STEPS is answered instead, and its steps are dropped; with no
 * steps it only asks. */
bool command_steps(command_t *cmd, const char *arg)
{
   char                list[1024];
   char                why[160];
   /* the stops go ahead of the steps, in the slots left before them,
    * and a queued list's marker ahead of those */
   struct command_step steps[1 + COMMAND_STOPS_MAX + COMMAND_STEPS_MAX];
   struct command_step *first = steps + 1 + COMMAND_STOPS_MAX;
   struct command_cond stops[COMMAND_STOPS_MAX];
   unsigned            ids[ARRAY_SIZE(command_input_names)];
   unsigned            count    = 0;
   unsigned            frames   = 0;
   unsigned            fixed    = 0;
   unsigned            budget;
   unsigned            i;
   unsigned            nstops   = 0;
   long                idle;
   bool                queue    = false;
   char               *entry;
   char               *next;
   char               *bar;

   if (!arg || strlcpy(list, arg, sizeof(list)) >= sizeof(list))
   {
      command_reply_error(cmd, "STEPS", "steps separated by \";\", in at "
            "most %u characters", (unsigned)sizeof(list) - 1);
      return false;
   }
   if ((bar = strrchr(list, '|')))
   {
      char *v = bar + 1;
      *bar    = '\0';
      while (*v == ' ')
         v++;
      if (string_is_equal(v, "on"))
         queue = true;
      else if (!string_is_equal(v, "off"))
      {
         command_reply_error(cmd, "STEPS", "on or off after the \"|\"");
         return false;
      }
   }
   memset(steps, 0, sizeof(steps));
   memset(stops, 0, sizeof(stops));
   budget = command_frames_default();
   for (entry = list; entry; entry = next)
   {
      struct command_step *s;
      struct command_cond *c   = NULL;
      char                *end = NULL;
      size_t               n;
      if ((next = strchr(entry, ';')))
         *next++ = '\0';
      while (*entry == ' ')
         entry++;
      for (n = strlen(entry); n && entry[n - 1] == ' '; n--)
         entry[n - 1] = '\0';
      if (!*entry)
         continue;
      /* a queued list is not one request's wait: only each step is */
      if (queue)
         fixed = 0;
      if (!strncmp(entry, "stop ", 5))
      {
         if (nstops >= COMMAND_STOPS_MAX)
         {
            command_reply_error(cmd, "STEPS", "at most %u stops",
                  (unsigned)COMMAND_STOPS_MAX);
            return false;
         }
         c      = &stops[nstops++];
         entry += 5;
      }
      else if (count >= COMMAND_STEPS_MAX)
      {
         command_reply_error(cmd, "STEPS", "at most %u steps",
               (unsigned)COMMAND_STEPS_MAX);
         return false;
      }
      else
      {
         s      = &first[count++];
         s->idx = (uint8_t)count;
         if (string_is_equal(entry, "checkpoint"))
         {
            if (!core_info_current_supports_savestate())
            {
               command_reply_error(cmd, "STEPS", "step %u: this core "
                     "cannot save a state", count);
               return false;
            }
            s->kind = CMD_STEP_CHECKPOINT;
         }
         else if (!strncmp(entry, "until ", 6))
         {
            s->kind = CMD_STEP_UNTIL;
            c       = &s->cond;
            entry  += 6;
            frames++;
         }
         else if (!strncmp(entry, "run ", 4))
         {
            s->kind = CMD_STEP_RUN;
            if (     !command_arg_uint(entry + 4, &s->frames)
                  || s->frames < 1 || s->frames > budget - fixed)
            {
               command_reply_error(cmd, "STEPS", "step %u: run a frame "
                     "count from 1, within the %u frames one request runs",
                     count, budget);
               return false;
            }
            fixed += s->frames;
            frames++;
         }
         else if (!strncmp(entry, "press ", 6))
         {
            const struct command_step *prev = s;
            unsigned player;
            unsigned nids;
            long     held;
            if (!command_input_args(cmd, "STEPS", "a frame count",
                     entry + 6, &player, ids, &nids, &held))
               return false;
            for (i = 0; i < nids; i++)
            {
               if (ids[i] >= 16)
               {
                  command_reply_error(cmd, "STEPS", "step %u: %s is an "
                        "axis, which INPUT set holds at a value", count,
                        command_input_names[ids[i]]);
                  return false;
               }
               s->mask |= (uint16_t)(1u << ids[i]);
            }
            /* a tap of what the press before it held runs a frame let go
             * first, as command_run_frames_tick does, a checkpoint
             * between them or not */
            while (prev > first && (prev - 1)->kind == CMD_STEP_CHECKPOINT)
               prev--;
            if (     prev > first && (prev - 1)->kind == CMD_STEP_PRESS
                  && (prev - 1)->port == player - 1
                  && ((prev - 1)->mask & s->mask))
               fixed++;
            if (     fixed > budget || held < 1
                  || (unsigned long)held > budget - fixed)
            {
               command_reply_error(cmd, "STEPS", "step %u: press for a "
                     "frame count from 1, within the %u frames one request "
                     "runs", count, budget);
               return false;
            }
            s->kind   = CMD_STEP_PRESS;
            s->port   = player - 1;
            s->frames = (unsigned)held;
            fixed    += s->frames;
            frames++;
         }
         else
         {
            command_reply_error(cmd, "STEPS", "step %u: run, press, until, "
                  "checkpoint or stop, not %s", count, entry);
            return false;
         }
      }
      if (c)
      {
         if (!command_cond_parse(entry, c, &end, why, sizeof(why)))
         {
            command_reply_error(cmd, "STEPS", "%s", why);
            return false;
         }
         while (*end == ' ')
            end++;
         if (*end)
         {
            command_reply_error(cmd, "STEPS", "nothing after a condition "
                  "but \";\"");
            return false;
         }
      }
   }
   /* the stops, numbered, just ahead of the steps */
   for (i = 0; i < nstops; i++)
   {
      struct command_step *s = first - nstops + i;
      s->cond = stops[i];
      s->kind = CMD_STEP_STOP;
      s->idx  = (uint8_t)(i + 1);
   }
   if (queue && !count && !nstops)
   {
      command_steps_report(cmd, command_steps_idle(), 0);
      return true;
   }
   if (!frames)
   {
      command_reply_error(cmd, "STEPS", "a step at least: run, press or "
            "until");
      return false;
   }
   if (!command_frames_ready(cmd, "STEPS"))
      return false;
   for (i = 0; i < count; i++)
      if (     first[i].kind == CMD_STEP_UNTIL
            && !command_cond_mapped(cmd, "STEPS", &first[i].cond))
         return false;
   for (i = 0; i < nstops; i++)
      if (!command_cond_mapped(cmd, "STEPS", &stops[i]))
         return false;
   if (!queue)
   {
      command_run_frames_start(cmd, "STEPS", first - nstops,
            nstops + count, budget, CMD_ANSWER_STEPS, NULL);
      return true;
   }
   if (runloop_state_get_ptr()->frame_work & RUNLOOP_WORK_RUN_FRAMES)
   {
      unsigned queued = nstops + count;
      if (!command_run_frames_st.queued)
      {
         command_reply_error(cmd, "STEPS", "a request waits on the steps "
               "running; queue once it is answered");
         return false;
      }
      for (i = command_run_frames_st.cur; i < command_run_frames_st.count; i++)
         if (     command_run_frames_st.steps[i].kind != CMD_STEP_STOP
               || command_run_frames_st.steps[i].idx)
            queued++;
      if (queued > COMMAND_QUEUE_MAX)
      {
         command_reply_error(cmd, "STEPS", "the queue is full: %u steps "
               "and stops at most, queued and running",
               (unsigned)COMMAND_QUEUE_MAX);
         return false;
      }
   }
   /* a stop the client has not heard about: told now, and these not
    * played on from a death */
   idle = command_steps_idle();
   /* stop 0 marks where the list starts, so it runs with its own stops
    * and counts its own frames (memset left its kind and idx) */
   first[-(int)nstops - 1].kind = CMD_STEP_STOP;
   if (!command_run_frames_st.prev[0])
      command_run_frames_queue(first - nstops - 1, 1 + nstops + count);
   command_steps_report(cmd, idle, count);
   return true;
}

/* GET_INPUT [player]
 *
 * What the player holds, without changing it. SET_INPUT answers in the
 * same shape but has to set something first, and RELEASE_INPUT lets go
 * before it answers, so neither can be used to look. */
bool command_get_input(command_t *cmd, const char *arg)
{
   char     reply[192];
   unsigned player = 1;

   while (arg && *arg == ' ')
      arg++;
   if (arg && *arg && (!command_arg_uint(arg, &player)
            || player < 1 || player > MAX_USERS))
   {
      command_reply_error(cmd, "GET_INPUT",
            "a player from 1 to %u, or nothing for the first",
            (unsigned)MAX_USERS);
      return false;
   }
   cmd->replier(cmd, reply,
         command_input_line(player - 1, reply, sizeof(reply)));
   return true;
}

/* RELEASE_INPUT [player]
 *
 * Lets go of everything SET_INPUT holds for the player, or for every
 * player. */
bool command_release_input(command_t *cmd, const char *arg)
{
   char   reply[192];
   unsigned player                = 0;

   while (arg && *arg == ' ')
      arg++;
   if (arg && *arg)
   {
      if (!command_arg_uint(arg, &player) || player < 1 || player > MAX_USERS)
      {
         command_reply_error(cmd, "RELEASE_INPUT",
               "a player from 1 to %u, or nothing for every player",
               (unsigned)MAX_USERS);
         return false;
      }
   }
   input_driver_command_release(player ? player - 1 : MAX_USERS);

   cmd->replier(cmd, reply,
         command_input_line(player ? player - 1 : 0, reply, sizeof(reply)));
   return true;
}

/* The core option @key, or false with the reason replied. */
static bool command_core_option_find(command_t *cmd, const char *name,
      const char *key, core_option_manager_t **opt, size_t *idx)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   if (!(runloop_get_flags() & RUNLOOP_FLAG_CORE_RUNNING))
   {
      command_reply_error(cmd, name, "no core is running: LOAD_CONTENT or "
            "CORE start starts one");
      return false;
   }
   if (!(*opt = runloop_st ? runloop_st->core_options : NULL) || !(*opt)->size)
   {
      command_reply_error(cmd, name, "the running core has no options");
      return false;
   }
   if (key && !core_option_manager_get_idx(*opt, key, idx))
   {
      command_reply_error(cmd, name,
            "no such core option %s; CORE_OPTION show lists them", key);
      return false;
   }
   return true;
}

/* @opt's line: key, value and description, tab separated, and its
 * values after a tab when @values. Returns what it wrote, or 0 when
 * that did not fit @len. */
static size_t command_core_option_line(const struct core_option *opt,
      char *s, size_t len, bool values)
{
   size_t i;
   size_t _len;
   int    n = snprintf(s, len, "%s\t%s\t%s", opt->key,
         opt->vals && opt->index < opt->vals->size
         ? opt->vals->elems[opt->index].data : "",
         opt->desc ? opt->desc : "");

   if (n < 0 || (size_t)n >= len)
      return 0;
   _len = (size_t)n;
   if (values && opt->vals)
      for (i = 0; i < opt->vals->size; i++)
      {
         n = snprintf(s + _len, len - _len, "%s%s", i ? "|" : "\t",
               opt->vals->elems[i].data);
         if (n < 0 || (size_t)n >= len - _len)
            return 0;
         _len += (size_t)n;
      }
   if (_len + 1 >= len)
      return 0;
   s[_len++] = '\n';
   s[_len]   = '\0';
   return _len;
}

/* GET_CORE_OPTION [key]
 *
 * Every option of the running core, one per line: key, value and
 * description, tab separated. With a key, that option's line alone,
 * with the values it takes after a further tab, separated by |. */
bool command_get_core_option(command_t *cmd, const char *arg)
{
   core_option_manager_t *opt = NULL;
   size_t idx                 = 0;
   size_t _len                = 0;
   size_t i;
   char  *reply;

   while (arg && *arg == ' ')
      arg++;
   if (!command_core_option_find(cmd, "GET_CORE_OPTION",
            arg && *arg ? arg : NULL, &opt, &idx))
      return false;
   if (!(reply = (char*)malloc(COMMAND_LIST_REPLY_MAX + 1)))
   {
      command_reply_error(cmd, "GET_CORE_OPTION", "out of memory");
      return false;
   }

   for (i = (arg && *arg) ? idx : 0; i < opt->size; i++)
   {
      size_t n = command_core_option_line(&opt->opts[i], reply + _len,
            COMMAND_LIST_REPLY_MAX + 1 - _len, arg && *arg);
      if (!n)
      {
         free(reply);
         command_reply_error(cmd, "GET_CORE_OPTION",
               "the options did not fit one reply, stopping at %s",
               opt->opts[i].key);
         return false;
      }
      _len += n;
      if (arg && *arg)
         break;
   }

   cmd->replier(cmd, reply, _len);
   free(reply);
   return true;
}

/* SET_CORE_OPTION <key> <value>
 *
 * The value is one GET_CORE_OPTION <key> lists. Asking for the value
 * the option already has changes nothing and succeeds. */
bool command_set_core_option(command_t *cmd, const char *arg)
{
   char   key[NAME_MAX_LENGTH];
   char   reply[NAME_MAX_LENGTH + PATH_MAX_LENGTH];
   core_option_manager_t *opt = NULL;
   const char *sp;
   size_t idx                 = 0;
   size_t val_idx             = 0;
   size_t _len;

   while (arg && *arg == ' ')
      arg++;
   if (!arg || !*arg || !(sp = strchr(arg, ' ')))
   {
      command_reply_error(cmd, "SET_CORE_OPTION",
            "a core option key and a value");
      return false;
   }
   if ((size_t)(sp - arg) >= sizeof(key))
   {
      command_reply_error(cmd, "SET_CORE_OPTION", "no such core option");
      return false;
   }
   memcpy(key, arg, (size_t)(sp - arg));
   key[sp - arg] = '\0';
   while (*sp == ' ')
      sp++;

   if (!command_core_option_find(cmd, "SET_CORE_OPTION", key, &opt, &idx))
      return false;
   if (!core_option_manager_get_val_idx(opt, idx, sp, &val_idx))
   {
      command_reply_error(cmd, "SET_CORE_OPTION",
            "%s does not take %s; GET_CORE_OPTION %s lists what it takes",
            key, sp, key);
      return false;
   }
   if (opt->opts[idx].index != val_idx)
      core_option_manager_set_val(opt, idx, val_idx, true);

   /* the shape GET_CORE_OPTION gives, so one reading serves both */
   if (!(_len = command_core_option_line(&opt->opts[idx], reply,
               sizeof(reply), false)))
      _len = (size_t)snprintf(reply, sizeof(reply), "%s\t%s\n", key, sp);
   cmd->replier(cmd, reply, _len);
   return true;
}

/* GET_CONFIG_PARAM <param name>
 *
 * The option that goes by this name, which GET_OPTION also reports;
 * "unsupported" for one this build does not have, as it always was.
 * Booleans are true and false here, the words this interface has always
 * answered, where GET_OPTION says on and off. */
bool command_get_config_param(command_t *cmd, const char* arg)
{
   char   value[PATH_MAX_LENGTH];
   char   reply[PATH_MAX_LENGTH + NAME_MAX_LENGTH];
   size_t _len;
   const struct cmd_option_map *o;

   while (arg && *arg == ' ')
      arg++;
   if (!arg)
      arg = "";
   if ((o = command_option_find(arg, true)))
      command_option_read(o, value, sizeof(value), "true", "false");
   else
      strlcpy(value, "unsupported", sizeof(value));

   _len = (size_t)snprintf(reply, sizeof(reply), "GET_CONFIG_PARAM %s %s",
         arg, value);
   cmd->replier(cmd, reply, _len);
   return true;
}


/* LIST_CORES [content path]
 *
 * The installed cores, one per line: display name, a tab, the path
 * LOAD_CONTENT takes. With a content path, only the cores that support
 * it - the candidates LOAD_CONTENT names when it is handed content and
 * no core to load it with. Filtering sorts the shared list, as the
 * menu does, so the unfiltered order can change afterwards. */
bool command_list_cores(command_t *cmd, const char *arg)
{
   core_info_list_t  *list  = NULL;
   const core_info_t *infos = NULL;
   size_t             count = 0;
   size_t i, len = 0;
   char  *reply;

   while (arg && *arg == ' ')
      arg++;

   if (!(reply = (char*)malloc(COMMAND_LIST_REPLY_MAX + 64)))
      return false;
   reply[0] = '\0';
   if (core_info_get_list(&list) && list)
   {
      if (arg && *arg)
         core_info_list_get_supported_cores(list, arg, &infos, &count);
      else
      {
         infos = list->list;
         count = list->count;
      }
      for (i = 0; i < count; i++)
      {
         const core_info_t *ci = &infos[i];
         int n;
         if (!ci->path)
            continue;
         n = snprintf(reply + len, COMMAND_LIST_REPLY_MAX - len, "%s\t%s\n",
               ci->display_name ? ci->display_name : "", ci->path);
         if (n < 0 || (size_t)n >= COMMAND_LIST_REPLY_MAX - len)
         {
            len += (size_t)snprintf(reply + len, 64, "MORE\n");
            break;
         }
         len += (size_t)n;
      }
   }
   if (!len)
      len = strlcpy(reply, "LIST_CORES none\n", COMMAND_LIST_REPLY_MAX);
   cmd->replier(cmd, reply, len);
   free(reply);
   return true;
}

/* LIST_PLAYLISTS and GET_PLAYLIST read files, which may be slow, so
 * they answer from a task: the text is made on the task thread and sent
 * from the callback. */
struct command_query
{
   command_deferred_t deferred;
   size_t     first;           /* GET_PLAYLIST: first entry */
   char      *reply;           /* made by the handler */
   size_t     reply_len;
   char       dir[PATH_MAX_LENGTH];
   char       name[NAME_MAX_LENGTH];
   bool       list_playlists;
   /* the reply reports a failure: no such playlist */
   bool       error;
   /* the playlist format settings, taken on the main thread */
   bool       old_format;
   bool       compress;
   bool       fuzzy_archive_match;
   bool       portable_paths;
   char       content_dir[DIR_MAX_LENGTH];
};

static void command_query_append(struct command_query *q, size_t *cap,
      const char *s)
{
   size_t n = strlen(s);
   if (!q->reply || q->reply_len + n + 1 > *cap)
      return;
   memcpy(q->reply + q->reply_len, s, n + 1);
   q->reply_len += n;
}

static void command_query_handler(retro_task_t *task)
{
   struct command_query *q = (struct command_query*)task->state;
   size_t cap              = COMMAND_LIST_REPLY_MAX;

   if ((q->reply = (char*)malloc(cap + 1)))
      q->reply[0] = '\0';

   if (q->list_playlists)
   {
      struct string_list *files = dir_list_new(q->dir, "lpl",
            false, false, false, false);
      size_t i;
      for (i = 0; files && i < files->size; i++)
      {
         char line[NAME_MAX_LENGTH + 2];
         const char *base = path_basename(files->elems[i].data);
         snprintf(line, sizeof(line), "%s\n", base);
         if (q->reply_len + strlen(line) + 6 > cap)
         {
            command_query_append(q, &cap, "MORE\n");
            break;
         }
         command_query_append(q, &cap, line);
      }
      if (files)
         string_list_free(files);
      if (!q->reply_len)
         command_query_append(q, &cap, "LIST_PLAYLISTS none\n");
   }
   else
   {
      /* the config holds two paths: on the heap, as this runs on a task
       * thread when the queue is threaded */
      playlist_config_t *config = (playlist_config_t*)calloc(1,
            sizeof(*config));
      playlist_t *pl            = NULL;

      if (config)
      {
         fill_pathname_join_special(config->path, q->dir, q->name,
               sizeof(config->path));
         if (!string_ends_with_size(config->path, ".lpl",
                  strlen(config->path), 4))
            strlcat(config->path, ".lpl", sizeof(config->path));
         config->capacity            = COLLECTION_SIZE;
         config->old_format          = q->old_format;
         config->compress            = q->compress;
         config->fuzzy_archive_match = q->fuzzy_archive_match;
         playlist_config_set_base_content_directory(config,
               q->portable_paths ? q->content_dir : NULL);
      }

      if (     !config || !path_is_valid(config->path)
            || !(pl = playlist_init(config)))
      {
         q->error = true;
         command_query_append(q, &cap, "GET_PLAYLIST ERROR no such playlist\n");
      }
      else
      {
         size_t i, n = playlist_size(pl);
         for (i = q->first; q->reply && i < n; i++)
         {
            const struct playlist_entry *e = NULL;
            char   more[32];
            size_t room = cap - q->reply_len;
            int    len;
            if (i - q->first >= COMMAND_PLAYLIST_PAGE)
            {
               snprintf(more, sizeof(more), "MORE %u\n", (unsigned)i);
               command_query_append(q, &cap, more);
               break;
            }
            playlist_get_index(pl, i, &e);
            if (!e)
               continue;
            /* the row goes straight into the reply, room kept for the
             * MORE that says where to go on from */
            len = room > 16 ? snprintf(q->reply + q->reply_len, room - 16,
                  "%u\t%s\t%s\t%s\n", (unsigned)i,
                  e->label     ? e->label     : "",
                  e->path      ? e->path      : "",
                  e->core_path ? e->core_path : "") : -1;
            if (len < 0 || (size_t)len >= room - 16)
            {
               q->reply[q->reply_len] = '\0';
               snprintf(more, sizeof(more), "MORE %u\n", (unsigned)i);
               command_query_append(q, &cap, more);
               break;
            }
            q->reply_len += (size_t)len;
         }
         if (!q->reply_len)
            command_query_append(q, &cap, "GET_PLAYLIST empty\n");
         playlist_free(pl);
      }
      free(config);
   }
   task_set_progress(task, 100);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void command_query_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   struct command_query *q = (struct command_query*)task->state;
   if (q)
      command_deferred_send(&q->deferred, q->reply, q->reply_len, q->error);
}

static void command_query_cleanup(retro_task_t *task)
{
   struct command_query *q = (struct command_query*)task->state;
   if (!q)
      return;
   free(q->reply);
   free(q->deferred.dest);
   free(q);
   task->state = NULL;
}

static bool command_query_push(command_t *cmd, bool list_playlists,
      const char *arg)
{
   settings_t           *settings = config_get_ptr();
   struct command_query *q;
   retro_task_t         *task;

   if (!(q = (struct command_query*)calloc(1, sizeof(*q))))
      return false;
   q->list_playlists = list_playlists;
   strlcpy(q->dir, settings->paths.directory_playlist, sizeof(q->dir));
   q->old_format          = settings->bools.playlist_use_old_format;
   q->compress            = settings->bools.playlist_compression;
   q->fuzzy_archive_match = settings->bools.playlist_fuzzy_archive_match;
   q->portable_paths      = settings->bools.playlist_portable_paths;
   strlcpy(q->content_dir, settings->paths.directory_menu_content,
         sizeof(q->content_dir));
   if (!list_playlists)
   {
      /* "<name> [first]": the name may hold spaces, so a last word that
       * is a number and nothing else is the first entry wanted. It has
       * to be the whole word: a playlist whose name ends in one, as
       * "Atari - 2600" and "Sony - PlayStation 2" do, was read as a
       * name without it and an entry to start at. */
      const char *sp;
      while (*arg == ' ')
         arg++;
      strlcpy(q->name, arg, sizeof(q->name));
      if ((sp = strrchr(q->name, ' ')) && sp[1] >= '0' && sp[1] <= '9')
      {
         char         *end   = NULL;
         unsigned long first = strtoul(sp + 1, &end, 10);
         /* the whole word, or it belongs to the name */
         if (end && !*end)
         {
            q->first              = (size_t)first;
            q->name[sp - q->name] = '\0';
         }
      }
      if (!*q->name || strchr(q->name, '/') || strchr(q->name, '\\'))
      {
         command_reply_error(cmd, "GET_PLAYLIST",
               "a playlist name, as LIST_PLAYLISTS gives");
         free(q);
         return false;
      }
   }
   if (!(task = task_init()))
   {
      free(q);
      return false;
   }
   command_deferred_take(&q->deferred, cmd);
   task->state    = q;
   task->handler  = command_query_handler;
   task->callback = command_query_callback;
   task->cleanup  = command_query_cleanup;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
   return true;
}

/* LIST_PLAYLISTS: the playlist files, one per line, as GET_PLAYLIST
 * takes them. */
bool command_list_playlists(command_t *cmd, const char *arg)
{
   return command_query_push(cmd, true, arg);
}

/* GET_PLAYLIST <playlist> [first]: a playlist's entries, one per line:
 * index, label, content path, core path, tab separated;
 * COMMAND_PLAYLIST_PAGE at a time, "MORE <next>" when there are more. */
bool command_get_playlist(command_t *cmd, const char *arg)
{
   return command_query_push(cmd, false, arg);
}

/* One HELP line: name, argument, description, and what it does to
 * RetroArch, appended to @s at *@len; false when it would not fit. */
static bool command_help_line(char *s, size_t size, size_t *len,
      const char *name, const char *arg_desc, const char *desc,
      unsigned flags)
{
   int n = snprintf(s + *len, size - *len, "%s%s%s\t%s%s\n",
         name,
         arg_desc ? " " : "", arg_desc ? arg_desc : "",
         desc ? desc : "",
           (flags & CMD_INFO_READ_ONLY)   ? " [read-only]"
         : (flags & CMD_INFO_DESTRUCTIVE) ? " [destructive]"
         : "");
   if (n < 0 || (size_t)n >= size - *len)
      return false;
   *len += (size_t)n;
   return true;
}

/* HELP [command]
 *
 * With no argument, every command, one per line: the name, its
 * argument, a tab, what it does, and [read-only] or [destructive].
 * Hotkey commands take no argument and press the hotkey for one
 * frame. With a command name, that command's line alone. */
bool command_help(command_t *cmd, const char* arg)
{
   size_t   i;
   size_t   _len  = 0;
   size_t   size  = 256
      + (ARRAY_SIZE(map) + ARRAY_SIZE(action_map)) * 160;
   bool     found = false;
   char    *reply;

   while (arg && *arg == ' ')
      arg++;
   if (!(reply = (char*)malloc(size)))
      return false;
   reply[0] = '\0';

   for (i = 0; i < ARRAY_SIZE(action_map); i++)
   {
      bool noarg = string_is_equal(action_map[i].arg_desc, "No argument");
      if (arg && *arg && !string_is_equal(arg, action_map[i].str))
         continue;
      found = true;
      if (!command_help_line(reply, size, &_len, action_map[i].str,
               noarg ? NULL : action_map[i].arg_desc,
               action_map[i].desc, action_map[i].flags))
         break;
   }
   for (i = 0; i < ARRAY_SIZE(map); i++)
   {
      size_t j;
      /* RESET is an action too, whose line says it */
      for (j = 0; j < ARRAY_SIZE(action_map); j++)
         if (string_is_equal(map[i].str, action_map[j].str))
            break;
      if (j < ARRAY_SIZE(action_map))
         continue;
      if (arg && *arg && !string_is_equal(arg, map[i].str))
         continue;
      found = true;
      if (!command_help_line(reply, size, &_len, map[i].str, NULL,
               map[i].desc, map[i].flags))
         break;
   }
   if (!found)
      _len = (size_t)snprintf(reply, size, "HELP ERROR unknown command %s\n",
            arg);

   cmd->replier(cmd, reply, _len);
   free(reply);
   return found;
}

bool command_version(command_t *cmd, const char* arg)
{
   char reply[256];
   size_t  _len  = strlcpy(reply, PACKAGE_VERSION, sizeof(reply));
   reply[  _len] = '\n';
   reply[++_len] = '\0';
   cmd->replier(cmd, reply, _len);
   return true;
}

/* Whether @path is a core that can be loaded, answered to @cmd with
 * why when it is not.
 *
 * Asked before anything is torn down for it: a path that exists and is
 * not a core - a stale library, something renamed - would otherwise
 * unload the running core and then fail, costing the game that was
 * running. libretro_get_system_info() is the probe core scanning
 * already uses. A built-in core is not a file on disk and has nothing
 * to probe. */
static bool command_core_at(command_t *cmd, const char *name,
      const char *path)
{
   struct retro_system_info info;

   if (string_ends_with_size(path, "builtin",
            strlen(path), STRLEN_CONST("builtin")))
      return true;
   if (!path_is_valid(path))
   {
      command_reply_error(cmd, name, "no core at %s", path);
      return false;
   }
   if (!libretro_get_system_info(path, &info, NULL))
   {
      command_reply_error(cmd, name, "%s is not a libretro core", path);
      return false;
   }
   return true;
}

/* LOAD_CORE <core path>
 *
 * Mirrors selecting a core in the menu's core list / file browser,
 * action_ok_load_core() -> generic_action_ok()'s ACTION_OK_LOAD_CORE
 * branch, with the same task_push_load_new_core() call. */
bool command_load_core(command_t *cmd, const char* arg)
{
   content_ctx_info_t content_info = {0};
   char path[PATH_MAX_LENGTH];

   while (arg && *arg == ' ')
      arg++;
   if (!arg || !*arg)
   {
      command_reply_error(cmd, "LOAD_CORE", "a core path");
      return false;
   }
#if TARGET_OS_IPHONE
   fill_pathname_expand_special(path, arg, sizeof(path));
#else
   strlcpy(path, arg, sizeof(path));
#endif
   if (!command_core_at(cmd, "LOAD_CORE", path))
      return false;
   if (!task_push_load_new_core(path, NULL,
            &content_info, CORE_TYPE_PLAIN, NULL, NULL))
   {
      command_reply_error(cmd, "LOAD_CORE", "could not load %s", path);
      return false;
   }
   return true;
}

/* START_CORE
 *
 * Mirrors the Main Menu "Start Core" entry, action_ok_start_core():
 * clears the content path and starts the currently loaded core without
 * content.  The list_cache() call that entry makes first is menu
 * animation bookkeeping and has no equivalent here. */
bool command_start_core(command_t *cmd, const char* arg)
{
   content_ctx_info_t content_info = {0};

   path_clear(RARCH_PATH_BASENAME);

   if (!task_push_start_current_core(&content_info))
      return false;
   return command_content_reply(cmd, "START_CORE", false, true, false);
}

/* LOAD_CONTENT <core path>|<content path>
 *
 * The separator is '|' rather than a space because both operands are
 * filesystem paths and may legitimately contain spaces.  Routed through
 * the companion UI entry point so that the command interface takes the
 * same content load path as any other frontend-external requester,
 * rather than duplicating the menu-only variant.
 *
 * Both paths are resolved and checked before that load is pushed: the
 * task unloads the running core first and only then finds out that a
 * path is unusable, so a typo would otherwise cost the caller the game
 * it was playing. */
bool command_load_content(command_t *cmd, const char* arg)
{
   char core_path[PATH_MAX_LENGTH];
#if TARGET_OS_IPHONE
   char expanded[PATH_MAX_LENGTH];
#endif
   content_ctx_info_t content_info = {0};
   const char *content_path        = NULL;
   const char *sep                 = NULL;
   const char *last;
   char       *trimmed             = NULL;
   size_t _len                     = 0;
   bool pause                      = false;
   bool ret                        = false;

   if (!arg || !*arg || !(sep = strchr(arg, '|')))
   {
      command_reply_error(cmd, "LOAD_CONTENT",
            "a core path, '|' and a content path");
      return false;
   }

   _len = (size_t)(sep - arg);
   if (_len >= sizeof(core_path))
   {
      command_reply_error(cmd, "LOAD_CONTENT", "core path is too long");
      return false;
   }
   if (!*(sep + 1))
   {
      command_reply_error(cmd, "LOAD_CONTENT", "no content path after '|'");
      return false;
   }

   memcpy(core_path, arg, _len);
   core_path[_len] = '\0';

   /* "|on" or "|off" after the content path: whether to pause once it
    * has loaded. Anything else after a "|" is the path's own. */
   content_path    = sep + 1;
   if (     (last = strrchr(content_path, '|'))
         && (string_is_equal(last + 1, "on")
            || string_is_equal(last + 1, "off")))
   {
      _len  = (size_t)(last - content_path);
      pause = string_is_equal(last + 1, "on");
      if (!(trimmed = (char*)malloc(_len + 1)))
         return false;
      memcpy(trimmed, content_path, _len);
      trimmed[_len] = '\0';
      content_path  = trimmed;
   }
#if TARGET_OS_IPHONE
   fill_pathname_expand_special(expanded, content_path, sizeof(expanded));
   content_path    = expanded;
#endif

   /* Content inside an archive is named as one path, which
    * path_is_valid() would reject */
   if (!playlist_content_path_is_valid(content_path))
   {
      command_reply_error(cmd, "LOAD_CONTENT", "no content at %s",
            content_path);
      goto end;
   }

   /* A playlist entry with no core of its own carries DETECT, so take
    * that, and nothing at all, as a request for a core that supports
    * the content.  The menu asks the user to pick one where the
    * playlist's default core does not settle it; nobody is here to
    * answer that, so an ambiguous one says so and leaves the choice;
    * LIST_CORES, given the same content path, lists the candidates. */
   if (!*core_path || string_is_equal(core_path, FILE_PATH_DETECT))
   {
      core_info_list_t *core_info_list = NULL;
      const core_info_t *supported     = NULL;
      size_t count                     = 0;

      if (core_info_get_list(&core_info_list))
         core_info_list_get_supported_cores(core_info_list, content_path,
               &supported, &count);

      if (count == 0)
      {
         command_reply_error(cmd, "LOAD_CONTENT",
               "no installed core supports %s", content_path);
         goto end;
      }
      if (count > 1)
      {
         command_reply_error(cmd, "LOAD_CONTENT",
               "%u cores support %s, name one of them; LIST_CORES with "
               "that content path lists them", (unsigned)count,
               content_path);
         goto end;
      }

      /* An entry whose core file is gone keeps its info and loses its
       * path; it supports the content and cannot be loaded. */
      if (!supported[0].path)
      {
         command_reply_error(cmd, "LOAD_CONTENT",
               "the only core that supports %s is not installed",
               content_path);
         goto end;
      }
      strlcpy(core_path, supported[0].path, sizeof(core_path));
   }
   else if (!command_core_at(cmd, "LOAD_CONTENT", core_path))
      goto end;

   if (!task_push_load_content_with_new_core_from_companion_ui(
         core_path, content_path, NULL, NULL, NULL, &content_info, NULL, NULL))
   {
      command_reply_error(cmd, "LOAD_CONTENT", "could not load %s with %s",
            content_path, core_path);
      goto end;
   }
   ret = command_content_reply(cmd, "LOAD_CONTENT", false, true, pause);
end:
   free(trimmed);
   return ret;
}

/* CLOSE_CONTENT
 *
 * On HAVE_MENU builds this is CMD_EVENT_CLOSE_CONTENT, the same event the
 * Quick Menu "Close Content" entry issues.
 *
 * On builds without a menu, CMD_EVENT_CLOSE_CONTENT is defined as
 * CMD_EVENT_QUIT -- reasonable for a hotkey on a frontend that has
 * nowhere to return to, but wrong here: a command interface consumer
 * asking to close content is not asking to terminate the process, and
 * has no way to discover that the two are the same on this build.  Use
 * CMD_EVENT_UNLOAD_CORE instead, which unloads the core and starts the
 * dummy core.  That is as close to "content closed" as a menuless build
 * gets, and it leaves the process alive to accept further commands.
 *
 * This was previously a map[] entry driving RARCH_CLOSE_CONTENT_KEY,
 * which is not what the menu entry does either: the hotkey path in
 * runloop_iterate() also applies the 'confirm_close' double press timer,
 * and nothing external can produce the second press inside the window,
 * so with that setting enabled the command was silently swallowed.  The
 * menu entry's confirmation is a dialog rather than a timer and is
 * equally unreachable over the command interface, so neither form of
 * confirmation applies on this path. */
bool command_close_content(command_t *cmd, const char* arg)
{
#ifdef HAVE_MENU
   /* this quits instead, and there is no unload to wait for */
   if (should_quit_on_close())
      return command_event(CMD_EVENT_CLOSE_CONTENT, NULL);
   /* the menu unloads the core on a later frame */
   if (!command_event(CMD_EVENT_CLOSE_CONTENT, NULL))
      return false;
   return command_content_reply(cmd, "CLOSE_CONTENT", true, false, false);
#else
   if (!command_event(CMD_EVENT_UNLOAD_CORE, NULL))
      return false;
   return command_content_reply(cmd, "CLOSE_CONTENT", false, false, false);
#endif
}

/* UNLOAD_CORE
 *
 * CMD_EVENT_UNLOAD_CORE releases the core and starts the dummy core, and
 * is not menu dependent.  The Main Menu "Unload Core" entry issues the
 * same event and then clears the last core path; that clear is a
 * frontend path operation rather than a menu one, so it belongs here
 * too.  Only the entry refresh afterwards is menu state. */
bool command_unload_core(command_t *cmd, const char* arg)
{
   if (!command_event(CMD_EVENT_UNLOAD_CORE, NULL))
      return false;

   command_content_reply(cmd, "UNLOAD_CORE", false, false, false);
   path_clear(RARCH_PATH_CORE_LAST);

#ifdef HAVE_MENU
   {
      struct menu_state *menu_st = menu_state_get_ptr();
      if (menu_st)
         menu_st->flags |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                         |  MENU_ST_FLAG_PREVENT_POPULATE;
   }
#endif

   return true;
}

/* VIDEO_REINIT / AUDIO_REINIT / DRIVERS_REINIT
 *
 * Driver reinit, split by scope.  None has a menu equivalent; they exist
 * so that a driver teardown and rebuild can be triggered on its own,
 * rather than only as a side effect of loading content.
 *
 * CMD_EVENT_REINIT takes an optional driver mask through its data
 * pointer and falls back to DRIVERS_CMD_ALL when passed NULL, so
 * VIDEO_REINIT passes DRIVER_VIDEO_MASK explicitly rather than relying on
 * the default -- otherwise it would tear down audio, input, MIDI and the
 * rest as well, and would not be a video reinit at all.  A video-only
 * mask through video_driver_reinit() is what the CRT switch path already
 * does.  DRIVERS_REINIT keeps the all-drivers behaviour under a name that
 * says so. */
bool command_video_reinit(command_t *cmd, const char* arg)
{
   int flags = DRIVER_VIDEO_MASK;
   command_event(CMD_EVENT_REINIT, &flags);
   return true;
}

bool command_audio_reinit(command_t *cmd, const char* arg)
{
   return command_event(CMD_EVENT_AUDIO_REINIT, NULL);
}

bool command_drivers_reinit(command_t *cmd, const char* arg)
{
   command_event(CMD_EVENT_REINIT, NULL);
   return true;
}

static const rarch_memory_descriptor_t* command_memory_get_descriptor(const rarch_memory_map_t* mmap, unsigned address, size_t* offset)
{
   const rarch_memory_descriptor_t* desc = mmap->descriptors;
   const rarch_memory_descriptor_t* end  = desc + mmap->num_descriptors;

   for (; desc < end; desc++)
   {
      if (desc->core.select == 0)
      {
         /* if select is 0, attempt to explicitly match the address */
         if (address >= desc->core.start && address < desc->core.start + desc->core.len)
         {
            *offset = address - desc->core.start;
            return desc;
         }
      }
      else
      {
         /* otherwise, attempt to match the address by matching the select bits */
         if (((desc->core.start ^ address) & desc->core.select) == 0)
         {
            /* adjust the address to the start of the descriptor */
            unsigned desc_offset = address - (unsigned)desc->core.start;

            /* address is unsigned. we only need that much of the disconnect mask */
            unsigned mask = (unsigned)desc->core.disconnect;

            /* this magic logic is copied from mmap_reduce. it removes any bits from
             * address that are non-zero in the disconnect field. bits above the
             * removed bits are shifted down to fill the gap. */
            while (mask)
            {
               const unsigned tmp = (mask - 1) & ~mask;
               desc_offset = (desc_offset & tmp) | ((desc_offset >> 1) & ~tmp);
               mask        = (mask & (mask - 1)) >> 1;
            }

            /* we've calculated the actual offset of the data within the descriptor */
            *offset = desc_offset;

            /* sanity check - make sure the descriptor is large enough to hold the target address */
            if (desc_offset < desc->core.len)
               return desc;
         }
      }
   }

   return NULL;
}

/* A core with no memory map still hands over its RAM as blocks, once a
 * game is loaded: system RAM from address 0 and save RAM straight after
 * it, as the achievements lay them out when they know no console - for
 * the SNES, the addresses RetroAchievements' notes give. Fills @blocks
 * (data, start, size) for the system and the save block, a NULL data
 * where the core has none; false where it has neither. */
struct command_ram_block
{
   uint8_t *data;
   size_t   start;
   size_t   size;
};

static bool command_memory_core_blocks(struct command_ram_block blocks[2])
{
   runloop_state_t        *runloop_st = runloop_state_get_ptr();
   retro_ctx_memory_info_t info;
   memset(blocks, 0, 2 * sizeof(*blocks));
   if (     !(runloop_st->current_core.flags & RETRO_CORE_FLAG_GAME_LOADED)
         || !runloop_st->current_core.retro_get_memory_data
         || !runloop_st->current_core.retro_get_memory_size)
      return false;
   info.id = RETRO_MEMORY_SYSTEM_RAM;
   core_get_memory(&info);
   if (info.data && info.size)
   {
      blocks[0].data = (uint8_t*)info.data;
      blocks[0].size = info.size;
   }
   info.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&info);
   if (info.data && info.size)
   {
      blocks[1].data  = (uint8_t*)info.data;
      blocks[1].start = blocks[0].size;
      blocks[1].size  = info.size;
   }
   return blocks[0].data || blocks[1].data;
}

static uint8_t *command_memory_get_pointer(
      const rarch_system_info_t* sys_info,
      unsigned address, unsigned int* max_bytes,
      int for_write, char *s, size_t len)
{
   if (!sys_info || sys_info->mmaps.num_descriptors == 0)
   {
      struct command_ram_block blocks[2];
      unsigned                 n;
      bool                     any = command_memory_core_blocks(blocks);
      for (n = 0; n < 2; n++)
         if (     blocks[n].data && address >= blocks[n].start
               && address < blocks[n].start + blocks[n].size)
         {
            *max_bytes = (unsigned int)(blocks[n].start + blocks[n].size
                  - address);
            return blocks[n].data + (address - blocks[n].start);
         }
      if (any)
         strlcpy_lit(s, " -1 no descriptor for address\n", len);
      else
         strlcpy_lit(s, " -1 no memory map defined\n", len);
   }
   else
   {
      size_t offset;
      const rarch_memory_descriptor_t* desc = command_memory_get_descriptor(&sys_info->mmaps, address, &offset);
      if (!desc)
         strlcpy_lit(s, " -1 no descriptor for address\n", len);
      else if (!desc->core.ptr)
         strlcpy_lit(s, " -1 no data for descriptor\n", len);
      else if (for_write && (desc->core.flags & RETRO_MEMDESC_CONST))
         strlcpy_lit(s, " -1 descriptor data is readonly\n", len);
      else
      {
         *max_bytes = (unsigned int)(desc->core.len - offset);
         return (uint8_t*)desc->core.ptr + desc->core.offset + offset;
      }
   }

   *max_bytes = 0;
   return NULL;
}

/* @v in lowercase hexadecimal, whatever the width of size_t */
static void command_hex(char *s, size_t v)
{
   char   tmp[24];
   size_t n = 0;
   do
   {
      tmp[n++] = "0123456789abcdef"[v & 15];
      v      >>= 4;
   } while (v);
   while (n)
      *s++ = tmp[--n];
   *s = '\0';
}

/* GET_MEMORY_MAP
 *
 * The running core's memory map, one region per line, tab separated:
 * the first system address READ_CORE_MEMORY takes in it, its length,
 * its select and disconnect masks (all hexadecimal), what it holds
 * (system, save, video or other), rw, ro, or none when nothing backs
 * it, its byte order, and its address space name. */
bool command_get_memory_map(command_t *cmd, const char *arg)
{
   unsigned i;
   size_t _len                     = 0;
   char *reply;
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   const rarch_memory_map_t *mmap  = runloop_st ? &runloop_st->system.mmaps
                                                : NULL;

   if (!(reply = (char*)malloc(COMMAND_LIST_REPLY_MAX + 1)))
   {
      command_reply_error(cmd, "GET_MEMORY_MAP", "out of memory");
      return false;
   }
   /* no map: the RAM the core hands over, where READ_CORE_MEMORY reads
    * it, in an address space named core */
   if (!mmap || !mmap->num_descriptors)
   {
      struct command_ram_block blocks[2];
      command_memory_core_blocks(blocks);
      for (i = 0; i < 2; i++)
      {
         char s_start[24], s_len[24];
         if (!blocks[i].data)
            continue;
         command_hex(s_start, blocks[i].start);
         command_hex(s_len,   blocks[i].size);
         _len += (size_t)snprintf(reply + _len,
               COMMAND_LIST_REPLY_MAX + 1 - _len,
               "%s\t%s\t0\t0\t%s\trw\tlittle\tcore\n", s_start, s_len,
               i ? "save" : "system");
      }
      if (!_len)
      {
         free(reply);
         command_reply_error(cmd, "GET_MEMORY_MAP",
               "the running core has no memory map, and hands over no RAM");
         return false;
      }
      cmd->replier(cmd, reply, _len);
      free(reply);
      return true;
   }

   for (i = 0; i < mmap->num_descriptors; i++)
   {
      const struct retro_memory_descriptor *d = &mmap->descriptors[i].core;
      const char *kind   = (d->flags & RETRO_MEMDESC_SYSTEM_RAM) ? "system"
                         : (d->flags & RETRO_MEMDESC_SAVE_RAM)   ? "save"
                         : (d->flags & RETRO_MEMDESC_VIDEO_RAM)  ? "video"
                         : "other";
      const char *access = !d->ptr ? "none"
                         : (d->flags & RETRO_MEMDESC_CONST) ? "ro" : "rw";
      char start[24], len[24], sel[24], disc[24];
      int n;
      command_hex(start,      d->start);
      command_hex(len,        d->len);
      command_hex(sel,        d->select);
      command_hex(disc,       d->disconnect);
      n = snprintf(reply + _len, COMMAND_LIST_REPLY_MAX + 1 - _len,
            "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
            start, len, sel, disc, kind, access,
            (d->flags & RETRO_MEMDESC_BIGENDIAN) ? "big" : "little",
            d->addrspace ? d->addrspace : "");
      if (n < 0 || (size_t)n >= COMMAND_LIST_REPLY_MAX + 1 - _len)
      {
         free(reply);
         command_reply_error(cmd, "GET_MEMORY_MAP",
               "the map did not fit one reply, stopping at region %u", i);
         return false;
      }
      _len += (size_t)n;
   }

   cmd->replier(cmd, reply, _len);
   free(reply);
   return true;
}

bool command_get_status(command_t *cmd, const char* arg)
{
   size_t _len;
   /* a status, a system name and a file name */
   char reply[1024];
   uint8_t flags;

   if (!cmd)
      return false;

   if (!cmd->replier)
      return false;

   flags = content_get_flags();

   if (flags & CONTENT_ST_FLAG_IS_INITED)
   {
      /* add some content info */
      core_info_t *core_info      = NULL;
      runloop_state_t *runloop_st = runloop_state_get_ptr();
      const char *basename_path   = NULL;

      if (!runloop_st)
      {
         _len = strlcpy_lit(reply, "GET_STATUS ERROR", sizeof(reply));
         cmd->replier(cmd, reply, _len);
         return false;
      }

      core_info_get_current_core(&core_info);

      _len = 0;
      strlcpy_append(reply, sizeof(reply), &_len, "GET_STATUS ");

      if (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
         strlcpy_append(reply, sizeof(reply), &_len, "PAUSED");
      else
         strlcpy_append(reply, sizeof(reply), &_len, "PLAYING");

      strlcpy_append(reply, sizeof(reply), &_len, " ");

      if (core_info && core_info->system_id)
         strlcpy_append(reply, sizeof(reply), &_len, core_info->system_id);
      else if (runloop_st->system.info.library_name)
         strlcpy_append(reply, sizeof(reply), &_len,
               runloop_st->system.info.library_name);
      else
         strlcpy_append(reply, sizeof(reply), &_len, "UNKNOWN");

      strlcpy_append(reply, sizeof(reply), &_len, ",");

      basename_path = path_get(RARCH_PATH_BASENAME);
      if (basename_path)
      {
         const char *basename = path_basename(basename_path);
         if (basename)
            strlcpy_append(reply, sizeof(reply), &_len, basename);
         else
            strlcpy_append(reply, sizeof(reply), &_len, "UNKNOWN");
      }
      else
         strlcpy_append(reply, sizeof(reply), &_len, "UNKNOWN");

      strlcpy_append(reply, sizeof(reply), &_len, "\n");
   }
   else
      _len = strlcpy_lit(reply, "GET_STATUS CONTENTLESS", sizeof(reply));

   cmd->replier(cmd, reply, _len);
   return true;
}

bool command_read_memory(command_t *cmd, const char *arg)
{
   char* reply                        = NULL;
   char* reply_at                     = NULL;
   const uint8_t* data                = NULL;
   unsigned int nbytes                = 0;
   unsigned int alloc_size            = 0;
   unsigned int address               = -1;
   size_t _len                        = 0;
   unsigned int max_bytes             = 0;
   runloop_state_t *runloop_st        = runloop_state_get_ptr();
   const rarch_system_info_t* sys_info= &runloop_st->system;

   {
      char *end       = NULL;
      unsigned long num;
      if (!command_parse_address(arg, &address, &end) || *end != ' ')
      {
         command_reply_error(cmd, "READ_CORE_MEMORY",
               "a hexadecimal address, a space and a byte count");
         return false;
      }
      num             = strtoul(end + 1, NULL, 10);
      nbytes          = num <= COMMAND_READ_NBYTES_MAX ? (unsigned)num : 0;
      if (nbytes == 0)
      {
         command_reply_error(cmd, "READ_CORE_MEMORY",
               "a byte count from 1 to %u", COMMAND_READ_NBYTES_MAX);
         return false;
      }
   }

   /* Ensure large enough to return all requested bytes or an error message */
   alloc_size = 96 + nbytes * 3;
   reply      = (char*)malloc(alloc_size);
   
   if (!reply)
   {
      command_reply_error(cmd, "READ_CORE_MEMORY", "out of memory");
      return false;
   }
   
   reply_at   = reply + snprintf(reply, alloc_size - 1, "READ_CORE_MEMORY %x", address);

   if ((data = command_memory_get_pointer(
               sys_info, address, &max_bytes,
               0, reply_at, alloc_size - strlen(reply))))
   {
      char         why[128];
      char        *end  = reply_at;
      unsigned int read = 0;
      /* on into the region that follows, as a core that splits its RAM
       * maps it; where nothing maps the rest, how much was read */
      while (data)
      {
         unsigned int n = nbytes - read < max_bytes
            ? nbytes - read : max_bytes;
         if (!n)
            break;
         end   = command_hex_bytes(end, data, n);
         read += n;
         if (read >= nbytes)
            break;
         data  = command_memory_get_pointer(sys_info, address + read,
               &max_bytes, 0, why, sizeof(why));
      }
      *end++               = '\n';
      if (read < nbytes)
         end += snprintf(end, alloc_size - (size_t)(end - reply),
               "TRUNCATED %u\n", read);
      _len                 = (size_t)(end - reply);
   }
   else
   {
      /* command_memory_get_pointer() has put the reason in the reply,
       * in the form line clients have always read */
      _len                 = strlen(reply);
      cmd->error           = true;
   }

   cmd->replier(cmd, reply, _len);
   free(reply);
   return !cmd->error;
}

bool command_write_memory(command_t *cmd, const char *arg)
{
   unsigned int address         = 0;
   unsigned int max_bytes       = 0;
   char reply[128]              = "";
   char *end                    = NULL;
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   const rarch_system_info_t
      *sys_info                 = &runloop_st->system;
   char *reply_at;
   uint8_t *data;

   if (     !command_parse_address(arg, &address, &end)
         || !command_bytes_valid(end))
   {
      command_reply_error(cmd, "WRITE_CORE_MEMORY", "a hexadecimal "
            "address, then bytes from 00 to ff separated by spaces");
      return false;
   }
   arg      = end;
   reply_at = reply + snprintf(reply, sizeof(reply) - 1, "WRITE_CORE_MEMORY %x", address);
   data     = command_memory_get_pointer(sys_info, address, &max_bytes, 1,
         reply_at, sizeof(reply) - strlen(reply) - 1);

   if (data)
   {
      uint8_t* start = data;
      while (max_bytes > 0 && command_next_byte(&arg, data) > 0)
      {
         --max_bytes;
         data++;
      }

      snprintf(reply_at, sizeof(reply) - strlen(reply) - 1,
            " %u\n", (unsigned)(data - start));

#ifdef HAVE_CHEEVOS
      if (rcheevos_hardcore_active())
      {
         RARCH_LOG("[Command] Achievements hardcore mode disabled by WRITE_CORE_MEMORY.\n");
         rcheevos_pause_hardcore();
      }
#endif
   }
   else
      /* command_memory_get_pointer() has put the reason in the reply,
       * in the form line clients have always read; nothing was written */
      cmd->error = true;

   cmd->replier(cmd, reply, strlen(reply));
   return !cmd->error;
}
#endif

void command_event_set_volume(
      settings_t *settings,
      float gain,
      bool widgets_active,
      bool audio_driver_mute_enable)
{
   size_t _len;
   char msg[128];
   float new_volume = settings->floats.audio_volume + gain;
   new_volume       = MAX(new_volume, -80.0f);
   new_volume       = MIN(new_volume, 12.0f);
   configuration_set_float(settings, settings->floats.audio_volume, new_volume);
   _len             = strlcpy(msg, msg_hash_to_str(MSG_AUDIO_VOLUME),
         sizeof(msg));
   _len            += strlcpy_lit(msg + _len, ": ", sizeof(msg) - _len);
   _len            += snprintf(msg + _len, sizeof(msg) - _len, "%.1f",
         new_volume);
   _len            += strlcpy_lit(msg + _len, " dB", sizeof(msg) - _len);

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_active)
      gfx_widget_volume_update_and_show(new_volume,
            audio_driver_mute_enable);
   else
#endif
      runloop_msg_queue_push(msg, _len, 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   RARCH_LOG("[Audio] %s.\n", msg);

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, new_volume);
}

/**
 * event_set_mixer_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
void command_event_set_mixer_volume(
      settings_t *settings,
      float gain)
{
   size_t _len;
   char msg[128];
   float new_volume = settings->floats.audio_mixer_volume + gain;
   new_volume       = MAX(new_volume, -80.0f);
   new_volume       = MIN(new_volume, 12.0f);
   configuration_set_float(settings, settings->floats.audio_mixer_volume, new_volume);
   _len             = strlcpy(msg, msg_hash_to_str(MSG_AUDIO_VOLUME),
         sizeof(msg));
   _len            += strlcpy_lit(msg + _len, ": ", sizeof(msg) - _len);
   _len            += snprintf(msg + _len, sizeof(msg) - _len, "%.1f",
         new_volume);
   _len            += strlcpy_lit(msg + _len, " dB", sizeof(msg) - _len);
   runloop_msg_queue_push(msg, _len, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   RARCH_LOG("[Audio] %s.\n", msg);

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, new_volume);
}

void command_event_init_controllers(rarch_system_info_t *sys_info,
      settings_t *settings, unsigned num_active_users)
{
   unsigned port;
   unsigned num_core_ports = sys_info->ports.size;

   for (port = 0; port < num_core_ports; port++)
   {
      size_t i;
      retro_ctx_controller_info_t pad;
      unsigned device                                 = RETRO_DEVICE_NONE;
      const struct retro_controller_description *desc = NULL;

      /* Check whether current core port is mapped
       * to an input device
       * > If is not, leave 'device' set to
       *   'RETRO_DEVICE_NONE'
       * > For example: if input ports 0 and 1 are
       *   mapped to core port 0, core port 1 will
       *   be unmapped and should be disabled */
      for (i = 0; i < num_active_users; i++)
      {
         if (i >= MAX_USERS)
            break;

         if (port == settings->uints.input_remap_ports[i])
         {
            device = input_config_get_device(port);
            break;
         }
      }

      desc = libretro_find_controller_description(
            &sys_info->ports.data[port], device);

      if (desc && !desc->desc)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */
         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix device,
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("[Input] Input device ID %u is unknown to this "
                  "libretro implementation. Using RETRO_DEVICE_JOYPAD.\n",
                  device);
            device = RETRO_DEVICE_JOYPAD;
         }
      }

      pad.device     = device;
      pad.port       = port;
      core_set_controller_port_device(&pad);
   }
}

#ifdef HAVE_CONFIGFILE
static size_t command_event_save_config(const char *config_path,
   char *s, size_t len)
{
   size_t _len      = 0;
   bool path_exists = *config_path;
   const char *str  = path_exists ? config_path :
      path_get(RARCH_PATH_CONFIG);

   /* Workaround for libdecor 0.2.0 setting unwanted locale */
#if defined(HAVE_WAYLAND) && defined(HAVE_DYNAMIC)
   setlocale(LC_NUMERIC,"C");
#endif
   if (path_exists && config_save_file(config_path))
   {
#if TARGET_OS_IPHONE
      char tmp[PATH_MAX_LENGTH] = {0};
      fill_pathname_abbreviate_special(tmp, config_path, sizeof(tmp));
      _len = snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_SAVED_NEW_CONFIG_TO),
            tmp);
#else
      _len = snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_SAVED_NEW_CONFIG_TO),
            config_path);
#endif
      RARCH_LOG("[Config] %s\n", s);
      return _len;
   }

   if (str && *str)
   {
      _len = snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_SAVING_CONFIG_TO),
            str);
      RARCH_ERR("[Config] %s\n", s);
   }

   return _len;
}
#endif

static size_t command_event_undo_save_state(char *s, size_t len)
{
   if (content_undo_save_buf_is_empty())
   {
      enum msg_hash_enums msg = content_undo_save_disabled()
         ? MSG_CORE_DOES_NOT_SUPPORT_SAVESTATE_UNDO
         : MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET;
      return strlcpy(s, msg_hash_to_str(msg), len);
   }
   if (!content_undo_save_state())
      return strlcpy(s,
         msg_hash_to_str(MSG_FAILED_TO_UNDO_SAVE_STATE), len);
   return strlcpy(s,
         msg_hash_to_str(MSG_UNDOING_SAVE_STATE), len);
}

static size_t command_event_undo_load_state(char *s, size_t len)
{
   if (content_undo_load_buf_is_empty())
   {
      enum msg_hash_enums msg = content_undo_save_disabled()
         ? MSG_CORE_DOES_NOT_SUPPORT_SAVESTATE_UNDO
         : MSG_NO_STATE_HAS_BEEN_LOADED_YET;
      return strlcpy(s, msg_hash_to_str(msg), len);
   }
   if (!content_undo_load_state())
      return snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_UNDO_LOAD_STATE),
            "RAM");
#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, NULL);
#endif
   return strlcpy(s,
         msg_hash_to_str(MSG_UNDID_LOAD_STATE), len);
}

bool command_event_resize_windowed_scale(settings_t *settings,
      unsigned window_scale)
{
   unsigned idx = 0;
   bool video_fullscreen = settings->bools.video_fullscreen
         || (video_driver_get_disp_flags() & VIDEO_FLAG_FORCE_FULLSCREEN);

   if (window_scale == 0)
      return false;

   configuration_set_uint(settings, settings->uints.video_scale, window_scale);

   if (!video_fullscreen)
   {
      if (     !video_driver_test_all_flags(GFX_CTX_FLAGS_WINDOW_RESIZE_IN_PLACE)
            || !video_driver_set_video_mode(
                  video_driver_window_dims(false), false))
      {
         int reinit_flags = DRIVER_VIDEO_AND_INPUT_MASK;
         command_event(CMD_EVENT_REINIT, &reinit_flags);
      }
   }

   retroarch_ctl(RARCH_CTL_SET_WINDOWED_SCALE, &idx);

   return true;
}

size_t command_event_save_auto_state(void)
{
   size_t _len;
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   const char *name_savestate  = runloop_st->name.savestate;
   char savestate_name_auto[PATH_MAX_LENGTH];
   const char *a = NULL;

   if (!core_info_current_supports_savestate())
      return 0;
   a = path_basename(path_get(RARCH_PATH_BASENAME));
   if (!a || !*a)
      return 0;
   _len = strlcpy(savestate_name_auto, name_savestate,
         sizeof(savestate_name_auto));
   _len += strlcpy_lit(savestate_name_auto + _len, ".auto",
           sizeof(savestate_name_auto) - _len);
   if (content_auto_save_state((const char*)savestate_name_auto))
	   RARCH_LOG("[State] %s \"%s\" %s.\n",
			   msg_hash_to_str(MSG_AUTO_SAVE_STATE_TO),
			   savestate_name_auto, "succeeded");
   else
	   RARCH_LOG("[State] %s \"%s\" %s.\n",
			   msg_hash_to_str(MSG_AUTO_SAVE_STATE_TO),
			   savestate_name_auto, "failed");
   return _len;
}

#ifdef HAVE_CHEATS
void command_event_init_cheats(
      bool apply_cheats_after_load,
      const char *path_cheat_db,
      void *bsv_movie_data)
{
#ifdef HAVE_NETWORKING
   bool allow_cheats             = !netplay_driver_ctl(
         RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL);
#else
   bool allow_cheats             = true;
#endif
#ifdef HAVE_BSV_MOVIE
   allow_cheats                 &= !(bsv_movie_data != NULL);
#endif

   if (!allow_cheats)
      return;

   cheat_manager_alloc_if_empty();
   cheat_manager_load_game_specific_cheats(path_cheat_db);

   if (apply_cheats_after_load)
      cheat_manager_apply_cheats(
            config_get_ptr()->bools.notification_show_cheats_applied);
}
#endif

bool command_event_load_entry_state(settings_t *settings)
{
   char entry_state_path[PATH_MAX_LENGTH] = "";
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   bool ret                        = false;

   /* No early save-capability gate here: content_load_state() decides,
    * and an existing entry-state file outranks stale metadata. */

#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      return false;
#endif
#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
      return false;
#endif

   if (!runloop_get_entry_state_path(
         entry_state_path, sizeof(entry_state_path),
         runloop_st->entry_state_slot))
      return false;

   if (!path_is_valid(entry_state_path))
   {
      if (!runloop_get_savestate_path(
            entry_state_path, sizeof(entry_state_path),
            runloop_st->entry_state_slot))
         return false;
   }

   if (!path_is_valid(entry_state_path))
      return false;

   ret = content_load_state(entry_state_path, false, true);

   RARCH_LOG("[State] %s \"%s\".\n",
         msg_hash_to_str(MSG_FOUND_ENTRY_STATE_IN),
         entry_state_path);
   RARCH_LOG("[State] %s \"%s\" %s.\n",
         msg_hash_to_str(MSG_LOADING_ENTRY_STATE_FROM),
         entry_state_path,
         ret ? "succeeded" : "failed");

   return ret;
}

bool command_event_load_auto_state(void)
{
   size_t _len;
   char savestate_name_auto[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   const char *name_savestate      = runloop_st->name.savestate;
   bool ret                        = false;
   bool must_initialize            = (core_serialization_quirks()
         & RETRO_SERIALIZATION_QUIRK_MUST_INITIALIZE) != 0;

   /* No early save-capability gate here: content_load_state() decides,
    * and an existing .auto state file outranks stale metadata. */

#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      return false;
#endif
#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
      return false;
#endif

   if (must_initialize
         && runloop_st->auto_state_load_attempted
         && !runloop_st->auto_state_load_ready)
      return false;

   _len = strlcpy(savestate_name_auto, name_savestate,
         sizeof(savestate_name_auto));
   strlcpy_lit(savestate_name_auto + _len, ".auto",
         sizeof(savestate_name_auto) - _len);

   if (!path_is_valid(savestate_name_auto))
      return false;

   if (must_initialize && !runloop_st->auto_state_load_ready)
   {
      if (!runloop_st->auto_state_load_pending)
      {
         runloop_st->auto_state_load_pending = true;
         RARCH_LOG("[State] Auto-loading savestate \"%s\" deferred until core initialization completes.\n",
               savestate_name_auto);
      }
      return true;
   }

   if (must_initialize)
   {
      runloop_st->auto_state_load_pending  = false;
      runloop_st->auto_state_load_attempted = true;
   }

   ret = content_load_state(savestate_name_auto, false, true);

   RARCH_LOG("[State] %s \"%s\".\n",
         msg_hash_to_str(MSG_FOUND_AUTO_SAVESTATE_IN),
         savestate_name_auto);
   RARCH_LOG("[State] %s \"%s\" %s.\n",
         msg_hash_to_str(MSG_AUTOLOADING_SAVESTATE_FROM),
         savestate_name_auto,
         ret ? "succeeded" : "failed");

   return ret;
}

/**
 * Scans existing states to determine which one should be loaded
 * and which one can be deleted, using savestate wraparound if
 * enabled.
 *
 * @param settings The usual RetroArch settings ptr.
 * @param last_index Return value for load slot.
 * @param @s Return value for file name that should be removed.
 */
static void command_scan_states(
      bool show_hidden_files,
      unsigned savestate_max_keep,
      int curr_state_slot,
      unsigned *last_index, char *s)
{
   /* Base name of 128 may be too short for some (<<1%) of the
      tosec-based file names, but in practice truncating will not
      lead to mismatch */
   char state_base[128];
   char state_dir[DIR_MAX_LENGTH];
   runloop_state_t *runloop_st        = runloop_state_get_ptr();
   const char *name_savestate         = runloop_st->name.savestate;

   unsigned max_idx                   = 0;
   unsigned loa_idx                   = 0;
   unsigned gap_idx                   = UINT_MAX;
   unsigned del_idx                   = UINT_MAX;
   retro_bits_512_t slot_mapping_low  = {{0}};
   retro_bits_512_t slot_mapping_high = {{0}};

   struct string_list *dir_list       = NULL;
   const char *savefile_root          = NULL;
   size_t savefile_root_length        = 0;

   size_t i, cnt                      = 0;
   size_t cnt_in_range                = 0;

   fill_pathname_basedir(state_dir, name_savestate,
         sizeof(state_dir));

   if (!(dir_list = dir_list_new_special(state_dir,
               DIR_LIST_PLAIN, NULL, show_hidden_files)))
      return;

   fill_pathname_base(state_base, name_savestate, sizeof(state_base));

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      size_t _len;
      char elem_base[128];
      const char *ext      = NULL;
      const char *end      = NULL;
      const char *dir_elem = dir_list->elems[i].data;

      if (!dir_elem || !*dir_elem)
         continue;

      _len = strlen(dir_elem);
      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      /* Only consider files with a '.state' extension
       * > i.e. Ignore '.state.auto', '.state.bak', etc. */
      ext = path_get_extension(elem_base);
      if (    (!ext || !*ext)
          || !string_starts_with_size(ext, "state", STRLEN_CONST("state")))
         continue;

      /* Check whether this file is associated with
       * the current content */
      if (!string_starts_with(elem_base, state_base))
         continue;

      /* This looks like a valid savestate */
      /* Save filename root and length (once) */
      if (savefile_root_length == 0)
      {
         savefile_root        = dir_elem;
         savefile_root_length = _len;
      }

      /* Decode the savestate index */
      end = dir_elem + _len;
      while ((end > dir_elem) && ISDIGIT((int)end[-1]))
      {
         end--;
         if (savefile_root == dir_elem)
            savefile_root_length--;
      }
      idx = string_to_unsigned(end);

      /* Simple administration: max, total. */
      if (idx > max_idx)
         max_idx = idx;
      cnt++;
      if (idx <= savestate_max_keep)
         cnt_in_range++;

      /* Maintain a 2x512 bit map of occupied save states */
      if (idx < 512)
         BIT512_SET(slot_mapping_low,idx);
      else if (idx < 1024)
         BIT512_SET(slot_mapping_high, idx - 512);
   }

   /* Next loop on the bitmap, since the file system may have presented the files in any order above */
   for (i = 0; i <= savestate_max_keep; i++)
   {
      /* Unoccupied save slots */
      if (   (i < 512 && !BIT512_GET(slot_mapping_low,  i))
          || (i > 511 && !BIT512_GET(slot_mapping_high, i-512)))
      {
         /* Gap index: lowest free slot in the wraparound range */
         if (gap_idx == UINT_MAX)
            gap_idx = (unsigned)i;
      }
      else /* Occupied save slots */
      {
         /* Del index: first occupied slot in the wraparound range,
            after gap index */
         if (    gap_idx < UINT_MAX && del_idx == UINT_MAX)
            del_idx = (unsigned)i;
      }
   }

   /* Special cases of wraparound */

   /* No previous savestate - set to end, so that first save
      goes to 0 */
   if (cnt_in_range == 0)
   {
      if (cnt == 0)
         loa_idx = savestate_max_keep;
      /* Transient: nothing in current range, but something is present
       * higher up -> load that */
      else
         loa_idx = max_idx;
      gap_idx    = savestate_max_keep;
      del_idx    = savestate_max_keep;
   }
   /* No gap was found - deduct from current index or default
      and set (missing) gap index to be deleted */
   else if (gap_idx == UINT_MAX)
   {
      /* Transient: no gap, and max is higher than currently
       * allowed -> load that, but wrap around so that next
       * time gap will be present */
      if (max_idx > savestate_max_keep)
      {
         loa_idx = max_idx;
         gap_idx = 1;
      }
      /* Current index is in range, so let's assume it is correct */
      else if ( (unsigned)curr_state_slot < savestate_max_keep)
      {
         loa_idx = curr_state_slot;
         gap_idx = curr_state_slot + 1;
      }
      else
      {
         loa_idx = savestate_max_keep;
         gap_idx = 0;
      }
      del_idx    = gap_idx;
   }
   /* Gap was found */
   else
   {
      /* No candidate to delete */
      /* Either gap is at the end of the range: wraparound.
         or there is no better idea than the lowest index  */
      if (del_idx == UINT_MAX)
         del_idx = 0;
      /* Adjust load index */
      if (gap_idx == 0)
         loa_idx = savestate_max_keep;
      else
         loa_idx = gap_idx - 1;
   }

   RARCH_DBG("[State] Save state scanning finished, used slots (in range): "
             "%d (%d), max:%d, load index %d, gap index %d, delete index %d.\n",
             cnt, cnt_in_range, max_idx, loa_idx, gap_idx, del_idx);

   if (last_index)
         *last_index = loa_idx;

   if (     s
         && cnt_in_range >= savestate_max_keep)
   {
      strlcpy(s, savefile_root, savefile_root_length + 1);
      /* ".state0" is just ".state" instead, so don't print that. */
      if (del_idx > 0)
         snprintf(s + savefile_root_length, 5, "%d", del_idx);
   }

   dir_list_free(dir_list);
}

/**
 * Determines next savestate slot in case of auto-increment,
 * i.e. save state scanning was done already earlier.
 * Logic moved here so that all save state wraparound code is
 * in this file.
 *
 * @param settings The usual RetroArch settings ptr.
 * @return \c The next savestate slot.
 */
int command_event_get_next_savestate_auto_index(settings_t *settings)
{
   unsigned savestate_max_keep = settings->uints.savestate_max_keep;
   int new_state_slot          = settings->ints.state_slot + 1;
   /* If previous save was above the wraparound range, or it overflows,
      return to the start of the range. */
   if (     (savestate_max_keep > 0)
         && (unsigned)new_state_slot > savestate_max_keep)
      return 0;
   return new_state_slot;
}

/**
 * Determines most recent savestate slot in case of content load.
 *
 * @param settings The usual RetroArch settings ptr.
 * @return \c The most recent savestate slot.
 */
void command_event_set_savestate_auto_index(settings_t *settings)
{
   unsigned max_idx          = 0;
   bool savestate_auto_index = settings->bools.savestate_auto_index;
   if (savestate_auto_index)
   {
      int prev_slot          = settings->ints.state_slot;
      command_scan_states(
            settings->bools.show_hidden_files,
            settings->uints.savestate_max_keep,
            settings->ints.state_slot, &max_idx, NULL);
      configuration_set_int(settings, settings->ints.state_slot, max_idx);
      RARCH_LOG("[State] %s: #%d (slot reset %d -> %u from on-disk scan, "
            "max_keep %u). If the previous slot was higher, earlier saves "
            "may be missing on disk.\n",
            msg_hash_to_str(MSG_FOUND_LAST_STATE_SLOT),
            max_idx, prev_slot, max_idx,
            settings->uints.savestate_max_keep);
   }
   else
      /* Reset savestate index to 0 when loading content. */
      configuration_set_int(settings, settings->ints.state_slot, 0);
}

/**
 * Deletes the oldest save state and its thumbnail, if needed.
 *
 * @param settings The usual RetroArch settings ptr.
 */
static void command_event_set_savestate_garbage_collect(settings_t *settings)
{
   size_t i;
   char state_to_delete[PATH_MAX_LENGTH] = {0};
   command_scan_states(
         settings->bools.show_hidden_files,
         settings->uints.savestate_max_keep,
         settings->ints.state_slot, NULL, state_to_delete);
   /* Only delete one save state per save action
    * > Conservative behaviour, designed to minimise
    *   the risk of deleting multiple incorrect files
    *   in case of accident */
   if (*state_to_delete)
   {
      filestream_delete(state_to_delete);
      RARCH_DBG("[State] Garbage collect, deleting \"%s\".\n",state_to_delete);
      /* Construct the save state thumbnail name
       * and delete that one as well. */
      i = strlen(state_to_delete);
      strlcpy_lit(state_to_delete + i,".png",STRLEN_CONST(".png")+1);
      filestream_delete(state_to_delete);
      RARCH_DBG("[State] Garbage collect, deleting \"%s\".\n",state_to_delete);
   }
}

void command_event_set_replay_auto_index(settings_t *settings)
{
   size_t i;
   char elem_base[128];
   char state_base[128];
   char state_dir[DIR_MAX_LENGTH];

   struct string_list *dir_list      = NULL;
   unsigned max_idx                  = 0;
   runloop_state_t *runloop_st       = runloop_state_get_ptr();
   const char *name_replay           = runloop_st->name.replay;
   bool replay_auto_index            = settings->bools.replay_auto_index;
   bool show_hidden_files            = settings->bools.show_hidden_files;

   if (!replay_auto_index)
      return;
   /* Find the file in the same directory as runloop_st->name.replay
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.replay will try to find
    * /foo/path/content.replay%d, where %d is the largest number available.
    */
   fill_pathname_basedir(state_dir, name_replay,
         sizeof(state_dir));

   if (!(dir_list = dir_list_new_special(state_dir,
               DIR_LIST_PLAIN, NULL, show_hidden_files)))
      return;

   fill_pathname_base(state_base, name_replay, sizeof(state_base));

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      const char *end                 = NULL;
      const char *dir_elem            = dir_list->elems[i].data;
      size_t _len = fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      if (strstr(elem_base, state_base) != elem_base)
         continue;

      end = dir_elem + _len;

      while ((end > dir_elem) && ISDIGIT((int)end[-1]))
         end--;

      idx = (unsigned)strtoul(end, NULL, 0);
      if (idx > max_idx)
         max_idx = idx;
   }

   dir_list_free(dir_list);

   configuration_set_int(settings, settings->ints.replay_slot, max_idx);

   if (max_idx)
      RARCH_LOG("[Replay] %s: #%d\n",
            msg_hash_to_str(MSG_FOUND_LAST_REPLAY_SLOT),
            max_idx);
}

void command_event_set_replay_garbage_collect(
      unsigned max_to_keep,
      bool show_hidden_files
      )
{
  /* TODO: debugme */
   size_t i, cnt = 0;
   char tmp[DIR_MAX_LENGTH];
   runloop_state_t *runloop_st       = runloop_state_get_ptr();
   const char *name_replay           = runloop_st->name.replay;
   struct string_list *dir_list      = NULL;
   unsigned min_idx                  = UINT_MAX;
   const char *oldest_save           = NULL;

   /* Similar to command_event_set_replay_auto_index(),
    * this will find the lowest numbered replay */
   fill_pathname_basedir(tmp, name_replay, sizeof(tmp));

   if (!(dir_list = dir_list_new_special(tmp,
               DIR_LIST_PLAIN, NULL, show_hidden_files)))
      return;

   fill_pathname_base(tmp, name_replay, sizeof(tmp));

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      size_t _len;
      char elem_base[128];
      const char *ext                 = NULL;
      const char *end                 = NULL;
      const char *dir_elem            = dir_list->elems[i].data;

      if (!dir_elem || !*dir_elem)
         continue;

      _len = fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      /* Only consider files with a '.replayXX' extension
       * > i.e. Ignore '.replay.auto', '.replay.bak', etc. */
      ext = path_get_extension(elem_base);
      if (    (!ext || !*ext)
          || !string_starts_with_size(ext, "replay", STRLEN_CONST("REPLAY")))
         continue;

      /* Check whether this file is associated with
       * the current content */
      if (!string_starts_with(elem_base, tmp))
         continue;

      /* This looks like a valid save */
      cnt++;

      /* > Get index */
      end = dir_elem + _len;

      while ((end > dir_elem) && ISDIGIT((int)end[-1]))
         end--;

      idx = string_to_unsigned(end);

      /* > Check if this is the lowest index so far */
      if (idx < min_idx)
      {
         min_idx     = idx;
         oldest_save = dir_elem;
      }
   }

   /* Only delete one save state per save action
    * > Conservative behaviour, designed to minimise
    *   the risk of deleting multiple incorrect files
    *   in case of accident */
   if (oldest_save && *oldest_save && (cnt > max_to_keep))
      filestream_delete(oldest_save);

   dir_list_free(dir_list);
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
bool command_set_shader(command_t *cmd, const char *arg)
{
   enum  rarch_shader_type type = video_shader_parse_type(arg);
   settings_t  *settings        = config_get_ptr();
   bool apply_new_shader        = arg && *arg;
   char abs_arg[PATH_MAX_LENGTH];
#ifdef HAVE_COMMAND
   struct command_reply *r;
#endif

   if (apply_new_shader)
   {
      gfx_ctx_flags_t flags;
      flags.flags     = 0;
      video_context_driver_get_flags(&flags);

      if (!BIT32_GET(flags.flags, video_shader_type_to_flag(type)))
      {
         command_reply_error(cmd, "SET_SHADER",
               "the video driver cannot run this kind of shader");
         return false;
      }

      /* rebase on shader directory */
      if (!path_is_absolute(arg))
      {
         const char *ref_path = settings->paths.directory_video_shader;
         fill_pathname_join_special(abs_arg, ref_path, arg, sizeof(abs_arg));
         arg = abs_arg;
      }
      /* drivers fall back to the stock shader for a preset they
       * cannot read, and call that success */
      if (!path_is_valid(arg))
      {
         command_reply_error(cmd, "SET_SHADER", "no shader preset at %s",
               arg);
         return false;
      }
   }

   /* only once it is known what to apply: a refused preset leaves the
    * setting as it was */
   configuration_set_bool(settings, settings->bools.video_shader_enable, apply_new_shader);
   if (!video_shader_apply_shader(settings, type, arg, true))
   {
      command_reply_error(cmd, "SET_SHADER",
            apply_new_shader ? "the preset could not be applied"
                             : "the shader could not be turned off");
      return false;
   }

#ifdef HAVE_COMMAND
   /* A deferred load compiles over the next frames: answered once it
    * is through. */
   if (cmd && cmd->structured && (r = command_reply_new("SET_SHADER")))
   {
      if (video_shader_deferred_notify(command_reply_done, r))
      {
         command_deferred_take(&r->deferred, cmd);
         return true;
      }
      free(r);
   }
#endif
   return true;
}
#endif

#ifdef HAVE_CONFIGFILE
bool command_event_save_core_config(
      const char *dir_menu_config,
      const char *rarch_path_config)
{
   char msg[128];
   char config_dir[DIR_MAX_LENGTH];
   char config_path[PATH_MAX_LENGTH];
   char config_name[NAME_MAX_LENGTH];
   size_t _len                     = 0;
   bool new_path_available         = false;
   bool overrides_active           = false;
   const char *core_path           = NULL;
   runloop_state_t *runloop_st     = runloop_state_get_ptr();

   msg[0]                          = '\0';

   if (dir_menu_config && *dir_menu_config)
      _len = strlcpy(config_dir, dir_menu_config, sizeof(config_dir));
   else if (rarch_path_config && *rarch_path_config) /* Fallback */
      _len = fill_pathname_basedir(config_dir, rarch_path_config,
            sizeof(config_dir));

   if (_len == 0)
   {
      const char *_msg = msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET);
      runloop_msg_queue_push(_msg, strlen(_msg), 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
      RARCH_ERR("[Config] %s\n", _msg);
      return false;
   }

   core_path                       = path_get(RARCH_PATH_CORE);

   /* Infer file name based on libretro core. */
   if (path_is_valid(core_path))
   {
      unsigned i;
      char tmp[PATH_MAX_LENGTH + 8];
      RARCH_LOG("[Config] %s\n", msg_hash_to_str(MSG_USING_CORE_NAME_FOR_NEW_CONFIG));

      fill_pathname(config_name, path_basename(core_path), "",
            sizeof(config_name));
      fill_pathname_join_special(config_path, config_dir, config_name,
            sizeof(config_path));

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         size_t __len = strlcpy(tmp, config_path, sizeof(tmp));

         if (i)
            __len += snprintf(tmp + __len, sizeof(tmp) - __len, "-%u", i);
         strlcpy_lit(tmp + __len, ".cfg", sizeof(tmp) - __len);

         if (!path_is_valid(tmp))
         {
            strlcpy(config_path, tmp, sizeof(config_path));
            new_path_available = true;
            break;
         }
      }
   }

   if (!new_path_available)
   {
      /* Fallback to system time... */
      RARCH_WARN("[Config] %s\n",
            msg_hash_to_str(MSG_CANNOT_INFER_NEW_CONFIG_PATH));
      fill_dated_filename(config_name, ".cfg", sizeof(config_name));
      fill_pathname_join_special(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   if (runloop_st->flags & RUNLOOP_FLAG_OVERRIDES_ACTIVE)
   {
      /* Overrides block config file saving,
       * make it appear as overrides weren't enabled
       * for a manual save. */
      runloop_st->flags &= ~RUNLOOP_FLAG_OVERRIDES_ACTIVE;
      overrides_active   = true;
   }

#ifdef HAVE_CONFIGFILE
   _len = command_event_save_config(config_path, msg, sizeof(msg));
#endif

   if (_len > 0)
      runloop_msg_queue_push(msg, _len, 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   if (overrides_active)
      runloop_st->flags |=  RUNLOOP_FLAG_OVERRIDES_ACTIVE;
   else
      runloop_st->flags &= ~RUNLOOP_FLAG_OVERRIDES_ACTIVE;

   return true;
}

void command_event_save_current_config(enum override_type type)
{
   runloop_state_t *runloop_st     = runloop_state_get_ptr();

   switch (type)
   {
      default:
      case OVERRIDE_NONE:
         {
            size_t _len;
            char msg[256];
            enum message_queue_category msg_cat = MESSAGE_QUEUE_CATEGORY_INFO;

            msg[0] = '\0';

            if (path_is_empty(RARCH_PATH_CONFIG))
            {
               msg_cat = MESSAGE_QUEUE_CATEGORY_ERROR;
               _len    = strlcpy_lit(msg, "Config directory not set, cannot save configuration.", sizeof(msg));
            }
            else
            {
               if (runloop_st->flags & RUNLOOP_FLAG_OVERRIDES_ACTIVE)
               {
                  msg_cat = MESSAGE_QUEUE_CATEGORY_ERROR;
                  _len    = strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_ACTIVE_NOT_SAVING), sizeof(msg));
               }
               else
               {
                  msg_cat = MESSAGE_QUEUE_CATEGORY_SUCCESS;
                  _len    = command_event_save_config(path_get(RARCH_PATH_CONFIG), msg, sizeof(msg));
               }
            }

            /* command_event_save_config() does its own logging */
            if (msg_cat == MESSAGE_QUEUE_CATEGORY_ERROR)
               RARCH_ERR("[Override] %s\n", msg);

            runloop_msg_queue_push(msg, _len, 1, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, msg_cat);
         }
         break;
      case OVERRIDE_GAME:
      case OVERRIDE_CORE:
      case OVERRIDE_CONTENT_DIR:
         {
            size_t _len;
            char msg[256];
            int8_t ret      = config_save_overrides(type, &runloop_st->system, false, NULL);
            enum message_queue_category msg_cat = MESSAGE_QUEUE_CATEGORY_INFO;

            switch (ret)
            {
               case 1:
                  msg_cat = MESSAGE_QUEUE_CATEGORY_SUCCESS;
                  _len    = strlcpy(msg,
                        msg_hash_to_str(MSG_OVERRIDES_SAVED_SUCCESSFULLY), sizeof(msg));
                  /* set overrides to active so the original config can be
                     restored after closing content */
                  runloop_st->flags |= RUNLOOP_FLAG_OVERRIDES_ACTIVE;
                  break;
               case -1:
                  msg_cat = MESSAGE_QUEUE_CATEGORY_WARNING;
                  _len    = strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_NOT_SAVED), sizeof(msg));
                  break;
               default:
               case 0:
                  msg_cat = MESSAGE_QUEUE_CATEGORY_ERROR;
                  _len = strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_ERROR_SAVING), sizeof(msg));
                  break;
            }

            switch (msg_cat)
            {
               case MESSAGE_QUEUE_CATEGORY_ERROR:
                  RARCH_ERR("[Override] %s\n", msg);
                  break;
               case MESSAGE_QUEUE_CATEGORY_WARNING:
                  RARCH_WARN("[Override] %s\n", msg);
                  break;
               case MESSAGE_QUEUE_CATEGORY_SUCCESS:
               default:
                  RARCH_LOG("[Override] %s\n", msg);
                  break;
            }

            runloop_msg_queue_push(msg, _len, 1, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, msg_cat);

#ifdef HAVE_MENU
            {
               struct menu_state *menu_st      = menu_state_get_ptr();
               menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                               |  MENU_ST_FLAG_PREVENT_POPULATE;
            }
#endif
         }
         break;
   }
}

void command_event_remove_current_config(enum override_type type)
{
   runloop_state_t *runloop_st     = runloop_state_get_ptr();

   switch (type)
   {
      default:
      case OVERRIDE_NONE:
         break;
      case OVERRIDE_GAME:
      case OVERRIDE_CORE:
      case OVERRIDE_CONTENT_DIR:
         {
            size_t _len;
            char msg[256];
            enum message_queue_category msg_cat = MESSAGE_QUEUE_CATEGORY_INFO;

            if (config_save_overrides(type, &runloop_st->system, true, NULL))
            {
               msg_cat = MESSAGE_QUEUE_CATEGORY_SUCCESS;
               _len    = strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_REMOVED_SUCCESSFULLY), sizeof(msg));
            }
            else
            {
               msg_cat = MESSAGE_QUEUE_CATEGORY_ERROR;
               _len    = strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_ERROR_REMOVING), sizeof(msg));
            }

            if (msg_cat == MESSAGE_QUEUE_CATEGORY_ERROR)
               RARCH_ERR("[Override] %s\n", msg);
            else
               RARCH_LOG("[Override] %s\n", msg);

            runloop_msg_queue_push(msg, _len, 1, 180, true, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, msg_cat);
#ifdef HAVE_MENU
            {
               struct menu_state *menu_st      = menu_state_get_ptr();
               menu_st->flags                 |=  MENU_ST_FLAG_ENTRIES_NEED_REFRESH
                                               |  MENU_ST_FLAG_PREVENT_POPULATE;
            }
#endif
         }
         break;
   }
}
#endif

bool command_event_main_state(unsigned cmd)
{
   char msg[128]               = "";
   char state_path[PATH_MAX_LENGTH] = "";
   size_t _len                 = 0;
   settings_t *settings        = config_get_ptr();
   bool savestates_enabled     = core_info_current_supports_savestate();
   bool ret                    = false;

   if (savestates_enabled)
   {
      size_t info_size;
      runloop_get_current_savestate_path(state_path,
            sizeof(state_path));

      info_size          = core_serialize_size();
      savestates_enabled = (info_size > 0);
   }

  /* TODO: Load state should act in one of three ways:
     - [X] Not during recording or playback: normally
     - [-] During playback: If the state is part of this replay, go back to
           that state and rewind the replay (not yet implemented); otherwise
           halt playback and go to that state normally.
     - [-] During recording: If the state is part of this replay, go back to
           that state and rewind the replay, clobbering the stuff in between
           then and now (not yet implemented); if the state is not part of
           the replay, do nothing and log a warning.
   */
   if (savestates_enabled)
   {
      switch (cmd)
      {
         case CMD_EVENT_SAVE_STATE:
         case CMD_EVENT_SAVE_STATE_TO_RAM:
            {
               /* TODO: Saving state during recording should associate
                * the state with the replay. */
               video_driver_state_t *video_st                 =
                  video_state_get_ptr();
               bool savestate_auto_index                      =
                     settings->bools.savestate_auto_index;
               unsigned savestate_max_keep                    =
                     settings->uints.savestate_max_keep;
               bool frame_time_counter_auto_reset             =
                     settings->bools.frame_time_counter_auto_reset;

               if (cmd == CMD_EVENT_SAVE_STATE)
               {
                  bool queued = content_save_state(state_path, true);
                  RARCH_LOG("[State] save dispatch for slot %d, path "
                        "\"%s\": content_save_state queued=%s "
                        "(auto_index=%s, max_keep=%u). NOTE: actual disk "
                        "write is asynchronous; success is only known when "
                        "the save task completes.\n",
                        settings->ints.state_slot, state_path,
                        queued ? "yes" : "NO",
                        savestate_auto_index ? "on" : "off",
                        savestate_max_keep);
               }
               else
                  content_save_state_to_ram();

               /* Clean up excess savestates if necessary */
               if (savestate_auto_index && (savestate_max_keep > 0))
                  command_event_set_savestate_garbage_collect(settings);

               if (frame_time_counter_auto_reset)
                  video_st->frame_time_count = 0;

               ret      = true;
            }
            break;
         case CMD_EVENT_LOAD_STATE:
         case CMD_EVENT_LOAD_STATE_FROM_RAM:
            {
               bool res = false;
               if (cmd == CMD_EVENT_LOAD_STATE)
                  res = content_load_state(state_path, false, false);
               else
                  res = content_load_state_from_ram();

               if (res)
               {
                  command_post_state_loaded();
                  ret = true;
               }
            }
            break;
        case CMD_EVENT_UNDO_LOAD_STATE:
           {
              /* TODO: To support this through re-recording would take some
               * care around moving the replay recording forward to the time
               * when the undo happened, which would need undo support for
               * replays. For now, forbid it during recording and halt
               * playback. */
#ifdef HAVE_BSV_MOVIE
              input_driver_state_t *input_st   = input_state_get_ptr();
              if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_RECORDING)
              {
                 RARCH_ERR("[State] Can't undo load state during movie record.\n");
                 return false;
              }
              if (input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_PLAYBACK)
              {
                 RARCH_LOG("[State] Undo load state during movie playback, halting playback.\n");
                 movie_stop(input_st);
              }
#endif
              _len = command_event_undo_load_state(msg, sizeof(msg));
              ret = true;
              break;
            }
         case CMD_EVENT_UNDO_SAVE_STATE:
            _len = command_event_undo_save_state(msg, sizeof(msg));
            ret  = true;
            break;
      }
   }
   else
      _len = strlcpy(msg, msg_hash_to_str(
               MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES), sizeof(msg));

   if (_len > 0)
   {
      runloop_msg_queue_push(msg, _len, 2, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_LOG("[State] %s\n", msg);
   }

   return ret;
}

bool command_event_disk_control_append_image(
      const char *path)
{
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   rarch_system_info_t *sys_info  = runloop_st ? (rarch_system_info_t*)&runloop_st->system : NULL;
   if (     !sys_info
         || !disk_control_append_image(&sys_info->disk_control, path))
      return false;

#ifdef HAVE_THREADS
   if (runloop_st->flags & RUNLOOP_FLAG_USE_SRAM)
      autosave_deinit();
#endif

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);

   return true;
}

/* Read-side callback for the snapshot phase of command_event_reinit.
 * Receives the cached frame's pixels (or NULL if HW-render / no
 * cached frame), copies into the static reuse buffer, and reports
 * dims back to the caller via userdata.  Skips the work entirely
 * if data is NULL -- caller checks the reported size to know
 * whether a snapshot was actually taken. */
struct command_reinit_snapshot_ctx
{
   void   **buf_p;       /* static cached_snapshot in the caller */
   size_t  *cap_p;       /* static cached_snapshot_cap in the caller */
   unsigned dims;
   size_t   p, size;
};

static void command_reinit_snapshot_cb(void *userdata,
      const void *data,
      unsigned dims, size_t pitch)
{
   struct command_reinit_snapshot_ctx *ctx
      = (struct command_reinit_snapshot_ctx*)userdata;
   size_t want;

   if (!ctx || !data || !VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims) || !pitch)
      return;

   want = pitch * VIDEO_SCALE_H(dims);
   if (want > *ctx->cap_p)
   {
      void *tmp = realloc(*ctx->buf_p, want);
      if (!tmp)
         return;
      *ctx->buf_p = tmp;
      *ctx->cap_p = want;
   }
   if (!*ctx->buf_p)
      return;

   memcpy(*ctx->buf_p, data, want);
   ctx->dims = dims;
   ctx->p    = pitch;
   ctx->size = want;
}

bool command_event_reinit(const int flags)
{
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
#ifdef HAVE_MENU
   gfx_display_t *p_disp          = disp_get_ptr();
   struct menu_state *menu_st     = menu_state_get_ptr();
   bool video_fullscreen          = settings->bools.video_fullscreen;
   bool adaptive_vsync            = settings->bools.video_adaptive_vsync;
   unsigned swap_interval_config  = settings->uints.video_swap_interval;
#endif
   enum input_game_focus_cmd_type
      game_focus_cmd              = GAME_FOCUS_CMD_REAPPLY;

   /* Snapshot the last cached core frame before tearing the video
    * driver down.  video_driver_free() invalidates the cache as
    * part of the reinit cycle (the pointer was borrowed from the
    * core's own framebuffer and isn't guaranteed to stay live
    * across the driver swap), so without a snapshot the new
    * driver would come up with no core image to replay.  Restored
    * + replayed below so the paused-core background remains
    * visible when reinit is triggered from inside the menu (e.g.
    * HDR mode toggle).
    *
    * The snapshot buffer is static and reused across reinits — we
    * need it to outlive command_event_reinit because we publish
    * the pointer back through video_driver_cached_frame_publish
    * for the new driver to read via video_driver_cached_frame(),
    * and the core's next real frame will replace the pointer at
    * its leisure.  Resizing in place on each call keeps it
    * bounded at one buffer's worth.
    *
    * The actual copy happens inside the cached_frame_read
    * callback (which is what this function was hand-rolling
    * before the unified read API existed -- exactly the same
    * pattern, lifted into one place).  HW-render frames are
    * skipped: the callback receives data == NULL for those, so
    * we don't even allocate. */
   static void  *cached_snapshot      = NULL;
   static size_t cached_snapshot_cap  = 0;
   unsigned      cached_snapshot_dims = 0;
   size_t        cached_snapshot_p    = 0;
   size_t        cached_snapshot_size = 0;
   /* A reinit while the video driver is down must not create a
    * driver instance. It happens when content loads over running
    * content: core deinit tears the drivers down, then unloading
    * the old content's override fires CMD_EVENT_REINIT (the
    * override changed video_fullscreen, and CORE_RUNNING is still
    * set). An instance created here is replaced by
    * retroarch_main_init's drivers_init without being freed - an
    * orphaned window and device that widget fonts keep drawing
    * into - while the next drivers_init applies the restored mode
    * anyway. Guarded here, in the layer that owns reinit, so every
    * caller is covered and call sites stay bare command_events.
    *
    * The reasons the pointer is captured for are kept above the
    * drivers, so there is nothing to put right about the grab here:
    * the next video driver has them applied to it. */
   if (!video_st->data)
      return false;

   /* A staged content load ends in a reinit of every driver against
    * the new core: one asked for before then - the old override
    * unloading, the new core's SET_SYSTEM_AV_INFO - would rebuild
    * the drivers the stage is about to rebuild again. */
   if (runloop_is_content_switching())
      return false;

   if (recording_gpu_defer_reinit(flags))
      return true;



   {
      struct command_reinit_snapshot_ctx ctx;
      ctx.buf_p = &cached_snapshot;
      ctx.cap_p = &cached_snapshot_cap;
      ctx.dims  = 0;
      ctx.p     = 0;
      ctx.size  = 0;
      video_driver_cached_frame_read(&ctx, command_reinit_snapshot_cb);
      cached_snapshot_dims = ctx.dims;
      cached_snapshot_p    = ctx.p;
      cached_snapshot_size = ctx.size;
   }
   (void)cached_snapshot_size;

   video_driver_reinit(flags);

   /* The hotkey that asked for this may still be down, and is not to
    * ask again when the new window gets the focus. */
   input_driver_hold_over_window_change();

   /* Restore the snapshot and ask the new driver to replay it so the
    * paused-core background appears in the first post-reinit frame.
    * The buffer stays live across subsequent frame_cb calls from the
    * core (which overwrite the pointer) and is reused on the next
    * reinit.  The static cached_snapshot itself is not freed at
    * shutdown - it's a one-shot leak bounded at one framebuffer's
    * worth of memory, reclaimed by the OS on process exit.  Adding
    * a teardown hook would mean wiring command_event_reinit's
    * statics into retroarch_deinit_drivers; the size cap makes the
    * leak benign in practice, so we leave it. */
   if (cached_snapshot_p && VIDEO_SCALE_H(cached_snapshot_dims))
   {
      video_driver_cached_frame_publish(cached_snapshot,
            cached_snapshot_dims, cached_snapshot_p);

#ifdef HAVE_MENU
      /* If the menu is alive across the reinit, the runloop's
       * pre-frame menu work (driver_ctx->render +
       * set_texture_enable) doesn't get a chance to run before
       * the cached_frame() replay below.  Two consequences if we
       * don't reproduce that work here:
       *
       * 1. The new video driver instance starts with its
       *    menu-texture flag (D3D11_ST_FLAG_MENU_ENABLE,
       *    GL2_FLAG_MENU_TEXTURE_ENABLE, etc.) cleared, so the
       *    replay frame falls into the driver's "else if
       *    (statistics_show)" branch and draws OSD stats on the
       *    bare core framebuffer instead of the menu overlay.
       *
       * 2. Menu drivers like ozone cache layout/font dimensions
       *    keyed on the previous viewport size and only
       *    recompute them when their render() callback notices a
       *    width/height change.  If we replay before render()
       *    runs, the menu draws at the old (windowed) scale into
       *    the new (fullscreen) viewport, so fonts and widgets
       *    appear undersized for one frame until the next
       *    runloop iteration corrects them.
       *
       * Mirroring the runloop's pre-frame menu sequence here --
       * render with the new dimensions, then raise the
       * texture-enable flag -- keeps the snapshot replay
       * visually consistent with subsequent frames.
       *
       * Gated on a valid snapshot: without one we won't be
       * pushing a replay frame anyway, and calling render() on a
       * freshly-(re)initialised menu driver before the runloop
       * has had a chance to populate it can leave it in a
       * partially-computed state for the next real frame
       * (e.g. ozone clears OZONE_FLAG_NEED_COMPUTE after a
       * premature render). */
      if (menu_st->flags & MENU_ST_FLAG_ALIVE)
      {
         unsigned output_size = VIDEO_DRIVER_OUTPUT_DIMS(video_st);
         if (     menu_st->driver_ctx
               && menu_st->driver_ctx->render)
            menu_st->driver_ctx->render(
                  menu_st->userdata, output_size, false);

         if (     video_st->poke
               && video_st->poke->set_texture_enable)
            video_st->poke->set_texture_enable(video_st->data,
                  true, false);
      }
#endif

      video_driver_cached_frame();
   }

   /* Poll input to avoid possibly stale data to corrupt things. */
   input_driver_poll_devices();
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &game_focus_cmd);

#ifdef HAVE_MENU
   p_disp->flags |= GFX_DISP_FLAG_FB_DIRTY;
   if (video_fullscreen)
   {
      if (     video_st->poke
            && video_st->poke->show_mouse)
         video_st->poke->show_mouse(video_st->data, false);
   }
   /* The menu runs with vsync if vsync is on - the rule
    * menu_driver_toggle() applies when the menu comes up - and only
    * then. This used to switch it on whenever the menu was up, whatever
    * the setting: with vsync off, every reinit with the menu up (a
    * fullscreen toggle, say) started the new driver without vsync, as
    * configured, and a few frames later turned it on here, which
    * rebuilds the swapchain that had just been made. The menu ran with
    * a vsync the user had switched off, unlike at startup, and on at
    * least one Windows Vulkan setup the first present after that second
    * swapchain failed or left a dead picture. */
   if (     (menu_st->flags & MENU_ST_FLAG_ALIVE)
         &&  settings->bools.video_vsync
         && !settings->bools.video_scanline_sync
         && video_st->current_video->set_nonblock_state)
      video_st->current_video->set_nonblock_state(
            video_st->data, false,
               video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC)
            && adaptive_vsync,
            runloop_get_video_swap_interval(swap_interval_config));
#endif
   return true;
}

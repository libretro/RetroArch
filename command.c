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
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <streams/stdin_stream.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

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
#include "core_info.h"
#include "cheat_manager.h"
#include "content.h"
#include "dynamic.h"
#include "list_special.h"
#include "paths.h"
#include "retroarch.h"
#include "verbosity.h"
#include "version.h"
#include "version_git.h"

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
         if (!argument)
            return false;
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
      RARCH_WARN(msg_hash_to_str(MSG_UNRECOGNIZED_COMMAND), tok);
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
   ssize_t ret;
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
   }
}

command_t* command_network_new(uint16_t port)
{
   struct addrinfo     *res  = NULL;
   command_t            *cmd = (command_t*)calloc(1, sizeof(command_t));
   command_network_t *netcmd = (command_network_t*)calloc(
                                   1, sizeof(command_network_t));
   int fd                    = socket_init(
         (void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM, AF_INET);

   RARCH_LOG("[NetCMD]: %s %hu.\n",
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
      RARCH_ERR("[NetCMD]: %s.\n",
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

   if (!cmd)
      return NULL;
   if (!stdincmd)
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

bool command_get_config_param(command_t *cmd, const char* arg)
{
   size_t _len;
   char reply[8192];
   const char      *value       = "unsupported";
   settings_t       *settings   = config_get_ptr();
   bool       video_fullscreen  = settings->bools.video_fullscreen;
   const char *dir_runtime_log  = settings->paths.directory_runtime_log;
   const char *log_dir          = settings->paths.log_dir;
   const char *directory_cache  = settings->paths.directory_cache;
   const char *directory_system = settings->paths.directory_system;
   const char *path_username    = settings->paths.username;

   if (string_is_equal(arg, "video_fullscreen"))
   {
      if (video_fullscreen)
         value = "true";
      else
         value = "false";
   }
   else if (string_is_equal(arg, "savefile_directory"))
      value = dir_get_ptr(RARCH_DIR_SAVEFILE);
   else if (string_is_equal(arg, "savestate_directory"))
      value = dir_get_ptr(RARCH_DIR_SAVESTATE);
   else if (string_is_equal(arg, "runtime_log_directory"))
      value = dir_runtime_log;
   else if (string_is_equal(arg, "log_dir"))
      value = log_dir;
   else if (string_is_equal(arg, "cache_directory"))
      value = directory_cache;
   else if (string_is_equal(arg, "system_directory"))
      value = directory_system;
   else if (string_is_equal(arg, "netplay_nickname"))
      value = path_username;
   /* TODO: query any string */

   strlcpy(reply, "GET_CONFIG_PARAM ", sizeof(reply));
   _len          = strlcat(reply, arg, sizeof(reply));
   reply[_len  ] = ' ';
   reply[_len+1] = '\0';
   strlcat(reply, value, sizeof(reply));
   cmd->replier(cmd, reply, strlen(reply));
   return true;
}

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
         buf[ret]        = '\0';
         udscmd->last_fd = fd;

         command_parse_msg(handle, buf);
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

   RARCH_ERR("[NetCMD]: Command \"%s\" is not recognized by the program.\n", cmd);
   RARCH_ERR("[NetCMD]: \tValid commands:\n");
   for (i = 0; i < ARRAY_SIZE(map); i++)
      RARCH_ERR("\t\t%s\n", map[i].str);

   for (i = 0; i < ARRAY_SIZE(action_map); i++)
      RARCH_ERR("\t\t%s %s\n", action_map[i].str, action_map[i].arg_desc);

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

      RARCH_LOG("[NetCMD]: %s: \"%s\" to %s:%hu\n",
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
   size_t len                   = 0;

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
      RARCH_LOG("[Command]: Achievements hardcore mode disabled by WRITE_CORE_RAM.\n");
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

bool command_version(command_t *cmd, const char* arg)
{
   char reply[256];
   size_t _len   = strlcpy(reply, PACKAGE_VERSION, sizeof(reply));
   reply[_len  ] = '\n';
   reply[_len+1] = '\0';
   cmd->replier(cmd, reply, strlen(reply));

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
               mask = (mask & (mask - 1)) >> 1;
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

uint8_t *command_memory_get_pointer(
      const rarch_system_info_t* system,
      unsigned address,
      unsigned int* max_bytes,
      int for_write,
      char *reply_at,
      size_t len)
{
   if (!system || system->mmaps.num_descriptors == 0)
      strlcpy(reply_at, " -1 no memory map defined\n", len);
   else
   {
      size_t offset;
      const rarch_memory_descriptor_t* desc = command_memory_get_descriptor(&system->mmaps, address, &offset);
      if (!desc)
         strlcpy(reply_at, " -1 no descriptor for address\n", len);
      else if (!desc->core.ptr)
         strlcpy(reply_at, " -1 no data for descriptor\n", len);
      else if (for_write && (desc->core.flags & RETRO_MEMDESC_CONST))
         strlcpy(reply_at, " -1 descriptor data is readonly\n", len);
      else
      {
         *max_bytes = (unsigned int)(desc->core.len - offset);
         return (uint8_t*)desc->core.ptr + desc->core.offset + offset;
      }
   }

   *max_bytes = 0;
   return NULL;
}

bool command_get_status(command_t *cmd, const char* arg)
{
   char reply[4096]            = {0};
   bool contentless            = false;
   bool is_inited              = false;
   runloop_state_t *runloop_st = runloop_state_get_ptr();

   content_get_status(&contentless, &is_inited);

   if (!is_inited)
       strlcpy(reply, "GET_STATUS CONTENTLESS", sizeof(reply));
   else
   {
       /* add some content info */
       const char *status       = "PLAYING";
       const char *content_name = path_basename(path_get(RARCH_PATH_BASENAME));  /* filename only without ext */
       int content_crc32        = content_get_crc();
       const char* system_id    = NULL;
       core_info_t *core_info   = NULL;

       core_info_get_current_core(&core_info);

       if (runloop_st->paused)
          status                = "PAUSED";
       if (core_info)
          system_id             = core_info->system_id;
       if (!system_id)
          system_id             = runloop_st->system.info.library_name;

       snprintf(reply, sizeof(reply), "GET_STATUS %s %s,%s,crc32=%x\n", status, system_id, content_name, content_crc32);
   }

   cmd->replier(cmd, reply, strlen(reply));

   return true;
}

bool command_read_memory(command_t *cmd, const char *arg)
{
   unsigned i;
   char* reply                       = NULL;
   char* reply_at                    = NULL;
   const uint8_t* data               = NULL;
   unsigned int nbytes               = 0;
   unsigned int alloc_size           = 0;
   unsigned int address              = -1;
   size_t len                        = 0;
   unsigned int max_bytes            = 0;
   runloop_state_t *runloop_st       = runloop_state_get_ptr();
   const rarch_system_info_t* system = &runloop_st->system;

   if (sscanf(arg, "%x %u", &address, &nbytes) != 2)
      return false;

   /* Ensure large enough to return all requested bytes or an error message */
   alloc_size = 64 + nbytes * 3;
   reply      = (char*)malloc(alloc_size);
   reply_at   = reply + snprintf(reply, alloc_size - 1, "READ_CORE_MEMORY %x", address);

   data       = command_memory_get_pointer(system, address, &max_bytes, 0, reply_at, alloc_size - strlen(reply));

   if (data)
   {
      if (nbytes > max_bytes)
          nbytes = max_bytes;

      for (i = 0; i < nbytes; i++)
         snprintf(reply_at + 3 * i, 4, " %02X", data[i]);

      reply_at[3 * nbytes] = '\n';
      len                  = reply_at + 3 * nbytes + 1 - reply;
   }
   else
      len                  = strlen(reply);

   cmd->replier(cmd, reply, len);
   free(reply);
   return true;
}

bool command_write_memory(command_t *cmd, const char *arg)
{
   unsigned int address         = (unsigned int)strtoul(arg, (char**)&arg, 16);
   unsigned int max_bytes       = 0;
   char reply[128]              = "";
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   const rarch_system_info_t
      *system                   = &runloop_st->system;
   char *reply_at               = reply + snprintf(reply, sizeof(reply) - 1, "WRITE_CORE_MEMORY %x", address);
   uint8_t *data                = command_memory_get_pointer(system, address, &max_bytes, 1, reply_at, sizeof(reply) - strlen(reply) - 1);

   if (data)
   {
      uint8_t* start = data;
      while (*arg && max_bytes > 0)
      {
         --max_bytes;
         *data = strtoul(arg, (char**)&arg, 16);
         data++;
      }

      snprintf(reply_at, sizeof(reply) - strlen(reply) - 1,
            " %u\n", (unsigned)(data - start));

#ifdef HAVE_CHEEVOS
      if (rcheevos_hardcore_active())
      {
         RARCH_LOG("[Command]: Achievements hardcore mode disabled by WRITE_CORE_MEMORY.\n");
         rcheevos_pause_hardcore();
      }
#endif
   }

   cmd->replier(cmd, reply, strlen(reply));
   return true;
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
   msg[_len  ]      = ':';
   msg[++_len]      = ' ';
   msg[++_len]      = '\0';
   _len            += snprintf(msg + _len, sizeof(msg) - _len, "%.1f",
         new_volume);
   msg[_len  ]      = ' ';
   msg[++_len]      = 'd';
   msg[++_len]      = 'B';
   msg[++_len]      = '\0';

#if defined(HAVE_GFX_WIDGETS)
   if (widgets_active)
      gfx_widget_volume_update_and_show(new_volume,
            audio_driver_mute_enable);
   else
#endif
      runloop_msg_queue_push(msg, 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   RARCH_LOG("[Audio]: %s\n", msg);

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
   msg[_len  ]      = ':';
   msg[++_len]      = ' ';
   msg[++_len]      = '\0';
   _len            += snprintf(msg + _len, sizeof(msg) - _len, "%.1f",
         new_volume);
   msg[_len  ]      = ' ';
   msg[++_len]      = 'd';
   msg[++_len]      = 'B';
   msg[++_len]      = '\0';
   runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   RARCH_LOG("[Audio]: %s\n", msg);

   audio_set_float(AUDIO_ACTION_VOLUME_GAIN, new_volume);
}

void command_event_init_controllers(rarch_system_info_t *info,
      settings_t *settings, unsigned num_active_users)
{
   unsigned port;
   unsigned num_core_ports = info->ports.size;

   for (port = 0; port < num_core_ports; port++)
   {
      unsigned i;
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
            &info->ports.data[port], device);

      if (desc && !desc->desc)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */
         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix device,
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("[Input]: Input device ID %u is unknown to this "
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
bool command_event_save_config(
      const char *config_path,
      char *s, size_t len)
{
   bool path_exists = !string_is_empty(config_path);
   const char *str  = path_exists ? config_path :
      path_get(RARCH_PATH_CONFIG);

   if (path_exists && config_save_file(config_path))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_SAVED_NEW_CONFIG_TO),
            config_path);
      RARCH_LOG("[Config]: %s\n", s);
      return true;
   }

   if (!string_is_empty(str))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_SAVING_CONFIG_TO),
            str);
      RARCH_ERR("[Config]: %s\n", s);
   }

   return false;
}
#endif

void command_event_undo_save_state(char *s, size_t len)
{
   if (content_undo_save_buf_is_empty())
   {
      strlcpy(s,
         msg_hash_to_str(MSG_NO_SAVE_STATE_HAS_BEEN_OVERWRITTEN_YET), len);
      return;
   }

   if (!content_undo_save_state())
   {
      strlcpy(s,
         msg_hash_to_str(MSG_FAILED_TO_UNDO_SAVE_STATE), len);
      return;
   }

   strlcpy(s,
         msg_hash_to_str(MSG_UNDOING_SAVE_STATE), len);
}

void command_event_undo_load_state(char *s, size_t len)
{

   if (content_undo_load_buf_is_empty())
   {
      strlcpy(s,
         msg_hash_to_str(MSG_NO_STATE_HAS_BEEN_LOADED_YET),
         len);
      return;
   }

   if (!content_undo_load_state())
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_UNDO_LOAD_STATE),
            "RAM");
      return;
   }

#ifdef HAVE_NETWORKING
   netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, NULL);
#endif

   strlcpy(s,
         msg_hash_to_str(MSG_UNDID_LOAD_STATE), len);
}

bool command_event_resize_windowed_scale(settings_t *settings,
      unsigned window_scale)
{
   unsigned                idx = 0;
   bool      video_fullscreen  = settings->bools.video_fullscreen;

   if (window_scale == 0)
      return false;

   configuration_set_uint(settings, settings->uints.video_scale, window_scale);

   if (!video_fullscreen)
      command_event(CMD_EVENT_REINIT, NULL);

   retroarch_ctl(RARCH_CTL_SET_WINDOWED_SCALE, &idx);

   return true;
}

bool command_event_save_auto_state(
      bool savestate_auto_save,
      const enum rarch_core_type current_core_type)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   char savestate_name_auto[PATH_MAX_LENGTH];

   if (runloop_st->entry_state_slot)
      return false;
   if (!savestate_auto_save)
      return false;
   if (current_core_type == CORE_TYPE_DUMMY)
      return false;
   if (!core_info_current_supports_savestate())
      return false;
   if (string_is_empty(path_basename(path_get(RARCH_PATH_BASENAME))))
      return false;

   strlcpy(savestate_name_auto,
         runloop_st->name.savestate,
         sizeof(savestate_name_auto));
   strlcat(savestate_name_auto,
         ".auto",
         sizeof(savestate_name_auto));

   if (content_save_state((const char*)savestate_name_auto, true, true))
	   RARCH_LOG("%s \"%s\" %s.\n",
			   msg_hash_to_str(MSG_AUTO_SAVE_STATE_TO),
			   savestate_name_auto, "succeeded");
   else
	   RARCH_LOG("%s \"%s\" %s.\n",
			   msg_hash_to_str(MSG_AUTO_SAVE_STATE_TO),
			   savestate_name_auto, "failed");

   return true;
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
   bsv_movie_t *
	  bsv_movie_state_handle      = (bsv_movie_t*)bsv_movie_data;
   allow_cheats                 &= !(bsv_movie_state_handle != NULL);
#endif

   if (!allow_cheats)
      return;

   cheat_manager_alloc_if_empty();
   cheat_manager_load_game_specific_cheats(path_cheat_db);

   if (apply_cheats_after_load)
      cheat_manager_apply_cheats();
}
#endif

bool command_event_load_entry_state(void)
{
   char entry_state_path[PATH_MAX_LENGTH];
   int entry_path_stats;
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   bool ret                        = false;

   if (!core_info_current_supports_savestate())
      return false;

#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      return false;
#endif
#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
      return false;
#endif

   entry_state_path[0] = '\0';

   if (!retroarch_get_entry_state_path(
            entry_state_path, sizeof(entry_state_path),
            runloop_st->entry_state_slot))
      return false;

   entry_path_stats = path_stat(entry_state_path);

   if ((entry_path_stats & RETRO_VFS_STAT_IS_VALID) == 0
         || (entry_path_stats & RETRO_VFS_STAT_IS_DIRECTORY) != 0)
      return false;

   ret = content_load_state(entry_state_path, false, true);

   RARCH_LOG("[State]: %s \"%s\".\n",
         msg_hash_to_str(MSG_FOUND_ENTRY_STATE_IN),
         entry_state_path);
   RARCH_LOG("[State]: %s \"%s\" %s.\n",
         msg_hash_to_str(MSG_LOADING_ENTRY_STATE_FROM),
         entry_state_path, ret ? "succeeded" : "failed"
         );

   return ret;
}

void command_event_load_auto_state(void)
{
   char savestate_name_auto[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st     = runloop_state_get_ptr();

   if (!core_info_current_supports_savestate())
      return;

#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      return;
#endif
#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
      return;
#endif

   strlcpy(savestate_name_auto,
         runloop_st->name.savestate,
         sizeof(savestate_name_auto));
   strlcat(savestate_name_auto,
         ".auto",
         sizeof(savestate_name_auto));

   if (!path_is_valid(savestate_name_auto))
      return;

   RARCH_LOG("[State]: %s \"%s\".\n",
         msg_hash_to_str(MSG_FOUND_AUTO_SAVESTATE_IN),
         savestate_name_auto);

   if ((content_load_state(savestate_name_auto, false, true)))
      RARCH_LOG("[State]: %s \"%s\" %s.\n",
            msg_hash_to_str(MSG_AUTOLOADING_SAVESTATE_FROM),
            savestate_name_auto, "succeeded");
   else
      RARCH_LOG("[State]: %s \"%s\" %s.\n",
            msg_hash_to_str(MSG_AUTOLOADING_SAVESTATE_FROM),
            savestate_name_auto, "failed");
}

void command_event_set_savestate_auto_index(settings_t *settings)
{
   size_t i;
   char state_base[128];
   char state_dir[PATH_MAX_LENGTH];

   struct string_list *dir_list      = NULL;
   unsigned max_idx                  = 0;
   runloop_state_t *runloop_st       = runloop_state_get_ptr();
   bool savestate_auto_index         = settings->bools.savestate_auto_index;
   bool show_hidden_files            = settings->bools.show_hidden_files;

   if (!savestate_auto_index)
      return;

   /* Find the file in the same directory as runloop_st->savestate_name
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.state, will try to find
    * /foo/path/content.state%d, where %d is the largest number available.
    */
   fill_pathname_basedir(state_dir, runloop_st->name.savestate,
         sizeof(state_dir));

   dir_list = dir_list_new_special(state_dir, DIR_LIST_PLAIN, NULL,
         show_hidden_files);

   if (!dir_list)
      return;

   fill_pathname_base(state_base, runloop_st->name.savestate,
         sizeof(state_base));

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      char elem_base[128]             = {0};
      const char *end                 = NULL;
      const char *dir_elem            = dir_list->elems[i].data;

      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      if (strstr(elem_base, state_base) != elem_base)
         continue;

      end = dir_elem + strlen(dir_elem);
      while ((end > dir_elem) && ISDIGIT((int)end[-1]))
         end--;

      idx = (unsigned)strtoul(end, NULL, 0);
      if (idx > max_idx)
         max_idx = idx;
   }

   dir_list_free(dir_list);

   configuration_set_int(settings, settings->ints.state_slot, max_idx);

   RARCH_LOG("[State]: %s: #%d\n",
         msg_hash_to_str(MSG_FOUND_LAST_STATE_SLOT),
         max_idx);
}

void command_event_set_savestate_garbage_collect(
      unsigned max_to_keep,
      bool show_hidden_files
      )
{
   size_t i, cnt = 0;
   char state_dir[PATH_MAX_LENGTH];
   char state_base[128];
   runloop_state_t *runloop_st       = runloop_state_get_ptr();

   struct string_list *dir_list      = NULL;
   unsigned min_idx                  = UINT_MAX;
   const char *oldest_save           = NULL;

   /* Similar to command_event_set_savestate_auto_index(),
    * this will find the lowest numbered save-state */
   fill_pathname_basedir(state_dir, runloop_st->name.savestate,
         sizeof(state_dir));

   dir_list = dir_list_new_special(state_dir, DIR_LIST_PLAIN, NULL,
         show_hidden_files);

   if (!dir_list)
      return;

   fill_pathname_base(state_base, runloop_st->name.savestate,
         sizeof(state_base));

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      char elem_base[128];
      const char *ext                 = NULL;
      const char *end                 = NULL;
      const char *dir_elem            = dir_list->elems[i].data;

      if (string_is_empty(dir_elem))
         continue;

      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      /* Only consider files with a '.state' extension
       * > i.e. Ignore '.state.auto', '.state.bak', etc. */
      ext = path_get_extension(elem_base);
      if (string_is_empty(ext) ||
          !string_starts_with_size(ext, "state", STRLEN_CONST("state")))
         continue;

      /* Check whether this file is associated with
       * the current content */
      if (!string_starts_with(elem_base, state_base))
         continue;

      /* This looks like a valid save */
      cnt++;

      /* > Get index */
      end = dir_elem + strlen(dir_elem);
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
   if (!string_is_empty(oldest_save) && (cnt > max_to_keep))
      filestream_delete(oldest_save);

   dir_list_free(dir_list);
}

#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
bool command_set_shader(command_t *cmd, const char *arg)
{
   enum  rarch_shader_type type = video_shader_parse_type(arg);
   settings_t  *settings        = config_get_ptr();

   if (!string_is_empty(arg))
   {
      if (!video_shader_is_supported(type))
         return false;

      /* rebase on shader directory */
      if (!path_is_absolute(arg))
      {
         char abs_arg[PATH_MAX_LENGTH];
         const char *ref_path = settings->paths.directory_video_shader;
         fill_pathname_join_special(abs_arg, ref_path, arg, sizeof(abs_arg));
         return apply_shader(settings, type, abs_arg, true);
      }
   }

   return apply_shader(settings, type, arg, true);
}
#endif

#ifdef HAVE_CONFIGFILE
bool command_event_save_core_config(
      const char *dir_menu_config,
      const char *rarch_path_config)
{
   char msg[128];
   char config_name[255];
   char config_path[PATH_MAX_LENGTH];
   char config_dir[PATH_MAX_LENGTH];
   bool new_path_available         = false;
   bool overrides_active           = false;
   const char *core_path           = NULL;
   runloop_state_t *runloop_st     = runloop_state_get_ptr();

   msg[0]                          = '\0';

   if (!string_is_empty(dir_menu_config))
      strlcpy(config_dir, dir_menu_config, sizeof(config_dir));
   else if (!string_is_empty(rarch_path_config)) /* Fallback */
      fill_pathname_basedir(config_dir, rarch_path_config,
            sizeof(config_dir));

   if (string_is_empty(config_dir))
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET), 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      RARCH_ERR("[Config]: %s\n", msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET));
      return false;
   }

   core_path                       = path_get(RARCH_PATH_CORE);

   /* Infer file name based on libretro core. */
   if (path_is_valid(core_path))
   {
      unsigned i;
      char tmp[PATH_MAX_LENGTH + 8];
      RARCH_LOG("[Config]: %s\n", msg_hash_to_str(MSG_USING_CORE_NAME_FOR_NEW_CONFIG));

      fill_pathname_base(config_name, core_path, sizeof(config_name));
      path_remove_extension(config_name);
      fill_pathname_join_special(config_path, config_dir, config_name,
            sizeof(config_path));

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         size_t _len = strlcpy(tmp, config_path, sizeof(tmp));
         if (i)
            snprintf(tmp + _len, sizeof(tmp) - _len, "-%u", i);
         strlcat(tmp, ".cfg", sizeof(tmp));

         if (!path_is_valid(tmp))
         {
            new_path_available = true;
            break;
         }
      }
   }

   if (!new_path_available)
   {
      /* Fallback to system time... */
      RARCH_WARN("[Config]: %s\n",
            msg_hash_to_str(MSG_CANNOT_INFER_NEW_CONFIG_PATH));
      fill_dated_filename(config_name, ".cfg", sizeof(config_name));
      fill_pathname_join_special(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   if (runloop_st->overrides_active)
   {
      /* Overrides block config file saving,
       * make it appear as overrides weren't enabled
       * for a manual save. */
      runloop_st->overrides_active      = false;
      overrides_active                  = true;
   }

#ifdef HAVE_CONFIGFILE
   command_event_save_config(config_path, msg, sizeof(msg));
#endif

   if (!string_is_empty(msg))
      runloop_msg_queue_push(msg, 1, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   runloop_st->overrides_active = overrides_active;

   return true;
}

void command_event_save_current_config(enum override_type type)
{
   runloop_state_t *runloop_st     = runloop_state_get_ptr();

   switch (type)
   {
      case OVERRIDE_NONE:
         {
            if (path_is_empty(RARCH_PATH_CONFIG))
            {
               char msg[128];
               strlcpy(msg, "[Config]: Config directory not set, cannot save configuration.", sizeof(msg));
               runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
            else
            {
               char msg[256];
               msg[0] = '\0';
               command_event_save_config(path_get(RARCH_PATH_CONFIG), msg, sizeof(msg));
               runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
            }
         }
         break;
      case OVERRIDE_GAME:
      case OVERRIDE_CORE:
      case OVERRIDE_CONTENT_DIR:
         {
            char msg[128];
            if (config_save_overrides(type, &runloop_st->system))
            {
               strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_SAVED_SUCCESSFULLY), sizeof(msg));
               /* set overrides to active so the original config can be
                  restored after closing content */
               runloop_st->overrides_active = true;
            }
            else
               strlcpy(msg, msg_hash_to_str(MSG_OVERRIDES_ERROR_SAVING), sizeof(msg));
            RARCH_LOG("[Overrides]: %s\n", msg);
            runloop_msg_queue_push(msg, 1, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }
         break;
   }
}
#endif

bool command_event_main_state(unsigned cmd)
{
   retro_ctx_size_info_t info;
   char msg[128];
   char state_path[16384];
   settings_t *settings        = config_get_ptr();
   bool savestates_enabled     = core_info_current_supports_savestate();
   bool ret                    = false;
   bool push_msg               = true;

   state_path[0] = msg[0]      = '\0';

   if (savestates_enabled)
   {
      retroarch_get_current_savestate_path(state_path,
            sizeof(state_path));

      core_serialize_size(&info);
      savestates_enabled = (info.size > 0);
   }

   if (savestates_enabled)
   {
      switch (cmd)
      {
         case CMD_EVENT_SAVE_STATE:
         case CMD_EVENT_SAVE_STATE_TO_RAM:
            {
               video_driver_state_t *video_st                 = 
                  video_state_get_ptr();
               bool savestate_auto_index                      =
                     settings->bools.savestate_auto_index;
               unsigned savestate_max_keep                    =
                     settings->uints.savestate_max_keep;
               bool frame_time_counter_reset_after_save_state =
                     settings->bools.frame_time_counter_reset_after_save_state;

               if (cmd == CMD_EVENT_SAVE_STATE)
                  content_save_state(state_path, true, false);
               else
                  content_save_state_to_ram();

               /* Clean up excess savestates if necessary */
               if (savestate_auto_index && (savestate_max_keep > 0))
                  command_event_set_savestate_garbage_collect(
                        settings->uints.savestate_max_keep,
                        settings->bools.show_hidden_files
                        );

               if (frame_time_counter_reset_after_save_state)
                  video_st->frame_time_count = 0;

               ret      = true;
               push_msg = false;
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
#ifdef HAVE_CHEEVOS
                  if (rcheevos_hardcore_active())
                  {
                     rcheevos_pause_hardcore();
                     runloop_msg_queue_push(msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_DISABLED), 0, 180, true, NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  }
#endif
                  ret = true;
#ifdef HAVE_NETWORKING
                  netplay_driver_ctl(RARCH_NETPLAY_CTL_LOAD_SAVESTATE, NULL);
#endif
                  {
                     video_driver_state_t *video_st                 = 
                        video_state_get_ptr();
                     bool frame_time_counter_reset_after_load_state =
                        settings->bools.frame_time_counter_reset_after_load_state;
                     if (frame_time_counter_reset_after_load_state)
                        video_st->frame_time_count = 0;
                  }
               }
            }
            push_msg = false;
            break;
         case CMD_EVENT_UNDO_LOAD_STATE:
            command_event_undo_load_state(msg, sizeof(msg));
            ret = true;
            break;
         case CMD_EVENT_UNDO_SAVE_STATE:
            command_event_undo_save_state(msg, sizeof(msg));
            ret = true;
            break;
      }
   }
   else
      strlcpy(msg, msg_hash_to_str(
               MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES), sizeof(msg));

   if (push_msg)
      runloop_msg_queue_push(msg, 2, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);

   if (!string_is_empty(msg))
      RARCH_LOG("[State]: %s\n", msg);

   return ret;
}

bool command_event_disk_control_append_image(
      const char *path)
{
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   rarch_system_info_t *sys_info  = runloop_st ? (rarch_system_info_t*)&runloop_st->system : NULL;
   if (  !sys_info ||
         !disk_control_append_image(&sys_info->disk_control, path))
      return false;

#ifdef HAVE_THREADS
   if (runloop_st->use_sram)
      autosave_deinit();
#endif

   /* TODO/FIXME: Need to figure out what to do with subsystems case. */
   if (path_is_empty(RARCH_PATH_SUBSYSTEM))
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      path_set(RARCH_PATH_NAMES, path);
      runloop_path_fill_names();
   }

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);

   return true;
}

void command_event_reinit(const int flags)
{
   settings_t *settings           = config_get_ptr();
   input_driver_state_t *input_st = input_state_get_ptr();
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
   const input_device_driver_t 
      *joypad                     = input_st->primary_joypad;
#ifdef HAVE_MFI
   const input_device_driver_t 
      *sec_joypad                 = input_st->secondary_joypad;
#else
   const input_device_driver_t 
      *sec_joypad                 = NULL;
#endif

   video_driver_reinit(flags);
   /* Poll input to avoid possibly stale data to corrupt things. */
   if (  joypad && joypad->poll)
      joypad->poll();
   if (  sec_joypad && sec_joypad->poll)
      sec_joypad->poll();
   if (  input_st->current_driver &&
         input_st->current_driver->poll)
      input_st->current_driver->poll(input_st->current_data);
   command_event(CMD_EVENT_GAME_FOCUS_TOGGLE, &game_focus_cmd);

#ifdef HAVE_MENU
   p_disp->framebuf_dirty = true;
   if (video_fullscreen)
      video_driver_hide_mouse();
   if (     menu_st->alive 
         && video_st->current_video->set_nonblock_state)
      video_st->current_video->set_nonblock_state(
            video_st->data, false,
            video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
            adaptive_vsync,
            runloop_get_video_swap_interval(swap_interval_config));
#endif
}

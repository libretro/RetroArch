/*  RetroArch - A frontend for libretro.
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

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <compat/posix_string.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>

#ifdef HAVE_COMMAND
#include <net/net_compat.h>
#include <net/net_socket.h>
#include <string/stdstring.h>
#endif

#include "command.h"

#include "defaults.h"
#include "frontend/frontend_driver.h"
#include "audio/audio_driver.h"
#include "record/record_driver.h"
#include "autosave.h"
#include "core_info.h"
#include "core_type.h"
#include "performance.h"
#include "dynamic.h"
#include "content.h"
#include "movie.h"
#include "general.h"
#include "screenshot.h"
#include "msg_hash.h"
#include "retroarch.h"
#include "managers/cheat_manager.h"
#include "managers/state_manager.h"
#include "system.h"
#include "ui/ui_companion_driver.h"
#include "list_special.h"

#ifdef HAVE_CHEEVOS
#include "cheevos.h"
#endif

#include "core.h"
#include "verbosity.h"
#include "runloop.h"
#include "configuration.h"
#include "input/input_remapping.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#include "menu/menu_display.h"
#include "menu/menu_shader.h"
#endif

#ifdef HAVE_NETPLAY
#include "network/netplay.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#endif

#ifdef HAVE_COMMAND
#define DEFAULT_NETWORK_CMD_PORT 55355
#define STDIN_BUF_SIZE           4096

#define COMMAND_EXT_GLSL         0x7c976537U
#define COMMAND_EXT_GLSLP        0x0f840c87U
#define COMMAND_EXT_CG           0x0059776fU
#define COMMAND_EXT_CGP          0x0b8865bfU
#define COMMAND_EXT_SLANG        0x105ce63aU
#define COMMAND_EXT_SLANGP       0x1bf9adeaU

struct command
{
#ifdef HAVE_STDIN_CMD
   bool stdin_enable;
   char stdin_buf[STDIN_BUF_SIZE];
   size_t stdin_buf_ptr;
#endif

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
   int net_fd;
#endif

   bool state[RARCH_BIND_LIST_END];
};

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

static bool cmd_set_shader(const char *arg);

static const struct cmd_action_map action_map[] = {
   { "SET_SHADER", cmd_set_shader, "<shader path>" },
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
   { "MUTE",                   RARCH_MUTE },
   { "OSK",                   RARCH_OSK },
   { "NETPLAY_FLIP",           RARCH_NETPLAY_FLIP },
   { "SLOWMOTION",             RARCH_SLOWMOTION },
   { "VOLUME_UP",              RARCH_VOLUME_UP },
   { "VOLUME_DOWN",            RARCH_VOLUME_DOWN },
   { "OVERLAY_NEXT",           RARCH_OVERLAY_NEXT },
   { "DISK_EJECT_TOGGLE",      RARCH_DISK_EJECT_TOGGLE },
   { "DISK_NEXT",              RARCH_DISK_NEXT },
   { "DISK_PREV",              RARCH_DISK_PREV },
   { "GRAB_MOUSE_TOGGLE",      RARCH_GRAB_MOUSE_TOGGLE },
   { "MENU_TOGGLE",            RARCH_MENU_TOGGLE },
   { "MENU_UP",                RETRO_DEVICE_ID_JOYPAD_UP },
   { "MENU_DOWN",              RETRO_DEVICE_ID_JOYPAD_DOWN },
   { "MENU_LEFT",              RETRO_DEVICE_ID_JOYPAD_LEFT },
   { "MENU_RIGHT",             RETRO_DEVICE_ID_JOYPAD_RIGHT },
   { "MENU_A",                 RETRO_DEVICE_ID_JOYPAD_A },
   { "MENU_B",                 RETRO_DEVICE_ID_JOYPAD_B },
};

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
static bool cmd_init_network(command_t *handle, uint16_t port)
{
   int fd;
   struct addrinfo *res  = NULL;

   RARCH_LOG("Bringing up command interface on port %hu.\n",
         (unsigned short)port);

   fd = socket_init((void**)&res, port, NULL, SOCKET_TYPE_DATAGRAM);

   if (fd < 0)
      goto error;

   handle->net_fd = fd;

   if (!socket_nonblock(handle->net_fd))
      goto error;

   if (!socket_bind(handle->net_fd, (void*)res))
   {
      RARCH_ERR("Failed to bind socket.\n");
      goto error;
   }

   freeaddrinfo_retro(res);
   return true;

error:
   if (res)
      freeaddrinfo_retro(res);
   return false;
}
#endif

#ifdef HAVE_STDIN_CMD
static bool cmd_init_stdin(command_t *handle)
{
#ifndef _WIN32
#ifdef HAVE_NETPLAY
   if (!socket_nonblock(STDIN_FILENO))
      return false;
#endif
#endif

   handle->stdin_enable = true;
   return true;
}
#endif

command_t *command_new(bool stdin_enable,
      bool network_enable, uint16_t port)
{
   command_t *handle = (command_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   (void)network_enable;
   (void)port;
   (void)stdin_enable;

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
   handle->net_fd = -1;
   if (network_enable && !cmd_init_network(handle, port))
      goto error;
#endif

#ifdef HAVE_STDIN_CMD
   handle->stdin_enable = stdin_enable;
   if (stdin_enable && !cmd_init_stdin(handle))
      goto error;
#endif

   return handle;

#if (defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)) || defined(HAVE_STDIN_CMD)
error:
   command_free(handle);
   return NULL;
#endif
}

static bool cmd_set_shader(const char *arg)
{
   char msg[256];
   enum rarch_shader_type type = RARCH_SHADER_NONE;
   const char             *ext = path_get_extension(arg);
   uint32_t ext_hash           = msg_hash_calculate(ext);

   switch (ext_hash)
   {
      case COMMAND_EXT_GLSL:
      case COMMAND_EXT_GLSLP:
         type = RARCH_SHADER_GLSL;
         break;
      case COMMAND_EXT_CG:
      case COMMAND_EXT_CGP:
         type = RARCH_SHADER_CG;
         break;
      case COMMAND_EXT_SLANG:
      case COMMAND_EXT_SLANGP:
         type = RARCH_SHADER_SLANG;
         break;
      default:
         return false;
   }

   snprintf(msg, sizeof(msg), "Shader: \"%s\"", arg);
   runloop_msg_queue_push(msg, 1, 120, true);
   RARCH_LOG("%s \"%s\".\n",
         msg_hash_to_str(MSG_APPLYING_SHADER),
         arg);

   return video_driver_set_shader(type, arg);
}

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

static void parse_sub_msg(command_t *handle, const char *tok)
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
      RARCH_WARN("%s \"%s\" %s.\n",
            msg_hash_to_str(MSG_UNRECOGNIZED_COMMAND),
            tok,
            msg_hash_to_str(MSG_RECEIVED));
}

static void parse_msg(command_t *handle, char *buf)
{
   char *save      = NULL;
   const char *tok = strtok_r(buf, "\n", &save);

   while (tok)
   {
      parse_sub_msg(handle, tok);
      tok = strtok_r(NULL, "\n", &save);
   }
}

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
static void network_cmd_poll(command_t *handle)
{
   fd_set fds;
   struct timeval tmp_tv = {0};

   if (handle->net_fd < 0)
      return;

   FD_ZERO(&fds);
   FD_SET(handle->net_fd, &fds);

   if (socket_select(handle->net_fd + 1, &fds, NULL, NULL, &tmp_tv) <= 0)
      return;

   if (!FD_ISSET(handle->net_fd, &fds))
      return;

   for (;;)
   {
      char buf[1024];
      ssize_t ret = recvfrom(handle->net_fd, buf,
            sizeof(buf) - 1, 0, NULL, NULL);

      if (ret <= 0)
         break;

      buf[ret] = '\0';
      parse_msg(handle, buf);
   }
}
#endif

#ifdef HAVE_STDIN_CMD

#ifdef _WIN32
static size_t read_stdin(char *buf, size_t size)
{
   DWORD i;
   DWORD has_read = 0;
   DWORD avail    = 0;
   bool echo      = false;
   HANDLE hnd     = GetStdHandle(STD_INPUT_HANDLE);

   if (hnd == INVALID_HANDLE_VALUE)
      return 0;

   /* Check first if we're a pipe
    * (not console). */

   /* If not a pipe, check if we're running in a console. */
   if (!PeekNamedPipe(hnd, NULL, 0, NULL, &avail, NULL))
   {
      INPUT_RECORD recs[256];
      bool has_key = false;
      DWORD mode = 0, has_read = 0;

      if (!GetConsoleMode(hnd, &mode))
         return 0;

      if ((mode & (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT))
            && !SetConsoleMode(hnd,
               mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT)))
         return 0;

      /* Win32, Y U NO SANE NONBLOCK READ!? */
      if (!PeekConsoleInput(hnd, recs,
               sizeof(recs) / sizeof(recs[0]), &has_read))
         return 0;

      for (i = 0; i < has_read; i++)
      {
         /* Very crude, but should get the job done. */
         if (recs[i].EventType == KEY_EVENT &&
               recs[i].Event.KeyEvent.bKeyDown &&
               (isgraph(recs[i].Event.KeyEvent.wVirtualKeyCode) ||
                recs[i].Event.KeyEvent.wVirtualKeyCode == VK_RETURN))
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

   if (!ReadFile(hnd, buf, avail, &has_read, NULL))
      return 0;

   for (i = 0; i < has_read; i++)
      if (buf[i] == '\r')
         buf[i] = '\n';

   /* Console won't echo for us while in non-line mode,
    * so do it manually ... */
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

static void stdin_cmd_poll(command_t *handle)
{
   char *last_newline;
   ssize_t ret;
   ptrdiff_t msg_len;

   if (!handle->stdin_enable)
      return;

   ret = read_stdin(handle->stdin_buf + handle->stdin_buf_ptr,
         STDIN_BUF_SIZE - handle->stdin_buf_ptr - 1);
   if (ret == 0)
      return;

   handle->stdin_buf_ptr += ret;
   handle->stdin_buf[handle->stdin_buf_ptr] = '\0';

   last_newline = strrchr(handle->stdin_buf, '\n');

   if (!last_newline)
   {
      /* We're receiving bogus data in pipe
       * (no terminating newline), flush out the buffer. */
      if (handle->stdin_buf_ptr + 1 >= STDIN_BUF_SIZE)
      {
         handle->stdin_buf_ptr = 0;
         handle->stdin_buf[0] = '\0';
      }

      return;
   }

   *last_newline++ = '\0';
   msg_len = last_newline - handle->stdin_buf;

   parse_msg(handle, handle->stdin_buf);

   memmove(handle->stdin_buf, last_newline,
         handle->stdin_buf_ptr - msg_len);
   handle->stdin_buf_ptr -= msg_len;
}
#endif

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
static bool send_udp_packet(const char *host,
      uint16_t port, const char *msg)
{
   char port_buf[16]           = {0};
   struct addrinfo hints       = {0};
   struct addrinfo *res        = NULL;
   const struct addrinfo *tmp  = NULL;
   int fd                      = -1;
   bool ret                    = true;

   hints.ai_socktype = SOCK_DGRAM;

   snprintf(port_buf, sizeof(port_buf), "%hu", (unsigned short)port);
   if (getaddrinfo_retro(host, port_buf, &hints, &res) < 0)
      return false;

   /* Send to all possible targets.
    * "localhost" might resolve to several different IPs. */
   tmp = (const struct addrinfo*)res;
   while (tmp)
   {
      ssize_t len, ret_len;

      fd = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol);
      if (fd < 0)
      {
         ret = false;
         goto end;
      }

      len     = strlen(msg);
      ret_len = sendto(fd, msg, len, 0, tmp->ai_addr, tmp->ai_addrlen);

      if (ret_len < len)
      {
         ret = false;
         goto end;
      }

      socket_close(fd);
      fd = -1;
      tmp = tmp->ai_next;
   }

end:
   freeaddrinfo_retro(res);
   if (fd >= 0)
      socket_close(fd);
   return ret;
}

static bool verify_command(const char *cmd)
{
   unsigned i;

   if (command_get_arg(cmd, NULL, NULL))
      return true;

   RARCH_ERR("Command \"%s\" is not recognized by the program.\n", cmd);
   RARCH_ERR("\tValid commands:\n");
   for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
      RARCH_ERR("\t\t%s\n", map[i].str);

   for (i = 0; i < sizeof(action_map) / sizeof(action_map[0]); i++)
      RARCH_ERR("\t\t%s %s\n", action_map[i].str, action_map[i].arg_desc);

   return false;
}

bool command_send(const char *cmd_)
{
   bool ret;
   char *command       = NULL;
   char *save          = NULL;
   const char *cmd     = NULL;
   const char *host    = NULL;
   const char *port_   = NULL;
   uint16_t port       = DEFAULT_NETWORK_CMD_PORT;

   if (!network_init())
      return false;

   if (!(command = strdup(cmd_)))
      return false;

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

   RARCH_LOG("%s: \"%s\" to %s:%hu\n",
         msg_hash_to_str(MSG_SENDING_COMMAND),
         cmd, host, (unsigned short)port);

   ret = verify_command(cmd) && send_udp_packet(host, port, cmd);
   free(command);

   return ret;
}
#endif

bool command_poll(command_t *handle)
{
   memset(handle->state, 0, sizeof(handle->state));

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
   network_cmd_poll(handle);
#endif

#ifdef HAVE_STDIN_CMD
   stdin_cmd_poll(handle);
#endif

   return true;
}

bool command_set(command_handle_t *handle)
{
   if (!handle || !handle->handle)
      return false;
   if (handle->id < RARCH_BIND_LIST_END)
      handle->handle->state[handle->id] = true;
   return true;
}

bool command_free(command_t *handle)
{
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
   if (handle && handle->net_fd >= 0)
      socket_close(handle->net_fd);
#endif

   free(handle);

   return true;
}

bool command_get(command_handle_t *handle)
{
   if (!handle || !handle->handle)
      return false;
   return handle->id < RARCH_BIND_LIST_END 
      && handle->handle->state[handle->id];
}
#endif

/**
 * command_event_disk_control_set_eject:
 * @new_state            : Eject or close the virtual drive tray.
 *                         false (0) : Close
 *                         true  (1) : Eject
 * @print_log            : Show message onscreen.
 *
 * Ejects/closes of the virtual drive tray.
 **/
static void command_event_disk_control_set_eject(bool new_state, bool print_log)
{
   char msg[128]                                     = {0};
   bool error                                        = false;
   rarch_system_info_t *info                         = NULL;
   const struct retro_disk_control_callback *control = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   if (info)
      control = (const struct retro_disk_control_callback*)&info->disk_control_cb;

   if (!control || !control->get_num_images)
      return;

   if (control->set_eject_state(new_state))
      snprintf(msg, sizeof(msg), "%s %s",
            new_state ? "Ejected" : "Closed",
            msg_hash_to_str(MSG_VIRTUAL_DISK_TRAY));
   else
   {
      error = true;
      snprintf(msg, sizeof(msg), "%s %s %s",
            msg_hash_to_str(MSG_FAILED_TO),
            new_state ? "eject" : "close",
            msg_hash_to_str(MSG_VIRTUAL_DISK_TRAY));
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      /* Only noise in menu. */
      if (print_log)
         runloop_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * command_event_disk_control_set_index:
 * @idx                : Index of disk to set as current.
 *
 * Sets current disk to @index.
 **/
static void command_event_disk_control_set_index(unsigned idx)
{
   unsigned num_disks;
   bool error                                        = false;
   char msg[128]                                     = {0};
   rarch_system_info_t                      *info    = NULL;
   const struct retro_disk_control_callback *control = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   if (info)
      control = (const struct retro_disk_control_callback*)&info->disk_control_cb;

   if (!control || !control->get_num_images)
      return;

   num_disks = control->get_num_images();

   if (control->set_image_index(idx))
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Setting disk %u of %u in tray.",
               idx + 1, num_disks);
      else
         strlcpy(msg,
               msg_hash_to_str(MSG_REMOVED_DISK_FROM_TRAY),
               sizeof(msg));
   }
   else
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Failed to set disk %u of %u.",
               idx + 1, num_disks);
      else
         strlcpy(msg,
               msg_hash_to_str(MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY),
               sizeof(msg));
      error = true;
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * command_event_disk_control_append_image:
 * @path                 : Path to disk image.
 *
 * Appends disk image to disk image list.
 **/
static bool command_event_disk_control_append_image(const char *path)
{
   unsigned new_idx;
   char msg[128]                                      = {0};
   struct retro_game_info info                        = {0};
   global_t                                  *global  = global_get_ptr();
   const struct retro_disk_control_callback *control  = NULL;
   rarch_system_info_t                       *sysinfo = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &sysinfo);

   if (sysinfo)
      control = (const struct retro_disk_control_callback*)
         &sysinfo->disk_control_cb;

   if (!control)
      return false;

   command_event_disk_control_set_eject(true, false);

   control->add_image_index();
   new_idx = control->get_num_images();
   if (!new_idx)
      return false;
   new_idx--;

   info.path = path;
   control->replace_image_index(new_idx, &info);

   snprintf(msg, sizeof(msg), "%s: ", msg_hash_to_str(MSG_APPENDED_DISK));
   strlcat(msg, path, sizeof(msg));
   RARCH_LOG("%s\n", msg);
   runloop_msg_queue_push(msg, 0, 180, true);

   command_event(CMD_EVENT_AUTOSAVE_DEINIT, NULL);

   /* TODO: Need to figure out what to do with subsystems case. */
   if (!*global->subsystem)
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      rarch_ctl(RARCH_CTL_SET_PATHS, (void*)path);
      rarch_ctl(RARCH_CTL_FILL_PATHNAMES, NULL);
   }

   command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);
   command_event_disk_control_set_index(new_idx);
   command_event_disk_control_set_eject(false, false);

   return true;
}

/**
 * command_event_check_disk_prev:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to previous index action (Core Disk Options).
 **/
static void command_event_check_disk_prev(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks    = 0;
   unsigned current      = 0;
   bool disk_prev_enable = false;

   if (!control || !control->get_num_images)
      return;
   if (!control->get_image_index)
      return;

   num_disks        = control->get_num_images();
   current          = control->get_image_index();
   disk_prev_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_prev_enable)
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_GOT_INVALID_DISK_INDEX));
      return;
   }

   if (current > 0)
      current--;
   command_event_disk_control_set_index(current);
}

/**
 * command_event_check_disk_next:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to next index action (Core Disk Options).
 **/
static void command_event_check_disk_next(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks        = 0;
   unsigned current          = 0;
   bool     disk_next_enable = false;

   if (!control || !control->get_num_images)
      return;
   if (!control->get_image_index)
      return;

   num_disks        = control->get_num_images();
   current          = control->get_image_index();
   disk_next_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_next_enable)
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_GOT_INVALID_DISK_INDEX));
      return;
   }

   if (current < num_disks - 1)
      current++;
   command_event_disk_control_set_index(current);
}

/**
 * event_set_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
static void command_event_set_volume(float gain)
{
   char msg[128];
   settings_t *settings      = config_get_ptr();

   settings->audio.volume += gain;
   settings->audio.volume  = MAX(settings->audio.volume, -80.0f);
   settings->audio.volume  = MIN(settings->audio.volume, 12.0f);

   snprintf(msg, sizeof(msg), "Volume: %.1f dB", settings->audio.volume);
   runloop_msg_queue_push(msg, 1, 180, true);
   RARCH_LOG("%s\n", msg);

   audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));
}

/**
 * command_event_init_controllers:
 *
 * Initialize libretro controllers.
 **/
static void command_event_init_controllers(void)
{
   unsigned i;
   settings_t      *settings = config_get_ptr();
   rarch_system_info_t *info = NULL;
   
   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   for (i = 0; i < MAX_USERS; i++)
   {
      retro_ctx_controller_info_t pad;
      const char *ident   = NULL;
      bool set_controller = false;
      const struct retro_controller_description *desc = NULL;
      unsigned device = settings->input.libretro_device[i];

      if (i < info->ports.size)
         desc = libretro_find_controller_description(
               &info->ports.data[i], device);

      if (desc)
         ident = desc->desc;

      if (!ident)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */

         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix settings->input.libretro_device[i],
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("Input device ID %u is unknown to this "
                  "libretro implementation. Using RETRO_DEVICE_JOYPAD.\n",
                  device);
            device = RETRO_DEVICE_JOYPAD;
         }
         ident = "Joypad";
      }

      switch (device)
      {
         case RETRO_DEVICE_NONE:
            RARCH_LOG("Disconnecting device from port %u.\n", i + 1);
            set_controller = true;
            break;
         case RETRO_DEVICE_JOYPAD:
            break;
         default:
            /* Some cores do not properly range check port argument.
             * This is broken behavior of course, but avoid breaking
             * cores needlessly. */
            RARCH_LOG("Connecting %s (ID: %u) to port %u.\n", ident,
                  device, i + 1);
            set_controller = true;
            break;
      }

      if (set_controller)
      {
         pad.device     = device;
         pad.port       = i;
         core_set_controller_port_device(&pad);
      }
   }
}

static void command_event_deinit_core(bool reinit)
{
#ifdef HAVE_CHEEVOS
   cheevos_unload();
#endif

   core_unload_game();
   core_unload();

   if (reinit)
   {
      int flags = DRIVERS_CMD_ALL;
      driver_ctl(RARCH_DRIVER_CTL_UNINIT, &flags);
   }

   /* auto overrides: reload the original config */
   if (runloop_ctl(RUNLOOP_CTL_IS_OVERRIDES_ACTIVE, NULL))
   {
      config_unload_override();
      runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);
   }
}

static void command_event_init_cheats(void)
{
   bool allow_cheats = true;
#ifdef HAVE_NETPLAY
   allow_cheats &= !netplay_driver_ctl(
         RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL);
#endif
   allow_cheats &= !bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL);

   if (!allow_cheats)
      return;

   /* TODO/FIXME - add some stuff here. */
}

static bool event_load_save_files(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   if (!global)
      return false;
   if (!global->savefiles || global->sram.load_disable)
      return false;

   for (i = 0; i < global->savefiles->size; i++)
   {
      ram_type_t ram;
      ram.path = global->savefiles->elems[i].data;
      ram.type = global->savefiles->elems[i].attr.i;

      content_load_ram_file(&ram);
   }

   return true;
}

static void command_event_load_auto_state(void)
{
   bool ret;
   char msg[128]                             = {0};
   char savestate_name_auto[PATH_MAX_LENGTH] = {0};
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

#ifdef HAVE_NETPLAY
   if (global->netplay.enable && !global->netplay.is_spectate)
      return;
#endif

#ifdef HAVE_CHEEVOS
   if (settings->cheevos.hardcore_mode_enable)
      return;
#endif

   if (!settings->savestate_auto_load)
      return;

   fill_pathname_noext(savestate_name_auto, global->name.savestate,
         ".auto", sizeof(savestate_name_auto));

   if (!path_file_exists(savestate_name_auto))
      return;

   ret = content_load_state(savestate_name_auto);

   RARCH_LOG("Found auto savestate in: %s\n", savestate_name_auto);

   snprintf(msg, sizeof(msg), "Auto-loading savestate from \"%s\" %s.",
         savestate_name_auto, ret ? "succeeded" : "failed");
   runloop_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);
}

static void command_event_set_savestate_auto_index(void)
{
   size_t i;
   char state_dir[PATH_MAX_LENGTH]  = {0};
   char state_base[PATH_MAX_LENGTH] = {0};
   struct string_list *dir_list     = NULL;
   unsigned max_idx                 = 0;
   settings_t *settings             = config_get_ptr();
   global_t   *global               = global_get_ptr();

   if (!settings->savestate_auto_index)
      return;

   /* Find the file in the same directory as global->savestate_name
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.state, will try to find
    * /foo/path/content.state%d, where %d is the largest number available.
    */

   fill_pathname_basedir(state_dir, global->name.savestate,
         sizeof(state_dir));
   fill_pathname_base(state_base, global->name.savestate,
         sizeof(state_base));

   if (!(dir_list = dir_list_new_special(state_dir, DIR_LIST_PLAIN, NULL)))
      return;

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
      while ((end > dir_elem) && isdigit((int)end[-1]))
         end--;

      idx = strtoul(end, NULL, 0);
      if (idx > max_idx)
         max_idx = idx;
   }

   dir_list_free(dir_list);

   settings->state_slot = max_idx;
   RARCH_LOG("Found last state slot: #%d\n", settings->state_slot);
}

static bool event_init_content(void)
{
   rarch_ctl(RARCH_CTL_SET_SRAM_ENABLE, NULL);

   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
      return true;

   if (!content_does_not_need_content())
      rarch_ctl(RARCH_CTL_FILL_PATHNAMES, NULL);

   if (!content_init())
      return false;

   if (content_does_not_need_content())
      return true;

   command_event_set_savestate_auto_index();

   if (event_load_save_files())
      RARCH_LOG("%s.\n",
            msg_hash_to_str(MSG_SKIPPING_SRAM_LOAD));

   command_event_load_auto_state();
   command_event(CMD_EVENT_BSV_MOVIE_INIT, NULL);
   command_event(CMD_EVENT_NETPLAY_INIT, NULL);

   return true;
}

static bool event_init_core(enum rarch_core_type *data)
{
   retro_ctx_environ_info_t info;
   settings_t *settings            = config_get_ptr();

   if (!core_init_symbols(data))
      return false;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_INIT, NULL);

   /* auto overrides: apply overrides */
   if(settings->auto_overrides_enable)
   {
      if (config_load_override())
         runloop_ctl(RUNLOOP_CTL_SET_OVERRIDES_ACTIVE, NULL);
      else
         runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);
   }

   /* reset video format to libretro's default */
   video_driver_set_pixel_format(RETRO_PIXEL_FORMAT_0RGB1555);

   info.env = rarch_environment_cb;
   core_set_environment(&info);

   /* Auto-remap: apply remap files */
   if(settings->auto_remaps_enable)
      config_load_remap();

   /* Per-core saves: reset redirection paths */
   rarch_ctl(RARCH_CTL_SET_PATHS_REDIRECT, NULL);

   if (!core_init())
      return false;

   if (!event_init_content())
      return false;

   if (!core_load())
      return false;

   return true;
}

static bool event_save_auto_state(void)
{
   bool ret;
   char savestate_name_auto[PATH_MAX_LENGTH] = {0};
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (!settings->savestate_auto_save)
      return false;
   if (rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
      return false;
   if (content_does_not_need_content())
      return false;

#ifdef HAVE_CHEEVOS
   if (settings->cheevos.hardcore_mode_enable)
      return false;
#endif

   fill_pathname_noext(savestate_name_auto, global->name.savestate,
         ".auto", sizeof(savestate_name_auto));

   ret = content_save_state((const char*)savestate_name_auto);
   RARCH_LOG("Auto save state to \"%s\" %s.\n", savestate_name_auto, ret ?
         "succeeded" : "failed");

   return true;
}

/**
 * command_event_save_core_config:
 *
 * Saves a new (core) configuration to a file. Filename is based
 * on heuristics to avoid typing.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool command_event_save_core_config(void)
{
   char config_dir[PATH_MAX_LENGTH]  = {0};
   char config_name[PATH_MAX_LENGTH] = {0};
   char config_path[PATH_MAX_LENGTH] = {0};
   char msg[128]                     = {0};
   bool ret                          = false;
   bool found_path                   = false;
   bool overrides_active             = false;
   settings_t *settings              = config_get_ptr();
   global_t   *global                = global_get_ptr();

   *config_dir = '\0';
   if (!string_is_empty(settings->directory.menu_config))
      strlcpy(config_dir, settings->directory.menu_config,
            sizeof(config_dir));
   else if (!string_is_empty(global->path.config)) /* Fallback */
      fill_pathname_basedir(config_dir, global->path.config,
            sizeof(config_dir));
   else
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET), 1, 180, true);
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET));
      return false;
   }

   /* Infer file name based on libretro core. */
   if (!string_is_empty(settings->path.libretro) 
   && path_file_exists(settings->path.libretro))
   {
      unsigned i;
      RARCH_LOG("Using core name for new config\n");
      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64];

         fill_pathname_base(
               config_name,
               settings->path.libretro,
               sizeof(config_name));

         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name,
               sizeof(config_path));
         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
            strlcpy(tmp, ".cfg", sizeof(tmp));

         strlcat(config_path, tmp, sizeof(config_path));
         if (!path_file_exists(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   /* Fallback to system time... */
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   /* Overrides block config file saving, make it appear as overrides 
    * weren't enabled for a manual save */
   if (runloop_ctl(RUNLOOP_CTL_IS_OVERRIDES_ACTIVE, NULL))
   {
      runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);
      overrides_active = true;
   }

   if ((ret = config_save_file(config_path)))
   {
      strlcpy(global->path.config, config_path,
            sizeof(global->path.config));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".",
            config_path);
      RARCH_LOG("%s\n", msg);
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".",
            config_path);
      RARCH_ERR("%s\n", msg);
   }

   runloop_msg_queue_push(msg, 1, 180, true);

   if (overrides_active)
      runloop_ctl(RUNLOOP_CTL_SET_OVERRIDES_ACTIVE, NULL);
   else
      runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);
   return ret;
}

/**
 * event_save_current_config:
 *
 * Saves current configuration file to disk, and (optionally)
 * autosave state.
 **/
void command_event_save_current_config(void)
{
   char msg[128];
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   bool ret             = false;

   if (settings->config_save_on_exit && !string_is_empty(global->path.config))
   {
      /* Save last core-specific config to the default config location,
       * needed on consoles for core switching and reusing last good
       * config for new cores.
       */

      /* Flush out the core specific config. */
      if (*global->path.core_specific_config &&
            settings->core_specific_config)
         ret = config_save_file(global->path.core_specific_config);
      else
         ret = config_save_file(global->path.config);
      if (ret)
      {
         snprintf(msg, sizeof(msg), "Saved new config to \"%s\".",
               global->path.config);
         RARCH_LOG("%s\n", msg);
      }
      else
      {
         snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".",
               global->path.config);
         RARCH_ERR("%s\n", msg);
      }

      runloop_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * event_save_state
 * @path            : Path to state.
 * @s               : Message.
 * @len             : Size of @s.
 *
 * Saves a state with path being @path.
 **/
static void command_event_save_state(const char *path,
      char *s, size_t len)
{
   settings_t *settings = config_get_ptr();

   if (!content_save_state(path))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO),
            path);
      return;
   }

   if (settings->state_slot < 0)
      snprintf(s, len, "%s #-1 (auto).",
            msg_hash_to_str(MSG_SAVED_STATE_TO_SLOT));
   else
      snprintf(s, len, "%s #%d.", msg_hash_to_str(MSG_SAVED_STATE_TO_SLOT),
            settings->state_slot);
}

/**
 * event_load_state
 * @path            : Path to state.
 * @s               : Message.
 * @len             : Size of @s.
 *
 * Loads a state with path being @path.
 **/
static void command_event_load_state(const char *path, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();

   if (!content_load_state(path))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE),
            path);
      return;
   }

   if (settings->state_slot < 0)
      snprintf(s, len, "%s #-1 (auto).",
            msg_hash_to_str(MSG_LOADED_STATE_FROM_SLOT));
   else
      snprintf(s, len, "%s #%d.", msg_hash_to_str(MSG_LOADED_STATE_FROM_SLOT),
            settings->state_slot);
}

static void command_event_main_state(unsigned cmd)
{
   retro_ctx_size_info_t info;
   char path[PATH_MAX_LENGTH] = {0};
   char msg[128]              = {0};
   global_t *global           = global_get_ptr();
   settings_t *settings       = config_get_ptr();

   if (settings->state_slot > 0)
      snprintf(path, sizeof(path), "%s%d",
            global->name.savestate, settings->state_slot);
   else if (settings->state_slot < 0)
      fill_pathname_join_delim(path,
            global->name.savestate, "auto", '.', sizeof(path));
   else
      strlcpy(path, global->name.savestate, sizeof(path));

   core_serialize_size(&info);

   if (info.size)
   {
      switch (cmd)
      {
         case CMD_EVENT_SAVE_STATE:
            command_event_save_state(path, msg, sizeof(msg));
            break;
         case CMD_EVENT_LOAD_STATE:
            command_event_load_state(path, msg, sizeof(msg));
            break;
      }
   }
   else
      strlcpy(msg, msg_hash_to_str(
               MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES), sizeof(msg));

   runloop_msg_queue_push(msg, 2, 180, true);
   RARCH_LOG("%s\n", msg);
}

static bool command_event_cmd_exec(void *data)
{
   char *fullpath = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (fullpath != data)
   {
      runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);
      if (data)
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, data);
   }

#if defined(HAVE_DYNAMIC)
   if (!rarch_ctl(RARCH_CTL_LOAD_CONTENT, NULL))
      return false;
#else
   frontend_driver_set_fork(FRONTEND_FORK_CORE_WITH_ARGS);
#endif

   return true;
}

/**
 * command_event:
 * @cmd                  : Event command index.
 *
 * Performs program event command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool command_event(enum event_command cmd, void *data)
{
   unsigned i                = 0;
   bool boolean              = false;
   settings_t *settings      = config_get_ptr();
   rarch_system_info_t *info = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   (void)i;

   switch (cmd)
   {
      case CMD_EVENT_MENU_REFRESH:
#ifdef HAVE_MENU
         menu_driver_ctl(RARCH_MENU_CTL_REFRESH, NULL);
#endif
         break;
      case CMD_EVENT_SET_PER_GAME_RESOLUTION:
#if defined(GEKKO)
         {
            unsigned width = 0, height = 0;

            command_event(CMD_EVENT_VIDEO_SET_ASPECT_RATIO, NULL);

            if (video_driver_get_video_output_size(&width, &height))
            {
               char msg[128] = {0};

               video_driver_set_video_mode(width, height, true);

               if (width == 0 || height == 0)
                  strlcpy(msg, "Resolution: DEFAULT", sizeof(msg));
               else
                  snprintf(msg, sizeof(msg),"Resolution: %dx%d",width, height);
               runloop_msg_queue_push(msg, 1, 100, true);
            }
         }
#endif
         break;
      case CMD_EVENT_LOAD_CONTENT_PERSIST:
#ifdef HAVE_DYNAMIC
         command_event(CMD_EVENT_LOAD_CORE, NULL);
#endif
         rarch_ctl(RARCH_CTL_LOAD_CONTENT, NULL);
         break;
#ifdef HAVE_FFMPEG
      case CMD_EVENT_LOAD_CONTENT_FFMPEG:
         rarch_ctl(RARCH_CTL_LOAD_CONTENT_FFMPEG, NULL);
         break;
#endif
      case CMD_EVENT_LOAD_CONTENT_IMAGEVIEWER:
         rarch_ctl(RARCH_CTL_LOAD_CONTENT_IMAGEVIEWER, NULL);
         break;
      case CMD_EVENT_LOAD_CONTENT:
         {
#ifdef HAVE_DYNAMIC
            command_event(CMD_EVENT_LOAD_CONTENT_PERSIST, NULL);
#else
            char *fullpath = NULL;
            runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
            runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, settings->path.libretro);
            command_event(CMD_EVENT_EXEC, (void*)fullpath);
            command_event(CMD_EVENT_QUIT, NULL);
#endif
         }
         break;
      case CMD_EVENT_LOAD_CORE_DEINIT:
#ifdef HAVE_MENU
         menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_DEINIT, NULL);
#endif
         break;
      case CMD_EVENT_LOAD_CORE_PERSIST:
         command_event(CMD_EVENT_LOAD_CORE_DEINIT, NULL);
         {
#ifdef HAVE_MENU
            bool *ptr = NULL;
            struct retro_system_info *system = NULL;

            menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET, &system);

            if (menu_driver_ctl(RARCH_MENU_CTL_LOAD_NO_CONTENT_GET, &ptr))
            {
               core_info_ctx_find_t info_find;

#if defined(HAVE_DYNAMIC)
               if (!(*settings->path.libretro))
                  return false;

               libretro_get_system_info(
                     settings->path.libretro,
                     system,
                     ptr);
#else
               libretro_get_system_info_static(system, ptr);
#endif
               info_find.path = settings->path.libretro;

               if (!core_info_load(&info_find))
                  return false;
            }
#endif
         }
         break;
      case CMD_EVENT_LOAD_CORE:
         command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
#ifndef HAVE_DYNAMIC
         command_event(CMD_EVENT_QUIT, NULL);
#endif
         break;
      case CMD_EVENT_LOAD_STATE:
         /* Immutable - disallow savestate load when
          * we absolutely cannot change game state. */
         if (bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
            return false;

#ifdef HAVE_NETPLAY
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
            return false;
#endif

#ifdef HAVE_CHEEVOS
         if (settings->cheevos.hardcore_mode_enable)
            return false;
#endif

         command_event_main_state(cmd);
         break;
      case CMD_EVENT_RESIZE_WINDOWED_SCALE:
         {
            unsigned idx = 0;
            unsigned *window_scale = NULL;

            runloop_ctl(RUNLOOP_CTL_GET_WINDOWED_SCALE, &window_scale);

            if (*window_scale == 0)
               return false;

            settings->video.scale = *window_scale;

            if (!settings->video.fullscreen)
               command_event(CMD_EVENT_REINIT, NULL);

            runloop_ctl(RUNLOOP_CTL_SET_WINDOWED_SCALE, &idx);
         }
         break;
      case CMD_EVENT_MENU_TOGGLE:
#ifdef HAVE_MENU
         if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
            rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
         else
            rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
#endif
         break;
      case CMD_EVENT_CONTROLLERS_INIT:
         command_event_init_controllers();
         break;
      case CMD_EVENT_RESET:
         RARCH_LOG("%s.\n", msg_hash_to_str(MSG_RESET));
         runloop_msg_queue_push(msg_hash_to_str(MSG_RESET), 1, 120, true);

#ifdef HAVE_CHEEVOS
         cheevos_set_cheats();
#endif
         core_reset();
         break;
      case CMD_EVENT_SAVE_STATE:
#ifdef HAVE_CHEEVOS
         if (settings->cheevos.hardcore_mode_enable)
            return false;
#endif

         if (settings->savestate_auto_index)
            settings->state_slot++;

         command_event_main_state(cmd);
         break;
      case CMD_EVENT_SAVE_STATE_DECREMENT:
         /* Slot -1 is (auto) slot. */
         if (settings->state_slot >= 0)
            settings->state_slot--;
         break;
      case CMD_EVENT_SAVE_STATE_INCREMENT:
         settings->state_slot++;
         break;
      case CMD_EVENT_TAKE_SCREENSHOT:
         if (!take_screenshot())
            return false;
         break;
      case CMD_EVENT_UNLOAD_CORE:
         runloop_ctl(RUNLOOP_CTL_PREPARE_DUMMY, NULL);
         command_event(CMD_EVENT_LOAD_CORE_DEINIT, NULL);
         break;
      case CMD_EVENT_QUIT:
         rarch_ctl(RARCH_CTL_QUIT, NULL);
         break;
      case CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE:
#ifdef HAVE_CHEEVOS
         cheevos_toggle_hardcore_mode();
#endif
         break;
      case CMD_EVENT_REINIT:
         {
            struct retro_hw_render_callback *hwr =
               video_driver_get_hw_context();

            if (hwr->cache_context)
               video_driver_set_video_cache_context();
            else
               video_driver_unset_video_cache_context();

            video_driver_unset_video_cache_context_ack();
            command_event(CMD_EVENT_RESET_CONTEXT, NULL);
            video_driver_unset_video_cache_context();

            /* Poll input to avoid possibly stale data to corrupt things. */
            input_driver_poll();

#ifdef HAVE_MENU
            menu_display_set_framebuffer_dirty_flag();
            if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
               command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);
#endif
         }
         break;
      case CMD_EVENT_CHEATS_DEINIT:
         cheat_manager_state_free();
         break;
      case CMD_EVENT_CHEATS_INIT:
         command_event(CMD_EVENT_CHEATS_DEINIT, NULL);
         command_event_init_cheats();
         break;
      case CMD_EVENT_CHEATS_APPLY:
         cheat_manager_apply_cheats();
         break;
      case CMD_EVENT_REWIND_DEINIT:
#ifdef HAVE_NETPLAY
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
            return false;
#endif
#ifdef HAVE_CHEEVOS
         if (settings->cheevos.hardcore_mode_enable)
            return false;
#endif

         state_manager_event_deinit();
         break;
      case CMD_EVENT_REWIND_INIT:
#ifdef HAVE_CHEEVOS
         if (settings->cheevos.hardcore_mode_enable)
            return false;
#endif
#ifdef HAVE_NETPLAY
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
#endif
            state_manager_event_init();
         break;
      case CMD_EVENT_REWIND_TOGGLE:
         if (settings->rewind_enable)
            command_event(CMD_EVENT_REWIND_INIT, NULL);
         else
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
         break;
      case CMD_EVENT_AUTOSAVE_DEINIT:
#ifdef HAVE_THREADS
         {
            global_t  *global         = global_get_ptr();
            if (!global->sram.use)
               return false;
            autosave_deinit();
         }
#endif
         break;
      case CMD_EVENT_AUTOSAVE_INIT:
         command_event(CMD_EVENT_AUTOSAVE_DEINIT, NULL);
#ifdef HAVE_THREADS
         autosave_init();
#endif
         break;
      case CMD_EVENT_AUTOSAVE_STATE:
         event_save_auto_state();
         break;
      case CMD_EVENT_AUDIO_STOP:
         if (!audio_driver_alive())
            return false;

         if (!audio_driver_stop())
            return false;
         break;
      case CMD_EVENT_AUDIO_START:
         if (audio_driver_alive())
            return false;

         if (!settings->audio.mute_enable && !audio_driver_start())
         {
            RARCH_ERR("Failed to start audio driver. "
                  "Will continue without audio.\n");
            audio_driver_unset_active();
         }
         break;
      case CMD_EVENT_AUDIO_MUTE_TOGGLE:
         {
            const char *msg = !settings->audio.mute_enable ?
               msg_hash_to_str(MSG_AUDIO_MUTED):
               msg_hash_to_str(MSG_AUDIO_UNMUTED);

            if (!audio_driver_toggle_mute())
            {
               RARCH_ERR("%s.\n",
                     msg_hash_to_str(MSG_FAILED_TO_UNMUTE_AUDIO));
               return false;
            }

            runloop_msg_queue_push(msg, 1, 180, true);
            RARCH_LOG("%s\n", msg);
         }
         break;
      case CMD_EVENT_OVERLAY_DEINIT:
#ifdef HAVE_OVERLAY
         input_overlay_free();
#endif
         break;
      case CMD_EVENT_OVERLAY_INIT:
         command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
#ifdef HAVE_OVERLAY
         input_overlay_init();
#endif
         break;
      case CMD_EVENT_OVERLAY_NEXT:
#ifdef HAVE_OVERLAY
         input_overlay_next(settings->input.overlay_opacity);
#endif
         break;
      case CMD_EVENT_DSP_FILTER_DEINIT:
         audio_driver_dsp_filter_free();
         break;
      case CMD_EVENT_DSP_FILTER_INIT:
         command_event(CMD_EVENT_DSP_FILTER_DEINIT, NULL);
         if (!*settings->path.audio_dsp_plugin)
            break;
         audio_driver_dsp_filter_init(settings->path.audio_dsp_plugin);
         break;
      case CMD_EVENT_GPU_RECORD_DEINIT:
         video_driver_gpu_record_deinit();
         break;
      case CMD_EVENT_RECORD_DEINIT:
         if (!recording_deinit())
            return false;
         break;
      case CMD_EVENT_RECORD_INIT:
         command_event(CMD_EVENT_HISTORY_DEINIT, NULL);
         if (!recording_init())
            return false;
         break;
      case CMD_EVENT_HISTORY_DEINIT:
         if (g_defaults.history)
         {
            content_playlist_write_file(g_defaults.history);
            content_playlist_free(g_defaults.history);
         }
         g_defaults.history = NULL;
         break;
      case CMD_EVENT_HISTORY_INIT:
         command_event(CMD_EVENT_HISTORY_DEINIT, NULL);
         if (!settings->history_list_enable)
            return false;
         RARCH_LOG("%s: [%s].\n",
               msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
               settings->path.content_history);
         g_defaults.history = content_playlist_init(
               settings->path.content_history,
               settings->content_history_size);
         break;
      case CMD_EVENT_CORE_INFO_DEINIT:
         core_info_deinit_list();
         break;
      case CMD_EVENT_CORE_INFO_INIT:
         command_event(CMD_EVENT_CORE_INFO_DEINIT, NULL);

         if (*settings->directory.libretro)
            core_info_init_list();
         break;
      case CMD_EVENT_CORE_DEINIT:
         {
            struct retro_hw_render_callback *hwr =
               video_driver_get_hw_context();
            command_event_deinit_core(true);

            if (hwr)
               memset(hwr, 0, sizeof(*hwr));

            break;
         }
      case CMD_EVENT_CORE_INIT:
         if (!event_init_core((enum rarch_core_type*)data))
            return false;
         break;
      case CMD_EVENT_VIDEO_APPLY_STATE_CHANGES:
         video_driver_apply_state_changes();
         break;
      case CMD_EVENT_VIDEO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case CMD_EVENT_VIDEO_SET_BLOCKING_STATE:
         video_driver_set_nonblock_state(boolean);
         break;
      case CMD_EVENT_VIDEO_SET_ASPECT_RATIO:
         video_driver_set_aspect_ratio();
         break;
      case CMD_EVENT_AUDIO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case CMD_EVENT_AUDIO_SET_BLOCKING_STATE:
         audio_driver_set_nonblocking_state(boolean);
         break;
      case CMD_EVENT_OVERLAY_SET_SCALE_FACTOR:
#ifdef HAVE_OVERLAY
         input_overlay_set_scale_factor(settings->input.overlay_scale);
#endif
         break;
      case CMD_EVENT_OVERLAY_SET_ALPHA_MOD:
#ifdef HAVE_OVERLAY
         input_overlay_set_alpha_mod(settings->input.overlay_opacity);
#endif
         break;
      case CMD_EVENT_AUDIO_REINIT:
         {
            int flags = DRIVER_AUDIO;
            driver_ctl(RARCH_DRIVER_CTL_UNINIT, &flags);
            driver_ctl(RARCH_DRIVER_CTL_INIT, &flags);
         }
         break;
      case CMD_EVENT_RESET_CONTEXT:
         {
            /* RARCH_DRIVER_CTL_UNINIT clears the callback struct so we
             * need to make sure to keep a copy */
            struct retro_hw_render_callback *hwr = NULL;
            struct retro_hw_render_callback hwr_copy;
            int flags = DRIVERS_CMD_ALL;

            hwr = video_driver_get_hw_context();
            memcpy(&hwr_copy, hwr, sizeof(hwr_copy));

            driver_ctl(RARCH_DRIVER_CTL_UNINIT, &flags);

            memcpy(hwr, &hwr_copy, sizeof(*hwr));

            driver_ctl(RARCH_DRIVER_CTL_INIT, &flags);
         }
         break;
      case CMD_EVENT_QUIT_RETROARCH:
         rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
         break;
      case CMD_EVENT_SHUTDOWN:
#if defined(__linux__) && !defined(ANDROID)
         runloop_msg_queue_push("Shutting down...", 1, 180, true);
         rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
         system("shutdown -P now");
#endif
         break;
      case CMD_EVENT_REBOOT:
#if defined(__linux__) && !defined(ANDROID)
         runloop_msg_queue_push("Rebooting...", 1, 180, true);
         rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
         system("shutdown -r now");
#endif
         break;
      case CMD_EVENT_RESUME:
         rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
         if (ui_companion_is_on_foreground())
            ui_companion_driver_toggle();
         break;
      case CMD_EVENT_RESTART_RETROARCH:
         if (!frontend_driver_set_fork(FRONTEND_FORK_RESTART))
            return false;
         break;
      case CMD_EVENT_MENU_SAVE_CURRENT_CONFIG:
         command_event_save_current_config();
         break;
      case CMD_EVENT_MENU_SAVE_CONFIG:
         if (!command_event_save_core_config())
            return false;
         break;
      case CMD_EVENT_SHADERS_APPLY_CHANGES:
#ifdef HAVE_MENU
         menu_shader_manager_apply_changes();
#endif
         break;
      case CMD_EVENT_PAUSE_CHECKS:
         if (runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL))
         {
            RARCH_LOG("%s\n", msg_hash_to_str(MSG_PAUSED));
            command_event(CMD_EVENT_AUDIO_STOP, NULL);

            if (settings->video.black_frame_insertion)
               video_driver_cached_frame_render();
         }
         else
         {
            RARCH_LOG("%s\n", msg_hash_to_str(MSG_UNPAUSED));
            command_event(CMD_EVENT_AUDIO_START, NULL);
         }
         break;
      case CMD_EVENT_PAUSE_TOGGLE:
         boolean = runloop_ctl(RUNLOOP_CTL_IS_PAUSED,  NULL);
         boolean = !boolean;
         runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
         command_event(CMD_EVENT_PAUSE_CHECKS, NULL);
         break;
      case CMD_EVENT_UNPAUSE:
         boolean = false;

         runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
         command_event(CMD_EVENT_PAUSE_CHECKS, NULL);
         break;
      case CMD_EVENT_PAUSE:
         boolean = true;

         runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
         command_event(CMD_EVENT_PAUSE_CHECKS, NULL);
         break;
      case CMD_EVENT_MENU_PAUSE_LIBRETRO:
#ifdef HAVE_MENU
         if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
         {
            if (settings->menu.pause_libretro)
               command_event(CMD_EVENT_AUDIO_STOP, NULL);
            else
               command_event(CMD_EVENT_AUDIO_START, NULL);
         }
         else
         {
            if (settings->menu.pause_libretro)
               command_event(CMD_EVENT_AUDIO_START, NULL);
         }
#endif
         break;
      case CMD_EVENT_SHADER_DIR_DEINIT:
         runloop_ctl(RUNLOOP_CTL_SHADER_DIR_DEINIT, NULL);
         break;
      case CMD_EVENT_SHADER_DIR_INIT:
         command_event(CMD_EVENT_SHADER_DIR_DEINIT, NULL);

         if (!runloop_ctl(RUNLOOP_CTL_SHADER_DIR_INIT, NULL))
            return false;
         break;
      case CMD_EVENT_SAVEFILES:
         {
            global_t  *global         = global_get_ptr();
            if (!global->savefiles || !global->sram.use)
               return false;

            for (i = 0; i < global->savefiles->size; i++)
            {
               ram_type_t ram;
               ram.type    = global->savefiles->elems[i].attr.i;
               ram.path    = global->savefiles->elems[i].data;

               RARCH_LOG("%s #%u %s \"%s\".\n",
                     msg_hash_to_str(MSG_SAVING_RAM_TYPE),
                     ram.type,
                     msg_hash_to_str(MSG_TO),
                     ram.path);
               content_save_ram_file(&ram);
            }
         }
         return true;
      case CMD_EVENT_SAVEFILES_DEINIT:
         {
            global_t  *global         = global_get_ptr();
            if (!global)
               break;

            if (global->savefiles)
               string_list_free(global->savefiles);
            global->savefiles = NULL;
         }
         break;
      case CMD_EVENT_SAVEFILES_INIT:
         {
            global_t  *global         = global_get_ptr();
            global->sram.use = global->sram.use && !global->sram.save_disable;
#ifdef HAVE_NETPLAY
            global->sram.use = global->sram.use && 
               (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL)
                || !global->netplay.is_client);
#endif

            if (!global->sram.use)
               RARCH_LOG("%s\n",
                     msg_hash_to_str(MSG_SRAM_WILL_NOT_BE_SAVED));

            if (global->sram.use)
               command_event(CMD_EVENT_AUTOSAVE_INIT, NULL);
         }
         break;
      case CMD_EVENT_BSV_MOVIE_DEINIT:
         bsv_movie_ctl(BSV_MOVIE_CTL_DEINIT, NULL);
         break;
      case CMD_EVENT_BSV_MOVIE_INIT:
         command_event(CMD_EVENT_BSV_MOVIE_DEINIT, NULL);
         bsv_movie_ctl(BSV_MOVIE_CTL_INIT, NULL);
         break;
      case CMD_EVENT_NETPLAY_DEINIT:
#ifdef HAVE_NETPLAY
         deinit_netplay();
#endif
         break;
      case CMD_EVENT_NETWORK_DEINIT:
#ifdef HAVE_NETWORKING
         network_deinit();
#endif
         break;
      case CMD_EVENT_NETWORK_INIT:
#ifdef HAVE_NETWORKING
         network_init();
#endif
         break;
      case CMD_EVENT_NETPLAY_INIT:
         command_event(CMD_EVENT_NETPLAY_DEINIT, NULL);
#ifdef HAVE_NETPLAY
         if (!init_netplay())
            return false;
#endif
         break;
      case CMD_EVENT_NETPLAY_FLIP_PLAYERS:
#ifdef HAVE_NETPLAY
         netplay_driver_ctl(RARCH_NETPLAY_CTL_FLIP_PLAYERS, NULL);
#endif
         break;
      case CMD_EVENT_FULLSCREEN_TOGGLE:
         if (!video_driver_has_windowed())
            return false;

         /* If we go fullscreen we drop all drivers and
          * reinitialize to be safe. */
         settings->video.fullscreen = !settings->video.fullscreen;
         command_event(CMD_EVENT_REINIT, NULL);
         break;
      case CMD_EVENT_COMMAND_DEINIT:
         input_driver_deinit_command();
         break;
      case CMD_EVENT_COMMAND_INIT:
         command_event(CMD_EVENT_COMMAND_DEINIT, NULL);
         input_driver_init_command();
         break;
      case CMD_EVENT_REMOTE_DEINIT:
         input_driver_deinit_remote();
         break;
      case CMD_EVENT_REMOTE_INIT:
         command_event(CMD_EVENT_REMOTE_DEINIT, NULL);
         input_driver_init_remote();
         break;
      case CMD_EVENT_TEMPORARY_CONTENT_DEINIT:
         content_deinit();
         break;
      case CMD_EVENT_SUBSYSTEM_FULLPATHS_DEINIT:
         {
            global_t  *global         = global_get_ptr();
            if (!global)
               break;

            if (global->subsystem_fullpaths)
               string_list_free(global->subsystem_fullpaths);
            global->subsystem_fullpaths = NULL;
         }
         break;
      case CMD_EVENT_LOG_FILE_DEINIT:
         retro_main_log_file_deinit();
         break;
      case CMD_EVENT_DISK_APPEND_IMAGE:
         {
            const char *path = (const char*)data;
            if (string_is_empty(path))
               return false;
            return command_event_disk_control_append_image(path);
         }
      case CMD_EVENT_DISK_EJECT_TOGGLE:
         if (info && info->disk_control_cb.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control_cb;

            if (control)
            {
               bool new_state = !control->get_eject_state();
               command_event_disk_control_set_eject(new_state, true);
            }
         }
         else
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                  1, 120, true);
         break;
      case CMD_EVENT_DISK_NEXT:
         if (info && info->disk_control_cb.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control_cb;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            command_event_check_disk_next(control);
         }
         else
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                  1, 120, true);
         break;
      case CMD_EVENT_DISK_PREV:
         if (info && info->disk_control_cb.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control_cb;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            command_event_check_disk_prev(control);
         }
         else
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                  1, 120, true);
         break;
      case CMD_EVENT_RUMBLE_STOP:
         for (i = 0; i < MAX_USERS; i++)
         {
            input_driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
            input_driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
         }
         break;
      case CMD_EVENT_GRAB_MOUSE_TOGGLE:
         {
            bool ret = false;
            static bool grab_mouse_state  = false;

            grab_mouse_state = !grab_mouse_state;

            if (grab_mouse_state)
               ret = input_driver_grab_mouse();
            else
               ret = input_driver_ungrab_mouse();

            if (!ret)
               return false;

            RARCH_LOG("%s: %s.\n",
                  msg_hash_to_str(MSG_GRAB_MOUSE_STATE),
                  grab_mouse_state ? "yes" : "no");

            if (grab_mouse_state)
               video_driver_hide_mouse();
            else
               video_driver_show_mouse();
         }
         break;
      case CMD_EVENT_PERFCNT_REPORT_FRONTEND_LOG:
         rarch_perf_log();
         break;
      case CMD_EVENT_VOLUME_UP:
         command_event_set_volume(0.5f);
         break;
      case CMD_EVENT_VOLUME_DOWN:
         command_event_set_volume(-0.5f);
         break;
      case CMD_EVENT_SET_FRAME_LIMIT:
         runloop_ctl(RUNLOOP_CTL_SET_FRAME_LIMIT, NULL);
         break;
      case CMD_EVENT_EXEC:
         return command_event_cmd_exec(data);
      case CMD_EVENT_NONE:
      default:
         return false;
   }

   return true;
}

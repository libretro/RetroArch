/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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
 #include <stdint.h>
 #include <stdlib.h>
 #include <stdarg.h>
 #include <string.h>
 #include <math.h>
 
 #include <winsock2.h>
 #include <windows.h>
 #include <ws2tcpip.h>

 #include <retro_miscellaneous.h>

#include "../../libretro.h"
 
#define SERVER "127.0.0.1"
#define PORT 55400

struct retro_log_callback logger;
retro_log_printf_t log_cb;
static uint16_t *frame_buf;
struct sockaddr_in si_other;
int s, slen=sizeof(si_other);
char message[64];
WSADATA wsa;
int input_state = 0;

void retro_init(void)
{
   frame_buf = (uint16_t*)calloc(320 * 240, sizeof(uint16_t));
   //Initialise winsock
   log_cb(RETRO_LOG_INFO, "Initialising sockets\n");
   if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
   {
      log_cb(RETRO_LOG_INFO, "Failed. Error Code : %d",WSAGetLastError());
   }
   log_cb(RETRO_LOG_INFO, "Sockets initialised.\n");

   //create socket
   if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
   {
      log_cb(RETRO_LOG_INFO, "socket() failed with error code : %d" , WSAGetLastError());
   }
   //setup address structure
   memset((char *) &si_other, 0, sizeof(si_other));
   si_other.sin_family = AF_INET;
   si_other.sin_port = htons(PORT);
   si_other.sin_addr.S_un.S_addr = inet_addr(SERVER);
}

void retro_deinit(void)
{
   if (frame_buf)
      free(frame_buf);
   frame_buf = NULL;
}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(
      unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(
      struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "RetroPad Remote";
   info->library_version  = "0.01";
   info->need_fullpath    = false;
   info->valid_extensions = ""; /* Nothing. */
}

/* Doesn't really matter, but need something sane. */
void retro_get_system_av_info(
      struct retro_system_av_info *info)
{
   info->timing.fps = 60.0;
   info->timing.sample_rate = 30000.0;

   info->geometry.base_width  = 320;
   info->geometry.base_height = 240;
   info->geometry.max_width   = 320;
   info->geometry.max_height  = 240;
   info->geometry.aspect_ratio = 4.0 / 3.0;
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void update_input()
{
   input_state = 0;
   input_poll_cb();
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
      input_state += pow(2, 4);
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN))
      input_state += pow(2, 5);
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT))
      input_state += pow(2, 6);
   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT))
      input_state += pow(2, 7);
}

void retro_set_environment(retro_environment_t cb)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   environ_cb = cb;
   bool no_content = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_content);

   environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);

   if (cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logger))
      log_cb = logger.log;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(
      retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

void retro_reset(void)
{}

void retro_run(void)
{
   unsigned i;
   update_input();
   itoa(input_state, message, 10);
   //send the message
   if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen)==-1)
   {
       log_cb(RETRO_LOG_INFO, "Error sending data");
   }
   for (i = 0; i < 320 * 240; i++)
      frame_buf[i] = 4 << 5;
   video_cb(frame_buf, 320, 240, 640);
}

/* This should never be called, it's only used as a placeholder. */
bool retro_load_game(const struct retro_game_info *info)
{
   (void)info;
   return true;
}

void retro_unload_game(void)
{}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type,
      const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool retro_unserialize(const void *data,
      size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned idx,
      bool enabled, const char *code)
{
   (void)idx;
   (void)enabled;
   (void)code;
}



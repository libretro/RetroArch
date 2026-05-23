/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2024 - RetroArch
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <retro_endianness.h>
#include <retro_timers.h>
#include <features/features_cpu.h>
#include <rthreads/rthreads.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_DSU)

#include <net/net_compat.h>
#include <net/net_socket.h>
#include <string/stdstring.h>

#include "input_dsu.h"
#include "input_driver.h"
#include "../verbosity.h"
#include "../tasks/task_content.h"
#include "../runloop.h"
#include "../file_path_special.h"
#include "../configuration.h"
#include "../record/record_driver.h"
#include "../gfx/video_driver.h"
#include "../playlist.h"
#include "../core_info.h"
#include <lists/dir_list.h>
#include <file/file_path.h>

/* ---- CRC32 (standard, polynomial 0xEDB88320) ---- */

static uint32_t dsu_crc32_table[256];
static bool     dsu_crc32_table_ready = false;

static void dsu_crc32_init_table(void)
{
   unsigned i, j;
   if (dsu_crc32_table_ready)
      return;
   for (i = 0; i < 256; i++)
   {
      uint32_t c = i;
      for (j = 0; j < 8; j++)
      {
         if (c & 1)
            c = DSU_CRC32_POLY ^ (c >> 1);
         else
            c >>= 1;
      }
      dsu_crc32_table[i] = c;
   }
   dsu_crc32_table_ready = true;
}

uint32_t dsu_crc32(const uint8_t *data, size_t len)
{
   size_t i;
   uint32_t crc = 0xFFFFFFFF;
   dsu_crc32_init_table();
   for (i = 0; i < len; i++)
      crc = dsu_crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
   return crc ^ 0xFFFFFFFF;
}

/* ---- Helpers ---- */

static int64_t dsu_get_time_us(void)
{
   retro_time_t t = cpu_features_get_time_usec();
   return (int64_t)t;
}

static uint32_t dsu_generate_id(void)
{
   return (uint32_t)(dsu_get_time_us() & 0xFFFFFFFF);
}

static void dsu_write_header(uint8_t *buf, const char *magic,
      uint16_t payload_len, uint32_t id, uint32_t msg_type)
{
   uint16_t total_after_header = payload_len + 4;
   memcpy(buf + 0, magic, 4);

   /* version - little endian */
   buf[4] = (uint8_t)(DSU_PROTOCOL_VERSION & 0xFF);
   buf[5] = (uint8_t)((DSU_PROTOCOL_VERSION >> 8) & 0xFF);

   /* payload length (bytes after offset 16) */
   buf[6] = (uint8_t)(total_after_header & 0xFF);
   buf[7] = (uint8_t)((total_after_header >> 8) & 0xFF);

   /* CRC32 placeholder */
   buf[8]  = 0;
   buf[9]  = 0;
   buf[10] = 0;
   buf[11] = 0;

   /* ID - little endian */
   buf[12] = (uint8_t)(id & 0xFF);
   buf[13] = (uint8_t)((id >> 8) & 0xFF);
   buf[14] = (uint8_t)((id >> 16) & 0xFF);
   buf[15] = (uint8_t)((id >> 24) & 0xFF);

   /* Message type */
   buf[16] = (uint8_t)(msg_type & 0xFF);
   buf[17] = (uint8_t)((msg_type >> 8) & 0xFF);
   buf[18] = (uint8_t)((msg_type >> 16) & 0xFF);
   buf[19] = (uint8_t)((msg_type >> 24) & 0xFF);
}

static void dsu_finalize_crc(uint8_t *buf, size_t total_len)
{
   uint32_t crc;
   buf[8]  = 0;
   buf[9]  = 0;
   buf[10] = 0;
   buf[11] = 0;
   crc = dsu_crc32(buf, total_len);
   buf[8]  = (uint8_t)(crc & 0xFF);
   buf[9]  = (uint8_t)((crc >> 8) & 0xFF);
   buf[10] = (uint8_t)((crc >> 16) & 0xFF);
   buf[11] = (uint8_t)((crc >> 24) & 0xFF);
}

static bool dsu_validate_header(const uint8_t *buf, size_t len,
      const char *expected_magic)
{
   uint32_t received_crc, computed_crc;
   uint8_t  tmp[DSU_MAX_PACKET_SIZE];

   if (len < DSU_HEADER_SIZE)
      return false;
   if (memcmp(buf, expected_magic, 4) != 0)
      return false;

   /* Check CRC */
   received_crc = (uint32_t)buf[8]
                | ((uint32_t)buf[9]  << 8)
                | ((uint32_t)buf[10] << 16)
                | ((uint32_t)buf[11] << 24);

   if (len > DSU_MAX_PACKET_SIZE)
      return false;

   memcpy(tmp, buf, len);
   tmp[8]  = 0;
   tmp[9]  = 0;
   tmp[10] = 0;
   tmp[11] = 0;
   computed_crc = dsu_crc32(tmp, len);

   return (received_crc == computed_crc);
}

static uint32_t dsu_read_u32_le(const uint8_t *p)
{
   return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static uint16_t dsu_read_u16_le(const uint8_t *p)
{
   return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static float dsu_read_float_le(const uint8_t *p)
{
   union { uint32_t u; float f; } val;
   val.u = dsu_read_u32_le(p);
   return val.f;
}

static void dsu_write_u32_le(uint8_t *p, uint32_t v)
{
   p[0] = (uint8_t)(v & 0xFF);
   p[1] = (uint8_t)((v >> 8) & 0xFF);
   p[2] = (uint8_t)((v >> 16) & 0xFF);
   p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void dsu_write_u16_le(uint8_t *p, uint16_t v)
{
   p[0] = (uint8_t)(v & 0xFF);
   p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static void dsu_write_float_le(uint8_t *p, float v)
{
   union { uint32_t u; float f; } val;
   val.f = v;
   dsu_write_u32_le(p, val.u);
}

/* ---- Build packets (client-side requests) ---- */

size_t dsu_build_version_request(uint8_t *buf, size_t buf_len, uint32_t id)
{
   size_t total = DSU_HEADER_SIZE;
   if (buf_len < total)
      return 0;
   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT, 0, id, DSU_MSG_VERSION);
   dsu_finalize_crc(buf, total);
   return total;
}

size_t dsu_build_controller_info_request(uint8_t *buf, size_t buf_len,
      uint32_t id, int port_count, const uint8_t *slot_indices)
{
   int request_count   = port_count;
   size_t payload_size;
   size_t total;
   int i;

   if (request_count < 0)
      request_count = 0;
   if (request_count > DSU_MAX_CONTROLLERS)
      request_count = DSU_MAX_CONTROLLERS;

   payload_size = 4 + (size_t)request_count;
   total        = DSU_HEADER_SIZE + payload_size;

   if (buf_len < total)
      return 0;
   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT,
         (uint16_t)payload_size, id, DSU_MSG_CONTROLLER);

   /* port count */
   dsu_write_u32_le(buf + DSU_HEADER_SIZE, (uint32_t)request_count);

   /* slot indices */
   for (i = 0; i < request_count; i++)
      buf[DSU_HEADER_SIZE + 4 + i] = slot_indices[i];

   dsu_finalize_crc(buf, total);
   return total;
}

size_t dsu_build_subscribe_request(uint8_t *buf, size_t buf_len,
      uint32_t id, uint8_t flags, uint8_t data_type_flags, uint8_t slot, const uint8_t *mac)
{
   size_t payload_size = mac ? 9 : 3;
   size_t total        = DSU_HEADER_SIZE + payload_size;

   if (buf_len < total)
      return 0;
   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT,
         (uint16_t)payload_size, id, DSU_MSG_DATA);

   buf[DSU_HEADER_SIZE]         = flags;
   buf[DSU_HEADER_SIZE + 1]     = data_type_flags;
   buf[DSU_HEADER_SIZE + 2]     = slot;
   if (mac)
      memcpy(buf + DSU_HEADER_SIZE + 3, mac, 6);

   dsu_finalize_crc(buf, total);
   return total;
}

static size_t dsu_build_rumble_packet(uint8_t *buf, size_t buf_len,
      uint32_t id, uint8_t flags, uint8_t slot, const uint8_t *mac,
      uint8_t motor_id, uint8_t intensity)
{
   size_t payload_size = 10;
   size_t total        = DSU_HEADER_SIZE + payload_size;

   if (buf_len < total)
      return 0;
   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT,
         (uint16_t)payload_size, id, DSU_MSG_RUMBLE);

   buf[DSU_HEADER_SIZE]     = flags;
   buf[DSU_HEADER_SIZE + 1] = slot;
   if (mac)
      memcpy(buf + DSU_HEADER_SIZE + 2, mac, 6);
   buf[DSU_HEADER_SIZE + 8] = motor_id;
   buf[DSU_HEADER_SIZE + 9] = intensity;

   dsu_finalize_crc(buf, total);
   return total;
}

static size_t dsu_build_state_packet(uint8_t *buf, size_t buf_len,
      uint32_t id, const char *game_name, const char *platform_name,
      const char *core_name, uint8_t state_flags)
{
   size_t payload_size = 1 + 64 + 64 + 64;
   size_t total        = DSU_HEADER_SIZE + payload_size;

   if (buf_len < total)
      return 0;

   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT,
         (uint16_t)payload_size, id, DSU_MSG_STATE);

   buf[DSU_HEADER_SIZE] = state_flags;
   if (game_name)
      strlcpy((char*)(buf + DSU_HEADER_SIZE + 1), game_name, 64);
   if (platform_name)
      strlcpy((char*)(buf + DSU_HEADER_SIZE + 65), platform_name, 64);
   if (core_name)
      strlcpy((char*)(buf + DSU_HEADER_SIZE + 129), core_name, 64);

   dsu_finalize_crc(buf, total);
   return total;
}

/* STREAM_STATUS payload layout (280 bytes):
 *   u32 state         (0=STOPPED, 1=ACTIVE, 2=ERROR)
 *   u32 error_code    (0=OK, 1=RECORD_INIT_FAIL, 2=AUX_INIT_FAIL)
 *   u32 stream_type   (0=Twitch,1=YouTube,2=Facebook,3=Local,4=Custom)
 *   u8  screen_id     (0=main stream, 1-4=aux screens)
 *   u8  reserved[3]
 *   char url[256]     (active stream URL, null-terminated)
 *   u16 width          source/base stream width hint
 *   u16 height         source/base stream height hint
 *   f32 aspect_ratio   intended display aspect ratio
 *
 * DSU clients use (url + stream_type) to optionally connect to the
 * video stream itself (e.g. open the Twitch page, or read the local
 * udp:// URL directly for on-device playback). */
#define DSU_STREAM_STATUS_URL_LEN 256
#define DSU_STREAM_STATUS_PAYLOAD (12 + 1 + 3 + DSU_STREAM_STATUS_URL_LEN + 2 + 2 + 4)

static void dsu_get_stream_video_info(uint16_t *width, uint16_t *height,
      float *aspect_ratio)
{
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = video_st ? &video_st->av_info : NULL;
   unsigned w                           = av_info ? av_info->geometry.base_width : 0;
   unsigned h                           = av_info ? av_info->geometry.base_height : 0;
   float aspect                         = av_info ? av_info->geometry.aspect_ratio : 0.0f;

   if (aspect <= 0.0f && w > 0 && h > 0)
      aspect = (float)w / (float)h;

   *width        = (uint16_t)((w > 0xFFFF) ? 0xFFFF : w);
   *height       = (uint16_t)((h > 0xFFFF) ? 0xFFFF : h);
   *aspect_ratio = aspect > 0.0f ? aspect : 1.0f;
}

static size_t dsu_build_stream_status_packet(uint8_t *buf, size_t buf_len,
      uint32_t id, uint32_t state, uint32_t error_code, uint32_t stream_type,
      uint8_t screen_id, const char *url)
{
   size_t payload_size = DSU_STREAM_STATUS_PAYLOAD;
   size_t total        = DSU_HEADER_SIZE + payload_size;

   if (buf_len < total)
      return 0;

   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT,
         (uint16_t)payload_size, id, DSU_MSG_STREAM_STATUS);

   dsu_write_u32_le(buf + DSU_HEADER_SIZE,     state);
   dsu_write_u32_le(buf + DSU_HEADER_SIZE + 4, error_code);
   dsu_write_u32_le(buf + DSU_HEADER_SIZE + 8, stream_type);
   buf[DSU_HEADER_SIZE + 12] = screen_id;
   if (url)
      strlcpy((char*)(buf + DSU_HEADER_SIZE + 16), url, DSU_STREAM_STATUS_URL_LEN);
   {
      uint16_t width;
      uint16_t height;
      float aspect_ratio;
      size_t meta_offset = DSU_HEADER_SIZE + 16 + DSU_STREAM_STATUS_URL_LEN;
      dsu_get_stream_video_info(&width, &height, &aspect_ratio);
      dsu_write_u16_le(buf + meta_offset, width);
      dsu_write_u16_le(buf + meta_offset + 2, height);
      dsu_write_float_le(buf + meta_offset + 4, aspect_ratio);
   }

   dsu_finalize_crc(buf, total);
   return total;
}

/* ---- Playlist packet building ---- */

static size_t dsu_build_playlist_list_packet(uint8_t *buf, size_t buf_len,
      uint32_t id, uint8_t page, uint8_t total_pages, uint8_t count,
      const char **playlist_names, const char **playlist_paths)
{
   size_t payload_size = 3 + (count * (64 + 256));
   size_t total        = DSU_HEADER_SIZE + payload_size;
   size_t offset;
   uint8_t i;

   if (buf_len < total)
      return 0;

   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT,
         (uint16_t)payload_size, id, DSU_MSG_PLAYLIST_LIST);

   buf[DSU_HEADER_SIZE] = page;
   buf[DSU_HEADER_SIZE + 1] = total_pages;
   buf[DSU_HEADER_SIZE + 2] = count;

   offset = DSU_HEADER_SIZE + 3;
   for (i = 0; i < count; i++)
   {
      if (playlist_names && playlist_names[i])
         strlcpy((char*)(buf + offset), playlist_names[i], 64);
      offset += 64;
      if (playlist_paths && playlist_paths[i])
         strlcpy((char*)(buf + offset), playlist_paths[i], 256);
      offset += 256;
   }

   dsu_finalize_crc(buf, total);
   return total;
}

static size_t dsu_build_playlist_contents_packet(uint8_t *buf, size_t buf_len,
      uint32_t id, uint8_t page, uint8_t total_pages, uint8_t count,
      const char **game_labels, const char **game_paths,
      const char **core_paths)
{
   size_t payload_size = 3 + (count * (64 + 256 + 256));
   size_t total        = DSU_HEADER_SIZE + payload_size;
   size_t offset;
   uint8_t i;

   if (buf_len < total)
      return 0;

   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT,
         (uint16_t)payload_size, id, DSU_MSG_PLAYLIST_CONTENTS);

   buf[DSU_HEADER_SIZE] = page;
   buf[DSU_HEADER_SIZE + 1] = total_pages;
   buf[DSU_HEADER_SIZE + 2] = count;

   offset = DSU_HEADER_SIZE + 3;
   for (i = 0; i < count; i++)
   {
      if (game_labels && game_labels[i])
         strlcpy((char*)(buf + offset), game_labels[i], 64);
      offset += 64;
      if (game_paths && game_paths[i])
         strlcpy((char*)(buf + offset), game_paths[i], 256);
      offset += 256;
      if (core_paths && core_paths[i])
         strlcpy((char*)(buf + offset), core_paths[i], 256);
      offset += 256;
   }

   dsu_finalize_crc(buf, total);
   return total;
}

static size_t dsu_build_contentless_core_list_packet(uint8_t *buf, size_t buf_len,
      uint32_t id, uint8_t page, uint8_t total_pages, uint8_t count,
      const char **core_names, const char **core_paths, const char **core_ids)
{
   size_t payload_size = 3 + (count * (64 + 256 + 64));
   size_t total        = DSU_HEADER_SIZE + payload_size;
   size_t offset;
   uint8_t i;

   if (buf_len < total)
      return 0;

   memset(buf, 0, total);
   dsu_write_header(buf, DSU_MAGIC_CLIENT,
         (uint16_t)payload_size, id, DSU_MSG_CONTENTLESS_CORE_LIST);

   buf[DSU_HEADER_SIZE] = page;
   buf[DSU_HEADER_SIZE + 1] = total_pages;
   buf[DSU_HEADER_SIZE + 2] = count;

   offset = DSU_HEADER_SIZE + 3;
   for (i = 0; i < count; i++)
   {
      if (core_names && core_names[i])
         strlcpy((char*)(buf + offset), core_names[i], 64);
      offset += 64;
      if (core_paths && core_paths[i])
         strlcpy((char*)(buf + offset), core_paths[i], 256);
      offset += 256;
      if (core_ids && core_ids[i])
         strlcpy((char*)(buf + offset), core_ids[i], 64);
      offset += 64;
   }

   dsu_finalize_crc(buf, total);
   return total;
}

/* Server-side packet building removed - client mode only */

/* ---- Parse incoming controller data ---- */

bool dsu_parse_controller_data(const uint8_t *payload, size_t len,
      dsu_controller_state_t *out)
{
   const uint8_t *p;
   uint8_t btn1, btn2;

   if (len < 80)
      return false;

   p = payload;

   /* Shared response (11 bytes) */
   out->slot      = p[0];
   out->connected = (p[1] == DSU_CONN_CONNECTED);
   out->model     = p[2];
   out->conn_type = p[3];
   memcpy(out->mac, p + 4, 6);
   out->battery   = p[10];
   p += 11;

   /* Byte 11: connected flag (0=disconnected, 1=connected) */
   if (*p++ == 0)
   {
      out->connected = false;
      return true;
   }

   /* packet number */
   out->packet_number = dsu_read_u32_le(p);
   p += 4;

   /* Digital buttons */
   btn1 = *p++;
   btn2 = *p++;

   out->buttons = 0;
   if (btn1 & DSU_BTN1_DPAD_LEFT)  out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
   if (btn1 & DSU_BTN1_DPAD_DOWN)  out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
   if (btn1 & DSU_BTN1_DPAD_RIGHT) out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
   if (btn1 & DSU_BTN1_DPAD_UP)    out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
   if (btn1 & DSU_BTN1_OPTIONS)    out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_START);
   if (btn1 & DSU_BTN1_R3)         out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_R3);
   if (btn1 & DSU_BTN1_L3)         out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_L3);
   if (btn1 & DSU_BTN1_SHARE)      out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_SELECT);

   if (btn2 & DSU_BTN2_Y)          out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_Y);
   if (btn2 & DSU_BTN2_B)          out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_B);
   if (btn2 & DSU_BTN2_A)          out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_A);
   if (btn2 & DSU_BTN2_X)          out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_X);
   if (btn2 & DSU_BTN2_R1)         out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_R);
   if (btn2 & DSU_BTN2_L1)         out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_L);
   if (btn2 & DSU_BTN2_R2)         out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_R2);
   if (btn2 & DSU_BTN2_L2)         out->buttons |= (1 << RETRO_DEVICE_ID_JOYPAD_L2);

   /* HOME + touch button */
   out->home      = (*p++ != 0);
   out->touch_btn = (*p++ != 0);

   /* Analog sticks: DSU uint8 [0..255] -> RetroArch int16 [-32768..32767] */
   {
      uint8_t lx_raw = *p++;
      uint8_t ly_raw = *p++;
      uint8_t rx_raw = *p++;
      uint8_t ry_raw = *p++;

      out->analog_lx = (int16_t)(((int)lx_raw - 128) * 256);
      out->analog_ly = (int16_t)(((int)ly_raw - 128) * 256);
      out->analog_rx = (int16_t)(((int)rx_raw - 128) * 256);
      out->analog_ry = (int16_t)(((int)ry_raw - 128) * 256);

      RARCH_LOG("[DSU] Analog raw: LX=%u LY=%u RX=%u RY=%u -> converted: LX=%d LY=%d RX=%d RY=%d\n",
         lx_raw, ly_raw, rx_raw, ry_raw,
         out->analog_lx, out->analog_ly, out->analog_rx, out->analog_ry);
   }

   /* Analog buttons (pressure) - skip d-pad pressure */
   p += 4; /* dpad left, down, right, up */
   p += 4; /* Y, B, A, X face buttons */
   p += 2; /* R1, L1 */
   out->r2_analog = *p++;
   out->l2_analog = *p++;

   /* Touch point 1 */
   out->touch1_active = (*p++ != 0);
   out->touch1_id     = *p++;
   out->touch1_x      = (int16_t)dsu_read_u16_le(p); p += 2;
   out->touch1_y      = (int16_t)dsu_read_u16_le(p); p += 2;

   /* Touch point 2 */
   out->touch2_active = (*p++ != 0);
   out->touch2_id     = *p++;
   out->touch2_x      = (int16_t)dsu_read_u16_le(p); p += 2;
   out->touch2_y      = (int16_t)dsu_read_u16_le(p); p += 2;

   /* Motion timestamp (uint64 LE) */
   {
      uint64_t ts = 0;
      int b;
      for (b = 7; b >= 0; b--)
         ts = (ts << 8) | p[b];
      out->motion_timestamp = ts;
      p += 8;
   }

   /* Accelerometer (in g - same unit as RetroArch) */
   out->accel[0] = dsu_read_float_le(p); p += 4;
   out->accel[1] = dsu_read_float_le(p); p += 4;
   out->accel[2] = dsu_read_float_le(p); p += 4;

   /* Gyroscope: DSU sends deg/s, RetroArch uses rad/s */
   out->gyro[0] = dsu_read_float_le(p) * DSU_DEG_TO_RAD; p += 4;
   out->gyro[1] = dsu_read_float_le(p) * DSU_DEG_TO_RAD; p += 4;
   out->gyro[2] = dsu_read_float_le(p) * DSU_DEG_TO_RAD; p += 4;


   return true;
}

static bool dsu_parse_keyboard_data(const uint8_t *payload, size_t len,
      dsu_controller_state_t *out)
{
   const uint8_t *p;
   size_t bit_bytes;

   if (!payload || !out)
      return false;

   bit_bytes = DSU_KEYBOARD_WORDS * sizeof(uint32_t);

   if (len < (1 + 2 + 4 + bit_bytes))
      return false;

   p = payload;

   out->slot = *p++;
   out->keyboard_mod = dsu_read_u16_le(p);
   p += 2;
   out->keyboard_character = dsu_read_u32_le(p);
   p += 4;
   memcpy(out->keyboard_bits, p, bit_bytes);

   return true;
}

static bool dsu_parse_mouse_data(const uint8_t *payload, size_t len,
      dsu_controller_state_t *out)
{
   const uint8_t *p;

   if (!payload || !out)
      return false;

   if (len < 15)
      return false;

   p = payload;

   out->slot = *p++;
   out->mouse_delta_x = (int16_t)dsu_read_u16_le(p);
   p += 2;
   out->mouse_delta_y = (int16_t)dsu_read_u16_le(p);
   p += 2;
   out->mouse_wheel_x = (int16_t)dsu_read_u16_le(p);
   p += 2;
   out->mouse_wheel_y = (int16_t)dsu_read_u16_le(p);
   p += 2;
   out->mouse_x = (int16_t)dsu_read_u16_le(p);
   p += 2;
   out->mouse_y = (int16_t)dsu_read_u16_le(p);
   p += 2;
   out->mouse_buttons = dsu_read_u16_le(p);

   return true;
}

/* ---- Resolve DSU slot from RetroArch port ---- */

static int dsu_resolve_slot(const dsu_state_t *dsu, unsigned port)
{
   if (port >= MAX_USERS)
      return -1;
   return (int)dsu->port_map[port];
}

/* ---- Sensor getters ---- */

float dsu_get_sensor(const dsu_state_t *dsu, unsigned port, unsigned id)
{
   int slot = dsu_resolve_slot(dsu, port);
   if (slot < 0 || slot >= DSU_MAX_CONTROLLERS)
      return 0.0f;
   if (!dsu->controllers[slot].connected)
      return 0.0f;

   float val = 0.0f;
   switch (id)
   {
      case RETRO_SENSOR_ACCELEROMETER_X:
         val = dsu->controllers[slot].accel[0];
         break;
      case RETRO_SENSOR_ACCELEROMETER_Y:
         val = dsu->controllers[slot].accel[1];
         break;
      case RETRO_SENSOR_ACCELEROMETER_Z:
         val = dsu->controllers[slot].accel[2];
         break;
      case RETRO_SENSOR_GYROSCOPE_X:
         val = dsu->controllers[slot].gyro[0];
         break;
      case RETRO_SENSOR_GYROSCOPE_Y:
         val = dsu->controllers[slot].gyro[1];
         break;
      case RETRO_SENSOR_GYROSCOPE_Z:
         val = dsu->controllers[slot].gyro[2];
         break;
      default: break;
   }
   return val;
}

bool dsu_has_sensor(const dsu_state_t *dsu, unsigned port)
{
   int slot = dsu_resolve_slot(dsu, port);
   if (slot < 0 || slot >= DSU_MAX_CONTROLLERS)
      return false;
   return dsu->controllers[slot].connected;
}

/* ---- Button/analog getters ---- */

uint16_t dsu_get_buttons(const dsu_state_t *dsu, unsigned port)
{
   int slot = dsu_resolve_slot(dsu, port);
   if (slot < 0 || slot >= DSU_MAX_CONTROLLERS)
      return 0;
   if (!dsu->controllers[slot].connected)
      return 0;

   uint16_t btns;
#ifdef HAVE_THREADS
   slock_lock((slock_t*)dsu->state_lock);
#endif
   btns = dsu->controllers[slot].buttons;
#ifdef HAVE_THREADS
   slock_unlock((slock_t*)dsu->state_lock);
#endif

   return btns;
}

bool dsu_get_home(const dsu_state_t *dsu, unsigned port)
{
   int slot = dsu_resolve_slot(dsu, port);
   bool home;

   if (slot < 0 || slot >= DSU_MAX_CONTROLLERS)
      return false;
   if (!dsu->controllers[slot].connected)
      return false;

#ifdef HAVE_THREADS
   slock_lock((slock_t*)dsu->state_lock);
#endif
   home = dsu->controllers[slot].home;
#ifdef HAVE_THREADS
   slock_unlock((slock_t*)dsu->state_lock);
#endif

   return home;
}

int16_t dsu_get_analog(const dsu_state_t *dsu, unsigned port,
      unsigned index, unsigned id)
{
   int slot = dsu_resolve_slot(dsu, port);
   if (slot < 0 || slot >= DSU_MAX_CONTROLLERS)
      return 0;
   if (!dsu->controllers[slot].connected)
      return 0;

   switch (index)
   {
      case RETRO_DEVICE_INDEX_ANALOG_LEFT:
         if (id == RETRO_DEVICE_ID_ANALOG_X) return dsu->controllers[slot].analog_lx;
         if (id == RETRO_DEVICE_ID_ANALOG_Y) return dsu->controllers[slot].analog_ly;
         break;
      case RETRO_DEVICE_INDEX_ANALOG_RIGHT:
         if (id == RETRO_DEVICE_ID_ANALOG_X) return dsu->controllers[slot].analog_rx;
         if (id == RETRO_DEVICE_ID_ANALOG_Y) return dsu->controllers[slot].analog_ry;
         break;
      default:
         break;
   }
   return 0;
}

/* ---- Touch getters ---- */

bool dsu_get_touch_active(const dsu_state_t *dsu, unsigned port,
      unsigned touch_index)
{
   int slot = dsu_resolve_slot(dsu, port);
   if (slot < 0 || slot >= DSU_MAX_CONTROLLERS)
      return false;
   if (!dsu->controllers[slot].connected)
      return false;
   if (touch_index == 0)
      return dsu->controllers[slot].touch1_active;
   if (touch_index == 1)
      return dsu->controllers[slot].touch2_active;
   return false;
}

int16_t dsu_get_touch_x(const dsu_state_t *dsu, unsigned port,
      unsigned touch_index)
{
   int slot = dsu_resolve_slot(dsu, port);
   if (slot < 0 || slot >= DSU_MAX_CONTROLLERS)
      return 0;
   if (!dsu->controllers[slot].connected)
      return 0;
   if (touch_index == 0)
      return dsu->controllers[slot].touch1_x;
   if (touch_index == 1)
      return dsu->controllers[slot].touch2_x;
   return 0;
}

int16_t dsu_get_touch_y(const dsu_state_t *dsu, unsigned port,
      unsigned touch_index)
{
   int slot = dsu_resolve_slot(dsu, port);
   if (slot < 0 || slot >= DSU_MAX_CONTROLLERS)
      return 0;
   if (!dsu->controllers[slot].connected)
      return 0;
   if (touch_index == 0)
      return dsu->controllers[slot].touch1_y;
   if (touch_index == 1)
      return dsu->controllers[slot].touch2_y;
   return 0;
}

/* ---- Per-player mode helpers ---- */

bool dsu_port_is_fullpad(const dsu_state_t *dsu, unsigned port)
{
   if (port >= MAX_USERS)
      return false;
   if (dsu->port_map[port] < 0)
      return false;
   return !dsu->player_addon_attached[port];
}

bool dsu_port_has_accel(const dsu_state_t *dsu, unsigned port)
{
   if (port >= MAX_USERS)
      return false;
   if (dsu->port_map[port] < 0)
      return false;
   return dsu->player_accel[port];
}

bool dsu_port_has_gyro(const dsu_state_t *dsu, unsigned port)
{
   if (port >= MAX_USERS)
      return false;
   if (dsu->port_map[port] < 0)
      return false;
   return dsu->player_gyro[port];
}

bool dsu_port_has_touch(const dsu_state_t *dsu, unsigned port)
{
   if (port >= MAX_USERS)
      return false;
   if (dsu->port_map[port] < 0)
      return false;
   return dsu->player_touch[port];
}

bool dsu_port_has_keyboard(const dsu_state_t *dsu, unsigned port)
{
   if (port >= MAX_USERS)
      return false;
   if (dsu->port_map[port] < 0)
      return false;
   return dsu->player_keyboard[port];
}

bool dsu_port_has_mouse(const dsu_state_t *dsu, unsigned port)
{
   if (port >= MAX_USERS)
      return false;
   if (dsu->port_map[port] < 0)
      return false;
   return dsu->player_mouse[port];
}

static const char *dsu_get_port_server_address(const dsu_state_t *dsu,
      unsigned port)
{
   if (port < MAX_USERS)
      return dsu->player_server_address[port];
   return NULL;
}

static uint16_t dsu_get_port_server_port(const dsu_state_t *dsu,
      unsigned port)
{
   if (port < MAX_USERS)
      return dsu->player_server_port[port];
   return 0;
}

static bool dsu_endpoint_matches_port(const dsu_state_t *dsu,
      unsigned port, const char *server_address, uint16_t server_port)
{
   const char *cfg_addr = dsu_get_port_server_address(dsu, port);
   uint16_t cfg_port = dsu_get_port_server_port(dsu, port);
   return string_is_equal(cfg_addr, server_address) && cfg_port == server_port;
}

static void dsu_add_endpoint(char addresses[MAX_USERS + 1][64],
      uint16_t ports[MAX_USERS + 1], unsigned *count,
      const char *server_address, uint16_t server_port)
{
   unsigned i;

   if (string_is_empty(server_address) || server_port == 0)
      return;

   for (i = 0; i < *count; i++)
   {
      if (ports[i] == server_port && string_is_equal(addresses[i], server_address))
         return;
   }

   if (*count >= (MAX_USERS + 1))
      return;

   strlcpy(addresses[*count], server_address, sizeof(addresses[*count]));
   ports[*count] = server_port;
   (*count)++;
}

static void dsu_collect_endpoints(const dsu_state_t *dsu,
      char addresses[MAX_USERS + 1][64], uint16_t ports[MAX_USERS + 1],
      unsigned *count)
{
   unsigned i;

   *count = 0;

   for (i = 0; i < MAX_USERS; i++)
      dsu_add_endpoint(addresses, ports, count,
            dsu_get_port_server_address(dsu, i),
            dsu_get_port_server_port(dsu, i));
}

static void dsu_send_packet_to_server(int fd, const uint8_t *pkt,
      size_t pkt_len, const char *server_address, uint16_t server_port)
{
   struct sockaddr_in addr;
   int ret;

   memset(&addr, 0, sizeof(addr));
   addr.sin_family = AF_INET;
   addr.sin_port   = htons(server_port);
#ifdef _WIN32
   addr.sin_addr.s_addr = inet_addr(server_address);
#else
   inet_aton(server_address, &addr.sin_addr);
#endif

   ret = sendto(fd, (const char*)pkt, (int)pkt_len, 0,
         (struct sockaddr*)&addr, sizeof(addr));
   if (ret < 0)
   {
#ifdef _WIN32
      RARCH_ERR("[DSU] sendto failed for %s:%u len=%zu error=%d\n",
            server_address, server_port, pkt_len, WSAGetLastError());
#else
      RARCH_ERR("[DSU] sendto failed for %s:%u len=%zu\n",
            server_address, server_port, pkt_len);
#endif
   }
   else
      RARCH_LOG("[DSU] sendto ok to %s:%u len=%zu ret=%d\n",
            server_address, server_port, pkt_len, ret);
}

static void dsu_send_requests_to_endpoints(dsu_state_t *dsu,
      bool send_controller_info)
{
   char addresses[MAX_USERS + 1][64];
   uint16_t ports[MAX_USERS + 1];
   uint8_t pkt[DSU_MAX_PACKET_SIZE];
   uint8_t slots[DSU_MAX_CONTROLLERS];
   unsigned count = 0;
   unsigned endpoint;
   unsigned slot;
   size_t pkt_len;

   dsu_collect_endpoints(dsu, addresses, ports, &count);

   for (slot = 0; slot < DSU_MAX_CONTROLLERS; slot++)
      slots[slot] = (uint8_t)slot;

   for (endpoint = 0; endpoint < count; endpoint++)
   {
      if (send_controller_info)
      {
         pkt_len = dsu_build_controller_info_request(pkt, sizeof(pkt),
               dsu->client_id, DSU_MAX_CONTROLLERS, slots);
         if (pkt_len > 0)
         {
            dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len,
                  addresses[endpoint], ports[endpoint]);
            RARCH_LOG("[DSU] Sent controller info request to %s:%u, size=%zu\n",
                  addresses[endpoint], ports[endpoint], pkt_len);
         }
      }

      for (slot = 0; slot < DSU_MAX_CONTROLLERS; slot++)
      {
         if (!dsu->subscribed[slot])
            continue;

         uint8_t data_type_flags = 0xFF;
         unsigned port;
         for (port = 0; port < MAX_USERS; port++)
         {
            if ((unsigned)dsu->port_map[port] == slot)
            {
               data_type_flags = 0;
               if (dsu->player_gamepad[port])
                  data_type_flags |= DSU_DATA_GAMEPAD;
               if (dsu->player_accel[port] || dsu->player_gyro[port])
                  data_type_flags |= DSU_DATA_MOTION;
               if (dsu->player_touch[port])
                  data_type_flags |= DSU_DATA_TOUCH;
               if (dsu->player_keyboard[port])
                  data_type_flags |= DSU_DATA_KEYBOARD;
               if (dsu->player_mouse[port])
                  data_type_flags |= DSU_DATA_MOUSE;
               break;
            }
         }

         pkt_len = dsu_build_subscribe_request(pkt, sizeof(pkt),
               dsu->client_id, DSU_SUB_SLOT_BASED, data_type_flags, (uint8_t)slot, NULL);
         if (pkt_len > 0)
         {
            dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len,
                  addresses[endpoint], ports[endpoint]);
            RARCH_LOG("[DSU] Sent subscribe request to %s:%u for slot %u, data_type_flags=0x%02x, size=%zu\n",
                  addresses[endpoint], ports[endpoint], slot, data_type_flags, pkt_len);
         }
      }
   }
}

static int dsu_find_controller_slot(const dsu_state_t *dsu,
      const char *server_address, uint16_t server_port, uint8_t remote_slot)
{
   int i;

   for (i = 0; i < DSU_MAX_CONTROLLERS; i++)
   {
      if (dsu->controllers[i].attached_port < 0)
         continue;
      if (dsu->controllers[i].slot != remote_slot)
         continue;
      if (dsu->controllers[i].server_port != server_port)
         continue;
      if (!string_is_equal(dsu->controllers[i].server_address, server_address))
         continue;
      return i;
   }

   return -1;
}

static int dsu_find_free_controller_slot(const dsu_state_t *dsu)
{
   int i;

   for (i = 0; i < DSU_MAX_CONTROLLERS; i++)
   {
      if (dsu->controllers[i].attached_port < 0)
         return i;
   }

   return -1;
}

static int dsu_find_attach_port(dsu_state_t *dsu,
      const char *server_address, uint16_t server_port, bool *addon_attached)
{
   unsigned i;

   *addon_attached = false;

   for (i = 0; i < MAX_USERS; i++)
   {
      if (dsu->port_map[i] >= 0)
         continue;
      if (!dsu_endpoint_matches_port(dsu, i, server_address, server_port))
         continue;
      if (!dsu->player_addon[i])
         continue;
      if (!input_joypad_port_has_hardware_gamepad(i))
         continue;

      *addon_attached = true;
      return (int)i;
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      if (dsu->port_map[i] >= 0)
         continue;
      if (!dsu_endpoint_matches_port(dsu, i, server_address, server_port))
         continue;
      return (int)i;
   }
   return -1;
}

static int dsu_attach_controller(dsu_state_t *dsu,
      const char *server_address, uint16_t server_port, uint8_t remote_slot)
{
   int controller_slot = dsu_find_controller_slot(dsu,
         server_address, server_port, remote_slot);
   bool addon_attached = false;
   int port;

   if (controller_slot >= 0)
      return controller_slot;

   controller_slot = dsu_find_free_controller_slot(dsu);
   if (controller_slot < 0)
      return -1;

   port = dsu_find_attach_port(dsu, server_address, server_port, &addon_attached);
   if (port < 0)
      return -1;

   memset(&dsu->controllers[controller_slot], 0, sizeof(dsu->controllers[controller_slot]));
   dsu->controllers[controller_slot].attached_port = (int8_t)port;
   dsu->controllers[controller_slot].slot          = remote_slot;
   strlcpy(dsu->controllers[controller_slot].server_address,
         server_address, sizeof(dsu->controllers[controller_slot].server_address));
   dsu->controllers[controller_slot].server_port   = server_port;
   dsu->port_map[port]                             = (int8_t)controller_slot;
   dsu->player_addon_attached[port]                = addon_attached;

   RARCH_LOG("[DSU] Attached %s:%u remote slot %u to port %u as %s using local slot %d\n",
         server_address, server_port, remote_slot, (unsigned)port,
         addon_attached ? "addon" : "fullpad", controller_slot);

   return controller_slot;
}

static void dsu_detach_controller(dsu_state_t *dsu, int controller_slot)
{
   unsigned port;

   if (controller_slot < 0 || controller_slot >= DSU_MAX_CONTROLLERS)
      return;

   port = (unsigned)dsu->controllers[controller_slot].attached_port;

   if (port < MAX_USERS && dsu->port_map[port] == (int8_t)controller_slot)
   {
      dsu->port_map[port] = -1;
      dsu->player_addon_attached[port] = false;
   }

   memset(&dsu->controllers[controller_slot], 0, sizeof(dsu->controllers[controller_slot]));
   dsu->controllers[controller_slot].attached_port = -1;
}

static bool dsu_get_packet_endpoint(const struct sockaddr_storage *addr,
      char *server_address, size_t server_address_size, uint16_t *server_port)
{
   const struct sockaddr_in *addr_in;

   if (!addr || addr->ss_family != AF_INET)
      return false;

   addr_in = (const struct sockaddr_in*)addr;

   if (!inet_ntop(AF_INET, &addr_in->sin_addr, server_address,
         (socklen_t)server_address_size))
      return false;

   *server_port = ntohs(addr_in->sin_port);
   return true;
}

bool dsu_client_init(dsu_state_t *dsu)
{
   struct addrinfo *res = NULL;
   int fd;
   int i;

   RARCH_LOG("[DSU] Client init\n");

   if (!dsu->enabled)
      return false;

   if (!network_init())
   {
      RARCH_ERR("[DSU] Network init failed\n");
      return false;
   }

   dsu->client_id      = dsu_generate_id();
   dsu->packet_counter = 0;

   RARCH_LOG("[DSU] Generated client_id=0x%08X\n", dsu->client_id);

   fd = socket_init((void**)&res, 0, NULL, SOCKET_TYPE_DATAGRAM, AF_INET);
   if (fd < 0)
   {
      RARCH_ERR("[DSU] Failed to create client socket.\n");
      return false;
   }

   dsu->socket_fd = fd;

   if (!socket_nonblock(fd))
      RARCH_WARN("[DSU] Failed to set client socket non-blocking.\n");

   if (res)
      freeaddrinfo_retro(res);

   RARCH_LOG("[DSU] Client initialized.\n");

   for (i = 0; i < DSU_MAX_CONTROLLERS; i++)
   {
      dsu->slot_remapped[i]        = false;
      dsu->subscribed[i]            = true;
      dsu->controllers[i].attached_port = -1;
   }

   dsu_send_requests_to_endpoints(dsu, true);
   dsu->last_request_time = dsu_get_time_us();

#ifdef HAVE_THREADS
   dsu->state_lock = slock_new();
   if (!dsu->state_lock)
   {
      RARCH_ERR("[DSU] Failed to create state lock\n");
      socket_close(dsu->socket_fd);
      dsu->socket_fd = -1;
      return false;
   }
#endif

   RARCH_LOG("[DSU] Client init complete\n");
   return true;
}

void dsu_client_deinit(dsu_state_t *dsu)
{
   int i;

   if (dsu->socket_fd >= 0)
   {
      socket_close(dsu->socket_fd);
      dsu->socket_fd = -1;
   }

   memset(dsu->controllers, 0, sizeof(dsu->controllers));
   for (i = 0; i < DSU_MAX_CONTROLLERS; i++)
      dsu->controllers[i].attached_port = -1;

#ifdef HAVE_THREADS
   if (dsu->state_lock)
   {
      slock_free(dsu->state_lock);
      dsu->state_lock = NULL;
   }
#endif
}

void dsu_client_poll(dsu_state_t *dsu)
{
   int64_t now;
   ssize_t ret;
   static int poll_count = 0;
   char source_address[64];

   if (!dsu->enabled || dsu->socket_fd < 0)
      return;

   {
      uint16_t source_port = 0;

   poll_count++;

   now = dsu_get_time_us();

   if (now - dsu->last_request_time > (DSU_CLIENT_TIMEOUT_US / 2))
   {
      dsu_send_requests_to_endpoints(dsu, false);
      dsu->last_request_time = now;
   }

   for (;;)
   {
      struct sockaddr_storage from_addr;
      socklen_t from_addr_len = sizeof(from_addr);

      ret = recvfrom(dsu->socket_fd,
            (char*)dsu->recv_buf, DSU_MAX_PACKET_SIZE,
            0, (struct sockaddr*)&from_addr, &from_addr_len);

      if (ret <= 0)
         break;

      if (!dsu_get_packet_endpoint(&from_addr, source_address,
               sizeof(source_address), &source_port))
         continue;

      if (!dsu_validate_header(dsu->recv_buf, (size_t)ret,
               DSU_MAGIC_SERVER))
      {
         RARCH_LOG("[DSU] Header validation failed\n");
         continue;
      }

      {
         uint32_t msg_type;
         const uint8_t *payload;
         size_t payload_len;
         msg_type    = dsu_read_u32_le(dsu->recv_buf + 16);
         payload     = dsu->recv_buf + DSU_HEADER_SIZE;
         payload_len = (size_t)ret - DSU_HEADER_SIZE;

         /* Auto-configure multicast stream URL for new DSU connections */
         {
            static char last_dsu_source_address[64] = {0};
            static uint16_t last_dsu_source_port = 0;
            settings_t *settings = config_get_ptr();
            const char *current_url = settings->paths.path_stream_url;

            if (!string_is_equal(source_address, last_dsu_source_address) ||
                source_port != last_dsu_source_port)
            {
               /* Check if URL is empty */
               if (string_is_empty(current_url))
               {
                  char multicast_url[256];
                  unsigned stream_port = source_port + 200; /* Offset from DSU port */

                  snprintf(multicast_url, sizeof(multicast_url),
                       "udp://%s:%u", source_address, stream_port);

                  strlcpy(settings->paths.path_stream_url, multicast_url,
                        sizeof(settings->paths.path_stream_url));

                  RARCH_LOG("[DSU] Auto-configured stream URL from new connection %s:%u -> %s\n",
                        source_address, source_port, multicast_url);
               }

               /* Update tracking */
               strlcpy(last_dsu_source_address, source_address,
                     sizeof(last_dsu_source_address));
               last_dsu_source_port = source_port;
            }
         }

         switch (msg_type)
         {
            case DSU_MSG_DATA:
               if (payload_len >= 80)
               {
                  uint8_t remote_slot;
                  int controller_slot;
                  bool old_touch1_active;
                  bool old_touch2_active;
                  remote_slot     = payload[0];
                  controller_slot = dsu_find_controller_slot(dsu,
                        source_address, source_port, remote_slot);

                  if (remote_slot >= DSU_MAX_CONTROLLERS)
                     break;
                  if (controller_slot < 0)
                     controller_slot = dsu_attach_controller(dsu,
                           source_address, source_port, remote_slot);
                  if (controller_slot < 0)
                     break;

                  old_touch1_active = dsu->controllers[controller_slot].touch1_active;
                  old_touch2_active = dsu->controllers[controller_slot].touch2_active;
#ifdef HAVE_THREADS
                  slock_lock(dsu->state_lock);
#endif
                  dsu_parse_controller_data(payload, payload_len,
                        &dsu->controllers[controller_slot]);
                  strlcpy(dsu->controllers[controller_slot].server_address,
                        source_address, sizeof(dsu->controllers[controller_slot].server_address));
                  dsu->controllers[controller_slot].server_port = source_port;
#ifdef HAVE_THREADS
                  slock_unlock(dsu->state_lock);
#endif
                  if (old_touch1_active != dsu->controllers[controller_slot].touch1_active
                        || old_touch2_active != dsu->controllers[controller_slot].touch2_active
                        || dsu->controllers[controller_slot].touch1_active
                        || dsu->controllers[controller_slot].touch2_active)
                     RARCH_LOG("[DSU] TOUCH parsed: source=%s:%u remote_slot=%u local_slot=%d t1=%d id1=%u x1=%d y1=%d t2=%d id2=%u x2=%d y2=%d\n",
                           source_address, source_port, remote_slot, controller_slot,
                           dsu->controllers[controller_slot].touch1_active,
                           dsu->controllers[controller_slot].touch1_id,
                           dsu->controllers[controller_slot].touch1_x,
                           dsu->controllers[controller_slot].touch1_y,
                           dsu->controllers[controller_slot].touch2_active,
                           dsu->controllers[controller_slot].touch2_id,
                           dsu->controllers[controller_slot].touch2_x,
                           dsu->controllers[controller_slot].touch2_y);
               }
               break;

            case DSU_MSG_KEYBOARD:
               {
                  uint8_t remote_slot;
                  int controller_slot;
                  int attached_port;
                  uint32_t old_bits[DSU_KEYBOARD_WORDS];
                  uint32_t new_bits[DSU_KEYBOARD_WORDS];
                  size_t word;

                  if (payload_len < 7)
                     break;

                  remote_slot = payload[0];

                  if (remote_slot >= DSU_MAX_CONTROLLERS)
                     break;

                  controller_slot = dsu_find_controller_slot(dsu,
                        source_address, source_port, remote_slot);
                  if (controller_slot < 0)
                     controller_slot = dsu_attach_controller(dsu,
                           source_address, source_port, remote_slot);
                  if (controller_slot < 0)
                     break;

                  attached_port = dsu->controllers[controller_slot].attached_port;
                  memcpy(old_bits, dsu->controllers[controller_slot].keyboard_bits,
                        sizeof(old_bits));

#ifdef HAVE_THREADS
                  slock_lock(dsu->state_lock);
#endif
                  if (!dsu_parse_keyboard_data(payload, payload_len,
                           &dsu->controllers[controller_slot]))
                  {
#ifdef HAVE_THREADS
                     slock_unlock(dsu->state_lock);
#endif
                     break;
                  }
                  strlcpy(dsu->controllers[controller_slot].server_address,
                        source_address, sizeof(dsu->controllers[controller_slot].server_address));
                  dsu->controllers[controller_slot].server_port = source_port;
                  memcpy(new_bits, dsu->controllers[controller_slot].keyboard_bits,
                        sizeof(new_bits));
#ifdef HAVE_THREADS
                  slock_unlock(dsu->state_lock);
#endif

                  if (attached_port >= 0 && attached_port < MAX_USERS &&
                        dsu->player_keyboard[attached_port])
                  {
                     for (word = 0; word < DSU_KEYBOARD_WORDS; word++)
                     {
                        uint32_t changed;
                        changed = old_bits[word] ^ new_bits[word];

                        while (changed)
                        {
                           unsigned bit;
                           unsigned key;
                           bit = 0;

                           while (((changed >> bit) & 1U) == 0U)
                              bit++;

                           key = (unsigned)(word * 32 + bit);
                           if (key < RETROK_LAST)
                           {
                              bool down;
                              down = ((new_bits[word] >> bit) & 1U) != 0U;
                              input_keyboard_event(down, key,
                                    down ? dsu->controllers[controller_slot].keyboard_character : 0,
                                    dsu->controllers[controller_slot].keyboard_mod,
                                    RETRO_DEVICE_KEYBOARD);
                           }

                           changed &= ~(1U << bit);
                        }
                     }
                  }

                  RARCH_LOG("[DSU] KEYBOARD message: source=%s:%u remote_slot=%u local_slot=%d attached_port=%d\n",
                        source_address, source_port, remote_slot, controller_slot,
                        attached_port);
               }
               break;

            case DSU_MSG_MOUSE:
               {
                  uint8_t remote_slot;
                  int controller_slot;

                  if (payload_len < 15)
                     break;

                  remote_slot = payload[0];

                  if (remote_slot >= DSU_MAX_CONTROLLERS)
                     break;

                  controller_slot = dsu_find_controller_slot(dsu,
                        source_address, source_port, remote_slot);
                  if (controller_slot < 0)
                     controller_slot = dsu_attach_controller(dsu,
                           source_address, source_port, remote_slot);
                  if (controller_slot < 0)
                     break;

#ifdef HAVE_THREADS
                  slock_lock(dsu->state_lock);
#endif
                  if (!dsu_parse_mouse_data(payload, payload_len,
                           &dsu->controllers[controller_slot]))
                  {
#ifdef HAVE_THREADS
                     slock_unlock(dsu->state_lock);
#endif
                     break;
                  }
                  strlcpy(dsu->controllers[controller_slot].server_address,
                        source_address, sizeof(dsu->controllers[controller_slot].server_address));
                  dsu->controllers[controller_slot].server_port = source_port;
#ifdef HAVE_THREADS
                  slock_unlock(dsu->state_lock);
#endif

                  RARCH_LOG("[DSU] MOUSE message: source=%s:%u remote_slot=%u local_slot=%d dx=%d dy=%d buttons=0x%x\n",
                        source_address, source_port, remote_slot, controller_slot,
                        dsu->controllers[controller_slot].mouse_delta_x,
                        dsu->controllers[controller_slot].mouse_delta_y,
                        dsu->controllers[controller_slot].mouse_buttons);
               }
               break;

            case DSU_MSG_COMMAND:
               if (payload_len >= 4 + 256 + 64)
               {
                  dsu_command_t cmd;
                  size_t copy_len = payload_len < sizeof(cmd) ? payload_len : sizeof(cmd);
                  memset(&cmd, 0, sizeof(cmd));
                  memcpy(&cmd, payload, copy_len);
                  dsu_handle_command(source_address, source_port, &cmd);
               }
               break;

            case DSU_MSG_STREAM_START:
               if (payload_len >= sizeof(dsu_stream_request_t))
               {
                  dsu_stream_request_t req;
                  memcpy(&req, payload, sizeof(req));
                  dsu_handle_stream_start(dsu, source_address, source_port, &req);
               }
               break;

            case DSU_MSG_STREAM_STOP:
               dsu_handle_stream_stop(dsu, source_address, source_port);
               break;

            case DSU_MSG_AUX_STREAM_START:
               if (payload_len >= sizeof(dsu_aux_stream_request_t))
               {
                  dsu_aux_stream_request_t req;
                  memcpy(&req, payload, sizeof(req));
                  dsu_handle_aux_stream_start(dsu, source_address, source_port, &req);
               }
               break;

            case DSU_MSG_AUX_STREAM_STOP:
               if (payload_len >= 1)
                  dsu_handle_aux_stream_stop(dsu, source_address, source_port, payload[0]);
               break;

            case DSU_MSG_CONTROLLER:
               if (payload_len >= 12)
               {
                  uint8_t remote_slot;
                  bool connected;
                  int controller_slot;
                  bool was_connected;
                  int attached_port;
                  remote_slot     = payload[0];
                  connected       = (payload[1] == DSU_CONN_CONNECTED);
                  controller_slot = dsu_find_controller_slot(dsu,
                        source_address, source_port, remote_slot);

                  if (remote_slot >= DSU_MAX_CONTROLLERS)
                     break;
                  if (controller_slot < 0 && connected)
                     controller_slot = dsu_attach_controller(dsu,
                           source_address, source_port, remote_slot);
                  if (controller_slot < 0)
                     break;

                  was_connected = dsu->controllers[controller_slot].connected;
                  attached_port = dsu->controllers[controller_slot].attached_port;
#ifdef HAVE_THREADS
                  slock_lock(dsu->state_lock);
#endif
                  dsu->controllers[controller_slot].slot       = remote_slot;
                  dsu->controllers[controller_slot].connected  = connected;
                  dsu->controllers[controller_slot].model      = payload[2];
                  dsu->controllers[controller_slot].conn_type  = payload[3];
                  memcpy(dsu->controllers[controller_slot].mac, payload + 4, 6);
                  dsu->controllers[controller_slot].battery    = payload[10];
                  strlcpy(dsu->controllers[controller_slot].server_address,
                        source_address, sizeof(dsu->controllers[controller_slot].server_address));
                  dsu->controllers[controller_slot].server_port = source_port;
#ifdef HAVE_THREADS
                  slock_unlock(dsu->state_lock);
#endif
                  RARCH_LOG("[DSU] CONTROLLER message: source=%s:%u remote_slot=%d local_slot=%d connected=%d model=%d battery=%d\n",
                        source_address, source_port, remote_slot, controller_slot,
                        dsu->controllers[controller_slot].connected,
                        dsu->controllers[controller_slot].model,
                        dsu->controllers[controller_slot].battery);

                  if (!was_connected && dsu->controllers[controller_slot].connected && attached_port >= 0)
                  {
                     char msg[128];
                     size_t msg_len;

                     if (dsu->player_addon_attached[attached_port])
                        msg_len = strlcpy(msg, "DSU Connected in Addon Mode to Port ", sizeof(msg));
                     else
                        msg_len = strlcpy(msg, "DSU Controller Connected to Port ", sizeof(msg));

                     msg_len += snprintf(msg + msg_len, sizeof(msg) - msg_len,
                           "%u", (unsigned)attached_port + 1);
                     RARCH_LOG("[DSU] %s\n", msg);
                     runloop_msg_queue_push(msg, msg_len, 1, 180, true, NULL,
                           MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
                  }
                  else if (was_connected && !dsu->controllers[controller_slot].connected)
                     dsu_detach_controller(dsu, controller_slot);
               }
               break;

            case DSU_MSG_VERSION:
               if (payload_len >= 2)
               {
                  uint16_t ver;
                  ver = dsu_read_u16_le(payload);
                  RARCH_LOG("[DSU] Server protocol version: %u\n", ver);
               }
               break;

            case DSU_MSG_RUMBLE:
               if (payload_len >= 4)
               {
                  uint8_t slot = payload[0];
                  uint8_t motor = payload[1];
                  uint8_t intensity = payload[2];
                  RARCH_LOG("[DSU] RUMBLE message from server: source=%s:%u slot=%u motor=%u intensity=%u\n",
                        source_address, source_port, slot, motor, intensity);
               }
               break;

            case DSU_MSG_PLAYLIST_LIST_REQUEST:
               if (payload_len >= 1)
               {
                  uint8_t page = payload[0];
                  RARCH_LOG("[DSU] PLAYLIST_LIST_REQUEST from %s:%u page=%u\n",
                        source_address, source_port, page);
                  dsu_send_playlist_list(dsu, source_address, source_port,
                        dsu->client_id, page);
               }
               break;

            case DSU_MSG_PLAYLIST_CONTENTS_REQUEST:
               if (payload_len >= 1)
               {
                  uint8_t page = payload[0];
                  char playlist_path[256];
                  size_t path_len = payload_len - 1;
                  if (path_len > sizeof(playlist_path) - 1)
                     path_len = sizeof(playlist_path) - 1;
                  strlcpy(playlist_path, (char*)(payload + 1), path_len + 1);
                  RARCH_LOG("[DSU] PLAYLIST_CONTENTS_REQUEST from %s:%u page=%u path='%s' (len=%zu)\n",
                        source_address, source_port, page, playlist_path, path_len);
                  dsu_send_playlist_contents(dsu, source_address, source_port,
                        dsu->client_id, playlist_path, page);
               }
               break;

            case DSU_MSG_CONTENTLESS_CORE_LIST_REQUEST:
               if (payload_len >= 1)
               {
                  uint8_t page = payload[0];
                  RARCH_LOG("[DSU] CONTENTLESS_CORE_LIST_REQUEST from %s:%u page=%u\n",
                        source_address, source_port, page);
                  dsu_send_contentless_core_list(dsu, source_address, source_port,
                        dsu->client_id, page);
               }
               break;

            default:
               RARCH_LOG("[DSU] Unknown message type: 0x%08X\n", msg_type);
               break;
         }
      }
   }
   }
}

/* ---- Rumble sending (client mode) ---- */

void dsu_send_rumble(dsu_state_t *dsu, unsigned slot,
      uint8_t motor_id, uint8_t intensity)
{
   uint8_t pkt[DSU_MAX_PACKET_SIZE];
   size_t  pkt_len;

   if (!dsu->enabled || dsu->socket_fd < 0)
      return;
   if (slot >= DSU_MAX_CONTROLLERS)
      return;

   pkt_len = dsu_build_rumble_packet(pkt, sizeof(pkt),
         dsu->client_id, DSU_SUB_SLOT_BASED, dsu->controllers[slot].slot,
         dsu->controllers[slot].mac, motor_id, intensity);
   if (pkt_len > 0)
      dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len,
            dsu->controllers[slot].server_address,
            dsu->controllers[slot].server_port);

   dsu->rumble_last_sent[slot] = dsu_get_time_us();
}

void dsu_send_state(dsu_state_t *dsu, const char *game_name,
      const char *platform_name, const char *core_name, uint8_t state_flags)
{
   char addresses[MAX_USERS + 1][64];
   uint16_t ports[MAX_USERS + 1];
   uint8_t pkt[DSU_MAX_PACKET_SIZE];
   size_t pkt_len;
   unsigned count = 0;
   unsigned i;

   if (!dsu || !dsu->enabled || !dsu->client_enabled || dsu->socket_fd < 0)
      return;

   pkt_len = dsu_build_state_packet(pkt, sizeof(pkt), dsu->client_id,
         game_name, platform_name, core_name, state_flags);
   if (pkt_len == 0)
      return;

   dsu_collect_endpoints(dsu, addresses, ports, &count);

   for (i = 0; i < count; i++)
   {
      dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len,
            addresses[i], ports[i]);
      RARCH_LOG("[DSU] Sent state update to %s:%u game=\"%s\" platform=\"%s\" core=\"%s\" flags=0x%02X\n",
            addresses[i], ports[i],
            game_name ? game_name : "",
            platform_name ? platform_name : "",
            core_name ? core_name : "",
            state_flags);
   }
}

/* ---- Pointer/mouse helpers ---- */

int16_t dsu_get_pointer_x(const dsu_state_t *dsu, unsigned port, unsigned idx)
{
   int slot;

   if (port >= MAX_USERS)
      return 0;
   slot = dsu->port_map[port];
   if (slot < 0)
      return 0;

   if (idx == 0 && dsu->controllers[slot].touch1_active)
   {
      RARCH_LOG("[DSU] TOUCH queried: port=%u idx=%u axis=x value=%d\n",
            port, idx, dsu->controllers[slot].touch1_x);
      return dsu->controllers[slot].touch1_x;
   }
   if (idx == 1 && dsu->controllers[slot].touch2_active)
   {
      RARCH_LOG("[DSU] TOUCH queried: port=%u idx=%u axis=x value=%d\n",
            port, idx, dsu->controllers[slot].touch2_x);
      return dsu->controllers[slot].touch2_x;
   }
   return 0;
}

int16_t dsu_get_pointer_y(const dsu_state_t *dsu, unsigned port, unsigned idx)
{
   int slot;

   if (port >= MAX_USERS)
      return 0;
   slot = dsu->port_map[port];
   if (slot < 0)
      return 0;

   if (idx == 0 && dsu->controllers[slot].touch1_active)
   {
      RARCH_LOG("[DSU] TOUCH queried: port=%u idx=%u axis=y value=%d\n",
            port, idx, dsu->controllers[slot].touch1_y);
      return dsu->controllers[slot].touch1_y;
   }
   if (idx == 1 && dsu->controllers[slot].touch2_active)
   {
      RARCH_LOG("[DSU] TOUCH queried: port=%u idx=%u axis=y value=%d\n",
            port, idx, dsu->controllers[slot].touch2_y);
      return dsu->controllers[slot].touch2_y;
   }
   return 0;
}

int16_t dsu_get_pointer_pressed(const dsu_state_t *dsu, unsigned port, unsigned idx)
{
   int slot;

   if (port >= MAX_USERS)
      return 0;
   slot = dsu->port_map[port];
   if (slot < 0)
      return 0;

   if (idx == 0 && dsu->controllers[slot].touch1_active)
   {
      RARCH_LOG("[DSU] TOUCH queried: port=%u idx=%u pressed=1\n", port, idx);
      return 1;
   }
   if (idx == 1 && dsu->controllers[slot].touch2_active)
   {
      RARCH_LOG("[DSU] TOUCH queried: port=%u idx=%u pressed=1\n", port, idx);
      return 1;
   }
   return 0;
}

int16_t dsu_get_pointer_count(const dsu_state_t *dsu, unsigned port)
{
   int slot;
   int count = 0;

   if (port >= MAX_USERS)
      return 0;
   slot = dsu->port_map[port];
   if (slot < 0)
      return 0;

   if (dsu->controllers[slot].touch1_active)
      count++;
   if (dsu->controllers[slot].touch2_active)
      count++;
   if (count > 0)
      RARCH_LOG("[DSU] TOUCH queried: port=%u count=%d\n", port, count);
   return count;
}

bool dsu_get_keyboard_key(const dsu_state_t *dsu, unsigned port, unsigned key)
{
   int slot;
   unsigned word;
   unsigned bit;

   if (port >= MAX_USERS || key >= RETROK_LAST)
      return false;

   slot = dsu->port_map[port];
   if (slot < 0)
      return false;
   if (!dsu->controllers[slot].connected)
      return false;

   word = key / 32;
   bit  = key % 32;

   if (word >= DSU_KEYBOARD_WORDS)
      return false;

   return ((dsu->controllers[slot].keyboard_bits[word] >> bit) & 1U) != 0U;
}

void dsu_get_mouse_delta(const dsu_state_t *dsu, unsigned port, int *delta_x, int *delta_y)
{
   int slot;

   if (delta_x)
      *delta_x = 0;
   if (delta_y)
      *delta_y = 0;

   if (port >= MAX_USERS)
      return;

   slot = dsu->port_map[port];
   if (slot < 0)
      return;
   if (!dsu->controllers[slot].connected)
      return;

   if (delta_x)
      *delta_x = dsu->controllers[slot].mouse_delta_x;
   if (delta_y)
      *delta_y = dsu->controllers[slot].mouse_delta_y;
}

bool dsu_get_mouse_button(const dsu_state_t *dsu, unsigned port, unsigned btn)
{
   int slot;
   uint16_t mask;

   if (port >= MAX_USERS)
      return false;

   slot = dsu->port_map[port];
   if (slot < 0)
      return false;
   if (!dsu->controllers[slot].connected)
      return false;
   if (btn == 5)
      return dsu->controllers[slot].mouse_wheel_y > 0;
   if (btn == 6)
      return dsu->controllers[slot].mouse_wheel_y < 0;
   if (btn == 7)
      return dsu->controllers[slot].mouse_wheel_x > 0;
   if (btn == 8)
      return dsu->controllers[slot].mouse_wheel_x < 0;
   if (btn >= 16)
      return false;

   mask = (uint16_t)(1U << btn);
   return (dsu->controllers[slot].mouse_buttons & mask) != 0;
}

/* ---- Remote command handling (game launch) ---- */

void dsu_handle_command(const char *source_address, uint16_t source_port,
      const dsu_command_t *cmd)
{
   settings_t *settings = config_get_ptr();
   bool allowed = false;
   unsigned i;

   /* Security: verify source is a configured DSU server with remote commands allowed */
   for (i = 0; i < MAX_USERS; i++)
   {
      const char *cfg_addr = settings->paths.network_dsu_player_server_address[i];
      unsigned cfg_port = settings->uints.network_dsu_player_server_port[i];

      if (string_is_empty(cfg_addr))
         continue;

      if (!settings->bools.network_dsu_player_allow_remote_commands[i])
         continue;

      if (string_is_equal(source_address, cfg_addr) &&
          (cfg_port == 0 || source_port == cfg_port))
      {
         allowed = true;
         break;
      }
   }

   if (!allowed)
   {
      RARCH_LOG("[DSU] Command rejected: source %s:%u not in configured servers or remote commands disabled\n",
            source_address, source_port);
      return;
   }

   if (cmd->cmd_type == 1) /* Launch game */
   {
      const char *content_path = cmd->content_path;
      const char *core_name = cmd->core_name;
      content_ctx_info_t content_info = {0};

      if (string_is_empty(content_path))
      {
         RARCH_LOG("[DSU] Command rejected: empty content path\n");
         return;
      }

      RARCH_LOG("[DSU] Executing command: launch game '%s' core='%s'\n",
            content_path, core_name);

      task_push_load_content_with_new_core_from_companion_ui(
            string_is_empty(core_name) ? NULL : core_name,
            content_path,
            NULL, /* label */
            NULL, /* db_name */
            NULL, /* crc32 */
            &content_info,
            NULL, /* cb */
            NULL); /* user_data */
   }
   else if (cmd->cmd_type == 2) /* Launch contentless core */
   {
      const char *core_path = cmd->core_name; /* Use core_name field for contentless core path */

      if (string_is_empty(core_path))
      {
         RARCH_LOG("[DSU] Command rejected: empty core path for contentless core launch\n");
         return;
      }

      RARCH_LOG("[DSU] Executing command: launch contentless core '%s'\n", core_path);

#ifdef HAVE_MENU
      /* Load contentless core directly */
      task_push_load_contentless_core_from_menu(core_path);
#else
      RARCH_WARN("[DSU] Contentless core launch requires HAVE_MENU\n");
#endif
   }
   else
   {
      RARCH_LOG("[DSU] Unknown command type: %u\n", cmd->cmd_type);
   }
}

/* ---- Stream handling ---- */

static int dsu_find_player_by_server(const char *source_address, uint16_t source_port)
{
   settings_t *settings = config_get_ptr();
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      const char *cfg_addr = settings->paths.network_dsu_player_server_address[i];
      unsigned cfg_port = settings->uints.network_dsu_player_server_port[i];

      if (string_is_empty(cfg_addr))
         continue;

      if (string_is_equal(source_address, cfg_addr) &&
          (cfg_port == 0 || source_port == cfg_port))
      {
         return (int)i;
      }
   }

   return -1;
}

static void dsu_broadcast_state(dsu_state_t *dsu, const char *source_address,
      uint16_t source_port, uint8_t state_flags)
{
   settings_t *settings = config_get_ptr();
   int player_idx;
   uint8_t pkt[256];
   size_t pkt_len;

   player_idx = dsu_find_player_by_server(source_address, source_port);
   if (player_idx < 0)
      return;

   if (!settings->bools.network_dsu_player_broadcast_state[player_idx])
   {
      RARCH_LOG("[DSU] State broadcast disabled for player %u\n", player_idx);
      return;
   }

   pkt_len = dsu_build_state_packet(pkt, sizeof(pkt),
         dsu->client_id, NULL, NULL, NULL, state_flags);

   if (pkt_len > 0)
   {
      dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len,
            source_address, source_port);
      RARCH_LOG("[DSU] Broadcast state to %s:%u for player %u, flags=0x%02x\n",
            source_address, source_port, player_idx, state_flags);
   }
}

static bool dsu_check_stream_permission(const char *source_address, uint16_t source_port)
{
   settings_t *settings = config_get_ptr();
   bool allowed = false;
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      const char *cfg_addr = settings->paths.network_dsu_player_server_address[i];
      unsigned cfg_port = settings->uints.network_dsu_player_server_port[i];

      if (string_is_empty(cfg_addr))
         continue;

      if (!settings->bools.network_dsu_player_allow_stream_control[i])
         continue;

      if (string_is_equal(source_address, cfg_addr) &&
          (cfg_port == 0 || source_port == cfg_port))
      {
         allowed = true;
         break;
      }
   }

   if (!allowed)
   {
      RARCH_LOG("[DSU] Stream control rejected: source %s:%u not in configured servers or stream control disabled\n",
            source_address, source_port);
      return false;
   }

   return true;
}

/* DSU stream_type wire value mapped to RetroArch streaming_mode.
 * 0 = Twitch, 1 = YouTube, 2 = Facebook, 3 = Local (UDP), 4 = Custom URL.
 * 0xFF = keep currently configured streaming_mode (URL passthrough only). */
#define DSU_STREAM_TYPE_KEEP  0xFFu

/* Stream status wire values (payload.state) */
#define DSU_STREAM_STATE_STOPPED 0u
#define DSU_STREAM_STATE_ACTIVE  1u
#define DSU_STREAM_STATE_ERROR   2u

/* Map DSU bitrate (kbps) to RetroArch video_stream_quality enum index.
 * The quality enum uses the same slots record_config_type defines for
 * streaming presets (LOW/MED/HIGH). Updating this setting makes the UI
 * and the next stream init pick it up, and the change persists to
 * config on exit like any other setting. */
static unsigned dsu_bitrate_to_stream_quality(unsigned bitrate_kbps)
{
   /* These numeric values correspond to RECORD_CONFIG_TYPE_STREAMING_*
    * ordering in record_driver.h (LOW=4, MED=5, HIGH=6 at time of writing,
    * but the enum order is stable: LOW, MED, HIGH, NETPLAY, CUSTOM). */
   if (bitrate_kbps == 0)
      return (unsigned)RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY;
   if (bitrate_kbps < 1500)
      return (unsigned)RECORD_CONFIG_TYPE_STREAMING_LOW_QUALITY;
   if (bitrate_kbps < 4000)
      return (unsigned)RECORD_CONFIG_TYPE_STREAMING_MED_QUALITY;
   return (unsigned)RECORD_CONFIG_TYPE_STREAMING_HIGH_QUALITY;
}

static void dsu_send_stream_status(dsu_state_t *dsu, const char *addr,
      uint16_t port, uint32_t state, uint32_t error_code, uint32_t stream_type,
      uint8_t screen_id, const char *url)
{
   uint8_t pkt[DSU_HEADER_SIZE + DSU_STREAM_STATUS_PAYLOAD];
   size_t pkt_len = dsu_build_stream_status_packet(pkt, sizeof(pkt),
         dsu->client_id, state, error_code, stream_type, screen_id, url);
   if (pkt_len > 0)
   {
      dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len, addr, port);
      RARCH_LOG("[DSU] Sent STREAM_STATUS state=%u err=%u type=%u screen=%u url='%s' to %s:%u\n",
            state, error_code, stream_type, screen_id, url ? url : "",
            addr, port);
   }
}

void dsu_handle_stream_start(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      const dsu_stream_request_t *req)
{
   settings_t *settings       = config_get_ptr();
   recording_state_t *rec_st  = recording_state_get_ptr();
   unsigned requested_mode    = req->stream_type;
   bool have_url              = (req->url[0] != '\0');

   if (!dsu_check_stream_permission(source_address, source_port))
      return;

   RARCH_LOG("[DSU] Stream start request: type=%u, url='%s', %ux%u@%u, bitrate=%u\n",
         req->stream_type, req->url, req->width, req->height, req->fps, req->bitrate);

   /* Map DSU stream_type to RetroArch streaming_mode for the well-known
    * presets (Twitch/YouTube/Facebook/Local/Custom). Using 0xFF keeps
    * whatever mode the user has configured in settings. */
   if (requested_mode <= STREAMING_MODE_CUSTOM)
      settings->uints.streaming_mode = requested_mode;

   /* Resolve URL:
    *   - CUSTOM + url provided      -> use DSU-supplied URL verbatim
    *   - TWITCH/YOUTUBE/FACEBOOK    -> build via stored stream keys
    *   - LOCAL                      -> build udp://127.0.0.1:<port>
    *   - KEEP (0xFF) + url provided -> override existing path_stream_url
    */
   if ((settings->uints.streaming_mode == STREAMING_MODE_CUSTOM
            || requested_mode == DSU_STREAM_TYPE_KEEP) && have_url)
   {
      strlcpy(settings->paths.path_stream_url, req->url,
            sizeof(settings->paths.path_stream_url));
   }
   else
   {
      /* Rebuild URL from streaming_mode + stored keys. */
      recording_driver_update_streaming_url();
   }

   /* Apply resolution override into recording_state geometry so
    * recording_init() picks it up in its non-GPU path. */
   if (req->width && req->height)
   {
      rec_st->width  = req->width;
      rec_st->height = req->height;
   }

   /* Map DSU bitrate hint to RetroArch video_stream_quality setting.
    * This uses the same knob as the Settings UI, so the change persists
    * on config save and takes effect the next time a stream is started. */
   if (req->bitrate > 0)
   {
      unsigned q = dsu_bitrate_to_stream_quality(req->bitrate);
      if (settings->uints.video_stream_quality != q)
      {
         RARCH_LOG("[DSU] Updating video_stream_quality %u -> %u (bitrate hint %u kbps)\n",
               settings->uints.video_stream_quality, q, req->bitrate);
         settings->uints.video_stream_quality = q;
      }
   }

   /* If already streaming, deinit first so the new parameters take effect. */
   if (rec_st->streaming_enable || rec_st->data)
      command_event(CMD_EVENT_RECORD_DEINIT, NULL);

   streaming_set_state(true);
   if (!command_event(CMD_EVENT_RECORD_INIT, NULL))
   {
      RARCH_ERR("[DSU] Stream start: RECORD_INIT failed (mode=%u url='%s').\n",
            settings->uints.streaming_mode, settings->paths.path_stream_url);
      streaming_set_state(false);
      dsu_send_stream_status(dsu, source_address, source_port,
            DSU_STREAM_STATE_ERROR, 1u, settings->uints.streaming_mode,
            0u, settings->paths.path_stream_url);
      return;
   }

   /* Notify requester + broadcast active state */
   dsu_send_stream_status(dsu, source_address, source_port,
         DSU_STREAM_STATE_ACTIVE, 0u, settings->uints.streaming_mode,
         0u, settings->paths.path_stream_url);
   dsu_broadcast_state(dsu, source_address, source_port, 0x01); /* 0x01 = streaming active */
}

void dsu_handle_stream_stop(dsu_state_t *dsu, const char *source_address, uint16_t source_port)
{
   recording_state_t *rec_st = recording_state_get_ptr();

   if (!dsu_check_stream_permission(source_address, source_port))
      return;

   RARCH_LOG("[DSU] Stream stop request (streaming_enable=%d data=%p)\n",
         (int)rec_st->streaming_enable, rec_st->data);

   if (rec_st->streaming_enable || rec_st->data)
      command_event(CMD_EVENT_RECORD_DEINIT, NULL);

   /* Clear DSU-set resolution override so future local recordings use defaults. */
   rec_st->width  = 0;
   rec_st->height = 0;

   /* Notify requester + broadcast inactive state */
   dsu_send_stream_status(dsu, source_address, source_port,
         DSU_STREAM_STATE_STOPPED, 0u, 0u, 0u, "");
   dsu_broadcast_state(dsu, source_address, source_port, 0x00); /* 0x00 = streaming inactive */
}

static bool dsu_check_aux_stream_permission(const char *source_address, uint16_t source_port)
{
   settings_t *settings = config_get_ptr();
   bool allowed = false;
   unsigned i;

   for (i = 0; i < MAX_USERS; i++)
   {
      const char *cfg_addr = settings->paths.network_dsu_player_server_address[i];
      unsigned cfg_port = settings->uints.network_dsu_player_server_port[i];

      if (string_is_empty(cfg_addr))
         continue;

      if (!settings->bools.network_dsu_player_allow_aux_streaming[i])
         continue;

      if (string_is_equal(source_address, cfg_addr) &&
          (cfg_port == 0 || source_port == cfg_port))
      {
         allowed = true;
         break;
      }
   }

   if (!allowed)
   {
      RARCH_LOG("[DSU] Aux stream rejected: source %s:%u not in configured servers or aux streaming disabled\n",
            source_address, source_port);
      return false;
   }

   return true;
}

void dsu_handle_aux_stream_start(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      const dsu_aux_stream_request_t *req)
{
   recording_state_t *rec_st;
   settings_t *settings;
   unsigned idx;

   if (!dsu_check_aux_stream_permission(source_address, source_port))
      return;

   RARCH_LOG("[DSU] Aux stream start: screen=%u, type=%u, url='%s', %ux%u@%u\n",
         req->screen_id, req->stream_type, req->url, req->width, req->height, req->fps);

   /* Broadcast state to server if enabled */
   dsu_broadcast_state(dsu, source_address, source_port, 0x02); /* 0x02 = aux streaming active */

   /* Validate screen_id supports 4 auxiliary screens (1-4) */
   if (req->screen_id < 1 || req->screen_id > 4)
   {
      RARCH_WARN("[DSU] Aux stream rejected: screen_id %u out of range (1-4 supported)\n",
            req->screen_id);
      return;
   }

   rec_st = recording_state_get_ptr();
   idx = req->screen_id - 1;
   (void)rec_st;

   /* Persist URL to RetroArch settings so UI reflects active stream */
   settings = config_get_ptr();
   if (idx < MAX_USERS)
   {
      strlcpy(settings->paths.aux_screen_url[idx], req->url,
            sizeof(settings->paths.aux_screen_url[idx]));
   }

   if (!recording_init_aux(idx, req->url,
            req->bitrate, req->width, req->height, req->fps))
   {
      RARCH_ERR("[DSU] Aux stream slot %u init failed for screen %u (url='%s').\n",
            idx, req->screen_id, req->url);
      dsu_send_stream_status(dsu, source_address, source_port,
            DSU_STREAM_STATE_ERROR, 2u, req->stream_type,
            req->screen_id, req->url);
      return;
   }

   RARCH_LOG("[DSU] Aux stream slot %u activated for screen %u -> %s\n",
         idx, req->screen_id, req->url);

   dsu_send_stream_status(dsu, source_address, source_port,
         DSU_STREAM_STATE_ACTIVE, 0u, req->stream_type,
         req->screen_id, req->url);
}

void dsu_handle_aux_stream_stop(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      uint8_t screen_id)
{
   recording_state_t *rec_st;
   unsigned idx;

   if (!dsu_check_aux_stream_permission(source_address, source_port))
      return;

   RARCH_LOG("[DSU] Aux stream stop: screen=%u\n", screen_id);

   /* Broadcast state to server if enabled */
   dsu_broadcast_state(dsu, source_address, source_port, 0x00); /* 0x00 = aux streaming inactive */

   /* Validate screen_id */
   if (screen_id < 1 || screen_id > 4)
   {
      RARCH_WARN("[DSU] Aux stream stop rejected: screen_id %u out of range\n", screen_id);
      return;
   }

   rec_st = recording_state_get_ptr();
   idx = screen_id - 1;
   (void)rec_st;

   recording_deinit_aux(idx);

   dsu_send_stream_status(dsu, source_address, source_port,
         DSU_STREAM_STATE_STOPPED, 0u, 0u, screen_id, "");
}

/* Broadcast stream status to ALL configured DSU servers.
 * Used when streaming starts/stops from RetroArch UI. */
void dsu_broadcast_stream_status_to_all(dsu_state_t *dsu, uint32_t state,
      uint32_t error_code, uint32_t stream_type, uint8_t screen_id, const char *url)
{
   settings_t *settings = config_get_ptr();
   unsigned i;

   if (!dsu || !dsu->enabled)
      return;

   for (i = 0; i < MAX_USERS; i++)
   {
      const char *cfg_addr = settings->paths.network_dsu_player_server_address[i];
      unsigned cfg_port = settings->uints.network_dsu_player_server_port[i];

      if (string_is_empty(cfg_addr))
         continue;

      /* Send stream status to this configured server */
      dsu_send_stream_status(dsu, cfg_addr, (uint16_t)cfg_port,
            state, error_code, stream_type, screen_id, url);
      RARCH_LOG("[DSU] Broadcast stream status to %s:%u state=%u screen=%u\n",
            cfg_addr, cfg_port, state, screen_id);
   }

   for (i = 0; i < DSU_MAX_CONTROLLERS; i++)
   {
      const char *live_addr = dsu->controllers[i].server_address;
      uint16_t live_port    = dsu->controllers[i].server_port;

      if (!dsu->controllers[i].connected || string_is_empty(live_addr) || !live_port)
         continue;

      dsu_send_stream_status(dsu, live_addr, live_port,
            state, error_code, stream_type, screen_id, url);
      RARCH_LOG("[DSU] Broadcast stream status to live endpoint slot %u %s:%u state=%u screen=%u\n",
            i, live_addr, live_port, state, screen_id);
   }
}

/* Broadcast aux stream status to a SPECIFIC player's DSU server.
 * Used when auxiliary streaming starts/stops for a particular player. */
void dsu_broadcast_aux_stream_status(dsu_state_t *dsu, unsigned player,
      uint32_t state, uint32_t error_code, uint32_t stream_type, const char *url)
{
   settings_t *settings = config_get_ptr();
   const char *cfg_addr;
   unsigned cfg_port;
   unsigned i;

   if (!dsu || !dsu->enabled || player >= MAX_USERS)
      return;

   cfg_addr = settings->paths.network_dsu_player_server_address[player];
   cfg_port = settings->uints.network_dsu_player_server_port[player];

   if (string_is_empty(cfg_addr))
      return;

   /* Send stream status to this specific player's DSU server */
   dsu_send_stream_status(dsu, cfg_addr, (uint16_t)cfg_port,
         state, error_code, stream_type, (uint8_t)(player + 1), url);
   RARCH_LOG("[DSU] Broadcast aux stream status to player %u %s:%u state=%u screen=%u\n",
         player + 1, cfg_addr, cfg_port, state, player + 1);

   for (i = 0; i < DSU_MAX_CONTROLLERS; i++)
   {
      const char *live_addr = dsu->controllers[i].server_address;
      uint16_t live_port    = dsu->controllers[i].server_port;

      if (!dsu->controllers[i].connected || dsu->controllers[i].attached_port != (int)player ||
            string_is_empty(live_addr) || !live_port)
         continue;

      dsu_send_stream_status(dsu, live_addr, live_port,
            state, error_code, stream_type, (uint8_t)(player + 1), url);
      RARCH_LOG("[DSU] Broadcast aux stream status to live endpoint slot %u player %u %s:%u state=%u screen=%u\n",
            i, player + 1, live_addr, live_port, state, player + 1);
   }
}

/* ---- Playlist query handling ---- */

void dsu_send_playlist_list(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      uint32_t id, uint8_t page)
{
   settings_t *settings = config_get_ptr();
   struct string_list *playlists = NULL;
   char dir_playlist[PATH_MAX_LENGTH];
   const char *names[6];
   const char *paths[6];
   uint8_t count = 0;
   uint8_t i;
   uint8_t total_pages = 1;
   uint8_t pkt[DSU_MAX_PACKET_SIZE];
   size_t pkt_len;

   if (!dsu || !dsu->enabled || dsu->socket_fd < 0)
      return;

   /* Get playlist directory - use actual playlist directory from settings */
   if (!string_is_empty(settings->paths.directory_playlist))
      strlcpy(dir_playlist, settings->paths.directory_playlist, sizeof(dir_playlist));
   else
      fill_pathname_join_special(dir_playlist, settings->paths.directory_menu_content,
            "playlists", sizeof(dir_playlist));
   RARCH_LOG("[DSU] Scanning playlists in: %s\n", dir_playlist);

   /* Scan for playlist files */
   playlists = dir_list_new(dir_playlist, "lpl", true, true, true, false);

   if (!playlists || playlists->size == 0)
   {
      /* Send empty response */
      names[0] = "";
      paths[0] = "";
      pkt_len = dsu_build_playlist_list_packet(pkt, sizeof(pkt), id, 0, 1, 0, names, paths);
      if (pkt_len > 0)
         dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len, source_address, source_port);
      RARCH_LOG("[DSU] Sent empty playlist list to %s:%u\n", source_address, source_port);
      return;
   }

   /* Calculate total pages (max 6 entries per packet) */
   total_pages = (playlists->size + 5) / 6;
   if (total_pages < 1) total_pages = 1;

   /* Build entries for this page */
   for (i = 0; i < 6 && (page * 6 + i) < (int)playlists->size; i++)
   {
      struct string_list_elem *elem = &playlists->elems[page * 6 + i];
      const char *filename = path_basename(elem->data);
      names[i] = filename;
      paths[i] = filename;
      count++;
   }

   /* Build and send packet */
   pkt_len = dsu_build_playlist_list_packet(pkt, sizeof(pkt), id, page, total_pages, count, names, paths);
   if (pkt_len > 0)
      dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len, source_address, source_port);

   RARCH_LOG("[DSU] Sent playlist list page %u/%u with %u entries to %s:%u\n",
         page + 1, total_pages, count, source_address, source_port);

   string_list_free(playlists);
}

void dsu_send_playlist_contents(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      uint32_t id, const char *playlist_path, uint8_t page)
{
   playlist_t *playlist = NULL;
   playlist_config_t config;
   const char *labels[8];
   const char *paths[8];
   const char *core_paths[8];
   uint8_t count = 0;
   uint8_t i;
   uint8_t total_pages = 1;
   uint8_t pkt[DSU_MAX_PACKET_SIZE];
   size_t pkt_len;
   size_t num_entries;

   if (!dsu || !dsu->enabled || dsu->socket_fd < 0 || string_is_empty(playlist_path))
      return;

   /* Initialize playlist config */
   settings_t *settings = config_get_ptr();
   config.capacity = COLLECTION_SIZE;
   config.old_format = false;
   config.compress = true;
   config.fuzzy_archive_match = false;
   config.autofix_paths = true;

   /* Resolve relative paths (filename only) to full paths */
   /* Windows absolute paths start with drive letter (e.g., "C:\") or backslash */
   if (!string_is_empty(playlist_path) &&
       !(playlist_path[1] == ':' || playlist_path[0] == '/'))
   {
      char dir_playlist[PATH_MAX_LENGTH];
      if (!string_is_empty(settings->paths.directory_playlist))
         strlcpy(dir_playlist, settings->paths.directory_playlist, sizeof(dir_playlist));
      else
         fill_pathname_join_special(dir_playlist, settings->paths.directory_menu_content,
               "playlists", sizeof(dir_playlist));
      fill_pathname_join(config.path, dir_playlist, playlist_path, sizeof(config.path));
      RARCH_LOG("[DSU] Resolved relative playlist '%s' to '%s'\n", playlist_path, config.path);
   }
   else
   {
      strlcpy(config.path, playlist_path, sizeof(config.path));
   }
   config.base_content_directory[0] = '\0';

   /* Load playlist */
   playlist = playlist_init(&config);
   if (!playlist)
   {
      RARCH_WARN("[DSU] Failed to load playlist: %s\n", playlist_path);
      /* Send empty response */
      labels[0] = "";
      paths[0] = "";
      core_paths[0] = "";
      pkt_len = dsu_build_playlist_contents_packet(pkt, sizeof(pkt), id, 0, 1, 0,
            labels, paths, core_paths);
      if (pkt_len > 0)
         dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len, source_address, source_port);
      return;
   }

   num_entries = playlist_size(playlist);

   /* Calculate total pages (max 3 entries per packet) */
   total_pages = (num_entries + 2) / 3;
   if (total_pages < 1) total_pages = 1;

   /* Build entries for this page */
   for (i = 0; i < 3 && (page * 3 + i) < (int)num_entries; i++)
   {
      const struct playlist_entry *entry = NULL;
      playlist_get_index(playlist, page * 3 + i, &entry);

      if (entry)
      {
         labels[i] = entry->label ? entry->label : path_basename(entry->path);
         paths[i] = entry->path;
         core_paths[i] = entry->core_path ? entry->core_path : "";
         count++;
      }
   }

   /* Build and send packet */
   pkt_len = dsu_build_playlist_contents_packet(pkt, sizeof(pkt), id, page, total_pages, count,
         labels, paths, core_paths);
   if (pkt_len > 0)
      dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len, source_address, source_port);

   RARCH_LOG("[DSU] Sent playlist contents page %u/%u with %u entries (total: %zu) from '%s' to %s:%u\n",
         page + 1, total_pages, count, num_entries, path_basename(playlist_path), source_address, source_port);

   playlist_free(playlist);
}

void dsu_send_contentless_core_list(dsu_state_t *dsu, const char *source_address, uint16_t source_port,
      uint32_t id, uint8_t page)
{
   core_info_list_t *core_info_list = NULL;
   const char *names[3];
   const char *paths[3];
   const char *core_ids[3];
   uint8_t count = 0;
   uint8_t i;
   uint8_t total_pages = 1;
   uint8_t pkt[DSU_MAX_PACKET_SIZE];
   size_t pkt_len;
   size_t contentless_count = 0;
   size_t start_idx;
   size_t contentless_seen = 0;

   if (!dsu || !dsu->enabled || dsu->socket_fd < 0)
      return;

   /* Get core info list */
   core_info_get_list(&core_info_list);
   RARCH_LOG("[DSU] Core info list: %p, count: %zu\n", (void*)core_info_list, core_info_list ? core_info_list->count : 0);

   if (!core_info_list || core_info_list->count == 0)
   {
      /* Send empty response */
      names[0] = "";
      paths[0] = "";
      core_ids[0] = "";
      pkt_len = dsu_build_contentless_core_list_packet(pkt, sizeof(pkt), id, 0, 1, 0, names, paths, core_ids);
      if (pkt_len > 0)
         dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len, source_address, source_port);
      RARCH_LOG("[DSU] Sent empty contentless core list to %s:%u\n", source_address, source_port);
      return;
   }

   /* Count contentless cores */
   for (i = 0; i < core_info_list->count; i++)
   {
      core_info_t *core_info = core_info_get(core_info_list, i);
      if (core_info)
      {
         bool supports_no_game = (core_info->flags & CORE_INFO_FLAG_SUPPORTS_NO_GAME) != 0;
         RARCH_LOG("[DSU] Core %d: '%s' flags=0x%08X supports_no_game=%d\n",
               i, core_info->display_name ? core_info->display_name : "???",
               (unsigned)core_info->flags, supports_no_game);
         if (supports_no_game)
            contentless_count++;
      }
   }

   /* Calculate total pages (max 3 entries per packet) */
   total_pages = (contentless_count + 2) / 3;
   if (total_pages < 1) total_pages = 1;

   /* Build entries for this page */
   start_idx = page * 3;
   count = 0;

   for (i = 0; i < core_info_list->count && count < 3; i++)
   {
      core_info_t *core_info = core_info_get(core_info_list, i);

      if (core_info && (core_info->flags & CORE_INFO_FLAG_SUPPORTS_NO_GAME))
      {
         if (contentless_seen++ < start_idx)
            continue;

         if (count < 3)
         {
            names[count] = core_info->display_name ? core_info->display_name : path_basename(core_info->path);
            paths[count] = core_info->path;
            core_ids[count] = core_info->core_file_id.str ? core_info->core_file_id.str : "";
            count++;
         }
      }
   }

   /* Build and send packet */
   pkt_len = dsu_build_contentless_core_list_packet(pkt, sizeof(pkt), id, page, total_pages, count, names, paths, core_ids);
   if (pkt_len > 0)
      dsu_send_packet_to_server(dsu->socket_fd, pkt, pkt_len, source_address, source_port);

   RARCH_LOG("[DSU] Sent contentless core list page %u/%u with %u entries (total: %zu) to %s:%u\n",
         page + 1, total_pages, count, contentless_count, source_address, source_port);
}

#endif

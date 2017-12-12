/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2017 - Andre Leiradella
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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <string.h>
#include <stdarg.h>

#include <libretro.h>

#include <civetweb/civetweb.h>
#include <string/stdstring.h>
#include <compat/zlib.h>

#include "../../core.h"
#include "../../retroarch.h"
#include "../../core.h"
#include "../../gfx/video_driver.h"
#include "../../managers/core_option_manager.h"
#include "../../cheevos/cheevos.h"
#include "../../content.h"

#define BASIC_INFO "info"
#define MEMORY_MAP "memoryMap"

static struct mg_callbacks s_httpserver_callbacks;
static struct mg_context   *s_httpserver_ctx       = NULL;

/* Based on https://github.com/zeromq/rfc/blob/master/src/spec_32.c */
static void httpserver_z85_encode_inplace(Bytef* data, size_t size)
{
   static char digits[85 + 1] =
   {
      "0123456789"
         "abcdefghij"
         "klmnopqrst"
         "uvwxyzABCD"
         "EFGHIJKLMN"
         "OPQRSTUVWX"
         "YZ.-:+=^!/"
         "*?&<>()[]{"
         "}@%$#"
   };

   uLong value;
   Bytef* source = data + size - 4;
   Bytef* dest   = data + size * 5 / 4 - 5;

   dest[5] = 0;

   if (source >= data)
   {
      do
      {
         value  = source[0] * 256 * 256 * 256;
         value += source[1] * 256 * 256;
         value += source[2] * 256;
         value += source[3];
         source -= 4;

         dest[4] = digits[value % 85];
         value /= 85;
         dest[3] = digits[value % 85];
         value /= 85;
         dest[2] = digits[value % 85];
         value /= 85;
         dest[1] = digits[value % 85];
         dest[0] = digits[value / 85];
         dest -= 5;

      } while (source >= data);
   }
}

static void json_string_encode(char* output, size_t size, const char* input)
{
   /* Don't use with UTF-8 strings. */
   char k;

   if (*input != 0 && size != 0)
   {
      do
      {
         switch (k = *input++)
         {
            case '"':  /* fall through */
            case '\\': /* fall through */
            case '/':  if (size >= 3) { *output++ = '\\'; *output++ = k; size -= 2; } break;
            case '\b': if (size >= 3) { *output++ = '\\'; *output++ = 'b'; size -= 2; } break;
            case '\f': if (size >= 3) { *output++ = '\\'; *output++ = 'f'; size -= 2; } break;
            case '\n': if (size >= 3) { *output++ = '\\'; *output++ = 'n'; size -= 2; } break;
            case '\r': if (size >= 3) { *output++ = '\\'; *output++ = 'r'; size -= 2; } break;
            case '\t': if (size >= 3) { *output++ = '\\'; *output++ = 't'; size -= 2; } break;
            default:   if (size >= 2) { *output++ = k; } size--; break;
         }
      }
      while (*input != 0);
   }

   *output = 0;
}

/*============================================================
HTTP ERRORS
============================================================ */

static int httpserver_error(struct mg_connection* conn, unsigned code, const char* fmt, ...)
{
   va_list args;
   char buffer[1024]  = {0};
   const char* reason = NULL;

   switch (code)
   {
      case 404:
         reason = "Not Found";
         break;

      case 405:
         reason = "Method Not Allowed";
         break;

      default:
         /* Send unknown codes as 500 */
         code = 500;
         reason = "Internal Server Error";
         break;
   }

   va_start(args, fmt);
   vsnprintf(buffer, sizeof(buffer), fmt, args);
   va_end(args);
   buffer[sizeof(buffer) - 1] = 0;

   mg_printf(conn, "HTTP/1.1 %u %s\r\nContent-Type: text/html\r\n\r\n", code, reason);
   mg_printf(conn, "<html><body><h1>%u %s</h1><p>%s</p></body></html>", code, reason, buffer);
   return 1;
}

/*============================================================
INFO
============================================================ */

static int httpserver_handle_basic_info(struct mg_connection* conn, void* cbdata)
{
   static const char *libretro_btn_desc[] = {
      "B (bottom)", "Y (left)", "Select", "Start",
      "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
      "A (right)", "X (up)",
      "L", "R", "L2", "R2", "L3", "R3",
   };

   unsigned p, q, r;
   retro_ctx_api_info_t api;
   retro_ctx_region_info_t region;
   retro_ctx_memory_info_t sram;
   retro_ctx_memory_info_t rtc;
   retro_ctx_memory_info_t sysram;
   retro_ctx_memory_info_t vram;
   char core_path[PATH_MAX_LENGTH]                 = {0};
   const char* pixel_format                        = NULL;
   const struct retro_subsystem_info* subsys       = NULL;
   const struct retro_subsystem_rom_info* rom      = NULL;
   const struct retro_subsystem_memory_info* mem   = NULL;
   const struct retro_controller_description* ctrl = NULL;
   const char* comma                               = NULL;
   const struct core_option* opts                  = NULL;
   const struct retro_system_av_info* av_info      = NULL;
   const core_option_manager_t* core_opts          = NULL;
   const struct mg_request_info              * req = mg_get_request_info(conn);
   const settings_t                     * settings = config_get_ptr();
   rarch_system_info_t *system                     = runloop_get_system_info();

   if (string_is_empty(system->info.library_name))
      return httpserver_error(conn, 500, "Core not initialized in %s", __FUNCTION__);

   if (!core_is_game_loaded())
      return httpserver_error(conn, 500, "Game not loaded in %s", __FUNCTION__);

   json_string_encode(core_path, sizeof(core_path), config_get_active_core_path());

   core_api_version(&api);
   core_get_region(&region);

   switch (video_driver_get_pixel_format())
   {
      case RETRO_PIXEL_FORMAT_0RGB1555:
         pixel_format = "RETRO_PIXEL_FORMAT_0RGB1555";
         break;
      case RETRO_PIXEL_FORMAT_XRGB8888:
         pixel_format = "RETRO_PIXEL_FORMAT_XRGB8888";
         break;
      case RETRO_PIXEL_FORMAT_RGB565:
         pixel_format = "RETRO_PIXEL_FORMAT_RGB565";
         break;
      default:
         pixel_format = "?";
         break;
   }

   sram.id = RETRO_MEMORY_SAVE_RAM;
   core_get_memory(&sram);

   rtc.id = RETRO_MEMORY_RTC;
   core_get_memory(&rtc);

   sysram.id = RETRO_MEMORY_SYSTEM_RAM;
   core_get_memory(&sysram);

   vram.id = RETRO_MEMORY_VIDEO_RAM;
   core_get_memory(&vram);

   mg_printf(conn,
         "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n"
         "{"
         "\"corePath\":\"%s\","
         "\"apiVersion\":%u,"
         "\"systemInfo\":"
         "{"
         "\"libraryName\":\"%s\","
         "\"libraryVersion\":\"%s\","
         "\"validExtensions\":\"%s\","
         "\"needsFullpath\":%s,"
         "\"blockExtract\":%s"
         "},"
         "\"region\":\"%s\","
         "\"pixelFormat\":\"%s\","
         "\"rotation\":%u,"
         "\"performaceLevel\":%u,"
         "\"supportsNoGame\":%s,"
#ifdef HAVE_CHEEVOS
         "\"frontendSupportsAchievements\":true,"
         "\"coreSupportsAchievements\":%s,"
#else
         "\"frontendSupportsAchievements\":false,"
         "\"coreSupportsAchievements\":null,"
#endif
         "\"saveRam\":{\"pointer\":\"%" PRIXPTR "\",\"size\":%" PRIu64 "},"
         "\"rtcRam\":{\"pointer\":\"%" PRIXPTR "\",\"size\":%" PRIu64 "},"
         "\"systemRam\":{\"pointer\":\"%" PRIXPTR "\",\"size\":%" PRIu64 "},"
         "\"videoRam\":{\"pointer\":\"%" PRIXPTR "\",\"size\":%" PRIu64 "},",
      core_path,
      api.version,
      system->info.library_name,
      system->info.library_version,
      system->info.valid_extensions,
      system->info.need_fullpath ? "true" : "false",
      system->info.block_extract ? "true" : "false",
      region.region ? "RETRO_REGION_PAL" : "RETRO_REGION_NTSC",
      pixel_format,
      system->rotation,
      system->performance_level,
      content_does_not_need_content() ? "true" : "false",
#ifdef HAVE_CHEEVOS
      cheevos_get_support_cheevos() ? "true" : "false",
#endif
      (uintptr_t)sram.data, sram.size,
      (uintptr_t)rtc.data, rtc.size,
      (uintptr_t)sysram.data, sysram.size,
      (uintptr_t)vram.data, vram.size
         );

   mg_printf(conn, "\"subSystems\":[");
   subsys = system->subsystem.data;

   for (p = 0; p < system->subsystem.size; p++, subsys++)
   {
      mg_printf(conn, "%s{\"id\":%u,\"description\":\"%s\",\"identifier\":\"%s\",\"roms\":[", p == 0 ? "" : ",", subsys->id, subsys->desc, subsys->ident);
      rom = subsys->roms;

      for (q = 0; q < subsys->num_roms; q++, rom++)
      {
         mg_printf(conn,
               "%s{"
               "\"description\":\"%s\","
               "\"extensions\":\"%s\","
               "\"needsFullpath\":%s,"
               "\"blockExtract\":%s,"
               "\"required\":%s,"
               "\"memory\":[",
               q == 0 ? "" : ",",
               rom->desc,
               rom->valid_extensions,
               rom->need_fullpath ? "true" : "false",
               rom->block_extract ? "true" : "false",
               rom->required ? "true" : "false"
               );

         mem = rom->memory;
         comma = "";

         for (r = 0; r < rom->num_memory; r++, mem++)
         {
            mg_printf(conn, "%s{\"extension\":\"%s\",\"type\":%u}", comma, mem->extension, mem->type);
            comma = ",";
         }

         mg_printf(conn, "]}");
      }

      mg_printf(conn, "]}");
   }

   av_info = video_viewport_get_system_av_info();

   mg_printf(conn,
         "],\"avInfo\":{"
         "\"geometry\":{"
         "\"baseWidth\":%u,"
         "\"baseHeight\":%u,"
         "\"maxWidth\":%u,"
         "\"maxHeight\":%u,"
         "\"aspectRatio\":%f"
         "},"
         "\"timing\":{"
         "\"fps\":%f,"
         "\"sampleRate\":%f"
         "}"
         "},",
         av_info->geometry.base_width,
         av_info->geometry.base_height,
         av_info->geometry.max_width,
         av_info->geometry.max_height,
         av_info->geometry.aspect_ratio,
         av_info->timing.fps,
         av_info->timing.sample_rate
            );

   mg_printf(conn, "\"ports\":[");
   comma = "";

   for (p = 0; p < system->ports.size; p++)
   {
      ctrl = system->ports.data[p].types;

      for (q = 0; q < system->ports.data[p].num_types; q++, ctrl++)
      {
         mg_printf(conn, "%s{\"id\":%u,\"description\":\"%s\"}", comma, ctrl->id, ctrl->desc);
         comma = ",";
      }
   }

   mg_printf(conn, "],\"inputDescriptors\":[");
   comma = "";

   if (core_has_set_input_descriptor())
   {
      for (p = 0; p < settings->input.max_users; p++)
      {
         for (q = 0; q < RARCH_FIRST_CUSTOM_BIND; q++)
         {
            const char* description = system->input_desc_btn[p][q];

            if (description)
            {
               mg_printf(conn,
                     "%s{\"player\":%u,\"button\":\"%s\",\"description\":\"%s\"}",
                     comma,
                     p + 1,
                     libretro_btn_desc[q],
                     description
                     );

               comma = ",";
            }
         }
      }
   }

   mg_printf(conn, "],\"coreOptions\":[");
   rarch_ctl(RARCH_CTL_CORE_OPTIONS_LIST_GET, (void*)&core_opts);
   opts = core_opts->opts;

   for (p = 0; p < core_opts->size; p++, opts++)
   {
      mg_printf(conn, "%s{\"key\":\"%s\",\"description\":\"%s\",\"values\":[", p == 0 ? "" : ",", opts->key, opts->desc);
      comma = "";

      for (q = 0; q < opts->vals->size; q++)
      {
         mg_printf(conn, "%s\"%s\"", comma, opts->vals->elems[q].data);
         comma = ",";
      }

      mg_printf(conn, "]}");
   }

   mg_printf(conn, "]}");
   return 1;
}

/*============================================================
MMAPS
============================================================ */

static int httpserver_handle_get_mmaps(struct mg_connection* conn, void* cbdata)
{
   unsigned id;
   const struct          mg_request_info* req = mg_get_request_info(conn);
   const                          char* comma = "";
   const struct retro_memory_map* mmaps       = NULL;
   const struct retro_memory_descriptor* mmap = NULL;
   rarch_system_info_t *system                = runloop_get_system_info();

   if (strcmp(req->request_method, "GET"))
      return httpserver_error(conn, 405, "Unimplemented method in %s: %s", __FUNCTION__, req->request_method);

   mmaps = &system->mmaps;
   mmap  = mmaps->descriptors;

   mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
   mg_printf(conn, "[");

   for (id = 0; id < mmaps->num_descriptors; id++, mmap++)
   {
      mg_printf(conn,
            "%s{"
            "\"id\":%u,"
            "\"flags\":%" PRIu64 ","
            "\"ptr\":\"%" PRIXPTR "\","
            "\"offset\":%" PRIu64 ","
            "\"start\":%" PRIu64 ","
            "\"select\":%" PRIu64 ","
            "\"disconnect\":%" PRIu64 ","
            "\"len\":%" PRIu64 ","
            "\"addrspace\":\"%s\""
            "}",
            comma,
            id,
            mmap->flags,
            (uintptr_t)mmap->ptr,
            mmap->offset,
            mmap->start,
            mmap->select,
            mmap->disconnect,
            mmap->len,
            mmap->addrspace ? mmap->addrspace : ""
               );

      comma = ",";
   }

   mg_printf(conn, "]");
   return 1;
}

static int httpserver_handle_get_mmap(struct mg_connection* conn, void* cbdata)
{
   size_t start, length;
   unsigned id;
   uLong buflen;
   const struct mg_request_info         * req = mg_get_request_info(conn);
   const char                         * comma = "";
   const struct retro_memory_map* mmaps       = NULL;
   const struct retro_memory_descriptor* mmap = NULL;
   const char* param                          = NULL;
   Bytef* buffer                              = NULL;
   rarch_system_info_t *system                = runloop_get_system_info();

   if (strcmp(req->request_method, "GET"))
      return httpserver_error(conn, 405, "Unimplemented method in %s: %s", __FUNCTION__, req->request_method);

   if (sscanf(req->request_uri, "/" MEMORY_MAP "/%u", &id) != 1)
      return httpserver_error(conn, 500, "Malformed request in %s: %s", __FUNCTION__, req->request_uri);

   mmaps = &system->mmaps;

   if (id >= mmaps->num_descriptors)
      return httpserver_error(conn, 404, "Invalid memory map id in %s: %u", __FUNCTION__, id);

   mmap   = mmaps->descriptors + id;
   start  = 0;
   length = mmap->len;

   if (req->query_string != NULL)
   {
      param = strstr(req->query_string, "start=");

      if (param != NULL)
         start = atoll(param + 6);

      param = strstr(req->query_string, "length=");

      if (param != NULL)
         length = atoll(param + 7);
   }

   if (start >= mmap->len)
      start = mmap->len - 1;

   if (length > mmap->len - start)
      length = mmap->len - start;

   buflen = compressBound(length);
   buffer = (Bytef*)malloc(((buflen + 3) / 4) * 5);

   if (buffer == NULL)
      return httpserver_error(conn, 500, "Out of memory in %s", __FUNCTION__);

   if (compress2(buffer, &buflen, (Bytef*)mmap->ptr + start, length, Z_BEST_COMPRESSION) != Z_OK)
   {
      free((void*)buffer);
      return httpserver_error(conn, 500, "Error during compression in %s", __FUNCTION__);
   }

   buffer[buflen] = 0;
   buffer[buflen + 1] = 0;
   buffer[buflen + 2] = 0;
   httpserver_z85_encode_inplace(buffer, (buflen + 3) & ~3);

   mg_printf(conn, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
   mg_printf(conn,
         "{"
         "\"start\":" STRING_REP_USIZE ","
         "\"length\":" STRING_REP_USIZE ","
         "\"compression\":\"deflate\","
         "\"compressedLength\":" STRING_REP_USIZE ","
         "\"encoding\":\"Z85\","
         "\"data\":\"%s\""
         "}",
         start,
         length,
         (size_t)buflen,
         (char*)buffer
         );

   free((void*)buffer);
   return 1;
}

static int httpserver_handle_mmaps(struct mg_connection* conn, void* cbdata)
{
   unsigned id;
   const struct mg_request_info* req = mg_get_request_info(conn);

   if (sscanf(req->request_uri, "/" MEMORY_MAP "/%u", &id) == 1)
      return httpserver_handle_get_mmap(conn, cbdata);

   return httpserver_handle_get_mmaps(conn, cbdata);
}

/*============================================================
HTTP SERVER
============================================================ */

int httpserver_init(unsigned port)
{
   char str[16];
   snprintf(str, sizeof(str), "%u", port);
   str[sizeof(str) - 1] = 0;

   const char* options[] =
   {
      "listening_ports", str,
      NULL, NULL
   };

   memset(&s_httpserver_callbacks, 0, sizeof(s_httpserver_callbacks));
   s_httpserver_ctx = mg_start(&s_httpserver_callbacks, NULL, options);

   if (s_httpserver_ctx == NULL)
      return -1;

   mg_set_request_handler(s_httpserver_ctx, "/" BASIC_INFO, httpserver_handle_basic_info, NULL);

   mg_set_request_handler(s_httpserver_ctx, "/" MEMORY_MAP, httpserver_handle_mmaps, NULL);
   mg_set_request_handler(s_httpserver_ctx, "/" MEMORY_MAP "/", httpserver_handle_mmaps, NULL);

   return 0;
}

void httpserver_destroy(void)
{
   mg_stop(s_httpserver_ctx);
}
